// Nonshipping diagnostic: isolate state families after the extreme H4 witness.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>

using E = anamorph::AnamorphEngine;
namespace access {
struct MB { friend auto member(MB); };
struct Banks { friend auto member(Banks); };
struct Haas { friend auto member(Haas); };
struct Aligned { friend auto member(Aligned); };
struct S1 { friend auto member(S1); }; struct S2 { friend auto member(S2); };
struct S3 { friend auto member(S3); }; struct S4 { friend auto member(S4); };
template<class T, auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<MB, &E::multiband>;
template struct Get<Banks, &anamorph::MultibandWidth::bank>;
template struct Get<Haas, &E::haas>;
template struct Get<Aligned, &E::dryAlignScratch>;
template struct Get<S1, &anamorph::LR4Xover::s1>;
template struct Get<S2, &anamorph::LR4Xover::s2>;
template struct Get<S3, &anamorph::LR4Xover::s3>;
template struct Get<S4, &anamorph::LR4Xover::s4>;
}

struct State { int invalid = 0; float maxFinite = 0; };
void sample(State& result, float value)
{
    if (!std::isfinite(value)) ++result.invalid;
    else result.maxFinite = std::max(result.maxFinite, std::abs(value));
}
void inspect(State& result, anamorph::LR4Xover& filter, int channel)
{
    using namespace access;
    sample(result, (filter.*member(S1{}))[channel]);
    sample(result, (filter.*member(S2{}))[channel]);
    sample(result, (filter.*member(S3{}))[channel]);
    sample(result, (filter.*member(S4{}))[channel]);
}
void clear(anamorph::LR4Xover& filter, int channel)
{
    using namespace access;
    (filter.*member(S1{}))[channel] = 0;
    (filter.*member(S2{}))[channel] = 0;
    (filter.*member(S3{}))[channel] = 0;
    (filter.*member(S4{}))[channel] = 0;
}
void run(const char* mode, float amplitude, bool dip)
{
    using namespace access;
    constexpr int n = 128;
    anamorph::EngineParameters p;
    p.width = 0; p.mbEnable = true; p.mbBands = 2;
    p.mbFreqLow = 17851.238f; p.mbFreqMid = 19636.36f; p.mbFreqHigh = 21600.0f; p.mix = 1;
    auto e = std::make_unique<E>(); e->primeParameters(p); e->prepare(48000, n);
    juce::AudioBuffer<float> b(2, n);
    for (int block = 0; block < 8; ++block) { b.clear(); e->process(b); }
    b.clear(); b.setSample(0, n-1, std::numeric_limits<float>::max() * amplitude); e->process(b);
    const float wetL = b.getSample(0, n-1), wetR = b.getSample(1, n-1);
    const float dryL = (e.get()->*member(Aligned{})).getSample(0, n-1);
    auto& banks = (e.get()->*member(MB{})).*member(Banks{});
    State wet[2], dry[2];
    for (auto& bank : banks) for (int ch = 0; ch < 2; ++ch) {
        for (auto& x : bank.x) inspect(wet[ch], x, ch);
        for (auto& x : bank.ax) inspect(wet[ch], x, ch);
        for (auto& x : bank.dx) inspect(dry[ch], x, ch);
        for (auto& x : bank.dax) inspect(dry[ch], x, ch);
    }
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
    std::printf("mode=%s amplitude=%g dip=%d wet_last=%g,%g dry_last=%g "
                "wet_bad=%d,%d wet_max=%g,%g dry_bad=%d,%d dry_max=%g,%g "
                "next_zero=%d,%d next_nonfinite=%d,%d next_first=%g,%g\n",
                mode, amplitude, dip, wetL, wetR, dryL, wet[0].invalid, wet[1].invalid,
                wet[0].maxFinite, wet[1].maxFinite, dry[0].invalid, dry[1].invalid,
                dry[0].maxFinite, dry[1].maxFinite, zeros[0], zeros[1], invalid[0], invalid[1],
                b.getSample(0, 0), b.getSample(1, 0));
}
int main()
{
    juce::ScopedNoDenormals noDenormals;
    run("none", .95f, true); run("none", .95f, false);
    run("dry-left", .95f, true); run("wet-left", .95f, true);
    run("wet-right", .95f, true); run("wet-both", .95f, true);
    run("haas", .95f, true); run("none", .1f, true);
}
