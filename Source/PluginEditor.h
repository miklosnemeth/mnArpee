#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"
#include "StepGrid.h"

class MnArpeeAudioProcessor;

//==============================================================================
// MnArpeeLookAndFeel: unified dark style for the whole UI.
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
// MnArpeeAudioEditor: dark card header + StepGrid + step editor strip.
// Header controls are bound to the parameters with attachments (host automation and
// state loads show up, gestures are reported); the grid and step strip edit the
// selected program through the processor.
//==============================================================================
class MnArpeeAudioEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit MnArpeeAudioEditor(MnArpeeAudioProcessor&);
    ~MnArpeeAudioEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void reloadProgram();
    void refreshStepEditor();
    void onGridClicked(int step, int row);
    void onStepEditorChanged();
    void commitStep(const mnArpee::Step& step);
    void setupSlider(juce::Slider& s);
    int selectedProgram() const;

    MnArpeeAudioProcessor& proc;
    MnArpeeLookAndFeel lnf;

    // Header
    juce::ComboBox programBox, chainBox, rateBox;
    juce::Slider octaveSlider, gateSlider, swingSlider, stepsSlider;
    juce::ToggleButton latchButton;
    juce::Label brandLabel;

    // Matrix
    StepGrid grid;

    // Step editor
    juce::ComboBox stepTypeBox, stepKeyBox;
    juce::Slider stepOctSlider, stepVelSlider;

    // Labels
    juce::Label programLabel, chainLabel, rateLabel;
    juce::Label octaveLabel, gateLabel, swingLabel, stepsLabel;
    juce::Label stepTypeLabel, stepKeyLabel, stepOctLabel, stepVelLabel;

    // Attachments (declared after the controls, so they are destroyed first)
    std::unique_ptr<ComboAttachment> programAtt, chainAtt, rateAtt;
    std::unique_ptr<SliderAttachment> octaveAtt, gateAtt, swingAtt, stepsAtt;
    std::unique_ptr<ButtonAttachment> latchAtt;

    // Info bar
    juce::String statusText;

    // Card rectangles (drawn by paint, computed by resized)
    juce::Rectangle<int> paramsCard, gridCard, editCard;

    // The program shown in the grid (a copy, so a state load can never leave it dangling).
    mnArpee::Program shownProgram;
    int shownProgramIndex = -1;
    int shownModelVersion = -1;
    int editStep = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MnArpeeAudioEditor)
};
