// Nonshipping actual-engine N2-06 candidate assessment. The selected test
// inputs make the pre-Mix wet value independently calculable from Mid/Side.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

using E=anamorph::AnamorphEngine;
namespace n206_access {
struct Mix { friend auto member(Mix); }; struct Dry { friend auto member(Dry); };
struct Write { friend auto member(Write); }; struct OS { friend auto member(OS); };
template<class T,auto P> struct Get { friend auto member(T){return P;} };
template struct Get<Mix,&E::mixSmooth>; template struct Get<Dry,&E::dryDelayBuffer>;
template struct Get<Write,&E::dryDelayWrite>; template struct Get<OS,&E::osRunning>;
}
static bool bits(float a,float b){return std::memcmp(&a,&b,sizeof a)==0;}
static uint32_t rng=0x81c31ab5u;
static float next(){rng=rng*1664525u+1013904223u;
    return static_cast<float>(static_cast<int32_t>(rng>>8)-8388608)*(1.0f/8388608.0f);}
int main(){
    using namespace n206_access;
    juce::ScopedNoDenormals noDenormals;
    int checks=0,failures=0;
    auto check=[&](bool pass,const char* name){++checks;if(!pass){++failures;std::printf("FAIL %s\n",name);}};
    int endpointBad=0,interiorBad=0,legacyDifferent=0,samples=0;
    for(float mix:{0.0f,0.125f,0.5f,0.875f,1.0f}){
        anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.0f;
        p.width=0.0f;p.mix=mix;p.mbEnable=false;p.autoGainMatch=false;
        auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,128);
        juce::AudioBuffer<float> a(2,128);
        for(int b=0;b<16;++b){
            float expected[2][128],old[2][128];
            for(int i=0;i<128;++i){const float l=next(),r=-l+std::ldexp(next(),-((b+i)%18));
                const float wet=(l+r)*0.5f;const float x[]={l,r};
                for(int ch=0;ch<2;++ch){old[ch][i]=x[ch]+mix*(wet-x[ch]);
                    expected[ch][i]=mix==1.0f?wet:old[ch][i];a.setSample(ch,i,x[ch]);}}
            e->process(a);
            for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i){
                if(mix==1.0f){endpointBad+=!bits(a.getSample(ch,i),expected[ch][i]);
                    legacyDifferent+=!bits(old[ch][i],expected[ch][i]);}
                else interiorBad+=!bits(a.getSample(ch,i),expected[ch][i]);
                ++samples;
            }
        }
    }
    check(legacyDifferent>0,"selected finite inputs discriminate historical Mix=1 arithmetic");
    check(endpointBad==0,"actual settled production Mix=1 selects finite wet bits");
    check(interiorBad==0,"actual settled production interiors retain historical arithmetic");
    // The independent smoother copy predicts each sample of the transition,
    // including the exact terminal sample inside a block.
    anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.0f;
    p.width=0.0f;p.mix=.25f;p.mbEnable=false;p.autoGainMatch=false;
    auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,127);
    p.mix=1.0f;e->setParameters(p);auto clock=e.get()->*member(Mix{});
    juce::AudioBuffer<float> a(2,127);int rampBad=0,clockBad=0,terminal=0;
    for(int b=0;b<12;++b){float expected[2][127];
        for(int i=0;i<127;++i){const float l=next(),r=-l+std::ldexp(next(),-((b+i)%18));
            const float wet=(l+r)*.5f,m=clock.getNextValue(),x[]={l,r};
            terminal+=m==1.0f;
            for(int ch=0;ch<2;++ch){expected[ch][i]=m==1.0f?wet:x[ch]+m*(wet-x[ch]);a.setSample(ch,i,x[ch]);}}
        e->process(a);for(int ch=0;ch<2;++ch)for(int i=0;i<127;++i)rampBad+=!bits(a.getSample(ch,i),expected[ch][i]);
        clockBad+=!bits(clock.getCurrentValue(),(e.get()->*member(Mix{})).getCurrentValue());
    }
    check(terminal>0,"Mix ramp reaches its exact endpoint");
    check(rampBad==0,"actual smoothing path uses the correct endpoint and unchanged interiors");
    check(clockBad==0,"Mix smoother advances exactly once per sample");
    // A deliberately poisoned delay slot is read after a warm, selected x4
    // latency stand-in. Direct unconditional wet selection would hide it.
    p.width=1.0f;p.mix=1.0f;p.oversample=anamorph::OversampleFactor::x4;
    auto poison=std::make_unique<E>();poison->primeParameters(p);poison->prepare(48000,64);
    juce::AudioBuffer<float> b(2,64);
    for(int k=0;k<12;++k){b.clear();for(int c=0;c<2;++c)for(int i=0;i<64;++i)b.setSample(c,i,.25f);poison->process(b);}
    const int latency=poison->getLatencySamples();auto& dry=poison.get()->*member(Dry{});
    int read=(poison.get()->*member(Write{}))-latency;if(read<0)read+=dry.getNumSamples();
    check(latency>0&&!(poison.get()->*member(OS{})),"selected latency stand-in actually executes");
    dry.setSample(0,read,std::numeric_limits<float>::quiet_NaN());b.setSize(2,1);b.setSample(0,0,.25f);b.setSample(1,0,.25f);
    poison->process(b);
    check(bits(b.getSample(0,0),0.0f)&&bits(b.getSample(1,0),.25f),"invalid dry reaches existing output self-heal without muting valid opposite sample");
    b.setSize(2,64);for(int c=0;c<2;++c)for(int i=0;i<64;++i)b.setSample(c,i,.25f);poison->process(b);
    check(bits(b.getSample(0,63),.25f),"valid processing resumes after invalid dry recovery");
    p.oversample=anamorph::OversampleFactor::Off;
    auto wetBad=std::make_unique<E>();wetBad->primeParameters(p);wetBad->prepare(48000,64);
    for(int c=0;c<2;++c)for(int i=0;i<64;++i)b.setSample(c,i,2.0e38f);
    wetBad->process(b);check(bits(b.getSample(0,63),0.0f),"invalid wet still reaches existing output self-heal");
    for(int c=0;c<2;++c)for(int i=0;i<64;++i)b.setSample(c,i,.25f);wetBad->process(b);
    check(bits(b.getSample(0,63),.25f),"valid processing resumes after invalid wet recovery");
    std::printf("samples=%d legacy_endpoint_differences=%d endpoint_bad=%d interior_bad=%d ramp_bad=%d clock_bad=%d\n",
                samples,legacyDifferent,endpointBad,interiorBad,rampBad,clockBad);
    std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
