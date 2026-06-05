#pragma once

#include <atomic>
#include <cmath>
#include <algorithm>

namespace anamorph
{

// ============================================================================
//  LevelMeters
//
//  Input and output L/R metering for the (default-hidden) level meter (#10):
//  per channel a decaying PEAK and a ~300 ms RMS, published in dBFS via atomics
//  for the GUI. Audio-thread safe (no allocation/locks).
// ============================================================================
class StereoLevel
{
public:
    void prepare (double sampleRate) noexcept
    {
        peakDecay = std::exp (-1.0 / (0.35 * sampleRate));        // ~350 ms peak fall
        rmsCoeff  = 1.0f - std::exp (-1.0f / (float) (0.30 * sampleRate)); // ~300 ms RMS
        reset();
    }

    void reset() noexcept
    {
        peakL = peakR = 0.0f; msL = msR = 0.0f;
        store (peakLdb, -100.0f); store (peakRdb, -100.0f);
        store (rmsLdb, -100.0f);  store (rmsRdb, -100.0f);
    }

    void process (const float* L, const float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float al = std::abs (L[i]), ar = std::abs (R[i]);
            peakL = al > peakL ? al : peakL * peakDecayF();
            peakR = ar > peakR ? ar : peakR * peakDecayF();
            msL += rmsCoeff * (L[i] * L[i] - msL);
            msR += rmsCoeff * (R[i] * R[i] - msR);
        }
    }

    void publish() noexcept
    {
        store (peakLdb, db (peakL)); store (peakRdb, db (peakR));
        store (rmsLdb, db (std::sqrt (msL))); store (rmsRdb, db (std::sqrt (msR)));
    }

    float getPeakL() const noexcept { return peakLdb.load (std::memory_order_relaxed); }
    float getPeakR() const noexcept { return peakRdb.load (std::memory_order_relaxed); }
    float getRmsL()  const noexcept { return rmsLdb.load (std::memory_order_relaxed); }
    float getRmsR()  const noexcept { return rmsRdb.load (std::memory_order_relaxed); }

private:
    float peakDecayF() const noexcept { return (float) peakDecay; }
    static float db (float lin) noexcept { return lin > 1.0e-6f ? 20.0f * std::log10 (lin) : -100.0f; }
    static void store (std::atomic<float>& a, float v) noexcept { a.store (v, std::memory_order_relaxed); }

    double peakDecay = 0.9999;
    float  rmsCoeff = 0.001f;
    float  peakL = 0, peakR = 0, msL = 0, msR = 0;
    std::atomic<float> peakLdb { -100.0f }, peakRdb { -100.0f }, rmsLdb { -100.0f }, rmsRdb { -100.0f };
};

struct LevelMeters
{
    StereoLevel input, output;
    void prepare (double sr) { input.prepare (sr); output.prepare (sr); }
    void reset()             { input.reset();      output.reset(); }
    void publish()           { input.publish();    output.publish(); }
};

} // namespace anamorph
