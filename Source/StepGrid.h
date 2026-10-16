#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"

//==============================================================================
// StepGrid: the step matrix with the value lanes under it. Column = step.
//  - The matrix is for the keys: bottom to top Random, Root, Key 1 .. Key 5 (clicking toggles the
//    step on that key; Tie steps are drawn linked to the previous column, Rest and Off as a mark).
//  - Under it, one row per other value: TYPE, OCTAVE and VELOCITY. A value box is adjusted by
//    dragging up and down or with the mouse wheel; the dice in front of the lane fills it randomly.
//==============================================================================
class StepGrid : public juce::Component
{
public:
    using StepClicked = std::function<void(int stepIndex, int row)>;
    // A value box was grabbed (delta 0) or adjusted by `delta` units.
    enum class Lane { Key, Type, Octave, Velocity };
    using LaneAdjusted = std::function<void(int stepIndex, Lane lane, int delta)>;
    // Right click on a matrix cell (Lane::Key, row >= 0) or on a value box (row -1).
    using ContextRequested = std::function<void(int stepIndex, Lane lane, int row)>;
    // The dice of a lane was clicked.
    using RandomizeRequested = std::function<void(Lane lane)>;

    // Unified dark palette (used by the whole UI).
    struct Palette
    {
        static juce::Colour bg()      { return juce::Colour(0xff0b1510); }
        static juce::Colour card()    { return juce::Colour(0xff13211a); }
        static juce::Colour cardHi()  { return juce::Colour(0xff1c3026); }
        static juce::Colour border()  { return juce::Colour(0xff2d4a3a); }
        static juce::Colour text()    { return juce::Colour(0xfff0faf3); }
        static juce::Colour dimText() { return juce::Colour(0xffb4cdbc); }
        static juce::Colour accent()  { return juce::Colour(0xff3ddc84); }
        static juce::Colour accent2() { return juce::Colour(0xffff5d5d); }
        static juce::Colour danger()  { return juce::Colour(0xffff7a7a); }
        static juce::Colour cellOff() { return juce::Colour(0xff22352b); }
        static juce::Colour playHead(){ return juce::Colour(0x40e8ffa0); }
    };

    StepGrid();

    // The grid keeps its own copy, so it never points into data that may be replaced.
    void setProgram(const mnArpee::Program& prog);
    // Only the old and new playhead columns are repainted.
    void setPlayStep(int step);
    void setEditStep(int step);
    void onStepClicked(StepClicked fn) { clicked = std::move(fn); }
    void onLaneAdjusted(LaneAdjusted fn) { adjusted = std::move(fn); }
    void onContextRequested(ContextRequested fn) { contextRequested = std::move(fn); }
    void onRandomizeRequested(RandomizeRequested fn) { randomizeRequested = std::move(fn); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;

    // Matrix rows, top to bottom: Key 5 .. Key 1, Root, Random.
    static constexpr int numRows = 7;
    static constexpr int rootRow = 5;
    static constexpr int randomRow = 6;
    // KEY SELECT value of a key row (Key 5 .. Key 1, Root); -2 for the Random row.
    static int keyForRow(int row) { return row == rootRow ? mnArpee::rootKeyIndex : (row < rootRow ? 4 - row : -2); }
    static int rowForKey(int key) { return key == mnArpee::rootKeyIndex ? rootRow : 4 - std::clamp(key, 0, 4); }

private:
    juce::Rectangle<int> boardArea() const;
    juce::Rectangle<int> cellRect(int step, int row) const;
    juce::Rectangle<int> columnBounds(int step) const;
    int visibleSteps() const;
    int activeSteps() const; // columns that are part of the pattern
    void cellAt(juce::Point<int> p, int& step, int& row) const;
    int rowForStep(const mnArpee::Step& s) const;
    void clickAt(juce::Point<int> p);
    juce::Rectangle<int> laneBox(int step, int laneRow) const;
    juce::Rectangle<int> laneCaption(int laneRow) const;
    juce::Rectangle<int> laneDice(int laneRow) const;
    bool laneBoxAt(juce::Point<int> p, int& step, int& laneRow) const;

    mnArpee::Program program;
    bool hasProgram = false;
    int playStep = -1;
    int editStep = 0;
    int hoverStep = -1;
    int lastClickStep = -1, lastClickRow = -1;
    // Value box drag: the step, the lane row and the vertical movement not yet applied.
    int dragStep = -1, dragLaneRow = -1, dragLastY = 0, dragRemainder = 0;
    StepClicked clicked;
    LaneAdjusted adjusted;
    ContextRequested contextRequested;
    RandomizeRequested randomizeRequested;
};
