# mnArpee

Pattern arpeggiator based on the BlueArp manual (VST3 + Standalone), JUCE 9 + C++23.
No input pre-filtering: held notes are sorted by pitch before they reach the engine.

## Features (MVP)

- No arp modes: the step matrix draws the whole pattern (KEY SELECT k1..k5 picks from the pitch-sorted input; fewer held notes wrap around)
- 64 steps / program, step types: Off / Normal / Rest / Tie / Chord / Random
  - Rest lengthens the previous note by a step, Tie plays legato (new note starts before the old one ends; a note tied to itself keeps sounding)
- Per step: KEY SELECT k1..k5, octave -3..+3, velocity, chance
- Per program: rate 1/64..3/8 (bar fractions, BlueArp manual), gate 1..125%, swing -50..+50%, octave -2..+2, steps 0..64
  - The header controls always show the selected program; switching programs stores and reloads them
  - Steps 0 = MIDI thru, 1 = quantized thru
- Host sync: steps lock to the song position (bar grid) and the pattern position follows it; free-run when the transport is stopped (a new phrase starts the pattern right away)
- 8 programs + 2 chains (Verse/Chorus), quantized chain switching; turning a chain off returns to the selected program
- Latch (a new chord replaces the latched one), sustain pedal, panic / all-notes-off, CC/bend/aftertouch pass-through, output on the input channel
- Sample-accurate MIDI handling; the engine is allocation-free on the audio thread and edits reach it without locks
- Matrix UI: clicking a cell sets the step; the bottom strip edits the selected step

## Build (Windows)

```powershell
cmake -S . -B build
cmake --build build --config Release
```

JUCE is taken from `d:/dev/JUCE` if it exists, otherwise downloaded (9.0.1). Use another
local checkout with `-DMNARPEE_JUCE_DIR=<path>`.

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
