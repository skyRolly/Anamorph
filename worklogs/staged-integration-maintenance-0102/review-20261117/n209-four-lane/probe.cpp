// Nonshipping, deliberately injected call-site observation. The test hook and
// optional widening exist only in the diagnostic engine translation unit.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
namespace anamorph {
using N209DiagnosticHook = void (*) (int,int,float,float,float,float,float) noexcept;
extern N209DiagnosticHook n209DiagnosticHook;
extern int n209DiagnosticLane, n209DiagnosticMode, n209DiagnosticMask;
}
using E=anamorph::AnamorphEngine;
namespace n209_access {
struct OS { friend auto member(OS); };
struct Running { friend auto member(Running); };
struct Write { friend auto member(Write); };
struct Align { friend auto member(Align); };
struct Reference { friend auto member(Reference); };
template<class T,auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<OS,&E::osRunning>;
template struct Get<Running,&E::mbRunning>;
template struct Get<Write,&E::dryDelayWrite>;
template struct Get<Align,&E::dryAlignDelayBuffer>;
template struct Get<Reference,&E::loudnessRefScratch>;
}
static bool bits(float a,float b){return std::memcmp(&a,&b,sizeof a)==0;}
struct Stats {
 int injected=0,oldOverflow=0,repaired=0,wrongRepair=0,ordinaryMismatch=0;
 int invalid=0,invalidChanged=0,zero=0,one=0,endpointBad=0;
 bool blockInjected=false;float blockResult=0;
} stats;
static void observe(int lane,int i,float from,float to,float b,float old,float result) noexcept {
 using namespace anamorph;
 const bool selected=i==1 && lane==n209DiagnosticLane && b>0.0f && b<1.0f;
 if(selected && n209DiagnosticMode==1) {
  ++stats.injected;stats.blockInjected=true;stats.blockResult=result;
  if(!std::isfinite(old))++stats.oldOverflow;
  const float expected=(float)((double)from+(double)b*((double)to-(double)from));
  if(std::isfinite(result)&&bits(result,expected))++stats.repaired;else ++stats.wrongRepair;
 } else if(selected && n209DiagnosticMode==2) {
  ++stats.invalid;
  if(std::isfinite(old)||std::isfinite(result)||(std::isnan(old)!=std::isnan(result)))++stats.invalidChanged;
 } else if(std::isfinite(old) && !bits(old,result)) ++stats.ordinaryMismatch;
 if(b==0.0f){++stats.zero;if(!bits(result,from))++stats.endpointBad;}
 if(b==1.0f){++stats.one;if(!bits(result,to))++stats.endpointBad;}
}
int main(int argc,char** argv){
 using namespace n209_access;
 const std::string mode=argc>1?argv[1]:"positive";
 int skip=-1;
 if(mode!="positive"){
  if(mode.size()!=6||mode.rfind("skip-",0)!=0||mode[5]<'0'||mode[5]>'3')return 2;
  skip=mode[5]-'0';
 }
 int checks=0,failures=0,history=0,ref=0,forced=0;
 auto check=[&](bool ok,const char* msg){++checks;if(!ok){++failures;std::printf("FAIL %s\n",msg);}};
 juce::ScopedNoDenormals nd;
 for(bool wrapped:{false,true}) for(int lane=0;lane<4;++lane){
  stats=Stats{};int laneHistory=0,laneRef=0;anamorph::n209DiagnosticHook=nullptr;anamorph::n209DiagnosticLane=-1;
  anamorph::n209DiagnosticMode=0;anamorph::n209DiagnosticMask=15;
  anamorph::EngineParameters p;p.algorithm=anamorph::Algorithm::Haas;p.algoAmount=0.0f;
  p.mbEnable=true;p.mbBands=2;p.mbFreqLow=1000.0f;p.mbFreqMid=2000.0f;p.mbFreqHigh=4000.0f;p.mix=.4f;p.driveDb=6.0f;
  if(wrapped)p.oversample=anamorph::OversampleFactor::x4;
  auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,128);
  juce::AudioBuffer<float> audio(2,128);
  auto fill=[&](){for(int i=0;i<128;++i){audio.setSample(0,i,.125f);audio.setSample(1,i,.25f);}};
  for(int k=0;k<8;++k){fill();e->process(audio);}
  check((e.get()->*member(OS{}))==wrapped && e->getLatencySamples()==(wrapped?6:0),"actual selected wrapping and latency");
  p.mbEnable=false;e->setParameters(p);
  anamorph::n209DiagnosticLane=lane;anamorph::n209DiagnosticMode=1;
  anamorph::n209DiagnosticMask=15 & ~(skip==lane?(1<<lane):0);
  anamorph::n209DiagnosticHook=observe;
  for(int block=0;block<5;++block){
   fill();stats.blockInjected=false;
   const int start=e.get()->*member(Write{});e->process(audio);
   if(stats.blockInjected && lane>=2 && skip!=lane){
    auto& ring=e.get()->*member(Align{});auto& reference=e.get()->*member(Reference{});
    const int ch=lane-2,write=(start+1)%ring.getNumSamples();
    const int read=(write-e->getLatencySamples()+ring.getNumSamples())%ring.getNumSamples();
    if(bits(ring.getSample(ch,write),stats.blockResult)){++history;++laneHistory;}
    if(bits(reference.getSample(ch,1),ring.getSample(ch,read))){++ref;++laneRef;}
   }
  }
  anamorph::n209DiagnosticHook=nullptr;
  check(stats.injected>0 && stats.oldOverflow==stats.injected,"actual selected call site receives finite overflowing operands");
  check(stats.repaired==stats.injected && stats.wrongRepair==0,"candidate repairs selected finite interior call site");
  check(stats.ordinaryMismatch==0 && stats.endpointBad==0,"ordinary finite results and observed endpoints unchanged");
  forced+=stats.injected;
  if(lane>=2 && skip!=lane)check(laneHistory==stats.injected&&laneRef==stats.injected,"finite aligned history and latency-offset reference observed");
 }
 // Separately force an originally non-finite operand into every actual lane.
 int invalidObserved=0;
 for(int lane=0;lane<4;++lane){
  stats=Stats{};anamorph::n209DiagnosticHook=nullptr;anamorph::n209DiagnosticLane=-1;
  anamorph::n209DiagnosticMode=0;anamorph::n209DiagnosticMask=15;
  anamorph::EngineParameters p;p.mbEnable=true;p.mbBands=2;p.mix=.4f;
  auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,128);
  juce::AudioBuffer<float> a(2,128);for(int k=0;k<4;++k){a.clear();e->process(a);}
  const bool wasRunning=e.get()->*member(Running{});
  p.mbEnable=false;e->setParameters(p);anamorph::n209DiagnosticLane=lane;
  anamorph::n209DiagnosticMode=2;anamorph::n209DiagnosticHook=observe;a.clear();e->process(a);
  anamorph::n209DiagnosticHook=nullptr;
  invalidObserved+=stats.invalid;
  check(stats.invalid>0&&stats.invalidChanged==0,"non-finite operands retain original invalid classification");
  bool finiteOutput=true;for(int c=0;c<2;++c)for(int i=0;i<128;++i)finiteOutput &= std::isfinite(a.getSample(c,i));
  check(wasRunning && !(e.get()->*member(Running{})) && finiteOutput,
        "invalid operand reaches existing final recovery and finite output");
 }
 // Observe terminal endpoints in real disable and enable ramps without injection.
 stats=Stats{};anamorph::n209DiagnosticLane=-1;anamorph::n209DiagnosticMode=0;anamorph::n209DiagnosticMask=15;
 anamorph::EngineParameters p;p.mbEnable=true;p.mbBands=2;p.mix=.4f;
 auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,128);
 juce::AudioBuffer<float> a(2,128);for(int k=0;k<4;++k){a.clear();e->process(a);}
 anamorph::n209DiagnosticHook=observe;p.mbEnable=false;e->setParameters(p);
 for(int k=0;k<6;++k){a.clear();e->process(a);}
 p.mbEnable=true;e->setParameters(p);for(int k=0;k<6;++k){a.clear();e->process(a);}
 anamorph::n209DiagnosticHook=nullptr;
 check(stats.zero>0&&stats.one>0&&stats.endpointBad==0,"actual terminal Enable endpoints preserve source/destination bits");
 check(stats.ordinaryMismatch==0,"finite unmodified float results retain their bits");
 std::printf("forced=%d history=%d reference=%d invalid=%d terminal_zero=%d terminal_one=%d\n",forced,history,ref,invalidObserved,stats.zero,stats.one);
 std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
