// Nonshipping candidate probe. State injections establish sensitivity, not reachability.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>
using E=anamorph::AnamorphEngine;using M=anamorph::MultibandWidth;
namespace h4check {
struct MB{friend auto member(MB);};struct Banks{friend auto member(Banks);};
struct Active{friend auto member(Active);};struct Fading{friend auto member(Fading);};
struct Mask{friend auto member(Mask);};struct S1{friend auto member(S1);};
struct S2{friend auto member(S2);};struct S3{friend auto member(S3);};struct S4{friend auto member(S4);};
struct OS{friend auto member(OS);};struct Mix{friend auto member(Mix);};
struct DryRing{friend auto member(DryRing);};struct AlignRing{friend auto member(AlignRing);};
struct RawRing{friend auto member(RawRing);};struct Write{friend auto member(Write);};
template<class T,auto P>struct Get{friend auto member(T){return P;}};
template struct Get<MB,&E::multiband>;template struct Get<Banks,&M::bank>;
template struct Get<Active,&M::active>;template struct Get<Fading,&M::fading>;template struct Get<Mask,&M::lastDryBankMask>;
template struct Get<S1,&anamorph::LR4Xover::s1>;template struct Get<S2,&anamorph::LR4Xover::s2>;
template struct Get<S3,&anamorph::LR4Xover::s3>;template struct Get<S4,&anamorph::LR4Xover::s4>;
template struct Get<OS,&E::osRunning>;template struct Get<Mix,&E::mixSmooth>;
template struct Get<DryRing,&E::dryDelayBuffer>;template struct Get<AlignRing,&E::dryAlignDelayBuffer>;
template struct Get<RawRing,&E::bypassDelayBuffer>;template struct Get<Write,&E::dryDelayWrite>;
}
static bool same(float a,float b){return std::memcmp(&a,&b,sizeof a)==0;}
static bool sameBuffer(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,int first,int last){
 for(int c=first;c<last;++c)for(int i=0;i<a.getNumSamples();++i)if(!same(a.getSample(c,i),b.getSample(c,i)))return false;return true;}
static void moduleBlock(M& m,int n){std::array<float,128> l{},r{},dl{},dr{},a{},b{};for(int i=0;i<n;++i){l[i]=.2f;r[i]=-.1f;dl[i]=.12f;dr[i]=-.07f;}m.processBlock(l.data(),r.data(),n,dl.data(),dr.data(),a.data(),b.data());}
static void fill(juce::AudioBuffer<float>& b,int block){for(int i=0;i<b.getNumSamples();++i){float t=float(block*b.getNumSamples()+i);b.setSample(0,i,.18f*std::sin(.017f*t));b.setSample(1,i,.11f*std::cos(.013f*t));}}
int main(){using namespace h4check;juce::ScopedNoDenormals ftz;int checks=0,failures=0;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
 const float bad=std::numeric_limits<float>::quiet_NaN();
 for(int bands=1;bands<=4;++bands){M m;m.setBandCount(bands);m.setCrossovers(100,800,3000);m.prepare(48000,128);moduleBlock(m,128);
  auto& banks=m.*member(Banks{});int a=m.*member(Active{}), count=0;
  for(int i=0;i<bands-1;++i){for(auto ptr:{member(S1{}),member(S2{}),member(S3{}),member(S4{})}){auto& x=(banks[a].dx[i].*ptr)[0];float old=x;x=bad;count+=!m.dryChannelFinite(0);x=old;}
   if(i>0)for(auto ptr:{member(S1{}),member(S2{})}){auto& x=(banks[a].dax[i].*ptr)[0];float old=x;x=bad;count+=!m.dryChannelFinite(0);x=old;}}
  const int expected[]={0,4,10,16};check(count==expected[bands-1],"consumed state count");
  if(bands>1){auto& x=(banks[1-a].dx[0].*member(S1{}))[0];float old=x;x=bad;check(m.dryChannelFinite(0),"dormant bank excluded");x=old;
   auto& y=(banks[a].dax[0].*member(S1{}))[0];old=y;y=bad;check(m.dryChannelFinite(0),"allpass zero excluded");y=old;}
  if(bands>2){auto& x=(banks[a].dax[1].*member(S3{}))[0];float old=x;x=bad;check(m.dryChannelFinite(0),"unused second section excluded");x=old;}
  std::printf("bands=%d relevant_states=%d\n",bands,count);
 }
 {M m;m.setBandCount(3);m.setCrossovers(100,800,3000);m.prepare(48000,128);moduleBlock(m,128);m.setCrossovers(1000,5000,9000);moduleBlock(m,128);
  check(m.*member(Fading{}),"actual crossover fade started");check((m.*member(Mask{}))==3,"both fade banks recorded");
  auto& banks=m.*member(Banks{});for(int b=0;b<2;++b){auto& x=(banks[b].dx[0].*member(S1{}))[0];float old=x;x=bad;check(!m.dryChannelFinite(0),"each live fade bank checked");check(m.dryChannelFinite(1),"other channel remains finite");x=old;}
  while(m.*member(Fading{}))moduleBlock(m,128);
  check((m.*member(Mask{}))==3,"retired fade source remains checked for finishing block");
  int old=1-(m.*member(Active{}));auto& x=(banks[old].dx[0].*member(S1{}))[0];float prior=x;x=bad;check(!m.dryChannelFinite(0),"retired source state checked after fade");x=prior;
 }
 for(bool wrapped:{false,true}){constexpr int n=128;anamorph::EngineParameters p;p.mbEnable=true;p.mbBands=2;p.mix=1.0f;p.autoGainMatch=false;p.monoMakerEnable=false;if(wrapped){p.oversample=anamorph::OversampleFactor::x4;p.driveDb=6;}
  auto clean=std::make_unique<E>(),fault=std::make_unique<E>();for(E* e:{clean.get(),fault.get()}){e->primeParameters(p);e->prepare(48000,n);}
  juce::AudioBuffer<float>a(2,n),b(2,n);for(int k=0;k<12;++k){fill(a,k);fill(b,k);clean->process(a);fault->process(b);}
  check((fault.get()->*member(OS{}))==wrapped,"wrapped path engagement");check(wrapped?fault->getLatencySamples()>0:fault->getLatencySamples()==0,"latency premise");
  auto& mm=(fault.get()->*member(MB{}));auto& banks=mm.*member(Banks{});int active=mm.*member(Active{});
  auto& poison=(banks[active].dx[0].*member(S1{}))[0];poison=bad;
  p.mix=.5f;clean->setParameters(p);fault->setParameters(p);check((fault.get()->*member(Mix{})).isSmoothing(),"first consuming Mix ramp");
  auto& align=fault.get()->*member(AlignRing{});int write=fault.get()->*member(Write{}),size=align.getNumSamples();std::vector<float> prior(static_cast<size_t>(size));std::vector<bool> touched(static_cast<size_t>(size));
  for(int i=0;i<size;++i)prior[static_cast<size_t>(i)]=align.getSample(0,i);for(int i=0;i<n;++i)touched[static_cast<size_t>((write+i)%size)]=true;
  fill(a,12);fill(b,12);clean->process(a);fault->process(b);
  bool finite=true,rightSame=true,nonzero=false;for(int i=0;i<n;++i){finite &= std::isfinite(b.getSample(0,i))&&std::isfinite(b.getSample(1,i));rightSame &= same(a.getSample(1,i),b.getSample(1,i));nonzero |= !same(b.getSample(0,i),0.0f);}
  check(finite&&rightSame&&nonzero,"first consuming output finite and opposite output preserved");
  auto& cm=clean.get()->*member(MB{});auto& cb=cm.*member(Banks{});auto& fb=mm.*member(Banks{});int ca=cm.*member(Active{}),fa=mm.*member(Active{});
  check(same((cb[ca].x[0].*member(S1{}))[1],(fb[fa].x[0].*member(S1{}))[1]),"valid opposite wet state preserved");
  check(sameBuffer(clean.get()->*member(DryRing{}),fault.get()->*member(DryRing{}),0,2),"conditioned history preserved");
  check(sameBuffer(clean.get()->*member(RawRing{}),fault.get()->*member(RawRing{}),0,2),"raw history preserved");
  check(sameBuffer(clean.get()->*member(AlignRing{}),fault.get()->*member(AlignRing{}),1,2),"opposite aligned history preserved");
  bool untouched=true;int retained=0;for(int i=0;i<size;++i)if(!touched[static_cast<size_t>(i)]){++retained;untouched &= same(prior[static_cast<size_t>(i)],align.getSample(0,i));}
  check(retained>0&&untouched,"unwritten aligned history preserved");check(mm.dryChannelFinite(0),"affected channel state recovered");
  std::printf("engine=%s latency=%d retained_aligned=%d finite=%d right_equal=%d\n",wrapped?"x4-drive":"off",fault->getLatencySamples(),retained,finite,rightSame);
 }
 std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;}
