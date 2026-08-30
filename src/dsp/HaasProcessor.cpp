#include "HaasProcessor.h"
#include <cmath>
#include <algorithm>

namespace anamorph
{

static int nextPow2 (int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

void HaasProcessor::prepare (double sampleRate, int /*maxBlock*/)
{
    sr = sampleRate;
    // Max Haas delay 35 ms + headroom for interpolation.
    const int maxSamps = (int) std::ceil (0.040 * sr) + 4;
    const int size = nextPow2 (maxSamps + 1);
    bufL.assign ((size_t) size, 0.0f);
    bufR.assign ((size_t) size, 0.0f);
    bufMask = size - 1;
    reset();
}

void HaasProcessor::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writeL = writeR = 0;
    currentSamples = targetSamples;
}

float HaasProcessor::readDelayed (std::vector<float>& line, int& widx, float delaySamps) noexcept
{
    // Fractional read with linear interpolation.
    const float readPos = (float) widx - delaySamps;
    int i0 = (int) std::floor (readPos);
    const float frac = readPos - (float) i0;
    int i1 = i0 + 1;
    i0 &= bufMask; i1 &= bufMask;
    return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
}

void HaasProcessor::processBlock (float* left, float* right, int numSamples) noexcept
{
    constexpr float smooth = 0.0005f; // glide delay changes to avoid zipper noise
    constexpr float aSmooth = 0.001f; // glide the wet amount (click-free, #1)

    // A7-9: WHY THIS IS A FIXPOINT TEST AND NOT A VALUE TEST. Turning Amount
    // down does NOT bring `currentAmount` to zero. With a 0 target the update
    // is `a -= 0.001f * a`, and under the block's ScopedNoDenormals the
    // DECREMENT underflows before `a` does, so the glide stalls at
    // ~FLT_MIN/0.001 = 1.17e-35 and every later decrement is exactly 0. A test
    // for `currentAmount == 0` therefore stayed FALSE forever after a
    // ramp-down, and this path -- written for, and measured in, the fresh
    // prepare() state where prepare() assigns currentAmount = targetAmount --
    // was unreachable from the one route a user actually takes.
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
    // glide at zero" but "can the glide still move". `aNext == currentAmount`
    // is that question, and it is decisive for a whole block rather than one
    // sample, because `amount` is fixed across the block and the map is
    // therefore idempotent -- the fixpoint is absorbing. It is the same test
    // VelvetNoise has always used for its density glide.
    //
    // CLASS B, and no Class-A variant exists. Parked, the module is an exact
    // identity; stalled, it added `a*(d - x)` with an a just under
    // FLT_MIN/0.001 = 1.175e-35, which is bit-exactly x for any normal x and is
    // NOT when x is +0. The residual therefore appears only on digital silence,
    // where the dry term cannot absorb it: 8.043e-36 measured here against the
    // pre-fix sources, 0 of 102,400 samples different on real signal. Removing
    // it IS what the fix means -- it is exactly what distinguished the stalled
    // state from the parked one, and after the fix the silence output is an
    // exact zero rather than a smaller residual. Test 41 is the gate and is
    // proven to fail without this change. Measured in
    // worklogs/performance/PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md,
    // re-verified in PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md, implemented and
    // (bound corrected) in PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md §4c.
    const float aNext = currentAmount + aSmooth * (amount - currentAmount);

    // Parked fast path (Wave 4; gate repaired by A7-9): with the target at 0 and
    // the wet glide unable to move off its current value, the blend
    // x + a*(d - x) contributes nothing this block -- exactly nothing when a is
    // 0, and nothing the output can hold when a is the stalled ~1e-35 -- so the
    // interpolated read and the blend are pure waste. The delay lines MUST keep
    // recording (a re-engage reads the history written while parked -- the same
    // reasoning that rejected the Velvet env freeze, W3-9) and the delay glide
    // keeps tracking retargets, so only the read + blend are skipped. Skipping
    // the amount tick is likewise a no-op: at the fixpoint it changes nothing,
    // and it is the same value the full path would have used.
    //
    // Exact compares, no epsilon. The second disjunct is the PRE-A7-9 entry
    // condition kept verbatim, for the one input the fixpoint test does not
    // subsume: a NaN target with currentAmount already 0, where the old gate
    // parked (identity, NaN never enters the state) and `aNext == currentAmount`
    // would not. The gate can only ever admit MORE than it did before A7-9.
    if (! (std::abs (amount) > 0.0f)
        && (aNext == currentAmount || ! (std::abs (currentAmount) > 0.0f)))
    {
        for (int n = 0; n < numSamples; ++n)
        {
            currentSamples += smooth * (targetSamples - currentSamples);
            bufL[(size_t) writeL] = left[n];
            bufR[(size_t) writeR] = right[n];
            writeL = (writeL + 1) & bufMask;
            writeR = (writeR + 1) & bufMask;
        }
        return;
    }

    for (int n = 0; n < numSamples; ++n)
    {
        currentSamples += smooth  * (targetSamples - currentSamples);
        currentAmount  += aSmooth * (amount        - currentAmount);

        // Write current input.
        bufL[(size_t) writeL] = left[n];
        bufR[(size_t) writeR] = right[n];

        // Blend dry with the delayed side by `amount` so amount 0 == identity.
        if (delayRight)
        {
            const float d = readDelayed (bufR, writeR, currentSamples);
            right[n] = right[n] + currentAmount * (d - right[n]);
        }
        else
        {
            const float d = readDelayed (bufL, writeL, currentSamples);
            left[n] = left[n] + currentAmount * (d - left[n]);
        }

        writeL = (writeL + 1) & bufMask;
        writeR = (writeR + 1) & bufMask;
    }
}

} // namespace anamorph
