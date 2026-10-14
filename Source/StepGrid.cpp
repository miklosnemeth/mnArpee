#include "StepGrid.h"

#include "Fonts.h"

namespace
{
constexpr int labelW = 80;   // row label strip on the left
constexpr int stepH = 24;    // step row height
constexpr int gutterTop = 20; // column labels (1,5,9,...)
constexpr int cellGap = 3;
constexpr int laneRowH = 22;  // value lane row height
constexpr int laneGap = 8;    // between the matrix and the value lanes
constexpr int numLaneRows = 4;
constexpr int dragPixelsPerUnit = 8; // type, key and octave; the velocity moves one unit per pixel

// Value lanes, top to bottom (as in BlueARP): step type, key select, octave, velocity.
constexpr StepGrid::Lane laneOfRow[numLaneRows] = { StepGrid::Lane::Type, StepGrid::Lane::Key,
                                                    StepGrid::Lane::Octave, StepGrid::Lane::Velocity };
constexpr const char* laneCaptions[numLaneRows] = { "TYPE", "KEY", "OCTAVE", "VELOCITY" };
}

StepGrid::StepGrid()
{
    setWantsKeyboardFocus(false);
    setBufferedToImage(true);
}

int StepGrid::rowsFor(Lane l)
{
    switch (l)
    {
        case Lane::Type:     return 5;
        case Lane::Octave:   return 7;
        case Lane::Velocity: return velocityLevels;
        default:             return 7;
    }
}

mnArpee::StepType StepGrid::typeForRow(int row)
{
    using mnArpee::StepType;
    constexpr StepType order[] = { StepType::Normal, StepType::Random, StepType::Tie,
                                   StepType::Rest, StepType::Off };
    return order[std::clamp(row, 0, 4)];
}

void StepGrid::setLane(Lane l)
{
    if (l == lane)
        return;
    lane = l;
    repaint();
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
    return hasProgram ? std::clamp(program.numSteps, 1, mnArpee::maxSteps) : 16;
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
    switch (lane)
    {
        case Lane::Type:
            for (int r = 0; r < 5; ++r)
                if (typeForRow(r) == s.type)
                    return r;
            return 0;
        case Lane::Octave:
            return 3 - std::clamp((int) s.octave, -3, 3);
        case Lane::Velocity:
            // Top row of the bar: level 1..8 of the velocity.
            return velocityLevels - std::clamp((s.velocity * velocityLevels + 126) / 127, 1, velocityLevels);
        default:
            break;
    }
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

bool StepGrid::laneBoxAt(juce::Point<int> p, int& step, int& laneRow) const
{
    step = laneRow = -1;
    if (! hasProgram)
        return false;
    for (int s = 0; s < visibleSteps(); ++s)
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
    const int ch = board.getHeight() / numRows();
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
    const int ch = board.getHeight() / numRows();
    if (! board.contains(p))
        return;
    const int sx = (p.x - board.getX()) / cw;
    const int ry = (p.y - board.getY()) / ch;
    if (sx >= 0 && sx < n && ry >= 0 && ry < numRows())
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
    g.setFont(mnArpee::fonts::text(14.0f));
    g.setColour(Palette::dimText());
    for (int s = 0; s < n; s += 4)
    {
        auto r = cellRect(s, 0);
        g.drawText(juce::String(s + 1), r.getX() - 4, 4, r.getWidth() + 8, 14,
                   juce::Justification::centredLeft);
    }

    const int rows = numRows();
    for (int r = 0; r < rows; ++r)
    {
        juce::String name;
        bool highlight = false;
        switch (lane)
        {
            case Lane::Type:
                name = juce::StringArray { "Normal", "Random", "Tie", "Rest", "Off" }[r];
                break;
            case Lane::Octave:
                name = octaveForRow(r) > 0 ? "+" + juce::String(octaveForRow(r)) : juce::String(octaveForRow(r));
                highlight = octaveForRow(r) == 0;
                break;
            case Lane::Velocity:
                name = juce::String(velocityForRow(r));
                break;
            default:
                name = juce::StringArray { "Key 5", "Key 4", "Key 3", "Key 2", "Key 1", "Root", "Random" }[r];
                highlight = r >= rootRow;
                break;
        }
        auto first = cellRect(0, r);
        g.setColour(highlight ? Palette::accent2() : Palette::dimText());
        g.drawText(name, 8, first.getY(), labelW - 14, first.getHeight(),
                   juce::Justification::centredRight);
    }

    const auto clip = g.getClipBounds();
    const auto markFont = mnArpee::fonts::text(13.0f, true);
    for (int s = 0; s < n; ++s)
    {
        if (! clip.intersects(columnBounds(s)))
            continue; // partial repaints (playhead) only touch their own columns

        const auto& step = program.steps[(size_t) s];
        const int stepRow = rowForStep(step);
        const bool isEdit = (s == editStep);
        const bool isPlay = (s == playStep);

        const bool silent = step.type == mnArpee::StepType::Off || step.type == mnArpee::StepType::Rest;
        for (int r = 0; r < rows; ++r)
        {
            auto cell = cellRect(s, r);

            // Step column highlight (playhead glow strip + edited column).
            if (isPlay)
            {
                g.setColour(Palette::playHead());
                g.fillRect(cell.expanded(0, 1));
            }

            // Velocity lane: a bar from the bottom up to the step's level.
            const bool active = lane == Lane::Velocity ? r >= stepRow : r == stepRow;
            juce::Colour col;
            if (active)
            {
                // Intensity proportional to velocity in the highlight blue.
                const float vel = juce::jlimit(0.0f, 1.0f, step.velocity / 127.0f);
                col = Palette::accent().withBrightness(0.45f + 0.30f * vel);
                if (lane != Lane::Key && lane != Lane::Type && silent)
                    col = col.withAlpha(0.35f); // the step plays nothing
            }
            else if (s == hoverStep)
                col = Palette::cellOff().brighter(0.15f);
            else
                col = Palette::cellOff();
            if (isEdit)
                col = col.brighter(0.12f);

            g.setColour(col);
            g.fillRoundedRectangle(cell.toFloat(), 3.0f);

            if (lane == Lane::Key && active && step.type == mnArpee::StepType::Tie && s > 0)
            {
                // Tie: a bridge into the previous column.
                g.setColour(col.darker(0.2f));
                g.fillRect(cell.getX() - cellGap - 2, cell.getCentreY() - 2, cellGap + 4, 4);
            }

            const char* mark = "";
            juce::Colour markCol = Palette::bg().withAlpha(0.85f);
            if (lane != Lane::Key)
            {
                // Type lane: Random and Tie show their mark, the others need none.
                if (lane == Lane::Type && active && step.type == mnArpee::StepType::Random)
                    mark = "?";
                else if (lane == Lane::Type && active && step.type == mnArpee::StepType::Tie)
                    mark = "~";
            }
            else if (r == 0 && step.type == mnArpee::StepType::Off)
            {
                mark = "x"; // nothing plays
                markCol = Palette::danger().withAlpha(0.65f);
            }
            else if (r == 0 && step.type == mnArpee::StepType::Rest)
            {
                mark = "-"; // the previous note continues
                markCol = Palette::accent().withAlpha(0.8f);
            }
            else if (active && lane == Lane::Key)
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

            if (isEdit)
            {
                g.setColour(Palette::accent2().withAlpha(0.9f));
                g.drawRoundedRectangle(cell.toFloat(), 3.0f, 1.4f);
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
                case Lane::Key:
                    text = step.keyIndex == mnArpee::rootKeyIndex ? juce::String("Rt")
                                                                  : "K" + juce::String(step.keyIndex + 1);
                    dim = silent || step.type == mnArpee::StepType::Random; // the key makes no difference
                    break;
                case Lane::Octave:
                    text = step.octave > 0 ? "+" + juce::String(step.octave) : juce::String(step.octave);
                    dim = silent;
                    break;
                case Lane::Velocity:
                    text = juce::String(step.velocity);
                    dim = silent;
                    break;
            }
            g.setColour(laneId == lane ? Palette::cardHi().brighter(0.15f) : Palette::cardHi());
            g.fillRoundedRectangle(box.toFloat(), 3.0f);
            g.setColour(dim ? Palette::dimText().withAlpha(0.5f) : Palette::text());
            g.setFont(mnArpee::fonts::text(visibleSteps() > 32 ? 10.0f : 13.0f, true));
            g.drawText(text, box, juce::Justification::centred, false);
            if (isEdit)
            {
                g.setColour(Palette::accent2().withAlpha(0.9f));
                g.drawRoundedRectangle(box.toFloat(), 3.0f, 1.2f);
            }
        }
    }

    // Lane captions: the selected one is highlighted, a click selects the lane.
    g.setFont(mnArpee::fonts::text(14.0f, true));
    for (int lr = 0; lr < numLaneRows; ++lr)
    {
        const auto cap = laneCaption(lr);
        if (laneOfRow[lr] == lane)
        {
            g.setColour(Palette::accent2());
            g.fillRoundedRectangle(cap.toFloat(), 3.0f);
        }
        g.setColour(laneOfRow[lr] == lane ? Palette::bg() : Palette::text());
        g.drawText(laneCaptions[lr], cap.reduced(4, 0), juce::Justification::centredRight);
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

    for (int lr = 0; lr < numLaneRows; ++lr)
        if (laneCaption(lr).expanded(2).contains(e.getPosition()))
        {
            setLane(laneOfRow[lr]);
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
        const int perUnit = laneOfRow[dragLaneRow] == Lane::Velocity ? 1 : dragPixelsPerUnit;
        const int units = dragRemainder / perUnit;
        if (units != 0 && adjusted)
        {
            dragRemainder -= units * perUnit;
            adjusted(dragStep, laneOfRow[dragLaneRow], units);
        }
        return;
    }

    // Painting across the steps only makes sense for the value lanes; in the Key lane a
    // click toggles, which would flip every cell the mouse passes.
    if (lane != Lane::Key)
        clickAt(e.getPosition());
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
