#include "Localization.h"
#include <unordered_map>
#include <string>

namespace virtualfret::i18n
{

namespace
{
    struct Entry { const char* en; const char* ja; };

    const std::unordered_map<std::string, Entry>& table()
    {
        static const std::unordered_map<std::string, Entry> map =
        {
            { "strings",          { "Strings",                    "弦数" } },
            { "tuning",           { "Tuning",                     "チューニング" } },
            { "saveTuning",       { "Save tuning...",             "チューニングを保存..." } },
            { "deleteTuning",     { "Delete tuning",              "チューニングを削除" } },
            { "tuningNamePrompt", { "Name for this tuning:",      "チューニング名:" } },
            { "custom",           { "Custom",                     "カスタム" } },
            { "chordMode",        { "Chord",                      "コード" } },
            { "root",             { "Root",                       "ルート" } },
            { "type",             { "Type",                       "タイプ" } },
            { "nextForm",         { "Form",                       "フォーム" } },
            { "saveChord",        { "Save chord...",              "コードを保存..." } },
            { "deleteChord",      { "Delete chord",               "コードを削除" } },
            { "userChords",       { "User chords",                "ユーザーコード" } },
            { "chordNamePrompt",  { "Name for this chord:",       "コード名:" } },
            { "clear",            { "Clear",                      "クリア" } },
            { "muteAll",          { "Mute",                       "ミュート" } },
            { "velocity",         { "Vel",                        "ベロシティ" } },
            { "channel",          { "Ch",                         "Ch" } },
            { "channelSingle",    { "Single",                     "単一" } },
            { "channelPerString", { "Per-string",                 "弦別" } },
            { "settings",         { "Settings",                   "設定" } },
            { "showNoteNames",    { "Show note names",            "ノート名を表示" } },
            { "inputHighlight",   { "Highlight input MIDI",       "入力 MIDI をハイライト" } },
            { "latchAudition",    { "Audition on latch",          "ラッチ時に試聴" } },
            { "strumSensitivity", { "Strum sensitivity",          "ストラム感度" } },
            { "strumFixedVel",    { "Fixed strum velocity",       "ストラムを固定ベロシティに" } },
            { "language",         { "Language",                   "言語" } },
            { "langEnglish",      { "English",                    "English" } },
            { "langJapanese",     { "日本語",                    "日本語" } },
            { "ok",               { "OK",                         "OK" } },
            { "cancel",           { "Cancel",                     "キャンセル" } },
            { "save",             { "Save",                       "保存" } },
        };
        return map;
    }
} // namespace

juce::String tr (Lang lang, const char* key)
{
    const auto& map = table();
    const auto it = map.find (key);
    if (it == map.end())
        return juce::String (key);
    return juce::String::fromUTF8 (lang == Lang::ja ? it->second.ja : it->second.en);
}

Lang fromCode (const juce::String& code)
{
    return code.startsWithIgnoreCase ("ja") ? Lang::ja : Lang::en;
}

juce::String toCode (Lang lang)
{
    return lang == Lang::ja ? "ja" : "en";
}

Lang detectOSLanguage()
{
    return fromCode (juce::SystemStats::getUserLanguage());
}

} // namespace virtualfret::i18n
