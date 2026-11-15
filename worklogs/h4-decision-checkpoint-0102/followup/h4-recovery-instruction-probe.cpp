// Nonshipping repeated injected-dry-state recovery cost; not a reachability or timing claim.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <valgrind/callgrind.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
namespace recovery_cost_access {
struct MB { friend auto member(MB); }; struct Banks { friend auto member(Banks); };
struct Active { friend auto member(Active); }; struct S1 { friend auto member(S1); };
struct OS { friend auto member(OS); };
template<class T, auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<MB, &anamorph::AnamorphEngine::multiband>;
template struct Get<Banks, &anamorph::MultibandWidth::bank>;
template struct Get<Active, &anamorph::MultibandWidth::active>;
template struct Get<S1, &anamorph::LR4Xover::s1>;
template struct Get<OS, &anamorph::AnamorphEngine::osRunning>;
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    using namespace recovery_cost_access;
    const bool inject=std::atoi(argv[1])!=0, wrapped=std::atoi(argv[2])!=0;
    constexpr int rate=48000,block=128,measured=256;
    juce::ScopedNoDenormals noDenormals;
    anamorph::EngineParameters p;p.mbEnable=true;p.mbBands=4;p.mix=1.0f;
    p.mbFreqLow=100;p.mbFreqMid=4000;p.mbFreqHigh=10000;
    if(wrapped){p.oversample=anamorph::OversampleFactor::x4;p.driveDb=6;}
    auto e=std::make_unique<anamorph::AnamorphEngine>();e->primeParameters(p);e->prepare(rate,block);
    juce::AudioBuffer<float> b(2,block);unsigned seed=1234567;volatile float sink=0;
    int recovered=0,observedWrapped=0,nonfinite=0;
    for(int k=0;k<128+measured;++k)
    {
        for(int i=0;i<block;++i){seed=1664525u*seed+1013904223u;float x=float(int(seed>>8)-8388608)/8388608.0f*0.35f;b.setSample(0,i,x);b.setSample(1,i,-0.37f*x+float(0.06*std::cos(0.037*(k*block+i))));}
        auto& mb=e.get()->*member(MB{});auto& banks=mb.*member(Banks{});const int active=mb.*member(Active{});
        auto& state=(banks[active].dx[0].*member(S1{}))[0];
        if(inject && k>=128)state=std::numeric_limits<float>::quiet_NaN();
        if(k>=128)CALLGRIND_TOGGLE_COLLECT;
        e->setParameters(p);e->process(b);
        if(k>=128)CALLGRIND_TOGGLE_COLLECT;
        if(k>=128){if(inject && mb.dryChannelFinite(0))++recovered;observedWrapped += e.get()->*member(OS{});for(int ch=0;ch<2;++ch)for(int i=0;i<block;++i)nonfinite += !std::isfinite(b.getSample(ch,i));}
        sink=b.getSample(0,block-1);
    }
    std::printf("%d measured blocks; injected=%d; recovered_blocks=%d; wrapped_blocks=%d; nonfinite_output=%d; checksum=%g\n",measured,inject,recovered,observedWrapped,nonfinite,double(sink));
    return (nonfinite==0 && (!inject || recovered==measured) && observedWrapped==(wrapped?measured:0))?0:1;
}
