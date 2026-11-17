#pragma once
#include <cstdint>
#include <cstring>
#include <limits>

namespace n2_mix_endpoint_tests
{
using Engine = anamorph::AnamorphEngine;
// Explicit return types match the portable access helper in the existing
// Multiband endpoint regression (MSVC cannot use an undeduced friend `auto`).
struct Mix { using type = juce::SmoothedValue<float> Engine::*; friend type member(Mix); };
struct Dry { using type = juce::AudioBuffer<float> Engine::*; friend type member(Dry); };
struct Write { using type = int Engine::*; friend type member(Write); };
struct OS { using type = bool Engine::*; friend type member(OS); };
template<class Tag, typename Tag::type Ptr> struct Access { friend typename Tag::type member(Tag) { return Ptr; } };
template struct Access<Mix, &Engine::mixSmooth>;
template struct Access<Dry, &Engine::dryDelayBuffer>;
template struct Access<Write, &Engine::dryDelayWrite>;
template struct Access<OS, &Engine::osRunning>;

static bool bits(float a,float b) { return std::memcmp(&a,&b,sizeof a)==0; }
static uint32_t randomState=0x81c31ab5u;
static float next()
{
    randomState=randomState*1664525u+1013904223u;
    return static_cast<float>(static_cast<int32_t>(randomState>>8)-8388608)*(1.0f/8388608.0f);
}

static void test()
{
    std::printf("N2-06: exact finite Mix=1 wet endpoint\n");
    juce::ScopedNoDenormals noDenormals;
    randomState=0x81c31ab5u;
    int endpointBad=0,interiorBad=0,legacyDifferent=0;
    for(float mix:{0.0f,0.125f,0.5f,0.875f,1.0f})
    {
        anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.0f;
        p.width=0.0f;p.mix=mix;p.mbEnable=false;p.autoGainMatch=false;
        auto engine=std::make_unique<Engine>();engine->primeParameters(p);engine->prepare(48000,128);
        juce::AudioBuffer<float> audio(2,128);
        for(int block=0;block<16;++block)
        {
            float expected[2][128],old[2][128];
            for(int i=0;i<128;++i)
            {
                const float l=next(),r=-l+std::ldexp(next(),-((block+i)%18));
                const float wet=(l+r)*0.5f,input[]={l,r};
                for(int ch=0;ch<2;++ch)
                {
                    old[ch][i]=input[ch]+mix*(wet-input[ch]);
                    expected[ch][i]=juce::exactlyEqual(mix,1.0f)?wet:old[ch][i];
                    audio.setSample(ch,i,input[ch]);
                }
            }
            engine->process(audio);
            for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)
            {
                if(juce::exactlyEqual(mix,1.0f))
                {
                    endpointBad+=!bits(audio.getSample(ch,i),expected[ch][i]);
                    legacyDifferent+=!bits(old[ch][i],expected[ch][i]);
                }
                else interiorBad+=!bits(audio.getSample(ch,i),expected[ch][i]);
            }
        }
    }
    check(legacyDifferent>0,"N2-06 premise: finite actual-engine inputs discriminate the old Mix=1 expression");
    check(endpointBad==0,"N2-06: settled production Mix=1 selects finite wet bits");
    check(interiorBad==0,"N2-06: settled non-endpoint Mix retains historical float arithmetic");

    anamorph::EngineParameters extremeP;extremeP.algorithm=anamorph::Algorithm::Haas;
    extremeP.algoAmount=0.0f;extremeP.width=0.0f;extremeP.mix=1.0f;extremeP.mbEnable=false;
    auto extreme=std::make_unique<Engine>();extreme->primeParameters(extremeP);extreme->prepare(48000,64);
    juce::AudioBuffer<float> extremeAudio(2,64);
    for(int i=0;i<64;++i){extremeAudio.setSample(0,i,1.0e8f);extremeAudio.setSample(1,i,-1.0e8f+8.0f);}
    extreme->process(extremeAudio);
    check(bits(extremeAudio.getSample(0,63),4.0f)&&bits(extremeAudio.getSample(1,63),4.0f),
          "N2-06: finite extreme cancellation witness preserves actual wet on both channels");

    anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.0f;
    p.width=0.0f;p.mix=.25f;p.mbEnable=false;p.autoGainMatch=false;
    auto engine=std::make_unique<Engine>();engine->primeParameters(p);engine->prepare(48000,127);
    p.mix=1.0f;engine->setParameters(p);auto clock=engine.get()->*member(Mix{});
    juce::AudioBuffer<float> audio(2,127);int rampBad=0,clockBad=0,terminalInside=0;
    for(int block=0;block<12;++block)
    {
        float expected[2][127];
        for(int i=0;i<127;++i)
        {
            const float l=next(),r=-l+std::ldexp(next(),-((block+i)%18));
            const float wet=(l+r)*.5f,previous=clock.getCurrentValue(),m=clock.getNextValue(),input[]={l,r};
            terminalInside+=i>0&&i<126&&!juce::exactlyEqual(previous,1.0f)&&juce::exactlyEqual(m,1.0f);
            for(int ch=0;ch<2;++ch)
            {
                expected[ch][i]=juce::exactlyEqual(m,1.0f)?wet:input[ch]+m*(wet-input[ch]);
                audio.setSample(ch,i,input[ch]);
            }
        }
        engine->process(audio);
        for(int ch=0;ch<2;++ch)for(int i=0;i<127;++i)rampBad+=!bits(audio.getSample(ch,i),expected[ch][i]);
        clockBad+=!bits(clock.getCurrentValue(),(engine.get()->*member(Mix{})).getCurrentValue());
    }
    check(terminalInside>0,"N2-06 premise: Mix reaches exact one within a processing block");
    check(rampBad==0,"N2-06: smoothing path selects finite wet and preserves interior arithmetic");
    check(clockBad==0,"N2-06: Mix smoother advances exactly once per sample");

    // Keep the old self-heal observable when a stale dry ring read is invalid.
    // The selected x4 factor supplies real latency via the stand-in; no wrap
    // is claimed in this case.
    p.width=1.0f;p.mix=1.0f;p.oversample=anamorph::OversampleFactor::x4;
    auto poison=std::make_unique<Engine>();poison->primeParameters(p);poison->prepare(48000,64);
    juce::AudioBuffer<float> delayed(2,64);
    for(int block=0;block<12;++block)
    {
        for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)delayed.setSample(ch,i,.25f);
        poison->process(delayed);
    }
    const int latency=poison->getLatencySamples();auto& dry=poison.get()->*member(Dry{});
    int read=(poison.get()->*member(Write{}))-latency;if(read<0)read+=dry.getNumSamples();
    check(latency>0&&!(poison.get()->*member(OS{})),"N2-06 premise: selected-latency stand-in actually executes");
    dry.setSample(0,read,std::numeric_limits<float>::quiet_NaN());delayed.setSize(2,1);
    delayed.setSample(0,0,.25f);delayed.setSample(1,0,.25f);poison->process(delayed);
    check(bits(delayed.getSample(0,0),0.0f)&&bits(delayed.getSample(1,0),.25f),
          "N2-06: invalid dry reaches the existing self-heal without muting the valid opposite sample");
    delayed.setSize(2,64);for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)delayed.setSample(ch,i,.25f);
    poison->process(delayed);
    check(bits(delayed.getSample(0,63),.25f),"N2-06: valid processing resumes after invalid dry recovery");
    p.oversample=anamorph::OversampleFactor::Off;
    auto wetBad=std::make_unique<Engine>();wetBad->primeParameters(p);wetBad->prepare(48000,64);
    for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)delayed.setSample(ch,i,2.0e38f);
    wetBad->process(delayed);
    check(bits(delayed.getSample(0,63),0.0f),"N2-06: invalid wet still reaches the existing output self-heal");
    for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)delayed.setSample(ch,i,.25f);
    wetBad->process(delayed);
    check(bits(delayed.getSample(0,63),.25f),"N2-06: valid processing resumes after invalid wet recovery");

    // Exercise the other OS path as well: the ramp remains base-sample-clocked
    // while the Drive wrap is actually running.
    p.mix=.25f;p.driveDb=6.0f;p.oversample=anamorph::OversampleFactor::x4;
    auto wrapped=std::make_unique<Engine>();wrapped->primeParameters(p);wrapped->prepare(48000,64);
    for(int block=0;block<12;++block)
    {
        for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)delayed.setSample(ch,i,.125f);
        wrapped->process(delayed);
    }
    check(wrapped.get()->*member(OS{}),"N2-06 premise: nonlinear x4 wrapping actually executes");
    p.mix=1.0f;wrapped->setParameters(p);auto wrappedClock=wrapped.get()->*member(Mix{});
    int wrappedClockBad=0,invalidOutput=0;
    for(int block=0;block<20;++block)
    {
        for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)delayed.setSample(ch,i,.125f);
        for(int i=0;i<64;++i)wrappedClock.getNextValue();
        wrapped->process(delayed);
        wrappedClockBad+=!bits(wrappedClock.getCurrentValue(),(wrapped.get()->*member(Mix{})).getCurrentValue());
        for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)invalidOutput+=!std::isfinite(delayed.getSample(ch,i));
    }
    check(wrappedClockBad==0&&invalidOutput==0,"N2-06: wrapped Mix transition keeps its base-sample clock and finite output");
    // Independent production-output oracle for the actual wrapped endpoint:
    // a one-band H4 twin selects the same wet path without reblending it. The
    // subject reaches one inside a smoothing block, where H4 cannot yet gate.
    anamorph::EngineParameters wp;wp.algorithm=anamorph::Algorithm::Haas;wp.algoAmount=0.0f;
    wp.width=1.0f;wp.mix=.25f;wp.mbEnable=true;wp.mbBands=1;wp.autoGainMatch=false;
    wp.driveDb=6.0f;wp.oversample=anamorph::OversampleFactor::x4;
    auto subject=std::make_unique<Engine>(),wetReference=std::make_unique<Engine>();
    subject->primeParameters(wp);auto referenceP=wp;referenceP.mix=1.0f;
    wetReference->primeParameters(referenceP);subject->prepare(48000,127);wetReference->prepare(48000,127);
    juce::AudioBuffer<float> actualWet(2,127),referenceWet(2,127);
    for(int block=0;block<12;++block)
    {
        for(int i=0;i<127;++i){actualWet.setSample(0,i,1.0e8f);actualWet.setSample(1,i,-1.0e8f);
            referenceWet.setSample(0,i,1.0e8f);referenceWet.setSample(1,i,-1.0e8f);}
        subject->process(actualWet);wetReference->process(referenceWet);
    }
    const int actualLatency=subject->getLatencySamples();
    check((subject.get()->*member(OS{}))&&(wetReference.get()->*member(OS{}))&&actualLatency>0
          &&wetReference->getLatencySamples()==actualLatency,
          "N2-06 premise: subject and independent wet twin execute the actual x4 wrap");
    wp.mix=1.0f;subject->setParameters(wp);auto endpointClock=subject.get()->*member(Mix{});
    int wrappedEndpointSamples=0,wrappedEndpointBad=0,wrappedLegacyDifferent=0,wrappedTerminalBlocks=0;
    for(int block=0;block<12;++block)
    {
        const bool wasSmoothing=endpointClock.isSmoothing();bool endpoint[127];
        for(int i=0;i<127;++i){endpoint[i]=juce::exactlyEqual(endpointClock.getNextValue(),1.0f);
            actualWet.setSample(0,i,1.0e8f);actualWet.setSample(1,i,-1.0e8f);
            referenceWet.setSample(0,i,1.0e8f);referenceWet.setSample(1,i,-1.0e8f);}
        subject->process(actualWet);wetReference->process(referenceWet);
        if(wasSmoothing)for(int i=0;i<127;++i)if(endpoint[i])
        {
            ++wrappedTerminalBlocks;
            for(int ch=0;ch<2;++ch){const float dry=ch==0?1.0e8f:-1.0e8f,wet=referenceWet.getSample(ch,i);
                ++wrappedEndpointSamples;wrappedEndpointBad+=!bits(actualWet.getSample(ch,i),wet);
                wrappedLegacyDifferent+=!bits(dry+1.0f*(wet-dry),wet);}
        }
    }
    check(wrappedTerminalBlocks>0&&wrappedEndpointSamples>0&&wrappedLegacyDifferent>0&&wrappedEndpointBad==0,
          "N2-06: wrapped terminal Mix selects the independently observed wet output");
    std::printf("  x4 terminal samples=%d mismatches=%d legacy-differences=%d latency=%d\n",
                wrappedEndpointSamples,wrappedEndpointBad,wrappedLegacyDifferent,actualLatency);
    std::printf("  endpoint/interior/ramp/clock mismatches %d/%d/%d/%d; discriminating legacy samples %d\n",
                endpointBad,interiorBad,rampBad,clockBad,legacyDifferent);
}
}
