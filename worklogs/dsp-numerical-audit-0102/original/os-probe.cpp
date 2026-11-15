#include <juce_dsp/juce_dsp.h>
#include "AnamorphEngine.h"
#include <cmath>
#include <cstdio>
#include <memory>
int main(){juce::ScopedNoDenormals guard;for(auto factor:{anamorph::OversampleFactor::x2,anamorph::OversampleFactor::x8})for(int dir=0;dir<2;++dir){anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.oversample=factor;p.driveDb=dir?6.0f:0.0f;auto e=std::make_unique<anamorph::AnamorphEngine>();e->primeParameters(p);e->prepare(48000,127);juce::AudioBuffer<float>b(2,127);int t=0;std::printf("OS case factor=%d direction=%d latency=%d\n",1<<(int)factor,dir,e->getLatencySamples());std::fflush(stdout);for(int k=0;k<48;++k){if(k==32)p.driveDb=dir?0.0f:6.0f;for(int i=0;i<127;++i){const float v=.32f*std::sin((t+i)*.177f)+.13f*std::cos((t+i)*.067f);b.setSample(0,i,v);b.setSample(1,i,-.4f*v);}e->setParameters(p);e->process(b);t+=127;}}}
