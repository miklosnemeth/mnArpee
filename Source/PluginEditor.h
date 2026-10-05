#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"
#include "StepGrid.h"

class MnArpeeAudioProcessor;

//==============================================================================
// MnArpeeLookAndFeel: egységes sötét stílus a teljes UI-hoz.
//==============================================================================
class MnArpeeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MnArpeeLookAndFeel();
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
};

//==============================================================================
// MnArpeeAudioEditor: kártyás sötét fejléc + StepGrid + step-szerkesztő sáv.
//==============================================================================
class MnArpeeAudioEditor : public juce::AudioProcessorEditor,
                           private juce::Timer,
                           private juce::ComboBox::Listener,
                           private juce::Slider::Listener
{
public:
    explicit MnArpeeAudioEditor(MnArpeeAudioProcessor&);
    ~MnArpeeAudioEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void comboBoxChanged(juce::ComboBox*) override;
    void sliderValueChanged(juce::Slider*) override;
    void refreshFromState();
    void pushStepEditToEngine();
    void setupKnob(juce::Slider& s, double lo, double hi, double step);

    MnArpeeAudioProcessor& proc;
    MnArpeeLookAndFeel lnf;

    // Fejléc
    juce::ComboBox programBox, chainBox, rateBox;
    juce::Slider octaveSlider, gateSlider, swingSlider, stepsSlider;
    juce::ToggleButton latchButton;
    juce::Label brandLabel;

    // Matrix
    StepGrid grid;

    // Step-szerkesztő
    juce::ComboBox stepTypeBox, stepKeyBox;
    juce::Slider stepOctSlider, stepVelSlider;

    // Címkék
    juce::Label programLabel, chainLabel, rateLabel;
    juce::Label octaveLabel, gateLabel, swingLabel, stepsLabel;
    juce::Label stepTypeLabel, stepKeyLabel, stepOctLabel, stepVelLabel;

    // Info sáv
    juce::String statusText;
    juce::String lastStatusText;

    // Kártya-téglalapok (paint rajzolja, resized számolja)
    juce::Rectangle<int> paramsCard, gridCard, editCard;

    int editStep = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MnArpeeAudioEditor)
};
