#pragma once

#include <juce_dsp/juce_dsp.h>

namespace anamorph
{

// ============================================================================
//  MultibandWidth  (Advanced Mode -- spectral Imager backend)
//
//  Splits the spectrum into 4 phase-coherent bands using three cascaded
//  Linkwitz-Riley crossovers, applies an independent MS width to each band, and
//  recombines. Typical use: narrow the lows, widen the highs -- now per drag in
//  the spectral Imager.
//
//      b1   = LP(f1)
//      r1   = HP(f1)
//      b2   = LP(f2, r1)
//      r2   = HP(f2, r1)
//      b3   = LP(f3, r2)
//      b4   = HP(f3, r2)
//      sum  = b1 + b2 + b3 + b4         (allpass reconstruction -> flat)
//
//  Each band is widened in the MS domain (L+R = 2*Mid is preserved per band),
//  so the recombined output stays mono-compatible. Linear -> OUTSIDE
//  oversampling.
// ============================================================================
class MultibandWidth
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setCrossovers (float f1, float f2, float f3) noexcept;
    void setWidths (float b1, float b2, float b3, float b4) noexcept
    {
        w1 = b1; w2 = b2; w3 = b3; w4 = b4;
    }

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> x1; // f1: band1 vs rest
    juce::dsp::LinkwitzRileyFilter<float> x2; // f2: band2 vs rest
    juce::dsp::LinkwitzRileyFilter<float> x3; // f3: band3 vs band4

    float w1 = 1.0f, w2 = 1.0f, w3 = 1.0f, w4 = 1.0f;
};

} // namespace anamorph
