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
*/
class StrumZoneComponent : public juce::Component
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
    /** Sounds string `stringIndex` as part of the current stroke, if it
        has a note to play (chord mode skips muted strings). */
    void strumString (int stringIndex, float relMs, int velocity);

    int velocityForSpeed (float pxPerMs) const;

    VirtualFretProcessor& processor;

    juce::uint8 strokeId = 0;
    double strokeStartMs = 0.0;
    float lastY = 0.0f;
    double lastMs = 0.0;
    bool crossedAny = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrumZoneComponent)
};

} // namespace virtualfret
