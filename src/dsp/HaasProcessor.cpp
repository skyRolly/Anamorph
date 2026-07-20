#include "HaasProcessor.h"
#include <cmath>

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

    // Parked fast path (Wave 4): with the wet glide settled at EXACTLY 0 (the
    // audio thread runs under ScopedNoDenormals, so the asymptotic amount tail
    // flushes to true zero) and the target still 0, the blend x + 0*(d - x) is
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
