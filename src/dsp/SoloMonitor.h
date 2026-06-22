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
};

} // namespace anamorph
