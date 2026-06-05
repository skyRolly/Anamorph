#include "VelvetNoise.h"
#include <cmath>
#include <algorithm>

namespace anamorph
{

static int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }

void VelvetNoise::prepare (double sampleRate, unsigned seed)
{
    sr = sampleRate;
    rng.seed (seed);

    // ~45 ms decorrelation window -> sized history buffer (power of two).
    const int winSamps = (int) std::ceil (0.045 * sr) + 4;
    const int size = nextPow2 (winSamps + 1);
    midHist.assign ((size_t) size, 0.0f);
    histMask = size - 1;
    writePos = 0;
    taps.reserve (80);   // max tap count -> clear()/push_back never reallocates
    rebuild = true;
    buildTaps();
}

void VelvetNoise::reset()
{
    std::fill (midHist.begin(), midHist.end(), 0.0f);
    writePos = 0;
}

void VelvetNoise::buildTaps()
{
    // Map density 0..1 to a tap count over the decorrelation window.
    const float win = 0.045f; // seconds
    const int minTaps = 12, maxTaps = 80;
    const int n = minTaps + (int) std::round (density * (maxTaps - minTaps));

    taps.clear();
    taps.reserve ((size_t) n);

    // Velvet noise: one impulse per grid cell at a random position inside it,
    // with a random sign. Grid spacing = window / n.
    const int winSamps = std::max (8, (int) std::round (win * sr));
    const float cell = (float) winSamps / (float) n;
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);

    for (int m = 0; m < n; ++m)
    {
        int pos = (int) std::round (m * cell + uni (rng) * (cell - 1.0f));
        pos = std::max (1, std::min (winSamps - 1, pos)); // skip tap 0 (keep side decorrelated)
        const float sign = (uni (rng) < 0.5f) ? -1.0f : 1.0f;
        taps.push_back ({ pos, sign });
    }

    norm = 1.0f / std::sqrt ((float) std::max (1, n));
    rebuild = false;
}

void VelvetNoise::processBlock (float* left, float* right, int numSamples) noexcept
{
    if (rebuild)
        buildTaps();

    for (int i = 0; i < numSamples; ++i)
    {
        const float L = left[i], R = right[i];
        const float mid  = (L + R) * 0.5f;
        const float side = (L - R) * 0.5f;

        midHist[(size_t) writePos] = mid;

        // Sparse convolution: sum of signed, delayed Mid samples.
        float decorr = 0.0f;
        for (const auto& t : taps)
        {
            const int idx = (writePos - t.delay) & histMask;
            decorr += t.sign * midHist[(size_t) idx];
        }
        decorr *= norm;

        const float newSide = side + decorr; // amount folded into density build
        left[i]  = mid + newSide;
        right[i] = mid - newSide;

        writePos = (writePos + 1) & histMask;
    }
}

} // namespace anamorph
