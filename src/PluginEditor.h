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
                                     private juce::Timer
{
public:
    explicit AnamorphAudioProcessorEditor (AnamorphAudioProcessor&);
    ~AnamorphAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

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
    void showLoadPreset();               // OS file chooser (#3)
    void setupRotary (juce::Slider&, juce::Label&, const juce::String& name, const juce::String& tip);
    void attachSlider (juce::Slider&, const char* id);
    void setupCombo (juce::ComboBox&, const char* id, const juce::String& tip);
    void passComboHoverThrough (juce::ComboBox&); // let hover reach the whole box (recurring)
    void setupToggle (juce::ToggleButton&, const char* id, const juce::String& text, const juce::String& tip);
    void updateAlgoControls();
    void updateModeVisibility();
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
    juce::TooltipWindow tooltips { nullptr, 600 };

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
        std::function<void()> onSweep;

        void doReset()
        {
            setValue (resetValue, juce::sendNotificationSync);
            if (onSweep) onSweep();
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isAltDown()) { doReset(); return; } // Option/Alt-click reset
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
    float meterAnim = 0.0f;     // 0..1 eased meter reveal (#19)
    bool  persistDragging = false; // dragging the Settings Persist bar (#26)
    int   persistHold = 0;      // frames the Persist bar has been held (anti-flicker, #7)
    // Non-drag (scroll / type) reveal: a sustained adjustment turns the window
    // see-through and holds it ~0.5 s after the last change; a single nudge does
    // not trigger it (#1).
    double persistScrollWindow = 0.0;
    double persistRevealTimer  = 0.0;
    int   widenOutputDividerY = 0; // y of the Widen/Output divider in advanced (#10/#11)

    // Meter reveal runs on the display's vblank (not the 24 Hz timer) and lays
    // out ONLY the scope/meter block per frame -- the full-window relayout per
    // coarse timer tick is what stuttered (#6). Same ease curve, time-based.
    juce::VBlankAttachment meterVBlank;
    double lastFrameTime = 0.0;

    // Micro-animation driver (F3): per-frame eased "hovA"/"actA"/"onA" component
    // properties the LookAndFeel blends with; repaints fire only while moving.
    juce::Array<juce::Component*> animated;
    bool uiAnimOn = true;
    int  lastScaleIdx = -1;             // applied UI-scale step (F4)
    int  brPrevAlgo = -1;              // last Widen algorithm seen, for the bottom-right knob sweep (#8)
    // Knobs/sliders only EASE to a new value during this short window, which is
    // opened by a preset / A-B / undo / algorithm change; a scroll-wheel or host
    // automation edit leaves it closed, so those snap and never mislead (#3).
    double knobSweepTime = 0.0;

    // The upper region is the same 940x720 in both modes, with the scope/meter
    // block at its original size (0.6.7 #2). Advanced GROWS the window downward by
    // kMultiBarH for a full-width MULTIBAND bar, instead of squeezing the scope.
    static constexpr int kWidth     = 940;
    static constexpr int kHeight    = 720;  // top bar + scope/INPUT + right panel
    static constexpr int kStripHeight = 200; // INPUT strip at the bottom of the left column
    static constexpr int kMultiBarH   = 196; // full-width Multiband bar, advanced only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnamorphAudioProcessorEditor)
};
