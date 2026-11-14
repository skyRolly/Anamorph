// Nonshipping deterministic whole-engine instruction attribution.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <valgrind/callgrind.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

int main(int argc,char** argv)
{
    if(argc!=7)return 2;
    juce::ScopedNoDenormals noDenormals;
    const int rate=std::atoi(argv[1]),block=std::atoi(argv[2]),bands=std::atoi(argv[3]);
    const bool moving=std::atoi(argv[4])!=0,os=std::atoi(argv[5])!=0;
    anamorph::EngineParameters p;p.mbEnable=true;p.mbBands=bands;p.mix=std::atof(argv[6]);
    p.mbFreqLow=100;
    if(os){p.oversample=anamorph::OversampleFactor::x4;p.driveDb=6;}
    auto e=std::make_unique<anamorph::AnamorphEngine>();e->primeParameters(p);e->prepare(rate,block);
    juce::AudioBuffer<float> b(2,block);unsigned seed=1234567;volatile float sink=0;
    constexpr int measured=256;
    for(int k=0;k<128+measured;++k)
    {
        for(int i=0;i<block;++i)
        {
            seed=1664525u*seed+1013904223u;const float x=float(int(seed>>8)-8388608)/8388608.0f*0.35f;
            b.setSample(0,i,x);b.setSample(1,i,-0.37f*x+float(0.06*std::cos(0.037*(k*block+i))));
        }
        if(moving)p.mbFreqLow=100*std::exp2(float(0.3*std::sin(double(k*block)/rate)));
        if(k>=128)CALLGRIND_TOGGLE_COLLECT;
        e->setParameters(p);e->process(b);
        if(k>=128)CALLGRIND_TOGGLE_COLLECT;
        sink=b.getSample(0,block-1);
    }
    std::printf("%d measured blocks; checksum %g\n",measured,double(sink));
}
