#pragma once
// H4's exact-Wet path isolates the Drive blend from separate Mix cancellation.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

namespace n2_drive_endpoint_tests {
using E=anamorph::AnamorphEngine;
namespace n207_access {
struct Drive { using type=juce::SmoothedValue<float> E::*;friend type member(Drive); };
struct Blend { using type=juce::SmoothedValue<float> E::*;friend type member(Blend); };
struct OS { using type=bool E::*;friend type member(OS); };
struct OSBlend { using type=juce::SmoothedValue<float> E::*;friend type member(OSBlend); };
struct OSScratch { using type=juce::AudioBuffer<float> E::*;friend type member(OSScratch); };
template<class T,typename T::type P> struct Get { friend typename T::type member(T){return P;} };
template struct Get<Drive,&E::driveSmooth>;template struct Get<Blend,&E::driveBlendSmooth>;
template struct Get<OS,&E::osRunning>;
template struct Get<OSBlend,&E::osBlend>;template struct Get<OSScratch,&E::osPathScratch>;
}
static bool bits(float a,float b){return std::memcmp(&a,&b,sizeof a)==0;}
static anamorph::EngineParameters params(float drive,anamorph::OversampleFactor factor=anamorph::OversampleFactor::Off){
    anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.0f;
    p.width=1.0f;p.mix=1.0f;p.mbEnable=true;p.mbBands=1;p.driveDb=drive;p.oversample=factor;
    p.autoGainMatch=false;return p;
}
static void fill(juce::AudioBuffer<float>& a,float x){for(int c=0;c<2;++c)for(int i=0;i<a.getNumSamples();++i)a.setSample(c,i,x);}
static void test(){
    using namespace n207_access;
    juce::ScopedNoDenormals noDenormals;
    const double g=std::pow(10.0,6.0/20.0),expected=1.0/std::tanh(g);
    for(float x:{1.0e8f,-1.0e8f}){
        auto p=params(6);auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,128);
        juce::AudioBuffer<float>a(2,128);fill(a,x);e->process(a);
        const float y=a.getSample(0,127);const double reference=std::copysign(expected,static_cast<double>(x));
        check(std::isfinite(y)&&std::abs(double(y)-reference)<2.0e-5,"full-wet one-band Drive endpoint retains saturated output");
        std::printf("extreme input=%a output=%a independent_reference=%.9g\n",double(x),double(y),reference);
    }
    // Inject an interior blend while keeping the Drive gain settled. A twin at
    // blend one supplies the actual saturation; the historical float expression
    // is then evaluated independently for the same finite input.
    auto interiorP=params(6);auto interior=std::make_unique<E>();
    auto saturated=std::make_unique<E>();interior->primeParameters(interiorP);
    saturated->primeParameters(interiorP);interior->prepare(48000,128);saturated->prepare(48000,128);
    (interior.get()->*member(Blend{})).setCurrentAndTargetValue(.5f);
    juce::AudioBuffer<float>a(2,128),saturatedAudio(2,128);
    for(int i=0;i<128;++i)for(int c=0;c<2;++c){const float x=(i&1)?1.0e8f:-1.0e8f;a.setSample(c,i,x);saturatedAudio.setSample(c,i,x);}
    saturated->process(saturatedAudio);interior->process(a);
    int interiorBad=0;
    for(int i=0;i<128;++i)for(int c=0;c<2;++c){const float x=(i&1)?1.0e8f:-1.0e8f;
        const float expectedInterior=x+.5f*(saturatedAudio.getSample(c,i)-x);
        interiorBad+=!bits(a.getSample(c,i),expectedInterior);}
    check(interiorBad==0,"N2-07: interior Drive blend retains historical float arithmetic");
    // Follow copies of both Drive clocks across the actual unwrapped transition.
    auto rampP=params(0);auto ramp=std::make_unique<E>();ramp->primeParameters(rampP);ramp->prepare(48000,128);
    rampP.driveDb=6;ramp->setParameters(rampP);auto gClock=ramp.get()->*member(Drive{}),bClock=ramp.get()->*member(Blend{});
    auto referenceP=params(6);auto reference=std::make_unique<E>();reference->primeParameters(referenceP);reference->prepare(48000,128);
    juce::AudioBuffer<float> referenceAudio(2,128);uint32_t rampSeed=0x9e0173u;
    int clockBad=0,invalid=0,interiorTicks=0,endTicks=0,terminalBad=0,terminalDiscriminating=0,terminalInside=0;
    for(int b=0;b<12;++b){bool endpoint[128];float source[128];
        for(int i=0;i<128;++i){const float previous=bClock.getCurrentValue(),blend=bClock.getNextValue();gClock.getNextValue();
        interiorTicks+=blend>0.0f&&blend<1.0f;endTicks+=juce::exactlyEqual(blend,1.0f);
        endpoint[i]=juce::exactlyEqual(blend,1.0f)&&juce::exactlyEqual(gClock.getCurrentValue(),gClock.getTargetValue());
        terminalInside+=i>0&&i<127&&!juce::exactlyEqual(previous,1.0f)&&juce::exactlyEqual(blend,1.0f);
        rampSeed=1664525u*rampSeed+1013904223u;source[i]=endpoint[i] ? ((i&1) ? 1.0e8f : -1.0e8f) : float(int(rampSeed>>8)-8388608)/8388608.0f*.5f;
        for(int c=0;c<2;++c){a.setSample(c,i,source[i]);referenceAudio.setSample(c,i,source[i]);}}
        reference->process(referenceAudio);ramp->process(a);
        for(int i=0;i<128;++i)if(endpoint[i]){
            const float ref=referenceAudio.getSample(0,i);terminalBad+=!bits(a.getSample(0,i),ref);
            terminalDiscriminating+=!bits(source[i]+1.0f*(ref-source[i]),ref);}
        clockBad+=!bits(gClock.getCurrentValue(),(ramp.get()->*member(Drive{})).getCurrentValue());
        clockBad+=!bits(bClock.getCurrentValue(),(ramp.get()->*member(Blend{})).getCurrentValue());
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)invalid+=!std::isfinite(a.getSample(c,i));}
    check(interiorTicks>0&&endTicks>0,"Drive ramp actually traverses interior and endpoint");
    check(clockBad==0&&invalid==0,"unwrapped Drive clocks and finite output are preserved");
    check(terminalInside>0&&terminalDiscriminating>0,"ramp reaches a discriminating endpoint within a block");
    check(terminalBad==0,"actual unwrapped ramp terminal samples match a settled independent engine");
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
    // Force a currently settled Drive envelope into an OS crossfade to observe
    // the held-envelope call site at blend=1. This is a deliberately injected
    // internal state; it is not a claim that that combination is host reachable.
    wrappedP.driveDb=7.0f;wrapped->setParameters(wrappedP);
    auto& osClock=wrapped.get()->*member(OSBlend{});osClock.setCurrentAndTargetValue(0.0f);osClock.setTargetValue(1.0f);
    const bool entering=osClock.isSmoothing()&&(wrapped.get()->*member(Drive{})).isSmoothing()
        &&juce::exactlyEqual((wrapped.get()->*member(Blend{})).getCurrentValue(),1.0f);
    fill(a,1.0e8f);wrapped->process(a);
    const float held=(wrapped.get()->*member(OSScratch{})).getSample(0,127);
    const bool heldPath=entering&&osClock.isSmoothing()&&(wrapped.get()->*member(OS{}));
    check(heldPath,"injected OS fade executes the settled Drive held-envelope path");
    check(std::isfinite(held)&&held>.9f&&held<1.2f,"held-envelope wrapped contribution retains finite saturation");
    std::printf("held_path=%d held_output=%a terminal_inside=%d terminal_discriminating=%d terminal_bad=%d\n",
                int(heldPath),double(held),terminalInside,terminalDiscriminating,terminalBad);
    std::printf("wrapped output=%a latency=%d clock_bad=%d\n",double(wrappedOut),wrapped->getLatencySamples(),clockBad);
}
} // namespace n2_drive_endpoint_tests
