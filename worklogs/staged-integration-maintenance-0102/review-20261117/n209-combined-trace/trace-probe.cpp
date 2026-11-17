// Nonshipping observation and exceptional-only widening comparison.
// The trace callback is diagnostic instrumentation and not a performance test.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
namespace anamorph {
using H4DiagnosticTrace = void (*) (int, int, int, int, int, int, float, float, float, float) noexcept;
extern H4DiagnosticTrace h4DiagnosticTrace;
extern bool h4DiagnosticWiden;
}
using E=anamorph::AnamorphEngine;
namespace trace_probe {
struct OS { friend auto member(OS); };
template<class T,auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<OS,&E::osRunning>;
struct Stats {
    int overflow=0,preBad=0,cleanBad=0,writes=0,reads=0,badMix=0,scrubs=0,heals=0,repairedWrites=0,unexpected=0,secondary=0;
    bool badAt[2][128]{}; bool witness=false,secondWitness=false;
    float clean=0,aligned=0,blend=0,result=0;
    float cleanAt[2][128]{},alignedAt[2][128]{};
    float secondClean=0,secondAligned=0,secondDry=0,secondWet=0,secondMix=0,secondOutput=0;
} stats;
void trace(int stage,int ch,int i,int /*wp*/,int /*rp*/,int /*lat*/,
           float a,float b,float c,float d) noexcept {
    if (stage==1 && i>=0 && i<128) {
        const bool interior=c>0.0f && c<1.0f;
        if(interior && !std::isfinite(a)) ++stats.cleanBad;
        if(interior && !std::isfinite(b)) ++stats.preBad;
        const bool overflow=interior && std::isfinite(a) && std::isfinite(b) && !std::isfinite(d);
        stats.badAt[ch][i]=overflow;
        if(overflow) {
            ++stats.overflow;
            if(!stats.witness){stats.witness=true;stats.clean=a;stats.aligned=b;stats.blend=c;stats.result=d;}
        }
        const float old=a+c*(b-a);
        if(interior && std::isfinite(old) && std::isfinite(d) && std::memcmp(&old,&d,sizeof old)!=0) ++stats.unexpected;
    } else if(stage==2 && i>=0 && i<128) {
        if(!std::isfinite(b)) ++stats.writes;
        if(!std::isfinite(c)) ++stats.reads;
        if(stats.badAt[ch][i] && std::isfinite(b)) ++stats.repairedWrites;
    } else if(stage==6 && i>=0 && i<128) {
        stats.cleanAt[ch][i]=a; stats.alignedAt[ch][i]=b;
    } else if(stage==3 && i>=0 && i<128) {
        if(!std::isfinite(d)) {
            ++stats.badMix;
            if(std::isfinite(stats.cleanAt[ch][i]) && std::isfinite(stats.alignedAt[ch][i]) && !std::isfinite(a)) {
                ++stats.secondary;
                if(!stats.secondWitness) { stats.secondWitness=true; stats.secondClean=stats.cleanAt[ch][i];
                    stats.secondAligned=stats.alignedAt[ch][i]; stats.secondDry=a; stats.secondWet=b;
                    stats.secondMix=c; stats.secondOutput=d; }
            }
        }
    }
    else if(stage==4) ++stats.scrubs;
    else if(stage==5) ++stats.heals;
}
}
int main() {
    using namespace trace_probe;
    juce::ScopedNoDenormals noDenormals;
    constexpr int n=128;
    int failures=0;
    for(bool wrapped:{false,true}) for(bool widen:{false,true}) {
        stats=Stats{}; anamorph::h4DiagnosticWiden=widen; anamorph::h4DiagnosticTrace=nullptr;
        anamorph::EngineParameters p; p.algorithm=anamorph::Algorithm::Haas; p.algoAmount=0;
        p.mbEnable=true;p.mbBands=2;p.mbFreqLow=1000.0f;p.mbFreqMid=1200.0f;p.mbFreqHigh=1500.0f;p.driveDb=6.0f;p.mix=.4f;
        if(wrapped)p.oversample=anamorph::OversampleFactor::x4;
        auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,n);
        juce::AudioBuffer<float> audio(2,n);
        for(int k=0;k<8;++k){for(int i=0;i<n;++i){audio.setSample(0,i,.125f);audio.setSample(1,i,.25f);}e->process(audio);}
        const bool observedWrap=e.get()->*member(OS{});
        p.mbEnable=false;e->setParameters(p);anamorph::h4DiagnosticTrace=trace;
        const float amp=std::numeric_limits<float>::max()*.49f;
        for(int block=0;block<5;++block) {
            std::memset(stats.badAt,0,sizeof stats.badAt);
            for(int i=0;i<n;++i){const int t=block*n+i;audio.setSample(0,i,(t/64)%2==0?amp:-amp);audio.setSample(1,i,.125f);}
            e->process(audio);
        }
        anamorph::h4DiagnosticTrace=nullptr;anamorph::h4DiagnosticWiden=false;
        if(wrapped!=observedWrap || e->getLatencySamples()!=(wrapped?6:0) || stats.unexpected!=0 || stats.cleanBad!=0 || stats.overflow==0)++failures;
        if(!widen && stats.overflow>0 && (stats.writes==0 || stats.reads==0 || stats.badMix==0 || stats.scrubs==0 || stats.heals==0))++failures;
        if(widen && stats.repairedWrites!=stats.overflow)++failures;
        std::printf("case=%s widen=%d latency=%d overflow=%d pre_nonfinite=%d clean_nonfinite=%d writes=%d reads=%d bad_mix=%d secondary_dry_overflow=%d scrubs=%d heals=%d repaired=%d finite_expr_mismatch=%d\n",
            wrapped?"x4":"off",widen,e->getLatencySamples(),stats.overflow,stats.preBad,stats.cleanBad,stats.writes,stats.reads,stats.badMix,stats.secondary,stats.scrubs,stats.heals,stats.repairedWrites,stats.unexpected);
        if(stats.witness)std::printf("witness clean=%a aligned_before=%a blend=%a old_result=%a\n",(double)stats.clean,(double)stats.aligned,(double)stats.blend,(double)stats.result);
        if(stats.secondWitness)std::printf("secondary clean=%a aligned=%a mixed_dry=%a wet=%a mix=%a output=%a\n",
            (double)stats.secondClean,(double)stats.secondAligned,(double)stats.secondDry,(double)stats.secondWet,(double)stats.secondMix,(double)stats.secondOutput);
    }
    std::printf("premise_failures=%d\n",failures);return failures?1:0;
}
