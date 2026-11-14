// Nonshipping fault injection into real MultibandWidth banks; not an engine Mix test.
#include "dsp/MultibandWidth.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace bank_access {
struct Banks { friend auto member(Banks); };
struct Active { friend auto member(Active); };
struct Fading { friend auto member(Fading); };
struct S1 { friend auto member(S1); };
struct S3 { friend auto member(S3); };
template<class T,auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<Banks,&anamorph::MultibandWidth::bank>;
template struct Get<Active,&anamorph::MultibandWidth::active>;
template struct Get<Fading,&anamorph::MultibandWidth::fading>;
template struct Get<S1,&anamorph::LR4Xover::s1>;
template struct Get<S3,&anamorph::LR4Xover::s3>;
}
using M=anamorph::MultibandWidth;
struct Samples { std::array<float,16> wl{},wr{},dl{},dr{}; };
static Samples process(M& m)
{
    Samples s;std::array<float,16> l{},r{};
    for(int i=0;i<16;++i){s.wl[i]=0.21f;s.wr[i]=-0.09f;l[i]=0.13f;r[i]=-0.07f;}
    m.processBlock(s.wl.data(),s.wr.data(),16,l.data(),r.data(),s.dl.data(),s.dr.data());
    return s;
}
static bool finite(const std::array<float,16>& a)
{ for(float x:a) if(!std::isfinite(x)) return false; return true; }
static bool same(const Samples& a,const Samples& b)
{
    for(int i=0;i<16;++i)
        for(auto pair:{std::pair{a.wl[i],b.wl[i]},std::pair{a.wr[i],b.wr[i]},
                       std::pair{a.dl[i],b.dl[i]},std::pair{a.dr[i],b.dr[i]}})
            if(std::memcmp(&pair.first,&pair.second,sizeof(float))!=0) return false;
    return true;
}
int main()
{
    using namespace bank_access;
    juce::ScopedNoDenormals noDenormals;
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}};
    M base;base.setBandCount(3);base.setCrossovers(100,800,3000);base.prepare(48000,16);
    for(int i=0;i<8;++i) (void)process(base);
    const float bad=std::numeric_limits<float>::quiet_NaN();
    auto jump=[](M& m){m.setCrossovers(1000,3000,7000);};

    M idle=base,reference=base;
    const int a=idle.*member(Active{}),dormant=1-a;
    auto& idleBank=(idle.*member(Banks{}))[dormant];
    (idleBank.dx[0].*member(S1{}))[0]=bad;
    check(!std::isfinite((idleBank.dx[0].*member(S1{}))[0]),"idle fault is present");
    const auto idleOut=process(idle),refOut=process(reference);
    check(!(idle.*member(Fading{}))&&same(idleOut,refOut),"idle bank is not consumed without fade");
    check(!std::isfinite((idleBank.dx[0].*member(S1{}))[0]),"idle fault remains dormant before copy");
    jump(idle);jump(reference);
    const auto afterCopy=process(idle),afterReference=process(reference);
    check(idle.*member(Fading{}),"discrete jump starts an actual fade");
    check(std::isfinite((idleBank.dx[0].*member(S1{}))[0])&&same(afterCopy,afterReference),
          "fade-start copy replaces stale destination state before consumption");

    M active=base;
    auto& live=(active.*member(Banks{}))[active.*member(Active{})];
    (live.dx[0].*member(S1{}))[0]=bad;jump(active);
    const auto activeOut=process(active);
    auto& ab=(active.*member(Banks{}));
    check(active.*member(Fading{}),"active-fault case enters fade");
    check(!std::isfinite((ab[0].dx[0].*member(S1{}))[0])&&!std::isfinite((ab[1].dx[0].*member(S1{}))[0]),
          "fade-start copy propagates an invalid active state");
    check(!finite(activeOut.dl)&&finite(activeOut.dr)&&finite(activeOut.wl)&&finite(activeOut.wr),
          "active dry fault reaches its dry channel while wet and other dry stay finite");

    for(bool destination:{false,true})
    {
        M fading=base;jump(fading);(void)process(fading);
        const int from=fading.*member(Active{}),selected=destination?1-from:from;
        auto& bank=(fading.*member(Banks{}))[selected];
        (bank.dx[0].*member(S1{}))[0]=bad;
        const auto out=process(fading);
        check(fading.*member(Fading{}),"fault is injected during a live fade");
        check(!finite(out.dl)&&finite(out.dr)&&finite(out.wl)&&finite(out.wr),
              destination?"destination dry bank is consumed":"source dry bank is consumed");
    }

    M unused=base;
    auto& unusedBank=(unused.*member(Banks{}))[unused.*member(Active{})];
    (unusedBank.dax[1].*member(S3{}))[0]=bad;
    auto beforeJump=process(unused);jump(unused);auto duringJump=process(unused);
    check(finite(beforeJump.dl)&&finite(duringJump.dl)&&unused.*member(Fading{}),
          "the injected allpass second-section state is unconsumed through fade");
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
