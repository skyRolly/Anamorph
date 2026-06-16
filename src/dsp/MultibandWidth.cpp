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
    for (int n = 0; n < numSamples; ++n)
    {
        float b1L, r1L, b1R, r1R;
        x1.processSample (0, left[n],  b1L, r1L);
        x1.processSample (1, right[n], b1R, r1R);

        float b2L, r2L, b2R, r2R;
        x2.processSample (0, r1L, b2L, r2L);
        x2.processSample (1, r1R, b2R, r2R);

        float b3L, b4L, b3R, b4R;
        x3.processSample (0, r2L, b3L, b4L);
        x3.processSample (1, r2R, b3R, b4R);

        applyWidth (b1L, b1R, w1);
        applyWidth (b2L, b2R, w2);
        applyWidth (b3L, b3R, w3);
        applyWidth (b4L, b4R, w4);

        left[n]  = b1L + b2L + b3L + b4L;
        right[n] = b1R + b2R + b3R + b4R;
    }
}

} // namespace anamorph
