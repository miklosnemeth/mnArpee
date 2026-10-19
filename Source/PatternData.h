#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <random>
#include <vector>
#include <type_traits>

//==============================================================================
// PatternData: simplified data model based on the BlueArp manual (no input pre-filtering).
//
// Manual coverage (v2.8.0):
//  - steps: 0..32 (0 = MIDI thru, 1 = quantized thru)
//  - sync: 1/64..3/8 as fractions of a bar
//  - gate 1..125%, swing -50..+50%
//  - STEP TYPE: Off/Nrm/Rst/Tie/Rnd (no Chr: every step plays a single note)
//  - KEY SELECT: Fixed/Root/k1..k5
//  - SCALE STEP -12..+12, OCTAVE -3..+3
//  - VELOCITY 0..127, GATE TIME mult, CHANCE 1..100%
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

constexpr int maxSteps = 32;
constexpr int maxInputKeys = 5;
// Step::keyIndex value for the root of the held chord (KEY SELECT "Root").
constexpr std::int8_t rootKeyIndex = -1;
constexpr int maxPrograms = 16;
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
    std::uint8_t ratchet = 1;    // 1..4: the note is repeated this many times within the step
};

struct Program
{
    Name name;
    int numSteps = 16;               // 0..32
    SyncRate sync = SyncRate::R1_16;
    int gatePercent = 80;            // 1..125
    int swingPercent = 0;            // -50..+50
    int octaveShift = 0;             // -2..+2 global arp octave
    int transposeOct = 0;            // -3..+3 program-level
    std::array<Step, maxSteps> steps {};
};

struct Bank
{
    std::array<Program, maxPrograms> programs {};
    int numPrograms = 0;
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

// Pattern editing (the numeric lane values are Key: -1 root / 0..4 key 1..5 / -2 random step,
// Type: StepType, Octave: -3..3, Velocity: 1..127, Ratchet: 1..4, Gate: 10..800 % of the preset gate).
enum class PatternLane { Key, Type, Octave, Velocity, Ratchet, Gate };
// Sets one lane of every stored step (not just the playing ones) to a value.
void setLaneValue(Program& p, PatternLane lane, int value);
// Rotates the playing steps by one: direction < 0 moves them left, > 0 right.
void shiftPattern(Program& p, int direction);
void reversePattern(Program& p);
// Fills the playing steps of one lane with random values (the first step is never a rest or a tie).
void randomizeLane(Program& p, PatternLane lane, std::mt19937& rng);

// Fills the keys of the playing steps in a classic arp order over the first numKeys keys
// (type Normal; octave, velocity and ratchet are left alone).
enum class GeneratorMode { Up, Down, UpDown, DownUp, Converge, Random };
void generateKeys(Program& p, GeneratorMode mode, int numKeys, std::mt19937& rng);

// A random bass line: every playing step becomes the Root key (or a rest, off or tie): a strong first
// step, accents on the beats, ghost notes, a few octave jumps, glides and ratchets. Other values of
// the steps (scale, gate, chance) are reset.
void generateBassline(Program& p, std::mt19937& rng);

// The values a step's gate takes in the editor (percent of the preset's gate; 100 = unchanged).
inline constexpr int gateSteps[] = { 10, 25, 50, 75, 100, 125, 150, 200, 300, 400, 600, 800 };
inline constexpr int numGateSteps = (int) (sizeof(gateSteps) / sizeof(gateSteps[0]));
// The index of the gate step nearest to a percentage.
int nearestGateStep(int percent) noexcept;

Program makeInitProgram(std::string_view name = "Init");
Bank makeFactoryBank();

// The factory presets: "Melodic" patterns on the held keys and "Bassline" patterns that play
// only the root of the held chord (every step is the Root key, a rest, a tie or off).
struct FactoryPattern
{
    const char* group;
    const char* styles; // comma separated: "House,Trance"
    Program program;
};
std::vector<FactoryPattern> makeFactoryPatterns();

} // namespace mnArpee
