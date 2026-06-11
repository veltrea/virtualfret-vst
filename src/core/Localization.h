#pragma once

#include <juce_core/juce_core.h>

namespace virtualfret::i18n
{

enum class Lang { en, ja };

/** Looks up a UI string. Unknown keys return the key itself so a missing
    entry is visible instead of blank. */
juce::String tr (Lang lang, const char* key);

/** "ja" -> ja, anything else -> en. */
Lang fromCode (const juce::String& code);

juce::String toCode (Lang lang);

/** Language to use when the state carries no explicit choice. */
Lang detectOSLanguage();

/** Resolves a state's language field ("" = follow the OS). */
inline Lang resolve (const juce::String& stateLanguage)
{
    return stateLanguage.isEmpty() ? detectOSLanguage() : fromCode (stateLanguage);
}

} // namespace virtualfret::i18n
