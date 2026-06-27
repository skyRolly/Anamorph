#pragma once

#include <juce_dsp/juce_dsp.h>

namespace anamorph
{

// ============================================================================
//  MonoMaker
//
//  Phase-coherent low-frequency mono via a Linkwitz-Riley crossover, applied
//  IN PLACE at the very end of the processing chain (after the dry/wet Mix):
//
//      low_L, high_L = LR(L);   low_R, high_R = LR(R)
//      monoLow       = (low_L + low_R) * 0.5
//      L = high_L + monoLow;    R = high_R + monoLow
//
//  Below the cutoff the Side is collapsed to mono; the Mid is allpass-reconstructed
//  (LP + HP = allpass), so |Mid| is flat -- no low-frequency cancellation in the
//  mono sum. Running it post-Mix means it acts on whatever the dry/wet blend
//  produced, so the final low end is always mono regardless of the Mix amount.
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

    // Clamp Nyquist-safe (defensive, consistent with the multiband crossovers): the
    // param range already tops out at 500 Hz, but never feed the LR filter a cutoff
    // that could approach Nyquist and destabilise its coefficients (0.8.2).
    void setFrequency (float hz) noexcept
    {
        targetFreq = juce::jlimit (20.0f, juce::jmax (1000.0f, 0.45f * (float) sr), hz);
    }

    // Collapses the low band to mono in place (highs untouched, lows summed to mono).
    void process (float* left, float* right, int numSamples) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> xover;
    double sr = 44100.0;
    float  targetFreq  = 120.0f;
    float  currentFreq = 120.0f;
    float  glideCoeff  = 0.0f;
};

} // namespace anamorph
