#pragma once

#include "PatternData.h"

#include <array>
#include <cstdint>
#include <functional>
#include <random>

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

// Fixed-capacity event list, so rendering never allocates.
class EventBuffer
{
public:
    static constexpr int capacity = 1024;

    void clear() noexcept { count = 0; }
    void push(const EngineEvent& e) noexcept
    {
        if (count < capacity)
            events[(size_t) count++] = e;
    }
    int size() const noexcept { return count; }
    bool empty() const noexcept { return count == 0; }
    const EngineEvent& operator[](int i) const noexcept { return events[(size_t) i]; }
    EngineEvent* begin() noexcept { return events.data(); }
    EngineEvent* end() noexcept { return events.data() + count; }
    const EngineEvent* begin() const noexcept { return events.data(); }
    const EngineEvent* end() const noexcept { return events.data() + count; }

private:
    std::array<EngineEvent, capacity> events {};
    int count = 0;
};

struct HeldNote
{
    int note = 60;
    int velocity = 96;
};

// Active input notes, sorted by ascending pitch.
struct InputNotes
{
    std::array<HeldNote, 128> notes {};
    int size = 0;

    bool empty() const noexcept { return size == 0; }
    bool contains(int note) const noexcept
    {
        for (int i = 0; i < size; ++i)
            if (notes[(size_t) i].note == note)
                return true;
        return false;
    }
};

// Root of the chord formed by the input notes (triads, sus, 7th chords, power chord;
// inversions included), returned as the lowest held note of that pitch class. Without a
// recognisable chord it is the lowest note. Returns -1 for empty input.
int chordRootNote(const InputNotes& input) noexcept;

//==============================================================================
// ArpEngine: plays the step patterns of a Bank. render() is allocation-free and
// intended for the audio thread; all setters are cheap and must be called from
// that same thread (or while it is not rendering).
//==============================================================================
class ArpEngine
{
public:
    using RandomFn = std::function<int(int lo, int hi)>;

    ArpEngine();

    // Replaces the bank and restarts playback from step 0.
    void setBank(const Bank& bank);
    // Replaces the bank (e.g. after a step edit) without touching the playback position.
    void updateBank(const Bank& bank);
    // Live program parameters, applied without copying the whole bank.
    void setProgramParams(int programIndex, SyncRate sync, int gatePercent, int swingPercent,
                          int octaveShift, int numSteps,
                          VelocityMode velocityMode = VelocityMode::Lane);
    // Selected program: switches at the end of the running pattern, or at once when idle.
    void setCurrentProgram(int index);
    // -1 = chain off. Switches on the bank's chain quantize grid.
    void setCurrentChain(int chainIndex);
    void setLatch(bool on);
    void setOutputChannel(int channel) { outChannel = channel < 1 ? 1 : (channel > 16 ? 16 : channel); }
    void setRandomFn(RandomFn fn) { randomFn = std::move(fn); }

    void noteOn(int note, int velocity);
    void noteOff(int note);
    void sustain(bool down);
    void panic();

    // Appends the events of the next numSamples samples to out (offsets relative to
    // this call, time-ordered, note-offs before note-ons at the same sample).
    void render(int numSamples, const HostPosition& host, EventBuffer& out);

    int getCurrentProgram() const { return currentProgram; }
    int getCurrentChain() const { return currentChain; }
    // Step index that sounded last, or -1 while idle.
    int getLastPlayedStep() const { return lastPlayedStep; }
    // True while the active program has 0 steps (MIDI thru).
    bool isThru() const { return activeProgram().numSteps <= 0; }
    InputNotes getActiveInput() const;
    const Bank& getBank() const { return bank; }

private:
    const Program& activeProgram() const;
    bool chainActive() const;
    int chainProgramAt(int pos) const;
    void collectInput(InputNotes& out) const;
    static double stepQuarters(const Program& prog);
    double swingQuarters(const Program& prog, long long index, double stepQ) const;
    double stepTime(const Program& prog, long long index, double stepQ) const;
    int sampleAt(double ppq) const;

    void resync(double ppq, bool hostSynced);
    void switchProgram(int index);
    void advanceProgram();
    void applyPendingChain(double blockEndPpq);
    void playOneStep(const Program& prog, const InputNotes& input, double t, double stepQ);
    void playQuantizedThru(const InputNotes& input, int at);

    void emitOn(int note, int vel, int at);
    void emitOff(int note, int at);
    void killAll(int at);
    void scheduleOff(int note, double ppq);
    void flushOffs(double uptoPpq);

    Bank bank;
    int currentProgram = 0;
    int selectedProgram = 0;
    int pendingProgram = -1;
    int currentChain = -1;
    int pendingChain = -2;              // -2 = no switch pending
    double pendingChainAtPpq = -1.0;
    int chainPos = 0;

    // Input state, indexed by MIDI note (velocity, 0 = not present).
    std::array<std::uint8_t, 128> heldVel {};
    std::array<std::uint8_t, 128> sustainedVel {};
    std::array<std::uint8_t, 128> latchedVel {};
    int heldCount = 0;
    bool latch = false;
    bool sustainDown = false;
    bool hadInput = false;

    // Timing: steps fall on gridOrigin + gridIndex * stepQ (+ swing on odd indices).
    bool positionInit = false;
    bool wasPlaying = false;
    double freePpq = 0.0;
    double gridOriginPpq = 0.0;
    long long gridIndex = 0;
    double lastStepQ = 0.0;
    int currentStep = 0;
    int stepsPlayedInProgram = 0;
    int lastPlayedStep = -1;

    // Output state.
    std::array<bool, 128> sounding {};
    std::array<bool, 128> hasPendingOff {};
    std::array<double, 128> pendingOffPpq {};
    bool panicPending = false;
    int outChannel = 1;

    // Per-render context.
    EventBuffer* outBuf = nullptr;
    int outSamples = 0;
    double blockStartPpq = 0.0;
    double samplesPerQuarter = 22050.0;
    InputNotes scratchInput;

    RandomFn randomFn;
    std::mt19937 rng { 12345u };
};

} // namespace mnArpee
