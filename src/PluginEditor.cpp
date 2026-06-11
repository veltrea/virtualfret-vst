#include "PluginEditor.h"

namespace virtualfret
{

VirtualFretEditor::VirtualFretEditor (VirtualFretProcessor& processorIn)
    : AudioProcessorEditor (processorIn),
      virtualFret (processorIn),
      headstock (processorIn),
      fretboard (processorIn),
      strumZone (processorIn)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (toolbar);
    addAndMakeVisible (headstock);
    addAndMakeVisible (fretboard);
    addAndMakeVisible (strumZone);

    toolbar.onStringCount = [this] (int n) { stringCountChanged (n); };
    toolbar.onTuningPicked = [this] (int index) { tuningPicked (index); };
    toolbar.onTuningMenu = [this] { showTuningMenu(); };
    toolbar.onChordMode = [this] (bool on)
    {
        {
            const juce::ScopedLock sl (virtualFret.getStateLock());
            auto& state = virtualFret.getModel();
            state.chordMode = on;
            if (! on)
                state.resetLatch();   // SPEC §8: leaving chord mode resets to neutral
        }
        if (! on)
        {
            virtualFret.requestAllNotesOff();   // silence ringing strums too
            currentForms.clearQuick();
            formIndex = 0;
            refreshFormLabel();
        }
        fretboard.repaint();
    };
    toolbar.onChordApply = [this] { chordApply(); };
    toolbar.onFormNext = [this] { formNext(); };
    toolbar.onChordMenu = [this] { showChordMenu(); };
    toolbar.onClear = [this]
    {
        {
            const juce::ScopedLock sl (virtualFret.getStateLock());
            virtualFret.getModel().resetLatch();
        }
        fretboard.repaint();
    };
    toolbar.onMute = [this] { virtualFret.requestAllNotesOff(); };
    toolbar.onVelocity = [this] (int velocity)
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        virtualFret.getModel().velocity = juce::jlimit (1, 127, velocity);
    };
    toolbar.onPerString = [this] (bool perString)
    {
        // No panic needed: the ledger releases with the channel each note
        // actually started on, so a mode flip cannot strand anything.
        const juce::ScopedLock sl (virtualFret.getStateLock());
        virtualFret.getModel().perStringChannels = perString;
    };
    toolbar.onChannel = [this] (int channel)
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        auto& state = virtualFret.getModel();
        if (state.perStringChannels)
            state.perStringBaseChannel = juce::jlimit (1, 16, channel);
        else
            state.channel = juce::jlimit (1, 16, channel);
    };
    toolbar.onSettings = [this] { showSettingsMenu(); };

    headstock.onTuningEdited = [this]
    {
        refreshTuningList();
        fretboard.repaint();
    };
    fretboard.onLatchEdited = [this]
    {
        // A hand-edited latch no longer matches the preset cycle.
        currentForms.clearQuick();
        formIndex = 0;
        refreshFormLabel();
    };

    virtualFret.addChangeListener (this);
    startTimerHz (30);

    setResizable (true, true);
    // The floor is deliberately low: with a reduced fret zoom the board
    // stays usable as a Guitar-Pro-style slim strip; how cramped is
    // acceptable is the user's call.
    setResizeLimits (720, 170, 4000, 1400);
    setSize (1180, 440);
    setWantsKeyboardFocus (true);

    refreshAll();
}

VirtualFretEditor::~VirtualFretEditor()
{
    // The editor is the only note source; closing it must never leave
    // notes hanging downstream.
    virtualFret.requestAllNotesOff();
    virtualFret.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void VirtualFretEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::c (theme::foundation));
}

void VirtualFretEditor::resized()
{
    auto bounds = getLocalBounds();
    toolbar.setBounds (bounds.removeFromTop (58));
    headstock.setBounds (bounds.removeFromLeft (56));
    strumZone.setBounds (bounds.removeFromRight (64));
    fretboard.setBounds (bounds);
}

bool VirtualFretEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress (juce::KeyPress::escapeKey))
    {
        virtualFret.requestAllNotesOff();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        strumZone.performKeyboardStroke (! key.getModifiers().isShiftDown());
        return true;
    }
    return false;
}

void VirtualFretEditor::mouseDown (const juce::MouseEvent&)
{
    grabKeyboardFocus();   // make Esc/Space work after any background click
}

void VirtualFretEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Host restored a project (setStateInformation); the cached preset
    // cycle belongs to the old state.
    currentForms.clearQuick();
    formIndex = 0;
    refreshAll();
}

void VirtualFretEditor::timerCallback()
{
    bool changed = false;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        const int now = virtualFret.soundingNote[(size_t) s].load (std::memory_order_relaxed);
        if (now != lastSounding[(size_t) s])
        {
            lastSounding[(size_t) s] = now;
            changed = true;
        }
    }
    for (int n = 0; n < 128; ++n)
    {
        const bool now = virtualFret.inputActive[(size_t) n].load (std::memory_order_relaxed);
        if (now != lastInput[(size_t) n])
        {
            lastInput[(size_t) n] = now;
            changed = true;
        }
    }

    if (changed)
    {
        fretboard.repaint();
        strumZone.repaint();
    }
}

void VirtualFretEditor::refreshAll()
{
    VirtualFretState snapshot;
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        snapshot = virtualFret.getModel();
    }

    toolbar.setLanguage (i18n::resolve (snapshot.language));
    toolbar.setStringCount (snapshot.numStrings);
    toolbar.setChordMode (snapshot.chordMode);
    toolbar.setVelocity (snapshot.velocity);
    toolbar.setChannelMode (snapshot.perStringChannels);
    toolbar.setChannel (snapshot.perStringChannels ? snapshot.perStringBaseChannel
                                                   : snapshot.channel);

    juce::StringArray labels, ids;
    for (const auto& type : virtualFret.getChordShapes().types())
    {
        labels.add (type.label);
        ids.add (type.id);
    }
    toolbar.setChordTypes (labels, ids);

    refreshTuningList();
    refreshFormLabel();

    headstock.repaint();
    fretboard.repaint();
    strumZone.repaint();
}

void VirtualFretEditor::refreshTuningList()
{
    int numStrings = 6;
    juce::String tuningName;
    juce::Array<int> openNotes;
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        numStrings = virtualFret.getModel().numStrings;
        tuningName = virtualFret.getModel().tuningName;
        openNotes = virtualFret.getModel().openNotes;
    }

    currentTunings = virtualFret.getTunings().tuningsForStringCount (numStrings);

    juce::StringArray names;
    int selected = -1;
    for (int i = 0; i < currentTunings.size(); ++i)
    {
        const auto& t = currentTunings.getReference (i);
        names.add (t.isUser ? t.name + " *" : t.name);
        if (selected < 0 && t.name == tuningName && t.openNotes == openNotes)
            selected = i;
    }

    toolbar.setTuningList (names, selected);
    if (selected < 0)
        toolbar.showCustomTuning();

    headstock.repaint();
}

void VirtualFretEditor::refreshFormLabel()
{
    if (currentForms.isEmpty() || ! juce::isPositiveAndBelow (formIndex, currentForms.size()))
    {
        toolbar.setFormLabel ({});
        return;
    }
    const auto& form = currentForms.getReference (formIndex);
    toolbar.setFormLabel (form.name + " @" + juce::String (form.baseFret));
}

void VirtualFretEditor::stringCountChanged (int numStrings)
{
    const auto* standard = virtualFret.getTunings().standardFor (numStrings);
    if (standard == nullptr)
        return;

    // Changing the string count releases everything, loads that count's
    // default tuning and clears the latch (SPEC §6).
    virtualFret.requestAllNotesOff();
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        auto& state = virtualFret.getModel();
        state.applyTuning (standard->name, standard->openNotes);
        state.resetLatch();
    }
    currentForms.clearQuick();
    formIndex = 0;
    refreshAll();
}

void VirtualFretEditor::tuningPicked (int listIndex)
{
    if (! juce::isPositiveAndBelow (listIndex, currentTunings.size()))
        return;

    const auto& tuning = currentTunings.getReference (listIndex);
    virtualFret.requestAllNotesOff();   // offs use the ledger's note numbers
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        virtualFret.getModel().applyTuning (tuning.name, tuning.openNotes);
    }
    refreshTuningList();
    fretboard.repaint();
}

void VirtualFretEditor::showTuningMenu()
{
    i18n::Lang lang;
    juce::String tuningName;
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        lang = i18n::resolve (virtualFret.getModel().language);
        tuningName = virtualFret.getModel().tuningName;
    }

    bool currentIsUser = false;
    for (const auto& t : currentTunings)
        if (t.isUser && t.name == tuningName)
            currentIsUser = true;

    juce::PopupMenu menu;
    menu.addItem (1, i18n::tr (lang, "saveTuning"));
    menu.addItem (2, i18n::tr (lang, "deleteTuning"), currentIsUser);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toolbar),
        [safe = juce::Component::SafePointer (this), lang, tuningName] (int result)
        {
            if (safe == nullptr)
                return;
            auto& self = *safe;

            if (result == 1)
            {
                self.promptForName (i18n::tr (lang, "saveTuning"),
                                    i18n::tr (lang, "tuningNamePrompt"),
                                    tuningName.isEmpty() ? "My tuning" : tuningName,
                    [safe] (const juce::String& name)
                    {
                        if (safe == nullptr || name.isEmpty())
                            return;
                        Tuning tuning;
                        tuning.name = name;
                        {
                            const juce::ScopedLock sl (safe->virtualFret.getStateLock());
                            tuning.openNotes = safe->virtualFret.getModel().openNotes;
                            safe->virtualFret.getModel().tuningName = name;
                        }
                        safe->virtualFret.getTunings().saveUserTuning (TuningLibrary::defaultUserFile(), tuning);
                        safe->refreshTuningList();
                    });
            }
            else if (result == 2)
            {
                self.virtualFret.getTunings().deleteUserTuning (TuningLibrary::defaultUserFile(), tuningName);
                self.refreshTuningList();
            }
        });
}

void VirtualFretEditor::chordApply()
{
    int numStrings = 6;
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        numStrings = virtualFret.getModel().numStrings;
    }

    currentForms = virtualFret.getChordShapes().resolveForms (toolbar.selectedTypeId(),
                                                            toolbar.selectedRootPc(),
                                                            numStrings);
    formIndex = 0;
    if (! currentForms.isEmpty())
        applyForm (currentForms.getReference (0));
    refreshFormLabel();
}

void VirtualFretEditor::formNext()
{
    if (currentForms.isEmpty())
    {
        chordApply();   // nothing cached yet: resolve from the current root/type
        return;
    }
    formIndex = (formIndex + 1) % currentForms.size();
    applyForm (currentForms.getReference (formIndex));
    refreshFormLabel();
}

void VirtualFretEditor::applyForm (const ResolvedForm& form)
{
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        auto& state = virtualFret.getModel();
        state.chordMode = true;            // picking a preset turns chord mode on (SPEC §10)
        state.latch = form.latch;
    }
    toolbar.setChordMode (true);
    fretboard.repaint();
}

void VirtualFretEditor::showChordMenu()
{
    i18n::Lang lang;
    int numStrings = 6;
    bool hasLatch = false;
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        const auto& state = virtualFret.getModel();
        lang = i18n::resolve (state.language);
        numStrings = state.numStrings;
        for (auto f : state.latch)
            hasLatch = hasLatch || f >= 0;
    }

    const auto userChords = virtualFret.getChordShapes().userChordsFor (numStrings);

    juce::PopupMenu menu;
    menu.addItem (1, i18n::tr (lang, "saveChord"), hasLatch);

    juce::PopupMenu pick, remove;
    for (int i = 0; i < userChords.size(); ++i)
    {
        pick.addItem (100 + i, userChords.getReference (i).name);
        remove.addItem (200 + i, userChords.getReference (i).name);
    }
    menu.addSubMenu (i18n::tr (lang, "userChords"), pick, ! userChords.isEmpty());
    menu.addSubMenu (i18n::tr (lang, "deleteChord"), remove, ! userChords.isEmpty());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toolbar),
        [safe = juce::Component::SafePointer (this), lang, userChords] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            auto& self = *safe;

            if (result == 1)
            {
                self.promptForName (i18n::tr (lang, "saveChord"),
                                    i18n::tr (lang, "chordNamePrompt"), "My chord",
                    [safe] (const juce::String& name)
                    {
                        if (safe == nullptr || name.isEmpty())
                            return;
                        ChordShapeLibrary::UserChord chord;
                        chord.name = name;
                        {
                            const juce::ScopedLock sl (safe->virtualFret.getStateLock());
                            chord.latch = safe->virtualFret.getModel().latch;
                        }
                        safe->virtualFret.getChordShapes().saveUserChord (
                            ChordShapeLibrary::defaultUserChordFile(), chord);
                    });
            }
            else if (result >= 200)
            {
                const int index = result - 200;
                if (juce::isPositiveAndBelow (index, userChords.size()))
                    self.virtualFret.getChordShapes().deleteUserChord (
                        ChordShapeLibrary::defaultUserChordFile(),
                        userChords.getReference (index).name);
            }
            else if (result >= 100)
            {
                const int index = result - 100;
                if (juce::isPositiveAndBelow (index, userChords.size()))
                {
                    ResolvedForm form;
                    form.latch = userChords.getReference (index).latch;
                    self.applyForm (form);
                    self.currentForms.clearQuick();
                    self.formIndex = 0;
                    self.refreshFormLabel();
                }
            }
        });
}

void VirtualFretEditor::showSettingsMenu()
{
    VirtualFretState snapshot;
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        snapshot = virtualFret.getModel();
    }
    const auto lang = i18n::resolve (snapshot.language);

    juce::PopupMenu menu;
    menu.addItem (1, i18n::tr (lang, "showNoteNames"), true, snapshot.showNoteNames);
    menu.addItem (2, i18n::tr (lang, "inputHighlight"), true, snapshot.inputHighlight);
    menu.addItem (3, i18n::tr (lang, "latchAudition"), true, snapshot.latchAudition);
    menu.addItem (4, i18n::tr (lang, "strumFixedVel"), true, snapshot.strumFixedVelocity);

    juce::PopupMenu sensitivity;
    const float levels[] = { 0.5f, 1.0f, 2.0f, 4.0f };
    for (int i = 0; i < 4; ++i)
        sensitivity.addItem (10 + i, juce::String (levels[i], 1) + "x", true,
                             juce::approximatelyEqual (snapshot.strumSensitivity, levels[i]));
    menu.addSubMenu (i18n::tr (lang, "strumSensitivity"), sensitivity);

    juce::PopupMenu fretZoom;
    const int fretChoices[] = { 12, 15, 18, 21, 24 };
    for (int i = 0; i < 5; ++i)
        fretZoom.addItem (30 + i, juce::String (fretChoices[i]), true,
                          snapshot.visibleFrets == fretChoices[i]);
    menu.addSubMenu (i18n::tr (lang, "visibleFrets"), fretZoom);

    juce::PopupMenu language;
    language.addItem (20, i18n::tr (lang, "langEnglish"), true, lang == i18n::Lang::en);
    language.addItem (21, i18n::tr (lang, "langJapanese"), true, lang == i18n::Lang::ja);
    menu.addSubMenu (i18n::tr (lang, "language"), language);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toolbar),
        [safe = juce::Component::SafePointer (this)] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            auto& self = *safe;

            {
                const juce::ScopedLock sl (self.virtualFret.getStateLock());
                auto& state = self.virtualFret.getModel();
                switch (result)
                {
                    case 1: state.showNoteNames = ! state.showNoteNames; break;
                    case 2: state.inputHighlight = ! state.inputHighlight; break;
                    case 3: state.latchAudition = ! state.latchAudition; break;
                    case 4: state.strumFixedVelocity = ! state.strumFixedVelocity; break;
                    case 10: state.strumSensitivity = 0.5f; break;
                    case 11: state.strumSensitivity = 1.0f; break;
                    case 12: state.strumSensitivity = 2.0f; break;
                    case 13: state.strumSensitivity = 4.0f; break;
                    case 20: state.language = "en"; break;
                    case 21: state.language = "ja"; break;
                    case 30: state.visibleFrets = 12; break;
                    case 31: state.visibleFrets = 15; break;
                    case 32: state.visibleFrets = 18; break;
                    case 33: state.visibleFrets = 21; break;
                    case 34: state.visibleFrets = 24; break;
                    default: break;
                }
            }
            self.refreshAll();
        });
}

void VirtualFretEditor::promptForName (const juce::String& title, const juce::String& message,
                                       const juce::String& initialText,
                                       std::function<void (const juce::String&)> onConfirm)
{
    i18n::Lang lang;
    {
        const juce::ScopedLock sl (virtualFret.getStateLock());
        lang = i18n::resolve (virtualFret.getModel().language);
    }

    auto* window = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", initialText);
    window->addButton (i18n::tr (lang, "save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton (i18n::tr (lang, "cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true,
        juce::ModalCallbackFunction::create (
            [window, confirm = std::move (onConfirm)] (int result)
            {
                // Read the editor synchronously, while the window is intact.
                const auto name = window->getTextEditorContents ("name").trim();
                if (result == 1 && confirm)
                    confirm (name);
            }),
        true);   // delete the window when dismissed
}

} // namespace virtualfret
