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
    for (int n = 0; n < numSamples; ++n)
    {
        currentSamples += smooth * (targetSamples - currentSamples);

        // Write current input.
        bufL[(size_t) writeL] = left[n];
        bufR[(size_t) writeR] = right[n];

        if (delayRight)
        {
            // Left passes through; right is delayed.
            const float d = readDelayed (bufR, writeR, currentSamples);
            right[n] = d;
        }
        else
        {
            const float d = readDelayed (bufL, writeL, currentSamples);
            left[n] = d;
        }

        writeL = (writeL + 1) & bufMask;
        writeR = (writeR + 1) & bufMask;
    }
}

} // namespace anamorph
