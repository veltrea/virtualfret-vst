#include "ToolbarComponent.h"
#include "Theme.h"
#include "../core/FretModel.h"

namespace virtualfret
{

ToolbarComponent::ToolbarComponent()
{
    const auto addBox = [this] (juce::ComboBox& box)
    {
        addAndMakeVisible (box);
        box.setWantsKeyboardFocus (false);
    };
    const auto addButton = [this] (juce::Button& button)
    {
        addAndMakeVisible (button);
        button.setWantsKeyboardFocus (false);
    };

    addBox (stringsBox);
    for (int n = kMinStrings; n <= kMaxStrings; ++n)
        stringsBox.addItem (juce::String (n), n);
    stringsBox.onChange = [this]
    {
        if (! suppressCallbacks && onStringCount)
            onStringCount (stringsBox.getSelectedId());
    };

    addBox (tuningBox);
    tuningBox.onChange = [this]
    {
        const int index = tuningBox.getSelectedId() - 1;
        if (! suppressCallbacks && index >= 0 && onTuningPicked)
            onTuningPicked (index);
    };

    addButton (tuningMenuButton);
    tuningMenuButton.onClick = [this] { if (onTuningMenu) onTuningMenu(); };

    addButton (chordToggle);
    chordToggle.onClick = [this]
    {
        if (! suppressCallbacks && onChordMode)
            onChordMode (chordToggle.getToggleState());
    };

    addBox (rootBox);
    for (int pc = 0; pc < 12; ++pc)
        rootBox.addItem (pitchClassName (pc), pc + 1);
    rootBox.setSelectedId (1, juce::dontSendNotification);
    rootBox.onChange = [this]
    {
        if (! suppressCallbacks && onChordApply)
            onChordApply();
    };

    addBox (typeBox);
    typeBox.onChange = [this]
    {
        if (! suppressCallbacks && onChordApply)
            onChordApply();
    };

    addButton (formButton);
    formButton.onClick = [this] { if (onFormNext) onFormNext(); };

    addButton (chordMenuButton);
    chordMenuButton.onClick = [this] { if (onChordMenu) onChordMenu(); };

    addButton (clearButton);
    clearButton.onClick = [this] { if (onClear) onClear(); };

    addButton (muteButton);
    muteButton.onClick = [this] { if (onMute) onMute(); };

    addAndMakeVisible (velocityLabel);
    velocityLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (velocitySlider);
    velocitySlider.setSliderStyle (juce::Slider::LinearBar);
    velocitySlider.setRange (1.0, 127.0, 1.0);
    velocitySlider.setValue (100.0, juce::dontSendNotification);
    velocitySlider.setWantsKeyboardFocus (false);
    velocitySlider.onValueChange = [this]
    {
        if (! suppressCallbacks && onVelocity)
            onVelocity ((int) velocitySlider.getValue());
    };

    addAndMakeVisible (channelLabel);
    channelLabel.setJustificationType (juce::Justification::centredRight);

    addBox (channelModeBox);
    channelModeBox.onChange = [this]
    {
        if (! suppressCallbacks && onPerString)
            onPerString (channelModeBox.getSelectedId() == 2);
    };

    addBox (channelBox);
    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem (juce::String (ch), ch);
    channelBox.onChange = [this]
    {
        if (! suppressCallbacks && onChannel)
            onChannel (channelBox.getSelectedId());
    };

    addButton (settingsButton);
    settingsButton.onClick = [this] { if (onSettings) onSettings(); };

    rebuildStaticItems();
}

void ToolbarComponent::rebuildStaticItems()
{
    const auto tr = [this] (const char* key) { return i18n::tr (lang, key); };

    chordToggle.setButtonText (tr ("chordMode"));
    formButton.setButtonText (tr ("nextForm"));
    clearButton.setButtonText (tr ("clear"));
    muteButton.setButtonText (tr ("muteAll"));
    settingsButton.setButtonText (tr ("settings"));
    velocityLabel.setText (tr ("velocity"), juce::dontSendNotification);
    channelLabel.setText (tr ("channel"), juce::dontSendNotification);

    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    const int previous = channelModeBox.getSelectedId();
    channelModeBox.clear (juce::dontSendNotification);
    channelModeBox.addItem (tr ("channelSingle"), 1);
    channelModeBox.addItem (tr ("channelPerString"), 2);
    channelModeBox.setSelectedId (previous > 0 ? previous : 1, juce::dontSendNotification);
}

void ToolbarComponent::setLanguage (i18n::Lang newLang)
{
    lang = newLang;
    rebuildStaticItems();
    repaint();
}

void ToolbarComponent::setStringCount (int numStrings)
{
    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    stringsBox.setSelectedId (juce::jlimit (kMinStrings, kMaxStrings, numStrings),
                              juce::dontSendNotification);
}

void ToolbarComponent::setTuningList (const juce::StringArray& names, int selectedIndex)
{
    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    tuningBox.clear (juce::dontSendNotification);
    for (int i = 0; i < names.size(); ++i)
        tuningBox.addItem (names[i], i + 1);
    if (selectedIndex >= 0 && selectedIndex < names.size())
        tuningBox.setSelectedId (selectedIndex + 1, juce::dontSendNotification);
}

void ToolbarComponent::showCustomTuning()
{
    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    tuningBox.setSelectedId (0, juce::dontSendNotification);
    tuningBox.setText (i18n::tr (lang, "custom"), juce::dontSendNotification);
}

void ToolbarComponent::setChordTypes (const juce::StringArray& labels, const juce::StringArray& idsIn)
{
    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    const auto previous = selectedTypeId();
    typeIds = idsIn;
    typeBox.clear (juce::dontSendNotification);
    for (int i = 0; i < labels.size(); ++i)
        typeBox.addItem (labels[i], i + 1);

    int select = typeIds.indexOf (previous);
    if (select < 0 && ! labels.isEmpty())
        select = 0;
    if (select >= 0)
        typeBox.setSelectedId (select + 1, juce::dontSendNotification);
}

void ToolbarComponent::setChordMode (bool on)
{
    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    chordToggle.setToggleState (on, juce::dontSendNotification);
}

void ToolbarComponent::setFormLabel (const juce::String& text)
{
    formButton.setButtonText (text.isEmpty() ? i18n::tr (lang, "nextForm") : text);
}

void ToolbarComponent::setVelocity (int velocity)
{
    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    velocitySlider.setValue (velocity, juce::dontSendNotification);
}

void ToolbarComponent::setChannelMode (bool perString)
{
    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    channelModeBox.setSelectedId (perString ? 2 : 1, juce::dontSendNotification);
}

void ToolbarComponent::setChannel (int channel)
{
    const juce::ScopedValueSetter<bool> svs (suppressCallbacks, true);
    channelBox.setSelectedId (juce::jlimit (1, 16, channel), juce::dontSendNotification);
}

int ToolbarComponent::selectedRootPc() const
{
    return juce::jmax (0, rootBox.getSelectedId() - 1);
}

juce::String ToolbarComponent::selectedTypeId() const
{
    const int index = typeBox.getSelectedId() - 1;
    return juce::isPositiveAndBelow (index, typeIds.size()) ? typeIds[index] : juce::String();
}

void ToolbarComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::c (theme::toolbar));
    g.setColour (theme::c (theme::hairline));
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void ToolbarComponent::resized()
{
    const int rowH = getHeight() / 2;
    const int pad = 4;
    const int controlH = rowH - pad * 2;

    auto layoutRow = [&] (int rowIndex)
    {
        return juce::Rectangle<int> (0, rowIndex * rowH, getWidth(), rowH)
                   .reduced (6, pad);
    };

    auto row = layoutRow (0);
    stringsBox.setBounds (row.removeFromLeft (46).withHeight (controlH));
    row.removeFromLeft (4);
    tuningBox.setBounds (row.removeFromLeft (168).withHeight (controlH));
    row.removeFromLeft (2);
    tuningMenuButton.setBounds (row.removeFromLeft (24).withHeight (controlH));
    row.removeFromLeft (14);
    chordToggle.setBounds (row.removeFromLeft (74).withHeight (controlH));
    row.removeFromLeft (4);
    rootBox.setBounds (row.removeFromLeft (52).withHeight (controlH));
    row.removeFromLeft (4);
    typeBox.setBounds (row.removeFromLeft (64).withHeight (controlH));
    row.removeFromLeft (4);
    formButton.setBounds (row.removeFromLeft (92).withHeight (controlH));
    row.removeFromLeft (2);
    chordMenuButton.setBounds (row.removeFromLeft (24).withHeight (controlH));
    row.removeFromLeft (14);
    clearButton.setBounds (row.removeFromLeft (56).withHeight (controlH));

    row = layoutRow (1);
    muteButton.setBounds (row.removeFromLeft (56).withHeight (controlH));
    row.removeFromLeft (14);
    velocityLabel.setBounds (row.removeFromLeft (56).withHeight (controlH));
    row.removeFromLeft (4);
    velocitySlider.setBounds (row.removeFromLeft (72).withHeight (controlH));
    row.removeFromLeft (14);
    channelLabel.setBounds (row.removeFromLeft (24).withHeight (controlH));
    row.removeFromLeft (4);
    channelModeBox.setBounds (row.removeFromLeft (92).withHeight (controlH));
    row.removeFromLeft (4);
    channelBox.setBounds (row.removeFromLeft (46).withHeight (controlH));
    row.removeFromLeft (14);
    settingsButton.setBounds (row.removeFromLeft (76).withHeight (controlH));
}

} // namespace virtualfret
