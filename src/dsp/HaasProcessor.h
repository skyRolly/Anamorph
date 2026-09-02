#pragma once

#include <vector>

namespace anamorph
{

// ============================================================================
//  HaasProcessor
//
//  Precedence (Haas) widening: a short inter-channel delay (1..35 ms) applied
//  to one side. One channel passes through undelayed so the wet onset stays
//  aligned with the dry signal (important for the dry/wet mix). Pure delay =>
//  linear => stays OUTSIDE oversampling.
// ============================================================================
class HaasProcessor
{
public:
    void prepare (double sampleRate, int /*maxBlock*/);
    void reset();

    void setDelayMs (float ms) noexcept { targetSamples = ms * 0.001f * (float) sr; }
    void setSide (bool right) noexcept  { delayRight = right; }
    void setAmount (float a) noexcept   { amount = a; } // 0 = identity (dry)

    // Land every smoothed value on its target with no glide. Called by the engine
    // at the END of prepare(), once the restored snapshot has been pushed in --
    // this module's own prepare() snaps too, but it necessarily runs BEFORE those
    // targets exist, so on a restored session it snapped to the previous values
    // (ER-DSP-09, round 20). NOT called from reset(): a duck-bottom reset keeps
    // its existing behaviour, and live edits keep smoothing normally.
    void snapToTargets() noexcept { currentSamples = targetSamples; currentAmount = amount; }

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    inline float readDelayed (std::vector<float>& line, int& widx, float delaySamps) noexcept;

    double sr = 44100.0;
    std::vector<float> bufL, bufR;
    int   writeL = 0, writeR = 0;
    int   bufMask = 0;
    float targetSamples = 0.0f;
    float currentSamples = 0.0f; // smoothed to avoid zipper/clicks
    float amount = 0.0f;         // wet blend; smoothed in processBlock
    float currentAmount = 0.0f;
    bool  delayRight = true;
};

} // namespace anamorph
