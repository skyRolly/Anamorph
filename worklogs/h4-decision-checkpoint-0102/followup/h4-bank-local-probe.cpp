// Nonshipping bank-local candidate: deliberate dry-state injection during an
// observed live crossover fade. The injected state is not a reachability claim.
#include "dsp/AnamorphEngine.h"
#include "../../../tests/AllocationGuard.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

namespace anamorph { extern bool h4DiagnosticUseConditionedFallback; }
using E = anamorph::AnamorphEngine;
using M = anamorph::MultibandWidth;
namespace h4_fade_access {
struct MB { friend auto member(MB); }; struct Banks { friend auto member(Banks); };
struct Active { friend auto member(Active); }; struct Fading { friend auto member(Fading); };
struct FadePos { friend auto member(FadePos); }; struct OS { friend auto member(OS); };
struct S1 { friend auto member(S1); }; struct S2 { friend auto member(S2); };
struct S3 { friend auto member(S3); }; struct S4 { friend auto member(S4); };
struct DryRing { friend auto member(DryRing); }; struct AlignRing { friend auto member(AlignRing); };
struct RawRing { friend auto member(RawRing); }; struct CleanScratch { friend auto member(CleanScratch); };
struct AlignScratch { friend auto member(AlignScratch); };
template<class T, auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<MB, &E::multiband>; template struct Get<Banks, &M::bank>;
template struct Get<Active, &M::active>; template struct Get<Fading, &M::fading>;
template struct Get<FadePos, &M::fadePos>; template struct Get<OS, &E::osRunning>;
template struct Get<S1, &anamorph::LR4Xover::s1>; template struct Get<S2, &anamorph::LR4Xover::s2>;
template struct Get<S3, &anamorph::LR4Xover::s3>; template struct Get<S4, &anamorph::LR4Xover::s4>;
template struct Get<DryRing, &E::dryDelayBuffer>; template struct Get<AlignRing, &E::dryAlignDelayBuffer>;
template struct Get<RawRing, &E::bypassDelayBuffer>; template struct Get<CleanScratch, &E::dryScratch>;
template struct Get<AlignScratch, &E::dryAlignScratch>;
}
static bool same(float a,float b) { return std::memcmp(&a,&b,sizeof a)==0; }
static bool sameFilter(const anamorph::LR4Xover& a,const anamorph::LR4Xover& b,int ch) {
    using namespace h4_fade_access;
    return same((a.*member(S1{}))[ch],(b.*member(S1{}))[ch])
        && same((a.*member(S2{}))[ch],(b.*member(S2{}))[ch])
        && same((a.*member(S3{}))[ch],(b.*member(S3{}))[ch])
        && same((a.*member(S4{}))[ch],(b.*member(S4{}))[ch]);
}
static bool sameBuffer(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,int ch) {
    if(a.getNumSamples()!=b.getNumSamples())return false;
    for(int i=0;i<a.getNumSamples();++i)if(!same(a.getSample(ch,i),b.getSample(ch,i)))return false;
    return true;
}
static void fill(juce::AudioBuffer<float>& b,int t) {
    for(int i=0;i<b.getNumSamples();++i) {
        const float x=static_cast<float>(t+i);
        b.setSample(0,i,.18f*std::sin(.017f*x)+.07f*std::cos(.031f*x));
        b.setSample(1,i,.11f*std::cos(.013f*x)-.03f*std::sin(.023f*x));
    }
}
int main(int argc,char** argv) {
    using namespace h4_fade_access;
    const std::string mode=argc>1?argv[1]:"positive";
    if(mode!="positive"&&mode!="claim-reset"&&mode!="no-fallback")return 2;
    juce::ScopedNoDenormals noDenormals;
    int checks=0,failures=0,lanes=0,faults=0,validBankPreserved=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    const auto live=anamorph::testing::selfCheck();
    check(live.newLive&&live.alignedNewLive&&live.mallocCompiledIn&&live.mallocLive,"allocation observers live");
    for(int bands:{2,3,4})for(bool wrapped:{false,true})for(bool injectDestination:{false,true}) {
        ++lanes;
        anamorph::h4DiagnosticUseConditionedFallback=mode!="no-fallback";
        anamorph::EngineParameters p;p.mbEnable=true;p.mbBands=bands;p.mix=.4f;
        p.mbFreqLow=180.0f;p.mbFreqMid=900.0f;p.mbFreqHigh=3500.0f;
        if(wrapped){p.oversample=anamorph::OversampleFactor::x4;p.driveDb=6.0f;}
        auto clean=std::make_unique<E>(),fault=std::make_unique<E>();
        for(E* e:{clean.get(),fault.get()}){e->primeParameters(p);e->prepare(48000,128);}
        juce::AudioBuffer<float> a(2,128),b(2,128);int t=0;
        for(int k=0;k<16;++k){fill(a,t);fill(b,t);t+=128;clean->process(a);fault->process(b);}
        check((fault.get()->*member(OS{}))==wrapped,"actual wrapped execution");
        check(fault->getLatencySamples()==(wrapped?6:0),"actual latency");
        p.mbFreqLow=700.0f;clean->setParameters(p);fault->setParameters(p);
        a.setSize(2,1);b.setSize(2,1);fill(a,t);fill(b,t);++t;clean->process(a);fault->process(b);
        auto& cm=clean.get()->*member(MB{});auto& fm=fault.get()->*member(MB{});
        const int active=fm.*member(Active{}),injected=injectDestination?1-active:active,other=1-injected;
        check((fm.*member(Fading{}))&&(cm.*member(Fading{}))&&(fm.*member(FadePos{}))>0,
              "both banks actually fading before injection");
        for(int repeat=0;repeat<2;++repeat) {
            auto& fb=fm.*member(Banks{});auto& cb=cm.*member(Banks{});
            // Advance a copy of the *fault engine's own* other live bank below.
            // On the second recovery it has already diverged from the clean twin,
            // so the twin alone cannot prove that this call reset it again.
            auto expectedOther=fb[other].dx[0];
            (fb[injected].dx[0].*member(S1{}))[0]=std::numeric_limits<float>::quiet_NaN();
            fill(a,t);fill(b,t);++t;clean->process(a);
            anamorph::testing::resetCounts();
            {anamorph::testing::Armed armed;fault->process(b);}
            const long news=anamorph::testing::newCount.load(),mallocs=anamorph::testing::mallocCount.load();
            ++faults;
            check((fm.*member(Fading{}))&&(fm.*member(Active{}))==active,"fade remains live through recovery");
            check(news==0&&mallocs==0,"no observed allocation in injected recovery call");
            const float actual=(fault.get()->*member(AlignScratch{})).getSample(0,0);
            const float conditioned=(fault.get()->*member(CleanScratch{})).getSample(0,0);
            float expectedLow=0.0f,expectedHigh=0.0f;
            expectedOther.processSample(0,conditioned,expectedLow,expectedHigh);
            check(std::isfinite(actual)&&same(actual,conditioned),"fault block publishes conditioned fallback");
            check((fb[injected].dx[0].*member(S1{}))[0]==0.0f,"injected channel state reset");
            const bool otherWasValid=std::isfinite(expectedLow)&&std::isfinite(expectedHigh)
                                   &&std::isfinite((expectedOther.*member(S1{}))[0])
                                   &&(expectedOther.*member(S1{}))[0]!=0.0f;
            const bool sameValidBank=sameFilter(fb[other].dx[0],expectedOther,0);
            validBankPreserved+=otherWasValid&&sameValidBank;
            check(otherWasValid&&sameValidBank,"other live dry bank retains independently advanced affected-channel state");
            if(mode=="claim-reset")check(!sameValidBank,"negative claim that valid live dry bank was reset");
            bool wet=true,opposite=true;
            for(int k=0;k<2;++k)for(int s=0;s<bands-1;++s) {
                for(int ch=0;ch<2;++ch)wet &= sameFilter(fb[k].x[s],cb[k].x[s],ch)
                    &&(s==0||sameFilter(fb[k].ax[s],cb[k].ax[s],ch));
                opposite &= sameFilter(fb[k].dx[s],cb[k].dx[s],1)
                    &&(s==0||sameFilter(fb[k].dax[s],cb[k].dax[s],1));
            }
            check(wet,"both wet banks retain checked recursive state");
            check(opposite&&same(a.getSample(1,0),b.getSample(1,0)),"opposite dry state and output preserved");
            check(sameBuffer(clean.get()->*member(AlignRing{}),fault.get()->*member(AlignRing{}),1),
                  "opposite aligned history preserved");
            check(sameBuffer(clean.get()->*member(DryRing{}),fault.get()->*member(DryRing{}),0)
                  &&sameBuffer(clean.get()->*member(DryRing{}),fault.get()->*member(DryRing{}),1),
                  "conditioned history preserved");
            check(sameBuffer(clean.get()->*member(RawRing{}),fault.get()->*member(RawRing{}),0)
                  &&sameBuffer(clean.get()->*member(RawRing{}),fault.get()->*member(RawRing{}),1),
                  "raw history preserved");
        }
    }
    std::printf("lanes=%d injected_recoveries=%d valid_other_bank_preserved=%d\n",lanes,faults,validBankPreserved);
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
