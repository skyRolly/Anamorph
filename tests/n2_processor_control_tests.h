#pragma once
#include "n2_test_support.h"
static void testProcessorCrossfadedControlsDuringDuck()
{
    using namespace n2_tests;
    std::printf("State test 159: independently crossfaded processor controls during an ordinary duck (N2-02)\n");
    for(bool mb:{false,true})for(bool on:{false,true})
    {
        auto a=std::make_unique<AnamorphAudioProcessor>(),b=std::make_unique<AnamorphAudioProcessor>();
        auto c=std::make_unique<AnamorphAudioProcessor>(),d=std::make_unique<AnamorphAudioProcessor>();
        auto set=[](AnamorphAudioProcessor& p,const char* id,float normalized){p.getAPVTS().getParameter(id)->setValueNotifyingHost(normalized);};
        for(auto* p:{a.get(),b.get(),c.get(),d.get()})
        {
            set(*p,pid::advancedMode,1);set(*p,pid::algorithm,0);set(*p,pid::amount,0.65f);set(*p,pid::width,0.9f);
            set(*p,pid::mbEnable,0);set(*p,pid::mbWidthLow,0.1f);set(*p,pid::mbWidthMid,0.9f);
            set(*p,pid::mbWidthHiMid,0.175f);set(*p,pid::mbWidthHigh,0.85f);set(*p,mb?pid::mbEnable:pid::bypass,on?1.0f:0.0f);
            p->setRateAndBufferSizeDetails(48000,64);p->prepareToPlay(48000,64);
        }
        juce::AudioBuffer<float> aa(2,64),bb(2,64),cc(2,64),dd(2,64);juce::MidiBuffer midi;int time=0;
        auto run=[&](AnamorphAudioProcessor& p,juce::AudioBuffer<float>& out){fill(out,time);p.processBlock(out,midi);};
        for(int k=0;k<30;++k,time+=64){run(*a,aa);run(*b,bb);run(*c,cc);run(*d,dd);}
        set(*a,pid::algorithm,1.0f/3.0f);set(*b,pid::algorithm,1.0f/3.0f);
        juce::SmoothedValue<float> ref;ref.reset(48000,mb?0.012:0.010);ref.setCurrentAndTargetValue(on?1.0f:0.0f);
        bool readback=true,adopted=true,ticks=true,duckRan=false;double heard=0,control=0,duck=0;
        for(int k=0;k<40;++k,time+=64)
        {
            const bool pulse=(k>=8&&k<16)||(k>=20&&k<22);const bool live=pulse?!on:on;
            for(auto* p:{a.get(),c.get()})set(*p,mb?pid::mbEnable:pid::bypass,live?1.0f:0.0f);
            readback &= (a->getAPVTS().getRawParameterValue(mb?pid::mbEnable:pid::bypass)->load()>0.5f)==live;
            run(*a,aa);run(*b,bb);run(*c,cc);run(*d,dd);
            duckRan |= a->getEngine().*member(Phase{}) < 0.9f;
            ref.setTargetValue(live?1.0f:0.0f);for(int i=0;i<64;++i)ref.getNextValue();
            adopted &= value(a->getEngine().getAdoptedParameters(),mb)==live;
            ticks &= bits(blend(a->getEngine(),mb).getCurrentValue(),ref.getCurrentValue());
            heard=std::max(heard,difference(aa,bb));control=std::max(control,difference(cc,dd));duck=std::max(duck,difference(bb,dd));
        }
        check(readback,"N2-02 processor premise: host-style notifications produce both raw return edges");
        check(control>1e-4&&duckRan&&((!mb&&on)||duck>0.01),"N2-02 processor premises: audible control and executed ordinary duck (True Bypass may hide duck audio)");
        check(adopted,"N2-02 processor: every block adopts Bypass/Multiband independently during ordinary duck");
        check(ticks,"N2-02 processor: crossfade progresses from the request block with one tick per sample");
        check(heard>1e-5,"N2-02 processor: processed audio responds, not only readback");
    }
}
