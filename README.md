# mnArpee

Pattern arpeggiator based on the BlueArp manual (VST3 + Standalone), JUCE 9 + C++23.
No input pre-filtering: held notes are sorted by pitch before they reach the engine.

## Features (MVP)

- No arp modes: the step matrix draws the whole pattern (KEY SELECT k1..k5 picks from the pitch-sorted input), octave -2..+2
- 64 steps / program, step types: Off / Normal / Rest / Tie / Chord / Random
- KEY SELECT k1..k5, octave -3..+3, velocity, chance
- Rate 1/64..3/8 (bar fractions, BlueArp manual), Gate %, Swing %, host sync + free-run
- 8 programs + 2 chains (Verse/Chorus), quantized chain switching
- Latch, sustain pedal, panic, CC/bend/aftertouch pass-through
- Matrix UI: clicking a cell sets the step; the bottom strip edits the selected step

## Build (Windows)

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Artifacts:

| Target | Path |
|--------|------|
| Standalone | `build/mnArpee_artefacts/Release/Standalone/mnArpee.exe` |
| VST3 | `build/mnArpee_artefacts/Release/VST3/mnArpee.vst3` |

## Tests

```powershell
cmake -S . -B build-tests -DMNARPEE_TESTS_ONLY=ON -DMNARPEE_BUILD_TESTS=ON
cmake --build build-tests --config Release
build-tests/Release/mnArpeeTests.exe
```

## License

AGPLv3 (following the mnKord pattern); JUCE's AGPL option is compatible.
