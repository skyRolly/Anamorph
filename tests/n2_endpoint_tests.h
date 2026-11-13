#pragma once
#include <cstring>

namespace n2_endpoint_tests
{
using Engine = anamorph::AnamorphEngine;
struct Kernel { using type = float (*)(float,float,float) noexcept; friend type member(Kernel); };
struct Blend { using type = juce::SmoothedValue<float> Engine::*; friend type member(Blend); };
struct Bank { using type = anamorph::MultibandWidth Engine::*; friend type member(Bank); };
struct Running { using type = bool Engine::*; friend type member(Running); };
struct Pre { using type = juce::AudioBuffer<float> Engine::*; friend type member(Pre); };
struct Clean { using type = juce::AudioBuffer<float> Engine::*; friend type member(Clean); };
struct Aligned { using type = juce::AudioBuffer<float> Engine::*; friend type member(Aligned); };
struct Reference { using type = juce::AudioBuffer<float> Engine::*; friend type member(Reference); };
template<class Tag, typename Tag::type Ptr> struct Access { friend typename Tag::type member(Tag) { return Ptr; } };
template struct Access<Kernel, &Engine::blendMultibandEnableSample>;
template struct Access<Blend, &Engine::mbEnableBlend>;
template struct Access<Bank, &Engine::multiband>;
template struct Access<Running, &Engine::mbRunning>;
template struct Access<Pre, &Engine::preMbScratch>;
template struct Access<Clean, &Engine::dryScratch>;
template struct Access<Aligned, &Engine::dryAlignScratch>;
template struct Access<Reference, &Engine::loudnessRefScratch>;
inline bool bits(float a,float b) { return std::memcmp(&a,&b,sizeof a)==0; }

static void test()
{
    std::printf("Test 23 extension: Multiband Enable exact stage endpoints and unchanged interiors (N2-03)\n");
    juce::ScopedNoDenormals noDenormals;
    const auto kernel=member(Kernel{});
    // Endpoint selection is tested at the actual private production kernel,
    // before downstream Mix / Output can round away a stage error.
    const float values[]={-0.0f,0.0f,-0.0540225133f,0.0540225133f,-0.75f,0.125f,1.0e-20f,-1.0e8f};
    bool zero=true,one=true,interior=true;int discriminating=0;
    for(float a:values)for(float b:values)
    {
        zero &= bits(kernel(a,b,0),a);one &= bits(kernel(a,b,1),b);
        discriminating += !bits(a+1.0f*(b-a),b);
        for(float t:{0.0001f,0.125f,0.499f,0.9999f,-0.25f,1.25f})interior &= bits(kernel(a,b,t),a+t*(b-a));
    }
    check(discriminating>0,"N2-03 premise: independent endpoint inputs discriminate the legacy subtraction/addition");
    check(zero,"N2-03 kernel: zero selects exact source, including signed zero");
    check(one,"N2-03 kernel: one selects exact destination bits");
    check(interior,"N2-03 kernel: interior and other finite non-endpoints retain historical float arithmetic");

    struct Format {double rate;int block;};
    for(auto fmt:{Format{44100,64},Format{48000,64},Format{48000,127},Format{96000,256}})
    for(int bands:{2,3,4})for(float mix:{1.0f,0.4f})for(bool match:{false,true})
    {
        anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.35f;p.width=1.4f;
        p.mbEnable=false;p.mbBands=bands;p.mbWidthLow=0.2f;p.mbWidthMid=1.8f;p.mbWidthHiMid=0.35f;p.mbWidthHigh=1.7f;
        p.mix=mix;p.autoGainMatch=match;
        auto e=std::make_unique<Engine>();e->primeParameters(p);e->prepare(fmt.rate,fmt.block);
        juce::AudioBuffer<float> audio(2,fmt.block),wet(2,fmt.block),dry(2,fmt.block);
        const int duration=int(fmt.rate*0.012),cycle=(duration+fmt.block-1)/fmt.block+4;
        int time=0,terminalInside=0,endpointZero=0,endpointOne=0,interiors=0,wetDifferent=0,dryDifferent=0;
        int wetBad=0,dryBad=0,outputBad=0,refBad=0,interiorBad=0;double peak=0;
        for(int k=-32;k<5*cycle;++k,time+=fmt.block)
        {
            if(k==0||k==2*cycle||k==3*cycle)p.mbEnable=true;
            if(k==cycle||k==2*cycle+1||k==4*cycle)p.mbEnable=false;
            for(int i=0;i<fmt.block;++i)
            {
                const float t=float(time+i);
                audio.setSample(0,i,0.42f*std::sin(0.137f*t)+0.19f*std::cos(0.037f*t));
                audio.setSample(1,i,0.11f*std::cos(0.091f*t)-0.24f*std::sin(0.057f*t));
            }
            e->setParameters(p);
            auto clock=e.get()->*member(Blend{});
            const bool active=p.mbEnable||clock.isSmoothing()||clock.getCurrentValue()>0;
            const bool blending=clock.isSmoothing()||clock.getCurrentValue()<1;
            // Copy the production filter state, NOT any blended samples. This
            // bank computes the independent unblended destinations below.
            auto destination=e.get()->*member(Bank{});
            if(active&&!(e.get()->*member(Running{})))destination.reset();
            e->process(audio);
            if(k<0||!active||!blending)continue;
            const auto& pre=e.get()->*member(Pre{});const auto& clean=e.get()->*member(Clean{});
            const auto& actualDry=e.get()->*member(Aligned{});const auto& reference=e.get()->*member(Reference{});
            wet.makeCopyOf(pre);
            destination.processBlock(wet.getWritePointer(0),wet.getWritePointer(1),fmt.block,
                clean.getReadPointer(0),clean.getReadPointer(1),dry.getWritePointer(0),dry.getWritePointer(1));
            float previous=clock.getCurrentValue();
            for(int i=0;i<fmt.block;++i)
            {
                const float b=clock.getNextValue();const bool atZero=b<=0,atOne=b>=1;
                endpointZero+=atZero;endpointOne+=atOne;interiors+=!atZero&&!atOne;
                if((atZero||atOne)&&!bits(previous,b)&&i>0&&i<fmt.block-1)++terminalInside;
                previous=b;
                for(int ch=0;ch<2;++ch)
                {
                    const float from=pre.getSample(ch,i),to=wet.getSample(ch,i);
                    const float cd=clean.getSample(ch,i),ad=dry.getSample(ch,i);
                    wetDifferent+=!bits(from,to);dryDifferent+=!bits(cd,ad);
                    const float expectedWet=atZero?from:atOne?to:from+b*(to-from);
                    const float expectedDry=atZero?cd:atOne?ad:cd+b*(ad-cd);
                    wetBad+=!bits(kernel(from,to,b),expectedWet);
                    dryBad+=!bits(actualDry.getSample(ch,i),expectedDry);
                    refBad+=!bits(reference.getSample(ch,i),expectedDry); // latency is zero in this matrix
                    if(!atZero&&!atOne)interiorBad+=!bits(kernel(from,to,b),from+b*(to-from));
                    // Model the existing downstream float operations explicitly:
                    // an output bit comparison alone is NOT the stage oracle.
                    const float ts0=juce::jlimit(0.0f,1.0f,mix*20.0f),ts=ts0*ts0*(3.0f-2.0f*ts0);
                    const float mixedDry=cd+ts*(expectedDry-cd);
                    const float expected=mixedDry+mix*(expectedWet-mixedDry);
                    if(!match)
                    {
                        outputBad+=!bits(audio.getSample(ch,i),expected);
                        peak=std::max(peak,std::abs(double(audio.getSample(ch,i))-expected));
                    }
                }
            }
        }
        std::printf("  %.0f/%d bands%d Mix%.1f Match%d: zero/one/interior %d/%d/%d terminalInside %d bad wet/dry/ref/output/interior %d/%d/%d/%d/%d peak %.9g\n",fmt.rate,fmt.block,bands,mix,match,endpointZero,endpointOne,interiors,terminalInside,wetBad,dryBad,refBad,outputBad,interiorBad,peak);
        check(wetDifferent>0&&dryDifferent>0,"N2-03 premises: actual non-neutral wet and phase-aligned dry banks execute");
        check(endpointZero>0&&endpointOne>0&&interiors>0&&((duration%fmt.block==0)||terminalInside>0),"N2-03 premises: both endpoints, interiors, and applicable within-block terminal samples execute");
        check(wetBad==0,"N2-03 wet stage: every endpoint selects the independent production-bank destination exactly");
        check(dryBad==0&&refBad==0,"N2-03 aligned dry stage: exact endpoints also feed the Level-Match reference");
        check(outputBad==0,"N2-03 engine: actual output matches the independently reconstructed boundary plus downstream rounding");
        check(interiorBad==0,"N2-03 engine-driven interior kernel arithmetic is bit unchanged");
    }
}
}
