// Nonshipping N2-05 boundary experiment. Injected wet state is not a claim of natural reachability.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
using E=anamorph::AnamorphEngine;using M=anamorph::MultibandWidth;
namespace access205 {
struct MB{friend auto member(MB);};struct Banks{friend auto member(Banks);};
struct Active{friend auto member(Active);};struct S1{friend auto member(S1);};
struct Mix{friend auto member(Mix);};
template<class T,auto P>struct Get{friend auto member(T){return P;}};
template struct Get<MB,&E::multiband>;template struct Get<Banks,&M::bank>;
template struct Get<Active,&M::active>;template struct Get<S1,&anamorph::LR4Xover::s1>;
template struct Get<Mix,&E::mixSmooth>;
}
static void fill(juce::AudioBuffer<float>& b,float x){for(int c=0;c<2;++c)for(int i=0;i<b.getNumSamples();++i)b.setSample(c,i,x);}
static bool bits(float a,float b){return std::memcmp(&a,&b,sizeof a)==0;}
int main(){
 using namespace access205;juce::ScopedNoDenormals guard;
 anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.0f;p.width=1.0f;
 p.mbEnable=true;p.mbBands=2;p.mix=0.0f;p.autoGainMatch=false;
 auto clean=std::make_unique<E>(),fault=std::make_unique<E>();
 for(auto* e:{clean.get(),fault.get()}){e->primeParameters(p);e->prepare(48000,64);}
 juce::AudioBuffer<float>a(2,64),b(2,64);for(int j=0;j<12;++j){fill(a,.25f);fill(b,.25f);clean->process(a);fault->process(b);}
 auto& mb=fault.get()->*member(MB{});auto& banks=mb.*member(Banks{});const int active=mb.*member(Active{});
 auto& state=(banks[active].x[0].*member(S1{}))[0];state=std::numeric_limits<float>::quiet_NaN();
 if(std::isfinite(state)){std::puts("injection-failed");return 2;}
 fill(a,.25f);fill(b,.25f);clean->process(a);fault->process(b);
 int firstZeros=0,firstDry=0;for(int c=0;c<2;++c)for(int i=0;i<64;++i){firstZeros+=bits(b.getSample(c,i),0.0f);firstDry+=bits(b.getSample(c,i),.25f);}
 const bool badFirst=!std::isfinite(state);
 fill(a,.25f);fill(b,.25f);clean->process(a);fault->process(b);
 const bool badNext=!std::isfinite(state);int nextDry=0;for(int c=0;c<2;++c)for(int i=0;i<64;++i)nextDry+=bits(b.getSample(c,i),.25f);
 p.mix=.5f;clean->setParameters(p);fault->setParameters(p);fill(a,.25f);fill(b,.25f);clean->process(a);fault->process(b);
 int returnZeros=0,cleanZeros=0;for(int c=0;c<2;++c)for(int i=0;i<64;++i){returnZeros+=bits(b.getSample(c,i),0.0f);cleanZeros+=bits(a.getSample(c,i),0.0f);}
 const bool advancing=(fault.get()->*member(Mix{})).getCurrentValue()>0.0f;
 std::printf("injected=1 first_dry=%d first_zero=%d wet_bad_after_fault=%d wet_bad_next=%d next_dry=%d reentry_zeros=%d clean_zeros=%d mix_advancing=%d wet_bad_after_reentry=%d\n",firstDry,firstZeros,int(badFirst),int(badNext),nextDry,returnZeros,cleanZeros,int(advancing),int(!std::isfinite(state)));
 return advancing?0:3;
}
