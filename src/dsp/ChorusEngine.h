#pragma once

#include <vector>

namespace anamorph
{

// ============================================================================
//  ChorusEngine  (Chorus  +  Roland Dimension-D emulation)
//
//  A modulated delay line. Two voicings share the implementation:
//
//   * Chorus      : one modulated tap per channel; L/R LFOs run in anti-phase
//                   so a mono source becomes wide. Classic, lush, some motion.
//
//   * Dimension-D : the headline "no audible pitch wobble" mode. Each channel
//                   sums TWO taps whose delays are modulated in ANTI-PHASE.
//                   When one tap's delay rises (pitch falls) the other falls
//                   (pitch rises), so the Doppler pitch-shifts CANCEL to first
//                   order -> spaciousness and width with no seasick vibrato.
//                   L and R use offset LFO phases for the stereo image.
//
//  Modulation stage => MUST run inside oversampling. Buffers are sized for the
//  maximum oversampled rate so changing the OS factor never reallocates; the
//  per-sample increments are derived from the working rate set each block.
// ============================================================================
class ChorusEngine
{
public:
    enum class Voice { Chorus, DimensionD };

    void prepare (double maxWorkingRate);
    void reset();

    void setWorkingRate (double sr) noexcept { workingRate = sr; }
    void setVoice (Voice v) noexcept         { voice = v; }
    void setRate (float hz) noexcept          { rateHz = hz; }
    void setDepth (float d01) noexcept        { depth = d01; }
    void setAmount (float a01) noexcept       { amount = a01; }
    void setDimMode (int mode) noexcept;       // 1..4 classic voicings

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    inline float readFrac (const std::vector<float>& line, int writeIdx, float delaySamps) const noexcept;

    double maxRate    = 352800.0;
    double workingRate = 44100.0;
    Voice  voice      = Voice::Chorus;

    float  rateHz = 0.6f;
    float  depth  = 0.5f;
    float  amount = 0.5f;

    // Dimension-D mode voicing presets (base delay ms, depth ms, rate Hz).
    float  dimBaseMs = 12.0f, dimDepthMs = 1.6f, dimRateHz = 0.5f;

    std::vector<float> bufL, bufR;
    int    bufMask = 0;
    int    writeL = 0, writeR = 0;

    float  phase = 0.0f; // 0..1 LFO phase
};

} // namespace anamorph
