#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace anamorph::gui
{

// ============================================================================
//  Palette + LookAndFeel
//
//  A clean, premium "digital plugin" aesthetic (spec section 10): near-black
//  background, a restrained cool accent gradient, modern thin-arc knobs, no
//  skeuomorphism (no wood, brushed metal or vintage VU meters).
// ============================================================================
namespace colours
{
    const juce::Colour bg        { 0xff0e1014 };
    const juce::Colour bgPanel   { 0xff161a21 };
    const juce::Colour bgRaised  { 0xff1d222b };
    const juce::Colour outline   { 0xff2a313d };
    const juce::Colour text      { 0xffd7dde6 };
    const juce::Colour textDim   { 0xff8b94a3 };
    const juce::Colour accent    { 0xff35d0c0 }; // teal/cyan
    const juce::Colour accent2   { 0xff5aa6ff }; // soft blue
    const juce::Colour warn      { 0xffe0a94a };
}

// ============================================================================
//  Glass surfaces (feedback #17)
//
//  A subtle, reversible "iOS-26 liquid glass" treatment shared by every framed
//  surface (scope, meters, panels): a diagonal micro-gradient that is brightest
//  at the TOP-RIGHT and darkest at the BOTTOM-LEFT, plus soft highlight edges on
//  the top-left and bottom-right so the frame reads like a pane of glass. Kept
//  deliberately faint so it never overpowers the existing dark aesthetic.
// ============================================================================
namespace glass
{
    // Highlight edges + base hairline only (the caller fills the interior). The
    // top-left corner catches the brightest, thickest highlight; the bottom-right
    // a dimmer one; the other two corners stay un-lit for diagonal contrast, and
    // a soft inset stroke blends the bright edge into the content.
    void drawEdges (juce::Graphics&, juce::Rectangle<float> bounds, float radius,
                    float strength = 1.0f);
    // Diagonal depth gradient (top-right bright -> bottom-left dark) + glass edges.
    void fillPanel (juce::Graphics&, juce::Rectangle<float> bounds, float radius,
                    juce::Colour base, float strength = 1.0f);
    // Glass rim for round controls: a bright top-left arc with a faint glow on the
    // opposite edge, matching the panel edges (#16).
    void drawCircleEdge (juce::Graphics&, float centreX, float centreY, float radius,
                         float strength = 1.0f);
}

// Eased 0..1 animation property ("hovA"/"actA"/"onA") published by the editor's
// micro-anim driver (F3). Falls back to the binary state for components that
// aren't registered (or before the first animated frame), so every drawing path
// works with or without the driver.
inline float animOr (const juce::Component& c, const char* key, bool fallback)
{
    if (const auto* v = c.getProperties().getVarPointer (key))
        return (float) (double) *v;
    return fallback ? 1.0f : 0.0f;
}

class AnamorphLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AnamorphLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    // Inset the interactive track by a thumb-radius so the thumb stays fully on the
    // track AND tracks the cursor 1:1 (no lag), without a remap that desynced them
    // (#4/#5).
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    // Indent the selected text a little from the left edge (#13).
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    // Honour each Label's explicitly-set font instead of forcing one size, so the
    // larger Simple-mode Widen text actually renders (recurring font request).
    void drawLabel (juce::Graphics&, juce::Label&) override;

    // Glassy highlight on the hovered pop-up row (Apple "liquid glass", #6).
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    // Unify the pop-up list with the rounded flat-design of the combo box (#22).
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    // Small dim caps header for the preset menu's FACTORY / USER sections (F2).
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override;
    int  getPopupMenuBorderSize() override { return 3; } // narrower top/bottom dead-zone (#9)
    // Fixed, uniform row height so a taller combo doesn't get taller rows (#3).
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardHeight, int& idealWidth, int& idealHeight) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // A value box you can drag (up/down) to change the value, like the knob (#2).
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // Uniform, compact font for every combo + its pop-up list (#13).
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    // Styled tooltip to match the design language (no system tooltip, #20).
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> pos,
                                           juce::Rectangle<int> parentArea) override;
};

// A variant with a smaller pop-up list, applied only to the compact Input
// Channel / M/S Solo combos so their lists feel balanced (#12).
class CompactComboLookAndFeel : public AnamorphLookAndFeel
{
public:
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getPopupMenuFont() override                { return juce::Font (juce::FontOptions (12.0f)); }
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardHeight, int& w, int& h) override
    {
        AnamorphLookAndFeel::getIdealPopupMenuItemSize (text, isSeparator, standardHeight, w, h);
        if (! isSeparator) h = 19;
    }
};

// A larger-text variant for the two Simple-mode Widen combos (algorithm +
// Style/Focus) so their text scales up with the rest of the enlarged Simple
// controls; the pop-up list rows grow to match (#17).
class SimpleComboLookAndFeel : public AnamorphLookAndFeel
{
public:
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (15.5f)); }
    juce::Font getPopupMenuFont() override                { return juce::Font (juce::FontOptions (15.0f)); }
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardHeight, int& w, int& h) override
    {
        AnamorphLookAndFeel::getIdealPopupMenuItemSize (text, isSeparator, standardHeight, w, h);
        if (! isSeparator) h = 27;
    }
};

} // namespace anamorph::gui
