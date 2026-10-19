#include "PluginEditor.h"
#include "PluginProcessor.h"

#include "Fonts.h"

#ifndef MNARPEE_VERSION_STRING
 #define MNARPEE_VERSION_STRING "dev"
#endif

//==============================================================================
// MnArpeeLookAndFeel
//==============================================================================
MnArpeeLookAndFeel::MnArpeeLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, Pal::cardHi());
    setColour(juce::ComboBox::textColourId, Pal::text());
    setColour(juce::ComboBox::outlineColourId, Pal::border());
    setColour(juce::ComboBox::arrowColourId, Pal::dimText());
    setColour(juce::PopupMenu::backgroundColourId, Pal::cardHi());
    setColour(juce::PopupMenu::textColourId, Pal::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Pal::accent().withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, Pal::text());
    setColour(juce::Label::textColourId, Pal::dimText());
    setColour(juce::Slider::textBoxTextColourId, Pal::text());
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::trackColourId, Pal::accent());
    setColour(juce::Slider::thumbColourId, Pal::text());
    setColour(juce::ToggleButton::textColourId, Pal::text());
    setColour(juce::ToggleButton::tickColourId, Pal::accent());
    setColour(juce::ToggleButton::tickDisabledColourId, Pal::border());
    setColour(juce::TextButton::buttonColourId, Pal::cardHi());
    setColour(juce::TextButton::textColourOffId, Pal::text());
    setColour(juce::TextEditor::backgroundColourId, Pal::bg());
    setColour(juce::TextEditor::textColourId, Pal::text());
    setColour(juce::TextEditor::outlineColourId, Pal::border());
    setColour(juce::TextEditor::focusedOutlineColourId, Pal::accent());
    setColour(juce::TextEditor::highlightColourId, Pal::accent().withAlpha(0.35f));
    setColour(juce::CaretComponent::caretColourId, Pal::accent());
}

void MnArpeeLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                      bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(r, 5.0f, 1.0f);

    // Up/down arrows (small chevron).
    auto ar = r.removeFromRight(18.0f).reduced(5.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    juce::Path p;
    p.startNewSubPath(ar.getX(), ar.getCentreY() - 2.0f);
    p.lineTo(ar.getCentreX(), ar.getCentreY() + 2.0f);
    p.lineTo(ar.getRight(), ar.getCentreY() - 2.0f);
    g.strokePath(p, juce::PathStrokeType(1.6f));
}

void MnArpeeLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h)
{
    g.fillAll(Pal::cardHi());
    g.setColour(Pal::border());
    g.drawRect(0, 0, w, h, 1);
}

void MnArpeeLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
                                          bool highlighted, bool)
{
    auto r = btn.getLocalBounds().toFloat().reduced(1.0f);
    auto box = r.removeFromLeft(r.getHeight());
    const bool on = btn.getToggleState();
    g.setColour(on ? Pal::accent() : Pal::cardHi());
    g.fillRoundedRectangle(box, box.getHeight() * 0.35f);
    g.setColour(highlighted ? Pal::accent().brighter(0.2f) : Pal::border());
    g.drawRoundedRectangle(box, box.getHeight() * 0.35f, 1.0f);
    if (on)
    {
        g.setColour(Pal::bg());
        auto tick = box.reduced(box.getWidth() * 0.3f);
        juce::Path p;
        p.startNewSubPath(tick.getX(), tick.getCentreY());
        p.lineTo(tick.getCentreX(), tick.getBottom());
        p.lineTo(tick.getRight(), tick.getY());
        g.strokePath(p, juce::PathStrokeType(2.0f));
    }
    g.setColour(Pal::text());
    g.setFont(mnArpee::fonts::text(17.0f, true));
    g.drawText(btn.getButtonText(), r.reduced(6.0f, 0.0f), juce::Justification::centredLeft);
}

juce::Font MnArpeeLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return mnArpee::fonts::text(juce::jmin(17.0f, (float) box.getHeight() * 0.62f), true);
}

juce::Font MnArpeeLookAndFeel::getPopupMenuFont() { return mnArpee::fonts::text(17.0f, true); }

juce::Font MnArpeeLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return mnArpee::fonts::text(juce::jmin(17.0f, (float) buttonHeight * 0.62f), true);
}


juce::Font MnArpeeLookAndFeel::getSliderPopupFont(juce::Slider&) { return mnArpee::fonts::text(16.0f, true); }

void MnArpeeLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearBar && style != juce::Slider::LinearBarVertical)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    // Value box: flat, the value text is drawn over it by the slider's own label.
    const auto r = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    g.setColour(Pal::cardHi());
    g.fillRoundedRectangle(r, 5.0f);
}

juce::Label* MnArpeeLookAndFeel::createSliderTextBox(juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(s);
    l->setFont(mnArpee::fonts::text(15.0f, true));
    l->setColour(juce::Label::textColourId, Pal::text());
    l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
// HistoryButton
//==============================================================================
void HistoryButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(down ? Pal::cardHi().brighter(0.2f) : highlighted ? Pal::cardHi().brighter(0.1f) : Pal::cardHi());
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(Pal::border());
    g.drawRoundedRectangle(r, 5.0f, 1.0f);

    // A curved arrow: the arc, and the head at its end (mirrored for redo).
    const auto c = r.getCentre();
    const float rad = std::min(r.getWidth(), r.getHeight()) * 0.28f;
    juce::Path arc;
    arc.addCentredArc(c.x, c.y + 1.0f, rad, rad, 0.0f, -2.3f, 1.6f, true);
    juce::Path head;
    head.addTriangle(-4.5f, 0.0f, 4.5f, 0.0f, 0.0f, -6.0f);
    g.setColour(isEnabled() ? Pal::text() : Pal::dimText().withAlpha(0.35f));
    juce::AffineTransform flip = redo ? juce::AffineTransform::scale(-1.0f, 1.0f, c.x, c.y) : juce::AffineTransform();
    g.strokePath(arc, juce::PathStrokeType(2.0f), flip);
    const auto tip = arc.getPointAlongPath(0.0f).transformedBy(flip);
    head = head.createPathWithRoundedCorners(1.0f);
    g.fillPath(head, juce::AffineTransform::rotation(redo ? 0.6f : -0.6f).translated(tip.x, tip.y));
}

//==============================================================================
// The random options of a lane: a card over the dimmed editor with the range of the random values (two bars
// to drag up and down) and the buttons Randomize, Reset range, Reset lane and Close.
namespace
{
class RangePanel : public juce::Component
{
public:
    RangePanel(const juce::String& titleIn, bool hasRangeIn, int boundLo, int boundHi, mnArpee::RandomRange current,
               std::function<juce::String(int)> textIn, double pixelsPerUnit, std::function<void(mnArpee::RandomRange)> changedIn,
               std::function<void()> randomizeIn, std::function<void()> resetRangeIn, std::function<void()> resetLaneIn,
               std::function<void()> closeIn)
        : title(titleIn), hasRange(hasRangeIn), text(std::move(textIn)), changed(std::move(changedIn)),
          randomize(std::move(randomizeIn)), resetRange(std::move(resetRangeIn)), resetLane(std::move(resetLaneIn)),
          close(std::move(closeIn))
    {
        if (hasRange)
        {
            for (auto* bar : { &minBar, &maxBar })
            {
                addAndMakeVisible(*bar); // first: the value label takes the colours of the look and feel
                bar->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
                bar->setSliderStyle(juce::Slider::LinearBarVertical);
                bar->setSliderSnapsToMousePosition(false);
                bar->setRange(boundLo, boundHi, 1.0);
                bar->setPixelsPerUnit(pixelsPerUnit);
                bar->setTextBoxStyle(juce::Slider::TextBoxLeft, true, 80, 20);
                bar->setTextBoxIsEditable(false);
            }
            minBar.setValue(current.lo, juce::dontSendNotification);
            maxBar.setValue(current.hi, juce::dontSendNotification);
            minBar.textFromValueFunction = [this](double v) { return "MIN " + text((int) std::lround(v)); };
            maxBar.textFromValueFunction = [this](double v) { return "MAX " + text((int) std::lround(v)); };
            minBar.updateText();
            maxBar.updateText();
            // The minimum never goes above the maximum, and the other way round.
            minBar.onValueChange = [this]
            {
                if (minBar.getValue() > maxBar.getValue())
                    maxBar.setValue(minBar.getValue(), juce::dontSendNotification), maxBar.updateText();
                report();
            };
            maxBar.onValueChange = [this]
            {
                if (maxBar.getValue() < minBar.getValue())
                    minBar.setValue(maxBar.getValue(), juce::dontSendNotification), minBar.updateText();
                report();
            };
        }
        const char* names[] = { "Randomize", "Reset range", "Reset lane", "Close" };
        for (int i = 0; i < 4; ++i)
        {
            if (i == 1 && ! hasRange)
                continue; // nothing to reset
            auto b = std::make_unique<juce::TextButton>(names[i]);
            b->onClick = [this, i]
            {
                if (i == 0)
                    randomize();
                else if (i == 1)
                    resetRange();
                else if (i == 2)
                    resetLane();
                else
                    close();
            };
            if (i == 0)
                b->setColour(juce::TextButton::buttonColourId, Pal::accent().darker(0.55f));
            addAndMakeVisible(*b);
            buttons.push_back(std::move(b));
        }
        setWantsKeyboardFocus(false);
    }

    void setRange(mnArpee::RandomRange range)
    {
        minBar.setValue(range.lo, juce::dontSendNotification);
        maxBar.setValue(range.hi, juce::dontSendNotification);
        minBar.updateText();
        maxBar.updateText();
    }

    void resized() override
    {
        const int panelW = std::min(500, getWidth() - 40);
        const int panelH = 20 + 28 + 12 + (hasRange ? 34 + 14 : 20) + 36 + 20;
        panel = juce::Rectangle<int>(panelW, panelH).withCentre({ getWidth() / 2, getHeight() / 2 });
        auto r = panel.reduced(20, 20);
        titleArea = r.removeFromTop(28);
        r.removeFromTop(12);
        if (hasRange)
        {
            auto bars = r.removeFromTop(34);
            minBar.setBounds(bars.removeFromLeft(bars.getWidth() / 2 - 6));
            bars.removeFromLeft(12);
            maxBar.setBounds(bars);
            r.removeFromTop(14);
        }
        else
        {
            r.removeFromTop(20);
        }
        auto row = r.removeFromTop(36);
        for (int i = (int) buttons.size() - 1; i >= 0; --i)
        {
            const int w = std::max(92, juce::GlyphArrangement::getStringWidthInt(mnArpee::fonts::text(17.0f, true),
                                                                                  buttons[(size_t) i]->getButtonText()) + 32);
            buttons[(size_t) i]->setBounds(row.removeFromRight(w));
            row.removeFromRight(8);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(getLocalBounds());
        g.setColour(Pal::card());
        g.fillRoundedRectangle(panel.toFloat(), 8.0f);
        g.setColour(Pal::border());
        g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 8.0f, 1.0f);
        g.setColour(Pal::accent());
        g.setFont(mnArpee::fonts::text(21.0f, true));
        g.drawText(title, titleArea, juce::Justification::centredLeft);
        if (! hasRange)
        {
            g.setColour(Pal::dimText());
            g.setFont(mnArpee::fonts::text(15.0f, true));
            g.drawText("The step types have no range.", panel.reduced(20, 0).withTrimmedTop(60).withHeight(20),
                       juce::Justification::centredLeft);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (! panel.contains(e.getPosition()))
            close();
    }

private:
    void report()
    {
        changed({ (int) std::lround(minBar.getValue()), (int) std::lround(maxBar.getValue()) });
    }

    juce::String title;
    bool hasRange;
    std::function<juce::String(int)> text;
    std::function<void(mnArpee::RandomRange)> changed;
    std::function<void()> randomize, resetRange, resetLane, close;
    ValueBar minBar, maxBar;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    juce::Rectangle<int> panel, titleArea;
};
} // namespace

namespace
{
mnArpee::PatternLane patternLaneOf(StepGrid::Lane lane)
{
    switch (lane)
    {
        case StepGrid::Lane::Type: return mnArpee::PatternLane::Type;
        case StepGrid::Lane::Octave: return mnArpee::PatternLane::Octave;
        case StepGrid::Lane::Velocity: return mnArpee::PatternLane::Velocity;
        case StepGrid::Lane::Ratchet: return mnArpee::PatternLane::Ratchet;
        case StepGrid::Lane::Gate: return mnArpee::PatternLane::Gate;
        default: return mnArpee::PatternLane::Key;
    }
}
} // namespace

void MnArpeeAudioEditor::randomizeLane(StepGrid::Lane lane)
{
    auto edited = shownProgram;
    std::mt19937 rng((unsigned) juce::Time::currentTimeMillis());
    if (lane == StepGrid::Lane::Root)
        mnArpee::generateBassline(edited, rng);
    else
        mnArpee::randomizeLane(edited, patternLaneOf(lane), rng, proc.getRandomRange(patternLaneOf(lane)));
    applySteps(edited);
}

void MnArpeeAudioEditor::closeRangePanel()
{
    if (rangePanel == nullptr)
        return;
    rangePanel->setVisible(false);
    juce::Component::SafePointer<juce::Component> doomed(rangePanel.release());
    juce::MessageManager::callAsync([doomed] { delete doomed.getComponent(); });
}

void MnArpeeAudioEditor::showRangePanel(StepGrid::Lane lane)
{
    if (lane == StepGrid::Lane::Root)
        return; // the bass line has no range
    closeRangePanel();
    closeDialog();
    closePresetBrowser();

    const auto pl = patternLaneOf(lane);
    const auto full = mnArpee::fullRandomRange(pl);
    const bool hasRange = pl != mnArpee::PatternLane::Type;
    const char* titles[] = { "KEYS", "TYPE", "OCTAVE", "VELOCITY", "RATCHET", "GATE" };
    std::function<juce::String(int)> text = [pl](int v)
    {
        switch (pl)
        {
            case mnArpee::PatternLane::Key: return "Key " + juce::String(v + 1);
            case mnArpee::PatternLane::Octave: return v > 0 ? "+" + juce::String(v) : juce::String(v);
            case mnArpee::PatternLane::Gate: return juce::String(mnArpee::gateSteps[juce::jlimit(0, mnArpee::numGateSteps - 1, v)]) + "%";
            default: return juce::String(v);
        }
    };
    const double pixels = pl == mnArpee::PatternLane::Velocity ? 2.0 : 12.0;

    juce::Component::SafePointer<MnArpeeAudioEditor> safe(this);
    auto content = std::make_unique<RangePanel>(
        juce::String("Random ") + titles[(int) pl], hasRange, full.lo, full.hi, proc.getRandomRange(pl), text, pixels,
        [safe, pl](mnArpee::RandomRange range)
        {
            if (safe != nullptr)
                safe->proc.setRandomRange(pl, range);
        },
        [safe, lane] { if (safe != nullptr) safe->randomizeLane(lane); },
        [safe, pl]
        {
            if (safe == nullptr)
                return;
            safe->proc.setRandomRange(pl, mnArpee::defaultRandomRange(pl));
            if (auto* panel = dynamic_cast<RangePanel*>(safe->rangePanel.get()))
                panel->setRange(mnArpee::defaultRandomRange(pl));
        },
        [safe, pl]
        {
            if (safe == nullptr)
                return;
            auto edited = safe->shownProgram;
            mnArpee::resetLane(edited, pl);
            safe->applySteps(edited);
        },
        [safe] { if (safe != nullptr) safe->closeRangePanel(); });
    content->setBounds(getLocalBounds());
    addAndMakeVisible(*content);
    content->toFront(true);
    rangePanel = std::move(content);
}

//==============================================================================
// ValueBar
//==================================================================================
void ValueBar::mouseDown(const juce::MouseEvent& e)
{
    valueOnMouseDown = getValue();
    juce::Slider::mouseDown(e); // starts the change gesture; it does not move the value
}

void ValueBar::mouseDrag(const juce::MouseEvent& e)
{
    if (! e.mouseWasDraggedSinceMouseDown())
        return;
    // Relative to where the drag started, from the vertical distance only.
    const double dy = e.getMouseDownPosition().y - e.position.y;
    setValue(valueOnMouseDown + dy / pixelsPerUnit, juce::sendNotificationSync);
}

void ValueBar::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    // Only the vertical wheel; one interval per notch, whatever the speed.
    if (! isEnabled() || wheel.deltaY == 0.0f || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        return;
    const double step = std::max(getInterval(), 1.0e-6);
    setValue(getValue() + (wheel.deltaY > 0.0f ? step : -step), juce::sendNotificationSync);
}

//==============================================================================
MnArpeeAudioEditor::MnArpeeAudioEditor(MnArpeeAudioProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&lnf);
    setSize(900, 560);
    setScaleFactor(proc.getZoom());
    setResizable(true, true);
    setResizeLimits(840, 480, 1600, 1000);

    addAndMakeVisible(grid);
    grid.onStepClicked([this](int step, int row) { onGridClicked(step, row); });
    grid.onLaneAdjusted([this](int step, StepGrid::Lane lane, int delta) { onLaneAdjusted(step, lane, delta); });
    grid.onContextRequested([this](int step, StepGrid::Lane lane, int row) { showPatternMenu(step, lane, row); });
    grid.onRandomizeRequested([this](StepGrid::Lane lane) { randomizeLane(lane); });
    grid.onRandomOptionsRequested([this](StepGrid::Lane lane) { showRangePanel(lane); });

    prevPresetButton.onClick = [this] { stepPreset(-1); };
    nextPresetButton.onClick = [this] { stepPreset(1); };
    prevPresetButton.setTooltip("Previous preset");
    nextPresetButton.setTooltip("Next preset");
    presetButton.onClick = [this] { showPresetBrowser(); };
    presetButton.setTooltip("Choose a preset");
    newButton.setButtonText("New");
    newButton.setTooltip("A new, empty pattern");
    newButton.onClick = [this] { newClicked(); };
    saveButton.setButtonText("Save");
    saveButton.setTooltip("Save the preset");
    saveButton.onClick = [this] { saveClicked(); };
    menuButton.setButtonText("Menu");
    menuButton.setTooltip("Save as, revert, rename, import, export, delete");
    menuButton.onClick = [this] { showPresetMenu(); };
    undoButton.setTooltip("Undo");
    redoButton.setTooltip("Redo");
    undoButton.onClick = [this] { proc.undo(); };
    redoButton.onClick = [this] { proc.redo(); };
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);
    for (auto* b : { static_cast<juce::Component*>(&prevPresetButton), static_cast<juce::Component*>(&presetButton),
                     static_cast<juce::Component*>(&nextPresetButton), static_cast<juce::Component*>(&newButton),
                     static_cast<juce::Component*>(&saveButton), static_cast<juce::Component*>(&menuButton) })
        addAndMakeVisible(*b);

    auto signedValue = [](double v) { return (v > 0.0 ? "+" : "") + juce::String((int) std::lround(v)); };
    auto& apvts = proc.getApvts();
    rateAtt = std::make_unique<SliderAttachment>(apvts, "rate", rateSlider);
    octaveAtt = std::make_unique<SliderAttachment>(apvts, "octave", octaveSlider);
    gateAtt = std::make_unique<SliderAttachment>(apvts, "gate", gateSlider);
    swingAtt = std::make_unique<SliderAttachment>(apvts, "swing", swingSlider);
    stepsAtt = std::make_unique<SliderAttachment>(apvts, "steps", stepsSlider);

    // The attachments come first: they set their own value text, which is replaced here.
    setupValueBar(rateSlider, [](double v)
                  { return "RATE " + juce::String(mnArpee::syncRateName((mnArpee::SyncRate) juce::jlimit(
                                         0, mnArpee::numSyncRates - 1, (int) std::lround(v)))); },
                  4.0, 8.0);
    setupValueBar(octaveSlider, [signedValue](double v) { return "OCT " + signedValue(v); }, 0.0, 14.0);
    setupValueBar(gateSlider, [](double v) { return "GATE " + juce::String((int) std::lround(v)) + "%"; }, 80.0, 2.0);
    setupValueBar(swingSlider, [signedValue](double v) { return "SWING " + signedValue(v) + "%"; }, 0.0, 2.5);
    setupValueBar(stepsSlider, [](double v) { return "STEPS " + juce::String((int) std::lround(v)); }, 16.0, 7.0);

    addAndMakeVisible(latchButton);
    latchButton.setButtonText("Latch");
    latchAtt = std::make_unique<ButtonAttachment>(apvts, "latch", latchButton);

    reloadProgram();
    refreshPresetUi();
    startTimerHz(30);
}

MnArpeeAudioEditor::~MnArpeeAudioEditor()
{
    stopTimer();
    presetBrowser.reset();
    dialog.reset();
    rangePanel.reset();
    setLookAndFeel(nullptr);
}

void MnArpeeAudioEditor::setupValueBar(ValueBar& s, std::function<juce::String(double)> text,
                                       double defaultValue, double pixelsPerUnit)
{
    // Added first: the value label takes its colours from the look and feel it is created under,
    // otherwise it keeps the default grey outline.
    addAndMakeVisible(s);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    // Dragging up and down changes the value (a click does not jump to a position).
    s.setSliderStyle(juce::Slider::LinearBarVertical);
    s.setSliderSnapsToMousePosition(false);
    s.setPixelsPerUnit(pixelsPerUnit);
    s.setTextBoxStyle(juce::Slider::TextBoxLeft, true, 80, 20);
    s.setTextBoxIsEditable(false);
    s.setDoubleClickReturnValue(true, defaultValue);
    s.textFromValueFunction = std::move(text);
    s.updateText();
}

//==============================================================================
// Preset browser: a panel inside the editor (it never leaves the plugin window) with a search
// box, group and style filters and the list. A click outside the panel closes it.
namespace
{
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::TextEditor::Listener,
                      private juce::ComboBox::Listener
{
public:
    PresetBrowser(std::vector<mnArpee::Preset> presetsIn, int activeIndexIn, juce::Rectangle<int> anchorIn,
                  std::function<void(int)> chooseIn, std::function<void(int)> contextIn,
                  std::function<void()> closeIn, std::function<void(int, bool)> favoriteIn)
        : presets(std::move(presetsIn)), activeIndex(activeIndexIn), anchor(anchorIn),
          choose(std::move(chooseIn)), context(std::move(contextIn)), close(std::move(closeIn)),
          favorite(std::move(favoriteIn)),
          list("presets", this)
    {
        search.setTextToShowWhenEmpty("Search presets", Pal::dimText());
        search.setFont(mnArpee::fonts::text(17.0f, true));
        search.addListener(this);
        search.onEscapeKey = [this] { close(); };
        addAndMakeVisible(search);

        juce::StringArray groups, styles;
        for (const auto& p : presets)
        {
            if (! groups.contains(p.group))
                groups.add(p.group);
            for (const auto& style : mnArpee::presetStyles(p))
                if (! styles.contains(style))
                    styles.add(style);
        }
        styles.sort(true);
        groupBox.addItem("All groups", 1);
        groupBox.addItem("Favorites", 2);
        for (int i = 0; i < groups.size(); ++i)
            groupBox.addItem(groups[i], i + 3);
        styleBox.addItem("All styles", 1);
        for (int i = 0; i < styles.size(); ++i)
            styleBox.addItem(styles[i], i + 2);
        groupBox.setSelectedId(1, juce::dontSendNotification);
        styleBox.setSelectedId(1, juce::dontSendNotification);
        groupBox.addListener(this);
        styleBox.addListener(this);
        addAndMakeVisible(groupBox);
        addAndMakeVisible(styleBox);

        list.setRowHeight(32);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour(juce::ListBox::outlineColourId, Pal::border());
        list.setOutlineThickness(1);
        addAndMakeVisible(list);

        rebuild();
    }

    void parentSizeChanged() override {}

    void resized() override
    {
        // The panel sits under the preset name and stays inside the editor.
        const int w = std::min(520, getWidth() - 24);
        const int x = juce::jlimit(12, std::max(12, getWidth() - 12 - w), anchor.getCentreX() - w / 2);
        const int y = anchor.getBottom() + 6;
        const int h = std::max(160, std::min(540, getHeight() - y - 12));
        panel = { x, y, w, h };

        auto r = panel.reduced(10);
        search.setBounds(r.removeFromTop(30));
        r.removeFromTop(8);
        auto filters = r.removeFromTop(28);
        groupBox.setBounds(filters.removeFromLeft(filters.getWidth() / 2 - 4));
        filters.removeFromLeft(8);
        styleBox.setBounds(filters);
        r.removeFromTop(8);
        list.setBounds(r);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(getLocalBounds());
        g.setColour(Pal::card());
        g.fillRoundedRectangle(panel.toFloat(), 8.0f);
        g.setColour(Pal::border());
        g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 8.0f, 1.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (! panel.contains(e.getPosition()))
            close();
    }

    void visibilityChanged() override
    {
        if (isVisible())
        {
            search.grabKeyboardFocus();
            for (int row = 0; row < (int) shown.size(); ++row)
                if (shown[(size_t) row] == activeIndex)
                    list.scrollToEnsureRowIsOnscreen(row);
        }
    }

private:
    void rebuild()
    {
        const auto query = search.getText().trim();
        const bool favoritesOnly = groupBox.getSelectedId() == 2;
        const auto group = groupBox.getSelectedId() > 2 ? groupBox.getText() : juce::String();
        const auto style = styleBox.getSelectedId() > 1 ? styleBox.getText() : juce::String();
        shown.clear();
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            const auto& p = presets[(size_t) i];
            if (favoritesOnly && ! p.favorite)
                continue;
            if (group.isNotEmpty() && p.group != group)
                continue;
            if (style.isNotEmpty() && ! mnArpee::presetStyles(p).contains(style))
                continue;
            if (query.isNotEmpty() && ! (p.name.containsIgnoreCase(query) || p.group.containsIgnoreCase(query)
                                         || p.tags.containsIgnoreCase(query) || p.author.containsIgnoreCase(query)))
                continue;
            shown.push_back(i);
        }
        list.updateContent();
        list.repaint();
    }

    void textEditorTextChanged(juce::TextEditor&) override { rebuild(); }
    void textEditorReturnKeyPressed(juce::TextEditor&) override
    {
        if (! shown.empty())
            choose(shown.front());
    }
    void comboBoxChanged(juce::ComboBox*) override { rebuild(); }

    int getNumRows() override { return (int) shown.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= (int) shown.size())
            return;
        const int index = shown[(size_t) row];
        const auto& p = presets[(size_t) index];
        const bool isActive = index == activeIndex;
        if (selected || isActive)
        {
            g.setColour(Pal::accent().withAlpha(selected ? 0.28f : 0.16f));
            g.fillRoundedRectangle(2.0f, 1.0f, (float) width - 4.0f, (float) height - 2.0f, 4.0f);
        }
        const int nameW = (int) (width * 0.42f);
        const int groupW = (int) (width * 0.20f);

        // The star: a favourite is filled.
        juce::Path star;
        star.addStar({ 18.0f, (float) height * 0.5f }, 5, 4.5f, 9.0f);
        g.setColour(p.favorite ? Pal::accent() : Pal::dimText().withAlpha(0.45f));
        if (p.favorite)
            g.fillPath(star);
        else
            g.strokePath(star, juce::PathStrokeType(1.3f));

        g.setColour(isActive ? Pal::accent() : Pal::text());
        g.setFont(mnArpee::fonts::text(18.0f, true));
        g.drawText(p.name, 36, 0, nameW - 36, height, juce::Justification::centredLeft, true);
        g.setColour(Pal::dimText().brighter(0.15f));
        g.setFont(mnArpee::fonts::text(16.0f, true));
        g.drawText(p.group, nameW, 0, groupW, height, juce::Justification::centredLeft, true);
        g.drawText(p.tags.replace(",", ", "), nameW + groupW, 0, width - nameW - groupW - 8, height,
                   juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= (int) shown.size())
            return;
        const int index = shown[(size_t) row];
        if (e.mods.isPopupMenu())
        {
            context(index);
        }
        else if (e.x < 34)
        {
            // The star: toggle the favourite and stay in the list.
            presets[(size_t) index].favorite = ! presets[(size_t) index].favorite;
            favorite(index, presets[(size_t) index].favorite);
            rebuild();
        }
        else
        {
            choose(index);
        }
    }

    void returnKeyPressed(int row) override
    {
        if (row >= 0 && row < (int) shown.size())
            choose(shown[(size_t) row]);
    }

    std::vector<mnArpee::Preset> presets;
    std::vector<int> shown;  // library indices that match the filters
    int activeIndex;
    juce::Rectangle<int> anchor, panel;
    std::function<void(int)> choose, context;
    std::function<void()> close;
    std::function<void(int, bool)> favorite;
    juce::TextEditor search;
    juce::ComboBox groupBox, styleBox;
    juce::ListBox list;
};
} // namespace

void MnArpeeAudioEditor::closePresetBrowser()
{
    if (presetBrowser == nullptr)
        return;
    // It may be closing from inside its own callback: take it off the screen now, delete it later.
    presetBrowser->setVisible(false);
    juce::Component::SafePointer<juce::Component> doomed(presetBrowser.release());
    juce::MessageManager::callAsync([doomed] { delete doomed.getComponent(); });
}

void MnArpeeAudioEditor::showPresetBrowser()
{
    if (presetBrowser != nullptr)
    {
        closePresetBrowser();
        return;
    }
    auto content = std::make_unique<PresetBrowser>(
        proc.getPresetList(), proc.getActivePresetIndex(), presetButton.getBounds(),
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](int index)
        {
            if (safe == nullptr)
                return;
            safe->closePresetBrowser();
            safe->loadPresetChecked(index);
        },
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](int index)
        {
            if (safe != nullptr)
                safe->showPresetContextMenu(index);
        },
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)]
        {
            if (safe != nullptr)
                safe->closePresetBrowser();
        },
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](int index, bool on)
        {
            if (safe != nullptr)
                safe->proc.setPresetFavorite(index, on);
        });
    content->setBounds(getLocalBounds());
    addAndMakeVisible(*content);
    content->toFront(true);
    presetBrowser = std::move(content);
}

void MnArpeeAudioEditor::showPresetContextMenu(int libraryIndex)
{
    const auto list = proc.getPresetList();
    if (libraryIndex < 0 || libraryIndex >= (int) list.size())
        return;
    const auto& entry = list[(size_t) libraryIndex];
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lnf);
    menu.addItem(1, "Load");
    menu.addItem(2, "Duplicate");
    menu.addItem(3, "Rename...", ! entry.factory);
    menu.addItem(4, "Show in folder", entry.file.existsAsFile());
    menu.addItem(5, "Delete", ! entry.factory);
    menu.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this),
                       [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), libraryIndex, file = entry.file](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;
                           safe->closePresetBrowser();
                           switch (result)
                           {
                               case 1: safe->loadPresetChecked(libraryIndex); break;
                               case 2:
                                   safe->confirmLeavePreset([safe, libraryIndex]
                                                            {
                                                                if (safe == nullptr)
                                                                    return;
                                                                const int copy = safe->proc.duplicatePreset(libraryIndex);
                                                                if (copy >= 0)
                                                                    safe->proc.loadPreset(copy);
                                                            });
                                   break;
                               case 3: safe->renameDialog(libraryIndex); break;
                               case 4: file.revealToUser(); break;
                               case 5: safe->deleteDialog(libraryIndex); break;
                               default: break;
                           }
                       });
}

void MnArpeeAudioEditor::loadPresetChecked(int libraryIndex)
{
    confirmLeavePreset([safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), libraryIndex]
                       {
                           if (safe != nullptr)
                               safe->proc.loadPreset(libraryIndex);
                       });
}

void MnArpeeAudioEditor::stepPreset(int delta)
{
    const int n = (int) proc.getPresetList().size();
    if (n == 0)
        return;
    const int current = proc.getActivePresetIndex();
    const int next = current < 0 ? (delta > 0 ? 0 : n - 1) : (current + delta + n) % n;
    loadPresetChecked(next);
}

//==============================================================================
// The plugin's own dialogs: a card over a dimmed editor, so they match the look and stay inside the
// plugin window.
namespace
{
class DialogOverlay : public juce::Component
{
public:
    DialogOverlay(const juce::String& titleIn, const juce::String& messageIn,
                  const std::vector<std::pair<juce::String, juce::String>>& fields, const juce::StringArray& buttonNames,
                  int defaultButtonIn, int cancelButtonIn, std::function<void(int, juce::StringArray)> finishIn)
        : title(titleIn), message(messageIn), defaultButton(defaultButtonIn), cancelButton(cancelButtonIn),
          finish(std::move(finishIn))
    {
        setWantsKeyboardFocus(true);
        for (const auto& [label, initial] : fields)
        {
            auto l = std::make_unique<juce::Label>();
            l->setText(label, juce::dontSendNotification);
            l->setFont(mnArpee::fonts::text(15.0f, true));
            l->setColour(juce::Label::textColourId, Pal::dimText());
            addAndMakeVisible(*l);
            labels.push_back(std::move(l));

            auto e = std::make_unique<juce::TextEditor>();
            e->setFont(mnArpee::fonts::text(17.0f, true));
            e->setText(initial, false);
            e->setColour(juce::TextEditor::backgroundColourId, Pal::bg());
            e->setColour(juce::TextEditor::textColourId, Pal::text());
            e->setColour(juce::TextEditor::outlineColourId, Pal::border());
            e->setColour(juce::TextEditor::focusedOutlineColourId, Pal::accent());
            e->setColour(juce::TextEditor::highlightColourId, Pal::accent().withAlpha(0.35f));
            e->setColour(juce::CaretComponent::caretColourId, Pal::accent());
            e->onReturnKey = [this] { press(defaultButton); };
            e->onEscapeKey = [this] { press(cancelButton); };
            addAndMakeVisible(*e);
            editors.push_back(std::move(e));
        }
        for (int i = 0; i < buttonNames.size(); ++i)
        {
            auto b = std::make_unique<juce::TextButton>(buttonNames[i]);
            b->onClick = [this, i] { press(i); };
            if (i == defaultButton)
                b->setColour(juce::TextButton::buttonColourId, Pal::accent().darker(0.55f));
            addAndMakeVisible(*b);
            buttons.push_back(std::move(b));
        }
    }

    void resized() override
    {
        const int panelW = std::min(500, getWidth() - 40);
        const int contentW = panelW - 40;

        juce::AttributedString text;
        text.append(message, mnArpee::fonts::text(16.0f, true), Pal::text());
        text.setWordWrap(juce::AttributedString::byWord);
        messageLayout.createLayout(text, (float) contentW);
        const int messageH = message.isEmpty() ? 0 : (int) std::ceil(messageLayout.getHeight());

        const int fieldsH = (int) editors.size() * (18 + 34 + 10);
        const int panelH = 20 + 28 + 10 + messageH + (messageH > 0 ? 14 : 0) + fieldsH + 6 + 36 + 20;
        panel = juce::Rectangle<int>(panelW, panelH).withCentre({ getWidth() / 2, getHeight() * 45 / 100 });

        auto r = panel.reduced(20, 20);
        titleArea = r.removeFromTop(28);
        r.removeFromTop(10);
        messageArea = r.removeFromTop(messageH);
        if (messageH > 0)
            r.removeFromTop(14);
        for (size_t i = 0; i < editors.size(); ++i)
        {
            labels[i]->setBounds(r.removeFromTop(18));
            editors[i]->setBounds(r.removeFromTop(34));
            r.removeFromTop(10);
        }
        r.removeFromTop(6);
        auto row = r.removeFromTop(36);
        for (int i = (int) buttons.size() - 1; i >= 0; --i)
        {
            const int w = std::max(92, juce::GlyphArrangement::getStringWidthInt(mnArpee::fonts::text(17.0f, true),
                                                                                  buttons[(size_t) i]->getButtonText()) + 40);
            buttons[(size_t) i]->setBounds(row.removeFromRight(w));
            row.removeFromRight(10);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(getLocalBounds());
        g.setColour(Pal::card());
        g.fillRoundedRectangle(panel.toFloat(), 8.0f);
        g.setColour(Pal::border());
        g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 8.0f, 1.0f);
        g.setColour(Pal::accent());
        g.setFont(mnArpee::fonts::text(21.0f, true));
        g.drawText(title, titleArea, juce::Justification::centredLeft);
        if (! message.isEmpty())
            messageLayout.draw(g, messageArea.toFloat());
    }

    void mouseDown(const juce::MouseEvent&) override {} // modal: the editor behind is not reachable

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
            press(cancelButton);
        else if (key == juce::KeyPress::returnKey)
            press(defaultButton);
        return true;
    }

    void visibilityChanged() override
    {
        if (! isVisible())
            return;
        if (! editors.empty())
        {
            editors.front()->grabKeyboardFocus();
            editors.front()->selectAll();
        }
        else
        {
            grabKeyboardFocus();
        }
    }

private:
    void press(int button)
    {
        if (pressed)
            return;
        pressed = true;
        juce::StringArray values;
        for (const auto& e : editors)
            values.add(e->getText());
        finish(button, values);
    }

    juce::String title, message;
    int defaultButton, cancelButton;
    std::function<void(int, juce::StringArray)> finish;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::TextEditor>> editors;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    juce::TextLayout messageLayout;
    juce::Rectangle<int> panel, titleArea, messageArea;
    bool pressed = false;
};
} // namespace

void MnArpeeAudioEditor::closeDialog()
{
    if (dialog == nullptr)
        return;
    // It may be closing from inside its own button: take it off the screen now, delete it later.
    dialog->setVisible(false);
    juce::Component::SafePointer<juce::Component> doomed(dialog.release());
    juce::MessageManager::callAsync([doomed] { delete doomed.getComponent(); });
}

void MnArpeeAudioEditor::showDialog(const juce::String& title, const juce::String& message,
                                    std::vector<std::pair<juce::String, juce::String>> fields, juce::StringArray buttons,
                                    int defaultButton, int cancelButton,
                                    std::function<void(int, juce::StringArray)> done)
{
    closeDialog();
    closePresetBrowser();
    auto content = std::make_unique<DialogOverlay>(
        title, message, fields, buttons, defaultButton, cancelButton,
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), done = std::move(done)](int button, juce::StringArray values)
        {
            if (safe == nullptr)
                return;
            safe->closeDialog();
            if (done)
                done(button, values);
        });
    content->setBounds(getLocalBounds());
    addAndMakeVisible(*content);
    content->toFront(true);
    dialog = std::move(content);
}

void MnArpeeAudioEditor::showMessage(const juce::String& title, const juce::String& message)
{
    showDialog(title, message, {}, juce::StringArray { "OK" }, 0, 0, nullptr);
}

void MnArpeeAudioEditor::confirmLeavePreset(std::function<void()> proceed)
{
    const auto active = proc.getActivePreset();
    if (! active.dirty)
    {
        proceed();
        return;
    }
    showDialog("Unsaved changes", "Save the changes to \"" + active.name + "\" first?", {},
               juce::StringArray { "Save", "Don't save", "Cancel" }, 0, 2,
               [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), proceed = std::move(proceed)](int button, juce::StringArray)
               {
                   if (safe == nullptr || button == 2)
                       return; // Cancel
                   if (button == 1)
                   {
                       proceed(); // Don't save
                       return;
                   }
                   const auto a = safe->proc.getActivePreset();
                   if (a.factory || a.file == juce::File())
                       safe->saveActiveAs(proceed);
                   else if (safe->proc.saveActivePreset())
                       proceed();
               });
}

void MnArpeeAudioEditor::promptName(const juce::String& title, const juce::String& message,
                                    const juce::String& initial, std::function<void(juce::String)> done)
{
    showDialog(title, message, { { {}, initial } }, juce::StringArray { "OK", "Cancel" }, 0, 1,
               [done = std::move(done)](int button, juce::StringArray values)
               {
                   if (button == 0)
                       done(values[0].trim());
               });
}

void MnArpeeAudioEditor::promptSave(const juce::String& initialName, const juce::String& initialStyles,
                                    std::function<void(juce::String, juce::String)> done)
{
    showDialog("Save preset", "", { { "Name", initialName }, { "Styles (comma separated, e.g. House, Trance)", initialStyles } },
               juce::StringArray { "Save", "Cancel" }, 0, 1,
               [done = std::move(done)](int button, juce::StringArray values)
               {
                   if (button == 0)
                       done(values[0].trim(), values[1].trim());
               });
}

void MnArpeeAudioEditor::saveActiveAs(std::function<void()> then)
{
    const auto active = proc.getActivePreset();
    promptSave(proc.freePresetName(active.name), active.tags,
               [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), then = std::move(then)](juce::String name,
                                                                                                       juce::String styles)
               {
                   if (safe == nullptr)
                       return;
                   safe->proc.setActivePresetStyles(styles);
                   if (name.isEmpty() || ! safe->proc.saveActivePresetAs(name))
                   {
                       safe->showMessage("Cannot save", name.isEmpty() ? juce::String("The name is empty.")
                                                                       : "A preset named \"" + name + "\" already exists.");
                       return;
                   }
                   if (then)
                       then();
               });
}

void MnArpeeAudioEditor::renameDialog(int libraryIndex)
{
    const auto list = proc.getPresetList();
    if (libraryIndex < 0 || libraryIndex >= (int) list.size() || list[(size_t) libraryIndex].factory)
        return;
    promptName("Rename preset", "New name:", list[(size_t) libraryIndex].name,
               [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), libraryIndex](juce::String name)
               {
                   if (safe != nullptr && ! safe->proc.renamePreset(libraryIndex, name))
                       safe->showMessage("Cannot rename", "The name is empty or already used by another preset.");
               });
}

void MnArpeeAudioEditor::deleteDialog(int libraryIndex)
{
    const auto list = proc.getPresetList();
    if (libraryIndex < 0 || libraryIndex >= (int) list.size() || list[(size_t) libraryIndex].factory)
        return;
    showDialog("Delete preset", "Delete \"" + list[(size_t) libraryIndex].name + "\"? The file is moved to the recycle bin.", {},
               juce::StringArray { "Delete", "Cancel" }, 1, 1,
               [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), libraryIndex](int button, juce::StringArray)
               {
                   if (safe != nullptr && button == 0)
                       safe->proc.deletePreset(libraryIndex);
               });
}

void MnArpeeAudioEditor::newClicked()
{
    confirmLeavePreset([safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)]
                       {
                           if (safe != nullptr)
                               safe->proc.newPreset();
                       });
}

void MnArpeeAudioEditor::saveClicked()
{
    const auto active = proc.getActivePreset();
    if (! active.dirty)
        return;
    if (active.factory || active.file == juce::File())
        saveActiveAs(nullptr);
    else
        proc.saveActivePreset();
}

void MnArpeeAudioEditor::showPresetMenu()
{
    const auto active = proc.getActivePreset();
    const bool saved = ! active.factory && active.file != juce::File();
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lnf);
    menu.addItem(1, "Save as...");
    menu.addItem(2, "Revert", active.dirty && active.file != juce::File());
    menu.addItem(3, "Rename...", saved);
    menu.addItem(8, "Style...");
    menu.addSeparator();
    menu.addItem(4, "Import...");
    menu.addItem(5, "Export...");
    menu.addItem(6, "Show in folder");
    menu.addSeparator();
    {
        juce::PopupMenu zoom;
        for (const int percent : { 80, 90, 100, 110, 125, 150, 175, 200 })
            zoom.addItem(200 + percent, juce::String(percent) + "%", true, std::abs(proc.getZoom() * 100.0f - (float) percent) < 1.0f);
        menu.addSubMenu("Zoom", zoom);
    }
    menu.addSeparator();
    menu.addItem(7, "Delete", saved);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton).withParentComponent(this),
                       [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;
                           auto& p = safe->proc;
                           if (result >= 200)
                           {
                               p.setZoom((float) (result - 200) / 100.0f);
                               safe->setScaleFactor(p.getZoom());
                               return;
                           }
                           switch (result)
                           {
                               case 1: safe->saveActiveAs(nullptr); break;
                               case 2: p.revertActivePreset(); break;
                               case 3: safe->renameDialog(p.getActivePresetIndex()); break;
                               case 4: safe->importPresets(); break;
                               case 5: safe->exportPreset(); break;
                               case 6:
                               {
                                   const auto file = p.getActivePreset().file;
                                   if (file.existsAsFile())
                                       file.revealToUser();
                                   else
                                   {
                                       p.getUserPresetFolder().createDirectory();
                                       p.getUserPresetFolder().revealToUser();
                                   }
                                   break;
                               }
                               case 7: safe->deleteDialog(p.getActivePresetIndex()); break;
                               case 8:
                                   safe->promptName("Style", "Musical styles, comma separated (e.g. House, Trance):",
                                                    p.getActivePreset().tags,
                                                    [safe](juce::String styles)
                                                    {
                                                        if (safe != nullptr)
                                                            safe->proc.setActivePresetStyles(styles);
                                                    });
                                   break;
                               default: break;
                           }
                       });
}

void MnArpeeAudioEditor::importPresets()
{
    confirmLeavePreset(
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)]
        {
            if (safe == nullptr)
                return;
            safe->fileChooser = std::make_unique<juce::FileChooser>(
                "Import presets", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
            safe->fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                           [safe](const juce::FileChooser& chooser)
                                           {
                                               const auto file = chooser.getResult();
                                               if (safe == nullptr || file == juce::File())
                                                   return;
                                               if (safe->proc.importPresets(file) == 0)
                                                   safe->showMessage("Import", "No presets were found in that file.");
                                           });
        });
}

void MnArpeeAudioEditor::exportPreset()
{
    const auto active = proc.getActivePreset();
    fileChooser = std::make_unique<juce::FileChooser>(
        "Export preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(juce::File::createLegalFileName(active.name) + ".xml"),
        "*.xml");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
                             [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](const juce::FileChooser& chooser)
                             {
                                 const auto file = chooser.getResult();
                                 if (safe != nullptr && file != juce::File())
                                     safe->proc.exportActivePreset(file.withFileExtension("xml"));
                             });
}

void MnArpeeAudioEditor::refreshPresetUi()
{
    const auto a = proc.getActivePreset();
    if (shownPresetValid && a.name == shownPreset.name && a.dirty == shownPreset.dirty && a.file == shownPreset.file
        && a.factory == shownPreset.factory)
        return;
    shownPreset = a;
    shownPresetValid = true;
    presetButton.setButtonText(a.name);
    saveButton.setEnabled(a.dirty);
    repaint(presetButton.getBounds().expanded(4));
}

void MnArpeeAudioEditor::paintOverChildren(juce::Graphics& g)
{
    // Unsaved changes: a dot at the left of the preset name.
    if (shownPresetValid && shownPreset.dirty)
    {
        const auto b = presetButton.getBounds();
        g.setColour(Pal::accent());
        g.fillEllipse((float) b.getX() + 10.0f, (float) b.getCentreY() - 4.0f, 8.0f, 8.0f);
    }
}

void MnArpeeAudioEditor::paint(juce::Graphics& g)
{
    g.fillAll(Pal::bg());
    g.setColour(Pal::border());
    g.drawRect(getLocalBounds(), 1);

    // Cards behind the controls.
    g.setColour(Pal::card());
    g.fillRoundedRectangle(paramsCard.toFloat(), 8.0f);
    g.setColour(Pal::border());
    g.drawRoundedRectangle(paramsCard.toFloat(), 8.0f, 1.0f);

    g.setColour(Pal::card());
    g.fillRoundedRectangle(gridCard.toFloat(), 8.0f);
    g.setColour(Pal::border());
    g.drawRoundedRectangle(gridCard.toFloat(), 8.0f, 1.0f);

    // Title. The variable font's default instance plus synthetic bold is a bit light,
    // so the text is stroked twice with a small offset (as in mnKord).
    g.setColour(Pal::accent());
    g.setFont(mnArpee::fonts::title(28.0f));
    for (const float dx : { 0.0f, 0.6f })
        g.drawText("mnArpee", brandArea.toFloat().translated(dx, 0.0f), juce::Justification::centredLeft);

    // The version under the logo.
    g.setColour(Pal::dimText());
    g.setFont(mnArpee::fonts::text(13.0f, true));
    g.drawText("v" MNARPEE_VERSION_STRING, versionArea, juce::Justification::centredLeft);

    // Section labels.
    g.setFont(mnArpee::fonts::text(16.0f, true));
    g.setColour(Pal::dimText());
    g.drawText("PATTERN", gridCard.getX() + 14, gridCard.getY() + 7, 120, 16,
               juce::Justification::centredLeft);

    // The held notes, right of the "PATTERN" label. White: a key of the matrix. Dim: the same note
    // in another octave as a key, merged into it. Red: left out, more than five different notes.
    const auto font = mnArpee::fonts::text(17.0f, true);
    g.setFont(font);
    int x = statusArea.getX();
    auto draw = [&](const juce::String& text, juce::Colour colour)
    {
        const int w = juce::GlyphArrangement::getStringWidthInt(font, text);
        g.setColour(colour);
        g.drawText(text, x, statusArea.getY(), w + 2, statusArea.getHeight(), juce::Justification::centredLeft, false);
        x += w;
    };
    draw("keys  ", Pal::dimText());
    if (heldShown.empty())
        draw("-", Pal::dimText());
    int leftOut = 0;
    for (const int v : heldShown)
    {
        const auto use = (mnArpee::KeyUse) (v & 3);
        leftOut += use == mnArpee::KeyUse::LeftOut ? 1 : 0;
        draw(juce::MidiMessage::getMidiNoteName(v >> 2, true, true, 4) + " ",
             use == mnArpee::KeyUse::Used ? Pal::text()
             : use == mnArpee::KeyUse::Merged ? Pal::dimText().withAlpha(0.6f)
                                              : Pal::accent2());
    }
    if (leftOut > 0)
        draw(" +" + juce::String(leftOut) + " left out", Pal::accent2());

    // The chord the held notes form, at the right end of the strip.
    const auto chord = mnArpee::ChordInfo::fromCode(shownChord);
    if (chord.valid())
    {
        g.setColour(Pal::accent());
        g.setFont(mnArpee::fonts::text(20.0f, true));
        g.drawText(juce::String(mnArpee::chordName(chord)), statusArea, juce::Justification::centredRight, false);
    }
}

void MnArpeeAudioEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // Header: brand, the preset picker (arrows, name, New, Save, Menu), the held notes and the latch.
    auto header = area.removeFromTop(56);
    auto latchArea = header.removeFromRight(112);
    auto brand = header.removeFromLeft(170);
    brandArea = brand.removeFromTop(38);
    versionArea = brand.removeFromTop(16);
    latchButton.setBounds(latchArea.getX(), latchArea.getCentreY() - 12, 100, 24);

    // The preset picker and the undo / redo buttons form a group, centred in the header.
    auto group = header.withSizeKeepingCentre(26 + 4 + 190 + 4 + 26 + 8 + 52 + 6 + 52 + 6 + 62 + 14 + 30 + 6 + 30, 34);
    auto picker = group.removeFromLeft(26 + 4 + 190 + 4 + 26 + 8 + 52 + 6 + 52 + 6 + 62);
    group.removeFromLeft(14);
    undoButton.setBounds(group.removeFromLeft(30));
    group.removeFromLeft(6);
    redoButton.setBounds(group.removeFromLeft(30));
    prevPresetButton.setBounds(picker.removeFromLeft(26).reduced(6, 8));
    picker.removeFromLeft(4);
    presetButton.setBounds(picker.removeFromLeft(190));
    picker.removeFromLeft(4);
    nextPresetButton.setBounds(picker.removeFromLeft(26).reduced(6, 8));
    picker.removeFromLeft(8);
    newButton.setBounds(picker.removeFromLeft(52));
    picker.removeFromLeft(6);
    saveButton.setBounds(picker.removeFromLeft(52));
    picker.removeFromLeft(6);
    menuButton.setBounds(picker.removeFromLeft(62));

    // Values card: one row of "LABEL value" bars.
    paramsCard = area.removeFromTop(54);
    area.removeFromTop(10);

    // The rest is the matrix card.
    gridCard = area;
    statusArea = juce::Rectangle<int>(gridCard.getX() + 120, gridCard.getY() + 4, gridCard.getWidth() - 132, 22);

    auto inner = paramsCard.reduced(12, 11);
    const int gap = 10;
    const int barW = (inner.getWidth() - 4 * gap) / 5;
    for (auto* bar : { &rateSlider, &octaveSlider, &gateSlider, &swingSlider, &stepsSlider })
    {
        bar->setBounds(inner.removeFromLeft(barW));
        inner.removeFromLeft(gap);
    }

    if (presetBrowser != nullptr)
        presetBrowser->setBounds(getLocalBounds());
    if (dialog != nullptr)
        dialog->setBounds(getLocalBounds());
    if (rangePanel != nullptr)
        rangePanel->setBounds(getLocalBounds());

    // Grid inside the card (label strip on top).
    grid.setBounds(gridCard.getX() + 6, gridCard.getY() + 24,
                   gridCard.getWidth() - 12, gridCard.getHeight() - 30);
}

void MnArpeeAudioEditor::timerCallback()
{
    // Follow program switches, model changes (edits, state loads) and the step count.
    refreshPresetUi();
    undoButton.setEnabled(proc.canUndo());
    redoButton.setEnabled(proc.canRedo());
    const int prog = proc.getActiveSlot();
    int steps = shownProgram.numSteps;
    if (auto* sp = proc.getApvts().getRawParameterValue("steps"))
        steps = (int) std::lround(sp->load());
    if (prog != shownProgramIndex || proc.getModelVersion() != shownModelVersion)
        reloadProgram();
    else if (steps != shownProgram.numSteps)
    {
        shownProgram.numSteps = steps;
        grid.setProgram(shownProgram);
    }

    const auto& ui = proc.getUiStatus();
    grid.setPlayStep(ui.playingProgram.load(std::memory_order_relaxed) == shownProgramIndex
                         ? ui.playStep.load(std::memory_order_relaxed)
                         : -1);

    std::vector<int> held;
    const int n = ui.numInput.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        held.push_back(ui.input[(size_t) i].load(std::memory_order_relaxed));
    const int chordCode = ui.chord.load(std::memory_order_relaxed);
    if (held != heldShown || chordCode != shownChord)
    {
        heldShown = std::move(held);
        shownChord = chordCode;
        repaint(statusArea);
    }
}

void MnArpeeAudioEditor::reloadProgram()
{
    shownModelVersion = proc.getModelVersion();
    shownProgramIndex = proc.getActiveSlot();
    shownProgram = proc.getProgramCopy();
    grid.setProgram(shownProgram);
    grid.setEditStep(editStep);
}

void MnArpeeAudioEditor::commitStep(const mnArpee::Step& step)
{
    shownProgram.steps[(size_t) editStep] = step;
    proc.setStep(editStep, step);
    shownModelVersion = proc.getModelVersion(); // our own edit: no reload needed
    grid.setProgram(shownProgram);
}

void MnArpeeAudioEditor::applySteps(const mnArpee::Program& edited)
{
    shownProgram.steps = edited.steps;
    proc.setAllSteps(edited.steps);
    shownModelVersion = proc.getModelVersion(); // our own edit: no reload needed
    grid.setProgram(shownProgram);
}

namespace
{
// The pattern on the clipboard, if there is one (a copied pattern or a preset file's text).
bool patternOnClipboard(mnArpee::Preset& out)
{
    auto xml = juce::parseXML(juce::SystemClipboard::getTextFromClipboard());
    if (xml == nullptr)
        return false;
    const auto* element = xml->hasTagName("Preset") ? xml.get() : xml->getChildByName("Preset");
    return element != nullptr && mnArpee::presetFromXml(*element, out);
}
} // namespace

void MnArpeeAudioEditor::copyPattern()
{
    mnArpee::Preset copy;
    copy.name = "Pattern";
    copy.program = proc.getProgramCopy();
    juce::SystemClipboard::copyTextToClipboard(mnArpee::presetToXml(copy)->toString());
}

void MnArpeeAudioEditor::pastePattern()
{
    mnArpee::Preset clip;
    if (! patternOnClipboard(clip))
        return;
    auto edited = shownProgram;
    edited.steps = clip.program.steps;
    if (auto* steps = proc.getApvts().getParameter("steps"))
        steps->setValueNotifyingHost(steps->convertTo0to1((float) clip.program.numSteps));
    applySteps(edited);
}

void MnArpeeAudioEditor::showPatternMenu(int step, StepGrid::Lane lane, int row)
{
    using mnArpee::PatternLane;
    using mnArpee::StepType;

    // What "set all steps" sets: the clicked row of the matrix, or the value of the clicked box.
    const auto& st = shownProgram.steps[(size_t) std::clamp(step, 0, mnArpee::maxSteps - 1)];
    PatternLane patternLane = PatternLane::Key;
    int value = 0;
    juce::String valueText;
    const char* laneName = "keys";
    switch (lane)
    {
        case StepGrid::Lane::Key:
        {
            patternLane = PatternLane::Key;
            value = row >= 0 ? (row == StepGrid::randomRow ? -2 : StepGrid::keyForRow(row))
                             : (st.type == StepType::Random ? -2 : (int) st.keyIndex);
            valueText = value == -2 ? "Random" : value == mnArpee::rootKeyIndex ? "Root" : "Key " + juce::String(value + 1);
            laneName = "keys";
            break;
        }
        case StepGrid::Lane::Type:
        {
            patternLane = PatternLane::Type;
            const auto type = st.type;
            value = (int) type;
            valueText = type == StepType::Off ? "Off" : type == StepType::Rest ? "Rest" : type == StepType::Tie ? "Tie"
                      : type == StepType::Random ? "Random" : "Normal";
            laneName = "types";
            break;
        }
        case StepGrid::Lane::Octave:
            patternLane = PatternLane::Octave;
            value = (int) st.octave;
            valueText = value > 0 ? "+" + juce::String(value) : juce::String(value);
            laneName = "octaves";
            break;
        case StepGrid::Lane::Velocity:
            patternLane = PatternLane::Velocity;
            value = (int) st.velocity;
            valueText = juce::String(value);
            laneName = "velocities";
            break;
        case StepGrid::Lane::Ratchet:
            patternLane = PatternLane::Ratchet;
            value = (int) st.ratchet;
            valueText = juce::String(value);
            laneName = "ratchets";
            break;
        case StepGrid::Lane::Gate:
            patternLane = PatternLane::Gate;
            value = (int) st.gateScalePercent;
            valueText = juce::String(value) + "%";
            laneName = "gates";
            break;
    }

    mnArpee::Preset clip;
    const bool canPaste = patternOnClipboard(clip);
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lnf);
    menu.addItem(1, "Set all steps to " + valueText);
    menu.addItem(5, juce::String("Randomize the ") + laneName);
    {
        // Generate keys > mode > number of keys (ids 100 + mode * 10 + keys).
        static const char* modes[] = { "Up", "Down", "Up-Down", "Down-Up", "Converge", "Random" };
        juce::PopupMenu generate;
        for (int mode = 0; mode < 6; ++mode)
        {
            juce::PopupMenu keys;
            for (int n = 3; n <= mnArpee::maxInputKeys; ++n)
                keys.addItem(100 + mode * 10 + n, juce::String(n) + " keys");
            generate.addSubMenu(modes[mode], keys);
        }
        menu.addSubMenu("Generate keys", generate);
    }
    menu.addItem(9, "Random bass line (root only)");
    menu.addSeparator();
    menu.addItem(2, "Shift pattern left");
    menu.addItem(3, "Shift pattern right");
    menu.addItem(4, "Reverse pattern");
    menu.addItem(6, "Clear pattern");
    menu.addSeparator();
    menu.addItem(7, "Copy pattern");
    menu.addItem(8, "Paste pattern", canPaste);
    menu.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this),
                       [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), patternLane, value, clip](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;
                           auto edited = safe->shownProgram;
                           if (result >= 100)
                           {
                               std::mt19937 rng((unsigned) juce::Time::currentTimeMillis());
                               mnArpee::generateKeys(edited, (mnArpee::GeneratorMode) ((result - 100) / 10), (result - 100) % 10, rng);
                               safe->applySteps(edited);
                               return;
                           }
                           switch (result)
                           {
                               case 1: mnArpee::setLaneValue(edited, patternLane, value); break;
                               case 2: mnArpee::shiftPattern(edited, -1); break;
                               case 3: mnArpee::shiftPattern(edited, 1); break;
                               case 4: mnArpee::reversePattern(edited); break;
                               case 5:
                               {
                                   std::mt19937 rng((unsigned) juce::Time::currentTimeMillis());
                                   mnArpee::randomizeLane(edited, patternLane, rng);
                                   break;
                               }
                               case 6:
                               {
                                   const auto init = mnArpee::makeInitProgram();
                                   edited.steps = init.steps;
                                   break;
                               }
                               case 9:
                               {
                                   std::mt19937 rng((unsigned) juce::Time::currentTimeMillis());
                                   mnArpee::generateBassline(edited, rng);
                                   break;
                               }
                               case 7: safe->copyPattern(); return;
                               case 8: safe->pastePattern(); return;
                               default: return;
                           }
                           safe->applySteps(edited);
                       });
}

void MnArpeeAudioEditor::onLaneAdjusted(int step, StepGrid::Lane lane, int delta)
{
    using mnArpee::StepType;
    editStep = step;
    auto st = shownProgram.steps[(size_t) step];
    if (delta != 0)
    {
        switch (lane)
        {
            case StepGrid::Lane::Type:
            {
                // Ascending as in BlueARP's list.
                constexpr StepType order[] = { StepType::Off, StepType::Normal, StepType::Rest,
                                               StepType::Tie, StepType::Random };
                int i = 0;
                while (i < 4 && order[i] != st.type)
                    ++i;
                st.type = order[std::clamp(i + delta, 0, 4)];
                break;
            }
            case StepGrid::Lane::Key:
                st.keyIndex = (std::int8_t) std::clamp((int) st.keyIndex + delta, (int) mnArpee::rootKeyIndex, 4);
                break;
            case StepGrid::Lane::Octave:
                st.octave = (std::int8_t) std::clamp((int) st.octave + delta, -3, 3);
                break;
            case StepGrid::Lane::Velocity:
                st.velocity = (std::uint8_t) std::clamp((int) st.velocity + delta, 1, 127);
                break;
            case StepGrid::Lane::Ratchet:
                st.ratchet = (std::uint8_t) std::clamp((int) st.ratchet + delta, 1, 4);
                break;
            case StepGrid::Lane::Gate:
            {
                // One notch of the gate steps per unit.
                const int index = std::clamp(mnArpee::nearestGateStep(st.gateScalePercent) + delta, 0, mnArpee::numGateSteps - 1);
                st.gateScalePercent = (std::uint16_t) mnArpee::gateSteps[index];
                break;
            }
        }
        commitStep(st);
    }
    grid.setEditStep(step);
}

void MnArpeeAudioEditor::onGridClicked(int step, int row)
{
    using mnArpee::StepType;
    editStep = step;
    auto st = shownProgram.steps[(size_t) step];
    if (row != StepGrid::randomRow)
    {
        // Key and Root rows: toggle the step on that key (a Tie step stays a Tie).
        const int key = StepGrid::keyForRow(row);
        const bool pitched = st.type == StepType::Normal || st.type == StepType::Tie;
        if (pitched && st.keyIndex == key)
            st.type = StepType::Off;
        else if (st.type != StepType::Tie)
            st.type = StepType::Normal;
        st.keyIndex = (std::int8_t) key;
    }
    else
    {
        st.type = st.type == StepType::Random ? StepType::Off : StepType::Random;
    }
    commitStep(st);
    grid.setEditStep(step);
}
