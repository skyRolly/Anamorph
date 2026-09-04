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
    // Bypass
    inline constexpr const char* bypass         = "bypass";
    // UI-only (still saved with state)
    inline constexpr const char* advancedMode   = "advancedMode";
    // Oversampling, Scope Persistence, Show Meters, Tooltips, UI Animations and UI Scale
    // are NO LONGER APVTS parameters -- they live in anamorph::InternalState (host-hidden) so
    // the host can't list them. Their legacy string ids are referenced only by the one-time
    // migration in InternalState::migrateFromLegacyApvts (read directly as literals).

    // The shared "view" parameters that, although still APVTS parameters, must never be
    // part of A/B, undo history or presets. Only Bypass remains here now: the Settings
    // controls + Show Meters were moved OUT of the APVTS entirely (anamorph::InternalState)
    // so the host can't list them, and advancedMode (ADV) travels with A/B (Issue 4).
    inline constexpr const char* const viewParams[] = {
        bypass
    };

    inline bool isViewParam (const juce::String& id) noexcept
    {
        for (auto* v : viewParams)
            if (id == v) return true;
        return false;
    }

    // Presets additionally never store or recall the solo mask OR the ADV toggle:
    // loading a preset leaves the live solo reset to off and ADV untouched, so the
    // sound presets don't fight the view state (0.6.10 #9, Issue 4).
    inline bool isPresetExcluded (const juce::String& id) noexcept
    {
        return isViewParam (id) || id == mbSolo || id == advancedMode;
    }
}

// ---------------------------------------------------------------------------
//  THE SOUND VALUE, AS THE PLUG-IN ACTUALLY RENDERS AND STORES IT.
//
//  A parameter's normalised `getValue()` is not always the value that reaches the
//  DSP or a file. The DSP reads `getRawParameterValue()`, which is the DENORMALISED
//  value -- `convertFrom0to1(getValue())`, snapped to the range's interval -- and both
//  the preset format and the APVTS session tree store that same denormalised number.
//  `RawChoice` / `RawBool` above keep the EXACT normalised value in `getValue()` on
//  purpose (pluginval sets a raw normalised value and reads it back), so for those the
//  two differ: a 4-choice automated to 0.66 renders, and stores, as index 2.
//
//  This maps a normalised value onto the grid the plug-in can actually render and keep.
//  Every "has the sound changed?" signature is built from it, so they all answer the
//  same question -- the preset modified-marker (`PresetManager::soundSignatureFor`) and
//  the undo / A-B coalescer (`AnamorphAudioProcessor::soundSignature`) alike. Before
//  D-2 round 9 they used the raw normalised value, which made a sub-step move on a
//  discrete parameter count as an edit even though nothing about the sound, the file or
//  the DSP input changed by it (ADR-0036 §17).
//
//  It is a no-op for every stock `juce::AudioParameterFloat`, which already stores the
//  denormalised value and reports `convertTo0to1` of it. APPLY IT EXACTLY ONCE per
//  value: for the frequency ranges built from custom log/exp conversion lambdas it is
//  the identity in real arithmetic but NOT idempotent in float, so a second application
//  moves the last bits and can cross a decimal rounding boundary in a signature. A value
//  resolved out of a saved tree has already had it applied, because the tree holds the
//  denormalised number -- see PresetManager::soundSignatureForSavedTree.
// ---------------------------------------------------------------------------
inline float normalisedAsRendered (const juce::RangedAudioParameter& rp, float normalised) noexcept
{
    return rp.convertTo0to1 (rp.convertFrom0to1 (normalised));
}

// The same question for a parameter that may not be ranged: a non-ranged parameter has
// no grid to land on, so its own value is already the answer.
inline float normalisedAsRendered (const juce::AudioProcessorParameter& p) noexcept
{
    if (auto* rp = dynamic_cast<const juce::RangedAudioParameter*> (&p))
        return normalisedAsRendered (*rp, rp->getValue());
    return p.getValue();
}

juce::AudioProcessorValueTreeState::ParameterLayout createAnamorphLayout();

// Cached raw atomic pointers + conversion to the DSP snapshot.
struct ParamPointers
{
    void bind (juce::AudioProcessorValueTreeState& s);
    // oversampleIndex (0..3) comes from anamorph::InternalState, not the APVTS, because
    // Oversampling is no longer a host parameter (it is hidden from the host's list).
    anamorph::EngineParameters toEngine (int oversampleIndex) const;

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
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* advancedMode = nullptr;
};
