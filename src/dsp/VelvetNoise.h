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

    // Host transport state, fed once per block by the wrapper. The pause burst
    // (#4) is the sparse FIR replaying its last ~45 ms of Mid history at program
    // level the instant the dry signal stops masking it -- and a LEVEL detector
    // fundamentally cannot catch that in time: the presence envelope needs
    // hundreds of ms to fall below its threshold, while making it faster
    // flutters on ordinary musical decays (tried, made it worse). The transport
    // edge is the one signal that distinguishes "paused" from "quiet passage",
    // so a play->stop transition triggers a fast tail fade instead.
    void setTransportPlaying (bool isPlaying) noexcept
    {
        if (transportPlaying && ! isPlaying)
            stopping = true; // pause: fade the ringing tail out, then flush
        transportPlaying = isPlaying;
    }

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

    // Presence-driven gate (feedback #10): `env` follows the input level; `gate`
    // ramps 0<->1 at FIXED times (not level-dependent), so on play it fades the
    // decorrelation in slowly enough to mask the sparse-FIR burst, and on pause
    // it fades the tail out. Decoupling the ramp from level is the fix.
    float  env = 0.0f, envAtk = 0.0f, envRel = 0.0f;
    float  gate = 0.0f, gateAtk = 0.0f, gateRel = 0.0f;

    // Transport-stop tail kill (#4): a ~4 ms smoothstep fade on the wet tap sum,
    // after which the history is flushed and the presence gate re-armed -- so
    // live input through a STOPPED transport still fades back in normally.
    bool   transportPlaying = false, stopping = false;
    float  stopGain = 1.0f, stopStep = 0.0f;
};

} // namespace anamorph
