#include "StrumZoneComponent.h"
#include "Theme.h"

namespace virtualfret
{

namespace
{
    // Drag speed (px/ms) that reaches the velocity ceiling at 1x sensitivity.
    constexpr float kReferenceSpeed = 3.0f;
}

StrumZoneComponent::StrumZoneComponent (VirtualFretProcessor& processorIn)
    : processor (processorIn)
{
    setOpaque (true);
}

void StrumZoneComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::c (theme::strumZone));

    int numStrings = 6;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        numStrings = processor.getModel().numStrings;
    }

    const auto width = (float) getWidth();
    const auto height = (float) getHeight();

    // soundhole-ish accent: a vertical pickup bar
    g.setColour (theme::c (theme::hairline));
    g.fillRoundedRectangle (width * 0.5f - 3.0f, height * 0.06f, 6.0f, height * 0.88f, 3.0f);

    for (int s = 0; s < numStrings; ++s)
    {
        const int row = ui::stringToRow (s, numStrings);
        const float y = ui::stringRowY (row, numStrings, height);
        const float thickness = ui::stringThickness (s, numStrings);

        const bool sounding = processor.soundingNote[(size_t) s].load (std::memory_order_relaxed) >= 0;
        g.setColour (sounding ? theme::c (theme::accent)
                              : theme::c (theme::stringCol));
        g.fillRect (0.0f, y - thickness * 0.5f, width, thickness);
    }

    g.setColour (theme::c (theme::hairline));
    g.drawRect (getLocalBounds());
}

int StrumZoneComponent::velocityForSpeed (float pxPerMs) const
{
    const juce::ScopedLock sl (processor.getStateLock());
    const auto& state = processor.getModel();

    if (state.strumFixedVelocity)
        return state.velocity;

    const float reference = kReferenceSpeed / juce::jmax (0.25f, state.strumSensitivity);
    const float norm = juce::jlimit (0.0f, 1.0f, pxPerMs / reference);
    const int span = state.strumVelMax - state.strumVelMin;
    return juce::jlimit (1, 127, state.strumVelMin + (int) std::lround (norm * (float) span));
}

void StrumZoneComponent::strumString (int stringIndex, float relMs, int velocity)
{
    int note = -1;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        const auto& state = processor.getModel();

        if (state.chordMode)
        {
            // latched fret, open ring, or skip a muted string
            const int fret = stringIndex < state.latch.size() ? state.latch[stringIndex] : -1;
            if (fret >= 0)
                note = state.noteForCell (stringIndex, fret);
        }
        else
        {
            note = state.noteForCell (stringIndex, 0);
        }
    }

    if (note >= 0)
        processor.strumNote (stringIndex, note, velocity, relMs, strokeId);
}

void StrumZoneComponent::mouseDown (const juce::MouseEvent& e)
{
    // Arm the pick: silent until a string line is crossed.
    ++strokeId;
    strokeStartMs = juce::Time::getMillisecondCounterHiRes();
    lastY = e.position.y;
    lastMs = strokeStartMs;
    crossedAny = false;
}

void StrumZoneComponent::mouseDrag (const juce::MouseEvent& e)
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const float y0 = lastY;
    const float y1 = e.position.y;

    if (juce::approximatelyEqual (y0, y1))
        return;

    int numStrings = 6;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        numStrings = processor.getModel().numStrings;
    }

    const double dtMs = juce::jmax (0.1, nowMs - lastMs);
    const float speed = std::abs (y1 - y0) / (float) dtMs;
    const int velocity = velocityForSpeed (speed);
    const bool downward = y1 > y0;   // screen-down = toward lower rows = downstroke

    // Collect every string line inside (y0, y1] in traversal order; the
    // crossing instant is linearly interpolated for the sample-accurate
    // stroke spacing (SPEC §5/§9).
    struct Crossing { float lineY; int string; };
    juce::Array<Crossing> crossings;
    for (int row = 0; row < numStrings; ++row)
    {
        const float lineY = ui::stringRowY (row, numStrings, (float) getHeight());
        const float lo = juce::jmin (y0, y1);
        const float hi = juce::jmax (y0, y1);
        if (lineY > lo && lineY <= hi)
            crossings.add ({ lineY, ui::rowToString (row, numStrings) });
    }

    std::sort (crossings.begin(), crossings.end(),
               [downward] (const Crossing& a, const Crossing& b)
               { return downward ? a.lineY < b.lineY : a.lineY > b.lineY; });

    for (const auto& crossing : crossings)
    {
        const double tMs = lastMs + (double) ((crossing.lineY - y0) / (y1 - y0)) * dtMs;
        strumString (crossing.string, (float) (tMs - strokeStartMs), velocity);
        crossedAny = true;
    }

    lastY = y1;
    lastMs = nowMs;
}

void StrumZoneComponent::mouseUp (const juce::MouseEvent&)
{
    // Click-and-release without crossing a string = pick mute (SPEC §9).
    if (! crossedAny)
        processor.requestAllNotesOff();
    crossedAny = false;
}

void StrumZoneComponent::performKeyboardStroke (bool downstroke)
{
    ++strokeId;

    int numStrings = 6;
    int gapMs = 15;
    int velocity = 100;
    {
        const juce::ScopedLock sl (processor.getStateLock());
        const auto& state = processor.getModel();
        numStrings = state.numStrings;
        gapMs = state.keyboardStrumMs;
        velocity = state.velocity;
    }

    // Down = low string first; the gap counts only strings that sound,
    // keeping the stroke even when chord mode mutes some.
    int emitted = 0;
    for (int i = 0; i < numStrings; ++i)
    {
        const int s = downstroke ? i : numStrings - 1 - i;

        int note = -1;
        {
            const juce::ScopedLock sl (processor.getStateLock());
            const auto& state = processor.getModel();
            if (state.chordMode)
            {
                const int fret = s < state.latch.size() ? state.latch[s] : -1;
                if (fret >= 0)
                    note = state.noteForCell (s, fret);
            }
            else
            {
                note = state.noteForCell (s, 0);
            }
        }

        if (note >= 0)
        {
            processor.strumNote (s, note, velocity, (float) (emitted * gapMs), strokeId);
            ++emitted;
        }
    }
}

} // namespace virtualfret
