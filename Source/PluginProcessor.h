#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"
#include "PresetLibrary.h"

#include <array>
#include <atomic>
#include <vector>

//==============================================================================
// MnArpeeAudioProcessor: VST3/Standalone MIDI-effect.
//
// The plugin plays one arp pattern at a time: the active preset. Presets live in the library
// (PresetLibrary: factory presets and the user's own files); the project stores the active
// preset's current state.
//
// Threading:
//  - `model` holds the active pattern in one of two slots. It lives on the message thread
//    (editor, host program calls, state save/load) and is guarded by modelLock, which the
//    audio thread never takes. Loading a preset fills the other slot and switches to it, so
//    the engine changes pattern at the end of the running one.
//  - publishBank() copies the model into `pendingBank`; the audio thread picks it up
//    with a try-lock, so it never blocks and never allocates.
//  - The rate/gate/swing/octave/steps parameters always show the active pattern.
//  - MIDI is processed sample-accurately: the engine renders the segments between
//    incoming events.
//==============================================================================
class MnArpeeAudioProcessor : public juce::AudioProcessor,
                              private juce::AsyncUpdater
{
public:
    // The host's program list mirrors the preset library (a fixed number of slots, so hosts
    // keep their indices; unused ones show "-").
    static constexpr int numHostPrograms = 128;

    // Written by the audio thread, read by the editor.
    struct UiStatus
    {
        std::atomic<int> playStep { -1 };
        std::atomic<int> playingProgram { 0 };
        // The held notes (up to maxShownInput), each as note * 4 + KeyUse, and how many there are.
        static constexpr int maxShownInput = 16;
        std::atomic<int> chord { 0 }; // ChordInfo::code() of the held notes
        std::atomic<int> numInput { 0 };
        std::array<std::atomic<int>, maxShownInput> input {};
    };

    // The active preset as the editor shows it.
    struct ActivePreset
    {
        juce::String name, group, author, tags;
        juce::File file;      // empty: not saved yet
        bool factory = false;
        bool dirty = false;   // changed since it was loaded or saved
    };

    MnArpeeAudioProcessor();
    ~MnArpeeAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

    // Model access for the editor (message thread).
    // The slot the engine plays the active pattern from (changes with every preset load).
    int getActiveSlot() const { return activeSlot.load(); }
    mnArpee::Program getProgramCopy() const;
    void setStep(int stepIndex, const mnArpee::Step& step);
    // Undo / redo of the step edits of the active pattern (edits within 0.6 s are one step; a preset
    // load starts a new history).
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    // The range of the random values of a lane (the right click on its dice; kept with the project).
    mnArpee::RandomRange getRandomRange(mnArpee::PatternLane lane) const;
    void setRandomRange(mnArpee::PatternLane lane, mnArpee::RandomRange range);
    // The size of the editor: 0.5 .. 3 (kept with the project).
    float getZoom() const { return zoom.load(); }
    void setZoom(float factor) { zoom.store(juce::jlimit(0.5f, 3.0f, factor)); }
    // Replaces all the steps of the active pattern (pattern tools, paste).
    void setAllSteps(const std::array<mnArpee::Step, mnArpee::maxSteps>& steps);
    // Increments whenever the model changes (edits, preset load, state load).
    int getModelVersion() const { return modelVersion.load(); }
    const UiStatus& getUiStatus() const { return ui; }
    // Message thread: runs a pending host program switch now instead of on the next message
    // loop iteration (used by the plugin tests).
    void handlePendingUpdates() { handleUpdateNowIfNeeded(); }

    // Presets (message thread).
    ActivePreset getActivePreset() const;
    // The library. Keep the copy short-lived: it changes with every save, rename and delete.
    std::vector<mnArpee::Preset> getPresetList() const;
    // Library index of the active preset, or -1 when it is not saved (or was deleted).
    int getActivePresetIndex() const;
    bool loadPreset(int libraryIndex);
    // Sets or clears the star of the preset at this library index.
    void setPresetFavorite(int libraryIndex, bool favorite);
    // A fresh pattern ("New Arp"), not saved yet.
    void newPreset();
    // Writes the active preset into its file; false when it has none (or is a factory preset).
    bool saveActivePreset();
    // Saves the active pattern as a user preset with this name (it becomes the active one).
    bool saveActivePresetAs(const juce::String& name);
    void revertActivePreset();
    // The styles of the active preset ("House, Trance"); an unsaved change like any other.
    void setActivePresetStyles(const juce::String& styles);
    // Renames the user preset at this library index (also the active one when it is).
    bool renamePreset(int libraryIndex, const juce::String& newName);
    bool deletePreset(int libraryIndex);
    // Duplicates the preset at this library index as a user preset; returns its new index or -1.
    int duplicatePreset(int libraryIndex);
    // Saves the presets of a preset or bank file as user presets; returns how many, and loads the first.
    int importPresets(const juce::File& file);
    bool exportActivePreset(const juce::File& file) const;
    juce::File getPresetFolder() const { return library.root(); }
    juce::File getUserPresetFolder() const { return library.userDir(); }
    juce::String freePresetName(const juce::String& wanted) const;

private:
    struct ParamValues
    {
        int program = -1, rate = -1, gate = -1, swing = -1000, octave = -1000, steps = -1;
        bool latch = false;
    };

    void handleAsyncUpdate() override;

    ParamValues readParams() const;
    void storeParamsInto(mnArpee::Program& prog) const;
    void loadParamsFrom(const mnArpee::Program& prog);
    void publishBank(bool restartPlayback = false);
    void setParamValue(const char* id, float plainValue);

    // Message thread, with modelLock held.
    mnArpee::Preset currentPresetLocked() const;
    void activatePresetLocked(const mnArpee::Preset& preset, bool saved, bool restartPlayback);
    // Remembers the steps before an edit (modelLock held).
    void pushUndoLocked();

    void pickUpPendingBank();
    void applyParamsToEngine();
    void handleMidiMessage(const juce::MidiMessage& msg, int at);
    void renderSegment(int start, int length, const mnArpee::HostPosition& host);
    void releaseThruNotes(int at);
    void updateUiStatus();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* gateParam = nullptr;
    std::atomic<float>* swingParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* stepsParam = nullptr;
    std::atomic<float>* latchParam = nullptr;

    // Message thread.
    mutable juce::CriticalSection modelLock;
    mnArpee::Bank model;                 // two slots; the active pattern is in activeSlot
    std::atomic<int> activeSlot { 0 };
    std::atomic<int> modelVersion { 0 };
    mnArpee::PresetLibrary library;
    mnArpee::Preset active;              // name, group, author and file of the active preset
    juce::String savedSnapshot;          // its content when it was loaded or saved
    std::atomic<int> pendingHostProgram { -1 };
    using StepList = std::array<mnArpee::Step, mnArpee::maxSteps>;
    std::vector<StepList> undoStack, redoStack;
    juce::int64 lastEditMs = 0;
    std::atomic<float> zoom { 1.0f };
    std::array<mnArpee::RandomRange, 6> randomRanges {}; // by PatternLane; filled with the defaults in the constructor

    // Hand-over to the audio thread.
    juce::SpinLock pendingLock;
    mnArpee::Bank pendingBank;
    std::atomic<bool> bankDirty { false };
    std::atomic<bool> bankRestart { false };

    // Audio thread.
    mnArpee::ArpEngine engine;
    mnArpee::EventBuffer events;
    juce::MidiBuffer outMidi;
    ParamValues applied;
    bool resendProgram = true;
    std::array<std::array<bool, 128>, 16> thruOn {};
    bool wasThru = false;
    UiStatus ui;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MnArpeeAudioProcessor)
};
