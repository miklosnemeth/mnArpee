#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

//==============================================================================
// PatternData: simplified data model based on the BlueArp manual (no input pre-filtering).
//
// Manual coverage (v2.8.0):
//  - steps: 0..64 (0 = MIDI thru, 1 = quantized thru)
//  - sync: 1/64..3/8 as fractions of a bar
//  - gate 1..125%, swing -50..+50%
//  - STEP TYPE: Off/Nrm/Rst/Tie/Chr/Rnd
//  - KEY SELECT: Fixed/Root/k1..k5
//  - SCALE STEP -12..+12, OCTAVE -3..+3
//  - VELOCITY 0..127, GATE TIME mult, CHANCE 1..100%
//  - chains: max 16, chain quantize, restart-on-switch
// MVP simplifications (by request):
//  - no input range filter / wrap, no missing-keys substitution,
//    no chord detection / scale derivation, no input quantize,
//    no fixed-key sequencer mode, no scale enforcement,
//  - no arp modes: the step matrix defines the whole pattern.
//==============================================================================

namespace mnArpee
{

constexpr int maxSteps = 64;
constexpr int maxInputKeys = 5;
constexpr int maxPrograms = 128;
constexpr int maxChains = 16;
constexpr int maxChainSteps = 64;

enum class StepType : std::uint8_t
{
    Off = 0,    // produces no note
    Normal,     // normal note
    Rest,       // continues the previous step's note (extends it)
    Tie,        // overlaps the previous note (glide on mono/legato synths)
    Chord,      // all input notes at once
    Random,     // random input note
};

enum class SyncRate : std::uint8_t
{
    R1_64 = 0, R1_48, R1_32, R1_24, R1_16, R1_12, R1_8, R1_6, R1_4,
    R3_64, R3_32, R3_16, R3_8,
};

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

struct Step
{
    StepType type = StepType::Normal;
    // Which sorted input note: 0..4 (k1..k5). Ignored for Chord/Random.
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
    std::string name = "Init";
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
    std::string name;
    // Chained program indices in order.
    std::vector<int> programs;
    int nextChain = -1;              // -1 = no auto-jump
};

struct Bank
{
    std::vector<Program> programs;
    std::vector<Chain> chains;
    int currentChain = -1;           // -1 = chain off
    int numChains = 4;               // 1..16 (range of the automatable "current chain" knob)
    double chainQuantizeBars = 1.0;  // in bars; 0.25 = one beat in 4/4
    bool restartChainOnSwitch = true;
};

Program makeInitProgram(const std::string& name = "Init");
Bank makeFactoryBank();

} // namespace mnArpee
