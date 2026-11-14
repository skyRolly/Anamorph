#include "PluginProcessor.h"
#include "/workspace/anamorph-audit-113b9189/source/tests/state_filesystem.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdio>
#include <memory>

static void set(AnamorphAudioProcessor& p,const char* id,float norm) {
 auto* param=p.getAPVTS().getParameter(id); if(!param){std::fprintf(stderr,"missing parameter %s\n",id);std::abort();}
 param->setValueNotifyingHost(norm);
}
static void init(AnamorphAudioProcessor& p) {
 set(p,pid::advancedMode,1);set(p,pid::algorithm,0);set(p,pid::amount,.65f);set(p,pid::width,.9f);set(p,pid::mbEnable,0);
 set(p,pid::mbWidthLow,.1f);set(p,pid::mbWidthMid,.9f);set(p,pid::mbWidthHiMid,.175f);set(p,pid::mbWidthHigh,.85f);
 p.setRateAndBufferSizeDetails(48000,64);p.prepareToPlay(48000,64);
}
static void fill(juce::AudioBuffer<float>& b,int t) {for(int i=0;i<64;++i){float x=(float)(t+i);b.setSample(0,i,.42f*std::sin(.137f*x)+.19f*std::cos(.037f*x));b.setSample(1,i,.11f*std::cos(.091f*x)-.24f*std::sin(.057f*x));}}
static void run(AnamorphAudioProcessor& p,juce::AudioBuffer<float>& b,juce::MidiBuffer& m,int t){fill(b,t);p.processBlock(b,m);}
int main(){state_test_filesystem::Profile profile;if(!profile.initialise(juce::File::getSpecialLocation(juce::File::tempDirectory)))return 2;juce::ScopedJuceInitialiser_GUI juceInit; for(int kind=0;kind<2;++kind){
 auto a=std::make_unique<AnamorphAudioProcessor>(),b=std::make_unique<AnamorphAudioProcessor>(),c=std::make_unique<AnamorphAudioProcessor>(),d=std::make_unique<AnamorphAudioProcessor>();
 for(auto* p:{a.get(),b.get(),c.get(),d.get()})init(*p);
 juce::AudioBuffer<float> aa(2,64),bb(2,64),cc(2,64),dd(2,64);juce::MidiBuffer midi;int t=0;
 for(int k=0;k<30;++k,t+=64){run(*a,aa,midi,t);run(*b,bb,midi,t);run(*c,cc,midi,t);run(*d,dd,midi,t);}
 set(*a,pid::algorithm,1.0f/3.0f);set(*b,pid::algorithm,1.0f/3.0f);
 double ab=0,cd=0,duck=0;int nab=0,ncd=0;float onRead=0,offRead=0;
 for(int k=0;k<36;++k,t+=64){bool pulse=k>=8&&k<16;const char* id=kind==0?pid::bypass:pid::mbEnable;
  if(k==8||k==16){set(*a,id,pulse?1:0);set(*c,id,pulse?1:0);float raw=a->getAPVTS().getRawParameterValue(id)->load();if(k==8)onRead=raw;else offRead=raw;}
  run(*a,aa,midi,t);run(*b,bb,midi,t);run(*c,cc,midi,t);run(*d,dd,midi,t);
  for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i){double x=std::abs((double)aa.getSample(ch,i)-bb.getSample(ch,i));double y=std::abs((double)cc.getSample(ch,i)-dd.getSample(ch,i));double z=std::abs((double)bb.getSample(ch,i)-dd.getSample(ch,i));if(x>ab)ab=x;if(y>cd)cd=y;if(z>duck)duck=z;nab+=x!=0;ncd+=y!=0;}
 }
 std::printf("processor %s readback on/off=%g/%g overlap count=%d peak=%.9g no-duck count=%d peak=%.9g duck peak=%.9g\n",kind==0?"bypass":"multiband",onRead,offRead,nab,ab,ncd,cd,duck);
 }
 {auto p=std::make_unique<AnamorphAudioProcessor>();set(*p,pid::advancedMode,1);set(*p,pid::algorithm,0);set(*p,pid::mbEnable,0);set(*p,pid::mix,0);p->setRateAndBufferSizeDetails(48000,64);p->prepareToPlay(48000,64);juce::AudioBuffer<float> v(2,64);juce::MidiBuffer m;for(int c=0;c<2;++c)for(int i=0;i<64;++i)v.setSample(c,i,2.0e38f);p->processBlock(v,m);std::printf("processor mix0 extreme=%a next=",v.getSample(0,0));for(int c=0;c<2;++c)for(int i=0;i<64;++i)v.setSample(c,i,.25f);p->processBlock(v,m);std::printf("%a\n",v.getSample(0,63));}
 {auto a=std::make_unique<AnamorphAudioProcessor>(),b=std::make_unique<AnamorphAudioProcessor>();
  for(auto* p:{a.get(),b.get()}){set(*p,pid::advancedMode,1);set(*p,pid::algorithm,0);set(*p,pid::amount,0);set(*p,pid::width,.5f);set(*p,pid::mbEnable,1);set(*p,pid::mbBands,1.0f/3.0f);auto* f=p->getAPVTS().getParameter(pid::mbFreqLow);f->setValueNotifyingHost(f->convertTo0to1(20.f));set(*p,pid::mix,1);p->setRateAndBufferSizeDetails(48000,128);p->prepareToPlay(48000,128);}
  juce::AudioBuffer<float> x(2,128),y(2,128);juce::MidiBuffer midi;double err=0,sa=0,sb=0,peak=0;int count=0;
  for(int k=0;k<1133;++k){if(k==1125)set(*a,pid::mix,.5f);for(int i=0;i<128;++i){float v=.5f*std::sin(6.283185307179586*20.0*(k*128+i)/48000.0);x.setSample(0,i,v);x.setSample(1,i,v);y.setSample(0,i,v);y.setSample(1,i,v);}a->processBlock(x,midi);b->processBlock(y,midi);if(k>=1125)for(int i=0;i<128&&count<960;++i,++count){double v=x.getSample(0,i),w=y.getSample(0,i),d=v-w;err+=d*d;sa+=v*v;sb+=w*w;if(std::abs(d)>peak)peak=std::abs(d);}}
  std::printf("processor gated-dry 20Hz: count=%d peak=%.9g rmsdiff=%.9g relative-level=%.6f dB\n",count,peak,std::sqrt(err/count),10*std::log10(sa/sb));}

}
