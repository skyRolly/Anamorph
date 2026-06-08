#pragma once

#include <atomic>
#include <cmath>
#include <algorithm>

namespace anamorph
{

// ============================================================================
//  LevelMeters
//
//  Input/output L/R metering for the level meter. Per channel it tracks:
//    * a held PEAK number (max sample peak since the last reset, never falls
//      until you click it or playback restarts -- #15);
//    * a rate-limited, hold-then-fall RMS number (#20 / #21);
//    * two RMS bar envelopes -- a fast-rise/slow-fall dim one (#18) and a
//      faster-falling bright one (#19);
//    * a peak-hold bar tick (hold then fall -- #24);
//    * clip latches that flag when peak / rms crossed 0 dBFS (#14).
//  Everything is published via atomics for the GUI. Audio-thread safe.
// ============================================================================
class StereoLevel
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        auto envC = [sampleRate] (double tau) { return 1.0f - std::exp (-1.0f / (float) (tau * sampleRate)); };
        // DIM = a fast PEAK envelope (instant attack, ~300 ms release): it tracks
        // transients up to the held peak line and "pushes it up" (#13/#12). Release
        // eased further (190 -> 300 ms) so it no longer drops away too quickly (#15).
        dimRel   = std::exp (-1.0f / (float) (0.300 * sampleRate));
        // BRIGHT = the RMS body: a quick rise to catch the signal (unchanged, it
        // feels right), and a clearly QUICKER fall so it doesn't crawl down after a
        // pause (#12). Still smooth enough that a steady signal sits stable.
        briRise  = envC (0.160);  briFall  = envC (0.260);
        // Numeric RMS source: a steady ~150 ms RMS; the display logic adds the
        // fast-rise / long-hold / slow-fall behaviour (#15).
        numRise  = envC (0.150);  numFall  = envC (0.150);
        reset();
    }

    void reset() noexcept
    {
        pkDimL = pkDimR = msBriL = msBriR = msNumL = msNumR = 0.0f;
        barPeakL = barPeakR = 0.0f; holdL = holdR = 0.0;
        peakHoldL = peakHoldR = 0.0f;
        rmsNumL = rmsNumR = -100.0f; rmsHoldL = rmsHoldR = 0.0;
        rmsClipL = rmsClipR = false;
        publishAll();
    }

    // Reset the HELD readouts (numeric peak, clip colours, hold timers) on a
    // number click or a playback restart. Thread-safe: just flags the request,
    // which the audio thread consumes (#14/#15/#16).
    void resetHold() noexcept { resetReq.store (1, std::memory_order_relaxed); }

    void process (const float* L, const float* R, int n) noexcept
    {
        if (resetReq.exchange (0, std::memory_order_relaxed) != 0)
        {
            peakHoldL = peakHoldR = 0.0f;
            rmsClipL = rmsClipR = false;
            rmsHoldL = rmsHoldR = 0.0;
        }

        float bpL = 0.0f, bpR = 0.0f, phL = peakHoldL, phR = peakHoldR;
        for (int i = 0; i < n; ++i)
        {
            const float al = std::abs (L[i]), ar = std::abs (R[i]);
            const float l2 = L[i] * L[i], r2 = R[i] * R[i];

            pkDimL = al > pkDimL ? al : pkDimL * dimRel; // instant attack, slow release
            pkDimR = ar > pkDimR ? ar : pkDimR * dimRel;
            msBriL += (l2 > msBriL ? briRise : briFall) * (l2 - msBriL);
            msBriR += (r2 > msBriR ? briRise : briFall) * (r2 - msBriR);
            msNumL += (l2 > msNumL ? numRise : numFall) * (l2 - msNumL);
            msNumR += (r2 > msNumR ? numRise : numFall) * (r2 - msNumR);

            bpL = al > bpL ? al : bpL;
            bpR = ar > bpR ? ar : bpR;
            phL = al > phL ? al : phL;   // held max sample peak (#15)
            phR = ar > phR ? ar : phR;
        }
        peakHoldL = phL; peakHoldR = phR;
        blockPeakL = bpL; blockPeakR = bpR;
        blockDur = (double) n / sr;
    }

    void publish() noexcept
    {
        // ---- peak-hold bar tick: hold, then fall (#24) ----
        auto stepBar = [&] (float& bar, double& hold, float blockPeak)
        {
            if (blockPeak >= bar) { bar = blockPeak; hold = kBarHold; }
            else if ((hold -= blockDur) <= 0.0) bar *= barFall();
        };
        stepBar (barPeakL, holdL, blockPeakL);
        stepBar (barPeakR, holdR, blockPeakR);

        // ---- numeric RMS: rate-limited, hold-then-fall (#20 / #21) ----
        stepRmsNumber (rmsNumL, rmsHoldL, db (std::sqrt (msNumL)), rmsClipL);
        stepRmsNumber (rmsNumR, rmsHoldR, db (std::sqrt (msNumR)), rmsClipR);

        publishAll();
    }

    // ---- getters (linear bar heights as dBFS where noted) ----
    float getDimL()  const noexcept { return load (dimLdb); }
    float getDimR()  const noexcept { return load (dimRdb); }
    float getBriL()  const noexcept { return load (briLdb); }
    float getBriR()  const noexcept { return load (briRdb); }
    float getBarL()  const noexcept { return load (barLdb); }
    float getBarR()  const noexcept { return load (barRdb); }
    float getPeakHoldL() const noexcept { return load (peakHoldLdb); }
    float getPeakHoldR() const noexcept { return load (peakHoldRdb); }
    float getRmsNumL()   const noexcept { return load (rmsNumLdb); }
    float getRmsNumR()   const noexcept { return load (rmsNumRdb); }
    bool  getPeakClipL() const noexcept { return clipL.load (std::memory_order_relaxed) != 0; }
    bool  getPeakClipR() const noexcept { return clipR.load (std::memory_order_relaxed) != 0; }
    bool  getRmsClipL()  const noexcept { return rclipL.load (std::memory_order_relaxed) != 0; }
    bool  getRmsClipR()  const noexcept { return rclipR.load (std::memory_order_relaxed) != 0; }

private:
    float barFall() const noexcept { return std::exp (-(float) blockDur / 0.10f); } // fast fall after hold
    static float db (float lin) noexcept { return lin > 1.0e-6f ? 20.0f * std::log10 (lin) : -100.0f; }
    static void  store (std::atomic<float>& a, float v) noexcept { a.store (v, std::memory_order_relaxed); }
    static void  store (std::atomic<int>& a, int v) noexcept { a.store (v, std::memory_order_relaxed); }
    static float load (const std::atomic<float>& a) noexcept { return a.load (std::memory_order_relaxed); }

    void stepRmsNumber (float& num, double& hold, float targetDb, bool& clip) noexcept
    {
        // Snap UP quickly, then hold and fall SLOWLY; a small dead-band keeps the
        // reading rock-steady once settled instead of jittering (#15).
        if (targetDb > num + 0.3f)
        {
            num = targetDb;       // fast rise straight to the target
            hold = kRmsHold;
        }
        else if (targetDb < num - 0.6f)
        {
            if ((hold -= blockDur) <= 0.0)
                num = std::max (targetDb, num - (float) (kRmsFall * blockDur)); // slow fall after the hold
        }
        else
        {
            hold = kRmsHold;      // within the dead-band: steady, re-arm the hold
        }
        if (num > 0.0f) clip = true; // latch orange once RMS crosses 0 dBFS (#14)
    }

    void publishAll() noexcept
    {
        store (dimLdb, db (pkDimL)); store (dimRdb, db (pkDimR));
        store (briLdb, db (std::sqrt (msBriL))); store (briRdb, db (std::sqrt (msBriR)));
        store (barLdb, db (barPeakL)); store (barRdb, db (barPeakR));
        store (peakHoldLdb, db (peakHoldL)); store (peakHoldRdb, db (peakHoldR));
        store (rmsNumLdb, rmsNumL); store (rmsNumRdb, rmsNumR);
        store (clipL, db (peakHoldL) > 0.0f ? 1 : 0); store (clipR, db (peakHoldR) > 0.0f ? 1 : 0);
        store (rclipL, rmsClipL ? 1 : 0); store (rclipR, rmsClipR ? 1 : 0);
    }

    static constexpr double kBarHold = 1.0;  // s, peak tick hold (Insight 2: 1 s, #9/#24)
    static constexpr double kRmsHold = 1.2;  // s, RMS number hold before falling (#15)
    static constexpr double kRmsFall = 8.0;  // dB/s slow RMS-number fall (#15)

    double sr = 48000.0, blockDur = 0.01;
    float dimRel = 0, briRise = 0, briFall = 0, numRise = 0, numFall = 0;

    float pkDimL = 0, pkDimR = 0, msBriL = 0, msBriR = 0, msNumL = 0, msNumR = 0;
    float barPeakL = 0, barPeakR = 0; double holdL = 0, holdR = 0;
    float blockPeakL = 0, blockPeakR = 0;
    float peakHoldL = 0, peakHoldR = 0;
    float rmsNumL = -100, rmsNumR = -100; double rmsHoldL = 0, rmsHoldR = 0;
    bool  rmsClipL = false, rmsClipR = false;

    std::atomic<float> dimLdb { -100 }, dimRdb { -100 }, briLdb { -100 }, briRdb { -100 };
    std::atomic<float> barLdb { -100 }, barRdb { -100 }, peakHoldLdb { -100 }, peakHoldRdb { -100 };
    std::atomic<float> rmsNumLdb { -100 }, rmsNumRdb { -100 };
    std::atomic<int>   clipL { 0 }, clipR { 0 }, rclipL { 0 }, rclipR { 0 };
    std::atomic<int>   resetReq { 0 };
};

struct LevelMeters
{
    StereoLevel input, output;
    void prepare (double sr) { input.prepare (sr); output.prepare (sr); }
    void reset()             { input.reset();      output.reset(); }
    void publish()           { input.publish();    output.publish(); }
    void resetHold()         { input.resetHold();  output.resetHold(); } // click / replay (#16)
};

} // namespace anamorph
