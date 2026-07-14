#include "SoloMonitor.h"
#include <cmath>

namespace anamorph
{

void SoloMonitor::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::ignoreUnused (maxBlock); // LR4Xover state is flat, not block-sized
    for (auto* b : { &bank[0], &bank[1] })
        for (int i = 0; i < 3; ++i)
            b->x[i].prepare (sampleRate);
    // ~12 ms cutoff crossfade for DISCRETE target jumps only, matching the
    // Multiband (0.8.10) and the gain crossfade below.
    fadeLen = juce::jmax (1, (int) std::lround (0.012 * sampleRate));
    // Hard ~1 oct/s cutoff rate cap: bounds the swept-allpass frequency shift
    // at ~0.31 Hz, below the pure-tone JND (see MultibandWidth.h).
    glideStep = std::exp2 (1.0f / (float) sr);

    // ~12 ms crossfade: long enough to be click-free, short enough to feel instant.
    const double xfade = 0.012;
    passGain.reset (sampleRate, xfade);
    for (auto& g : bandGain) g.reset (sampleRate, xfade);

    reset();
}

void SoloMonitor::reset()
{
    for (auto* b : { &bank[0], &bank[1] })
        for (int i = 0; i < 3; ++i)
            b->x[i].reset();
    // Snap both banks to the target cutoffs and drop any fade in flight (reset
    // runs under the engine duck / at prepare, so the jump is inaudible).
    setBankCutoffs (bank[0]);
    setBankCutoffs (bank[1]);
    for (int i = 0; i < 3; ++i) prevTargetF[i] = targetF[i];
    active      = 0;
    fading      = false;
    fadePos     = 0;
    pendingJump = false;
    passGain.setCurrentAndTargetValue (1.0f);     // settle to the true passthrough
    for (auto& g : bandGain) g.setCurrentAndTargetValue (0.0f);
    running = true; // bank just cleaned: warm by definition (no stale state to flush)
}

void SoloMonitor::setBankCutoffs (XoverBank& b) noexcept
{
    for (int i = 0; i < 3; ++i)
    {
        b.f[i] = targetF[i];
        b.x[i].setCutoffFrequency (targetF[i]);
    }
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
    const int active_ = mask & ((1 << bands) - 1);
    const bool anySolo = active_ != 0;

    const int crossovers = bands - 1; // 0..3
    auto heard = [active_] (int b) noexcept { return (active_ & (1 << b)) != 0; };

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
    // The cutoff check mirrors the fade trigger's 0.05 Hz criterion below.
    if (! anySolo && ! fading && ! passGain.isSmoothing()
        && ! (std::abs (passGain.getCurrentValue() - 1.0f) > 0.0f))
    {
        bool settled = true;
        for (int b = 0; b < 4 && settled; ++b)
            settled = ! bandGain[b].isSmoothing()
                   && ! (std::abs (bandGain[b].getCurrentValue()) > 0.0f);
        for (int i = 0; i < crossovers && settled; ++i)
            settled = ! (std::abs (bank[active].f[i] - targetF[i]) > 0.05f);
        if (settled)
        {
            running = false; // filters go cold; the re-entry below re-warms them
            return;
        }
    }

    // Cold re-entry (the engine's mbRunning warm/cold pattern): the filters were
    // skipped while the monitor sat at settled passthrough, so their state is
    // stale. Clear them and snap the cutoffs to their targets NOW, while every
    // band gain is still ~0 and the passthrough still carries the output -- the
    // ~12 ms crossfade below masks the fresh filters' charge-up exactly as it
    // masks a Multiband cold enable. The gain smoothers are NOT touched: their
    // just-set targets ARE the click-free crossfade.
    if (! running)
    {
        for (auto* b : { &bank[0], &bank[1] })
            for (int i = 0; i < 3; ++i)
                b->x[i].reset();
        setBankCutoffs (bank[active]);
        fading  = false;
        fadePos = 0;
        running = true;
    }

    // DISCRETE-JUMP bank crossfade (0.8.10, mirrors MultibandWidth): only a
    // TARGET that stepped > kFadeThresholdOct since the previous block fades to
    // the state-copied idle bank -- one bounded event. Continuous movement of
    // any speed glides per sample below under the ~1 oct/s inaudibility cap.
    {
        bool step = pendingJump;
        for (int i = 0; i < crossovers && ! step; ++i)
            step = std::abs (std::log2 (targetF[i] / prevTargetF[i])) > kFadeThresholdOct;
        for (int i = 0; i < 3; ++i) prevTargetF[i] = targetF[i];
        if (step)
        {
            if (fading)
                pendingJump = true;             // remember; fire when this fade lands
            else
            {
                bool worthIt = false;           // skip if the target came back meanwhile
                for (int i = 0; i < crossovers && ! worthIt; ++i)
                    worthIt = std::abs (std::log2 (bank[active].f[i] / targetF[i])) > 0.1f;
                pendingJump = false;
                if (worthIt)
                {
                    auto& to = bank[1 - active];
                    for (int i = 0; i < 3; ++i)
                        to.x[i].copyStateFrom (bank[active].x[i]);
                    setBankCutoffs (to);
                    fading  = true;
                    fadePos = 0;
                }
            }
        }
    }

    // One bank, one sample: the passthrough plus every band's gain-blended sum.
    // Identical arithmetic to the pre-0.8.10 single-bank loop (the gains are
    // hoisted -- each smoother still advances exactly once per sample).
    auto runBank = [crossovers] (XoverBank& bk, float inL, float inR,
                                 float pg, const float* g,
                                 float& outL, float& outR) noexcept
    {
        float accL = pg * inL, accR = pg * inR;
        float curL = inL, curR = inR;
        for (int i = 0; i < crossovers; ++i)
        {
            float loL, hiL, loR, hiR;
            bk.x[i].processSample (0, curL, loL, hiL);
            bk.x[i].processSample (1, curR, loR, hiR);
            accL += g[i] * loL; accR += g[i] * loR;
            curL = hiL; curR = hiR;
        }
        accL += g[crossovers] * curL; accR += g[crossovers] * curR;
        outL = accL; outR = accR;
    };

    for (int n = 0; n < numSamples; ++n)
    {
        // RATE-CAPPED cutoff glide (0.8.10): ease each active split toward its
        // target per sample at <= ~1 oct/s (the swept-allpass shift stays below
        // the pure-tone JND); paused while a discrete-jump fade is in flight.
        if (! fading)
        {
            auto& bk = bank[active];
            for (int i = 0; i < crossovers; ++i)
                if (std::abs (bk.f[i] - targetF[i]) > 0.05f)
                {
                    bk.f[i] = targetF[i] > bk.f[i]
                                ? juce::jmin (targetF[i], bk.f[i] * glideStep)
                                : juce::jmax (targetF[i], bk.f[i] / glideStep);
                    bk.x[i].setCutoffFrequency (bk.f[i]);
                }
        }

        // Advance every smoother exactly once per sample (bank-independent).
        const float pg = passGain.getNextValue();
        float g[4];
        for (int b = 0; b <= crossovers; ++b) g[b] = bandGain[b].getNextValue();
        for (int b = crossovers + 1; b < 4; ++b) { g[b] = 0.0f; bandGain[b].getNextValue(); }

        const float inL = left[n], inR = right[n];

        if (fading)
        {
            float aL, aR, bL, bR;
            runBank (bank[active],     inL, inR, pg, g, aL, aR);
            runBank (bank[1 - active], inL, inR, pg, g, bL, bR);
            ++fadePos;
            const float w = (float) fadePos / (float) fadeLen; // ends at exactly 1
            left[n]  = aL + w * (bL - aL);
            right[n] = aR + w * (bR - aR);
            if (fadePos >= fadeLen)
            {
                active = 1 - active; // the new-cutoff bank takes over
                fading = false;      // a further move retriggers next block
            }
        }
        else
        {
            runBank (bank[active], inL, inR, pg, g, left[n], right[n]);
        }
    }
}

} // namespace anamorph
