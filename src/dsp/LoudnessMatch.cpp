#include "LoudnessMatch.h"
#include <cmath>
#include <algorithm>

namespace anamorph
{

static constexpr double kPi = 3.14159265358979323846;

void LoudnessMatch::KWeighting::setSampleRate (double sr)
{
    // ----- Stage 1: high-shelf "head" filter (BS.1770) -----
    {
        const double f0 = 1681.974450955533;
        const double G  = 3.999843853973347;
        const double Q  = 0.7071752369554196;
        const double K  = std::tan (kPi * f0 / sr);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
        shelf.b1 = 2.0 * (K * K - Vh) / a0;
        shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelf.a1 = 2.0 * (K * K - 1.0) / a0;
        shelf.a2 = (1.0 - K / Q + K * K) / a0;
    }
    // ----- Stage 2: RLB high-pass (BS.1770) -----
    {
        const double f0 = 38.13547087602444;
        const double Q  = 0.5003270373238773;
        const double K  = std::tan (kPi * f0 / sr);
        const double a0 = 1.0 + K / Q + K * K;
        hp.b0 = 1.0;
        hp.b1 = -2.0;
        hp.b2 = 1.0;
        hp.a1 = 2.0 * (K * K - 1.0) / a0;
        hp.a2 = (1.0 - K / Q + K * K) / a0;
    }
    reset();
}

void LoudnessMatch::prepare (double sr)
{
    sampleRate = sr;
    kDryL.setSampleRate (sr);
    kDryR.setSampleRate (sr);
    kWetL.setSampleRate (sr);
    kWetR.setSampleRate (sr);

    // ~400 ms integration (momentary-loudness style) single-pole window.
    const double tau = 0.4;
    smoothCoeff = 1.0 - std::exp (-1.0 / (tau * sr));
    reset();
}

void LoudnessMatch::reset()
{
    kDryL.reset(); kDryR.reset(); kWetL.reset(); kWetR.reset();
    meanSqDry = meanSqWet = 1.0e-9;
    displayedGainDb = 0.0;
    matchGainDb.store (0.0f, std::memory_order_relaxed);
}

void LoudnessMatch::process (const float* dryL, const float* dryR,
                             const float* wetL, const float* wetR, int numSamples) noexcept
{
    for (int n = 0; n < numSamples; ++n)
    {
        const double dl = kDryL.process (dryL[n]);
        const double dr = kDryR.process (dryR[n]);
        const double wl = kWetL.process (wetL[n]);
        const double wr = kWetR.process (wetR[n]);

        const double dPow = dl * dl + dr * dr; // BS.1770 channel weights = 1.0 (L,R)
        const double wPow = wl * wl + wr * wr;

        meanSqDry += smoothCoeff * (dPow - meanSqDry);
        meanSqWet += smoothCoeff * (wPow - meanSqWet);
    }

    // Convert to LUFS and take the difference. Guard against silence.
    const double floorMs = 1.0e-7;
    const double dLufs = -0.691 + 10.0 * std::log10 (std::max (meanSqDry, floorMs));
    const double wLufs = -0.691 + 10.0 * std::log10 (std::max (meanSqWet, floorMs));

    double target = dLufs - wLufs;
    target = std::max (-24.0, std::min (24.0, target));

    // Adaptive smoothing of the PUBLISHED value (spec feedback #19):
    //  - small fluctuations are averaged over a long window -> a steady readout
    //    you can actually judge before pressing Apply;
    //  - a big change (e.g. cranking a knob that drops the level) snaps quickly
    //    so Match doesn't lag with the level audibly wrong for a second.
    const double blockDur = (double) numSamples / sampleRate;
    const double diff = target - displayedGainDb;
    const double tau  = (std::abs (diff) > 2.0) ? 0.06 : 0.9; // fast vs. slow
    const double coeff = 1.0 - std::exp (-blockDur / tau);
    displayedGainDb += coeff * diff;

    matchGainDb.store ((float) displayedGainDb, std::memory_order_relaxed);
}

} // namespace anamorph
