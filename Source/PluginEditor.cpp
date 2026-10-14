#include "PluginEditor.h"
#include "PluginProcessor.h"

#include "Fonts.h"

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
    g.setFont(mnArpee::fonts::text(17.0f));
    g.drawText(btn.getButtonText(), r.reduced(6.0f, 0.0f), juce::Justification::centredLeft);
}

juce::Font MnArpeeLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return mnArpee::fonts::text(juce::jmin(17.0f, (float) box.getHeight() * 0.62f));
}

juce::Font MnArpeeLookAndFeel::getPopupMenuFont() { return mnArpee::fonts::text(17.0f); }

juce::Font MnArpeeLookAndFeel::getSliderPopupFont(juce::Slider&) { return mnArpee::fonts::text(16.0f); }

void MnArpeeLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    // Value bar: the value text is drawn over it by the slider's own label.
    const auto r = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(0.5f);
    g.setColour(Pal::cardHi());
    g.fillRoundedRectangle(r, 5.0f);

    // Fills from the left, or from zero for a value that goes both ways.
    float from = r.getX();
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        from = r.getX() + (float) slider.valueToProportionOfLength(0.0) * r.getWidth();
    const float to = juce::jlimit(r.getX(), r.getRight(), sliderPos);
    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(r.toNearestInt());
        g.setColour(Pal::accent().withAlpha(0.42f));
        g.fillRect(juce::Rectangle<float>(juce::jmin(from, to), r.getY(), std::abs(to - from), r.getHeight()));
    }
    g.setColour(Pal::border());
    g.drawRoundedRectangle(r, 5.0f, 1.0f);
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
MnArpeeAudioEditor::MnArpeeAudioEditor(MnArpeeAudioProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&lnf);
    setSize(900, 560);
    setResizable(true, true);
    setResizeLimits(760, 480, 1600, 1000);

    addAndMakeVisible(grid);
    grid.onStepClicked([this](int step, int row) { onGridClicked(step, row); });
    grid.onLaneAdjusted([this](int step, StepGrid::Lane lane, int delta) { onLaneAdjusted(step, lane, delta); });

    prevProgramButton.onClick = [this] { stepProgram(-1); };
    nextProgramButton.onClick = [this] { stepProgram(1); };
    prevProgramButton.setTooltip("Previous program");
    nextProgramButton.setTooltip("Next program");
    programButton.onClick = [this] { showProgramMenu(); };
    programButton.setTooltip("Choose the program");
    for (auto* b : { static_cast<juce::Component*>(&prevProgramButton), static_cast<juce::Component*>(&programButton),
                     static_cast<juce::Component*>(&nextProgramButton) })
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
                  4.0);
    setupValueBar(octaveSlider, [signedValue](double v) { return "OCT " + signedValue(v); }, 0.0);
    setupValueBar(gateSlider, [](double v) { return "GATE " + juce::String((int) std::lround(v)) + "%"; }, 80.0);
    setupValueBar(swingSlider, [signedValue](double v) { return "SWING " + signedValue(v) + "%"; }, 0.0);
    setupValueBar(stepsSlider, [](double v) { return "STEPS " + juce::String((int) std::lround(v)); }, 16.0);

    addAndMakeVisible(latchButton);
    latchButton.setButtonText("Latch");
    latchAtt = std::make_unique<ButtonAttachment>(apvts, "latch", latchButton);

    statusText = "keys  -";
    reloadProgram();
    startTimerHz(30);
}

MnArpeeAudioEditor::~MnArpeeAudioEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MnArpeeAudioEditor::setupValueBar(juce::Slider& s, std::function<juce::String(double)> text,
                                       double defaultValue)
{
    s.setSliderStyle(juce::Slider::LinearBar);
    s.setTextBoxStyle(juce::Slider::TextBoxLeft, true, 80, 20);
    s.setTextBoxIsEditable(false);
    s.setDoubleClickReturnValue(true, defaultValue);
    s.textFromValueFunction = std::move(text);
    s.updateText();
    addAndMakeVisible(s);
}

void MnArpeeAudioEditor::setProgramIndex(int index)
{
    if (auto* p = proc.getApvts().getParameter("program"))
    {
        const int i = juce::jlimit(0, MnArpeeAudioProcessor::numUserPrograms - 1, index);
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float) (i + 1)));
        p->endChangeGesture();
    }
}

void MnArpeeAudioEditor::stepProgram(int delta)
{
    const int n = MnArpeeAudioProcessor::numUserPrograms;
    setProgramIndex((selectedProgram() + delta + n) % n);
}

void MnArpeeAudioEditor::showProgramMenu()
{
    juce::PopupMenu menu;
    const int current = selectedProgram();
    for (int i = 0; i < MnArpeeAudioProcessor::numUserPrograms; ++i)
        menu.addItem(i + 1, juce::String(i + 1) + "  " + proc.getProgramName(i), true, i == current);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&programButton).withMinimumWidth(programButton.getWidth()),
                       [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](int result)
                       {
                           if (safe != nullptr && result > 0)
                               safe->setProgramIndex(result - 1);
                       });
}

int MnArpeeAudioEditor::selectedProgram() const
{
    if (auto* p = proc.getApvts().getRawParameterValue("program"))
        return juce::jlimit(0, MnArpeeAudioProcessor::numUserPrograms - 1, (int) std::lround(p->load()) - 1);
    return 0;
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

    // Section labels.
    g.setFont(mnArpee::fonts::text(16.0f, true));
    g.setColour(Pal::dimText());
    g.drawText("PATTERN", gridCard.getX() + 14, gridCard.getY() + 7, 120, 16,
               juce::Justification::centredLeft);

    // Info line in the header (between brand and latch): held notes.
    g.setFont(mnArpee::fonts::text(17.0f));
    g.setColour(Pal::text());
    g.drawText(statusText, statusArea, juce::Justification::centredLeft);
}

void MnArpeeAudioEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // Header: brand, the program picker, the held notes, and the latch on the right.
    auto header = area.removeFromTop(56);
    auto latchArea = header.removeFromRight(150);
    brandArea = header.removeFromLeft(170);
    latchButton.setBounds(latchArea.getX(), latchArea.getCentreY() - 12, 100, 24);

    header.removeFromLeft(14);
    auto picker = header.removeFromLeft(26 + 4 + 230 + 4 + 26).withSizeKeepingCentre(26 + 4 + 230 + 4 + 26, 34);
    prevProgramButton.setBounds(picker.removeFromLeft(26).reduced(6, 8));
    picker.removeFromLeft(4);
    programButton.setBounds(picker.removeFromLeft(230));
    picker.removeFromLeft(4);
    nextProgramButton.setBounds(picker.removeFromLeft(26).reduced(6, 8));
    header.removeFromLeft(18);
    statusArea = header.withSizeKeepingCentre(header.getWidth(), 24);

    // Values card: one row of "LABEL value" bars.
    paramsCard = area.removeFromTop(54);
    area.removeFromTop(10);

    // The rest is the matrix card.
    gridCard = area;

    auto inner = paramsCard.reduced(12, 11);
    const int gap = 10;
    const int barW = (inner.getWidth() - 4 * gap) / 5;
    for (auto* bar : { &rateSlider, &octaveSlider, &gateSlider, &swingSlider, &stepsSlider })
    {
        bar->setBounds(inner.removeFromLeft(barW));
        inner.removeFromLeft(gap);
    }

    // Grid inside the card (label strip on top).
    grid.setBounds(gridCard.getX() + 6, gridCard.getY() + 24,
                   gridCard.getWidth() - 12, gridCard.getHeight() - 30);
}

void MnArpeeAudioEditor::timerCallback()
{
    // Follow program switches, model changes (edits, state loads) and the step count.
    const int prog = selectedProgram();
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

    juce::String txt;
    const int n = ui.numInput.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        txt << (i > 0 ? " " : "") << juce::MidiMessage::getMidiNoteName(
                   ui.input[(size_t) i].load(std::memory_order_relaxed), true, true, 4);
    txt = "keys  " + (txt.isNotEmpty() ? txt : juce::String("-"));
    if (txt != statusText)
    {
        statusText = txt;
        repaint(statusArea);
    }
}

void MnArpeeAudioEditor::reloadProgram()
{
    shownModelVersion = proc.getModelVersion();
    shownProgramIndex = selectedProgram();
    shownProgram = proc.getProgramCopy(shownProgramIndex);
    programButton.setButtonText(juce::String(shownProgramIndex + 1) + "  " + proc.getProgramName(shownProgramIndex));
    grid.setProgram(shownProgram);
    grid.setEditStep(editStep);
}

void MnArpeeAudioEditor::commitStep(const mnArpee::Step& step)
{
    shownProgram.steps[(size_t) editStep] = step;
    proc.setStep(shownProgramIndex, editStep, step);
    shownModelVersion = proc.getModelVersion(); // our own edit: no reload needed
    grid.setProgram(shownProgram);
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
    if (grid.getLane() == StepGrid::Lane::Type)
        st.type = StepGrid::typeForRow(row);
    else if (grid.getLane() == StepGrid::Lane::Octave)
        st.octave = (std::int8_t) StepGrid::octaveForRow(row);
    else if (grid.getLane() == StepGrid::Lane::Velocity)
        st.velocity = (std::uint8_t) StepGrid::velocityForRow(row);
    else if (row != StepGrid::randomRow)
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
        st.type = st.type == StepType::Random ? StepType::Off : StepType::Random;
    commitStep(st);
    grid.setEditStep(step);
}
