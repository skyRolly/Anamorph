// Nonshipping N2-01/V2-01 experiment. No production diagnostic hooks.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using Engine = anamorph::AnamorphEngine;
using Params = anamorph::EngineParameters;
namespace h4_access
{
struct MB { friend auto member(MB); };
struct Banks { friend auto member(Banks); };
struct Fading { friend auto member(Fading); };
struct Dry { friend auto member(Dry); };
struct Ref { friend auto member(Ref); };
struct Mix { friend auto member(Mix); };
struct Enable { friend auto member(Enable); };
struct Bypass { friend auto member(Bypass); };
template<class Tag, auto Ptr> struct Get { friend auto member(Tag) { return Ptr; } };
template struct Get<MB, &Engine::multiband>;
template struct Get<Banks, &anamorph::MultibandWidth::bank>;
template struct Get<Fading, &anamorph::MultibandWidth::fading>;
template struct Get<Dry, &Engine::dryScratch>;
template struct Get<Ref, &Engine::loudnessRefScratch>;
template struct Get<Mix, &Engine::mixSmooth>;
template struct Get<Enable, &Engine::mbEnableBlend>;
template struct Get<Bypass, &Engine::bypassBlend>;
}
struct Case
{
    std::string name;
    double rate=48000, idle=3;
    int block=128, bands=2;
    float freq=20, tone=20, mix=0.5f;
    bool noise=false, unequal=false, width=false, variable=false, moving=false;
    int action=0; // 1 repeated/reversed Mix; 2 topology; 3 MB toggle; 4 Bypass; 5 Match
    bool os=false;
};
Params params(const Case& c)
{
    Params p; p.mbEnable=true; p.mbBands=c.bands; p.mbFreqLow=c.freq;
    p.mbFreqMid=4000; p.mbFreqHigh=10000;
    if(c.width) { p.mbWidthLow=0.2f;p.mbWidthMid=1.8f;p.mbWidthHiMid=0.35f;p.mbWidthHigh=1.7f; }
    if(c.os) { p.oversample=anamorph::OversampleFactor::x4;p.driveDb=6.0f; }
    return p;
}
void stimulus(juce::AudioBuffer<float>& b, int t, const Case& c, unsigned& seed)
{
    for(int i=0;i<b.getNumSamples();++i)
    {
        seed=1664525u*seed+1013904223u;
        const float x=c.noise ? (float(int(seed>>8)-8388608)/8388608.0f)*0.35f
                             : float(0.5*std::sin(juce::MathConstants<double>::twoPi*c.tone*(t+i)/c.rate));
        b.setSample(0,i,x);
        b.setSample(1,i,c.unequal ? -0.37f*x+float(0.06*std::cos(0.037*(t+i))) : x);
    }
}
std::vector<Case> cases()
{
    Case g;g.name="sustained-glide-48";g.freq=1000;g.noise=g.unequal=true;g.bands=4;g.action=7;
    Case h=g;h.name="sustained-glide-192";h.rate=192000;h.block=32;
    Case j;j.name="overlap-jump";j.bands=4;j.width=j.noise=j.unequal=true;j.action=6;
    Case k=j;k.name="overlap-jump-os4";k.os=true;
    return {g,h,j,k};
}
int correctness(const char* path)
{
    auto* samples=std::fopen(path,"w");if(!samples)return 2;
    std::fprintf(samples,"case,t,actual,expected,wet\n");
    std::puts("case,peak960,rms960,level960_db,peak_first16,peak_all,tail_peak,ratio2560,gate_clean_blocks,dry_frozen_blocks,dry_changed_blocks,wet_changed_blocks,reentry_blocks,last_above_16eps_sample,bank_fade_blocks,proved_glide_samples,reference_kind");
    for(const auto& c:cases())
    {
        auto p=params(c);auto e=std::make_unique<Engine>(), twin=std::make_unique<Engine>();
        for(auto* x:{e.get(),twin.get()}) {x->primeParameters(p);x->prepare(c.rate,c.block);}
        anamorph::MultibandWidth dry;dry.prepare(c.rate,c.block);dry.setBandCount(p.mbBands);
        dry.setCrossovers(p.mbFreqLow,p.mbFreqMid,p.mbFreqHigh);dry.reset();
        juce::AudioBuffer<float> b(2,c.block), w(2,c.block), input(2,c.block), throwaway(2,c.block), aligned(2,c.block);
        std::vector<std::array<float,2>> history(256);int ring=0,t=0,frame=0,previousBands=p.mbBands;
        unsigned seed=1234567;double sq=0,out=0,refSq=0,peak=0,early=0,all=0,tail=0,in2560=0,out2560=0;
        int count=0,gates=0,frozen=0,changed=0,wetChanged=0,reentry=0,lastAbove=-1,fadeBlocks=0,provedGlide=0;bool previousActive=true;
        const int idle=int(c.rate*c.idle);
        while(t<idle+int(c.rate*0.6))
        {
            const int n=c.variable ? std::array<int,4>{c.block,17,c.block/2,1}[frame%4] : c.block;
            for(auto* a:{&b,&w,&input,&throwaway,&aligned})a->setSize(2,n,false,false,true);
            stimulus(input,t,c,seed);b.makeCopyOf(input);w.makeCopyOf(input);
            const int rt=t-idle;
            p.mix=rt>=0?c.mix:1.0f;
            if(c.action==1&&rt>=0)
                p.mix=(rt<int(c.rate*0.04)||rt>=int(c.rate*0.20))?0.5f:(rt<int(c.rate*0.09)||rt>=int(c.rate*0.12))?1.0f:0.25f;
            if(c.action==7) p.mbFreqLow=1000*std::exp2(float(0.5*std::sin(8.0*t/c.rate)));
            if(c.action==6&&rt>=-int(c.rate*0.004))p.mbFreqLow=800;
            if(c.action==2&&t>idle-int(c.rate*0.1))p.mbBands=3;
            if(c.action==3&&rt>=0)p.mbEnable=!(rt>=int(c.rate*0.08)&&rt<int(c.rate*0.16));
            if(c.action==4&&rt>=0)p.bypass=rt>=int(c.rate*0.08)&&rt<int(c.rate*0.16);
            if(c.action==5&&rt>=0)p.autoGainMatch=rt>=int(c.rate*0.08)&&rt<int(c.rate*0.30);
            e->setParameters(p);auto q=p;q.mix=1;twin->setParameters(q);
            auto mix=e.get()->*member(h4_access::Mix{});
            auto blend=e.get()->*member(h4_access::Enable{});
            const auto& adopted=e->getAdoptedParameters();
            const bool active=adopted.mbEnable||blend.isSmoothing()||blend.getCurrentValue()>0;
            const bool blending=blend.isSmoothing()||blend.getCurrentValue()<1;
            const auto bankBefore=(e.get()->*member(h4_access::MB{})).*member(h4_access::Banks{});
            // Arrays decay with auto: copy explicit dry filter values before processing.
            auto dxBefore=bankBefore[0].dx[0];auto wetBefore=bankBefore[0].x[0];
            auto& mb=e.get()->*member(h4_access::MB{});
            const bool bankFading=mb.*member(h4_access::Fading{});
            const float cur=mb.getLiveCutoff(0), demand=p.mbFreqLow;
            const float coeff=1-std::exp(-1.0f/(0.02f*float(c.rate)));
            const double bound=std::abs(double(demand)-cur)*std::pow(1-double(coeff),n)-2*n*std::numeric_limits<float>::epsilon()*std::max(cur,demand);
            if(c.action==7&&!bankFading&&bound>0.05+2e-4*demand)provedGlide+=n;
            e->process(b);twin->process(w);
            fadeBlocks+=bankFading||(mb.*member(h4_access::Fading{}));
            const auto& afterBanks=(e.get()->*member(h4_access::MB{})).*member(h4_access::Banks{});
            const auto& conditioned=e.get()->*member(h4_access::Dry{});
            const auto& loudnessRef=e.get()->*member(h4_access::Ref{});
            if(t>int(c.rate*0.02)&&t+n<idle&&!c.moving&&c.action!=2)
            {
                bool clean=true;for(int i=0;i<n;++i)for(int ch=0;ch<2;++ch)
                    clean &= std::memcmp(&loudnessRef.getReadPointer(ch)[i],&conditioned.getReadPointer(ch)[i],4)==0;
                if(clean&&!c.os)++gates;
                const bool same=std::memcmp(&dxBefore,&afterBanks[0].dx[0],sizeof dxBefore)==0;
                if(same)++frozen;else ++changed;
                wetChanged += std::memcmp(&wetBefore,&afterBanks[0].x[0],sizeof wetBefore)!=0;
            }
            const auto ap=e->getAdoptedParameters();
            dry.setBandCount(ap.mbBands);dry.setCrossovers(ap.mbFreqLow,ap.mbFreqMid,ap.mbFreqHigh);
            if(ap.mbBands!=previousBands||(active&&!previousActive))dry.reset();
            previousBands=ap.mbBands;previousActive=active;
            throwaway.makeCopyOf(conditioned);aligned.makeCopyOf(conditioned);
            if(active)dry.processBlock(throwaway.getWritePointer(0),throwaway.getWritePointer(1),n,
                conditioned.getReadPointer(0),conditioned.getReadPointer(1),aligned.getWritePointer(0),aligned.getWritePointer(1));
            const int lat=e->getLatencySamples();
            for(int i=0;i<n;++i)
            {
                const float en=blend.getNextValue(), m=mix.getNextValue();
                std::array<float,2> a;
                for(int ch=0;ch<2;++ch)
                {
                    const float cd=conditioned.getSample(ch,i),ad=aligned.getSample(ch,i);
                    a[ch]=blending?cd+en*(ad-cd):ad;
                }
                history[ring]=a;const auto delayed=history[(ring+256-lat)%256];ring=(ring+1)%256;
                if(t+i<idle)continue;
                ++reentry;const int r=t+i-idle;
                for(int ch=0;ch<2;++ch)
                {
                    const float clean=conditioned.getSample(ch,i);const float ts0=juce::jlimit(0.0f,1.0f,m*20);
                    const float ts=ts0*ts0*(3-2*ts0);
                    // OS cases only use Mix >= .05: the clean reference is then irrelevant.
                    const float d=clean+ts*(delayed[ch]-clean);
                    const float wet=w.getSample(ch,i);
                    const bool settledFull=!(mix.isSmoothing())&&m==1.0f;
                    const float expected=((c.action>=2&&c.action<=5) || settledFull)?wet:d+m*(wet-d);
                    const float actual=b.getSample(ch,i);const double error=std::abs(double(actual)-expected);
                    if(error>16*std::numeric_limits<float>::epsilon())lastAbove=r;
                    all=std::max(all,error);if(r>=int(c.rate*0.5))tail=std::max(tail,error);
                    if(r<16)early=std::max(early,error);
                    if(r<960) { peak=std::max(peak,error);sq+=error*error;out+=double(actual)*actual;refSq+=double(expected)*expected;++count; }
                    if(r<2560) {out2560+=double(actual)*actual;const float x=input.getSample(ch,i);in2560+=double(x)*x;}
                    if(ch==0&&r<int(c.rate*0.4))std::fprintf(samples,"%s,%d,%a,%a,%a\n",c.name.c_str(),r,actual,expected,wet);
                }
            }
            t+=n;++frame;
        }
        std::printf("%s,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",c.name.c_str(),peak,std::sqrt(sq/std::max(1,count)),10*std::log10(out/refSq),early,all,tail,std::sqrt(out2560/in2560),gates,frozen,changed,wetChanged,reentry,lastAbove,fadeBlocks,provedGlide,(c.action>=2&&c.action<=5)?"unit-width-fullwet-twin":"warm-production-module-plus-fullwet-twin");
    }
    std::fclose(samples);return 0;
}
int benchmark()
{
    using Clock=std::chrono::steady_clock;
    std::puts("rate,block,bands,moving,os,mix,repeat,ns_per_sample,p99_block_us,max_block_us,reentry_max_us,checksum");
    volatile float sink=0;
    for(int rate:{48000,192000})for(int block:{32,256})for(int bands:{2,3,4})for(bool moving:{false,true})
    for(int mode=0;mode<3;++mode)for(int rep=0;rep<5;++rep)
    {
        Case c;c.rate=rate;c.block=block;c.bands=bands;c.moving=moving;c.noise=c.unequal=true;c.os=mode==2;
        auto p=params(c);p.mix=mode==1?0.5f:1.0f;auto e=std::make_unique<Engine>();e->primeParameters(p);e->prepare(rate,block);
        juce::AudioBuffer<float> b(2,block),input(2,block);unsigned seed=1234567;int time=0;
        std::vector<double> timings;const int warm=rate/block,blocks=std::max(400,rate/(block*3));timings.reserve(blocks);
        double total=0,reentry=0;
        for(int k=0;k<warm+blocks+20;++k,time+=block)
        {
            stimulus(input,time,c,seed);b.makeCopyOf(input);
            if(moving)p.mbFreqLow=100*std::exp2(float(0.3*std::sin(double(time)/rate)));
            if(k==warm+blocks)p.mix=0.5f;
            const auto start=Clock::now();e->setParameters(p);e->process(b);const auto end=Clock::now();
            const double ns=std::chrono::duration<double,std::nano>(end-start).count();sink=b.getSample(0,block-1);
            if(k>=warm&&k<warm+blocks){total+=ns;timings.push_back(ns);}
            if(k>=warm+blocks)reentry=std::max(reentry,ns);
        }
        std::sort(timings.begin(),timings.end());
        std::printf("%d,%d,%d,%d,%d,%g,%d,%.9g,%.9g,%.9g,%.9g,%g\n",rate,block,bands,moving,c.os,mode==1?0.5:1.0,rep,total/(blocks*block),timings[timings.size()*99/100]/1000,timings.back()/1000,reentry/1000,double(sink));
    }
    return 0;
}
int main(int argc,char** argv)
{
    juce::ScopedNoDenormals noDenormals;
    if(argc==2&&std::strcmp(argv[1],"--bench")==0)return benchmark();
    return argc==2?correctness(argv[1]):2;
}
