#define N2_RECOVERY_SUPPORT_ONLY
#include "recovery_probe.cpp"
#include <cstring>
namespace recovery_access
{
struct Dry{friend auto member(Dry);};
template struct Get<Dry,&E::dryScratch>;
}
bool sameRight(anamorph::MultibandWidth& a,anamorph::MultibandWidth& b)
{
    using namespace recovery_access;
    auto& x=a.*member(Banks{});auto& y=b.*member(Banks{});
    for(int bank=0;bank<2;++bank)for(int split=0;split<3;++split)
        for(auto which:{0,1})
        {
            auto& f=which?x[bank].dax[split]:x[bank].dx[split];
            auto& g=which?y[bank].dax[split]:y[bank].dx[split];
            for(auto ptr:{member(S1{}),member(S2{}),member(S3{}),member(S4{})})
                if(std::memcmp(&(f.*ptr)[1],&(g.*ptr)[1],sizeof(float))!=0)return false;
        }
    return true;
}
int main()
{
    using namespace recovery_access;
    juce::ScopedNoDenormals noDenormals;
    constexpr int n=512;
    anamorph::EngineParameters p;p.driveDb=6;p.mbEnable=true;p.mbBands=2;p.mbFreqLow=800;
    auto e=std::make_unique<E>();e->primeParameters(p);e->prepare(48000,n);
    juce::AudioBuffer<float> b(2,n);int time=0;
    for(int k=0;k<20;++k,time+=n)
    {for(int i=0;i<n;++i){b.setSample(0,i,0.2f);b.setSample(1,i,0.125f+0.02f*std::sin(0.017f*float(time+i)));}e->process(b);}
    anamorph::n2WetHeals=0;anamorph::n2DryHeals[0]=anamorph::n2DryHeals[1]=0;
    anamorph::LR4Xover impulse;impulse.prepare(48000);impulse.setCutoffFrequency(800);
    std::array<float,n> h{},zero{},outL{},outR{},wetL{},wetR{};
    for(int i=0;i<n;++i){float lo,hi;impulse.processSample(0,i==0?1.0f:0.0f,lo,hi);h[i]=lo+hi;}
    int rightMatches=0;bool finiteWet=true,finiteInputs=true;
    for(int k=0;k<4;++k,time+=n)
    {
        auto reference=e.get()->*member(MB{});
        for(int i=0;i<n;++i)
        {const float x=std::copysign(1.68e38f,h[n-1-i]);finiteInputs &= std::isfinite(x);b.setSample(0,i,x);b.setSample(1,i,0.125f+0.02f*std::sin(0.017f*float(time+i)));}
        e->process(b);
        const auto& conditioned=e.get()->*member(Dry{});
        // Right dry state advances independently of the left and wet inputs.
        reference.processBlock(wetL.data(),wetR.data(),n,zero.data(),conditioned.getReadPointer(1),outL.data(),outR.data());
        rightMatches+=sameRight(e.get()->*member(MB{}),reference);
        for(int i=0;i<n;++i)finiteWet &= std::isfinite(b.getSample(0,i))&&std::isfinite(b.getSample(1,i));
    }
    std::printf("finite_inputs,finite_wet,dry_heals_left,dry_heals_right,wet_heals,right_state_matches,blocks\n%d,%d,%d,%d,%d,%d,4\n",
        finiteInputs,finiteWet,anamorph::n2DryHeals[0],anamorph::n2DryHeals[1],anamorph::n2WetHeals,rightMatches);
    return finiteInputs&&finiteWet&&anamorph::n2DryHeals[0]>0&&anamorph::n2DryHeals[1]==0&&anamorph::n2WetHeals==0&&rightMatches==4?0:1;
}
