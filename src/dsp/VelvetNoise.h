#pragma once

#include <vector>
#include <array>
#include <random>

namespace anamorph
{

// ============================================================================
//  VelvetNoise decorrelation
//
//  Turns mono into stereo (and widens stereo) by synthesising a DECORRELATED
//  SIDE signal from the Mid via a sparse "velvet noise" FIR. The Mid is left
//  untouched, so L + R = 2*Mid always holds -> mono compatible.
//
//      S' = S + amount * (velvetFIR * M)
//      L  = M + S' ,  R = M - S'
//
//  Click-free design (spec feedback #1): the random tap set is generated ONCE
//  in prepare(); both `density` (how many taps are active) and `amount` (wet
//  level) are continuous and smoothed, so dragging a knob fades taps in/out
//  smoothly instead of regenerating a brand-new random sequence every block
//  (the old behaviour, which produced crackle). amount 0 == identity.
// ============================================================================
class VelvetNoise
{
public:
    void prepare (double sampleRate, unsigned seed = 0x1234abcdU);
    void reset();

    void setDensity (float d) noexcept { targetDensity = d; }
    void setAmount  (float a) noexcept { targetAmount  = a; } // 0 = identity

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    void updateWeights() noexcept;

    static constexpr int maxTaps = 64;

    double sr = 44100.0;

    std::vector<float> midHist;      // circular history of Mid
    int    histMask = 0;
    int    writePos = 0;

    std::array<int,   maxTaps> pos {};   // tap delay (samples), fixed
    std::array<float, maxTaps> sign {};  // tap sign +/-1, fixed
    std::array<float, maxTaps> weight {};// continuous active weight per tap
    int    activeTaps = 0;
    float  norm = 1.0f;

    float  targetDensity = 0.5f, currentDensity = 0.5f;
    float  targetAmount  = 0.0f, currentAmount  = 0.0f;

    // Input-presence gate: fades the decorrelation tail into silence so pausing
    // playback doesn't leave a short noise burst (feedback #17).
    float  env = 0.0f, envAtk = 0.0f, envRel = 0.0f;
};

} // namespace anamorph
