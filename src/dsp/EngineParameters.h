#pragma once

// ============================================================================
//  EngineParameters
//
//  A plain-old-data snapshot of every value the DSP engine needs for a block.
//  The plugin wrapper (PluginProcessor) reads the APVTS atomics once per block
//  and fills this struct, then hands it to AnamorphEngine. This keeps the DSP
//  core fully decoupled from JUCE's AudioProcessorValueTreeState / plugin
//  format wrapper -- the engine knows nothing about parameter IDs or hosts.
// ============================================================================

namespace anamorph
{

enum class ChannelMode  { Stereo = 0, Mono, LeftOnly, RightOnly };
enum class Algorithm    { Haas = 0, Velvet, Chorus, DimensionD };
enum class HaasSide     { Left = 0, Right };
enum class SoloMode     { Off = 0, Mid, Side };
enum class OversampleFactor { Off = 0, x2, x4, x8 };

struct EngineParameters
{
    // --- 1. Input conditioning -------------------------------------------
    ChannelMode channelMode = ChannelMode::Stereo;
    bool        swapLR      = false;
    float       inputBalance = 0.0f;   // -1 (L) .. +1 (R)
    bool        polarityL   = false;
    bool        polarityR   = false;

    // --- 2/6. Mid-Side mode ----------------------------------------------
    bool        msMode      = false;   // process the effect chain in M/S

    // --- 3. Effect / widening engine -------------------------------------
    float       driveDb     = 0.0f;    // 0 .. 24 dB pre-saturation gain
    Algorithm   algorithm   = Algorithm::Velvet;

    // Haas
    float       haasDelayMs = 12.0f;   // 1 .. 35 ms
    HaasSide    haasSide    = HaasSide::Right;

    // Velvet noise decorrelation
    float       velvetDensity = 0.5f;  // 0 .. 1 (diffusion amount)

    // Chorus
    float       chorusRate  = 0.6f;    // Hz
    float       chorusDepth = 0.5f;    // 0 .. 1

    // Dimension-D (anti-phase, no pitch wobble). Mode selects a voicing.
    int         dimMode     = 1;       // 1 .. 4 classic mode buttons
    float       dimAmount   = 0.5f;    // 0 .. 1

    // Global width (MS-domain)
    float       width       = 1.0f;    // 0 = mono, 1 = unchanged, 2 = wide

    // --- 4. Multiband width (Advanced) -----------------------------------
    bool        mbEnable    = false;
    float       mbFreqLow   = 250.0f;  // low/mid crossover
    float       mbFreqHigh  = 2500.0f; // mid/high crossover
    float       mbWidthLow  = 1.0f;
    float       mbWidthMid  = 1.0f;
    float       mbWidthHigh = 1.0f;

    // --- 5. Mono Maker ----------------------------------------------------
    bool        monoMakerEnable = false;
    float       monoMakerFreq   = 120.0f; // below this -> mono

    // --- 7. Mix -----------------------------------------------------------
    float       mix         = 1.0f;    // 0 = dry, 1 = wet

    // --- 8. Output / Auto gain -------------------------------------------
    float       outputGainDb = 0.0f;   // manual output trim
    bool        autoGainMatch = false; // real-time loudness match (A/B aid)

    // --- Monitoring -------------------------------------------------------
    bool        polarityOutL = false;  // unused placeholder for symmetry
    SoloMode    solo        = SoloMode::Off;

    // --- 9. Oversampling --------------------------------------------------
    OversampleFactor oversample = OversampleFactor::Off;

    // --- Bypass -----------------------------------------------------------
    bool        bypass      = false;
};

} // namespace anamorph
