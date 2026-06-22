#include "SoloMonitor.h"
#include <cmath>

namespace anamorph
{

void SoloMonitor::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
    for (auto* x : { &x1, &x2, &x3 })
    {
        x->prepare (spec);
        x->setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    }
    glideCoeff = std::exp2 (8.0f / (float) sr); // ~8 octaves/sec, matches the Multiband
    for (int i = 0; i < 3; ++i) { currentF[i] = targetF[i]; }
    x1.setCutoffFrequency (currentF[0]);
    x2.setCutoffFrequency (currentF[1]);
    x3.setCutoffFrequency (currentF[2]);
    reset();
}

void SoloMonitor::reset()
{
    x1.reset();
    x2.reset();
    x3.reset();
}

void SoloMonitor::setCrossovers (float f1, float f2, float f3) noexcept
{
    f2 = juce::jmax (f2, f1 * 1.1f);
    f3 = juce::jmax (f3, f2 * 1.1f);
    targetF[0] = f1;
    targetF[1] = f2;
    targetF[2] = f3;
}

void SoloMonitor::process (float* left, float* right, int mask, int numSamples) noexcept
{
    const int active = mask & ((1 << bands) - 1);
    if (active == 0)
        return; // nothing soloed -> the output passes through untouched

    juce::dsp::LinkwitzRileyFilter<float>* xs[3] = { &x1, &x2, &x3 };
    const int crossovers = bands - 1; // 1..3
    auto heard = [active] (int b) noexcept { return (active & (1 << b)) != 0; };

    for (int n = 0; n < numSamples; ++n)
    {
        // Glide cutoffs so dragging a split while soloing can't chirp (0.6.7 #1).
        for (int i = 0; i < crossovers; ++i)
        {
            if (std::abs (currentF[i] - targetF[i]) > 0.05f)
            {
                currentF[i] = targetF[i] > currentF[i]
                                ? juce::jmin (targetF[i], currentF[i] * glideCoeff)
                                : juce::jmax (targetF[i], currentF[i] / glideCoeff);
                xs[i]->setCutoffFrequency (currentF[i]);
            }
        }

        float curL = left[n], curR = right[n];
        float accL = 0.0f, accR = 0.0f;

        // Peel off one low band per crossover; sum only the soloed bands.
        for (int i = 0; i < crossovers; ++i)
        {
            float loL, hiL, loR, hiR;
            xs[i]->processSample (0, curL, loL, hiL);
            xs[i]->processSample (1, curR, loR, hiR);
            if (heard (i)) { accL += loL; accR += loR; }
            curL = hiL; curR = hiR;
        }
        if (heard (crossovers)) { accL += curL; accR += curR; }

        left[n]  = accL;
        right[n] = accR;
    }
}

} // namespace anamorph
