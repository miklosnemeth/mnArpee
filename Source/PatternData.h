#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

//==============================================================================
// PatternData: BlueArp-manual alapu egyszerusitett adatmodell (eloszures nelkul).
//
// Manual-lefedettseg (v2.8.0):
//  - steps: 0..64 (0 = MIDI thru, 1 = quantized thru)
//  - sync: 1/64..3/8 törtjei (bar törtjeként)
//  - gate 1..125%, swing -50..+50%
//  - STEP TYPE: Off/Nrm/Rst/Tie/Chr/Rnd
//  - KEY SELECT: Fixed/Root/k1..k5
//  - SCALE STEP -12..+12, OCTAVE -3..+3
//  - VELOCITY 0..127, GATE TIME mult, CHANCE 1..100%
//  - chains: max 16, chain quantize, restart-on-switch
// Egyszerusites az MVP-ben (keres szerint):
//  - nincs input range filter / wrap, nincs missing-keys substitution,
//    nincs akkordfelismeres / skala-derivalas, nincs input quantize,
//    nincs fixed-key sequencer mod, nincs skala-kenyszerites.
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
    Off = 0,    // nem general hangot
    Normal,     // normal hang
    Rest,       // elozo step hangjat folytatja (hosszabbitas)
    Tie,        // atlapolas az elozovel (glide mono/legato szintin)
    Chord,      // osszes input hang egyszerre
    Random,     // veletlen input hang
};

enum class ArpMode : std::uint8_t
{
    Up = 0,
    Down,
    UpDown,
    DownUp,
    Random,
    AsPlayed,
};

enum class SyncRate : std::uint8_t
{
    R1_64 = 0, R1_48, R1_32, R1_24, R1_16, R1_12, R1_8, R1_6, R1_4,
    R3_64, R3_32, R3_16, R3_8,
};

constexpr double syncRateToBars(SyncRate r) noexcept
{
    // bar-tört: BlueArp manual "sync" tablazat (1/64 .. 3/8 bar).
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
    // Melyik rendezett input hang: 0..4 (k1..k5). Chord/Random eseten figyelmen kivul.
    std::int8_t keyIndex = 0;
    std::int8_t scaleStep = 0;   // -12..+12 felhang (MVP: kromatikus)
    std::int8_t octave = 0;      // -3..+3
    std::uint8_t velocity = 96;  // 0..127
    // Gate-szorzo szazalekban: 100 = nincs valtozas (manual GATE TIME "2x" stb. helyett).
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
    ArpMode mode = ArpMode::Up;
    int octaveShift = 0;             // -2..+2 global arp oktav
    int transposeOct = 0;            // -3..+3 program-szintu
    std::array<Step, maxSteps> steps {};
};

struct Chain
{
    std::string name;
    // Lancolt program-indexek sorrendben.
    std::vector<int> programs;
    int nextChain = -1;              // -1 = nincs auto-ugras
};

struct Bank
{
    std::vector<Program> programs;
    std::vector<Chain> chains;
    int currentChain = -1;           // -1 = lanc ki
    int numChains = 4;               // 1..16 (automatizálható "current chain" knob tartomanyahoz)
    double chainQuantizeBars = 1.0;  // default 1/4 ~ 1 beat 4/4-ben = 1.0 bar? lasd megjegyzest
    bool restartChainOnSwitch = true;
};

Program makeInitProgram(const std::string& name = "Init");
Bank makeFactoryBank();

} // namespace mnArpee
