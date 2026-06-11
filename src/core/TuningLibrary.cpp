#include "TuningLibrary.h"

namespace virtualfret
{

bool TuningLibrary::parseTunings (const juce::var& parsed, juce::Array<Tuning>& out, bool markUser)
{
    // Accept both { "tunings": [...] } and a bare [...] so a hand-written
    // user file with just the array still loads.
    const juce::var* listVar = &parsed;
    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty ("tunings"))
            listVar = &obj->getProperty ("tunings");
    }

    const auto* list = listVar->getArray();
    if (list == nullptr)
        return false;

    for (const auto& entry : *list)
    {
        auto* obj = entry.getDynamicObject();
        if (obj == nullptr)
            continue;

        Tuning t;
        t.name = obj->getProperty ("name").toString().trim();
        t.isUser = markUser;

        const auto* notes = obj->getProperty ("strings").getArray();
        if (t.name.isEmpty() || notes == nullptr)
            continue;

        bool valid = notes->size() >= kMinStrings && notes->size() <= kMaxStrings;
        for (const auto& v : *notes)
        {
            const int note = (int) v;
            if (! juce::isPositiveAndBelow (note, 128))
                valid = false;
            t.openNotes.add (note);
        }

        if (valid)
            out.add (std::move (t));
    }
    return true;
}

void TuningLibrary::loadBuiltins (const void* data, int size)
{
    builtins.clearQuick();
    const auto json = juce::String::fromUTF8 (static_cast<const char*> (data), size);
    parseTunings (juce::JSON::parse (json), builtins, false);
}

void TuningLibrary::loadUserTunings (const juce::File& file)
{
    users.clearQuick();
    if (file.existsAsFile())
        parseTunings (juce::JSON::parse (file.loadFileAsString()), users, true);
}

bool TuningLibrary::writeUserFile (const juce::File& file, const juce::Array<Tuning>& tunings)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);

    juce::Array<juce::var> list;
    for (const auto& t : tunings)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("name", t.name);
        juce::Array<juce::var> notes;
        for (auto n : t.openNotes)
            notes.add (n);
        obj->setProperty ("strings", notes);
        list.add (juce::var (obj));
    }
    root->setProperty ("tunings", list);

    if (! file.getParentDirectory().createDirectory())
        return false;
    return file.replaceWithText (juce::JSON::toString (juce::var (root)) + "\n");
}

bool TuningLibrary::saveUserTuning (const juce::File& file, const Tuning& tuning)
{
    if (tuning.name.isEmpty()
        || tuning.openNotes.size() < kMinStrings || tuning.openNotes.size() > kMaxStrings)
        return false;

    for (int i = users.size(); --i >= 0;)
        if (users.getReference (i).name == tuning.name)
            users.remove (i);

    auto copy = tuning;
    copy.isUser = true;
    users.add (std::move (copy));
    return writeUserFile (file, users);
}

bool TuningLibrary::deleteUserTuning (const juce::File& file, const juce::String& name)
{
    bool removed = false;
    for (int i = users.size(); --i >= 0;)
    {
        if (users.getReference (i).name == name)
        {
            users.remove (i);
            removed = true;
        }
    }
    return removed && writeUserFile (file, users);
}

juce::Array<Tuning> TuningLibrary::tuningsForStringCount (int numStrings) const
{
    juce::Array<Tuning> result;
    for (const auto& t : builtins)
        if (t.numStrings() == numStrings)
            result.add (t);
    for (const auto& t : users)
        if (t.numStrings() == numStrings)
            result.add (t);
    return result;
}

const Tuning* TuningLibrary::standardFor (int numStrings) const
{
    for (const auto& t : builtins)
        if (t.numStrings() == numStrings)
            return &t;
    return nullptr;
}

juce::File TuningLibrary::defaultUserFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("VirtualFret")
               .getChildFile ("tunings.json");
}

} // namespace virtualfret
