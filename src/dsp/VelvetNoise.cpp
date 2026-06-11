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

        float decorr = 0.0f;
        for (int t = 0; t < activeTaps; ++t)
        {
            const int idx = (writePos - pos[(size_t) t]) & histMask;
            decorr += weight[(size_t) t] * sign[(size_t) t] * midHist[(size_t) idx];
        }
        decorr *= norm * currentAmount * gate * stopG;

        const float newSide = side + decorr;
        left[i]  = mid + newSide;
        right[i] = mid - newSide;

        writePos = (writePos + 1) & histMask;
    }
}

} // namespace anamorph
