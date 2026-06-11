**Read this in other languages:** [日本語](SPEC.ja.md)

# VirtualFret — Specification

An on-screen MIDI keyboard shaped like a guitar fingerboard. Click the fretboard to play single notes, latch positions to build chords, drag across a strum zone to reproduce pick strokes — all emitted as MIDI notes to a downstream instrument. The plugin itself makes no sound.

## 1. Background

On-screen MIDI keyboards are almost always piano-shaped, but what a guitarist has memorized is the fingerboard: chord shapes, positions, tunings. When programming MIDI for guitar instruments (sampled or modeled), a piano UI gives no intuitive way to build guitar voicings.

VirtualFret puts the fingerboard itself on screen so that clicks and drags become MIDI directly. A guitarist who knows chord shapes by hand can enter them into the DAW from that same muscle memory.

The technical foundation reuses the MIDI-output VST3 patterns established in NoteNamer (`/Volumes/DISK/dev/notenamer-vst`). Read `/Volumes/DISK/dev/knowledge/vst3_midi_out_plugin.md` before designing anything.

## 2. Goals

- On-screen MIDI keyboard reproducing a guitar fingerboard with **6 / 7 / 8 / 9 strings**
- **Click to play**: pressing a cell (string × fret) sends that pitch as a MIDI note
- **Independent tuning changes**: change each string's open pitch in semitone steps without altering the visual fretboard design. Built-in presets for down-tunings, half-step-down, open tunings, etc.; users can add and save their own tunings
- **Chord mode (latch)**: when enabled, clicked positions stay lit, and clicking across different strings builds up a chord one note at a time
- **Strum zone**: a drag gesture like running a pick across the strings reproduces a stroke with realistic per-string timing
- **Built-in chord presets**: a shape library of common barre chords, seventh chords, ninth chords and more; picking a root + type auto-places the latch positions
- Settings and latch state persist with the DAW project (plugin state)

## 3. Non-Goals

- **Sound generation** — the plugin is silent; audio comes from the downstream instrument
- **Bends / slides / vibrato / MPE** — articulation is a future candidate (v1 sends note on/off only)
- **Arpeggiator / strum-pattern sequencing** — future candidate; v1 strums are manual gestures only
- **Tablature import or playback**
- **Left-handed / flipped display** — post-v1
- **Dedicated capo UI** — covered by tuning (uniform offset across strings); future candidate
- Audio processing; transforming CC / pitch bend / anything else

## 4. Platform & Distribution

| Item | Details |
|---|---|
| Formats | **VST3 only + Standalone** (macOS first, Windows later). No AU build: Ableton Live cannot route MIDI out of AU plugins, so an AU would only be a dead browser entry |
| OS | macOS 12+ (Universal: Apple Silicon + Intel). Windows 10+ later |
| Framework | JUCE 8 / C++20 / CMake |
| GUI | Native JUCE components (no WebView) |
| Signing & distribution | Ad-hoc signing on macOS. Distributed via GitHub Releases |
| Identifiers | `BUNDLE_ID com.veltrea.virtualfret` / `PLUGIN_MANUFACTURER_CODE Vltr` / `PLUGIN_CODE Vfrt` |

Apply the full checklist from `vst3_midi_out_plugin.md`:
`VST3_CATEGORIES Instrument` (Live refuses Fx without audio inputs), a permanent silent stereo output bus, `getTailLengthSeconds() = infinity` (defeats Live's plug-in auto-suspend), `COPY_PLUGIN_AFTER_BUILD FALSE` plus an install script, no `MessageManager::callAsync` (use an `AsyncUpdater` member), and Release builds with `-g`, LTO off, dSYM archived.

## 5. MIDI Behavior

- **Thru**: all incoming MIDI passes through unmodified (notes, CC, everything)
- **Generated notes are merged** into the output stream from GUI gestures (clicks, latch audition, strums)
- **Threading**: NoteNamer pattern — GUI → lock-free FIFO (`AbstractFifo` + POD commands) → `processBlock`. The sounding ledger `sounding[16][128]` is audio-thread-only; a panic flag releases everything
- **Note-off guarantees** (no stuck notes):
  - Editor close and transport stop release all sounding notes (panic path)
  - **Monophonic per string**: starting a new note on a string always releases that string's previous note first (this also mirrors the physical constraint of a real guitar)
  - Changing tuning or string count releases all sounding notes before applying (offs are sent with the note numbers they were started with — the ledger stores note numbers, so this is naturally correct)
- **Velocity**: single clicks / latch audition = fixed value (GUI setting 1–127, default 100). Strums = derived from drag speed (§9)
- **Channel modes** (GUI setting):
  - **Single** (default): every note on one channel (default 1, configurable 1–16)
  - **Per-string**: one channel per string (string 1 = ch1, string 2 = ch2, …) for guitar instruments and MIDI-guitar conventions that interpret per-string channels. A base-channel offset is configurable
- **Strum timing precision**: stroke notes are expressed with **sample offsets** inside the block. Even with large audio buffers (e.g. 1024 samples) the per-string spacing must not collapse onto one timestamp (keep the GUI event time, convert to an offset in processBlock, carry overflow into the next block)

## 6. Fretboard (Display & Single-Note Playing)

- **Orientation**: headstock on the left, body (strum zone) on the right. Horizontal = frets, vertical = strings
- **String order**: top = string 1 (highest), bottom = lowest string (same as tablature)
- **Frets**: open (an open-string zone left of the nut) + frets 1–24
- **Position markers**: 3 / 5 / 7 / 9 / 12 / 15 / 17 / 19 / 21 / 24 (double dots at 12 and 24). Fret spacing, string thickness and other visuals are detailed in DESIGN.md (created with Stitch)
- **String count switch**: 6 / 7 / 8 / 9 in the toolbar. Switching loads that count's default (Standard) tuning, clears latches, and releases sounding notes
- **Single-note playing**:
  - Mouse-down on a cell = note on; mouse-up = note off
  - Dragging along the same string while held = fret substitution (old off → new on; stands in for hammer-ons/pull-offs)
  - Dragging vertically onto another string while held = old string off → new string on (simple substitution)
  - Clicking the open-string zone plays the open string
- **Sounding indication**: light up every sounding cell (from single notes, latch audition, and strums alike)
- **Incoming-MIDI highlight**: dimly light **every candidate position** that can produce a received note's pitch (a learning aid; can be disabled in settings)
- **Note-name overlay**: optional display of pitch names (C, C#, …) on cells

## 7. Tuning

### Data model

A tuning is **a list of open-string MIDI note numbers, lowest string → highest**. A cell's output note = open-string note + fret number.

**Invariant-appearance principle**: changing the tuning never changes the fretboard drawing (fret positions, markers, strings). Only the note numbers the cells emit change.

### Per-string editing

The headstock area (left edge) shows each string's open pitch name; click / ▲▼ / drag moves it in **semitone steps**. This makes down-tunings, half-step-down, and irregular tunings freely buildable per string.

### Built-in presets (assets/tunings.json)

| Strings | Preset | Open strings (low → high) | MIDI notes |
|---|---|---|---|
| 6 | Standard | E2 A2 D3 G3 B3 E4 | 40 45 50 55 59 64 |
| 6 | Half-step down (Eb) | Eb2 Ab2 Db3 Gb3 Bb3 Eb4 | 39 44 49 54 58 63 |
| 6 | Whole-step down (D Standard) | D2 G2 C3 F3 A3 D4 | 38 43 48 53 57 62 |
| 6 | Drop D | D2 A2 D3 G3 B3 E4 | 38 45 50 55 59 64 |
| 6 | Drop C# | C#2 G#2 C#3 F#3 A#3 D#4 | 37 44 49 54 58 63 |
| 6 | Drop C | C2 G2 C3 F3 A3 D4 | 36 43 48 53 57 62 |
| 6 | Drop B | B1 F#2 B2 E3 G#3 C#4 | 35 42 47 52 56 61 |
| 6 | Open G | D2 G2 D3 G3 B3 D4 | 38 43 50 55 59 62 |
| 6 | Open D | D2 A2 D3 F#3 A3 D4 | 38 45 50 54 57 62 |
| 6 | Open E | E2 B2 E3 G#3 B3 E4 | 40 47 52 56 59 64 |
| 6 | Open A | E2 A2 E3 A3 C#4 E4 | 40 45 52 57 61 64 |
| 6 | DADGAD | D2 A2 D3 G3 A3 D4 | 38 45 50 55 57 62 |
| 7 | Standard (B) | B1 E2 A2 D3 G3 B3 E4 | 35 40 45 50 55 59 64 |
| 7 | Drop A | A1 E2 A2 D3 G3 B3 E4 | 33 40 45 50 55 59 64 |
| 8 | Standard (F#) | F#1 B1 E2 A2 D3 G3 B3 E4 | 30 35 40 45 50 55 59 64 |
| 8 | Drop E | E1 B1 E2 A2 D3 G3 B3 E4 | 28 35 40 45 50 55 59 64 |
| 9 | Standard (C#) | C#1 F#1 B1 E2 A2 D3 G3 B3 E4 | 25 30 35 40 45 50 55 59 64 |

### User tunings

- The current tuning can be saved under a name, and saved tunings can be deleted
- Stored in a user file (`VirtualFret/tunings.json` under JUCE's userApplicationDataDirectory), **shared across all instances and projects** (separate from plugin state)
- Same JSON format as the built-in presets; hand-written entries in the file load fine:

```json
{ "name": "My Drop C", "strings": [36, 43, 48, 53, 57, 62] }
```

(`strings` lists open-string MIDI note numbers, lowest → highest. Array length = string count.)

## 8. Chord Mode (Latch)

Toggled in the toolbar (default OFF).

- **Latching**: while ON, clicking a cell lights it persistently
  - **One position per string**: clicking another cell on the same string replaces that string's latch
  - Re-clicking the same cell clears that string's latch (back to muted)
  - Clicking the open-string zone latches "open (○)"
  - **A string with no latch = muted (×)**: skipped during strums. The three states match a chord diagram's ×/○/fretted notation
- **Latch audition**: latching or replacing a position immediately plays that note briefly (default ON, can be disabled)
- **Clear**: a toolbar button clears all latches
- Turning chord mode OFF **keeps** the latch state (restored on re-enable — an accidental toggle never destroys a built chord). Sounding notes are unaffected
- Latch state persists in the plugin state

## 9. Strum Zone

A permanent vertical strip to the right of the fretboard — the metaphor is the guitar's body/pickup area, with each string's extension line running through it.

- **Sounding model**: mouse-down inside the zone = pick poised (silent). While dragging, **each time the pointer crosses a string line**, that string sounds
  - Chord mode ON: the **latched position's pitch** (muted strings are skipped; open ○ plays the open string)
  - Chord mode OFF: the open strings (current tuning as-is)
- **Stroke direction**: strings sound in the order crossed. Downward drag = downstroke (low strings first); upward = upstroke. Reversing mid-drag also follows crossing order
- **Timing**: the actual crossing time is kept and mapped to sample offsets (§5), so drag speed directly becomes stroke speed
- **Velocity**: derived from crossing speed (px/ms). Settings: sensitivity, min / max, fixed-value mode
- **Let ring**: strummed notes keep sounding after mouse-up. A string's previous note is released when that string sounds again
- **Mute all**:
  - Click & release inside the strum zone without crossing any string = mute all (resting the pick on the strings)
  - Toolbar mute button; Esc key
  - Editor close / transport stop (panic path, §5)
- **Keyboard strumming** (M5): Space = downstroke, Shift+Space = upstroke, with a configurable inter-string interval (ms)

## 10. Chord Presets

Choosing a **root (12 of C…B) + chord type** in the toolbar auto-places latches from the shape library (chord mode switches ON automatically). The result can then be hand-edited with the §8 gestures.

- **Types (built into v1)**: maj / min / 7 / maj7 / m7 / sus4 / sus2 / add9 / 9 / m9 / dim / aug / 5 (power chord)
- **Voicing/form cycling**: each chord can have multiple forms (E-form barre / A-form barre / open, etc.); a "next form" button cycles through them. Each form lands at a different base fret

### Shape data model (assets/chord-shapes.json)

A shape = root string + per-string relative frets (`-1` = muted). The base fret is computed from the requested root note.

```json
{ "name": "E-form maj", "rootString": 6, "frets": [0, 2, 2, 1, 0, 0], "movable": true }
```

Example: F major = E-form moved to root F (string 6, fret 1) → actual frets `[1, 3, 3, 2, 1, 1]` (a barre chord).

- Open (non-movable) chords use `movable: false` with a fixed root in the same format (e.g. open C maj = x32010 → `frets: [-1, 3, 2, 0, 1, 0]`)
- Shapes are defined **against 6-string standard tuning**. Under any other tuning the same finger shape is applied verbatim (the resulting pitches change — exactly like a real guitar). Shapes designed for open tunings are a future extension
- On 7–9 strings, 6-string shapes apply to strings 1–6 and the extra low strings are muted (extended-range shapes are future work)
- **User chords**: the current latch state can be saved under a name (user file `VirtualFret/chords.json`, same mechanism as tunings) — M5

## 11. UI

- **Layout**: toolbar on top / headstock area (tuning display & editing) on the left / fretboard in the center / strum zone on the right edge
- **Toolbar**: string count, tuning preset selector, chord-mode toggle, chord presets (root · type · form), clear, mute, velocity, channel mode, settings (note names / input highlight / latch audition / language)
- Resizable window, landscape-oriented (mind the minimum height with 9 strings)
- **Design**: follows DESIGN.md (a design system created with Google Stitch), expected to inherit the conventions of NoteNamer's "Ableton Live Native" system (dark grays + amber LED accents)
- Keyboard: Esc = mute all. Space / Shift+Space = strum (M5). `EDITOR_WANTS_KEYBOARD_FOCUS TRUE`

## 12. State & Persistence

- **Plugin state** (saved with the DAW project): string count, current tuning (name + note list), chord mode on/off, latch state, velocity setting, channel mode, strum settings, display options, language. JSON with a version field (same convention as NoteNamer)
- **User files** (shared across instances): `tunings.json` / `chords.json` (user-defined entries only; built-in presets live inside the binary)

## 13. Host Setup

| Host | How to insert |
|---|---|
| Ableton Live | Two-track setup: the plugin on track A (as an instrument); on instrument track B set **MIDI From to "A-VirtualFret"** and in the **second chooser pick the "VirtualFret" device tap** ("Post FX" never carries the generated notes). Monitor = **In** |
| Studio One | Insert in a Note FX slot |
| REAPER | Insert directly in the FX chain |

The README must include an illustrated walkthrough of the Live setup (this is where every user gets stuck).

## 14. Localization

- UI strings are split into EN / JA resources from the start (no hardcoding). Default language follows the OS locale; switchable in the GUI
- Commit messages in English. Root documents are maintained as hand-written `.md` (EN) + `.ja.md` (JA) pairs

## 15. Testing Strategy

- **Core logic isolation**: the tuning model, chord-shape resolution, strum state machine, and state serialization depend only on `juce_core` + `juce_data_structures`, exercised by console tests (ctest)
- Priority test areas:
  - Shape → fret placement resolution (root movement, muted strings, form selection when a shape would fall off the fretboard)
  - Releasing sounding notes on tuning changes (offs must use the note numbers the notes were *started* with)
  - Monophonic-per-string substitution
  - Strum ordering and sample-offset computation
  - State JSON round-trip
- GUI verified in the Standalone build (stress: rapid clicking, back-and-forth drags, closing mid-gesture) → always finish with a real Ableton Live pass
- Installation via the `scripts/install-plugins.sh` pattern (`pgrep -x Live` guard + deep ad-hoc re-sign after copying)

## 16. Milestones

- **M1**: fretboard display + single-note clicking (6-string Standard fixed). Confirm a downstream guitar instrument sounds in Live
- **M2**: tuning (all built-in presets + per-string editing + user save) + 6–9 string switching
- **M3**: chord mode (latch, ○/×/fretted three states, audition, clear, state persistence)
- **M4**: strum zone (crossing-based sounding, sample offsets, speed-derived velocity, let ring, mute all)
- **M5**: chord presets (shape library, form cycling, user chord save) + keyboard strumming
- **M6**: polish (incoming-MIDI highlight, note-name overlay, full i18n, README pair, package-release, distribution)

## 17. Open Questions (decide during implementation)

- Vertical drags on the fretboard itself: v1 treats them as single-note substitution (§6), but turning them into strums is an option. Revisit at M4 based on feel
- Default per-string channel mapping (string 1 = ch1 assumed). Confirm against real per-string-capable guitar instruments
- Strum zone width and crossing hysteresis: decide with DESIGN.md and a prototype
- Vertical resolution and minimum window size with 9 strings
