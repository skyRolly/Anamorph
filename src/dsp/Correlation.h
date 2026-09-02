#pragma once

#include <atomic>
#include <cmath>

namespace anamorph
{

// ============================================================================
//  CorrelationMeter
//
//  Running stereo phase-correlation estimator using single-pole smoothing of
//  the cross- and auto-products. Result is the Pearson correlation in [-1, +1]:
//     +1  perfectly correlated (mono),  0 = decorrelated,  -1 = anti-phase.
//
//  Two independent time constants are kept so the UI can show a snappy meter
//  and a slower averaged meter (bottom + right, per the spec). Audio-thread
//  safe: no allocation, results published via atomics for the GUI.
// ============================================================================
class CorrelationMeter
{
public:
    void prepare (double sampleRate, double fastMs = 120.0, double slowMs = 600.0) noexcept
    {
        fastCoeff = coeffFor (sampleRate, fastMs);
        slowCoeff = coeffFor (sampleRate, slowMs);
        reset();
    }

    void reset() noexcept
    {
        lrFast = llFast = rrFast = 0.0f;
        lrSlow = llSlow = rrSlow = 0.0f;
    }

    inline void process (float l, float r) noexcept
    {
        const float lr = l * r, ll = l * l, rr = r * r;

        lrFast += fastCoeff * (lr - lrFast);
        llFast += fastCoeff * (ll - llFast);
        rrFast += fastCoeff * (rr - rrFast);

        lrSlow += slowCoeff * (lr - lrSlow);
        llSlow += slowCoeff * (ll - llSlow);
        rrSlow += slowCoeff * (rr - rrSlow);
    }

    // Publish current values for the GUI (call once per block, audio thread).
    void publish() noexcept
    {
        // Self-heal (ADR-0009, decision bullet 3 -- the rule LevelMeters::sanitize
        // already applies): one non-finite sample that reaches this tap drives an
        // accumulator to Inf, and the next finite block turns it NaN (Inf - Inf),
        // after which the one-poles never recover and the meter publishes NaN
        // until re-prepare. The engine's self-heal runs BEFORE the bypass
        // crossfade, which can re-introduce raw non-finite input this tap then
        // integrates -- so the guard must live here. Flush any non-finite
        // accumulator back to 0, the meter's documented idle value (#1).
        sanitize (lrFast); sanitize (llFast); sanitize (rrFast);
        sanitize (lrSlow); sanitize (llSlow); sanitize (rrSlow);

        fast.store (correlation (lrFast, llFast, rrFast), std::memory_order_relaxed);
        slow.store (correlation (lrSlow, llSlow, rrSlow), std::memory_order_relaxed);

        // L/R energy balance in [-1 (L) .. +1 (R)] for the bottom meter (#9).
        //
        // OVERFLOW GUARD (ER-DSP-11), and a DIFFERENT operation from ER-DSP-10's:
        // that one is the phase meter's `ll * rr` PRODUCT inside correlation();
        // this one is the balance's `ll + rr` SUM, and fixing the product did
        // nothing for the sum. Both accumulators are finite here (sanitize has
        // just run) and non-negative by construction, but a float ADD of two
        // mean-square values still leaves float once their sum passes FLT_MAX --
        // for equal channels from steady input above 1.30438174e19, and for
        // unequal ones anywhere the pair sums past it, up to the 1.84467435e19
        // where the per-sample square itself would stop being finite.
        //
        // WHAT IT COST, and why "finite" was never the test. The NUMERATOR does
        // not overflow -- `rr - ll` lies in [-ll, rr] for non-negative operands,
        // so it stays finite and carries the whole imbalance -- and `+Inf` sails
        // past the 1e-12 small-signal guard below. So the division was
        // finite/+Inf, which is a perfectly well-formed 0: the meter reported
        // PERFECTLY CENTRED for a badly lopsided pair. MEASURED at
        // l = 1.8e19, r = 1.0e19: llSlow 3.23707947e38, rrSlow 9.98539635e37,
        // numerator -2.23853994e38 (finite), sum +Inf (exactly 4.2356191e38 in
        // double, against FLT_MAX 3.40282347e38), published balance -0.0 where
        // the true figure is -0.5285036. A pair whose sum stays under FLT_MAX is
        // unaffected and always was: at l = 1.8e19, r = 0.2e19 the sum is
        // 3.277e38 and the meter already read -0.9756 correctly, which is what
        // shows the defect is the OVERFLOW and not the level.
        //
        // The recovery is the sum, in double, on that overflow alone: two finite
        // floats sum to at most ~6.8e38, nowhere near DBL_MAX, and the quotient
        // is then bounded by 1 in magnitude because |rr - ll| <= rr + ll. The
        // float expression below is untouched character for character and the
        // double path is unreachable while the sum is finite, so every ordinary
        // reading is bit-for-bit what it always was.
        const float sum = llSlow + rrSlow;
        float bal;
        if (! std::isfinite (sum))
        {
            const double d = (double) llSlow + (double) rrSlow;   // >= FLT_MAX here, never 0
            bal = (float) ((double) (rrSlow - llSlow) / d);
        }
        else
            bal = sum > 1.0e-12f ? (rrSlow - llSlow) / sum : 0.0f;
        bal = bal < -1.0f ? -1.0f : (bal > 1.0f ? 1.0f : bal);
        balance.store (bal, std::memory_order_relaxed);

        // Fast mean-square energy so the GUI can tell "playing" from "silent" and
        // glide the pointers back to centre when the input stops (#1/#2).
        energy.store (llFast + rrFast, std::memory_order_relaxed);
    }

    float getFast() const noexcept    { return fast.load (std::memory_order_relaxed); }
    float getSlow() const noexcept    { return slow.load (std::memory_order_relaxed); }
    float getBalance() const noexcept { return balance.load (std::memory_order_relaxed); }
    float getEnergy() const noexcept  { return energy.load (std::memory_order_relaxed); }

private:
    static float coeffFor (double sr, double ms) noexcept
    {
        const double tau = ms * 0.001;
        return (float) (1.0 - std::exp (-1.0 / (tau * sr)));
    }

    static void sanitize (float& v) noexcept { if (! std::isfinite (v)) v = 0.0f; } // recover a NaN-latched accumulator

    static float correlation (float lr, float ll, float rr) noexcept
    {
        // OVERFLOW GUARD (ER-DSP-10). Everything arriving here is FINITE --
        // publish() has just sanitized the six accumulators -- and `ll`/`rr` are
        // non-negative by construction (one-poles of squares, started at 0). None
        // of that stops their PRODUCT from overflowing: `ll * rr` is a float
        // multiply, so two mean-square values above ~1.844e19 (sqrt(FLT_MAX))
        // give +Inf. That is reachable from finite input samples above
        // ~4.295e9 -- the engine's NaN/Inf self-heal is explicitly NOT a level
        // limiter, and under Bypass this tap sees the host's raw buffer -- and
        // the damage is silent rather than loud: sqrt(+Inf) is +Inf, +Inf is not
        // < 1e-12 so the small-signal guard below does not fire, and `lr / +Inf`
        // is 0. MEASURED at l = r = 1e10: all three accumulators land on
        // 9.997e19, every one of them finite; `ll * rr` is +Inf; the meter
        // publishes 0.0 -- "fully decorrelated" -- for a PERFECTLY CORRELATED
        // mono signal whose true correlation is +1 (anti-phase publishes -0.0
        // instead of -1). The same arithmetic in double gives 1.
        //
        // WHY THE BRANCH, AND WHY DOUBLE ONLY INSIDE IT. `ll * rr` is the exact
        // operation that overflows, and it is the only one: `lr` is finite, and
        // once the denominator is right the division and the clamp are
        // exact-range. Double makes the recovery UNCONDITIONALLY safe with no
        // case analysis: the product of two finite floats is at most ~1.16e77,
        // and it carries NO ROUNDING AT ALL -- a float significand is 24 bits,
        // so a double holds their 48-bit product exactly -- which is why this
        // needs no argument about how any intermediate rounds. The obvious
        // float-only alternative, re-associating as sqrt(ll)*sqrt(rr), is in
        // fact also safe here (swept: every representable pair in float's top
        // binade against FLT_MAX and against itself, 2 x 8388608 pairs, none
        // overflowing, largest exact product 3.402823264e38 against FLT_MAX
        // 3.402823466e38) -- but that safety rests on which way a correctly
        // rounded sqrt happens to land near the top of the range, which is a
        // worse thing to depend on than an exact product, and it would cost a
        // second sqrt. Confining the double to the taken-only-on-overflow branch
        // is what keeps the ordinary range BIT-FOR-BIT unchanged: the expression
        // below is the original, character for character, and no normal-range
        // value reaches the double path. Verified differentially against the
        // pre-fix expression over 19,995,466 randomised finite-product triples
        // spanning ll/rr from 1e-40 to 1e19: ZERO differing bit patterns.
        if (! std::isfinite (ll * rr))
        {
            const double d = std::sqrt ((double) ll * (double) rr);
            const double c = (double) lr / d;   // d >= 1.844e19 here, so |c| is small
            return c < -1.0 ? -1.0f : (c > 1.0 ? 1.0f : (float) c);
        }

        const float denom = std::sqrt (ll * rr);
        if (denom < 1.0e-12f) return 0.0f;
        float c = lr / denom;
        return c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
    }

    float fastCoeff = 0.01f, slowCoeff = 0.002f;
    float lrFast = 0, llFast = 0, rrFast = 0;
    float lrSlow = 0, llSlow = 0, rrSlow = 0;

    // Correlation has no meaning without signal: idle / silent reads as 0
    // (decorrelated), not +1, so the meter sits centred at rest (#1).
    std::atomic<float> fast { 0.0f };
    std::atomic<float> slow { 0.0f };
    std::atomic<float> balance { 0.0f };
    std::atomic<float> energy { 0.0f };
};

} // namespace anamorph
