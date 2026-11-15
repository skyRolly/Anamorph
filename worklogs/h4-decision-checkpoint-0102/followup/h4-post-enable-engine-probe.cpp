// Nonshipping bounded search for actual post-Enable dry arithmetic overflow.
// A negative result is scoped to the tested stimuli and proves no global bound.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
using E=anamorph::AnamorphEngine;
namespace post_enable_probe {
struct Blend { friend auto member(Blend); }; struct Clean { friend auto member(Clean); };
struct Aligned { friend auto member(Aligned); }; struct Reference { friend auto member(Reference); };
struct Ring { friend auto member(Ring); }; struct OS { friend auto member(OS); };
template<class T,auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<Blend,&E::mbEnableBlend>; template struct Get<Clean,&E::dryScratch>;
template struct Get<Aligned,&E::dryAlignScratch>; template struct Get<Reference,&E::loudnessRefScratch>;
template struct Get<Ring,&E::dryAlignDelayBuffer>; template struct Get<OS,&E::osRunning>;
}
static bool bits(float a,float b) { return std::memcmp(&a,&b,sizeof a)==0; }
int main() {
    using namespace post_enable_probe;
    juce::ScopedNoDenormals noDenormals;
    constexpr int n=128;
    const float max=std::numeric_limits<float>::max();
    int cases=0, failures=0, interior=0, badClean=0, badAligned=0, badReference=0, badRing=0, clockBad=0, wrapped=0;
    for (int bands : {2,3,4}) for (float cutoff : {50.0f,1000.0f,10000.0f})
    for (float ratio : {.25f,.49f,.75f,.99f}) for (int pattern : {0,1,2}) for (bool x4 : {false,true}) {
        anamorph::EngineParameters p; p.algorithm=anamorph::Algorithm::Haas; p.algoAmount=0;
        p.mbEnable=true; p.mbBands=bands; p.mbFreqLow=cutoff;
        p.mbFreqMid=std::max(cutoff*1.2f,cutoff+30.0f); p.mbFreqHigh=std::max(cutoff*1.5f,cutoff+100.0f);
        p.driveDb=6.0f; p.mix=.4f; if(x4)p.oversample=anamorph::OversampleFactor::x4;
        auto e=std::make_unique<E>(); e->primeParameters(p); e->prepare(48000,n);
        juce::AudioBuffer<float> audio(2,n);
        for(int k=0;k<8;++k) { for(int i=0;i<n;++i) { audio.setSample(0,i,.125f); audio.setSample(1,i,.25f); } e->process(audio); }
        wrapped += x4 && (e.get()->*member(OS{}));
        p.mbEnable=false; e->setParameters(p);
        int caseBad=0,caseInterior=0;
        const float amplitude=max*ratio;
        for(int block=0;block<5;++block) {
            auto clock=e.get()->*member(Blend{});
            const bool blending=clock.isSmoothing() || clock.getCurrentValue()<1.0f;
            if(!blending)continue;
            for(int i=0;i<n;++i) {
                const int t=block*n+i;
                const bool positive=pattern==0 ? ((t/8)%2==0) : pattern==1 ? ((t/64)%2==0) : ((t%2)==0);
                audio.setSample(0,i,positive?amplitude:-amplitude);
                audio.setSample(1,i,.125f);
            }
            e->process(audio);
            const auto& clean=e.get()->*member(Clean{}); const auto& aligned=e.get()->*member(Aligned{});
            const auto& ref=e.get()->*member(Reference{});
            for(int i=0;i<n;++i) {
                const float b=clock.getNextValue();
                if(b>0.0f && b<1.0f) { ++interior; ++caseInterior;
                    for(int ch=0;ch<2;++ch) { badClean+=!std::isfinite(clean.getSample(ch,i));
                        const bool bad=!std::isfinite(aligned.getSample(ch,i)); badAligned+=bad; caseBad+=bad;
                        badReference+=!std::isfinite(ref.getSample(ch,i)); }
                }
            }
            clockBad+=!bits(clock.getCurrentValue(),(e.get()->*member(Blend{})).getCurrentValue());
            const auto& ring=e.get()->*member(Ring{});
            for(int ch=0;ch<2;++ch)for(int i=0;i<ring.getNumSamples();++i) badRing+=!std::isfinite(ring.getSample(ch,i));
        }
        if(caseBad) std::printf("found bands=%d cutoff=%g ratio=%g pattern=%d x4=%d interior=%d aligned_nonfinite=%d\n",bands,cutoff,ratio,pattern,x4,caseInterior,caseBad);
        failures+=caseInterior==0; ++cases;
    }
    failures+=(badClean!=0); failures+=(clockBad!=0); failures+=(wrapped!=cases/2);
    std::printf("cases=%d interior=%d wrapped_cases=%d bad_clean=%d post_enable_nonfinite=%d reference_nonfinite=%d retained_ring_nonfinite=%d clock_mismatch=%d\n",
                cases,interior,wrapped,badClean,badAligned,badReference,badRing,clockBad);
    std::printf("premise_failures=%d\n",failures); return failures?1:0;
}
