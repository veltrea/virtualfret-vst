#pragma once

#include <juce_core/juce_core.h>
#include "FretModel.h"

namespace virtualfret
{

/** One fingering shape, defined against 6-string standard tuning.
    frets[] runs low string -> high string; -1 = muted. */
struct ChordShape
{
    juce::String name;            // "E-form", "Open C", ...
    int rootString = 6;           // 1-based, 6 = lowest string of the reference guitar
    juce::Array<int> frets;       // size 6
    bool movable = true;
};

struct ChordType
{
    juce::String id;              // "maj", "m7", ...
    juce::String label;
    juce::Array<ChordShape> shapes;
};

/** A shape resolved to a concrete position for the current string count. */
struct ResolvedForm
{
    juce::String name;            // shape name, e.g. "E-form"
    int baseFret = 0;             // transposition applied to the shape
    juce::Array<int> latch;       // per string (low -> high): -1 mute, 0 open, n fret
};

/**
    Built-in chord shape library (assets/chord-shapes.json) plus user
    chords (a named latch snapshot, stored in a shared per-user file).

    Shapes are defined on the 6-string standard reference. Applying one
    under a different tuning keeps the same finger positions — the
    sounding pitches change, exactly like a real guitar (SPEC §10). On
    7..9 strings the shape covers the highest six; extra low strings
    are muted.
*/
class ChordShapeLibrary
{
public:
    void loadBuiltins (const void* data, int size);

    const juce::Array<ChordType>& types() const noexcept { return typeList; }
    const ChordType* findType (const juce::String& id) const;

    /** All playable positions of one chord (root pitch class 0..11),
        expanded to numStrings, ordered by base fret. Empty if the type
        is unknown or nothing fits on the fretboard. */
    juce::Array<ResolvedForm> resolveForms (const juce::String& typeId,
                                            int rootPitchClass,
                                            int numStrings) const;

    // --- user chords (named latch snapshots, M5) --------------------------
    struct UserChord
    {
        juce::String name;
        juce::Array<int> latch;   // size = its string count
    };

    void loadUserChords (const juce::File& file);
    bool saveUserChord (const juce::File& file, const UserChord& chord);
    bool deleteUserChord (const juce::File& file, const juce::String& name);
    juce::Array<UserChord> userChordsFor (int numStrings) const;

    /** ~/Library/Application Support/VirtualFret/chords.json (per-OS). */
    static juce::File defaultUserChordFile();

private:
    juce::Array<ChordType> typeList;
    juce::Array<UserChord> userChords;

    static bool writeUserChordFile (const juce::File& file, const juce::Array<UserChord>& chords);
};

} // namespace virtualfret
