#include <juce_dsp/juce_dsp.h>
#include "AnamorphEngine.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <limits>
#include <cstdint>

static void step(anamorph::AnamorphEngine& e, anamorph::EngineParameters& p, juce::AudioBuffer<float>& b, int start) {
  for (int i=0;i<b.getNumSamples();++i) {
    int t=start+i;
    float l=0.42f*std::sin(0.137*t)+0.19f*std::cos(0.037*t);
    float r=0.11f*std::cos(0.091*t)-0.24f*std::sin(0.057*t);
    b.setSample(0,i,l); b.setSample(1,i,r);
  }
  e.setParameters(p); e.process(b);
}
static void transitions(const char* path) {
  FILE* f=std::fopen(path,"w");
  std::fprintf(f,"bands,mix,leg,t,ch,value\n");
  constexpr int bs=127;
  for (int bands : {2,3,4}) for (float mix : {1.0f,0.4f}) {
    anamorph::EngineParameters p; p.algorithm=anamorph::Algorithm::Haas; p.mbBands=bands; p.mix=mix;
    p.mbWidthLow=0.2f; p.mbWidthMid=1.8f; p.mbWidthHiMid=0.35f; p.mbWidthHigh=1.7f;
    p.mbFreqLow=200; p.mbFreqMid=900; p.mbFreqHigh=3500;
    auto e=std::make_unique<anamorph::AnamorphEngine>(); e->primeParameters(p); e->prepare(48000,bs);
    juce::AudioBuffer<float> b(2,bs); int t=0;
    for(int k=0;k<12;++k,t+=bs)step(*e,p,b,t);
    for(int leg=0;leg<2;++leg) {
      p.mbEnable=(leg==0);
      for(int k=0;k<12;++k,t+=bs) {
        step(*e,p,b,t);
        for(int i=0;i<bs;++i) for(int ch=0;ch<2;++ch)
          std::fprintf(f,"%d,%.1f,%d,%d,%d,%a\n",bands,mix,leg,k*bs+i,ch,b.getSample(ch,i));
      }
    }
  }
  std::fclose(f);
}
static void extreme() {
  constexpr int bs=64;
  for(float input : {0.25f,1.0e20f,2.0e38f}) {
    anamorph::EngineParameters p; p.algorithm=anamorph::Algorithm::Haas; p.mix=0; p.width=1; p.oversample=anamorph::OversampleFactor::Off;
    auto e=std::make_unique<anamorph::AnamorphEngine>(); e->primeParameters(p); e->prepare(48000,bs);
    juce::AudioBuffer<float> b(2,bs);
    for(int i=0;i<bs;++i) {b.setSample(0,i,input); b.setSample(1,i,input);}
    e->process(b); int same=0,zero=0;
    for(int ch=0;ch<2;++ch)for(int i=0;i<bs;++i){float v=b.getSample(ch,i); same+=std::memcmp(&v,&input,4)==0;zero+=(v==0);}
    for(int i=0;i<bs;++i){b.setSample(0,i,0.25f); b.setSample(1,i,0.25f);} e->process(b);
    std::printf("mix0 input=%a exact=%d/128 zero=%d/128 recovery=%a\n",input,same,zero,b.getSample(0,bs-1));
  }
  {
    anamorph::EngineParameters p; p.algorithm=anamorph::Algorithm::Haas; p.mix=1; p.width=0;
    auto e=std::make_unique<anamorph::AnamorphEngine>(); e->primeParameters(p); e->prepare(48000,bs);
    juce::AudioBuffer<float> b(2,bs); for(int i=0;i<bs;++i){b.setSample(0,i,1.0e8f);b.setSample(1,i,-1.0e8f+8.0f);}
    e->process(b); std::printf("mix1 width0 input=100000000,-99999992 mathematical wet=4 output=%a,%a\n",b.getSample(0,bs-1),b.getSample(1,bs-1));
  }
  for(float input : {0.25f,1.0e8f,-1.0e8f}) {
    anamorph::EngineParameters p; p.algorithm=anamorph::Algorithm::Haas; p.mix=1; p.driveDb=6; p.oversample=anamorph::OversampleFactor::Off;
    auto e=std::make_unique<anamorph::AnamorphEngine>(); e->primeParameters(p); e->prepare(48000,bs);
    juce::AudioBuffer<float> b(2,bs); for(int i=0;i<bs;++i){b.setSample(0,i,input);b.setSample(1,i,input);}
    e->process(b); std::printf("drive6 input=%a output=%a\n",input,b.getSample(0,bs-1));
  }
}
static void overlap() {
  constexpr int bs=64;
  for (int kind=0; kind<2; ++kind) {
    anamorph::EngineParameters initial; initial.algorithm=anamorph::Algorithm::Haas; initial.algoAmount=0.65f; initial.width=1.8f;
    initial.mbWidthLow=0.2f; initial.mbWidthMid=1.8f; initial.mbWidthHiMid=0.35f; initial.mbWidthHigh=1.7f;
    auto a=std::make_unique<anamorph::AnamorphEngine>(); auto b=std::make_unique<anamorph::AnamorphEngine>();
    auto c=std::make_unique<anamorph::AnamorphEngine>(); auto d=std::make_unique<anamorph::AnamorphEngine>();
    for(auto* e : {a.get(),b.get(),c.get(),d.get()}) {e->primeParameters(initial);e->prepare(48000,bs);}
    juce::AudioBuffer<float> aa(2,bs),bb(2,bs),cc(2,bs),dd(2,bs); int time=0;
    for(int k=0;k<30;++k,time+=bs) {auto q=initial;step(*a,q,aa,time);step(*b,q,bb,time);step(*c,q,cc,time);step(*d,q,dd,time);}
    double overlapDiff=0, controlDiff=0, duckEffect=0; int overlapCount=0, controlCount=0;
    for(int k=0;k<36;++k,time+=bs) {
      auto ap=initial,bp=initial,cp=initial,dp=initial;
      ap.algorithm=bp.algorithm=anamorph::Algorithm::Velvet;
      bool pulse=k>=8 && k<16;
      if(kind==0) ap.bypass=cp.bypass=pulse; else ap.mbEnable=cp.mbEnable=pulse;
      step(*a,ap,aa,time); step(*b,bp,bb,time);step(*c,cp,cc,time);step(*d,dp,dd,time);
      for(int ch=0;ch<2;++ch)for(int i=0;i<bs;++i) {
        float av=aa.getSample(ch,i),bv=bb.getSample(ch,i),cv=cc.getSample(ch,i),dv=dd.getSample(ch,i);
        double ad=std::abs((double)av-bv),cd=std::abs((double)cv-dv);
        if(ad>overlapDiff)overlapDiff=ad;if(cd>controlDiff)controlDiff=cd;
        overlapCount+=ad!=0;controlCount+=cd!=0;
        double effect=std::abs((double)bv-dv);if(effect>duckEffect)duckEffect=effect;
      }
    }
    std::printf("overlap %s: event-vs-no-event differing=%d peak=%.9g; no-duck control differing=%d peak=%.9g; duck-vs-normal peak=%.9g\n",kind==0?"bypass":"multiband",overlapCount,overlapDiff,controlCount,controlDiff,duckEffect);
  }
}
int main(int argc,char** argv){juce::ScopedNoDenormals guard; if(argc>1)transitions(argv[1]); extreme(); overlap();}
