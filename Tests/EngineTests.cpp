// mnArpee engine tests (JUCE-independent, plain console app).
// Build: enabled by the CMake option MNARPEE_BUILD_TESTS=ON.
// Run: build/Release/mnArpeeTests.exe

#include "ArpEngine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace mnArpee;

int failures = 0;
int checks = 0;

void check(bool cond, const std::string& name)
{
    ++checks;
    if (! cond)
    {
        ++failures;
        std::printf("FAIL: %s\n", name.c_str());
    }
    else
    {
        std::printf("ok: %s\n", name.c_str());
    }
}

constexpr double sr = 44100.0;
constexpr long long eighth = 11025;   // 1/8 at 120 bpm, in samples
constexpr long long sixteenth = 5512; // 1/16 at 120 bpm (rounded down)

HostPosition freeHost()
{
    HostPosition h;
    h.bpm = 120.0;
    h.isPlaying = false;
    h.sampleRate = sr;
    return h;
}

struct AbsEvent
{
    long long sample;
    EngineEvent ev;
};

// Renders `seconds` in blocks; events carry absolute sample positions. With a playing
// host the song position starts at startPpq and advances with the blocks.
struct Runner
{
    long long done = 0;
    bool playing = false;
    double startPpq = 0.0;

    std::vector<AbsEvent> run(ArpEngine& e, double seconds, int block = 512)
    {
        std::vector<AbsEvent> all;
        auto buf = std::make_unique<EventBuffer>();
        const long long total = (long long) (seconds * sr);
        for (long long pos = 0; pos < total;)
        {
            const int n = (int) std::min<long long>(block, total - pos);
            auto host = freeHost();
            host.isPlaying = playing;
            host.ppqPosition = startPpq + (double) done / (sr * 0.5);
            buf->clear();
            e.render(n, host, *buf);
            for (const auto& ev : *buf)
                all.push_back({ done + ev.sampleOffset, ev });
            pos += n;
            done += n;
        }
        return all;
    }
};

std::vector<AbsEvent> runFor(ArpEngine& e, double seconds, int block = 512)
{
    Runner r;
    return r.run(e, seconds, block);
}

int countOn(const std::vector<AbsEvent>& evs)
{
    return (int) std::count_if(evs.begin(), evs.end(), [](const AbsEvent& a) { return a.ev.isNoteOn; });
}

std::vector<int> noteOnSequence(const std::vector<AbsEvent>& evs)
{
    std::vector<int> seq;
    for (const auto& a : evs)
        if (a.ev.isNoteOn)
            seq.push_back(a.ev.note);
    return seq;
}

long long findAbs(const std::vector<AbsEvent>& evs, bool noteOn, int note, int nth = 0)
{
    for (const auto& a : evs)
        if (a.ev.isNoteOn == noteOn && a.ev.note == note && nth-- == 0)
            return a.sample;
    return -1;
}

bool near(long long a, long long b, long long tol = 1) { return std::llabs(a - b) <= tol; }

std::unique_ptr<Bank> singleProgramBank(int numSteps, SyncRate rate, int gate)
{
    auto b = std::make_unique<Bank>(makeFactoryBank());
    b->programs[0].numSteps = numSteps;
    b->programs[0].sync = rate;
    b->programs[0].gatePercent = gate;
    b->programs[0].swingPercent = 0;
    for (auto& s : b->programs[0].steps)
        s = Step {};
    return b;
}

// Every note-on is matched by a later note-off and nothing is left hanging.
bool balanced(const std::vector<AbsEvent>& evs)
{
    std::array<int, 128> open {};
    for (const auto& a : evs)
    {
        auto& o = open[(size_t) a.ev.note];
        o += a.ev.isNoteOn ? 1 : -1;
        if (o < 0 || o > 1)
            return false;
    }
    return std::all_of(open.begin(), open.end(), [](int o) { return o == 0; });
}
} // namespace

int main()
{
    // 1. Default pattern: C-E-G chord, 16 x 1/16 = 4 quarters = 2s @120bpm.
    {
        auto e = std::make_unique<ArpEngine>();
        e->noteOn(60, 100);
        e->noteOn(64, 100);
        e->noteOn(67, 100);
        auto evs = runFor(*e, 2.0);
        check(countOn(evs) >= 15, "factory-plays-16-steps");
        check(noteOnSequence(evs).front() == 60, "factory-first-note-is-lowest");
    }

    // 2. Empty input = silence.
    {
        auto e = std::make_unique<ArpEngine>();
        check(countOn(runFor(*e, 1.0)) == 0, "silence-without-input");
    }

    // 3. Custom pattern: k3, k1, k2 order (G, C, E from a C-E-G chord).
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(3, SyncRate::R1_16, 80);
        b->programs[0].steps[0].keyIndex = 2;
        b->programs[0].steps[2].keyIndex = 1;
        e->setBank(*b);
        e->noteOn(64, 100);
        e->noteOn(67, 100);
        e->noteOn(60, 100);
        auto seq = noteOnSequence(runFor(*e, 1.0));
        check(seq.size() >= 6 && seq[0] == 67 && seq[1] == 60 && seq[2] == 64
                  && seq[3] == 67 && seq[4] == 60 && seq[5] == 64,
              "custom-pattern-key-order");
    }

    // 4. Every step plays a single note, even with a full chord held.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setBank(*singleProgramBank(2, SyncRate::R1_8, 80));
        for (int n : { 60, 64, 67 })
            e->noteOn(n, 100);
        auto evs = runFor(*e, 0.1);
        check(countOn(evs) == 1 && findAbs(evs, true, 60) == 0, "step-plays-one-note");
    }

    // 5. Latch: keeps sounding after release; a new chord replaces the latched one.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setLatch(true);
        e->noteOn(60, 100);
        e->noteOff(60);
        check(countOn(runFor(*e, 0.5)) > 0, "latch-keeps-playing");
        e->noteOn(64, 100);
        e->noteOff(64);
        auto evs = runFor(*e, 0.5);
        check(findAbs(evs, true, 64) >= 0 && findAbs(evs, true, 60) < 0, "latch-new-chord-replaces");
        e->setLatch(false);
        auto after = runFor(*e, 0.5);
        check(countOn(after) == 0, "latch-off-releases-chord");
    }

    // 6. Sync rate: 1/8 yields half as many notes as 1/16.
    {
        auto e8 = std::make_unique<ArpEngine>();
        auto e16 = std::make_unique<ArpEngine>();
        e8->setBank(*singleProgramBank(16, SyncRate::R1_8, 80));
        e16->setBank(*singleProgramBank(16, SyncRate::R1_16, 80));
        e8->noteOn(60, 100);
        e16->noteOn(60, 100);
        check(countOn(runFor(*e8, 2.0)) * 2 == countOn(runFor(*e16, 2.0)), "rate-16th-twice-8th");
    }

    // 7. Gate 50% at 1/8: note-off after half a step.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setBank(*singleProgramBank(2, SyncRate::R1_8, 50));
        e->noteOn(60, 100);
        auto evs = runFor(*e, 1.0);
        check(findAbs(evs, true, 60) == 0, "gate-first-on-at-zero");
        check(near(findAbs(evs, false, 60), eighth / 2), "gate-50-percent-off-time");
    }

    // 8. Gate 125%: the first note is still sounding when the second starts.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(2, SyncRate::R1_8, 125);
        b->programs[0].steps[1].keyIndex = 1;
        e->setBank(*b);
        e->noteOn(60, 100);
        e->noteOn(64, 100);
        auto evs = runFor(*e, 1.0);
        check(findAbs(evs, true, 64) == eighth && near(findAbs(evs, false, 60), eighth * 5 / 4),
              "gate-125-overlaps-next-step");
    }

    // 9. Rest extends the previous note by one step (gate 50%: off at 1.5 steps).
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(4, SyncRate::R1_8, 50);
        b->programs[0].steps[1].type = StepType::Rest;
        b->programs[0].steps[2].type = StepType::Off;
        b->programs[0].steps[3].type = StepType::Off;
        e->setBank(*b);
        e->noteOn(60, 100);
        check(near(findAbs(runFor(*e, 0.9), false, 60), eighth * 3 / 2), "rest-extends-note");
    }

    // 10. Tie at gate 50%: the old note is held into the tie step and ends after the new one starts.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(2, SyncRate::R1_8, 50);
        b->programs[0].steps[1].type = StepType::Tie;
        b->programs[0].steps[1].keyIndex = 1;
        e->setBank(*b);
        e->noteOn(60, 100);
        e->noteOn(64, 100);
        auto evs = runFor(*e, 0.4);
        const auto on2 = findAbs(evs, true, 64);
        const auto off1 = findAbs(evs, false, 60);
        check(on2 == eighth && off1 == eighth + 1, "tie-legato-overlap");
    }

    // 11. Tie to the same note: no retrigger.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(2, SyncRate::R1_8, 50);
        b->programs[0].steps[1].type = StepType::Tie;
        e->setBank(*b);
        e->noteOn(60, 100);
        auto evs = runFor(*e, 0.4);
        check(findAbs(evs, true, 60, 1) < 0 && near(findAbs(evs, false, 60), eighth * 3 / 2),
              "tie-same-note-sustains");
    }

    // 12. Panic releases sounding notes.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setBank(*singleProgramBank(2, SyncRate::R1_4, 100));
        e->noteOn(60, 100);
        runFor(*e, 0.1);
        e->panic();
        check(findAbs(runFor(*e, 0.02), false, 60) >= 0, "panic-sends-note-off");
    }

    // 13. Quantized thru (1 step): released keys get a note-off.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setBank(*singleProgramBank(1, SyncRate::R1_16, 80));
        e->noteOn(60, 100);
        auto evs = runFor(*e, 0.2);
        e->noteOff(60);
        auto rel = runFor(*e, 0.2);
        evs.insert(evs.end(), rel.begin(), rel.end());
        check(countOn(evs) == 1 && balanced(evs), "quantized-thru-releases");
    }

    // 14. 0 steps = MIDI thru: the engine stays silent and reports thru.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setBank(*singleProgramBank(0, SyncRate::R1_16, 80));
        e->noteOn(60, 100);
        check(e->isThru() && countOn(runFor(*e, 0.5)) == 0, "zero-steps-is-thru");
    }

    // 15. Program switch works for a 1-step program and while idle.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setBank(*singleProgramBank(1, SyncRate::R1_16, 80));
        e->setCurrentProgram(1);
        runFor(*e, 0.1);
        check(e->getCurrentProgram() == 1, "program-switch-from-passthrough");
    }

    // 16. Live program params (no bank copy, no restart).
    {
        auto e = std::make_unique<ArpEngine>();
        e->noteOn(60, 100);
        runFor(*e, 0.3);
        const int stepBefore = e->getLastPlayedStep();
        e->setProgramParams(0, SyncRate::R1_8, 80, 0, 0, 16);
        check(e->getLastPlayedStep() == stepBefore, "params-do-not-reset-playback");
        const int n = countOn(runFor(*e, 1.0));
        check(n >= 3 && n <= 5, "params-apply-live"); // 1/8 = 4 notes/s at 120 bpm
    }

    // 17. Step edits (updateBank) keep an active chain running.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = std::make_unique<Bank>(makeFactoryBank());
        e->setBank(*b);
        e->setCurrentChain(1); // Chorus: 0, 4, 0, 5
        e->noteOn(60, 100);
        runFor(*e, 0.5);
        e->updateBank(*b);
        bool sawFive = false;
        for (int i = 0; i < 400 && ! sawFive; ++i)
        {
            runFor(*e, 0.05);
            sawFive = e->getCurrentProgram() == 5;
        }
        check(e->getCurrentChain() == 1 && sawFive, "chain-survives-update-bank");
    }

    // 18. Chain off returns to the selected program.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setCurrentProgram(2);
        e->setCurrentChain(1);
        e->noteOn(60, 100);
        runFor(*e, 0.5);
        e->setCurrentChain(-1);
        runFor(*e, 0.5);
        check(e->getCurrentChain() == -1 && e->getCurrentProgram() == 2, "chain-off-back-to-selected");
    }

    // 19. Playhead reports the step that sounded last.
    {
        auto e = std::make_unique<ArpEngine>();
        e->setBank(*singleProgramBank(4, SyncRate::R1_8, 50));
        e->noteOn(60, 100);
        runFor(*e, 0.1); // only step 0 has sounded
        check(e->getLastPlayedStep() == 0, "playhead-is-last-played-step");
    }

    // 20. Host sync: transport starting mid-step lands on the grid and on the right pattern step.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(4, SyncRate::R1_8, 50); // a step = half a quarter
        for (int i = 0; i < 4; ++i)
            b->programs[0].steps[(size_t) i].keyIndex = (std::int8_t) i;
        e->setBank(*b);
        for (int n : { 60, 62, 64, 65 })
            e->noteOn(n, 100);
        Runner r;
        r.playing = true;
        r.startPpq = 4.3; // grid points at 4.5 (step 9 -> pattern step 1), 5.0, ...
        auto evs = r.run(*e, 0.5);
        const long long expected = std::llround(0.2 * sr * 0.5); // 0.2 quarter later
        check(! evs.empty() && evs.front().ev.isNoteOn && near(evs.front().sample, expected)
                  && evs.front().ev.note == 62,
              "host-sync-on-grid");
    }

    // 21. Swing timing does not depend on the block size.
    {
        std::vector<long long> times[2];
        int idx = 0;
        for (int block : { 64, 4096 })
        {
            auto e = std::make_unique<ArpEngine>();
            auto b = singleProgramBank(4, SyncRate::R1_8, 30);
            b->programs[0].swingPercent = 40;
            e->setBank(*b);
            e->noteOn(60, 100);
            for (const auto& a : runFor(*e, 1.0, block))
                if (a.ev.isNoteOn)
                    times[idx].push_back(a.sample);
            ++idx;
        }
        check(times[0] == times[1] && times[0].size() >= 2
                  && near(times[0][1], eighth + eighth * 2 / 5),
              "swing-block-size-independent");
    }

    // 22. Negative swing pulls odd steps earlier.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(4, SyncRate::R1_8, 30);
        b->programs[0].swingPercent = -20;
        e->setBank(*b);
        e->noteOn(60, 100);
        check(near(findAbs(runFor(*e, 0.5), true, 60, 1), eighth - eighth / 5), "swing-negative");
    }

    // 23. Long run with random steps, chance and chains: note-ons and note-offs balance.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = std::make_unique<Bank>(makeFactoryBank());
        for (auto& p : b->programs)
            for (int i = 0; i < maxSteps; ++i)
                p.steps[(size_t) i].chance = (std::uint8_t) (i % 3 == 0 ? 60 : 100);
        e->setBank(*b);
        e->setCurrentChain(1);
        for (int n : { 48, 55, 60, 64, 67 })
            e->noteOn(n, 100);
        auto evs = runFor(*e, 8.0, 333);
        for (int n : { 48, 55, 60, 64, 67 })
            e->noteOff(n);
        auto tail = runFor(*e, 1.0, 333);
        evs.insert(evs.end(), tail.begin(), tail.end());
        check(countOn(evs) > 30 && balanced(evs), "long-run-balanced");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
