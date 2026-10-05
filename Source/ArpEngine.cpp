#include "ArpEngine.h"

#include <algorithm>
#include <cmath>

namespace mnArpee
{
namespace
{
double barsToQuarters(double bars) { return bars * 4.0; }
int clampNote(int n) { return std::clamp(n, 0, 127); }
}

ArpEngine::ArpEngine() { bank = makeFactoryBank(); }

void ArpEngine::setBank(const Bank& b)
{
    bank = b;
    if (! bank.programs.empty())
        currentProgram = std::clamp(currentProgram, 0, (int) bank.programs.size() - 1);
    pendingProgram = -1;
    pendingChain = -2;
    currentStep = 0;
    stepsPlayedInProgram = 0;
    chainStepPos = 0;
    previousStepNotes.clear();
}

void ArpEngine::updateBank(const Bank& b)
{
    bank = b;
    if (! bank.programs.empty())
        currentProgram = std::clamp(currentProgram, 0, (int) bank.programs.size() - 1);
    currentStep = std::min(currentStep, maxSteps - 1);
}

void ArpEngine::setProgramParams(int programIndex, SyncRate sync, int gatePercent,
                                 int swingPercent, int octaveShift, int numSteps)
{
    if (programIndex < 0 || programIndex >= (int) bank.programs.size())
        return;
    auto& p = bank.programs[(size_t) programIndex];
    p.sync = sync;
    p.gatePercent = gatePercent;
    p.swingPercent = swingPercent;
    p.octaveShift = octaveShift;
    p.numSteps = numSteps;
}

void ArpEngine::setCurrentProgram(int index)
{
    if (bank.programs.empty())
        return;
    pendingProgram = std::clamp(index, 0, (int) bank.programs.size() - 1);
}

void ArpEngine::setCurrentChain(int chainIndex)
{
    pendingChain = chainIndex;
    pendingChainAtPpq = -1.0;
}

void ArpEngine::noteOn(int note, int velocity)
{
    if (latch && held.empty())
        latchedNotes.clear(); // a new chord replaces the latched one
    HeldNote h{ note, std::clamp(velocity, 1, 127), orderCounter++ };
    held[note] = h;
    sustainedNotes.erase(note);
    if (latch)
    {
        latchedNotes.erase(
            std::remove_if(latchedNotes.begin(), latchedNotes.end(),
                           [note](const HeldNote& x) { return x.note == note; }),
            latchedNotes.end());
        latchedNotes.push_back(h);
    }
}

void ArpEngine::noteOff(int note)
{
    auto it = held.find(note);
    if (it == held.end())
        return;
    if (sustainDown)
        sustainedNotes.insert(note);
    if (latch)
    {
        // Latch on: a released note stays part of the held chord.
        latchedNotes.erase(
            std::remove_if(latchedNotes.begin(), latchedNotes.end(),
                           [note](const HeldNote& x) { return x.note == note; }),
            latchedNotes.end());
        latchedNotes.push_back(it->second);
    }
    held.erase(it);
    if (! bank.programs.empty() && activeProgram().numSteps <= 1)
        sounding.erase(note);
}

void ArpEngine::sustain(bool down)
{
    sustainDown = down;
    if (! down)
        sustainedNotes.clear();
}

void ArpEngine::panic()
{
    held.clear();
    latchedNotes.clear();
    sustainedNotes.clear();
    previousStepNotes.clear();
    stepsPlayedInProgram = 0;
    currentStep = 0;
    panicPending = true; // render() sends note-offs for everything still sounding
}

std::vector<HeldNote> ArpEngine::getActiveInput() const { return orderedInput(); }

const Program& ArpEngine::activeProgram() const
{
    static const Program fallback = makeInitProgram();
    if (bank.programs.empty())
        return fallback;
    const int idx = std::clamp(currentProgram, 0, (int) bank.programs.size() - 1);
    return bank.programs[(size_t) idx];
}

std::vector<int> ArpEngine::resolveChainPrograms() const
{
    if (bank.currentChain < 0 || bank.currentChain >= (int) bank.chains.size())
        return {};
    std::vector<int> out;
    for (int p : bank.chains[(size_t) bank.currentChain].programs)
        if (p >= 0 && p < (int) bank.programs.size())
            out.push_back(p);
    return out;
}


std::vector<HeldNote> ArpEngine::orderedInput() const
{
    std::vector<HeldNote> notes;
    notes.reserve(held.size() + sustainedNotes.size());
    for (const auto& kv : held)
        notes.push_back(kv.second);
    for (int n : sustainedNotes)
    {
        if (held.count(n) != 0)
            continue;
        auto it = std::find_if(latchedNotes.begin(), latchedNotes.end(),
                               [n](const HeldNote& h) { return h.note == n; });
        notes.push_back(it != latchedNotes.end() ? *it : HeldNote{ n, 96, 0 });
    }
    if (notes.empty() && latch && ! latchedNotes.empty())
        notes = latchedNotes;

    // No pre-filtering: input is always sorted by ascending pitch; each step's KEY SELECT picks the note.
    std::sort(notes.begin(), notes.end(),
              [](const HeldNote& a, const HeldNote& b) { return a.note < b.note; });
    return notes;
}

double ArpEngine::stepBars() const { return syncRateToBars(activeProgram().sync); }

double ArpEngine::swingOffsetBars(int stepIndex) const
{
    const int swing = activeProgram().swingPercent;
    if (swing == 0 || (stepIndex % 2) == 0)
        return 0.0;
    return stepBars() * (swing / 100.0);
}

std::vector<int> ArpEngine::pickStepNotes(const Program& prog, const Step& step,
                                          const std::vector<HeldNote>& input)
{
    std::vector<int> picked;
    if (input.empty())
        return picked;
    const int n = (int) input.size();
    if (step.type == StepType::Chord)
    {
        for (const auto& h : input)
            picked.push_back(clampNote(h.note + step.scaleStep + step.octave * 12
                                       + prog.transposeOct * 12 + prog.octaveShift * 12));
        return picked;
    }
    // KEY SELECT: k1..k5 index into the sorted input; wraps around if fewer notes are held.
    int pick = 0;
    if (step.type == StepType::Random)
        pick = randomFn ? randomFn(0, n - 1) : (int) (rng() % (unsigned) n);
    else
        pick = std::clamp((int) step.keyIndex, 0, maxInputKeys - 1) % n;
    const auto& h = input[(size_t) std::clamp(pick, 0, n - 1)];
    const int tr = step.scaleStep + step.octave * 12 + prog.transposeOct * 12 + prog.octaveShift * 12;
    picked.push_back(clampNote(h.note + tr));
    return picked;
}

void ArpEngine::emitOnBuf(int note, int vel, int at)
{
    if (outBuf == nullptr)
        return;
    note = clampNote(note);
    at = std::clamp(at, 0, outSamples - 1);
    if (sounding.count(note) != 0) // retrigger: close the old note first
        outBuf->push_back(EngineEvent{ at, false, 1, note, 0 });
    sounding[note] = 1;
    pendingOffs.erase(note);
    outBuf->push_back(EngineEvent{ at, true, 1, note, std::clamp(vel, 1, 127) });
}

void ArpEngine::emitOffBuf(int note, int at)
{
    if (outBuf == nullptr)
        return;
    pendingOffs.erase(note);
    if (sounding.erase(note) != 0)
        outBuf->push_back(EngineEvent{ std::clamp(at, 0, outSamples - 1), false, 1, note, 0 });
}

void ArpEngine::scheduleOff(int note, double ppq) { pendingOffs[note] = ppq; }

void ArpEngine::flushOffs(double uptoPpq, double blockStartPpq, double samplesPerQuarter)
{
    std::vector<std::pair<int, double>> due;
    for (const auto& kv : pendingOffs)
        if (kv.second <= uptoPpq)
            due.emplace_back(kv.first, kv.second);
    for (const auto& d : due)
        emitOffBuf(d.first,
                   (int) std::llround((d.second - blockStartPpq) * samplesPerQuarter));
}

void ArpEngine::killAllBuf(int at)
{
    std::vector<int> toKill;
    for (const auto& kv : sounding)
        toKill.push_back(kv.first);
    for (int nn : toKill)
        emitOffBuf(nn, at);
}

void ArpEngine::playOneStep(Program& prog, const std::vector<HeldNote>& input,
                            int at, int numSamples, double samplesPerQuarter)
{
    const int numSteps = std::clamp(prog.numSteps, 2, maxSteps);
    const Step& step = prog.steps[(size_t) (currentStep % numSteps)];
    const double swBars = swingOffsetBars(currentStep);
    const int swung = std::clamp(
        at + (int) std::llround((swBars * 4.0) * samplesPerQuarter), 0, numSamples - 1);

    bool play = step.chance >= 100;
    if (! play)
    {
        const int roll = randomFn ? randomFn(1, 100) : (int) (rng() % 100) + 1;
        play = roll <= std::clamp((int) step.chance, 1, 100);
    }

    const double stepQ = barsToQuarters(stepBars());
    const double startPpq = nextStepPpq + swBars * 4.0;

    if (play && step.type == StepType::Rest)
    {
        // Nothing to trigger: the preceding note was already extended when it was scheduled.
    }
    else if (play && step.type != StepType::Off)
    {
        auto newNotes = pickStepNotes(prog, step, input);
        // Tie overlaps: the previous notes are released as the new ones start.
        // Otherwise old notes end by their own gate (which may exceed 100%).
        if (step.type == StepType::Tie)
        {
            std::vector<int> olds;
            for (const auto& kv : sounding)
                if (std::find(newNotes.begin(), newNotes.end(), kv.first) == newNotes.end())
                    olds.push_back(kv.first);
            for (int nn : newNotes)
                emitOnBuf(nn, step.velocity, swung);
            for (int nn : olds)
                emitOffBuf(nn, swung);
        }
        else
        {
            for (int nn : newNotes)
                emitOnBuf(nn, step.velocity, swung);
        }
        const double gateQ = stepQ * (std::clamp(prog.gatePercent, 1, 125) / 100.0)
                             * (std::clamp((int) step.gateScalePercent, 10, 800) / 100.0);
        // Rest steps directly after this one lengthen the note (up to the pattern end).
        int restSteps = 0;
        for (int k = currentStep + 1; k < numSteps && prog.steps[(size_t) k].type == StepType::Rest; ++k)
            ++restSteps;
        for (int nn : newNotes)
            scheduleOff(clampNote(nn), startPpq + std::max(gateQ, 1.0e-4) + restSteps * stepQ);
        previousStepNotes = newNotes;
    }
    else if (step.type == StepType::Off)
    {
        previousStepNotes.clear();
    }

    currentStep = (currentStep + 1) % numSteps;
    stepsPlayedInProgram++;
}

std::vector<EngineEvent> ArpEngine::render(int numSamples, const HostPosition& host,
                                                     const std::vector<EngineEvent>& thru)
{
    std::vector<EngineEvent> out;
    out.reserve(thru.size() + 32);
    for (auto e : thru)
    {
        e.sampleOffset = std::clamp(e.sampleOffset, 0, std::max(0, numSamples - 1));
        out.push_back(e);
    }
    outBuf = &out;
    outSamples = numSamples;
    if (numSamples <= 0 || bank.programs.empty())
    {
        outBuf = nullptr;
        return out;
    }

    const double safeBpm = host.bpm > 0.0 ? host.bpm : 120.0;
    const double sr = host.sampleRate > 0.0 ? host.sampleRate : 44100.0;
    const double samplesPerQuarter = sr * 60.0 / safeBpm;

    double blockStartPpq = freePpq;
    double blockEndPpq = freePpq + numSamples / samplesPerQuarter;
    if (host.isPlaying)
    {
        blockStartPpq = host.ppqPosition;
        blockEndPpq = blockStartPpq + numSamples / samplesPerQuarter;
        if (positionInit && std::abs(blockStartPpq - freePpq) > stepBars() * 2.0 + 1.0)
            positionInit = false;
    }
    freePpq = blockEndPpq;

    if (! positionInit)
    {
        killAllBuf(0);
        pendingOffs.clear();
        nextStepPpq = blockStartPpq;
        positionInit = true;
    }
    if (nextStepPpq < blockStartPpq - stepBars() * 64.0)
        nextStepPpq = blockStartPpq;

    if (pendingChain != -2)
    {
        const double gridQ = std::max(barsToQuarters(bank.chainQuantizeBars), 1.0 / 256.0);
        if (pendingChainAtPpq < 0.0)
            pendingChainAtPpq = std::ceil(blockStartPpq / gridQ) * gridQ;
        if (blockEndPpq >= pendingChainAtPpq)
        {
            bank.currentChain = std::clamp(pendingChain, -1, (int) bank.chains.size() - 1);
            pendingChain = -2;
            if (bank.restartChainOnSwitch)
            {
                chainStepPos = 0;
                stepsPlayedInProgram = 0;
                currentStep = 0;
                const auto seq = resolveChainPrograms();
                if (! seq.empty())
                    currentProgram = seq[0];
            }
        }
    }

    if (panicPending)
    {
        killAllBuf(0);
        pendingOffs.clear();
        panicPending = false;
    }

    while (nextStepPpq < blockEndPpq)
    {
        const double stepQuarters = barsToQuarters(stepBars());
        flushOffs(nextStepPpq, blockStartPpq, samplesPerQuarter);
        const int at = std::clamp(
            (int) std::llround((nextStepPpq - blockStartPpq) * samplesPerQuarter),
            0, numSamples - 1);
        const int progIdx = std::clamp(currentProgram, 0, (int) bank.programs.size() - 1);
        Program& prog = bank.programs[(size_t) progIdx];
        const int numSteps = std::clamp(prog.numSteps, 0, maxSteps);

        if (prog.numSteps > 1 && stepsPlayedInProgram >= numSteps)
        {
            stepsPlayedInProgram = 0;
            currentStep = 0;
            const auto seq = resolveChainPrograms();
            if (! seq.empty())
            {
                chainStepPos = (chainStepPos + 1) % (int) seq.size();
                currentProgram = seq[(size_t) chainStepPos];
            }
            else if (pendingProgram >= 0)
            {
                currentProgram = pendingProgram;
                pendingProgram = -1;
            }
            continue;
        }

        const auto input = orderedInput();

        if (numSteps <= 1)
        {
            // No pattern end to wait for: switch program right away.
            if (pendingProgram >= 0 && resolveChainPrograms().empty())
            {
                currentProgram = pendingProgram;
                pendingProgram = -1;
                continue;
            }
            if (numSteps == 1 && ! input.empty())
            {
                for (const auto& h : input)
                    if (sounding.count(h.note) == 0)
                        emitOnBuf(h.note, h.velocity, at);
                std::vector<int> toKill;
                for (const auto& kv : sounding)
                {
                    bool alive = false;
                    for (const auto& h : input)
                        if (h.note == kv.first)
                            alive = true;
                    if (! alive)
                        toKill.push_back(kv.first);
                }
                for (int nn : toKill)
                    emitOffBuf(nn, at);
            }
        }
        else if (! input.empty())
        {
            playOneStep(prog, input, at, numSamples, samplesPerQuarter);
        }
        else
        {
            killAllBuf(at);
            previousStepNotes.clear();
        }

        nextStepPpq += stepQuarters;
        if (stepQuarters <= 0.0)
            break;
    }

    flushOffs(blockEndPpq - 1.0e-9, blockStartPpq, samplesPerQuarter);
    outBuf = nullptr;
    // Time order; at the same sample, note-offs come before note-ons.
    std::stable_sort(out.begin(), out.end(),
                     [](const EngineEvent& a, const EngineEvent& b)
                     {
                         if (a.sampleOffset != b.sampleOffset)
                             return a.sampleOffset < b.sampleOffset;
                         return ! a.isNoteOn && b.isNoteOn;
                     });
    return out;
}

} // namespace mnArpee
