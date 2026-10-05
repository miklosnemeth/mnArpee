// mnArpee engine tests (JUCE-independent, plain console app).
// Build: enabled by the CMake option MNARPEE_BUILD_TESTS=ON.
// Run: build/Release/mnArpeeTests.exe

#include "ArpEngine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
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

mnArpee::HostPosition freeHost(double bpm = 120.0, double sr = 44100.0)
{
    mnArpee::HostPosition h;
    h.bpm = bpm;
    h.isPlaying = false;
    h.sampleRate = sr;
    return h;
}

std::vector<mnArpee::EngineEvent> runFor(mnArpee::ArpEngine& e, double seconds,
                                         mnArpee::HostPosition host, int block = 512)
{
    std::vector<mnArpee::EngineEvent> all;
    const int total = (int) (seconds * host.sampleRate);
    int left = total;
    while (left > 0)
    {
        const int n = left > block ? block : left;
        auto evs = e.render(n, host, {});
        all.insert(all.end(), evs.begin(), evs.end());
        left -= n;
    }
    return all;
}

int countOn(const std::vector<mnArpee::EngineEvent>& evs)
{
    int c = 0;
    for (const auto& e : evs)
        if (e.isNoteOn)
            ++c;
    return c;
}

// First note-on in absolute time (the per-block `at` is not monotonic!).
int firstNoteOn(const std::vector<mnArpee::EngineEvent>& evs)
{
    for (const auto& e : evs)
        if (e.isNoteOn)
            return e.note;
    return -1;
}

struct AbsEvent
{
    long long sample;
    mnArpee::EngineEvent ev;
};

// Like runFor, but with absolute sample positions across blocks.
std::vector<AbsEvent> runAbs(mnArpee::ArpEngine& e, double seconds, mnArpee::HostPosition host,
                             int block = 512)
{
    std::vector<AbsEvent> all;
    long long done = 0;
    const long long total = (long long) (seconds * host.sampleRate);
    while (done < total)
    {
        const int n = (int) std::min<long long>(block, total - done);
        for (const auto& ev : e.render(n, host, {}))
            all.push_back({ done + ev.sampleOffset, ev });
        done += n;
    }
    return all;
}

long long findAbs(const std::vector<AbsEvent>& evs, bool noteOn, int note, int nth = 0)
{
    for (const auto& a : evs)
        if (a.ev.isNoteOn == noteOn && a.ev.note == note && nth-- == 0)
            return a.sample;
    return -1;
}

mnArpee::Bank singleProgramBank(int numSteps, mnArpee::SyncRate rate, int gate)
{
    auto b = mnArpee::makeFactoryBank();
    b.programs[0].numSteps = numSteps;
    b.programs[0].sync = rate;
    b.programs[0].gatePercent = gate;
    return b;
}
} // namespace

int main()
{
    using namespace mnArpee;

    // 1. Default pattern: C-E-G chord, 16 x 1/16 = 4 quarters = 2s @120bpm.
    {
        ArpEngine e;
        e.noteOn(60, 100);
        e.noteOn(64, 100);
        e.noteOn(67, 100);
        auto evs = runFor(e, 2.0, freeHost());
        check(countOn(evs) >= 14, "up16-plays-16-steps");
        check(firstNoteOn(evs) == 60, "up16-first-note-is-lowest");
    }

    // 2. Empty input = silence.
    {
        ArpEngine e;
        auto evs = runFor(e, 1.0, freeHost());
        check(countOn(evs) == 0, "silence-without-input");
    }

    // 3. Custom pattern: k3, k1, k2 order (G, C, E from a C-E-G chord).
    {
        ArpEngine e;
        Bank b = makeFactoryBank();
        b.programs[0].numSteps = 3;
        b.programs[0].steps[0].keyIndex = 2;
        b.programs[0].steps[1].keyIndex = 0;
        b.programs[0].steps[2].keyIndex = 1;
        e.setBank(b);
        e.noteOn(64, 100);
        e.noteOn(67, 100);
        e.noteOn(60, 100);
        auto evs = runFor(e, 1.0, freeHost());
        std::vector<int> seq;
        for (const auto& ev : evs)
            if (ev.isNoteOn)
                seq.push_back(ev.note);
        check(seq.size() >= 6 && seq[0] == 67 && seq[1] == 60 && seq[2] == 64
                  && seq[3] == 67 && seq[4] == 60 && seq[5] == 64,
              "custom-pattern-key-order");
    }

    // 4. Chord step: both notes at once.
    {
        ArpEngine e;
        Bank b = makeFactoryBank();
        b.programs[0].numSteps = 2;
        b.programs[0].steps[0].type = StepType::Chord;
        b.programs[0].steps[1].type = StepType::Off;
        e.setBank(b);
        e.noteOn(60, 100);
        e.noteOn(64, 100);
        auto evs = runFor(e, 1.0, freeHost());
        int chordNotes = 0;
        for (const auto& ev : evs)
            if (ev.isNoteOn)
                ++chordNotes;
        check(chordNotes >= 2, "chord-step-plays-all");
    }

    // 5. Latch: keeps sounding after release.
    {
        ArpEngine e;
        e.setLatch(true);
        e.noteOn(60, 100);
        e.noteOff(60);
        auto evs = runFor(e, 1.0, freeHost());
        check(countOn(evs) > 0, "latch-keeps-playing");
    }

    // 6. Sync rate change: 1/8 yields half as many notes as 1/16.
    {
        ArpEngine e8, e16;
        Bank b = makeFactoryBank();
        b.programs[0].sync = SyncRate::R1_8;
        e8.setBank(b);
        e8.noteOn(60, 100);
        e16.noteOn(60, 100);
        auto ev8 = runFor(e8, 2.0, freeHost());
        auto ev16 = runFor(e16, 2.0, freeHost());
        check(countOn(ev16) >= countOn(ev8) * 1.5, "rate-16th-denser-than-8th");
    }

    // 7. Gate 50% at 1/8 (0.25s = 11025 samples): note-off after half a step.
    {
        ArpEngine e;
        e.setBank(singleProgramBank(2, SyncRate::R1_8, 50));
        e.noteOn(60, 100);
        auto evs = runAbs(e, 1.0, freeHost());
        check(findAbs(evs, true, 60) == 0, "gate-first-on-at-zero");
        check(std::abs(findAbs(evs, false, 60) - 5512) <= 1, "gate-50-percent-off-time");
    }

    // 8. Gate 125%: the first note is still sounding when the second starts.
    {
        ArpEngine e;
        auto b = singleProgramBank(2, SyncRate::R1_8, 125);
        b.programs[0].steps[1].keyIndex = 1;
        e.setBank(b);
        e.noteOn(60, 100);
        e.noteOn(64, 100);
        auto evs = runAbs(e, 1.0, freeHost());
        const auto on2 = findAbs(evs, true, 64);
        const auto off1 = findAbs(evs, false, 60);
        check(on2 == 11025 && std::abs(off1 - 13781) <= 1, "gate-125-overlaps-next-step");
    }

    // 9. Rest extends the previous note by one step (gate 50%: off at 1.5 steps).
    {
        ArpEngine e;
        auto b = singleProgramBank(4, SyncRate::R1_8, 50);
        b.programs[0].steps[1].type = StepType::Rest;
        b.programs[0].steps[2].type = StepType::Off;
        b.programs[0].steps[3].type = StepType::Off;
        e.setBank(b);
        e.noteOn(60, 100);
        auto evs = runAbs(e, 0.9, freeHost());
        check(std::abs(findAbs(evs, false, 60) - 16537) <= 1, "rest-extends-note");
    }

    // 10. Panic releases sounding notes.
    {
        ArpEngine e;
        e.setBank(singleProgramBank(2, SyncRate::R1_4, 100));
        e.noteOn(60, 100);
        runAbs(e, 0.1, freeHost());
        e.panic();
        auto evs = runAbs(e, 0.02, freeHost());
        check(findAbs(evs, false, 60) >= 0, "panic-sends-note-off");
    }

    // 11. Program switch works for a 1-step (pass-through) program.
    {
        ArpEngine e;
        e.setBank(singleProgramBank(1, SyncRate::R1_16, 80));
        e.setCurrentProgram(1);
        runAbs(e, 0.1, freeHost());
        check(e.getCurrentProgram() == 1, "program-switch-from-passthrough");
    }

    // 12. Latch: a new chord replaces the latched one.
    {
        ArpEngine e;
        e.setLatch(true);
        e.noteOn(60, 100);
        e.noteOff(60);
        e.noteOn(64, 100);
        e.noteOff(64);
        auto evs = runAbs(e, 0.5, freeHost());
        check(findAbs(evs, true, 64) >= 0 && findAbs(evs, true, 60) < 0, "latch-new-chord-replaces");
    }

    // 13. Live program params (no bank copy, no restart).
    {
        ArpEngine e;
        e.noteOn(60, 100);
        runAbs(e, 0.3, freeHost());
        const int stepBefore = e.getCurrentStep();
        e.setProgramParams(0, SyncRate::R1_8, 80, 0, 0, 16);
        check(e.getCurrentStep() == stepBefore, "params-do-not-reset-playback");
        const int n = countOn(runFor(e, 1.0, freeHost()));
        check(n >= 3 && n <= 5, "params-apply-live"); // 1/8 = 4 notes/s at 120 bpm
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
