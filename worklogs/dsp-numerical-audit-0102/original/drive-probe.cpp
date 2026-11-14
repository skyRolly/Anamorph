#include <juce_dsp/juce_dsp.h>
#include "AnamorphEngine.h"
#include <cstdio>
#include <memory>
int main(){juce::ScopedNoDenormals d;for(float x:{.25f,1e8f,-1e8f}){anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.driveDb=6;p.mix=1;p.mbEnable=true;p.mbBands=1;auto e=std::make_unique<anamorph::AnamorphEngine>();e->primeParameters(p);e->prepare(48000,64);juce::AudioBuffer<float>b(2,64);for(int c=0;c<2;++c)for(int i=0;i<64;++i)b.setSample(c,i,x);e->process(b);std::printf("fullwet-gated one-band drive6 input=%a out=%a\n",x,b.getSample(0,63));}}
