#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"
#include "gui/Vectorscope.h"
#include "gui/CorrelationMeter.h"

// ============================================================================
//  AnamorphAudioProcessorEditor  (v0.2 UI pass)
//
//  Clean, premium two-mode UI. Simple Mode shows only the core widening /
//  output controls around the vectorscope; Advanced Mode reveals the grouped
//  INPUT module + multiband. Top bar carries A/B (single toggle + Copy),
//  Settings, Bypass, Advanced and undo/redo. Title opens an in-window About
//  overlay. An OpenGL context GPU-composites everything.
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

    // A translucent backdrop hosting a centred panel (About / Settings). Clicking
    // outside the panel dismisses it -- no close button, in-window, modern (#18).
    struct Backdrop : public juce::Component
    {
        std::function<void()> onDismiss;
        juce::Rectangle<int>  panel;
        bool   aboutText = false;       // draw the About copy
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (aboutText || ! panel.contains (e.getPosition()))
                if (onDismiss) onDismiss();
        }
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

    // A/B compare (FabFilter-style: one slot button + Copy)
    void captureTo (int slot);
    void switchTo (int slot);
    void copyCurrentToOther();

    AnamorphAudioProcessor& processor;
    anamorph::gui::AnamorphLookAndFeel lnf;
    juce::OpenGLContext openGLContext;
    juce::TooltipWindow tooltips { nullptr, 600 };

    // Visual centrepiece
    std::unique_ptr<anamorph::gui::Vectorscope> scope;
    std::unique_ptr<anamorph::gui::StereoMeter> balanceMeter, corrMeter;

    // Top bar
    juce::TextButton   titleButton;                 // invisible hit-area over the wordmark
    juce::TextButton   abButton { "A" }, copyButton { "COPY" };
    juce::TextButton   settingsButton { "SETTINGS" };
    juce::TextButton   undoButton { juce::String::charToString ((juce::juce_wchar) 0x21B6) };
    juce::TextButton   redoButton { juce::String::charToString ((juce::juce_wchar) 0x21B7) };
    juce::ToggleButton advancedToggle, bypassToggle;

    // WIDEN module
    juce::ComboBox algorithmBox, haasSideBox, dimModeBox;
    juce::Label    algorithmLabel;
    juce::Slider driveK, amountK, widthK;
    juce::Label  driveL, amountL, widthL;
    juce::Slider haasDelayK, velvetK, chorusRateK, chorusDepthK;
    juce::Label  haasDelayL, velvetL, chorusRateL, chorusDepthL;

    // OUTPUT module
    juce::Slider mixK, outputK, outBalanceK;
    juce::Label  mixL, outputL, outBalanceL;
    juce::ToggleButton autoMatchToggle;
    juce::TextButton   applyGainButton { "Apply" };
    juce::Label        matchReadout;

    // MONO MAKER (slim bar)
    juce::ToggleButton monoMakerToggle;
    juce::Slider monoFreqK;  juce::Label monoFreqL;

    // INPUT module (advanced)
    juce::ComboBox channelModeBox, soloBox;
    juce::Label    channelModeLabel, soloLabel, inputModuleLabel;
    juce::ToggleButton monoToggle, swapToggle, msToggle, polLToggle, polRToggle;
    juce::Slider balanceK; juce::Label balanceL;

    // MULTIBAND (advanced)
    juce::ToggleButton mbEnableToggle;
    juce::Slider mbFreqLowK, mbFreqHighK, mbWLowK, mbWMidK, mbWHighK;
    juce::Label  mbFreqLowL, mbFreqHighL, mbWLowL, mbWMidL, mbWHighL;
    juce::Slider scopePersistK; juce::Label scopePersistL;

    // Bypass dim (painted on top, but does NOT block mouse -- #4)
    struct DimLayer : public juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0x66090b0e)); }
    };
    DimLayer dimOverlay;
    Backdrop aboutBackdrop, settingsBackdrop;

    // Settings controls (live inside the settings panel)
    juce::ComboBox oversampleBox;  juce::Label oversampleLabel;
    juce::ToggleButton zeroLatencyToggle, tooltipsToggle;
    juce::Label settingsTitle;

    juce::OwnedArray<SliderAttachment>   sliderAtts;
    juce::OwnedArray<ButtonAttachment>   buttonAtts;
    juce::OwnedArray<ComboBoxAttachment> comboAtts;

    // A/B state
    juce::ValueTree stateA, stateB;
    int  activeSlot = 0;     // 0 = A, 1 = B
    bool advanced = false;
    bool tooltipsOn = true;

    static constexpr int kWidth = 920;
    static constexpr int kHeightSimple = 560;
    static constexpr int kHeightAdvanced = 770;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnamorphAudioProcessorEditor)
};
