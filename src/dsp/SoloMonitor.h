#pragma once

#include <juce_dsp/juce_dsp.h>
#include "LR4Xover.h"

namespace anamorph
{

// ============================================================================
//  SoloMonitor  (Band Solo -- POST-EVERYTHING monitoring filter)
//
//  Band Solo is purely a MONITORING aid: it band-pass filters the ALREADY-PRODUCED
//  final output so you can audition one or more bands. It is the very last stage in
//  the chain and never affects the effect DSP -- no processing stage changes its
//  behaviour based on the solo state. Run it on the signal that would otherwise be
//  sent to the plugin output; when nothing is soloed it leaves that signal untouched.
//
//  It mirrors the Multiband's band split: the same crossover frequencies and band
//  count, so soloing "band b" auditions exactly band b's spectral region of the
//  final output. Cutoff changes use the Multiband's strategy (0.8.10 final): a
//  slew-limited smoother for continuous movement (~20 ms one-pole demand
//  clamped to R(f) = 4 * max(1, f/300) oct/s -- normal drags track 1:1, the
//  swept-allpass FM stays <= ~7 cents of the crossing above 300 Hz; flat
//  magnitude at every instant) and a single ~12 ms bank crossfade for a
//  discrete multi-octave target step -- see MultibandWidth.h.
//
//  Click-free by construction (0.8.1): the crossover filters run every block that
//  can be heard and the output is a per-band SMOOTHED crossfade between the true
//  passthrough and the soloed band-sum. Engaging, clearing or changing the solo set
//  is therefore a short morph, never a hard switch -- so it needs no output duck and
//  can't tick on a transport edge, into silence, or on a zero/restarted buffer (no
//  stale-filter charge-up, no revealed tail). When nothing is soloed the passthrough
//  gain settles to exactly 1 -> bit-exact true output.
//
//  Settled fast path (0.8.9 / H1): once nothing is soloed AND every gain smoother
//  has fully settled (passGain == 1, all bandGains == 0) AND no cutoff change is
//  pending, process() is a provable passthrough (out = 1*in + 0*bands), so the
//  filter/smoother work is skipped and the bank goes cold. Re-entry resets the
//  filters and snaps the cutoffs to their targets while the band gains are still
//  ~0, so the charge-up is masked by the same ~12 ms crossfade that always covered
//  an engage (the engine's mbRunning warm/cold pattern). The crossfade still
//  advances on every block in which ANY gain is unsettled -- the click-free
//  invariant holds.
// ============================================================================
class SoloMonitor
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setBandCount (int n) noexcept { bands = juce::jlimit (1, 4, n); }
    void setCrossovers (float f1, float f2, float f3) noexcept;

    // Band-pass the output IN PLACE to the bands selected by `mask` (bit b = band b).
    // mask == 0 (no solo) leaves L/R untouched -- the true plugin output.
    void process (float* left, float* right, int mask, int numSamples) noexcept;

    // Test-only introspection (dsp_tests, Test 32): the live cutoff of split i
    // on the active bank (== the setCrossovers target exactly once the glide
    // has snapped), and whether the settled-passthrough fast path (H1) is
    // engaged (filters cold) -- its cutoff gate needs every split within
    // 0.05 Hz of target, which after any real move only the exact snap reaches.
    float getLiveCutoff (int i) const noexcept { return bank[active].f[juce::jlimit (0, 2, i)]; }
    bool  isSettledCold() const noexcept       { return ! running; }

private:
    // One crossover bank (flat-state LR4Xover, H6). Cutoff changes use the same
    // strategy as the Multiband (0.8.10 final, full rationale in
    // MultibandWidth.h): continuous movement is a slew-limited smoother -- per
    // sample each cutoff moves by its ~20 ms one-pole demand, clamped to the
    // frequency-proportional cap R(f) = 4 * max(1, f/300) oct/s. A flat
    // 4 oct/s floor below 300 Hz bounds the swept-allpass shift at ~1.25 Hz;
    // above it the cap grows with the cutoff so the shift stays ~0.42% of the
    // crossing (~7 cents) while normal drags track 1:1 -- flat magnitude at
    // every instant; only a DISCRETE target step (> 1.5 oct between
    // consecutive blocks) crossfades to the state-copied idle bank over
    // ~12 ms (one bounded event instead of a multi-second crawl).
    struct XoverBank
    {
        LR4Xover x[3];
        float    f[3] { 180.0f, 800.0f, 3000.0f };
    };
    XoverBank bank[2];
    int       active  = 0;
    bool      fading  = false;
    int       fadePos = 0;
    int       fadeLen = 1;   // ~12 ms in samples
    bool      pendingJump = false; // a discrete step arrived while fading

    static constexpr float kFadeThresholdOct = 1.5f;  // matches MultibandWidth
    static constexpr float kRateRefHz        = 300.0f; // rate-cap anchor, matches MultibandWidth

    void setBankCutoffs (XoverBank& b) noexcept;

    double sr = 44100.0;
    float  glideStep = 1.0f; // per-sample multiplicative BASE cap, 2^(4/sr) (~4 oct/s at f <= kRateRefHz)
    float  targetF[3]     { 180.0f, 800.0f, 3000.0f };
    float  prevTargetF[3] { 180.0f, 800.0f, 3000.0f }; // last block's targets (step detector)

    // One-pole demand coefficient of the slew-limited smoother (~20 ms),
    // mirroring MultibandWidth (full rationale there): per sample each cutoff
    // moves gap*smoothCoeff clamped to the R(f) cap; the one-pole leg
    // de-staircases the UI cadence and tapers arrivals, and the f-scaled snap
    // eps guarantees the settled fast path can reach cutoffs == targets
    // despite float one-pole stall. Above ~170 kHz the eps alone no longer
    // out-runs the stall, so the glide also snaps the moment the float add
    // stops moving the cutoff (exact convergence at 192 kHz; bit-identical
    // at <= 96 kHz -- see MultibandWidth.h).
    float smoothCoeff = 0.0f;
    int    bands = 4;
    bool   running = true; // false = settled-passthrough fast path active, filters cold (H1)

    // Click-free monitor crossfade: passGain blends in the true output (1 when nothing
    // is soloed), each bandGain blends in that band's sum (1 only while soloed). All
    // smoothed, so engaging / changing / clearing solo is a morph, never a step.
    juce::SmoothedValue<float> passGain;
    juce::SmoothedValue<float> bandGain[4];
};

} // namespace anamorph
