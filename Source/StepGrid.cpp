#include "StepGrid.h"

#include "Fonts.h"

namespace
{
constexpr int labelW = 112;  // row label strip on the left
constexpr int stepH = 24;    // step row height
constexpr int gutterTop = 20; // column labels (1,5,9,...)
constexpr int cellGap = 3;
constexpr int laneRowH = 25;  // value lane row height
constexpr int laneGap = 8;    // between the matrix and the value lanes
constexpr int numLaneRows = 5;
constexpr int minVisibleSteps = 16; // columns always shown
constexpr int dragPixelsPerUnit = 8; // type, octave and ratchet; velocity: 1 px per unit, gate: 3 px (5 % per unit)

// Value lanes under the matrix, top to bottom: step type, octave, velocity, gate, ratchet.
constexpr StepGrid::Lane laneOfRow[numLaneRows] = { StepGrid::Lane::Type, StepGrid::Lane::Octave,
                                                    StepGrid::Lane::Velocity, StepGrid::Lane::Gate,
                                                    StepGrid::Lane::Ratchet };
constexpr const char* laneCaptions[numLaneRows] = { "TYPE", "OCTAVE", "VELOCITY", "STEP GATE", "RATCHET" };
}

namespace
{
// A dice: a rounded square with five dots.
void drawDice(juce::Graphics& g, juce::Rectangle<float> dice)
{
    g.setColour(StepGrid::Palette::cardHi());
    g.fillRoundedRectangle(dice, 4.0f);
    g.setColour(StepGrid::Palette::accent());
    g.drawRoundedRectangle(dice, 4.0f, 1.2f);
    const float d = std::min(dice.getWidth(), dice.getHeight()) * 0.22f;
    for (const auto& dot : { juce::Point<float>(-1.0f, -1.0f), juce::Point<float>(1.0f, -1.0f), juce::Point<float>(0.0f, 0.0f),
                             juce::Point<float>(-1.0f, 1.0f), juce::Point<float>(1.0f, 1.0f) })
        g.fillEllipse(dice.getCentreX() + dot.x * d - 1.6f, dice.getCentreY() + dot.y * d - 1.6f, 3.2f, 3.2f);
}
} // namespace

StepGrid::StepGrid()
{
    setWantsKeyboardFocus(false);
    setBufferedToImage(true);
}

void StepGrid::setProgram(const mnArpee::Program& prog)
{
    program = prog;
    hasProgram = true;
    repaint();
}

void StepGrid::setPlayStep(int step)
{
    if (step == playStep)
        return;
    if (playStep >= 0)
        repaint(columnBounds(playStep));
    playStep = step;
    if (playStep >= 0)
        repaint(columnBounds(playStep));
}

void StepGrid::setEditStep(int step)
{
    if (step == editStep)
        return;
    repaint(columnBounds(editStep));
    editStep = step;
    repaint(columnBounds(editStep));
}

int StepGrid::visibleSteps() const
{
    // At least 16 columns: with fewer steps the rest of the row is shown, but not available.
    return hasProgram ? std::max(minVisibleSteps, std::clamp(program.numSteps, 1, mnArpee::maxSteps))
                      : minVisibleSteps;
}

int StepGrid::activeSteps() const
{
    // 0 steps = MIDI thru and 1 step = quantized thru: the pattern is not used.
    return hasProgram && program.numSteps > 1 ? std::min(program.numSteps, mnArpee::maxSteps) : 0;
}

juce::Rectangle<int> StepGrid::columnBounds(int step) const
{
    // Cells of the column plus the header strip with the playhead arrow.
    auto top = cellRect(step, 0);
    auto bottom = laneBox(step, numLaneRows - 1);
    return top.getUnion(bottom).withTop(0).expanded(4, 2);
}

int StepGrid::rowForStep(const mnArpee::Step& s) const
{
    using mnArpee::StepType;
    if (s.type == StepType::Off || s.type == StepType::Rest)
        return -1;
    if (s.type == StepType::Random)
        return randomRow;
    return rowForKey(s.keyIndex);
}

juce::Rectangle<int> StepGrid::boardArea() const
{
    auto area = getLocalBounds().reduced(8);
    area.removeFromLeft(labelW);
    area.removeFromTop(gutterTop);
    area.removeFromBottom(numLaneRows * laneRowH + laneGap);
    return area;
}

juce::Rectangle<int> StepGrid::laneBox(int step, int laneRow) const
{
    // Same columns as the matrix, below its last row.
    auto cell = cellRect(step, 0);
    const auto board = boardArea();
    return { cell.getX(), board.getBottom() + laneGap + laneRow * laneRowH + cellGap / 2,
             cell.getWidth(), laneRowH - cellGap };
}

juce::Rectangle<int> StepGrid::laneCaption(int laneRow) const
{
    const auto board = boardArea();
    return { 8, board.getBottom() + laneGap + laneRow * laneRowH + cellGap / 2,
             board.getX() - 8 - 6, laneRowH - cellGap };
}

juce::Rectangle<int> StepGrid::keysDice() const
{
    // In the corner above the row labels.
    return { 8, 2, laneRowH + 4, gutterTop };
}

juce::Rectangle<int> StepGrid::rootDice() const
{
    // At the left of the "Root" label.
    const auto cell = cellRect(0, rootRow);
    const int h = std::min(cell.getHeight(), laneRowH);
    return { 8, cell.getCentreY() - h / 2, laneRowH + 4, h };
}

juce::Rectangle<int> StepGrid::laneDice(int laneRow) const
{
    // At the left of the caption.
    const auto cap = laneCaption(laneRow);
    return cap.withWidth(cap.getHeight() + 4);
}

bool StepGrid::laneBoxAt(juce::Point<int> p, int& step, int& laneRow) const
{
    step = laneRow = -1;
    if (! hasProgram)
        return false;
    for (int s = 0; s < activeSteps(); ++s)
        for (int r = 0; r < numLaneRows; ++r)
            if (laneBox(s, r).expanded(cellGap / 2).contains(p))
            {
                step = s;
                laneRow = r;
                return true;
            }
    return false;
}

juce::Rectangle<int> StepGrid::cellRect(int step, int row) const
{
    auto board = boardArea();
    const int n = visibleSteps();
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
    if (! hasProgram)
        return;
    auto board = boardArea();
    const int n = visibleSteps();
    const int cw = board.getWidth() / n;
    const int ch = board.getHeight() / numRows;
    if (! board.contains(p))
        return;
    const int sx = (p.x - board.getX()) / cw;
    const int ry = (p.y - board.getY()) / ch;
    if (sx >= 0 && sx < std::min(n, activeSteps()) && ry >= 0 && ry < numRows)
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

    if (! hasProgram)
        return;

    const int n = visibleSteps();
    auto board = boardArea();

    // Column labels (every 4th step highlighted).
    g.setFont(mnArpee::fonts::text(16.0f, true));
    g.setColour(Palette::dimText());
    for (int s = 0; s < n; s += 4)
    {
        auto r = cellRect(s, 0);
        g.drawText(juce::String(s + 1), r.getX() - 4, 4, r.getWidth() + 8, 14,
                   juce::Justification::centredLeft);
    }

    static const char* rowNames[numRows] = { "Key 5", "Key 4", "Key 3", "Key 2", "Key 1", "Root", "Random" };
    for (int r = 0; r < numRows; ++r)
    {
        auto first = cellRect(0, r);
        g.setColour(r >= rootRow ? Palette::accent2() : Palette::dimText());
        g.drawText(rowNames[r], 8, first.getY(), labelW - 14, first.getHeight(),
                   juce::Justification::centredRight);
    }

    const auto clip = g.getClipBounds();
    const auto markFont = mnArpee::fonts::text(15.0f, true);
    for (int s = 0; s < n; ++s)
    {
        if (! clip.intersects(columnBounds(s)))
            continue; // partial repaints (playhead) only touch their own columns

        if (s >= activeSteps())
        {
            // Not part of the pattern: faint empty cells, nothing to edit.
            g.setColour(Palette::cellOff().withAlpha(0.3f));
            for (int r = 0; r < numRows; ++r)
                g.fillRoundedRectangle(cellRect(s, r).toFloat(), 3.0f);
            g.setColour(Palette::cardHi().withAlpha(0.3f));
            for (int lr = 0; lr < numLaneRows; ++lr)
                g.fillRoundedRectangle(laneBox(s, lr).toFloat(), 3.0f);
            continue;
        }

        const auto& step = program.steps[(size_t) s];
        const int stepRow = rowForStep(step);
        const bool isPlay = (s == playStep);

        const bool silent = step.type == mnArpee::StepType::Off || step.type == mnArpee::StepType::Rest;
        for (int r = 0; r < numRows; ++r)
        {
            auto cell = cellRect(s, r);

            // Step column highlight (playhead glow strip + edited column).
            if (isPlay)
            {
                g.setColour(Palette::playHead());
                g.fillRect(cell.expanded(0, 1));
            }

            const bool active = r == stepRow;
            juce::Colour col;
            if (active)
            {
                // Intensity proportional to the step's velocity.
                const float vel = juce::jlimit(0.0f, 1.0f, step.velocity / 127.0f);
                col = Palette::accent().withBrightness(0.45f + 0.30f * vel);
            }
            else if (s == hoverStep)
                col = Palette::cellOff().brighter(0.15f);
            else
                col = Palette::cellOff();

            g.setColour(col);
            g.fillRoundedRectangle(cell.toFloat(), 3.0f);

            if (active && step.type == mnArpee::StepType::Tie && s > 0)
            {
                // Tie: a bridge into the previous column.
                g.setColour(col.darker(0.2f));
                g.fillRect(cell.getX() - cellGap - 2, cell.getCentreY() - 2, cellGap + 4, 4);
            }

            const char* mark = "";
            juce::Colour markCol = Palette::bg().withAlpha(0.85f);
            if (r == 0 && step.type == mnArpee::StepType::Off)
            {
                mark = "x"; // nothing plays
                markCol = Palette::danger().withAlpha(0.65f);
            }
            else if (r == 0 && step.type == mnArpee::StepType::Rest)
            {
                mark = "-"; // the previous note continues
                markCol = Palette::accent().withAlpha(0.8f);
            }
            else if (active)
            {
                mark = step.type == mnArpee::StepType::Random   ? "?"
                     : step.type == mnArpee::StepType::Tie      ? "~"
                                                                : "";
            }
            if (mark[0] != 0)
            {
                g.setColour(markCol);
                g.setFont(markFont);
                g.drawText(mark, cell, juce::Justification::centred);
            }
        }

        // Value lanes: one box per step and lane.
        for (int lr = 0; lr < numLaneRows; ++lr)
        {
            const auto box = laneBox(s, lr);
            const auto laneId = laneOfRow[lr];
            juce::String text;
            bool dim = false;
            switch (laneId)
            {
                case Lane::Type:
                    switch (step.type)
                    {
                        case mnArpee::StepType::Off:    text = "Off"; break;
                        case mnArpee::StepType::Rest:   text = "Rst"; break;
                        case mnArpee::StepType::Tie:    text = "Tie"; break;
                        case mnArpee::StepType::Random: text = "Rnd"; break;
                        default:                        text = "Nrm"; break;
                    }
                    break;
                case Lane::Key: break; // the keys are the matrix
                case Lane::Ratchet:
                    text = juce::String(step.ratchet);
                    dim = silent;
                    break;
                case Lane::Gate:
                    text = juce::String(step.gateScalePercent);
                    dim = silent;
                    break;
                case Lane::Root: break; // only the dice of the Root row
                case Lane::Octave:
                    text = step.octave > 0 ? "+" + juce::String(step.octave) : juce::String(step.octave);
                    dim = silent;
                    break;
                case Lane::Velocity:
                    text = juce::String(step.velocity);
                    dim = silent;
                    break;
            }
            g.setColour(Palette::cardHi());
            g.fillRoundedRectangle(box.toFloat(), 3.0f);
            g.setColour(dim ? Palette::dimText().withAlpha(0.5f) : Palette::text());
            g.setFont(mnArpee::fonts::text(visibleSteps() > 32 ? 11.0f : 15.0f, true));
            g.drawText(text, box, juce::Justification::centred, false);
        }
    }

    // Lane captions, each with a dice that fills the lane randomly.
    g.setFont(mnArpee::fonts::text(16.0f, true));
    for (int lr = 0; lr < numLaneRows; ++lr)
    {
        const auto cap = laneCaption(lr);
        g.setColour(Palette::text());
        g.drawText(laneCaptions[lr], cap.reduced(4, 0), juce::Justification::centredRight);

        drawDice(g, laneDice(lr).toFloat().reduced(2.0f));
    }

    drawDice(g, keysDice().toFloat().reduced(2.0f));
    drawDice(g, rootDice().toFloat().reduced(2.0f));

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

void StepGrid::clickAt(juce::Point<int> p)
{
    int step = -1, row = -1;
    cellAt(p, step, row);
    if (step < 0 || row < 0 || ! clicked)
        return;
    if (step == lastClickStep && row == lastClickRow)
        return; // dragging inside one cell
    lastClickStep = step;
    lastClickRow = row;
    clicked(step, row);
}

void StepGrid::mouseDown(const juce::MouseEvent& e)
{
    lastClickStep = lastClickRow = -1;
    dragStep = dragLaneRow = -1;

    if (e.mods.isPopupMenu())
    {
        // The dice: the options of its random values.
        if (randomOptionsRequested)
        {
            if (keysDice().expanded(2).contains(e.getPosition()))
            {
                randomOptionsRequested(Lane::Key);
                return;
            }
            for (int lr = 0; lr < numLaneRows; ++lr)
                if (laneDice(lr).expanded(2).contains(e.getPosition()))
                {
                    randomOptionsRequested(laneOfRow[lr]);
                    return;
                }
        }
        int step = -1, laneRow = -1, row = -1;
        if (contextRequested && laneBoxAt(e.getPosition(), step, laneRow))
            contextRequested(step, laneOfRow[laneRow], -1);
        else if (contextRequested)
        {
            cellAt(e.getPosition(), step, row);
            if (step >= 0 && row >= 0)
                contextRequested(step, Lane::Key, row);
        }
        return;
    }

    if (keysDice().expanded(2).contains(e.getPosition()))
    {
        if (randomizeRequested)
            randomizeRequested(Lane::Key);
        return;
    }
    if (rootDice().expanded(2).contains(e.getPosition()))
    {
        if (randomizeRequested)
            randomizeRequested(Lane::Root);
        return;
    }
    for (int lr = 0; lr < numLaneRows; ++lr)
        if (laneDice(lr).expanded(2).contains(e.getPosition()))
        {
            if (randomizeRequested)
                randomizeRequested(laneOfRow[lr]);
            return;
        }

    int step = -1, laneRow = -1;
    if (laneBoxAt(e.getPosition(), step, laneRow))
    {
        dragStep = step;
        dragLaneRow = laneRow;
        dragLastY = e.getPosition().y;
        dragRemainder = 0;
        if (adjusted)
            adjusted(step, laneOfRow[laneRow], 0); // selects the step
        return;
    }
    clickAt(e.getPosition());
}

void StepGrid::mouseUp(const juce::MouseEvent&)
{
    dragStep = dragLaneRow = -1;
}

void StepGrid::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int step = -1, laneRow = -1;
    if (! laneBoxAt(e.getPosition(), step, laneRow) || ! adjusted || wheel.deltaY == 0.0f)
        return;
    adjusted(step, laneOfRow[laneRow], wheel.deltaY > 0.0f ? 1 : -1);
}

void StepGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (dragStep >= 0)
    {
        // Dragging up raises the value.
        dragRemainder += dragLastY - e.getPosition().y;
        dragLastY = e.getPosition().y;
        const auto dragLane = laneOfRow[dragLaneRow];
        const int perUnit = dragLane == Lane::Velocity ? 1 : dragLane == Lane::Gate ? 3 : dragPixelsPerUnit;
        const int units = dragRemainder / perUnit;
        if (units != 0 && adjusted)
        {
            dragRemainder -= units * perUnit;
            adjusted(dragStep, laneOfRow[dragLaneRow], units);
        }
        return;
    }

    // Dragging over the matrix does nothing: a click toggles, which would flip every cell passed.
}

void StepGrid::mouseMove(const juce::MouseEvent& e)
{
    int step = -1, row = -1;
    cellAt(e.getPosition(), step, row);
    if (step == hoverStep)
        return;
    if (hoverStep >= 0)
        repaint(columnBounds(hoverStep));
    hoverStep = step;
    if (hoverStep >= 0)
        repaint(columnBounds(hoverStep));
}

void StepGrid::mouseExit(const juce::MouseEvent&)
{
    if (hoverStep >= 0)
        repaint(columnBounds(hoverStep));
    hoverStep = -1;
}
