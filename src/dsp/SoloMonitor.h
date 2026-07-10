#pragma once

#include <juce_dsp/juce_dsp.h>

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
//  final output. The cutoffs glide per sample (like the Multiband / Mono Maker) so
//  dragging a split while soloing can't chirp.
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
//  has fully settled (passGain == 1, all bandGains == 0) AND no crossover glide is
//  pending, process() is a provable passthrough (out = 1*in + 0*bands), so the
//  filter/smoother work is skipped and the bank goes cold. Re-entry resets the
//  filters and snaps the cutoff glide while the band gains are still ~0, so the
//  charge-up is masked by the same ~12 ms crossfade that always covered an engage
//  (the engine's mbRunning warm/cold pattern). The crossfade still advances on
//  every block in which ANY gain is unsettled -- the click-free invariant holds.
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
    juce::dsp::LinkwitzRileyFilter<float> x1, x2, x3;
    double sr = 44100.0;
    float  glideCoeff = 0.0f;
    float  targetF[3]  { 180.0f, 800.0f, 3000.0f };
    float  currentF[3] { 180.0f, 800.0f, 3000.0f };
    int    bands = 4;
    bool   running = true; // false = settled-passthrough fast path active, filters cold (H1)

    // Click-free monitor crossfade: passGain blends in the true output (1 when nothing
    // is soloed), each bandGain blends in that band's sum (1 only while soloed). All
    // smoothed, so engaging / changing / clearing solo is a morph, never a step.
    juce::SmoothedValue<float> passGain;
    juce::SmoothedValue<float> bandGain[4];
};

} // namespace anamorph
