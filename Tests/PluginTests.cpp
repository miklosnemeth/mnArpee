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

// Library index of the preset with this name (-1 when there is none).
int presetIndex(MnArpeeAudioProcessor& p, const char* name)
{
    const auto list = p.getPresetList();
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].name == name)
            return i;
    return -1;
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

    // The presets of these tests go to a folder of their own, not to the user's Documents.
    const auto presetDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("mnArpeeTests-" + juce::String(juce::Time::currentTimeMillis()));
   #if JUCE_WINDOWS
    _putenv_s("MNARPEE_PRESET_DIR", presetDir.getFullPathName().toRawUTF8());
   #else
    setenv("MNARPEE_PRESET_DIR", presetDir.getFullPathName().toRawUTF8(), 1);
   #endif

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

    // Presets: the library, loading, unsaved changes.
    {
        auto p = makeProcessor();
        const auto list = p->getPresetList();
        check(list.size() >= 8 && list[0].factory && list[0].name == "Trance Up 16", "factory-presets-listed");
        const int bass = presetIndex(*p, "Bass 8ths");
        const int upDown = presetIndex(*p, "Up-Down Pad");
        check(bass > 0 && upDown > 0 && p->getNumPrograms() == 128 && p->getProgramName(bass) == "Bass 8ths"
                  && p->getProgramName(100) == "-",
              "host-program-list-mirrors-presets");
        check(! p->getActivePreset().dirty && p->getActivePresetIndex() == 0, "first-preset-active-and-clean");

        check(p->loadPreset(bass), "load-preset");
        check(param(*p, "rate") == 6 && param(*p, "gate") == 85 && p->getActivePreset().name == "Bass 8ths"
                  && ! p->getActivePreset().dirty,
              "load-preset-loads-params");
        setParam(*p, "gate", 50);
        check(p->getActivePreset().dirty, "param-edit-marks-preset-dirty");
        check(! p->saveActivePreset(), "factory-preset-cannot-be-overwritten");
        p->revertActivePreset();
        check(param(*p, "gate") == 85 && ! p->getActivePreset().dirty, "revert-restores-preset");

        // The host's program change loads a preset too.
        p->setCurrentProgram(upDown);
        p->handlePendingUpdates();
        check(p->getActivePreset().name == "Up-Down Pad" && p->getCurrentProgram() == upDown, "host-program-change-loads-preset");
    }

    // Presets: save as, save, rename, duplicate, export/import, delete.
    {
        auto p = makeProcessor();
        const auto before = p->getPresetList().size();
        p->newPreset();
        check(p->getActivePreset().name == "New Arp" && p->getActivePreset().dirty
                  && p->getActivePreset().file == juce::File(),
              "new-preset-is-unsaved");
        check(! p->saveActivePresetAs("Bass 8ths") && ! p->saveActivePresetAs("  "), "save-as-rejects-used-or-empty-name");
        setParam(*p, "gate", 33);
        auto st = p->getProgramCopy().steps[2];
        st.type = mnArpee::StepType::Tie;
        st.octave = 2;
        p->setStep(2, st);
        check(p->saveActivePresetAs("My Test Arp"), "save-as");
        const auto saved = p->getActivePreset();
        check(saved.file.existsAsFile() && saved.file.getParentDirectory() == p->getUserPresetFolder()
                  && ! saved.dirty && p->getPresetList().size() == before + 1,
              "save-as-writes-user-file");

        st.velocity = 77;
        p->setStep(3, st);
        check(p->getActivePreset().dirty && p->saveActivePreset() && ! p->getActivePreset().dirty, "save-overwrites-file");

        // Loading it from the list gives the same pattern back.
        int index = -1;
        const auto list = p->getPresetList();
        for (int i = 0; i < (int) list.size(); ++i)
            if (list[(size_t) i].name == "My Test Arp")
                index = i;
        check(index >= 0 && list[(size_t) index].program.gatePercent == 33
                  && list[(size_t) index].program.steps[2].type == mnArpee::StepType::Tie
                  && list[(size_t) index].program.steps[2].octave == 2 && list[(size_t) index].program.steps[3].velocity == 77,
              "saved-preset-keeps-the-pattern");

        // Rename (the file follows).
        check(p->renamePreset(index, "My Renamed Arp") && p->getActivePreset().name == "My Renamed Arp"
                  && p->getActivePreset().file.getFileName() == "My Renamed Arp.xml" && ! p->getActivePreset().dirty,
              "rename-preset");
        check(! p->renamePreset(0, "Whatever"), "factory-preset-cannot-be-renamed");

        // Export, import (gets a free name), duplicate.
        const auto exported = p->getUserPresetFolder().getSiblingFile("exported.xml");
        check(p->exportActivePreset(exported) && exported.existsAsFile(), "export-preset");
        check(p->importPresets(exported) == 1 && p->getActivePreset().name == "My Renamed Arp 2", "import-preset-gets-free-name");
        const int copy = p->duplicatePreset(p->getActivePresetIndex());
        check(copy >= 0 && p->getPresetList()[(size_t) copy].name == "My Renamed Arp 2 copy", "duplicate-preset");
        exported.deleteFile();

        // Delete: the active preset stays as an unsaved one.
        const auto filesBefore = p->getUserPresetFolder().getNumberOfChildFiles(juce::File::findFiles, "*.xml");
        check(p->deletePreset(p->getActivePresetIndex())
                  && p->getUserPresetFolder().getNumberOfChildFiles(juce::File::findFiles, "*.xml") == filesBefore - 1
                  && p->getActivePreset().file == juce::File() && p->getActivePreset().dirty,
              "delete-preset");
    }

    // Styles: factory presets have them, they can be edited, and they stay with saved and exported presets.
    {
        auto p = makeProcessor();
        const auto list = p->getPresetList();
        bool factoryStyled = ! list.empty();
        bool groupsKnown = true;
        for (const auto& preset : list)
            if (preset.factory)
            {
                factoryStyled = factoryStyled && mnArpee::presetStyles(preset).size() >= 1;
                groupsKnown = groupsKnown && (preset.group == "Bassline" || preset.group == "Melodic");
            }
        check(factoryStyled && groupsKnown, "factory-presets-have-group-and-styles");

        p->loadPreset(presetIndex(*p, "Trance Up 16"));
        check(p->getActivePreset().tags == "Trance" && ! p->getActivePreset().dirty, "style-of-loaded-preset");
        p->setActivePresetStyles("Techno, Acid");
        check(p->getActivePreset().dirty, "style-edit-is-an-unsaved-change");
        check(p->saveActivePresetAs("Styled Arp"), "save-as-with-styles");
        const int saved = presetIndex(*p, "Styled Arp");
        check(saved >= 0 && p->getPresetList()[(size_t) saved].tags == "Techno, Acid"
                  && mnArpee::presetStyles(p->getPresetList()[(size_t) saved]).size() == 2
                  && p->getPresetList()[(size_t) saved].group == "User",
              "styles-are-saved-with-the-preset");

        const auto file = p->getUserPresetFolder().getSiblingFile("styled.xml");
        p->exportActivePreset(file);
        p->deletePreset(saved);
        check(p->importPresets(file) == 1 && p->getActivePreset().tags == "Techno, Acid", "styles-survive-export-and-import");
        file.deleteFile();
    }

    // Favourites (stars) and replacing all steps.
    {
        auto p = makeProcessor();
        const int up = presetIndex(*p, "Up 8ths");
        check(! p->getPresetList()[(size_t) up].favorite, "preset-is-not-a-favourite-at-first");
        p->setPresetFavorite(up, true);
        check(p->getPresetList()[(size_t) up].favorite, "set-favourite");
        auto other = makeProcessor(); // another instance reads the same file
        check(other->getPresetList()[(size_t) presetIndex(*other, "Up 8ths")].favorite, "favourites-are-shared-and-kept");

        // A user preset keeps its star when it is renamed.
        p->newPreset();
        check(p->saveActivePresetAs("Starred Arp"), "save-starred");
        const int starred = presetIndex(*p, "Starred Arp");
        p->setPresetFavorite(starred, true);
        check(p->renamePreset(starred, "Starred Arp 2")
                  && p->getPresetList()[(size_t) presetIndex(*p, "Starred Arp 2")].favorite,
              "favourite-survives-rename");
        p->setPresetFavorite(presetIndex(*p, "Starred Arp 2"), false);
        p->setPresetFavorite(up, false);
        check(! p->getPresetList()[(size_t) up].favorite, "clear-favourite");

        std::array<mnArpee::Step, mnArpee::maxSteps> steps {};
        for (auto& s : steps)
            s.octave = 2;
        p->setAllSteps(steps);
        check(p->getProgramCopy().steps[10].octave == 2 && p->getActivePreset().dirty, "set-all-steps-replaces-the-pattern");
    }

    // The ratchet of a step is saved with the preset.
    {
        auto p = makeProcessor();
        p->newPreset();
        auto st = p->getProgramCopy().steps[2];
        st.ratchet = 3;
        p->setStep(2, st);
        check(p->saveActivePresetAs("Ratchet Arp"), "save-ratchet-preset");
        const int index = presetIndex(*p, "Ratchet Arp");
        check(index >= 0 && p->getPresetList()[(size_t) index].program.steps[2].ratchet == 3
                  && p->getPresetList()[(size_t) index].program.steps[1].ratchet == 1,
              "ratchet-is-saved-in-the-preset");
    }

    // Undo / redo of the step edits.
    {
        auto p = makeProcessor();
        check(! p->canUndo() && ! p->canRedo(), "no-undo-at-first");
        auto st = p->getProgramCopy().steps[2];
        st.octave = 1;
        p->setStep(2, st);
        st.octave = 2; // a burst of edits: one undo step
        p->setStep(2, st);
        check(p->canUndo() && p->getProgramCopy().steps[2].octave == 2, "edit-can-be-undone");
        check(p->undo() && p->getProgramCopy().steps[2].octave == 0 && p->canRedo(), "undo-restores-the-steps-before-the-burst");
        check(p->redo() && p->getProgramCopy().steps[2].octave == 2 && ! p->canRedo(), "redo-brings-the-edit-back");

        juce::Thread::sleep(700); // a later edit is a step of its own
        st.octave = -1;
        p->setStep(2, st);
        p->undo();
        check(p->getProgramCopy().steps[2].octave == 2, "later-edit-is-its-own-undo-step");
        juce::Thread::sleep(700);
        auto other = p->getProgramCopy().steps[3];
        other.velocity = 33;
        p->setStep(3, other);
        check(! p->canRedo(), "a-new-edit-clears-redo");
        p->loadPreset(presetIndex(*p, "Up 8ths"));
        check(! p->canUndo() && ! p->canRedo(), "preset-load-starts-a-new-history");

        std::array<mnArpee::Step, mnArpee::maxSteps> steps {};
        for (auto& step : steps)
            step.ratchet = 2;
        p->setAllSteps(steps);
        check(p->undo() && p->getProgramCopy().steps[5].ratchet == 1, "set-all-steps-can-be-undone");
    }

    // The zoom is kept with the project and applied to the editor.
    {
        auto p = makeProcessor();
        p->setZoom(1.25f);
        juce::MemoryBlock state;
        p->getStateInformation(state);
        auto q = makeProcessor();
        q->setStateInformation(state.getData(), (int) state.getSize());
        check(std::abs(q->getZoom() - 1.25f) < 0.001f, "zoom-is-saved-in-the-state");
        if (haveDisplay())
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed(q->createEditor());
            check(std::abs(ed->getTransform().getScaleFactor() - 1.25f) < 0.01f, "editor-uses-the-zoom");
        }
    }

    // State: the project stores the active preset's content.
    {
        auto p = makeProcessor();
        const int bassIndex = presetIndex(*p, "Bass 8ths");
        p->loadPreset(bassIndex);
        setParam(*p, "gate", 50);
        setParam(*p, "steps", 4);
        auto st = p->getProgramCopy().steps[1];
        st.type = mnArpee::StepType::Tie;
        st.keyIndex = 3;
        p->setStep(1, st);
        setParam(*p, "swing", 25);
        juce::MemoryBlock state;
        p->getStateInformation(state);

        auto q = makeProcessor();
        q->setStateInformation(state.getData(), (int) state.getSize());
        const auto prog = q->getProgramCopy();
        const auto a = q->getActivePreset();
        check(prog.steps[1].type == mnArpee::StepType::Tie && prog.steps[1].keyIndex == 3
                  && prog.gatePercent == 50 && prog.swingPercent == 25 && prog.numSteps == 4,
              "state-roundtrip");
        check(a.name == "Bass 8ths" && a.factory && a.dirty && q->getActivePresetIndex() == bassIndex,
              "state-rebinds-preset-and-keeps-unsaved-changes");

        // A preset the library does not have comes back as an unsaved one.
        auto r = makeProcessor();
        r->newPreset();
        r->saveActivePresetAs("Only On This Machine");
        juce::MemoryBlock otherState;
        r->getStateInformation(otherState);
        r->deletePreset(r->getActivePresetIndex());
        auto s = makeProcessor();
        s->setStateInformation(otherState.getData(), (int) otherState.getSize());
        check(s->getActivePreset().name == "Only On This Machine" && s->getActivePreset().file == juce::File()
                  && s->getActivePreset().dirty,
              "state-with-unknown-preset-is-unsaved");

        const char garbage[] = "definitely not a plugin state";
        q->setStateInformation(garbage, (int) sizeof(garbage));
        check(q->getProgramCopy().steps[1].type == mnArpee::StepType::Tie, "state-garbage-ignored");

        // States from before the presets (a bank of programs; 64 steps per program before the
        // 32-step limit, "AMN2" with a chain section): the selected program comes back as an
        // unsaved preset.
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), (int) state.getSize());
        xml->removeAttribute("preset");
        auto legacyBank = [](int magic, bool withChains)
        {
            juce::MemoryOutputStream o;
            o.writeInt(magic);
            o.writeInt(2); // programs
            for (int pi = 0; pi < 2; ++pi)
            {
                o.writeString(pi == 0 ? "Legacy 1" : "Legacy 2");
                o.writeInt(40); // more than 32 steps
                o.writeInt(4);  // rate
                o.writeInt(pi == 0 ? 55 : 90);
                o.writeInt(0);
                o.writeInt(0);
                o.writeInt(0);
                for (int si = 0; si < 64; ++si)
                {
                    o.writeByte(pi == 1 && si == 1 ? 3 : 1); // Tie / Normal
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
            auto* selected = xml->createNewChildElement("PARAM");
            selected->setAttribute("id", "program");
            selected->setAttribute("value", 2);
            juce::MemoryBlock legacyState;
            juce::AudioProcessor::copyXmlToBinary(*xml, legacyState);
            xml->removeChildElement(selected, true);

            auto legacy = makeProcessor();
            legacy->setStateInformation(legacyState.getData(), (int) legacyState.getSize());
            const auto lp = legacy->getProgramCopy();
            check(legacy->getActivePreset().name == "Legacy 2" && legacy->getActivePreset().file == juce::File()
                      && lp.steps[1].type == mnArpee::StepType::Tie && lp.steps[31].velocity == 96 && lp.gatePercent == 90
                      && lp.numSteps == 32,
                  withChains ? "legacy-state-with-chains-loads" : "legacy-state-with-64-steps-loads");
        }
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

    // The editor's note display: which held notes are keys, merged octave doubles, or left out.
    {
        auto p = makeProcessor();
        juce::MidiBuffer chord;
        for (int n : { 48, 60, 64, 67, 71, 74, 77 }) // C3 C4 E4 G4 B4 D5 F5
            chord.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0);
        run(*p, 2, chord);
        const auto& ui = p->getUiStatus();
        std::vector<int> uses;
        for (int i = 0; i < ui.numInput.load(); ++i)
            uses.push_back(ui.input[(size_t) i].load() & 3);
        check(ui.numInput.load() == 7 && (ui.input[0].load() >> 2) == 48
                  && uses == std::vector<int> { 0, 1, 0, 0, 0, 0, 2 },
              "ui-shows-merged-and-left-out-notes");
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

        // The plugin's own "Unsaved changes" dialog (inside the editor, not an OS window).
        {
            std::function<juce::TextButton*(juce::Component&, const juce::String&)> findButton;
            findButton = [&findButton](juce::Component& root, const juce::String& text) -> juce::TextButton*
            {
                for (auto* child : root.getChildren())
                {
                    if (auto* b = dynamic_cast<juce::TextButton*>(child); b != nullptr && b->getButtonText() == text && b->isVisible())
                        return b;
                    if (child->isVisible())
                        if (auto* found = findButton(*child, text))
                            return found;
                }
                return nullptr;
            };

            // Cancel keeps the pattern and the changes.
            {
                auto q = makeProcessor();
                std::unique_ptr<juce::AudioProcessorEditor> ed(q->createEditor());
                ed->setSize(900, 560);
                setParam(*q, "gate", 33);
                auto* newButton = findButton(*ed, "New");
                if (newButton != nullptr)
                    newButton->onClick();
                auto* cancel = findButton(*ed, "Cancel");
                const bool asked = findButton(*ed, "Don't save") != nullptr && cancel != nullptr;
                if (cancel != nullptr)
                    cancel->onClick();
                check(asked && q->getActivePreset().name == "Trance Up 16" && q->getActivePreset().dirty,
                      "unsaved-changes-dialog-cancel-keeps-the-preset");
            }
            // Don't save goes on to the new pattern.
            {
                auto q = makeProcessor();
                std::unique_ptr<juce::AudioProcessorEditor> ed(q->createEditor());
                ed->setSize(900, 560);
                setParam(*q, "gate", 33);
                if (auto* newButton = findButton(*ed, "New"))
                    newButton->onClick();
                if (auto* dontSave = findButton(*ed, "Don't save"))
                    dontSave->onClick();
                check(q->getActivePreset().name.startsWith("New Arp") && q->getActivePreset().file == juce::File(),
                      "unsaved-changes-dialog-dont-save-goes-on");
            }
        }
    }
    else
    {
        std::printf("skip: editor checks (no display)\n");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
