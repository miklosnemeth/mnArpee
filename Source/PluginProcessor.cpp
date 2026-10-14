#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int stateMagic = 0x334e4d41;    // "AMN3"
constexpr int stateMagicWithChains = 0x324e4d41; // "AMN2": has a chain section, skipped on load

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
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "program", "Program", 1, MnArpeeAudioProcessor::numUserPrograms, 1));
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
    , model(mnArpee::makeFactoryBank())
{
    programParam = apvts.getRawParameterValue("program");
    rateParam = apvts.getRawParameterValue("rate");
    gateParam = apvts.getRawParameterValue("gate");
    swingParam = apvts.getRawParameterValue("swing");
    octaveParam = apvts.getRawParameterValue("octave");
    stepsParam = apvts.getRawParameterValue("steps");
    latchParam = apvts.getRawParameterValue("latch");

    model.numPrograms = std::min(model.numPrograms, numUserPrograms);
    loadParamsFrom(model.programs[0]);
    engine.setBank(model);
    apvts.addParameterListener("program", this);
}

MnArpeeAudioProcessor::~MnArpeeAudioProcessor()
{
    apvts.removeParameterListener("program", this);
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
    v.program = juce::jlimit(0, numUserPrograms - 1, toInt(programParam) - 1);
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
    // Called with modelLock held. The parameters hold the newest values of the edited program.
    storeParamsInto(model.programs[(size_t) editedProgram]);
    {
        const juce::SpinLock::ScopedLockType lock(pendingLock);
        pendingBank = model;
    }
    if (restartPlayback)
        bankRestart.store(true);
    bankDirty.store(true, std::memory_order_release);
    ++modelVersion;
}

void MnArpeeAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    // May arrive on any thread (host automation): handle on the message thread.
    if (parameterID == "program")
        triggerAsyncUpdate();
}

void MnArpeeAudioProcessor::handleAsyncUpdate()
{
    const juce::ScopedLock lock(modelLock);
    const int newProgram = readParams().program;
    if (newProgram == editedProgram)
        return;
    storeParamsInto(model.programs[(size_t) editedProgram]);
    editedProgram = newProgram;
    loadParamsFrom(model.programs[(size_t) editedProgram]);
    publishBank();
}

mnArpee::Program MnArpeeAudioProcessor::getProgramCopy(int index) const
{
    const juce::ScopedLock lock(modelLock);
    auto prog = model.programs[(size_t) juce::jlimit(0, numUserPrograms - 1, index)];
    if (index == editedProgram)
        storeParamsInto(prog);
    return prog;
}

void MnArpeeAudioProcessor::setStep(int programIndex, int stepIndex, const mnArpee::Step& step)
{
    if (programIndex < 0 || programIndex >= numUserPrograms || stepIndex < 0
        || stepIndex >= mnArpee::maxSteps)
        return;
    const juce::ScopedLock lock(modelLock);
    model.programs[(size_t) programIndex].steps[(size_t) stepIndex] = step;
    publishBank();
}

int MnArpeeAudioProcessor::getNumPrograms() { return numUserPrograms; }

int MnArpeeAudioProcessor::getCurrentProgram() { return readParams().program; }

void MnArpeeAudioProcessor::setCurrentProgram(int index)
{
    setParamValue("program", (float) (juce::jlimit(0, numUserPrograms - 1, index) + 1));
}

const juce::String MnArpeeAudioProcessor::getProgramName(int index)
{
    if (index < 0 || index >= numUserPrograms)
        return {};
    const juce::ScopedLock lock(modelLock);
    return juce::String::fromUTF8(model.programs[(size_t) index].name.chars.data());
}

void MnArpeeAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    if (index < 0 || index >= numUserPrograms)
        return;
    const juce::ScopedLock lock(modelLock);
    model.programs[(size_t) index].name.set(newName.toStdString());
    publishBank();
}

//==============================================================================
// Audio thread

void MnArpeeAudioProcessor::prepareToPlay(double, int)
{
    {
        const juce::ScopedLock lock(modelLock);
        storeParamsInto(model.programs[(size_t) editedProgram]);
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
    const int n = std::min(input.size, mnArpee::maxInputKeys);
    for (int i = 0; i < n; ++i)
        ui.input[(size_t) i].store(input.notes[(size_t) i].note, std::memory_order_relaxed);
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

void MnArpeeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // The bank is stored as a base64 attribute inside the APVTS XML, so there is
    // a single, self-describing state blob (copyXmlToBinary overwrites destData).
    juce::MemoryBlock bankBlock;
    {
        const juce::ScopedLock lock(modelLock);
        storeParamsInto(model.programs[(size_t) editedProgram]);
        juce::MemoryOutputStream out(bankBlock, false);
        out.writeInt(stateMagic);
        out.writeInt(model.numPrograms);
        for (int pi = 0; pi < model.numPrograms; ++pi)
        {
            const auto& prog = model.programs[(size_t) pi];
            writeName(out, prog.name);
            out.writeInt(prog.numSteps);
            out.writeInt((int) prog.sync);
            out.writeInt(prog.gatePercent);
            out.writeInt(prog.swingPercent);
            out.writeInt(prog.octaveShift);
            out.writeInt(prog.transposeOct);
            for (const auto& s : prog.steps)
            {
                out.writeByte((char) s.type);
                out.writeByte(s.keyIndex);
                out.writeByte(s.scaleStep);
                out.writeByte(s.octave);
                out.writeByte((char) s.velocity);
                out.writeShort((short) s.gateScalePercent);
                out.writeByte((char) s.chance);
            }
        }
    }
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        xml->setAttribute("bankData", bankBlock.toBase64Encoding());
        copyXmlToBinary(*xml, destData);
    }
}

void MnArpeeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName(apvts.state.getType()))
        return;

    // Parse into a heap copy first; the model is only replaced when everything is valid.
    auto loaded = std::make_unique<mnArpee::Bank>(mnArpee::makeFactoryBank());
    bool bankOk = false;
    juce::MemoryBlock bankBlock;
    if (bankBlock.fromBase64Encoding(xml->getStringAttribute("bankData")))
    {
        juce::MemoryInputStream in(bankBlock, false);
        const int magic = in.readInt();
        if (magic == stateMagic || magic == stateMagicWithChains)
        {
            const int numProgs = in.readInt();
            bankOk = numProgs > 0 && numProgs <= mnArpee::maxPrograms;
            for (int pi = 0; bankOk && pi < numProgs; ++pi)
            {
                auto& prog = loaded->programs[(size_t) pi];
                readName(in, prog.name);
                prog.numSteps = juce::jlimit(0, mnArpee::maxSteps, in.readInt());
                prog.sync = (mnArpee::SyncRate) juce::jlimit(0, mnArpee::numSyncRates - 1, in.readInt());
                prog.gatePercent = juce::jlimit(1, 125, in.readInt());
                prog.swingPercent = juce::jlimit(-50, 50, in.readInt());
                prog.octaveShift = juce::jlimit(-2, 2, in.readInt());
                prog.transposeOct = juce::jlimit(-3, 3, in.readInt());
                for (auto& s : prog.steps)
                {
                    s.type = mnArpee::stepTypeFromInt((int) in.readByte());
                    s.keyIndex = (std::int8_t) juce::jlimit((int) mnArpee::rootKeyIndex, 4, (int) in.readByte());
                    s.scaleStep = (std::int8_t) juce::jlimit(-12, 12, (int) in.readByte());
                    s.octave = (std::int8_t) juce::jlimit(-3, 3, (int) in.readByte());
                    s.velocity = (std::uint8_t) juce::jlimit(0, 127, (int) (unsigned char) in.readByte());
                    s.gateScalePercent = (std::uint16_t) juce::jlimit(10, 800, (int) in.readShort());
                    s.chance = (std::uint8_t) juce::jlimit(1, 100, (int) (unsigned char) in.readByte());
                }
            }
            // Slots that are not in the state keep their factory content.
            loaded->numPrograms = numUserPrograms;

            if (magic == stateMagicWithChains)
            {
                // Chains were removed: skip the section (chains, quantize grid, restart flag).
                const int numChains = in.readInt();
                bankOk = bankOk && numChains >= 0 && numChains <= 16;
                for (int ci = 0; bankOk && ci < numChains; ++ci)
                {
                    in.readString();
                    const int length = in.readInt();
                    bankOk = length >= 0 && length <= 64;
                    in.skipNextBytes(length);
                }
                in.readDouble(); // chain quantize grid
                in.readBool();   // restart chain on switch
            }
        }
    }

    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    {
        const juce::ScopedLock lock(modelLock);
        if (bankOk)
            model = *loaded;
        editedProgram = readParams().program;
        publishBank(true);
    }
}
