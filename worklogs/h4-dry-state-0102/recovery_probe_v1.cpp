// Candidate-only stress: an unused dry bank must not retain invalid state
// indefinitely after otherwise finite wet audio resumes. No product fix.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
using E=anamorph::AnamorphEngine;
namespace recovery_access
{
struct MB{friend auto member(MB);};struct Banks{friend auto member(Banks);};
struct S1{friend auto member(S1);};struct S2{friend auto member(S2);};
struct S3{friend auto member(S3);};struct S4{friend auto member(S4);};
template<class T,auto P>struct Get{friend auto member(T){return P;}};
template struct Get<MB,&E::multiband>;
template struct Get<Banks,&anamorph::MultibandWidth::bank>;
template struct Get<S1,&anamorph::LR4Xover::s1>;template struct Get<S2,&anamorph::LR4Xover::s2>;
template struct Get<S3,&anamorph::LR4Xover::s3>;template struct Get<S4,&anamorph::LR4Xover::s4>;
}
int bad(E& e)
{
    using namespace recovery_access;
    auto& banks=(e.*member(MB{})).*member(Banks{});int n=0;
    for(auto& bank:banks)for(auto& x:bank.dx)for(int c=0;c<2;++c)
        n+=!std::isfinite((x.*member(S1{}))[c])+!std::isfinite((x.*member(S2{}))[c])
          +!std::isfinite((x.*member(S3{}))[c])+!std::isfinite((x.*member(S4{}))[c]);
    return n;
}
int main()
{
    juce::ScopedNoDenormals noDenormals;
    std::puts("cutoff,finite_extreme_inputs,invalid_after_extreme,invalid_after_120_valid_blocks,wet_peak_valid,first_dip_zero_samples,invalid_after_dip");
    for(float cutoff:{20.0f,800.0f,8000.0f})
    {
        constexpr int block=512;
        anamorph::EngineParameters p;p.width=0;p.mbEnable=true;p.mbBands=2;p.mbFreqLow=cutoff;
        auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,block);
        anamorph::LR4Xover impulse;impulse.prepare(48000);impulse.setCutoffFrequency(cutoff);
        std::array<float,block> h{};
        for(int i=0;i<block;++i){float lo,hi;impulse.processSample(0,i==0?1.0f:0.0f,lo,hi);h[i]=lo+hi;}
        juce::AudioBuffer<float> b(2,block);bool finite=true;
        for(int k=0;k<4;++k)
        {
            for(int i=0;i<block;++i){const float x=std::copysign(1.68e38f,h[block-1-i]);finite &= std::isfinite(x)&&std::isfinite(x-(-x));b.setSample(0,i,x);b.setSample(1,i,-x);}
            e->process(b);
        }
        const int afterExtreme=bad(*e);double peak=0;
        for(int k=0;k<120;++k)
        {for(int c=0;c<2;++c)for(int i=0;i<block;++i)b.setSample(c,i,0.2f);e->process(b);for(int i=0;i<block;++i)peak=std::max(peak,std::abs(double(b.getSample(0,i))));}
        const int afterValid=bad(*e);
        p.mix=0.5f;e->setParameters(p);for(int c=0;c<2;++c)for(int i=0;i<block;++i)b.setSample(c,i,0.2f);e->process(b);
        int zero=0;for(int i=0;i<block;++i)zero+=b.getSample(0,i)==0;
        std::printf("%g,%d,%d,%d,%.9g,%d,%d\n",cutoff,finite,afterExtreme,afterValid,peak,zero,bad(*e));
    }
}
