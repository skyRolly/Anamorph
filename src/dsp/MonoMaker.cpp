#include "MonoMaker.h"
#include <cmath>

namespace anamorph
{

void MonoMaker::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
    xover.prepare (spec);
    xover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    currentFreq = targetFreq;
    xover.setCutoffFrequency (currentFreq);
    reset();
}

void MonoMaker::reset()
{
    xover.reset();
}

void MonoMaker::processBlock (float* left, float* right, int numSamples) noexcept
{
    // Glide the crossover frequency in small chunks so dragging the Freq control
    // never snaps the LR coefficients (feedback #12). ~8 ms time-constant; the
    // cutoff is only re-derived once per chunk (cheap, artefact-free).
    constexpr int kChunk = 16;
    const float coeff = 1.0f - std::exp (-(float) kChunk / (float) (0.008 * sr));

    int n = 0;
    while (n < numSamples)
    {
        const int len = juce::jmin (kChunk, numSamples - n);

        if (std::abs (currentFreq - targetFreq) > 0.01f)
        {
            currentFreq += coeff * (targetFreq - currentFreq);
            xover.setCutoffFrequency (currentFreq);
        }

        for (int i = n; i < n + len; ++i)
        {
            float lowL, highL, lowR, highR;
            // LinkwitzRileyFilter::processSample gives BOTH band outputs at once.
            xover.processSample (0, left[i],  lowL, highL);
            xover.processSample (1, right[i], lowR, highR);

            const float lowMono = (lowL + lowR) * 0.5f; // collapse lows to mono
            left[i]  = lowMono + highL;
            right[i] = lowMono + highR;
        }

        n += len;
    }
}

} // namespace anamorph
