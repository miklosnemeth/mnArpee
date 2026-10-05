#include "StepGrid.h"

namespace
{
constexpr int labelW = 56;   // row label strip on the left
constexpr int stepH = 24;    // step row height
constexpr int gutterTop = 20; // column labels (1,5,9,...)
constexpr int cellGap = 3;
}

StepGrid::StepGrid()
{
    setWantsKeyboardFocus(false);
    setBufferedToImage(true);
}

void StepGrid::setProgram(const mnArpee::Program* prog)
{
    program = prog;
    repaint();
}

int StepGrid::rowForStep(const mnArpee::Step& s) const
{
    using mnArpee::StepType;
    if (s.type == StepType::Off || s.type == StepType::Rest || s.type == StepType::Tie)
        return -1;
    if (s.type == StepType::Chord)
        return 5;
    if (s.type == StepType::Random)
        return 6;
    return std::clamp((int) s.keyIndex, 0, 4);
}

juce::Rectangle<int> StepGrid::boardArea() const
{
    auto area = getLocalBounds().reduced(8);
    area.removeFromLeft(labelW);
    area.removeFromTop(gutterTop);
    return area;
}

juce::Rectangle<int> StepGrid::cellRect(int step, int row) const
{
    auto board = boardArea();
    const int n = program != nullptr ? std::clamp(program->numSteps, 1, mnArpee::maxSteps) : 16;
    const int cw = board.getWidth() / n;
    const int ch = board.getHeight() / numRows;
    return { board.getX() + step * cw + cellGap / 2,
             board.getY() + row * ch + cellGap / 2,
             cw - cellGap, ch - cellGap };
}

void StepGrid::cellAt(juce::Point<int> p, int& step, int& row) const
{
    step = -1;
    row = -1;
    if (program == nullptr)
        return;
    auto board = boardArea();
    const int n = std::clamp(program->numSteps, 1, mnArpee::maxSteps);
    const int cw = board.getWidth() / n;
    const int ch = board.getHeight() / numRows;
    if (! board.contains(p))
        return;
    const int sx = (p.x - board.getX()) / cw;
    const int ry = (p.y - board.getY()) / ch;
    if (sx >= 0 && sx < n && ry >= 0 && ry < numRows)
    {
        step = sx;
        row = ry;
    }
}

void StepGrid::paint(juce::Graphics& g)
{
    // Transparent background: the editor card shows through.
    g.setColour(Palette::bg().withAlpha(0.55f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

    if (program == nullptr)
        return;

    const int n = std::clamp(program->numSteps, 1, mnArpee::maxSteps);
    auto board = boardArea();

    // Column labels (every 4th step highlighted).
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(Palette::dimText());
    for (int s = 0; s < n; s += 4)
    {
        auto r = cellRect(s, 0);
        g.drawText(juce::String(s + 1), r.getX() - 4, 4, r.getWidth() + 8, 14,
                   juce::Justification::centredLeft);
    }

    static const char* rowNames[numRows] = { "k1", "k2", "k3", "k4", "k5", "Chr", "Rnd" };

    for (int r = 0; r < numRows; ++r)
    {
        auto first = cellRect(0, r);
        g.setColour(r <= 4 ? Palette::dimText() : Palette::accent2());
        g.drawText(rowNames[r], 8, first.getY(), labelW - 14, first.getHeight(),
                   juce::Justification::centredRight);

        for (int s = 0; s < n; ++s)
        {
            const auto& step = program->steps[(size_t) s];
            auto cell = cellRect(s, r);
            const bool isEdit = (s == editStep);
            const bool isPlay = (s == playStep);

            // Step column highlight (playhead glow strip + edited column).
            if (isPlay)
            {
                g.setColour(Palette::playHead());
                g.fillRect(cell.expanded(0, 1));
            }

            const bool active = (r == rowForStep(step)
                                 && step.type != mnArpee::StepType::Off);
            juce::Colour col;
            if (active)
            {
                // Intensity proportional to velocity in the highlight blue.
                const float vel = juce::jlimit(0.0f, 1.0f, step.velocity / 127.0f);
                col = Palette::accent().withBrightness(0.45f + 0.30f * vel);
            }
            else if (r == rowForStep(step))
            {
                col = Palette::cellOff().brighter(0.08f);
            }
            else if (s == hoverStep)
            {
                col = Palette::cellOff().brighter(0.15f);
            }
            else
            {
                col = Palette::cellOff();
            }
            if (isEdit)
                col = col.brighter(0.12f);

            g.setColour(col);
            g.fillRoundedRectangle(cell.toFloat(), 3.0f);

            // Off step: faint red x in the first row (nothing plays).
            if (step.type == mnArpee::StepType::Off && r == 0)
            {
                g.setColour(Palette::danger().withAlpha(0.65f));
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                g.drawText("x", cell, juce::Justification::centred);
            }
            else if (active && step.type != mnArpee::StepType::Normal)
            {
                const char* mark = step.type == mnArpee::StepType::Chord ? "C"
                    : step.type == mnArpee::StepType::Random ? "?" : "";
                if (mark[0] != 0)
                {
                    g.setColour(Palette::bg().withAlpha(0.85f));
                    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                    g.drawText(mark, cell, juce::Justification::centred);
                }
            }

            if (isEdit)
            {
                g.setColour(Palette::accent2().withAlpha(0.9f));
                g.drawRoundedRectangle(cell.toFloat(), 3.0f, 1.4f);
            }
        }
    }

    // Playhead arrow in the header.
    if (playStep >= 0 && playStep < n)
    {
        auto c = cellRect(playStep, 0);
        const float cx = (float) c.getCentreX();
        juce::Path tri;
        tri.addTriangle(cx - 5.0f, 14.0f, cx + 5.0f, 14.0f, cx, 6.0f);
        g.setColour(Palette::accent());
        g.fillPath(tri);
    }
}

void StepGrid::mouseDown(const juce::MouseEvent& e)
{
    int step = -1, row = -1;
    cellAt(e.getPosition(), step, row);
    if (step >= 0 && row >= 0 && clicked)
        clicked(step, row);
}

void StepGrid::mouseMove(const juce::MouseEvent& e)
{
    int step = -1, row = -1;
    cellAt(e.getPosition(), step, row);
    if (step != hoverStep)
    {
        hoverStep = step;
        repaint();
    }
}

void StepGrid::mouseExit(const juce::MouseEvent&)
{
    hoverStep = -1;
    repaint();
}
