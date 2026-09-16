#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "ParameterDispatch.h"  // ADR-0036 round 27 (R1390): the attachment straddle raises the dispatch depth
#include "gui/LookAndFeel.h"
#include "gui/Vectorscope.h"
#include "gui/SpectrumImager.h"
#include "gui/CorrelationMeter.h"
#include "gui/LevelMeter.h"

// ============================================================================
//  AnamorphAudioProcessorEditor  (v0.3 UI pass)
// ============================================================================
// ---- Which component should provide the tooltip text this tick -------------------------------
//
// THE DEFECT. JUCE's tooltip tick reads TWO DIFFERENT SOURCES OF TRUTH and mixes them
// (juce_TooltipWindow.cpp:209, :221, :223):
//
//     newComp  = Desktop::getMainMouseSource().getComponentUnderMouse();   // the TEXT comes from here
//     mousePos = Desktop::getMainMouseSource().getScreenPosition();        // the BOX POSITION from here
//
// The two are on DIFFERENT CLOCKS. `getScreenPosition()` is LIVE: it asks the OS on every call
// (getRawScreenPosition -> MouseInputSource::getCurrentRawMousePosition,
// juce_MouseInputSourceImpl.h:96-101), and Desktop::getMousePosition() is exactly that same value
// (juce_Desktop.cpp:167-173). `getComponentUnderMouse()` is a CACHE
// (juce_MouseInputSourceImpl.h:54-56), and the only thing that writes it is
//
//     setComponentUnderMouse (findComponentAt (lastPointerState.position, lastPeer))
//
// -- the LAST EVENT's position against the LAST peer (:247-270, :293-297).
//
// AND THAT WRITE IS NOT ONLY DRIVEN BY OS EVENTS, which is the part that ties the defect to
// REPOSITIONING and was got wrong at first. `TooltipWindow::updatePosition` is `setBounds()` then
// `setVisible (true)` (juce_TooltipWindow.cpp:92-96), and BOTH of those call
// `Component::sendFakeMouseMove()` (juce_Component.cpp:1105 and :559) -> `triggerFakeMove()` ->
// `handleAsyncUpdate()` -> `setPointerState (lastPointerState, ..., forceUpdate = true)`
// (juce_MouseInputSourceImpl.h:453-462, :292-297). So EVERY show, move or hide of the box
// re-derives "what is under the mouse" -- from the last event's position, never from the live one.
//
// That is the mechanism: the reposition itself rewrites the text's source at a position the
// pointer has already left, while the box is placed where the pointer actually is. The box lands
// where you are and is labelled with where you were, and since the rewrite is driven by the
// tooltip's own geometry rather than by the mouse, nothing corrects it until the pointer moves
// again -- one more pixel delivers an event and the right tip returns. Persistence and one-pixel
// recovery both fall out of it, and none of it is platform-specific: it is shared JUCE code.
//
// Reproduced deterministically 5 times out of 5 by driving the two clocks apart directly, and 0/5
// after this fix; confirmed on macOS by the reporter. What is NOT claimed is a traced account of
// why the rewrite preferentially lands on the row ABOVE. See docs/DOCUMENTATION_COVERAGE.md
// (tooltip source-of-truth round) for the geometric fit that accounts for all four observed
// controls, and for the limits of it.
//
// THE FIX. Before trusting the cached component, check it against the LIVE pointer -- the same
// position JUCE is about to place the box at. If it holds, the cached component is used exactly as
// before, which is the overwhelmingly common case. If it does not, what is really under the pointer
// is used instead; a null answer means no tip, exactly as an empty tip does today. Note this does
// not repair JUCE's cache -- it cannot, the cache is private -- it re-derives the answer at the one
// place this editor controls. That is sufficient because the tooltip is the only consumer of
// getComponentUnderMouse() in this tree.
//
// Kept as a pure function of (cached component, live pointer, component really under it) so the
// decision is regression-tested headlessly in AnamorphStateTests -- no display, no editor, no
// tooltip window.
struct TooltipSource
{
    static juce::Component* choose (juce::Component* cached,
                                    juce::Point<int> livePointer,
                                    juce::Component* underLivePointer) noexcept
    {
        if (cached != nullptr && cached->getScreenBounds().contains (livePointer))
            return cached;

        return underLivePointer;
    }
};

class AnamorphAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer,
                                     private juce::ComponentListener // pop-up windows, see PopupShield
{
public:
    explicit AnamorphAudioProcessorEditor (AnamorphAudioProcessor&);
    ~AnamorphAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    // The host reports its display/DPI scale here (Windows hosts call this). We
    // COMPOSE it with the user UI-scale rather than let JUCE's default overwrite our
    // transform -- that overwrite is what made the window open at the wrong size and
    // ignore the UI-Scale combo on some Windows hosts (Mac uses backing scale, so
    // it never hit this).
    void setScaleFactor (float newScale) override;

    // Close any host change gesture a value box is still holding from a press
    // whose release was never delivered (KI-028). Called from the release-outside
    // reconcile on the editor timer, under the predicate that already decides a
    // button is logically down but physically up. Public and separately callable
    // so the sweep can be tested without synthesising OS-level button state --
    // the predicate itself is pre-existing, shipped since v0.8.12. Its macOS
    // limitation (KI-013) was closed in round 4: the predicate now reads the
    // physical buttons through anamorph::gui::anyPhysicalMouseButtonDown().
    void abortAbandonedDragGestures();

    // The Save-preset overlay, opened from the preset menu and closed by Cancel, Escape, a click
    // on the backdrop or a completed save. PUBLIC for the same reason `abortAbandonedDragGestures`
    // above is: the round-28b regression for Devin R405-406 has to open a dialog, cancel it and
    // open another one, and the production route to it is a `juce::PopupMenu` item -- an
    // asynchronous, windowed thing a headless suite cannot drive. Nothing else changes: the
    // function is the same one the menu item calls, and every rule about save-attempt identity
    // lives inside it (see `saveAttempt` below).
    void showSavePreset (bool);

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // ========================================================================================
    //  ADR-0008, ROUND 18. THE CONTROL THAT WROTE THE PARAMETER SAYS WHAT IT WROTE.
    // ========================================================================================
    //  A JUCE parameter attachment declares OWNERSHIP when its gesture opens and declares no VALUE
    //  at all, so until this class existed the undo batch had nothing to close a step with except a
    //  live read of the parameter at the gesture close. A host write landing after the user's last
    //  attachment write and before that close therefore became the value Redo restored -- the user's
    //  own value recoverable from neither end of the step (State test 91 legs A, B, D, E, G).
    //
    //  WHAT IS HARD ABOUT IT, stated rather than implied: a host write reaches the control's
    //  value-changed callbacks EXACTLY as a user write does. `SliderParameterAttachment::setValue`
    //  pushes the host's value in with `sendNotificationSync`, and the `ignoreCallbacks` flag that
    //  suppresses the echo is private to JUCE's own attachment with no accessor -- so
    //  `Slider::valueChanged`, `Slider::Listener::sliderValueChanged` and `Slider::onValueChange`
    //  all fire for both, and the value alone cannot tell them apart. `Slider::snapValue` CAN
    //  (JUCE reaches it only from user input), but its coverage hole is categorical: it is declared
    //  on `juce::Slider` alone, so it can see no Button and no ComboBox write, and four of this
    //  editor's own user-write sites call `Slider::setValue` directly and never consult it.
    //
    //  SO THE DISCRIMINATOR IS NOT THE VALUE, IT IS WHO MOVED THE PARAMETER. Two hooks per control,
    //  one registered BEFORE JUCE's attachment and one AFTER it -- `juce::ListenerList` dispatches
    //  in registration order, and the attachment writes the parameter inside its own callback, so
    //  the pair straddles the write:
    //
    //      user   : before = P_old ... attachment writes P ... after = P_new   -> P MOVED, record
    //      host   : before = P_host ... attachment suppresses itself ...       -> P did not move
    //
    //  A host push writes the parameter FIRST and only then sets the control, so the parameter
    //  cannot move during the control's own notification; a user write is the only thing that can.
    //  A user write that lands on the value the parameter already holds moves nothing either, and
    //  JUCE skips it outright (`ParameterAttachment::callIfParameterValueChanged`) -- correctly, as
    //  it produced nothing to record.
    //
    //  WHAT IS RECORDED IS WHAT THE CONTROL ASKED FOR, not a second reading of the parameter: a
    //  host answering the write re-entrantly would be in the live value by then, which is the same
    //  reason `SpectrumImager::storeOwned` passes its own installed value (round 15).
    //
    //  THREADING: message thread only, inside the user's own event handler, on the same synchronous
    //  stack as the write. No timer, no lock, no `callAsync`, and nothing added to
    //  `parameterValueChanged` -- the audio-thread-reachable path ADR-0036 forbids.
    struct AttachmentWitness
    {
        AttachmentWitness (AnamorphAudioProcessor& p, juce::RangedAudioParameter& rp)
            : proc (p), param (rp), before (*this, false), after (*this, true) {}

        // ADR-0036 ROUND 27 (Devin R1390). THE ONE PARAMETER-DISPATCH BRACKET IN THE TREE THAT IS
        // NOT A SINGLE SCOPE, because the thing it brackets is not a call this editor makes:
        // JUCE's own attachment writes the parameter from inside its listener callback, and this
        // witness already STRADDLES that write -- `before` is registered ahead of the attachment
        // and `after` behind it (`juce::ListenerList` dispatches in registration order). So the
        // raise belongs to the BEFORE hook and the lower to the AFTER one, and the pair covers
        // `beginChangeGesture` (from `sliderDragStarted`), the value write (from
        // `sliderValueChanged` / `buttonClicked` / `comboBoxChanged`) and `endChangeGesture` (from
        // `sliderDragEnded`) -- every dispatch a JUCE attachment can start.
        //
        // WHY A LEAK IS NOT POSSIBLE, which is the question a two-callback bracket has to answer.
        // `juce::Slider` dispatches through `listeners.callChecked` with a `BailOutChecker` on the
        // control, so if the control is DELETED from inside the attachment's callback -- which a
        // host pumping its message loop there really can do, by closing the editor -- the AFTER
        // hook is never reached. A leaked raise would make `flushDeferredCommands` refuse forever,
        // which is the "strand indefinitely" failure the mechanism exists to prevent. So the
        // witness counts its own raises and unwinds them in its destructor: the witnesses are
        // members of the editor and the controls they watch are members too, so a control that
        // dies takes its witness with it and the count returns to zero. `raised` is a count and
        // not a flag because a nested notification can straddle a straddle.
        struct ScopedStraddle
        {
            ScopedStraddle (AttachmentWitness& o, bool isAfter) : owner (o), post (isAfter)
            { if (! post) owner.raiseDispatch(); }
            ~ScopedStraddle() { if (post) owner.lowerDispatch(); }
            ScopedStraddle (const ScopedStraddle&)            = delete;
            ScopedStraddle& operator= (const ScopedStraddle&) = delete;
            AttachmentWitness& owner;
            const bool         post;
        };

        // One object serves all three control families: the three JUCE listener interfaces have
        // distinct method names, so there is nothing to disambiguate.
        struct Hook final : juce::Slider::Listener,
                            juce::Button::Listener,
                            juce::ComboBox::Listener
        {
            Hook (AttachmentWitness& o, bool isAfter) : owner (o), post (isAfter) {}
            void sliderValueChanged (juce::Slider* s) override
            {
                const ScopedStraddle straddle (owner, post);
                owner.mark (post, owner.param.convertTo0to1 ((float) s->getValue()));
            }
            // ROUND 22, RISK-012. A PRESS THAT PRODUCES NOTHING SAYS SO. JUCE opens the change
            // gesture from `SliderParameterAttachment::sliderDragStarted` and closes it from
            // `sliderDragEnded`, so a press that never moves the slider -- a click on a knob or a
            // value box with no drag -- brackets a gesture around no write at all. The batch close
            // then had nothing declared to prefer and fell back to a LIVE READ, which is whatever
            // host automation left in the parameter during the press: a value the user never
            // produced, becoming that press's Undo/Redo endpoint (ADR-0008). Only the BEFORE hook
            // acts, because `juce::ListenerList` dispatches in registration order and this hook is
            // registered ahead of JUCE's attachment -- so the refusal is recorded before
            // `endChangeGesture` runs, which is where the close reads it.
            void sliderDragStarted (juce::Slider*) override
            {
                const ScopedStraddle straddle (owner, post);
                if (! post) owner.pressProduced = false;
            }
            void sliderDragEnded   (juce::Slider*) override
            {
                const ScopedStraddle straddle (owner, post);
                if (! post) owner.notePressEnded();
            }
            void buttonClicked (juce::Button* b) override
            {
                const ScopedStraddle straddle (owner, post);
                owner.mark (post, owner.param.convertTo0to1 (b->getToggleState() ? 1.0f : 0.0f));
            }
            void comboBoxChanged (juce::ComboBox* c) override
            {
                const ScopedStraddle straddle (owner, post);
                // The arithmetic JUCE's own `ComboBoxParameterAttachment::comboBoxChanged` does.
                const int n = c->getNumItems();
                const float raw = n > 1 ? (float) c->getSelectedItemIndex() / (float) (n - 1) : 0.0f;
                owner.mark (post, owner.param.convertTo0to1 (owner.param.convertFrom0to1 (raw)));
            }
            AttachmentWitness& owner;
            const bool post;
        };

        void mark (bool post, float produced) noexcept
        {
            if (! post)
            {
                wasNorm = param.getValue();
                // ROUND 20: SAY WHAT THIS CONTROL IS ABOUT TO ASK FOR, BEFORE THE ATTACHMENT RUNS.
                // For a ComboBox or a Button the attachment opens, writes and CLOSES the gesture in
                // its own callback, so the close -- which is where the batch becomes pollable --
                // happens before `after` below can state anything. The request is what the close
                // reads instead of the live parameter. It claims nothing: no ownership, no episode
                // bit, no step; a host push arms it and disarms it with no gesture in between.
                prevRequest = proc.noteAttachmentRequest (&param, produced);
                return;
            }
            // ...and hand the previous request back before anything else, so this runs even for the
            // host push that returns below.
            proc.restoreAttachmentRequest (prevRequest);
            prevRequest = {};
            if (juce::exactlyEqual (param.getValue(), wasNorm)) return;  // the host pushed IN
            proc.noteOwnedParamEndpoint (&param, produced);
            pressProduced = true;   // ...and this press has an endpoint of its own (round 22)
        }

        // The other half of the round-22 rule above: the press is over and nothing this control
        // did moved the parameter, so it has NO endpoint to state and the close must not invent
        // one. `noteOwnedParamRefused` is exactly that sentence -- it suppresses the live read and
        // leaves `after` where `noteFirstOwnership` seeded it, which is `before`, which is how the
        // poll comes to record no step for a press that did nothing (ADR-0008, ADR-0053).
        void notePressEnded() noexcept
        {
            if (! pressProduced) proc.noteOwnedParamRefused (&param);
            pressProduced = false;
        }

        // The order is the mechanism: `listenBefore` runs before JUCE's attachment is constructed
        // and `listenAfter` after it, so the pair straddles the attachment's own callback.
        template <typename Control> void listenBefore (Control& c)
        {
            unhook = [this, &c] { c.removeListener (&before); c.removeListener (&after); };
            c.addListener (&before);
        }
        template <typename Control> void listenAfter (Control& c)
        { c.addListener (&after); straddleArmed = true; }

        // The raise/lower pair `ScopedStraddle` drives, and the count that makes it unwindable.
        //
        // ARMED ONLY ONCE BOTH HOOKS ARE ON THE CONTROL, and that guard is not a nicety: it was
        // found by this round's own suite. `attachSlider` registers `before`, CONSTRUCTS the JUCE
        // attachment, then registers `after` -- and the attachment's constructor calls
        // `ParameterAttachment::sendInitialUpdate()`, which pushes the parameter's value into the
        // control and therefore fires `sliderValueChanged` while only `before` is listening. That
        // raise has no `after` to lower it. Measured: the dispatch depth stood at 14 -- one per
        // parameter-backed control -- from the moment the editor finished constructing, so every
        // deferred command in State tests 99 and 100 refused forever. The initial update writes no
        // parameter (it is the host->control direction, and JUCE suppresses the echo), so there is
        // nothing to bracket there and not raising is the correct answer as well as the safe one.
        void raiseDispatch() noexcept
        { if (! straddleArmed) return; ++raised; anamorph::param::enterDispatch(); }
        void lowerDispatch() noexcept { if (raised > 0) { --raised; anamorph::param::exitDispatch(); } }

        // Both hooks come off the control while the control is still alive: the witnesses are
        // declared after every control they watch, so they are destroyed first. ROUND 27: and any
        // raise this witness still holds comes off here, so a control deleted from inside its own
        // notification cannot leave the dispatch depth standing (see `ScopedStraddle`).
        ~AttachmentWitness() { while (raised > 0) lowerDispatch(); if (unhook) unhook(); }

        AnamorphAudioProcessor&     proc;
        juce::RangedAudioParameter& param;
        Hook  before, after;
        float wasNorm = 0.0f;
        // Round 22: did anything this control did move the parameter between its drag start and
        // its drag end? Message thread only, like everything else in this type.
        bool  pressProduced = false;
        // ROUND 27 (R1390): how many dispatch raises this witness is currently holding, and
        // whether the pair that balances them is complete (see `raiseDispatch`).
        int   raised = 0;
        bool  straddleArmed = false;
        AnamorphAudioProcessor::AttachmentRequest prevRequest {};
        std::function<void()> unhook;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AttachmentWitness)
    };

    // Make the witness for `id`, or nullptr when the control is not parameter-backed.
    AttachmentWitness* makeWitness (const char* id);

    // Consumes the click that dismissed a pop-up, so it cannot also act on whatever sits under it.
    //
    // JUCE re-delivers that click on purpose: Component::internalMouseDown sees the modal menu,
    // calls internalModalInputAttempt() -- which dismisses it synchronously -- and then, because
    // the modal loop has now exited, hands the SAME mouse-down to the component underneath
    // (juce_Component.cpp:2507-2544 in the pinned tree; the comment there says so outright).
    // Underneath is whatever the cursor happens to be over, and several of those act on the press
    // itself: ABControl::mouseDown toggles A/B, SpectrumImager::mouseDown can ADD a band, a
    // Backdrop closes its panel and discards what was typed into it.
    //
    // A shield is the whole enforcement layer: raised in front of everything while any pop-up is on
    // screen, it is the component that click lands on, and it does nothing with it. One mechanism,
    // one place, rather than a predicate bolted onto every control that could be hit -- and it
    // covers controls added later for free.
    //
    // It never covers the pop-up itself, and that is structural rather than a matter of ordering.
    // A ComboBox / TextEditor menu is its own desktop window, so the shield (an editor child) is not
    // even in the same hierarchy. The preset menu IS an editor child -- but PopupMenu::MenuWindow
    // sets setAlwaysOnTop (true) in its constructor (juce_PopupMenu.cpp:365), and Component::toFront
    // on a NON-always-on-top component walks its insert index back past every always-on-top sibling
    // (juce_Component.cpp:914-922). The shield does not set that flag, so it cannot be raised in
    // front of a menu even if it is raised while one is already open. Nothing else in src/ sets
    // alwaysOnTop, so a menu window is the only sibling that can outrank it. (showPresetMenu also
    // raises the shield BEFORE showMenuAsync, so the append order agrees with the flag order.)
    //
    // Keyboard focus is deliberately left alone: toFront (false) skips grabKeyboardFocus
    // (juce_Component.cpp:928-934) and setMouseClickGrabsKeyboardFocus (false) covers the click, so
    // raising the shield cannot pull focus out of the Save Preset field mid-edit.
    // It is ALWAYS visible and paints nothing; only its mouse interception is toggled. (dimOverlay is
    // the precedent for the transparent-to-the-mouse half only -- it is a full-editor overlay with
    // setInterceptsMouseClicks (false, false) -- but it is NOT always visible: it is added with
    // addChildComponent and follows the Bypass state.)
    //
    // WHY RAISING THE SHIELD CANNOT DISTURB HOVER -- and it is not the order we raise it in.
    // Every fake mouse move in play here is ASYNCHRONOUS: Component::sendFakeMouseMove ->
    // MouseInputSource::triggerFakeMove -> triggerAsyncUpdate (juce_MouseInputSourceImpl.h:449-451).
    // It is dispatched a message-loop pass later, so it lands after showWithOptionalCallback has run
    // setVisible(true), enterModalState AND toFront on the menu (juce_PopupMenu.cpp:2290-2294) and
    // returned -- our own toFront, and the one JUCE fires from the menu's setVisible, are the same
    // deferred move. Two independent properties make that dispatch a no-op for hover:
    //   1. The menu is modal by then, and Component::internalMouseEnter/internalMouseExit BOTH
    //      early-return for a target that isCurrentlyBlockedByAnotherModalComponent()
    //      (juce_Component.cpp:2414-2420, :2452-2458). MenuWindow does not override
    //      canModalEventBeSentToComponent, so every editor child -- the control under the cursor and
    //      this shield alike -- is blocked (juce_ComponentHelpers.h:213-219). No mouseExit/mouseEnter
    //      is delivered, so the only two event-driven hover consumers in src/ (SpectrumImager's hover
    //      indices, ABControl::hovered) cannot be cleared, whatever the hit test resolves to.
    //   2. Every other hover visual here is derived GEOMETRICALLY, never from enter/exit:
    //      stepMicroAnims takes `over` from getMouseXYRelative() to drive hovA (PluginEditor.cpp:
    //      1678-1682) and the combo "hov" flag does the same (:1392-1395). That is the v0.6.1
    //      stuck-hover fix, and it makes hovA immune to componentUnderMouse churn by construction.
    //      It also makes it blind to OCCLUSION, which is the other half and was missing until 0.9.4:
    //      getMouseXYRelative is a pure coordinate transform (juce_Component.cpp:3233-3236), so a
    //      control keeps containing the cursor while a drop-down is stacked over it and lit up with
    //      the pointer provably on the menu. cursorIsOverOpenPopup() supplies the missing term, and
    //      cursorOverlay()/occludes() the other one: a Backdrop covers the editor EXCEPT its own
    //      contents, so unlike a pop-up it cannot be a single per-frame bool (KI-024).
    //      THE PLACEMENT RULE, and it is load-bearing: occlusion is ANDed into the ANSWER (`over`,
    //      `hov`) and NEVER into the GATE (`mouseInside` :1571, `comboCursorInside` :1380). Folding
    //      it into either looks tidier and is wrong: `! mouseInside` is the shared prefix of both
    //      S11 early returns (:1596, :1620), so a gate that goes false the moment a menu opens seals
    //      the driver with hovA parked at 1.0 -- turning a false highlight into a frozen one for the
    //      life of the menu. refreshPopupShield un-settles on the transition for the same reason.
    //      That gate USED to be able to seal on a lit control by itself, from the other direction
    //      (KI-025): `microSettled` only says the last pass moved nothing, which is as true at
    //      hovA 1.0 as at 0.0. It now also asks `microLit`, and stepVal lands on its target instead
    //      of approaching it for ever -- without that second half the first would never let the
    //      gate seal again. Measured: idle stays at 0 passes/s with the cursor outside.
    // Toggling interception rather than visibility is therefore about cost and side effects, not
    // hover: setInterceptsMouseClicks is pure flag assignment (juce_Component.cpp:1336-1341), where
    // setVisible would add a full-editor repaint() on every menu open plus a repaintParent() and a
    // cached-image release on every close (:555-563), for no behavioural gain.
    struct PopupShield : public juce::Component
    {
        PopupShield()
        {
            setInterceptsMouseClicks (false, false); // inert until raised; see refreshPopupShield
            setMouseClickGrabsKeyboardFocus (false);
            setWantsKeyboardFocus (false);
        }
        // Deliberately empty: consuming the event IS the behaviour. The first four only state that
        // intent -- juce::Component's versions are already `{}` (juce_Component.cpp:2310-2314).
        //
        // The last two are the ones that actually do something. Component::mouseWheelMove and
        // ::mouseMagnify are NOT empty in the base class: each forwards the event to the nearest
        // enabled ancestor (:2316-2328), which for this shield is the editor itself. Without these,
        // a scroll or a pinch over a raised shield would arrive at
        // AnamorphAudioProcessorEditor::mouseWheelMove -- harmless today, since the Persistence-reveal
        // branch there keys on `e.eventComponent == &scopePersistK`, but it makes "the shield
        // consumes the gesture" false in a way that only holds by luck. Overriding them costs
        // nothing and makes the contract literal.
        void mouseDown        (const juce::MouseEvent&) override {}
        void mouseUp          (const juce::MouseEvent&) override {}
        void mouseDrag        (const juce::MouseEvent&) override {}
        void mouseDoubleClick (const juce::MouseEvent&) override {}
        void mouseWheelMove   (const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}
        void mouseMagnify     (const juce::MouseEvent&, float) override {}
    };

    // Translucent modal backdrop hosting a centred panel (About / Settings).
    struct Backdrop : public juce::Component
    {
        std::function<void()> onDismiss;
        juce::Rectangle<int>  panel;
        bool   aboutText = false;
        float  reveal = 0.0f;   // 0 = solid, 1 = see-through (Persistence drag, #26)
        bool   dropShadow = false;       // soft feathered outer shadow (Settings, #14)
        bool   lensFlare  = false;       // STATIC anamorphic flare near the top edge (About, #2/#13)
        void paint (juce::Graphics&) override;
        void paintFlare (juce::Graphics&, juce::Rectangle<float> panelF);       // #13
        void paintBrightEdges (juce::Graphics&, juce::Rectangle<float>, float radius); // 0.5.5 About edges (#3)
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (aboutText || ! panel.contains (e.getPosition()))
                if (onDismiss) onDismiss();
        }
    };

    // Bypass dim layer: painted on top, never blocks the mouse (#4 / #8).
    struct DimLayer : public juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0x66090b0e)); }
    };

    // A/B control: shows "A / B" with the active letter bright, the other dim,
    // a single click toggles (FabFilter-style). Wrapped in a racetrack/stadium
    // frame with a micro-gradient + edge glow to match the design language (#6).
    struct ABControl : public juce::Component, public juce::SettableTooltipClient
    {
        std::function<int()>  getActive;
        std::function<void()> onToggle;
        bool hovered = false;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { if (onToggle) onToggle(); }
        void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); } // hover (#10)
        void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
    };

    void timerCallback() override;
    void layoutScopeArea();              // scope + meter block; re-run per frame during the reveal (#6)
    void stepMeterReveal (double dt);    // vsync-driven meter reveal animation (#6/#3)
    void stepMicroAnims (double dt);     // eased hover/press/toggle micro-animations (F3)
    void registerAnimated (juce::Component&);
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override; // Persistence scroll reveal (#1)
    void applyUiScale();                 // whole-window XS..XL transform scale (F4)
    void refreshPresetDisplay();         // preset name + dirty mark (F2)
    void showPresetMenu();
    void focusSaveNameField (int attemptsLeft); // deferred, verified grab (Space-vs-host fix)
    // ADR-0036 round 27 (R640): the Save dialog's PENDING state. A save that could not run
    // synchronously -- one issued from inside a multi-store user transaction -- is queued to a
    // safe boundary, and the dialog must neither close nor claim success until its completion
    // says what happened. While pending the OK button is disabled, so one click is one save.
    void setSavePending (bool pending);
    void showLoadPreset();               // OS file chooser (#3)
    void setupRotary (juce::Slider&, juce::Label&, const juce::String& name, const juce::String& tip);
    void attachSlider (juce::Slider&, const char* id);
    void setupCombo (juce::ComboBox&, const char* id, const juce::String& tip);
    void passComboHoverThrough (juce::ComboBox&); // let hover reach the whole box (recurring)
    void setupToggle (juce::ToggleButton&, const char* id, const juce::String& text, const juce::String& tip);
    // Host-hidden (InternalState) variants: bound via juce::Value, not the APVTS.
    void setupComboInternal (juce::ComboBox&, const juce::StringArray& items, const juce::String& tip, juce::Value);
    void setupToggleInternal (juce::ToggleButton&, const juce::String& text, const juce::String& tip, juce::Value);
    void updateAlgoControls();
    void updateModeVisibility();
    void applyWidenFonts();   // mode-dependent Widen fonts, applied inside resized() so they change in step with the resize (0.6.16 #F)
    void updateMsLabels(); // swap polarity/balance wording between L/R and M/S (#12/#13)
    void showAbout (bool);
    void showSettings (bool);
    void applyTooltipsEnabled();
    void applyScopePersist();

    AnamorphAudioProcessor& processor;
    anamorph::gui::AnamorphLookAndFeel lnf;
    anamorph::gui::CompactComboLookAndFeel compactCombo; // smaller list for Input combos (#12)
    anamorph::gui::SimpleComboLookAndFeel  simpleCombo;  // bigger text for Simple-mode Widen combos (#17)
    juce::OpenGLContext openGLContext;
    // Tooltips are switched off at the SOURCE, not just slowed down. The Settings toggle used to
    // only push millisecondsBeforeTipAppears to a huge value, which does not touch a tip already on
    // screen and -- worse -- is bypassed entirely while one is: TooltipWindow::timerCallback takes a
    // fast path when `isVisible() || now < lastHideTime + 500` and calls showTip() on any tip change
    // without consulting the delay (juce_TooltipWindow.cpp:242-247). That is exactly the reported
    // "disable it and the tip stays, then moving quickly to another control shows a new one".
    //
    // getTipFor is virtual, so returning nothing while disabled makes JUCE's own state machine do
    // the work: the same fast path hides on an empty tip rather than showing one, and the slow path
    // has nothing to show either. One override, no second tooltip system, no timer of our own.
    struct GatedTooltipWindow : public juce::TooltipWindow
    {
        using juce::TooltipWindow::TooltipWindow;
        // NOT named `isEnabled`: juce::Component::isEnabled() is a non-virtual member function
        // (juce_Component.h:1592) that a data member of that name would HIDE in this scope -- and
        // hide silently, since `tooltips.isEnabled()` would still compile and still return a bool,
        // just the wrong one. Empty => behave exactly like juce::TooltipWindow.
        std::function<bool()> tooltipsEnabled;

        // Live hit test, supplied by the editor. Empty => a stale cache yields no tip rather than
        // the wrong one, which is the safe direction.
        std::function<juce::Component*(juce::Point<int>)> componentAt;

        juce::String getTipFor (juce::Component& c) override
        {
            if (tooltipsEnabled && ! tooltipsEnabled()) return {};

            // `c` is the CACHED component under the mouse; see TooltipSource above for why it can
            // disagree with where the pointer actually is, and why that shows another control's tip.
            // The hit test is evaluated eagerly rather than only on disagreement: `choose` ignores
            // it whenever the cache agrees, so the RESULT is unchanged in that case, but the walk
            // does run. It is one coordinate transform and one tree descent per 123 ms tooltip tick
            // on the message thread, measured to cost nothing that shows up in the idle profile,
            // and keeping it a plain argument keeps `choose` a pure function of three values --
            // which is what makes the decision testable without a display.
            const auto live = juce::Desktop::getMousePosition();
            auto* src = TooltipSource::choose (&c, live, componentAt ? componentAt (live) : nullptr);

            // Whichever component wins, the BASE class answers for it, so every suppression JUCE
            // applies -- a mouse button down, a modal component blocking the target, a backgrounded
            // process, a target that is not a TooltipClient -- still reaches the state machine.
            return src != nullptr ? juce::TooltipWindow::getTipFor (*src) : juce::String();
        }
    };
    // 600 ms is the ONLY place the appear-delay is set; applyTooltipsEnabled never touches it.
    GatedTooltipWindow tooltips { nullptr, 600 };

    // --- Pop-up dismissal: one shield, one flag, three feeders -------------------------------
    // Declared AFTER the look-and-feel members on purpose, like every other child component here:
    // members are destroyed in reverse declaration order, so a child declared later dies BEFORE the
    // look-and-feels it may resolve through. `popupShield` does not resolve one today -- it paints
    // nothing and never calls setLookAndFeel -- but the moment it gains a paint() that does, the
    // inverted order would surface as the `~LookAndFeel` live-WeakReference assertion that
    // showPresetMenu's INC-010 comment describes. Keeping the convention costs nothing.
    //
    // `openMenus` holds every PopupMenu window currently on screen that reported itself through
    // AnamorphLookAndFeel::onPopupMenuWindowCreated (ComboBox drop-downs, TextEditor context
    // menus), as SafePointers so a destroyed window drops out on its own. `presetMenusOpen` counts
    // the menus this editor shows itself, which do NOT reach that hook -- their look-and-feel is
    // null at construction, so JUCE resolves the default one there. Either being non-empty raises
    // the shield.
    PopupShield popupShield;
    bool shieldRaised = false;   // the shield is always visible; this is whether it intercepts
    // Whether Process::isForegroundProcess() read true at the last pop-up open -- i.e. whether that
    // test means anything in this host. Out-of-process / bridged hosting makes it permanently false;
    // see dismissOrphanedPopupMenus for why that must not be read as "the user switched away".
    bool popupOpenedWhileForeground = false;
    juce::Array<juce::Component::SafePointer<juce::Component>> openMenus;
    int  presetMenusOpen = 0;
    void notePopupMenuOpened (juce::Component& menuWindow);
    void refreshPopupShield();   // prunes dead windows and shows/hides the shield
    bool cursorIsOverOpenPopup() const;  // the cursor is inside a pop-up of ours, so it is NOT
                                         // over whatever this editor draws underneath it

    // The overlay panel the cursor is inside, or nullptr. A pop-up is a window ON TOP of the
    // editor; an overlay is a full-editor CHILD (Settings / About / Save Preset) that covers
    // everything except its own contents -- so unlike the pop-up term this one cannot be a single
    // per-frame answer, and `occludes()` below finishes the question per widget (KI-024).
    juce::Component* cursorOverlay() const;

    // Is `c` behind `overlay`? Its own descendants are not: an overlay covers the editor, not the
    // panel it puts there, so a Settings control keeps hovering while a knob behind it does not.
    static bool occludes (const juce::Component* overlay, const juce::Component* c) noexcept
    {
        return overlay != nullptr && overlay != c && ! overlay->isParentOf (c);
    }
    void dismissTrackedPopupMenus();   // cancels every pop-up this editor owns, unconditionally
    void cancelInlineTextEdits();      // parks in-progress inline edits without applying them
    void dismissOrphanedPopupMenus();  // ... and the same, once one can no longer belong to us
    void componentBeingDeleted (juce::Component&) override; // a tracked pop-up window went away

    // Centrepiece + meters
    std::unique_ptr<anamorph::gui::Vectorscope> scope;
    std::unique_ptr<anamorph::gui::StereoMeter> balanceMeter, corrMeter;
    std::unique_ptr<anamorph::gui::LevelMeter>  levelMeter;

    // Top bar
    juce::TextButton   titleButton;
    ABControl          abControl;
    juce::TextButton   copyButton { "Copy" };
    juce::TextButton   settingsButton { "Settings" };
    juce::TextButton   undoButton, redoButton;
    juce::ToggleButton metersToggle, advancedToggle, bypassToggle;

    // Preset browser (F2): ‹ name ›, the name opens the preset menu.
    juce::TextButton   presetPrev, presetNext, presetName;

    // Knob: a slider that resets to its default on a clean double-click OR an
    // Option/Alt-click (#6 / 0.6.7 #21). onSweep lets the editor play the eased
    // position travel when a RESET happens (but not on a drag).
    struct Knob : public juce::Slider, public anamorph::gui::WheelDragOwner
    {
        // THE VELOCITY-SWAP MODIFIER, PINNED (round 30, Devin `src/PluginEditor.h:R822-828`).
        // JUCE decides which of its TWO drag mappings an event takes with
        //     isVelocityBased == (userKeyOverridesVelocity && mods.testFlags (modifierToSwapModes))
        // (`Slider::Pimpl::isAbsoluteDragMode`, juce_Slider.cpp) and exposes getters for the first
        // two and NONE for `modifierToSwapModes`. `dragIsVelocity` below has to answer the same
        // question this class answers the notch with, so the modifier is set rather than assumed.
        //
        // THE VALUES ARE JUCE'S OWN DEFAULTS, so nothing about the feel changes: the member
        // initialisers are `velocityModeSensitivity = 1.0, velocityModeOffset = 0`,
        // `velocityModeThreshold = 1`, `userKeyOverridesVelocity = true` and
        // `modifierToSwapModes = ModifierKeys::ctrlAltCommandModifiers` (juce_Slider.cpp). This
        // states them where the predicate can be read against them.
        Knob()
        {
            setVelocityModeParameters (1.0, 1, 0.0, true,
                                       juce::ModifierKeys::ctrlAltCommandModifiers);
        }

        double resetValue = 0.0;
        // The attached parameter (null for host-hidden InternalState knobs): the
        // ALT-CLICK reset must open its own host change gesture. Our mouseDown
        // intercepts the event BEFORE juce::Slider can start a drag, so the
        // SliderAttachment never opens one, and the programmatic setValue reached
        // the processor's undo coalescer gesture-less -- which is the automation
        // path, folded into the baseline with NO undo step and NO redo clear.
        // That is why Option/Alt-click reset was un-undoable (and left Redo
        // alive). THE DOUBLE-CLICK RESET NEEDS THE SAME WRAP, and this comment used to say the
        // opposite -- "its second press runs Slider::mouseDown first, whose ScopedDragNotification
        // has already opened the drag gesture that mouseUp will close, so wrapping there would
        // nest". The premise is false: JUCE dispatches `mouseDoubleClick` from
        // `Component::internalMouseUp`, AFTER `mouseUp` has run, so the press's gesture is already
        // CLOSED by then and there is nothing to nest inside. JUCE's own
        // `Slider::Pimpl::mouseDoubleClick` wraps its write in a `ScopedDragNotification` for that
        // reason; this class overrides `mouseDoubleClick` and never reaches it, so until round 14
        // the reset reached the host as a gesture-less write -- the automation shape KI-010
        // names, and the reason its undo step existed only as a side effect of the whole-state
        // push rule. Bracketed now, gated on `resetWouldMove()` exactly as the Alt path is
        // (ADR-0052: an input that performs no edit has no side effects).
        juce::RangedAudioParameter* resetParam = nullptr;
        std::function<void()> onSweep;
        // ADR-0053. THE WHEEL IS PART OF THE INTERACTION IT LANDS IN, and this member is what lets a
        // drag carry ON from a notch instead of erasing it. JUCE discards the wheel outright while a
        // button is held -- `! e.mods.isAnyMouseButtonDown()` guards its whole handler
        // (juce_Slider.cpp) -- so a notch mid-drag used to do nothing at all; and merely writing the
        // value would not survive either, because `handleAbsoluteDrag` recomputes it from
        // `valueOnMouseDown` plus the cursor delta on EVERY mouse move and never reads the live value
        // back. Both of those are private Pimpl members with no public setter, and
        // `getThumbBeingDragged()` is the only part of that state a subclass can see -- so the notch
        // is remembered OUT HERE and re-applied on top of whatever the drag computed. In PROPORTION
        // space, so one notch means the same travel on a skewed range as on a linear one.
        //
        // Zero for a press with no notch in it, and every line that reads it returns immediately on
        // zero, so an ordinary drag makes exactly the parameter writes it always did.
        //
        // IN PIXELS SINCE ROUND 29, AND THAT IS THE FIX RATHER THAN A TIDY-UP. It was a PROPORTION,
        // added to whatever the drag computed, in a `snapValue` override -- and `snapValue` is
        // called by JUCE AFTER it has already clamped its own mapping to [0, 1]
        // (`handleAbsoluteDrag` ends `newPos = jlimit (0.0, 1.0, ...)`, juce_Slider.cpp, and
        // `mouseDrag` clamps again to the range before it calls `snapValue`). So the offset sat
        // OUTSIDE the clamp: after a notch that took the value DOWN by half the range, the drag's
        // own proportion saturated at 1.0 while the returned proportion was `1.0 + offset` = 0.5,
        // and the knob could not be dragged to its maximum for the rest of that press. That is the
        // reported "stuck around 50%".
        //
        // A pixel offset is applied where the ANCHOR is instead: `mouseDrag` below hands JUCE an
        // event whose position is shifted by this many pixels, so the shift is inside
        // `mouseDiff` -- i.e. inside what JUCE clamps -- and the remaining physical travel maps
        // exactly as it would have if the press had started at the value the notch produced. It is
        // the same shape the multiband display and the value box have always used (both anchor in
        // CURSOR space and clamp once, at the end), which is why neither of them has this defect.
        //
        // Positive means "the value the drag computes is raised", whichever axis and whichever
        // direction the style reads the cursor in; `wheelDragShift()` turns it into the offset for
        // this slider's own style.
        double wheelDragPx = 0.0;
        // ADR-0053, round 11. JUCE's DUPLICATE-EVENT FILTER, restated for the in-drag path -- and it
        // is restated because the floor above made it load-bearing. JUCE's own reason
        // (`juce_Slider.cpp`) is exactly that: "sometimes duplicate wheel events seem to be sent, so
        // since we're going to bump the value by a minimum of the interval, avoid doing this twice".
        // It is about two DISTINCT events carrying one timestamp, not about one event delivered
        // twice -- so single delivery, which the routing does guarantee, is not an answer to it.
        // Before the floor, a duplicate asked for the same sub-interval nothing twice; with the
        // floor it asks for two whole intervals, which would make the in-drag notch move FURTHER
        // than the standalone one and break the same contract from the other side.
        //
        // IT STAYS PER-OWNER (round 30). A register-wide filter looks tidier and is wrong: it would
        // refuse the second and later notches of a smooth-trackpad burst, which share a millisecond
        // because `juce::Time::getCurrentTime()` has no finer resolution. It guards this floor, not
        // a delivery.
        juce::Time lastNotchTime;
        // The processor, for the wheel's undo grouping (ADR-0053). Null for a knob with no APVTS
        // parameter behind it -- which is the Settings Persistence bar, and exactly the control whose
        // undo participation must not change: with no parameter there is no change gesture and no
        // sound signature, so it cannot record a step whatever this does.
        AnamorphAudioProcessor* owner = nullptr;

        // ADR-0052 (round 11). A RESET THAT HAS NOTHING TO RESET IS NOT AN EDIT. The knob is
        // already sitting on `resetValue`, so `setValue` below would write the value the control
        // already holds and JUCE would drop it -- but the animation, the `vpos` seed and, on the
        // Alt path, a host change gesture would all have gone out for an interaction that changed
        // nothing. Asked in VALUE space, which is the space `setValue` compares in.
        bool resetWouldMove() const { return ! juce::exactlyEqual (getValue(), resetValue); }

        void doReset()
        {
            if (! resetWouldMove()) return;   // no edit, no sweep, no latched "vpos" (ADR-0052)
            // Seed the sweep from the CURRENT position so the eased travel has a real
            // "from" to leave. onSweep (below) then flags the reset sweep -- but only
            // when animations are on -- so the value-travel easing plays even though the
            // mouse button is still physically held (an alt-click and a double-click's
            // 2nd press are both mouse-down events). Without that flag the held button
            // snaps the knob straight to the target, which is why alt-click stopped
            // animating; with animations off the knob just snaps, exactly as before.
            getProperties().set ("vpos", (double) valueToProportionOfLength (getValue()));
            setValue (resetValue, juce::sendNotificationSync);
            if (onSweep) onSweep();
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            wheelDragPx = 0.0;      // a new press starts with no notch in it (ADR-0053)
            velocityInject = 0.0;   // ...in either of the two mappings (round 30)
            injectingVelocity = false;
            lastDragMode = juce::Slider::notDragging;
            // ADR-0053 round 29: this press owns the wheel until it is released, wherever the
            // cursor travels. Claimed for EVERY press, not only one that starts a drag: the rule
            // the owner approved is that no other control may be moved by the wheel while a button
            // is held, and a press that holds no value simply has nothing to add a notch to.
            anamorph::gui::claimDragWheel (*this, *this, anamorph::gui::wheelPointerOf (e.source));
            if (e.mods.isAltDown()) // Option/Alt-click reset, as ONE undoable user gesture
            {
                // ...and the gesture is part of what an edit costs, so the same question is asked
                // before it opens rather than inside `doReset` alone: a begin/end pair on a
                // parameter that never moved is an automation punch-in a host recording touch or
                // latch writes a point for (ADR-0052, round 11 -- the same rule the multiband
                // wheel branches answer for their own rails).
                if (! resetWouldMove()) return;
                anamorph::param::beginChangeGesture (resetParam);
                doReset();
                noteResetProducedNothing();   // round 23, see below
                anamorph::param::endChangeGesture (resetParam);
                return;
            }
            juce::Slider::mouseDown (e);
        }

        // A DRAG AFTER AN ALT-CLICK RESET IS ALREADY INERT, and nothing is added here to make it so
        // (round 30, investigated while proving R3302 and DISPROVEN). The branch above returns
        // without calling `juce::Slider::mouseDown`, and `Pimpl::useDragEvents` is cleared by
        // nothing else -- `Pimpl::mouseUp` leaves it set -- so `Pimpl::mouseDrag` really does run
        // its body on the next drag with state from the press before last. It writes nothing:
        // `~ScopedDragNotification` calls `sendDragEnd`, which sets `sliderBeingDragged = -1`
        // (juce_Slider.cpp:396-399), and every one of `mouseDrag`'s three stores is behind a
        // `sliderBeingDragged == 0 / 1 / 2` test (:939-961). No `setValue`, so no gesture-less
        // parameter write; `owner.snapValue` is not reached either, which is what State test 107
        // leg G measures. Nothing the stale pass wrote survives, because `Pimpl::mouseDown` re-seeds
        // `valueWhenLastDragged` and `valueOnMouseDown` from the live value on the next real press
        // (:887-890). The one residue is the cursor hide `handleVelocityDrag` asks for, and the
        // same press's own `mouseUp` restores it (`restoreMouseIfHidden`, guarded by the same stale
        // `useDragEvents`). A guard on this side was written and then removed: it changed no
        // observable behaviour, so it could not be covered, and an uncoverable guard against a
        // defect that does not exist is worse than the comment that says so.

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (e.getNumberOfClicks() != 2 || ! resetWouldMove()) return;
            anamorph::param::beginChangeGesture (resetParam);
            doReset();
            noteResetProducedNothing();   // round 23, see below
            anamorph::param::endChangeGesture (resetParam);
        }

        // ADR-0008, ROUND 23 (Devin R1117-1119). A RESET THAT REACHES ITS CLOSE WITH NOTHING
        // DECLARED SAYS SO, and the two lines above are the reason it can.
        //
        // `resetWouldMove()` asks the SLIDER, because that is the space `Slider::setValue` compares
        // in and ADR-0052's rule is about what the interaction costs. The PARAMETER can already be
        // sitting on the reset value while the slider is not: a parameter written without notifying
        // its listeners never reaches `ParameterAttachment`, and an off-message-thread write reaches
        // it only through `triggerAsyncUpdate`, so the control lags by up to one message-loop turn.
        // In that window the guard says "this moves something", the gesture opens, and JUCE's
        // attachment then DROPS the write because the parameter already holds that value
        // (`ParameterAttachment::setValueAsPartOfGesture` -> `callIfParameterValueChanged`). The
        // witness's AFTER hook declares nothing for a parameter that did not move, no store was
        // refused, and the batch close was left with a live read of whatever a host lane had put
        // there -- the host's value as the user's Redo destination. State test 96 leg C measured it.
        //
        // Stated UNCONDITIONALLY, exactly as `SpectrumImager::endGesture` states it since round 22
        // and for the same reason: it carries no value, and the close skips a parameter whose store
        // DECLARED an endpoint (bit 2) before it ever consults the refusal bit -- so a reset that
        // really did move the parameter keeps the endpoint the witness stated for it.
        void noteResetProducedNothing() noexcept
        {
            if (resetParam != nullptr && owner != nullptr)
                owner->noteOwnedParamRefused (resetParam);
        }
        // ADR-0053, ROUND 14: THE NOTCH TOTAL IS FOLDED IN BEFORE THE DRAG WRITES, NOT AFTER IT.
        // JUCE asks this immediately before its own drag store --
        // `setValue (owner.snapValue (valueWhenLastDragged, dragMode), sendNotificationSync)` -- so
        // answering with the COMBINED value makes a drag event publish once. Until round 14 the
        // offset was applied by a second `setValue` in a `mouseDrag` override, which published the
        // PURE DRAG value first: measured on the Drive knob with one notch banked, the host's
        // `audioProcessorParameterChanged` and the DSP atomic both took 2.1100 dB while the control
        // stood at 3.9100 -- a full notch BACKWARDS, on every mouse move, inside the open
        // touch/latch punch-in the press holds, so a DAW recording automation wrote the spike into
        // the lane. Both writes landed in one gesture, so nothing downstream of the release could
        // see it and no existing check did.
        //
        // GATED ON THE NOTCH TOTAL, NOT ON `dragMode`: JUCE leaves `dragMode == notDragging` for the
        // plain `Rotary` style, so a `dragMode` test would silently stop folding for a style this
        // editor does not happen to use today. `wheelDragProp` is non-zero only between a notch and
        // the release of THIS slider's own press -- `mouseDown` and `mouseUp` both zero it -- which
        // is exactly the drag path, and every other `snapValue` caller in JUCE (the wheel, the text
        // box, the inc/dec buttons) is reached only with it at zero.
        //
        // `base` COMES FROM THE SNAPPED VALUE because that is what the old `getValue()` returned:
        // JUCE runs `constrainedValue` (i.e. `snapToLegalValue`) on whatever this answers, so taking
        // the raw `attempted` would drop a quantisation step the arithmetic had already applied.
        // Measured equivalent to the old code on 8640 sequences -- every APVTS parameter x four drag
        // paths x five start values x four wheel deltas x three notch positions -- with the raw form
        // differing on 89 of them and the snapped form on none.
        //
        // A two- or three-value slider deriving from `Knob` would get the offset folded into its
        // min/max thumbs as well (`setMinValue`/`setMaxValue` call this with a live `dragMode`).
        // There is no such slider here -- every one is single-value -- and this is recorded rather
        // than guarded, because a guard with no caller cannot be tested.
        // HOW MANY PIXELS OF CURSOR TRAVEL ARE ONE WHOLE RANGE, asked of JUCE rather than restated
        // here, because a scale of our own would be a second sensitivity to keep in step. The two
        // families this editor uses answer differently and both answers are public:
        //   * the relative styles -- every `RotaryVerticalDrag` knob here -- map
        //     `mouseDiff / pixelsForFullDragExtent`, which is `getMouseDragSensitivity()`;
        //   * an absolute linear style -- the two `LinearHorizontal` knobs, which keep JUCE's
        //     default `snapsToMousePos` -- maps `(mousePos - sliderRegionStart) / sliderRegionSize`,
        //     and `sliderRegionSize` is exactly the distance between the positions of the two ends.
        // The condition is JUCE's own (`handleAbsoluteDrag`, juce_Slider.cpp), spelled the same way
        // round, so a style that changes there changes here with it.
        [[nodiscard]] bool dragIsRelativeToPress() const
        {
            const auto st = getSliderStyle();
            if (st == juce::Slider::RotaryHorizontalDrag || st == juce::Slider::RotaryVerticalDrag
                || st == juce::Slider::RotaryHorizontalVerticalDrag || st == juce::Slider::IncDecButtons)
                return true;
            if (st == juce::Slider::LinearHorizontal || st == juce::Slider::LinearVertical
                || st == juce::Slider::LinearBar || st == juce::Slider::LinearBarVertical)
                return ! getSliderSnapsToMousePosition();
            return false;   // `Rotary` steers by ANGLE; see `wheelDragShift`
        }

        // ---- THE VELOCITY MAPPING (round 30, Devin `src/PluginEditor.h:R822-828`) --------------
        //
        // JUCE HAS TWO DRAG MAPPINGS AND ONLY ONE OF THEM IS AFFINE IN THE CURSOR.
        // `handleAbsoluteDrag` is `prop (valueOnMouseDown) + mouseDiff / pixelsForFullDragExtent`,
        // which is what `wheelDragPx` above relies on: a constant pixel shift is a constant value
        // shift. `handleVelocityDrag` is an INTEGRATOR --
        //     speed  = a sine curve of |e.position - mousePosWhenLastDragged|
        //     newPos = prop (valueWhenLastDragged) + speed
        // (juce_Slider.cpp) -- and a pixel shift there is neither constant nor a shift: it enters
        // through `mouseDiff`, is bent by the curve, and lands as one spurious kick.
        //
        // WORSE, THE NOTCH ITSELF IS DISCARDED. `Pimpl::setValue` never writes
        // `valueWhenLastDragged` -- every write to it is in `handleRotaryDrag`,
        // `handleAbsoluteDrag`, `handleVelocityDrag`, `mouseDown` and the clamp in `mouseDrag`, and
        // none of them is reachable from `Slider::setValue` -- so a notch that writes the value
        // leaves the integrator behind and the NEXT event recomputes from the stale base. Measured
        // on Drive with the modifier held: `0.0042 -> notch -> 0.1542 -> next drag 0.0550`, where
        // the drag should have continued from 0.1542.
        //
        // SO THE NOTCH IS BANKED IN THE INTEGRATOR'S OWN SPACE, as a PROPORTION, and injected into
        // the one place the integrator reads: `owner.valueToProportionOfLength (valueWhenLastDragged)`.
        // That is a public virtual, so this class can add the banked proportion to exactly that
        // call and nothing else -- the flag is armed for the duration of one
        // `juce::Slider::mouseDrag` and disarms itself on the FIRST call, which is provably
        // `handleVelocityDrag`'s: nothing earlier in `Pimpl::mouseDrag` calls it (the
        // `useDragEvents` test, the `Rotary` branch, the `IncDecButtons` threshold and
        // `isAbsoluteDragMode` read no proportion). The injection therefore lands INSIDE the
        // `jlimit (0, 1, ...)` two lines below it, so the whole remaining range stays reachable,
        // it is applied exactly once, and it never passes through the velocity curve.
        double velocityInject    = 0.0;    // proportion a notch banked for the integrator
        bool   injectingVelocity = false;  // armed only around one juce::Slider::mouseDrag call

        // JUCE's own branch test, from `Slider::Pimpl::mouseDrag` and `::isAbsoluteDragMode`
        // (juce_Slider.cpp), negated. The swap modifier is pinned in this class's constructor
        // because JUCE exposes no getter for it; everything else it reads has one.
        [[nodiscard]] bool dragIsVelocity (const juce::ModifierKeys& mods) const
        {
            if (getSliderStyle() == juce::Slider::Rotary) return false;   // `handleRotaryDrag`, neither branch
            if (getVelocityBasedMode() == (getVelocityModeIsSwappable()
                                            && mods.testFlags (juce::ModifierKeys::ctrlAltCommandModifiers)))
                return false;   // `isAbsoluteDragMode` said absolute
            // ...and the second disjunct, which forces absolute mode for a range too coarse to
            // steer: `(normRange.end - normRange.start) / sliderRegionSize < normRange.interval`.
            // `sliderRegionSize` is private; it is initialised to 1 and `Pimpl::resized` assigns it
            // only for the horizontal and vertical styles (juce_Slider.cpp:1266-1274, :1324), so a
            // rotary divides by 1 and the test reads `range < interval` -- false for every
            // parameter this editor has. Measured from OUTSIDE, the same span is what
            // `getPositionOfValue` maps a whole range across; it is the live geometry for a linear
            // style and 0 for a rotary (which has no linear position to report), and a zero region
            // is excluded below rather than divided by. Either way the rotary never reaches here:
            // the first line returned already.
            const double region = std::abs ((double) getPositionOfValue (getMaximum())
                                          - (double) getPositionOfValue (getMinimum()));
            if (region > 0.0 && (getMaximum() - getMinimum()) / region < getInterval())
                return false;
            return true;
        }

        // THE INJECTION POINT. Everything outside the one armed call is JUCE's own answer.
        double valueToProportionOfLength (double v) override
        {
            const double p = juce::Slider::valueToProportionOfLength (v);
            if (! injectingVelocity) return p;
            injectingVelocity = false;          // exactly ONE call sees it -- handleVelocityDrag's
            const double inject = velocityInject;
            velocityInject = 0.0;               // ...and it is banked exactly once
            // CLAMPED HERE TOO, though the only caller that can see the injection clamps
            // immediately afterwards: `handleVelocityDrag`'s next line is
            // `newPos = (isRotary() && ! rotaryParams.stopAtEnd) ? newPos - floor (newPos)
            //                                                    : jlimit (0.0, 1.0, newPos)`
            // (juce_Slider.cpp:843-845), and `stopAtEnd` is true for every slider in this editor --
            // the Pimpl constructor sets it (:58) and `setRotaryParameters` is never called. So a
            // mutant that drops this `jlimit` is EQUIVALENT and no test can kill it; it is kept
            // because the wrap branch two characters away would not be, and because this function
            // is public and JUCE may call it from somewhere else tomorrow.
            return juce::jlimit (0.0, 1.0, p + inject);
        }

        // WHICH BRANCH JUCE ACTUALLY TOOK, recorded rather than predicted. `Pimpl::mouseDrag`
        // passes the mode it chose to `owner.snapValue`, so this is JUCE's own answer to the
        // question `dragIsVelocity` predicts -- and State test 107 asserts the two never disagree.
        // The value is returned untouched: round 29 moved the wheel fold OUT of here and into
        // `mouseDrag`, and nothing has moved back.
        double snapValue (double attempted, juce::Slider::DragMode m) override
        {
            lastDragMode = m;
            return juce::Slider::snapValue (attempted, m);
        }
        juce::Slider::DragMode lastDragMode = juce::Slider::notDragging;

        [[nodiscard]] double pixelsPerWholeRange() const
        {
            if (dragIsRelativeToPress())
                return (double) juce::jmax (1, getMouseDragSensitivity());
            const double a = (double) getPositionOfValue (getMinimum());
            const double b = (double) getPositionOfValue (getMaximum());
            return juce::jmax (1.0, std::abs (b - a));
        }

        // The offset to add to an event's position so that JUCE's own mapping comes out `wheelDragPx`
        // higher in VALUE. Every style this editor uses reads exactly one axis, and reads it in the
        // direction recorded here: `x` rises with the value, `y` falls with it (JUCE's vertical
        // forms are `mouseDragStartPos.y - e.position.y` and `1.0 - (y - start) / size`).
        //
        // `Rotary` -- the angle-steered style -- is NOT one of them: its mapping is not affine in
        // the cursor, so a constant pixel shift is not a constant value shift and this returns a
        // zero offset for it. That is recorded rather than guarded because it is unreachable in this
        // editor: `setSliderStyle` is called three times in `src/` (`src/PluginEditor.cpp:541`,
        // `:687`, `:812`) and none of them names `Rotary`. The assertion is what would notice if a
        // fourth call ever did.
        [[nodiscard]] juce::Point<float> wheelDragShift() const
        {
            if (juce::exactlyEqual (wheelDragPx, 0.0)) return {};
            const auto st = getSliderStyle();
            jassert (st != juce::Slider::Rotary);
            if (st == juce::Slider::RotaryHorizontalDrag || st == juce::Slider::LinearHorizontal
                || st == juce::Slider::LinearBar)
                return { (float) wheelDragPx, 0.0f };
            if (st == juce::Slider::Rotary)
                return {};
            return { 0.0f, (float) -wheelDragPx };   // every vertical form, and the H+V rotary
        }

        // THE ONE PLACE THE OFFSET IS APPLIED. JUCE recomputes the drag's value from its own anchor
        // and the cursor on every move and never reads the live value back, so a notch that only
        // wrote the value would be erased by the next move (ADR-0053's original finding). Shifting
        // the position it is handed moves the ANCHOR instead -- `mouseDiff` is `e.position` minus a
        // start point JUCE captured at the press, so a constant shift is a constant value offset --
        // and, unlike the `snapValue` override this replaces, it is inside the clamp rather than
        // outside it, so the drag keeps its whole remaining travel in both directions.
        void mouseDrag (const juce::MouseEvent& e) override
        {
            // THE VELOCITY BRANCH TAKES THE OTHER MECHANISM (round 30). A pixel shift means nothing
            // to an integrator that reads a per-event cursor DELTA, so the notch goes in through
            // `valueToProportionOfLength` instead and the event is handed over untouched.
            if (dragIsVelocity (e.mods))
            {
                const juce::ScopedValueSetter<bool> arm (injectingVelocity,
                                                        ! juce::exactlyEqual (velocityInject, 0.0));
                juce::Slider::mouseDrag (e);
                return;
            }
            const auto shift = wheelDragShift();
            if (shift.isOrigin()) { juce::Slider::mouseDrag (e); return; }
            juce::Slider::mouseDrag ({ e.source, e.position + shift, e.mods,
                                       e.pressure, e.orientation, e.rotation, e.tiltX, e.tiltY,
                                       e.eventComponent, e.originalComponent, e.eventTime,
                                       e.mouseDownPosition, e.mouseDownTime,
                                       e.getNumberOfClicks(), e.mouseWasDraggedSinceMouseDown() });
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseUp (e);
            wheelDragPx = 0.0;
            velocityInject = 0.0;
            injectingVelocity = false;
            anamorph::gui::releaseDragWheel (*this);
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            // THE PRESS DECIDES, NOT THE POINTER (ADR-0053, owner decision). If some other control
            // is holding a press, this notch is ITS notch however far its cursor has wandered onto
            // this knob; if THIS knob holds the press, the notch is offered to it here; and if a
            // button is down with nothing claimed, nobody may have it (round 30). All three are
            // the same question, so there is one call and nothing below it to get wrong.
            if (anamorph::gui::wheelTakenByAnyPress (*this, this, e, w)) return;
            mouseWheelMoveTail (e, w);
        }

        // A NOTCH INSIDE THIS KNOB'S OWN DRAG (ADR-0053). `getThumbBeingDragged()` is >= 0 only
        // between the mouseDown that actually STARTED a drag and the drag-end notification, so a
        // press that started none -- a pop-up-menu click, a single-click reset -- does not qualify
        // and a notch can never write outside a change gesture.
        //
        // ...AND IT IS THE `WheelDragOwner` HOOK (round 29), so the same body serves a notch
        // delivered here by the pointer and one posted here by the register while the cursor is
        // somewhere else entirely. `false` means this press has nothing to add the notch to.
        bool takeWheelNotch (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            // `e.mods.isAnyMouseButtonDown()` is the register's precondition, not this hook's:
            // round 30 made `wheelTakenByAnyPress` the only caller and it asks first.
            if (isScrollWheelEnabled() && getThumbBeingDragged() >= 0)
            {
                // What JUCE's own handler would move this slider to for this event -- direction,
                // scale, rails AND its one-interval floor, from the single source in LookAndFeel.h
                // -- so one notch means the same travel whether or not a button is held. It said
                // that here before and did not deliver it: without the floor a sub-interval notch
                // was snapped straight back by `setValue` and the press ate it (round 11).
                if (e.eventTime == lastNotchTime) return true;   // ...the duplicate, before anything (ADR-0052)
                lastNotchTime = e.eventTime;
                const double v0     = getValue();
                const double target = anamorph::gui::wheelTargetValue (*this, w, v0);
                if (juce::exactlyEqual (target, v0)) return true;  // ADR-0052: no edit, no side effects
                const double base = juce::Slider::valueToProportionOfLength (v0);
                setValue (target, juce::sendNotificationSync);
                // BANK WHAT ACTUALLY MOVED, read back from the slider rather than from the request:
                // the write is clamped to the range and snapped to the interval grid, and banking
                // the request instead would leave the drag carrying travel the control never took
                // -- dead travel the next mouse move would then apply as a jump (ADR-0052's latch
                // half). ...and the drag carries on from the value the notch left behind.
                //
                // IN PIXELS (round 29): the proportion that actually moved, times the travel JUCE
                // itself maps a whole range across. See `wheelDragPx`.
                // ...IN THE SPACE THE ACTIVE MAPPING READS. An absolute drag recomputes from the
                // cursor, so the notch is banked as the pixels that would have moved it there; a
                // velocity drag integrates a proportion, so it is banked as that proportion.
                // `lastDragMode` is what JUCE told us the last event took (`snapValue`), which for
                // a press that has had at least one event -- and `Pimpl::mouseDown` ends with one
                // -- is the branch the next event takes too unless the modifier changes under the
                // user's finger, and `mouseDrag` asks the CURRENT event's modifiers for that.
                const double moved = juce::Slider::valueToProportionOfLength (getValue()) - base;
                if (lastDragMode == juce::Slider::velocityDrag) velocityInject += moved;
                else                                            wheelDragPx    += moved * pixelsPerWholeRange();
                return true;
            }
            return false;
        }

        // A STANDALONE SCROLL OF THIS KNOB HAPPENED (round 30). The Settings Persistence bar reveals
        // its window on a sustained scroll, and until this round the editor learned about that by
        // registering itself as a `MouseListener` on the bar. That worked, and it also made
        // `Component::internalMouseWheel` offer the SAME notch to the wheel register twice -- once
        // when it called the bar's own handler and once when it called the bar's listeners
        // (juce_Component.cpp) -- so a press held elsewhere had its notch posted to the holder
        // twice. The knob and the value box filtered the repeat on `eventTime`; the multiband
        // display had no filter and would have applied it twice. A callback the knob raises where
        // the scroll actually is says the same thing once.
        std::function<void()> onStandaloneWheel;

        void mouseWheelMoveTail (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
        {
            if (onStandaloneWheel) onStandaloneWheel();
            // (The value box under this knob no longer has to be asked from here. It claims the
            // wheel for itself at its own `mouseDown`, so a notch delivered to this knob during the
            // box's drag is routed by the register above -- one rule for every control instead of
            // one parent knowing about one child.)

            // A STANDALONE SCROLL NAMES THE CONTROL IT EDITS, so the processor can keep the whole
            // scroll -- however many notches, and however many pauses between them -- as ONE undo
            // step (ADR-0053). The value box below the knob forwards its own wheel events here, so
            // the knob and the number under it name the same control, which is what they are.
            if (owner != nullptr && resetParam != nullptr)
            {
                const AnamorphAudioProcessor::ScopedWheelStep step
                    (*owner, AnamorphAudioProcessor::wheelStepKeyFor (resetParam));
                sendWheelToJuce (e, w);
                return;
            }
            sendWheelToJuce (e, w);
        }

        // A NOTCH THAT LANDED HERE WITH A BUTTON DOWN AND NO CONTROL HOLDING A PRESS (ADR-0053).
        // JUCE routes a wheel event by POINTER and not by capture -- `getTargetForGesture`
        // hit-tests the peer at the event position whether or not a drag is in flight -- and then
        // discards it if any button is down, because its whole wheel body sits behind
        // `! e.mods.isAnyMouseButtonDown()`: a notch the user makes and never sees.
        //
        // ROUND 29 NARROWED WHAT REACHES HERE, and this comment used to describe the case that no
        // longer does, and round 30 REMOVED the branch that served it. The case this laundering
        // existed for -- "a button is held somewhere that owns no wheel press, and the pointer is
        // over this knob, so let JUCE act as if no button were down" -- is exactly the case the
        // owner reversed: while any button is held, no control moves but the one being held, and
        // `wheelTakenByAnyPress` now returns true for it at the top of `mouseWheelMove`. Nothing
        // reaches here with a button down any more, so the laundered event has no reader, and the
        // one line left is what a standalone scroll always did.
        //
        // KEPT AS A FUNCTION rather than inlined: `mouseWheelMoveTail` names the undo grouping and
        // this names the delivery, and the two are separate decisions.
        void sendWheelToJuce (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
        {
            jassert (! e.mods.isAnyMouseButtonDown());   // round 30: the register consumed those
            juce::Slider::mouseWheelMove (e, w);
        }
    };

    // WIDEN module
    juce::ComboBox algorithmBox, haasSideBox, dimModeBox;
    juce::Label    algorithmLabel, algoOptLabel; // algoOptLabel captions the side/voicing combo (#9)
    Knob driveK, amountK, widthK;
    juce::Label  driveL, amountL, widthL;
    Knob haasDelayK, velvetK, chorusRateK, chorusDepthK;
    juce::Label  haasDelayL, velvetL, chorusRateL, chorusDepthL;

    // OUTPUT module (advanced, #24)
    juce::Label  outputModuleLabel;
    Knob mixK, outputK, outBalanceK;
    juce::Label  mixL, outputL, outBalanceL;
    juce::ToggleButton autoMatchToggle;
    juce::TextButton   applyGainButton { "Apply" };
    juce::Label        matchReadout;

    // MONO MAKER (slim bar, inside the Output module)
    juce::ToggleButton monoMakerToggle;
    Knob monoFreqK;  juce::Label monoFreqL;

    // INPUT module (advanced)
    juce::ComboBox channelModeBox, soloBox;
    juce::Label    channelModeLabel, soloLabel, inputModuleLabel;
    juce::ToggleButton monoToggle, swapToggle, msToggle, polLToggle, polRToggle;
    Knob balanceK; juce::Label balanceL;

    // IMAGER module (advanced): drag-to-split spectral band editor replaces the
    // rotary multiband (4 bands, FFT spectrum, draggable crossovers + widths).
    juce::Label  multibandLabel;
    juce::ToggleButton mbEnableToggle;
    std::unique_ptr<anamorph::gui::SpectrumImager> imager;
    Knob scopePersistK; juce::Label scopePersistL;

    // Overlays
    DimLayer dimOverlay;
    Backdrop aboutBackdrop, settingsBackdrop;
    juce::HyperlinkButton aboutLink { "www.rolly.tech", juce::URL ("https://www.rolly.tech") }; // #4

    // Settings controls
    juce::ComboBox oversampleBox;  juce::Label oversampleLabel;
    juce::ComboBox uiScaleBox;     juce::Label uiScaleLabel; // XS..XL window scale (F4)
    juce::ToggleButton tooltipsToggle;
    juce::ToggleButton animToggle;  // micro-animation switch (F3)
    juce::Label settingsTitle;
    juce::Label persistLabel;   // Persistence moved into Settings as a bar (#21)

    // Save-preset overlay (F2) + the OS Load chooser (#3)
    Backdrop savePresetBackdrop;
    juce::Label      saveTitle;
    juce::TextEditor saveNameEditor;
    juce::TextButton saveOkButton { "Save" }, saveCancelButton { "Cancel" };
    // ADR-0036 ROUND 28b (Devin R405-406). WHICH SAVE ATTEMPT THE DIALOG IN FRONT OF THE USER
    // BELONGS TO, and it is not the same question as "does the editor still exist".
    //
    // Round 27 made the save's completion asynchronous (R640): a save issued from inside a user
    // transaction is queued, and the dialog waits for the answer. The completion captured a
    // `SafePointer`, which answers EDITOR LIFETIME and nothing else -- so a user who cancels the
    // dialog, opens it again and starts a second save has the FIRST save's completion land on the
    // SECOND save's dialog: closing it, or painting "SAVE FAILED" on it, or taking its focus, for
    // a file operation that has nothing to do with what is on screen.
    //
    // The owner's ruling is that cancelling the dialog cancels the UI ASSOCIATION and not the file
    // operation: a write already queued may still complete and its result must still be processed
    // internally (the preset list and the dirty mark really did change). So identity is carried
    // explicitly. `nextSaveAttempt` only ever increases; `saveAttempt` is the attempt the dialog
    // currently belongs to, and it is cleared to 0 by EVERY show and EVERY hide -- cancel, Escape,
    // the backdrop dismiss and a successful close all go through `showSavePreset`. A completion
    // whose captured id is not `saveAttempt` touches no part of the dialog.
    //
    // A counter rather than a token object because the whole question is "is this still the one",
    // and a `uint32` answers it with no allocation, no lifetime and nothing to keep in step. It is
    // message-thread-only: both the click and the completion run there.
    juce::uint32 saveAttempt     = 0;
    juce::uint32 nextSaveAttempt = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::OwnedArray<AttachmentWitness>  writeWitnesses;   // ADR-0008 round 18, see the class above
    // ROUND 27: the slot the A/B letter was last painted for. -1 forces the first tick to
    // paint, which costs one repaint at editor construction and removes a special case.
    int lastAbSlot = -1;
    juce::OwnedArray<SliderAttachment>   sliderAtts;
    juce::OwnedArray<ButtonAttachment>   buttonAtts;
    juce::OwnedArray<ComboBoxAttachment> comboAtts;
    juce::Array<juce::ComboBox*>         allCombos; // timer-driven hover repaint (#20)

    bool  advanced = false;
    bool  tooltipsOn = false;   // tooltips default OFF
    bool  metersOn = false;
    bool  msState = false;      // cached M/S decoder state (drives L/R<->M/S labels, #12/#13)

    // 24 Hz timer memoisation (Wave 4). The shown preset text is a pure
    // function of (name, dirty, slot width): the GlyphArrangement shaping in
    // refreshPresetDisplay re-runs only when one of them changed. The combo
    // hover poll is pre-gated by one editor-level cursor test (the S11 idiom):
    // with the cursor outside no visible box can contain it, so the per-box
    // queries only run while the cursor is inside or a box is still lit. The
    // match readout re-formats only when the raw published float changed
    // (bitwise compare, so even a NaN transition still updates).
    juce::String presetShownName;      // pm.currentName() the shaping last ran for
    bool  presetShownDirty = false;
    int   presetShownWidth = -1;       // presetName.getWidth() it last ran for
    bool  comboHoverLit = false;       // some box's "hov" property is currently set
    float shownMatchGainDb = -1.0e9f;  // raw getMatchGainDb() last formatted
    float meterAnim = 0.0f;     // 0..1 eased meter reveal (#19)
    bool  persistDragging = false; // dragging the Settings Persistence bar (#26)
    int   persistHold = 0;      // frames the Persistence bar has been held (anti-flicker, #7)
    // Non-drag (scroll / type) reveal: a sustained adjustment turns the window
    // see-through and holds it ~0.5 s after the last change; a single nudge does
    // not trigger it (#1).
    double persistScrollWindow = 0.0;
    double persistRevealTimer  = 0.0;

    // Meter reveal runs on the display's vblank (not the 24 Hz timer) and lays
    // out ONLY the scope/meter block per frame -- the full-window relayout per
    // coarse timer tick is what stuttered (#6). Same ease curve, time-based.
    juce::VBlankAttachment meterVBlank;
    double lastFrameTime = 0.0;

    // Micro-animation driver (F3): per-frame eased "hovA"/"actA"/"onA" component
    // properties the LookAndFeel blends with; repaints fire only while moving.
    // The widget type is resolved ONCE at registration (S11) -- previously two
    // dynamic_casts per widget per display frame.
    struct AnimatedWidget
    {
        juce::Component*    comp   = nullptr;
        juce::Slider*       slider = nullptr; // set when comp is a Slider
        juce::ToggleButton* toggle = nullptr; // set when comp is a ToggleButton
    };
    juce::Array<AnimatedWidget> animated;
    juce::uint64 microProbe = 0;   // slider-value/toggle-state fingerprint of the last pass (S11)
    bool microSettled = false;     // the last pass moved nothing (S11)
    // ...and the last pass left something LIT. microSettled alone is a MOTION latch: it reads the
    // same with hovA parked at 0.0 and at 1.0, so on its own it let the idle gate seal on a control
    // that was still glowing (KI-025). The resting value of hovA/actA with the cursor outside is 0,
    // so "nothing is lit" is the missing half of "provably static".
    bool microLit = false;
    // Generations the micro-anim poll last re-armed on (H15): sound params, view
    // params (Bypass) and the host-hidden InternalState. Init 0 vs the counters'
    // initial 1 -> the first frame always runs a full pass.
    juce::uint32 microSoundGen = 0, microViewGen = 0, microInternalGen = 0;
    bool uiAnimOn = true;
    float hostScale = 1.0f;             // host display/DPI scale (Windows), composed with the UI scale
    int  lastScaleIdx = -1;             // applied UI-scale step (F4)
    int  comboFontMode = -1;            // Widen combo LnF mode last applied (sync font with resize, 0.6.17 #4)
    int  brPrevAlgo = -1;              // last Widen algorithm seen, for the bottom-right knob sweep (#8)
    // Knobs/sliders only EASE to a new value during this short window, which is
    // opened by a preset / A-B / undo / algorithm change; a scroll-wheel or host
    // automation edit leaves it closed, so those snap and never mislead (#3).
    double knobSweepTime = 0.0;

    // SIMPLE is 940x720. ADVANCED stacks four full-width tiers (0.6.8 #7):
    //   top bar | scope+Widen row | full-width MULTIBAND | INPUT|OUTPUT block.
    static constexpr int kWidth     = 940;
    static constexpr int kHeight    = 720;  // SIMPLE window height (scope + Widen)
    static constexpr int kScopeRowH = 474;  // scope/meters + Widen row (advanced)
    static constexpr int kMultiBarH = 176;  // full-width Multiband bar (advanced)
    static constexpr int kIoH       = 204;  // INPUT | OUTPUT horizontal block (advanced)
    static constexpr int kAdvHeight = 46 + kScopeRowH + kMultiBarH + kIoH; // 900

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnamorphAudioProcessorEditor)
};
