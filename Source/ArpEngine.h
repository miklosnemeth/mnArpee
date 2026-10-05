#pragma once

#include "PatternData.h"

#include <cstdint>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <vector>

namespace mnArpee
{

struct HostPosition
{
    double ppqPosition = 0.0;
    double bpm = 120.0;
    bool isPlaying = false;
    double sampleRate = 44100.0;
};

struct EngineEvent
{
    int sampleOffset = 0;
    bool isNoteOn = true;
    int channel = 1;
    int note = 60;
    int velocity = 96;
};

struct HeldNote
{
    int note = 60;
    int velocity = 96;
    std::uint64_t order = 0;
};

class ArpEngine
{
public:
    using RandomFn = std::function<int(int lo, int hi)>;

    ArpEngine();

    // Replaces the bank and restarts playback from step 0.
    void setBank(const Bank& bank);
    // Replaces the bank (e.g. after a step edit) without touching the playback position.
    void updateBank(const Bank& bank);
    // Live program parameters, applied without copying the whole bank (audio-thread safe).
    void setProgramParams(int programIndex, SyncRate sync, int gatePercent, int swingPercent,
                          int octaveShift, int numSteps);
    void setCurrentProgram(int index);
    void setCurrentChain(int chainIndex);
    void setLatch(bool on) { latch = on; }
    void setRandomFn(RandomFn fn) { randomFn = std::move(fn); }

    void noteOn(int note, int velocity);
    void noteOff(int note);
    void sustain(bool down);
    void panic();

    std::vector<EngineEvent> render(int numSamples, const HostPosition& host,
                                    const std::vector<EngineEvent>& thru);

    int getCurrentProgram() const { return currentProgram; }
    int getCurrentStep() const { return currentStep; }
    std::vector<HeldNote> getActiveInput() const;

private:
    std::vector<int> resolveChainPrograms() const;
    const Program& activeProgram() const;
    std::vector<HeldNote> orderedInput() const;
    double stepBars() const;
    double swingOffsetBars(int stepIndex) const;
    std::vector<int> pickStepNotes(const Program& prog, const Step& step,
                                   const std::vector<HeldNote>& input);

    void playOneStep(Program& prog, const std::vector<HeldNote>& input,
                     int at, int numSamples, double samplesPerQuarter);
    void scheduleOff(int note, double ppq);
    void flushOffs(double uptoPpq, double blockStartPpq, double samplesPerQuarter);
    void emitOnBuf(int note, int vel, int at);
    void emitOffBuf(int note, int at);
    void killAllBuf(int at);

    Bank bank;
    int currentProgram = 0;
    int pendingProgram = -1;
    int pendingChain = -2;
    double pendingChainAtPpq = 0.0;

    std::map<int, HeldNote> held;
    std::vector<HeldNote> latchedNotes;
    std::uint64_t orderCounter = 0;
    bool latch = false;
    bool sustainDown = false;
    std::set<int> sustainedNotes;

    double nextStepPpq = 0.0;
    bool positionInit = false;
    double freePpq = 0.0;
    int currentStep = 0;
    int stepsPlayedInProgram = 0;
    int chainStepPos = 0;

    std::map<int, int> sounding;
    std::map<int, double> pendingOffs; // note -> ppq at which the gate closes
    bool panicPending = false;
    std::vector<int> previousStepNotes;

    RandomFn randomFn;
    std::mt19937 rng { 12345u };
    std::vector<EngineEvent>* outBuf = nullptr;
    int outSamples = 0;
};

} // namespace mnArpee
