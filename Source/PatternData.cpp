#include "PatternData.h"

#include <algorithm>

namespace mnArpee
{

Program makeInitProgram(const std::string& name)
{
    Program p;
    p.name = name;
    p.numSteps = 16;
    p.sync = SyncRate::R1_16;
    p.gatePercent = 80;
    p.swingPercent = 0;
    p.octaveShift = 0;
    p.transposeOct = 0;
    for (int i = 0; i < maxSteps; ++i)
    {
        Step s;
        s.type = StepType::Normal;
        s.keyIndex = 0; // k1; a szamlalo lepteti (Up), a pattern felulirhatja
        s.velocity = 96;
        p.steps[(size_t) i] = s;
    }
    return p;
}

namespace
{
Program up16(const std::string& name, SyncRate sync, int gate, int swing, int oct)
{
    auto p = makeInitProgram(name);
    p.sync = sync;
    p.gatePercent = gate;
    p.swingPercent = swing;
    p.octaveShift = oct;
    // Egyedi pattern: k1 k2 k3 k4 ismetlodik.
    for (int i = 0; i < maxSteps; ++i)
        p.steps[(size_t) i].keyIndex = static_cast<std::int8_t>(i % 4);
    return p;
}
} // namespace

Bank makeFactoryBank()
{
    Bank bank;
    bank.programs.reserve(16);
    bank.programs.push_back(up16("Trance Up 16", SyncRate::R1_16, 80, 0, 0));
    bank.programs.push_back(up16("Bass 8ths", SyncRate::R1_8, 90, 0, 0));

    {
        auto p = up16("Up-Down Pad", SyncRate::R1_8, 100, 0, 0);
        // k1 k2 k3 k4 k3 k2 (fel-le minta kezzel kirakva)
        static constexpr int seq[] = { 0, 1, 2, 3, 2, 1 };
        for (int i = 0; i < maxSteps; ++i)
            p.steps[(size_t) i].keyIndex = static_cast<std::int8_t>(seq[i % 6]);
        bank.programs.push_back(p);
    }
    {
        auto p = up16("Shuffle 16", SyncRate::R1_16, 75, 20, 0);
            bank.programs.push_back(p);
    }
    {
        auto p = makeInitProgram("Chord Stabs");
        p.numSteps = 8;
        p.sync = SyncRate::R1_8;
        p.gatePercent = 70;
        for (int i = 0; i < 8; ++i)
        {
            p.steps[(size_t) i].type = (i % 2 == 0) ? StepType::Chord : StepType::Off;
            p.steps[(size_t) i].velocity = (i % 2 == 0) ? 110 : 96;
        }
        bank.programs.push_back(p);
    }
    {
        auto p = makeInitProgram("Random Sparkle");
        p.sync = SyncRate::R1_32;
        p.gatePercent = 60;
        for (int i = 0; i < 16; ++i)
            p.steps[(size_t) i].type = StepType::Random;
        bank.programs.push_back(p);
    }
    {
        auto p = makeInitProgram("Octave Bass");
        p.numSteps = 8;
        p.sync = SyncRate::R1_8;
        p.gatePercent = 95;
        for (int i = 0; i < 8; ++i)
        {
            p.steps[(size_t) i].keyIndex = 0;
            p.steps[(size_t) i].octave = static_cast<std::int8_t>((i % 2 == 0) ? 0 : 1);
            p.steps[(size_t) i].velocity = (i % 4 == 0) ? 115 : 95;
        }
        bank.programs.push_back(p);
    }
    {
        auto p = makeInitProgram("Tie Glide");
        p.numSteps = 8;
        p.sync = SyncRate::R1_8;
        for (int i = 0; i < 8; ++i)
        {
            p.steps[(size_t) i].type = (i % 4 == 3) ? StepType::Tie : StepType::Normal;
            p.steps[(size_t) i].keyIndex = static_cast<std::int8_t>(i % 3);
        }
        bank.programs.push_back(p);
    }

    bank.chains.reserve(4);
    bank.chains.push_back(Chain{ "Verse", { 0, 1 }, -1 });
    bank.chains.push_back(Chain{ "Chorus", { 0, 4, 0, 5 }, -1 });
    bank.numChains = 4;
    bank.chainQuantizeBars = 0.25; // 1 beat 4/4-ben
    bank.restartChainOnSwitch = true;
    bank.currentChain = -1;
    return bank;
}

} // namespace mnArpee
