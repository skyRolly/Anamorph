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
    // One complete crossover bank at ONE FIXED set of cutoffs (flat-state
    // LR4Xover units, H6):
    //   x[]   -- the wet crossovers (x[0]: band1 vs rest, x[1]: band2 vs rest,
    //            x[2]: band3 vs band4);
    //   dx[]  -- a parallel bank that reconstructs the DRY signal at unit width,
    //            A(dry), so the dry/wet Mix's dry path carries the exact same
    //            crossover allpass phase as the wet and never combs (KI #1);
    //   ax[]  -- the reconstruction phase-compensation allpasses (flat
    //            recombination): the running low-sum passes through each higher
    //            split's allpass (LR4 lo+hi) before the next band is added, so
    //            close crossovers no longer dip (only i = 1..bands-2 are used);
    //   dax[] -- the dry twins of ax[], keeping A(dry) phase-identical to the wet.
    // All 12 filters of a bank share f[], so wet, dry, and compensation stay
    // phase-locked by construction.
    struct XoverBank
    {
        LR4Xover x[3];
        LR4Xover dx[3];
        LR4Xover ax[3];
        LR4Xover dax[3];
        float    f[3] { 180.0f, 800.0f, 3000.0f };
    };

    // Cutoff-change strategy (0.8.10, two mechanisms picked by move size):
    //
    //  * CONTINUOUS MOVEMENT (any delta up to kFadeThresholdOct): the active
    //    bank's cutoffs glide per sample with a ONE-POLE toward the target
    //    (tau ~15 ms). Unlike the pre-0.8.10 ~8 oct/s RATE-CAPPED glide -- whose
    //    banked catch-up kept detuning the audio for hundreds of ms after a fast
    //    drag stopped -- the one-pole settles in bounded time (~5 tau after the
    //    last move), and unlike a chain of bank crossfades it is a true LR4 at
    //    every instant: the magnitude response stays EXACTLY allpass-flat while
    //    a split moves, and the phase trajectory is smooth, so a pure tone near
    //    the split shows no modulation sidebands (measured: chained 12 ms fades
    //    sprayed spurs at -25..-28 dBc around the tone during a fast drag -- the
    //    0.8.10 sine report; the one-pole glide measures at the -37..-41 dBc
    //    analysis floor, with < 0.1 cents residual pitch 50 ms after the drag).
    //
    //  * LARGE JUMPS (delta > kFadeThresholdOct on any active split -- an
    //    automation step or a click-jump, never a mouse drag at UI cadence): a
    //    glide would sweep the allpass phase through multiple full turns (a
    //    loud chirp, measured -4.7 dBc at a 4-octave step), so instead the idle
    //    bank adopts the active bank's ladder state plus the newest cutoffs and
    //    the output crossfades to it over ~12 ms. In a fade the endpoint phase
    //    difference wraps mod 2pi, so the same 4-octave step measures -18 dBc:
    //    a one-shot sub-dB ripple where the banks' allpass phases differ. Both
    //    mechanisms measure equal at ~2 octaves (-11.5 dBc, the physics floor
    //    for an instant coefficient change); the threshold sits below that,
    //    inside the glide's winning range.
    XoverBank bank[2];
    int       active  = 0;     // the glide/settled bank; 1-active fades in on a jump
    bool      fading  = false;
    int       fadePos = 0;     // 0..fadeLen-1 while fading
    int       fadeLen = 1;     // ~12 ms in samples

    // A jump larger than this (octaves) crossfades banks; anything smaller
    // glides. 1.5 oct is safely inside the region where the glide measures
    // cleaner (crossover point ~2 oct, see above).
    static constexpr float kFadeThresholdOct = 1.5f;

    void setBankCutoffs (XoverBank& b) noexcept;                       // -> targetF
    void copyBankState  (XoverBank& to, const XoverBank& from) noexcept;

    double sr = 44100.0;
    float  glideK = 0.0f;      // per-sample one-pole coefficient (tau ~15 ms)
    float  targetF[3]  { 180.0f, 800.0f, 3000.0f };

    // Per-sample one-pole smoothing of the band widths (0.7.0 #1).
    float wCoeff     = 0.0f;
    float targetW[4]  { 1.0f, 1.0f, 1.0f, 1.0f };
    float currentW[4] { 1.0f, 1.0f, 1.0f, 1.0f };

    int   bands = 4;
};

} // namespace anamorph
