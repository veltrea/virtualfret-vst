#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/FretModel.h"

// Shared row geometry so the headstock, fretboard and strum zone agree
// on where each string runs. Display rows count from the top (row 0 =
// highest-pitched string, like tab); internal string indices count from
// the lowest string.
namespace virtualfret::ui
{

inline float stringRowY (int row, int numStrings, float height)
{
    return ((float) row + 0.5f) * height / (float) numStrings;
}

inline int rowFromY (float y, int numStrings, float height)
{
    const float rowH = height / (float) numStrings;
    return juce::jlimit (0, numStrings - 1, (int) (y / rowH));
}

inline int rowToString (int row, int numStrings)  { return numStrings - 1 - row; }
inline int stringToRow (int s, int numStrings)    { return numStrings - 1 - s; }

/** Low strings draw thicker, like wound strings. */
inline float stringThickness (int stringIndex, int numStrings)
{
    if (numStrings <= 1)
        return 1.5f;
    const float t = (float) stringIndex / (float) (numStrings - 1);   // 0 = lowest string
    return 2.6f - 1.6f * t;
}

} // namespace virtualfret::ui
