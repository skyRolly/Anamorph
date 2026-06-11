#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginParameters.h"
#include "PresetManager.h"
#include "dsp/AnamorphEngine.h"

// ============================================================================
//  AnamorphAudioProcessor
//
//  The VST3 / Standalone format wrapper. Owns the APVTS (parameter tree, state
//  save/recall, host automation) and the format-agnostic AnamorphEngine.
//  Declares the two supported I/O layouts: stereo->stereo and mono->stereo
//  (the "turn Mono into Stereo" headline feature). Output is always stereo.
// ============================================================================
class AnamorphAudioProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener
{
public:
    AnamorphAudioProcessor();
    ~AnamorphAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Anamorph"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // --- editor access ---
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    anamorph::AnamorphEngine& getEngine() noexcept          { return engine; }
    anamorph::PresetManager&  getPresets() noexcept         { return presets; }

    // Custom Undo/Redo: each A/B slot keeps its OWN stack of SOUND-param
    // snapshots; the "view"/Settings params (Bypass, Advanced, Meters, Tooltips,
    // Oversampling, Persist) and A/B switches themselves are never recorded
    // (feedback #10 / #11 / #12). The editor calls pollUndoCoalesce() on its timer
    // to fold a knob gesture into a single step.
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! abUndo[abActive].undo.empty(); }
    bool canRedo() const noexcept { return ! abUndo[abActive].redo.empty(); }
    void pollUndoCoalesce();

    // Auto-Gain "Apply": locks the measured loudness-match gain into Output Gain.
    void applyAutoGain();

    // A/B compare lives in the processor so it survives editor close / session
    // recall. Switching A/B never touches the shared view/Settings params (#13).
    int  abActiveSlot() const noexcept { return abActive; }
    void abSwitchTo (int slot);
    void abCopyToOther();

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void updateLatency();

    // A/B helpers (preserve the shared view/Settings params across a slot apply)
    void abEnsureInit();
    void abApplySlot (int slot);

    // Undo helpers
    static bool isViewParam (const juce::String& id) noexcept;
    juce::String soundSignature() const;
    void applyStatePreservingView (const juce::ValueTree& target);
    void syncCommitted();

    struct UndoStacks { std::vector<juce::ValueTree> undo, redo; };
    UndoStacks abUndo[2];
    juce::ValueTree committedState;
    juce::String committedSig, lastPolledSig;

    juce::ValueTree abSlotA, abSlotB;
    int abActive = 0;
    float abMatchGain[2] = { 0.0f, 0.0f }; // remembered Level-Match per A/B slot (#23)

    juce::AudioProcessorValueTreeState apvts;
    ParamPointers params;
    anamorph::PresetManager presets { apvts }; // top-bar preset browser backing (F2)
    anamorph::AnamorphEngine engine;

    juce::AudioParameterBool* bypassParam = nullptr;
    bool prevPlaying = false; // transport edge-detect for meter reset (#15)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnamorphAudioProcessor)
};
