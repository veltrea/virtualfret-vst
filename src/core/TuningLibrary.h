#pragma once

#include <juce_core/juce_core.h>
#include "FretModel.h"

namespace virtualfret
{

struct Tuning
{
    juce::String name;
    juce::Array<int> openNotes;   // low -> high
    bool isUser = false;

    int numStrings() const noexcept { return openNotes.size(); }
};

/**
    Built-in tuning presets (from assets/tunings.json, baked into the
    binary) plus the user's own tunings stored in a shared per-user file
    so every instance and project sees the same list.

    File access takes an explicit juce::File so tests can point it at a
    temp directory; the plugin passes defaultUserFile().
*/
class TuningLibrary
{
public:
    /** Parse the built-in preset JSON (BinaryData). */
    void loadBuiltins (const void* data, int size);

    /** Replace the user list from the shared file (missing file = empty). */
    void loadUserTunings (const juce::File& file);

    /** Add or replace (by name) a user tuning and rewrite the file. */
    bool saveUserTuning (const juce::File& file, const Tuning& tuning);

    /** Remove a user tuning by name and rewrite the file. */
    bool deleteUserTuning (const juce::File& file, const juce::String& name);

    /** Built-ins first, then user tunings, filtered to one string count. */
    juce::Array<Tuning> tuningsForStringCount (int numStrings) const;

    /** The default (first built-in, i.e. Standard) for a string count. */
    const Tuning* standardFor (int numStrings) const;

    /** ~/Library/Application Support/VirtualFret/tunings.json (per-OS). */
    static juce::File defaultUserFile();

private:
    juce::Array<Tuning> builtins, users;

    static bool parseTunings (const juce::var& parsed, juce::Array<Tuning>& out, bool markUser);
    static bool writeUserFile (const juce::File& file, const juce::Array<Tuning>& tunings);
};

} // namespace virtualfret
