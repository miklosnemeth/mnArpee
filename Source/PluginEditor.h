#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"
#include "PluginProcessor.h"
#include "StepGrid.h"

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
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getSliderPopupFont(juce::Slider&) override;
};

//==============================================================================
// HistoryButton: the undo (curved arrow to the left) and redo (to the right) icon button.
//==============================================================================
class HistoryButton : public juce::Button
{
public:
    explicit HistoryButton(bool redoIn) : juce::Button(redoIn ? "redo" : "undo"), redo(redoIn) {}
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

private:
    bool redo;
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
    bool keyPressed(const juce::KeyPress&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void reloadProgram();
    void onGridClicked(int step, int row);
    void onLaneAdjusted(int step, StepGrid::Lane lane, int delta);
    void commitStep(const mnArpee::Step& step);
    // Pattern tools (right click in the matrix): set all, shift, reverse, randomize, clear, copy, paste.
    void showPatternMenu(int step, StepGrid::Lane lane, int row);
    void applySteps(const mnArpee::Program& edited);
    void copyPattern();
    void pastePattern();
    void promptSave(const juce::String& initialName, const juce::String& initialStyles,
                    std::function<void(juce::String, juce::String)> done);
    // The plugin's own dialog, inside the editor: a title, a message, optional text fields (label,
    // initial text) and buttons. Enter presses defaultButton, Escape cancelButton; `done` gets the
    // pressed button and the texts of the fields.
    void showDialog(const juce::String& title, const juce::String& message,
                    std::vector<std::pair<juce::String, juce::String>> fields, juce::StringArray buttons,
                    int defaultButton, int cancelButton,
                    std::function<void(int button, juce::StringArray values)> done);
    void showMessage(const juce::String& title, const juce::String& message);
    void closeDialog();
    // A header value shown as one line ("OCT +3") that is dragged to change.
    void setupValueBar(ValueBar& s, std::function<juce::String(double)> text, double defaultValue,
                       double pixelsPerUnit);

    // Presets (the preset library is managed by the processor; the dialogs are here).
    void stepPreset(int delta);
    void showPresetBrowser();
    void closePresetBrowser();
    void showPresetMenu();
    void newClicked();
    void saveClicked();
    void loadPresetChecked(int libraryIndex);
    // Runs `proceed` now, or after the user has saved or discarded the unsaved changes.
    void confirmLeavePreset(std::function<void()> proceed);
    void promptName(const juce::String& title, const juce::String& message, const juce::String& initial,
                    std::function<void(juce::String)> done);
    void saveActiveAs(std::function<void()> then);
    void renameDialog(int libraryIndex);
    void deleteDialog(int libraryIndex);
    void showPresetContextMenu(int libraryIndex);
    void importPresets();
    void exportPreset();
    void refreshPresetUi();

    MnArpeeAudioProcessor& proc;
    MnArpeeLookAndFeel lnf;

    // Header
    // Preset picker in the header, as in mnKord: previous / name (opens the browser) / next,
    // then New, Save and the preset menu.
    juce::ArrowButton prevPresetButton { "previous preset", 0.5f, Pal::text() };
    juce::ArrowButton nextPresetButton { "next preset", 0.0f, Pal::text() };
    juce::TextButton presetButton, newButton, saveButton, menuButton;
    HistoryButton undoButton { false }, redoButton { true };
    ValueBar rateSlider, octaveSlider, gateSlider, swingSlider, stepsSlider;
    juce::ToggleButton latchButton;
    juce::Rectangle<int> brandArea, versionArea, statusArea;

    // Matrix, with the value lanes below it (a lane caption chooses what the rows edit).
    StepGrid grid;

    // Attachments (declared after the controls, so they are destroyed first)
    std::unique_ptr<SliderAttachment> rateAtt, octaveAtt, gateAtt, swingAtt, stepsAtt;
    std::unique_ptr<ButtonAttachment> latchAtt;

    // The held notes in the pattern card: each as note * 4 + KeyUse (see ArpEngine.h).
    std::vector<int> heldShown;
    int shownChord = 0; // ChordInfo::code() of the held notes

    // Card rectangles (drawn by paint, computed by resized)
    juce::Rectangle<int> paramsCard, gridCard;

    MnArpeeAudioProcessor::ActivePreset shownPreset;
    bool shownPresetValid = false;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::Component> presetBrowser; // overlay inside the editor, null while closed
    std::unique_ptr<juce::Component> dialog;        // the dialog, null while closed

    // The program shown in the grid (a copy, so a state load can never leave it dangling).
    mnArpee::Program shownProgram;
    int shownProgramIndex = -1;
    int shownModelVersion = -1;
    int editStep = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MnArpeeAudioEditor)
};
