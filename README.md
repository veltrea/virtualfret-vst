**Read this in other languages:** [日本語](README.ja.md)

# VirtualFret

A guitar-fretboard MIDI keyboard as a VST3 plugin. Click the fretboard to play single notes, latch chord shapes, and drag across the strum zone to play time-spread strokes — all sent as MIDI to the guitar instrument downstream. VirtualFret itself makes no sound.

Built for guitarists who think in fretboard positions, not piano keys: enter MIDI for guitar sample libraries and modeling instruments with the chord shapes you already know.

## Features

- **6 / 7 / 8 / 9 string** fretboards (open + 24 frets, tab orientation: top row = string 1)
- **Single-note play**: press a cell = note on, release = note off; drag along or across strings to swap notes (one note per string, like a real guitar)
- **Tunings**: 17 built-in presets (Standard, Drop D/C#/C/B, Eb, D Standard, Open G/D/E/A, DADGAD, 7/8/9-string sets), per-string semitone editing on the headstock, and user tunings saved to a shared file
- **Chord mode**: latch one position per string (fret / open ○ / muted ×), audition on latch, state saved with the project
- **Chord presets**: root × 13 types (maj, min, 7, maj7, m7, sus4, sus2, add9, 9, m9, dim, aug, 5) resolved from a shape library; cycle through forms (E-form / A-form / open) with one button; save your own voicings
- **Strum zone**: drag across the string lines to strum; crossing speed = velocity, crossing times are rendered sample-accurately even at large buffer sizes; let-ring behaviour; click without crossing = mute all
- **Keyboard strum**: Space = downstroke, Shift+Space = upstroke, Esc = all notes off
- **Channel modes**: single channel, or per-string channels for guitar instruments that understand them
- **Input MIDI highlight**: incoming notes light up every fretboard position that could play them
- **Compact view**: choose how many frets fill the window (12 / 15 / 18 / 21 / 24) in Settings, and shrink the window down to a slim Guitar-Pro-style fretboard strip
- **Left-handed mode**: mirror the whole board (headstock right, strum zone left) from Settings
- **UI languages**: English / 日本語 (follows the OS, switchable in Settings)

## Requirements

- macOS 12+ (Universal: Apple Silicon + Intel). Windows support is planned.
- A VST3 host (Ableton Live, REAPER, Studio One, ...)
- VST3 + Standalone only. There is deliberately no AU build: Ableton Live cannot route MIDI out of AU plugins.

## Install

1. Download `VirtualFret-<version>-macOS.zip` from GitHub Releases and unzip.
2. Copy `VirtualFret.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
3. The plugin is ad-hoc signed. If your DAW refuses to load it:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/VirtualFret.vst3
```

## DAW setup

### Ableton Live — read this first

VirtualFret *generates* MIDI, and Live only exposes generated MIDI through the plugin's **device tap**. Use two tracks:

```
Track A                          Track B
+--------------------+          +----------------------------+
| VirtualFret        |          | your guitar instrument     |
| (as an instrument) |          |                            |
+--------------------+          | MIDI From:                 |
                                |   [A-VirtualFret      v]   |  <- track A
                                |   [VirtualFret        v]   |  <- the DEVICE TAP,
                                |                            |     NOT "Post FX"
                                | Monitor: [In]              |
                                +----------------------------+
```

1. Insert VirtualFret on track A (it appears as an instrument).
2. On track B, set **MIDI From** to track A, and in the **second chooser pick "VirtualFret"** — the device tap. **"Post FX" will not work**: it only carries MIDI that passed through, not notes the plugin generated.
3. Set track B's **Monitor to In**.

If clicks make no sound but incoming MIDI passes through, the second chooser is on "Post FX" — switch it to the device tap.

### Studio One

Insert VirtualFret in the instrument's **Note FX** slot.

### REAPER

Insert VirtualFret in the FX chain before the instrument; MIDI flows through the chain.

## Using the fretboard

- **Play**: click a cell (string × fret); the open-string zone left of the nut plays open strings. Drag to slide between frets/strings.
- **Tune**: click the note names on the headstock (top half = up, bottom half = down a semitone) or drag vertically. The fretboard drawing never changes — only the pitches do. Save your tuning via the `...` button next to the tuning selector.
- **Chords**: enable Chord mode, click cells to latch (click again to unlatch; unlatched strings are muted ×). Or pick a root + type in the toolbar and cycle forms with the Form button. Save voicings via the `...` button next to it.
- **Strum**: drag across the string lines in the right-hand zone. Faster = louder. A click that crosses no string mutes everything (pick mute). Strummed notes ring until the same string sounds again.
- **Keys**: Esc = all notes off. Space / Shift+Space = down/up strum.

## User files

Shared across all instances and projects:

- `~/Library/Application Support/VirtualFret/tunings.json`
- `~/Library/Application Support/VirtualFret/chords.json`

Hand-written entries load fine; a tuning is just:

```json
{ "name": "My Drop C", "strings": [36, 43, 48, 53, 57, 62] }
```

(`strings` runs low to high, one open-string MIDI note per string.)

## Building from source

```bash
git clone https://github.com/veltrea/virtualfret-vst.git
cd virtualfret-vst
# Place JUCE 8 at external/JUCE (it is not committed):
#   git clone --depth 1 https://github.com/juce-framework/JUCE external/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VirtualFret_VST3 VirtualFret_Standalone CoreTests -j8
ctest --test-dir build            # core logic tests
bash scripts/install-plugins.sh   # refuses to run while Live is open
```

`scripts/package-release.sh` produces the ad-hoc-signed release zip plus dSYMs.

## Notes

- The plugin passes all incoming MIDI through unchanged and merges its own notes into the stream.
- Editor close, transport stop, tuning changes and string-count changes always release every sounding note — no stuck notes by design.
