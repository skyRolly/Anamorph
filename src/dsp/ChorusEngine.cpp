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

    // Smooth wet + depth per-sample so changing Amount/Depth/Mode never clicks.
    const float wSmooth = 1.0f / (float) std::max (1.0, 0.01 * workingRate); // ~10 ms

    // Amount-0 idle fast path (H12, 0.8.9; the VelvetNoise S5 pattern): with the
    // wet glide settled at exactly 0 (it flushes to true 0 under the block's
    // ScopedNoDenormals) and the target still 0, both voices are an exact
    // identity -- out = in * 1 + wet * 0 -- so the LFO sins and the 2 (chorus)
    // / 4 (Dimension-D) interpolated reads are pure waste. The reduced loop
    // keeps every piece of state bit-identical for a later re-engage: the
    // delay-line writes and write indices, the per-sample iterated phase
    // accumulation (NOT closed-form, so the wrap sequence matches exactly)
    // and the depth glide all advance exactly as before; currentWet stays an
    // exact 0 either way (0 + w*(0-0) == 0). Only zero-contribution work is
    // skipped. Exact compares, no epsilon.
    if (! (std::abs (currentWet) > 0.0f) && ! (std::abs (wetTarget) > 0.0f))
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
