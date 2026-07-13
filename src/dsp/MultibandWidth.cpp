#include "MultibandWidth.h"
#include "MidSide.h"
#include <cmath>

namespace anamorph
{

void MultibandWidth::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::ignoreUnused (maxBlock); // LR4Xover state is flat, not block-sized
    for (auto* x : { &x1, &x2, &x3, &dx1, &dx2, &dx3,
                     &ax[0], &ax[1], &ax[2], &dax[0], &dax[1], &dax[2] })
        x->prepare (sampleRate);
    // ~8 octaves/sec slew cap (matches Mono Maker), so a quick split drag never
    // modulates the LR cutoff fast enough to pitch-shift (0.6.7 #1).
    glideCoeff = std::exp2 (8.0f / (float) sr);
    for (int i = 0; i < 3; ++i) { currentF[i] = targetF[i]; }
    x1.setCutoffFrequency (currentF[0]);
    x2.setCutoffFrequency (currentF[1]);
    x3.setCutoffFrequency (currentF[2]);
    // The dry-align bank tracks the same cutoffs in lockstep (Known Issue #1).
    dx1.setCutoffFrequency (currentF[0]);
    dx2.setCutoffFrequency (currentF[1]);
    dx3.setCutoffFrequency (currentF[2]);
    // The compensation allpasses share each split's cutoff (ax[i]/dax[i] at f[i]).
    for (int i = 0; i < 3; ++i) { ax[i].setCutoffFrequency (currentF[i]); dax[i].setCutoffFrequency (currentF[i]); }
    // One-pole on the band widths at ~20 ms (the global Width smoother's ramp),
    // so a fast band-width drag glides instead of stepping per block (0.7.0 #1).
    wCoeff = std::exp (-1.0f / (0.02f * (float) sr));
    for (int i = 0; i < 4; ++i) { currentW[i] = targetW[i]; }
    reset();
}

void MultibandWidth::reset()
{
    x1.reset();
    x2.reset();
    x3.reset();
    dx1.reset();
    dx2.reset();
    dx3.reset();
    for (int i = 0; i < 3; ++i) { ax[i].reset(); dax[i].reset(); }
    // Settle the width glide to its target. reset() only runs while the engine is
    // ducked to silence (or in prepare), so snapping here can never click and the
    // band starts at its true width when audio resumes.
    for (int i = 0; i < 4; ++i) { currentW[i] = targetW[i]; }
}

void MultibandWidth::setCrossovers (float f1, float f2, float f3) noexcept
{
    // Keep the three crossovers strictly ordered with a little separation, so a
    // drag can't cross them over. These are TARGETS; the cutoffs glide toward them
    // per sample in processBlock (0.6.7 #1).
    //
    // CRITICAL (0.8.2): clamp every crossover to a Nyquist-safe band [20 Hz, 0.45*sr]
    // BEFORE ordering. Automating a split toward 20 kHz used to push the ordered
    // separation (f3 >= f2*1.1 >= f1*1.21) above Nyquist, where the Linkwitz-Riley
    // coefficients blow up to NaN/Inf -- the "+600 dB" burst that left one channel
    // stuck and the other dead. The ceiling is enforced from the TOP down so the
    // 1.1x separation can never lift a cutoff past it, whatever the automation does.
    const float fMax = juce::jmax (1000.0f, 0.45f * (float) sr); // safe below Nyquist
    const float fMin = 20.0f;
    f1 = juce::jlimit (fMin, fMax, f1);
    f2 = juce::jlimit (fMin, fMax, f2);
    f3 = juce::jlimit (fMin, fMax, f3);
    f2 = juce::jmax (f2, f1 * 1.1f);
    f3 = juce::jmax (f3, f2 * 1.1f);
    // Re-clamp downward so the separation never pushes a cutoff above the ceiling.
    f3 = juce::jmin (f3, fMax);
    f2 = juce::jmin (f2, f3 / 1.1f);
    f1 = juce::jmin (f1, f2 / 1.1f);
    targetF[0] = f1;
    targetF[1] = f2;
    targetF[2] = f3;
}

void MultibandWidth::processBlock (float* left, float* right, int numSamples,
                                   const float* dryInL, const float* dryInR,
                                   float* dryOutL, float* dryOutR) noexcept
{
    const bool align = (dryOutL != nullptr && dryOutR != nullptr
                        && dryInL != nullptr && dryInR != nullptr);

    // One band: a plain MS width on the whole signal, no crossovers. With no
    // crossover the reconstruction is identity (A == 1), so the dry is already
    // phase-identical -- just pass it through unchanged.
    if (bands <= 1)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            currentW[0] = targetW[0] + (currentW[0] - targetW[0]) * wCoeff;
            applyWidth (left[n], right[n], currentW[0]);
        }
        if (align)
            for (int n = 0; n < numSamples; ++n) { dryOutL[n] = dryInL[n]; dryOutR[n] = dryInR[n]; }
        return;
    }

    LR4Xover* xs[3]  = { &x1, &x2, &x3 };
    LR4Xover* dxs[3] = { &dx1, &dx2, &dx3 };
    const int crossovers = bands - 1; // 1..3

    // Re-sync the dry bank's cutoffs to the live values at block start, so a block
    // that resumes aligning starts phase-matched (Known Issue #1). The dry
    // compensation allpasses re-sync with it.
    if (align)
        for (int i = 0; i < crossovers; ++i)
        {
            dxs[i]->setCutoffFrequency (currentF[i]);
            dax[i].setCutoffFrequency (currentF[i]);
        }

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
                ax[i].setCutoffFrequency (currentF[i]); // compensation allpass glides with its split
                if (align) { dxs[i]->setCutoffFrequency (currentF[i]); dax[i].setCutoffFrequency (currentF[i]); } // dry bank locked to the wet
            }
        }

        // Glide each active band width toward its target (one-pole), so a fast
        // band-width drag never steps the side-gain between blocks (0.7.0 #1).
        for (int i = 0; i <= crossovers; ++i)
            currentW[i] = targetW[i] + (currentW[i] - targetW[i]) * wCoeff;

        float curL = left[n], curR = right[n];
        float loL, hiL, loR, hiR;

        // Band 0 (lowest): peel it off, it seeds the running low-sum.
        xs[0]->processSample (0, curL, loL, hiL);
        xs[0]->processSample (1, curR, loR, hiR);
        applyWidth (loL, loR, currentW[0]);
        float accL = loL, accR = loR;
        curL = hiL; curR = hiR;

        // Each higher split: phase-align the running low-sum through THIS split's
        // allpass (LR4 lo+hi) so it stays coherent with the bands above it, THEN
        // peel + add the next band. This telescopes the sum to an allpass (flat)
        // instead of dipping around close crossovers (naive sum was b0+b1+b2+b3).
        for (int i = 1; i < crossovers; ++i)
        {
            float aL0, aL1, aR0, aR1;
            ax[i].processSample (0, accL, aL0, aL1); accL = aL0 + aL1;
            ax[i].processSample (1, accR, aR0, aR1); accR = aR0 + aR1;

            xs[i]->processSample (0, curL, loL, hiL);
            xs[i]->processSample (1, curR, loR, hiR);
            applyWidth (loL, loR, currentW[i]);
            accL += loL; accR += loR;
            curL = hiL; curR = hiR;
        }

        // The final remainder is the top band (no further allpass -- it already
        // carries every split's high-side phase).
        applyWidth (curL, curR, currentW[crossovers]);
        accL += curL; accR += curR;

        left[n]  = accL;
        right[n] = accR;

        // Phase-matched dry: reconstruct the dry through the SAME crossovers at unit
        // width -- the full A(dry) -- sharing the wet's exact per-sample cutoffs above,
        // so it can never lag the glide and re-introduce comb during a split drag (KI #1).
        if (align)
        {
            float dcurL = dryInL[n], dcurR = dryInR[n];
            float dloL, dhiL, dloR, dhiR;
            dxs[0]->processSample (0, dcurL, dloL, dhiL);
            dxs[0]->processSample (1, dcurR, dloR, dhiR);
            float daccL = dloL, daccR = dloR;   // unit width -> just the band itself
            dcurL = dhiL; dcurR = dhiR;

            // Same allpass phase-compensation as the wet, so A(dry) telescopes to
            // the identical A1.A2.A3 allpass -- the dry/wet Mix stays comb-free AND
            // the dry reference is itself flat (Known Issue #1 + flat recombination).
            for (int i = 1; i < crossovers; ++i)
            {
                float aL0, aL1, aR0, aR1;
                dax[i].processSample (0, daccL, aL0, aL1); daccL = aL0 + aL1;
                dax[i].processSample (1, daccR, aR0, aR1); daccR = aR0 + aR1;

                dxs[i]->processSample (0, dcurL, dloL, dhiL);
                dxs[i]->processSample (1, dcurR, dloR, dhiR);
                daccL += dloL; daccR += dloR;
                dcurL = dhiL; dcurR = dhiR;
            }
            daccL += dcurL; daccR += dcurR;
            dryOutL[n] = daccL; dryOutR[n] = daccR;
        }
    }
}

} // namespace anamorph
