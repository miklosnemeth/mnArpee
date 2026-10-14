#include "PluginEditor.h"
#include "PluginProcessor.h"

#include "Fonts.h"

using Pal = StepGrid::Palette;

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
    g.setFont(mnArpee::fonts::text(13.0f));
    g.drawText(btn.getButtonText(), r.reduced(6.0f, 0.0f), juce::Justification::centredLeft);
}

juce::Font MnArpeeLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return mnArpee::fonts::text(juce::jmin(15.0f, (float) box.getHeight() * 0.6f));
}

juce::Font MnArpeeLookAndFeel::getPopupMenuFont() { return mnArpee::fonts::text(15.0f); }

juce::Font MnArpeeLookAndFeel::getSliderPopupFont(juce::Slider&) { return mnArpee::fonts::text(14.0f); }

juce::Label* MnArpeeLookAndFeel::createSliderTextBox(juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(s);
    l->setFont(mnArpee::fonts::text(12.0f, true));
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

    juce::StringArray progNames;
    for (int i = 0; i < MnArpeeAudioProcessor::numUserPrograms; ++i)
        progNames.add("P" + juce::String(i + 1));
    programBox.addItemList(progNames, 1);
    chainBox.addItemList({ "Off", "Chain 1", "Chain 2" }, 1);
    for (int i = 0; i < mnArpee::numSyncRates; ++i)
        rateBox.addItem(mnArpee::syncRateName((mnArpee::SyncRate) i), i + 1);
    // Item id = StepType value + 1 (the values have a gap where Chord used to be).
    stepTypeBox.addItem("Off", (int) mnArpee::StepType::Off + 1);
    stepTypeBox.addItem("Normal", (int) mnArpee::StepType::Normal + 1);
    stepTypeBox.addItem("Rest", (int) mnArpee::StepType::Rest + 1);
    stepTypeBox.addItem("Tie", (int) mnArpee::StepType::Tie + 1);
    stepTypeBox.addItem("Random", (int) mnArpee::StepType::Random + 1);
    // Item id = KEY SELECT value + 2 (Root = -1).
    stepKeyBox.addItem("Root", mnArpee::rootKeyIndex + 2);
    for (int k = 0; k < mnArpee::maxInputKeys; ++k)
        stepKeyBox.addItem("Key " + juce::String(k + 1), k + 2);
    velModeBox.addItem("Lane", 1);
    velModeBox.addItem("Input", 2);
    velModeBox.addItem("Lane x Input", 3);
    for (auto* cb : { &programBox, &chainBox, &rateBox, &velModeBox, &stepTypeBox, &stepKeyBox })
        addAndMakeVisible(*cb);

    for (auto* s : { &octaveSlider, &gateSlider, &swingSlider, &stepsSlider, &stepOctSlider,
                     &stepVelSlider })
        setupSlider(*s);
    stepOctSlider.setRange(-3, 3, 1);
    stepVelSlider.setRange(1, 127, 1);

    addAndMakeVisible(latchButton);
    latchButton.setButtonText("Latch");

    auto& apvts = proc.getApvts();
    programAtt = std::make_unique<ComboAttachment>(apvts, "program", programBox);
    chainAtt = std::make_unique<ComboAttachment>(apvts, "chain", chainBox);
    rateAtt = std::make_unique<ComboAttachment>(apvts, "rate", rateBox);
    velModeAtt = std::make_unique<ComboAttachment>(apvts, "velmode", velModeBox);
    octaveAtt = std::make_unique<SliderAttachment>(apvts, "octave", octaveSlider);
    gateAtt = std::make_unique<SliderAttachment>(apvts, "gate", gateSlider);
    swingAtt = std::make_unique<SliderAttachment>(apvts, "swing", swingSlider);
    stepsAtt = std::make_unique<SliderAttachment>(apvts, "steps", stepsSlider);
    latchAtt = std::make_unique<ButtonAttachment>(apvts, "latch", latchButton);

    stepTypeBox.onChange = [this] { onStepEditorChanged(); };
    stepKeyBox.onChange = [this] { onStepEditorChanged(); };
    stepOctSlider.onValueChange = [this] { onStepEditorChanged(); };
    stepVelSlider.onValueChange = [this] { onStepEditorChanged(); };

    auto setupLabel = [this](juce::Label& l, const char* text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setFont(mnArpee::fonts::text(12.0f, true));
        l.setColour(juce::Label::textColourId, Pal::dimText());
        l.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(l);
    };
    setupLabel(programLabel, "PROGRAM");
    setupLabel(chainLabel, "CHAIN");
    setupLabel(rateLabel, "RATE");
    setupLabel(velModeLabel, "VEL SOURCE");
    setupLabel(octaveLabel, "OCT");
    setupLabel(gateLabel, "GATE %");
    setupLabel(swingLabel, "SWING %");
    setupLabel(stepsLabel, "STEPS");
    setupLabel(stepTypeLabel, "TYPE");
    setupLabel(stepKeyLabel, "KEY");
    setupLabel(stepOctLabel, "OCT");
    setupLabel(stepVelLabel, "VEL");

    statusText = "keys  -";
    reloadProgram();
    startTimerHz(30);
}

MnArpeeAudioEditor::~MnArpeeAudioEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MnArpeeAudioEditor::setupSlider(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 18);
    s.setPopupDisplayEnabled(true, true, this);
    addAndMakeVisible(s);
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

    g.setColour(Pal::card());
    g.fillRoundedRectangle(editCard.toFloat(), 8.0f);
    g.setColour(Pal::border());
    g.drawRoundedRectangle(editCard.toFloat(), 8.0f, 1.0f);

    // Title. The variable font's default instance plus synthetic bold is a bit light,
    // so the text is stroked twice with a small offset (as in mnKord).
    g.setColour(Pal::accent());
    g.setFont(mnArpee::fonts::title(28.0f));
    for (const float dx : { 0.0f, 0.6f })
        g.drawText("mnArpee", brandArea.toFloat().translated(dx, 0.0f), juce::Justification::centredLeft);

    // Section labels.
    g.setFont(mnArpee::fonts::text(12.0f, true));
    g.setColour(Pal::dimText());
    g.drawText("PATTERN", gridCard.getX() + 14, gridCard.getY() + 7, 120, 16,
               juce::Justification::centredLeft);
    g.drawText("STEP " + juce::String(editStep + 1),
               editCard.getX() + 14, editCard.getY() + 7, 200, 16,
               juce::Justification::centredLeft);

    // Info line in the header (between brand and latch): held notes.
    g.setFont(mnArpee::fonts::text(13.0f));
    g.setColour(Pal::text());
    const int sx = 12 + 180;
    const int sw = getWidth() - 12 - 150 - sx - 10;
    g.drawText(statusText, sx, 26, sw, 24, juce::Justification::centredLeft);
}

void MnArpeeAudioEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // Header: brand on the left, latch + status on the right.
    auto header = area.removeFromTop(56);
    auto latchArea = header.removeFromRight(150);
    brandArea = header.removeFromLeft(170);
    latchButton.setBounds(latchArea.getX(), latchArea.getCentreY() - 12, 100, 24);

    // Parameter card.
    paramsCard = area.removeFromTop(74);
    area.removeFromTop(10);

    // Bottom: step editor card, with the matrix above it.
    editCard = area.removeFromBottom(76);
    area.removeFromBottom(10);
    gridCard = area;

    auto inner = paramsCard.reduced(12);
    inner.removeFromTop(4);
    auto place = [](juce::Rectangle<int>& r, juce::Label& lab, juce::Component& comp, int w)
    {
        auto col = r.removeFromLeft(w);
        lab.setBounds(col.getX(), col.getY(), col.getWidth(), 14);
        comp.setBounds(col.getX(), col.getY() + 18, col.getWidth() - 8, 26);
        r.removeFromLeft(8);
    };
    place(inner, programLabel, programBox, 80);
    place(inner, chainLabel, chainBox, 100);
    place(inner, rateLabel, rateBox, 86);
    place(inner, velModeLabel, velModeBox, 110);
    place(inner, octaveLabel, octaveSlider, 100);
    place(inner, gateLabel, gateSlider, 100);
    place(inner, swingLabel, swingSlider, 100);
    place(inner, stepsLabel, stepsSlider, 100);

    // Grid inside the card (label strip on top).
    grid.setBounds(gridCard.getX() + 6, gridCard.getY() + 24,
                   gridCard.getWidth() - 12, gridCard.getHeight() - 30);

    // Step editor strip.
    auto edit = editCard.reduced(12);
    edit.removeFromTop(18);
    auto place2 = [](juce::Rectangle<int>& r, juce::Label& lab, juce::Component& comp, int w)
    {
        auto col = r.removeFromLeft(w);
        lab.setBounds(col.getX(), col.getY(), col.getWidth(), 14);
        comp.setBounds(col.getX(), col.getY() + 18, col.getWidth() - 8, 26);
        r.removeFromLeft(10);
    };
    place2(edit, stepTypeLabel, stepTypeBox, 140);
    place2(edit, stepKeyLabel, stepKeyBox, 100);
    place2(edit, stepOctLabel, stepOctSlider, 150);
    place2(edit, stepVelLabel, stepVelSlider, 170);
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
        repaint(180, 12, getWidth() - 342, 56);
    }
}

void MnArpeeAudioEditor::reloadProgram()
{
    shownModelVersion = proc.getModelVersion();
    shownProgramIndex = selectedProgram();
    shownProgram = proc.getProgramCopy(shownProgramIndex);
    grid.setProgram(shownProgram);
    grid.setEditStep(editStep);
    refreshStepEditor();
}

void MnArpeeAudioEditor::refreshStepEditor()
{
    const auto& st = shownProgram.steps[(size_t) std::clamp(editStep, 0, mnArpee::maxSteps - 1)];
    stepTypeBox.setSelectedId((int) st.type + 1, juce::dontSendNotification);
    stepKeyBox.setSelectedId(std::clamp((int) st.keyIndex, (int) mnArpee::rootKeyIndex, 4) + 2,
                           juce::dontSendNotification);
    stepOctSlider.setValue(st.octave, juce::dontSendNotification);
    stepVelSlider.setValue(std::max(1, (int) st.velocity), juce::dontSendNotification);
    repaint(editCard);
}

void MnArpeeAudioEditor::commitStep(const mnArpee::Step& step)
{
    shownProgram.steps[(size_t) editStep] = step;
    proc.setStep(shownProgramIndex, editStep, step);
    shownModelVersion = proc.getModelVersion(); // our own edit: no reload needed
    grid.setProgram(shownProgram);
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
        st.type = st.type == StepType::Random ? StepType::Off : StepType::Random;
    commitStep(st);
    grid.setEditStep(step);
    refreshStepEditor();
}

void MnArpeeAudioEditor::onStepEditorChanged()
{
    auto st = shownProgram.steps[(size_t) editStep];
    st.type = mnArpee::stepTypeFromInt(stepTypeBox.getSelectedId() - 1);
    st.keyIndex = (std::int8_t) juce::jlimit((int) mnArpee::rootKeyIndex, 4, stepKeyBox.getSelectedId() - 2);
    st.octave = (std::int8_t) stepOctSlider.getValue();
    st.velocity = (std::uint8_t) stepVelSlider.getValue();
    commitStep(st);
}
