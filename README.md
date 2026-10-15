# mnArpee

Pattern arpeggiator based on the BlueArp manual (VST3 + Standalone), JUCE 9 + C++23.
Held notes are sorted by pitch before they reach the engine.

## Features (MVP)

- No arp modes: the step matrix draws the whole pattern. Rows, bottom to top: Random, Root, Key 1 .. Key 5
  - Key 1..5 pick from the held notes, sorted by pitch, one key per pitch class: the same note in another octave counts as one (the lowest stays); more than five different notes: the lowest five are the keys and the rest is left out. A key that is not held (a pattern with more keys than notes) plays the chord root instead, same as the Root row. Random picks among the keys.
  - The held notes are shown next to "PATTERN": white = a key, dim = merged into a key of another octave, red = left out (with a "+N left out" note)
  - Root plays the root of the held chord (triads, sus, 7th chords, power chords, inversions too), else the lowest note
- up to 32 steps per pattern, step types: Off / Normal / Rest / Tie / Random (every step plays one note)
  - Rest lengthens the previous note by a step, Tie plays legato (new note starts before the old one ends; a note tied to itself keeps sounding)
- Per step: KEY SELECT (Root, Key 1..5), octave -3..+3, velocity, chance
- Per pattern: rate 1/64..3/8 (bar fractions, BlueArp manual), gate 1..125%, swing -50..+50%, octave -2..+2, steps 0..32
  - The header values (rate, octave, gate, swing, steps) always belong to the active preset
  - Steps 0 = MIDI thru, 1 = quantized thru
- Host sync: steps lock to the song position (bar grid) and the pattern position follows it; free-run when the transport is stopped (a new phrase starts the pattern right away)
- Preset library like mnKord: factory presets plus your own, with a preset browser (search), previous/next arrows, New, Save (a dot marks unsaved changes), and a Menu with Save as, Revert, Rename, Import, Export, Show in folder and Delete; a preset switches at the end of the running pattern
- The host's program list mirrors the preset list (128 slots, unused ones show "-")
- Latch (a new chord replaces the latched one), sustain pedal, panic / all-notes-off, CC/bend/aftertouch pass-through, output on the input channel
- Sample-accurate MIDI handling; the engine is allocation-free on the audio thread and edits reach it without locks
- Matrix UI: the lane captions (Type, Key, Octave, Velocity) choose what the matrix shows; clicking a cell sets the step, in the value lanes dragging paints across steps
- Value lanes under the matrix (as in BlueARP): drag a box up and down or use the mouse wheel

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

Engine tests only (no JUCE needed):

```powershell
cmake -S . -B build-tests -DMNARPEE_TESTS_ONLY=ON -DMNARPEE_BUILD_TESTS=ON
cmake --build build-tests --config Release
build-tests/Release/mnArpeeTests.exe
```

Engine and plugin tests (the plugin tests drive the real processor and editor without a host):

```powershell
cmake -S . -B build -DMNARPEE_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Linux the editor checks need an X display (`xvfb-run ctest ...`); without one they are skipped.

## Presets

Presets live in `Documents\MNAudio\mnArpee`:

| Folder | Contents |
|--------|----------|
| `Factory` | The built-in presets, written by the plugin and rewritten when they change. Read only: edit and **Save as** to keep a variant. |
| `User` | One `.xml` file per preset you saved, shared by every project and plugin instance. Drop a file in and it shows up the next time the browser opens. |

A DAW project stores the **active preset's current content**, not a reference, so it plays back the same on any machine; there, the preset comes back as an unsaved one if the library does not have it, and **Save** writes it into that machine's `User` folder. Leaving a preset with unsaved changes asks whether to save them. **Delete** moves the file to the recycle bin. The environment variable `MNARPEE_PRESET_DIR` replaces the folder (the tests use it).

Projects saved before the presets (a bank of 8 programs) load with the program that was selected, as an unsaved preset.

Preset file (a bank file with several `<Preset>` elements is accepted by **Import** too):

```xml
<mnArpeePreset version="1">
  <Preset name="My Arp" group="User" author="you" rate="1/16" gate="80" swing="0" octave="0" transpose="0" steps="16">
    <Step type="normal" key="1" scale="0" octave="0" velocity="96" gate="100" chance="100"/>
    <!-- one Step per stored step -->
  </Preset>
</mnArpeePreset>
```

## Installer (Windows)

The installer puts the VST3 into `%CommonProgramFiles%\VST3` (the Standalone is not included) and the
license and readme into `Program Files\MNAudio\mnArpee`. It is built with
[Inno Setup 6](https://jrsoftware.org/isinfo.php) from a Release build:

```powershell
cmake -S . -B build
cmake --build build --config Release --target mnArpee_VST3
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" /DMyAppVersion=1.0.0 installer\mnArpee.iss
```

The result is `build\installer\mnArpee-1.0.0-win-x64-setup.exe`. Use the version in `CMakeLists.txt` for `MyAppVersion`.
Presets are not part of the installer: the plugin writes its factory presets to
`Documents\MNAudio\mnArpee` the first time it runs, and the uninstaller leaves your own presets alone.

## License

AGPLv3 (following the mnKord pattern); JUCE's AGPL option is compatible.

The bundled fonts (Montserrat for the title, Roboto Condensed for all other text) are licensed
under the SIL Open Font License.
