// mnArpee motor tesztek (JUCE-fuggetlen, sima console app).
// Forditas: a CMake MNARPEE_BUILD_TESTS=ON kapcsoloja.
// Futtatas: build/Release/mnArpeeTests.exe

#include "ArpEngine.h"

#include <cstdio>
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

// Abszolut ido szerinti elso note-on (a blokkonkenti `at` nem monoton!).
int firstNoteOn(const std::vector<mnArpee::EngineEvent>& evs)
{
    for (const auto& e : evs)
        if (e.isNoteOn)
            return e.note;
    return -1;
}
} // namespace

int main()
{
    using namespace mnArpee;

    // 1. Alap Up arp: C-E-G akkord, 16 x 1/16 ~ 4 negyed = 2s @120bpm.
    {
        ArpEngine e;
        e.noteOn(60, 100);
        e.noteOn(64, 100);
        e.noteOn(67, 100);
        auto evs = runFor(e, 2.0, freeHost());
        check(countOn(evs) >= 14, "up16-plays-16-steps");
        check(firstNoteOn(evs) == 60, "up16-first-note-is-lowest");
    }

    // 2. Ures input = csend.
    {
        ArpEngine e;
        auto evs = runFor(e, 1.0, freeHost());
        check(countOn(evs) == 0, "silence-without-input");
    }

    // 3. Egyedi pattern: k3, k1, k2 sorrend (C-E-G akkordbol G, C, E).
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

    // 4. Chord step: mindket hang egyszerre.
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

    // 5. Latch: elengedes utan is szol.
    {
        ArpEngine e;
        e.setLatch(true);
        e.noteOn(60, 100);
        e.noteOff(60);
        auto evs = runFor(e, 1.0, freeHost());
        check(countOn(evs) > 0, "latch-keeps-playing");
    }

    // 6. Sync rate valtas: 1/8 feleannyi note mint 1/16.
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

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
