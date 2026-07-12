#include "SoloMonitor.h"
#include <cmath>

namespace anamorph
{

void SoloMonitor::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::ignoreUnused (maxBlock); // LR4Xover state is flat, not block-sized
    for (auto* x : { &x1, &x2, &x3 })
        x->prepare (sampleRate);
    glideCoeff = std::exp2 (8.0f / (float) sr); // ~8 octaves/sec, matches the Multiband
    for (int i = 0; i < 3; ++i) { currentF[i] = targetF[i]; }
    x1.setCutoffFrequency (currentF[0]);
    x2.setCutoffFrequency (currentF[1]);
    x3.setCutoffFrequency (currentF[2]);

    // ~12 ms crossfade: long enough to be click-free, short enough to feel instant.
    const double xfade = 0.012;
    passGain.reset (sampleRate, xfade);
    for (auto& g : bandGain) g.reset (sampleRate, xfade);

    reset();
}

void SoloMonitor::reset()
{
    x1.reset();
    x2.reset();
    x3.reset();
    passGain.setCurrentAndTargetValue (1.0f);     // settle to the true passthrough
    for (auto& g : bandGain) g.setCurrentAndTargetValue (0.0f);
    running = true; // bank just cleaned: warm by definition (no stale state to flush)
}

void SoloMonitor::setCrossovers (float f1, float f2, float f3) noexcept
{
    // Nyquist-safe clamp + ordering, identical to MultibandWidth (0.8.2): the monitor
    // mirrors the same band split, so it must reject the same out-of-range automation
    // that would otherwise blow up the Linkwitz-Riley coefficients.
    const float fMax = juce::jmax (1000.0f, 0.45f * (float) sr);
    const float fMin = 20.0f;
    f1 = juce::jlimit (fMin, fMax, f1);
    f2 = juce::jlimit (fMin, fMax, f2);
    f3 = juce::jlimit (fMin, fMax, f3);
    f2 = juce::jmax (f2, f1 * 1.1f);
    f3 = juce::jmax (f3, f2 * 1.1f);
    f3 = juce::jmin (f3, fMax);
    f2 = juce::jmin (f2, f3 / 1.1f);
    f1 = juce::jmin (f1, f2 / 1.1f);
    targetF[0] = f1;
    targetF[1] = f2;
    targetF[2] = f3;
}

void SoloMonitor::process (float* left, float* right, int mask, int numSamples) noexcept
{
    const int active = mask & ((1 << bands) - 1);
    const bool anySolo = active != 0;

    LR4Xover* xs[3] = { &x1, &x2, &x3 };
    const int crossovers = bands - 1; // 0..3
    auto heard = [active] (int b) noexcept { return (active & (1 << b)) != 0; };

    // Targets for the click-free crossfade. The passthrough is heard when nothing is
    // soloed; each active band's gain is 1 only while that band is soloed. The bands
    // ABOVE the active count stay at 0 so a band-count change settles cleanly.
    passGain.setTargetValue (anySolo ? 0.0f : 1.0f);
    for (int b = 0; b < 4; ++b)
        bandGain[b].setTargetValue ((b <= crossovers && heard (b)) ? 1.0f : 0.0f);

    // ---- Settled fast path (H1, 0.8.9) --------------------------------------
    // With nothing soloed and every smoother fully settled, each sample computes
    // exactly 1*in + 0*band0 + ... : the passthrough the header already documents
    // as the bit-exact true output. Skipping the whole loop reproduces it without
    // running 6 LR4 filters + 5 settled smoother ticks per sample (~half of the
    // transparent engine floor, measured 0.8.8). Exact compares, no epsilon: a
    // settled JUCE SmoothedValue holds its target exactly, and getNextValue() on
    // a settled smoother is mutation-free, so skipping it is state-identical.
    // The gate sits AFTER the setTargetValue calls above: any target change this
    // block arms the ramp -> isSmoothing() -> the crossfade advances as always.
    // The crossover-glide check mirrors the loop's own 0.05 Hz idle criterion.
    if (! anySolo && ! passGain.isSmoothing()
        && ! (std::abs (passGain.getCurrentValue() - 1.0f) > 0.0f))
    {
        bool settled = true;
        for (int b = 0; b < 4 && settled; ++b)
            settled = ! bandGain[b].isSmoothing()
                   && ! (std::abs (bandGain[b].getCurrentValue()) > 0.0f);
        for (int i = 0; i < crossovers && settled; ++i)
            settled = ! (std::abs (currentF[i] - targetF[i]) > 0.05f);
        if (settled)
        {
            running = false; // filters go cold; the re-entry below re-warms them
            return;
        }
    }

    // Cold re-entry (the engine's mbRunning warm/cold pattern): the filters were
    // skipped while the monitor sat at settled passthrough, so their state is
    // stale. Clear them and snap the cutoff glide to its target NOW, while every
    // band gain is still ~0 and the passthrough still carries the output -- the
    // ~12 ms crossfade below masks the fresh filters' charge-up exactly as it
    // masks a Multiband cold enable. The gain smoothers are NOT touched: their
    // just-set targets ARE the click-free crossfade.
    if (! running)
    {
        x1.reset();
        x2.reset();
        x3.reset();
        for (int i = 0; i < 3; ++i) currentF[i] = targetF[i];
        x1.setCutoffFrequency (currentF[0]);
        x2.setCutoffFrequency (currentF[1]);
        x3.setCutoffFrequency (currentF[2]);
        running = true;
    }

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

        const float inL = left[n], inR = right[n];
        // The filters run unconditionally (kept warm), so an engage has no charge-up
        // transient; gains are summed in, so nothing soloed -> passGain 1 -> true output.
        const float pg = passGain.getNextValue();
        float accL = pg * inL, accR = pg * inR;

        float curL = inL, curR = inR;
        for (int i = 0; i < crossovers; ++i)
        {
            float loL, hiL, loR, hiR;
            xs[i]->processSample (0, curL, loL, hiL);
            xs[i]->processSample (1, curR, loR, hiR);
            const float g = bandGain[i].getNextValue();
            accL += g * loL; accR += g * loR;
            curL = hiL; curR = hiR;
        }
        const float gTop = bandGain[crossovers].getNextValue(); // highest band = the remaining high
        accL += gTop * curL; accR += gTop * curR;

        // Advance the unused band smoothers so they keep tracking 0 in lock-step.
        for (int b = crossovers + 1; b < 4; ++b) bandGain[b].getNextValue();

        left[n]  = accL;
        right[n] = accR;
    }
}

} // namespace anamorph
