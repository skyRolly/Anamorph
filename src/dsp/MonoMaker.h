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
//  Placed AFTER widening so we collapse the lows the widener just spread.
//  Linear -> stays OUTSIDE oversampling.
// ============================================================================
class MonoMaker
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setFrequency (float hz) noexcept { xover.setCutoffFrequency (hz); }

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> xover;
};

} // namespace anamorph
