#pragma once

#include <atomic>
#include <cstdint>

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
//  Two cooperating parts (0.8.1 rework):
//    * MEASURE -- the real K-weighted LUFS difference. This is the GROUND TRUTH
//      and the only thing that governs while there is valid audio. When the input
//      decays to silence the measurement WAITS (holds the last trusted value); it
//      never drifts toward 0 or wanders on its own (no state drift).
//    * PREDICT -- an ABSOLUTE feed-forward estimate of the loudness the wet adds,
//      computed as a pure function of Drive and Mix (the two controls that move the
//      internal gain a lot). The moment that estimate RISES (Drive/Mix cranked) the
//      published gain is pre-ducked so the first sound out -- even straight out of a
//      pause -- can't slam. Because it is absolute, not an accumulator, reversing the
//      controls reverses the predict exactly: it can never ratchet or path-depend.
//
//  The predict only ever LOWERS the gain to forestall a slam; it never raises it
//  (that is left to the measurement, so removing Drive eases back smoothly instead of
//  surging). It therefore can't pollute the measurement -- on play, MEASURE converges
//  fast and is the final authority.
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

    // Re-arm the measurement (clear filter state + integrators) WITHOUT zeroing
    // the published gain, so a re-arm (a swap that changes the signal path, an A/B injection
    // whose slots differ in what the measurement reads, a host reset, or a same-rate re-prepare
    // that keeps its result -- ADR-0007) re-converges smoothly from the current value instead of
    // snapping (feedback #16).
    void softReset() noexcept;

    // Feeds both signals; updates the published match gain. Audio-thread safe.
    void process (const float* dryL, const float* dryR,
                  const float* wetL, const float* wetR, int numSamples) noexcept;

    float getMatchGainDb() const noexcept { return matchGainDb.load (std::memory_order_relaxed); }

    // Restore a remembered match value (per A/B slot) so a switch doesn't have to
    // re-converge from scratch and lurch in level (feedback #23). Restoring the value and
    // restoring its validity are two answers, and the caller gives both: `measured` says whether
    // the value is the measure's confirmed answer for the inputs the matcher now reads (ADR-0007,
    // Amendment of 2026-09-25, A/B provenance). One that is not -- a slot left before the measure
    // had confirmed it, or restored into a state that reads different inputs -- is a value from
    // another time, so it starts not current, exactly as a change reported by inputsChanged()
    // does, and the measure confirms it from there.
    void setDisplayedGainDb (float db, bool measured) noexcept
    {
        displayedGainDb = (double) db;
        matchGainDb.store (db, std::memory_order_relaxed);
        if (measured) { resultStale = false; resultMeasured = true; }
        else          inputsChanged();
    }

    // IS THE PUBLISHED RESULT A MEASUREMENT OF THE CURRENT INPUTS? (ADR-0007, Amendment of
    // 2026-09-25.) The engine calls inputsChanged() whenever it adopts a change to anything this
    // measurement reads -- a live edit, a duck bottom, a forced swap. From then on the published
    // value, and the analysis behind it, describe the PREVIOUS state: the integrators still hold
    // its energy (tau 0.4 s) and the published value glides after them. process() separates, in
    // what the published value glides toward, the measurement of the audio heard since the change
    // alone (the linear integrators give it exactly: their energy minus the pre-change energy,
    // decayed) from everything older. The result is current again once those post-change
    // measurements make up at least half of the published value and it is within kCurrentDb of
    // their mean -- everything older moves it by no more than that. An A/B slot's restored value is
    // current when its caller says it is measured (setDisplayedGainDb); a flush (reset) is no
    // measurement of another state, so it clears the question. prepare() keeps a result only while
    // it is current.
    void inputsChanged() noexcept
    {
        resultStale = true;
        resultMeasured = false;
        staleDry0 = meanSqDry;
        staleWet0 = meanSqWet;
        stalePreWeight = 1.0;
        postShare = postSum = 0.0;
    }
    bool isResultCurrent() const noexcept { return ! resultStale; }

    // IS THE PUBLISHED RESULT THE MEASURE'S OWN ANSWER? (ADR-0007, Amendment of 2026-09-25, A/B
    // provenance.) Stricter than current, which it implies: current says the value describes no
    // PREVIOUS state; measured says the measure has confirmed it for the inputs it now reads, by
    // the same criterion that makes a changed result current again. A flush is current but not
    // measured -- 0 dB, then the predict floor, are no measurement -- and so is a value the predict
    // floor lowers; either becomes measured when the measure confirms it. What an A/B slot
    // records when it is left: a value that is current only because a flush left it so is right
    // in the flush's own context (the floor lands the first block), not in another one.
    bool isResultMeasured() const noexcept { return resultMeasured; }

    // Tell the matcher the current state of the two big-gain controls. estBoostDb()
    // turns these into an ABSOLUTE predicted boost (no internal accumulation), so the
    // pre-duck reacts the instant Drive OR Mix is raised -- paused or playing -- and
    // can never ratchet (feedback #14/#19, and the Mix-coupling case).
    void setDriveDb (float db) noexcept { currentDriveDb = (double) db; }
    void setMix     (float m)  noexcept { currentMix     = (double) m; }

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

    // Predict (feed-forward) state. ABSOLUTE -- a pure function of Drive + Mix, never
    // an accumulator -> no ratcheting, no history path dependence.
    double currentDriveDb     = 0.0;
    double currentMix         = 1.0;
    double prevPredictedGainDb = 0.0; // last block's absolute predict (to spot a rise)

    // Wave-5 per-block memos -- bit-exact caches of pure functions:
    //  * estBoostDb keyed on the BITWISE (Drive, Mix) pair (stateless; the
    //    pair only moves while a knob drags);
    //  * the MEASURE smoothing coefficients keyed on the block size (their
    //    only other input, sampleRate, re-keys via prepare()).
    std::uint64_t memoDriveBits = 0, memoMixBits = 0;
    double memoBoostDb   = 0.0;
    bool   memoBoostValid = false;
    int    coeffForN = -1;
    double coeffFast = 0.0, coeffSlow = 0.0;

    // The result's currency (inputsChanged()). staleDry0 / staleWet0: the integrators at the last
    // change; stalePreWeight: the share of that pre-change energy still in them, (1 - smoothCoeff)^N
    // after N samples -- the integrators' own decay, block by block (memo keyed on the block size).
    // postShare / postSum: the post-change measurements' weight in the published value's glide, and
    // their weighted sum (postSum / postShare is their glide-weighted mean). resultMeasured
    // (isResultMeasured) implies ! resultStale; the bookkeeping runs until it is set.
    bool   resultStale = false;
    bool   resultMeasured = false;
    double staleDry0 = 0.0, staleWet0 = 0.0, stalePreWeight = 1.0;
    double postShare = 0.0, postSum = 0.0;
    int    decayForN = -1;
    double decayPerBlock = 1.0;

    std::atomic<float> matchGainDb { 0.0f };
};

} // namespace anamorph
