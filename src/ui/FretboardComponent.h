#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "BoardGeometry.h"

namespace virtualfret
{

/**
    The fretboard itself: open-string zone + frets 1..24, one row per
    string (top row = highest string, tab orientation).

    Mouse behaviour (SPEC §6/§8):
      - chord mode off: press = note on, release = note off; dragging
        swaps the sounding cell (old off, new on), across strings too.
      - chord mode on: clicks latch/unlatch positions (one per string),
        with an optional press-held audition of the latched note.

    Painting reads the model under the state lock, then draws from a
    local copy; sounding/input highlights poll the processor's atomics
    (the editor's timer drives repaints).
*/
class FretboardComponent : public juce::Component
{
public:
    explicit FretboardComponent (VirtualFretProcessor& processorIn);

    /** Fired after any latch edit so the owner can refresh dependants. */
    std::function<void()> onLatchEdited;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    struct Cell
    {
        int string = -1;   // internal index, 0 = lowest
        int fret = -1;     // 0 = open
        bool valid() const noexcept { return string >= 0 && fret >= 0; }
        bool operator== (const Cell& other) const noexcept
        {
            return string == other.string && fret == other.fret;
        }
    };

    Cell cellAt (juce::Point<float> p);

    /** Cell centre in component coords, already mirrored when the
        left-handed option is on. */
    float cellCentreX (int fret) const;

    /** Lazily rebuilds fret geometry when the zoom (visible frets) or
        width changed — called at the top of paint and hit-testing. */
    void ensureGeometry (int visibleFrets);

    VirtualFretProcessor& processor;

    // Geometry is always built right-handed (nut on the left); the
    // `mirrored` flag flips drawing and hit-testing for lefties.
    float fretX[kNumFrets + 1] = {};   // fretX[0] = nut; fretX[f] = wire right of fret f
    float openZoneWidth = 46.0f;
    int builtFrets = 0;                // last zoom the geometry was built for
    float builtWidth = -1.0f;
    bool mirrored = false;             // refreshed from the state on paint/hit-test

    Cell pressed;              // melodic press in progress
    Cell hover;
    int auditionString = -1;   // chord-mode audition held down, -1 = none

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FretboardComponent)
};

} // namespace virtualfret
