#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"
#include "gui/Vectorscope.h"
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
        void paint (juce::Graphics&) override;
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
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { if (onToggle) onToggle(); }
    };

    void timerCallback() override;
    void setupRotary (juce::Slider&, juce::Label&, const juce::String& name, const juce::String& tip);
    void attachSlider (juce::Slider&, const char* id);
    void setupCombo (juce::ComboBox&, const char* id, const juce::String& tip);
    void setupToggle (juce::ToggleButton&, const char* id, const juce::String& text, const juce::String& tip);
    void updateAlgoControls();
    void updateModeVisibility();
    void showAbout (bool);
    void showSettings (bool);
    void applyTooltipsEnabled();
    void applyScopePersist();

    AnamorphAudioProcessor& processor;
    anamorph::gui::AnamorphLookAndFeel lnf;
    anamorph::gui::CompactComboLookAndFeel compactCombo; // smaller list for Input combos (#12)
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

    // Knob: a slider that resets to its default on a clean double-click only --
    // a triple-or-more click no longer resets on every extra click (feedback #6).
    struct Knob : public juce::Slider
    {
        double resetValue = 0.0;
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (e.getNumberOfClicks() == 2)
                setValue (resetValue, juce::sendNotificationSync);
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

    // MULTIBAND module (advanced, its own section -- #16)
    juce::Label  multibandLabel;
    juce::ToggleButton mbEnableToggle;
    Knob mbFreqLowK, mbFreqHighK, mbWLowK, mbWMidK, mbWHighK;
    juce::Label  mbFreqLowL, mbFreqHighL, mbWLowL, mbWMidL, mbWHighL;
    Knob scopePersistK; juce::Label scopePersistL;

    // Overlays
    DimLayer dimOverlay;
    Backdrop aboutBackdrop, settingsBackdrop;
    juce::HyperlinkButton aboutLink { "www.rolly.tech", juce::URL ("https://www.rolly.tech") }; // #4

    // Settings controls
    juce::ComboBox oversampleBox;  juce::Label oversampleLabel;
    juce::ToggleButton tooltipsToggle;
    juce::Label settingsTitle;
    juce::Label persistLabel;   // Persist moved into Settings as a bar (#21)

    juce::OwnedArray<SliderAttachment>   sliderAtts;
    juce::OwnedArray<ButtonAttachment>   buttonAtts;
    juce::OwnedArray<ComboBoxAttachment> comboAtts;

    bool  advanced = false;
    bool  tooltipsOn = false;   // tooltips default OFF
    bool  metersOn = false;
    float meterAnim = 0.0f;     // 0..1 eased meter reveal (#19)
    bool  persistDragging = false; // dragging the Settings Persist bar (#26)

    // Single fixed window for both modes: toggling Advanced relays out the
    // content in place, so the host never resizes us and nothing flickers (#20).
    static constexpr int kWidth  = 940;
    static constexpr int kHeight = 720;
    static constexpr int kStripHeight     = 200;  // advanced INPUT/MULTIBAND strip

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnamorphAudioProcessorEditor)
};
