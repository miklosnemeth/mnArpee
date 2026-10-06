#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"

#include <array>
#include <atomic>

//==============================================================================
// MnArpeeAudioProcessor: VST3/Standalone MIDI-effect.
//
// Threading:
//  - `model` is the editable bank. It lives on the message thread (editor, host
//    program calls, state save/load) and is guarded by modelLock, which the audio
//    thread never takes.
//  - publishBank() copies the model into `pendingBank`; the audio thread picks it up
//    with a try-lock, so it never blocks and never allocates.
//  - The rate/gate/swing/octave/steps parameters always show the selected program:
//    switching programs stores them into the old program and loads the new one.
//  - MIDI is processed sample-accurately: the engine renders the segments between
//    incoming events.
//==============================================================================
class MnArpeeAudioProcessor : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater
{
public:
    static constexpr int numUserPrograms = 8;

    // Written by the audio thread, read by the editor.
    struct UiStatus
    {
        std::atomic<int> playStep { -1 };
        std::atomic<int> playingProgram { 0 };
        std::atomic<int> numInput { 0 };
        std::array<std::atomic<int>, mnArpee::maxInputKeys> input {};
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
    mnArpee::Program getProgramCopy(int index) const;
    void setStep(int programIndex, int stepIndex, const mnArpee::Step& step);
    // Increments whenever the model changes (edits, program switch, state load).
    int getModelVersion() const { return modelVersion.load(); }
    const UiStatus& getUiStatus() const { return ui; }
    // Message thread: runs a pending program switch now instead of on the next message
    // loop iteration (used by the plugin tests).
    void handlePendingUpdates() { handleUpdateNowIfNeeded(); }

private:
    struct ParamValues
    {
        int program = -1, chain = -2, rate = -1, gate = -1, swing = -1000, octave = -1000, steps = -1;
        bool latch = false;
    };

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    ParamValues readParams() const;
    void storeParamsInto(mnArpee::Program& prog) const;
    void loadParamsFrom(const mnArpee::Program& prog);
    void publishBank(bool restartPlayback = false);
    void setParamValue(const char* id, float plainValue);

    void pickUpPendingBank();
    void applyParamsToEngine();
    void handleMidiMessage(const juce::MidiMessage& msg, int at);
    void renderSegment(int start, int length, const mnArpee::HostPosition& host);
    void releaseThruNotes(int at);
    void updateUiStatus();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* programParam = nullptr;
    std::atomic<float>* chainParam = nullptr;
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* gateParam = nullptr;
    std::atomic<float>* swingParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* stepsParam = nullptr;
    std::atomic<float>* latchParam = nullptr;

    // Message thread.
    mutable juce::CriticalSection modelLock;
    mnArpee::Bank model;
    int editedProgram = 0;
    std::atomic<int> modelVersion { 0 };

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
