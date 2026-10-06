#pragma once

#include <JuceHeader.h>

#include "ArpEngine.h"

//==============================================================================
// StepGrid: 64-step matrix. Row = KEY SELECT (k1..k5 + Chr + Rnd),
// column = step. Clicking a cell toggles that setting. Tie steps are drawn linked
// to the previous column, Rest and Off steps as a mark in the first row.
//==============================================================================
class StepGrid : public juce::Component
{
public:
    using StepClicked = std::function<void(int stepIndex, int row)>;

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

    // The grid keeps its own copy, so it never points into data that may be replaced.
    void setProgram(const mnArpee::Program& prog);
    // Only the old and new playhead columns are repainted.
    void setPlayStep(int step);
    void setEditStep(int step);
    void onStepClicked(StepClicked fn) { clicked = std::move(fn); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;

    static constexpr int numRows = 7; // k1..k5, Chr, Rnd

private:
    juce::Rectangle<int> boardArea() const;
    juce::Rectangle<int> cellRect(int step, int row) const;
    juce::Rectangle<int> columnBounds(int step) const;
    int visibleSteps() const;
    void cellAt(juce::Point<int> p, int& step, int& row) const;
    int rowForStep(const mnArpee::Step& s) const;

    mnArpee::Program program;
    bool hasProgram = false;
    int playStep = -1;
    int editStep = 0;
    int hoverStep = -1;
    StepClicked clicked;
};
