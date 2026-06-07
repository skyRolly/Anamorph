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
    // Per-sample multiplicative slew cap (~8 octaves/sec): a fast Freq drag can no
    // longer sweep the IIR quickly enough to chirp / pitch-shift (feedback #9).
    glideCoeff = std::exp2 (8.0f / (float) sr);
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
        // Glide the cutoff toward the target, capped at a fixed octaves/second
        // rate so the time-varying LR filter never modulates fast enough to
        // pitch-shift on a quick drag (feedback #9).
        if (std::abs (currentFreq - targetFreq) > 0.05f)
        {
            if (targetFreq > currentFreq) currentFreq = juce::jmin (targetFreq, currentFreq * glideCoeff);
            else                          currentFreq = juce::jmax (targetFreq, currentFreq / glideCoeff);
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
