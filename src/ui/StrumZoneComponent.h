#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "BoardGeometry.h"

namespace virtualfret
{

/**
    The strum strip right of the fretboard (SPEC §9). Mouse down arms
    the pick silently; every string line the pointer crosses while
    dragging sounds that string — latched notes in chord mode, open
    strings otherwise. Crossing times are interpolated to real time and
    sent as stroke-relative offsets so the processor can spread them
    across sample offsets. A click that crosses nothing is the
    pick-mute gesture: all notes off.

    Strings flash amber for an instant when picked and fade right back
    (they do not stay lit while a note rings) so rapid up/down strokes
    each read as their own hit.
*/
class StrumZoneComponent : public juce::Component,
                           private juce::Timer
{
public:
    explicit StrumZoneComponent (VirtualFretProcessor& processorIn);

    /** Space-bar strum (M5): fixed per-string gap from the settings. */
    void performKeyboardStroke (bool downstroke);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    /** Sounds string `stringIndex` as part of the current stroke, if it
        has a note to play (chord mode skips muted strings). Returns
        true when a note was actually queued. */
    bool strumString (int stringIndex, float relMs, int velocity);

    /** Schedules the pick flash for a string, `delayMs` from now (to
        match keyboard strums, whose notes start staggered). */
    void markHit (int stringIndex, double delayMs);

    int velocityForSpeed (float pxPerMs) const;

    VirtualFretProcessor& processor;

    juce::uint8 strokeId = 0;
    double strokeStartMs = 0.0;
    float lastY = 0.0f;
    double lastMs = 0.0;
    bool crossedAny = false;

    static constexpr double flashMs = 120.0;
    double hitAtMs[kMaxStrings] = {};   // absolute time of each string's last pick

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrumZoneComponent)
};

} // namespace virtualfret
