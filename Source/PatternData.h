#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>

//==============================================================================
// PatternData: simplified data model based on the BlueArp manual (no input pre-filtering).
//
// Manual coverage (v2.8.0):
//  - steps: 0..64 (0 = MIDI thru, 1 = quantized thru)
//  - sync: 1/64..3/8 as fractions of a bar
//  - gate 1..125%, swing -50..+50%
//  - STEP TYPE: Off/Nrm/Rst/Tie/Rnd (no Chr: every step plays a single note)
//  - KEY SELECT: Fixed/Root/k1..k5
//  - SCALE STEP -12..+12, OCTAVE -3..+3
//  - VELOCITY 0..127, GATE TIME mult, CHANCE 1..100%
//  - chains: max 16, chain quantize, restart-on-switch
// MVP simplifications (by request):
//  - no input range filter / wrap, no missing-keys substitution,
//    chord detection only for the Root key (no scale derivation), no input quantize,
//    no fixed-key sequencer mode, no scale enforcement,
//  - no arp modes: the step matrix defines the whole pattern.
//
// Every type here is trivially copyable and fixed size, so a whole Bank can be
// handed to the audio thread with a plain copy (no heap allocation).
//==============================================================================

namespace mnArpee
{

constexpr int maxSteps = 64;
constexpr int maxInputKeys = 5;
// Step::keyIndex value for the root of the held chord (KEY SELECT "Root").
constexpr std::int8_t rootKeyIndex = -1;
constexpr int maxPrograms = 16;
constexpr int maxChains = 16;
constexpr int maxChainSteps = 64;
constexpr int maxNameLength = 32; // including the terminating zero

enum class StepType : std::uint8_t
{
    Off = 0,    // produces no note
    Normal,     // normal note
    Rest,       // continues the previous step's note (extends it)
    Tie,        // overlaps the previous note (glide on mono/legato synths)
    Random = 5, // random input note (4 was Chord, which has been removed)
};

enum class SyncRate : std::uint8_t
{
    R1_64 = 0, R1_48, R1_32, R1_24, R1_16, R1_12, R1_8, R1_6, R1_4,
    R3_64, R3_32, R3_16, R3_8,
};
constexpr int numSyncRates = 13;

constexpr double syncRateToBars(SyncRate r) noexcept
{
    // Bar fractions: BlueArp manual "sync" table (1/64 .. 3/8 bar).
    switch (r)
    {
        case SyncRate::R1_64: return 1.0 / 64.0;
        case SyncRate::R1_48: return 1.0 / 48.0;
        case SyncRate::R1_32: return 1.0 / 32.0;
        case SyncRate::R1_24: return 1.0 / 24.0;
        case SyncRate::R1_16: return 1.0 / 16.0;
        case SyncRate::R1_12: return 1.0 / 12.0;
        case SyncRate::R1_8:  return 1.0 / 8.0;
        case SyncRate::R1_6:  return 1.0 / 6.0;
        case SyncRate::R1_4:  return 1.0 / 4.0;
        case SyncRate::R3_64: return 3.0 / 64.0;
        case SyncRate::R3_32: return 3.0 / 32.0;
        case SyncRate::R3_16: return 3.0 / 16.0;
        case SyncRate::R3_8:  return 3.0 / 8.0;
    }
    return 1.0 / 16.0;
}

constexpr const char* syncRateName(SyncRate r) noexcept
{
    constexpr const char* names[numSyncRates] = { "1/64", "1/48", "1/32", "1/24", "1/16",
                                                  "1/12", "1/8",  "1/6",  "1/4",  "3/64",
                                                  "3/32", "3/16", "3/8" };
    const int i = (int) r;
    return (i >= 0 && i < numSyncRates) ? names[i] : "1/16";
}

// Fixed-size, zero-terminated name.
struct Name
{
    std::array<char, maxNameLength> chars {};

    void set(std::string_view s) noexcept
    {
        chars.fill(0);
        const auto n = std::min(s.size(), chars.size() - 1);
        std::copy_n(s.data(), n, chars.data());
    }
    std::string_view view() const noexcept { return { chars.data() }; }
};

struct Step
{
    StepType type = StepType::Normal;
    // Which input note: 0..4 = k1..k5 (sorted by pitch), rootKeyIndex = root of the held
    // chord. Ignored for Random.
    std::int8_t keyIndex = 0;
    std::int8_t scaleStep = 0;   // -12..+12 semitones up (MVP: chromatic)
    std::int8_t octave = 0;      // -3..+3
    std::uint8_t velocity = 96;  // 0..127
    // Gate multiplier in percent: 100 = unchanged (instead of the manual's GATE TIME "2x" etc.).
    std::uint16_t gateScalePercent = 100;
    std::uint8_t chance = 100;   // 1..100%
};

struct Program
{
    Name name;
    int numSteps = 16;               // 0..64
    SyncRate sync = SyncRate::R1_16;
    int gatePercent = 80;            // 1..125
    int swingPercent = 0;            // -50..+50
    int octaveShift = 0;             // -2..+2 global arp octave
    int transposeOct = 0;            // -3..+3 program-level
    std::array<Step, maxSteps> steps {};
};

struct Chain
{
    Name name;
    // Chained program indices in order.
    std::array<std::int8_t, maxChainSteps> programs {};
    int length = 0;
};

struct Bank
{
    std::array<Program, maxPrograms> programs {};
    int numPrograms = 0;
    std::array<Chain, maxChains> chains {};
    int numChains = 0;
    double chainQuantizeBars = 0.25; // in bars; 0.25 = one beat in 4/4
    bool restartChainOnSwitch = true;
};

// Step types read from older states: the removed Chord (4) becomes Normal.
constexpr StepType stepTypeFromInt(int v) noexcept
{
    switch (v)
    {
        case 0: return StepType::Off;
        case 2: return StepType::Rest;
        case 3: return StepType::Tie;
        case 5: return StepType::Random;
        default: return StepType::Normal;
    }
}

static_assert(std::is_trivially_copyable_v<Bank>, "Bank must be copyable without allocation");

Program makeInitProgram(std::string_view name = "Init");
Bank makeFactoryBank();

} // namespace mnArpee
