#include "ArpEngine.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <initializer_list>

namespace mnArpee
{
namespace
{
int clampNote(int n) { return std::clamp(n, 0, 127); }
bool validNote(int n) { return n >= 0 && n < 128; }
// Re-grid tolerance, in quarters, against floating point noise.
constexpr double gridEpsilon = 1.0e-9;
// Host positions that differ from the expected one by more than this are a jump.
constexpr double jumpToleranceQuarters = 0.01;
}

void reduceToKeys(const InputNotes& all, InputNotes& keys) noexcept
{
    keys.size = 0;
    unsigned taken = 0;
    for (int i = 0; i < all.size && keys.size < maxInputKeys; ++i)
    {
        const unsigned bit = 1u << (all.notes[(size_t) i].note % 12);
        if ((taken & bit) == 0) // ascending pitch: the first of a pitch class is its lowest
        {
            taken |= bit;
            keys.notes[(size_t) keys.size++] = all.notes[(size_t) i];
        }
    }
}

KeyUse keyUseOf(const InputNotes& keys, int note) noexcept
{
    if (keys.contains(note))
        return KeyUse::Used;
    for (int i = 0; i < keys.size; ++i)
        if (keys.notes[(size_t) i].note % 12 == note % 12)
            return KeyUse::Merged;
    return KeyUse::LeftOut;
}

int chordRootNote(const InputNotes& input) noexcept
{
    if (input.empty())
        return -1;

    // Chord shapes as interval masks from the root, larger (more specific) ones first.
    constexpr auto mask = [](std::initializer_list<int> intervals)
    {
        unsigned m = 0;
        for (int i : intervals)
            m |= 1u << i;
        return m;
    };
    constexpr unsigned shapes[] = {
        mask({ 0, 4, 7, 11 }), // maj7
        mask({ 0, 4, 7, 10 }), // 7
        mask({ 0, 3, 7, 10 }), // m7
        mask({ 0, 3, 6, 10 }), // m7b5
        mask({ 0, 3, 6, 9 }),  // dim7
        mask({ 0, 4, 7 }),     // major
        mask({ 0, 3, 7 }),     // minor
        mask({ 0, 3, 6 }),     // diminished
        mask({ 0, 4, 8 }),     // augmented
        mask({ 0, 5, 7 }),     // sus4
        mask({ 0, 2, 7 }),     // sus2
        mask({ 0, 7 }),        // power chord
    };

    unsigned present = 0;
    for (int i = 0; i < input.size; ++i)
        present |= 1u << (input.notes[(size_t) i].note % 12);
    const int bassPc = input.notes[0].note % 12;

    // Score: notes explained by the shape, minus notes it leaves out. Ties go to the bass
    // note (inversions of symmetric chords), then to the earlier, more specific shape.
    int bestPc = bassPc;
    int bestScore = -1000;
    for (int pc = 0; pc < 12; ++pc)
    {
        if ((present & (1u << pc)) == 0)
            continue;
        const unsigned rotated = ((present >> pc) | (present << (12 - pc))) & 0xfffu;
        for (unsigned shape : shapes)
        {
            if ((rotated & shape) != shape)
                continue;
            const int score = std::popcount(shape) * 10 - std::popcount(rotated & ~shape) * 11
                              + (pc == bassPc ? 1 : 0);
            if (score > bestScore)
            {
                bestScore = score;
                bestPc = pc;
            }
            break; // shapes are ordered: the first match is the best one for this root
        }
    }

    for (int i = 0; i < input.size; ++i)
        if (input.notes[(size_t) i].note % 12 == bestPc)
            return input.notes[(size_t) i].note;
    return input.notes[0].note;
}

ArpEngine::ArpEngine() { setBank(makeFactoryBank()); }

//==============================================================================
void ArpEngine::setBank(const Bank& b)
{
    updateBank(b);
    pendingProgram = -1;
    currentStep = 0;
    stepsPlayedInProgram = 0;
    lastPlayedStep = -1;
    positionInit = false;
}

void ArpEngine::updateBank(const Bank& b)
{
    bank = b;
    bank.numPrograms = std::clamp(bank.numPrograms, 0, maxPrograms);
    const int last = std::max(0, bank.numPrograms - 1);
    currentProgram = std::clamp(currentProgram, 0, last);
    selectedProgram = std::clamp(selectedProgram, 0, last);
    if (pendingProgram > last)
        pendingProgram = -1;
}

void ArpEngine::setProgramParams(int programIndex, SyncRate sync, int gatePercent,
                                 int swingPercent, int octaveShift, int numSteps)
{
    if (programIndex < 0 || programIndex >= bank.numPrograms)
        return;
    auto& p = bank.programs[(size_t) programIndex];
    p.sync = (SyncRate) std::clamp((int) sync, 0, numSyncRates - 1);
    p.gatePercent = std::clamp(gatePercent, 1, 125);
    p.swingPercent = std::clamp(swingPercent, -50, 50);
    p.octaveShift = std::clamp(octaveShift, -2, 2);
    p.numSteps = std::clamp(numSteps, 0, maxSteps);
}

void ArpEngine::setCurrentProgram(int index)
{
    if (bank.numPrograms <= 0)
        return;
    selectedProgram = std::clamp(index, 0, bank.numPrograms - 1);
    pendingProgram = selectedProgram != currentProgram ? selectedProgram : -1;
}

void ArpEngine::setLatch(bool on)
{
    latch = on;
    if (! on)
        latchedVel.fill(0);
}

//==============================================================================
void ArpEngine::noteOn(int note, int velocity)
{
    if (! validNote(note))
        return;
    if (latch && heldCount == 0)
        latchedVel.fill(0); // a new chord replaces the latched one
    const auto vel = (std::uint8_t) std::clamp(velocity, 1, 127);
    if (heldVel[(size_t) note] == 0)
        ++heldCount;
    heldVel[(size_t) note] = vel;
    sustainedVel[(size_t) note] = 0;
    if (latch)
        latchedVel[(size_t) note] = vel;
}

void ArpEngine::noteOff(int note)
{
    if (! validNote(note) || heldVel[(size_t) note] == 0)
        return;
    if (sustainDown)
        sustainedVel[(size_t) note] = heldVel[(size_t) note];
    heldVel[(size_t) note] = 0;
    --heldCount;
}

void ArpEngine::sustain(bool down)
{
    sustainDown = down;
    if (! down)
        sustainedVel.fill(0);
}

void ArpEngine::panic()
{
    heldVel.fill(0);
    sustainedVel.fill(0);
    latchedVel.fill(0);
    heldCount = 0;
    stepsPlayedInProgram = 0;
    currentStep = 0;
    lastPlayedStep = -1;
    panicPending = true; // render() sends note-offs for everything still sounding
}

//==============================================================================
const Program& ArpEngine::activeProgram() const
{
    static const Program fallback = makeInitProgram();
    if (bank.numPrograms <= 0)
        return fallback;
    return bank.programs[(size_t) std::clamp(currentProgram, 0, bank.numPrograms - 1)];
}

void ArpEngine::collectInput(InputNotes& out) const
{
    out.size = 0;
    for (int n = 0; n < 128; ++n) // ascending pitch: no sort needed
    {
        const int v = heldVel[(size_t) n] != 0 ? heldVel[(size_t) n] : sustainedVel[(size_t) n];
        if (v != 0)
            out.notes[(size_t) out.size++] = HeldNote{ n, v };
    }
    if (out.size == 0 && latch)
        for (int n = 0; n < 128; ++n)
            if (latchedVel[(size_t) n] != 0)
                out.notes[(size_t) out.size++] = HeldNote{ n, latchedVel[(size_t) n] };
}

InputNotes ArpEngine::getActiveInput() const
{
    InputNotes in;
    collectInput(in);
    return in;
}

double ArpEngine::stepQuarters(const Program& prog) { return syncRateToBars(prog.sync) * 4.0; }

double ArpEngine::swingQuarters(const Program& prog, long long index, double stepQ) const
{
    // Swing delays (or with a negative amount, advances) every odd step of the grid.
    if (prog.swingPercent == 0 || (index & 1) == 0)
        return 0.0;
    return stepQ * (std::clamp(prog.swingPercent, -50, 50) / 100.0);
}

double ArpEngine::stepTime(const Program& prog, long long index, double stepQ) const
{
    return gridOriginPpq + (double) index * stepQ + swingQuarters(prog, index, stepQ);
}

int ArpEngine::sampleAt(double ppq) const
{
    const double s = (ppq - blockStartPpq) * samplesPerQuarter;
    return (int) std::clamp(std::llround(s), 0LL, (long long) std::max(0, outSamples - 1));
}

//==============================================================================
void ArpEngine::emitOn(int note, int vel, int at)
{
    note = clampNote(note);
    if (sounding[(size_t) note]) // retrigger: close the old note first
        outBuf->push(EngineEvent{ at, false, outChannel, note, 0 });
    sounding[(size_t) note] = true;
    hasPendingOff[(size_t) note] = false;
    outBuf->push(EngineEvent{ at, true, outChannel, note, std::clamp(vel, 1, 127) });
}

void ArpEngine::emitOff(int note, int at)
{
    hasPendingOff[(size_t) note] = false;
    if (! sounding[(size_t) note])
        return;
    sounding[(size_t) note] = false;
    outBuf->push(EngineEvent{ at, false, outChannel, note, 0 });
}

void ArpEngine::killAll(int at)
{
    for (int n = 0; n < 128; ++n)
        emitOff(n, at);
}

void ArpEngine::scheduleOff(int note, double ppq)
{
    hasPendingOff[(size_t) note] = true;
    pendingOffPpq[(size_t) note] = ppq;
}

void ArpEngine::flushOffs(double uptoPpq)
{
    for (int n = 0; n < 128; ++n)
        if (hasPendingOff[(size_t) n] && pendingOffPpq[(size_t) n] <= uptoPpq)
            emitOff(n, sampleAt(pendingOffPpq[(size_t) n]));
}

//==============================================================================
void ArpEngine::resync(double ppq, bool hostSynced)
{
    // Host sync: the grid is anchored to ppq 0, so steps land on the bar grid and the
    // pattern position follows the song position. Free-run: anchored to "now".
    const auto& prog = activeProgram();
    const double stepQ = stepQuarters(prog);
    gridOriginPpq = hostSynced ? 0.0 : ppq;
    gridIndex = (long long) std::ceil((ppq - gridOriginPpq) / stepQ - gridEpsilon);
    lastStepQ = stepQ;
    const int numSteps = std::clamp(prog.numSteps, 0, maxSteps);
    if (hostSynced && numSteps > 1)
    {
        currentStep = (int) (((gridIndex % numSteps) + numSteps) % numSteps);
        stepsPlayedInProgram = currentStep;
    }
    else
    {
        currentStep = 0;
        stepsPlayedInProgram = 0;
    }
    positionInit = true;
}

void ArpEngine::switchProgram(int index)
{
    currentProgram = std::clamp(index, 0, std::max(0, bank.numPrograms - 1));
    pendingProgram = -1;
    currentStep = 0;
    stepsPlayedInProgram = 0;
}

void ArpEngine::advanceProgram()
{
    currentStep = 0;
    stepsPlayedInProgram = 0;
    if (pendingProgram >= 0)
        switchProgram(pendingProgram);
}

void ArpEngine::playOneStep(const Program& prog, const InputNotes& input, double t, double stepQ)
{
    const int numSteps = std::clamp(prog.numSteps, 2, maxSteps);
    const int stepIndex = currentStep % numSteps;
    const Step& step = prog.steps[(size_t) stepIndex];
    const int at = sampleAt(t);
    const double oneSample = 1.0 / samplesPerQuarter;

    bool play = step.chance >= 100;
    if (! play)
    {
        const int roll = randomFn ? randomFn(1, 100) : (int) (rng() % 100) + 1;
        play = roll <= std::clamp((int) step.chance, 1, 100);
    }

    if (play && step.type != StepType::Off && step.type != StepType::Rest)
    {
        // Rest is silent here: the preceding note was lengthened when it was scheduled.
        // KEY SELECT: k1..k5 index into the keys (the held notes, one per pitch class, sorted);
        // Root is the root of the held chord, and it also stands in for a key that is not held.
        reduceToKeys(input, scratchKeys);
        const auto& keys = scratchKeys;
        int source = 0;
        if (step.type == StepType::Random)
        {
            const int pick = randomFn ? randomFn(0, keys.size - 1) : (int) (rng() % (unsigned) keys.size);
            source = keys.notes[(size_t) std::clamp(pick, 0, keys.size - 1)].note;
        }
        else if (step.keyIndex == rootKeyIndex)
        {
            source = chordRootNote(input);
        }
        else if (step.keyIndex >= keys.size)
        {
            source = chordRootNote(input); // the key is not held: the root takes its place
        }
        else
        {
            source = keys.notes[(size_t) std::clamp((int) step.keyIndex, 0, maxInputKeys - 1)].note;
        }
        const int note = clampNote(source + step.scaleStep + step.octave * 12 + prog.transposeOct * 12
                                   + prog.octaveShift * 12);

        if (step.type == StepType::Tie)
        {
            // Legato: the new note starts first, the old ones end one sample later, and a
            // note that is tied to itself keeps sounding without a retrigger.
            for (int n = 0; n < 128; ++n)
                if (sounding[(size_t) n] && n != note)
                    scheduleOff(n, t + oneSample);
            if (! sounding[(size_t) note])
                emitOn(note, step.velocity, at);
        }
        else
        {
            // Notes without a gate (left over from quantized thru) end here.
            for (int n = 0; n < 128; ++n)
                if (sounding[(size_t) n] && ! hasPendingOff[(size_t) n] && n != note)
                    emitOff(n, at);
            emitOn(note, step.velocity, at);
        }

        // Gate, lengthened by directly following Rest steps (up to the pattern end) and
        // held over into a following Tie step.
        const double gateQ = stepQ * (std::clamp(prog.gatePercent, 1, 125) / 100.0)
                             * (std::clamp((int) step.gateScalePercent, 10, 800) / 100.0);
        int restSteps = 0;
        int k = stepIndex + 1;
        for (; k < numSteps && prog.steps[(size_t) k].type == StepType::Rest; ++k)
            ++restSteps;
        double offAt = t + std::max(gateQ, oneSample) + restSteps * stepQ;
        if (k < numSteps && prog.steps[(size_t) k].type == StepType::Tie)
            offAt = std::max(offAt, stepTime(prog, gridIndex + 1 + restSteps, stepQ) + oneSample);
        scheduleOff(note, offAt);
    }

    lastPlayedStep = stepIndex;
    currentStep = (stepIndex + 1) % numSteps;
    ++stepsPlayedInProgram;
}

void ArpEngine::playQuantizedThru(const InputNotes& input, int at)
{
    for (int i = 0; i < input.size; ++i)
        if (! sounding[(size_t) input.notes[(size_t) i].note])
            emitOn(input.notes[(size_t) i].note, input.notes[(size_t) i].velocity, at);
    for (int n = 0; n < 128; ++n)
        if (sounding[(size_t) n] && ! input.contains(n))
            emitOff(n, at);
}

//==============================================================================
void ArpEngine::render(int numSamples, const HostPosition& host, EventBuffer& out)
{
    if (numSamples <= 0 || bank.numPrograms <= 0)
        return;

    outBuf = &out;
    outSamples = numSamples;
    const int firstEvent = out.size();

    const double bpm = host.bpm > 0.0 ? host.bpm : 120.0;
    const double sr = host.sampleRate > 0.0 ? host.sampleRate : 44100.0;
    samplesPerQuarter = sr * 60.0 / bpm;

    blockStartPpq = host.isPlaying ? host.ppqPosition : freePpq;
    const double blockEndPpq = blockStartPpq + numSamples / samplesPerQuarter;
    // Anything that rounds to the next block's first sample belongs to the next block.
    const double blockLimitPpq = blockStartPpq + (numSamples - 0.5) / samplesPerQuarter;

    // Transport start, stop or jump (loop, locate): realign to the new position.
    if (host.isPlaying != wasPlaying
        || (host.isPlaying && std::abs(blockStartPpq - freePpq) > jumpToleranceQuarters))
        positionInit = false;
    wasPlaying = host.isPlaying;
    freePpq = blockEndPpq;

    if (panicPending || ! positionInit)
    {
        killAll(0);
        panicPending = false;
    }
    if (! positionInit)
        resync(blockStartPpq, host.isPlaying);

    collectInput(scratchInput);
    if (! host.isPlaying && ! hadInput && ! scratchInput.empty())
    {
        // Free-run: the first key of a new phrase starts the pattern right away.
        if (pendingProgram >= 0)
            switchProgram(pendingProgram);
        resync(blockStartPpq, false);
        currentStep = 0;
        stepsPlayedInProgram = 0;
    }

    for (int guard = 0; guard < 4 * EventBuffer::capacity; ++guard)
    {
        const Program& prog = activeProgram();
        const double stepQ = stepQuarters(prog);
        if (std::abs(stepQ - lastStepQ) > 1.0e-12)
        {
            // Rate change: continue on the next point of the new grid.
            const double nextBase = gridOriginPpq + (double) gridIndex * lastStepQ;
            gridIndex = (long long) std::ceil((nextBase - gridOriginPpq) / stepQ - gridEpsilon);
            lastStepQ = stepQ;
        }

        const double t = stepTime(prog, gridIndex, stepQ);
        if (t >= blockLimitPpq)
            break;
        flushOffs(t);
        const int at = sampleAt(t);
        const int numSteps = std::clamp(prog.numSteps, 0, maxSteps);

        if (numSteps > 1 && stepsPlayedInProgram >= numSteps)
        {
            advanceProgram();
            continue;
        }

        collectInput(scratchInput);
        if (numSteps <= 1)
        {
            // 0 steps = MIDI thru (handled by the caller), 1 step = quantized thru.
            // There is no pattern end to wait for, so program switches happen at once.
            if (numSteps == 0)
                killAll(at);
            else
                playQuantizedThru(scratchInput, at);
            lastPlayedStep = scratchInput.empty() ? -1 : 0;
            ++gridIndex;
            if (pendingProgram >= 0)
                switchProgram(pendingProgram);
            continue;
        }

        if (scratchInput.empty())
        {
            killAll(at);
            lastPlayedStep = -1;
            if (pendingProgram >= 0)
                switchProgram(pendingProgram);
        }
        else
        {
            playOneStep(prog, scratchInput, t, stepQ);
        }
        ++gridIndex;
    }

    flushOffs(blockLimitPpq - gridEpsilon);
    collectInput(scratchInput);
    hadInput = ! scratchInput.empty();
    outBuf = nullptr;

    // Time order; at the same sample, note-offs come before note-ons.
    std::sort(out.begin() + firstEvent, out.end(),
              [](const EngineEvent& a, const EngineEvent& b)
              {
                  if (a.sampleOffset != b.sampleOffset)
                      return a.sampleOffset < b.sampleOffset;
                  return ! a.isNoteOn && b.isNoteOn;
              });
}

} // namespace mnArpee
