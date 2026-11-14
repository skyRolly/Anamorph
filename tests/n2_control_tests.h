#pragma once
#include "n2_test_support.h"

static void testCrossfadedControlsDuringOrdinaryDuck()
{
    using namespace n2_tests;
    std::printf("Test 77: independently crossfaded controls remain live during ordinary ducks (N2-02)\n");
    juce::ScopedNoDenormals noDenormals;
    struct Format { double rate; int block; };
    for (auto fmt : {Format{44100,64},Format{48000,127},Format{96000,256}})
    for (bool mb : {false,true}) for (bool on : {false,true}) for(int script=0;script<7;++script)
    {
        auto p=source(mb,on); auto a=std::make_unique<Engine>(), b=std::make_unique<Engine>();
        auto c=std::make_unique<Engine>(),d=std::make_unique<Engine>();
        for(auto* e:{a.get(),b.get(),c.get(),d.get()}) {e->primeParameters(p);e->prepare(fmt.rate,fmt.block);}
        juce::AudioBuffer<float> aa(2,fmt.block),bb(2,fmt.block),cc(2,fmt.block),dd(2,fmt.block);
        int time=0;
        auto run=[&](Engine& e, Params q, juce::AudioBuffer<float>& out){fill(out,time);e.setParameters(q);e.process(out);};
        for(int k=0;k<32;++k,time+=fmt.block){run(*a,p,aa);run(*b,p,bb);run(*c,p,cc);run(*d,p,dd);}
        const int bot=static_cast<int>(std::ceil(0.006*fmt.rate/fmt.block));
        const int in=bot+2, end=static_cast<int>(std::ceil(0.07*fmt.rate/fmt.block));
        juce::SmoothedValue<float> ref; ref.reset(fmt.rate,mb?0.012:0.010);ref.setCurrentAndTargetValue(on?1.0f:0.0f);
        bool adopted=true, targets=true, progression=true, latency=true, requestSeen=false, settled=true;
        int ducks=0, entered=0, left=0; double heard=0, control=0, duckEffect=0;
        bool previous=on; bool controlUnducked=true;
        for(int k=0;k<end;++k,time+=fmt.block)
        {
            bool event=false;
            if(script==0)event=k>=1&&k<bot;                         // fade-out return
            if(script==1)event=k>=in&&k<in+2;                       // fade-in return
            if(script==2)event=k>=bot-1&&k<in+2;                    // crosses bottom
            if(script==3)event=k>=1;                               // held beyond retirement
            if(script==4)event=(k>=in&&k<in+2)||(k>=in+4&&k<in+6);  // repeated return
            if(script==5)event=k>=in&&k<in+8&&(k%2==0);             // rapid retarget
            if(script==6)event=k>=in&&k<in+4;                       // ordinary interruption
            const bool live=event?!on:on;
            entered+=live!=previous&&live!=on;left+=live!=previous&&live==on;previous=live;
            auto ap=p,bp=p,cp=p;
            ap.algorithm=bp.algorithm=(script==6&&k>=in+1)?anamorph::Algorithm::Chorus:anamorph::Algorithm::Velvet;
            target(ap,mb,live);target(cp,mb,live);
            fill(aa,time);a->setParameters(ap);
            adopted &= value(a->getAdoptedParameters(),mb)==live;
            targets &= bits(blend(*a,mb).getTargetValue(),live?1.0f:0.0f);
            ref.setTargetValue(live?1.0f:0.0f);
            requestSeen |= live!=on;
            a->process(aa);for(int i=0;i<fmt.block;++i)ref.getNextValue();
            progression &= bits(blend(*a,mb).getCurrentValue(),ref.getCurrentValue());
            ducks += a.get()->*member(Phase{}) < 0.9f;
            latency &= a->getLatencySamples()==0;
            run(*b,bp,bb);run(*c,cp,cc);run(*d,p,dd);
            controlUnducked &= c.get()->*member(Phase{}) == 1.0f;
            heard=std::max(heard,difference(aa,bb));control=std::max(control,difference(cc,dd));duckEffect=std::max(duckEffect,difference(bb,dd));
            if(k==end-1)settled=bits(blend(*a,mb).getCurrentValue(),live?1.0f:0.0f);
        }
        std::printf("  %.0f/%d %s initial%d script%d: live=%d target=%d ticks=%d audio=%.5g control=%.5g edges=%d/%d\n",fmt.rate,fmt.block,mb?"MB":"Bypass",on,script,adopted,targets,progression,heard,control,entered,left);
        check(requestSeen&&entered>0,"N2-02 premise: return or held request was issued");
        // True Bypass legitimately hides the duck in the initial-On no-pulse twin.
        check(ducks>0&&controlUnducked&&((!mb&&on)||duckEffect>0.01),"N2-02 premise: real ordinary duck runs; the no-duck control remains settled");
        check(control>1e-4,"N2-02 premise: the control is audible without a duck");
        check(adopted,"N2-02: ordinary snapshots adopt the independently crossfaded control at the next block");
        check(targets,"N2-02: smoother target follows every live request, including before/after bottom");
        check(progression,"N2-02: exactly one crossfade tick per sample, no bottom restart or delayed pulse");
        check(heard>1e-5,"N2-02: the actual processed output responds during the duck");
        check(latency&&settled,"N2-02: latency unchanged and latest held/returned target settles");
    }
    // Structural/factor ducks keep their bottom ownership while crossfades stay live.
    for(bool factor:{false,true})for(bool mb:{false,true})for(bool on:{false,true})
    {
        auto p=source(mb,on);p.mbBands=2;p.mbEnable=mb?on:true;
        auto e=std::make_unique<Engine>();e->primeParameters(p);e->prepare(48000,64);
        juce::AudioBuffer<float> buf(2,64);int time=0;
        for(int k=0;k<20;++k,time+=64){fill(buf,time);e->process(buf);}
        juce::SmoothedValue<float> ref;ref.reset(48000,mb?0.012:0.010);ref.setCurrentAndTargetValue(on?1.0f:0.0f);
        bool live=true,ticks=true,deferred=true; int beforeBottom=0;
        for(int k=0;k<32;++k,time+=64)
        {
            auto q=p;if(factor)q.oversample=anamorph::OversampleFactor::x4;else q.mbBands=4;
            const bool requested=k<2?!on:on;target(q,mb,requested);
            e->setParameters(q);live &= value(e->getAdoptedParameters(),mb)==requested;
            if(k<3){++beforeBottom;deferred &= factor?e->getLatencySamples()==0:e->getAdoptedParameters().mbBands==p.mbBands;}
            fill(buf,time);e->process(buf);ref.setTargetValue(requested?1.0f:0.0f);
            for(int i=0;i<64;++i)ref.getNextValue();
            ticks &= bits(blend(*e,mb).getCurrentValue(),ref.getCurrentValue());
        }
        check(live&&ticks,"N2-02 structural/factor duck: entry pulse remains live with the same sample clock");
        check(beforeBottom==3&&deferred&&(factor?e->getLatencySamples()>0:e->getAdoptedParameters().mbBands==4),
              "N2-02 structural/factor duck: genuine discrete target is adopted only at bottom");
    }
    // Completion freeze and forced bottom are independent of the ordinary merge.
    for(bool mb:{false,true})for(bool on:{false,true})for(bool upgrade:{false,true})
    {
        auto p=source(mb,on);auto e=std::make_unique<Engine>();e->primeParameters(p);e->prepare(48000,64);
        juce::AudioBuffer<float> buf(2,64);int time=0;
        for(int k=0;k<20;++k,time+=64){fill(buf,time);e->process(buf);}
        if(upgrade){auto q=p;q.algorithm=anamorph::Algorithm::Velvet;e->setParameters(q);fill(buf,time);e->process(buf);time+=64;}
        e->requestDuck(1);auto dest=p;dest.algorithm=anamorph::Algorithm::Dimensional;dest.width=0.4f;target(dest,mb,!on);
        bool frozen=true, deferred=true, landed=false, afterLive=false;int waiting=0, preBottom=0;
        for(int k=0;k<50;++k,time+=64)
        {
            if(k==4)e->completeBulkApply(1);
            e->setParametersFrom([&]{return dest;});
            if(k<4){++waiting;frozen &= value(e->getAdoptedParameters(),mb)==on && e->isBulkSwapPending();}
            if(k>=4&&k<15&&e->getAdoptedParameters().algorithm!=dest.algorithm)
            {++preBottom;deferred &= value(e->getAdoptedParameters(),mb)==on && bits(blend(*e,mb).getTargetValue(),on?1.0f:0.0f);}
            fill(buf,time);e->process(buf);
            if(e->getAdoptedParameters().algorithm==dest.algorithm)
            {
                if(!landed){landed=bits(blend(*e,mb).getCurrentValue(),!on?1.0f:0.0f)&&!e->isBulkSwapPending();}
                // The forced bottom has passed: a new live return uses its own ramp even in fade-in.
                if(k==15){target(dest,mb,on);dest.algorithm=anamorph::Algorithm::Haas;}
            }
            if(k==16)afterLive=value(e->getAdoptedParameters(),mb)==on;
        }
        check(waiting==4&&frozen,"N2-02 forced: no destination control adopted before bulk completion");
        check(preBottom>0&&deferred,"N2-02 forced: completed destination still deferred through fade-out");
        check(landed,"N2-02 forced: both crossfades snap at complete-destination bottom");
        check(afterLive,"N2-02 forced-to-ordinary: fresh request is live during re-entry");
    }
}
