#include "MultibandWidth.h"
#include "MidSide.h"
#include <cmath>

namespace anamorph
{

void MultibandWidth::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
    for (auto* x : { &x1, &x2, &x3 })
    {
        x->prepare (spec);
        x->setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    }
    // ~8 octaves/sec slew cap (matches Mono Maker), so a quick split drag never
    // modulates the LR cutoff fast enough to pitch-shift (0.6.7 #1).
    glideCoeff = std::exp2 (8.0f / (float) sr);
    for (int i = 0; i < 3; ++i) { currentF[i] = targetF[i]; }
    x1.setCutoffFrequency (currentF[0]);
    x2.setCutoffFrequency (currentF[1]);
    x3.setCutoffFrequency (currentF[2]);
    reset();
}

void MultibandWidth::reset()
{
    x1.reset();
    x2.reset();
    x3.reset();
}

void MultibandWidth::setCrossovers (float f1, float f2, float f3) noexcept
{
    // Keep the three crossovers strictly ordered with a little separation, so a
    // drag can't cross them over. These are TARGETS; the cutoffs glide toward them
    // per sample in processBlock (0.6.7 #1).
    f2 = juce::jmax (f2, f1 * 1.1f);
    f3 = juce::jmax (f3, f2 * 1.1f);
    targetF[0] = f1;
    targetF[1] = f2;
    targetF[2] = f3;
}

void MultibandWidth::processBlock (float* left, float* right, int numSamples) noexcept
{
    // A solo only counts if it points at an active band; otherwise full mix.
    const int solo = (soloBand >= 0 && soloBand < bands) ? soloBand : -1;

    // One band: a plain MS width on the whole signal, no crossovers.
    if (bands <= 1)
    {
        for (int n = 0; n < numSamples; ++n)
            applyWidth (left[n], right[n], w[0]);
        return; // a single band soloed is just itself
    }

    juce::dsp::LinkwitzRileyFilter<float>* xs[3] = { &x1, &x2, &x3 };
    const int crossovers = bands - 1; // 1..3

    for (int n = 0; n < numSamples; ++n)
    {
        // Glide each active cutoff toward its target, capped at a fixed
        // octaves/second rate so a quick split drag never chirps (0.6.7 #1).
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

        // Peel off one low band per crossover; the running remainder feeds the next.
        for (int i = 0; i < crossovers; ++i)
        {
            float loL, hiL, loR, hiR;
            xs[i]->processSample (0, curL, loL, hiL);
            xs[i]->processSample (1, curR, loR, hiR);
            applyWidth (loL, loR, w[i]);
            if (solo < 0 || solo == i) { accL += loL; accR += loR; }
            curL = hiL; curR = hiR;
        }

        // The final remainder is the top band.
        applyWidth (curL, curR, w[crossovers]);
        if (solo < 0 || solo == crossovers) { accL += curL; accR += curR; }

        left[n]  = accL;
        right[n] = accR;
    }
}

} // namespace anamorph
