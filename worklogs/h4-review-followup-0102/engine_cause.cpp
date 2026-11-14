// Nonshipping diagnostic: isolate state families after the extreme H4 witness.
#include "dsp/AnamorphEngine.h"
#include "dsp/MidSide.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>

using E = anamorph::AnamorphEngine;
namespace h4_trace_access {
struct MB { friend auto member(MB); };
struct Banks { friend auto member(Banks); };
struct Active { friend auto member(Active); };
struct Haas { friend auto member(Haas); };
struct S1 { friend auto member(S1); }; struct S2 { friend auto member(S2); };
struct S3 { friend auto member(S3); }; struct S4 { friend auto member(S4); };
struct G { friend auto member(G); }; struct R2 { friend auto member(R2); }; struct H { friend auto member(H); };
template<class T, auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<MB, &E::multiband>;
template struct Get<Banks, &anamorph::MultibandWidth::bank>;
template struct Get<Active, &anamorph::MultibandWidth::active>;
template struct Get<Haas, &E::haas>;
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
void run(const char* mode, float amplitude, bool dip)
{
    using namespace h4_trace_access;
    constexpr int n = 128;
    anamorph::EngineParameters p;
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
    // input is known here: Haas Amount is zero, then global Width is zero.
    // The copy cannot change the engine's subsequent production processing.
    auto filter = banks[active].x[0];
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
    std::printf("wet-filter-arithmetic mode=%s coefficient=%g s1=%g s2=%g product=%g finite=%d "
                "first_difference=%g finite=%d numerator=%g finite=%d "
                "wide_low=%g wide_high=%g wide_max=%g representable=%d\n",
                mode, coefficient, s1, s2, product, std::isfinite(product),
                firstDifference, std::isfinite(firstDifference), numerator, std::isfinite(numerator),
                yl2, high, maximum, std::isfinite(maximum) && maximum <= limit);
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
        return 0;
    }
    run("none", .95f, true); run("none", .95f, false);
    run("dry-left", .95f, true); run("wet-left", .95f, true);
    run("wet-right", .95f, true); run("wet-both", .95f, true);
    run("haas", .95f, true); run("none", .1f, true);
}
