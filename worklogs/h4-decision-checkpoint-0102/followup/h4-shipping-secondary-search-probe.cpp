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
    int interior=0,overflow=0,preBad=0,cleanBad=0,writes=0,reads=0,badMix=0,scrubs=0,heals=0,repairedWrites=0,unexpected=0,secondary=0;
    bool badAt[2][128]{}; bool witness=false,secondWitness=false;
    float clean=0,aligned=0,blend=0,result=0;
    float cleanAt[2][128]{},alignedAt[2][128]{};
    float secondClean=0,secondAligned=0,secondDry=0,secondWet=0,secondMix=0,secondOutput=0;
} stats;
void trace(int stage,int ch,int i,int /*wp*/,int /*rp*/,int /*lat*/,
           float a,float b,float c,float d) noexcept {
    if (stage==1 && i>=0 && i<128) {
        const bool interior=c>0.0f && c<1.0f;
        if (interior) ++stats.interior;
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
    const float max=std::numeric_limits<float>::max();
    int cases=0, wrapped=0, failures=0, interior=0, overflow=0, secondary=0, badMix=0, heals=0;
    bool witnessPrinted=false;
    anamorph::h4DiagnosticWiden=false;
    for (int bands : {2,3,4}) for (float cutoff : {50.0f,1000.0f,10000.0f})
    for (float ratio : {.25f,.49f,.75f,.99f}) for (int pattern : {0,1,2}) for (bool x4 : {false,true}) {
        stats=Stats{}; anamorph::h4DiagnosticTrace=nullptr;
        anamorph::EngineParameters p; p.algorithm=anamorph::Algorithm::Haas; p.algoAmount=0;
        p.mbEnable=true; p.mbBands=bands; p.mbFreqLow=cutoff;
        p.mbFreqMid=std::max(cutoff*1.2f,cutoff+30.0f); p.mbFreqHigh=std::max(cutoff*1.5f,cutoff+100.0f);
        p.driveDb=6.0f; p.mix=.4f; if(x4)p.oversample=anamorph::OversampleFactor::x4;
        auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,n);
        juce::AudioBuffer<float> audio(2,n);
        for(int k=0;k<8;++k){for(int i=0;i<n;++i){audio.setSample(0,i,.125f);audio.setSample(1,i,.25f);}e->process(audio);}
        const bool observedWrap=e.get()->*member(OS{});
        wrapped+=observedWrap; failures+=x4!=observedWrap;
        p.mbEnable=false;e->setParameters(p);anamorph::h4DiagnosticTrace=trace;
        const float amp=max*ratio;
        for(int block=0;block<5;++block) {
            std::memset(stats.badAt,0,sizeof stats.badAt);
            for(int i=0;i<n;++i) {
                const int t=block*n+i;
                const bool positive=pattern==0 ? ((t/8)%2==0) : pattern==1 ? ((t/64)%2==0) : ((t%2)==0);
                audio.setSample(0,i,positive?amp:-amp);audio.setSample(1,i,.125f);
            }
            e->process(audio);
        }
        anamorph::h4DiagnosticTrace=nullptr;
        failures+=(stats.interior==0 || stats.unexpected!=0 || stats.cleanBad!=0);
        if(stats.secondWitness && !witnessPrinted) {
            witnessPrinted=true;
            std::printf("witness bands=%d cutoff=%g ratio=%g pattern=%d x4=%d clean=%a aligned=%a dry=%a wet=%a mix=%a output=%a\n",
                bands,cutoff,ratio,pattern,x4,(double)stats.secondClean,(double)stats.secondAligned,
                (double)stats.secondDry,(double)stats.secondWet,(double)stats.secondMix,(double)stats.secondOutput);
        }
        interior+=stats.interior; overflow+=stats.overflow; secondary+=stats.secondary;
        badMix+=stats.badMix; heals+=stats.heals; ++cases;
    }
    failures+=(cases!=216 || wrapped!=108);
    std::printf("cases=%d wrapped=%d interior_channel_ticks=%d enable_overflow=%d secondary_dry_overflow=%d bad_mix=%d heals=%d\n",
        cases,wrapped,interior,overflow,secondary,badMix,heals);
    std::printf("premise_failures=%d\n",failures); return failures?1:0;
}
