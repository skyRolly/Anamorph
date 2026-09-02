#include "VelvetNoise.h"
#include <cmath>
#include <algorithm>

namespace anamorph
{

static int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }

void VelvetNoise::prepare (double sampleRate, int maxBlockSize, unsigned seed)
{
    sr = sampleRate;

    // ~45 ms decorrelation window -> sized history buffer (power of two).
    const int winSamps = (int) std::ceil (0.045 * sr) + 4;
    const int size = nextPow2 (winSamps + 1);
    midHist.assign ((size_t) size, 0.0f);
    histMask = size - 1;
    writePos = 0;

    // Generate the fixed velvet tap set ONCE: one impulse per grid cell at a
    // random position with a random sign. density later decides how many of
    // these are active (continuously), never regenerating them.
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    decorrSamps = std::max (8, (int) std::round (0.045 * sr));
    const float cell = (float) decorrSamps / (float) maxTaps;
    for (int m = 0; m < maxTaps; ++m)
    {
        int p = (int) std::round ((float) m * cell + uni (rng) * (cell - 1.0f));
        p = std::max (1, std::min (decorrSamps - 1, p)); // skip tap 0 (keep side decorrelated)
        pos[(size_t) m]  = p;
        sign[(size_t) m] = (uni (rng) < 0.5f) ? -1.0f : 1.0f;
    }

    snapToTargets();
    // Presence follower (fast attack, slow release) -> drives the gate's on/off.
    // Reverted to the previous, gentler timings: the faster gate tried last round
    // fluttered during decays and made the pause burst worse, not better (#5).
    envAtk  = 1.0f - std::exp (-1.0f / (float) (0.002 * sr));
    envRel  = 1.0f - std::exp (-1.0f / (float) (0.080 * sr));
    // Gate RAMP times (fixed): fade the decorrelation in over ~22 ms on play so
    // the FIR burst is masked, and out over ~28 ms on pause.
    gateAtk = 1.0f - std::exp (-1.0f / (float) (0.022 * sr));
    gateRel = 1.0f - std::exp (-1.0f / (float) (0.028 * sr));
    // Transport-stop tail fade: ~4 ms, matching the engine's switch duck (#4).
    stopStep = 1.0f / (float) std::max (1.0, 0.004 * sr);

    // H5 gather scratch (see processBlock): this block's Mids + per-sample tap
    // sums. A7-2B removed the linear history image; the ring IS the history.
    const int maxN = std::max (1, maxBlockSize);
    accum.assign ((size_t) maxN, 0.0f);
    midBlk.assign ((size_t) maxN, 0.0f);

    updateWeights();
    reset();
}

void VelvetNoise::reset()
{
    std::fill (midHist.begin(), midHist.end(), 0.0f);
    writePos = 0;
    env = 0.0f;
    gate = 0.0f;
    stopping = false;
    stopGain = 1.0f;
}

void VelvetNoise::updateWeights() noexcept
{
    weightsDensity = currentDensity; // record the input this build is valid for (S4)

    // Continuous active count: each tap fades in over its own unit interval, so
    // changing density never causes a step discontinuity. The fixed +/-1 tap
    // sign is folded into the stored weight here (ALG-4, Wave 2) so the gather
    // loop below does one multiply per tap instead of two -- bit-identical,
    // because w * (+/-1) is an exact sign flip and the gather's evaluation
    // order (weight*sign)*sample is unchanged.
    const float f = currentDensity * (float) maxTaps;
    float sumSq = 0.0f;
    int   highest = 0;
    for (int i = 0; i < maxTaps; ++i)
    {
        const float w = std::min (1.0f, std::max (0.0f, f - (float) i));
        weight[(size_t) i] = w * sign[(size_t) i];
        sumSq += w * w;
        if (w > 0.0f) highest = i + 1;
    }
    activeTaps = highest;
    norm = 1.0f / std::sqrt (std::max (1.0f, sumSq));
}

void VelvetNoise::processBlock (float* left, float* right, int numSamples) noexcept
{
    constexpr float dSmooth = 0.0015f; // glide density
    constexpr float aSmooth = 0.0015f; // glide wet amount

    // Tap-outer gather fast path (H5, Wave 2; read straight from the ring since
    // A7-2B). The 64 random-index history reads per sample (45.6 % of the row's
    // D1 read misses) become CONTIGUOUS unit-stride runs per tap.
    //
    // WHERE TAP t's HISTORY LIVES. For sample i the per-sample loop below reads
    // midHist[(writePos_i - pos[t]) & histMask]. While i < k (= pos[t]) that
    // slot was written before this block began; from i >= k it is this block's
    // own mid_{i-k}, which the ring does not hold yet and `midBlk` does. So the
    // run splits at i == k: a RING portion of min(k, numSamples) samples
    // starting at (writePos - k) & histMask, emitted as 1-2 runs bounded by the
    // ring end, then a `midBlk` tail. Never more than 3 runs, all unit-stride.
    //
    // WHY THE RING MAY BE READ IN BULK, BEFORE THE PER-SAMPLE LOOP WRITES IT.
    // The read at sample i could only collide with one of this block's own
    // writes if (i - k) + ringSize <= i - 1, i.e. ringSize <= k - 1. prepare()
    // clamps pos[] to [1, decorrSamps-1] and sizes the ring to at least
    // decorrSamps + 5, so k < ringSize always and the collision is impossible at
    // every block length and sample rate -- including numSamples greater than
    // decorrSamps, and greater than the ring itself.
    //
    // TWO SPELLINGS ARE FORBIDDEN, both measured (PERF_AUDIT_A7-2_A7-5_A7-9):
    // taking the ring run as `k` rather than min(k, numSamples) overruns
    // `accum` on every small block (ASan catches it); basing the tail on
    // `midBlk.data() - k` forms a pointer before the object, which is UB that
    // ASan + UBSan + local-bounds + pointer-overflow together do NOT catch. The
    // tail is indexed as midBlk[i - k] with i >= k for that reason.
    //
    // Bit-exactness: the split is along i, not along t, so accum[i] still adds
    // w*hist in the SAME ascending-t order the per-sample loop uses, starting
    // from the same +0 (zero-fill first -- an assign-first form could flip the
    // signed zero the S5 algebra relies on); the per-sample pass below then runs
    // the identical envelope/glide/output arithmetic, only substituting the
    // precomputed sum. Guarded by Test 40, which compares this path against the
    // per-sample loop directly (testVelvetGatherEqualsPerSampleLoop).
    // Eligibility (block-wise, per the Wave-2 design):
    //  * not stopping -- the stop fade flushes the ring mid-block; that path
    //    keeps the original loop verbatim (it can only assert between blocks);
    //  * density glide at its float fixpoint (one tick provably changes
    //    nothing -- the fixpoint is absorbing within a block since the target
    //    only moves between blocks) and weights already built for it, so the
    //    weights are constant across the block (a glide re-weights per sample
    //    and MUST keep the original loop -- feedback #18);
    //  * amount engaged or engaging (else the original loop's per-sample
    //    zero-skip is already the cheaper path -- the parked default);
    //  * the block fits the prepare()-sized scratch (always true from the
    //    engine; belt-and-braces for direct callers).
    const float dNext = currentDensity + dSmooth * (targetDensity - currentDensity);
    // Density glide at its float fixpoint with the weights already built for it:
    // one tick provably changes nothing, so a whole block of ticks changes
    // nothing (the fixpoint is absorbing; targets only move between blocks).
    // Shared by the H5 gather gate below and the Wave-5 parked gate after it.
    const bool densityAtFixpoint = dNext == currentDensity
                                && currentDensity == weightsDensity;

    // A7-9: the AMOUNT glide gets the same fixpoint treatment the density glide
    // has always had, and for the same reason -- see the block above the parked
    // path below for why a value test was wrong. `amountParked` is computed ONCE
    // and used in both directions, so the gather gate and the parked gate stay
    // exact complements of each other (within `! stopping && densityAtFixpoint`,
    // which both share) and no state can be eligible for neither or for both.
    const float aNext = currentAmount + aSmooth * (targetAmount - currentAmount);
    const bool amountParked = ! (targetAmount > 0.0f)
                           && (aNext == currentAmount
                               || ! (currentAmount > 0.0f));

    if (! stopping
        && densityAtFixpoint
        && ! amountParked
        && numSamples <= (int) accum.size())
    {
        for (int i = 0; i < numSamples; ++i)
            midBlk[(size_t) i] = (left[i] + right[i]) * 0.5f;

        std::fill (accum.begin(), accum.begin() + numSamples, 0.0f);
        const float* const ring  = midHist.data();
        const int          ringN = histMask + 1;
        for (int t = 0; t < activeTaps; ++t)
        {
            const float w = weight[(size_t) t];
            float*      acc = accum.data();
            const int   k = pos[(size_t) t];

            const int fromRing = std::min (k, numSamples);
            const int r0 = (writePos - k) & histMask;
            if (r0 + fromRing <= ringN)
            {
                // The common case: the ring portion does not cross the origin,
                // so it is ONE run and the wrap bookkeeping is pure overhead.
                const float* src = ring + r0;
                for (int i = 0; i < fromRing; ++i)
                    acc[i] += w * src[i];
            }
            else
            {
                int done = 0, r = r0;
                while (done < fromRing)
                {
                    const int    run = std::min (fromRing - done, ringN - r);
                    const float* src = ring + r;
                    for (int i = 0; i < run; ++i)
                        acc[done + i] += w * src[i];
                    done += run;
                    r = (r + run) & histMask;
                }
            }
            for (int i = fromRing; i < numSamples; ++i)
                acc[i] += w * midBlk[(size_t) (i - k)];
        }

        for (int i = 0; i < numSamples; ++i)
        {
            currentDensity += dSmooth * (targetDensity - currentDensity); // no-op at the fixpoint
            currentAmount  += aSmooth * (targetAmount  - currentAmount);

            const float mid  = midBlk[(size_t) i];
            const float side = (left[i] - right[i]) * 0.5f;

            const float a = std::abs (mid);
            env += (a > env ? envAtk : envRel) * (a - env);
            const float gateTarget = (env > 0.0005f) ? 1.0f : 0.0f; // ~ -66 dBFS presence
            gate += (gateTarget > gate ? gateAtk : gateRel) * (gateTarget - gate);

            midHist[(size_t) writePos] = mid;

            // Where the loop below would SKIP the sum (amount or gate exactly 0)
            // the multiplier is exactly +0 and the S5 signed-zero algebra makes
            // a multiplied-in finite sum produce the same output bits, so using
            // the precomputed sum unconditionally is output-identical. stopG is
            // omitted: it is exactly 1 whenever this path is eligible.
            float decorr = accum[(size_t) i];
            decorr *= norm * currentAmount * gate;

            const float newSide = side + decorr;
            left[i]  = mid + newSide;
            right[i] = mid - newSide;

            writePos = (writePos + 1) & histMask;
        }
        return;
    }

    // A7-9: WHY `amountParked` IS A FIXPOINT TEST AND NOT A VALUE TEST. Turning
    // Amount down does NOT bring `currentAmount` to zero. With a 0 target the
    // update is `a -= 0.0015f * a`, and under the block's ScopedNoDenormals the
    // DECREMENT underflows before `a` does, so the glide stalls at
    // ~FLT_MIN/0.0015 = 7.83e-36 and every later decrement is exactly 0. A test
    // for `currentAmount == 0` therefore stayed FALSE forever after a ramp-down:
    // this path -- written for, and measured in, the fresh prepare() state,
    // where prepare() assigns currentAmount = targetAmount -- was unreachable
    // from the one route a user actually takes, and the gather gate above kept
    // its eligibility instead.
    //
    // THE STALL VALUE ABOVE IS ONE CONFIGURATION'S, not a universal: it is the
    // x86-64 baseline's, where ADR-0031 pins contraction off and FTZ is on.
    // Measured elsewhere (platform-coverage audit, F-1): with FTZ OFF (valgrind;
    // any platform without a flush mode) the DECREMENT underflows to zero while
    // `a` is still a ~7e-43 SUBNORMAL, so the glide fixpoints there instead;
    // under FMA CONTRACTION (arm64-class builds -- FMLA is base ISA; measured
    // through an x86 FMA analogue) the fused decrement has no separately
    // rounded intermediate and the glide walks to an EXACT 0. The fixpoint test
    // parks correctly at all three terminal states -- the second disjunct
    // covers the exact-zero one. PERF_AUDIT_PLATFORM_COVERAGE.md F-1.
    //
    // The repair is to ask the question the path really depends on: not "is the
    // glide at zero" but "can the glide still move" -- the SAME test the density
    // glide three blocks above has always used, now applied to amount. It is
    // decisive for a whole block rather than one sample because the targets only
    // move between blocks, so the map is idempotent and the fixpoint absorbing.
    //
    // CLASS B, and no Class-A variant exists. Parked, `decorr` is the exact
    // signed zero the general loop produces; stalled, the general loop scaled
    // the tap sum by a currentAmount just under FLT_MIN/0.0015 = 7.837e-36,
    // which `side + decorr` absorbs bit-exactly once |side| clears ~2^24..2^25 *
    // |decorr| (binade-dependent) -- NOT "for any normal side", as this
    // comment once claimed. The
    // residual therefore lands on the SILENCE-REGION sample class: an exactly-
    // or near-zero side (mono content is the everyday member) whose mid history
    // is recent enough to keep the -66 dBFS presence gate open. Measured
    // against the pre-fix sources: 7.145e-36 on silence; up to 6.019e-36 on
    // 1e-25..1e-37-amplitude mono tails, on BOTH channels (the residual rides
    // side); 0 of 102,400 samples different on real signal, re-verified
    // bit-exact at every tail amplitude down to 1e-20
    // (PERF_AUDIT_A7-9_NEARSILENT_SCOPE.md). The programme-wide worst case is ChorusEngine's
    // at 192 kHz, whose coefficient is the only rate-dependent one. After the
    // fix the silence output is an exact zero rather than a smaller residual.
    // Test 41 is the gate and is proven to fail without this change; Test 42
    // pins the near-silent half the same way. Measured in
    // worklogs/performance/PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md,
    // re-verified in PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md, implemented and
    // (bound corrected) in PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md §4c.
    //
    // The second disjunct in `amountParked` is the PRE-A7-9 entry condition kept
    // verbatim, for the one input the fixpoint test does not subsume: a NaN
    // amount with currentAmount already 0. The gate can only ever admit MORE
    // than it did before A7-9.

    // Parked fast path (Wave 5 -- the Haas-parked / W3-9-compliant shape). With
    // the density glide at its fixpoint, the amount glide parked on a 0 target,
    // and no stop fade in flight, every skipped operation below is provably a
    // no-op this block: the density tick (fixpoint), the weights compare (equal
    // by the gate), the amount tick (also a fixpoint -- 0 += k*0 when parked,
    // and unchanged by definition when stalled), and the stop machine
    // (! stopping). What MUST keep running does: the presence env/gate keep
    // tracking the input so a re-engage opens with the correct state (the exact
    // reasoning that REJECTED freezing them, W3-9), the history keeps recording
    // for the same reason, and the output write-back keeps the full multiplier
    // chain verbatim -- decorr stays the same signed zero the general loop
    // produces for a parked amount (stopG omitted: it is exactly 1 here, the H5
    // precedent) -- so the MS round-trip lands on identical bits.
    if (! stopping
        && densityAtFixpoint
        && amountParked)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float L = left[i], R = right[i];
            const float mid  = (L + R) * 0.5f;
            const float side = (L - R) * 0.5f;

            const float a = std::abs (mid);
            env += (a > env ? envAtk : envRel) * (a - env);
            const float gateTarget = (env > 0.0005f) ? 1.0f : 0.0f; // ~ -66 dBFS presence
            gate += (gateTarget > gate ? gateAtk : gateRel) * (gateTarget - gate);

            midHist[(size_t) writePos] = mid;

            float decorr = 0.0f;
            decorr *= norm * currentAmount * gate;

            const float newSide = side + decorr;
            left[i]  = mid + newSide;
            right[i] = mid - newSide;

            writePos = (writePos + 1) & histMask;
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // Re-weight EVERY sample while the density glides: the normalisation
        // (1/sqrt(sumSq)) must move continuously or it steps and zippers when the
        // Density knob is turned quickly (feedback #18). Once the glide reaches
        // its float fixpoint the density stops changing AT ALL, and updateWeights
        // -- a pure function of currentDensity -- would rebuild identical
        // weights/norm, so it is skipped on an EXACT compare only (S4). Never an
        // epsilon threshold here: the pre-0.4.1 drift gate was the #18 zipper.
        currentDensity += dSmooth * (targetDensity - currentDensity);
        if (std::abs (currentDensity - weightsDensity) > 0.0f)
            updateWeights();

        currentAmount += aSmooth * (targetAmount - currentAmount);

        const float L = left[i], R = right[i];
        const float mid  = (L + R) * 0.5f;
        const float side = (L - R) * 0.5f;

        // Presence detect, then ramp the GATE at fixed times (decoupled from the
        // input level) so the decorrelation always fades in/out over a fixed
        // window -- masking the FIR burst on play and the tail on pause (#10).
        const float a = std::abs (mid);
        env += (a > env ? envAtk : envRel) * (a - env);
        const float gateTarget = (env > 0.0005f) ? 1.0f : 0.0f; // ~ -66 dBFS presence
        gate += (gateTarget > gate ? gateAtk : gateRel) * (gateTarget - gate);

        // Transport-stop tail kill (#4): the host paused, so the dry signal that
        // masked the FIR tail is gone -- fade the wet sum out over ~4 ms (zero-
        // slope smoothstep), then flush the history and re-arm the presence gate.
        float stopG = 1.0f;
        if (stopping)
        {
            stopGain -= stopStep;
            if (stopGain <= 0.0f)
            {
                std::fill (midHist.begin(), midHist.end(), 0.0f);
                env = 0.0f;
                gate = 0.0f;
                stopGain = 1.0f;
                stopping = false;
            }
            else
                stopG = stopGain * stopGain * (3.0f - 2.0f * stopGain);
        }

        midHist[(size_t) writePos] = mid;

        // The tap sum only reaches the output through the multiplier
        // norm * currentAmount * gate * stopG below. norm is always > 0 and,
        // outside a stop fade, stopG is 1 -- so when the amount or the gate sits
        // at EXACTLY 0 (their one-poles flush to true zero under the block's
        // ScopedNoDenormals), the multiplier is exactly +0 and the summed taps
        // are multiplied into a signed zero that provably cannot change L/R
        // (side == +/-0 forces mid to a zero whose +/- algebra lands on the same
        // bits either way -- S5). Only the ACCUMULATION is skipped: the history
        // write above, the envelopes/glides, the stop machine and the multiply/
        // add path below all run unchanged, and any stop fade in flight keeps
        // the loop running so the stopping path stays instruction-identical.
        float decorr = 0.0f;
        if (stopping || (currentAmount > 0.0f && gate > 0.0f))
            for (int t = 0; t < activeTaps; ++t)
            {
                const int idx = (writePos - pos[(size_t) t]) & histMask;
                decorr += weight[(size_t) t] * midHist[(size_t) idx];
            }
        decorr *= norm * currentAmount * gate * stopG;

        const float newSide = side + decorr;
        left[i]  = mid + newSide;
        right[i] = mid - newSide;

        writePos = (writePos + 1) & histMask;
    }
}

} // namespace anamorph
