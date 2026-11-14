// Nonshipping control: live N2-02 crossfades with Match on across reset/re-prepare.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

using Engine = anamorph::AnamorphEngine;
namespace access_n2_match {
struct Bypass { using type = juce::SmoothedValue<float> Engine::*; friend type member(Bypass); };
struct Multiband { using type = juce::SmoothedValue<float> Engine::*; friend type member(Multiband); };
struct Phase { using type = float Engine::*; friend type member(Phase); };
template<class Tag, typename Tag::type P> struct Access { friend typename Tag::type member(Tag) { return P; } };
template struct Access<Bypass, &Engine::bypassBlend>;
template struct Access<Multiband, &Engine::mbEnableBlend>;
template struct Access<Phase, &Engine::switchPhase>;
}
static bool same(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

int main()
{
    using namespace access_n2_match;
    juce::ScopedNoDenormals guard;
    int checks=0, failures=0;
    auto check=[&](bool condition, const char* label)
    {
        ++checks;
        if (!condition) { ++failures; std::printf("FAIL %s\n", label); }
    };
    for (bool multiband : {false, true}) for (bool initial : {false, true})
    for (bool reprepare : {false, true})
    {
        anamorph::EngineParameters p;
        p.algorithm=anamorph::Algorithm::Haas; p.algoAmount=0.65f; p.width=1.8f;
        p.mbWidthLow=0.2f; p.mbWidthMid=1.8f; p.mbWidthHiMid=0.35f; p.mbWidthHigh=1.7f;
        p.autoGainMatch=true; p.driveDb=6.0f;
        if (multiband) p.mbEnable=initial; else p.bypass=initial;
        auto engine=std::make_unique<Engine>(); engine->primeParameters(p); engine->prepare(48000,64);
        juce::AudioBuffer<float> audio(2,64); int time=0;
        auto fill=[&]
        {
            for (int i=0; i<64; ++i)
            {
                const float t=static_cast<float>(time+i);
                audio.setSample(0,i,0.42f*std::sin(0.137f*t)+0.19f*std::cos(0.037f*t));
                audio.setSample(1,i,0.11f*std::cos(0.091f*t)-0.24f*std::sin(0.057f*t));
            }
            time+=64;
        };
        for (int k=0; k<1600; ++k) { fill(); engine->process(audio); }
        auto q=p; q.algorithm=anamorph::Algorithm::Velvet;
        if (multiband) q.mbEnable=!initial; else q.bypass=!initial;
        auto& blend=multiband ? engine.get()->*member(Multiband{}) : engine.get()->*member(Bypass{});
        juce::SmoothedValue<float> clock; clock.reset(48000,multiband?0.012:0.010);
        clock.setCurrentAndTargetValue(initial?1.0f:0.0f); clock.setTargetValue(initial?0.0f:1.0f);
        engine->setParameters(q);
        const auto& adopted=engine->getAdoptedParameters();
        const bool live=(multiband ? adopted.mbEnable : adopted.bypass)==!initial;
        const bool target=same(blend.getTargetValue(),initial?0.0f:1.0f);
        fill(); engine->process(audio);
        for (int i=0;i<64;++i) clock.getNextValue();
        const bool tick=same(blend.getCurrentValue(),clock.getCurrentValue());
        const bool duck=engine.get()->*member(Phase{}) < 1.0f;
        const float matchBefore=engine->getMatchGainDb();
        const bool nontrivial=std::isfinite(matchBefore) && std::abs(matchBefore)>0.001f;
        if (reprepare) { engine->primeParameters(q); engine->prepare(48000,64); }
        else engine->reset(Engine::ResetScope::audioTailsOnly);
        const auto& landed=engine->getAdoptedParameters();
        const bool after=(multiband ? landed.mbEnable : landed.bypass)==!initial
            && same(blend.getCurrentValue(),initial?0.0f:1.0f)
            && same(blend.getTargetValue(),initial?0.0f:1.0f) && !blend.isSmoothing();
        const float matchAfter=engine->getMatchGainDb();
        const bool matchPolicy=reprepare ? std::abs(matchAfter)<0.000001f : same(matchBefore,matchAfter);
        fill(); engine->process(audio);
        bool finite=true;
        for(int c=0;c<2;++c) for(int i=0;i<64;++i) finite &= std::isfinite(audio.getSample(c,i));
        const bool stable=same(blend.getCurrentValue(),initial?0.0f:1.0f) && !blend.isSmoothing();
        std::printf("control=%s initial=%d reprepare=%d live=%d target=%d tick=%d duck=%d match_before=%.8g match_after=%.8g settled=%d finite=%d\n",
            multiband?"multiband":"bypass",initial,reprepare,live,target,tick,duck,
            static_cast<double>(matchBefore),static_cast<double>(matchAfter),after,finite);
        check(live&&target,"ordinary duck adopts live crossfade target");
        check(tick,"crossfade advances once per sample with Match on");
        check(duck,"ordinary duck actually started");
        check(nontrivial,"published Match gain is finite and nonzero");
        check(after&&stable,"reset or re-prepare settles adopted crossfade");
        check(matchPolicy,"host reset retains published gain; changed-sound re-prepare flushes");
        check(finite,"post-reset output is finite");
    }
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
