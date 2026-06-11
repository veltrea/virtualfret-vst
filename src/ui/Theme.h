#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Palette and look-and-feel following NoteNamer's "Ableton Live Native"
// design system (DESIGN.md, Google Stitch): achromatic Live-dark greys,
// one amber accent, strictly flat, 2px corners.
namespace virtualfret::theme
{

inline constexpr juce::uint32 foundation = 0xff3c3c3c;  // window/panel base
inline constexpr juce::uint32 toolbar    = 0xff464646;  // control strip
inline constexpr juce::uint32 well       = 0xff2f2f2f;  // recessed areas (fretboard body)
inline constexpr juce::uint32 openZone   = 0xff383838;  // open-string zone left of the nut
inline constexpr juce::uint32 strumZone  = 0xff343434;  // strum strip
inline constexpr juce::uint32 button     = 0xff555555;  // button fill
inline constexpr juce::uint32 hairline   = 0xff2a2a2a;  // borders & dividers
inline constexpr juce::uint32 editField  = 0xff222222;  // inline editor body
inline constexpr juce::uint32 accent     = 0xffffaa17;  // amber LED
inline constexpr juce::uint32 textPri    = 0xffd9d9d9;
inline constexpr juce::uint32 textSec    = 0xff9b9b9b;
inline constexpr juce::uint32 textGhost  = 0xff6f6f6f;

inline constexpr juce::uint32 fretWire   = 0xff5e5e5e;  // fret lines
inline constexpr juce::uint32 nut        = 0xff8a8a8a;  // nut line
inline constexpr juce::uint32 stringCol  = 0xff8f8f8f;  // string lines
inline constexpr juce::uint32 marker     = 0xff4a4a4a;  // position dots

inline constexpr float cornerRadius = 2.0f;
inline constexpr float inputHitAlpha = 0.22f;   // amber overlay for input-MIDI candidates

inline juce::Colour c (juce::uint32 argb) { return juce::Colour (argb); }

/** Flat Live-style LookAndFeel: rectangles, hairlines, amber accents. */
class VirtualFretLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VirtualFretLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, c (foundation));
        setColour (juce::DocumentWindow::backgroundColourId, c (foundation));

        setColour (juce::TextButton::buttonColourId, c (button));
        setColour (juce::TextButton::buttonOnColourId, c (accent));
        setColour (juce::TextButton::textColourOffId, c (textPri));
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);

        setColour (juce::ComboBox::backgroundColourId, c (well));
        setColour (juce::ComboBox::textColourId, c (textPri));
        setColour (juce::ComboBox::arrowColourId, c (textSec));
        setColour (juce::ComboBox::outlineColourId, c (hairline));

        setColour (juce::PopupMenu::backgroundColourId, c (toolbar));
        setColour (juce::PopupMenu::textColourId, c (textPri));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c (accent));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);

        setColour (juce::TextEditor::backgroundColourId, c (editField));
        setColour (juce::TextEditor::textColourId, c (accent));
        setColour (juce::TextEditor::outlineColourId, c (accent).withAlpha (0.5f));
        setColour (juce::TextEditor::focusedOutlineColourId, c (accent).withAlpha (0.5f));
        setColour (juce::TextEditor::highlightColourId, c (accent).withAlpha (0.25f));
        setColour (juce::TextEditor::highlightedTextColourId, c (accent));
        setColour (juce::CaretComponent::caretColourId, c (accent));

        setColour (juce::Label::textColourId, c (textPri));

        setColour (juce::ToggleButton::textColourId, c (textSec));
        setColour (juce::ToggleButton::tickColourId, c (accent));
        setColour (juce::ToggleButton::tickDisabledColourId, c (textGhost));

        // Live-style value box: recessed well, amber value
        setColour (juce::Slider::textBoxTextColourId, c (accent));
        setColour (juce::Slider::textBoxBackgroundColourId, c (well));
        setColour (juce::Slider::textBoxOutlineColourId, c (hairline));
        setColour (juce::Slider::textBoxHighlightColourId, c (accent).withAlpha (0.25f));
        setColour (juce::Slider::trackColourId, c (accent).withAlpha (0.6f));
        setColour (juce::Slider::backgroundColourId, c (well));
        setColour (juce::Slider::thumbColourId, c (accent));

        setColour (juce::TooltipWindow::backgroundColourId, c (toolbar));
        setColour (juce::TooltipWindow::textColourId, c (textPri));
        setColour (juce::TooltipWindow::outlineColourId, c (hairline));

        setColour (juce::AlertWindow::backgroundColourId, c (toolbar));
        setColour (juce::AlertWindow::textColourId, c (textPri));
        setColour (juce::AlertWindow::outlineColourId, c (hairline));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font { juce::FontOptions { 10.0f } };
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font { juce::FontOptions { 11.0f } };
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font { juce::FontOptions { 11.0f } };
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font { juce::FontOptions { 10.0f } };
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& buttonComp,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = buttonComp.getLocalBounds().toFloat().reduced (0.5f);
        auto fill = backgroundColour;
        if (shouldDrawButtonAsDown)
            fill = fill.brighter (0.15f);
        else if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter (0.06f);   // Live hover: ~6% brighter
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, cornerRadius);
        g.setColour (c (hairline));
        g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        // The stock layout reserves 30px for the arrow, which starves
        // narrow Live-style value boxes.
        label.setBounds (1, 1, box.getWidth() - 14, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setMinimumHorizontalScale (1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
        auto fill = box.findColour (juce::ComboBox::backgroundColourId);
        if (box.isMouseOver (true))
            fill = fill.brighter (0.06f);
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, cornerRadius);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);

        juce::Path arrow;
        const auto cx = (float) width - 8.0f;
        const auto cy = (float) height * 0.5f;
        arrow.addTriangle (cx - 3.0f, cy - 1.5f, cx + 3.0f, cy - 1.5f, cx, cy + 2.5f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    /** Square 14x14 LED toggle: off = recessed well, on = solid amber. */
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& toggle,
                           bool shouldDrawButtonAsHighlighted, bool) override
    {
        const auto bounds = toggle.getLocalBounds();
        const int led = 14;
        auto ledBox = juce::Rectangle<float> (0.5f, (float) (bounds.getHeight() - led) * 0.5f,
                                              (float) led, (float) led);

        auto fill = toggle.getToggleState() ? c (accent) : c (well);
        if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter (0.06f);
        g.setColour (fill);
        g.fillRoundedRectangle (ledBox, cornerRadius);
        g.setColour (c (hairline));
        g.drawRoundedRectangle (ledBox, cornerRadius, 1.0f);

        g.setColour (toggle.findColour (juce::ToggleButton::textColourId));
        g.setFont (juce::Font { juce::FontOptions { 10.0f } });
        g.drawText (toggle.getButtonText(),
                    bounds.withTrimmedLeft (led + 6),
                    juce::Justification::centredLeft);
    }
};

} // namespace virtualfret::theme
