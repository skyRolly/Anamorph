#include "ChorusEngine.h"
#include <cmath>
#include <algorithm>

namespace anamorph
{

static int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }
static constexpr double kTwoPi = 6.283185307179586476925286766559;

void ChorusEngine::prepare (double maxWorkingRate)
{
    maxRate = maxWorkingRate;
    // Max delay = base + depth headroom (~30 ms) at the highest OS rate.
    const int maxDelaySamps = (int) std::ceil (0.030 * maxRate) + 8;
    const int size = nextPow2 (maxDelaySamps + 1);
    bufL.assign ((size_t) size, 0.0f);
    bufR.assign ((size_t) size, 0.0f);
    bufMask = size - 1;
    reset();
}

void ChorusEngine::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writeL = writeR = 0;
    phase = 0.0f;
    currentWet = 0.0f;
    currentDepth = 0.0f;
}

void ChorusEngine::setDimMode (int mode) noexcept
{
    // Four classic "mode buttons": progressively wider/deeper, all slow.
    switch (mode)
    {
        case 1: dimBaseMs = 10.0f; dimDepthMs = 1.0f; dimRateHz = 0.40f; break;
        case 2: dimBaseMs = 12.0f; dimDepthMs = 1.6f; dimRateHz = 0.50f; break;
        case 3: dimBaseMs = 14.0f; dimDepthMs = 2.2f; dimRateHz = 0.62f; break;
        default:dimBaseMs = 16.0f; dimDepthMs = 3.0f; dimRateHz = 0.75f; break;
    }
}

float ChorusEngine::readFrac (const std::vector<float>& line, int writeIdx, float delaySamps) const noexcept
{
    const float readPos = (float) writeIdx - delaySamps;
    int i0 = (int) std::floor (readPos);
    const float frac = readPos - (float) i0;
    int i1 = i0 + 1;
    i0 &= bufMask; i1 &= bufMask;
    return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
}

void ChorusEngine::processBlock (float* left, float* right, int numSamples) noexcept
{
    const bool isDim = (voice == Voice::DimensionD);

    const float baseMs   = isDim ? dimBaseMs  : 14.0f;
    const float depthMs  = isDim ? dimDepthMs : (1.0f + depth * 5.0f);
    const float rate     = isDim ? dimRateHz  : rateHz;

    const float baseSamps      = baseMs  * 0.001f * (float) workingRate;
    const float depthSampsTarget = depthMs * 0.001f * (float) workingRate;
    const float phaseInc       = rate / (float) workingRate;
    // amount 0 == identity for BOTH voices (spec feedback #3).
    const float wetTarget = amount;

    // A prepare-time snap that could not be completed until the working rate was
    // known (ER-DSP-09): land the depth on its target before any smoothing runs,
    // so a restored session's modulation starts at its stored depth instead of
    // ramping up from zero. One-shot; ordinary blocks never take it.
    if (snapDepthPending)
    {
        currentDepth = depthSampsTarget;
        snapDepthPending = false;
    }

    // Smooth wet + depth per-sample so changing Amount/Depth/Mode never clicks.
    const float wSmooth = 1.0f / (float) std::max (1.0, 0.01 * workingRate); // ~10 ms

    // A7-9: WHY THIS IS A FIXPOINT TEST AND NOT A VALUE TEST. Turning Amount
    // down does NOT bring `currentWet` to zero. With a 0 target the update is
    // `a -= wSmooth * a`, and under the block's ScopedNoDenormals the DECREMENT
    // underflows before `a` does, so the glide stalls just below
    // FLT_MIN/wSmooth and every later decrement is exactly 0. That threshold
    // SCALES WITH THE SAMPLE RATE here, alone among the three modules, because
    // wSmooth is 1/(0.01*workingRate): 5.64e-36 at 48 kHz, 2.26e-35 at 192 kHz.
    // A test for `currentWet == 0` therefore stayed FALSE forever after a
    // ramp-down, and this path -- written for, and measured in, the fresh
    // prepare() state -- was unreachable from the one route a user takes.
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
    // glide at zero" but "can the glide still move". `wNext == currentWet` is
    // that question, and it is decisive for a whole block rather than one
    // sample, because wetTarget and wSmooth are both fixed across the block and
    // the map is therefore idempotent -- the fixpoint is absorbing. It is the
    // same test VelvetNoise has always used for its density glide.
    //
    // CLASS B, and no Class-A variant exists. Parked, both voices are an exact
    // identity; stalled, each added `w*(d - x)` with a w just under
    // FLT_MIN/wSmooth, which the sum absorbs bit-exactly once
    // |x| clears ~2^24..2^25 * |w*(d - x)| (the boundary shifts within each
    // binade) -- NOT "for any normal x", as this comment once claimed. The residual therefore lands on the SILENCE-REGION sample
    // class: digital silence and near-silent samples under ~4e-28 of full
    // scale co-occurring with a louder delay history -- and THIS module sets
    // the programme-wide worst case for the rate-scaling reason above:
    // 1.563e-35 measured at 192 kHz on silence against the pre-fix sources
    // (~ -696 dBFS), 1.204e-35 on 1e-25..1e-37-amplitude tails, 0 of 102,400
    // samples different on real signal, re-verified bit-exact at every tail
    // amplitude down to 1e-20 (PERF_AUDIT_A7-9_NEARSILENT_SCOPE.md). After the
    // fix the silence output is an exact zero rather than a smaller residual.
    // Test 41 is the gate, at BOTH ends of the rate range, and is proven to fail
    // without this change; Test 42 pins the near-silent half the same way. Measured in
    // worklogs/performance/PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md,
    // re-verified in PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md, implemented and
    // (bound corrected) in PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md §4c.
    const float wNext = currentWet + wSmooth * (wetTarget - currentWet);

    // Amount-0 idle fast path (H12, 0.8.9; the VelvetNoise S5 pattern; gate
    // repaired by A7-9): with the target at 0 and the wet glide unable to move
    // off its current value, both voices contribute nothing this block -- out is
    // exactly in * 1 + wet * 0 when wet is 0, and nothing the output can hold
    // when wet is the stalled ~1e-35 -- so the LFO sins and the 2 (chorus) / 4
    // (Dimension-D) interpolated reads are pure waste. The reduced loop keeps
    // every piece of state bit-identical for a later re-engage: the delay-line
    // writes and write indices, the per-sample iterated phase accumulation (NOT
    // closed-form, so the wrap sequence matches exactly) and the depth glide all
    // advance exactly as before; currentWet is left where it is, which is what
    // the full path's tick would have produced too (0 + w*(0-0) == 0 when parked,
    // and the fixpoint by definition when stalled). Only zero-contribution work
    // is skipped.
    //
    // Exact compares, no epsilon. The second disjunct is the PRE-A7-9 entry
    // condition kept verbatim, for the one input the fixpoint test does not
    // subsume: a NaN target with currentWet already 0, where the old gate parked
    // (identity, NaN never enters the state) and `wNext == currentWet` would
    // not. The gate can only ever admit MORE than it did before A7-9.
    if (! (std::abs (wetTarget) > 0.0f)
        && (wNext == currentWet || ! (std::abs (currentWet) > 0.0f)))
    {
        for (int n = 0; n < numSamples; ++n)
        {
            currentDepth += wSmooth * (depthSampsTarget - currentDepth);
            bufL[(size_t) writeL] = left[n];
            bufR[(size_t) writeR] = right[n];
            writeL = (writeL + 1) & bufMask;
            writeR = (writeR + 1) & bufMask;
            phase += phaseInc;
            if (phase >= 1.0f) phase -= 1.0f;
        }
        return;
    }

    // Quadrature LFO recurrence (H11, Wave 2): the two per-sample libm sines
    // are one rotated (sin, cos) pair -- the right channel's +0.25-turn offset
    // is exactly the quadrature identity sin(2pi(p + 0.25)) == cos(2pi p).
    // The pair is seeded from `phase` at every block start and advanced by the
    // fixed per-sample rotation R(2pi*phaseInc); state and coefficients are
    // double, so in-block drift is ~1e-13 and nothing carries across blocks.
    // `phase` itself keeps its iterated float accumulation below (identical
    // wrap sequence), so LFO continuity across blocks -- and re-engage from
    // the amount-0 fast path above, which advances the same `phase` -- are
    // bit-identical to the per-sample-sin version.
    double lfoS = std::sin (kTwoPi * (double) phase);
    double lfoC = std::cos (kTwoPi * (double) phase);
    const double rotC = std::cos (kTwoPi * (double) phaseInc);
    const double rotS = std::sin (kTwoPi * (double) phaseInc);

    for (int n = 0; n < numSamples; ++n)
    {
        currentWet   += wSmooth * (wetTarget        - currentWet);
        currentDepth += wSmooth * (depthSampsTarget - currentDepth);
        const float wet = currentWet;
        const float depthSamps = currentDepth;

        bufL[(size_t) writeL] = left[n];
        bufR[(size_t) writeR] = right[n];

        const float sinL = (float) lfoS;   // sin at the left LFO phase
        const float sinR = (float) lfoC;   // == sin at phase + 0.25 (right, 90 degrees for width)

        float outL, outR;

        if (isDim)
        {
            // Two anti-phase taps per channel -> pitch modulation cancels.
            const float dL1 = baseSamps + depthSamps * sinL;
            const float dL2 = baseSamps - depthSamps * sinL;
            const float dR1 = baseSamps + depthSamps * sinR;
            const float dR2 = baseSamps - depthSamps * sinR;

            const float wetL = 0.5f * (readFrac (bufL, writeL, dL1) + readFrac (bufL, writeL, dL2));
            const float wetR = 0.5f * (readFrac (bufR, writeR, dR1) + readFrac (bufR, writeR, dR2));

            outL = left[n]  + wet * (wetL - left[n]);
            outR = right[n] + wet * (wetR - right[n]);
        }
        else
        {
            // Chorus: single modulated tap, L/R anti-phase for width.
            const float dL = baseSamps + depthSamps * sinL;
            const float dR = baseSamps + depthSamps * sinR;
            const float wetL = readFrac (bufL, writeL, dL);
            const float wetR = readFrac (bufR, writeR, dR);
            outL = left[n]  * (1.0f - wet) + wetL * wet;
            outR = right[n] * (1.0f - wet) + wetR * wet;
        }

        left[n]  = outL;
        right[n] = outR;

        writeL = (writeL + 1) & bufMask;
        writeR = (writeR + 1) & bufMask;

        const double lfoNext = lfoS * rotC + lfoC * rotS;
        lfoC = lfoC * rotC - lfoS * rotS;
        lfoS = lfoNext;

        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

} // namespace anamorph
