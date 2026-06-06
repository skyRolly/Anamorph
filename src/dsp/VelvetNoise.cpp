#include "VelvetNoise.h"
#include <cmath>
#include <algorithm>

namespace anamorph
{

static int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }

void VelvetNoise::prepare (double sampleRate, unsigned seed)
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
    const int decorrSamps = std::max (8, (int) std::round (0.045 * sr));
    const float cell = (float) decorrSamps / (float) maxTaps;
    for (int m = 0; m < maxTaps; ++m)
    {
        int p = (int) std::round (m * cell + uni (rng) * (cell - 1.0f));
        p = std::max (1, std::min (decorrSamps - 1, p)); // skip tap 0 (keep side decorrelated)
        pos[(size_t) m]  = p;
        sign[(size_t) m] = (uni (rng) < 0.5f) ? -1.0f : 1.0f;
    }

    currentDensity = targetDensity;
    currentAmount  = targetAmount;
    // Open slowly so the decorrelation FADES IN on play (masking any burst from
    // stale history), but close fast so the sparse-FIR tail is cut quickly on
    // pause -- the pause "white noise" was that lingering tail (feedback #34).
    envAtk = 1.0f - std::exp (-1.0f / (float) (0.010 * sr)); // 10 ms attack (slow open)
    envRel = 1.0f - std::exp (-1.0f / (float) (0.018 * sr)); // 18 ms release (fast close)
    updateWeights();
    reset();
}

void VelvetNoise::reset()
{
    std::fill (midHist.begin(), midHist.end(), 0.0f);
    writePos = 0;
    env = 0.0f;
}

void VelvetNoise::updateWeights() noexcept
{
    // Continuous active count: each tap fades in over its own unit interval, so
    // changing density never causes a step discontinuity.
    const float f = currentDensity * (float) maxTaps;
    float sumSq = 0.0f;
    int   highest = 0;
    for (int i = 0; i < maxTaps; ++i)
    {
        const float w = std::min (1.0f, std::max (0.0f, f - (float) i));
        weight[(size_t) i] = w;
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

    for (int i = 0; i < numSamples; ++i)
    {
        // Re-weight EVERY sample while the density glides: the normalisation
        // (1/sqrt(sumSq)) must move continuously or it steps and zippers when the
        // Density knob is turned quickly (feedback #18).
        currentDensity += dSmooth * (targetDensity - currentDensity);
        updateWeights();

        currentAmount += aSmooth * (targetAmount - currentAmount);

        const float L = left[i], R = right[i];
        const float mid  = (L + R) * 0.5f;
        const float side = (L - R) * 0.5f;

        // Track input presence; gate closes in near-silence so the sparse-FIR
        // tail fades out instead of leaving a noise burst on pause (#17).
        const float a = std::abs (mid);
        env += (a > env ? envAtk : envRel) * (a - env);
        const float gate = std::min (1.0f, env * 333.0f); // ~ -50 dBFS knee

        midHist[(size_t) writePos] = mid;

        float decorr = 0.0f;
        for (int t = 0; t < activeTaps; ++t)
        {
            const int idx = (writePos - pos[(size_t) t]) & histMask;
            decorr += weight[(size_t) t] * sign[(size_t) t] * midHist[(size_t) idx];
        }
        decorr *= norm * currentAmount * gate;

        const float newSide = side + decorr;
        left[i]  = mid + newSide;
        right[i] = mid - newSide;

        writePos = (writePos + 1) & histMask;
    }
}

} // namespace anamorph
