#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

#include "EngineParameters.h"
#include "HaasProcessor.h"
#include "VelvetNoise.h"
#include "ChorusEngine.h"
#include "MonoMaker.h"
#include "MultibandWidth.h"
#include "LoudnessMatch.h"
#include "Correlation.h"
#include "LevelMeters.h"
#include "ScopeBuffer.h"

namespace anamorph
{

// ============================================================================
//  AnamorphEngine
//
//  The complete, format-agnostic DSP chain (spec section 2.2):
//
//    1. Input conditioning (channel kill / swap / balance / polarity)
//    2. MS encode               (only if MS mode)
//    3. Effect engine           Drive -> algorithm -> global Width
//         (Drive + Chorus/Dim-D run INSIDE oversampling; Haas/Velvet/Width
//          are linear and stay OUTSIDE)
//    4. Multiband Width         (Advanced Mode, optional)
//    5. Mono Maker              (lows -> mono, AFTER widening)
//    6. MS decode               (only if MS mode)
//    7. Mix (dry/wet)           dry path is delay-compensated to the wet latency
//    8. Output Gain / Auto Gain
//    9. Metering tap            taps the FINAL output (scope + correlation)
//
//  Knows nothing about JUCE's plugin wrapper / APVTS -- it is driven purely by
//  an EngineParameters snapshot, so AU/AAX wrappers could reuse it unchanged.
// ============================================================================
class AnamorphEngine
{
public:
    AnamorphEngine() = default;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setParameters (const EngineParameters& params) noexcept { p = params; updateDerived(); }

    // Processes a STEREO buffer in place (the wrapper up-mixes mono -> stereo).
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    // PDC: latency added by the chain (oversampling only). Integer samples.
    int getLatencySamples() const noexcept;

    // Latency predicted for an arbitrary parameter snapshot (message thread can
    // call this to update host PDC without racing the audio thread's state).
    int predictLatency (const EngineParameters& e) const noexcept;

    // --- shared with the editor (GUI thread reads) -----------------------
    ScopeBuffer&      getScopeBuffer() noexcept   { return scope; }
    CorrelationMeter& getCorrelation() noexcept   { return correlation; }
    LevelMeters&      getLevels() noexcept         { return levels; }
    float getMatchGainDb() const noexcept         { return loudness.getMatchGainDb(); }

private:
    void updateDerived();
    void applyInputConditioning (float* L, float* R, int n) noexcept;
    void processNonlinearRegion (float* L, float* R, int n, double rate) noexcept;
    juce::dsp::Oversampling<float>* currentOversampler() noexcept;

    double sr = 44100.0;
    int    maxBlock = 512;

    EngineParameters p;

    // DSP modules
    HaasProcessor  haas;
    VelvetNoise    velvet;
    ChorusEngine   chorus;
    MultibandWidth multiband;
    MonoMaker      monoMaker;
    LoudnessMatch  loudness;
    CorrelationMeter correlation;
    LevelMeters    levels;
    ScopeBuffer    scope;

    // Oversamplers for 2x / 4x / 8x (orders 1 / 2 / 3). Off = bypass.
    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4, os8;
    int latency2 = 0, latency4 = 0, latency8 = 0;

    // Smoothed continuous controls (avoid zipper noise / clicks -- #1).
    juce::SmoothedValue<float> widthSmooth, mixSmooth, outGainSmooth, matchGainSmooth;
    juce::SmoothedValue<float> balanceSmooth, outBalanceSmooth, driveSmooth;
    juce::SmoothedValue<float> polLSmooth, polRSmooth; // smoothed polarity sign (no click)

    // Structural-change click suppression (#9 / #19): when the algorithm or an
    // input-routing switch changes, briefly fade the output and clear stale
    // algorithm tails so toggling never pops -- even during silence.
    Algorithm        prevAlgorithm  = Algorithm::Velvet;
    ChannelMode      prevChannelMode = ChannelMode::Stereo;
    OversampleFactor prevOversample = OversampleFactor::Off;
    bool  prevMsMode = false, prevSwap = false, prevMonoSum = false, structInit = false;
    float structFade = 1.0f, structFadeInc = 0.0f;

    // Dry-path delay (integer) to align dry with wet latency in the mix.
    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayWrite = 0;

    // Scratch
    juce::AudioBuffer<float> dryScratch;  // conditioned dry (pre-effect)
    juce::AudioBuffer<float> wetScratch;  // wet pre-output-gain (for loudness)

    bool driveActive = false;
};

} // namespace anamorph
