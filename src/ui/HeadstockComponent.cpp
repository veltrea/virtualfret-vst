#include "HeadstockComponent.h"
#include "Theme.h"

namespace virtualfret
{

namespace
{
    constexpr float kDragPixelsPerSemitone = 8.0f;
}

HeadstockComponent::HeadstockComponent (VirtualFretProcessor& processorIn)
    : processor (processorIn)
{
    setOpaque (true);
}

void HeadstockComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::c (theme::toolbar));

    juce::Array<int> openNotes;
    int numStrings = 6;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        openNotes = processor.getModel().openNotes;
        numStrings = processor.getModel().numStrings;
    }

    const auto height = (float) getHeight();
    g.setFont (juce::Font { juce::FontOptions { 11.0f, juce::Font::bold } });

    for (int s = 0; s < numStrings && s < openNotes.size(); ++s)
    {
        const float y = ui::stringRowY (ui::stringToRow (s, numStrings), numStrings, height);

        g.setColour (theme::c (theme::accent));
        g.drawText (noteLabel (openNotes[s]),
                    juce::Rectangle<float> (2.0f, y - 9.0f, (float) getWidth() - 16.0f, 18.0f),
                    juce::Justification::centred);

        // +/- affordance arrows on the right edge
        const float ax = (float) getWidth() - 9.0f;
        juce::Path up, down;
        up.addTriangle (ax - 3.5f, y - 3.0f, ax + 3.5f, y - 3.0f, ax, y - 8.0f);
        down.addTriangle (ax - 3.5f, y + 3.0f, ax + 3.5f, y + 3.0f, ax, y + 8.0f);
        g.setColour (theme::c (theme::textGhost));
        g.fillPath (up);
        g.fillPath (down);
    }

    g.setColour (theme::c (theme::hairline));
    g.drawRect (getLocalBounds());
}

void HeadstockComponent::setOpenNote (int stringIndex, int newNote)
{
    newNote = juce::jlimit (0, 127, newNote);
    bool changed = false;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        auto& state = processor.getModel();
        if (! juce::isPositiveAndBelow (stringIndex, state.openNotes.size())
            || state.openNotes[stringIndex] == newNote)
            return;

        // Tuning edits must never strand a note: the ledger releases with
        // the note numbers that actually started (SPEC §5).
        processor.requestAllNotesOff();
        state.openNotes.set (stringIndex, newNote);

        // Keep the preset name only while the notes still match one.
        state.tuningName.clear();
        for (const auto& t : processor.getTunings().tuningsForStringCount (state.numStrings))
        {
            if (t.openNotes == state.openNotes)
            {
                state.tuningName = t.name;
                break;
            }
        }
        changed = true;
    }

    if (changed)
    {
        repaint();
        if (onTuningEdited)
            onTuningEdited();
    }
}

void HeadstockComponent::applyDelta (int stringIndex, int deltaSemitones)
{
    int current = -1;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        const auto& state = processor.getModel();
        if (juce::isPositiveAndBelow (stringIndex, state.openNotes.size()))
            current = state.openNotes[stringIndex];
    }
    if (current >= 0)
        setOpenNote (stringIndex, current + deltaSemitones);
}

void HeadstockComponent::mouseDown (const juce::MouseEvent& e)
{
    int numStrings = 6;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        numStrings = processor.getModel().numStrings;
    }

    const int row = ui::rowFromY (e.position.y, numStrings, (float) getHeight());
    dragString = ui::rowToString (row, numStrings);
    dragStartY = e.position.y;
    dragged = false;

    const juce::ScopedLock sl (processor.getStateLock());
    const auto& notes = processor.getModel().openNotes;
    dragStartNote = juce::isPositiveAndBelow (dragString, notes.size()) ? notes[dragString] : -1;
}

void HeadstockComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragString < 0 || dragStartNote < 0)
        return;

    const float dy = dragStartY - e.position.y;   // up = sharper
    if (! dragged && std::abs (dy) < 4.0f)
        return;

    dragged = true;
    setOpenNote (dragString, dragStartNote + (int) std::lround (dy / kDragPixelsPerSemitone));
}

void HeadstockComponent::mouseUp (const juce::MouseEvent& e)
{
    if (dragString >= 0 && ! dragged)
    {
        // Plain click: top half raises, bottom half lowers.
        int numStrings = 6;
        {
            const juce::ScopedLock sl (processor.getStateLock());
            numStrings = processor.getModel().numStrings;
        }
        const float rowH = (float) getHeight() / (float) numStrings;
        const float rowTop = (float) ui::stringToRow (dragString, numStrings) * rowH;
        const bool topHalf = (e.position.y - rowTop) < rowH * 0.5f;
        applyDelta (dragString, topHalf ? 1 : -1);
    }

    dragString = -1;
    dragStartNote = -1;
    dragged = false;
}

} // namespace virtualfret
