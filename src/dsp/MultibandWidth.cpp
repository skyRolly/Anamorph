#include "MultibandWidth.h"
#include "MidSide.h"

namespace anamorph
{

void MultibandWidth::prepare (double sampleRate, int maxBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
    for (auto* x : { &x1, &x2, &x3 })
    {
        x->prepare (spec);
        x->setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    }
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
    // drag can't cross them over.
    f2 = juce::jmax (f2, f1 * 1.1f);
    f3 = juce::jmax (f3, f2 * 1.1f);
    x1.setCutoffFrequency (f1);
    x2.setCutoffFrequency (f2);
    x3.setCutoffFrequency (f3);
}

void MultibandWidth::processBlock (float* left, float* right, int numSamples) noexcept
{
    // One band: a plain MS width on the whole signal, no crossovers.
    if (bands <= 1)
    {
        for (int n = 0; n < numSamples; ++n)
            applyWidth (left[n], right[n], w[0]);
        return;
    }

    juce::dsp::LinkwitzRileyFilter<float>* xs[3] = { &x1, &x2, &x3 };
    const int crossovers = bands - 1; // 1..3

    for (int n = 0; n < numSamples; ++n)
    {
        float curL = left[n], curR = right[n];
        float accL = 0.0f, accR = 0.0f;

        // Peel off one low band per crossover; the running remainder feeds the next.
        for (int i = 0; i < crossovers; ++i)
        {
            float loL, hiL, loR, hiR;
            xs[i]->processSample (0, curL, loL, hiL);
            xs[i]->processSample (1, curR, loR, hiR);
            applyWidth (loL, loR, w[i]);
            accL += loL; accR += loR;
            curL = hiL; curR = hiR;
        }

        // The final remainder is the top band.
        applyWidth (curL, curR, w[crossovers]);
        accL += curL; accR += curR;

        left[n]  = accL;
        right[n] = accR;
    }
}

} // namespace anamorph
