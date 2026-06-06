#pragma once

#include <juce_dsp/juce_dsp.h>

namespace anamorph
{

// ============================================================================
//  MonoMaker
//
//  Phase-coherent low-frequency mono via a Linkwitz-Riley crossover. Rather than
//  collapsing the lows in place, it SPLITS the signal: the low band is summed to
//  mono and returned separately, while L/R are replaced with the HIGH band only.
//  The engine then routes only the high band through the widener and adds the
//  mono lows back at the end (feedback #2 + #20):
//
//    * the decorrelators never widen the low band  -> no external mono-sum
//      comb-cancellation of the bass;
//    * the mono lows survive the widener untouched  -> Mono Maker actually
//      controls the final low end (it "works").
//
//  The crossover frequency is glided PER SAMPLE (feedback #19): updating the LR
//  coefficients only every N samples stepped them at an audible rate and sounded
//  like a pitch wobble / doubling while dragging the Freq control.
// ============================================================================
class MonoMaker
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setFrequency (float hz) noexcept { targetFreq = hz; }

    // Splits the band: writes the mono low band to lowMonoOut and replaces L/R
    // with the high band. (Recombine downstream: out = highL/R + lowMono.)
    void processSplit (float* left, float* right, float* lowMonoOut, int numSamples) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> xover;
    double sr = 44100.0;
    float  targetFreq  = 120.0f;
    float  currentFreq = 120.0f;
    float  glideCoeff  = 0.0f;
};

} // namespace anamorph
