#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"
#include "gui/Vectorscope.h"
#include "gui/CorrelationMeter.h"

// ============================================================================
//  AnamorphAudioProcessorEditor
//
//  Clean, premium two-mode UI (spec section 5 / 10). Simple Mode shows only the
//  core controls around the vectorscope centrepiece; Advanced Mode reveals the
//  rest (MS, multiband, monitoring, oversampling, ...). An OpenGL context is
//  attached so all rendering -- especially the vectorscope -- is GPU-composited.
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

    void timerCallback() override;
    void setupRotary (juce::Slider&, juce::Label&, const juce::String& name);
    void attachSlider (juce::Slider&, const char* id);
    void setupCombo (juce::ComboBox&, const char* id);
    void setupToggle (juce::ToggleButton&, const char* id, const juce::String& text);
    void updateAlgoControls();
    void updateModeVisibility();

    // A/B compare
    void captureTo (int slot);
    void switchTo (int slot);
    void copyAB (int from, int to);

    AnamorphAudioProcessor& processor;
    anamorph::gui::AnamorphLookAndFeel lnf;
    juce::OpenGLContext openGLContext;

    // Visual centrepiece
    std::unique_ptr<anamorph::gui::Vectorscope>       scope;
    std::unique_ptr<anamorph::gui::CorrelationMeter>  corrH, corrV;

    // Top bar
    juce::TextButton aButton { "A" }, bButton { "B" }, copyToB { "A>B" }, copyToA { "B>A" };
    juce::ToggleButton advancedToggle, bypassToggle;

    // Combos
    juce::ComboBox algorithmBox, channelModeBox, haasSideBox, dimModeBox, soloBox, oversampleBox;
    juce::Label    algorithmLabel, channelModeLabel, soloLabel, oversampleLabel;

    // Core rotary controls
    juce::Slider driveK, widthK, mixK, outputK;
    juce::Label  driveL, widthL, mixL, outputL;

    // Algorithm-specific
    juce::Slider haasDelayK, velvetK, chorusRateK, chorusDepthK, dimAmountK;
    juce::Label  haasDelayL, velvetL, chorusRateL, chorusDepthL, dimAmountL;

    // Mono maker
    juce::ToggleButton monoMakerToggle;
    juce::Slider monoFreqK;  juce::Label monoFreqL;

    // Auto gain
    juce::ToggleButton autoMatchToggle;
    juce::TextButton   applyGainButton { "Apply" };
    juce::Label        matchReadout;

    // Advanced: input conditioning + monitoring
    juce::ToggleButton swapToggle, polLToggle, polRToggle, msToggle;
    juce::Slider balanceK; juce::Label balanceL;

    // Advanced: multiband
    juce::ToggleButton mbEnableToggle;
    juce::Slider mbFreqLowK, mbFreqHighK, mbWLowK, mbWMidK, mbWHighK;
    juce::Label  mbFreqLowL, mbFreqHighL, mbWLowL, mbWMidL, mbWHighL;

    // Advanced: scope persistence
    juce::Slider scopePersistK; juce::Label scopePersistL;

    juce::OwnedArray<SliderAttachment>   sliderAtts;
    juce::OwnedArray<ButtonAttachment>   buttonAtts;
    juce::OwnedArray<ComboBoxAttachment> comboAtts;

    // A/B state
    juce::ValueTree stateA, stateB;
    int activeSlot = 0; // 0 = A, 1 = B
    bool advanced = false;

    static constexpr int kWidth = 920;
    static constexpr int kHeightSimple = 560;
    static constexpr int kHeightAdvanced = 760;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnamorphAudioProcessorEditor)
};
