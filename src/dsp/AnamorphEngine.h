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
//    2. Mono Maker              (lows -> mono, BEFORE widening, feedback #2)
//    3. MS encode               (only if MS mode)
//    4. Effect engine           Drive -> algorithm -> global Width
//         (Drive + Chorus/Dim-D run INSIDE oversampling; Haas/Velvet/Width
//          are linear and stay OUTSIDE)
//    5. Multiband Width         (Advanced Mode, optional)
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

    // Adopts a new parameter snapshot. Continuous controls update immediately
    // (they are all smoothed); a change to any DISCRETE control (algorithm,
    // routing, bypass, ...) is deferred behind a short raised-cosine duck so the
    // switch is click-free (feedback #10 / #11).
    void setParameters (const EngineParameters& params) noexcept;

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

    // A/B per-slot Level-Match memory (feedback #23): the wrapper restores a
    // remembered match value on a slot switch; consumed on the audio thread.
    void injectMatchGainDb (float db) noexcept { matchInject.store (db, std::memory_order_relaxed); }

private:
    void updateDerived();
    void applyInputConditioning (float* L, float* R, int n) noexcept;
    void processNonlinearRegion (float* L, float* R, int n, double rate) noexcept;
    juce::dsp::Oversampling<float>* currentOversampler() noexcept;

    // True when two snapshots differ in a control that would click if applied
    // instantly (so the switch must be ducked rather than applied live).
    static bool discreteDiffers (const EngineParameters& a, const EngineParameters& b) noexcept;
    // True when the actual PROCESSING differs (excludes Level-Match / Bypass),
    // i.e. when the loudness measurement is genuinely stale and must re-arm (#1).
    static bool processingDiffers (const EngineParameters& a, const EngineParameters& b) noexcept;
    // Copies only the continuous (smoothed) fields, leaving discrete ones intact.
    static void copyContinuous (EngineParameters& dst, const EngineParameters& src) noexcept;

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
    juce::SmoothedValue<float> balanceSmooth, outBalanceSmooth, driveSmooth, driveBlendSmooth;
    juce::SmoothedValue<float> monoDriveSmooth, monoDriveBlendSmooth; // Drive applied to the mono low band (#1)
    juce::SmoothedValue<float> polLSmooth, polRSmooth; // smoothed polarity sign (no click)

    // Click-free discrete switching (feedback #10 / #11). A change to a discrete
    // control is applied at the BOTTOM of a short raised-cosine "duck": fade the
    // output down with the OLD settings, swap in the new settings while silent,
    // fade back up. The cosine has zero slope at the bottom so the seam is
    // inaudible even on bypass / algorithm changes during playback.
    enum class SwitchState { Normal, FadeOut, FadeIn };
    SwitchState switchState = SwitchState::Normal;
    float switchPhase = 1.0f;   // 1 = full level, 0 = silent
    float switchInc   = 0.0f;   // per-sample phase step (~4 ms each direction)
    EngineParameters pendingP;  // snapshot to adopt once the duck reaches silence
    bool  pendingAlgoReset = false;

    static constexpr float kNoInject = -1000.0f;
    std::atomic<float> matchInject { kNoInject }; // pending per-slot match restore (#23)

    // Dry-path delay (integer) to align dry with wet latency in the mix.
    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayWrite = 0;

    // Scratch
    juce::AudioBuffer<float> dryScratch;   // dry for the dry/wet mix (high band when Mono Maker on)
    juce::AudioBuffer<float> wetScratch;   // wet pre-output-gain (for loudness)
    juce::AudioBuffer<float> inputScratch; // full conditioned input (loudness reference, #25)

    // Mono Maker band-split: the mono low band is held aside while only the high
    // band is widened, then added back (delay-aligned to the wet latency) (#20).
    juce::AudioBuffer<float> monoLow;       // mono low band for this block
    juce::AudioBuffer<float> monoLowDelay;  // delay line to align lows with OS latency
    int monoLowWrite = 0;

    bool driveActive = false;
};

} // namespace anamorph
