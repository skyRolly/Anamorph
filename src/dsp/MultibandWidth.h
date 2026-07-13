#pragma once

#include <juce_dsp/juce_dsp.h>
#include "LR4Xover.h"

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
//
//  A naive sum b1+b2+b3+b4 is ONLY flat for a single crossover: an LR4 low+high
//  is an allpass A (magnitude 1, but with phase), so with more crossovers the
//  lower bands are missing the allpass phase of the crossovers ABOVE them and
//  partially cancel around the crossover region -- a magnitude dip that deepens
//  as the splits approach each other (measured: -17 dB at three close splits).
//  Flat reconstruction phase-compensates each lower band by the crossovers above
//  it: b1'=A2.A3.b1, b2'=A3.b2, b3'=b3, b4'=b4, whose sum telescopes to
//  A1.A2.A3 (allpass -> flat). This is done cheaply with a shared accumulator:
//  peel band0, then for each higher split, run the running low-sum through THAT
//  split's allpass before adding the next band. So only (bands-2) extra allpass
//  crossovers are needed (0 for 1-2 bands, 1 for 3, 2 for 4).
//
//  With N < 4 active bands only the first N-1 crossovers run, so the trailing
//  remainder becomes the last band. Each band is widened in the MS domain
//  (L+R = 2*Mid is preserved per band), so the recombined output stays
//  mono-compatible. The compensation allpass acts on L and R equally, so it does
//  not change any band's M/S ratio and adds ZERO integer latency (IIR allpass).
//  Linear -> OUTSIDE oversampling.
//
//  This stage is SOLO-AGNOSTIC: it always processes and sums every active band.
//  Band Solo is a post-everything MONITORING filter (see SoloMonitor), so no DSP
//  stage changes its behaviour based on the solo state.
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

    void setCrossovers (float f1, float f2, float f3) noexcept;

    // Per-band MS width TARGETS. The widths are not applied raw: each glides
    // toward its target per sample in processBlock (one-pole, ~20 ms, matching
    // the global Width smoother), so a fast band-width drag can no longer step
    // the side-gain at every block boundary and crackle. A width is a pure
    // side-gain, so smoothing it never pitch-shifts or combs (0.7.0 #1).
    void setWidths (float b1, float b2, float b3, float b4) noexcept
    {
        targetW[0] = b1; targetW[1] = b2; targetW[2] = b3; targetW[3] = b4;
    }

    // Processes the wet signal in place. If the dryOut pointers are supplied, the
    // dry input is ALSO reconstructed through the SAME (gliding) crossovers at unit
    // width -- a phase-matched A(dry) for the dry/wet Mix. Because the dry shares
    // the wet's exact crossover allpass phase, a partial-Mix recombination no longer
    // combs (notably the mono sum L+R = 2*Mid stays intact at any Mix). When the
    // dryOut pointers are null the dry bank is skipped (Known Issue #1).
    void processBlock (float* left, float* right, int numSamples,
                       const float* dryInL = nullptr, const float* dryInR = nullptr,
                       float* dryOutL = nullptr, float* dryOutR = nullptr) noexcept;

private:
    // Flat-state LR4 crossovers (H6, Wave 2): bit-identical arithmetic to the
    // juce::dsp::LinkwitzRileyFilter<float> they replaced, without its
    // per-sample heap-vector state indexing (see LR4Xover.h).
    LR4Xover x1; // f1: band1 vs rest
    LR4Xover x2; // f2: band2 vs rest
    LR4Xover x3; // f3: band3 vs band4

    // A SECOND, parallel crossover bank that reconstructs the DRY signal at unit
    // width -- A(dry) -- so the dry/wet Mix's dry path carries the exact same
    // crossover allpass phase as the wet and the recombination never combs. Driven
    // by the SAME gliding cutoffs as x1..x3, in lockstep, so a fast split drag stays
    // phase-matched (a separate bank that lagged the glide would comb) (KI #1).
    LR4Xover dx1, dx2, dx3;

    // Reconstruction phase-compensation allpasses (flat-recombination fix): the
    // running low-sum is passed through each higher split's allpass (LR4 lo+hi)
    // before the next band is added, so the bands stay phase-coherent and no
    // longer dip around close crossovers. ax[i] is the allpass at crossover i
    // (only i = 1..bands-2 are used); dax[] mirrors it for the dry bank so A(dry)
    // stays phase-identical to the wet. Same gliding cutoffs as x1..x3 / dx1..dx3.
    LR4Xover ax[3];
    LR4Xover dax[3];

    // Per-sample multiplicative slew on each crossover, exactly like Mono Maker:
    // a fast drag of a split can no longer sweep the IIR quickly enough to chirp /
    // pitch-shift (0.6.7 #1).
    double sr = 44100.0;
    float  glideCoeff = 0.0f;
    float  targetF[3]  { 180.0f, 800.0f, 3000.0f };
    float  currentF[3] { 180.0f, 800.0f, 3000.0f };

    // Per-sample one-pole smoothing of the band widths (0.7.0 #1).
    float wCoeff     = 0.0f;
    float targetW[4]  { 1.0f, 1.0f, 1.0f, 1.0f };
    float currentW[4] { 1.0f, 1.0f, 1.0f, 1.0f };

    int   bands = 4;
};

} // namespace anamorph
