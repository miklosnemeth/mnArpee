// mnArpee engine tests (JUCE-independent, plain console app).
// Build: enabled by the CMake option MNARPEE_BUILD_TESTS=ON.
// Run: build/Release/mnArpeeTests.exe

#include "ArpEngine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
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

    // 11b. A Rest at the start of the pattern lengthens the last note of the previous round.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(4, SyncRate::R1_8, 50);
        b->programs[0].steps[0].type = StepType::Rest;
        e->setBank(*b);
        e->noteOn(60, 100);
        auto evs = runFor(*e, 1.3);
        // step 3 sounds at 3 eighths and lasts a half step plus the Rest of the next round
        check(near(findAbs(evs, false, 60, 2), eighth * 3 + eighth * 3 / 2), "rest-wraps-over-the-pattern-end");
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

    // 23. Long run with random steps and chance: note-ons and note-offs balance.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = std::make_unique<Bank>(makeFactoryBank());
        for (auto& p : b->programs)
            for (int i = 0; i < maxSteps; ++i)
                p.steps[(size_t) i].chance = (std::uint8_t) (i % 3 == 0 ? 60 : 100);
        e->setBank(*b);
        for (int n : { 48, 55, 60, 64, 67 })
            e->noteOn(n, 100);
        auto evs = runFor(*e, 8.0, 333);
        for (int n : { 48, 55, 60, 64, 67 })
            e->noteOff(n);
        auto tail = runFor(*e, 1.0, 333);
        evs.insert(evs.end(), tail.begin(), tail.end());
        check(countOn(evs) > 30 && balanced(evs), "long-run-balanced");
    }

    // 24. Chord root detection: inversions, 7th chords, symmetric chords, no chord.
    {
        const auto root = [](std::initializer_list<int> notes)
        {
            InputNotes in;
            for (int n : notes)
                in.notes[(size_t) in.size++] = HeldNote { n, 100 };
            return chordRootNote(in);
        };
        check(root({ 60, 64, 67 }) == 60, "root-c-major");
        check(root({ 52, 55, 60 }) == 60, "root-first-inversion");   // E G C -> C
        check(root({ 55, 60, 64 }) == 60, "root-second-inversion");  // G C E -> C
        check(root({ 57, 60, 64, 67 }) == 57, "root-am7-not-c6");     // A C E G -> A
        check(root({ 64, 67, 69, 72 }) == 69, "root-am7-inverted");   // E G A C -> A
        check(root({ 62, 66, 69, 72 }) == 62, "root-d7");
        check(root({ 59, 62, 65, 68 }) == 59, "root-dim7-uses-bass");
        check(root({ 48, 55 }) == 48, "root-power-chord");
        check(root({ 60, 61 }) == 60 && root({}) == -1, "root-fallback-lowest");
    }

    // 25. A Root step plays the chord root, in the octave it is held.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(2, SyncRate::R1_8, 50);
        b->programs[0].steps[0].keyIndex = rootKeyIndex;
        e->setBank(*b);
        for (int n : { 64, 67, 72 }) // E G C: C major, first inversion
            e->noteOn(n, 100);
        check(noteOnSequence(runFor(*e, 0.1)).front() == 72, "root-step-plays-root");
    }

    // 28. The same note in another octave is a key of its own.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(4, SyncRate::R1_8, 50);
        for (int i = 0; i < 4; ++i)
            b->programs[0].steps[(size_t) i].keyIndex = (std::int8_t) i;
        e->setBank(*b);
        for (int n : { 48, 60, 64, 67 }) // C3 C4 E4 G4: four keys
            e->noteOn(n, 100);
        const auto seq = noteOnSequence(runFor(*e, 1.1));
        check(seq.size() >= 5 && seq[0] == 48 && seq[1] == 60 && seq[2] == 64 && seq[3] == 67 && seq[4] == 48,
              "octave-doubles-are-keys-of-their-own");
    }

    // 29. More than five different notes: the keys are the lowest five, the rest is left out.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(6, SyncRate::R1_16, 50);
        for (int i = 0; i < 6; ++i)
            b->programs[0].steps[(size_t) i].keyIndex = (std::int8_t) std::min(i, 4);
        b->programs[0].steps[5].type = StepType::Random;
        e->setBank(*b);
        e->setRandomFn([](int, int hi) { return hi; }); // the last key
        for (int n : { 60, 62, 64, 65, 67, 69, 71 }) // C D E F G A B
            e->noteOn(n, 100);
        const auto seq = noteOnSequence(runFor(*e, 0.9));
        check(seq.size() >= 6 && seq[0] == 60 && seq[1] == 62 && seq[2] == 64 && seq[3] == 65 && seq[4] == 67
                  && seq[5] == 67,
              "only-five-keys-reach-the-matrix");
        bool leftOutPlayed = false;
        for (int n : seq)
            leftOutPlayed = leftOutPlayed || n == 69 || n == 71;
        check(! leftOutPlayed, "left-out-notes-do-not-play");

        InputNotes all, keys;
        all.size = 0;
        for (int n : { 48, 60, 64, 67, 71, 74, 77 }) // C3 C4 E4 G4 B4 D5 F5
            all.notes[(size_t) all.size++] = HeldNote{ n, 100 };
        reduceToKeys(all, keys);
        check(keys.size == 5 && keys.notes[0].note == 48 && keys.notes[4].note == 71
                  && keyUseOf(keys, 60) == KeyUse::Used && keyUseOf(keys, 71) == KeyUse::Used
                  && keyUseOf(keys, 74) == KeyUse::LeftOut && keyUseOf(keys, 77) == KeyUse::LeftOut,
              "key-use-classification");
    }

    // 30. A key that is not held (more keys in the pattern than notes held): the chord root plays.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(5, SyncRate::R1_16, 50);
        for (int i = 0; i < 5; ++i)
            b->programs[0].steps[(size_t) i].keyIndex = (std::int8_t) i;
        e->setBank(*b);
        for (int n : { 64, 67, 72 }) // E G C: C major, first inversion; Key 4 and Key 5 are missing
            e->noteOn(n, 100);
        const auto seq = noteOnSequence(runFor(*e, 0.9));
        check(seq.size() >= 5 && seq[0] == 64 && seq[1] == 67 && seq[2] == 72 && seq[3] == 72 && seq[4] == 72,
              "missing-keys-play-the-root");

        // Two notes that form no chord: the root falls back to the lowest note.
        auto e2 = std::make_unique<ArpEngine>();
        e2->setBank(*b);
        for (int n : { 60, 61 })
            e2->noteOn(n, 100);
        const auto seq2 = noteOnSequence(runFor(*e2, 0.9));
        check(seq2.size() >= 5 && seq2[0] == 60 && seq2[1] == 61 && seq2[2] == 60 && seq2[3] == 60 && seq2[4] == 60,
              "missing-keys-without-chord-play-the-lowest-note");
    }

    // 31. Factory presets: basslines play only the root, melodic ones use the held keys; every
    // preset has a name of its own and a musical style.
    {
        const auto patterns = makeFactoryPatterns();
        int basslines = 0, melodics = 0;
        bool bassOnlyRoot = true, melodicUsesKeys = true, stylesGiven = true, namesUnique = true;
        std::vector<std::string> names;
        for (const auto& pattern : patterns)
        {
            const std::string name = pattern.program.name.chars.data();
            namesUnique = namesUnique && std::find(names.begin(), names.end(), name) == names.end();
            names.push_back(name);
            stylesGiven = stylesGiven && pattern.styles != nullptr && pattern.styles[0] != 0;
            bool usesKeys = false;
            for (const auto& step : pattern.program.steps)
            {
                const bool playsNote = step.type == StepType::Normal || step.type == StepType::Tie;
                if (std::string(pattern.group) == "Bassline")
                    bassOnlyRoot = bassOnlyRoot && step.type != StepType::Random && (! playsNote || step.keyIndex == rootKeyIndex);
                else
                    usesKeys = usesKeys || step.type == StepType::Random || (playsNote && step.keyIndex != rootKeyIndex);
            }
            if (std::string(pattern.group) == "Bassline")
                ++basslines;
            else
            {
                ++melodics;
                melodicUsesKeys = melodicUsesKeys && usesKeys;
            }
        }
        check(basslines >= 12 && melodics >= 18, "factory-has-basslines-and-melodic-presets");
        check(bassOnlyRoot, "factory-basslines-play-only-the-root");
        check(melodicUsesKeys, "factory-melodic-presets-use-the-keys");
        check(stylesGiven && namesUnique, "factory-presets-have-styles-and-unique-names");
    }

    // 34. Chord names.
    {
        auto name = [](std::initializer_list<int> notes)
        {
            InputNotes in;
            for (int n : notes)
                in.notes[(size_t) in.size++] = HeldNote{ n, 100 };
            return chordName(detectChord(in));
        };
        check(name({ 60, 64, 67 }) == "C" && name({ 57, 60, 64 }) == "Am" && name({ 60, 64, 67, 71 }) == "Cmaj7",
              "chord-names-triads-and-sevenths");
        check(name({ 64, 67, 72 }) == "C/E" && name({ 55, 60, 64 }) == "C/G", "chord-names-show-inversions");
        check(name({ 48, 55 }) == "C5" && name({ 62, 65, 69, 72 }) == "Dm7" && name({ 60, 61 }).empty(),
              "chord-names-power-minor-seventh-and-none");
    }

    // 35. Pattern tools: set all, shift, reverse, randomize.
    {
        auto keysOf = [](const Program& p, int n)
        {
            std::vector<int> keys;
            for (int i = 0; i < n; ++i)
                keys.push_back(p.steps[(size_t) i].keyIndex);
            return keys;
        };
        auto base = makeInitProgram("Tools");
        base.numSteps = 4;
        for (int i = 0; i < maxSteps; ++i)
            base.steps[(size_t) i].keyIndex = (std::int8_t) (i % 4);

        auto left = base;
        shiftPattern(left, -1);
        auto right = base;
        shiftPattern(right, 1);
        auto reversed = base;
        reversePattern(reversed);
        check(keysOf(left, 4) == std::vector<int> { 1, 2, 3, 0 } && keysOf(right, 4) == std::vector<int> { 3, 0, 1, 2 }
                  && keysOf(reversed, 4) == std::vector<int> { 3, 2, 1, 0 },
              "pattern-shift-and-reverse-the-playing-steps");
        check(left.steps[5].keyIndex == base.steps[5].keyIndex && reversed.steps[7].keyIndex == base.steps[7].keyIndex,
              "pattern-tools-leave-steps-past-the-end-alone");

        auto all = base;
        setLaneValue(all, PatternLane::Octave, 2);
        setLaneValue(all, PatternLane::Velocity, 111);
        setLaneValue(all, PatternLane::Key, rootKeyIndex);
        bool allSet = true;
        for (const auto& s : all.steps)
            allSet = allSet && s.octave == 2 && s.velocity == 111 && s.keyIndex == rootKeyIndex && s.type == StepType::Normal;
        auto randomSteps = base;
        setLaneValue(randomSteps, PatternLane::Key, -2);
        check(allSet && randomSteps.steps[31].type == StepType::Random, "pattern-set-all-steps");

        std::mt19937 rng(7);
        auto random = base;
        randomizeLane(random, PatternLane::Velocity, rng);
        randomizeLane(random, PatternLane::Type, rng);
        bool inRange = random.steps[0].type == StepType::Normal;
        for (int i = 0; i < 4; ++i)
            inRange = inRange && random.steps[(size_t) i].velocity >= 60 && random.steps[(size_t) i].velocity <= 127;
        check(inRange && random.steps[4].velocity == base.steps[4].velocity && random.steps[4].type == base.steps[4].type,
              "pattern-randomize-the-playing-steps");
    }

    // 36. Ratchet: a step plays its note 2..4 times, evenly spread over the step.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(2, SyncRate::R1_8, 80);
        b->programs[0].steps[0].ratchet = 3;
        b->programs[0].steps[1].ratchet = 1;
        e->setBank(*b);
        e->noteOn(60, 100);
        Runner r;
        auto evs = r.run(*e, 1.1); // 1/8 at 120 bpm = 11025 samples per step
        std::vector<long long> ons;
        for (const auto& ev : evs)
            if (ev.ev.isNoteOn && ons.size() < 4)
                ons.push_back(ev.sample);
        check(ons.size() == 4 && ons[0] == 0 && near(ons[1], 3675, 2) && near(ons[2], 7350, 2) && near(ons[3], eighth, 2),
              "ratchet-spreads-notes-over-the-step");
        e->noteOff(60);
        const auto tail = r.run(*e, 0.6);
        evs.insert(evs.end(), tail.begin(), tail.end());
        check(balanced(evs), "ratchet-notes-are-balanced");

        // Every note of the ratchet is off before the next one starts.
        bool ordered = true;
        int sounding = 0;
        for (const auto& ev : evs)
        {
            sounding += ev.ev.isNoteOn ? 1 : -1;
            ordered = ordered && sounding >= 0 && sounding <= 1;
        }
        check(ordered, "ratchet-notes-do-not-overlap");
    }

    // 37. Generated key orders.
    {
        auto keys = [](GeneratorMode mode, int numKeys, int steps)
        {
            auto p = makeInitProgram("Gen");
            p.numSteps = steps;
            for (int i = 0; i < maxSteps; ++i)
                p.steps[(size_t) i].type = StepType::Rest; // proves that the generator makes them Normal
            std::mt19937 rng(3);
            generateKeys(p, mode, numKeys, rng);
            std::vector<int> out;
            for (int i = 0; i < steps; ++i)
                out.push_back(p.steps[(size_t) i].keyIndex);
            return std::make_pair(out, p);
        };
        check(keys(GeneratorMode::Up, 4, 8).first == std::vector<int> { 0, 1, 2, 3, 0, 1, 2, 3 }, "generate-up");
        check(keys(GeneratorMode::Down, 3, 6).first == std::vector<int> { 2, 1, 0, 2, 1, 0 }, "generate-down");
        check(keys(GeneratorMode::UpDown, 4, 12).first == std::vector<int> { 0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1 }, "generate-up-down");
        check(keys(GeneratorMode::DownUp, 4, 6).first == std::vector<int> { 3, 2, 1, 0, 1, 2 }, "generate-down-up");
        check(keys(GeneratorMode::Converge, 5, 5).first == std::vector<int> { 0, 4, 1, 3, 2 }, "generate-converge");
        const auto random = keys(GeneratorMode::Random, 3, 16);
        bool inRange = true;
        for (int k : random.first)
            inRange = inRange && k >= 0 && k <= 2;
        const auto up = keys(GeneratorMode::Up, 4, 8);
        check(inRange && up.second.steps[0].type == StepType::Normal && up.second.steps[8].type == StepType::Rest,
              "generate-random-stays-within-the-keys-and-leaves-other-steps");
    }

    // 38. A random bass line: root only, a strong first step, a few notes, different every time.
    {
        auto make = [](unsigned seed, int steps)
        {
            auto p = makeInitProgram("Bass");
            p.numSteps = steps;
            for (int i = 0; i < maxSteps; ++i)
            {
                p.steps[(size_t) i].type = StepType::Random; // proves that they are replaced
                p.steps[(size_t) i].keyIndex = 3;
            }
            std::mt19937 rng(seed);
            generateBassline(p, rng);
            return p;
        };
        bool rootOnly = true, firstStrong = true, enoughNotes = true;
        for (unsigned seed = 1; seed <= 40; ++seed)
        {
            const auto p = make(seed, 16);
            int notes = 0;
            for (int i = 0; i < 16; ++i)
            {
                const auto& s = p.steps[(size_t) i];
                const bool plays = s.type == StepType::Normal || s.type == StepType::Tie;
                notes += plays ? 1 : 0;
                rootOnly = rootOnly && s.type != StepType::Random && (! plays || s.keyIndex == rootKeyIndex);
            }
            firstStrong = firstStrong && p.steps[0].type == StepType::Normal && p.steps[0].velocity >= 110;
            enoughNotes = enoughNotes && notes >= 16 / 3;
        }
        check(rootOnly, "bassline-plays-only-the-root");
        check(firstStrong && enoughNotes, "bassline-starts-strong-and-is-not-too-sparse");
        const auto a = make(1, 16), b = make(2, 16);
        bool different = false;
        for (int i = 0; i < 16; ++i)
            different = different || a.steps[(size_t) i].type != b.steps[(size_t) i].type
                        || a.steps[(size_t) i].velocity != b.steps[(size_t) i].velocity;
        check(different && make(1, 16).steps[20].type == StepType::Random, "bassline-differs-and-leaves-steps-past-the-end");
    }

    // 39. The gate of a step (percent of the preset's gate): a short and a long note.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(2, SyncRate::R1_8, 50); // the preset gate: 50% of 11025 samples
        b->programs[0].steps[0].gateScalePercent = 50;
        b->programs[0].steps[1].gateScalePercent = 200;
        e->setBank(*b);
        e->noteOn(60, 100);
        const auto evs = runFor(*e, 0.6);
        std::vector<long long> ons, offs;
        for (const auto& ev : evs)
            (ev.ev.isNoteOn ? ons : offs).push_back(ev.sample);
        const long long on0 = ons.size() > 1 ? ons[0] : -1, on1 = ons.size() > 1 ? ons[1] : -1;
        const long long off0 = offs.size() > 1 ? offs[0] : -1, off1 = offs.size() > 1 ? offs[1] : -1;
        check(on0 == 0 && near(off0 - on0, eighth / 4, 2) && near(off1 - on1, eighth, 2), "step-gate-scales-the-note-length");
        check(nearestGateStep(100) == 18 && nearestGateStep(98) == 18 && nearestGateStep(103) == 19 && nearestGateStep(1000) == numGateSteps - 1
                  && nearestGateStep(5) == 0 && gateStepPercent(0) == 10 && gateStepPercent(numGateSteps - 1) == 400,
              "nearest-gate-step");

        auto all = makeInitProgram("Gates");
        setLaneValue(all, PatternLane::Gate, 150);
        std::mt19937 rng(11);
        auto random = all;
        randomizeLane(random, PatternLane::Gate, rng);
        bool onSteps = true;
        for (int i = 0; i < 16; ++i)
        {
            const int g = random.steps[(size_t) i].gateScalePercent;
            onSteps = onSteps && g >= 50 && g <= 150 && g % 5 == 0;
        }
        check(all.steps[31].gateScalePercent == 150 && onSteps, "gate-lane-set-all-and-random-use-the-gate-steps");
    }

    // 40. Random ranges and the reset of a lane.
    {
        auto base = makeInitProgram("Ranges");
        base.numSteps = 16;
        std::mt19937 rng(5);
        auto keys = base;
        randomizeLane(keys, PatternLane::Key, rng, { 1, 2 });
        auto octaves = base;
        randomizeLane(octaves, PatternLane::Octave, rng, { 2, 3 });
        auto velocities = base;
        randomizeLane(velocities, PatternLane::Velocity, rng, { 100, 100 });
        auto gates = base;
        randomizeLane(gates, PatternLane::Gate, rng, { 0, 1 });
        auto ratchets = base;
        randomizeLane(ratchets, PatternLane::Ratchet, rng, { 3, 4 });
        bool inRange = true;
        for (int i = 0; i < 16; ++i)
        {
            inRange = inRange && keys.steps[(size_t) i].keyIndex >= 1 && keys.steps[(size_t) i].keyIndex <= 2
                      && octaves.steps[(size_t) i].octave >= 2 && octaves.steps[(size_t) i].octave <= 3
                      && velocities.steps[(size_t) i].velocity == 100
                      && (gates.steps[(size_t) i].gateScalePercent == 10 || gates.steps[(size_t) i].gateScalePercent == 15)
                      && ratchets.steps[(size_t) i].ratchet >= 3 && ratchets.steps[(size_t) i].ratchet <= 4;
        }
        check(inRange, "random-values-stay-in-the-range");
        auto odd = base;
        randomizeLane(odd, PatternLane::Velocity, rng, { 90, 20 }); // an upside down range is fixed
        check(odd.steps[0].velocity == 90 || odd.steps[0].velocity >= 90, "random-range-minimum-above-maximum-is-fixed");
        check(odd.steps[20].velocity == 96, "random-leaves-steps-past-the-end");

        auto reset = keys;
        reset.steps[3].octave = 2;
        reset.steps[3].velocity = 20;
        reset.steps[3].gateScalePercent = 400;
        reset.steps[3].ratchet = 3;
        reset.steps[3].type = StepType::Tie;
        for (auto lane : { PatternLane::Key, PatternLane::Type, PatternLane::Octave, PatternLane::Velocity, PatternLane::Gate,
                           PatternLane::Ratchet })
            resetLane(reset, lane);
        check(reset.steps[3].keyIndex == 3 && reset.steps[5].keyIndex == 1 && reset.steps[3].type == StepType::Normal
                  && reset.steps[3].octave == 0 && reset.steps[3].velocity == 96 && reset.steps[3].gateScalePercent == 100
                  && reset.steps[3].ratchet == 1 && reset.steps[31].velocity == 96,
              "reset-lane-restores-the-defaults");
        bool defaultsInside = true;
        for (auto lane : { PatternLane::Key, PatternLane::Octave, PatternLane::Velocity, PatternLane::Gate, PatternLane::Ratchet })
        {
            const auto d = defaultRandomRange(lane), f = fullRandomRange(lane);
            defaultsInside = defaultsInside && d.lo >= f.lo && d.hi <= f.hi && d.lo <= d.hi;
        }
        check(defaultsInside, "default-random-ranges-are-inside-the-full-ranges");
    }

    // 40. Group octaves: one key per pitch class (the lowest), or a key for every held note.
    {
        InputNotes all, keys;
        all.size = 0;
        for (int n : { 48, 60, 64, 67, 71, 74, 77 }) // C3 C4 E4 G4 B4 D5 F5
            all.notes[(size_t) all.size++] = HeldNote{ n, 100 };
        reduceToKeys(all, keys, true);
        check(keys.size == 5 && keys.notes[0].note == 48 && keys.notes[1].note == 64 && keys.notes[4].note == 74
                  && keyUseOf(keys, 60, true) == KeyUse::Merged && keyUseOf(keys, 77, true) == KeyUse::LeftOut
                  && keyUseOf(keys, 64, true) == KeyUse::Used,
              "group-octaves-key-use");

        auto e = std::make_unique<ArpEngine>();
        e->setGroupOctaves(true);
        auto b = singleProgramBank(4, SyncRate::R1_8, 50);
        for (int i = 0; i < 4; ++i)
            b->programs[0].steps[(size_t) i].keyIndex = (std::int8_t) i;
        e->setBank(*b);
        for (int n : { 48, 60, 64, 67 }) // C3 C4 E4 G4: with grouping three keys, Key 4 plays the root
            e->noteOn(n, 100);
        const auto seq = noteOnSequence(runFor(*e, 0.9));
        check(seq.size() >= 4 && seq[0] == 48 && seq[1] == 64 && seq[2] == 67 && seq[3] == 48, "group-octaves-merges-the-doubles");
    }

    // 41. Cut on release: the sounding notes stop with the last key, not at the end of their gate.
    for (const bool cut : { false, true })
    {
        auto e = std::make_unique<ArpEngine>();
        e->setCutOnRelease(cut);
        auto b = singleProgramBank(4, SyncRate::R1_8, 125);
        b->programs[0].steps[0].gateScalePercent = 400; // a note that would sound for a long time
        e->setBank(*b);
        e->noteOn(60, 100);
        Runner r;
        r.run(*e, 0.1);
        e->noteOff(60);
        const auto tail = r.run(*e, 0.05);
        int offs = 0;
        for (const auto& ev : tail)
            offs += ev.ev.isNoteOn ? 0 : 1;
        check(cut ? offs == 1 : offs == 0, cut ? "cut-on-release-stops-the-note" : "release-without-cut-lets-the-note-ring");
    }

    // 42. Chance: a step that fails its roll is silent; the lane has 5 % steps.
    {
        auto e = std::make_unique<ArpEngine>();
        auto b = singleProgramBank(4, SyncRate::R1_8, 50);
        b->programs[0].steps[1].chance = 50;
        e->setBank(*b);
        e->setRandomFn([](int, int hi) { return hi; }); // the roll is 100: a step with a lower chance does not play
        e->noteOn(60, 100);
        check(noteOnSequence(runFor(*e, 0.9)).size() == 3, "step-chance-skips-a-failed-roll");

        auto p = makeInitProgram("Chances");
        setLaneValue(p, PatternLane::Chance, 50);
        std::mt19937 rng(5);
        auto q = p;
        randomizeLane(q, PatternLane::Chance, rng, { 10, 20 });
        bool ok = true;
        for (int i = 0; i < 16; ++i)
            ok = ok && q.steps[(size_t) i].chance >= 50 && q.steps[(size_t) i].chance <= 100 && q.steps[(size_t) i].chance % 5 == 0;
        resetLane(q, PatternLane::Chance);
        check(p.steps[31].chance == 50 && ok && q.steps[3].chance == 100 && nearestChanceStep(100) == numChanceSteps
                  && nearestChanceStep(0) == 1 && nearestChanceStep(47) == 9,
              "chance-lane-tools");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
