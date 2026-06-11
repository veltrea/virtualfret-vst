#include "FretboardComponent.h"
#include "Theme.h"

namespace virtualfret
{

namespace
{
    // Fret widths shrink geometrically toward the body — recognisably a
    // guitar neck, but gentler than the real 2^(1/12) so high frets stay
    // clickable.
    constexpr float kFretShrink = 0.97f;

    bool isMarkerFret (int fret)    { return fret == 3 || fret == 5 || fret == 7 || fret == 9
                                          || fret == 15 || fret == 17 || fret == 19 || fret == 21; }
    bool isDoubleMarkerFret (int f) { return f == 12 || f == 24; }
}

FretboardComponent::FretboardComponent (VirtualFretProcessor& processorIn)
    : processor (processorIn)
{
    setOpaque (true);
}

void FretboardComponent::ensureGeometry (int visibleFrets)
{
    const int frets = juce::jlimit (5, kNumFrets, visibleFrets);
    if (frets == builtFrets && juce::approximatelyEqual (builtWidth, (float) getWidth()))
        return;

    builtFrets = frets;
    builtWidth = (float) getWidth();

    const float nutX = openZoneWidth;
    const float boardW = juce::jmax (1.0f, builtWidth - nutX);

    // The chosen zoom fills the width: fretX[f] = nut + boardW * (1 - r^f) / (1 - r^frets)
    const float full = 1.0f - std::pow (kFretShrink, (float) builtFrets);
    fretX[0] = nutX;
    for (int f = 1; f <= builtFrets; ++f)
        fretX[f] = nutX + boardW * (1.0f - std::pow (kFretShrink, (float) f)) / full;

    // Park the out-of-view wires on the right edge so any stray index
    // stays harmless.
    for (int f = builtFrets + 1; f <= kNumFrets; ++f)
        fretX[f] = fretX[builtFrets];
}

float FretboardComponent::cellCentreX (int fret) const
{
    if (fret <= 0)
        return openZoneWidth * 0.5f;
    return (fretX[fret - 1] + fretX[fret]) * 0.5f;
}

FretboardComponent::Cell FretboardComponent::cellAt (juce::Point<float> p)
{
    // Clamp instead of rejecting: a drag that wanders off the edges keeps
    // playing the nearest cell, like a finger that can't leave the neck.
    int numStrings = 6;
    int visibleFrets = kNumFrets;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        numStrings = processor.getModel().numStrings;
        visibleFrets = processor.getModel().visibleFrets;
    }
    ensureGeometry (visibleFrets);

    Cell cell;
    const int row = ui::rowFromY (juce::jlimit (0.0f, (float) getHeight() - 1.0f, p.y),
                                  numStrings, (float) getHeight());
    cell.string = ui::rowToString (row, numStrings);

    const float x = juce::jlimit (0.0f, fretX[builtFrets], p.x);
    cell.fret = 0;
    for (int f = 1; f <= builtFrets; ++f)
    {
        if (x > fretX[f - 1])
            cell.fret = f;
        else
            break;
    }
    return cell;
}

void FretboardComponent::paint (juce::Graphics& g)
{
    // Snapshot the model so the lock is not held while painting.
    VirtualFretState snapshot;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        snapshot = processor.getModel();
    }
    const int numStrings = snapshot.numStrings;
    const auto height = (float) getHeight();
    ensureGeometry (snapshot.visibleFrets);

    // -- board ------------------------------------------------------------
    g.fillAll (theme::c (theme::well));
    g.setColour (theme::c (theme::openZone));
    g.fillRect (0.0f, 0.0f, openZoneWidth, height);

    // position markers (between fret wires, vertically centred)
    for (int f = 1; f <= builtFrets; ++f)
    {
        if (! isMarkerFret (f) && ! isDoubleMarkerFret (f))
            continue;

        const float cx = cellCentreX (f);
        const float radius = juce::jmin (6.0f, (fretX[f] - fretX[f - 1]) * 0.18f + 2.0f);
        g.setColour (theme::c (theme::marker));

        if (isDoubleMarkerFret (f))
        {
            g.fillEllipse (cx - radius, height * 0.32f - radius, radius * 2.0f, radius * 2.0f);
            g.fillEllipse (cx - radius, height * 0.68f - radius, radius * 2.0f, radius * 2.0f);
        }
        else
        {
            g.fillEllipse (cx - radius, height * 0.5f - radius, radius * 2.0f, radius * 2.0f);
        }
    }

    // fret wires + nut
    g.setColour (theme::c (theme::fretWire));
    for (int f = 1; f <= builtFrets; ++f)
        g.fillRect (fretX[f] - 1.0f, 0.0f, 2.0f, height);
    g.setColour (theme::c (theme::nut));
    g.fillRect (fretX[0] - 2.0f, 0.0f, 4.0f, height);

    // strings
    for (int s = 0; s < numStrings; ++s)
    {
        const int row = ui::stringToRow (s, numStrings);
        const float y = ui::stringRowY (row, numStrings, height);
        const float thickness = ui::stringThickness (s, numStrings);
        g.setColour (theme::c (theme::stringCol));
        g.fillRect (0.0f, y - thickness * 0.5f, (float) getWidth(), thickness);
    }

    const float rowH = height / (float) numStrings;
    const float dotR = juce::jmin (rowH * 0.34f, 11.0f);

    // note-name overlay (optional)
    if (snapshot.showNoteNames)
    {
        g.setFont (juce::Font { juce::FontOptions { 8.5f } });
        for (int s = 0; s < numStrings; ++s)
        {
            const float y = ui::stringRowY (ui::stringToRow (s, numStrings), numStrings, height);
            for (int f = 0; f <= builtFrets; ++f)
            {
                const int note = snapshot.noteForCell (s, f);
                if (note < 0)
                    continue;
                g.setColour (theme::c (theme::textGhost));
                g.drawText (pitchClassName (note % 12),
                            juce::Rectangle<float> (cellCentreX (f) - 12.0f, y - rowH * 0.5f, 24.0f, rowH * 0.42f),
                            juce::Justification::centred);
            }
        }
    }

    // input-MIDI candidate positions (every cell that could sound a held input note)
    if (snapshot.inputHighlight)
    {
        g.setColour (theme::c (theme::accent).withAlpha (theme::inputHitAlpha));
        for (int s = 0; s < numStrings; ++s)
        {
            const float y = ui::stringRowY (ui::stringToRow (s, numStrings), numStrings, height);
            for (int f = 0; f <= builtFrets; ++f)
            {
                const int note = snapshot.noteForCell (s, f);
                if (note >= 0 && processor.inputActive[(size_t) note].load (std::memory_order_relaxed))
                    g.fillEllipse (cellCentreX (f) - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
            }
        }
    }

    // latched chord positions
    if (snapshot.chordMode)
    {
        for (int s = 0; s < numStrings; ++s)
        {
            const float y = ui::stringRowY (ui::stringToRow (s, numStrings), numStrings, height);
            const int fret = s < snapshot.latch.size() ? snapshot.latch[s] : -1;

            if (fret < 0)
            {
                // muted string: x in the open zone
                g.setColour (theme::c (theme::textGhost));
                const float r = dotR * 0.55f;
                const float cx = openZoneWidth * 0.5f;
                g.drawLine (cx - r, y - r, cx + r, y + r, 1.6f);
                g.drawLine (cx - r, y + r, cx + r, y - r, 1.6f);
            }
            else if (fret == 0)
            {
                // open string: ring in the open zone
                g.setColour (theme::c (theme::accent));
                g.drawEllipse (openZoneWidth * 0.5f - dotR * 0.75f, y - dotR * 0.75f,
                               dotR * 1.5f, dotR * 1.5f, 1.8f);
            }
            else if (fret <= builtFrets)
            {
                g.setColour (theme::c (theme::accent).withAlpha (0.55f));
                g.fillEllipse (cellCentreX (fret) - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
            }
        }
    }

    // sounding cells (from the audio-thread ledger)
    for (int s = 0; s < numStrings; ++s)
    {
        const int note = processor.soundingNote[(size_t) s].load (std::memory_order_relaxed);
        if (note < 0 || s >= snapshot.openNotes.size())
            continue;
        const int fret = note - snapshot.openNotes[s];
        if (fret < 0 || fret > builtFrets)
            continue;

        const float y = ui::stringRowY (ui::stringToRow (s, numStrings), numStrings, height);
        const float cx = cellCentreX (fret);
        g.setColour (theme::c (theme::accent));
        g.fillEllipse (cx - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
        if (dotR >= 6.5f)   // skip the label once rows get Guitar-Pro slim
        {
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.setFont (juce::Font { juce::FontOptions { 8.5f } });
            g.drawText (pitchClassName (note % 12),
                        juce::Rectangle<float> (cx - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f),
                        juce::Justification::centred);
        }
    }

    // hover hint
    if (hover.valid())
    {
        const float y = ui::stringRowY (ui::stringToRow (hover.string, numStrings), numStrings, height);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (cellCentreX (hover.fret) - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f, 1.5f);
    }

    // hairline frame
    g.setColour (theme::c (theme::hairline));
    g.drawRect (getLocalBounds());
}

void FretboardComponent::mouseDown (const juce::MouseEvent& e)
{
    // Children cover the whole editor, so route the keyboard focus up —
    // Esc (mute) and Space (strum) should work right after any click.
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        editor->grabKeyboardFocus();

    const auto cell = cellAt (e.position);
    if (! cell.valid())
        return;

    bool chordMode = false;
    bool audition = true;
    int note = -1;
    int latchedFret = -1;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        auto& state = processor.getModel();
        chordMode = state.chordMode;
        audition = state.latchAudition;
        note = state.noteForCell (cell.string, cell.fret);

        if (chordMode && cell.string < state.latch.size())
        {
            // one position per string; clicking the latched cell unlatches
            const int current = state.latch[cell.string];
            state.latch.set (cell.string, current == cell.fret ? -1 : cell.fret);
            latchedFret = state.latch[cell.string];
        }
    }

    if (chordMode)
    {
        if (audition && latchedFret >= 0 && note >= 0)
        {
            processor.playNote (cell.string, note);
            auditionString = cell.string;
        }
        if (onLatchEdited)
            onLatchEdited();
        repaint();
        return;
    }

    if (note >= 0)
    {
        processor.playNote (cell.string, note);
        pressed = cell;
    }
}

void FretboardComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! pressed.valid())
        return;

    const auto cell = cellAt (e.position);
    if (! cell.valid() || cell == pressed)
        return;

    int note = -1;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        note = processor.getModel().noteForCell (cell.string, cell.fret);
    }
    if (note < 0)
        return;

    // Swap: hammer-on/pull-off stand-in along a string, plain swap across
    // strings. Same-string swaps rely on the per-string mono ledger.
    if (cell.string != pressed.string)
        processor.releaseString (pressed.string);
    processor.playNote (cell.string, note);
    pressed = cell;
}

void FretboardComponent::mouseUp (const juce::MouseEvent&)
{
    if (pressed.valid())
    {
        processor.releaseString (pressed.string);
        pressed = {};
    }
    if (auditionString >= 0)
    {
        processor.releaseString (auditionString);
        auditionString = -1;
    }
}

void FretboardComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto cell = cellAt (e.position);
    if (! (cell == hover))
    {
        hover = cell;
        repaint();
    }
}

void FretboardComponent::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    repaint();
}

} // namespace virtualfret
