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
//  `Slider::snapValue` is NOT restated, and since round 14 that needs a sentence rather than a
//  clause: `Knob` DOES override it, to fold a held press's banked notch total into the value JUCE's
//  own drag is about to write (ADR-0053). This path never reaches that override -- the two wheel
//  branches call `Slider::setValue` directly, which does not consult `snapValue`, and the override
//  returns the base answer whenever no notch is banked. So the base implementation is still what
//  this question would get, and restating it would still be restating the identity. The duplicate-event filter (`e.eventTime != lastMouseWheelTime`)
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
// ----------------------------------------------------------------------------
//  THE WHEEL'S ONE AXIS RULE, IN ONE PLACE (ADR-0053, round 29).
//
//  A wheel event carries TWO axes, and a trackpad delivers a horizontal two-finger gesture in
//  `deltaX` alone. The rule for turning the pair into the single "more or less" a one-dimensional
//  control needs is **the dominant axis**: whichever of the two has the larger magnitude decides,
//  with `deltaX` negated so a rightward gesture reads as the same direction a downward vertical one
//  does. This is not invented here -- it is JUCE's own rule, spelled exactly this way in
//  `juce::Slider::Pimpl::mouseWheelMove` (juce_Slider.cpp), which is the behaviour every knob and
//  slider in this editor has always had by inheriting it.
//
//  IT IS A FUNCTION RATHER THAN A LINE COPIED TWICE, and that is the whole of Devin's
//  `src/gui/SpectrumImager.cpp:3293` finding. The imager's handler opened with
//  `wheel.deltaY * (isReversed ? -1 : 1)` and read `deltaX` NOWHERE, so a horizontal trackpad
//  gesture moved every knob in the editor and did nothing at all over the MultiBand display -- the
//  same physical gesture, two answers, because the rule lived in two places and only one of them
//  was ever extended. One spelling, two callers, and a third caller cannot drift from it.
inline double wheelDominantDelta (const juce::MouseWheelDetails& w) noexcept
{
    return (std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY)
         * (w.isReversed ? -1.0 : 1.0);
}

inline double wheelTargetValue (juce::Slider& s, const juce::MouseWheelDetails& w, double v0)
{
    const double amount = wheelDominantDelta (w);
    const double base  = s.valueToProportionOfLength (v0);
    const double want  = juce::jlimit (0.0, 1.0, base + amount * 0.15);
    const double delta = s.proportionOfLengthToValue (want) - v0;
    if (juce::approximatelyEqual (delta, 0.0)) return v0;
    return v0 + juce::jmax (s.getInterval(), std::abs (delta)) * (delta < 0.0 ? -1.0 : 1.0);
}

// ----------------------------------------------------------------------------
//  A CONTROL THAT CAN TAKE A WHEEL NOTCH ON BEHALF OF A PRESS IT IS HOLDING (ADR-0053).
//
//  Split out of `DragGestureOwner` in round 29 so that the three control families that hold
//  presses -- the knob, the value box under it, and the multiband display -- can all be wheel
//  targets without every one of them also having to be an abortable gesture holder. The abort half
//  is a reconcile for a release that never arrived (KI-028) and has exactly the implementers it
//  always had; this half is about routing.
// ----------------------------------------------------------------------------
struct WheelDragOwner
{
    virtual ~WheelDragOwner() = default;

    // ADR-0053. Take one wheel notch on behalf of a drag this control is HOLDING, and say whether
    // it was taken. JUCE routes a wheel event to the component under the POINTER and never to the
    // one holding the press (`MouseInputSourceImpl::getTargetForGesture` is a bare
    // `getComponentAt`), so the notches of any drag that has carried the cursor off its own control
    // are delivered somewhere else entirely. Writing the value there is not enough: a control that
    // steers an anchor of its own recomputes from it on the next drag event and erases anything
    // written behind its back, so the notch has to reach the anchor. Returning false means "nothing
    // to add it to" -- the press exists but holds no value, like the multiband's pending delete
    // click -- and an implementation must never forward the event onward from here, or the ask
    // would come straight back.
    //
    // ROUND 30: `false` no longer means the event is free. It means only that this press had
    // nothing to add the notch to; the press still owns the event, and `wheelTakenByAnyPress`
    // consumes it either way. The return value is now read by nobody but the tests, and is kept
    // because "did this press take it" is the question each implementation answers and a silent
    // `void` would make every one of them look like it had.
    virtual bool takeWheelNotch (const juce::MouseEvent&, const juce::MouseWheelDetails&)
    { return false; }
};

struct DragGestureOwner : WheelDragOwner
{
    // Close any gesture this control is holding, as if the release had arrived.
    // Must be idempotent: the reconcile calls it on every candidate, every tick.
    virtual void abortDragGesture() = 0;
};

// ----------------------------------------------------------------------------
//  THE WHEEL BELONGS TO THE PRESS, NOT TO THE POINTER (ADR-0053, round 29 -- owner decision).
//
//  JUCE hit-tests the pointer for every wheel event, with no regard for a drag in flight:
//  `handleWheel` takes its target from `getTargetForGesture`, which ends in
//  `peer.getComponent().getComponentAt (pos)` (juce_MouseInputSourceImpl.h). So a knob drag that
//  has carried the cursor onto a neighbour delivers its notches to the NEIGHBOUR -- and until this
//  round the neighbour acted on them, deliberately (the knob laundered the held button out of the
//  event so JUCE's own `! isAnyMouseButtonDown()` gate would pass). The approved behaviour is the
//  opposite one: while a button is held, the wheel steers the control that owns the press and no
//  other control may be touched by it.
//
//  ONE REGISTER, CONSULTED BY EVERY WHEEL HANDLER, rather than a rule restated per class: there is
//  no ancestor every wheel event passes through (the editor's own handler only sees what a child
//  declined), so a per-class rule is a rule three classes can drift on. The register is a single
//  message-thread cell because a mouse has one press at a time; it is held as a `SafePointer` so a
//  control destroyed mid-press cannot be reached through it, and it self-clears the moment a wheel
//  arrives with no button down, so a lost `mouseUp` cannot strand it.
// ----------------------------------------------------------------------------
//  A PRESS THAT DECLINES THE NOTCH STILL OWNS IT (round 30, Devin
//  `src/gui/SpectrumImager.cpp:R3302-3304` -- owner decision). Round 29 asked only "does some OTHER
//  control hold the press", which left two answers identical that are not the same question:
//
//      no active press owns this event                      -> the pointer decides, as it always has
//      an active press owns it and has NO editable target    -> the press decides, and decides nothing
//
//  The second is an Alt-click reset held down, a press on the multiband display's blank area, an add
//  the band count refused, a click that started no drag. Each of those OWNS the interaction; none of
//  them holds a value a notch can be added to. Round 29 let every one of them fall through to the
//  pointer-targeted path -- `standaloneWheel`, or JUCE's own handler with the held button laundered
//  out -- so the control under the cursor moved while the user was holding something else. The rule
//  the owner approved is that the press decides until the button comes up, whether or not it has
//  anything to do with the notch, so this function answers the whole question in one place and the
//  callers have nothing left to decide.
//  ...AND "THE PRESS" IS ONE PER POINTING DEVICE, not one per process (round 30, Devin
//  `src/gui/LookAndFeel.cpp:R16`). JUCE does not model a single pointer: `MouseInputSourceList`
//  holds an array of them and hands each its own `MouseInputSourceImpl`, whose `buttonState` is
//  private to that device -- `getCurrentModifiers()` is the global modifiers with the mouse
//  buttons stripped and THAT device's buttons put back (juce_MouseInputSourceImpl.h:59-64). So the
//  button-down test below already answers per device; only the register was process-wide, and a
//  second device pressing anything would evict a claim it had nothing to do with.
//
//  That is reachable where Anamorph ships, and the envelope is exact:
//    * macOS -- NOT reachable. `MouseInputSourceList::canUseTouch()` is `false` and `addSource()`
//      refuses every index past 0 (juce_NSViewComponentPeer_mac.mm:2986-2999), so the process has
//      exactly one source for its whole life.
//    * Linux/BSD -- REACHABLE, with no opt-out. `XWindowSystem::canUseMultiTouch()` is true
//      whenever XI2 sets up (juce_XWindowSystem_linux.cpp:2299-2306, and `JUCE_USE_XINPUT`
//      defaults to 1), every window JUCE creates masks XI_TouchBegin/Update/End unconditionally
//      (:676-678), and a touch dispatches as `InputSourceType::touch` with a per-finger index
//      (:4176-4189) alongside the live `mouse` source. A finger on one control while the mouse
//      holds another is an ordinary state there.
//    * Windows -- a second source is created (a synthesised touch or pen message is still typed
//      from `GetMessageExtraInfo()`, and `doMouseDown`'s early return is gated on
//      `canUseMultiTouch()`, juce_Windowing_windows.cpp:2606-2613), but the editor never opts into
//      real multi-touch: `AudioProcessorEditor::usesWindowsMultiTouch()` returns false
//      (juce_AudioProcessorEditor.cpp:260-263) and nothing here overrides it, so `RegisterTouchWindow`
//      is never called and the OS synthesises ONE cursor. Concurrency is not established there;
//      the per-device cell is simply correct rather than needed.
//
//  Keyed by (type, index) because that pair IS the identity JUCE itself uses:
//  `getOrCreateMouseInputSource` matches a mouse or pen on type alone and a touch on type and
//  finger number (juce_MouseInputSourceList.h:67-89). Naming the key as a value rather than
//  reaching into `e.source` inside the register is also what makes the routing testable: nothing
//  public creates a second `MouseInputSource`, so a test drives the overloads below directly.
struct WheelPointer
{
    int type  = (int) juce::MouseInputSource::InputSourceType::mouse;
    int index = 0;

    bool operator== (const WheelPointer& o) const noexcept { return type == o.type && index == o.index; }
    bool operator!= (const WheelPointer& o) const noexcept { return ! operator== (o); }
};

inline WheelPointer wheelPointerOf (const juce::MouseInputSource& s) noexcept
{
    return { (int) s.getType(), s.getIndex() };
}

// TRUE when this press now owns the component's drag, FALSE when another device already does
// (round 33, Devin `src/gui/LookAndFeel.cpp:R101-103`). A caller MUST ask before it writes any of
// its own drag state: refusing the wheel claim alone left the rejected press free to run the rest
// of its `mouseDown` and replace the anchor the first device is dragging from. `[[nodiscard]]` so
// that forgetting to ask is a compile error rather than a silent second owner.
//
// The device that already holds the component is granted it again, so a duplicate or re-entrant
// `mouseDown` from the owner is not rejected as its own rival.
[[nodiscard]] bool claimDragWheel (juce::Component&, WheelDragOwner&, WheelPointer);

// THE COMPONENT'S SHARED DRAG HAS ENDED (round 32, owner decision; Devin
// `src/gui/LookAndFeel.cpp:R106-110`). One component holds ONE drag -- one `valueOnMouseDown`, one
// anchor, one `sliderBeingDragged` that `sendDragEnd` puts back to -1 on the first release -- so a
// release is a statement about the CONTROL, not about the device that made it, and nothing may
// still be claiming a control whose drag is over. `claimDragWheel` refuses to make a second claim
// on a component that already has one, so there is at most one cell to clear; clearing by component
// is the statement of that invariant rather than a scan that hopes to find only one.
//
// ROUND 31 KEYED THIS PER DEVICE and that was the mismatch seen from the wrong end: it kept a
// second device's claim alive past the release that ended the component's only drag, and that claim
// then steered the component's NEXT drag on behalf of a device that was not making it.
//
// It serves the event paths and the two event-less lost-release safety nets
// (`SpectrumImager::cancelActiveDrag`, `ValueBox::abortDragGesture`) alike: they say the same
// thing, which is that this control's gesture is over.
void releaseDragWheel (const juce::Component&);

juce::Component* dragWheelHolder (WheelPointer) noexcept;

// TRUE when a DIFFERENT pointing device is holding this component's drag (round 33, Devin
// `src/gui/LookAndFeel.cpp:R101-103` -- the half of that press which is not its `mouseDown`).
// `claimDragWheel` answers the same question at the press and writes the cell; this is the
// read-only twin the press's OTHER events ask, because a press is refused for its whole life and
// not merely at its first event:
//
//   * `juce::Slider::Pimpl::mouseDrag` runs on the OWNER's `useDragEvents` and `sliderBeingDragged`
//     and writes the parameter from whatever cursor it is handed (juce_Slider.cpp:906-970);
//   * `Pimpl::mouseUp` ends with an unconditional `currentDrag.reset()` (:997), so a refused
//     release closed the owner's host change gesture and set `sliderBeingDragged` back to -1;
//   * `SpectrumImager::mouseUp` fires the ON-RELEASE ACTIONS the owner's press latched -- a solo
//     toggle, a band removal -- and both double-click handlers write a parameter outright.
//
// ASKED, NOT REMEMBERED. A flag set by the refused `mouseDown` would be per (component, device)
// state -- the thing the owner's decision forbids -- and would have to be cleared on paths that do
// not always run. The register already holds the answer.
//
// AND IT IS "SOMEONE ELSE HOLDS IT", NOT "I HOLD IT". The two differ exactly where the safety nets
// live: a claim emptied while the button is still down (KI-028's self-heal, a destroyed holder)
// leaves NOBODY holding the component, and the component's own release must still run -- so the
// guard stands down when the cell is empty and speaks only when a rival is named.
[[nodiscard]] bool dragWheelHeldByOther (const juce::Component&, WheelPointer) noexcept;

// True when this event has been dealt with and the caller must do nothing whatsoever with it --
// not act on it, and not pass it on. That is every case in which a mouse button is down ON THE
// DEVICE THAT SENT IT:
//   * the press belongs to some other control  -> the notch is posted to that control;
//   * the press belongs to THIS control        -> `selfOwner` is offered the notch here;
//   * a button is down but nothing claimed     -> nobody is offered it, and nobody may have it.
// `selfOwner` may be null for a component that holds no presses of its own (the editor's backstop).
// False means this device has no button down, which is the only state in which the pointer decides.
bool wheelTakenByAnyPress (juce::Component& self, WheelDragOwner* selfOwner,
                           const juce::MouseEvent&, const juce::MouseWheelDetails&, WheelPointer);

inline bool wheelTakenByAnyPress (juce::Component& self, WheelDragOwner* selfOwner,
                                  const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    return wheelTakenByAnyPress (self, selfOwner, e, w, wheelPointerOf (e.source));
}

} // namespace anamorph::gui
