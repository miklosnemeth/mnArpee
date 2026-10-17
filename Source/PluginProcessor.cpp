#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int stateMagic = 0x344e4d41;           // "AMN4": 32 steps per program
constexpr int stateMagic64Steps = 0x334e4d41;    // "AMN3": 64 steps per program, the rest is dropped on load
constexpr int stateMagicWithChains = 0x324e4d41; // "AMN2": 64 steps and a chain section, skipped on load

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    juce::StringArray rates;
    for (int i = 0; i < mnArpee::numSyncRates; ++i)
        rates.add(mnArpee::syncRateName((mnArpee::SyncRate) i));
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterChoice>("rate", "Rate", rates, 4));
    params.push_back(std::make_unique<juce::AudioParameterInt>("octave", "Octave", -2, 2, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("gate", "Gate %", 1, 125, 80));
    params.push_back(std::make_unique<juce::AudioParameterInt>("swing", "Swing %", -50, 50, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("latch", "Latch", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>("steps", "Steps", 0, mnArpee::maxSteps, 16));
    return { params.begin(), params.end() };
}

int toInt(const std::atomic<float>* p) { return (int) std::lround(p->load()); }

void writeName(juce::OutputStream& out, const mnArpee::Name& n)
{
    out.writeString(juce::String::fromUTF8(n.chars.data()));
}

void readName(juce::InputStream& in, mnArpee::Name& n)
{
    n.set(in.readString().toStdString());
}
} // namespace

MnArpeeAudioProcessor::MnArpeeAudioProcessor()
    : AudioProcessor(BusesProperties())
    , apvts(*this, nullptr, "mnArpee", makeLayout())
{
    rateParam = apvts.getRawParameterValue("rate");
    gateParam = apvts.getRawParameterValue("gate");
    swingParam = apvts.getRawParameterValue("swing");
    octaveParam = apvts.getRawParameterValue("octave");
    stepsParam = apvts.getRawParameterValue("steps");
    latchParam = apvts.getRawParameterValue("latch");

    library.refresh();
    model.numPrograms = 2;

    // The first preset of the library, or an empty pattern if the files cannot be written.
    mnArpee::Preset first;
    if (library.size() > 0)
    {
        first = library.presets().front();
    }
    else
    {
        first.name = "Init";
        first.program = mnArpee::makeInitProgram("Init");
    }
    {
        const juce::ScopedLock lock(modelLock);
        activatePresetLocked(first, true, false);
    }
    engine.setBank(model);
}

MnArpeeAudioProcessor::~MnArpeeAudioProcessor()
{
    cancelPendingUpdate();
}

const juce::String MnArpeeAudioProcessor::getName() const { return "mnArpee"; }
bool MnArpeeAudioProcessor::acceptsMidi() const { return true; }
bool MnArpeeAudioProcessor::producesMidi() const { return true; }
bool MnArpeeAudioProcessor::isMidiEffect() const { return true; }
double MnArpeeAudioProcessor::getTailLengthSeconds() const { return 0.0; }
bool MnArpeeAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* MnArpeeAudioProcessor::createEditor()
{
    return new MnArpeeAudioEditor(*this);
}

//==============================================================================
// Model (message thread)

MnArpeeAudioProcessor::ParamValues MnArpeeAudioProcessor::readParams() const
{
    ParamValues v;
    v.program = activeSlot.load();
    v.rate = juce::jlimit(0, mnArpee::numSyncRates - 1, toInt(rateParam));
    v.gate = juce::jlimit(1, 125, toInt(gateParam));
    v.swing = juce::jlimit(-50, 50, toInt(swingParam));
    v.octave = juce::jlimit(-2, 2, toInt(octaveParam));
    v.steps = juce::jlimit(0, mnArpee::maxSteps, toInt(stepsParam));
    v.latch = latchParam->load() > 0.5f;
    return v;
}

void MnArpeeAudioProcessor::storeParamsInto(mnArpee::Program& prog) const
{
    const auto v = readParams();
    prog.sync = (mnArpee::SyncRate) v.rate;
    prog.gatePercent = v.gate;
    prog.swingPercent = v.swing;
    prog.octaveShift = v.octave;
    prog.numSteps = v.steps;
}

void MnArpeeAudioProcessor::setParamValue(const char* id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
    {
        const float norm = p->convertTo0to1(plainValue);
        if (std::abs(p->getValue() - norm) > 1.0e-6f)
            p->setValueNotifyingHost(norm);
    }
}

void MnArpeeAudioProcessor::loadParamsFrom(const mnArpee::Program& prog)
{
    setParamValue("rate", (float) prog.sync);
    setParamValue("gate", (float) prog.gatePercent);
    setParamValue("swing", (float) prog.swingPercent);
    setParamValue("octave", (float) prog.octaveShift);
    setParamValue("steps", (float) prog.numSteps);
}

void MnArpeeAudioProcessor::publishBank(bool restartPlayback)
{
    // Called with modelLock held. The parameters hold the newest values of the active pattern.
    storeParamsInto(model.programs[(size_t) activeSlot.load()]);
    {
        const juce::SpinLock::ScopedLockType lock(pendingLock);
        pendingBank = model;
    }
    if (restartPlayback)
        bankRestart.store(true);
    bankDirty.store(true, std::memory_order_release);
    ++modelVersion;
}

mnArpee::Program MnArpeeAudioProcessor::getProgramCopy() const
{
    const juce::ScopedLock lock(modelLock);
    auto prog = model.programs[(size_t) activeSlot.load()];
    storeParamsInto(prog);
    return prog;
}

void MnArpeeAudioProcessor::setStep(int stepIndex, const mnArpee::Step& step)
{
    if (stepIndex < 0 || stepIndex >= mnArpee::maxSteps)
        return;
    const juce::ScopedLock lock(modelLock);
    pushUndoLocked();
    model.programs[(size_t) activeSlot.load()].steps[(size_t) stepIndex] = step;
    publishBank();
}

void MnArpeeAudioProcessor::setAllSteps(const std::array<mnArpee::Step, mnArpee::maxSteps>& steps)
{
    const juce::ScopedLock lock(modelLock);
    pushUndoLocked();
    model.programs[(size_t) activeSlot.load()].steps = steps;
    publishBank();
}

void MnArpeeAudioProcessor::pushUndoLocked()
{
    // A burst of edits (dragging a value) is one undo step: the state before the first edit.
    const auto now = juce::Time::getMillisecondCounter();
    if (undoStack.empty() || now - lastEditMs > 600)
    {
        undoStack.push_back(model.programs[(size_t) activeSlot.load()].steps);
        if (undoStack.size() > 100)
            undoStack.erase(undoStack.begin());
    }
    lastEditMs = now;
    redoStack.clear();
}

bool MnArpeeAudioProcessor::canUndo() const
{
    const juce::ScopedLock lock(modelLock);
    return ! undoStack.empty();
}

bool MnArpeeAudioProcessor::canRedo() const
{
    const juce::ScopedLock lock(modelLock);
    return ! redoStack.empty();
}

bool MnArpeeAudioProcessor::undo()
{
    const juce::ScopedLock lock(modelLock);
    if (undoStack.empty())
        return false;
    auto& steps = model.programs[(size_t) activeSlot.load()].steps;
    redoStack.push_back(steps);
    steps = undoStack.back();
    undoStack.pop_back();
    lastEditMs = 0;
    publishBank();
    return true;
}

bool MnArpeeAudioProcessor::redo()
{
    const juce::ScopedLock lock(modelLock);
    if (redoStack.empty())
        return false;
    auto& steps = model.programs[(size_t) activeSlot.load()].steps;
    undoStack.push_back(steps);
    steps = redoStack.back();
    redoStack.pop_back();
    lastEditMs = 0;
    publishBank();
    return true;
}

//==============================================================================
// Presets (message thread)

mnArpee::Preset MnArpeeAudioProcessor::currentPresetLocked() const
{
    auto preset = active;
    preset.program = model.programs[(size_t) activeSlot.load()];
    storeParamsInto(preset.program);
    preset.program.name.set(preset.name.toStdString());
    return preset;
}

void MnArpeeAudioProcessor::activatePresetLocked(const mnArpee::Preset& preset, bool saved, bool restartPlayback)
{
    // The pattern goes into the slot that is not playing, so the engine changes over at the
    // end of the running pattern (at once when it is idle).
    const int slot = 1 - activeSlot.load();
    model.programs[(size_t) slot] = preset.program;
    model.programs[(size_t) slot].name.set(preset.name.toStdString());
    active = preset;
    active.program = {};
    undoStack.clear(); // a new pattern starts a new history
    redoStack.clear();
    activeSlot.store(slot);
    loadParamsFrom(model.programs[(size_t) slot]);
    savedSnapshot = saved ? mnArpee::presetSnapshot(preset) : juce::String();
    publishBank(restartPlayback);
}

MnArpeeAudioProcessor::ActivePreset MnArpeeAudioProcessor::getActivePreset() const
{
    const juce::ScopedLock lock(modelLock);
    ActivePreset a;
    a.name = active.name;
    a.group = active.group;
    a.author = active.author;
    a.tags = active.tags;
    a.file = active.file;
    a.factory = active.factory;
    a.dirty = savedSnapshot.isEmpty() || mnArpee::presetSnapshot(currentPresetLocked()) != savedSnapshot;
    return a;
}

std::vector<mnArpee::Preset> MnArpeeAudioProcessor::getPresetList() const
{
    const juce::ScopedLock lock(modelLock);
    return library.presets();
}

int MnArpeeAudioProcessor::getActivePresetIndex() const
{
    const juce::ScopedLock lock(modelLock);
    return library.indexOfFile(active.file);
}

bool MnArpeeAudioProcessor::loadPreset(int libraryIndex)
{
    const juce::ScopedLock lock(modelLock);
    if (libraryIndex < 0 || libraryIndex >= library.size())
        return false;
    activatePresetLocked(library.presets()[(size_t) libraryIndex], true, false);
    return true;
}

void MnArpeeAudioProcessor::setPresetFavorite(int libraryIndex, bool favorite)
{
    const juce::ScopedLock lock(modelLock);
    if (libraryIndex >= 0 && libraryIndex < library.size())
        library.setFavorite(library.presets()[(size_t) libraryIndex].name, favorite);
}

void MnArpeeAudioProcessor::newPreset()
{
    const juce::ScopedLock lock(modelLock);
    mnArpee::Preset p;
    p.name = library.freeName("New Arp");
    p.program = mnArpee::makeInitProgram();
    activatePresetLocked(p, false, false);
}

bool MnArpeeAudioProcessor::saveActivePreset()
{
    const juce::ScopedLock lock(modelLock);
    if (active.factory || active.file == juce::File())
        return false;
    auto preset = currentPresetLocked();
    if (! library.save(preset))
        return false;
    active.file = preset.file;
    savedSnapshot = mnArpee::presetSnapshot(preset);
    return true;
}

bool MnArpeeAudioProcessor::saveActivePresetAs(const juce::String& name)
{
    const juce::ScopedLock lock(modelLock);
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || library.nameExists(trimmed))
        return false;
    auto preset = currentPresetLocked();
    preset.name = trimmed;
    preset.factory = false;
    preset.file = {};
    preset.group = "User";
    if (! library.save(preset))
        return false;
    active = preset;
    active.program = {};
    model.programs[(size_t) activeSlot.load()].name.set(preset.name.toStdString());
    savedSnapshot = mnArpee::presetSnapshot(preset);
    publishBank();
    return true;
}

void MnArpeeAudioProcessor::revertActivePreset()
{
    const juce::ScopedLock lock(modelLock);
    const int index = library.indexOfFile(active.file);
    if (index >= 0)
        activatePresetLocked(library.presets()[(size_t) index], true, false);
}

void MnArpeeAudioProcessor::setActivePresetStyles(const juce::String& styles)
{
    const juce::ScopedLock lock(modelLock);
    active.tags = styles.trim();
}

bool MnArpeeAudioProcessor::renamePreset(int libraryIndex, const juce::String& newName)
{
    const juce::ScopedLock lock(modelLock);
    if (libraryIndex < 0 || libraryIndex >= library.size())
        return false;
    const auto entry = library.presets()[(size_t) libraryIndex];
    const auto name = newName.trim();
    if (entry.factory || name.isEmpty() || (library.nameExists(name) && ! entry.name.equalsIgnoreCase(name)))
        return false;
    mnArpee::Preset renamed;
    if (! library.rename(entry, name, renamed))
        return false;
    if (active.file == entry.file)
    {
        active.name = renamed.name;
        active.file = renamed.file;
        model.programs[(size_t) activeSlot.load()].name.set(renamed.name.toStdString());
        savedSnapshot = mnArpee::presetSnapshot(renamed);
        publishBank();
    }
    return true;
}

bool MnArpeeAudioProcessor::deletePreset(int libraryIndex)
{
    const juce::ScopedLock lock(modelLock);
    if (libraryIndex < 0 || libraryIndex >= library.size())
        return false;
    const auto entry = library.presets()[(size_t) libraryIndex];
    if (! library.remove(entry))
        return false;
    if (active.file == entry.file)
    {
        active.file = {}; // the pattern stays, as an unsaved preset
        savedSnapshot = {};
    }
    return true;
}

int MnArpeeAudioProcessor::duplicatePreset(int libraryIndex)
{
    const juce::ScopedLock lock(modelLock);
    if (libraryIndex < 0 || libraryIndex >= library.size())
        return -1;
    mnArpee::Preset copy;
    if (! library.duplicate(library.presets()[(size_t) libraryIndex], copy))
        return -1;
    return library.indexOfFile(copy.file);
}

int MnArpeeAudioProcessor::importPresets(const juce::File& file)
{
    const juce::ScopedLock lock(modelLock);
    juce::String first;
    const int count = library.importFile(file, &first);
    if (count > 0)
        for (int i = 0; i < library.size(); ++i)
            if (! library.presets()[(size_t) i].factory && library.presets()[(size_t) i].name == first)
            {
                activatePresetLocked(library.presets()[(size_t) i], true, false);
                break;
            }
    return count;
}

bool MnArpeeAudioProcessor::exportActivePreset(const juce::File& file) const
{
    const juce::ScopedLock lock(modelLock);
    return library.exportPreset(currentPresetLocked(), file);
}

juce::String MnArpeeAudioProcessor::freePresetName(const juce::String& wanted) const
{
    const juce::ScopedLock lock(modelLock);
    return library.freeName(wanted);
}

//==============================================================================
// Host programs: the preset list (message thread for loads).

int MnArpeeAudioProcessor::getNumPrograms() { return numHostPrograms; }

int MnArpeeAudioProcessor::getCurrentProgram()
{
    return juce::jlimit(0, numHostPrograms - 1, getActivePresetIndex());
}

void MnArpeeAudioProcessor::setCurrentProgram(int index)
{
    // May be called on any thread: the load happens on the message thread.
    pendingHostProgram.store(index);
    triggerAsyncUpdate();
}

void MnArpeeAudioProcessor::handleAsyncUpdate()
{
    const int index = pendingHostProgram.exchange(-1);
    if (index >= 0 && index != getActivePresetIndex())
        loadPreset(index);
}

const juce::String MnArpeeAudioProcessor::getProgramName(int index)
{
    const juce::ScopedLock lock(modelLock);
    if (index < 0 || index >= library.size())
        return "-";
    return library.presets()[(size_t) index].name;
}

void MnArpeeAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
// Audio thread

void MnArpeeAudioProcessor::prepareToPlay(double, int)
{
    {
        const juce::ScopedLock lock(modelLock);
        storeParamsInto(model.programs[(size_t) activeSlot.load()]);
        engine.setBank(model);
    }
    bankDirty.store(false);
    bankRestart.store(false);
    applied = readParams(); // the engine bank above already holds these values
    applied.latch = ! applied.latch;
    resendProgram = true;
    events.clear();
    outMidi.ensureSize(4096);
    for (auto& ch : thruOn)
        ch.fill(false);
    wasThru = false;
}

void MnArpeeAudioProcessor::releaseResources() {}

void MnArpeeAudioProcessor::pickUpPendingBank()
{
    if (! bankDirty.load(std::memory_order_acquire))
        return;
    const juce::SpinLock::ScopedTryLockType lock(pendingLock);
    if (! lock.isLocked())
        return; // the editor is publishing right now: take it next block
    if (bankRestart.exchange(false))
        engine.setBank(pendingBank);
    else
        engine.updateBank(pendingBank); // step edits must not restart the pattern
    bankDirty.store(false);
    resendProgram = true; // the new bank may not know the selected program yet
}

void MnArpeeAudioProcessor::applyParamsToEngine()
{
    const auto v = readParams();
    const bool programChanged = v.program != applied.program;
    if (programChanged || resendProgram)
        engine.setCurrentProgram(v.program);
    resendProgram = false;
    // During a program change the parameters still hold the previous program's values
    // until the message thread loads the new ones: they must not go into the new program.
    if (! programChanged
        && (v.rate != applied.rate || v.gate != applied.gate || v.swing != applied.swing
            || v.octave != applied.octave || v.steps != applied.steps))
    {
        engine.setProgramParams(v.program, (mnArpee::SyncRate) v.rate, v.gate, v.swing,
                                v.octave, v.steps);
    }
    if (v.latch != applied.latch)
        engine.setLatch(v.latch);
    applied = v;
}

void MnArpeeAudioProcessor::releaseThruNotes(int at)
{
    for (int ch = 0; ch < 16; ++ch)
        for (int n = 0; n < 128; ++n)
            if (thruOn[(size_t) ch][(size_t) n])
            {
                outMidi.addEvent(juce::MidiMessage::noteOff(ch + 1, n), at);
                thruOn[(size_t) ch][(size_t) n] = false;
            }
}

void MnArpeeAudioProcessor::handleMidiMessage(const juce::MidiMessage& msg, int at)
{
    if (msg.isNoteOn())
    {
        const int ch = msg.getChannel();
        engine.setOutputChannel(ch);
        engine.noteOn(msg.getNoteNumber(), msg.getVelocity());
        if (engine.isThru())
        {
            outMidi.addEvent(msg, at);
            thruOn[(size_t) (ch - 1)][(size_t) msg.getNoteNumber()] = true;
        }
    }
    else if (msg.isNoteOff())
    {
        engine.noteOff(msg.getNoteNumber());
        auto& on = thruOn[(size_t) (msg.getChannel() - 1)][(size_t) msg.getNoteNumber()];
        if (on)
        {
            outMidi.addEvent(msg, at);
            on = false;
        }
    }
    else if (msg.isSustainPedalOn() || msg.isSustainPedalOff())
    {
        engine.sustain(msg.isSustainPedalOn());
        if (engine.isThru())
            outMidi.addEvent(msg, at);
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        engine.panic();
        releaseThruNotes(at);
        outMidi.addEvent(msg, at);
    }
    else
    {
        outMidi.addEvent(msg, at); // CC, pitch bend, aftertouch, ... pass through
    }
}

void MnArpeeAudioProcessor::renderSegment(int start, int length, const mnArpee::HostPosition& host)
{
    auto seg = host;
    if (host.isPlaying)
        seg.ppqPosition += start * host.bpm / (60.0 * host.sampleRate);
    events.clear();
    engine.render(length, seg, events);
    for (const auto& e : events)
    {
        const auto msg = e.isNoteOn
            ? juce::MidiMessage::noteOn(e.channel, e.note, (juce::uint8) e.velocity)
            : juce::MidiMessage::noteOff(e.channel, e.note);
        outMidi.addEvent(msg, start + e.sampleOffset);
    }

    const bool thru = engine.isThru();
    if (wasThru && ! thru)
        releaseThruNotes(start + length - 1);
    wasThru = thru;
}

void MnArpeeAudioProcessor::updateUiStatus()
{
    ui.playStep.store(engine.getLastPlayedStep(), std::memory_order_relaxed);
    ui.playingProgram.store(engine.getCurrentProgram(), std::memory_order_relaxed);
    const auto input = engine.getActiveInput();
    mnArpee::InputNotes keys;
    mnArpee::reduceToKeys(input, keys);
    const int n = std::min(input.size, (int) UiStatus::maxShownInput);
    for (int i = 0; i < n; ++i)
    {
        const int note = input.notes[(size_t) i].note;
        ui.input[(size_t) i].store(note * 4 + (int) mnArpee::keyUseOf(keys, note), std::memory_order_relaxed);
    }
    ui.chord.store(mnArpee::detectChord(input).code(), std::memory_order_relaxed);
    ui.numInput.store(n, std::memory_order_release);
}

void MnArpeeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    pickUpPendingBank();
    applyParamsToEngine();

    mnArpee::HostPosition host;
    host.sampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            host.bpm = pos->getBpm().orFallback(120.0);
            host.isPlaying = pos->getIsPlaying();
            host.ppqPosition = pos->getPpqPosition().orFallback(0.0);
        }
    }
    if (host.bpm <= 0.0)
        host.bpm = 120.0;

    outMidi.clear();
    outMidi.ensureSize(4096);
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit(0, std::max(0, numSamples - 1), meta.samplePosition);
        if (at > pos)
        {
            renderSegment(pos, at - pos, host);
            pos = at;
        }
        handleMidiMessage(meta.getMessage(), at);
    }
    if (pos < numSamples)
        renderSegment(pos, numSamples - pos, host);

    midi.swapWith(outMidi);
    updateUiStatus();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MnArpeeAudioProcessor();
}

//==============================================================================
// State
//
// The project stores the active preset's current content (not a reference to a file), so it
// plays back the same wherever it is opened. When the library has a preset of that name the
// active preset is bound to it again, otherwise it comes back as an unsaved one.

void MnArpeeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::String presetText;
    {
        const juce::ScopedLock lock(modelLock);
        presetText = mnArpee::presetToXml(currentPresetLocked())->toString(juce::XmlElement::TextFormat().singleLine());
    }
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        xml->setAttribute("preset", presetText);
        xml->setAttribute("zoom", (double) zoom.load());
        copyXmlToBinary(*xml, destData);
    }
}

namespace
{
// Reads the bank of states saved before the presets ("AMN2".."AMN4"); the programs go into `out`.
bool readLegacyBank(const juce::MemoryBlock& bankBlock, mnArpee::Bank& out)
{
    juce::MemoryInputStream in(bankBlock, false);
    const int magic = in.readInt();
    if (magic != stateMagic && magic != stateMagic64Steps && magic != stateMagicWithChains)
        return false;

    const int numProgs = in.readInt();
    if (numProgs <= 0 || numProgs > mnArpee::maxPrograms)
        return false;
    out.numPrograms = numProgs;
    for (int pi = 0; pi < numProgs; ++pi)
    {
        auto& prog = out.programs[(size_t) pi];
        readName(in, prog.name);
        prog.numSteps = juce::jlimit(0, mnArpee::maxSteps, in.readInt());
        prog.sync = (mnArpee::SyncRate) juce::jlimit(0, mnArpee::numSyncRates - 1, in.readInt());
        prog.gatePercent = juce::jlimit(1, 125, in.readInt());
        prog.swingPercent = juce::jlimit(-50, 50, in.readInt());
        prog.octaveShift = juce::jlimit(-2, 2, in.readInt());
        prog.transposeOct = juce::jlimit(-3, 3, in.readInt());
        // States from before the 32-step limit store 64 steps per program.
        const int stepsInState = magic == stateMagic ? mnArpee::maxSteps : 64;
        for (int si = 0; si < stepsInState; ++si)
        {
            mnArpee::Step dropped;
            auto& s = si < mnArpee::maxSteps ? prog.steps[(size_t) si] : dropped;
            s.type = mnArpee::stepTypeFromInt((int) in.readByte());
            s.keyIndex = (std::int8_t) juce::jlimit((int) mnArpee::rootKeyIndex, 4, (int) in.readByte());
            s.scaleStep = (std::int8_t) juce::jlimit(-12, 12, (int) in.readByte());
            s.octave = (std::int8_t) juce::jlimit(-3, 3, (int) in.readByte());
            s.velocity = (std::uint8_t) juce::jlimit(0, 127, (int) (unsigned char) in.readByte());
            s.gateScalePercent = (std::uint16_t) juce::jlimit(10, 800, (int) in.readShort());
            s.chance = (std::uint8_t) juce::jlimit(1, 100, (int) (unsigned char) in.readByte());
        }
    }
    return true; // the chain section after the programs is not needed
}
} // namespace

void MnArpeeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName(apvts.state.getType()))
        return;

    // The active preset: from the "preset" attribute, or (older states) the selected program
    // of the stored bank, which comes back as an unsaved preset.
    mnArpee::Preset preset;
    bool havePreset = false;
    if (auto parsed = juce::parseXML(xml->getStringAttribute("preset")))
        havePreset = mnArpee::presetFromXml(*parsed, preset);

    if (! havePreset)
    {
        juce::MemoryBlock bankBlock;
        if (bankBlock.fromBase64Encoding(xml->getStringAttribute("bankData")))
        {
            auto bank = std::make_unique<mnArpee::Bank>();
            if (readLegacyBank(bankBlock, *bank))
            {
                int selected = 1;
                for (const auto* e : xml->getChildIterator())
                    if (e->getStringAttribute("id") == "program")
                        selected = e->getIntAttribute("value", 1);
                const auto& prog = bank->programs[(size_t) juce::jlimit(0, bank->numPrograms - 1, selected - 1)];
                preset.name = juce::String::fromUTF8(prog.name.chars.data());
                preset.program = prog;
                preset.group = "User";
                havePreset = preset.name.isNotEmpty();
            }
        }
    }

    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    setZoom((float) xml->getDoubleAttribute("zoom", 1.0));
    if (! havePreset)
        return; // nothing usable in the state: the parameters are what was loaded

    const juce::ScopedLock lock(modelLock);
    preset.factory = false;
    juce::String snapshot;
    if (const int index = library.indexOf(preset); index >= 0)
    {
        preset.file = library.presets()[(size_t) index].file;
        preset.factory = library.presets()[(size_t) index].factory;
        snapshot = mnArpee::presetSnapshot(library.presets()[(size_t) index]);
    }
    else
    {
        preset.file = {};
    }
    activatePresetLocked(preset, false, true);
    savedSnapshot = snapshot;
}
