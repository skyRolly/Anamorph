#pragma once

#include <juce_dsp/juce_dsp.h>

namespace anamorph
{

// ============================================================================
//  MultibandWidth  (Advanced Mode -- Multiband display backend)
//
//  Splits the spectrum into 1..4 phase-coherent bands using up to three cascaded
//  Linkwitz-Riley crossovers, applies an independent MS width to each band, and
//  recombines. Typical use: narrow the lows, widen the highs -- now per drag in
//  the Multiband display.
//
//      b1   = LP(f1)
//      r1   = HP(f1)
//      b2   = LP(f2, r1)
//      r2   = HP(f2, r1)
//      b3   = LP(f3, r2)
//      b4   = HP(f3, r2)
//      sum  = b1 + b2 + b3 + b4         (allpass reconstruction -> flat)
//
//  With N < 4 active bands only the first N-1 crossovers run, so the trailing
//  remainder becomes the last band. Each band is widened in the MS domain
//  (L+R = 2*Mid is preserved per band), so the recombined output stays
//  mono-compatible. Linear -> OUTSIDE oversampling.
// ============================================================================
class MultibandWidth
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    // Active band count (1..4). Only (bandCount - 1) crossovers and bandCount
    // widths are used; changing it is a structural change and is routed through
    // the engine's silent switch-duck (with a reset) so it never clicks.
    void setBandCount (int n) noexcept { bands = juce::jlimit (1, 4, n); }

    // Solo a single band for monitoring: 0 = off, 1..4 = hear that band alone.
    // All filters keep running (state preserved); only the SUM changes.
    void setSolo (int oneBased) noexcept { soloBand = juce::jlimit (0, 4, oneBased) - 1; }

    void setCrossovers (float f1, float f2, float f3) noexcept;
    void setWidths (float b1, float b2, float b3, float b4) noexcept
    {
        w[0] = b1; w[1] = b2; w[2] = b3; w[3] = b4;
    }

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> x1; // f1: band1 vs rest
    juce::dsp::LinkwitzRileyFilter<float> x2; // f2: band2 vs rest
    juce::dsp::LinkwitzRileyFilter<float> x3; // f3: band3 vs band4

    // Per-sample multiplicative slew on each crossover, exactly like Mono Maker:
    // a fast drag of a split can no longer sweep the IIR quickly enough to chirp /
    // pitch-shift (0.6.7 #1).
    double sr = 44100.0;
    float  glideCoeff = 0.0f;
    float  targetF[3]  { 180.0f, 800.0f, 3000.0f };
    float  currentF[3] { 180.0f, 800.0f, 3000.0f };

    int   bands = 4;
    int   soloBand = -1; // -1 = no solo
    float w[4] { 1.0f, 1.0f, 1.0f, 1.0f };
};

} // namespace anamorph
