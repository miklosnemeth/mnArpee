#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"

//==============================================================================
// StepGrid: 64-step matrix, column = step, with the value lanes (as in BlueARP) underneath.
// Clicking a lane caption selects the lane that the matrix shows; the value box of a step
// is adjusted by dragging up and down or with the mouse wheel. The lane picks what the rows edit:
//  - Key:      bottom to top Random, Root, Key 1 .. Key 5 (clicking toggles the step on that key;
//              Tie steps are drawn linked to the previous column, Rest and Off as a mark)
//  - Type:     Normal, Random, Tie, Rest, Off
//  - Octave:   +3 .. -3
//  - Velocity: eight levels, drawn as bars
//==============================================================================
class StepGrid : public juce::Component
{
public:
    using StepClicked = std::function<void(int stepIndex, int row)>;
    // A value box was grabbed (delta 0) or adjusted by `delta` units.
    enum class Lane { Key, Type, Octave, Velocity };
    using LaneAdjusted = std::function<void(int stepIndex, Lane lane, int delta)>;

    // Unified dark palette (used by the whole UI).
    struct Palette
    {
        static juce::Colour bg()      { return juce::Colour(0xff0e1117); }
        static juce::Colour card()    { return juce::Colour(0xff161b24); }
        static juce::Colour cardHi()  { return juce::Colour(0xff1d2430); }
        static juce::Colour border()  { return juce::Colour(0xff262f3d); }
        static juce::Colour text()    { return juce::Colour(0xffdbe4f0); }
        static juce::Colour dimText() { return juce::Colour(0xff7f8ea3); }
        static juce::Colour accent()  { return juce::Colour(0xff4fc3f7); }
        static juce::Colour accent2() { return juce::Colour(0xff7c5cff); }
        static juce::Colour danger()  { return juce::Colour(0xffef5f6b); }
        static juce::Colour cellOff() { return juce::Colour(0xff252e3d); }
        static juce::Colour playHead(){ return juce::Colour(0x40ffee8c); }
    };

    StepGrid();

    void setLane(Lane l);
    Lane getLane() const { return lane; }
    // Row count of a lane (rows are numbered from the top).
    static int rowsFor(Lane l);
    static constexpr int velocityLevels = 8;
    // Lane values of a row: Type = StepType, Octave = octave, Velocity = velocity.
    static mnArpee::StepType typeForRow(int row);
    static int octaveForRow(int row) { return 3 - row; }
    static int velocityForRow(int row) { return (127 * (velocityLevels - row) + velocityLevels / 2) / velocityLevels; }

    // The grid keeps its own copy, so it never points into data that may be replaced.
    void setProgram(const mnArpee::Program& prog);
    // Only the old and new playhead columns are repainted.
    void setPlayStep(int step);
    void setEditStep(int step);
    void onStepClicked(StepClicked fn) { clicked = std::move(fn); }
    void onLaneAdjusted(LaneAdjusted fn) { adjusted = std::move(fn); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;

    // Key lane, top to bottom: Key 5 .. Key 1, Root, Random.
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
    void cellAt(juce::Point<int> p, int& step, int& row) const;
    int rowForStep(const mnArpee::Step& s) const;
    int numRows() const { return rowsFor(lane); }
    void clickAt(juce::Point<int> p);
    juce::Rectangle<int> laneBox(int step, int laneRow) const;
    juce::Rectangle<int> laneCaption(int laneRow) const;
    bool laneBoxAt(juce::Point<int> p, int& step, int& laneRow) const;

    mnArpee::Program program;
    bool hasProgram = false;
    int playStep = -1;
    int editStep = 0;
    int hoverStep = -1;
    Lane lane = Lane::Key;
    int lastClickStep = -1, lastClickRow = -1;
    // Value box drag: the step, the lane row and the vertical movement not yet applied.
    int dragStep = -1, dragLaneRow = -1, dragLastY = 0, dragRemainder = 0;
    StepClicked clicked;
    LaneAdjusted adjusted;
};
