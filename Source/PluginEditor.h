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
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont(juce::Slider&) override;
};

//==============================================================================
// MnArpeeAudioEditor: dark card header + StepGrid (matrix and value lanes).
// Header controls are bound to the parameters with attachments (host automation and
// state loads show up, gestures are reported); the grid (matrix and value lanes) edits the
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
    void onGridClicked(int step, int row);
    void onLaneAdjusted(int step, StepGrid::Lane lane, int delta);
    void commitStep(const mnArpee::Step& step);
    void setupSlider(juce::Slider& s);
    int selectedProgram() const;

    MnArpeeAudioProcessor& proc;
    MnArpeeLookAndFeel lnf;

    // Header
    juce::ComboBox programBox, chainBox, rateBox, velModeBox;
    juce::Slider octaveSlider, gateSlider, swingSlider, stepsSlider;
    juce::ToggleButton latchButton;
    juce::Rectangle<int> brandArea;

    // Matrix, with the value lanes below it (a lane caption chooses what the rows edit).
    StepGrid grid;

    // Labels
    juce::Label programLabel, chainLabel, rateLabel, velModeLabel;
    juce::Label octaveLabel, gateLabel, swingLabel, stepsLabel;

    // Attachments (declared after the controls, so they are destroyed first)
    std::unique_ptr<ComboAttachment> programAtt, chainAtt, rateAtt, velModeAtt;
    std::unique_ptr<SliderAttachment> octaveAtt, gateAtt, swingAtt, stepsAtt;
    std::unique_ptr<ButtonAttachment> latchAtt;

    // Info bar
    juce::String statusText;

    // Card rectangles (drawn by paint, computed by resized)
    juce::Rectangle<int> paramsCard, gridCard;

    // The program shown in the grid (a copy, so a state load can never leave it dangling).
    mnArpee::Program shownProgram;
    int shownProgramIndex = -1;
    int shownModelVersion = -1;
    int editStep = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MnArpeeAudioEditor)
};
