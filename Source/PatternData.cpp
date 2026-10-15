#include "PatternData.h"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

namespace mnArpee
{

Program makeInitProgram(std::string_view name)
{
    Program p;
    p.name.set(name);
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
        s.keyIndex = 0; // k1; the pattern overrides this per step
        s.velocity = 96;
        p.steps[(size_t) i] = s;
    }
    return p;
}

namespace
{
Program up16(std::string_view name, SyncRate sync, int gate, int swing, int oct)
{
    auto p = makeInitProgram(name);
    p.sync = sync;
    p.gatePercent = gate;
    p.swingPercent = swing;
    p.octaveShift = oct;
    // Custom pattern: k1 k2 k3 k4 repeating.
    for (int i = 0; i < maxSteps; ++i)
        p.steps[(size_t) i].keyIndex = static_cast<std::int8_t>(i % 4);
    return p;
}
} // namespace

Bank makeFactoryBank()
{
    Bank bank;
    auto add = [&bank](const Program& p) { bank.programs[(size_t) bank.numPrograms++] = p; };
    add(up16("Trance Up 16", SyncRate::R1_16, 80, 0, 0));
    add(up16("Bass 8ths", SyncRate::R1_8, 90, 0, 0));

    {
        auto p = up16("Up-Down Pad", SyncRate::R1_8, 100, 0, 0);
        // k1 k2 k3 k4 k3 k2 (up-down shape spelled out by hand)
        static constexpr int seq[] = { 0, 1, 2, 3, 2, 1 };
        for (int i = 0; i < maxSteps; ++i)
            p.steps[(size_t) i].keyIndex = static_cast<std::int8_t>(seq[i % 6]);
        add(p);
    }
    {
        auto p = up16("Shuffle 16", SyncRate::R1_16, 75, 20, 0);
            add(p);
    }
    {
        auto p = makeInitProgram("Offbeat Stabs");
        p.numSteps = 8;
        p.sync = SyncRate::R1_8;
        p.gatePercent = 70;
        for (int i = 0; i < 8; ++i)
        {
            p.steps[(size_t) i].type = (i % 2 == 0) ? StepType::Normal : StepType::Off;
            p.steps[(size_t) i].keyIndex = static_cast<std::int8_t>((i / 2) % 3);
            p.steps[(size_t) i].velocity = (i % 2 == 0) ? 110 : 96;
        }
        add(p);
    }
    {
        auto p = makeInitProgram("Random Sparkle");
        p.sync = SyncRate::R1_32;
        p.gatePercent = 60;
        for (int i = 0; i < 16; ++i)
            p.steps[(size_t) i].type = StepType::Random;
        add(p);
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
        add(p);
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
        add(p);
    }

    return bank;
}

namespace
{
// Patterns are written as notation: one token per step, separated by spaces.
//   1..5  Key 1..5      R  the root of the held chord      ?  a random key
//   .     off           _  rest (the previous note goes on)
//   ~     before a note: tie (legato into it)
//   + -   before a note: an octave up / down (repeat for more)
//   !     after a note: accent      ,  after a note: soft
Step parseStep(std::string_view token)
{
    Step s;
    s.type = StepType::Normal;
    s.keyIndex = 0;
    s.velocity = 96;
    size_t i = 0;
    for (; i < token.size(); ++i)
    {
        if (token[i] == '~')
            s.type = StepType::Tie;
        else if (token[i] == '+')
            s.octave = static_cast<std::int8_t>(std::min(3, s.octave + 1));
        else if (token[i] == '-')
            s.octave = static_cast<std::int8_t>(std::max(-3, s.octave - 1));
        else
            break;
    }
    const char core = i < token.size() ? token[i++] : '1';
    switch (core)
    {
        case 'R': s.keyIndex = rootKeyIndex; break;
        case '?': s.type = StepType::Random; break;
        case '_': s.type = StepType::Rest; break;
        case '.': s.type = StepType::Off; break;
        default:
            if (core >= '1' && core <= '5')
                s.keyIndex = static_cast<std::int8_t>(core - '1');
            break;
    }
    for (; i < token.size(); ++i)
    {
        if (token[i] == '!')
            s.velocity = 120;
        else if (token[i] == ',')
            s.velocity = 70;
    }
    return s;
}

// The pattern is repeated to fill all steps, so raising STEPS goes on with it.
Program pattern(std::string_view name, SyncRate rate, int gate, int swing, std::string_view notation,
                int octaveShift = 0)
{
    std::vector<Step> steps;
    for (size_t pos = 0; pos < notation.size();)
    {
        const auto end = std::min(notation.find(' ', pos), notation.size());
        if (end > pos)
            steps.push_back(parseStep(notation.substr(pos, end - pos)));
        pos = end + 1;
    }

    auto p = makeInitProgram(name);
    p.sync = rate;
    p.gatePercent = gate;
    p.swingPercent = swing;
    p.octaveShift = octaveShift;
    p.numSteps = std::min((int) steps.size(), maxSteps);
    for (int i = 0; i < maxSteps; ++i)
        p.steps[(size_t) i] = steps[(size_t) i % steps.size()];
    return p;
}

std::string repeat(std::string_view token, int times)
{
    std::string out;
    for (int i = 0; i < times; ++i)
        out += (i > 0 ? " " : "") + std::string(token);
    return out;
}
} // namespace

std::vector<FactoryPattern> makeFactoryPatterns()
{
    std::vector<FactoryPattern> out;
    auto melodic = [&out](const char* styles, const Program& p) { out.push_back({ "Melodic", styles, p }); };
    auto bass = [&out](const char* styles, const Program& p) { out.push_back({ "Bassline", styles, p }); };

    // The first patterns: the original factory set, except its two bass lines (they are
    // root-only bass lines below).
    const auto bank = makeFactoryBank();
    for (int i = 0; i < bank.numPrograms; ++i)
    {
        const auto name = std::string_view(bank.programs[(size_t) i].name.chars.data());
        if (name == "Bass 8ths" || name == "Octave Bass")
            continue;
        const char* styles = name == "Trance Up 16"   ? "Trance"
                           : name == "Up-Down Pad"    ? "Ambient"
                           : name == "Shuffle 16"     ? "House"
                           : name == "Offbeat Stabs"  ? "House"
                           : name == "Random Sparkle" ? "Ambient"
                                                      : "Synthwave"; // Tie Glide
        melodic(styles, bank.programs[(size_t) i]);
    }

    melodic("Pop", pattern("Up 8ths", SyncRate::R1_8, 85, 0, "1 2 3 4 1 2 3 4"));
    melodic("Trance", pattern("Down 16ths", SyncRate::R1_16, 75, 0, repeat("4 3 2 1", 4)));
    melodic("Pop", pattern("Up-Down 16", SyncRate::R1_16, 80, 0, repeat("1 2 3 4 5 4 3 2", 2)));
    melodic("Trance", pattern("Octave Climb", SyncRate::R1_16, 80, 0, "1 2 3 +1 +2 +3 +1 3 2 1 2 3 +1 +2 +1 3"));
    melodic("Pop", pattern("Pluck Thirds", SyncRate::R1_16, 55, 0, repeat("1 3 2 4", 4)));
    melodic("Techno", pattern("Accent Climb", SyncRate::R1_16, 75, 0, "1! 2 3 2 1! 2 3 4 1! 2 3 2 1! 3 2 4"));
    melodic("Trance", pattern("Gated Trance", SyncRate::R1_16, 50, 0, repeat("1 . 2 . 3 . 2 .", 2)));
    melodic("Synthwave", pattern("Cascade", SyncRate::R1_16, 70, 0, "5 4 3 2 1 5 4 3 2 1 5 4 3 2 1 5"));
    melodic("Disco", pattern("Skipping Thirds", SyncRate::R1_16, 75, 0, repeat("1 3 2 4 3 5 4 5", 2)));
    melodic("Chill", pattern("Triplet Up", SyncRate::R1_12, 80, 0, "1 2 3 4 5 4 3 2 1 2 3 4"));
    melodic("Ambient", pattern("Random 8ths", SyncRate::R1_8, 80, 0, repeat("?", 8)));
    melodic("Funk", pattern("Swing Pluck", SyncRate::R1_8, 70, 30, "1 2 3 2 4 3 2 3"));
    melodic("House", pattern("Echo Stabs", SyncRate::R1_16, 40, 0, ". 1 . 1 . 2 . 2 . 3 . 3 . 4 . 4"));
    melodic("Synthwave", pattern("Wide Arp", SyncRate::R1_8, 90, 0, "1 +3 2 +4 3 +5 +1 +2"));
    melodic("Ambient", pattern("Slow Pad Arp", SyncRate::R1_4, 100, 0, "1 2 3 4 5 4 3 2"));
    melodic("Chill", pattern("Tie Climb", SyncRate::R1_8, 100, 0, "1 ~2 ~3 ~4 1 ~2 ~3 ~+1"));

    // Basslines: only the root of the held chord, an octave below it.
    bass("Pop", pattern("Bass 8ths", SyncRate::R1_8, 85, 0, repeat("R", 8), -1));
    bass("Techno", pattern("Bass 16ths", SyncRate::R1_16, 70, 0, repeat("R", 16), -1));
    bass("House,Trance", pattern("Offbeat Bass", SyncRate::R1_8, 75, 0, ". R . R . R . R", -1));
    bass("Disco", pattern("Octave Bass", SyncRate::R1_16, 70, 0, repeat("R +R", 8), -1));
    bass("Trance", pattern("Trance Bass", SyncRate::R1_16, 60, 0, repeat(". R R R", 4), -1));
    bass("House", pattern("Pumping Bass", SyncRate::R1_16, 60, 0, repeat("R! R R R R R! R R", 2), -1));
    bass("Techno", pattern("Rolling Bass", SyncRate::R1_16, 65, 0, "R R +R R R R +R R R R +R R R +R R +R", -1));
    bass("Synthwave", pattern("Dotted Bass", SyncRate::R1_16, 90, 0, "R _ _ R _ _ R _ R _ _ R _ _ R _", -1));
    bass("Ambient", pattern("Long Sub", SyncRate::R1_4, 100, 0, "R _ _ _ R _ _ _", -2));
    bass("Synthwave", pattern("Glide Bass", SyncRate::R1_8, 100, 0, "R R ~+R R R R ~+R R", -1));
    bass("Techno", pattern("Galloping Bass", SyncRate::R1_16, 65, 0, "R R . R R . R R R R . R R . R R", -1));
    bass("Funk", pattern("Funk Bass", SyncRate::R1_16, 55, 0, "R! . . R, . . R . R! . . R, . R . .", -1));
    bass("Dubstep", pattern("Half-time Bass", SyncRate::R1_8, 90, 0, "R _ _ _ R _ +R _", -1));
    bass("Funk", pattern("Swing Bass", SyncRate::R1_8, 80, 25, "R . R R . R R .", -1));
    bass("House", pattern("Ghost 16ths", SyncRate::R1_16, 60, 0, repeat("R R,", 8), -1));
    return out;
}

} // namespace mnArpee
