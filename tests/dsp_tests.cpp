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
#include "dsp/AnamorphEngine.h"
#include "dsp/MidSide.h"

#include <cmath>
#include <cstdio>
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

        // Feed an impulse; the bypassed output must be the input delayed by lat.
        const int N = 4096;
        juce::AudioBuffer<float> buf (2, N);
        buf.clear();
        buf.setSample (0, 0, 1.0f);
        buf.setSample (1, 0, 1.0f);
        engine.setParameters (p);
        engine.process (buf);

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
                inSq += s * s; // input side magnitude == |s|
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

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    if (failures == 0) { std::printf ("ALL TESTS PASSED\n"); return 0; }
    std::printf ("TESTS FAILED\n");
    return 1;
}
