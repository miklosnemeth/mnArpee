# mnArpee

BlueArp-manual alapu pattern arpeggiator (VST3 + Standalone), JUCE 9 + C++23.
Eloszures nelkul: a lenyomott hangok pitch szerint rendezve kerulnek a motorba.

## Funkciok (MVP)

- Arp modok: Up / Down / UpDown / DownUp / Random / AsPlayed + oktav -2..+2
- 64 step / program, step-tipusok: Off / Normal / Rest / Tie / Chord / Random
- KEY SELECT k1..k5, oktav -3..+3, velocity, chance
- Rate 1/64..3/8 (bar-tort, BlueArp manual), Gate %, Swing %, host-sync + free-run
- 8 program + 2 chain (Verse/Chorus), kvantalt chain-valtas
- Latch, sustain-pedal, panic, CC/bend/aftertouch thru
- Matrix UI: cella-klikk valtja a stepet, also sav a kivalasztott stephez

## Build (Windows)

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Artefaktok:

| Target | Path |
|--------|------|
| Standalone | `build/mnArpee_artefacts/Release/Standalone/mnArpee.exe` |
| VST3 | `build/mnArpee_artefacts/Release/VST3/mnArpee.vst3` |

## Tesztek

```powershell
cmake -S . -B build-tests -DMNARPEE_TESTS_ONLY=ON -DMNARPEE_BUILD_TESTS=ON
cmake --build build-tests --config Release
build-tests/Release/mnArpeeTests.exe
```

## Licenc

AGPLv3 (mnKord-minta), JUCE AGPL-opcioval kompatibilis.
