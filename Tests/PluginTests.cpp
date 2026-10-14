// mnArpee plugin tests: drive the real AudioProcessor (and editor) without a host.
// Build: CMake options MNARPEE_BUILD_TESTS=ON (and not MNARPEE_TESTS_ONLY).
// Run: ctest -C Release in the build folder, or build/Release/mnArpeePluginTests.exe
// On Linux the editor checks need an X display (e.g. xvfb-run); they are skipped without one.

#include "PluginProcessor.h"
#include "StepGrid.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace
{
int failures = 0;
int checks = 0;

void check(bool cond, const char* name)
{
    ++checks;
    if (! cond)
        ++failures;
    std::printf("%s: %s\n", cond ? "ok" : "FAIL", name);
}

constexpr double sampleRate = 44100.0;
constexpr int blockSize = 512;

struct NoteEvent
{
    long long time;
    bool on;
    int note;
    int channel;
};

// Feeds `in` in the first block and renders `blocks` blocks; returns the note events
// with absolute sample times (continuing `clock`).
std::vector<NoteEvent> run(MnArpeeAudioProcessor& p, int blocks, const juce::MidiBuffer& in = {},
                           long long* clock = nullptr)
{
    std::vector<NoteEvent> out;
    const long long base = clock != nullptr ? *clock : 0;
    juce::AudioBuffer<float> audio(0, blockSize);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)
            midi = in;
        p.processBlock(audio, midi);
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (msg.isNoteOnOrOff())
                out.push_back({ base + (long long) b * blockSize + meta.samplePosition, msg.isNoteOn(),
                                msg.getNoteNumber(), msg.getChannel() });
        }
    }
    if (clock != nullptr)
        *clock = base + (long long) blocks * blockSize;
    return out;
}

juce::MidiBuffer noteOn(int channel, int note, int at)
{
    juce::MidiBuffer m;
    m.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 100), at);
    return m;
}

juce::MidiBuffer noteOff(int channel, int note, int at)
{
    juce::MidiBuffer m;
    m.addEvent(juce::MidiMessage::noteOff(channel, note), at);
    return m;
}

void setParam(MnArpeeAudioProcessor& p, const char* id, float plainValue)
{
    auto* param = p.getApvts().getParameter(id);
    param->setValueNotifyingHost(param->convertTo0to1(plainValue));
    p.handlePendingUpdates();
}

int param(MnArpeeAudioProcessor& p, const char* id)
{
    return (int) std::lround(p.getApvts().getRawParameterValue(id)->load());
}

std::unique_ptr<MnArpeeAudioProcessor> makeProcessor()
{
    auto p = std::make_unique<MnArpeeAudioProcessor>();
    p->setRateAndBufferSizeDetails(sampleRate, blockSize);
    p->prepareToPlay(sampleRate, blockSize);
    return p;
}

bool haveDisplay()
{
   #if JUCE_LINUX || JUCE_BSD
    return std::getenv("DISPLAY") != nullptr;
   #else
    return true;
   #endif
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // MIDI timing, channel and live parameters.
    {
        auto p = makeProcessor();
        auto ev = run(*p, 20, noteOn(3, 60, 100));
        check(! ev.empty() && ev[0].on && ev[0].time == 100 && ev[0].channel == 3,
              "sample-accurate-start-and-channel");

        setParam(*p, "gate", 50);
        setParam(*p, "steps", 4);
        run(*p, 50, noteOff(3, 60, 0));
        auto ev2 = run(*p, 20, noteOn(1, 62, 0));
        check(ev2.size() >= 2 && ev2[1].time - ev2[0].time == 2756, "gate-param-live"); // 1/16 at 50%
    }

    // Programs: switching loads the program's own parameters and keeps edits.
    {
        auto p = makeProcessor();
        setParam(*p, "gate", 50);
        setParam(*p, "program", 2); // factory "Bass 8ths": 1/8, gate 90
        check(param(*p, "rate") == 6 && param(*p, "gate") == 90, "program-switch-loads-params");
        setParam(*p, "program", 1);
        check(param(*p, "gate") == 50, "program-switch-keeps-edits");
        check(p->getProgramName(1) == "Bass 8ths" && p->getNumPrograms() == 8, "host-program-list");
    }

    // State: save and load into a new instance.
    {
        auto p = makeProcessor();
        auto st = p->getProgramCopy(0).steps[1];
        st.type = mnArpee::StepType::Tie;
        st.keyIndex = 3;
        p->setStep(0, 1, st);
        setParam(*p, "gate", 50);
        setParam(*p, "steps", 4);
        setParam(*p, "swing", 25);
        juce::MemoryBlock state;
        p->getStateInformation(state);

        auto q = makeProcessor();
        q->setStateInformation(state.getData(), (int) state.getSize());
        q->handlePendingUpdates();
        const auto prog = q->getProgramCopy(0);
        check(prog.steps[1].type == mnArpee::StepType::Tie && prog.steps[1].keyIndex == 3
                  && prog.gatePercent == 50 && prog.swingPercent == 25 && prog.numSteps == 4,
              "state-roundtrip");
        check(q->getProgramCopy(1).gatePercent == 90 && q->getProgramName(1) == "Bass 8ths",
              "state-other-programs");

        const char garbage[] = "definitely not a plugin state";
        q->setStateInformation(garbage, (int) sizeof(garbage));
        check(q->getProgramCopy(0).steps[1].type == mnArpee::StepType::Tie, "state-garbage-ignored");

        // States from before the 32-step limit (64 steps per program; "AMN2" also has a chain section).
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), (int) state.getSize());
        auto legacyBank = [](int magic, bool withChains)
        {
            juce::MemoryOutputStream o;
            o.writeInt(magic);
            o.writeInt(2); // programs
            for (int pi = 0; pi < 2; ++pi)
            {
                o.writeString("Legacy");
                o.writeInt(40); // more than 32 steps
                o.writeInt(4);  // rate
                o.writeInt(pi == 0 ? 55 : 90);
                o.writeInt(0);
                o.writeInt(0);
                o.writeInt(0);
                for (int si = 0; si < 64; ++si)
                {
                    o.writeByte(pi == 0 && si == 1 ? 3 : 1); // Tie / Normal
                    o.writeByte((char) (si % 3));
                    o.writeByte(0);
                    o.writeByte(0);
                    o.writeByte((char) (si < 32 ? 96 : 17)); // the steps past 32 are dropped
                    o.writeShort(100);
                    o.writeByte(100);
                }
            }
            if (withChains)
            {
                o.writeInt(1);          // one chain
                o.writeString("Verse"); // name
                o.writeInt(2);          // two programs
                o.writeByte(0);
                o.writeByte(1);
                o.writeDouble(0.25);    // chain quantize grid
                o.writeBool(true);      // restart on switch
            }
            return o.getMemoryBlock();
        };
        for (const bool withChains : { true, false })
        {
            xml->setAttribute("bankData", legacyBank(withChains ? 0x324e4d41 : 0x334e4d41, withChains).toBase64Encoding());
            juce::MemoryBlock legacyState;
            juce::AudioProcessor::copyXmlToBinary(*xml, legacyState);
            auto r = makeProcessor();
            r->setStateInformation(legacyState.getData(), (int) legacyState.getSize());
            r->handlePendingUpdates();
            const auto lp = r->getProgramCopy(0);
            // (program 0 takes its numeric values from the parameters, so program 1 shows the stored ones)
            check(lp.steps[1].type == mnArpee::StepType::Tie && r->getProgramCopy(1).numSteps == 32
                      && lp.steps[31].velocity == 96 && r->getProgramCopy(1).gatePercent == 90,
                  withChains ? "state-with-chains-still-loads" : "state-with-64-steps-still-loads");
        }
    }

    // The pattern is at most 32 steps long.
    {
        auto p = makeProcessor();
        setParam(*p, "steps", 64);
        check(param(*p, "steps") == 32, "steps-max-32");
    }

    // Steps 0 = MIDI thru: notes pass unchanged and in time.
    {
        auto p = makeProcessor();
        setParam(*p, "steps", 0);
        auto on = run(*p, 2, noteOn(2, 64, 10));
        auto off = run(*p, 2, noteOff(2, 64, 5));
        check(on.size() == 1 && on[0].time == 10 && on[0].channel == 2 && off.size() == 1 && ! off[0].on,
              "steps-zero-midi-thru");

        // Leaving thru mode releases the notes that went through.
        run(*p, 2, noteOn(2, 65, 0));
        setParam(*p, "steps", 16);
        auto rel = run(*p, 2);
        check(! rel.empty() && ! rel[0].on && rel[0].note == 65, "thru-notes-released-on-mode-change");
    }

    // Controller messages pass through; all-notes-off stops the arp.
    {
        auto p = makeProcessor();
        setParam(*p, "gate", 125); // overlapping gates: a note is always sounding
        run(*p, 10, noteOn(1, 60, 0));
        juce::MidiBuffer m;
        m.addEvent(juce::MidiMessage::controllerEvent(1, 1, 64), 3);
        m.addEvent(juce::MidiMessage::allNotesOff(1), 7);
        juce::AudioBuffer<float> audio(0, blockSize);
        p->processBlock(audio, m);
        bool cc = false, off = false;
        for (const auto meta : m)
        {
            cc = cc || (meta.getMessage().isController() && meta.samplePosition == 3);
            off = off || (meta.getMessage().isNoteOff() && meta.getMessage().getNoteNumber() == 60);
        }
        check(cc && off && run(*p, 20).empty(), "cc-thru-and-all-notes-off");
    }

    // Matrix lanes: row <-> value mapping.
    {
        using mnArpee::StepType;
        bool ok = StepGrid::typeForRow(0) == StepType::Normal && StepGrid::typeForRow(4) == StepType::Off
                  && StepGrid::octaveForRow(0) == 3 && StepGrid::octaveForRow(6) == -3
                  && StepGrid::velocityForRow(0) == 127 && StepGrid::velocityForRow(StepGrid::velocityLevels - 1) < 20;
        for (int r = 1; r < StepGrid::velocityLevels; ++r)
            ok = ok && StepGrid::velocityForRow(r) < StepGrid::velocityForRow(r - 1);
        check(ok && StepGrid::rowsFor(StepGrid::Lane::Key) == 7, "matrix-lane-values");
    }

    // Editor: opens, follows a state load without dangling data.
    if (haveDisplay())
    {
        auto p = makeProcessor();
        std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
        editor->setSize(900, 560);
        run(*p, 30, noteOn(1, 60, 0));
        const auto before = editor->createComponentSnapshot(editor->getLocalBounds());
        juce::MemoryBlock state;
        p->getStateInformation(state);
        p->setStateInformation(state.getData(), (int) state.getSize());
        editor->repaint();
        const auto after = editor->createComponentSnapshot(editor->getLocalBounds());
        check(before.isValid() && after.isValid(), "editor-survives-state-load");
        // The header value bars change from vertical mouse movement only (STEPS is the last slider).
        {
            juce::Slider* steps = nullptr;
            for (auto* c : editor->getChildren())
                if (auto* sl = dynamic_cast<juce::Slider*>(c))
                    steps = sl;
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            auto ev = [&](juce::Point<float> pos, juce::Point<float> down)
            {
                return juce::MouseEvent(src, pos, juce::ModifierKeys::leftButtonModifier, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f,
                                        steps, steps, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, true);
            };
            const auto start = steps->getValue();
            const juce::Point<float> a(60.0f, 15.0f);
            steps->mouseDown(ev(a, a));
            const auto afterClick = steps->getValue();
            steps->mouseDrag(ev({ 200.0f, 15.0f }, a));
            const auto sideways = steps->getValue();
            steps->mouseDrag(ev({ -40.0f, 1.0f }, a));
            const auto up = steps->getValue();
            steps->mouseDrag(ev({ 250.0f, 1.0f }, a));
            const auto upMovedSideways = steps->getValue();
            steps->mouseUp(ev({ 250.0f, 1.0f }, a));
            check(afterClick == start && sideways == start && up > start && upMovedSideways == up,
                  "value-bar-vertical-drag-only");
        }
    }
    else
    {
        std::printf("skip: editor checks (no display)\n");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
