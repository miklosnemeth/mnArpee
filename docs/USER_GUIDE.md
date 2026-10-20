# mnArpee user guide

**English** · [Magyar](USER_GUIDE.hu.md) · [Documentation home](README.md)

mnArpee is a pattern arpeggiator. You hold a chord, and mnArpee plays it as a pattern of single notes. It
does not make sound by itself: put it in front of a synth, and the synth plays the notes.

```text
MIDI keyboard  ->  mnArpee  ->  synth / instrument
 (hold a chord)    (pattern)    (plays the notes)
```

There are no arp modes such as Up or Down. You draw the whole pattern yourself, step by step, or pick one of the
factory presets and change it.

## Install

**Windows:** run `mnArpee-<version>-win-x64-setup.exe`. It puts the plugin (VST3) into
`C:\Program Files\Common Files\VST3\MNAudio\` (a folder of its own inside the standard VST3 folder, which DAWs scan).
Rescan the plugins in your DAW if it does not show up. The Standalone program is not part of the installer.

The license, the readme and this user guide (PDF, with Start menu shortcuts) are installed into `C:\Program Files\MNAudio\mnArpee`. Installing a newer version over an old
one works: Setup shows the version it found ("Upgrade: v1.0.0 -> v1.1.0"), asks before going back to an older one, and
asks you to close your DAW if it has mnArpee loaded.

To remove it, use *Settings → Apps → mnArpee → Uninstall*. Your own presets (see below) are kept.

## Use it in a DAW

1. Create a **track** and add **mnArpee** to it.
2. Put the synth that should play the notes **after** mnArpee on the same track (or route mnArpee's MIDI output
   to the synth's track). How this is done differs between DAWs: look for "MIDI effect" or "MIDI output routing"
   in your DAW's manual.
3. Play a chord, or hold one. mnArpee plays the pattern of the selected preset. Everything that is not a note
   (pitch bend, modulation, sustain pedal, ...) passes through, and the notes leave on the channel they came in on.

When the DAW plays, the steps lock to the song position, so the pattern always lines up with the bar grid. When the
transport is stopped, mnArpee runs on its own tempo clock, and a new chord starts the pattern at once.

## The window

![The mnArpee window](images/main.png)

From the top:

- **Header.** The logo with the version, the preset picker (the arrows, the preset name, **New**, **Save**, **Menu**), the
  undo and redo buttons.
- **Values.** RATE, OCT, GATE, SWING and STEPS of the pattern, and at the right end the chips **Latch**, **Group** and **Cut**. Drag a value up or down, or use the mouse wheel.
  Sideways movement does nothing, so the value never jumps when you move the mouse.
- **PATTERN.** The notes you hold, and the chord they make (Cmaj7, Am, C/E, G5, ...).
- **The key matrix.** Each column is a step; the rows say which note the step plays.
- **The lanes.** Under the matrix: TYPE, OCTAVE, VELOCITY, STEP GATE, RATCHET and CHANCE, with one box per step.

The plugin has **no keyboard shortcuts**. It never takes a key away from your DAW.

## The keys

mnArpee does not know your notes in advance, so a step does not say "C". It says "the first key", "the second key" and
so on, and the keys are the notes you hold:

- The held notes are sorted by pitch. **Key 1** is the lowest note, **Key 2** the next one, up to **Key 5**.
- Every held note is a key, the same note in another octave too (C3 and C4 are two keys). Turn **Group** on to make the same note in several octaves one key (the lowest one), so a wide chord does not use up the five keys.
- More than five notes: the lowest five are the keys, and the rest is left out. The PATTERN strip shows the
  held notes: **white** is a key, **dim** is grouped into the key of its pitch class (Group on), **red** is left out ("+N left out").
- **Root** plays the root of the held chord. It understands triads, sus, seventh chords, power chords and inversions.
  If it finds no chord, the lowest note is the root.
- **Random** picks one of the keys at random.
- A key that is not held (the pattern has Key 4, but you hold three notes) plays the **root** instead. So a pattern keeps
  sounding right on any chord.

Click a cell in the matrix to put the step on that row. Click the same cell again to turn the step off. The Random row works the same way: click to make the step random, click again to turn it off.

## The lanes

| Lane | What it does |
|---|---|
| **TYPE** | `Nrm` normal note, `Off` the step is silent, `Rst` rest, `Tie` legato, `Rnd` a random key. |
| **OCTAVE** | -3 to +3 octaves for the step. |
| **VELOCITY** | 1 to 127. |
| **STEP GATE** | The length of the step's note: 10 to 400 % of the pattern's GATE, in steps of 5 %. 100 = unchanged. |
| **RATCHET** | 1 to 4. The step plays its note that many times, evenly spread over the step. |
| **CHANCE** | 5 to 100 %, in steps of 5 %. The chance that the step plays at all: 100 always plays, 50 plays about every second time. |

Drag a box up or down, or use the mouse wheel. The **dice** in front of a lane fills the whole lane with random values.

**Rest** makes the note before it longer by a step. **Tie** plays legato: the new note starts before the old one ends,
and a note that is tied to itself keeps sounding without a new attack.

### Two gates

The **GATE** in the header is the note length of the whole pattern, as a percentage of a step (1 to 125 %). **STEP GATE**
scales it for single steps. The length of a note is GATE x STEP GATE. Over 100 % the notes overlap, which is
good for legato lines.

### Random ranges

Right-click on a dice: a panel opens where you set the smallest and the largest value (**MIN** and **MAX**) the dice may
use. **Randomize** fills the lane at once, **Reset range** brings the default range back, and **Reset lane** puts the
lane back to its defaults. The ranges are saved with the project.

![The random options of the keys](images/random-range.png)

### The bass line dice

The dice at the **Root** row makes a random **bass line**: only Root steps, with accents, ghost notes, rests, glides,
octave jumps and ratchets. The dice in the corner above the row names fills the keys randomly.

## The header values

| Value | Range | |
|---|---|---|
| **RATE** | 1/64 to 3/8 | The step length, as a fraction of the bar. |
| **OCT** | -2 to +2 | Moves the whole pattern by octaves. |
| **GATE** | 1 to 125 % | The note length of the pattern. |
| **SWING** | -50 to +50 % | Delays (or hurries) every second step. |
| **STEPS** | 0 to 32 | The length of the pattern. `0` is MIDI thru, `1` is quantized thru. |

With less than 16 steps the columns past the end are dimmed and cannot be used.

These values always belong to the active preset. Change one and the preset shows the unsaved dot.

Three chips sit at the right end of the values card (hover over one for a hint; all three are host parameters):

- **Latch** keeps the arpeggio going after you let the keys go. A new chord replaces the latched one.
- **Group** makes the same note in several octaves one key (the lowest).
- **Cut** stops the sounding notes at once when you release the last key. Without it, the notes sound until the end of their gate
  or the next step.

The **sustain pedal** works as well, and **panic** (all notes off) from the host stops everything at once.

## Pattern tools

Right-click in the matrix or on a value box:

- **Set all steps to** the value you clicked
- **Randomize** the lane
- **Generate keys**: Up, Down, Up-Down, Down-Up, Converge or Random, over 3, 4 or 5 keys
- **Random bass line (root only)**
- **Shift pattern left / right**, **Reverse pattern**, **Clear pattern**
- **Copy pattern** and **Paste pattern**. The pattern goes to the system clipboard as the preset XML, so you can paste it
  into another plugin window or send it to somebody.

**Undo** and **redo** (the two round arrows in the header) take back every step edit.

## Presets

![The preset browser](images/preset-browser.png)

Click the preset name to open the **browser**: it lists every preset with its group and style, with a search field, a
group filter (All groups, Favorites, Bassline, Melodic, User) and a style filter. Click the **star** to make a preset a
favorite; the stars are shared by every project. The arrows next to the name step through the presets. Right-click a
preset in the list for **Load**, **Duplicate**, **Rename**, **Show in folder** and **Delete**.

- **Bassline** presets play only the root of the held chord. **Melodic** presets play the held keys.
- Every preset has style tags: Trance, House, Techno, Disco, Funk, Pop, Synthwave, Ambient, Chill, Dubstep.
- A preset changes **at the end of the running pattern**, so a switch never breaks the rhythm.
- The factory presets are read only. Edit one and use **Save as** to keep your version.

The header buttons:

- **New** starts a new pattern in which every step plays Key 1.
- **Save** writes the active preset. A dot on the preset name means unsaved changes. Leaving a preset with
  unsaved changes asks whether to save them.
- **Menu**: *Save as* (asks for the name and the styles), *Revert*, *Rename*, *Style*, *Import*, *Export*,
  *Show in folder*, *Zoom* (80 to 200 %, kept with the project) and *Delete*.

### Where the presets are

Presets live in `Documents\MNAudio\mnArpee`:

| Folder | Contents |
|--------|----------|
| `Factory` | The built-in presets. The plugin writes them and rewrites them when they change. |
| `User` | One `.xml` file per preset you saved, shared by every project. Drop a file in and it shows up the next time the browser opens. |

**Import** accepts a single preset or a bank file with many presets. **Delete** moves the file to the recycle bin.

A DAW project stores the **content of the active preset**, not a reference, so it plays back the same on any computer.
There the preset comes back as an unsaved one if the library does not have it, and **Save** writes it into the `User`
folder of that computer.

### Host programs

The host's program list mirrors the preset list (128 slots), so program changes and automation pick the same
presets. Unused slots show "-".

## Tips

- Make a bass line from a chord progression: pick a **Bassline** preset, hold the chord, and the root of every chord
  follows.
- Start from a factory preset and change two or three steps instead of drawing from nothing.
- Put a random range on VELOCITY (for example 80 to 127) and press the dice: a quick way to a human feel.
- A pattern with five keys sounds good on a 3-note chord too, because the missing keys play the root.
- Put a CHANCE of 50 to 75 % on a few steps (and press the dice of the lane) so that the pattern is a little different every round.
- Use RATCHET 2 or 3 on single steps for rolls, and a STEP GATE of 200 % or more for notes that hold over the next step.

## Troubleshooting

- **The plugin does not show up.** Rescan the plugins in your DAW. The VST3 is in
  `C:\Program Files\Common Files\VST3\MNAudio\`.
- **No sound.** mnArpee makes none. Check that a synth is after it on the same track, and that the synth listens
  to the MIDI channel you play on.
- **Notes stay silent on some steps.** The step may be `Off` or `Rst` (or it is a `Tie` to the same note, which keeps sounding without a new attack).
- **The chord does not show.** Hold the notes: the PATTERN strip shows only held notes.

## Build from source

See the [README](../README.md#build-windows) for the build, the tests and the installer. mnArpee is free software
under the AGPL-3.0, built with [JUCE](https://juce.com).
