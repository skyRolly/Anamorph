// Nonshipping production-path N2-06 boundary and ordinary-rounding assessment.
#include <juce_dsp/juce_dsp.h>
#include "AnamorphEngine.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

static bool same(float a, float b) { return std::memcmp(&a,&b,sizeof a)==0; }
static uint32_t state=0x81c31ab5u;
static float next() {
    state=state*1664525u+1013904223u;
    return static_cast<float>(static_cast<int32_t>(state>>8)-8388608)*(1.0f/8388608.0f);
}
int main() {
    juce::ScopedNoDenormals noDenormals;
    constexpr int block=128, blocks=64;
    anamorph::EngineParameters p; p.algorithm=anamorph::Algorithm::Haas;
    p.algoAmount=0.0f; p.driveDb=0.0f; p.width=0.0f; p.mix=1.0f;
    p.mbEnable=false; p.autoGainMatch=false; p.oversample=anamorph::OversampleFactor::Off;
    auto engine=std::make_unique<anamorph::AnamorphEngine>();
    engine->primeParameters(p); engine->prepare(48000,block);
    juce::AudioBuffer<float> audio(2,block);
    int modelMismatch=0, differences=0, samples=0;
    double maxDifference=0.0;
    for(int b=0;b<blocks;++b) {
        float inputL[block],inputR[block],wet[block],legacyL[block],legacyR[block];
        for(int i=0;i<block;++i) {
            const float l=next(), perturb=std::ldexp(next(),-((b+i)%18));
            const float r=-l+perturb;
            const float mid=(l+r)*0.5f;
            inputL[i]=l;inputR[i]=r;wet[i]=mid;
            legacyL[i]=l+1.0f*(mid-l);legacyR[i]=r+1.0f*(mid-r);
            audio.setSample(0,i,l);audio.setSample(1,i,r);
        }
        engine->process(audio);
        for(int i=0;i<block;++i) for(int ch=0;ch<2;++ch) {
            const float got=audio.getSample(ch,i),legacy=ch==0?legacyL[i]:legacyR[i];
            modelMismatch+=!same(got,legacy);
            differences+=!same(got,wet[i]);
            const double delta=std::abs(static_cast<double>(got)-wet[i]);
            if(delta>maxDifference)maxDifference=delta;
            ++samples;
        }
    }
    auto extreme=std::make_unique<anamorph::AnamorphEngine>();
    extreme->primeParameters(p);extreme->prepare(48000,block);
    for(int i=0;i<block;++i){audio.setSample(0,i,100000000.0f);audio.setSample(1,i,-99999992.0f);}
    extreme->process(audio);
    const bool witness=same(audio.getSample(0,block-1),0.0f)
                    && same(audio.getSample(1,block-1),8.0f);
    const int failures=(modelMismatch!=0)+(samples!=block*blocks*2)+(!witness);
    std::printf("normal_samples=%d legacy_model_mismatches=%d normal_wet_differences=%d max_abs=%.12g\n",
                samples,modelMismatch,differences,maxDifference);
    std::printf("extreme_actual=%a,%a desired_wet=%a witness=%d\n",
                (double)audio.getSample(0,block-1),(double)audio.getSample(1,block-1),4.0,(int)witness);
    std::printf("premise_failures=%d\n",failures); return failures?1:0;
}
