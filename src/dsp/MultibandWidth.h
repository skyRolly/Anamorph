#pragma once

#include <juce_dsp/juce_dsp.h>

namespace anamorph
{

// ============================================================================
//  MultibandWidth  (Advanced Mode only)
//
//  Splits the spectrum into 3 phase-coherent bands (low / mid / high) using two
//  Linkwitz-Riley crossovers, applies an independent MS width to each band, and
//  recombines. Typical use: narrow the lows, widen the highs.
//
//      low  = LP(f1, x)
//      rest = HP(f1, x)
//      mid  = LP(f2, rest)
//      high = HP(f2, rest)
//      sum  = low + mid + high     (allpass reconstruction -> flat magnitude)
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

    void setCrossovers (float fLowMid, float fMidHigh) noexcept;
    void setWidths (float low, float mid, float high) noexcept
    {
        wLow = low; wMid = mid; wHigh = high;
    }

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> xLow;  // f1: low vs rest
    juce::dsp::LinkwitzRileyFilter<float> xHigh; // f2: mid vs high

    float wLow = 1.0f, wMid = 1.0f, wHigh = 1.0f;
};

} // namespace anamorph
