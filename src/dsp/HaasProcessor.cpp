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

    // A7-9: HOW THIS STATE IS REACHED, AND HOW IT IS NOT. It is reached from a
    // fresh prepare() with Amount at its 0 default -- prepare() assigns
    // currentAmount = targetAmount -- which is the state this path was written
    // for and measured in. It is NOT reached by turning Amount down: with a 0
    // target the update is `a -= 0.001f * a`, and under the block's
    // ScopedNoDenormals the DECREMENT underflows before `a` does, so the glide
    // stalls at ~FLT_MIN/0.001 = 1.17e-35 and the next decrement is exactly 0.
    // the `std::abs (currentAmount) > 0.0f` test below therefore stays true and the full path runs instead. An earlier version of this comment claimed
    // the one-pole "flushes to true zero"; it does not, and the difference is
    // measured in worklogs/performance/PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md
    // and re-verified in PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md. Filed as A7-9
    // (a Class-B repair, not yet approved); this comment is the A7-9C half.
    // Parked fast path (Wave 4): with the wet glide at EXACTLY 0 and the target
    // still 0, the blend x + 0*(d - x) is
    // bit-exactly x for any finite d -- so the interpolated read and the blend
    // are pure waste. The delay lines MUST keep recording (a re-engage reads
    // the history written while parked -- the same reasoning that rejected the
    // Velvet env freeze, W3-9) and the delay glide keeps tracking retargets, so
    // only the read + blend are skipped. Exact compares, no epsilon: any
    // non-zero amount takes the full path unchanged.
    if (! (std::abs (amount) > 0.0f) && ! (std::abs (currentAmount) > 0.0f))
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
