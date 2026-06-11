#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "BoardGeometry.h"

namespace virtualfret
{

/**
    The headstock strip left of the nut: shows each string's open note
    and edits it in semitones — click top/bottom half = +/-1, vertical
    drag = continuous (SPEC §7). The fretboard look never changes with
    the tuning; only the per-cell notes do.

    Every edit releases all sounding notes first (the ledger sends the
    offs with the notes that actually started) and renames the tuning to
    "" (= Custom) unless the result matches a preset.
*/
class HeadstockComponent : public juce::Component
{
public:
    explicit HeadstockComponent (VirtualFretProcessor& processorIn);

    /** Fired after any tuning edit so the owner refreshes the toolbar. */
    std::function<void()> onTuningEdited;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void applyDelta (int stringIndex, int deltaSemitones);
    void setOpenNote (int stringIndex, int newNote);

    VirtualFretProcessor& processor;

    int dragString = -1;
    int dragStartNote = -1;
    float dragStartY = 0.0f;
    bool dragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeadstockComponent)
};

} // namespace virtualfret
