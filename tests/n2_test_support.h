#pragma once
#include <cstring>

namespace n2_tests
{
using Engine = anamorph::AnamorphEngine;
using Params = anamorph::EngineParameters;
// Explicit-instantiation access exemption, confined to nonshipping tests. No class/layout hook.
struct Bypass { using type = juce::SmoothedValue<float> Engine::*; friend type member(Bypass); };
struct Multiband { using type = juce::SmoothedValue<float> Engine::*; friend type member(Multiband); };
struct Phase { using type = float Engine::*; friend type member(Phase); };
struct Forced { using type = bool Engine::*; friend type member(Forced); };
template<class Tag, typename Tag::type P> struct Access { friend typename Tag::type member(Tag) { return P; } };
template struct Access<Bypass, &Engine::bypassBlend>;
template struct Access<Multiband, &Engine::mbEnableBlend>;
template struct Access<Phase, &Engine::switchPhase>;
template struct Access<Forced, &Engine::pendingForced>;
inline auto& blend(Engine& e, bool mb) { return mb ? e.*member(Multiband{}) : e.*member(Bypass{}); }
inline bool value(const Params& p, bool mb) { return mb ? p.mbEnable : p.bypass; }
inline void target(Params& p, bool mb, bool on) { if (mb) p.mbEnable = on; else p.bypass = on; }
inline bool bits(float x, float y) { return std::memcmp(&x,&y,sizeof x)==0; }
inline Params source(bool mb, bool on)
{
    Params p; p.algorithm=anamorph::Algorithm::Haas; p.algoAmount=0.65f; p.width=1.8f;
    p.mbWidthLow=0.2f; p.mbWidthMid=1.8f; p.mbWidthHiMid=0.35f; p.mbWidthHigh=1.7f;
    target(p,mb,on); return p;
}
inline void fill(juce::AudioBuffer<float>& b, int time)
{
    for (int i=0;i<b.getNumSamples();++i)
    {
        const float t=static_cast<float>(time+i);
        b.setSample(0,i,0.42f*std::sin(0.137f*t)+0.19f*std::cos(0.037f*t));
        b.setSample(1,i,0.11f*std::cos(0.091f*t)-0.24f*std::sin(0.057f*t));
    }
}
inline double difference(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b)
{
    double d=0;
    for(int c=0;c<2;++c) for(int i=0;i<a.getNumSamples();++i)
        d=std::max(d,std::abs(static_cast<double>(a.getSample(c,i))-b.getSample(c,i)));
    return d;
}
}
