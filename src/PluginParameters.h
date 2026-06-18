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
    // Multiband (1..4 bands, up to 3 crossovers)
    inline constexpr const char* mbEnable       = "mbEnable";
    inline constexpr const char* mbBands        = "mbBands";     // active band count 1..4
    inline constexpr const char* mbSolo         = "mbSolo";      // 4-bit solo mask (bit b = band b)
    inline constexpr const char* mbFreqLow      = "mbFreqLow";   // band 1|2
    inline constexpr const char* mbFreqMid      = "mbFreqMid";   // band 2|3
    inline constexpr const char* mbFreqHigh     = "mbFreqHigh";  // band 3|4
    inline constexpr const char* mbWidthLow     = "mbWidthLow";  // band 1
    inline constexpr const char* mbWidthMid     = "mbWidthMid";  // band 2
    inline constexpr const char* mbWidthHiMid   = "mbWidthHiMid";// band 3
    inline constexpr const char* mbWidthHigh    = "mbWidthHigh"; // band 4
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
    inline constexpr const char* uiAnimations   = "uiAnimations"; // micro-animation toggle (F3)
    inline constexpr const char* uiScale        = "uiScale";      // window scale XS..XL (F4)

    // The shared "view" / Settings parameters: never part of A/B, undo history
    // or presets -- one list so every consumer stays in sync.
    inline constexpr const char* const viewParams[] = {
        bypass, advancedMode, oversample, metersOn, tooltipsOn, scopePersist,
        uiAnimations, uiScale, mbSolo
    };

    inline bool isViewParam (const juce::String& id) noexcept
    {
        for (auto* v : viewParams)
            if (id == v) return true;
        return false;
    }
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
    std::atomic<float>* mbBands = nullptr;
    std::atomic<float>* mbSolo = nullptr;
    std::atomic<float>* mbFreqLow = nullptr;
    std::atomic<float>* mbFreqMid = nullptr;
    std::atomic<float>* mbFreqHigh = nullptr;
    std::atomic<float>* mbWidthLow = nullptr;
    std::atomic<float>* mbWidthMid = nullptr;
    std::atomic<float>* mbWidthHiMid = nullptr;
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
