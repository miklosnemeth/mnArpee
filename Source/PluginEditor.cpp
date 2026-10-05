#include "PluginEditor.h"
#include "PluginProcessor.h"

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

    // Le-fel nyilak (kis chevron).
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
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(btn.getButtonText(), r.reduced(6.0f, 0.0f), juce::Justification::centredLeft);
}

juce::Label* MnArpeeLookAndFeel::createSliderTextBox(juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(s);
    l->setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
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

    brandLabel.setText("mnArpee", juce::dontSendNotification);
    brandLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    brandLabel.setColour(juce::Label::textColourId, Pal::accent());
    addAndMakeVisible(brandLabel);

    addAndMakeVisible(grid);
    grid.onStepClicked([this](int step, int row)
    {
        editStep = step;
        auto& apvts = proc.getApvts();
        juce::ScopedLock lock(proc.getEngineLock());
        auto& bank = proc.getEngineBankForEdit();
        int progIdx = 0;
        if (auto* pp = apvts.getRawParameterValue("program"))
            progIdx = juce::jlimit(0, 7, (int) pp->load() - 1);
        auto& prog = bank.programs[(size_t) progIdx];
        auto& st = prog.steps[(size_t) step];
        if (row <= 4)
        {
            st.type = (st.type == mnArpee::StepType::Normal
                       && st.keyIndex == row) ? mnArpee::StepType::Off
                                              : mnArpee::StepType::Normal;
            st.keyIndex = (std::int8_t) row;
        }
        else if (row == 5)
            st.type = (st.type == mnArpee::StepType::Chord) ? mnArpee::StepType::Off
                                                            : mnArpee::StepType::Chord;
        else
            st.type = (st.type == mnArpee::StepType::Random) ? mnArpee::StepType::Off
                                                             : mnArpee::StepType::Random;
        proc.requestBankRefresh();
        grid.setEditStep(step);
        refreshFromState();
    });

    juce::StringArray progNames;
    for (int i = 0; i < 8; ++i)
        progNames.add("P" + juce::String(i + 1));
    programBox.addItemList(progNames, 1);
    chainBox.addItemList({ "Off", "Chain 1", "Chain 2" }, 1);
    modeBox.addItemList({ "Up", "Down", "UpDown", "DownUp", "Random", "As Played" }, 1);
    rateBox.addItemList({ "1/64", "1/48", "1/32", "1/24", "1/16", "1/12",
                          "1/8", "1/6", "1/4", "3/64", "3/32", "3/16", "3/8" }, 1);
    stepTypeBox.addItemList({ "Off", "Normal", "Rest", "Tie", "Chord", "Random" }, 1);
    stepKeyBox.addItemList({ "k1", "k2", "k3", "k4", "k5" }, 1);

    for (auto* cb : { &programBox, &chainBox, &modeBox, &rateBox, &stepTypeBox, &stepKeyBox })
    {
        addAndMakeVisible(*cb);
        cb->addListener(this);
    }

    setupKnob(octaveSlider, -2, 2, 1);
    setupKnob(gateSlider, 1, 125, 1);
    setupKnob(swingSlider, -50, 50, 1);
    setupKnob(stepsSlider, 1, 64, 1);
    setupKnob(stepOctSlider, -3, 3, 1);
    setupKnob(stepVelSlider, 0, 127, 1);

    addAndMakeVisible(latchButton);
    latchButton.setButtonText("Latch");
    latchButton.onClick = [this]
    {
        proc.setLatchDirect(latchButton.getToggleState());
    };

    auto setupLabel = [this](juce::Label& l, const char* text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, Pal::dimText());
        l.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(l);
    };
    setupLabel(programLabel, "PROGRAM");
    setupLabel(chainLabel, "CHAIN");
    setupLabel(modeLabel, "MODE");
    setupLabel(rateLabel, "RATE");
    setupLabel(octaveLabel, "OCT");
    setupLabel(gateLabel, "GATE %");
    setupLabel(swingLabel, "SWING %");
    setupLabel(stepsLabel, "STEPS");
    setupLabel(stepTypeLabel, "TYPE");
    setupLabel(stepKeyLabel, "KEY");
    setupLabel(stepOctLabel, "OCT");
    setupLabel(stepVelLabel, "VEL");

    refreshFromState();
    lastStatusText = {};      // első timer-tick mindig frissít
    statusText = "keys  -";
    startTimerHz(15);
}

void MnArpeeAudioEditor::setupKnob(juce::Slider& s, double lo, double hi, double step)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 18);
    s.setRange(lo, hi, step);
    s.setPopupDisplayEnabled(true, true, this);
    addAndMakeVisible(s);
    s.addListener(this);
}

void MnArpeeAudioEditor::paint(juce::Graphics& g)
{
    g.fillAll(Pal::bg());
    g.setColour(Pal::border());
    g.drawRect(getLocalBounds(), 1);

    // Kártyák a vezérlők mögött.
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

    // Szekció-címkék.
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(Pal::dimText());
    g.drawText("PATTERN", gridCard.getX() + 14, gridCard.getY() + 7, 120, 16,
               juce::Justification::centredLeft);
    g.drawText("STEP " + juce::String(editStep + 1),
               editCard.getX() + 14, editCard.getY() + 7, 200, 16,
               juce::Justification::centredLeft);

    // Info sor a fejlécen (márka és latch között): lenyomott hangok.
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(Pal::text());
    const int sx = 12 + 180;
    const int sw = getWidth() - 12 - 150 - sx - 10;
    g.drawText(statusText, sx, 26, sw, 24, juce::Justification::centredLeft);
}

void MnArpeeAudioEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // Fejléc: márka balra, latch + statusz jobbra.
    auto header = area.removeFromTop(56);
    auto latchArea = header.removeFromRight(150);
    brandLabel.setBounds(header.removeFromLeft(170));
    latchButton.setBounds(latchArea.getX(), latchArea.getCentreY() - 12, 100, 24);

    // Paraméter-kártya.
    paramsCard = area.removeFromTop(74);
    area.removeFromTop(10);

    // Alsó: step-szerkesztő kártya, fölötte a matrix.
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
    place(inner, modeLabel, modeBox, 110);
    place(inner, rateLabel, rateBox, 86);
    place(inner, octaveLabel, octaveSlider, 112);
    place(inner, gateLabel, gateSlider, 112);
    place(inner, swingLabel, swingSlider, 112);
    place(inner, stepsLabel, stepsSlider, 112);

    // Grid a kártya belsejében (címke-sávval a tetején).
    grid.setBounds(gridCard.getX() + 6, gridCard.getY() + 24,
                   gridCard.getWidth() - 12, gridCard.getHeight() - 30);

    // Step-szerkesztő sáv.
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

MnArpeeAudioEditor::~MnArpeeAudioEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}
void MnArpeeAudioEditor::timerCallback()
{
    juce::String txt;
    {
        juce::ScopedLock lock(proc.getEngineLock());
        grid.setPlayStep(proc.getEngine().getCurrentStep());
        auto input = proc.getEngine().getActiveInput();
        for (size_t i = 0; i < input.size() && i < 5; ++i)
        {
            if (i > 0)
                txt += " ";
            txt += juce::String(input[i].note);
        }
    }
    if (txt != lastStatusText)
    {
        lastStatusText = txt;
        statusText = txt.isNotEmpty() ? "keys  " + txt : "keys  -";
        repaint(180, 12, getWidth() - 342, 56);
    }
}

void MnArpeeAudioEditor::comboBoxChanged(juce::ComboBox* cb)
{
    auto& apvts = proc.getApvts();
    auto setInt = [&](const char* id, int v)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(id)))
            p->setValueNotifyingHost(p->convertTo0to1((float) v));
    };
    auto setChoice = [&](const char* id, int v)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
            p->setValueNotifyingHost(p->convertTo0to1((float) v));
    };
    if (cb == &programBox)
        setInt("program", programBox.getSelectedItemIndex() + 1);
    else if (cb == &chainBox)
        setInt("chain", chainBox.getSelectedItemIndex());
    else if (cb == &modeBox)
        setChoice("mode", modeBox.getSelectedItemIndex());
    else if (cb == &rateBox)
        setChoice("rate", rateBox.getSelectedItemIndex());
    else if (cb == &stepTypeBox || cb == &stepKeyBox)
        pushStepEditToEngine();
}

void MnArpeeAudioEditor::sliderValueChanged(juce::Slider* s)
{
    auto& apvts = proc.getApvts();
    auto setInt = [&](const char* id, int v)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(id)))
            p->setValueNotifyingHost(p->convertTo0to1((float) v));
    };
    if (s == &octaveSlider)
        setInt("octave", (int) s->getValue());
    else if (s == &gateSlider)
        setInt("gate", (int) s->getValue());
    else if (s == &swingSlider)
        setInt("swing", (int) s->getValue());
    else if (s == &stepsSlider)
        setInt("steps", (int) s->getValue());
    else
        pushStepEditToEngine();
}

void MnArpeeAudioEditor::refreshFromState()
{
    auto& apvts = proc.getApvts();
    auto getInt = [&](const char* id) -> int
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return (int) p->load();
        return 0;
    };
    programBox.setSelectedItemIndex(juce::jlimit(0, 7, getInt("program") - 1),
                                    juce::dontSendNotification);
    chainBox.setSelectedItemIndex(juce::jlimit(0, 2, getInt("chain")),
                                  juce::dontSendNotification);
    modeBox.setSelectedItemIndex(juce::jlimit(0, 5, getInt("mode")),
                                 juce::dontSendNotification);
    rateBox.setSelectedItemIndex(juce::jlimit(0, 12, getInt("rate")),
                                 juce::dontSendNotification);
    octaveSlider.setValue(getInt("octave"), juce::dontSendNotification);
    gateSlider.setValue(getInt("gate"), juce::dontSendNotification);
    swingSlider.setValue(getInt("swing"), juce::dontSendNotification);
    stepsSlider.setValue(getInt("steps"), juce::dontSendNotification);
    latchButton.setToggleState(getInt("latch") != 0, juce::dontSendNotification);

    juce::ScopedLock lock(proc.getEngineLock());
    auto& bank = proc.getEngineBankForEdit();
    int progIdx = juce::jlimit(0, 7, getInt("program") - 1);
    auto& prog = bank.programs[(size_t) progIdx];
    grid.setProgram(&prog);
    grid.setEditStep(editStep);
    const auto& st = prog.steps[(size_t) std::clamp(editStep, 0, 63)];
    stepTypeBox.setSelectedItemIndex((int) st.type, juce::dontSendNotification);
    stepKeyBox.setSelectedItemIndex(std::clamp((int) st.keyIndex, 0, 4),
                                    juce::dontSendNotification);
    stepOctSlider.setValue(st.octave, juce::dontSendNotification);
    stepVelSlider.setValue(st.velocity, juce::dontSendNotification);
    repaint();
}

void MnArpeeAudioEditor::pushStepEditToEngine()
{
    auto& apvts = proc.getApvts();
    int progIdx = 0;
    if (auto* pp = apvts.getRawParameterValue("program"))
        progIdx = juce::jlimit(0, 7, (int) pp->load() - 1);
    juce::ScopedLock lock(proc.getEngineLock());
    auto& bank = proc.getEngineBankForEdit();
    auto& st = bank.programs[(size_t) progIdx].steps[(size_t) std::clamp(editStep, 0, 63)];
    st.type = (mnArpee::StepType) stepTypeBox.getSelectedItemIndex();
    st.keyIndex = (std::int8_t) stepKeyBox.getSelectedItemIndex();
    st.octave = (std::int8_t) stepOctSlider.getValue();
    st.velocity = (std::uint8_t) stepVelSlider.getValue();
    proc.requestBankRefresh();
    refreshFromState();
}

