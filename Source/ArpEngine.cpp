#include "ArpEngine.h"

#include <algorithm>
#include <cmath>

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

ArpEngine::ArpEngine() { setBank(makeFactoryBank()); }

//==============================================================================
void ArpEngine::setBank(const Bank& b)
{
    updateBank(b);
    pendingProgram = -1;
    pendingChain = -2;
    currentStep = 0;
    stepsPlayedInProgram = 0;
    chainPos = 0;
    lastPlayedStep = -1;
    positionInit = false;
}

void ArpEngine::updateBank(const Bank& b)
{
    bank = b;
    bank.numPrograms = std::clamp(bank.numPrograms, 0, maxPrograms);
    bank.numChains = std::clamp(bank.numChains, 0, maxChains);
    const int last = std::max(0, bank.numPrograms - 1);
    currentProgram = std::clamp(currentProgram, 0, last);
    selectedProgram = std::clamp(selectedProgram, 0, last);
    if (pendingProgram > last)
        pendingProgram = -1;
    if (currentChain >= bank.numChains)
        currentChain = -1;
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

void ArpEngine::setCurrentChain(int chainIndex)
{
    pendingChain = std::clamp(chainIndex, -1, maxChains - 1);
    pendingChainAtPpq = -1.0;
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

bool ArpEngine::chainActive() const
{
    return currentChain >= 0 && currentChain < bank.numChains
           && bank.chains[(size_t) currentChain].length > 0;
}

int ArpEngine::chainProgramAt(int pos) const
{
    const auto& c = bank.chains[(size_t) currentChain];
    const int len = std::clamp(c.length, 1, maxChainSteps);
    const int p = c.programs[(size_t) (((pos % len) + len) % len)];
    return std::clamp(p, 0, std::max(0, bank.numPrograms - 1));
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
    if (chainActive())
    {
        chainPos = (chainPos + 1) % std::clamp(bank.chains[(size_t) currentChain].length, 1, maxChainSteps);
        currentProgram = chainProgramAt(chainPos);
    }
    else if (pendingProgram >= 0)
    {
        switchProgram(pendingProgram);
    }
}

void ArpEngine::applyPendingChain(double blockEndPpq)
{
    if (pendingChain == -2)
        return;
    const double gridQ = std::max(bank.chainQuantizeBars * 4.0, 1.0 / 256.0);
    if (pendingChainAtPpq < 0.0)
        pendingChainAtPpq = std::ceil(blockStartPpq / gridQ - gridEpsilon) * gridQ;
    if (blockEndPpq < pendingChainAtPpq)
        return;

    currentChain = pendingChain < bank.numChains ? pendingChain : -1;
    pendingChain = -2;
    if (chainActive())
    {
        if (bank.restartChainOnSwitch)
        {
            chainPos = 0;
            switchProgram(chainProgramAt(0));
        }
    }
    else
    {
        // Chain off: go back to the program that is selected.
        switchProgram(selectedProgram);
    }
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
        std::array<int, 128> picked {};
        int numPicked = 0;
        const int shift = step.scaleStep + step.octave * 12 + prog.transposeOct * 12
                          + prog.octaveShift * 12;
        if (step.type == StepType::Chord)
        {
            for (int i = 0; i < input.size; ++i)
                picked[(size_t) numPicked++] = clampNote(input.notes[(size_t) i].note + shift);
        }
        else
        {
            // KEY SELECT: k1..k5 index into the sorted input; wraps around if fewer notes are held.
            int pick = 0;
            if (step.type == StepType::Random)
                pick = randomFn ? randomFn(0, input.size - 1) : (int) (rng() % (unsigned) input.size);
            else
                pick = std::clamp((int) step.keyIndex, 0, maxInputKeys - 1) % input.size;
            pick = std::clamp(pick, 0, input.size - 1);
            picked[(size_t) numPicked++] = clampNote(input.notes[(size_t) pick].note + shift);
        }
        const auto isPicked = [&](int note)
        {
            return std::find(picked.begin(), picked.begin() + numPicked, note) != picked.begin() + numPicked;
        };

        if (step.type == StepType::Tie)
        {
            // Legato: the new note starts first, the old ones end one sample later, and a
            // note that is tied to itself keeps sounding without a retrigger.
            for (int n = 0; n < 128; ++n)
                if (sounding[(size_t) n] && ! isPicked(n))
                    scheduleOff(n, t + oneSample);
            for (int i = 0; i < numPicked; ++i)
                if (! sounding[(size_t) picked[(size_t) i]])
                    emitOn(picked[(size_t) i], step.velocity, at);
        }
        else
        {
            // Notes without a gate (left over from quantized thru) end here.
            for (int n = 0; n < 128; ++n)
                if (sounding[(size_t) n] && ! hasPendingOff[(size_t) n] && ! isPicked(n))
                    emitOff(n, at);
            for (int i = 0; i < numPicked; ++i)
                emitOn(picked[(size_t) i], step.velocity, at);
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
        for (int i = 0; i < numPicked; ++i)
            scheduleOff(picked[(size_t) i], offAt);
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
        if (pendingProgram >= 0 && ! chainActive())
            switchProgram(pendingProgram);
        resync(blockStartPpq, false);
        currentStep = 0;
        stepsPlayedInProgram = 0;
    }

    applyPendingChain(blockEndPpq);

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
            if (chainActive())
                advanceProgram();
            else if (pendingProgram >= 0)
                switchProgram(pendingProgram);
            continue;
        }

        if (scratchInput.empty())
        {
            killAll(at);
            lastPlayedStep = -1;
            if (pendingProgram >= 0 && ! chainActive())
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
