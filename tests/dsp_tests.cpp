// ============================================================================
//  Anamorph DSP self-tests (spec section 11.3)
//
//  Lightweight, dependency-free acceptance checks run headlessly in CI:
//    1. MS encode -> decode round-trip is bit-exact (within tiny epsilon).
//    2. Engine output contains no NaN / Inf / denormals across every algorithm
//       and feature combination, for noise AND silence.
//    3. Reported latency exactly matches the actual delay through the chain.
//    4. True bypass is null: bypassed output == delay-aligned input.
//
//  Exits non-zero on any failure so the build gate can fail the run.
// ============================================================================

#include <juce_dsp/juce_dsp.h>
#include <juce_data_structures/juce_data_structures.h>
#include "dsp/AnamorphEngine.h"
#include "dsp/MidSide.h"
#include "AbSlotIndex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>

namespace
{
    int failures = 0;
    int checks   = 0;

    void check (bool cond, const char* what)
    {
        ++checks;
        if (! cond) { ++failures; std::printf ("  [FAIL] %s\n", what); }
    }

    bool isBad (float x)
    {
        if (std::isnan (x) || std::isinf (x)) return true;
        const float a = std::abs (x);
        return a > 0.0f && a < 1.17549435e-38f; // denormal
    }

    void fillNoise (juce::AudioBuffer<float>& b, unsigned seed)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> d (-0.7f, 0.7f);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, d (rng));
    }
}

// ---------------------------------------------------------------------------
static void testMidSideRoundTrip()
{
    std::printf ("Test 1: MS encode/decode round-trip\n");
    std::mt19937 rng (12345);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    float maxErr = 0.0f;
    for (int i = 0; i < 100000; ++i)
    {
        const float L = d (rng), R = d (rng);
        float M, S, L2, R2;
        anamorph::MidSide::encode (L, R, M, S);
        anamorph::MidSide::decode (M, S, L2, R2);
        maxErr = juce::jmax (maxErr, std::abs (L2 - L), std::abs (R2 - R));
    }
    std::printf ("  max round-trip error = %.3e\n", maxErr);
    check (maxErr < 1.0e-6f, "MS round-trip within 1e-6");
}

// ---------------------------------------------------------------------------
static void testNoBadSamples()
{
    std::printf ("Test 2: no NaN / Inf / denormals across feature matrix\n");
    juce::ScopedNoDenormals noDenormals;

    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);

    using namespace anamorph;
    const Algorithm algos[] = { Algorithm::Haas, Algorithm::Velvet, Algorithm::Chorus, Algorithm::DimensionD };
    const OversampleFactor os[] = { OversampleFactor::Off, OversampleFactor::x2, OversampleFactor::x4, OversampleFactor::x8 };

    bool anyBad = false;

    for (auto a : algos)
        for (auto o : os)
            for (int variant = 0; variant < 2; ++variant)
            {
                EngineParameters p;
                p.algorithm = a;
                p.oversample = o;
                p.driveDb = 8.0f;
                p.width = 1.6f;
                p.mix = 0.8f;
                p.msMode = (variant == 0);
                p.mbEnable = true;
                p.monoMakerEnable = true;
                p.autoGainMatch = true;
                engine.setParameters (p);
                engine.reset();

                // Process many blocks of noise, then many blocks of silence
                // (silence is where denormals would otherwise creep in).
                for (int phase = 0; phase < 2; ++phase)
                {
                    for (int n = 0; n < 200; ++n)
                    {
                        juce::AudioBuffer<float> buf (2, block);
                        if (phase == 0) fillNoise (buf, (unsigned) (n * 7 + 1));
                        else            buf.clear();
                        engine.setParameters (p);
                        engine.process (buf);

                        for (int ch = 0; ch < 2; ++ch)
                            for (int i = 0; i < block; ++i)
                                if (isBad (buf.getSample (ch, i))) anyBad = true;
                    }
                }
            }

    check (! anyBad, "engine output free of NaN/Inf/denormals");
}

// ---------------------------------------------------------------------------
static void testBypassNullAndLatency()
{
    std::printf ("Test 3+4: true-bypass null + latency reporting\n");
    const double sr = 48000.0;
    const int block = 512;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);

    // --- 4a. OS off -> zero latency, exact null bypass ---
    {
        anamorph::EngineParameters p;
        p.bypass = true;
        p.oversample = anamorph::OversampleFactor::Off;
        engine.setParameters (p);
        engine.reset();

        check (engine.getLatencySamples() == 0, "latency == 0 with oversampling off");

        juce::AudioBuffer<float> in (2, block), work (2, block);
        fillNoise (in, 99);
        work.makeCopyOf (in);
        engine.setParameters (p);
        engine.process (work);

        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (work.getSample (ch, i) - in.getSample (ch, i)));
        std::printf ("  bypass null max diff (OS off) = %.3e\n", maxDiff);
        check (maxDiff == 0.0f, "true bypass is bit-exact null with OS off");
    }

    // --- 4b. OS on + drive -> reported latency == actual bypass delay ---
    for (auto factor : { anamorph::OversampleFactor::x2, anamorph::OversampleFactor::x4, anamorph::OversampleFactor::x8 })
    {
        anamorph::EngineParameters p;
        p.bypass = true;
        p.oversample = factor;
        p.driveDb = 6.0f; // makes oversampling "active" -> non-zero latency
        engine.setParameters (p);
        engine.reset();

        const int lat = engine.getLatencySamples();
        check (lat > 0, "latency > 0 when oversampling active");

        // Feed an impulse; the bypassed output must be the input delayed by lat. Process
        // in <= maxBlock chunks (the engine runs the full chain even while bypassed now,
        // so its scratch is sized for maxBlock -- a host never exceeds samplesPerBlock).
        const int N = 4096;
        juce::AudioBuffer<float> buf (2, N);
        buf.clear();
        buf.setSample (0, 0, 1.0f);
        buf.setSample (1, 0, 1.0f);
        engine.setParameters (p);
        for (int off = 0; off < N; off += block)
        {
            const int len = juce::jmin (block, N - off);
            float* chans[2] = { buf.getWritePointer (0) + off, buf.getWritePointer (1) + off };
            juce::AudioBuffer<float> sub (chans, 2, len);
            engine.process (sub);
        }

        int peakPos = -1; float peak = 0.0f;
        for (int i = 0; i < N; ++i)
            if (std::abs (buf.getSample (0, i)) > peak) { peak = std::abs (buf.getSample (0, i)); peakPos = i; }

        std::printf ("  OS factor latency=%d, impulse peak at %d\n", lat, peakPos);
        check (peakPos == lat, "bypass delay matches reported latency");
    }
}

// ---------------------------------------------------------------------------
//  A freshly-loaded plug-in (default parameters) must be transparent (#3):
//  amount 0, width 100%, mix 100%, drive 0 -> output == input (within epsilon).
static void testTransparentDefault()
{
    std::printf ("Test 5: default parameters are transparent\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters def; // all defaults
    engine.setParameters (def);
    engine.reset();

    float maxDiff = 0.0f;
    for (int n = 0; n < 40; ++n)
    {
        juce::AudioBuffer<float> in (2, block), work (2, block);
        fillNoise (in, (unsigned) (n + 3));
        work.makeCopyOf (in);
        engine.setParameters (def);
        engine.process (work);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (work.getSample (ch, i) - in.getSample (ch, i)));
    }
    std::printf ("  default transparency max diff = %.3e\n", maxDiff);
    check (maxDiff < 1.0e-5f, "default parameters leave the signal unchanged");
}

// ---------------------------------------------------------------------------
//  Mono Maker must collapse low-frequency SIDE content to mono (#20). Feed a
//  pure-side low tone (L = +tone, R = -tone) and confirm the side energy below
//  the crossover is removed when Mono Maker is on, and preserved when it's off.
static void testMonoMaker()
{
    std::printf ("Test 6: Mono Maker collapses low-frequency side energy\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;
    const double freq = 60.0; // well below the 200 Hz crossover

    auto measureSide = [&] (bool monoMakerOn)
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;            // transparent defaults
        p.monoMakerEnable = monoMakerOn;
        p.monoMakerFreq   = 200.0f;
        engine.setParameters (p);
        engine.reset();

        double phase = 0.0;
        const double inc = 2.0 * 3.14159265358979 * freq / sr;
        double sideSq = 0.0; int counted = 0;
        for (int n = 0; n < 60; ++n) // let the crossover settle, then measure
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase); phase += inc;
                buf.setSample (0, i,  s);   // pure side: L = +s, R = -s
                buf.setSample (1, i, -s);
            }
            engine.setParameters (p);
            engine.process (buf);
            if (n >= 40)
                for (int i = 0; i < block; ++i)
                {
                    const float side = 0.5f * (buf.getSample (0, i) - buf.getSample (1, i));
                    sideSq += static_cast<double> (side) * static_cast<double> (side); ++counted;
                }
        }
        return std::sqrt (sideSq / juce::jmax (1, counted));
    };

    const double sideOn  = measureSide (true);
    const double sideOff = measureSide (false);
    std::printf ("  side RMS  on=%.4f  off=%.4f\n", sideOn, sideOff);
    check (sideOff > 0.4, "Mono Maker OFF preserves the side tone");
    check (sideOn < 0.1 * sideOff, "Mono Maker ON removes the low-frequency side");

    // A MONO low tone (L = R) must be PRESERVED, not cut (feedback #25). Measure
    // the Mid energy with Mono Maker on; it should match the input level.
    auto measureMonoMid = [&] (bool monoMakerOn)
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.monoMakerEnable = monoMakerOn;
        p.monoMakerFreq   = 200.0f;
        engine.setParameters (p);
        engine.reset();
        double phase = 0.0;
        const double inc = 2.0 * 3.14159265358979 * freq / sr;
        double midSq = 0.0; int counted = 0;
        for (int nb = 0; nb < 60; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s); // mono low
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 40)
                for (int i = 0; i < block; ++i)
                {
                    const float mid = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                    midSq += static_cast<double> (mid) * static_cast<double> (mid); ++counted;
                }
        }
        return std::sqrt (midSq / juce::jmax (1, counted));
    };
    const double midOn = measureMonoMid (true);
    std::printf ("  mono-low Mid RMS on=%.4f (expect ~0.70)\n", midOn);
    check (midOn > 0.6, "Mono Maker preserves a MONO low tone (not a low-cut)");
}

// ---------------------------------------------------------------------------
//  The Multiband must not comb the dry/wet recombination at partial Mix
//  (Known Issue #1). With every band width = 1 the wet is a pure allpass A(input),
//  so a PHASE-MATCHED dry keeps the mono sum (L+R) energy-preserving at any Mix;
//  an unaligned (clean) dry would notch it to ~ -3 dB. Feed decorrelated stereo
//  noise at Mix = 0.5 and confirm the output mono-sum RMS tracks the input's.
static void testMultibandMonoCompat()
{
    std::printf ("Test 7: Multiband preserves the mono sum across Mix (phase fix)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    auto monoRatio = [&] (float mix) -> double
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;   // transparent defaults (amount 0, width 1, drive 0)
        p.mbEnable   = true;
        p.mbBands    = 4;
        p.mbWidthLow = p.mbWidthMid = p.mbWidthHiMid = p.mbWidthHigh = 1.0f; // pure allpass wet
        p.mix        = mix;
        engine.setParameters (p);
        engine.reset();

        double inSq = 0.0, outSq = 0.0;
        for (int nb = 0; nb < 80; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            fillNoise (buf, (unsigned) (nb * 13 + 5));
            double blkIn = 0.0;
            for (int i = 0; i < block; ++i)
            {
                const float mono = buf.getSample (0, i) + buf.getSample (1, i);
                const double monoD = static_cast<double> (mono);
                blkIn += monoD * monoD;
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 40) // let the crossovers settle before measuring
            {
                for (int i = 0; i < block; ++i)
                {
                    const float mono = buf.getSample (0, i) + buf.getSample (1, i);
                    outSq += static_cast<double> (mono) * static_cast<double> (mono);
                }
                inSq += blkIn;
            }
        }
        return std::sqrt (outSq / juce::jmax (1.0e-12, inSq));
    };

    for (float mix : { 0.25f, 0.5f, 0.75f })
    {
        const double ratio = monoRatio (mix);
        std::printf ("  Mix=%.2f mono-sum RMS out/in = %.3f (expect ~1.0; unaligned dry combs to <0.8)\n",
                     mix, ratio);
        check (ratio > 0.95, "Multiband keeps the mono sum intact (dry/wet phase-aligned)");
    }
}

// ---------------------------------------------------------------------------
//  Mono Maker now runs POST-Mix on the recombined signal, so it collapses the low
//  Side regardless of the Mix amount. Feed a pure-SIDE low tone (L=+s, R=-s) with
//  the Multiband widening the lows, and confirm the output low Side is removed at
//  every Mix (and the mono sum stays sane).
static void testMonoMakerPostMix()
{
    std::printf ("Test 8: Mono Maker (post-Mix) collapses the low Side at any Mix\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;
    const double freq = 60.0; // below the 200 Hz Mono Maker cutoff

    auto sideRatio = [&] (float mix) -> double
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.mbEnable        = true;
        p.mbBands         = 4;
        p.mbWidthLow      = 1.5f;   // the widener spreads the lows -> Mono Maker must still collapse them
        p.monoMakerEnable = true;
        p.monoMakerFreq   = 200.0f;
        p.mix             = mix;
        engine.setParameters (p);
        engine.reset();

        double phase = 0.0;
        const double inc = 2.0 * 3.14159265358979 * freq / sr;
        double inSq = 0.0, outSq = 0.0; int cnt = 0;
        for (int nb = 0; nb < 70; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase); phase += inc;
                buf.setSample (0, i,  s); // pure side: L = +s, R = -s
                buf.setSample (1, i, -s);
                inSq += static_cast<double> (s) * static_cast<double> (s); // input side magnitude == |s|
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 45)
                for (int i = 0; i < block; ++i)
                {
                    const float side = 0.5f * (buf.getSample (0, i) - buf.getSample (1, i));
                    outSq += static_cast<double> (side) * static_cast<double> (side); ++cnt;
                }
        }
        // input side RMS == sin RMS ~ 0.707; compare the measured tail to it.
        const double inRms  = std::sqrt (0.5);
        const double outRms = std::sqrt (outSq / juce::jmax (1, cnt));
        return outRms / inRms;
    };

    for (float mix : { 0.25f, 0.5f, 0.75f })
    {
        const double r = sideRatio (mix);
        std::printf ("  Mix=%.2f  output low-Side / input = %.3f (expect << 1)\n", mix, r);
        check (r < 0.15, "Mono Maker collapses the low Side at this Mix");
    }
}

// ---------------------------------------------------------------------------
//  Band Solo is a POST-EVERYTHING monitoring band-pass: it never changes the DSP,
//  it only filters the final output to the soloed band(s). Verify (a) selectivity --
//  soloing the low band passes a low tone and rejects a high one (and vice-versa);
//  (b) soloing ALL bands is energy-transparent (the monitor sums to an allpass).
static void testSoloMonitor()
{
    std::printf ("Test 9: Band Solo is a post-everything band-pass monitor\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    auto toneRms = [&] (double freq, int soloMask) -> double
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.mbEnable = true; p.mbBands = 4; p.mbSolo = soloMask; p.mix = 1.0f;
        engine.setParameters (p);
        engine.reset();
        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * freq / sr;
        double sq = 0.0; int cnt = 0;
        for (int nb = 0; nb < 70; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            { const float s = (float) std::sin (phase); phase += inc; buf.setSample (0, i, s); buf.setSample (1, i, s); }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 45)
                for (int i = 0; i < block; ++i) { const float v = buf.getSample (0, i); sq += static_cast<double> (v) * static_cast<double> (v); ++cnt; }
        }
        return std::sqrt (sq / juce::jmax (1, cnt));
    };

    // Crossovers default 180 / 800 / 3000 Hz: band 0 = <180, band 3 = >3000.
    const double lowInBand0  = toneRms (100.0,  0x1);
    const double lowInBand3  = toneRms (100.0,  0x8);
    const double highInBand3 = toneRms (6000.0, 0x8);
    const double highInBand0 = toneRms (6000.0, 0x1);
    std::printf ("  100Hz: band0 %.3f band3 %.3f ; 6kHz: band3 %.3f band0 %.3f\n",
                 lowInBand0, lowInBand3, highInBand3, highInBand0);
    check (lowInBand0  > 0.3,  "Solo band 0 passes a low tone");
    check (lowInBand3  < 0.05, "Solo band 3 rejects a low tone");
    check (highInBand3 > 0.3,  "Solo band 3 passes a high tone");
    check (highInBand0 < 0.05, "Solo band 0 rejects a high tone");

    // Energy transparency: soloing every band sums to an allpass of the output.
    auto noiseEnergy = [&] (int soloMask) -> double
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.mbEnable = true; p.mbBands = 4; p.mbSolo = soloMask; p.mix = 1.0f;
        engine.setParameters (p);
        engine.reset();
        double sq = 0.0;
        for (int nb = 0; nb < 80; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            fillNoise (buf, (unsigned) (nb * 17 + 2));
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 40)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < block; ++i) { const float v = buf.getSample (ch, i); sq += static_cast<double> (v) * static_cast<double> (v); }
        }
        return sq;
    };
    const double eNone = noiseEnergy (0x0);
    const double eAll  = noiseEnergy (0xF);
    std::printf ("  energy  no-solo %.1f  all-bands-solo %.1f (ratio %.3f, expect ~1)\n",
                 eNone, eAll, eAll / juce::jmax (1.0e-9, eNone));
    check (std::abs (eAll - eNone) < 0.05 * eNone, "Soloing all bands is energy-transparent");
}

// ---------------------------------------------------------------------------
//  Level Match measures the post-Mono-Maker output (the real processed signal) and
//  is independent of Band Solo (which is post-everything). With Drive boosting, the
//  match gain must go negative to compensate; and it must be the SAME with solo off
//  vs on (proving solo never changes the DSP / the measurement).
static void testLevelMatchAndSolo()
{
    std::printf ("Test 10: Level Match works and is solo-independent\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    auto matchDb = [&] (int soloMask) -> float
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.mbEnable      = true; p.mbBands = 4; p.mbSolo = soloMask;
        p.driveDb       = 6.0f;     // a real loudness boost to compensate
        p.autoGainMatch = true;
        p.mix           = 1.0f;
        engine.setParameters (p);
        engine.reset();
        for (int nb = 0; nb < 200; ++nb) // ~1 s for the 400 ms integrator to settle
        {
            juce::AudioBuffer<float> buf (2, block);
            fillNoise (buf, (unsigned) (nb * 11 + 4));
            engine.setParameters (p);
            engine.process (buf);
        }
        return engine.getMatchGainDb();
    };

    const float off = matchDb (0x0);
    const float on  = matchDb (0x1);
    std::printf ("  match gain  solo off %.2f dB  solo band0 %.2f dB\n", off, on);
    check (off < -0.3f, "Level Match compensates the Drive loudness boost (gain < 0)");
    check (std::abs (on - off) < 0.05f, "Level Match is identical with solo on/off (solo doesn't change DSP)");
}

// ---------------------------------------------------------------------------
//  Click-free transition matrix (0.8.1). A steady low sine is fed continuously while
//  the full set of state changes the user listed is applied at block boundaries:
//  Band Solo on/off (and changing the set), Mix 0<->1, Output gain / Balance jumps, a
//  forced bulk swap (A/B-style duck), and a Parameter Reset. The output must never step
//  (no sample-to-sample discontinuity beyond a small bound) and never go bad. A clean
//  220 Hz sine slews < 0.008 / sample, so a real click (a routing/level step) shows up
//  as a far larger jump; the bound catches it without flagging the smooth morphs.
static void testNoClicksAcrossTransitions()
{
    std::printf ("Test 11: no clicks across Solo / Mix / Gain / Balance / A-B / Reset\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 128;
    const double freq = 220.0;
    const float amp = 0.25f;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);

    anamorph::EngineParameters p; // transparent defaults
    p.mbEnable = true; p.mbBands = 4; // Multiband on so the post-everything Solo monitor runs
    engine.setParameters (p);
    engine.reset();

    double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * freq / sr;
    float prev = 0.0f; bool havePrev = false;
    double maxDelta = 0.0, maxAbs = 0.0; bool bad = false;
    const int warmup = 24;
    const int nBlocks = 360;

    for (int nb = 0; nb < nBlocks; ++nb)
    {
        // --- schedule the transitions (one mutation per milestone block) ---
        bool forced = false;
        switch (nb)
        {
            case 30:  p.mbSolo = 0x2;  break;                 // solo band 1 (contains 220 Hz)
            case 60:  p.mbSolo = 0xA;  break;                 // change the set: bands 1 + 3
            case 90:  p.mbSolo = 0x8;  break;                 // solo band 3 (rejects 220 Hz)
            case 120: p.mbSolo = 0x0;  break;                 // clear solo
            case 150: p.mix = 0.0f;    break;                 // Mix -> dry
            case 180: p.mix = 1.0f;    break;                 // Mix -> wet
            case 210: p.outputGainDb = -18.0f; break;         // big output-gain drop
            case 240: p.outputGainDb = 0.0f;   break;
            case 270: p.outputBalance = -1.0f; break;         // hard balance jump
            case 300: p.outputBalance = 0.0f;  break;
            case 320: forced = true; p.width = 1.8f; p.mbWidthLow = 1.6f; break; // A/B-style bulk swap
            case 340: p = anamorph::EngineParameters(); p.mbEnable = true; p.mbBands = 4; break; // Reset
            default: break;
        }
        if (forced) engine.requestDuck();
        engine.setParameters (p);

        juce::AudioBuffer<float> buf (2, block);
        for (int i = 0; i < block; ++i)
        {
            const float s = amp * (float) std::sin (phase); phase += inc;
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
        engine.process (buf);

        for (int i = 0; i < block; ++i)
        {
            const float v = buf.getSample (0, i);
            if (isBad (v)) bad = true;
            maxAbs = std::max (maxAbs, (double) std::abs (v));
            if (nb >= warmup && havePrev) maxDelta = std::max (maxDelta, (double) std::abs (v - prev));
            prev = v; havePrev = true;
        }
    }

    std::printf ("  max sample-to-sample delta = %.4f (clean-sine slew ~0.008) ; max |out| = %.3f\n",
                 maxDelta, maxAbs);
    check (! bad, "transition stream is free of NaN/Inf/denormals");
    check (maxDelta < 0.04, "no click: output stays continuous across every transition");
    check (maxAbs < 1.5, "no slam: output never blows up during a transition");
}

// ---------------------------------------------------------------------------
//  Ghost-signal guard (0.8.1): toggling Band Solo while the input is silent (DAW
//  paused / stopped / zero buffer) must not emit any signal. With the warm, crossfaded
//  monitor, silence in -> silence out whatever the solo set does.
static void testSoloNoGhostInSilence()
{
    std::printf ("Test 12: toggling Band Solo in silence emits no ghost signal\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 128;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.mbEnable = true; p.mbBands = 4;
    engine.setParameters (p);
    engine.reset();
    engine.setTransportPlaying (false);

    double maxAbs = 0.0; bool bad = false;
    const int masks[] = { 0x1, 0x0, 0x8, 0x4, 0xF, 0x0, 0x2 };
    int mi = 0;
    for (int nb = 0; nb < 140; ++nb)
    {
        if (nb % 18 == 0) { p.mbSolo = masks[mi % 7]; ++mi; engine.setParameters (p); }
        juce::AudioBuffer<float> buf (2, block);
        buf.clear(); // zero input buffer (paused / silent)
        engine.process (buf);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (ch, i);
                if (isBad (v)) bad = true;
                maxAbs = std::max (maxAbs, (double) std::abs (v));
            }
    }
    std::printf ("  max |out| over silent solo toggles = %.2e\n", maxAbs);
    check (! bad, "silent solo-toggle stream is clean");
    check (maxAbs < 1.0e-5, "no ghost: Band Solo toggled in silence stays silent");
}

// ---------------------------------------------------------------------------
//  Level Match must read ~0 at true unity (output == input): no measurement bias.
static void testLevelMatchUnity()
{
    std::printf ("Test 13: Level Match reads ~0 at unity (no bias)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p; // transparent defaults: width 1, mix 1, drive 0, no modules
    p.autoGainMatch = true;
    engine.setParameters (p);
    engine.reset();

    for (int nb = 0; nb < 240; ++nb)
    {
        juce::AudioBuffer<float> buf (2, block);
        fillNoise (buf, (unsigned) (nb * 13 + 7));
        engine.setParameters (p);
        engine.process (buf);
    }
    const float db = engine.getMatchGainDb();
    std::printf ("  unity match gain = %.3f dB (expect ~0)\n", db);
    check (std::abs (db) < 0.1f, "Level Match is unbiased at unity (output == input)");
}

// ---------------------------------------------------------------------------
//  The Drive predict must be ABSOLUTE: repeatedly cranking Drive up/down while PAUSED
//  (silent) must not ratchet the predicted gain toward the -24 dB floor. The published
//  gain must stay bounded near the single-cycle predict, however many cycles are run.
static void testLevelMatchNoRatchet()
{
    std::printf ("Test 14: Level Match predict doesn't ratchet on repeated Drive up/down (paused)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.autoGainMatch = true; p.mix = 1.0f;
    engine.setParameters (p);
    engine.reset();
    engine.setTransportPlaying (false);

    auto silentBlocks = [&] (int count)
    {
        for (int i = 0; i < count; ++i)
        {
            juce::AudioBuffer<float> buf (2, block); buf.clear();
            engine.setParameters (p);
            engine.process (buf);
        }
    };

    float worst = 0.0f; // most negative published gain seen across all cycles
    for (int cycle = 0; cycle < 6; ++cycle)
    {
        p.driveDb = 24.0f; silentBlocks (8);
        worst = std::min (worst, engine.getMatchGainDb());
        p.driveDb = 0.0f;  silentBlocks (8);
        worst = std::min (worst, engine.getMatchGainDb());
    }
    p.driveDb = 24.0f; silentBlocks (8);
    const float finalGain = engine.getMatchGainDb();
    std::printf ("  most-negative paused predict = %.2f dB ; final (drive 24) = %.2f dB\n", worst, finalGain);
    check (worst > -15.0f, "predict never ratchets toward the -24 dB floor");
    check (finalGain > -15.0f && finalGain < -6.0f, "predict stays at the single-cycle value, not accumulating");
}

// ---------------------------------------------------------------------------
//  Mix must feed the predict too, and the pause->play edge must not slam. Drive maxed +
//  Mix 0 -> match ~0 (output is dry). Then PAUSE, raise Mix to 100%, PLAY: the first
//  audible block must already be pre-ducked, so its peak is near the level-matched
//  steady state -- not ~4x louder (the old slam).
static void testLevelMatchMixCouplingNoSlam()
{
    std::printf ("Test 15: Mix feeds the predict; pause->Mix-up->play doesn't slam\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.autoGainMatch = true; p.driveDb = 24.0f; p.mix = 0.0f; // full drive, but fully dry
    engine.setParameters (p);
    engine.reset();
    engine.setTransportPlaying (true);

    // Play dry: output == input, so the match settles near 0 dB.
    for (int nb = 0; nb < 200; ++nb)
    {
        juce::AudioBuffer<float> buf (2, block);
        fillNoise (buf, (unsigned) (nb * 9 + 1));
        engine.setParameters (p);
        engine.process (buf);
    }
    const float dryMatch = engine.getMatchGainDb();

    // Establish the level-matched steady-state output peak at Mix = 100%.
    auto steadyPeak = [&] () -> double
    {
        p.mix = 1.0f;
        double pk = 0.0;
        for (int nb = 0; nb < 200; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            fillNoise (buf, (unsigned) (nb * 9 + 1));
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 150)
                for (int i = 0; i < block; ++i) pk = std::max (pk, (double) std::abs (buf.getSample (0, i)));
        }
        return pk;
    };
    const double steady = steadyPeak();

    // Now reproduce the user's gesture: back to dry + converged, PAUSE (silence) while
    // raising Mix to 100%, then PLAY -- measure the very first audible block's peak.
    p.mix = 0.0f;
    for (int nb = 0; nb < 200; ++nb)
    {
        juce::AudioBuffer<float> buf (2, block);
        fillNoise (buf, (unsigned) (nb * 9 + 1));
        engine.setParameters (p);
        engine.process (buf);
    }
    engine.setTransportPlaying (false);
    p.mix = 1.0f;                         // raise Mix while paused
    for (int nb = 0; nb < 12; ++nb) { juce::AudioBuffer<float> b (2, block); b.clear(); engine.setParameters (p); engine.process (b); }
    engine.setTransportPlaying (true);
    juce::AudioBuffer<float> first (2, block);
    fillNoise (first, 9999);
    engine.setParameters (p);
    engine.process (first);
    double firstPeak = 0.0;
    for (int i = 0; i < block; ++i) firstPeak = std::max (firstPeak, (double) std::abs (first.getSample (0, i)));

    std::printf ("  dry match=%.2f dB ; Mix100 steady peak=%.3f ; first played peak=%.3f (ratio %.2f)\n",
                 dryMatch, steady, firstPeak, firstPeak / juce::jmax (1.0e-6, steady));
    check (std::abs (dryMatch) < 0.6f, "Drive maxed + Mix 0 -> match ~0 (output is dry)");
    check (firstPeak < steady * 1.7, "pause -> Mix-up -> play does not slam (pre-ducked first block)");
}

// ---------------------------------------------------------------------------
//  On silence the MEASURE must wait: hold the last trusted value, never drift to 0.
static void testLevelMatchSilenceFreeze()
{
    std::printf ("Test 16: Level Match holds its value on silence (measure waits)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.autoGainMatch = true; p.driveDb = 8.0f; p.mix = 1.0f;
    engine.setParameters (p);
    engine.reset();
    engine.setTransportPlaying (true);

    for (int nb = 0; nb < 220; ++nb) // converge on audio
    {
        juce::AudioBuffer<float> buf (2, block);
        fillNoise (buf, (unsigned) (nb * 5 + 3));
        engine.setParameters (p);
        engine.process (buf);
    }
    const float converged = engine.getMatchGainDb();

    for (int nb = 0; nb < 200; ++nb) // ~1 s of silence
    {
        juce::AudioBuffer<float> buf (2, block); buf.clear();
        engine.setParameters (p);
        engine.process (buf);
    }
    const float held = engine.getMatchGainDb();
    std::printf ("  converged=%.2f dB ; after 1 s silence=%.2f dB\n", converged, held);
    check (converged < -1.0f, "Level Match compensates the Drive boost on audio");
    check (std::abs (held - converged) < 0.4f, "Level Match holds on silence (no drift toward 0)");
}

// ---------------------------------------------------------------------------
//  Issue 7: automating a crossover toward Nyquist (4 bands, all splits pushed high)
//  must NOT blow up the Linkwitz-Riley coefficients. Sweep mbFreqLow up past 20 kHz at
//  several sample rates, with Mix < 1 so the dry-align bank runs too, and confirm the
//  output stays finite and bounded (no +600 dB burst, no dead channel).
static void testCrossoverAutomationSafe()
{
    std::printf ("Test 17: Multiband crossover automation is Nyquist-safe (no explosion)\n");
    juce::ScopedNoDenormals noDenormals;
    bool anyBad = false; double worstAbs = 0.0;

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const int block = 128;
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.mbEnable = true; p.mbBands = 4; p.mix = 0.7f; // Mix<1 exercises the dry-align bank too

        const int N = 500;
        double maxAbs = 0.0; bool bad = false;
        for (int nb = 0; nb < N; ++nb)
        {
            const float t = (float) nb / (float) (N - 1);
            const float f = 180.0f + t * (20000.0f - 180.0f); // drive split 1 toward 20 kHz
            p.mbFreqLow  = f;
            p.mbFreqMid  = juce::jmin (20000.0f, f * 1.4f);   // crowd them all up near Nyquist
            p.mbFreqHigh = 20000.0f;
            juce::AudioBuffer<float> buf (2, block);
            fillNoise (buf, (unsigned) (nb * 7 + 1));
            engine.setParameters (p);
            engine.process (buf);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (isBad (v)) bad = true;
                    maxAbs = std::max (maxAbs, (double) std::abs (v));
                }
        }
        std::printf ("  sr=%.0f  max|out|=%.3f%s\n", sr, maxAbs, bad ? "  [BAD SAMPLES]" : "");
        anyBad = anyBad || bad;
        worstAbs = std::max (worstAbs, maxAbs);
    }
    check (! anyBad, "no NaN/Inf during extreme crossover automation");
    check (worstAbs < 4.0, "output stays bounded under extreme crossover automation");
}

// ---------------------------------------------------------------------------
//  Issue 2: with Multiband ON but all band widths at unity (no audible processing),
//  Level Match must read ~0 dB -- the allpass-reconstruction ripple cancels because the
//  loudness reference is the matched A(dry) reconstruction, not the raw input.
static void testMultibandUnityMatch()
{
    std::printf ("Test 18: Level Match reads ~0 at unity with Multiband ON\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0; const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.mbEnable = true; p.mbBands = 4; p.autoGainMatch = true; // widths all default 1.0
    engine.setParameters (p);
    engine.reset();
    for (int nb = 0; nb < 240; ++nb)
    {
        juce::AudioBuffer<float> buf (2, block);
        fillNoise (buf, (unsigned) (nb * 13 + 7));
        engine.setParameters (p);
        engine.process (buf);
    }
    const float db = engine.getMatchGainDb();
    std::printf ("  multiband-on unity match = %.3f dB (expect ~0)\n", db);
    check (std::abs (db) < 0.1f, "Level Match unbiased at unity with Multiband on (Issue 2)");
}

// ---------------------------------------------------------------------------
//  Issue 8: a NaN/Inf burst used to latch the meter envelopes at NaN forever (the bright
//  bar vanished). Poison the meter, then feed real audio and confirm the bright reading
//  recovers to a sane finite level.
static void testMeterRecoversFromNaN()
{
    std::printf ("Test 19: meters self-heal after a NaN/Inf burst\n");
    const double sr = 48000.0; const int block = 256;

    anamorph::StereoLevel meter;
    meter.prepare (sr);

    juce::AudioBuffer<float> bad (2, block);
    for (int i = 0; i < block; ++i)
    {
        bad.setSample (0, i, std::numeric_limits<float>::quiet_NaN());
        bad.setSample (1, i, std::numeric_limits<float>::infinity());
    }
    meter.process (bad.getReadPointer (0), bad.getReadPointer (1), block);
    meter.publish();

    float bri = -100.0f;
    for (int nb = 0; nb < 40; ++nb)
    {
        juce::AudioBuffer<float> buf (2, block);
        for (int i = 0; i < block; ++i) { buf.setSample (0, i, 0.5f); buf.setSample (1, i, 0.5f); }
        meter.process (buf.getReadPointer (0), buf.getReadPointer (1), block);
        meter.publish();
        bri = meter.getBriL();
    }
    std::printf ("  bright reading after recovery = %.2f dB (expect ~ -6)\n", bri);
    check (std::isfinite (bri) && bri > -20.0f, "bright meter recovers after a NaN burst (Issue 8)");
}

// ---------------------------------------------------------------------------
//  Issue 1: toggling Bypass must never burst or leave stale state. Toggle it during
//  playback (no NaN, bounded) and confirm that once settled into bypass on a silent
//  input the output is exactly silent (no leaked fragment, buffers cleared at the duck).
static void testBypassToggleRobust()
{
    std::printf ("Test 20: bypass toggling is clean (no burst; silent-in -> silent-out)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0; const int block = 128;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.mbEnable = true; p.mbBands = 4; p.monoMakerEnable = true;
    p.oversample = anamorph::OversampleFactor::x4; p.driveDb = 6.0f; // OS latency in play
    engine.setParameters (p);
    engine.reset();
    engine.setTransportPlaying (true);

    double maxAbs = 0.0; bool bad = false;
    for (int nb = 0; nb < 200; ++nb)
    {
        if (nb % 11 == 0) { p.bypass = ! p.bypass; engine.setParameters (p); }
        juce::AudioBuffer<float> buf (2, block);
        fillNoise (buf, (unsigned) (nb * 7 + 1));
        engine.setParameters (p);
        engine.process (buf);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
            { const float v = buf.getSample (ch, i); if (isBad (v)) bad = true; maxAbs = std::max (maxAbs, (double) std::abs (v)); }
    }
    check (! bad, "bypass toggling during playback never produces NaN/Inf");
    // Full-scale noise through Drive(+6 dB) + OS + 4-band Multiband is an extreme
    // crest-factor case (~2.0 peak inherent). The flat-recombination fix adds ~2 %
    // peak (allpass phase preserves energy but raises crest: measured 1.98 -> 2.02
    // toggling / 2.08 steady, both stable over 200 blocks -- not a bypass artifact).
    // The guard is against a real BURST (a stuck channel / +600 dB NaN blow-up would
    // be far above this and NaN is caught separately), so bound at 2.5.
    check (maxAbs < 2.5, "bypass toggling during playback never bursts");

    // Settle into bypass with a silent input, then assert the output is exactly silent.
    p.bypass = true; engine.setParameters (p);
    for (int nb = 0; nb < 80; ++nb) { juce::AudioBuffer<float> b (2, block); b.clear(); engine.process (b); }
    double tail = 0.0;
    for (int nb = 0; nb < 20; ++nb)
    {
        juce::AudioBuffer<float> b (2, block); b.clear();
        engine.process (b);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i) tail = std::max (tail, (double) std::abs (b.getSample (ch, i)));
    }
    std::printf ("  settled bypass silent-in tail = %.2e ; play-toggle max|out| = %.3f\n", tail, maxAbs);
    check (tail == 0.0, "settled bypass passes silence through as exact silence (no stale leak)");
}

// ---------------------------------------------------------------------------
//  Issue 2: Bypass must NOT stop the analysis. Level Match still has to Measure (and
//  Predict) while bypassed, and arrive at the same value as when active -- Bypass only
//  changes the audio path, never the analysis path.
static void testLevelMatchRunsInBypass()
{
    std::printf ("Test 21: Level Match keeps measuring while bypassed\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0; const int block = 256;

    auto matchAfter = [&] (bool bypass) -> float
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.driveDb = 12.0f; p.autoGainMatch = true; p.mix = 1.0f; p.bypass = bypass;
        engine.setParameters (p);
        engine.reset();
        engine.setTransportPlaying (true);
        for (int nb = 0; nb < 200; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            fillNoise (buf, (unsigned) (nb * 11 + 3));
            engine.setParameters (p);
            engine.process (buf);
        }
        return engine.getMatchGainDb();
    };

    const float active   = matchAfter (false);
    const float bypassed = matchAfter (true);
    std::printf ("  match: active %.2f dB  bypassed %.2f dB\n", active, bypassed);
    check (bypassed < -1.0f, "Level Match measures the boost even while bypassed (Issue 2)");
    check (std::abs (bypassed - active) < 0.3f, "Bypass doesn't change the analysis result");
}

// ---------------------------------------------------------------------------
//  Issue 3: Bypass is a click-free crossfade -- no click, and crucially NO mute /
//  dropout. Toggle it repeatedly on a steady tone with an audible level offset between
//  processed (Output Gain -6 dB) and bypassed (0 dB) and confirm the output never steps
//  and never collapses toward silence during the transition.
static void testBypassCrossfadeClickFree()
{
    std::printf ("Test 22: bypass crossfade is click-free and never mutes\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0; const int block = 128;
    const double freq = 220.0; const float amp = 0.25f;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.outputGainDb = -6.0f; // processed is clearly quieter than bypass -> a real transition
    engine.setParameters (p);
    engine.reset();

    double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * freq / sr;
    float prev = 0.0f; bool havePrev = false;
    double maxDelta = 0.0, minBlockPeak = 1.0e9; bool bad = false;
    const int warmup = 24;
    for (int nb = 0; nb < 300; ++nb)
    {
        if (nb % 30 == 0) { p.bypass = ! p.bypass; engine.setParameters (p); }
        juce::AudioBuffer<float> buf (2, block);
        for (int i = 0; i < block; ++i)
        { const float s = amp * (float) std::sin (phase); phase += inc; buf.setSample (0, i, s); buf.setSample (1, i, s); }
        engine.process (buf);

        double blockPeak = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const float v = buf.getSample (0, i);
            if (isBad (v)) bad = true;
            if (nb >= warmup && havePrev) maxDelta = std::max (maxDelta, (double) std::abs (v - prev));
            prev = v; havePrev = true;
            blockPeak = std::max (blockPeak, (double) std::abs (v));
        }
        if (nb >= warmup) minBlockPeak = std::min (minBlockPeak, blockPeak);
    }
    std::printf ("  max delta=%.4f ; min block peak=%.3f (processed 0.125 .. bypass 0.25)\n", maxDelta, minBlockPeak);
    check (! bad, "bypass crossfade stream is clean");
    check (maxDelta < 0.04, "bypass crossfade is click-free (no step)");
    check (minBlockPeak > 0.1, "bypass crossfade never mutes (no dropout)");
}

// ---------------------------------------------------------------------------
//  Multiband Enable is now a click-free OUTPUT crossfade (the Bypass model), NOT a
//  duck-to-silence: toggling it must not click and, crucially, must NOT mute/drop the
//  output. Toggle it on a steady stereo tone with band widths != 1 so multiband-on is
//  audibly different from off (a real transition), and confirm the output never steps
//  and never collapses toward silence while the crossover bank fades in/out.
static void testMultibandEnableCrossfadeClickFree()
{
    std::printf ("Test 23: Multiband Enable crossfade is click-free and never mutes\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0; const int block = 128;
    const double freq = 220.0; const float amp = 0.25f;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.mbEnable = true; p.mbBands = 4;
    // Widen every band so multiband-on clearly differs from off (a real transition),
    // while a quadrature (decorrelated) stereo input never collapses toward silence.
    p.mbWidthLow = p.mbWidthMid = p.mbWidthHiMid = p.mbWidthHigh = 1.6f;
    engine.setParameters (p);
    engine.reset();

    double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * freq / sr;
    float prev = 0.0f; bool havePrev = false;
    double maxDelta = 0.0, minBlockPeak = 1.0e9; bool bad = false;
    const int warmup = 24;
    for (int nb = 0; nb < 300; ++nb)
    {
        if (nb % 30 == 0) { p.mbEnable = ! p.mbEnable; engine.setParameters (p); }
        juce::AudioBuffer<float> buf (2, block);
        for (int i = 0; i < block; ++i)
        {
            const float sL = amp * (float) std::sin (phase);
            const float sR = amp * (float) std::cos (phase); // quadrature -> real Side energy
            phase += inc;
            buf.setSample (0, i, sL); buf.setSample (1, i, sR);
        }
        engine.process (buf);

        double blockPeak = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const float v = buf.getSample (0, i);
            if (isBad (v)) bad = true;
            if (nb >= warmup && havePrev) maxDelta = std::max (maxDelta, (double) std::abs (v - prev));
            prev = v; havePrev = true;
            blockPeak = std::max (blockPeak, (double) std::abs (v));
        }
        if (nb >= warmup) minBlockPeak = std::min (minBlockPeak, blockPeak);
    }
    std::printf ("  max delta=%.4f ; min block peak=%.3f\n", maxDelta, minBlockPeak);
    check (! bad, "Multiband Enable crossfade stream is clean");
    check (maxDelta < 0.05, "Multiband Enable crossfade is click-free (no step)");
    check (minBlockPeak > 0.1, "Multiband Enable crossfade never mutes (no dropout)");
}

// ---------------------------------------------------------------------------
//  Regression (0.8.6): with a Band Solo active, toggling Multiband Enable must stay
//  click-free. The Band Solo monitor is click-free ONLY if process() runs EVERY block so
//  its passGain/bandGain crossfade can morph; the old `if (p.mbEnable)` gate hard-switched
//  the whole band-pass in/out on the toggle (an amplitude + phase step = the click), on
//  both edges. DEFAULT band widths make the multiband itself identity, so this isolates the
//  monitor: solo one band that contains the tone, toggle Multiband Enable on a steady tone,
//  and confirm no step (click-free) and no dropout (never mutes).
static void testSoloMultibandEnableClickFree()
{
    std::printf ("Test 24: Band Solo + Multiband Enable toggle is click-free\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0; const int block = 128;
    const double freq = 280.0; const float amp = 0.4f; // inside band 1 (180..800 Hz)

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.mbEnable = true; p.mbBands = 4; p.mix = 1.0f;
    p.mbSolo = 0x2; // solo band 1 -- it contains the tone, so the soloed output is NOT silent
    engine.setParameters (p);
    engine.reset();

    double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * freq / sr;
    float prev = 0.0f; bool havePrev = false;
    double maxDelta = 0.0, minBlockPeak = 1.0e9; bool bad = false;
    // Settle the soloed state across the first window, THEN toggle Multiband Enable (the
    // solo stays set the whole time) so every toggle crosses a real soloed<->passthrough edge.
    const int firstToggle = 30, measureFrom = 28;
    for (int nb = 0; nb < 300; ++nb)
    {
        if (nb >= firstToggle && (nb - firstToggle) % 30 == 0) { p.mbEnable = ! p.mbEnable; engine.setParameters (p); }
        juce::AudioBuffer<float> buf (2, block);
        for (int i = 0; i < block; ++i)
        { const float s = amp * (float) std::sin (phase); phase += inc; buf.setSample (0, i, s); buf.setSample (1, i, s); }
        engine.process (buf);

        double blockPeak = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const float v = buf.getSample (0, i);
            if (isBad (v)) bad = true;
            if (nb >= measureFrom && havePrev) maxDelta = std::max (maxDelta, (double) std::abs (v - prev));
            prev = v; havePrev = true;
            blockPeak = std::max (blockPeak, (double) std::abs (v));
        }
        if (nb >= measureFrom) minBlockPeak = std::min (minBlockPeak, blockPeak);
    }
    std::printf ("  max delta=%.4f ; min block peak=%.3f\n", maxDelta, minBlockPeak);
    check (! bad, "Solo + Multiband Enable toggle stream is clean");
    check (maxDelta < 0.05, "Solo + Multiband Enable toggle is click-free (no step)");
    check (minBlockPeak > 0.1, "Solo + Multiband Enable toggle never mutes (no dropout)");
}

// ---------------------------------------------------------------------------
//  State-restoration robustness (NOT a DSP test): a corrupted / hand-edited /
//  forward-version session can carry an out-of-range A/B "active" index. The
//  restore path (PluginProcessor.cpp setStateInformation) must clamp it so it
//  can never index the size-2 abSlot[]/abUndo[] arrays out of bounds. We can't
//  link the full AudioProcessor headlessly (no juce_audio_processors here), so we
//  drive the SAME corrupted "AB" ValueTree through the SAME read+clamp expression
//  the processor uses (anamorph::clampAbSlotIndex). This fails on the pre-fix code
//  (unclamped (int)getProperty would yield 2/3/-1) and passes on the fix.
static void testAbActiveClampOnCorruptState()
{
    std::printf ("State test: A/B active-slot clamp on corrupted state\n");

    // A real corrupted blob: the "AB" child carries an out-of-range active index.
    for (int corrupt : { -100, -1, 2, 3, 99 })
    {
        auto xml = juce::parseXML ("<AB active=\"" + juce::String (corrupt) + "\"/>");
        check (xml != nullptr, "corrupted AB XML parses");
        if (xml == nullptr) continue; // check() does not abort -- guard the deref below

        auto ab = juce::ValueTree::fromXml (*xml);

        // EXACTLY mirrors PluginProcessor.cpp setStateInformation.
        const int slot = anamorph::clampAbSlotIndex ((int) ab.getProperty ("active", 0));
        check (slot >= 0 && slot < anamorph::kNumAbSlots,
               "corrupted active index clamps to a valid in-bounds A/B slot");
    }

    // Valid states (0 = A, 1 = B) must round-trip UNCHANGED (no behaviour change).
    for (int valid : { 0, 1 })
    {
        auto xml = juce::parseXML ("<AB active=\"" + juce::String (valid) + "\"/>");
        check (xml != nullptr, "valid AB XML parses");
        if (xml == nullptr) continue; // guard the deref (check() does not abort)

        auto ab  = juce::ValueTree::fromXml (*xml);
        const int slot = anamorph::clampAbSlotIndex ((int) ab.getProperty ("active", 0));
        check (slot == valid, "valid active index is preserved exactly");
    }
}

// ---------------------------------------------------------------------------
//  H4 (Wave 2) comb regression: with Multiband on and Mix parked at exactly 1
//  (Match off, no crossfade in flight) the dry-align bank is gated off. A Mix
//  dip must re-engage it phase-matched -- a dry bank that came back stale or
//  unsynced would comb the mono sum exactly like pre-KI-#1. Same metric as
//  Test 7: mono-sum RMS out/in (unaligned dry combs to <0.8).
static void testDryAlignGateRecomb()
{
    std::printf ("Test 25: dry-align gate (H4) -- Mix re-engage after a gated stretch stays comb-free\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;
    p.mbEnable   = true;
    p.mbBands    = 4;
    p.mbWidthLow = p.mbWidthMid = p.mbWidthHiMid = p.mbWidthHigh = 1.0f; // pure allpass wet
    p.mix        = 1.0f; // gate active: dry bank cold
    engine.setParameters (p);
    engine.reset();

    double inSq = 0.0, outSq = 0.0;
    double inSqTr = 0.0, outSqTr = 0.0;
    for (int nb = 0; nb < 160; ++nb)
    {
        if (nb == 80) p.mix = 0.5f; // dip: the gated bank must re-engage aligned
        juce::AudioBuffer<float> buf (2, block);
        fillNoise (buf, (unsigned) (nb * 13 + 5));
        double blkIn = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const float mono = buf.getSample (0, i) + buf.getSample (1, i);
            const double m = static_cast<double> (mono);
            blkIn += m * m;
        }
        engine.setParameters (p);
        engine.process (buf);
        double blkOut = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const float mono = buf.getSample (0, i) + buf.getSample (1, i);
            const double m = static_cast<double> (mono);
            blkOut += m * m;
        }
        if (nb >= 80 && nb < 90)  { inSqTr += blkIn; outSqTr += blkOut; } // transition window
        if (nb >= 100)            { inSq   += blkIn; outSq   += blkOut; } // settled at mix 0.5
    }
    const double trRatio = std::sqrt (outSqTr / juce::jmax (1.0e-12, inSqTr));
    const double ratio   = std::sqrt (outSq   / juce::jmax (1.0e-12, inSq));
    std::printf ("  transition mono-sum RMS out/in = %.3f, settled = %.3f (unaligned combs to <0.8)\n",
                 trRatio, ratio);
    check (trRatio > 0.90, "mono sum survives the re-engage transition (bank re-warms masked)");
    check (ratio   > 0.95, "mono sum intact once re-engaged (dry/wet phase-aligned again)");
}

// ---------------------------------------------------------------------------
//  Undo/redo dropout guard: a FORCED bulk swap (undo / redo / A/B / preset --
//  requestDuck() + setParameters(), exactly what the wrapper's undo() does) must
//  no longer pass through silence. The forced duck is dry-filled with the delay-
//  aligned raw input (the true-bypass ring), so short-window RMS across the swap
//  must stay near the steady level. The pre-fix engine multiplied the output by a
//  raised cosine that reached exactly 0 and dwelt there (~6 ms out + up to one
//  block of zeros + a slow 28 ms in): its minimum window RMS is ~0, which this
//  test rejects. (A latency-crossing forced swap deliberately keeps the original
//  duck-to-silence -- the ring read offset would jump at full dry weight -- and
//  is not asserted here.)
static void testForcedSwapNoDropout()
{
    std::printf ("Test 26: a forced bulk swap (undo / A-B / preset) never dips to silence\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 128;
    const double freq = 220.0;
    const float amp = 0.25f;

    struct Scenario { const char* name; anamorph::EngineParameters from, to; double minRatio; };
    Scenario scenarios[2];
    // 1) Continuous-only bulk swap on a near-transparent chain: raw and processed
    //    carry the same sine, so any dip below ~85 % of steady is the duck itself.
    scenarios[0].name = "continuous bulk swap (width/mix)";
    scenarios[0].from.width = 1.3f;
    scenarios[0].to.width = 0.8f; scenarios[0].to.mix = 0.9f;
    scenarios[0].minRatio = 0.85;
    // 2) Algorithm-carrying swap under real processing (Velvet 0.5 -> Haas 0.4,
    //    OS off so the swap is latency-neutral): the dry fill crossfades toward
    //    decorrelated wet, so allow interference dips but never a dropout. The
    //    pre-fix duck still bottoms at ~0 here (fails any positive floor).
    scenarios[1].name = "algorithm bulk swap (velvet -> haas)";
    scenarios[1].from.algorithm = anamorph::Algorithm::Velvet;
    scenarios[1].from.algoAmount = 0.5f;
    scenarios[1].to.algorithm = anamorph::Algorithm::Haas;
    scenarios[1].to.algoAmount = 0.4f;
    scenarios[1].minRatio = 0.35;

    for (const auto& sc : scenarios)
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        engine.setParameters (sc.from);
        engine.reset();

        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * freq / sr;
        const int settleBlocks = 375;              // 1 s to settle every glide
        const int steadyBlocks = 38;               // ~100 ms steady reference
        const int swapBlocks   = 38;               // ~100 ms covering the whole duck (~34 ms)
        const int win = 96;                        // 2 ms RMS windows (bottom dwell is >= a block)

        auto runBlock = [&] (juce::AudioBuffer<float>& buf)
        {
            for (int i = 0; i < block; ++i)
            {
                const float s = amp * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            engine.process (buf);
        };

        juce::AudioBuffer<float> buf (2, block);
        auto p = sc.from;
        for (int nb = 0; nb < settleBlocks; ++nb) { engine.setParameters (p); runBlock (buf); }

        // Windowed RMS (mono sum of both channels) over a span of blocks.
        double winSq = 0.0; int winN = 0; double minWin = 1.0e9;
        auto scanWindows = [&] (const juce::AudioBuffer<float>& b)
        {
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
                winSq += (double) v * v;
                if (++winN == win)
                {
                    minWin = std::min (minWin, std::sqrt (winSq / win));
                    winSq = 0.0; winN = 0;
                }
            }
        };

        double steadySq = 0.0; long steadyN = 0;
        for (int nb = 0; nb < steadyBlocks; ++nb)
        {
            engine.setParameters (p); runBlock (buf);
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                steadySq += (double) v * v; ++steadyN;
            }
        }
        const double steadyRms = std::sqrt (steadySq / (double) steadyN);

        engine.requestDuck();                      // the wrapper's undo()/redo() shape
        p = sc.to;
        bool bad = false;
        for (int nb = 0; nb < swapBlocks; ++nb)
        {
            engine.setParameters (p); runBlock (buf);
            scanWindows (buf);
            for (int i = 0; i < block; ++i)
                if (isBad (buf.getSample (0, i)) || isBad (buf.getSample (1, i))) bad = true;
        }

        const double ratio = minWin / juce::jmax (1.0e-12, steadyRms);
        std::printf ("  %s: min 2 ms window RMS across the swap = %.3f of steady (pre-fix ~0)\n",
                     sc.name, ratio);
        check (! bad, "forced-swap stream is free of NaN/Inf/denormals");
        check (ratio > sc.minRatio, "forced bulk swap keeps audio present (no silent gap)");
    }
}

// ---------------------------------------------------------------------------
//  Rapid consecutive forced swaps: a SECOND forced swap arrives while the first
//  forced duck is still fading in. The second must RE-EVALUATE its dry-fill
//  against the state being heard now -- it must never reuse the first swap's
//  stale dryDuck / dryDuckLat. The discriminating case: the first swap is
//  latency-CROSSING (engages oversampling: dry-fill correctly disabled, ducks to
//  silence), then the second swap is latency-NEUTRAL relative to that new state.
//  The correct engine re-latches and dry-fills the second swap, so audio stays
//  present through it; the pre-fix engine kept the stale "no dry-fill" decision
//  and dipped the second swap to silence too. A control case (both swaps
//  latency-neutral) confirms the ordinary rapid pair stays dry-filled.
static void testRapidForcedSwapDryFill()
{
    std::printf ("Test 27: rapid consecutive forced swaps re-evaluate dry-fill (no stale state)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 128;
    const double freq = 220.0;
    const float amp = 0.25f;

    // Fire swap1 (settled), then swap2 a few blocks later. @128/48k the fade-out is
    // ~6 ms (~2.25 blocks, blocks 6..8) and the fade-in ~28 ms (~blocks 8..19), so
    // swap2 at block 12 lands in the fade-IN and block 7 lands in the fade-OUT.
    const int settle    = 375;   // 1 s to settle `from`
    const int swap1At    = 6;     // blocks after settle
    const int tail       = 40;    // run past swap2's fade-in
    const int win        = 96;    // 2 ms RMS window

    struct Case { const char* name; anamorph::EngineParameters from, swap1, swap2;
                  int s2at; bool assertSwap2Present; bool assertSwap2Silent; };
    Case cases[4] {};

    // Control: both swaps latency-neutral (OS off throughout). Ordinary rapid undo
    // pair -- must stay dry-filled the whole way.
    cases[0].name = "neutral -> neutral (control)";
    cases[0].from.width = 1.3f;
    cases[0].swap1.width = 0.8f;
    cases[0].swap2.width = 1.5f; cases[0].swap2.mix = 0.85f;
    cases[0].s2at = 12; cases[0].assertSwap2Present = true;

    // Discriminating A: swap1 engages oversampling (latency-crossing: correctly ducks
    // to silence), swap2 keeps OS on (latency-neutral vs swap1) and only moves a
    // continuous control -- the correct engine RE-ENABLES dry-fill at the NEW latency
    // offset; the stale engine keeps the "no dry-fill" decision and dips to silence.
    cases[1].name = "latency-cross -> neutral (re-enable dry-fill)";
    cases[1].from.algorithm = anamorph::Algorithm::Chorus; cases[1].from.algoAmount = 0.3f; // OS off (default)
    cases[1].swap1 = cases[1].from; cases[1].swap1.oversample = anamorph::OversampleFactor::x4; // OS engages -> latency
    cases[1].swap2 = cases[1].swap1; cases[1].swap2.width = 1.6f; // OS stays on: neutral, continuous-only
    cases[1].s2at = 12; cases[1].assertSwap2Present = true;

    // Discriminating B (reverse latency direction): swap1 latency-neutral (dry-fills),
    // swap2 ENGAGES oversampling during swap1's fade-in (latency-crossing). The correct
    // engine re-evaluates dryDuck=false and duck-to-silences swap2 (a latency change
    // cannot be dry-filled seamlessly -- the ring offset would jump); the stale engine
    // keeps swap1's dryDuck=true + offset 0 and dry-fills at the WRONG offset. So the
    // correct engine reaches near-silence at swap2's bottom, the stale one does not.
    cases[2].name = "neutral -> latency-cross during fade-IN (disable dry-fill, no wrong-offset read)";
    cases[2].from.algorithm = anamorph::Algorithm::Chorus; cases[2].from.algoAmount = 0.3f; // OS off
    cases[2].swap1 = cases[2].from; cases[2].swap1.width = 0.8f;                            // neutral (OS off)
    cases[2].swap2 = cases[2].swap1; cases[2].swap2.oversample = anamorph::OversampleFactor::x4; // OS engages -> latency
    cases[2].s2at = 12; cases[2].assertSwap2Silent = true;

    // Discriminating C: the FADE-OUT retarget/tighten path. swap2 arrives while swap1
    // is still FADING OUT (before the silent bottom), so it hits the "else if
    // (pendingForced)" AND-down branch rather than the FadeIn re-duck. swap1 is
    // neutral (dry-fills); swap2 turns latency-crossing, so the tighten must set
    // dryDuck=false and the swap must reach silence. The stale engine leaves swap1's
    // dryDuck=true + offset 0 in place and dry-fills at the wrong offset (stays
    // present). Exercises the tighten branch that the fade-IN cases do not.
    cases[3].name = "neutral -> latency-cross during fade-OUT (tighten branch)";
    cases[3].from.algorithm = anamorph::Algorithm::Chorus; cases[3].from.algoAmount = 0.3f; // OS off
    cases[3].swap1 = cases[3].from; cases[3].swap1.width = 0.8f;                            // neutral (OS off)
    cases[3].swap2 = cases[3].swap1; cases[3].swap2.oversample = anamorph::OversampleFactor::x4; // OS engages -> latency
    cases[3].s2at = 7; cases[3].assertSwap2Silent = true; // block 7 = during swap1's fade-out

    for (const auto& c : cases)
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        engine.setParameters (c.from);
        engine.reset();

        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * freq / sr;
        juce::AudioBuffer<float> buf (2, block);
        auto runBlock = [&]
        {
            for (int i = 0; i < block; ++i)
            {
                const float s = amp * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            engine.process (buf);
        };

        for (int nb = 0; nb < settle; ++nb) { engine.setParameters (c.from); runBlock(); }

        // Settled reference at the FINAL state (swap2 target), measured after the
        // whole sequence -- so the ratio is level-matched to what swap2 dry-fills to.
        // Windowed-RMS scan with a min captured only over blocks >= swap2At.
        double winSq = 0.0; int winN = 0; double minAfterSwap2 = 1.0e9;
        bool bad = false;
        auto p = c.from;
        const int total = c.s2at + tail;
        for (int nb = 0; nb < total; ++nb)
        {
            if (nb == swap1At) { engine.requestDuck(); p = c.swap1; }
            if (nb == c.s2at)  { engine.requestDuck(); p = c.swap2; }
            engine.setParameters (p);
            runBlock();
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                if (isBad (buf.getSample (0, i)) || isBad (buf.getSample (1, i))) bad = true;
                winSq += (double) v * v;
                if (++winN == win)
                {
                    const double r = std::sqrt (winSq / win);
                    if (nb >= c.s2at) minAfterSwap2 = std::min (minAfterSwap2, r);
                    winSq = 0.0; winN = 0;
                }
            }
        }
        // Settled RMS at the final state.
        double stSq = 0.0; long stN = 0;
        for (int nb = 0; nb < 40; ++nb)
        {
            engine.setParameters (c.swap2); runBlock();
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                stSq += (double) v * v; ++stN;
            }
        }
        const double steadyRms = std::sqrt (stSq / (double) stN);
        const double ratio = minAfterSwap2 / juce::jmax (1.0e-12, steadyRms);
        std::printf ("  %s: min 2 ms window RMS through swap2 = %.3f of steady (stale-state engine ~0)\n",
                     c.name, ratio);
        check (! bad, "rapid forced-swap stream is free of NaN/Inf/denormals");
        if (c.assertSwap2Present)
            check (ratio > 0.30, "second forced swap re-evaluates dry-fill and keeps audio present");
        if (c.assertSwap2Silent)
            check (ratio < 0.15, "latency-crossing second swap re-evaluates to duck-to-silence (no stale wrong-offset dry read)");
    }
}

// ---------------------------------------------------------------------------
//  Multiband flat recombination: at UNIT width the recombined output must be
//  flat (an allpass reconstruction), even when the crossovers are close. The
//  naive serial split-and-sum was NOT phase-compensated, so close splits combed
//  a deep magnitude dip around the crossover region (measured -17.75 dB at three
//  close splits) -- the "EQ cut" users reported. The phase-compensated
//  reconstruction telescopes to a true allpass, so the impulse-response
//  magnitude stays within a fraction of a dB of 0 across the band.
static void testMultibandFlatRecombination()
{
    std::printf ("Test 28: multiband reconstruction is flat (no EQ dip at close crossovers)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 128;

    // Worst in-band magnitude deviation (dB) of the unit-width recombination,
    // via the mono impulse response FFT (mono: width is irrelevant, pure Mid).
    auto worstDeviationDb = [&] (float f1, float f2, float f3, int bands) -> double
    {
        anamorph::MultibandWidth mb;
        mb.prepare (sr, block);
        mb.setBandCount (bands);
        mb.setWidths (1.0f, 1.0f, 1.0f, 1.0f);
        mb.setCrossovers (f1, f2, f3);

        // Settle the cutoff glide (state decays to ~0) with ~1 s of zeros.
        std::vector<float> z (block, 0.0f), z2 (block, 0.0f);
        for (int b = 0; b < (int) (sr / block); ++b)
        {
            std::fill (z.begin(), z.end(), 0.0f); std::fill (z2.begin(), z2.end(), 0.0f);
            mb.processBlock (z.data(), z2.data(), block);
        }

        const int order = 14, N = 1 << order; // 16384
        std::vector<float> ir ((size_t) N, 0.0f);
        for (int i = 0; i < N; i += block)
        {
            const int n = std::min (block, N - i);
            std::vector<float> bl ((size_t) n, 0.0f), br ((size_t) n, 0.0f);
            if (i == 0) { bl[0] = 1.0f; br[0] = 1.0f; }
            mb.processBlock (bl.data(), br.data(), n);
            for (int k = 0; k < n; ++k) ir[(size_t) (i + k)] = bl[(size_t) k];
        }

        juce::dsp::FFT fft (order);
        std::vector<float> fd ((size_t) (2 * N), 0.0f);
        for (int i = 0; i < N; ++i) fd[(size_t) i] = ir[(size_t) i];
        fft.performRealOnlyForwardTransform (fd.data());

        double worst = 0.0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double hz = (double) k * sr / N;
            if (hz < 40.0 || hz > 18000.0) continue; // ignore the extreme band edges
            const double re = fd[(size_t) (2 * k)], im = fd[(size_t) (2 * k + 1)];
            const double db = 20.0 * std::log10 (std::max (1.0e-9, std::sqrt (re * re + im * im)));
            worst = std::min (worst, db); // most-negative deviation from 0 dB
        }
        return worst;
    };

    struct Cfg { const char* name; float f1, f2, f3; int bands; };
    const Cfg cfgs[] = {
        { "4-band, three close splits (800/1000/1250)", 800.0f, 1000.0f, 1250.0f, 4 },
        { "4-band, very close (900/1000/1100)",         900.0f, 1000.0f, 1100.0f, 4 },
        { "4-band, wide (200/1000/5000)",               200.0f, 1000.0f, 5000.0f, 4 },
        { "3-band (500/2000)",                          500.0f, 2000.0f, 8000.0f, 3 },
        { "2-band (single crossover)",                  1000.0f, 2000.0f, 4000.0f, 2 },
    };
    for (const auto& c : cfgs)
    {
        const double dip = worstDeviationDb (c.f1, c.f2, c.f3, c.bands);
        std::printf ("  %-44s worst deviation = %+.2f dB (pre-fix close splits combed to -17 dB)\n", c.name, dip);
        check (dip > -0.5, "multiband recombination stays flat (no EQ dip around crossovers)");
    }
}

// ---------------------------------------------------------------------------
//  Split movement must keep its FM within the ACCEPTED CONTROLLED BOUND
//  (0.8.10 final, four design rounds). A swept IIR crossover shifts every
//  frequency by dphi/dt (0.312*R Hz at sweep rate R oct/s); the shipped design
//  caps the cutoff sweep at ~4 oct/s -- a deliberate product trade (a small
//  controlled FM over interaction latency): drags up to 4 oct/s track EXACTLY,
//  and the worst crossing shift is ~1.25 Hz (~15 cents at 150 Hz, ~half the
//  original uncapped implementation) -- plus a single ~12 ms bank crossfade
//  only for DISCRETE multi-octave target steps. This test rejects the failed
//  designs with two measurements on both crossover consumers (Multiband
//  reconstruction + Band Solo monitor):
//   * the pitch check tracks a 150 Hz tone through the ENTIRE drag + catch-up,
//     including the moment the crossover crosses the tone: the shipped ~4 oct/s
//     cap measures ~14-16 cents there; the uncapped pre-0.8.10 glide (8 oct/s)
//     measures ~28-31 and the one-pole tracker ~50 -- both fail the 18-cent
//     bound.
//   * the spectral-purity check bounds spurs around a 1 kHz tone during a fast
//     60 Hz-cadence drag: the chained bank crossfades (first design) measure
//     -28.5 dBc there and fail.
//  Plus: a released flick must land by PLAIN GLIDING in bounded time (~1.5 s
//  for a violent 6-oct flick; the rejected 1.25 oct/s follower was still at
//  full lag there), a discrete 4-octave jump must land fast (bank fade, not a
//  crawl), and every stream must stay click-free.
static void testMultibandSplitDragNoPitchShift()
{
    std::printf ("Test 29: fast split drags do not pitch-shift (multiband + solo monitor)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 128;
    const double tone = 150.0;
    const float amp = 0.25f;

    // Worst |cents| deviation from `tone` over ~100 ms chunks of s[start..end),
    // measured from interpolated positive-going zero crossings (sub-sample
    // precision; a steady allpass-filtered sine measures ~0 cents).
    auto worstCents = [&] (const std::vector<float>& s, int start, int end) -> double
    {
        double worst = 0.0;
        const int chunk = (int) (0.1 * sr);
        for (int c0 = start; c0 + chunk <= end; c0 += chunk)
        {
            double first = -1.0, last = -1.0;
            int periods = 0;
            for (int i = c0 + 1; i < c0 + chunk; ++i)
                if (s[(size_t) (i - 1)] <= 0.0f && s[(size_t) i] > 0.0f)
                {
                    const double dy = (double) s[(size_t) i] - (double) s[(size_t) (i - 1)];
                    const double t  = (i - 1) + (dy > 0.0 ? -(double) s[(size_t) (i - 1)] / dy : 0.0);
                    if (first < 0.0) first = t;
                    else             { last = t; ++periods; }
                }
            if (periods < 3 || last <= first) continue;
            const double f = (double) periods * sr / (last - first);
            worst = std::max (worst, std::abs (1200.0 * std::log2 (f / tone)));
        }
        return worst;
    };

    // Drive `step` with a DOWNWARD split drag (start -> start*2^-octs over
    // 0.25 s at block cadence ~ a UI drag), then hold the target. Drags up to
    // the frequency-proportional cap R(f) = 4 * max(1, f/300) oct/s track
    // 1:1 (plus the ~20 ms ease); a faster flick leaves a residual lag that
    // keeps gliding at the cap until it lands -- continuous motion, no fades,
    // no timers (the 0.8.10 final follower + slow-drag fix).
    auto runDrag = [&] (float startHz, float octsDown, auto&& setSplit, auto&& step,
                        int totalBlocks) -> std::vector<float>
    {
        std::vector<float> outStream;
        outStream.reserve ((size_t) (totalBlocks * block));
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * tone / sr;
        std::vector<float> l ((size_t) block), r ((size_t) block);
        for (int nb = 0; nb < totalBlocks; ++nb)
        {
            const double t = (double) (nb * block) / sr;
            const double dragT = juce::jlimit (0.0, 1.0, t / 0.25);
            setSplit (startHz * (float) std::exp2 (-octsDown * dragT));
            for (int i = 0; i < block; ++i)
            {
                l[(size_t) i] = r[(size_t) i] = amp * (float) std::sin (phase);
                phase += inc;
            }
            step (l.data(), r.data(), block);
            for (int i = 0; i < block; ++i) outStream.push_back (l[(size_t) i]);
        }
        return outStream;
    };

    // Worst 100 ms pitch chunk inside the given windows must stay below the
    // ACCEPTED CONTROLLED-FM bound of 18 cents: under the R(f) cap a 150 Hz
    // crossing happens at ~4 oct/s and measures ~14-17 cents (the deliberate
    // product trade, ADR-0015 refinement); the uncapped pre-0.8.10 glide
    // measures ~28-31 and the bare one-pole tracker ~50 -- both fail. No fade
    // ever fires during a drag, so one unbroken window can span the whole
    // drag + catch-up.
    auto validate = [&] (const char* name, const std::vector<float>& s,
                         std::initializer_list<std::pair<double, double>> windows)
    {
        double cents = 0.0;
        for (const auto& w : windows)
            cents = std::max (cents, worstCents (s, (int) (w.first * sr),
                                                 (int) juce::jmin ((double) s.size(), w.second * sr)));
        double maxDelta = 0.0;
        bool bad = false;
        for (size_t i = 1; i < s.size(); ++i)
        {
            if (isBad (s[i])) bad = true;
            if (i > (size_t) (0.02 * sr)) // skip the initial filter charge-up
                maxDelta = std::max (maxDelta, (double) std::abs (s[i] - s[i - 1]));
        }
        std::printf ("  %-13s worst pitch deviation = %.2f cents (uncapped: 28+, one-pole: ~50); max delta = %.4f\n",
                     name, cents, maxDelta);
        check (! bad, "split-drag stream is free of NaN/Inf");
        check (cents < 18.0, "split-move FM stays within the accepted controlled bound");
        check (maxDelta < 0.04, "no click during / after the split drag");
    };

    {
        anamorph::MultibandWidth mb;
        mb.prepare (sr, block);
        mb.setBandCount (2);
        mb.setWidths (1.0f, 1.0f, 1.0f, 1.0f);
        mb.setCrossovers (6400.0f, 8000.0f, 16000.0f);
        std::vector<float> z ((size_t) block, 0.0f), z2 ((size_t) block, 0.0f);
        for (int nb = 0; nb < 40; ++nb) // settle from prepare defaults
        {
            std::fill (z.begin(), z.end(), 0.0f);
            std::fill (z2.begin(), z2.end(), 0.0f);
            mb.processBlock (z.data(), z2.data(), block);
        }
        // 6-octave flick: the target lands in 0.25 s; the bank keeps gliding
        // under the R(f) cap (fast down to 300 Hz, then the flat 4 oct/s
        // floor) and lands well under a second in -- continuous motion, no
        // fade, so one unbroken window spans the whole drag + catch-up.
        auto s = runDrag (6400.0f, 6.0f,
                          [&] (float f) { mb.setCrossovers (f, 8000.0f, 16000.0f); },
                          [&] (float* L, float* R, int n) { mb.processBlock (L, R, n); },
                          (int) (2.5 * sr) / block);
        validate ("multiband:", s, { { 0.05, 2.40 } });
    }

    {
        // Moderate drag (300 -> 110 Hz, 1.45 oct in 0.25 s): the glide carries
        // the crossover down PAST the 150 Hz tone at the ~4 oct/s cap -- the
        // sustained-FM regression proper: the crossing must stay within the
        // controlled bound in an unbroken window (no fade fires).
        anamorph::MultibandWidth mb;
        mb.prepare (sr, block);
        mb.setBandCount (2);
        mb.setWidths (1.0f, 1.0f, 1.0f, 1.0f);
        mb.setCrossovers (300.0f, 8000.0f, 16000.0f);
        std::vector<float> z ((size_t) block), z2 ((size_t) block);
        for (int nb = 0; nb < 40; ++nb)
        {
            std::fill (z.begin(), z.end(), 0.0f);
            std::fill (z2.begin(), z2.end(), 0.0f);
            mb.processBlock (z.data(), z2.data(), block);
        }
        auto s = runDrag (300.0f, 1.4497f, // -> ~110 Hz
                          [&] (float f) { mb.setCrossovers (f, 8000.0f, 16000.0f); },
                          [&] (float* L, float* R, int n) { mb.processBlock (L, R, n); },
                          (int) (2.5 * sr) / block);
        validate ("crawl-cross:", s, { { 0.05, 2.40 } });
    }

    // --- spectral purity while the split moves (the 0.8.10 sine report) ------
    // A pure 1 kHz tone while the split is dragged 250 -> 4000 Hz across it in
    // 0.25 s. The chained fixed-bank crossfades of the first 0.8.10 fix were
    // amplitude/phase modulation at the fade cadence and sprayed sidebands
    // around the tone (max spur ~ -26 dBc on this scenario -- audibly "new
    // frequencies around the original tone"); the rate-capped glide is a true
    // allpass at every instant and measures at the ~ -37 dBc analysis floor
    // (the pre-0.8.10 uncapped glide also passes this check -- it failed on
    // pitch, which the checks above cover). Max spur = the strongest spectral
    // component more than +-30 Hz from the tone, relative to the tone, over
    // sliding 100 ms Hann windows spanning the drag.
    {
        const double spurTone = 1000.0;
        anamorph::MultibandWidth mb;
        mb.prepare (sr, block);
        mb.setBandCount (2);
        mb.setWidths (1.0f, 1.0f, 1.0f, 1.0f);
        mb.setCrossovers (250.0f, 8000.0f, 16000.0f);
        std::vector<float> z ((size_t) block), z2 ((size_t) block);
        for (int nb = 0; nb < 40; ++nb)
        {
            std::fill (z.begin(), z.end(), 0.0f);
            std::fill (z2.begin(), z2.end(), 0.0f);
            mb.processBlock (z.data(), z2.data(), block);
        }

        std::vector<float> s;
        s.reserve ((size_t) sr);
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * spurTone / sr;
        std::vector<float> l ((size_t) block), r ((size_t) block);
        const int totalBlocks = (int) (0.6 * sr) / block;
        for (int nb = 0; nb < totalBlocks; ++nb)
        {
            // Quantize the target stream to a ~60 Hz UI cadence: a real mouse
            // drag delivers stepped targets, and the fade-chain artifact this
            // check guards against is strongest against stepped targets (a
            // per-block-smooth ramp lets even the fade chain slip through).
            const double t = std::floor ((double) (nb * block) / sr * 60.0) / 60.0;
            const double dragT = juce::jlimit (0.0, 1.0, t / 0.25);
            mb.setCrossovers (250.0f * (float) std::exp2 (4.0 * dragT), 8000.0f, 16000.0f);
            for (int i = 0; i < block; ++i)
            {
                l[(size_t) i] = r[(size_t) i] = amp * (float) std::sin (phase);
                phase += inc;
            }
            mb.processBlock (l.data(), r.data(), block);
            for (int i = 0; i < block; ++i) s.push_back (l[(size_t) i]);
        }

        const int fftOrder = 15, N = 1 << fftOrder; // 32768 (window zero-padded)
        juce::dsp::FFT fft (fftOrder);
        const int win = (int) (0.1 * sr), hop = win / 4;
        std::vector<float> fd ((size_t) (2 * N));
        double worstSpur = -200.0;
        for (int start = (int) (0.05 * sr); start + win <= (int) (0.30 * sr); start += hop)
        {
            std::fill (fd.begin(), fd.end(), 0.0f);
            for (int i = 0; i < win; ++i)
            {
                const float w = 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi * i / (win - 1));
                fd[(size_t) i] = s[(size_t) (start + i)] * w;
            }
            fft.performRealOnlyForwardTransform (fd.data());
            double carrier = 0.0, spur = 0.0;
            for (int k = 1; k < N / 2; ++k)
            {
                const double hz = (double) k * sr / N;
                if (hz < 20.0 || hz > 20000.0) continue;
                const double re = fd[(size_t) (2 * k)], im = fd[(size_t) (2 * k + 1)];
                const double mag = std::sqrt (re * re + im * im);
                if (std::abs (hz - spurTone) < 30.0) carrier = std::max (carrier, mag);
                else                                 spur    = std::max (spur, mag);
            }
            if (carrier > 0.0)
                worstSpur = std::max (worstSpur, 20.0 * std::log10 (spur / carrier));
        }
        std::printf ("  multiband:    max spur while the split crosses a 1 kHz tone = %+.1f dBc (chained fades: ~-26; threshold -31)\n",
                     worstSpur);
        check (worstSpur < -31.0, "no modulation sidebands around a pure tone while the split moves");
    }

    {
        // The band-solo whole-band drag: band 0 stays soloed while its upper
        // split crawls down past the tone. The tone ends up outside the soloed
        // band (LP4 at 100 Hz leaves ~-14 dB of the 150 Hz sine), but it stays a
        // clean measurable sine throughout -- the crossing itself must not bend
        // its pitch beyond the JND bound.
        anamorph::SoloMonitor mon;
        mon.prepare (sr, block);
        mon.setBandCount (2);
        mon.setCrossovers (6400.0f, 8000.0f, 16000.0f);
        // Engage solo on band 0 (contains the tone) and let the crossfade settle.
        std::vector<float> l ((size_t) block), r ((size_t) block);
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * tone / sr;
        for (int nb = 0; nb < 40; ++nb)
        {
            for (int i = 0; i < block; ++i)
            {
                l[(size_t) i] = r[(size_t) i] = amp * (float) std::sin (phase);
                phase += inc;
            }
            mon.process (l.data(), r.data(), 0x1, block);
        }
        auto s = runDrag (6400.0f, 6.0f,
                          [&] (float f) { mon.setCrossovers (f, 8000.0f, 16000.0f); },
                          [&] (float* L, float* R, int n) { mon.process (L, R, 0x1, n); },
                          (int) (2.5 * sr) / block);
        validate ("solo monitor:", s, { { 0.05, 2.40 } });

        // BOUNDED CATCH-UP (0.8.10 final): the soloed band is LP(f1); once the
        // glide lands f1 at ~100 Hz, the 150 Hz tone must be attenuated
        // (~-14 dB). Under the R(f) cap a 6-oct flick lands well under a
        // second in; the rejected 1.25 oct/s follower was still ~2 octaves
        // high at 2 s, so the tone sat at FULL level in this window and the
        // check fails.
        double sq = 0.0; int cnt = 0;
        for (int i = (int) (1.7 * sr); i < (int) (2.2 * sr) && i < (int) s.size(); ++i)
        {
            sq += (double) s[(size_t) i] * s[(size_t) i]; ++cnt;
        }
        const double rms = std::sqrt (sq / juce::jmax (1, cnt));
        const double fullRms = amp / std::sqrt (2.0);
        std::printf ("  convergence:  level 1.7-2.2 s after a 6-oct flick = %.2f of full (1.25 oct/s follower: ~1.0)\n",
                     rms / fullRms);
        check (rms < 0.45 * fullRms, "a released flick lands in bounded time (~1.5 s for 6 oct), not seconds");
    }

    {
        // DISCRETE jumps must LAND fast via the bank crossfade, never crawl:
        // solo band 0 and step its upper split 250 -> 4000 Hz in ONE call
        // (> 1.5 oct between consecutive blocks). A 1 kHz tone sits ~ -48 dB
        // outside the soloed band before the jump and at full level inside it
        // after -- the level must arrive within ~200 ms (even the R(f)-capped
        // glide would need ~0.4 s), click-free.
        const double jumpTone = 1000.0;
        anamorph::SoloMonitor mon;
        mon.prepare (sr, block);
        mon.setBandCount (2);
        mon.setCrossovers (250.0f, 8000.0f, 16000.0f);
        std::vector<float> l ((size_t) block), r ((size_t) block);
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * jumpTone / sr;
        auto run = [&] (int blocks, std::vector<float>* cap)
        {
            for (int nb = 0; nb < blocks; ++nb)
            {
                for (int i = 0; i < block; ++i)
                {
                    l[(size_t) i] = r[(size_t) i] = amp * (float) std::sin (phase);
                    phase += inc;
                }
                mon.process (l.data(), r.data(), 0x1, block);
                if (cap != nullptr)
                    for (int i = 0; i < block; ++i) cap->push_back (l[(size_t) i]);
            }
        };
        run ((int) (0.5 * sr) / block, nullptr);           // settle soloed, tone rejected
        mon.setCrossovers (4000.0f, 8000.0f, 16000.0f);    // one 4-octave step
        std::vector<float> s;
        run ((int) (0.4 * sr) / block, &s);
        double sq = 0.0; int cnt = 0;
        for (int i = (int) (0.2 * sr); i < (int) (0.35 * sr); ++i) { sq += (double) s[(size_t) i] * s[(size_t) i]; ++cnt; }
        const double rms = std::sqrt (sq / juce::jmax (1, cnt));
        const double fullRms = amp / std::sqrt (2.0);
        double maxDelta = 0.0;
        for (size_t i = 1; i < s.size(); ++i)
            maxDelta = std::max (maxDelta, (double) std::abs (s[i] - s[i - 1]));
        std::printf ("  discrete 4-oct jump: level at +200..350 ms = %.2f of full (crawl would be ~0.004); max delta = %.4f\n",
                     rms / fullRms, maxDelta);
        check (rms > 0.7 * fullRms, "a discrete multi-octave split jump lands via the bank fade, not a crawl");
        check (maxDelta < 0.06, "the discrete-jump bank fade is click-free");
    }

    // --- NORMAL-DRAG TRACKING (the v0.8.10 slow-drag regression) -------------
    // The Multiband display spans ~10 octaves in ~900 px, so an ordinary
    // 600 px/s drag is ~6.6 oct/s -- ABOVE the old flat 4 oct/s cap. That cap
    // pinned the DSP split whole octaves behind the mouse for the entire drag
    // and let it crawl on for ~a second after release, while a violent flick
    // escaped through the discrete-jump fade and felt instant -- "slow drags
    // are limited harder than fast ones". Under the frequency-proportional cap
    // the split must arrive WITH the gesture: drag one split 150 Hz -> 12 kHz
    // over 0.95 s at a 60 Hz UI cadence (6.65 oct/s) and require the audible
    // band edge to be AT the target 0.1..0.35 s after release. The flat-cap
    // follower is still ~1.3 octaves shy at that point on both paths -- both
    // checks fail on it; the 20 ms ease of the fixed follower converges within
    // ~0.1 s.
    {
        // Solo-monitor path: band 0 soloed, a 4 kHz tone starts far outside
        // the LP band (split 150 Hz -> silent) and must sit at FULL level in
        // the post-release window once the split has climbed to 12 kHz.
        anamorph::SoloMonitor mon;
        mon.prepare (sr, block);
        mon.setBandCount (2);
        mon.setCrossovers (150.0f, 8000.0f, 16000.0f);
        const double dragTone = 4000.0;
        std::vector<float> l ((size_t) block), r ((size_t) block);
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * dragTone / sr;
        for (int nb = 0; nb < 40; ++nb) // settle the solo crossfade, tone rejected
        {
            for (int i = 0; i < block; ++i)
            {
                l[(size_t) i] = r[(size_t) i] = amp * (float) std::sin (phase);
                phase += inc;
            }
            mon.process (l.data(), r.data(), 0x1, block);
        }
        std::vector<float> s;
        const int totalBlocks = (int) (1.5 * sr) / block;
        for (int nb = 0; nb < totalBlocks; ++nb)
        {
            const double t = std::floor ((double) (nb * block) / sr * 60.0) / 60.0;
            const double dragT = juce::jlimit (0.0, 1.0, t / 0.95);
            mon.setCrossovers (150.0f * (float) std::exp2 (6.3219 * dragT), 8000.0f, 16000.0f);
            for (int i = 0; i < block; ++i)
            {
                l[(size_t) i] = r[(size_t) i] = amp * (float) std::sin (phase);
                phase += inc;
            }
            mon.process (l.data(), r.data(), 0x1, block);
            for (int i = 0; i < block; ++i) s.push_back (l[(size_t) i]);
        }
        double sq = 0.0; int cnt = 0;
        for (int i = (int) (1.05 * sr); i < (int) (1.30 * sr) && i < (int) s.size(); ++i)
        {
            sq += (double) s[(size_t) i] * s[(size_t) i]; ++cnt;
        }
        const double rms = std::sqrt (sq / juce::jmax (1, cnt));
        const double fullRms = amp / std::sqrt (2.0);
        double maxDelta = 0.0;
        for (size_t i = 1; i < s.size(); ++i)
            maxDelta = std::max (maxDelta, (double) std::abs (s[i] - s[i - 1]));
        std::printf ("  normal drag:  solo band edge at +100..350 ms after release = %.2f of full (flat 4 oct/s cap: ~0.5); max delta = %.4f\n",
                     rms / fullRms, maxDelta);
        check (rms > 0.9 * fullRms, "a normal-speed drag's band edge arrives with the gesture (solo monitor)");
        // A full-level 4 kHz sine's own per-sample slope is amp*2*pi*4000/sr
        // ~= 0.131; a click would spike above it.
        check (maxDelta < 0.16, "the normal-speed drag is click-free (solo monitor)");
    }

    {
        // Multiband path, observed through the width routing: the tone plays
        // on the LEFT only and band 2 has width 0, so while the tone is ABOVE
        // the split it collapses to mono and leaks onto the RIGHT at half
        // level. Once the split passes it, the tone joins band 1 (width 1,
        // identity) and the RIGHT channel must fall silent in the same
        // post-release window.
        anamorph::MultibandWidth mb;
        mb.prepare (sr, block);
        mb.setBandCount (2);
        mb.setWidths (1.0f, 0.0f, 1.0f, 1.0f);
        mb.setCrossovers (150.0f, 8000.0f, 16000.0f);
        const double dragTone = 4000.0;
        std::vector<float> l ((size_t) block), r ((size_t) block);
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * dragTone / sr;
        for (int nb = 0; nb < 40; ++nb) // settle from prepare defaults
        {
            for (int i = 0; i < block; ++i)
            {
                l[(size_t) i] = amp * (float) std::sin (phase);
                r[(size_t) i] = 0.0f;
                phase += inc;
            }
            mb.processBlock (l.data(), r.data(), block);
        }
        std::vector<float> sR;
        const int totalBlocks = (int) (1.5 * sr) / block;
        for (int nb = 0; nb < totalBlocks; ++nb)
        {
            const double t = std::floor ((double) (nb * block) / sr * 60.0) / 60.0;
            const double dragT = juce::jlimit (0.0, 1.0, t / 0.95);
            mb.setCrossovers (150.0f * (float) std::exp2 (6.3219 * dragT), 8000.0f, 16000.0f);
            for (int i = 0; i < block; ++i)
            {
                l[(size_t) i] = amp * (float) std::sin (phase);
                r[(size_t) i] = 0.0f;
                phase += inc;
            }
            mb.processBlock (l.data(), r.data(), block);
            for (int i = 0; i < block; ++i) sR.push_back (r[(size_t) i]);
        }
        double sq = 0.0; int cnt = 0;
        for (int i = (int) (1.05 * sr); i < (int) (1.30 * sr) && i < (int) sR.size(); ++i)
        {
            sq += (double) sR[(size_t) i] * sR[(size_t) i]; ++cnt;
        }
        const double rms = std::sqrt (sq / juce::jmax (1, cnt));
        const double monoRms = 0.5 * amp / std::sqrt (2.0); // the width-0 mono leak level
        std::printf ("  normal drag:  multiband width-0 leak at +100..350 ms after release = %.2f of the leak level (flat cap: ~0.9)\n",
                     rms / monoRms);
        check (rms < 0.15 * monoRms, "a normal-speed drag's band edge arrives with the gesture (multiband)");
    }
}

// ---------------------------------------------------------------------------
//  The forced-duck dry fill must be presented at the OUTPUT-STAGE level, not at
//  raw unity (0.8.10 Task 4): with Output Gain at -24 dB, an undo/redo Mix
//  toggle used to burst the raw-level fill in up to 24 dB louder than the
//  surrounding processed audio. The fill gain is latched at fade-out entry, so
//  at unity gain the arithmetic is unchanged (Tests 26/27 cover that case).
static void testDryFillRespectsOutputGain()
{
    std::printf ("Test 30: forced-swap dry fill respects extreme Output Gain (no spike)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 128;
    const double freq = 220.0;
    const float amp = 0.25f;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p; // transparent defaults, OS off -> latency 0 (dry fill engages)
    p.outputGainDb = -24.0f;
    p.mix = 1.0f;
    engine.setParameters (p);
    engine.reset();

    double phase = 0.0;
    const double inc = 2.0 * juce::MathConstants<double>::pi * freq / sr;
    auto runBlocks = [&] (int blocks, double* outMaxAbs, bool* outBad)
    {
        for (int nb = 0; nb < blocks; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = amp * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            engine.setParameters (p);
            engine.process (buf);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (outBad != nullptr && isBad (v)) *outBad = true;
                    if (outMaxAbs != nullptr) *outMaxAbs = std::max (*outMaxAbs, (double) std::abs (v));
                }
        }
    };

    runBlocks (250, nullptr, nullptr); // settle at -24 dB
    double steadyPeak = 0.0; bool bad = false;
    runBlocks (40, &steadyPeak, &bad);

    // Undo-style forced swaps toggling Mix 1 <-> 0, tracking the transition peak.
    double transPeak = 0.0;
    for (int swap = 0; swap < 4; ++swap)
    {
        engine.requestDuck();
        p.mix = (swap % 2 == 0) ? 0.0f : 1.0f;
        runBlocks (60, &transPeak, &bad);  // ~160 ms: covers the whole duck + fill
    }

    std::printf ("  steady peak at -24 dB = %.4f ; worst transition peak = %.4f (%.1fx; raw-level fill spiked ~15x)\n",
                 steadyPeak, transPeak, transPeak / juce::jmax (1.0e-9, steadyPeak));
    check (! bad, "dry-filled swap stream at -24 dB is free of NaN/Inf");
    check (transPeak < 2.0 * steadyPeak, "no level spike: the dry fill follows the output-stage gain");
    check (transPeak > 0.25 * steadyPeak, "the dry fill still fills: the swap does not dip toward silence");
}

// ---------------------------------------------------------------------------
//  A forced bulk swap (undo / A-B / preset) can land while an ORDINARY discrete
//  duck is still fading OUT. The request is consumed from duckRequest on entry
//  to setParameters, so if the FadeOut path does not capture it the swap
//  finishes with normal-duck semantics: no wholesale swap at the silent bottom,
//  no smoother snap, and -- the observable used here -- no clean-slate reset,
//  so stale delay-line audio replays as the fade lifts. The fixed engine
//  upgrades the in-flight duck to a forced one (same fade, forced bottom).
//  Scenario A discriminates via a Haas delay line full of loud audio + silent
//  input: the forced bottom resets it (exact silence after the bottom); the
//  pre-fix ordinary bottom leaves it draining through the fade-in.
//  Scenario B guards the upgrade's transition quality on a steady sine: no
//  click at the upgrade moment, and the duck still bottoms at silence -- the
//  upgraded window deliberately keeps duck-to-silence (dry-fill is never
//  engaged mid-fade; the fresh-entry fill guarantee stays with Tests 26/27).
static void testForcedSwapDuringOrdinaryFadeOut()
{
    std::printf ("Test 31: a forced swap during an ordinary fade-out keeps forced semantics\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 128;                        // ~2.67 ms; fade-out ~6 ms spans ~2.25 blocks

    // --- Scenario A: stale-tail discriminator ------------------------------
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters from;          // Haas holds a 35 ms tail; OS off (latency 0)
        from.algorithm   = anamorph::Algorithm::Haas;
        from.algoAmount  = 1.0f;
        from.haasDelayMs = 35.0f;
        from.mix         = 1.0f;                  // wet-only: the tail is the whole output
        engine.setParameters (from);
        engine.reset();

        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 1000.0 / sr;
        juce::AudioBuffer<float> buf (2, block);
        auto runBlock = [&] (bool loud)
        {
            for (int i = 0; i < block; ++i)
            {
                const float s = loud ? 0.5f * (float) std::sin (phase) : 0.0f; phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            engine.process (buf);
        };

        for (int nb = 0; nb < 375; ++nb) { engine.setParameters (from); runBlock (true); }

        auto to = from;
        to.monoMakerEnable = true;                // duck-worthy discrete change, Haas untouched
        engine.setParameters (to);                // block 0: ordinary FadeOut begins
        runBlock (false);                         // input silent from here; tail keeps draining
        engine.requestDuck();                     // block 1 (~2.7 ms in, mid-fade-out):
        engine.setParameters (to);                //   the undo()/redo() shape lands mid-duck
        runBlock (false);

        bool bad = false; double postBottomMax = 0.0;
        for (int nb = 2; nb < 13; ++nb)           // bottom lands inside block 2 (~6 ms)
        {
            engine.setParameters (to); runBlock (false);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (isBad (v)) bad = true;
                    if (nb >= 3)                  // measure 8..35 ms: past the bottom, inside the tail
                        postBottomMax = std::max (postBottomMax, (double) std::abs (v));
                }
        }
        std::printf ("  A: max |out| after the silent bottom (silent input) = %.6f (pre-fix 0.494: stale Haas tail replays)\n",
                     postBottomMax);
        check (! bad, "upgraded-duck stream is free of NaN/Inf");
        check (postBottomMax < 1.0e-4, "forced bottom taken: stale delay-line audio does not replay");
    }

    // --- Scenario B: transition quality of the upgrade ---------------------
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters from;          // near-transparent defaults
        from.mix = 1.0f;
        engine.setParameters (from);
        engine.reset();

        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 220.0 / sr;
        const float amp = 0.25f;
        juce::AudioBuffer<float> buf (2, block);
        auto runBlock = [&]
        {
            for (int i = 0; i < block; ++i)
            {
                const float s = amp * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            engine.process (buf);
        };

        auto p = from;
        for (int nb = 0; nb < 375; ++nb) { engine.setParameters (p); runBlock(); }

        double steadySq = 0.0; long steadyN = 0;
        for (int nb = 0; nb < 38; ++nb)
        {
            engine.setParameters (p); runBlock();
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                steadySq += (double) v * v; ++steadyN;
            }
        }
        const double steadyRms = std::sqrt (steadySq / (double) steadyN);

        bool bad = false; double maxDelta = 0.0, minWinRms = 1.0e9;
        float prev = 0.0f; bool havePrev = false;
        double winSq = 0.0; int winN = 0; const int win = 96; // 2 ms windows
        auto scanBlock = [&]
        {
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                if (isBad (buf.getSample (0, i)) || isBad (buf.getSample (1, i))) bad = true;
                if (havePrev) maxDelta = std::max (maxDelta, (double) std::abs (v - prev));
                prev = v; havePrev = true;
                winSq += (double) v * v;
                if (++winN == win) { minWinRms = std::min (minWinRms, std::sqrt (winSq / win)); winSq = 0.0; winN = 0; }
            }
        };

        p.monoMakerEnable = true;                 // ordinary duck (input is dual-mono: level-neutral)
        engine.setParameters (p); runBlock(); scanBlock();
        engine.requestDuck();                     // forced swap lands mid-fade-out
        engine.setParameters (p); runBlock(); scanBlock();
        for (int nb = 0; nb < 36; ++nb) { engine.setParameters (p); runBlock(); scanBlock(); } // ~100 ms

        double tailSq = 0.0; long tailN = 0;      // settled level after the swap
        for (int nb = 0; nb < 38; ++nb)
        {
            engine.setParameters (p); runBlock();
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                tailSq += (double) v * v; ++tailN;
            }
        }
        const double tailRms = std::sqrt (tailSq / (double) tailN);

        std::printf ("  B: max sample delta %.4f (sine slope ~0.0072); duck floor %.3f of steady; recovery %.3f of steady\n",
                     maxDelta, minWinRms / steadyRms, tailRms / steadyRms);
        check (! bad, "upgrade transition stream is free of NaN/Inf");
        check (maxDelta < 0.02, "no click at the forced-upgrade moment (envelope stays smooth)");
        check (minWinRms < 0.10 * steadyRms, "upgraded duck still bottoms at silence (no mid-fade fill step)");
        check (tailRms > 0.9 * steadyRms && tailRms < 1.1 * steadyRms, "full recovery after the upgraded swap");
    }
}

// ---------------------------------------------------------------------------
static void testHighRateCrossoverSnap()
{
    std::printf ("Test 32: high-rate crossover snap lands exactly (192 kHz float stall)\n");

    // The cutoff glide's one-pole term gap*smoothCoeff shrinks with 1/sr but
    // the float lattice ulp(f) does not: the add f += move stops changing the
    // float once move < ulp(f)/2, a hard stall at a resting gap of
    // ulp(f)/(2*smoothCoeff). At 44.1/48/96 kHz the terminal-snap eps
    // (0.05 + 2e-4*f) covers that gap with a 1.76-4.3x margin, but at 192 kHz
    // the margin drops to 0.88-0.98x just past every binade edge >= 2048 Hz
    // (parameter-range stall zones [2049,2093] [4097,4437] [8194,9125]
    // [16388,18500] Hz; higher binades up to the 86.4 kHz DSP Nyquist clamp
    // stall too, same <= 0.4-cent resting error, covered by the same snap):
    // pre-fix the cutoff rested up to 3.75 Hz below target FOREVER -- audio
    // still correct (< 0.4 cents off), but cutoffs never equalled targets, so
    // the solo monitor's settled fast path could never engage and the filters
    // and smoothers stayed hot. The stall snap must land every cutoff EXACTLY
    // at 192 kHz, and at <= 96 kHz the eps snap must keep firing first
    // (unchanged behavior -- these rates pass pre-fix too).
    const int block = 512;

    // One target inside each of the three lower 192 kHz stall zones (all
    // Nyquist-safe at 44.1 kHz); the top zone is checked at 192 kHz below.
    const float startF [3] = { 2000.0f, 4040.0f, 8270.0f };
    const float targetF[3] = { 2080.0f, 4200.0f, 8600.0f };

    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::vector<float> l ((size_t) block, 0.0f), r ((size_t) block, 0.0f);
        const int glideBlocks  = (int) (1.2 * sr) / block; // ~90 ms one-pole + crawl, x10 margin
        const int settleBlocks = (int) (0.1 * sr) / block; // > the 12 ms gain crossfade

        // Multiband: the glide runs whenever bands > 1; step every split ~0.05
        // oct (glide path, far under the 1.5-oct fade threshold) and require
        // bitwise landing.
        anamorph::MultibandWidth mb;
        mb.setBandCount (4);
        mb.setWidths (1.0f, 1.0f, 1.0f, 1.0f);
        mb.setCrossovers (startF[0], startF[1], startF[2]);
        mb.prepare (sr, block);
        mb.setCrossovers (targetF[0], targetF[1], targetF[2]);
        for (int nb = 0; nb < glideBlocks; ++nb)
        {
            std::fill (l.begin(), l.end(), 0.0f);
            std::fill (r.begin(), r.end(), 0.0f);
            mb.processBlock (l.data(), r.data(), block);
        }
        bool mbExact = true;
        for (int i = 0; i < 3; ++i) mbExact = mbExact && ! (std::abs (mb.getLiveCutoff (i) - targetF[i]) > 0.0f);

        // Solo monitor: the glide only runs while the monitor is HOT, so keep
        // a band soloed for the whole drag (the real-world shape: dragging a
        // split while auditioning a band), then release and let the gains
        // settle. The bitwise getLiveCutoff checks below are what guard the
        // 192 kHz stall snap; since Wave 3 the cold gate hinges on the gains
        // only (cutoff-decoupled), so the isSettledCold checks are engagement
        // sanity, no longer a stall symptom.
        anamorph::SoloMonitor mon;
        mon.setBandCount (4);
        mon.setCrossovers (startF[0], startF[1], startF[2]);
        mon.prepare (sr, block);
        auto run = [&] (int blocks, int mask)
        {
            for (int nb = 0; nb < blocks; ++nb)
            {
                std::fill (l.begin(), l.end(), 0.0f);
                std::fill (r.begin(), r.end(), 0.0f);
                mon.process (l.data(), r.data(), mask, block);
            }
        };
        run (settleBlocks, 0x1);                       // solo engaged, monitor hot
        mon.setCrossovers (targetF[0], targetF[1], targetF[2]);
        run (glideBlocks, 0x1);                        // glide converges (or stalls)
        run (settleBlocks, 0);                         // release; gains settle; fast path may engage
        bool monExact = true;
        for (int i = 0; i < 3; ++i) monExact = monExact && ! (std::abs (mon.getLiveCutoff (i) - targetF[i]) > 0.0f);

        std::printf ("  sr %6.0f: mb gaps %+0.4f %+0.4f %+0.4f Hz; solo gaps %+0.4f %+0.4f %+0.4f Hz; cold=%d (pre-fix @192k: 0.47/0.94/1.87 short, never cold)\n",
                     sr,
                     targetF[0] - mb.getLiveCutoff (0),  targetF[1] - mb.getLiveCutoff (1),  targetF[2] - mb.getLiveCutoff (2),
                     targetF[0] - mon.getLiveCutoff (0), targetF[1] - mon.getLiveCutoff (1), targetF[2] - mon.getLiveCutoff (2),
                     (int) mon.isSettledCold());
        check (mbExact,  "multiband cutoffs land bitwise-exactly on their targets");
        check (monExact, "solo-monitor cutoffs land bitwise-exactly on their targets");
        check (mon.isSettledCold(), "solo monitor's settled fast path engages (filters go cold)");
    }

    // The top stall zone [16388,18500] Hz needs Nyquist headroom, so check it
    // at 192 kHz only -- the worst measured case (resting gap 3.75 Hz).
    {
        const double sr = 192000.0;
        std::vector<float> l ((size_t) block, 0.0f), r ((size_t) block, 0.0f);
        const int glideBlocks  = (int) (1.2 * sr) / block;
        const int settleBlocks = (int) (0.1 * sr) / block;

        anamorph::MultibandWidth mb;
        mb.setBandCount (2);
        mb.setWidths (1.0f, 1.0f, 1.0f, 1.0f);
        mb.setCrossovers (16000.0f, 19000.0f, 22000.0f);
        mb.prepare (sr, block);
        mb.setCrossovers (16600.0f, 19000.0f, 22000.0f);
        for (int nb = 0; nb < glideBlocks; ++nb)
        {
            std::fill (l.begin(), l.end(), 0.0f);
            std::fill (r.begin(), r.end(), 0.0f);
            mb.processBlock (l.data(), r.data(), block);
        }

        anamorph::SoloMonitor mon;
        mon.setBandCount (2);
        mon.setCrossovers (16000.0f, 19000.0f, 22000.0f);
        mon.prepare (sr, block);
        auto run = [&] (int blocks, int mask)
        {
            for (int nb = 0; nb < blocks; ++nb)
            {
                std::fill (l.begin(), l.end(), 0.0f);
                std::fill (r.begin(), r.end(), 0.0f);
                mon.process (l.data(), r.data(), mask, block);
            }
        };
        run (settleBlocks, 0x1);
        mon.setCrossovers (16600.0f, 19000.0f, 22000.0f);
        run (glideBlocks, 0x1);
        run (settleBlocks, 0);

        std::printf ("  192k top zone: mb gap %+0.4f Hz; solo gap %+0.4f Hz; cold=%d (pre-fix: 3.75 short, never cold)\n",
                     16600.0f - mb.getLiveCutoff (0), 16600.0f - mon.getLiveCutoff (0),
                     (int) mon.isSettledCold());
        check (! (std::abs (mb.getLiveCutoff (0)  - 16600.0f) > 0.0f), "multiband lands exactly in the worst 192 kHz stall zone (16.6 kHz)");
        check (! (std::abs (mon.getLiveCutoff (0) - 16600.0f) > 0.0f), "solo monitor lands exactly in the worst 192 kHz stall zone (16.6 kHz)");
        check (mon.isSettledCold(), "solo monitor goes cold after the worst-zone drag at 192 kHz");
    }
}

// ---------------------------------------------------------------------------
static void testSoloColdThroughDrag()
{
    std::printf ("Test 33: solo monitor stays cold through a no-solo split drag (Wave 3)\n");

    // The H1 settled fast path is gated on the GAINS only (Wave 3): with
    // nothing soloed the output is provably 1*in + 0*bands whatever the
    // cutoffs do, so a split drag must not wake the bank. Pre-Wave-3 the gate
    // also required every cutoff within 0.05 Hz of its target, so a no-solo
    // drag ran 6 LR4 filters + 5 smoother ticks + up to 3 tan updates per
    // sample just to compute that provable passthrough (the stayedCold check
    // below fails on that behaviour). Cold means the buffer is not even
    // touched; re-engaging must still snap the cutoffs to the FRESHEST
    // targets under the engage crossfade.
    const double sr = 48000.0;
    const int block = 512;
    const int settleBlocks = 20;   // >> the ~12 ms gain crossfade

    anamorph::SoloMonitor mon;
    mon.setBandCount (4);
    mon.setCrossovers (180.0f, 800.0f, 3000.0f);
    mon.prepare (sr, block);

    std::mt19937 rng (24680);
    std::uniform_real_distribution<float> d (-0.7f, 0.7f);
    std::vector<float> l ((size_t) block), r ((size_t) block), lRef ((size_t) block), rRef ((size_t) block);

    auto runBlock = [&] (int mask)
    {
        for (int i = 0; i < block; ++i) { l[(size_t) i] = d (rng); r[(size_t) i] = d (rng); }
        lRef = l; rRef = r;
        mon.process (l.data(), r.data(), mask, block);
    };

    for (int nb = 0; nb < settleBlocks; ++nb) runBlock (0);
    check (mon.isSettledCold(), "monitor is cold once nothing is soloed and the gains settle");

    // Drag the splits at UI cadence while nothing is soloed: the monitor must
    // stay cold and the output must stay the bit-untouched passthrough.
    bool untouched = true, stayedCold = true;
    for (int nb = 1; nb <= 40; ++nb)
    {
        mon.setCrossovers (180.0f  +  4.0f * (float) nb,
                           800.0f  +  8.0f * (float) nb,
                           3000.0f + 20.0f * (float) nb);
        runBlock (0);
        stayedCold = stayedCold && mon.isSettledCold();
        for (int i = 0; i < block && untouched; ++i)
            untouched = ! (std::abs (l[(size_t) i] - lRef[(size_t) i]) > 0.0f)
                     && ! (std::abs (r[(size_t) i] - rRef[(size_t) i]) > 0.0f);
    }
    check (stayedCold, "monitor stays cold through the whole no-solo drag");
    check (untouched,  "cold passthrough leaves the buffer bit-untouched during the drag");

    // Re-engage: cold re-entry snaps the cutoffs to the drag's FINAL targets
    // (not where the glide left off pre-drag) and the band-pass engages.
    runBlock (1);
    const float endF[3] = { 180.0f + 4.0f * 40.0f, 800.0f + 8.0f * 40.0f, 3000.0f + 20.0f * 40.0f };
    bool snapped = true;
    for (int i = 0; i < 3; ++i)
        snapped = snapped && ! (std::abs (mon.getLiveCutoff (i) - endF[i]) > 0.0f);
    check (snapped, "re-engage snaps the cutoffs to the freshest drag targets");

    bool changed = false, allFinite = true;
    for (int nb = 0; nb < settleBlocks; ++nb)
    {
        runBlock (1);
        for (int i = 0; i < block; ++i)
        {
            changed   = changed || (std::abs (l[(size_t) i] - lRef[(size_t) i]) > 0.0f);
            allFinite = allFinite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
        }
    }
    check (changed,   "re-engaged solo audibly band-passes (output differs from the passthrough)");
    check (allFinite, "re-engaged output stays finite");
}

// ---------------------------------------------------------------------------
int main()
{
    std::printf ("=== Anamorph DSP self-tests ===\n");
    testMidSideRoundTrip();
    testNoBadSamples();
    testBypassNullAndLatency();
    testTransparentDefault();
    testMonoMaker();
    testMultibandMonoCompat();
    testMonoMakerPostMix();
    testSoloMonitor();
    testLevelMatchAndSolo();
    testNoClicksAcrossTransitions();
    testSoloNoGhostInSilence();
    testLevelMatchUnity();
    testLevelMatchNoRatchet();
    testLevelMatchMixCouplingNoSlam();
    testLevelMatchSilenceFreeze();
    testCrossoverAutomationSafe();
    testMultibandUnityMatch();
    testMeterRecoversFromNaN();
    testBypassToggleRobust();
    testLevelMatchRunsInBypass();
    testBypassCrossfadeClickFree();
    testMultibandEnableCrossfadeClickFree();
    testSoloMultibandEnableClickFree();
    testDryAlignGateRecomb();
    testForcedSwapNoDropout();
    testRapidForcedSwapDryFill();
    testMultibandFlatRecombination();
    testMultibandSplitDragNoPitchShift();
    testDryFillRespectsOutputGain();
    testForcedSwapDuringOrdinaryFadeOut();
    testHighRateCrossoverSnap();
    testSoloColdThroughDrag();
    testAbActiveClampOnCorruptState(); // state-restoration robustness (not a DSP test)

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    if (failures == 0) { std::printf ("ALL TESTS PASSED\n"); return 0; }
    std::printf ("TESTS FAILED\n");
    return 1;
}
