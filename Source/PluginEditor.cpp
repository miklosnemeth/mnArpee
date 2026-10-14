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
    setColour(juce::AlertWindow::backgroundColourId, Pal::card());
    setColour(juce::AlertWindow::textColourId, Pal::text());
    setColour(juce::AlertWindow::outlineColourId, Pal::border());
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
// ValueBar
//==============================================================================
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
    setResizable(true, true);
    setResizeLimits(760, 480, 1600, 1000);

    addAndMakeVisible(grid);
    grid.onStepClicked([this](int step, int row) { onGridClicked(step, row); });
    grid.onLaneAdjusted([this](int step, StepGrid::Lane lane, int delta) { onLaneAdjusted(step, lane, delta); });

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

    statusText = "keys  -";
    reloadProgram();
    refreshPresetUi();
    startTimerHz(30);
}

MnArpeeAudioEditor::~MnArpeeAudioEditor()
{
    stopTimer();
    if (browserBox != nullptr)
    {
        browserBox->setLookAndFeel(nullptr);
        browserBox->dismiss();
    }
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
// Preset browser: a searchable list in a call-out box under the preset name.
namespace
{
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::TextEditor::Listener
{
public:
    PresetBrowser(std::vector<mnArpee::Preset> presetsIn, int activeIndexIn,
                  std::function<void(int)> chooseIn, std::function<void(int)> contextIn)
        : presets(std::move(presetsIn)), activeIndex(activeIndexIn),
          choose(std::move(chooseIn)), context(std::move(contextIn)), list("presets", this)
    {
        search.setTextToShowWhenEmpty("Search presets", Pal::dimText());
        search.setFont(mnArpee::fonts::text(16.0f));
        search.setColour(juce::TextEditor::backgroundColourId, Pal::bg());
        search.setColour(juce::TextEditor::textColourId, Pal::text());
        search.setColour(juce::TextEditor::outlineColourId, Pal::border());
        search.setColour(juce::TextEditor::focusedOutlineColourId, Pal::accent());
        search.addListener(this);
        addAndMakeVisible(search);

        list.setRowHeight(30);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour(juce::ListBox::outlineColourId, Pal::border());
        list.setOutlineThickness(1);
        addAndMakeVisible(list);

        rebuild();
        setSize(420, 460);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8);
        search.setBounds(r.removeFromTop(30));
        r.removeFromTop(8);
        list.setBounds(r);
    }

    void visibilityChanged() override
    {
        if (isVisible())
        {
            for (int row = 0; row < (int) shown.size(); ++row)
                if (shown[(size_t) row] == activeIndex)
                    list.scrollToEnsureRowIsOnscreen(row);
        }
    }

private:
    void rebuild()
    {
        const auto query = search.getText().trim();
        shown.clear();
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            const auto& p = presets[(size_t) i];
            if (query.isEmpty() || p.name.containsIgnoreCase(query) || p.group.containsIgnoreCase(query)
                || p.author.containsIgnoreCase(query))
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
        g.setColour(isActive ? Pal::accent() : Pal::text());
        g.setFont(mnArpee::fonts::text(17.0f, isActive));
        g.drawText(p.name, 12, 0, width - 130, height, juce::Justification::centredLeft, true);
        g.setColour(Pal::dimText());
        g.setFont(mnArpee::fonts::text(15.0f));
        g.drawText(p.group, width - 120, 0, 108, height, juce::Justification::centredRight, true);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= (int) shown.size())
            return;
        if (e.mods.isPopupMenu())
            context(shown[(size_t) row]);
        else
            choose(shown[(size_t) row]);
    }

    void returnKeyPressed(int row) override
    {
        if (row >= 0 && row < (int) shown.size())
            choose(shown[(size_t) row]);
    }

    std::vector<mnArpee::Preset> presets;
    std::vector<int> shown;  // library indices that match the search
    int activeIndex;
    std::function<void(int)> choose, context;
    juce::TextEditor search;
    juce::ListBox list;
};
} // namespace

void MnArpeeAudioEditor::showPresetBrowser()
{
    if (browserBox != nullptr)
    {
        browserBox->dismiss();
        return;
    }
    auto content = std::make_unique<PresetBrowser>(
        proc.getPresetList(), proc.getActivePresetIndex(),
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](int index)
        {
            if (safe == nullptr)
                return;
            if (safe->browserBox != nullptr)
                safe->browserBox->dismiss();
            safe->loadPresetChecked(index);
        },
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](int index)
        {
            if (safe != nullptr)
                safe->showPresetContextMenu(index);
        });
    auto& box = juce::CallOutBox::launchAsynchronously(std::move(content), presetButton.getScreenBounds(), nullptr);
    box.setLookAndFeel(&lnf);
    browserBox = &box;
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
    menu.showMenuAsync(juce::PopupMenu::Options(),
                       [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), libraryIndex, file = entry.file](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;
                           if (safe->browserBox != nullptr)
                               safe->browserBox->dismiss();
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

void MnArpeeAudioEditor::confirmLeavePreset(std::function<void()> proceed)
{
    const auto active = proc.getActivePreset();
    if (! active.dirty)
    {
        proceed();
        return;
    }
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Unsaved changes")
            .withMessage("Save the changes to \"" + active.name + "\" first?")
            .withButton("Save")
            .withButton("Don't save")
            .withButton("Cancel")
            .withAssociatedComponent(this),
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), proceed = std::move(proceed)](int result)
        {
            if (safe == nullptr || result == 0)
                return; // Cancel
            if (result == 2)
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
    auto* window = new juce::AlertWindow(title, message, juce::MessageBoxIconType::NoIcon, this);
    window->setLookAndFeel(&lnf);
    window->addTextEditor("name", initial);
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [window, done = std::move(done)](int result)
                                {
                                    if (result == 1)
                                        done(window->getTextEditorContents("name").trim());
                                }),
                            true);
}

void MnArpeeAudioEditor::saveActiveAs(std::function<void()> then)
{
    const auto active = proc.getActivePreset();
    promptName("Save preset", "Name of the preset:", proc.freePresetName(active.name),
               [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), then = std::move(then)](juce::String name)
               {
                   if (safe == nullptr)
                       return;
                   if (name.isEmpty() || ! safe->proc.saveActivePresetAs(name))
                   {
                       juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Cannot save",
                                                              name.isEmpty() ? juce::String("The name is empty.")
                                                                             : "A preset named \"" + name + "\" already exists.",
                                                              "OK", safe.getComponent());
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
                       juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Cannot rename",
                                                              "The name is empty or already used by another preset.",
                                                              "OK", safe.getComponent());
               });
}

void MnArpeeAudioEditor::deleteDialog(int libraryIndex)
{
    const auto list = proc.getPresetList();
    if (libraryIndex < 0 || libraryIndex >= (int) list.size() || list[(size_t) libraryIndex].factory)
        return;
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Delete preset")
            .withMessage("Delete \"" + list[(size_t) libraryIndex].name + "\"? The file is moved to the recycle bin.")
            .withButton("Delete")
            .withButton("Cancel")
            .withAssociatedComponent(this),
        [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this), libraryIndex](int result)
        {
            if (safe != nullptr && result == 1)
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
    menu.addSeparator();
    menu.addItem(4, "Import...");
    menu.addItem(5, "Export...");
    menu.addItem(6, "Show in folder");
    menu.addSeparator();
    menu.addItem(7, "Delete", saved);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton),
                       [safe = juce::Component::SafePointer<MnArpeeAudioEditor>(this)](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;
                           auto& p = safe->proc;
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
                                                   juce::AlertWindow::showMessageBoxAsync(
                                                       juce::MessageBoxIconType::WarningIcon, "Import",
                                                       "No presets were found in that file.", "OK", safe.getComponent());
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

    // Header: brand, the preset picker (arrows, name, New, Save, Menu), the held notes and the latch.
    auto header = area.removeFromTop(56);
    auto latchArea = header.removeFromRight(112);
    brandArea = header.removeFromLeft(170);
    latchButton.setBounds(latchArea.getX(), latchArea.getCentreY() - 12, 100, 24);

    header.removeFromLeft(10);
    auto picker = header.removeFromLeft(26 + 4 + 190 + 4 + 26 + 8 + 52 + 6 + 52 + 6 + 62).withSizeKeepingCentre(
        26 + 4 + 190 + 4 + 26 + 8 + 52 + 6 + 52 + 6 + 62, 34);
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
    header.removeFromLeft(14);
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
    refreshPresetUi();
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
