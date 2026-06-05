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
        fast.store (correlation (lrFast, llFast, rrFast), std::memory_order_relaxed);
        slow.store (correlation (lrSlow, llSlow, rrSlow), std::memory_order_relaxed);
    }

    float getFast() const noexcept { return fast.load (std::memory_order_relaxed); }
    float getSlow() const noexcept { return slow.load (std::memory_order_relaxed); }

private:
    static float coeffFor (double sr, double ms) noexcept
    {
        const double tau = ms * 0.001;
        return (float) (1.0 - std::exp (-1.0 / (tau * sr)));
    }

    static float correlation (float lr, float ll, float rr) noexcept
    {
        const float denom = std::sqrt (ll * rr);
        if (denom < 1.0e-12f) return 0.0f;
        float c = lr / denom;
        return c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
    }

    float fastCoeff = 0.01f, slowCoeff = 0.002f;
    float lrFast = 0, llFast = 0, rrFast = 0;
    float lrSlow = 0, llSlow = 0, rrSlow = 0;

    std::atomic<float> fast { 1.0f };
    std::atomic<float> slow { 1.0f };
};

} // namespace anamorph
