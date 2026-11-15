#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
using E=anamorph::AnamorphEngine;
namespace h4_access {
struct MB {friend auto member(MB);}; struct Banks {friend auto member(Banks);};
struct S1 {friend auto member(S1);};struct S2 {friend auto member(S2);};struct S3 {friend auto member(S3);};struct S4 {friend auto member(S4);};
struct Aligned {friend auto member(Aligned);};
template<class T,auto P> struct Get{friend auto member(T){return P;}};
template struct Get<MB,&E::multiband>;template struct Get<Banks,&anamorph::MultibandWidth::bank>;
template struct Get<S1,&anamorph::LR4Xover::s1>;template struct Get<S2,&anamorph::LR4Xover::s2>;
template struct Get<S3,&anamorph::LR4Xover::s3>;template struct Get<S4,&anamorph::LR4Xover::s4>;
template struct Get<Aligned,&E::dryAlignScratch>;
}
int bad(E& e,int c) {using namespace h4_access; auto& banks=(e.*member(MB{})).*member(Banks{});int n=0;
 for(auto& bank:banks)for(auto& x:bank.dx)n+=!std::isfinite((x.*member(S1{}))[c])+!std::isfinite((x.*member(S2{}))[c])+!std::isfinite((x.*member(S3{}))[c])+!std::isfinite((x.*member(S4{}))[c]);return n;}
int main(int argc,char** argv){juce::ScopedNoDenormals noDenormals; constexpr int n=128; anamorph::EngineParameters p;p.width=0;p.mbEnable=true;p.mbBands=2;p.mbFreqLow=17851.238f;p.mbFreqMid=19636.36f;p.mbFreqHigh=21600.0f;p.mix=1.0f;
 auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,n);juce::AudioBuffer<float> b(2,n);
 for(int k=0;k<8;++k){b.clear();e->process(b);} b.clear();b.setSample(0,n-1,std::numeric_limits<float>::max()*0.95f);e->process(b);
 const float dry=(e.get()->*member(h4_access::Aligned{})).getSample(0,n-1); const int invalid=bad(*e,0);const int right=bad(*e,1);float wet=b.getSample(0,n-1);
 std::printf("after gated extreme dry=%g finite=%d wet=%g wetfinite=%d left_invalid=%d right_invalid=%d\n",dry,std::isfinite(dry),wet,std::isfinite(wet),invalid,right);
 if(argc>1 && argv[1][0]=='r') (e.get()->*member(h4_access::MB{})).resetDryChannel(0);
 if(!(argc>1 && argv[1][0]=='n')) p.mix=.5f; e->setParameters(p);b.clear();for(int i=0;i<n;++i){b.setSample(0,i,.2f);b.setSample(1,i,.1f);}e->process(b);int zero=0;for(int i=0;i<n;++i)zero+=b.getSample(0,i)==0;
 std::printf("dip left_zero=%d left_invalid_after=%d right_invalid_after=%d firstL=%g firstR=%g\n",zero,bad(*e,0),bad(*e,1),b.getSample(0,0),b.getSample(1,0));
 return 0;
}
