#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EngineParameters.h"

// ============================================================================
//  PluginParameters
//
//  Single source of truth for the parameter tree. Builds the APVTS layout,
//  caches raw atomic pointers (no per-block string lookups), and converts the
//  host/automation state into the format-agnostic anamorph::EngineParameters
//  snapshot that drives the DSP core.
// ============================================================================
namespace pid
{
    // Input conditioning
    inline constexpr const char* channelMode    = "channelMode";
    inline constexpr const char* monoSum        = "monoSum";
    inline constexpr const char* swap           = "swap";
    inline constexpr const char* inputBalance   = "inputBalance";
    inline constexpr const char* polarityL      = "polarityL";
    inline constexpr const char* polarityR      = "polarityR";
    // MS
    inline constexpr const char* msMode         = "msMode";
    // Effect engine
    inline constexpr const char* drive          = "drive";
    inline constexpr const char* algorithm      = "algorithm";
    inline constexpr const char* amount         = "amount";
    inline constexpr const char* haasDelay      = "haasDelay";
    inline constexpr const char* haasSide       = "haasSide";
    inline constexpr const char* velvetDensity  = "velvetDensity";
    inline constexpr const char* chorusRate     = "chorusRate";
    inline constexpr const char* chorusDepth    = "chorusDepth";
    inline constexpr const char* dimMode        = "dimMode";
    inline constexpr const char* width          = "width";
    // Multiband
    inline constexpr const char* mbEnable       = "mbEnable";
    inline constexpr const char* mbFreqLow      = "mbFreqLow";
    inline constexpr const char* mbFreqHigh     = "mbFreqHigh";
    inline constexpr const char* mbWidthLow     = "mbWidthLow";
    inline constexpr const char* mbWidthMid     = "mbWidthMid";
    inline constexpr const char* mbWidthHigh    = "mbWidthHigh";
    // Mono maker
    inline constexpr const char* monoMakerOn    = "monoMakerOn";
    inline constexpr const char* monoMakerFreq  = "monoMakerFreq";
    // Mix / gain
    inline constexpr const char* mix            = "mix";
    inline constexpr const char* outputGain     = "outputGain";
    inline constexpr const char* outputBalance  = "outputBalance";
    inline constexpr const char* autoGainMatch  = "autoGainMatch";
    // Monitoring
    inline constexpr const char* solo           = "solo";
    // Oversampling
    inline constexpr const char* oversample     = "oversample";
    // Bypass
    inline constexpr const char* bypass         = "bypass";
    // UI-only (still saved with state)
    inline constexpr const char* advancedMode   = "advancedMode";
    inline constexpr const char* scopePersist   = "scopePersist";
    inline constexpr const char* metersOn       = "metersOn";   // persist Meters toggle (#15)
    inline constexpr const char* tooltipsOn     = "tooltipsOn"; // persist Tooltips toggle (#15)
}

juce::AudioProcessorValueTreeState::ParameterLayout createAnamorphLayout();

// Cached raw atomic pointers + conversion to the DSP snapshot.
struct ParamPointers
{
    void bind (juce::AudioProcessorValueTreeState& s);
    anamorph::EngineParameters toEngine() const;

    std::atomic<float>* channelMode = nullptr;
    std::atomic<float>* monoSum = nullptr;
    std::atomic<float>* swap = nullptr;
    std::atomic<float>* inputBalance = nullptr;
    std::atomic<float>* polarityL = nullptr;
    std::atomic<float>* polarityR = nullptr;
    std::atomic<float>* msMode = nullptr;
    std::atomic<float>* drive = nullptr;
    std::atomic<float>* algorithm = nullptr;
    std::atomic<float>* amount = nullptr;
    std::atomic<float>* haasDelay = nullptr;
    std::atomic<float>* haasSide = nullptr;
    std::atomic<float>* velvetDensity = nullptr;
    std::atomic<float>* chorusRate = nullptr;
    std::atomic<float>* chorusDepth = nullptr;
    std::atomic<float>* dimMode = nullptr;
    std::atomic<float>* width = nullptr;
    std::atomic<float>* mbEnable = nullptr;
    std::atomic<float>* mbFreqLow = nullptr;
    std::atomic<float>* mbFreqHigh = nullptr;
    std::atomic<float>* mbWidthLow = nullptr;
    std::atomic<float>* mbWidthMid = nullptr;
    std::atomic<float>* mbWidthHigh = nullptr;
    std::atomic<float>* monoMakerOn = nullptr;
    std::atomic<float>* monoMakerFreq = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* outputBalance = nullptr;
    std::atomic<float>* autoGainMatch = nullptr;
    std::atomic<float>* solo = nullptr;
    std::atomic<float>* oversample = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* advancedMode = nullptr;
};
