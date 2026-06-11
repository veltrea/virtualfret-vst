#include "ChordShapeLibrary.h"

namespace virtualfret
{

// Open-string notes of the 6-string standard reference the shape data
// is defined against (SPEC §10: shapes keep their finger positions under
// any actual tuning).
static constexpr int kReferenceOpen[6] = { 40, 45, 50, 55, 59, 64 };

void ChordShapeLibrary::loadBuiltins (const void* data, int size)
{
    typeList.clearQuick();

    const auto json = juce::String::fromUTF8 (static_cast<const char*> (data), size);
    const auto parsed = juce::JSON::parse (json);

    auto* rootObj = parsed.getDynamicObject();
    if (rootObj == nullptr)
        return;

    const auto* typesArr = rootObj->getProperty ("types").getArray();
    if (typesArr == nullptr)
        return;

    for (const auto& typeVar : *typesArr)
    {
        auto* typeObj = typeVar.getDynamicObject();
        if (typeObj == nullptr)
            continue;

        ChordType type;
        type.id = typeObj->getProperty ("id").toString();
        type.label = typeObj->getProperty ("label").toString();
        if (type.label.isEmpty())
            type.label = type.id;
        if (type.id.isEmpty())
            continue;

        if (const auto* shapesArr = typeObj->getProperty ("shapes").getArray())
        {
            for (const auto& shapeVar : *shapesArr)
            {
                auto* shapeObj = shapeVar.getDynamicObject();
                if (shapeObj == nullptr)
                    continue;

                ChordShape shape;
                shape.name = shapeObj->getProperty ("name").toString();
                shape.rootString = (int) shapeObj->getProperty ("rootString");
                shape.movable = (bool) shapeObj->getProperty ("movable");

                const auto* fretsArr = shapeObj->getProperty ("frets").getArray();
                if (fretsArr == nullptr || fretsArr->size() != 6)
                    continue;
                for (const auto& f : *fretsArr)
                    shape.frets.add (juce::jlimit (-1, kNumFrets, (int) f));

                // The root string must actually be fretted.
                const int rootIndex = 6 - shape.rootString;
                if (! juce::isPositiveAndBelow (rootIndex, 6) || shape.frets[rootIndex] < 0)
                    continue;

                type.shapes.add (std::move (shape));
            }
        }

        if (! type.shapes.isEmpty())
            typeList.add (std::move (type));
    }
}

const ChordType* ChordShapeLibrary::findType (const juce::String& id) const
{
    for (const auto& t : typeList)
        if (t.id == id)
            return &t;
    return nullptr;
}

juce::Array<ResolvedForm> ChordShapeLibrary::resolveForms (const juce::String& typeId,
                                                           int rootPitchClass,
                                                           int numStrings) const
{
    juce::Array<ResolvedForm> result;

    const auto* type = findType (typeId);
    if (type == nullptr
        || rootPitchClass < 0 || rootPitchClass > 11
        || numStrings < kMinStrings || numStrings > kMaxStrings)
        return result;

    for (const auto& shape : type->shapes)
    {
        const int rootIndex = 6 - shape.rootString;
        const int shapeRootPc = (kReferenceOpen[rootIndex] + shape.frets[rootIndex]) % 12;

        int baseFret = 0;
        if (shape.movable)
        {
            baseFret = ((rootPitchClass - shapeRootPc) % 12 + 12) % 12;
        }
        else if (shapeRootPc != rootPitchClass)
        {
            continue;   // an open chord only offers its fixed root
        }

        // Skip a position that would run off the fretboard.
        int maxFret = 0;
        for (auto f : shape.frets)
            maxFret = juce::jmax (maxFret, f);
        if (maxFret + baseFret > kNumFrets)
            continue;

        ResolvedForm form;
        form.name = shape.name;
        form.baseFret = baseFret;

        // Expand onto the current string count: the shape covers the
        // highest six strings, anything below is muted.
        const int extraLow = numStrings - 6;
        for (int s = 0; s < numStrings; ++s)
        {
            const int refIndex = s - extraLow;
            if (juce::isPositiveAndBelow (refIndex, 6) && shape.frets[refIndex] >= 0)
                form.latch.add (shape.frets[refIndex] + baseFret);
            else
                form.latch.add (-1);
        }

        // Drop duplicates (an open shape can coincide with a movable
        // one at base 0).
        bool duplicate = false;
        for (const auto& existing : result)
            if (existing.latch == form.latch)
                { duplicate = true; break; }
        if (! duplicate)
            result.add (std::move (form));
    }

    // Cycle order = position order on the neck.
    std::sort (result.begin(), result.end(),
               [] (const ResolvedForm& a, const ResolvedForm& b) { return a.baseFret < b.baseFret; });
    return result;
}

// --- user chords ----------------------------------------------------------

void ChordShapeLibrary::loadUserChords (const juce::File& file)
{
    userChords.clearQuick();
    if (! file.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    const juce::var* listVar = &parsed;
    if (auto* obj = parsed.getDynamicObject())
        if (obj->hasProperty ("chords"))
            listVar = &obj->getProperty ("chords");

    const auto* list = listVar->getArray();
    if (list == nullptr)
        return;

    for (const auto& entry : *list)
    {
        auto* obj = entry.getDynamicObject();
        if (obj == nullptr)
            continue;

        UserChord chord;
        chord.name = obj->getProperty ("name").toString().trim();

        const auto* latchArr = obj->getProperty ("latch").getArray();
        if (chord.name.isEmpty() || latchArr == nullptr
            || latchArr->size() < kMinStrings || latchArr->size() > kMaxStrings)
            continue;

        for (const auto& v : *latchArr)
            chord.latch.add (juce::jlimit (-1, kNumFrets, (int) v));
        userChords.add (std::move (chord));
    }
}

bool ChordShapeLibrary::writeUserChordFile (const juce::File& file, const juce::Array<UserChord>& chords)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);

    juce::Array<juce::var> list;
    for (const auto& c : chords)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("name", c.name);
        juce::Array<juce::var> latch;
        for (auto f : c.latch)
            latch.add (f);
        obj->setProperty ("latch", latch);
        list.add (juce::var (obj));
    }
    root->setProperty ("chords", list);

    if (! file.getParentDirectory().createDirectory())
        return false;
    return file.replaceWithText (juce::JSON::toString (juce::var (root)) + "\n");
}

bool ChordShapeLibrary::saveUserChord (const juce::File& file, const UserChord& chord)
{
    if (chord.name.isEmpty()
        || chord.latch.size() < kMinStrings || chord.latch.size() > kMaxStrings)
        return false;

    for (int i = userChords.size(); --i >= 0;)
        if (userChords.getReference (i).name == chord.name)
            userChords.remove (i);

    userChords.add (chord);
    return writeUserChordFile (file, userChords);
}

bool ChordShapeLibrary::deleteUserChord (const juce::File& file, const juce::String& name)
{
    bool removed = false;
    for (int i = userChords.size(); --i >= 0;)
    {
        if (userChords.getReference (i).name == name)
        {
            userChords.remove (i);
            removed = true;
        }
    }
    return removed && writeUserChordFile (file, userChords);
}

juce::Array<ChordShapeLibrary::UserChord> ChordShapeLibrary::userChordsFor (int numStrings) const
{
    juce::Array<UserChord> result;
    for (const auto& c : userChords)
        if (c.latch.size() == numStrings)
            result.add (c);
    return result;
}

juce::File ChordShapeLibrary::defaultUserChordFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("VirtualFret")
               .getChildFile ("chords.json");
}

} // namespace virtualfret
