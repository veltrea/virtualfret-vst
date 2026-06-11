// Console unit tests for the core logic (no GUI, no plugin host needed).
// Run via: ctest --test-dir build  (or the CoreTests binary directly)

#include <juce_core/juce_core.h>

#include "core/FretModel.h"
#include "core/TuningLibrary.h"
#include "core/ChordShapeLibrary.h"
#include "core/StrumScheduler.h"
#include "core/Localization.h"
#include "VirtualFretAssets.h"

#include <cstdio>

static int failures = 0;

#define EXPECT(cond)                                                            \
    do {                                                                        \
        if (! (cond)) {                                                         \
            ++failures;                                                         \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                       \
    } while (false)

#define EXPECT_EQ(a, b)                                                         \
    do {                                                                        \
        const auto va = (a); const auto vb = (b);                               \
        if (! (va == vb)) {                                                     \
            ++failures;                                                         \
            std::printf ("FAIL %s:%d  %s == %s\n   lhs: %s\n   rhs: %s\n",      \
                         __FILE__, __LINE__, #a, #b,                            \
                         juce::String (va).toRawUTF8(),                         \
                         juce::String (vb).toRawUTF8());                        \
        }                                                                       \
    } while (false)

using namespace virtualfret;

static juce::String latchToString (const juce::Array<int>& latch)
{
    juce::StringArray parts;
    for (auto f : latch)
        parts.add (juce::String (f));
    return parts.joinIntoString (",");
}

static void testNoteLabels()
{
    // Scientific pitch, matching the SPEC §7 tuning tables (E2 = 40).
    EXPECT_EQ (noteLabel (40), juce::String ("E2"));
    EXPECT_EQ (noteLabel (60), juce::String ("C4"));
    EXPECT_EQ (noteLabel (64), juce::String ("E4"));
    EXPECT_EQ (noteLabel (39), juce::String ("D#2"));
    EXPECT_EQ (noteLabel (0), juce::String ("C-1"));
    EXPECT_EQ (noteLabel (127), juce::String ("G9"));
    EXPECT_EQ (noteLabel (128), juce::String());
    EXPECT_EQ (pitchClassName (0), juce::String ("C"));
    EXPECT_EQ (pitchClassName (11), juce::String ("B"));
}

static void testCellAndChannels()
{
    VirtualFretState state;   // 6-string standard

    EXPECT_EQ (state.noteForCell (0, 0), 40);    // low E open
    EXPECT_EQ (state.noteForCell (0, 5), 45);
    EXPECT_EQ (state.noteForCell (5, 0), 64);    // high E open
    EXPECT_EQ (state.noteForCell (5, 24), 88);
    EXPECT_EQ (state.noteForCell (6, 0), -1);    // no such string
    EXPECT_EQ (state.noteForCell (0, 25), -1);   // past the last fret
    EXPECT_EQ (state.noteForCell (0, -1), -1);

    state.channel = 5;
    EXPECT_EQ (state.channelForString (0), 5);   // single mode: everything on ch5
    EXPECT_EQ (state.channelForString (5), 5);

    state.perStringChannels = true;
    state.perStringBaseChannel = 1;
    EXPECT_EQ (state.channelForString (5), 1);   // highest string = UI string 1 = base
    EXPECT_EQ (state.channelForString (0), 6);   // lowest of 6 = UI string 6
    state.perStringBaseChannel = 14;
    EXPECT_EQ (state.channelForString (0), 3);   // 14 + 5 wraps past 16 -> 3
}

static void testStateJsonRoundTrip()
{
    VirtualFretState state;
    state.numStrings = 7;
    state.tuningName = "Drop A";
    state.openNotes = { 33, 40, 45, 50, 55, 59, 64 };
    state.chordMode = true;
    state.latch = { -1, 0, 2, 2, 2, 0, -1 };
    state.velocity = 88;
    state.perStringChannels = true;
    state.channel = 3;
    state.perStringBaseChannel = 2;
    state.strumSensitivity = 2.0f;
    state.strumVelMin = 30;
    state.strumVelMax = 110;
    state.strumFixedVelocity = true;
    state.keyboardStrumMs = 25;
    state.showNoteNames = true;
    state.inputHighlight = false;
    state.latchAudition = false;
    state.visibleFrets = 15;
    state.language = "ja";

    VirtualFretState restored;
    EXPECT (restored.restoreFromStateJson (state.toStateJson()));
    EXPECT_EQ (restored.numStrings, 7);
    EXPECT_EQ (restored.tuningName, juce::String ("Drop A"));
    EXPECT (restored.openNotes == state.openNotes);
    EXPECT (restored.chordMode);
    EXPECT (restored.latch == state.latch);
    EXPECT_EQ (restored.velocity, 88);
    EXPECT (restored.perStringChannels);
    EXPECT_EQ (restored.channel, 3);
    EXPECT_EQ (restored.perStringBaseChannel, 2);
    EXPECT (juce::approximatelyEqual (restored.strumSensitivity, 2.0f));
    EXPECT_EQ (restored.strumVelMin, 30);
    EXPECT_EQ (restored.strumVelMax, 110);
    EXPECT (restored.strumFixedVelocity);
    EXPECT_EQ (restored.keyboardStrumMs, 25);
    EXPECT (restored.showNoteNames);
    EXPECT (! restored.inputHighlight);
    EXPECT (! restored.latchAudition);
    EXPECT_EQ (restored.visibleFrets, 15);
    EXPECT_EQ (restored.language, juce::String ("ja"));

    // Older states without the field keep the full 24-fret view.
    VirtualFretState legacy;
    EXPECT (legacy.restoreFromStateJson (
        R"({"numStrings": 6, "openNotes": [40,45,50,55,59,64]})"));
    EXPECT_EQ (legacy.visibleFrets, kNumFrets);

    // Malformed documents leave the state untouched.
    VirtualFretState untouched;
    EXPECT (! untouched.restoreFromStateJson ("not json at all"));
    EXPECT (! untouched.restoreFromStateJson ("{}"));
    EXPECT (! untouched.restoreFromStateJson (R"({"numStrings": 6, "openNotes": [40, 45]})"));
    EXPECT (! untouched.restoreFromStateJson (R"({"numStrings": 5, "openNotes": [1,2,3,4,5]})"));
    EXPECT_EQ (untouched.numStrings, 6);
    EXPECT_EQ (untouched.openNotes[0], 40);

    // A latch of the wrong size is replaced by all-muted, not rejected.
    VirtualFretState badLatch;
    EXPECT (badLatch.restoreFromStateJson (
        R"({"numStrings": 6, "openNotes": [40,45,50,55,59,64], "latch": [1,2]})"));
    EXPECT_EQ (badLatch.latch.size(), 6);
    EXPECT_EQ (badLatch.latch[0], -1);
}

static void testTuningBuiltins()
{
    TuningLibrary library;
    library.loadBuiltins (VirtualFretAssets::tunings_json, VirtualFretAssets::tunings_jsonSize);

    EXPECT_EQ (library.tuningsForStringCount (6).size(), 12);
    EXPECT_EQ (library.tuningsForStringCount (7).size(), 2);
    EXPECT_EQ (library.tuningsForStringCount (8).size(), 2);
    EXPECT_EQ (library.tuningsForStringCount (9).size(), 1);

    const auto* standard6 = library.standardFor (6);
    EXPECT (standard6 != nullptr);
    if (standard6 != nullptr)
    {
        EXPECT_EQ (standard6->name, juce::String ("Standard"));
        EXPECT ((standard6->openNotes == juce::Array<int> { 40, 45, 50, 55, 59, 64 }));
    }

    const auto* standard9 = library.standardFor (9);
    EXPECT (standard9 != nullptr);
    if (standard9 != nullptr)
        EXPECT_EQ (standard9->openNotes[0], 25);   // C#1

    EXPECT (library.standardFor (5) == nullptr);
}

static void testTuningUserFile()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("virtualfret-tests");
    dir.deleteRecursively();
    const auto file = dir.getChildFile ("tunings.json");

    TuningLibrary library;
    library.loadBuiltins (VirtualFretAssets::tunings_json, VirtualFretAssets::tunings_jsonSize);

    Tuning custom;
    custom.name = "My Drop C";
    custom.openNotes = { 36, 43, 48, 53, 57, 62 };
    EXPECT (library.saveUserTuning (file, custom));
    EXPECT (file.existsAsFile());

    // A fresh library picks it up from the file; built-ins come first.
    TuningLibrary reloaded;
    reloaded.loadBuiltins (VirtualFretAssets::tunings_json, VirtualFretAssets::tunings_jsonSize);
    reloaded.loadUserTunings (file);
    const auto list = reloaded.tuningsForStringCount (6);
    EXPECT_EQ (list.size(), 13);
    EXPECT (list.getLast().isUser);
    EXPECT_EQ (list.getLast().name, juce::String ("My Drop C"));

    // Saving the same name replaces, not duplicates.
    custom.openNotes.set (0, 35);
    EXPECT (library.saveUserTuning (file, custom));
    TuningLibrary replaced;
    replaced.loadUserTunings (file);
    EXPECT_EQ (replaced.tuningsForStringCount (6).size(), 1);
    EXPECT_EQ (replaced.tuningsForStringCount (6).getFirst().openNotes[0], 35);

    EXPECT (library.deleteUserTuning (file, "My Drop C"));
    TuningLibrary emptied;
    emptied.loadUserTunings (file);
    EXPECT_EQ (emptied.tuningsForStringCount (6).size(), 0);

    // SPEC §7: a hand-written single-entry list (no wrapper object) loads too.
    file.replaceWithText (R"([{ "name": "Hand-written", "strings": [36, 43, 48, 53, 57, 62] }])");
    TuningLibrary hand;
    hand.loadUserTunings (file);
    EXPECT_EQ (hand.tuningsForStringCount (6).size(), 1);

    dir.deleteRecursively();
}

static void testChordShapeResolution()
{
    ChordShapeLibrary library;
    library.loadBuiltins (VirtualFretAssets::chordshapes_json, VirtualFretAssets::chordshapes_jsonSize);

    EXPECT_EQ (library.types().size(), 13);
    EXPECT (library.findType ("maj") != nullptr);
    EXPECT (library.findType ("nope") == nullptr);

    // SPEC §10 worked example: F major via the E-form barre -> 133211.
    const auto fMajor = library.resolveForms ("maj", 5 /* F */, 6);
    EXPECT (! fMajor.isEmpty());
    bool foundEFormF = false;
    for (const auto& form : fMajor)
    {
        if (form.name == "E-form")
        {
            foundEFormF = true;
            EXPECT_EQ (form.baseFret, 1);
            EXPECT_EQ (latchToString (form.latch), juce::String ("1,3,3,2,1,1"));
        }
    }
    EXPECT (foundEFormF);

    // C major: open C at base 0 sorts first; A-form at 3 and E-form at 8 follow.
    const auto cMajor = library.resolveForms ("maj", 0 /* C */, 6);
    EXPECT_EQ (cMajor.size(), 3);
    if (cMajor.size() == 3)
    {
        EXPECT_EQ (cMajor[0].name, juce::String ("Open C"));
        EXPECT_EQ (latchToString (cMajor[0].latch), juce::String ("-1,3,2,0,1,0"));
        EXPECT_EQ (cMajor[1].name, juce::String ("A-form"));
        EXPECT_EQ (cMajor[1].baseFret, 3);
        EXPECT_EQ (cMajor[2].name, juce::String ("E-form"));
        EXPECT_EQ (cMajor[2].baseFret, 8);
    }

    // 7-string expansion: the extra low string is muted (SPEC §10).
    const auto fMajor7String = library.resolveForms ("maj", 5, 7);
    EXPECT (! fMajor7String.isEmpty());
    for (const auto& form : fMajor7String)
    {
        EXPECT_EQ (form.latch.size(), 7);
        EXPECT_EQ (form.latch[0], -1);
    }

    // Open chords only answer to their fixed root: G major must not
    // offer the open-D shape.
    for (const auto& form : library.resolveForms ("maj", 7 /* G */, 6))
        EXPECT (form.name != "Open D");

    // Power chord keeps the high strings muted.
    const auto a5 = library.resolveForms ("5", 9 /* A */, 6);
    EXPECT (! a5.isEmpty());
    if (! a5.isEmpty())
        EXPECT_EQ (latchToString (a5[0].latch), juce::String ("-1,0,2,2,-1,-1"));

    // Universal invariants over the whole library: every form of every
    // type/root/string-count stays on the fretboard, and (all built-in
    // shapes voice the root on their lowest sounding string under the
    // reference tuning) the lowest sounding string hits the root.
    static constexpr int reference[6] = { 40, 45, 50, 55, 59, 64 };
    for (const auto& type : library.types())
    {
        for (int root = 0; root < 12; ++root)
        {
            for (int strings = kMinStrings; strings <= kMaxStrings; ++strings)
            {
                const auto forms = library.resolveForms (type.id, root, strings);
                EXPECT (! forms.isEmpty());

                for (const auto& form : forms)
                {
                    EXPECT_EQ (form.latch.size(), strings);

                    int lowestNote = -1;
                    for (int s = 0; s < form.latch.size(); ++s)
                    {
                        const int fret = form.latch[s];
                        EXPECT (fret >= -1 && fret <= kNumFrets);
                        const int refIndex = s - (strings - 6);
                        if (fret >= 0 && lowestNote < 0 && refIndex >= 0)
                            lowestNote = reference[refIndex] + fret;
                    }
                    EXPECT (lowestNote >= 0);
                    if (lowestNote >= 0)
                        EXPECT_EQ (lowestNote % 12, root);
                }
            }
        }
    }
}

static void testUserChords()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("virtualfret-tests-chords");
    dir.deleteRecursively();
    const auto file = dir.getChildFile ("chords.json");

    ChordShapeLibrary library;

    ChordShapeLibrary::UserChord chord;
    chord.name = "My voicing";
    chord.latch = { -1, 3, 2, 0, 1, 0 };
    EXPECT (library.saveUserChord (file, chord));

    ChordShapeLibrary reloaded;
    reloaded.loadUserChords (file);
    EXPECT_EQ (reloaded.userChordsFor (6).size(), 1);
    EXPECT_EQ (reloaded.userChordsFor (7).size(), 0);   // filtered by string count
    EXPECT (reloaded.userChordsFor (6).getFirst().latch == chord.latch);

    EXPECT (library.deleteUserChord (file, "My voicing"));
    ChordShapeLibrary emptied;
    emptied.loadUserChords (file);
    EXPECT_EQ (emptied.userChordsFor (6).size(), 0);

    dir.deleteRecursively();
}

struct EmittedNote
{
    int block = 0;
    int offset = 0;
    int note = 0;
};

static void testStrumScheduler()
{
    // 48kHz, 512-sample blocks (~10.67ms). A 10ms-per-string strum must
    // spread across blocks at 480-sample spacing instead of collapsing
    // (SPEC §5).
    StrumScheduler scheduler;
    scheduler.prepare (48000.0);

    const auto makeCmd = [] (int note, float relMs, juce::uint8 strokeId)
    {
        NoteCommand cmd;
        cmd.type = NoteCommand::noteOn;
        cmd.note = (juce::uint8) note;
        cmd.relMs = relMs;
        cmd.strokeId = strokeId;
        return cmd;
    };

    juce::Array<EmittedNote> emitted;
    const auto runBlock = [&] (int blockIndex)
    {
        scheduler.processBlock (512, [&] (const NoteCommand& cmd, int offset)
        {
            emitted.add ({ blockIndex, offset, (int) cmd.note });
        });
    };

    for (int i = 0; i < 5; ++i)
        EXPECT (scheduler.push (makeCmd (60 + i, (float) (i * 10), 1)));

    runBlock (0);
    runBlock (1);
    runBlock (2);
    runBlock (3);

    EXPECT_EQ (emitted.size(), 5);
    if (emitted.size() == 5)
    {
        EXPECT_EQ (emitted[0].block, 0);  EXPECT_EQ (emitted[0].offset, 0);
        EXPECT_EQ (emitted[1].block, 0);  EXPECT_EQ (emitted[1].offset, 480);
        EXPECT_EQ (emitted[2].block, 1);  EXPECT_EQ (emitted[2].offset, 448);   // 960 - 512
        EXPECT_EQ (emitted[3].block, 2);  EXPECT_EQ (emitted[3].offset, 416);   // 1440 - 1024
        EXPECT_EQ (emitted[4].block, 3);  EXPECT_EQ (emitted[4].offset, 384);   // 1920 - 1536
        for (int i = 0; i < 5; ++i)
            EXPECT_EQ (emitted[i].note, 60 + i);   // strum order preserved
    }

    // A new stroke id re-anchors to the current block, even though the
    // GUI clock has moved on (relMs restarts near zero or keeps growing).
    emitted.clearQuick();
    EXPECT (scheduler.push (makeCmd (70, 500.0f, 2)));
    EXPECT (scheduler.push (makeCmd (71, 505.0f, 2)));
    runBlock (4);
    EXPECT_EQ (emitted.size(), 2);
    if (emitted.size() == 2)
    {
        EXPECT_EQ (emitted[0].offset, 0);
        EXPECT_EQ (emitted[1].offset, 240);   // 5ms at 48k
    }

    // Immediate commands (relMs < 0) land at the top of the next block.
    emitted.clearQuick();
    NoteCommand off;
    off.type = NoteCommand::stringOff;
    off.relMs = -1.0f;
    EXPECT (scheduler.push (off));
    runBlock (5);
    EXPECT_EQ (emitted.size(), 1);
    if (emitted.size() == 1)
        EXPECT_EQ (emitted[0].offset, 0);

    // clear() drops pending events (panic path).
    EXPECT (scheduler.push (makeCmd (80, 100.0f, 3)));
    scheduler.clear();
    emitted.clearQuick();
    runBlock (6);
    EXPECT_EQ (emitted.size(), 0);
}

static void testLocalization()
{
    using namespace virtualfret::i18n;

    EXPECT_EQ (tr (Lang::en, "chordMode"), juce::String ("Chord"));
    EXPECT (tr (Lang::ja, "chordMode").isNotEmpty());
    EXPECT (tr (Lang::ja, "chordMode") != tr (Lang::en, "chordMode"));
    EXPECT_EQ (tr (Lang::en, "missing-key"), juce::String ("missing-key"));

    EXPECT (fromCode ("ja") == Lang::ja);
    EXPECT (fromCode ("ja-JP") == Lang::ja);
    EXPECT (fromCode ("en") == Lang::en);
    EXPECT (fromCode ("") == Lang::en);
    EXPECT_EQ (toCode (Lang::ja), juce::String ("ja"));

    EXPECT (resolve ("ja") == Lang::ja);
    EXPECT (resolve ("en") == Lang::en);
}

int main()
{
    testNoteLabels();
    testCellAndChannels();
    testStateJsonRoundTrip();
    testTuningBuiltins();
    testTuningUserFile();
    testChordShapeResolution();
    testUserChords();
    testStrumScheduler();
    testLocalization();

    if (failures == 0)
    {
        std::printf ("all tests passed\n");
        return 0;
    }
    std::printf ("%d failure(s)\n", failures);
    return 1;
}
