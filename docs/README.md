# mnArpee documentation

**English** · [Magyar](USER_GUIDE.hu.md)

**mnArpee** is a pattern arpeggiator: hold a chord, and it plays a pattern of single notes that you draw step by
step. It makes no sound by itself, so it goes in front of a synth.

## Quick start

1. **Install it.** Run `mnArpee-<version>-win-x64-setup.exe` (Windows). The VST3 goes to
   `C:\Program Files\Common Files\VST3\MNAudio\`. Rescan the plugins in your DAW.
2. **Add it to a track.** Put mnArpee on a track, **before** the synth that should play the notes.
3. **Pick a preset.** Click the preset name in the header and choose one, for example *Accent Climb* or *Funk Bass*.
4. **Play.** Hold a chord. The pattern plays the keys of the chord (or only its root, in a Bassline preset).

To draw your own pattern: click cells in the key matrix, and set the TYPE, OCTAVE, VELOCITY, STEP GATE and RATCHET
lanes under it.

## The user guide

| | |
|---|---|
| **User guide (English)** | [Read it here](USER_GUIDE.md) · [PDF](mnArpee-User-Guide.pdf) |
| **Felhasználói útmutató (magyar)** | [Olvasd itt](USER_GUIDE.hu.md) · [PDF](mnArpee-Felhasznaloi-utmutato.pdf) |

What is in it:

- [Install](USER_GUIDE.md#install) and [use it in a DAW](USER_GUIDE.md#use-it-in-a-daw)
- [The window](USER_GUIDE.md#the-window) and [the keys](USER_GUIDE.md#the-keys)
- [The lanes](USER_GUIDE.md#the-lanes) and [the header values](USER_GUIDE.md#the-header-values)
- [Pattern tools](USER_GUIDE.md#pattern-tools)
- [Presets](USER_GUIDE.md#presets)
- [Tips](USER_GUIDE.md#tips) and [troubleshooting](USER_GUIDE.md#troubleshooting)

## More

- [Download](https://github.com/miklosnemeth/mnArpee/releases/latest) the latest release
- [Source code and build instructions](../README.md) (Windows)
- [Report a bug or ask for a feature](https://github.com/miklosnemeth/mnArpee/issues)
- [Home page](https://miklosnemeth.github.io/mnArpee/) (a web page with screenshots)

The home page and the PDFs are built from the project: `python site/build-site.py` and `python docs/build-pdf.py`.

mnArpee is free software under the [AGPL-3.0](../LICENSE). Vibecoded by Miklós Németh.
