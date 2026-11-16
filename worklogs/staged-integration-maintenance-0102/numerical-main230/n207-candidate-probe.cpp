// Nonshipping N2-07 actual-engine experiment; H4's exact-Wet path isolates
// the Drive blend from the separate N2-06 Mix cancellation.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

using E=anamorph::AnamorphEngine;
namespace n207_access {
struct Drive { using type=juce::SmoothedValue<float> E::*;friend type member(Drive); };
struct Blend { using type=juce::SmoothedValue<float> E::*;friend type member(Blend); };
struct OS { using type=bool E::*;friend type member(OS); };
template<class T,typename T::type P> struct Get { friend typename T::type member(T){return P;} };
template struct Get<Drive,&E::driveSmooth>;template struct Get<Blend,&E::driveBlendSmooth>;
template struct Get<OS,&E::osRunning>;
}
static bool bits(float a,float b){return std::memcmp(&a,&b,sizeof a)==0;}
static anamorph::EngineParameters params(float drive,anamorph::OversampleFactor factor=anamorph::OversampleFactor::Off){
    anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.0f;
    p.width=1.0f;p.mix=1.0f;p.mbEnable=true;p.mbBands=1;p.driveDb=drive;p.oversample=factor;
    p.autoGainMatch=false;return p;
}
static void fill(juce::AudioBuffer<float>& a,float x){for(int c=0;c<2;++c)for(int i=0;i<a.getNumSamples();++i)a.setSample(c,i,x);}
int main(){
    using namespace n207_access;
    juce::ScopedNoDenormals noDenormals;int checks=0,failures=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    const double g=std::pow(10.0,6.0/20.0),expected=1.0/std::tanh(g);
    for(float x:{1.0e8f,-1.0e8f}){
        auto p=params(6);auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,128);
        juce::AudioBuffer<float>a(2,128);fill(a,x);e->process(a);
        const float y=a.getSample(0,127);const double reference=std::copysign(expected,static_cast<double>(x));
        check(std::isfinite(y)&&std::abs(double(y)-reference)<2.0e-5,"full-wet one-band Drive endpoint retains saturated output");
        std::printf("extreme input=%a output=%a independent_reference=%.9g\n",double(x),double(y),reference);
    }
    // A deterministic non-endpoint workload is compared byte-for-byte between
    // candidate and main via its independently recorded output hash.
    auto interiorP=params(1.0f);auto interior=std::make_unique<E>();interior->primeParameters(interiorP);interior->prepare(48000,128);
    juce::AudioBuffer<float>a(2,128);uint32_t seed=0x56292a13u;uint64_t hash=1469598103934665603ull;
    for(int b=0;b<48;++b){for(int i=0;i<128;++i){seed=1664525u*seed+1013904223u;float x=float(int(seed>>8)-8388608)/8388608.0f*.5f;
        a.setSample(0,i,x);a.setSample(1,i,x*.375f);}interior->process(a);
        for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i){uint32_t raw;const float y=a.getSample(ch,i);std::memcpy(&raw,&y,sizeof raw);hash^=raw;hash*=1099511628211ull;}}
    std::printf("interior_output_hash=%016llx\n",static_cast<unsigned long long>(hash));
    // Follow copies of both Drive clocks across the actual unwrapped transition.
    auto rampP=params(0);auto ramp=std::make_unique<E>();ramp->primeParameters(rampP);ramp->prepare(48000,128);
    rampP.driveDb=6;ramp->setParameters(rampP);auto gClock=ramp.get()->*member(Drive{}),bClock=ramp.get()->*member(Blend{});
    int clockBad=0,invalid=0,interiorTicks=0,endTicks=0;
    for(int b=0;b<12;++b){for(int i=0;i<128;++i){const float blend=bClock.getNextValue();gClock.getNextValue();
        interiorTicks+=blend>0.0f&&blend<1.0f;endTicks+=juce::exactlyEqual(blend,1.0f);}
        fill(a,.25f);ramp->process(a);clockBad+=!bits(gClock.getCurrentValue(),(ramp.get()->*member(Drive{})).getCurrentValue());
        clockBad+=!bits(bClock.getCurrentValue(),(ramp.get()->*member(Blend{})).getCurrentValue());
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)invalid+=!std::isfinite(a.getSample(c,i));}
    check(interiorTicks>0&&endTicks>0,"Drive ramp actually traverses interior and endpoint");
    check(clockBad==0&&invalid==0,"unwrapped Drive clocks and finite output are preserved");
    // Invalid source must remain visible to the existing output self-heal.
    auto badP=params(6);auto bad=std::make_unique<E>();bad->primeParameters(badP);bad->prepare(48000,128);
    fill(a,std::numeric_limits<float>::infinity());bad->process(a);
    check(bits(a.getSample(0,127),0.0f),"non-finite Drive input reaches existing self-heal");
    fill(a,.25f);bad->process(a);check(std::isfinite(a.getSample(0,127))&&std::abs(a.getSample(0,127))>.1f,"processing resumes after invalid Drive input");
    auto wrappedP=params(6,anamorph::OversampleFactor::x4);auto wrapped=std::make_unique<E>();
    wrapped->primeParameters(wrappedP);wrapped->prepare(48000,128);
    for(int b=0;b<16;++b){fill(a,1.0e8f);wrapped->process(a);}
    const float wrappedOut=a.getSample(0,127);check((wrapped.get()->*member(OS{}))&&wrapped->getLatencySamples()>0,"actual x4 Drive wrap and latency execute");
    check(std::isfinite(wrappedOut)&&wrappedOut>.9f&&wrappedOut<1.2f,"actual x4 wrapped Drive retains saturated endpoint");
    std::printf("wrapped output=%a latency=%d clock_bad=%d\n",double(wrappedOut),wrapped->getLatencySamples(),clockBad);
    std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
