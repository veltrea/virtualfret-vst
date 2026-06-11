#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/ToolbarComponent.h"
#include "ui/HeadstockComponent.h"
#include "ui/FretboardComponent.h"
#include "ui/StrumZoneComponent.h"

namespace virtualfret
{

/**
    Toolbar on top, then headstock | fretboard | strum zone sharing the
    same string rows. The editor owns all model edits triggered from the
    toolbar (tuning lists, chord presets, settings menu) and polls the
    processor's atomics on a timer for the sounding/input highlights.

    Esc = all notes off. Space / Shift+Space = keyboard down/up strum.
*/
class VirtualFretEditor : public juce::AudioProcessorEditor,
                          private juce::ChangeListener,
                          private juce::Timer,
                          private juce::KeyListener
{
public:
    explicit VirtualFretEditor (VirtualFretProcessor&);
    ~VirtualFretEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void parentHierarchyChanged() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    /** KeyListener hook (registered on the top-level window so Esc/Space
        work wherever the focus sits). Delegates to keyPressed above. */
    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        return keyPressed (key);
    }

    void refreshAll();
    void refreshTuningList();
    void refreshFormLabel();

    // toolbar handlers
    void stringCountChanged (int numStrings);
    void tuningPicked (int listIndex);
    void showTuningMenu();
    void chordApply();
    void formNext();
    void applyForm (const ResolvedForm& form);
    void showChordMenu();
    void showSettingsMenu();

    void promptForName (const juce::String& title, const juce::String& message,
                        const juce::String& initialText,
                        std::function<void (const juce::String&)> onConfirm);

    VirtualFretProcessor& virtualFret;   // typed alias of the inherited processor ref
    theme::VirtualFretLookAndFeel lookAndFeel;

    ToolbarComponent toolbar;
    HeadstockComponent headstock;
    FretboardComponent fretboard;
    StrumZoneComponent strumZone;

    juce::Array<Tuning> currentTunings;       // list shown in the tuning box
    juce::Array<ResolvedForm> currentForms;   // chord preset cycle
    int formIndex = 0;

    std::array<int, kMaxStrings> lastSounding {};
    std::array<bool, 128> lastInput {};
    bool grabbedInitialFocus = false;
    juce::Component::SafePointer<juce::Component> keyListenerTarget;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualFretEditor)
};

} // namespace virtualfret
