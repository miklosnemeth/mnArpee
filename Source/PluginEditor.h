#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"
#include "StepGrid.h"

class MnArpeeAudioProcessor;
using Pal = StepGrid::Palette;

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
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle,
                          juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont(juce::Slider&) override;
};

//==============================================================================
// ValueBar: a header value drawn as one line ("OCT +3"). Only the vertical mouse movement
// changes it (dragging up raises it, the horizontal movement is ignored) and the wheel steps it
// by one interval, so the value never jumps to where the mouse was clicked.
//==============================================================================
class ValueBar : public juce::Slider
{
public:
    void setPixelsPerUnit(double pixels) { pixelsPerUnit = pixels; }

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    double pixelsPerUnit = 6.0;
    double valueOnMouseDown = 0.0;
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
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void reloadProgram();
    void onGridClicked(int step, int row);
    void onLaneAdjusted(int step, StepGrid::Lane lane, int delta);
    void commitStep(const mnArpee::Step& step);
    // A header value shown as one line ("OCT +3") that is dragged to change.
    void setupValueBar(ValueBar& s, std::function<juce::String(double)> text, double defaultValue,
                       double pixelsPerUnit);
    int selectedProgram() const;
    void setProgramIndex(int index);
    void stepProgram(int delta);
    void showProgramMenu();

    MnArpeeAudioProcessor& proc;
    MnArpeeLookAndFeel lnf;

    // Header
    // Program picker in the header: previous / name (opens the list) / next, as in mnKord.
    juce::ArrowButton prevProgramButton { "previous program", 0.5f, Pal::text() };
    juce::ArrowButton nextProgramButton { "next program", 0.0f, Pal::text() };
    juce::TextButton programButton;
    ValueBar rateSlider, octaveSlider, gateSlider, swingSlider, stepsSlider;
    juce::ToggleButton latchButton;
    juce::Rectangle<int> brandArea, statusArea;

    // Matrix, with the value lanes below it (a lane caption chooses what the rows edit).
    StepGrid grid;

    // Attachments (declared after the controls, so they are destroyed first)
    std::unique_ptr<SliderAttachment> rateAtt, octaveAtt, gateAtt, swingAtt, stepsAtt;
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
