// Nonshipping state fault injection: models an already-invalid dry integrator
// after a gated block. It does not claim that this injection occurred naturally.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
using E = anamorph::AnamorphEngine;
namespace access {
struct MB { friend auto member(MB); }; struct Banks { friend auto member(Banks); };
struct Active { friend auto member(Active); }; struct S1 { friend auto member(S1); };
struct OSRunning { friend auto member(OSRunning); }; struct Mix { friend auto member(Mix); };
template<class T, auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<MB,&E::multiband>;
template struct Get<Banks,&anamorph::MultibandWidth::bank>;
template struct Get<Active,&anamorph::MultibandWidth::active>;
template struct Get<S1,&anamorph::LR4Xover::s1>;
template struct Get<OSRunning,&E::osRunning>;
template struct Get<Mix,&E::mixSmooth>;
}
static float& dryState(E& e) {
    using namespace access; auto& mb=e.*member(MB{});
    return (((mb.*member(Banks{}))[mb.*member(Active{})].dx[0]).*member(S1{}))[0];
}
static float wetRightState(E& e) {
    using namespace access; auto& mb=e.*member(MB{});
    return (((mb.*member(Banks{}))[mb.*member(Active{})].x[0]).*member(S1{}))[1];
}
static bool same(float a,float b) { return std::memcmp(&a,&b,sizeof a)==0; }
static void fill(juce::AudioBuffer<float>& b,int block) {
    for(int i=0;i<b.getNumSamples();++i) {
        const float t=float(block*b.getNumSamples()+i);
        b.setSample(0,i,0.18f*std::sin(0.017f*t));
        b.setSample(1,i,0.11f*std::cos(0.013f*t));
    }
}
int main(int argc,char** argv) {
    if(argc!=3) return 2;
    const bool wrapped=std::string(argv[1])=="wrapped";
    const bool inject=std::string(argv[2])=="inject";
    constexpr int n=128;
    juce::ScopedNoDenormals noDenormals;
    anamorph::EngineParameters p; p.mbEnable=true;p.mbBands=2;p.mix=1.0f;
    p.autoGainMatch=false;p.monoMakerEnable=false;
    if(wrapped){p.oversample=anamorph::OversampleFactor::x4;p.driveDb=6.0f;}
    auto clean=std::make_unique<E>(), poisoned=std::make_unique<E>(), restored=std::make_unique<E>();
    for(E* e:{clean.get(),poisoned.get(),restored.get()}) {e->primeParameters(p);e->prepare(48000,n);}
    juce::AudioBuffer<float> a(2,n),b(2,n),c(2,n);
    for(int block=0;block<12;++block) {
        fill(a,block);fill(b,block);fill(c,block);
        clean->process(a);poisoned->process(b);restored->process(c);
    }
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* what){++checks;if(!ok){++failures;std::printf("FAIL %s\n",what);}};
    const float original=dryState(*poisoned);
    check(std::isfinite(original)&&same(original,dryState(*clean)),"initial dry states match and are finite");
    check((poisoned.get()->*access::member(access::OSRunning{}))==wrapped,"actual wrap engagement matches case");
    check(wrapped ? poisoned->getLatencySamples()>0 : poisoned->getLatencySamples()==0,"selected latency matches case");
    if(inject) {dryState(*poisoned)=std::numeric_limits<float>::quiet_NaN();dryState(*restored)=std::numeric_limits<float>::quiet_NaN();}
    fill(a,12);fill(b,12);fill(c,12);clean->process(a);poisoned->process(b);restored->process(c);
    int gatedDiff=0;for(int ch=0;ch<2;++ch)for(int i=0;i<n;++i)gatedDiff+=!same(a.getSample(ch,i),b.getSample(ch,i));
    check(gatedDiff==0,"full-wet gate output ignores injected dry state");
    check(!std::isfinite(dryState(*poisoned)),"injected dry state persists through gated block");
    dryState(*restored)=original;
    p.mix=0.5f;
    for(E* e:{clean.get(),poisoned.get(),restored.get()})e->setParameters(p);
    check((poisoned.get()->*access::member(access::Mix{})).isSmoothing(),"Mix re-entry ramp actually starts");
    fill(a,13);fill(b,13);fill(c,13);clean->process(a);poisoned->process(b);restored->process(c);
    int diffL=0,diffR=0,zeroL=0,restoreDiff=0;
    for(int i=0;i<n;++i) {
        diffL+=!same(a.getSample(0,i),b.getSample(0,i));
        diffR+=!same(a.getSample(1,i),b.getSample(1,i));
        zeroL+=b.getSample(0,i)==0.0f && a.getSample(0,i)!=0.0f;
        for(int ch=0;ch<2;++ch)restoreDiff+=!same(a.getSample(ch,i),c.getSample(ch,i));
    }
    check(diffL>0&&zeroL>0,"invalid dry state reaches actual Mix output and final guard");
    check(diffR==0,"opposite-channel output remains equal in this block");
    check(restoreDiff==0,"restoring only the injected dry state restores full output");
    check(!same(wetRightState(*clean),wetRightState(*poisoned)),"final guard changes valid opposite wet state");
    std::printf("case=%s fault=%s latency=%d gatedDiff=%d mixDiffL=%d mixDiffR=%d zeroL=%d restoredDiff=%d checks=%d failures=%d\n",
        wrapped?"wrapped":"off",inject?"injected":"absent",poisoned->getLatencySamples(),gatedDiff,diffL,diffR,zeroL,restoreDiff,checks,failures);
    return failures?1:0;
}
