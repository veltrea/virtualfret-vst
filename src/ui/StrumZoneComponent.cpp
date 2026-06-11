#include "StrumZoneComponent.h"
#include "Theme.h"

namespace virtualfret
{

namespace
{
    // Drag speed (px/ms) that reaches the velocity ceiling at 1x sensitivity.
    constexpr float kReferenceSpeed = 3.0f;

    // Fraction of the zone height kept clear above and below the string
    // lines as swing-through room.
    constexpr float kStringInset = 0.18f;
}

StrumZoneComponent::StrumZoneComponent (VirtualFretProcessor& processorIn)
    : processor (processorIn)
{
    setOpaque (true);
}

float StrumZoneComponent::stringLineY (int row, int numStrings) const
{
    const float height = (float) getHeight();
    const float margin = height * kStringInset;
    const float inner = height - margin * 2.0f;
    return margin + ((float) row + 0.5f) * inner / (float) juce::jmax (1, numStrings);
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
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    // soundhole-ish accent: a vertical pickup bar
    g.setColour (theme::c (theme::hairline));
    g.fillRoundedRectangle (width * 0.5f - 3.0f, height * 0.06f, 6.0f, height * 0.88f, 3.0f);

    for (int s = 0; s < numStrings; ++s)
    {
        const int row = ui::stringToRow (s, numStrings);
        const float y = stringLineY (row, numStrings);
        const float thickness = ui::stringThickness (s, numStrings);

        // Momentary pick flash, fading over flashMs — deliberately not
        // tied to the (let-ring) sounding state, which would leave every
        // string lit after one downstroke. In hold-light mode a picked
        // string stays fully lit until the mouse is released.
        auto colour = theme::c (theme::stringCol);
        if (litHeld[s])
        {
            colour = theme::c (theme::accent);
        }
        else
        {
            const double age = nowMs - hitAtMs[s];
            if (age >= 0.0 && age < flashMs)
                colour = colour.interpolatedWith (theme::c (theme::accent),
                                                  1.0f - (float) (age / flashMs));
        }
        g.setColour (colour);
        g.fillRect (0.0f, y - thickness * 0.5f, width, thickness);
    }

    g.setColour (theme::c (theme::hairline));
    g.drawRect (getLocalBounds());
}

void StrumZoneComponent::markHit (int stringIndex, double delayMs)
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;
    hitAtMs[stringIndex] = juce::Time::getMillisecondCounterHiRes() + delayMs;
    startTimerHz (60);   // animate the fade; stops itself when done
    repaint();
}

void StrumZoneComponent::timerCallback()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    bool active = false;
    for (auto t : hitAtMs)
        active = active || (nowMs - t < flashMs);   // pending (future) hits count too
    repaint();
    if (! active)
        stopTimer();
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

bool StrumZoneComponent::strumString (int stringIndex, float relMs, int velocity)
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

    if (note < 0)
        return false;

    processor.strumNote (stringIndex, note, velocity, relMs, strokeId);
    return true;
}

void StrumZoneComponent::mouseDown (const juce::MouseEvent& e)
{
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        editor->grabKeyboardFocus();   // keep Esc/Space live after strumming

    // Arm the pick: silent until a string line is crossed.
    ++strokeId;
    strokeStartMs = juce::Time::getMillisecondCounterHiRes();
    lastY = e.position.y;
    lastMs = strokeStartMs;
    crossedAny = false;

    {
        const juce::ScopedLock sl (processor.getStateLock());
        holdLightStroke = processor.getModel().strumHoldLight;
    }
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
        const float lineY = stringLineY (row, numStrings);
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
        if (strumString (crossing.string, (float) (tMs - strokeStartMs), velocity))
        {
            if (holdLightStroke)
            {
                litHeld[crossing.string] = true;
                repaint();
            }
            else
            {
                markHit (crossing.string, 0.0);
            }
        }
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

    // Hold-light mode: releasing the pick turns every lit string off.
    bool anyLit = false;
    for (auto lit : litHeld)
        anyLit = anyLit || lit;
    if (anyLit)
    {
        std::fill (std::begin (litHeld), std::end (litHeld), false);
        repaint();
    }
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
        if (strumString (s, (float) (emitted * gapMs), velocity))
        {
            markHit (s, (double) (emitted * gapMs));   // flash in step with the stagger
            ++emitted;
        }
    }
}

} // namespace virtualfret
