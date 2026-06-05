#pragma once

#include <vector>
#include <random>

namespace anamorph
{

// ============================================================================
//  VelvetNoise decorrelation
//
//  Turns mono into stereo (and widens stereo) by synthesising a DECORRELATED
//  SIDE signal from the Mid via a sparse "velvet noise" FIR (random +/-1 taps
//  at sparse, pseudo-random positions). The Mid is left untouched and only the
//  Side gains the diffuse, decorrelated energy:
//
//      S' = S + amount * (velvetFIR * M)
//      L  = M + S' ,  R = M - S'
//
//  Because L + R = 2M regardless of the side content, mono compatibility is
//  guaranteed (no comb filtering when summed). The FIR is linear => stays
//  OUTSIDE oversampling. Taps are precomputed in prepare() (no RT allocation).
// ============================================================================
class VelvetNoise
{
public:
    void prepare (double sampleRate, unsigned seed = 0x1234abcdU);
    void reset();

    void setDensity (float d) noexcept { if (d != density) { density = d; rebuild = true; } }

    void processBlock (float* left, float* right, int numSamples) noexcept;

private:
    void buildTaps();

    struct Tap { int delay; float sign; };

    double sr = 44100.0;
    float  density = 0.5f;
    bool   rebuild = true;

    std::vector<float> midHist;      // circular history of Mid
    int    histMask = 0;
    int    writePos = 0;

    std::vector<Tap> taps;
    float  norm = 1.0f;
    std::mt19937 rng;
};

} // namespace anamorph
