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

    // Cutoff-change strategy (0.8.10 final -- each element picked by
    // measurement against the pure-sine protocol: instantaneous frequency of
    // the fundamental, spurs outside +-30 Hz, envelope dips, at drag speeds
    // 1..24 oct/s):
    //
    //  * CONTINUOUS MOVEMENT: the active bank's cutoffs are a SLEW-LIMITED
    //    SMOOTHER of the target -- per sample each cutoff moves by the ~20 ms
    //    one-pole demand toward its target, clamped to a FREQUENCY-
    //    PROPORTIONAL RATE CAP R(f) = 4 oct/s * max(1, f/300 Hz). A swept IIR
    //    crossover is inherently a phase modulator -- its allpass phase at any
    //    fixed frequency rotates by up to 2pi per crossover crossing, and
    //    dphi/dt is a genuine frequency shift of 0.312*R Hz at sweep rate R
    //    oct/s. No smoothing shape can remove that (a bare one-pole tracking a
    //    fast drag FMs at the full drag rate -- rejected; chained bank
    //    crossfades are amplitude/phase modulation at the fade cadence,
    //    -25..-28 dBc sidebands -- rejected; a 1.25 oct/s "inaudibility" cap
    //    with a quiet-timeout release consolidation lagged every fast drag and
    //    read as a delayed jump after release -- rejected as a UX regression;
    //    a FLAT 4 oct/s cap fixed the violent-flick case but pinned every
    //    NORMAL drag: the display spans ~10 octaves in ~900 px, so ordinary
    //    400..2000 px/s gestures are 4..22 oct/s and trailed by whole octaves,
    //    draining at 0.25 s/oct after release -- the v0.8.10 slow-drag
    //    regression). The bound that matters perceptually is the SHIFT, and
    //    the shift at sweep rate R is a constant 0.312*R Hz regardless of
    //    where the crossing sits -- a cap flat in oct/s spends its whole
    //    budget protecting low crossings and only ADDS LAG at high ones.
    //    Scaling the cap with the current cutoff bounds the shift at 0.42% of
    //    the crossing (~7 cents) at any frequency above 300 Hz, and below
    //    300 Hz the flat 4 oct/s floor keeps it <= 1.25 Hz -- the bass
    //    register's constant-Hz pitch JND, the same worst case the flat cap
    //    accepted at a 150 Hz crossing (~14 cents, unchanged). The cap keeps
    //    every crossing slow enough that the FM skirts stay inside the +-30 Hz
    //    spur window (13.3 oct/s at 1 kHz measures at the -41 dBc analysis
    //    floor; the rejected fref=150 variant rode 27 oct/s past 1 kHz and
    //    sprayed -27 dBc). The one-pole leg matters too: it filters the 60 Hz
    //    UI staircase out of the demand AND tapers every arrival (rate -> 0
    //    smoothly as the gap closes) -- a hard rate-clamp arrival is a corner
    //    in the phase trajectory that measured -24 dBc when it landed near a
    //    tone. Kinematics: drags up to 4 oct/s track 1:1 (plus the 20 ms
    //    ease); above 300 Hz the allowed rate grows with f (13 oct/s at 1 kHz,
    //    160 at 12 kHz), so normal drags stay attached and even a full-panel
    //    flick lands in ~0.4-0.6 s of continuous motion -- no timers, no
    //    deferred catch-up, no post-release jump (ADR-0015 final + slow-drag
    //    refinement).
    //
    //  * DISCRETE JUMPS (the TARGET stepping > kFadeThresholdOct between two
    //    consecutive blocks -- an automation step / preset-style snap, never
    //    reachable by dragging at UI cadence): waiting ~seconds for the crawl
    //    would be musically wrong, so the idle bank adopts the active bank's
    //    ladder state plus the new cutoffs and the output crossfades over
    //    ~12 ms: ONE bounded transition event (a 4-octave step measures
    //    -18 dBc / -2.4 dB for 12 ms; the same step glided at a fast rate
    //    would chirp at -4.7 dBc). The fade's destination is LATCHED at fade
    //    start; a step arriving mid-fade is only remembered (pendingJump) --
    //    after the fade lands, a NEW fade may start toward the then-current
    //    targets (skipped if they came back within 0.1 oct; small residues
    //    drain via the glide instead).
    XoverBank bank[2];
    int       active  = 0;     // the glide/settled bank; 1-active fades in on a jump
    bool      fading  = false;
    int       fadePos = 0;     // 0..fadeLen-1 while fading
    int       fadeLen = 1;     // ~12 ms in samples
    bool      pendingJump = false; // a discrete step arrived while fading

    // A per-block TARGET delta larger than this (octaves) crossfades banks: a
    // discrete jump. Dragging at UI cadence never reaches it, so drags always
    // take the glide path.
    static constexpr float kFadeThresholdOct = 1.5f;

    // Anchor of the frequency-proportional rate cap: below this the cap is a
    // flat 4 oct/s (swept-allpass shift <= 1.25 Hz, the bass-register JND);
    // above it the cap grows as R(f) = 4 * f/kRateRefHz oct/s, which holds the
    // shift at a constant 0.42% of the crossing frequency (~7 cents) and keeps
    // every crossing's FM skirts inside the +-30 Hz spur criterion (300 is the
    // measured knee: fref 150 sprays -27 dBc past a 1 kHz tone, 300 sits at
    // the -41 dBc analysis floor with pitch unchanged).
    static constexpr float kRateRefHz = 300.0f;

    void setBankCutoffs (XoverBank& b) noexcept;                       // -> targetF
    void copyBankState  (XoverBank& to, const XoverBank& from) noexcept;

    double sr = 44100.0;
    float  glideStep = 1.0f;   // per-sample multiplicative BASE cap, 2^(4/sr) (~4 oct/s at f <= kRateRefHz)
    float  targetF[3]     { 180.0f, 800.0f, 3000.0f };
    float  prevTargetF[3] { 180.0f, 800.0f, 3000.0f }; // last block's targets (step detector)

    // One-pole demand coefficient of the slew-limited smoother (~20 ms, the
    // width glide's constant): per sample each cutoff moves gap*smoothCoeff,
    // clamped to the R(f) cap. The one-pole leg de-staircases the 60 Hz UI
    // target cadence AND tapers every arrival -- a bare rate-clamp lands at
    // full speed, a corner in the phase trajectory that measured -24 dBc of
    // splatter when it coincided with a tone. The glide's snap eps grows with
    // f because a float one-pole stalls once gap*coeff < ulp(f) (~1.5 Hz at
    // 20 kHz) -- without the snap the solo monitor's settled fast path could
    // never see cutoffs == targets and would stay hot forever.
    float smoothCoeff = 0.0f;

    // Per-sample one-pole smoothing of the band widths (0.7.0 #1).
    float wCoeff     = 0.0f;
    float targetW[4]  { 1.0f, 1.0f, 1.0f, 1.0f };
    float currentW[4] { 1.0f, 1.0f, 1.0f, 1.0f };

    int   bands = 4;
};

} // namespace anamorph
