#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"
#include "gui/Vectorscope.h"
#include "gui/SpectrumImager.h"
#include "gui/CorrelationMeter.h"
#include "gui/LevelMeter.h"

// ============================================================================
//  AnamorphAudioProcessorEditor  (v0.3 UI pass)
// ============================================================================
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

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

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
    //      1331-1333) and the combo "hov" flag does the same (:1072-1073). That is the v0.6.1
    //      stuck-hover fix, and it makes hovA immune to componentUnderMouse churn by construction.
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
        // AnamorphAudioProcessorEditor::mouseWheelMove -- harmless today, since the Persist-reveal
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
        float  reveal = 0.0f;   // 0 = solid, 1 = see-through (Persist drag, #26)
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override; // Persist scroll reveal (#1)
    void applyUiScale();                 // whole-window XS..XL transform scale (F4)
    void refreshPresetDisplay();         // preset name + dirty mark (F2)
    void showPresetMenu();
    void showSavePreset (bool);
    void focusSaveNameField (int attemptsLeft); // deferred, verified grab (Space-vs-host fix)
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
        juce::String getTipFor (juce::Component& c) override
        {
            if (tooltipsEnabled && ! tooltipsEnabled()) return {};
            return juce::TooltipWindow::getTipFor (c);
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
    juce::Array<juce::Component::SafePointer<juce::Component>> openMenus;
    int  presetMenusOpen = 0;
    void notePopupMenuOpened (juce::Component& menuWindow);
    void refreshPopupShield();   // prunes dead windows and shows/hides the shield
    void dismissTrackedPopupMenus();   // cancels every pop-up this editor owns, unconditionally
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
    struct Knob : public juce::Slider
    {
        double resetValue = 0.0;
        // The attached parameter (null for host-hidden InternalState knobs): the
        // ALT-CLICK reset must open its own host change gesture. Our mouseDown
        // intercepts the event BEFORE juce::Slider can start a drag, so the
        // SliderAttachment never opens one, and the programmatic setValue reached
        // the processor's undo coalescer gesture-less -- which is the automation
        // path, folded into the baseline with NO undo step and NO redo clear.
        // That is why Option/Alt-click reset was un-undoable (and left Redo
        // alive). The DOUBLE-CLICK reset needs no wrap: its second press runs
        // Slider::mouseDown first, whose ScopedDragNotification has already
        // opened the drag gesture that mouseUp will close -- wrapping there
        // would nest begin/endChangeGesture on the same parameter.
        juce::RangedAudioParameter* resetParam = nullptr;
        std::function<void()> onSweep;

        void doReset()
        {
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
            if (e.mods.isAltDown()) // Option/Alt-click reset, as ONE undoable user gesture
            {
                if (resetParam != nullptr) resetParam->beginChangeGesture();
                doReset();
                if (resetParam != nullptr) resetParam->endChangeGesture();
                return;
            }
            juce::Slider::mouseDown (e);
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (e.getNumberOfClicks() == 2) doReset();
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
    juce::Label persistLabel;   // Persist moved into Settings as a bar (#21)

    // Save-preset overlay (F2) + the OS Load chooser (#3)
    Backdrop savePresetBackdrop;
    juce::Label      saveTitle;
    juce::TextEditor saveNameEditor;
    juce::TextButton saveOkButton { "Save" }, saveCancelButton { "Cancel" };
    std::unique_ptr<juce::FileChooser> fileChooser;

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
    bool  persistDragging = false; // dragging the Settings Persist bar (#26)
    int   persistHold = 0;      // frames the Persist bar has been held (anti-flicker, #7)
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
