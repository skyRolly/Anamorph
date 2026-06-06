#pragma once

#include <juce_dsp/juce_dsp.h>

namespace anamorph
{

// ============================================================================
//  MonoMaker
//
//  Phase-coherent low-frequency mono. A Linkwitz-Riley crossover splits the
//  signal into low/high bands (LP + HP sum to an allpass -> no level dip at the
//  crossover). The LOW band is summed to mono (its side content is removed);
//  the HIGH band is left as-is; the two are recombined.
//
//  Placed BEFORE the widening engine (feedback #2): collapsing the lows first
//  means the decorrelators never spread the low band, so summing L+R later can't
//  comb-cancel the bass. Linear -> stays OUTSIDE oversampling.
//
//  The crossover frequency is glided in small sub-blocks (feedback #12): a hard
//  jump in the LR coefficients momentarily breaks the allpass-sum property and
//  produces an audible "doubled" artefact while dragging the Freq control.
// ============================================================================
class MonoMaker
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setFrequency (float hz) noexcept { targetFreq = hz; }

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> xover;
    double sr = 44100.0;
    float  targetFreq  = 120.0f;
    float  currentFreq = 120.0f;
};

} // namespace anamorph
