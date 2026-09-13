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
    // JUCE paints a "resizable frame" over a menu ONLY when the menu has a parent component
    // (juce_PopupMenu.cpp paintOverChildren) -- two translucent black rects in the 3 px border
    // ring, on top of the hairline drawPopupMenuBackground already draws. The preset menu became
    // a child in 0.9.2 (lifetime fix), so without this no-op it would gain a doubled edge the
    // rest of the UI does not have. Anamorph has no resizable windows or ResizableBorderComponent,
    // so this override has no other caller.
    void drawResizableFrame (juce::Graphics&, int, int, const juce::BorderSize<int>&) override {}
    // Fixed, uniform row height so a taller combo doesn't get taller rows (#3); the WIDTH is
    // measured from the item text in the menu's own font (see the .cpp for the chrome budget).
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardHeight, int& idealWidth, int& idealHeight) override;

    // Every PopupMenu window built through THIS look-and-feel reports itself here
    // (juce_PopupMenu.cpp:500 calls it from the MenuWindow constructor). It is the one hook that
    // catches a menu we did not create ourselves -- a ComboBox drop-down (juce_ComboBox.cpp:561
    // sets the menu's look-and-feel to ours) and a TextEditor context menu
    // (juce_TextEditor.cpp:1578 does the same). The editor uses it to know a pop-up is on screen;
    // see PluginEditor's pop-up shield. Empty when nobody is listening (safe to skip).
    //
    // NOT called for a menu whose own look-and-feel is null: findLookAndFeel returns
    // `menu.lookAndFeel.get()` (juce_PopupMenu.cpp:1422-1425), and the `lf` used at :500 is captured
    // at :368, BEFORE the window is parented -- so it is the default look-and-feel, not the one it
    // would inherit from its parent. The preset menu is that case (INC-010 dropped its
    // setLookAndFeel on purpose), and the editor tracks it directly instead.
    std::function<void (juce::Component& menuWindow)> onPopupMenuWindowCreated;
    void preparePopupMenuWindow (juce::Component& newMenuWindow) override
    {
        // Chain first, observe second: this hook is purely ADDITIVE. The inherited implementation is
        // LookAndFeel_V2's empty one in the pinned tree (juce_LookAndFeel_V2.cpp:1172), so today the
        // call is free -- but a later JUCE that gives menu windows real per-look-and-feel preparation
        // here (shadow, opacity, rounding) would otherwise be silently skipped for every Anamorph
        // menu. Same shape as the other overrides in this class that extend rather than replace
        // (getSliderLayout, drawButtonText, fillTextEditorBackground, drawTextEditorOutline).
        juce::LookAndFeel_V4::preparePopupMenuWindow (newMenuWindow);
        if (onPopupMenuWindowCreated) onPopupMenuWindowCreated (newMenuWindow);
    }

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // A value box you can drag (up/down) to change the value, like the knob (#2).
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // Focused text fields tagged with a "glow" property get the combo's subtle
    // accent micro-glow instead of a plain hard outline (#11).
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    // Uniform, compact font for every combo + its pop-up list (#13).
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    // Drop the combo pop-up BELOW the box (target its screen bounds) instead of the JUCE default,
    // which covers the box with the currently-selected item under the cursor. Restores the expected
    // drop-down position. (#combo)
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox&, juce::Label&) override;

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

// ----------------------------------------------------------------------------
//  A control that holds a host change GESTURE open across a mouse press, and can
//  be told to abandon it.
//
//  The value box behind every knob opens a juce::Slider::ScopedDragNotification
//  on mouseDown and closes it on mouseUp, so a drag records one undo step and
//  one host touch/latch span (ADR-0008, KI-010's class). If the release never
//  arrives -- released over the host window or the desktop, where the OS
//  delivers it to no JUCE peer at all -- the gesture stays open, and
//  pollUndoCoalesce commits nothing while openGestures > 0.
//
//  The editor already runs a release-outside reconcile on its timer and already
//  computes the "a button is logically down but physically up" predicate once
//  per tick. This interface is the only thing it was missing: a NAMED way to
//  reach a control that lives in an unnamed namespace in LookAndFeel.cpp. No
//  pointer is retained between ticks and the editor cannot outlive its own
//  descendants, so the call needs no lifetime contract beyond ordinary
//  parent-child ownership.
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//  ONE SOURCE FOR A NOTCH'S ARITHMETIC (ADR-0053, round 11)
//
//  JUCE's own handler IS the definition of what one wheel notch does to a
//  slider -- `Slider::Pimpl::getMouseWheelDelta` and `Slider::Pimpl::
//  mouseWheelMove` -- and a scroll with no button held reaches it directly. A
//  notch delivered DURING a drag cannot: JUCE's whole wheel body sits behind
//  `! e.mods.isAnyMouseButtonDown()`, so the two in-drag paths (the knob's own,
//  and the value box taking a notch for the drag it holds) have to restate it.
//  Restated ONCE, here, rather than once per file:
//
//   * direction -- the larger axis wins, X inverted, `isReversed` applied;
//   * scale     -- 0.15 of the slider's PROPORTION per wheel unit;
//   * the rails -- clamped to [0, 1]. JUCE's other arm wraps a rotary slider
//     around instead, but only when `rotaryParams.stopAtEnd` is false, and
//     nothing in this plug-in ever calls `setRotaryParameters` (JUCE's own
//     default is true), so that arm is unreachable here;
//   * THE FLOOR -- at least one `getInterval()` of VALUE movement. This is the
//     half the in-drag paths did not have, and the whole of round 11's first
//     finding: `SliderParameterAttachment` copies the parameter's interval onto
//     the slider (`newRange.interval = range.interval`, juce_ParameterAttachments
//     .cpp), and almost every parameter here declares one -- 0.001 for Amount,
//     Width and the rest, 0.01 for Drive and the gains -- so a macOS trackpad's
//     smallest precise notch (deltaY = 0.5/256) asked for 0.0003 of travel,
//     `Slider::setValue` snapped it straight back to the value it already held,
//     and the notch did nothing at all. With no button held the same notch moves
//     one interval, because JUCE floors it. Same physical input, two answers.
//
//  `Slider::snapValue` is NOT restated: JUCE calls it on this path, its base
//  implementation returns the value unchanged, and nothing in this plug-in
//  overrides it. The duplicate-event filter (`e.eventTime != lastMouseWheelTime`)
//  IS restated, but in the two callers rather than here, because it is state and
//  this is a pure question. It is restated at all because the floor makes it
//  load-bearing: JUCE's own reason for having it is "since we're going to bump
//  the value by a minimum of the interval, avoid doing this twice", and it is
//  about two DISTINCT events sharing a timestamp rather than one event delivered
//  twice -- so the single delivery this plug-in's routing does guarantee is not
//  an answer to it. Round 11's first version of this comment said it was.
//
//  Returns the slider's CURRENT value when the notch would move nothing, so a
//  caller has one thing to test before it does anything with side effects
//  (ADR-0052).
// ----------------------------------------------------------------------------
// (`valueToProportionOfLength` and its inverse are non-const in JUCE -- both are virtual hooks a
// subclass may implement against its own state -- so the slider is taken by mutable reference even
// though nothing here writes it.)
// ADR-0047, in the small: the caller has already READ the slider to have something to compare the
// answer with, so the reading is passed IN rather than taken again here. Message-thread-confined
// state and nothing dispatching between the two, so a second read could not misbehave today -- but
// "the reading that plans is the reading that proves" is the rule this repository applies to every
// other pass, and an idiom that merely cannot misbehave yet is one refactor from being one that can.
inline double wheelTargetValue (juce::Slider& s, const juce::MouseWheelDetails& w, double v0)
{
    const double amount = (std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY)
                        * (w.isReversed ? -1.0 : 1.0);
    const double base  = s.valueToProportionOfLength (v0);
    const double want  = juce::jlimit (0.0, 1.0, base + amount * 0.15);
    const double delta = s.proportionOfLengthToValue (want) - v0;
    if (juce::approximatelyEqual (delta, 0.0)) return v0;
    return v0 + juce::jmax (s.getInterval(), std::abs (delta)) * (delta < 0.0 ? -1.0 : 1.0);
}

struct DragGestureOwner
{
    virtual ~DragGestureOwner() = default;

    // Close any gesture this control is holding, as if the release had arrived.
    // Must be idempotent: the reconcile calls it on every candidate, every tick.
    virtual void abortDragGesture() = 0;

    // ADR-0053. Take one wheel notch on behalf of a drag this control is HOLDING, and say whether
    // it was taken. The parent asks before handling a notch itself, because JUCE routes a wheel
    // event to the component under the POINTER and a value-box drag maps 180 px of travel across a
    // box under 20 px tall -- so the cursor leaves the box almost immediately and the rest of that
    // drag's notches are delivered to the parent. Writing the value there is not enough: a control
    // that steers an anchor of its own recomputes from it on the next drag event and erases
    // anything written behind its back, so the notch has to reach the anchor. Returning false means
    // "not mine" and the caller handles the event as it otherwise would; an implementation must
    // never forward the event onward from here, or the parent's ask would come straight back.
    virtual bool takeWheelNotch (const juce::MouseEvent&, const juce::MouseWheelDetails&)
    { return false; }
};

} // namespace anamorph::gui
