#include "LoudnessMatch.h"
#include <cmath>
#include <algorithm>

namespace anamorph
{

static constexpr double kPi = 3.14159265358979323846;

static inline double clampd (double x, double lo, double hi) noexcept
{
    return x < lo ? lo : (x > hi ? hi : x);
}

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
    prevPredictedGainDb = 0.0; // default state = no boost
    matchGainDb.store (0.0f, std::memory_order_relaxed);
}

// Estimated K-weighted loudness boost (dB) the peak-preserving tanh Drive adds.
// The makeup gain 1/tanh(g) means small-signal content is lifted by ~g/tanh(g),
// so quiet/average material gets most of that boost; we anticipate a fraction of
// it (the rest is corrected by the real measurement on play). Feedback #19.
static double estDriveBoostDb (double driveDb) noexcept
{
    if (driveDb <= 0.0) return 0.0;
    const double g = std::pow (10.0, driveDb / 20.0);
    const double smallSig = 20.0 * std::log10 (g / std::tanh (g)); // small-signal makeup lift
    const double blend = std::min (1.0, driveDb / 2.0);            // Drive blend ramps in over 0..2 dB
    return std::min (14.0, 0.5 * blend * smallSig);
}

// ABSOLUTE predicted boost (dB) the wet adds over the dry, as a pure function of Drive
// and Mix. The dry/wet blend scales the Drive boost: at Mix=0 the wet is muted so the
// boost is 0; at Mix=1 it is the full Drive boost; in between it follows the linear
// blend. No internal state -> reversing Drive/Mix reverses this exactly (no ratchet).
static double estBoostDb (double driveDb, double mix) noexcept
{
    const double driveBoost = estDriveBoostDb (driveDb);
    if (driveBoost <= 0.0) return 0.0;
    const double m = clampd (mix, 0.0, 1.0);
    const double B = std::pow (10.0, driveBoost / 20.0);  // wet/dry linear loudness ratio
    const double ratio = (1.0 - m) + m * B;               // >= 1 (Drive only boosts)
    return 20.0 * std::log10 (ratio > 1.0e-6 ? ratio : 1.0e-6);
}

void LoudnessMatch::softReset() noexcept
{
    kDryL.reset(); kDryR.reset(); kWetL.reset(); kWetR.reset();
    meanSqDry = meanSqWet = 1.0e-9;
    // displayedGainDb / prevPredictedGainDb are intentionally preserved so the published
    // gain glides and a re-arm doesn't spuriously pre-duck.
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

    double target = clampd (dLufs - wLufs, -24.0, 24.0);

    // No valid measurement data while the input is silent: MEASURE waits (holds the
    // last trusted value) rather than deriving from near-zero energy. ~ -60 dBFS gate.
    const double kSilence = 1.0e-6;
    const bool silent = (meanSqDry < kSilence && meanSqWet < kSilence);
    const double blockDur = (double) numSamples / sampleRate;

    // ---- PREDICT: absolute, pure function of Drive + Mix (no accumulation) --------
    const double predictedGainDb = clampd (-estBoostDb (currentDriveDb, currentMix), -24.0, 0.0);
    const double predictDelta = predictedGainDb - prevPredictedGainDb;
    prevPredictedGainDb = predictedGainDb;
    // The instant the expected boost RISES (Drive/Mix cranked) pre-duck so neither the
    // first played block nor a live crank can slam. A FALLING estimate never jumps the
    // gain up here -- the measurement eases it back on play, so there is no surge. This
    // floor-only, absolute rule is what kills the old ratchet-to-(-24) behaviour.
    if (predictDelta < 0.0)
        displayedGainDb = std::min (displayedGainDb, predictedGainDb);

    // ---- MEASURE: ground truth while there is audio; frozen on silence ------------
    if (! silent)
    {
        const double diff = target - displayedGainDb;
        const double tau  = (std::abs (diff) > 2.0) ? 0.06 : 0.9; // fast vs. slow
        const double coeff = 1.0 - std::exp (-blockDur / tau);
        displayedGainDb += coeff * diff;
    }
    // silent -> hold displayedGainDb (no drift); the predict floor above still guards.

    displayedGainDb = clampd (displayedGainDb, -24.0, 24.0);
    matchGainDb.store ((float) displayedGainDb, std::memory_order_relaxed);
}

} // namespace anamorph
