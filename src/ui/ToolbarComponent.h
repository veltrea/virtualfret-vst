#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/Localization.h"

namespace virtualfret
{

/**
    Two-row control strip. Pure view: it fires callbacks and shows what
    the editor tells it to — model edits and menu logic live in the
    editor, which also rebuilds the tuning list / chord types on
    refresh.
*/
class ToolbarComponent : public juce::Component
{
public:
    ToolbarComponent();

    // --- editor wiring -----------------------------------------------------
    std::function<void (int)> onStringCount;            // 6..9
    std::function<void (int)> onTuningPicked;           // index into the supplied list
    std::function<void()> onTuningMenu;                 // save/delete popup
    std::function<void (bool)> onChordMode;
    std::function<void()> onChordApply;                 // root/type combo changed
    std::function<void()> onFormNext;
    std::function<void()> onChordMenu;                  // save/user chords popup
    std::function<void()> onClear;
    std::function<void()> onMute;
    std::function<void (int)> onVelocity;
    std::function<void (bool)> onPerString;
    std::function<void (int)> onChannel;
    std::function<void()> onSettings;

    // --- state the editor pushes in ----------------------------------------
    void setLanguage (i18n::Lang lang);
    void setStringCount (int numStrings);
    void setTuningList (const juce::StringArray& names, int selectedIndex);
    void showCustomTuning();                            // current notes match no preset
    void setChordTypes (const juce::StringArray& labels, const juce::StringArray& idsIn);
    void setChordMode (bool on);
    void setFormLabel (const juce::String& text);
    void setVelocity (int velocity);
    void setChannelMode (bool perString);
    void setChannel (int channel);

    int selectedRootPc() const;                          // 0..11
    juce::String selectedTypeId() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildStaticItems();

    i18n::Lang lang = i18n::Lang::en;

    juce::ComboBox stringsBox, tuningBox, rootBox, typeBox, channelModeBox, channelBox;
    juce::TextButton tuningMenuButton { "..." }, formButton, chordMenuButton { "..." },
                     clearButton, muteButton, settingsButton;
    juce::ToggleButton chordToggle;
    juce::Slider velocitySlider;
    juce::Label velocityLabel, channelLabel;

    juce::StringArray typeIds;
    bool suppressCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolbarComponent)
};

} // namespace virtualfret
