// Nonshipping direct production MultibandWidth path. Finite/NaN recursive
// states are deliberately injected; this is not a natural-reachability claim.
#include "dsp/MultibandWidth.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

using M = anamorph::MultibandWidth;
using X = anamorph::LR4Xover;
namespace h4_attrib_access {
struct Banks { friend auto member(Banks); };
struct Active { friend auto member(Active); };
struct Fading { friend auto member(Fading); };
struct FadePos { friend auto member(FadePos); };
struct FadeLen { friend auto member(FadeLen); };
struct S1 { friend auto member(S1); };
struct S2 { friend auto member(S2); };
struct S3 { friend auto member(S3); };
struct S4 { friend auto member(S4); };
template<class T, auto P> struct Get { friend auto member(T) { return P; } };
template struct Get<Banks, &M::bank>; template struct Get<Active, &M::active>;
template struct Get<Fading, &M::fading>; template struct Get<FadePos, &M::fadePos>;
template struct Get<FadeLen, &M::fadeLen>;
template struct Get<S1, &X::s1>; template struct Get<S2, &X::s2>;
template struct Get<S3, &X::s3>; template struct Get<S4, &X::s4>;
}
static bool same(float a,float b) { return std::memcmp(&a,&b,sizeof a)==0; }
static bool stateFinite(const X& f,int c) {
 using namespace h4_attrib_access;
 return std::isfinite((f.*member(S1{}))[c]) && std::isfinite((f.*member(S2{}))[c])
     && std::isfinite((f.*member(S3{}))[c]) && std::isfinite((f.*member(S4{}))[c]);
}
static bool sameState(const X& a,const X& b,int c) {
 using namespace h4_attrib_access;
 return same((a.*member(S1{}))[c],(b.*member(S1{}))[c])
     && same((a.*member(S2{}))[c],(b.*member(S2{}))[c])
     && same((a.*member(S3{}))[c],(b.*member(S3{}))[c])
     && same((a.*member(S4{}))[c],(b.*member(S4{}))[c]);
}
int main(int argc,char** argv) {
 using namespace h4_attrib_access;
 const std::string mode=argc>1?argv[1]:"positive";
 if(mode!="positive"&&mode!="bad-attribution"&&mode!="bad-bank"&&mode!="bad-arithmetic")return 2;
 juce::ScopedNoDenormals noDenormals;
 int checks=0,failures=0;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
 for(int scenario=0;scenario<3;++scenario) {
  M m;m.setBandCount(2);m.prepare(48000,16);
  m.setCrossovers(1200.0f,2000.0f,4000.0f);
  float l=0.0f,r=0.0f,dl=0.0f,dr=0.0f,ol=0.0f,orr=0.0f;
  m.processBlock(&l,&r,1,&dl,&dr,&ol,&orr);
  check(m.*member(Fading{}),"actual two-bank fade executed");
  auto& banks=m.*member(Banks{});int active=m.*member(Active{}),to=1-active;
  const float a=std::numeric_limits<float>::max()*0.25f;
  (banks[active].dx[0].*member(S1{}))[0]= scenario==2?std::numeric_limits<float>::quiet_NaN():a;
  (banks[to].dx[0].*member(S1{}))[0]= scenario==1?a:-a;
  auto fromCopy=banks[active].dx[0],toCopy=banks[to].dx[0];
  float loA=0,hiA=0,loB=0,hiB=0;
  fromCopy.processSample(0,0.0f,loA,hiA);toCopy.processSample(0,0.0f,loB,hiB);
  const float da=loA+hiA,db=loB+hiB;
  const float weight=float((m.*member(FadePos{}))+1)/float(m.*member(FadeLen{}));
  const double wide=double(da)+double(weight)*(double(db)-double(da));
  l=r=dl=dr=ol=orr=0.0f;
  m.processBlock(&l,&r,1,&dl,&dr,&ol,&orr);
  check(m.*member(Fading{}),"fade still live at observed sample");
  check(sameState(banks[active].dx[0],fromCopy,0),"source bank advanced as independently copied filter");
  check(sameState(banks[to].dx[0],toCopy,0),"destination bank advanced as independently copied filter");
  check(std::isfinite(r)&&std::isfinite(orr),"opposite wet and dry outputs finite");
  if(scenario==0) {
   check(std::isfinite(da)&&std::isfinite(db),"both independently computed bank outputs finite");
   check(stateFinite(fromCopy,0)&&stateFinite(toCopy,0),"both processed bank states finite");
   check(!std::isfinite(db-da)&&!std::isfinite(ol),"finite bank outputs overflow actual fade subtraction");
   check(std::isfinite(wide)&&std::abs(wide)<=double(std::numeric_limits<float>::max()),"wide convex comparison representable");
   if(mode=="bad-attribution")check(!std::isfinite(da)||!std::isfinite(db),"negative claim that one bank output must be invalid");
   if(mode=="bad-arithmetic")check(std::isfinite(ol),"negative claim that finite bank outputs guarantee finite fade");
  } else if(scenario==1) {
   check(std::isfinite(da)&&std::isfinite(db)&&std::isfinite(ol),"same-direction finite control");
   const float expected=da+weight*(db-da);
   check(same(ol,expected),"actual finite fade matches copied-bank expression");
  } else {
   check(!std::isfinite(da)&&std::isfinite(db)&&!std::isfinite(ol),"one invalid source bank identified independently");
   check(!stateFinite(fromCopy,0)&&stateFinite(toCopy,0),"invalid source and independently finite destination state distinguished");
   if(mode=="bad-bank")check(!stateFinite(toCopy,0),"negative claim that valid destination bank is invalid");
  }
  std::printf("scenario=%d fade=%d weight=%a bank_a=%a bank_b=%a out=%a bank_a_finite=%d bank_b_finite=%d\n",
    scenario,int(m.*member(Fading{})),double(weight),double(da),double(db),double(ol),int(stateFinite(fromCopy,0)),int(stateFinite(toCopy,0)));
 }
 std::printf("%d checks, %d failures\n",checks,failures);
 return failures?1:0;
}
