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
    // ~12 ms cutoff crossfade for LARGE jumps only: the house click-free fade
    // length (SoloMonitor / Multiband Enable use the same).
    fadeLen = juce::jmax (1, (int) std::lround (0.012 * sampleRate));
    // One-pole cutoff glide, tau ~15 ms: settles ~75 ms after the last target
    // move (bounded time -- no rate-capped catch-up tail), smooth trajectory
    // (no modulation sidebands), true LR4 at every instant (flat magnitude).
    glideK = 1.0f - std::exp (-1.0f / (0.015f * (float) sr));
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
    active  = 0;
    fading  = false;
    fadePos = 0;
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
    // cutoffs toward them with a bounded-time one-pole (or a single bank
    // crossfade for a multi-octave jump -- 0.8.10, see MultibandWidth.h).
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

    // LARGE-JUMP bank crossfade (0.8.10): only a delta beyond kFadeThresholdOct
    // (an automation step / click-jump -- a mouse drag at UI cadence never gets
    // near it) hands the LATEST cutoffs to the idle bank -- which adopts the
    // active bank's ladder state, so it starts transient-free -- and fades the
    // output over to it (the endpoint phase difference wraps mod 2pi, where a
    // glide would chirp through every intermediate turn). Anything smaller
    // glides per sample below. Checked at block rate: targets only change
    // between blocks (setCrossovers).
    if (! fading)
    {
        bool jump = false;
        for (int i = 0; i < crossovers && ! jump; ++i)
            jump = std::abs (std::log2 (bank[active].f[i] / targetF[i])) > kFadeThresholdOct;
        if (jump)
        {
            auto& to = bank[1 - active];
            copyBankState (to, bank[active]);
            setBankCutoffs (to);
            fading  = true;
            fadePos = 0;
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
        // ONE-POLE cutoff glide (0.8.10): each active split eases toward its
        // target per sample (tau ~15 ms, bounded settle -- no rate-capped
        // catch-up tail), keeping the reconstruction a true flat-magnitude LR4
        // allpass at every instant. Paused while a large-jump fade is in flight
        // (the fade's banks hold fixed coefficients); it resumes on the merged
        // residue the block after the fade lands.
        if (! fading)
        {
            auto& bk = bank[active];
            for (int i = 0; i < crossovers; ++i)
            {
                if (std::abs (bk.f[i] - targetF[i]) > 0.05f)
                {
                    bk.f[i] += (targetF[i] - bk.f[i]) * glideK;
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
