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
    glideCoeff = 1.0f - std::exp (-1.0f / (float) (0.020 * sr)); // ~20 ms per-sample glide
    reset();
}

void MonoMaker::reset()
{
    xover.reset();
}

void MonoMaker::processSplit (float* left, float* right, float* lowMonoOut, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        // Per-sample frequency glide: re-deriving the LR coefficients every sample
        // keeps a dragged Freq sweep smooth (no stepped-coefficient buzz, #19).
        if (std::abs (currentFreq - targetFreq) > 0.05f)
        {
            currentFreq += glideCoeff * (targetFreq - currentFreq);
            xover.setCutoffFrequency (currentFreq);
        }

        float lowL, highL, lowR, highR;
        xover.processSample (0, left[i],  lowL, highL);
        xover.processSample (1, right[i], lowR, highR);

        lowMonoOut[i] = (lowL + lowR) * 0.5f; // mono low band, kept out of the widener
        left[i]  = highL;                     // only the high band continues
        right[i] = highR;
    }
}

} // namespace anamorph
