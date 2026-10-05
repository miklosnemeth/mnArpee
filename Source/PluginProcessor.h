#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"

//==============================================================================
// MnArpeeAudioProcessor: VST3/Standalone MIDI-effect.
// - APVTS: rate, mode, octave, gate, swing, program, chain, latch.
// - A step-matrix az aktiv programot szerkeszti (UI -> engine bank).
// - MIDI: bemeneti hangok -> engine; CC/bend/aftertouch thru; sustain+refcount.
//==============================================================================
class MnArpeeAudioProcessor : public juce::AudioProcessor
{
public:
    MnArpeeAudioProcessor();
    ~MnArpeeAudioProcessor() override;

    // ToggleButton-kezeleshez: latch kozvetlen irasa.
    void setLatchDirect(bool on);

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    mnArpee::ArpEngine& getEngine() { return engine; }
    mnArpee::Bank& getEngineBankForEdit() { return bank; }
    juce::CriticalSection& getEngineLock() { return engineLock; }
    void pushEngineBankToProcessor();
    void requestBankRefresh() { bankDirty.store(true); }
    bool consumeBankRefresh() { return bankDirty.exchange(false); }

    static const char* modeName(mnArpee::ArpMode m);
    static const char* syncName(mnArpee::SyncRate r);

private:
    void applyApvtsToEngine();

    juce::AudioProcessorValueTreeState apvts;
    mnArpee::Bank bank;
    mnArpee::ArpEngine engine;
    juce::CriticalSection engineLock;
    std::atomic<bool> bankDirty { false };

    int lastProgramParam = -1;
    int lastChainParam = -1;
    bool lastLatchParam = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MnArpeeAudioProcessor)
};
