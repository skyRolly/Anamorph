// Nonshipping NR-012 measurement; no quality threshold or production test gate.
// Method, controls and limitations: worklogs/NR012_OVERSAMPLING_ALIAS_STUDY.md.
#include "../../src/dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
constexpr int order = 15, size = 1 << order, blockSize = 256;
constexpr float inputPeak = 0.4f;
constexpr double pi = 3.14159265358979323846;
struct Spectrum { double fundamental = 0, alias3 = 0, aliasSum = 0, largest = 0; int dominantOdd = 0; };

std::uint32_t sampleBits (float value)
{
    const auto bits = std::bit_cast<std::uint32_t> (value);
    return (bits & 0x7fffffffu) == 0 ? 0 : bits; // stereo comparison treats signed zeros equally
}

int folded (int harmonic, int bin)
{
    const int at = harmonic * bin % size;
    return std::min (at, size - at);
}

Spectrum analyse (const std::vector<float>& samples, int fundamental, bool omitThird = false)
{
    std::vector<float> data (2 * size, 0.0f);
    std::copy (samples.begin(), samples.end(), data.begin());
    juce::dsp::FFT fft (order);
    fft.performFrequencyOnlyForwardTransform (data.data(), true);
    const auto energy = [&] (int bin)
    {
        double sum = 0;
        for (int i = bin - 1; i <= bin + 1; ++i)
        {
            const double x = data[static_cast<std::size_t> (i)];
            sum += x * x;
        }
        return sum;
    };
    const double carrier = energy (fundamental);
    Spectrum out;
    out.fundamental = 2.0 * std::sqrt (carrier) / size;
    out.alias3 = energy (folded (3, fundamental)) / carrier;
    for (int h = omitThird ? 5 : 3; h <= 31; h += 2)
    {
        const double power = energy (folded (h, fundamental)) / carrier;
        out.aliasSum += power;
        if (power > out.largest) { out.largest = power; out.dominantOdd = h; }
    }
    return out;
}

double dbc (double powerRatio) { return 10.0 * std::log10 (std::max (powerRatio, 1.0e-30)); }

std::vector<float> tone (int bin)
{
    std::vector<float> out (size);
    for (int i = 0; i < size; ++i)
        out[static_cast<std::size_t> (i)] = inputPeak * static_cast<float> (std::sin (2.0 * pi * bin * i / size));
    return out;
}

bool calibration (int bin)
{
    auto samples = tone (bin);
    constexpr double injectedPeak = 0.01;
    for (int i = 0; i < size; ++i)
        samples[static_cast<std::size_t> (i)] += static_cast<float> (
            injectedPeak * std::sin (2.0 * pi * folded (3, bin) * i / size));
    const double expected = 20.0 * std::log10 (injectedPeak / inputPeak);
    const auto measured = analyse (samples, bin);
    const auto missing = analyse (samples, bin, true);
    // Known synthetic amplitude ratio, not a DSP quality tolerance. Omitting
    // the injected bin must fail the very same calibration acceptance surface.
    return std::abs (dbc (measured.aliasSum) - expected) < 0.02
        && std::abs (dbc (missing.aliasSum) - expected) >= 0.02;
}

struct Capture
{
    std::vector<float> first, second;
    int latency = 0;
    double repeatDelta = 0, peak = 0;
    bool valid = true;
};

Capture capture (double rate, int bin, anamorph::OversampleFactor factor, float drive, bool bypass)
{
    anamorph::EngineParameters p;
    p.algorithm = anamorph::Algorithm::Haas;
    p.algoAmount = 0.0f; // isolate Drive, no modulation/decorrelation
    p.driveDb = drive;
    p.mix = 1.0f;
    p.oversample = factor;
    p.bypass = bypass;
    auto engine = std::make_unique<anamorph::AnamorphEngine>();
    engine->primeParameters (p);
    engine->prepare (rate, blockSize);
    const auto input = tone (bin); // exact float period reused, no accumulating phase error
    juce::AudioBuffer<float> block (2, blockSize);
    Capture out { std::vector<float> (size), std::vector<float> (size) };
    for (int start = 0; start < 5 * size; start += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
            for (int ch = 0; ch < 2; ++ch)
                block.setSample (ch, i, input[static_cast<std::size_t> ((start + i) % size)]);
        engine->process (block);
        for (int i = 0; i < blockSize; ++i)
        {
            const float value = block.getSample (0, i);
            const float right = block.getSample (1, i);
            out.valid = out.valid && std::isfinite (value) && std::isfinite (right)
                && sampleBits (value) == sampleBits (right);
            if (start >= 3 * size && start < 4 * size)
                out.first[static_cast<std::size_t> (start + i - 3 * size)] = value;
            if (start >= 4 * size)
                out.second[static_cast<std::size_t> (start + i - 4 * size)] = value;
        }
    }
    out.latency = engine->getLatencySamples();
    out.valid = out.valid && out.latency == engine->predictLatency (p);
    for (int i = 0; i < size; ++i)
    {
        out.repeatDelta = std::max (out.repeatDelta,
            std::abs (static_cast<double> (out.first[static_cast<std::size_t> (i)]) - out.second[static_cast<std::size_t> (i)]));
        out.peak = std::max (out.peak, std::abs (static_cast<double> (out.second[static_cast<std::size_t> (i)])));
    }
    return out;
}
}

int main()
{
    constexpr std::array<int, 2> bins { 7501, 11203 }; // coprime to N, f0 > Nyquist/3
    for (int bin : bins)
        if (! calibration (bin))
        { std::fprintf (stderr, "alias study: injected-alias calibration/omission control FAILED\n"); return 3; }
    const std::array<anamorph::OversampleFactor, 4> factors { anamorph::OversampleFactor::Off,
        anamorph::OversampleFactor::x2, anamorph::OversampleFactor::x4, anamorph::OversampleFactor::x8 };
    std::printf ("rate_hz,tone_bin,tone_hz,mode,factor,latency,output_peak,fundamental_peak,alias3_dbc,odd3to31_dbc,dominant_odd,dominant_dbc,period_max_delta\n");
    for (double rate : { 48000.0, 96000.0 })
        for (int bin : bins)
        {
            double linearFloor = 0;
            for (int mode = 0; mode < 3; ++mode)
                for (std::size_t index = 0; index < factors.size(); ++index)
                {
                    if (mode == 2 && index != 0) continue; // one bypass control suffices
                    const auto out = capture (rate, bin, factors[index], mode == 0 ? 0.0f : 12.0f, mode == 2);
                    if (! out.valid) { std::fprintf (stderr, "alias study: nonfinite/stereo/latency control FAILED\n"); return 3; }
                    const auto a = analyse (out.second, bin);
                    if (mode == 0 && index == 0) linearFloor = a.aliasSum;
                    if (mode == 0 || mode == 2)
                        if (std::abs (a.fundamental - inputPeak) > 1.0e-5)
                        { std::fprintf (stderr, "alias study: linear/bypass carrier control FAILED\n"); return 3; }
                    if (mode == 1 && index == 0 && a.aliasSum <= linearFloor)
                    { std::fprintf (stderr, "alias study: Drive did not engage above the measured linear floor\n"); return 3; }
                    std::printf ("%.0f,%d,%.9f,%s,%d,%d,%.9g,%.9g,%.6f,%.6f,%d,%.6f,%.9g\n", rate, bin,
                        rate * bin / size, mode == 0 ? "linear" : mode == 1 ? "drive12" : "bypass",
                        1 << index, out.latency, out.peak, a.fundamental, dbc (a.alias3), dbc (a.aliasSum),
                        a.dominantOdd, dbc (a.largest), out.repeatDelta);
                }
        }
    return 0;
}
