#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

#include "EngineParameters.h"
#include "HaasProcessor.h"
#include "VelvetNoise.h"
#include "ChorusEngine.h"
#include "MonoMaker.h"
#include "MultibandWidth.h"
#include "SoloMonitor.h"
#include "LoudnessMatch.h"
#include "Correlation.h"
#include "LevelMeters.h"
#include "ScopeBuffer.h"

namespace anamorph
{

// ============================================================================
//  AnamorphEngine
//
//  The complete, format-agnostic DSP chain (see AnamorphEngine::process). Rebuilt
//  in 0.8.0 as a strictly serial chain; Band Solo is the last stage (monitor-only):
//
//    1. Input conditioning   channel kill / Swap / Balance / polarity; plus M/S
//                            decode + M/S solo when M/S mode is on
//    2. Effect engine        Drive -> algorithm (Haas / Velvet / Chorus / Dim-D)
//                            -> global Width -> Multiband Width. Drive + Chorus/
//                            Dim-D run INSIDE oversampling; the rest are linear.
//    3. Mix (dry/wet)        dry is delay-compensated AND phase-matched (A(dry))
//    4. Mono Maker           lows -> mono, POST-Mix, in place
//    5. Output stage         Output Balance / Gain / Level Match + switch duck
//    6. Band Solo monitor    POST-EVERYTHING audition band-pass (monitor-only)
//    + Bypass crossfade (processed <-> raw) and the metering tap close the chain
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

    // Host transport state, once per block from the wrapper (audio thread).
    // A play->stop edge triggers Velvet's noise-tail kill (#4).
    void setTransportPlaying (bool isPlaying) noexcept { velvet.setTransportPlaying (isPlaying); }

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

    // Request a short raised-cosine output duck around the NEXT parameter swap,
    // even if it's continuous-only (#1, 0.6.4 feedback): an A/B or preset jump can
    // move many params at once and pop, so the wrapper asks for the same masking
    // duck the engine already uses for discrete switches. Call BEFORE changing the
    // parameters so the duck is already running when the new values arrive.
    void requestDuck() noexcept { duckRequest.store (1, std::memory_order_relaxed); }

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
    MonoMaker      monoMaker;     // post-Mix low-frequency mono (in place)
    SoloMonitor    soloMonitor;   // POST-EVERYTHING Band Solo audition filter
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
    juce::SmoothedValue<float> polLSmooth, polRSmooth; // smoothed polarity sign (no click)

    // Click-free discrete switching (feedback #10 / #11). A change to a discrete
    // control is applied at the BOTTOM of a short raised-cosine "duck": fade the
    // output down with the OLD settings, swap in the new settings while silent,
    // fade back up. The cosine has zero slope at the bottom so the seam is
    // inaudible even on bypass / algorithm changes during playback.
    enum class SwitchState { Normal, FadeOut, FadeIn };
    SwitchState switchState = SwitchState::Normal;
    float switchPhase = 1.0f;   // 1 = full level, 0 = silent
    // Asymmetric duck: a quick fade-OUT pulls the old state to silence, then a
    // noticeably longer, gentle fade-IN eases the new state up. A short symmetric
    // duck removed the click but a big A/B / preset level jump still "swelled" on
    // the way back in (0.6.6 feedback #1); stretching only the fade-in turns that
    // jump into an inaudible ramp without adding any perceptible delay.
    float switchIncOut = 0.0f;  // per-sample phase step on the way down (~6 ms)
    float switchIncIn  = 0.0f;  // per-sample phase step on the way up   (~28 ms)
    EngineParameters pendingP;  // snapshot to adopt once the duck reaches silence
    bool  pendingAlgoReset = false;
    // A FORCED duck (A/B / preset / undo) keeps the OLD state live through the
    // fade-out and swaps EVERYTHING -- continuous included -- at the silent bottom,
    // snapping the smoothers there, so no parameter (smoothed or not) can pop
    // mid-fade. A normal discrete duck still applies continuous immediately (#1).
    bool  pendingForced = false;
    void  snapSmoothers() noexcept;

    static constexpr float kNoInject = -1000.0f;
    std::atomic<float> matchInject { kNoInject }; // pending per-slot match restore (#23)
    std::atomic<int>   duckRequest { 0 };         // force a duck around a bulk param swap (#1, 0.6.4)

    // Dry-path delay (integer) to align dry with wet latency in the mix.
    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayWrite = 0;

    // Phase-matched dry for the Multiband dry/wet Mix (Known Issue #1): A(dry),
    // reconstructed through the same crossovers as the wet so a partial Mix never
    // combs (esp. the mono sum). Delayed alongside the clean dry to the OS latency.
    juce::AudioBuffer<float> dryAlignScratch;      // A(dry) for this block (from MultibandWidth)
    juce::AudioBuffer<float> dryAlignDelayBuffer;   // delay line, shares dryDelayWrite/size

    // True-bypass crossfade (Issue 2/3): the chain + analysis ALWAYS run; Bypass only
    // crossfades the OUTPUT between the processed signal and the delay-aligned RAW input.
    // bypassBlend = 0 -> processed, 1 -> bypassed; a short, sample-safe ramp (no mute).
    juce::AudioBuffer<float> bypassDelayBuffer; // raw-input delay line (latency align)
    juce::AudioBuffer<float> bypassDryScratch;  // delay-aligned raw input for this block
    int bypassDelayWrite = 0;
    juce::SmoothedValue<float> bypassBlend;

    // Multiband Enable crossfade: toggling the multiband module is a short click-free
    // OUTPUT crossfade, NOT a duck-to-silence. The crossover bank stays WARM across the
    // toggle (it keeps running while the blend is non-zero, so there is no cold-start
    // settle) and its output is faded against the pre-multiband signal -- exactly the
    // bypassBlend model, localised to the multiband stage. 0 -> multiband NOT applied,
    // 1 -> fully applied; a settled toggle is bit-exact either way.
    juce::SmoothedValue<float> mbEnableBlend;
    juce::AudioBuffer<float>   preMbScratch; // pre-multiband signal: the dry side of the blend
    bool mbRunning = false;                  // is the crossover bank currently running (warm)?

    // Scratch
    juce::AudioBuffer<float> dryScratch;   // dry for the dry/wet mix (the full conditioned input)
    juce::AudioBuffer<float> wetScratch;   // post-Mono-Maker, pre-output-gain (loudness measurement)
    juce::AudioBuffer<float> inputScratch; // full conditioned input (silence-edge detection, #25)
    juce::AudioBuffer<float> loudnessRefScratch; // delay-aligned reconstruction A(dry): Level-Match dry ref (Issue 2)

    bool driveActive = false;

    // Silence->audio edge latch: on the first audible block after silence the applied
    // Level-Match gain is snapped to its pre-ducked target so it can't slam, even if the
    // host never ran the plugin while paused (click-free -- the prior block was silent).
    bool prevInputSilent = true;

    // Whether the oversampling wrap is ENGAGED, latched only at safe points
    // (reset / the silent duck bottom). Engaging the oversampler inserts its
    // group delay, so doing it live -- e.g. Drive crossing 0 with OS selected --
    // jump-cuts the timeline; latching routes every OS-path change through the
    // duck instead (#3).
    bool osEngaged = false;
};

} // namespace anamorph
