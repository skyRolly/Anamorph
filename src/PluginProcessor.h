#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginParameters.h"
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

    // Auto-Gain "Apply": locks the measured loudness-match gain into Output Gain.
    void applyAutoGain();

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void updateLatency();

    juce::AudioProcessorValueTreeState apvts;
    ParamPointers params;
    anamorph::AnamorphEngine engine;

    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnamorphAudioProcessor)
};
