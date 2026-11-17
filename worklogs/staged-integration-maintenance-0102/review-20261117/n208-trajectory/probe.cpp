// Nonshipping diagnostic: isolate state families after the extreme H4 witness.
#include "dsp/AnamorphEngine.h"
#include "dsp/MidSide.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

using E = anamorph::AnamorphEngine;
namespace h4_trace_access {
struct MB { friend auto member(MB); };
struct Banks { friend auto member(Banks); };
struct Active { friend auto member(Active); };
struct Haas { friend auto member(Haas); };
struct Velvet { friend auto member(Velvet); };
struct S1 { friend auto member(S1); }; struct S2 { friend auto member(S2); };
struct S3 { friend auto member(S3); }; struct S4 { friend auto member(S4); };
struct G { friend auto member(G); }; struct R2 { friend auto member(R2); }; struct H { friend auto member(H); };
template<class T, auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<MB, &E::multiband>;
template struct Get<Banks, &anamorph::MultibandWidth::bank>;
template struct Get<Active, &anamorph::MultibandWidth::active>;
template struct Get<Haas, &E::haas>;
template struct Get<Velvet, &E::velvet>;
template struct Get<S1, &anamorph::LR4Xover::s1>;
template struct Get<S2, &anamorph::LR4Xover::s2>;
template struct Get<S3, &anamorph::LR4Xover::s3>;
template struct Get<S4, &anamorph::LR4Xover::s4>;
template struct Get<G, &anamorph::LR4Xover::g>;
template struct Get<R2, &anamorph::LR4Xover::R2>;
template struct Get<H, &anamorph::LR4Xover::h>;
}

struct State { int invalid = 0; float maxFinite = 0; };
void sample(State& result, float value)
{
    if (!std::isfinite(value)) ++result.invalid;
    else result.maxFinite = std::max(result.maxFinite, std::abs(value));
}
void inspect(State& result, anamorph::LR4Xover& filter, int channel)
{
    using namespace h4_trace_access;
    sample(result, (filter.*member(S1{}))[channel]);
    sample(result, (filter.*member(S2{}))[channel]);
    sample(result, (filter.*member(S3{}))[channel]);
    sample(result, (filter.*member(S4{}))[channel]);
}
void clear(anamorph::LR4Xover& filter, int channel)
{
    using namespace h4_trace_access;
    (filter.*member(S1{}))[channel] = 0;
    (filter.*member(S2{}))[channel] = 0;
    (filter.*member(S3{}))[channel] = 0;
    (filter.*member(S4{}))[channel] = 0;
}

// Counterfactual calculation on a copied live filter, never a shipping change.
// Mode 0 widens only the first sample's first numerator, mode 1 each first
// numerator, and mode 2 either section's numerator when that result overflows.
// Other arithmetic is the current float expression, including state writes.
struct TrajectoryResult { int firstBad = -1, widened1 = 0, widened2 = 0, ordinaryMismatch = 0; };
TrajectoryResult trajectory(anamorph::LR4Xover source, int mode, bool glide)
{
    using namespace h4_trace_access;
    auto original = source;
    TrajectoryResult result;
    auto& s1 = (source.*member(S1{}))[0]; auto& s2 = (source.*member(S2{}))[0];
    auto& s3 = (source.*member(S3{}))[0]; auto& s4 = (source.*member(S4{}))[0];
    bool previouslyWidened = false;
    for (int i = 0; i < 128; ++i) {
        if (glide && i > 0) {
            const float hz = 17851.238f - static_cast<float>(i) * 50.0f;
            source.setCutoffFrequency(hz); original.setCutoffFrequency(hz);
        }
        const float g = source.*member(G{}), r2 = source.*member(R2{}), h = source.*member(H{});
        const float coefficient = r2 + g, input = .15f;
        float yH = (input - coefficient*s1 - s2)*h;
        if (!std::isfinite(yH) && (mode != 0 || i == 0) && std::isfinite(input)
            && std::isfinite(coefficient) && std::isfinite(s1) && std::isfinite(s2)
            && std::isfinite(h)) {
            yH = static_cast<float>((double(input) - double(coefficient)*s1 - s2)*h);
            ++result.widened1; previouslyWidened = true;
        }
        const float yB = g*yH + s1; s1 = g*yH + yB;
        const float yL = g*yB + s2; s2 = g*yB + yL;
        float yH2 = (yL - coefficient*s3 - s4)*h;
        if (!std::isfinite(yH2) && mode == 2 && std::isfinite(yL)
            && std::isfinite(coefficient) && std::isfinite(s3) && std::isfinite(s4)
            && std::isfinite(h)) {
            yH2 = static_cast<float>((double(yL) - double(coefficient)*s3 - s4)*h);
            ++result.widened2; previouslyWidened = true;
        }
        const float yB2 = g*yH2 + s3; s3 = g*yH2 + yB2;
        const float yL2 = g*yB2 + s4; s4 = g*yB2 + yL2;
        const float high = yL - r2*yB + yH - yL2;
        float expectedLow, expectedHigh; original.processSample(0, input, expectedLow, expectedHigh);
        if (!previouslyWidened && (std::memcmp(&yL2, &expectedLow, sizeof(float)) != 0
            || std::memcmp(&high, &expectedHigh, sizeof(float)) != 0)) ++result.ordinaryMismatch;
        if (!std::isfinite(yL2) || !std::isfinite(high) || !std::isfinite(s1)
            || !std::isfinite(s2) || !std::isfinite(s3) || !std::isfinite(s4)) {
            result.firstBad = i;
            break;
        }
    }
    return result;
}

int trajectoryFailures = 0;
void run(const char* mode, float amplitude, bool dip)
{
    using namespace h4_trace_access;
    constexpr int n = 128;
    anamorph::EngineParameters p;
    p.algorithm = anamorph::Algorithm::Velvet; p.algoAmount = 0;
    p.width = 0; p.mbEnable = true; p.mbBands = 2;
    p.mbFreqLow = 17851.238f; p.mbFreqMid = 19636.36f; p.mbFreqHigh = 21600.0f; p.mix = 1;
    auto e = std::make_unique<E>(); e->primeParameters(p); e->prepare(48000, n);
    juce::AudioBuffer<float> b(2, n);
    for (int block = 0; block < 8; ++block) { b.clear(); e->process(b); }
    b.clear(); b.setSample(0, n-1, std::numeric_limits<float>::max() * amplitude); e->process(b);
    const float wetL = b.getSample(0, n-1), wetR = b.getSample(1, n-1);
    auto& banks = (e.get()->*member(MB{})).*member(Banks{});
    const int active = (e.get()->*member(MB{})).*member(Active{});
    State wet[2], dry[2];
    for (auto& bank : banks) for (int ch = 0; ch < 2; ++ch) {
        for (auto& x : bank.x) inspect(wet[ch], x, ch);
        for (auto& x : bank.ax) inspect(wet[ch], x, ch);
        for (auto& x : bank.dx) inspect(dry[ch], x, ch);
        for (auto& x : bank.dax) inspect(dry[ch], x, ch);
    }
    // Observe the actual LR4 code on a copy of the live wet state. The next
    // input is known here: Velvet Amount is zero, then global Width is zero.
    // The copy cannot change the engine's subsequent production processing.
    auto filter = banks[active].x[0];
    if (std::string(mode) == "trajectory" || std::string(mode) == "small") {
        for (bool glide : {false, true}) for (int variant = 0; variant < 3; ++variant) {
            const auto t = trajectory(filter, variant, glide);
            std::printf("trajectory amplitude=%g glide=%d variant=%d first_bad=%d widened1=%d widened2=%d ordinary_mismatch=%d\n",
                        amplitude, glide, variant, t.firstBad, t.widened1, t.widened2, t.ordinaryMismatch);
            if (t.ordinaryMismatch != 0) ++trajectoryFailures;
            if (std::string(mode) == "trajectory" && t.widened1 == 0) ++trajectoryFailures;
            if (std::string(mode) == "small" && (t.firstBad != -1 || t.widened1 || t.widened2)) ++trajectoryFailures;
        }
    }
    float inputL = .2f, inputR = .1f, loL, hiL, loR, hiR;
    anamorph::applyWidth(inputL, inputR, p.width);
    const float g = filter.*member(G{}), r2 = filter.*member(R2{}), h = filter.*member(H{});
    const float s1 = (filter.*member(S1{}))[0], s2 = (filter.*member(S2{}))[0];
    const float s3 = (filter.*member(S3{}))[0], s4 = (filter.*member(S4{}))[0];
    const float coefficient = r2 + g;
    const float product = coefficient * s1;
    const float firstDifference = inputL - product;
    const float numerator = firstDifference - s2;
    // Widen only this copied-state calculation to test whether the reference
    // values are representable; it is not a candidate production algorithm.
    const double yh = (double(inputL) - double(coefficient)*s1 - s2)*h;
    const double yb = double(g)*yh+s1, newS1 = double(g)*yh+yb;
    const double yl = double(g)*yb+s2, newS2 = double(g)*yb+yl;
    const double yh2 = (yl-double(coefficient)*s3-s4)*h;
    const double yb2 = double(g)*yh2+s3, newS3 = double(g)*yh2+yb2;
    const double yl2 = double(g)*yb2+s4, newS4 = double(g)*yb2+yl2;
    const double high = yl-double(r2)*yb+yh-yl2;
    const double limit = std::numeric_limits<float>::max();
    const double maximum = std::max({std::abs(yh), std::abs(yb), std::abs(newS1),
        std::abs(yl), std::abs(newS2), std::abs(yh2), std::abs(yb2), std::abs(newS3),
        std::abs(yl2), std::abs(newS4), std::abs(high)});
    const float safeYh = static_cast<float>((double(firstDifference)-s2)*h);
    const float safeYb = g*safeYh+s1, safeS1 = g*safeYh+safeYb;
    const float safeYl = g*safeYb+s2, safeS2 = g*safeYb+safeYl;
    const float safeYh2 = (safeYl-coefficient*s3-s4)*h;
    const float safeYb2 = g*safeYh2+s3, safeS3 = g*safeYh2+safeYb2;
    const float safeYl2 = g*safeYb2+s4, safeS4 = g*safeYb2+safeYl2;
    const float safeHigh = safeYl-r2*safeYb+safeYh-safeYl2;
    const bool safeFinite = std::isfinite(safeYh) && std::isfinite(safeYb)
        && std::isfinite(safeS1) && std::isfinite(safeYl) && std::isfinite(safeS2)
        && std::isfinite(safeYh2) && std::isfinite(safeYb2) && std::isfinite(safeS3)
        && std::isfinite(safeYl2) && std::isfinite(safeS4) && std::isfinite(safeHigh);
    std::printf("wet-filter-arithmetic mode=%s coefficient=%g s1=%g s2=%g product=%g finite=%d "
                "first_difference=%g finite=%d numerator=%g finite=%d "
                "wide_low=%g wide_high=%g wide_max=%g representable=%d\n",
                mode, coefficient, s1, s2, product, std::isfinite(product),
                firstDifference, std::isfinite(firstDifference), numerator, std::isfinite(numerator),
                yl2, high, maximum, std::isfinite(maximum) && maximum <= limit);
    std::printf("wet-filter-one-wide-step mode=%s low=%g high=%g finite=%d\n",
                mode, safeYl2, safeHigh, safeFinite);
    filter.processSample(0, inputL, loL, hiL);
    filter.processSample(1, inputR, loR, hiR);
    std::printf("wet-filter-copy mode=%s input=%g,%g low=%g,%g high=%g,%g finite=%d,%d,%d,%d\n",
                mode, inputL, inputR, loL, loR, hiL, hiR,
                std::isfinite(loL), std::isfinite(loR), std::isfinite(hiL), std::isfinite(hiR));
    const std::string control(mode);
    for (auto& bank : banks) for (int ch = 0; ch < 2; ++ch) {
        if (control == "wet-both" || (control == "wet-left" && ch == 0)
            || (control == "wet-right" && ch == 1)) {
            for (auto& x : bank.x) clear(x, ch);
            for (auto& x : bank.ax) clear(x, ch);
        }
        if (control == "dry-left" && ch == 0) {
            for (auto& x : bank.dx) clear(x, ch);
            for (auto& x : bank.dax) clear(x, ch);
        }
    }
    if (control == "haas") (e.get()->*member(Haas{})).reset();
    if (control == "velvet") (e.get()->*member(Velvet{})).reset();
    if (dip) p.mix = .5f;
    e->setParameters(p);
    b.clear();
    for (int i = 0; i < n; ++i) { b.setSample(0, i, .2f); b.setSample(1, i, .1f); }
    e->process(b);
    int zeros[2] = {}, invalid[2] = {};
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) {
        zeros[ch] += b.getSample(ch, i) == 0;
        invalid[ch] += !std::isfinite(b.getSample(ch, i));
    }
    std::printf("mode=%s amplitude=%g dip=%d wet_last=%g,%g "
                "wet_bad=%d,%d wet_max=%g,%g dry_bad=%d,%d dry_max=%g,%g "
                "next_zero=%d,%d next_nonfinite=%d,%d next_first=%g,%g\n",
                mode, amplitude, dip, wetL, wetR, wet[0].invalid, wet[1].invalid,
                wet[0].maxFinite, wet[1].maxFinite, dry[0].invalid, dry[1].invalid,
                dry[0].maxFinite, dry[1].maxFinite, zeros[0], zeros[1], invalid[0], invalid[1],
                b.getSample(0, 0), b.getSample(1, 0));
}
int main(int argc, char** argv)
{
    juce::ScopedNoDenormals noDenormals;
    if (argc > 1) {
        const std::string mode(argv[1]);
        run(mode.c_str(), mode == "small" ? .1f : .95f, mode != "no-dip");
        std::printf("trajectory failures=%d\n", trajectoryFailures);
        return trajectoryFailures ? 1 : 0;
    }
    run("none", .95f, true); run("none", .95f, false);
    run("dry-left", .95f, true); run("wet-left", .95f, true);
    run("wet-right", .95f, true); run("wet-both", .95f, true);
    run("haas", .95f, true); run("velvet", .95f, true); run("none", .1f, true);
}
