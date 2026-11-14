#include <juce_dsp/juce_dsp.h>
#include "AnamorphEngine.h"
#include <cmath>
#include <cstdio>
#include <memory>
int main(int argc,char**argv){juce::ScopedNoDenormals g;if(argc<2)return 2;auto*f=std::fopen(argv[1],"w");std::fprintf(f,"xover,hz,t,out,fullwet\n");constexpr int bs=128;for(float freq:{20.f,50.f,200.f})for(float hz:{20.f,50.f,100.f,200.f}){anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.mbEnable=true;p.mbBands=2;p.mbFreqLow=freq;p.mix=1;auto e=std::make_unique<anamorph::AnamorphEngine>();e->primeParameters(p);e->prepare(48000,bs);auto twin=std::make_unique<anamorph::AnamorphEngine>();twin->primeParameters(p);twin->prepare(48000,bs);auto q=p;juce::AudioBuffer<float>b(2,bs),c(2,bs);int time=0;for(int k=0;k<1160;++k,time+=bs){if(k==1125)p.mix=.5f;for(int i=0;i<bs;++i){float x=.5f*std::sin(6.283185307179586*hz*(time+i)/48000.0);b.setSample(0,i,x);b.setSample(1,i,x);c.setSample(0,i,x);c.setSample(1,i,x);}e->setParameters(p);e->process(b);twin->setParameters(q);twin->process(c);if(k>=1125)for(int i=0;i<bs;++i)std::fprintf(f,"%.0f,%.0f,%d,%a,%a\n",freq,hz,(k-1125)*bs+i,b.getSample(0,i),c.getSample(0,i));}}std::fclose(f);}
