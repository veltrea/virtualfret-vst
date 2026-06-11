#pragma once

#include <juce_core/juce_core.h>

namespace virtualfret
{

inline constexpr int kMinStrings = 6;
inline constexpr int kMaxStrings = 9;
inline constexpr int kNumFrets   = 24;   // playable frets 1..24, fret 0 = open

/** "C", "C#", ... for a pitch class 0..11. */
juce::String pitchClassName (int pitchClass);

/** Scientific pitch label matching the tuning tables in SPEC §7 (E2 = MIDI 40). */
juce::String noteLabel (int note);

/**
    The whole editable state of one plugin instance. String arrays run
    low string -> high string (index 0 = lowest); the UI flips them for
    display (string 1 = highest = top row). Pure data — thread safety is
    the owner's concern.
*/
struct VirtualFretState
{
    int numStrings = 6;                      // 6..9
    juce::String tuningName = "Standard";
    juce::Array<int> openNotes;              // open-string MIDI notes, low -> high
    bool chordMode = false;
    juce::Array<int> latch;                  // per string: -1 = muted, 0 = open, 1..24 = fret
    int velocity = 100;                      // fixed velocity for clicks/audition, 1..127
    bool perStringChannels = false;          // false = Single channel mode
    int channel = 1;                         // Single mode channel, 1..16
    int perStringBaseChannel = 1;            // Per-string mode: UI string 1 (highest) gets this

    float strumSensitivity = 1.0f;           // velocity-from-speed scale, 0.25..4
    int strumVelMin = 40;
    int strumVelMax = 127;
    bool strumFixedVelocity = false;         // true: strums use `velocity` as-is
    int keyboardStrumMs = 15;                // per-string gap for Space strums
    bool strumHoldLight = false;             // true: picked strings stay lit until mouse-up

    bool showNoteNames = false;
    bool inputHighlight = true;
    bool latchAudition = true;
    int visibleFrets = kNumFrets;            // frets that fill the view (zoom); data is always 24
    juce::String language;                   // "en" / "ja"; empty = follow OS locale

    VirtualFretState();

    /** All strings unlatched (= muted in chord mode). */
    void resetLatch();

    /** MIDI note for a cell, or -1 if outside 0..127 / bad indices. */
    int noteForCell (int stringIndex, int fret) const noexcept;

    /** 1-based MIDI channel for a string under the current channel mode.
        Per-string mode counts from the highest string (UI string 1). */
    int channelForString (int stringIndex) const noexcept;

    /** Replace tuning (and string count, if the note list differs in size).
        Latch is resized; out-of-range entries are cleared to muted. */
    void applyTuning (const juce::String& name, const juce::Array<int>& notes);

    /** Serialize everything for getStateInformation. */
    juce::String toStateJson() const;

    /** Restore from setStateInformation. Unknown fields are ignored;
        a malformed document leaves the state untouched and returns false. */
    bool restoreFromStateJson (const juce::String& json);
};

} // namespace virtualfret
