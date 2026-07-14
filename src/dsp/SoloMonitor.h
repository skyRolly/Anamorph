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
//  final output. Cutoff changes are fixed-coefficient bank crossfades (like the
//  Multiband, 0.8.10) so dragging a split while soloing neither chirps nor
//  pitch-shifts -- the old per-sample glide swept the crossover phase and detuned
//  the soloed band for the whole catch-up of a fast drag.
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

private:
    // One crossover bank at ONE FIXED set of cutoffs (flat-state LR4Xover, H6).
    // Cutoff changes crossfade the output to the idle bank at the new cutoffs
    // (state-copied, so no charge-up) over ~12 ms instead of sweeping the
    // coefficients -- a swept LR4 rotates its phase and audibly pitch-shifts the
    // soloed band during a fast split/band drag (0.8.10; same fix as the
    // Multiband, see MultibandWidth.h).
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

    void setBankCutoffs (XoverBank& b) noexcept;

    double sr = 44100.0;
    float  targetF[3]  { 180.0f, 800.0f, 3000.0f };
    int    bands = 4;
    bool   running = true; // false = settled-passthrough fast path active, filters cold (H1)

    // Click-free monitor crossfade: passGain blends in the true output (1 when nothing
    // is soloed), each bandGain blends in that band's sum (1 only while soloed). All
    // smoothed, so engaging / changing / clearing solo is a morph, never a step.
    juce::SmoothedValue<float> passGain;
    juce::SmoothedValue<float> bandGain[4];
};

} // namespace anamorph
