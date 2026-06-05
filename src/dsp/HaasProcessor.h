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

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    inline float readDelayed (std::vector<float>& line, int& widx, float delaySamps) noexcept;

    double sr = 44100.0;
    std::vector<float> bufL, bufR;
    int   writeL = 0, writeR = 0;
    int   bufMask = 0;
    float targetSamples = 0.0f;
    float currentSamples = 0.0f; // smoothed to avoid zipper/clicks
    bool  delayRight = true;
};

} // namespace anamorph
