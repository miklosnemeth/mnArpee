#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    juce::StringArray rates { "1/64", "1/48", "1/32", "1/24", "1/16", "1/12",
                              "1/8", "1/6", "1/4", "3/64", "3/32", "3/16", "3/8" };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterChoice>("rate", "Rate", rates, 4));
    params.push_back(std::make_unique<juce::AudioParameterInt>("octave", "Octave", -2, 2, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("gate", "Gate %", 1, 125, 80));
    params.push_back(std::make_unique<juce::AudioParameterInt>("swing", "Swing %", -50, 50, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("program", "Program", 1, 8, 1));
    params.push_back(std::make_unique<juce::AudioParameterInt>("chain", "Chain", 0, 2, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("latch", "Latch", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>("steps", "Steps", 0, 64, 16));
    return { params.begin(), params.end() };
}
}

MnArpeeAudioProcessor::MnArpeeAudioProcessor()
    : AudioProcessor(BusesProperties())
    , apvts(*this, nullptr, "mnArpee", makeLayout())
    , bank(mnArpee::makeFactoryBank())
{
    engine.setBank(bank);
}

MnArpeeAudioProcessor::~MnArpeeAudioProcessor() = default;

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

const char* MnArpeeAudioProcessor::syncName(mnArpee::SyncRate r)
{
    switch (r)
    {
        case mnArpee::SyncRate::R1_64: return "1/64";
        case mnArpee::SyncRate::R1_48: return "1/48";
        case mnArpee::SyncRate::R1_32: return "1/32";
        case mnArpee::SyncRate::R1_24: return "1/24";
        case mnArpee::SyncRate::R1_16: return "1/16";
        case mnArpee::SyncRate::R1_12: return "1/12";
        case mnArpee::SyncRate::R1_8:  return "1/8";
        case mnArpee::SyncRate::R1_6:  return "1/6";
        case mnArpee::SyncRate::R1_4:  return "1/4";
        case mnArpee::SyncRate::R3_64: return "3/64";
        case mnArpee::SyncRate::R3_32: return "3/32";
        case mnArpee::SyncRate::R3_16: return "3/16";
        default:                       return "3/8";
    }
}

void MnArpeeAudioProcessor::prepareToPlay(double, int)
{
    const juce::ScopedLock lock(engineLock);
    engine.setBank(bank);
    lastProgramParam = -1;
    lastChainParam = -1;
}

void MnArpeeAudioProcessor::releaseResources() {}

void MnArpeeAudioProcessor::applyApvtsToEngine()
{
    auto* rateP = apvts.getRawParameterValue("rate");
    auto* octP = apvts.getRawParameterValue("octave");
    auto* gateP = apvts.getRawParameterValue("gate");
    auto* swingP = apvts.getRawParameterValue("swing");
    auto* progP = apvts.getRawParameterValue("program");
    auto* chainP = apvts.getRawParameterValue("chain");
    auto* latchP = apvts.getRawParameterValue("latch");
    auto* stepsP = apvts.getRawParameterValue("steps");

    const int progIdx = juce::jlimit(0, (int) bank.programs.size() - 1,
                                     (int) progP->load() - 1);
    auto& prog = bank.programs[(size_t) progIdx];
    prog.sync = (mnArpee::SyncRate) juce::jlimit(0, 12, (int) rateP->load());
    prog.octaveShift = juce::jlimit(-2, 2, (int) octP->load());
    prog.gatePercent = juce::jlimit(1, 125, (int) gateP->load());
    prog.swingPercent = juce::jlimit(-50, 50, (int) swingP->load());
    prog.numSteps = juce::jlimit(0, 64, (int) stepsP->load());
    engine.setProgramParams(progIdx, prog.sync, prog.gatePercent, prog.swingPercent,
                            prog.octaveShift, prog.numSteps);

    if (progIdx != lastProgramParam)
    {
        lastProgramParam = progIdx;
        engine.setCurrentProgram(progIdx);
    }
    const int chainIdx = juce::jlimit(-1, (int) bank.chains.size() - 1,
                                      (int) chainP->load() - 1);
    if (chainIdx != lastChainParam)
    {
        lastChainParam = chainIdx;
        engine.setCurrentChain(chainIdx);
    }
    const bool latch = latchP->load() > 0.5f;
    if (latch != lastLatchParam)
    {
        lastLatchParam = latch;
        engine.setLatch(latch);
    }
}

void MnArpeeAudioProcessor::pushEngineBankToProcessor()
{
    const juce::ScopedLock lock(engineLock);
    engine.setBank(bank);
    lastProgramParam = -1;
    lastChainParam = -1;
}

void MnArpeeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi)
{
    juce::ScopedLock lock(engineLock);
    buffer.clear();

    if (consumeBankRefresh())
        engine.updateBank(bank); // step edits must not restart the pattern
    applyApvtsToEngine();

    mnArpee::HostPosition host;
    host.sampleRate = getSampleRate();
    if (auto* ph = getPlayHead())
    {
        auto pos = ph->getPosition();
        if (pos.hasValue())
        {
            host.bpm = pos->getBpm().orFallback(120.0);
            host.isPlaying = pos->getIsPlaying();
            host.ppqPosition = pos->getPpqPosition().orFallback(0.0);
        }
    }

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            engine.noteOn(msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            engine.noteOff(msg.getNoteNumber());
        else if (msg.isSustainPedalOn())
            engine.sustain(true);
        else if (msg.isSustainPedalOff())
            engine.sustain(false);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine.panic();
    }

    auto events = engine.render(buffer.getNumSamples(), host, {});

    juce::MidiBuffer out;
    for (const auto& e : events)
    {
        auto msg = e.isNoteOn
            ? juce::MidiMessage::noteOn(e.channel, e.note, (juce::uint8) e.velocity)
            : juce::MidiMessage::noteOff(e.channel, e.note);
        out.addEvent(msg, e.sampleOffset);
    }

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn() && ! msg.isNoteOff() && ! msg.isSustainPedalOn()
            && ! msg.isSustainPedalOff() && ! msg.isAllNotesOff() && ! msg.isAllSoundOff())
            out.addEvent(msg, meta.samplePosition);
    }
    midi.swapWith(out);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MnArpeeAudioProcessor();
}

int MnArpeeAudioProcessor::getNumPrograms() { return (int) bank.programs.size(); }

int MnArpeeAudioProcessor::getCurrentProgram()
{
    auto* p = apvts.getRawParameterValue("program");
    return juce::jlimit(1, getNumPrograms(), p != nullptr ? (int) p->load() : 1) - 1;
}

void MnArpeeAudioProcessor::setCurrentProgram(int index)
{
    if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("program")))
        p->setValueNotifyingHost(p->convertTo0to1((float) (index + 1)));
}

const juce::String MnArpeeAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < (int) bank.programs.size())
        return juce::String(bank.programs[(size_t) index].name);
    return {};
}

void MnArpeeAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    if (index >= 0 && index < (int) bank.programs.size())
        bank.programs[(size_t) index].name = newName.toStdString();
}

void MnArpeeAudioProcessor::setLatchDirect(bool on)
{
    if (auto* p = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("latch")))
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
}

void MnArpeeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // The bank is stored as a base64 attribute inside the APVTS XML, so there is
    // a single, self-describing state blob (copyXmlToBinary overwrites destData).
    juce::MemoryBlock bankBlock;
    {
        const juce::ScopedLock lock(engineLock);
        juce::MemoryOutputStream out(bankBlock, false);
        out.writeInt((int) bank.programs.size());
        for (const auto& prog : bank.programs)
        {
            out.writeString(juce::String(prog.name));
            out.writeInt(prog.numSteps);
            out.writeInt((int) prog.sync);
            out.writeInt(prog.gatePercent);
            out.writeInt(prog.swingPercent);
            out.writeInt(prog.octaveShift);
            out.writeInt(prog.transposeOct);
            for (int i = 0; i < mnArpee::maxSteps; ++i)
            {
                const auto& s = prog.steps[(size_t) i];
                out.writeByte((char) s.type);
                out.writeByte(s.keyIndex);
                out.writeByte(s.scaleStep);
                out.writeByte(s.octave);
                out.writeByte((char) s.velocity);
                out.writeShort((short) s.gateScalePercent);
                out.writeByte((char) s.chance);
            }
        }
        out.writeInt(bank.currentChain);
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

    juce::MemoryBlock bankBlock;
    if (bankBlock.fromBase64Encoding(xml->getStringAttribute("bankData")) && bankBlock.getSize() > 4)
    {
        juce::MemoryInputStream in(bankBlock, false);
        const int numProgs = in.readInt();
        if (numProgs > 0 && numProgs <= mnArpee::maxPrograms)
        {
            mnArpee::Bank loaded;
            loaded.programs.reserve((size_t) numProgs);
            for (int pi = 0; pi < numProgs; ++pi)
            {
                mnArpee::Program prog;
                prog.name = in.readString().toStdString();
                prog.numSteps = juce::jlimit(0, 64, in.readInt());
                prog.sync = (mnArpee::SyncRate) juce::jlimit(0, 12, in.readInt());
                prog.gatePercent = juce::jlimit(1, 125, in.readInt());
                prog.swingPercent = juce::jlimit(-50, 50, in.readInt());
                prog.octaveShift = juce::jlimit(-2, 2, in.readInt());
                prog.transposeOct = juce::jlimit(-3, 3, in.readInt());
                for (int i = 0; i < mnArpee::maxSteps; ++i)
                {
                    auto& s = prog.steps[(size_t) i];
                    s.type = (mnArpee::StepType) juce::jlimit(0, 5, (int) in.readByte());
                    s.keyIndex = (std::int8_t) juce::jlimit(0, 4, (int) in.readByte());
                    s.scaleStep = (std::int8_t) juce::jlimit(-12, 12, (int) in.readByte());
                    s.octave = (std::int8_t) juce::jlimit(-3, 3, (int) in.readByte());
                    s.velocity = (std::uint8_t) juce::jlimit(0, 127, (int) (unsigned char) in.readByte());
                    s.gateScalePercent = (std::uint16_t) juce::jlimit(10, 800, (int) in.readShort());
                    s.chance = (std::uint8_t) juce::jlimit(1, 100, (int) (unsigned char) in.readByte());
                }
                loaded.programs.push_back(prog);
            }
            // Reads past the end return 0, so check that the trailing int really exists.
            const bool complete = in.getPosition() + 4 <= (juce::int64) bankBlock.getSize();
            loaded.currentChain = in.readInt();
            if (complete)
            {
                // Chains and quantize settings are not stored; keep the factory ones.
                loaded.chains = bank.chains;
                loaded.numChains = bank.numChains;
                loaded.chainQuantizeBars = bank.chainQuantizeBars;
                loaded.restartChainOnSwitch = bank.restartChainOnSwitch;
                loaded.currentChain = juce::jlimit(-1, (int) loaded.chains.size() - 1,
                                                   loaded.currentChain);
                const juce::ScopedLock lock(engineLock);
                bank = loaded;
                engine.setBank(bank);
                lastProgramParam = -1;
                lastChainParam = -1;
            }
        }
    }
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
