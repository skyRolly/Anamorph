#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace anamorph
{

// ============================================================================
//  LR4Xover  (flat-state Linkwitz-Riley 4th-order crossover -- Wave 2 / H6)
//
//  Drop-in replacement for the subset of juce::dsp::LinkwitzRileyFilter<float>
//  this project uses: exactly two channels, lowpass/highpass DUAL-output
//  processSample, reset, and a per-sample-glidable cutoff. The coefficient
//  derivation in update() and the TPT ladder in processSample() reproduce the
//  JUCE implementation expression-for-expression -- including which products
//  round in float and which sums run in double -- so the output is
//  BIT-IDENTICAL to the JUCE filter for every input and cutoff sequence.
//  The only change is the state storage: four flat per-channel floats instead
//  of four heap-allocated std::vectors (whose per-sample indexing was measured
//  at 4.5-7 % of every multiband/solo row in the Round-2 attribution).
//
//  If this ladder or update() is ever edited, it must be re-proven against
//  juce::dsp::LinkwitzRileyFilter on the full-engine twin dump -- the
//  bit-exactness is the contract that let it replace the JUCE filter without
//  an audible-change review.
// ============================================================================
class LR4Xover
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        update();
        reset();
    }

    void reset() noexcept
    {
        s1[0] = s1[1] = s2[0] = s2[1] = 0.0f;
        s3[0] = s3[1] = s4[0] = s4[1] = 0.0f;
    }

    void setCutoffFrequency (float newCutoffFrequencyHz) noexcept
    {
        cutoffFrequency = newCutoffFrequencyHz;
        update();
    }

    // Adopt another crossover's ladder state (coefficients untouched). Used by
    // the fixed-cutoff bank crossfade: the incoming bank continues the outgoing
    // bank's integrator history, so its first samples carry no charge-up
    // transient and the ~12 ms fade only has the coefficient difference to mask.
    void copyStateFrom (const LR4Xover& other) noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            s1[c] = other.s1[c];
            s2[c] = other.s2[c];
            s3[c] = other.s3[c];
            s4[c] = other.s4[c];
        }
    }

    // Low/high split, one channel (0 or 1). Identical arithmetic to
    // juce::dsp::LinkwitzRileyFilter<float>::processSample (in, low, high).
    void processSample (int channel, float inputValue, float& outputLow, float& outputHigh) noexcept
    {
        auto yH = (inputValue - (R2 + g) * s1[channel] - s2[channel]) * h;

        auto yB = g * yH + s1[channel];
        s1[channel] = g * yH + yB;

        auto yL = g * yB + s2[channel];
        s2[channel] = g * yB + yL;

        auto yH2 = (yL - (R2 + g) * s3[channel] - s4[channel]) * h;

        auto yB2 = g * yH2 + s3[channel];
        s3[channel] = g * yH2 + yB2;

        auto yL2 = g * yB2 + s4[channel];
        s4[channel] = g * yB2 + yL2;

        outputLow  = yL2;
        outputHigh = yL - R2 * yB + yH - yL2;
    }

private:
    void update() noexcept
    {
        // Same mixed-precision expressions as the JUCE filter: tan argument and
        // the h reciprocal in double, R2*g / g*g products rounded in float.
        g  = (float) std::tan (juce::MathConstants<double>::pi * cutoffFrequency / sampleRate);
        R2 = (float) std::sqrt (2.0);
        h  = (float) (1.0 / (1.0 + R2 * g + g * g));
    }

    float g = 0.0f, R2 = 0.0f, h = 0.0f;
    float s1[2] {}, s2[2] {}, s3[2] {}, s4[2] {};

    double sampleRate      = 44100.0;
    float  cutoffFrequency = 2000.0f;
};

} // namespace anamorph
