#pragma once

#include <atomic>

namespace anamorph
{

// ============================================================================
//  LoudnessMatch  (Auto Gain -- modeled on Soundtheory Kraftur's Match/Apply)
//
//  Perceptual loudness matching, NOT a peak/RMS comparison. Both the dry and
//  wet signals are K-weighted per ITU-R BS.1770 (a high-shelf "head" filter +
//  an RLB high-pass) and their mean-square loudness is integrated with a short
//  single-pole window. The match gain that brings the wet loudness up/down to
//  the dry loudness is published for the GUI:
//
//      matchGainDb = LUFS(dry) - LUFS(wet)
//
//  Workflow (see spec 6.2):
//    * Match : while engaged, the engine applies this gain (smoothed) so A/B
//              listening is level-matched in real time -- "louder != better".
//    * Apply : the GUI reads getMatchGainDb() and writes it into Output Gain as
//              a FIXED value, then disengages Match, so exports stay consistent.
//
//  This is deliberately NOT a continuously-adapting export-time AGC. The filters
//  are IIR (no bulk latency), keeping the matcher essentially latency-free.
// ============================================================================
class LoudnessMatch
{
public:
    void prepare (double sampleRate);
    void reset();

    // Feeds both signals; updates the published match gain. Audio-thread safe.
    void process (const float* dryL, const float* dryR,
                  const float* wetL, const float* wetR, int numSamples) noexcept;

    float getMatchGainDb() const noexcept { return matchGainDb.load (std::memory_order_relaxed); }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;
        inline double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() noexcept { z1 = z2 = 0; }
    };

    struct KWeighting
    {
        Biquad shelf, hp;
        void setSampleRate (double sr);
        inline double process (double x) noexcept { return hp.process (shelf.process (x)); }
        void reset() noexcept { shelf.reset(); hp.reset(); }
    };

    KWeighting kDryL, kDryR, kWetL, kWetR;
    double meanSqDry = 1.0e-9, meanSqWet = 1.0e-9;
    double smoothCoeff = 0.0; // loudness integration window
    double sampleRate = 48000.0;
    double displayedGainDb = 0.0; // adaptively-smoothed published value (#19)
    std::atomic<float> matchGainDb { 0.0f };
};

} // namespace anamorph
