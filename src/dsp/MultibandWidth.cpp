#include "MultibandWidth.h"
#include "MidSide.h"
#include <cmath>

namespace anamorph
{

void MultibandWidth::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::ignoreUnused (maxBlock); // LR4Xover state is flat, not block-sized
    for (auto* b : { &bank[0], &bank[1] })
        for (auto* x : { &b->x[0], &b->x[1], &b->x[2], &b->dx[0], &b->dx[1], &b->dx[2],
                         &b->ax[0], &b->ax[1], &b->ax[2], &b->dax[0], &b->dax[1], &b->dax[2] })
            x->prepare (sampleRate);
    // ~12 ms cutoff crossfade for DISCRETE target jumps only: the house
    // click-free fade length (SoloMonitor / Multiband Enable use the same).
    fadeLen = juce::jmax (1, (int) std::lround (0.012 * sampleRate));
    // Hard ~1 oct/s cutoff rate cap: a swept LR4 shifts every frequency by
    // 0.312*R Hz at sweep rate R, so this bounds the shift at ~0.31 Hz --
    // below the pure-tone JND at any drag speed (see MultibandWidth.h).
    glideStep = std::exp2 (1.0f / (float) sr);
    // One-pole on the band widths at ~20 ms (the global Width smoother's ramp),
    // so a fast band-width drag glides instead of stepping per block (0.7.0 #1).
    wCoeff = std::exp (-1.0f / (0.02f * (float) sr));
    for (int i = 0; i < 4; ++i) { currentW[i] = targetW[i]; }
    reset();
}

void MultibandWidth::reset()
{
    for (auto* b : { &bank[0], &bank[1] })
        for (auto* x : { &b->x[0], &b->x[1], &b->x[2], &b->dx[0], &b->dx[1], &b->dx[2],
                         &b->ax[0], &b->ax[1], &b->ax[2], &b->dax[0], &b->dax[1], &b->dax[2] })
            x->reset();
    // Snap both banks to the target cutoffs and drop any fade in flight. reset()
    // only runs while the engine is ducked to silence (or in prepare / while the
    // enable blend is ~0), so the coefficient jump can never click and the bank
    // starts at its true crossovers when audio resumes.
    setBankCutoffs (bank[0]);
    setBankCutoffs (bank[1]);
    for (int i = 0; i < 3; ++i) prevTargetF[i] = targetF[i];
    active      = 0;
    fading      = false;
    fadePos     = 0;
    pendingJump = false;
    // Settle the width glide to its target (same silent-reset argument).
    for (int i = 0; i < 4; ++i) { currentW[i] = targetW[i]; }
}

void MultibandWidth::setBankCutoffs (XoverBank& b) noexcept
{
    for (int i = 0; i < 3; ++i)
    {
        b.f[i] = targetF[i];
        b.x[i] .setCutoffFrequency (targetF[i]);
        b.dx[i].setCutoffFrequency (targetF[i]);  // dry twins stay phase-locked (KI #1)
        b.ax[i].setCutoffFrequency (targetF[i]);  // compensation allpasses share the split cutoff
        b.dax[i].setCutoffFrequency (targetF[i]);
    }
}

void MultibandWidth::copyBankState (XoverBank& to, const XoverBank& from) noexcept
{
    for (int i = 0; i < 3; ++i)
    {
        to.x[i]  .copyStateFrom (from.x[i]);
        to.dx[i] .copyStateFrom (from.dx[i]);
        to.ax[i] .copyStateFrom (from.ax[i]);
        to.dax[i].copyStateFrom (from.dax[i]);
    }
}

void MultibandWidth::setCrossovers (float f1, float f2, float f3) noexcept
{
    // Keep the three crossovers strictly ordered with a little separation, so a
    // drag can't cross them over. These are TARGETS; processBlock eases the live
    // cutoffs toward them under a ~1 oct/s inaudibility cap (or a single bank
    // crossfade for a discrete multi-octave step -- 0.8.10, see MultibandWidth.h).
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

    const int crossovers = bands - 1; // 1..3

    // DISCRETE-JUMP bank crossfade (0.8.10): only a TARGET that stepped more
    // than kFadeThresholdOct since the previous block (an automation step or
    // preset-style snap -- a mouse drag at UI cadence never steps that far in
    // one block) hands the newest cutoffs to the idle bank -- which adopts the
    // active bank's ladder state, so it starts transient-free -- and fades the
    // output over to it: one bounded transition event instead of a multi-second
    // crawl. Continuous movement of ANY speed never fades; it glides per sample
    // under the ~1 oct/s cap below (inaudible by construction). A step arriving
    // mid-fade latches pendingJump and re-fires at the next block after the
    // fade lands, straight to the LATEST target.
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
                    copyBankState (to, bank[active]);
                    setBankCutoffs (to);
                    fading  = true;
                    fadePos = 0;
                }
            }
        }
    }

    // Re-sync the dry bank's cutoffs to the live (possibly gliding) values at
    // block start, so a block that resumes aligning starts phase-matched -- the
    // per-sample glide below only updates dx/dax while align is on, exactly like
    // the pre-0.8.10 glide (Known Issue #1). The dry compensation allpasses
    // re-sync with it. (A fade needs none of this: setBankCutoffs assigns all
    // twelve filters of the incoming bank.)
    if (align && ! fading)
    {
        auto& bk = bank[active];
        for (int i = 0; i < crossovers; ++i)
        {
            bk.dx[i] .setCutoffFrequency (bk.f[i]);
            bk.dax[i].setCutoffFrequency (bk.f[i]);
        }
    }

    // Full reconstruction of one sample through one bank. Identical arithmetic
    // (expression for expression) to the pre-0.8.10 single-bank loop, so the
    // settled output is bit-exact with it:
    //   band 0 peels off and seeds the running low-sum; each higher split first
    //   phase-aligns the low-sum through THAT split's allpass (LR4 lo+hi), then
    //   peels + adds the next band, telescoping the sum to A1.A2.A3 (flat); the
    //   final remainder is the top band (it already carries every split's
    //   high-side phase).
    auto runWet = [this, crossovers] (XoverBank& bk, float inL, float inR,
                                      float& outL, float& outR) noexcept
    {
        float curL = inL, curR = inR;
        float loL, hiL, loR, hiR;

        bk.x[0].processSample (0, curL, loL, hiL);
        bk.x[0].processSample (1, curR, loR, hiR);
        applyWidth (loL, loR, currentW[0]);
        float accL = loL, accR = loR;
        curL = hiL; curR = hiR;

        for (int i = 1; i < crossovers; ++i)
        {
            float aL0, aL1, aR0, aR1;
            bk.ax[i].processSample (0, accL, aL0, aL1); accL = aL0 + aL1;
            bk.ax[i].processSample (1, accR, aR0, aR1); accR = aR0 + aR1;

            bk.x[i].processSample (0, curL, loL, hiL);
            bk.x[i].processSample (1, curR, loR, hiR);
            applyWidth (loL, loR, currentW[i]);
            accL += loL; accR += loR;
            curL = hiL; curR = hiR;
        }

        applyWidth (curL, curR, currentW[crossovers]);
        outL = accL + curL;
        outR = accR + curR;
    };

    // The dry twin: reconstruct the dry through the SAME fixed crossovers at unit
    // width -- the full A(dry) -- so it is phase-identical to the wet at every
    // instant, including mid-fade (both sides of the fade are blended with the
    // same weights), and the dry/wet Mix never combs (KI #1 + flat recombination).
    auto runDry = [crossovers] (XoverBank& bk, float inL, float inR,
                                float& outL, float& outR) noexcept
    {
        float curL = inL, curR = inR;
        float loL, hiL, loR, hiR;

        bk.dx[0].processSample (0, curL, loL, hiL);
        bk.dx[0].processSample (1, curR, loR, hiR);
        float accL = loL, accR = loR;   // unit width -> just the band itself
        curL = hiL; curR = hiR;

        for (int i = 1; i < crossovers; ++i)
        {
            float aL0, aL1, aR0, aR1;
            bk.dax[i].processSample (0, accL, aL0, aL1); accL = aL0 + aL1;
            bk.dax[i].processSample (1, accR, aR0, aR1); accR = aR0 + aR1;

            bk.dx[i].processSample (0, curL, loL, hiL);
            bk.dx[i].processSample (1, curR, loR, hiR);
            accL += loL; accR += loR;
            curL = hiL; curR = hiR;
        }
        outL = accL + curL;
        outR = accR + curR;
    };

    for (int n = 0; n < numSamples; ++n)
    {
        // RATE-CAPPED cutoff glide (0.8.10): each active split eases toward its
        // target per sample at <= ~1 oct/s, so the swept-allpass frequency
        // shift stays at ~0.31 Hz -- below the pure-tone JND at any drag speed
        // (see MultibandWidth.h) -- while the reconstruction stays a true
        // flat-magnitude LR4 allpass at every instant. Paused while a
        // discrete-jump fade is in flight (its banks hold fixed coefficients);
        // small residues keep gliding the block after the fade lands.
        if (! fading)
        {
            auto& bk = bank[active];
            for (int i = 0; i < crossovers; ++i)
            {
                if (std::abs (bk.f[i] - targetF[i]) > 0.05f)
                {
                    bk.f[i] = targetF[i] > bk.f[i]
                                ? juce::jmin (targetF[i], bk.f[i] * glideStep)
                                : juce::jmax (targetF[i], bk.f[i] / glideStep);
                    bk.x[i] .setCutoffFrequency (bk.f[i]);
                    bk.ax[i].setCutoffFrequency (bk.f[i]); // compensation allpass glides with its split
                    if (align) { bk.dx[i].setCutoffFrequency (bk.f[i]); bk.dax[i].setCutoffFrequency (bk.f[i]); } // dry bank locked to the wet
                }
            }
        }

        // Glide each active band width toward its target (one-pole), so a fast
        // band-width drag never steps the side-gain between blocks (0.7.0 #1).
        // Shared by both banks: a fade never changes a band's width.
        for (int i = 0; i <= crossovers; ++i)
            currentW[i] = targetW[i] + (currentW[i] - targetW[i]) * wCoeff;

        if (fading)
        {
            auto& from = bank[active];
            auto& to   = bank[1 - active];

            float aL, aR, bL, bR;
            runWet (from, left[n], right[n], aL, aR);
            runWet (to,   left[n], right[n], bL, bR);
            ++fadePos;
            const float w = (float) fadePos / (float) fadeLen; // ends at exactly 1
            left[n]  = aL + w * (bL - aL);
            right[n] = aR + w * (bR - aR);

            if (align)
            {
                float daL, daR, dbL, dbR;
                runDry (from, dryInL[n], dryInR[n], daL, daR);
                runDry (to,   dryInL[n], dryInR[n], dbL, dbR);
                dryOutL[n] = daL + w * (dbL - daL);
                dryOutR[n] = daR + w * (dbR - daR);
            }

            if (fadePos >= fadeLen)
            {
                active = 1 - active; // the new-cutoff bank takes over
                fading = false;      // a further move retriggers next block
            }
        }
        else
        {
            runWet (bank[active], left[n], right[n], left[n], right[n]);
            if (align)
                runDry (bank[active], dryInL[n], dryInR[n], dryOutL[n], dryOutR[n]);
        }
    }
}

} // namespace anamorph
