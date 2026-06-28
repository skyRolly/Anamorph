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

enum class ChannelMode  { Stereo = 0, LeftOnly, RightOnly };   // "kill a channel" (Mono is its own toggle)
enum class Algorithm    { Haas = 0, Velvet, Chorus, DimensionD };
enum class HaasSide     { Left = 0, Right };
enum class SoloMode     { Off = 0, Mid, Side };
enum class OversampleFactor { Off = 0, x2, x4, x8 };

struct EngineParameters
{
    // --- 1. Input conditioning -------------------------------------------
    ChannelMode channelMode = ChannelMode::Stereo;
    bool        monoSum     = false;   // sum L+R -> mono (own toggle, near Swap)
    bool        swapLR      = false;
    float       inputBalance = 0.0f;   // -1 (L) .. +1 (R)
    bool        polarityL   = false;
    bool        polarityR   = false;

    // --- Mid-Side mode (part of Input conditioning, stage 1) -------------
    bool        msMode      = false;   // process the effect chain in M/S

    // --- 2. Effect / widening engine -------------------------------------
    float       driveDb     = 0.0f;    // 0 .. 24 dB pre-saturation gain
    Algorithm   algorithm   = Algorithm::Velvet;

    // Unified widening intensity: 0 == identity (the wet path is transparent),
    // so a freshly-loaded plug-in does NOTHING to the sound (spec feedback #3).
    // The per-algorithm controls below only shape the character.
    float       algoAmount  = 0.0f;    // 0 .. 1

    // Haas
    float       haasDelayMs = 12.0f;   // 1 .. 35 ms
    HaasSide    haasSide    = HaasSide::Left;

    // Velvet noise decorrelation
    float       velvetDensity = 0.5f;  // 0 .. 1 (diffusion character)

    // Chorus
    float       chorusRate  = 0.5f;    // Hz
    float       chorusDepth = 0.5f;    // 0 .. 1

    // Dimension-D (anti-phase, no pitch wobble). Mode selects a voicing.
    int         dimMode     = 1;       // 1 .. 4 voicings

    // Global width (MS-domain). 1.0 (== 100%) is identity (spec feedback #3).
    float       width       = 1.0f;    // 0 = mono, 1 = unchanged, 2 = wide

    // --- 2d. Multiband width (Advanced -- end of the effect engine) ------
    // Up to four phase-coherent bands split by up to three crossovers
    // (low < mid < high), each with its own MS width. Driven by the drag-to-split
    // Multiband display. mbBands selects how many bands are active (1..4): only
    // the first (mbBands - 1) crossovers and the first mbBands widths are used.
    bool        mbEnable     = false;
    int         mbBands      = 4;       // active band count (1..4)
    int         mbSolo       = 0;       // 4-bit mask: bit b set = band b soloed (0 = none)
    float       mbFreqLow    = 180.0f;  // band 1 | 2 crossover
    float       mbFreqMid    = 800.0f;  // band 2 | 3 crossover
    float       mbFreqHigh   = 3000.0f; // band 3 | 4 crossover
    float       mbWidthLow   = 1.0f;    // band 1 (lows)
    float       mbWidthMid   = 1.0f;    // band 2 (low-mids)
    float       mbWidthHiMid = 1.0f;    // band 3 (high-mids)
    float       mbWidthHigh  = 1.0f;    // band 4 (highs)

    // --- 4. Mono Maker (post-Mix; numbered by chain stage, not field order)
    bool        monoMakerEnable = false;
    float       monoMakerFreq   = 120.0f; // below this -> mono

    // --- 3. Mix -----------------------------------------------------------
    float       mix         = 1.0f;    // 0 = dry, 1 = wet

    // --- 5. Output / Auto gain -------------------------------------------
    float       outputGainDb  = 0.0f;  // manual output trim
    float       outputBalance = 0.0f;  // -1 (L) .. +1 (R) whole-plugin balance
    bool        autoGainMatch = false; // real-time loudness match (A/B aid)

    // --- Monitoring (M/S Solo; Band Solo monitor = chain stage 6) --------
    SoloMode    solo        = SoloMode::Off;

    // --- Oversampling (global; wraps the nonlinear part of stage 2) ------
    OversampleFactor oversample = OversampleFactor::Off;  // default 1x (Off)

    // --- Bypass (chain stage 7 -- click-free output crossfade) -----------
    bool        bypass      = false;
};

} // namespace anamorph
