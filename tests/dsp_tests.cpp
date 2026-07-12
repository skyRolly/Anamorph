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
                    sideSq += side * side; ++counted;
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
                    midSq += mid * mid; ++counted;
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
                blkIn += mono * mono;
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 40) // let the crossovers settle before measuring
            {
                for (int i = 0; i < block; ++i)
                {
                    const float mono = buf.getSample (0, i) + buf.getSample (1, i);
                    outSq += mono * mono;
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
                    outSq += side * side; ++cnt;
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
                for (int i = 0; i < block; ++i) { const float v = buf.getSample (0, i); sq += v * v; ++cnt; }
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
                    for (int i = 0; i < block; ++i) { const float v = buf.getSample (ch, i); sq += v * v; }
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
    check (maxAbs < 2.0, "bypass toggling during playback never bursts");

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
            blkIn += mono * mono;
        }
        engine.setParameters (p);
        engine.process (buf);
        double blkOut = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const float mono = buf.getSample (0, i) + buf.getSample (1, i);
            blkOut += mono * mono;
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
    testAbActiveClampOnCorruptState(); // state-restoration robustness (not a DSP test)

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    if (failures == 0) { std::printf ("ALL TESTS PASSED\n"); return 0; }
    std::printf ("TESTS FAILED\n");
    return 1;
}
