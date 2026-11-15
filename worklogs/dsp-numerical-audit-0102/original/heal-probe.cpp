#include <juce_dsp/juce_dsp.h>
#include "AnamorphEngine.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <cmath>
int main(){juce::ScopedNoDenormals d;constexpr int n=64;anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.mix=0;p.outputGainDb=24;p.oversample=anamorph::OversampleFactor::x8;
 auto a=std::make_unique<anamorph::AnamorphEngine>(),b=std::make_unique<anamorph::AnamorphEngine>();for(auto*e:{a.get(),b.get()}){e->primeParameters(p);e->prepare(48000,n);}
 juce::AudioBuffer<float>x(2,n),y(2,n);int lat=a->getLatencySamples();std::printf("stand-in Haas drive=0 lat=%d\n",lat);
 for(int k=0;k<12;++k){for(int i=0;i<n;++i){float l=(k==8&&i==7)?1e38f:.125f;x.setSample(0,i,l);x.setSample(1,i,.25f);y.setSample(0,i,.125f);y.setSample(1,i,.25f);}a->setParameters(p);b->setParameters(p);a->process(x);b->process(y);
 int dif=0,zero=0;double worst=0;for(int i=0;i<n;++i){float q=x.getSample(1,i),r=y.getSample(1,i);dif+=std::memcmp(&q,&r,4)!=0;zero+=q==0;worst=std::max(worst,std::abs((double)q-r));}if(k>=8)std::printf("block=%d spikeOut=%a R_diff=%d R_zero=%d max=%.9g first=%a ref=%a last=%a\n",k,x.getSample(0,7+lat),dif,zero,worst,x.getSample(1,0),y.getSample(1,0),x.getSample(1,n-1));}}
