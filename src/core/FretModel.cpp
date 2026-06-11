#include "FretModel.h"

namespace virtualfret
{

juce::String pitchClassName (int pitchClass)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                   "F#", "G", "G#", "A", "A#", "B" };
    return names[((pitchClass % 12) + 12) % 12];
}

juce::String noteLabel (int note)
{
    if (! juce::isPositiveAndBelow (note, 128))
        return {};
    return pitchClassName (note % 12) + juce::String (note / 12 - 1);
}

VirtualFretState::VirtualFretState()
{
    // 6-string standard; the processor swaps in the tuning library's
    // defaults on demand, but a fresh instance must be playable as-is.
    openNotes = { 40, 45, 50, 55, 59, 64 };
    resetLatch();
}

void VirtualFretState::resetLatch()
{
    latch.clearQuick();
    for (int i = 0; i < numStrings; ++i)
        latch.add (-1);
}

int VirtualFretState::noteForCell (int stringIndex, int fret) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, openNotes.size()))
        return -1;
    if (fret < 0 || fret > kNumFrets)
        return -1;

    const int note = openNotes[stringIndex] + fret;
    return juce::isPositiveAndBelow (note, 128) ? note : -1;
}

int VirtualFretState::channelForString (int stringIndex) const noexcept
{
    if (! perStringChannels)
        return juce::jlimit (1, 16, channel);

    // UI string numbers count from the highest-pitched string (top row),
    // so internal index numStrings-1 (highest) maps to the base channel.
    const int uiString = juce::jlimit (1, kMaxStrings, numStrings - stringIndex);
    const int base = juce::jlimit (1, 16, perStringBaseChannel);
    return ((base - 1 + (uiString - 1)) % 16) + 1;
}

void VirtualFretState::applyTuning (const juce::String& name, const juce::Array<int>& notes)
{
    if (notes.size() < kMinStrings || notes.size() > kMaxStrings)
        return;

    tuningName = name;
    openNotes = notes;

    if (numStrings != notes.size())
    {
        numStrings = notes.size();
        resetLatch();
    }
}

juce::String VirtualFretState::toStateJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("version", 1);
    obj->setProperty ("numStrings", numStrings);
    obj->setProperty ("tuningName", tuningName);

    juce::Array<juce::var> notes;
    for (auto n : openNotes)
        notes.add (n);
    obj->setProperty ("openNotes", notes);

    obj->setProperty ("chordMode", chordMode);

    juce::Array<juce::var> latchVar;
    for (auto f : latch)
        latchVar.add (f);
    obj->setProperty ("latch", latchVar);

    obj->setProperty ("velocity", velocity);
    obj->setProperty ("perStringChannels", perStringChannels);
    obj->setProperty ("channel", channel);
    obj->setProperty ("perStringBaseChannel", perStringBaseChannel);
    obj->setProperty ("strumSensitivity", strumSensitivity);
    obj->setProperty ("strumVelMin", strumVelMin);
    obj->setProperty ("strumVelMax", strumVelMax);
    obj->setProperty ("strumFixedVelocity", strumFixedVelocity);
    obj->setProperty ("keyboardStrumMs", keyboardStrumMs);
    obj->setProperty ("showNoteNames", showNoteNames);
    obj->setProperty ("inputHighlight", inputHighlight);
    obj->setProperty ("latchAudition", latchAudition);
    obj->setProperty ("language", language);

    return juce::JSON::toString (juce::var (obj));
}

bool VirtualFretState::restoreFromStateJson (const juce::String& json)
{
    const auto parsed = juce::JSON::parse (json);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return false;

    const auto getInt = [obj] (const char* key, int fallback)
    {
        const auto& v = obj->getProperty (key);
        return v.isInt() || v.isInt64() || v.isDouble() ? (int) v : fallback;
    };
    const auto getBool = [obj] (const char* key, bool fallback)
    {
        const auto& v = obj->getProperty (key);
        return v.isBool() || v.isInt() ? (bool) v : fallback;
    };

    const int n = getInt ("numStrings", -1);
    if (n < kMinStrings || n > kMaxStrings)
        return false;

    const auto* notesArr = obj->getProperty ("openNotes").getArray();
    if (notesArr == nullptr || notesArr->size() != n)
        return false;

    juce::Array<int> newNotes;
    for (const auto& v : *notesArr)
    {
        const int note = (int) v;
        if (! juce::isPositiveAndBelow (note, 128))
            return false;
        newNotes.add (note);
    }

    juce::Array<int> newLatch;
    if (const auto* latchArr = obj->getProperty ("latch").getArray();
        latchArr != nullptr && latchArr->size() == n)
    {
        for (const auto& v : *latchArr)
            newLatch.add (juce::jlimit (-1, kNumFrets, (int) v));
    }
    else
    {
        for (int i = 0; i < n; ++i)
            newLatch.add (-1);
    }

    numStrings = n;
    openNotes = std::move (newNotes);
    latch = std::move (newLatch);
    tuningName = obj->getProperty ("tuningName").toString();
    chordMode = getBool ("chordMode", false);
    velocity = juce::jlimit (1, 127, getInt ("velocity", 100));
    perStringChannels = getBool ("perStringChannels", false);
    channel = juce::jlimit (1, 16, getInt ("channel", 1));
    perStringBaseChannel = juce::jlimit (1, 16, getInt ("perStringBaseChannel", 1));

    const auto& sens = obj->getProperty ("strumSensitivity");
    strumSensitivity = juce::jlimit (0.25f, 4.0f,
                                     sens.isDouble() || sens.isInt() ? (float) (double) sens : 1.0f);
    strumVelMin = juce::jlimit (1, 127, getInt ("strumVelMin", 40));
    strumVelMax = juce::jlimit (strumVelMin, 127, getInt ("strumVelMax", 127));
    strumFixedVelocity = getBool ("strumFixedVelocity", false);
    keyboardStrumMs = juce::jlimit (1, 200, getInt ("keyboardStrumMs", 15));

    showNoteNames = getBool ("showNoteNames", false);
    inputHighlight = getBool ("inputHighlight", true);
    latchAudition = getBool ("latchAudition", true);
    language = obj->getProperty ("language").toString();

    return true;
}

} // namespace virtualfret
