#include "MultibandWidth.h"
#include "MidSide.h"

namespace anamorph
{

void MultibandWidth::prepare (double sampleRate, int maxBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
    xLow.prepare (spec);
    xHigh.prepare (spec);
    xLow.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xHigh.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    reset();
}

void MultibandWidth::reset()
{
    xLow.reset();
    xHigh.reset();
}

void MultibandWidth::setCrossovers (float fLowMid, float fMidHigh) noexcept
{
    // Keep the two crossovers ordered with a little separation.
    fMidHigh = juce::jmax (fMidHigh, fLowMid * 1.1f);
    xLow.setCutoffFrequency (fLowMid);
    xHigh.setCutoffFrequency (fMidHigh);
}

void MultibandWidth::processBlock (float* left, float* right, int numSamples) noexcept
{
    for (int n = 0; n < numSamples; ++n)
    {
        float lowL, restL, lowR, restR;
        xLow.processSample (0, left[n],  lowL, restL);
        xLow.processSample (1, right[n], lowR, restR);

        float midL, highL, midR, highR;
        xHigh.processSample (0, restL, midL, highL);
        xHigh.processSample (1, restR, midR, highR);

        applyWidth (lowL,  lowR,  wLow);
        applyWidth (midL,  midR,  wMid);
        applyWidth (highL, highR, wHigh);

        left[n]  = lowL + midL + highL;
        right[n] = lowR + midR + highR;
    }
}

} // namespace anamorph
