// Linux-only nonshipping CPU-time batch check for noisy shared wall clocks.
// All variants include identical input copies and parameter adoption in timing.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <time.h>
#include <vector>
std::int64_t cpuNow()
{
    timespec t{};if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)!=0)std::abort();
    return std::int64_t(t.tv_sec)*1000000000+t.tv_nsec;
}
int main()
{
    juce::ScopedNoDenormals noDenormals;
    std::puts("rate,block,bands,moving,os,mix,rep,cpu_ns_sample,wall_ns_sample,checksum");
    volatile float sink=0;
    for(int rate:{48000,192000})for(int block:{32,256})for(int bands:{2,3,4})
    for(bool moving:{false,true})for(int mode=0;mode<3;++mode)for(int rep=0;rep<5;++rep)
    {
        anamorph::EngineParameters p;p.mbEnable=true;p.mbBands=bands;p.mbFreqLow=20;p.mbFreqMid=800;p.mbFreqHigh=4000;
        p.mix=mode==1?0.5f:1.0f;
        if(mode==2){p.oversample=anamorph::OversampleFactor::x4;p.driveDb=6;}
        auto e=std::make_unique<anamorph::AnamorphEngine>();e->primeParameters(p);e->prepare(rate,block);
        juce::AudioBuffer<float> b(2,block);unsigned seed=1234567;int time=0;
        auto fill=[&](float* left,float* right)
        {
            for(int i=0;i<block;++i)
            {seed=1664525u*seed+1013904223u;const float x=float(int(seed>>8)-8388608)/8388608.0f*0.35f;left[i]=x;right[i]=-0.37f*x+float(0.06*std::cos(0.037*(time+i)));}
        };
        auto update=[&]{if(moving)p.mbFreqLow=100*std::exp2(float(0.3*std::sin(double(time)/rate)));};
        for(int k=0;k<rate/block;++k,time+=block)
        {fill(b.getWritePointer(0),b.getWritePointer(1));update();e->setParameters(p);e->process(b);}
        const int count=std::max(512,rate/(3*block));
        std::vector<float> input(std::size_t(count*block*2));
        std::vector<anamorph::EngineParameters> snapshots;snapshots.reserve(count);
        for(int k=0;k<count;++k,time+=block)
        {fill(input.data()+k*block*2,input.data()+k*block*2+block);update();snapshots.push_back(p);}
        const auto wall=std::chrono::steady_clock::now();const auto cpu=cpuNow();
        for(int k=0;k<count;++k)
        {
            b.copyFrom(0,0,input.data()+k*block*2,block);b.copyFrom(1,0,input.data()+k*block*2+block,block);
            e->setParameters(snapshots[std::size_t(k)]);e->process(b);sink=b.getSample(0,block-1);
        }
        const auto cpuEnd=cpuNow();const auto wallEnd=std::chrono::steady_clock::now();
        std::printf("%d,%d,%d,%d,%d,%g,%d,%.9g,%.9g,%g\n",rate,block,bands,moving,mode==2,mode==1?0.5:1.0,rep,
            double(cpuEnd-cpu)/(count*block),std::chrono::duration<double,std::nano>(wallEnd-wall).count()/(count*block),double(sink));
    }
}
