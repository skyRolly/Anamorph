// Nonshipping one-sample engine trace. Invalid state is injected deliberately.
// This is a boundary oracle; it does not establish natural state reachability.
#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

namespace anamorph { extern bool h4DiagnosticUseConditionedFallback; }
using E = anamorph::AnamorphEngine;
using M = anamorph::MultibandWidth;
namespace history_probe {
struct MB { friend auto member(MB); }; struct Banks { friend auto member(Banks); };
struct Active { friend auto member(Active); }; struct S1 { friend auto member(S1); };
struct OS { friend auto member(OS); }; struct Mix { friend auto member(Mix); };
struct DryRing { friend auto member(DryRing); }; struct AlignRing { friend auto member(AlignRing); };
struct RawRing { friend auto member(RawRing); }; struct Write { friend auto member(Write); };
struct CleanScratch { friend auto member(CleanScratch); };
struct AlignScratch { friend auto member(AlignScratch); }; struct RefScratch { friend auto member(RefScratch); };
template<class T, auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<MB, &E::multiband>; template struct Get<Banks, &M::bank>;
template struct Get<Active, &M::active>; template struct Get<S1, &anamorph::LR4Xover::s1>;
template struct Get<OS, &E::osRunning>; template struct Get<Mix, &E::mixSmooth>;
template struct Get<DryRing, &E::dryDelayBuffer>; template struct Get<AlignRing, &E::dryAlignDelayBuffer>;
template struct Get<RawRing, &E::bypassDelayBuffer>; template struct Get<Write, &E::dryDelayWrite>;
template struct Get<CleanScratch, &E::dryScratch>;
template struct Get<AlignScratch, &E::dryAlignScratch>; template struct Get<RefScratch, &E::loudnessRefScratch>;
}
static bool same(float a, float b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
static void fill(juce::AudioBuffer<float>& b, int offset) {
    for (int i=0; i<b.getNumSamples(); ++i) {
        const float t = static_cast<float>(offset+i);
        b.setSample(0, i, .18f*std::sin(.017f*t));
        b.setSample(1, i, .11f*std::cos(.013f*t));
    }
}
static bool sameBuffer(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int ch) {
    if (a.getNumSamples()!=b.getNumSamples()) return false;
    for (int i=0; i<a.getNumSamples(); ++i) if (!same(a.getSample(ch,i),b.getSample(ch,i))) return false;
    return true;
}
int main(int argc, char** argv) {
    using namespace history_probe;
    const std::string mode = argc > 1 ? argv[1] : "positive";
    if (mode != "positive" && mode != "claim-clean" && mode != "invalid-history" && mode != "no-fallback") return 2;
    juce::ScopedNoDenormals noDenormals;
    int checks=0, failures=0;
    auto check=[&](bool ok, const char* name) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n",name); } };
    for (bool wrapped : {false,true}) {
        anamorph::h4DiagnosticUseConditionedFallback = mode != "no-fallback";
        constexpr int maxBlock=128, steps=32;
        anamorph::EngineParameters p; p.mbEnable=true; p.mbBands=2; p.mix=1.0f;
        p.autoGainMatch=false; p.monoMakerEnable=false;
        if (wrapped) { p.oversample=anamorph::OversampleFactor::x4; p.driveDb=6; }
        auto clean=std::make_unique<E>(), faulty=std::make_unique<E>(), wet=std::make_unique<E>();
        for (E* e : {clean.get(),faulty.get(),wet.get()}) { e->primeParameters(p); e->prepare(48000,maxBlock); }
        juce::AudioBuffer<float> c(2,maxBlock), f(2,maxBlock), w(2,maxBlock);
        int offset=0;
        for (int block=0;block<12;++block) {
            fill(c,offset); fill(f,offset); fill(w,offset); offset+=maxBlock;
            clean->process(c); faulty->process(f); wet->process(w);
        }
        check((faulty.get()->*member(OS{}))==wrapped,"actual wrapped execution");
        const int latency=faulty->getLatencySamples();
        check(wrapped ? latency==6 : latency==0,"actual latency premise");
        // During full-wet idle, the actual Measure reference reads clean history.
        // Difference from the aligned ring proves this observation discriminates it.
        auto& fullDry=faulty.get()->*member(DryRing{});
        auto& fullAlign=faulty.get()->*member(AlignRing{});
        const int size=fullDry.getNumSamples();
        int lastWrite=(faulty.get()->*member(Write{}))-1; if (lastWrite<0) lastWrite+=size;
        int lastRead=lastWrite-latency; if (lastRead<0) lastRead+=size;
        const auto& ref=faulty.get()->*member(RefScratch{});
        check(same(ref.getSample(0,maxBlock-1),fullDry.getSample(0,lastRead))
              && !same(fullDry.getSample(0,lastRead),fullAlign.getSample(0,lastRead)),
              "full-wet clean-reference gate observed");
        auto& mb=faulty.get()->*member(MB{}); auto& banks=mb.*member(Banks{});
        const int active=mb.*member(Active{});
        auto& injected=(banks[active].dx[0].*member(S1{}))[0];
        injected=std::numeric_limits<float>::quiet_NaN();
        p.mix=.5f; clean->setParameters(p); faulty->setParameters(p);
        check((faulty.get()->*member(Mix{})).isSmoothing(),"actual Mix re-entry ramp");
        int changedHistory=0, changedOutput=0, firstDifference=-1;
        bool writes=true, reads=true, finite=true, opposite=true, reconstructed=true, reset=false;
        bool conditioned=true, raw=true, invalidDetected=false, fallbackAtFault=false;
        c.setSize(2,1); f.setSize(2,1); w.setSize(2,1);
        for (int step=0;step<steps;++step) {
            auto& fa=faulty.get()->*member(AlignRing{});
            auto& ca=clean.get()->*member(AlignRing{});
            const int wp=faulty.get()->*member(Write{});
            int rp=wp-latency; if (rp<0) rp+=size;
            const float oldRead=fa.getSample(0,rp);
            fill(c,offset); fill(f,offset); fill(w,offset); ++offset;
            clean->process(c); faulty->process(f); wet->process(w);
            const float written=(faulty.get()->*member(AlignScratch{})).getSample(0,0);
            writes &= same(fa.getSample(0,wp),written);
            reads &= same(fa.getSample(0,rp),rp==wp ? written : oldRead);
            if (step==0) {
                reset = std::isfinite(injected) && injected==0.0f;
                const float cleanSample=(faulty.get()->*member(CleanScratch{})).getSample(0,0);
                fallbackAtFault=std::isfinite(cleanSample) && same(written,cleanSample);
            }
            if (mode=="invalid-history" && step==0) fa.setSample(0,wp,std::numeric_limits<float>::quiet_NaN());
            invalidDetected |= !std::isfinite(fa.getSample(0,wp));
            finite &= std::isfinite(fa.getSample(0,wp)) && std::isfinite(f.getSample(0,0));
            opposite &= same(f.getSample(1,0),c.getSample(1,0)) && sameBuffer(fa,ca,1);
            conditioned &= sameBuffer(faulty.get()->*member(DryRing{}),clean.get()->*member(DryRing{}),0)
                        && sameBuffer(faulty.get()->*member(DryRing{}),clean.get()->*member(DryRing{}),1);
            raw &= sameBuffer(faulty.get()->*member(RawRing{}),clean.get()->*member(RawRing{}),0)
                && sameBuffer(faulty.get()->*member(RawRing{}),clean.get()->*member(RawRing{}),1);
            changedHistory += !same(fa.getSample(0,wp),ca.getSample(0,wp));
            if (!same(f.getSample(0,0),c.getSample(0,0))) { ++changedOutput; if (firstDifference<0) firstDifference=step; }
            // All these samples remain above the 5% alignment threshold. Reconstruct
            // the actual Mix output from the separately observed full-wet twin and
            // the delayed ring entries, using the same finite expression and tick.
            const float m=(faulty.get()->*member(Mix{})).getCurrentValue();
            const float cl=(faulty.get()->*member(DryRing{})).getSample(0,rp);
            const float al=fa.getSample(0,rp);
            const float dry=cl+1.0f*(al-cl);
            const float oracle=dry+m*(w.getSample(0,0)-dry);
            reconstructed &= m>=.05f && same(oracle,f.getSample(0,0));
        }
        check(writes,"actual aligned samples published at write index");
        check(reads,"actual delay read position follows reported latency");
        check(reset,"affected recursive state reset at first consumption");
        check(fallbackAtFault,"fault block publishes finite conditioned dry fallback");
        check(finite,"new aligned history and output remain finite");
        check(opposite,"opposite channel output and aligned history preserved");
        check(conditioned,"conditioned history preserved");
        check(raw,"raw history preserved");
        check(reconstructed,"independent wet twin and actual history reconstruct Mix output");
        check(changedHistory>0 && changedOutput>0,"fault changes clean-reference history and output");
        check(firstDifference>=latency,"fault is not consumed before actual delay offset");
        if (mode=="claim-clean") check(changedHistory==0,"negative claim that finite history equals clean reference");
        if (mode=="invalid-history") std::printf("injected_invalid_history_detected=%d\n",invalidDetected);
        std::printf("case=%s latency=%d changed_history=%d changed_output=%d first_difference=%d fallback=%d\n",
                    wrapped?"x4":"off",latency,changedHistory,changedOutput,firstDifference,(int)fallbackAtFault);
    }
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
