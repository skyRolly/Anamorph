#include "MonoMaker.h"

namespace anamorph
{

void MonoMaker::prepare (double sampleRate, int maxBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
    xover.prepare (spec);
    xover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    reset();
}

void MonoMaker::reset()
{
    xover.reset();
}

void MonoMaker::processBlock (float* left, float* right, int numSamples) noexcept
{
    for (int n = 0; n < numSamples; ++n)
    {
        float lowL, highL, lowR, highR;
        // LinkwitzRileyFilter::processSample gives BOTH band outputs at once.
        xover.processSample (0, left[n],  lowL, highL);
        xover.processSample (1, right[n], lowR, highR);

        const float lowMono = (lowL + lowR) * 0.5f; // collapse lows to mono
        left[n]  = lowMono + highL;
        right[n] = lowMono + highR;
    }
}

} // namespace anamorph
