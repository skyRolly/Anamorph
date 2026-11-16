// Quantify finite-history reconstruction; no production replay is proposed here.
#include "dsp/LR4Xover.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>
int main()
{
    juce::ScopedNoDenormals noDenormals;
    std::puts("rate,moving,noise,history,peak_first960,replay_us,normal128_us,history_bytes_per_stereo_three_split_sample");
    for(int rate:{48000,192000})for(bool moving:{false,true})for(bool noise:{false,true})
    {
        const int idle=rate*3;std::vector<float> x(idle+960),f(x.size()),expected(960);
        unsigned seed=6901;anamorph::LR4Xover reference;reference.prepare(rate);
        for(int i=0;i<idle+960;++i)
        {
            seed=1664525u*seed+1013904223u;
            x[i]=noise?(float(int(seed>>8)-8388608)/8388608.0f)*0.5f:float(0.5*std::sin(juce::MathConstants<double>::twoPi*20*i/rate));
            f[i]=moving?20.0f*std::exp2(float(0.2*(1+std::sin(double(i)/rate)))):20.0f;
            reference.setCutoffFrequency(f[i]);float lo,hi;reference.processSample(0,x[i],lo,hi);
            if(i>=idle)expected[i-idle]=lo+hi;
        }
        for(int length:{512,4096,16384,65536})
        {
            anamorph::LR4Xover replay;replay.prepare(rate);
            // Replay every retained coefficient too. This is a stronger reference
            // than replaying input under the current coefficient, yet is not exact.
            const auto start=std::chrono::steady_clock::now();
            float lo,hi;for(int i=idle-length;i<idle;++i)
            {replay.setCutoffFrequency(f[i]);replay.processSample(0,x[i],lo,hi);}
            const auto end=std::chrono::steady_clock::now();
            double peak=0;const auto normalStart=std::chrono::steady_clock::now();
            for(int i=idle;i<idle+960;++i)
            {replay.setCutoffFrequency(f[i]);replay.processSample(0,x[i],lo,hi);peak=std::max(peak,std::abs(double(lo+hi)-expected[i-idle]));}
            const auto normalEnd=std::chrono::steady_clock::now();
            const double us=std::chrono::duration<double,std::micro>(end-start).count();
            const double normal=std::chrono::duration<double,std::micro>(normalEnd-normalStart).count()*128/960;
            std::printf("%d,%d,%d,%d,%.12g,%.9g,%.9g,20\n",rate,moving,noise,length,peak,us,normal);
        }
    }
}
