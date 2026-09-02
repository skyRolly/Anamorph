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
#include "AllocationGuard.h"
#include "dsp/AnamorphEngine.h"
#include "dsp/MidSide.h"
#include "AbSlotIndex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

    // THE DENORMAL HALF OF `isBad` IS A PROPERTY OF THE RUNNER AS WELL AS OF THE
    // ENGINE, and one runner in this pipeline does not have it. The audio path
    // runs under `juce::ScopedNoDenormals`, which sets the CPU's FTZ/DAZ bits so
    // a denormal result is flushed to zero IN HARDWARE. valgrind emulates
    // floating point and does not honour those bits, so under memcheck the flush
    // never happens, denormals survive into the output, and this check fails on a
    // build that is correct on every real CPU. Measured, not assumed: under
    // `valgrind --tool=memcheck` the whole feature matrix reports
    // "engine output free of NaN/Inf/denormals" as a failure while memcheck
    // itself reports ZERO errors -- the tool finds no memory defect and the test
    // fails anyway.
    //
    // ANAMORPH_TESTS_NO_FTZ is how the `sanitizers` job's valgrind step says so,
    // and it relaxes EXACTLY ONE HALF of the check: NaN and Inf remain failures
    // everywhere, because neither depends on FTZ. Only a literal "1" enables it,
    // so an unrelated variable in the environment cannot trip it, and it is read
    // once at start-up rather than per sample.
    //
    // NEVER SET THIS ON A NORMAL RUN. The denormal guard is a DSP_POLICY
    // invariant and these four call sites are the only thing asserting it; the
    // native Linux, Windows and macOS jobs all run without it, so the invariant
    // is still gated on every push on every platform. The alternative considered
    // and rejected was pointing valgrind at the state suite alone -- that suite
    // passes under memcheck untouched, but it would leave the DSP suite with no
    // uninitialised-read detector at all, which is the coverage this job exists
    // to add.
    const bool ftzUnavailable = []
    {
        const char* const v = std::getenv ("ANAMORPH_TESTS_NO_FTZ");
        return v != nullptr && std::strcmp (v, "1") == 0;
    }();

    bool isBad (float x)
    {
        if (std::isnan (x) || std::isinf (x)) return true;
        if (ftzUnavailable) return false;
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
                // Dimension-D: sweep all four voicings -- the only automated
                // execution this voice's ENGAGED synthesis otherwise gets is the
                // assertion-free dsp_dump. Other algorithms ignore dimMode.
                const int dimModes = (a == Algorithm::DimensionD) ? 4 : 1;
                for (int dm = 1; dm <= dimModes; ++dm)
                {
                    EngineParameters p;
                    p.algorithm = a;
                    p.oversample = o;
                    p.driveDb = 8.0f;
                    p.width = 1.6f;
                    p.mix = 0.8f;
                    // ENGAGED wet path. algoAmount defaults to 0 == identity, at
                    // which all three modules take their parked fast paths and the
                    // whole algorithm axis of this matrix asserts nothing over the
                    // wet synthesis code -- the same algoAmount=0 vacuity class
                    // dsp_dump.cpp:12-27 records for the dump. Engaged here, the
                    // invariant finally covers what the axis names.
                    p.algoAmount = 0.7f;
                    p.dimMode = dm;
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
// ---------------------------------------------------------------------------
// 47. reset() must flush the WHOLE duck state group, pendingForced included.
//
//     reset() exists so that a host re-prepare lands in a clean steady state: it
//     adopts pendingP, then clears pendingAlgoReset, switchState, switchPhase,
//     dryDuck and dryDuckLat. `pendingForced` is the one member of that group it
//     used to miss, so a FORCED duck (A/B, preset, undo -- requestDuck) that was
//     still fading when the host changed sample rate or buffer size left the flag
//     latched true underneath a Normal switchState.
//
//     The observable consequence is not the extra reset at the next duck bottom,
//     which is masked by silence -- it is the defensive Level-Match consumer at
//     the end of process(), which runs only `if (! pendingForced)`. With the flag
//     stuck, an injected trim is never adopted: the A/B slot's remembered Level
//     Match is silently dropped for the rest of the session, or until some later
//     duck happens to reach its bottom. That is the #23 behaviour this engine has
//     a whole injection path to guarantee.
//
//     ER-DSP-07, raised by the round-2 investigation sweep.
static void testResetClearsPendingForcedDuck()
{
    std::printf ("Test 47: reset() clears the forced-duck flag (ER-DSP-07)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 64;                     // short blocks: stay INSIDE the ~6 ms fade-out

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;             // transparent defaults
    p.autoGainMatch = true;
    engine.setParameters (p);

    auto runBlock = [&] (const anamorph::EngineParameters& np)
    {
        juce::AudioBuffer<float> buf (2, block);
        for (int i = 0; i < block; ++i) { buf.setSample (0, i, 0.2f); buf.setSample (1, i, 0.2f); }
        engine.setParameters (np);
        engine.process (buf);
    };

    // Start a FORCED duck and leave it mid-fade: one 64-sample block is ~1.3 ms
    // against a ~6 ms fade-out, so the bottom is not reached and pendingForced is
    // still set when the host re-prepares.
    auto swapped = p;
    swapped.algorithm = anamorph::Algorithm::Chorus;   // a discrete change to carry the swap
    engine.requestDuck();
    runBlock (swapped);

    // The host changes sample rate / buffer size mid-duck.
    engine.prepare (sr, block);
    engine.setParameters (swapped);

    // Now the A/B layer restores a remembered Level-Match trim. No duck is in
    // flight any more, so the defensive consumer at the end of process() is the
    // path that must adopt it.
    const float before = engine.getMatchGainDb();
    engine.injectMatchGainDb (-6.0f);
    runBlock (swapped);
    const float after = engine.getMatchGainDb();

    std::printf ("  match gain: %.3f dB before injection, %.3f dB after one block\n",
                 before, after);
    // The injection is a SEED, not a freeze (Test 37's comment, LoudnessMatch.h,
    // feedback #16/#23), so the displayed value keeps moving after it is adopted --
    // the assertion is that it was adopted AT ALL. Dropped reads as exactly `before`;
    // adopted lands near the injected -6 dB and drifts from there.
    check (after < before - 5.0f,
           "an injected Level-Match trim is adopted after a re-prepare mid-forced-duck");
}

// ---------------------------------------------------------------------------
// 48. A duck REQUESTED while the engine is inactive must not fire on activation.
//
//     ER-DSP-06 residual, raised by the round-3 brief: "do not assume the
//     ER-DSP-06 fix covers every duck lifetime transition."
//
//     The lifetime in question: requestDuck() stores into the `duckRequest`
//     atomic, and the ONLY consumer is setParameters' `duckRequest.exchange(0)`.
//     Neither primeParameters (which assigns p/pendingP wholesale) nor prepare()
//     -> reset() touches it. So a request raised while no audio is flowing --
//     the user hits A/B, loads a preset or undoes with the transport stopped --
//     survives the whole activation and is consumed by the POST-prepare
//     setParameters that prepareToPlay ends with.
//
//     And forceDuck SHORT-CIRCUITS the discreteDiffers test (see setParameters'
//     switchState == Normal branch), so the duck fires even though primeParameters
//     has already made np == p. The swap it exists to mask happened silently while
//     nothing was audible; there is nothing left to mask. The result is the
//     ER-DSP-06 shape again in a different transition: the first ~34 ms of audio
//     after activation attenuated for no reason.
//
//     Measured against a control engine driven through the identical sequence
//     WITHOUT the pending request, so the comparison isolates the request and
//     nothing else.
static void testPendingDuckDoesNotSurviveActivation()
{
    std::printf ("Test 48: a duck requested while inactive does not fire on activation (ER-DSP-06 residual)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 64;                     // 1.33 ms: ~26 blocks span the ~34 ms duck
    const int blocks = 32;

    // The widener must be ENGAGED, and this is the whole reason: a FORCED duck is
    // DRY-FILLED, not silenced (beginForcedDuck sets dryDuck when the latency is
    // unchanged, and stage 5 then blends toward the delay-aligned raw input). On a
    // transparent chain the fill IS the output, so a level probe reads 1.0000
    // whether or not the duck fired -- vacuous. Engaged, the discriminator is
    // sharp: the processed output carries SIDE content that the mono dry fill does
    // not, so the image collapses for exactly the duck's lifetime.
    //
    // The stimulus is deterministic NOISE, not a tone, and that is deliberate: a
    // 1 kHz tone through the default 12 ms Haas delay is exactly 12 periods, so the
    // channels re-align and the side energy is identically zero -- a numerology
    // accident that silently makes the measurement vacuous. Noise has no such
    // coincidence at any delay.
    anamorph::EngineParameters p;
    p.algorithm  = anamorph::Algorithm::Haas;
    p.algoAmount = 0.7f;
    p.outputGainDb = 0.0f;

    // The wrapper's activation sequence, verbatim from prepareToPlay:
    //   primeParameters(e); prepare(...); setParameters(e); updateLatency();
    auto activate = [&] (anamorph::AnamorphEngine& e)
    {
        e.primeParameters (p);
        e.prepare (sr, block);
        e.setParameters (p);
    };

    // One deterministic stimulus, replayed identically to both engines.
    // `block` is a constant expression, so reading it is not an odr-use and the
    // capture would be dead (clang's -Wunused-lambda-capture).
    auto fill = [] (juce::AudioBuffer<float>& buf, std::mt19937& rng)
    {
        std::uniform_real_distribution<float> dist (-0.35f, 0.35f);
        for (int i = 0; i < block; ++i)
        {
            const float sv = dist (rng);       // MONO in: all side content is the widener's
            buf.setSample (0, i, sv); buf.setSample (1, i, sv);
        }
    };

    anamorph::AnamorphEngine ducked, control;
    for (auto* e : { &ducked, &control })
    {
        activate (*e);
        std::mt19937 rng (24601);              // same warm-up sequence for both
        juce::AudioBuffer<float> warm (2, block);
        for (int nb = 0; nb < 60; ++nb)        // settle the Haas delay lines and every smoother
        {
            fill (warm, rng);
            e->setParameters (p);
            e->process (warm);
        }
    }

    // THE ONLY DIFFERENCE between the two engines: one carries a duck request
    // raised while no audio was flowing -- the user hitting A/B, loading a preset
    // or undoing with the transport stopped. Then both are re-activated with no
    // blocks processed in between, exactly the sequence the brief specifies.
    ducked.requestDuck();
    activate (ducked);
    activate (control);

    auto sideRms = [] (const juce::AudioBuffer<float>& buf)   // `block` is constexpr; see above
    {
        double sq = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const double sv = 0.5 * (buf.getSample (0, i) - buf.getSample (1, i));
            sq += sv * sv;
        }
        return std::sqrt (sq / block);
    };

    std::mt19937 rngA (98765), rngB (98765);   // identical post-activation stimulus
    double worstRatio = 1.0e9, settledSide = 0.0;
    int worstBlock = -1, blocksOffLevel = 0;

    for (int nb = 0; nb < blocks; ++nb)
    {
        juce::AudioBuffer<float> a (2, block), b (2, block);
        fill (a, rngA); fill (b, rngB);
        ducked.setParameters (p);  ducked.process (a);
        control.setParameters (p); control.process (b);

        const double sa = sideRms (a), sb = sideRms (b);
        if (nb >= blocks - 8) settledSide = juce::jmax (settledSide, sb);
        const double ratio = (sb > 1.0e-9) ? sa / sb : 1.0;
        if (ratio < worstRatio) { worstRatio = ratio; worstBlock = nb; }
        if (std::abs (ratio - 1.0) > 0.02) ++blocksOffLevel;
    }

    // Non-vacuity gate: if the CONTROL carries no side energy the ratio above is a
    // ratio of two nothings and proves nothing either way.
    std::printf ("  control side RMS (settled) = %.6f\n", settledSide);
    check (settledSide > 0.01, "the engaged widener produces side content for the probe to measure");

    std::printf ("  worst block %d: ducked/control SIDE RMS = %.6f; blocks off level by >2%%: %d (%.1f ms)\n",
                 worstBlock, worstRatio, blocksOffLevel,
                 blocksOffLevel * 1000.0 * block / sr);
    check (worstRatio > 0.98,
           "a duck requested while inactive does not collapse the image after activation");
    check (blocksOffLevel == 0,
           "...and no block after activation differs from the un-requested control");
}

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

static void testHaasParkedWarmHistory()
{
    std::printf ("Test 34: parked Haas passes through bit-untouched with WARM history (Wave 4)\n");

    // The Wave-4 parked fast path skips the interpolated read + blend once the
    // wet glide sits at exactly 0, but MUST keep writing the delay lines: a
    // re-engage reads history recorded while parked (the same reasoning that
    // rejected freezing Velvet's envelopes, W3-9). These checks are
    // path-agnostic behaviour invariants: (1) parked blocks leave the buffer
    // bit-untouched; (2) the first engaged blocks reproduce signal recorded
    // DURING the parked stretch -- this fails if a future "optimisation" stops
    // the parked ring writes; (3) a re-parked processor is bit-transparent
    // again after the wet glide drains.
    juce::ScopedNoDenormals noDenormals; // FTZ, exactly like the real audio thread

    const double sr = 48000.0;
    const int block = 512;

    anamorph::HaasProcessor haas;
    haas.prepare (sr, block);
    haas.setDelayMs (20.0f);   // 960 samples: the first engaged block reads parked-era history
    haas.setSide (true);       // the delayed blend lands on the RIGHT channel
    haas.setAmount (0.0f);

    std::mt19937 rng (13579);
    std::uniform_real_distribution<float> d (-0.7f, 0.7f);
    std::vector<float> l ((size_t) block), r ((size_t) block), lRef ((size_t) block), rRef ((size_t) block);

    bool untouched = true;
    for (int nb = 0; nb < 20; ++nb)
    {
        for (int i = 0; i < block; ++i) { l[(size_t) i] = d (rng); r[(size_t) i] = d (rng); }
        lRef = l; rRef = r;
        haas.processBlock (l.data(), r.data(), block);
        for (int i = 0; i < block && untouched; ++i)
            untouched = ! (std::abs (l[(size_t) i] - lRef[(size_t) i]) > 0.0f)
                     && ! (std::abs (r[(size_t) i] - rRef[(size_t) i]) > 0.0f);
    }
    check (untouched, "parked Haas (amount 0) leaves the buffer bit-untouched");

    // Engage on SILENT input: everything non-zero on the right channel must
    // come from the delay line, i.e. from history written while parked.
    haas.setAmount (1.0f);
    float peak = 0.0f;
    bool finite = true;
    std::fill (l.begin(), l.end(), 0.0f);
    std::fill (r.begin(), r.end(), 0.0f);
    haas.processBlock (l.data(), r.data(), block);
    for (int i = 0; i < block; ++i)
    {
        peak   = std::max (peak, std::abs (r[(size_t) i]));
        finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
    }
    check (peak > 0.01f, "re-engage plays back history recorded WHILE parked (rings stayed warm)");
    check (finite, "re-engaged output stays finite");

    // Re-park: after the wet glide drains (and FTZ flushes its tail to exactly
    // 0), the processor is bit-transparent again.
    haas.setAmount (0.0f);
    for (int nb = 0; nb < 250; ++nb) // ~2.7 s >> the ~1.8 s FTZ flush of the glide
    {
        for (int i = 0; i < block; ++i) { l[(size_t) i] = d (rng); r[(size_t) i] = d (rng); }
        haas.processBlock (l.data(), r.data(), block);
    }
    bool reparked = true;
    for (int nb = 0; nb < 5; ++nb)
    {
        for (int i = 0; i < block; ++i) { l[(size_t) i] = d (rng); r[(size_t) i] = d (rng); }
        lRef = l; rRef = r;
        haas.processBlock (l.data(), r.data(), block);
        for (int i = 0; i < block && reparked; ++i)
            reparked = ! (std::abs (l[(size_t) i] - lRef[(size_t) i]) > 0.0f)
                    && ! (std::abs (r[(size_t) i] - rRef[(size_t) i]) > 0.0f);
    }
    check (reparked, "re-parked Haas returns to a bit-untouched passthrough");
}

// ---------------------------------------------------------------------------
static void testMonoSumInputConditioning()
{
    std::printf ("Test 35: Mono sum collapses the input to mono (stage-1 conditioning)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;
    const double freq = 1000.0;

    // Output RMS of channel 0 and of the side signal, after the discrete-switch
    // duck has settled (monoSum is a discrete control, so it arrives ducked).
    auto measure = [&] (bool monoSumOn, bool pureSideInput, double& outCh0, double& outSide)
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;            // transparent defaults
        p.monoSum = monoSumOn;
        engine.setParameters (p);
        engine.reset();

        double phase = 0.0;
        const double inc = 2.0 * 3.14159265358979 * freq / sr;
        double sqCh0 = 0.0, sqSide = 0.0; int counted = 0;
        for (int nb = 0; nb < 60; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = 0.5f * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s);
                buf.setSample (1, i, pureSideInput ? -s : s);
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 40)
                for (int i = 0; i < block; ++i)
                {
                    const float l = buf.getSample (0, i), r = buf.getSample (1, i);
                    sqCh0  += static_cast<double> (l) * static_cast<double> (l);
                    const float side = 0.5f * (l - r);
                    sqSide += static_cast<double> (side) * static_cast<double> (side);
                    ++counted;
                }
        }
        outCh0  = std::sqrt (sqCh0  / juce::jmax (1, counted));
        outSide = std::sqrt (sqSide / juce::jmax (1, counted));
    };

    double ch0 = 0.0, side = 0.0;

    // A pure-side tone sums to nothing: L + R == 0, so mono sum silences it.
    measure (true, true, ch0, side);
    std::printf ("  monoSum ON , side tone : ch0 %.4f side %.4f\n", ch0, side);
    check (ch0 < 0.02, "mono sum silences a pure-side input");

    // A mono tone passes at level, and the output carries no side content.
    measure (true, false, ch0, side);
    std::printf ("  monoSum ON , mono tone : ch0 %.4f side %.4f\n", ch0, side);
    check (ch0 > 0.3,   "mono sum preserves a mono input at level");
    check (side < 0.02, "mono sum output carries no side content");

    // Control: with mono sum OFF the same side tone survives conditioning.
    measure (false, true, ch0, side);
    std::printf ("  monoSum OFF, side tone : ch0 %.4f side %.4f\n", ch0, side);
    check (side > 0.3, "mono sum OFF preserves the side tone");
}

// ---------------------------------------------------------------------------
static void testMsSoloInputIsolation()
{
    std::printf ("Test 36: M/S Solo isolates Mid/Side BEFORE the widening engine (#15)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;
    const double freq = 1000.0;

    // Output RMS (ch 0) for a solo mode + stimulus, with the widening amount as
    // given -- solo is a discrete control, so measurement waits out the duck.
    auto soloRms = [&] (anamorph::SoloMode mode, bool pureSideInput, float amount) -> double
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.solo = mode;
        p.algoAmount = amount;                   // the #15 claim: raised Amount
        if (amount > 0.0f) p.driveDb = 8.0f;     //   must not leak signal back in
        engine.setParameters (p);
        engine.reset();

        double phase = 0.0;
        const double inc = 2.0 * 3.14159265358979 * freq / sr;
        double sq = 0.0; int counted = 0;
        for (int nb = 0; nb < 70; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = 0.5f * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s);
                buf.setSample (1, i, pureSideInput ? -s : s);
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 45)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (0, i);
                    sq += static_cast<double> (v) * static_cast<double> (v); ++counted;
                }
        }
        return std::sqrt (sq / juce::jmax (1, counted));
    };

    using anamorph::SoloMode;
    const double midOnMono   = soloRms (SoloMode::Mid,  false, 0.0f);
    const double midOnSide   = soloRms (SoloMode::Mid,  true,  0.0f);
    const double sideOnSide  = soloRms (SoloMode::Side, true,  0.0f);
    const double sideOnMono  = soloRms (SoloMode::Side, false, 1.0f);
    std::printf ("  Mid solo : mono %.4f side %.4f ; Side solo: side %.4f mono(amount=1) %.4f\n",
                 midOnMono, midOnSide, sideOnSide, sideOnMono);

    check (midOnMono  > 0.3,  "Mid solo passes mono content");
    check (midOnSide  < 0.02, "Mid solo rejects pure-side content");
    check (sideOnSide > 0.3,  "Side solo passes pure-side content");
    // The documented property this stage exists for: solo runs BEFORE the
    // widener, so soloing Side on mono content stays silent even at Amount 1.
    check (sideOnMono < 0.02, "Side solo on mono content stays silent at full Amount");
}

// ---------------------------------------------------------------------------
static void testMatchInjectRestore()
{
    std::printf ("Test 37: injected Level-Match trim is adopted on both consume paths (#23)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);
    anamorph::EngineParameters p;                // transparent defaults
    p.autoGainMatch = true;
    engine.setParameters (p);
    engine.reset();

    double phase = 0.0;
    const double inc = 2.0 * 3.14159265358979 * 1000.0 / sr;
    auto runBlocks = [&] (int count) -> double   // returns RMS over the last 20 blocks
    {
        double sq = 0.0; int counted = 0;
        for (int nb = 0; nb < count; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = 0.25f * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= count - 20)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (0, i);
                    sq += static_cast<double> (v) * static_cast<double> (v); ++counted;
                }
        }
        return std::sqrt (sq / juce::jmax (1, counted));
    };

    // The injection is a SEED, not a freeze (LoudnessMatch.h:63-69, feedback
    // #16/#23): setDisplayedGainDb restores the remembered value so the switch
    // does not lurch, and MEASURE -- "the final authority" while audio plays --
    // then re-converges smoothly FROM it. The assertions below test exactly
    // that contract: the seed lands (both consume paths), and the measurement
    // walks it back to the transparent chain's ~0 dB without a level slam.

    // Settle the transparent chain with Level Match engaged: wet == dry, so the
    // engine's own measured match should sit at ~0 dB.
    const double amp0 = runBlocks (100);
    check (std::abs (engine.getMatchGainDb()) < 1.0, "transparent chain measures ~0 dB match");
    check (amp0 > 0.1, "steady tone present before injection");

    // DEFENSIVE consume path (AnamorphEngine.cpp "arrived WITHOUT a forced
    // duck"): the seed is adopted on the very next block rather than lost.
    engine.injectMatchGainDb (-6.0f);
    runBlocks (1);
    const float seeded = engine.getMatchGainDb();
    std::printf ("  un-ducked seed after 1 block: %.2f dB\n", (double) seeded);
    check (seeded < -4.0f, "un-ducked injection seeds the displayed match trim");

    const double ampBack = runBlocks (200);
    const float back = engine.getMatchGainDb();
    std::printf ("  re-converged after 200 blocks: %.2f dB\n", (double) back);
    check (std::abs (back) < 1.0, "measurement re-converges from the seed (authority kept)");
    check (ampBack / juce::jmax (1.0e-9, amp0) > 0.8, "steady level restored after re-convergence");

    // FORCED path (the A/B slot-switch choreography, feedback #23): request the
    // masking duck, inject the remembered trim, then hand over the (here:
    // identical) parameters; the seed is adopted at the silent bottom.
    engine.requestDuck();
    engine.injectMatchGainDb (-6.0f);
    engine.setParameters (p);
    float lowest = 0.0f;
    for (int nb = 0; nb < 40; ++nb)
    {
        runBlocks (1);
        lowest = juce::jmin (lowest, engine.getMatchGainDb());
    }
    std::printf ("  forced-duck seed: lowest displayed over 40 blocks: %.2f dB\n", (double) lowest);
    check (lowest < -4.0f, "forced-duck injection seeds the trim at the silent bottom");

    const double ampEnd = runBlocks (200);
    check (std::abs (engine.getMatchGainDb()) < 1.0,
           "measurement re-converges after the forced-duck seed");
    check (ampEnd / juce::jmax (1.0e-9, amp0) > 0.8, "steady level restored after the duck");
}

// ---------------------------------------------------------------------------
static void testProcessIsAllocationFree()
{
    std::printf ("Test 38: the audio path allocates nothing (portable guard, ADR-0029)\n");
    juce::ScopedNoDenormals noDenormals;

    // PROVE THE COUNTERS WORK BEFORE TRUSTING A ZERO. Which halves are live is a
    // property of the build, not of the engine (see AllocationGuard.h), so the
    // guard is asked rather than assumed, and a dead half is announced.
    const auto live = anamorph::testing::selfCheck();
    std::printf ("  guard liveness: operator new %s, aligned new %s, malloc family %s\n",
                 live.newLive ? "LIVE" : "not live",
                 live.alignedNewLive ? "LIVE" : "not live",
                 live.mallocLive ? "LIVE" : "not live (expected under ASan)");
    if (! live.newLive && ! live.mallocLive)
    {
        // The whole guard was compiled out. Two builds do that deliberately --
        // valgrind by flag, RealtimeSanitizer by self-detection, both explained
        // in AllocationGuard.h. Say so and assert nothing, rather than reporting
        // a zero nothing was watching for. Under RTSan the stronger detector is
        // running in this same binary and covers the same violation class.
        std::printf ("::warning::the allocation guard is compiled out in this build "
                     "(valgrind's -DANAMORPH_NO_ALLOC_GUARD, or RealtimeSanitizer, which "
                     "the guard would otherwise blind) -- the audio-path allocation "
                     "invariant is NOT asserted by Test 38 in this run.\n");
        return;
    }
    check (live.newLive, "allocation guard: the operator-new counter is live");
    check (live.alignedNewLive, "allocation guard: the over-aligned new counter is live");

    // TWO DIFFERENT THINGS, AND ONLY ONE OF THEM IS ACCEPTABLE. The malloc half
    // is legitimately absent on MSVC and macOS (no glibc to interpose) and under
    // ASan (its own interceptors own malloc); those builds say so and assert
    // less, by design. But a build where the half IS compiled in and still does
    // not observe its own probe is broken -- the interposition stopped working,
    // or the optimizer removed the probe (which is what an unescaped
    // malloc/free pair invites at -O2+, and `linux-lto-tests` compiles this at
    // -O3 -flto). That case used to print the same warning as the legitimate
    // one and skip the assertion, so the run stayed green having checked less
    // than the log implied. It is now a failure.
    if (live.mallocCompiledIn)
        check (live.mallocLive, "allocation guard: the malloc-family counter is live "
                                "(compiled in, so it must observe its own probe)");
    else
        std::printf ("::warning::the malloc half of the allocation guard is not compiled into "
                     "this build (MSVC/macOS have no glibc to interpose; ASan owns malloc) -- "
                     "the raw-malloc allocation route is NOT asserted in this run.\n");

    const double sr = 48000.0;
    const int block = 256;

    anamorph::AnamorphEngine engine;
    engine.prepare (sr, block);

    using namespace anamorph;
    const Algorithm algos[] = { Algorithm::Haas, Algorithm::Velvet, Algorithm::Chorus, Algorithm::DimensionD };
    const OversampleFactor os[] = { OversampleFactor::Off, OversampleFactor::x2, OversampleFactor::x4, OversampleFactor::x8 };

    // The buffer is made ONCE, outside every armed region: constructing an
    // AudioBuffer allocates, and that allocation belongs to the harness rather
    // than to the engine (Test 2 builds one per block, which is fine there and
    // would be counted here).
    juce::AudioBuffer<float> buf (2, block);

    long worstNew = 0, worstMalloc = 0;
    int armedCalls = 0, armedSwitchLandings = 0;

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
                // ENGAGED wet path (same reason as Test 2): at the algoAmount=0
                // default all three modules park and the allocation invariant is
                // never asserted over the engaged wet synthesis code at any OS
                // factor. 0.7 puts the algorithm axis of this matrix under the
                // guard for real.
                p.algoAmount = 0.7f;
                p.msMode = (variant == 0);
                p.mbEnable = true;
                p.monoMakerEnable = true;
                p.autoGainMatch = true;
                // THE SWITCH IS NOT APPLIED HERE, and that is the whole point of
                // this loop rather than an oversight. Applying it outside and
                // then repeating it inside -- which is what this test did until
                // 2026-08-19 -- left every armed block in the steady-state
                // no-change gate: `reset()` flushes an in-flight duck straight
                // to its target (`AnamorphEngine.cpp:138-145`), so the armed
                // region never once executed the block that ADOPTS a discrete
                // change. That block is where the structural work is
                // (`AnamorphEngine.cpp:684-759`: the algorithm tails cleared,
                // the three oversamplers and the chorus reset on an OS path
                // change, the crossover cleared on a topology change) and it
                // runs INSIDE `process()`, at the silent bottom of the duck.
                // Measured: an allocation seeded into it was invisible -- 3,840
                // armed calls, worst new = 0, all checks green.
                //
                // Leaving `p` unapplied makes the first armed block of each
                // configuration perform the real transition from the PREVIOUS
                // one, and the armed blocks that follow carry the duck through
                // its landing. Every configuration differs from its predecessor
                // in at least `msMode`, so all 32 are genuine mid-stream
                // switches; no `reset()` between them is deliberate, because a
                // host does not get one either.

                for (int phase = 0; phase < 2; ++phase)
                    for (int n = 0; n < 60; ++n)
                    {
                        if (phase == 0) fillNoise (buf, (unsigned) (n * 7 + 1));
                        else            buf.clear();

                        // setParameters is on the audio thread every block in the
                        // real wrapper, so it is inside the armed region too.
                        const int latBefore = engine.getLatencySamples();
                        {
                            anamorph::testing::resetCounts();
                            anamorph::testing::Armed arm;
                            engine.setParameters (p);
                            engine.process (buf);
                        }
                        ++armedCalls;
                        // A LANDING THIS RUN ACTUALLY SAW, not one it assumes.
                        // `osEngaged` -- and so the reported latency -- is
                        // re-latched ONLY in that adopt block, so a change here
                        // is proof the block executed while ARMED. Without it a
                        // future edit could quietly restore the pre-loop flush
                        // and leave this test measuring steady state again while
                        // still printing a green zero.
                        //
                        // READ ACROSS THE ARMED SCOPE ONLY -- immediately before
                        // it and immediately after -- never carried across
                        // configurations. Carried, it counts a latency the
                        // pre-loop flush moved OUTSIDE the scope, and the
                        // assertion then passes on the very code it exists to
                        // reject (measured: with the flush restored, a carried
                        // comparison still reported 11).
                        //
                        // A FLOOR, NOT A CENSUS, and the difference is measured
                        // rather than assumed: all 32 configuration changes land,
                        // but the half-band polyphase IIR reports 4 samples at x2
                        // and 6 at both x4 and x8, so the four x4 -> x8 landings
                        // move no latency and are not counted. 11 of the 15
                        // latency-visible transitions are, across all four
                        // algorithms -- which is what this assertion needs.
                        if (engine.getLatencySamples() != latBefore)
                            ++armedSwitchLandings;
                        worstNew    = juce::jmax (worstNew,    anamorph::testing::newCount.load());
                        worstMalloc = juce::jmax (worstMalloc, anamorph::testing::mallocCount.load());
                    }
            }

    std::printf ("  %d armed process() calls, %d of them landing an observable "
                 "structural switch; worst per call: new=%ld malloc=%ld\n",
                 armedCalls, armedSwitchLandings, worstNew, worstMalloc);
    check (armedSwitchLandings > 0,
           "the armed region lands structural switches, not steady state only");
    check (worstNew == 0,    "no operator-new allocation on the audio path");
    if (live.mallocLive)
        check (worstMalloc == 0, "no malloc-family allocation on the audio path");
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
static void testVelvetBlockLengthInvariance()
{
    std::printf ("Test 39: Velvet's output is block-length- and transition-safe "
                 "(A7-1, A7-2B)\n");

    // WHAT THIS PROTECTS, AND WHY IT OUTLIVED THE THING IT WAS WRITTEN FOR.
    // Written for A7-1, when `VelvetNoise` kept a LINEAR image of its Mid
    // history and SLID it forward each block: that made the image cross-block
    // state, correct only while every path that did not maintain it invalidated
    // it. A7-2B has since deleted the image and the offset with it -- the ring
    // is read in place, so there is no carried state left to go stale. The
    // assertion below is unchanged and still earns its place, because it is
    // about the MODULE's contract rather than about that one mechanism: it was
    // green against the pre-A7-1 engine, it caught a wrong slide and a missing
    // invalidation while the slide existed, and it now guards the ring split's
    // block-anchored arithmetic. Test 40 is the complementary one -- it compares
    // the gather against the per-sample loop, which is the axis this test cannot
    // see. (The test is named for block-length invariance rather than for any
    // implementation, precisely so the next mechanism does not orphan the name.)
    //
    // WHY BLOCK-LENGTH INVARIANCE IS THE RIGHT ASSERTION. Every piece of state in
    // this module advances per SAMPLE -- the two glides, the presence env, the
    // gate, the stop machine, the ring write -- and H5's own contract is that the
    // gathered sum equals the per-sample loop's "for any block length". So the
    // module's output is a function of the SAMPLE STREAM alone, and the same
    // audio driven at 32 and at 512 samples must land on identical BITS. Any
    // per-block bookkeeping that is wrong BY THE BLOCK LENGTH -- a stale slide
    // offset while A7-1's image existed, a mis-split ring run now -- perturbs
    // the two runs differently and cannot survive this comparison. It also
    // re-asserts the older H5 and Wave-5 contracts for free: both were written
    // to be block-length agnostic and nothing was checking it.
    juce::ScopedNoDenormals noDenormals; // FTZ, exactly like the real audio thread

    // SWEPT OVER SAMPLE RATE because the image's length is `round(0.045 * sr)`
    // -- 2160 samples at 48 kHz, 8640 at 192 kHz -- so the amount of history the
    // slide carries, and the ring's wrap relative to it, are rate-dependent. A
    // single-rate check would leave the 192 kHz case, where this cost mattered
    // most, unasserted.
    for (const double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
    constexpr int    kBigBlock = 512;
    constexpr int    kSmall    = 32;      // 512 % 32 == 0, so events land on a boundary in both
    constexpr int    kBlocks   = 260;     // ~2.8 s: long enough for the amount glide to flush to 0
    constexpr int    kTotal    = kBigBlock * kBlocks;

    // Deterministic stimulus, generated ONCE and fed to both runs: a noise bed
    // with a silent stretch (so the presence gate closes and re-opens) and a
    // loud stretch (so it saturates). Both runs see the identical sample stream.
    std::vector<float> inL ((size_t) kTotal), inR ((size_t) kTotal);
    {
        std::mt19937 rng (24680);
        std::uniform_real_distribution<float> d (-0.6f, 0.6f);
        for (int i = 0; i < kTotal; ++i)
        {
            const int blk = i / kBigBlock;
            const float g = (blk >= 60 && blk < 90) ? 0.0f          // silent stretch
                          : (blk >= 200 && blk < 220) ? 1.4f        // loud stretch
                          : 1.0f;
            const float tone = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 180.0f * (float) i / (float) sr);
            inL[(size_t) i] = (tone + d (rng)) * g;
            inR[(size_t) i] = (tone - d (rng)) * g;
        }
    }

    // The event schedule, in BIG blocks so every event lands on a block boundary
    // of both runs. Between them it walks every path in the module: the gather
    // fast path, the parked fast path (amount flushed to exactly 0), the general
    // per-sample loop (a moving density re-weights every sample) and the
    // transport-stop fade, which flushes the ring mid-block.
    struct Event { int atBigBlock; int kind; float value; };
    enum { kAmount = 0, kDensity = 1, kTransport = 2 };
    const Event events[] = {
        {   0, kTransport, 1.0f }, {   0, kAmount, 1.0f  }, {   0, kDensity, 0.5f },
        {  20, kAmount,    0.0f },                                   // park
        { 190, kAmount,    0.8f },                                   // re-engage
        { 215, kTransport, 0.0f },                                   // stop: fade + ring flush
        { 225, kTransport, 1.0f },
        { 235, kDensity,   0.9f },                                   // moving density
    };

    // A run is driven by a CYCLE of block sizes, repeated. A single-element cycle
    // is the fixed-size case; a multi-element one puts consecutive gather blocks
    // of DIFFERENT lengths next to each other. That was the case A7-1's slide
    // arithmetic turned on -- the offset carried the just-processed block's
    // length, so a run of equal-sized blocks could be correct with it confused
    // for a constant -- and it is now what varies the ring split's run lengths
    // and the phase of the wrap between neighbouring blocks. Every cycle here sums to
    // `kBigBlock`, so each event still lands on a block boundary in every run.
    auto run = [&] (std::initializer_list<int> cycle)
    {
        anamorph::VelvetNoise v;
        v.prepare (sr, kBigBlock);   // sized for the LARGEST block in any run
        v.reset();

        std::vector<float> outL ((size_t) kTotal), outR ((size_t) kTotal);
        std::vector<float> bufL ((size_t) kBigBlock), bufR ((size_t) kBigBlock);

        const std::vector<int> sizes (cycle);
        std::size_t next = 0;
        for (int start = 0; start < kTotal; )
        {
            for (const auto& e : events)
                if (e.atBigBlock * kBigBlock == start)
                {
                    if      (e.kind == kAmount)    v.setAmount (e.value);
                    else if (e.kind == kDensity)   v.setDensity (e.value);
                    else                           v.setTransportPlaying (e.value > 0.5f);
                }

            const int block = sizes[next++ % sizes.size()];
            std::copy (inL.begin() + start, inL.begin() + start + block, bufL.begin());
            std::copy (inR.begin() + start, inR.begin() + start + block, bufR.begin());
            v.processBlock (bufL.data(), bufR.data(), block);
            std::copy (bufL.begin(), bufL.begin() + block, outL.begin() + start);
            std::copy (bufR.begin(), bufR.begin() + block, outR.begin() + start);
            start += block;
        }
        return std::pair<std::vector<float>, std::vector<float>> { outL, outR };
    };

    const auto big     = run ({ kBigBlock });
    const auto small   = run ({ kSmall });
    // 32 + 128 + 64 + 256 + 32 = 512: four distinct sizes, every neighbouring
    // pair different, and the cycle lands back on the event grid every time.
    const auto varying = run ({ 32, 128, 64, 256, 32 });

    // THE PREMISE, CHECKED FIRST so nothing below is vacuously true of silence:
    // the engaged stretch must actually have decorrelated something. Without
    // this a module that returned its input unchanged would pass the bit
    // comparison perfectly (TESTING_POLICY rule 4).
    float maxSideDelta = 0.0f;
    for (int i = 10 * kBigBlock; i < 19 * kBigBlock; ++i)
    {
        const float inSide  = (inL[(size_t) i]        - inR[(size_t) i]) * 0.5f;
        const float outSide = (big.first[(size_t) i] - big.second[(size_t) i]) * 0.5f;
        maxSideDelta = std::max (maxSideDelta, std::abs (outSide - inSide));
    }
    check (maxSideDelta > 1.0e-3f, "the engaged stretch really decorrelates (premise)");
    std::printf ("  %6.0f Hz: engaged stretch max |side change| = %.4f\n",
                 sr, (double) maxSideDelta);

    // THE INVARIANT, compared on BITS rather than with `==`. Two reasons, and the
    // second is the substantive one: `-Wfloat-equal` is at zero in the Clang
    // baseline and this file is first-party; and a float `==` is the wrong
    // predicate for a bit-identity claim anyway -- it calls +0 and -0 equal
    // (which the S5 signed-zero algebra in this very module cares about) and
    // calls NaN unequal to itself.
    auto sameBits = [] (float a, float b) noexcept
    {
        std::uint32_t ua, ub;
        std::memcpy (&ua, &a, sizeof (ua));
        std::memcpy (&ub, &b, sizeof (ub));
        return ua == ub;
    };

    auto compare = [&] (const char* what,
                        const std::pair<std::vector<float>, std::vector<float>>& other)
    {
        int    firstDiff = -1;
        double worst     = 0.0;
        for (int i = 0; i < kTotal; ++i)
        {
            const bool same = sameBits (big.first[(size_t) i],  other.first[(size_t) i])
                           && sameBits (big.second[(size_t) i], other.second[(size_t) i]);
            if (! same)
            {
                worst = std::max (worst, (double) std::abs (big.first[(size_t) i]  - other.first[(size_t) i]));
                worst = std::max (worst, (double) std::abs (big.second[(size_t) i] - other.second[(size_t) i]));
                if (firstDiff < 0) firstDiff = i;
            }
        }
        check (firstDiff < 0, what);
        if (firstDiff >= 0)
            std::printf ("  [FAIL] %.0f Hz %s: first difference at sample %d (block %d of 512); "
                         "worst |delta| %.3e\n", sr, what, firstDiff, firstDiff / kBigBlock, worst);
    };

    compare ("512-sample and 32-sample runs are bit-identical", small);
    compare ("a run of MIXED block sizes is bit-identical to the 512-sample one", varying);

    // A NON-GATHER PATH REALLY RAN, asserted rather than assumed. A schedule
    // that never left the gather path would compare two runs of the same one
    // path and prove much less than it appears to; the mixed-path crossings are
    // also what made this test able to see A7-2T's seeded delay error at all
    // (Test 40 covers that axis properly). The transport stop is the observable: it is
    // implemented ONLY in the general per-sample loop, where it fades the wet
    // out over ~4 ms and then FLUSHES the history and re-arms the presence gate
    // -- so a window shortly after it must carry far less decorrelation than the
    // engaged window, and the two are compared to each other rather than to a
    // fixed number.
    //
    // THE BOUND IS MEASURED IN BOTH DIRECTIONS, which is what makes it a gate
    // rather than a hopeful inequality. With the stop event: 15.4-25.2 % of the
    // engaged figure across the four rates. With the stop event REMOVED and
    // nothing else changed: 90.6-128.9 %. Half-way between them separates the
    // two by more than 1.8x on either side. Proven live a second way as well:
    // seeding "the general path leaves a stale image" makes the bit comparison
    // above fail at exactly this block.
    //
    // WHAT IS NOT COVERED HERE, said out loud. The Wave-5 PARKED path is not
    // reached by any schedule this test can write, and the reason is a property
    // of the module rather than of the test: with a 0 target the amount one-pole
    // is `a -= 0.0015f * a`, and under FTZ the DECREMENT underflows to zero
    // while `a` is still ~7.8e-36, so the glide stalls just above zero instead of
    // reaching it. `currentAmount > 0.0f` therefore stays true and the gather
    // path keeps its eligibility. (Measured on the shipped code, pre-A7-1 and
    // post- alike; PERF_AUDIT_v0.9.5_IMPLEMENTATION.md §5 carries it. The parked
    // path is still reached from a fresh `prepare()` with Amount at its 0
    // default, which is the state it was written for.) Its invalidation duty is
    // held structurally instead: the offset is cleared on ENTRY to
    // `processBlock`, so no path can arm it by omission.
    const int stopAt   = 215 * kBigBlock;
    const int postFrom = stopAt + (int) (0.005 * sr);   // after the ~4 ms tail fade
    const int postTo   = stopAt + (int) (0.015 * sr);
    float postStop = 0.0f;
    for (int i = postFrom; i < postTo; ++i)
    {
        const float inSide  = (inL[(size_t) i]        - inR[(size_t) i]) * 0.5f;
        const float outSide = (big.first[(size_t) i] - big.second[(size_t) i]) * 0.5f;
        postStop = std::max (postStop, std::abs (outSide - inSide));
    }
    check (postStop < 0.5f * maxSideDelta,
           "the transport stop flushes the wet (a non-gather path ran)");
    std::printf ("  %6.0f Hz: post-stop |side change| = %.4f, %.1f%% of the engaged %.4f\n",
                 sr, (double) postStop, 100.0 * (double) postStop / (double) maxSideDelta,
                 (double) maxSideDelta);
    } // sample-rate sweep
}


static void testVelvetGatherEqualsPerSampleLoop()
{
    std::printf ("Test 40: Velvet's H5 gather is bit-identical to the per-sample loop (A7-2T)\n");

    // WHAT THIS PROTECTS, AND WHAT TEST 39 DOES AND DOES NOT DO. Test 39
    // compares the build under test AGAINST ITSELF at different block lengths.
    // That is the right assertion for the A7-1 slide, whose failure mode is an
    // image stale by the previous block's length -- but there is no reference
    // implementation anywhere in it, so its ORACLE can only see defects whose
    // extent is measured from the BLOCK start. A gather that computes a
    // valid-but-wrong FIR -- every tap reading one sample too deep, say -- is a
    // pure function of the sample stream: the same wrong answer at 32 samples
    // and at 512.
    //
    // MEASURED IN BOTH DIRECTIONS, because the first version of this comment
    // got it wrong and a seeded run corrected it. Test 39 DOES catch that seed
    // as committed -- but through its SCHEDULE rather than its oracle. Its first
    // difference lands at block 215, the transport stop; with the stop removed
    // it lands at block 247, the moving density; with EVERY path crossing
    // removed its two bit-identity comparisons pass at all four sample rates on
    // the seeded build. So its detection of this class is a side effect of the
    // schedule happening to cross from the gather to the per-sample loop, and a
    // schedule that stayed on the gather -- or a defect that only bites where
    // the crossings are not -- would go unseen. On the same seeded build this
    // test fails 20 of its 20 equivalence checks, at sample 3 of block 0,
    // independent of schedule. (PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md §3
    // carries the four runs.)
    //
    // THE ORACLE, AND WHY IT NEEDS NO PRODUCT CHANGE. The module already
    // contains two independent implementations of the same arithmetic: the H5
    // block gather and the general per-sample loop, which H5's own contract says
    // must agree for any block length. The gather's eligibility gate ends with
    // `numSamples <= (int) accum.size()` (VelvetNoise.cpp:155) -- a guard whose
    // stated purpose is direct callers rather than the engine -- and `accum` is
    // sized from `prepare()`'s `maxBlockSize` alone. So an instance prepared for
    // a SMALLER block runs the per-sample loop over the very same audio, and
    // everything else about it is identical: the ring, the tap positions and
    // signs, the weights, the envelope and gate coefficients and the stop step
    // all derive from the sample rate and the seed, never from the block size
    // (VelvetNoise.cpp:14-45). `accum` and `midBlk` are the only block-sized
    // state, and the per-sample loop touches neither.
    //
    // WHY THIS IS THE GATE FOR A7-2, AND IT HAS NOW BEEN SPENT. A7-2B replaced
    // the linear image with a 1-3-run split read straight from the ring. That
    // rewrite is bit-identical when it is right and silently
    // wrong-by-a-constant-delay when it is not, which is the one shape the rest
    // of the suite cannot see. It landed against this test, and this test is
    // what says the two paths still agree.
    //
    // WHAT IS NOT ASSERTED, said out loud. There is no output-observable way to
    // prove from outside which path an instance took, because the two paths are
    // required to produce identical bits -- that is the property under test. The
    // eligibility is established structurally instead: the targets are set
    // BEFORE `prepare()`, which assigns `currentAmount = targetAmount` and
    // `currentDensity = targetDensity` and then calls `updateWeights()` (so
    // `weightsDensity == currentDensity`), leaving the density glide at its
    // fixpoint and the amount engaged from the very first block; the transport
    // is playing and never stops, so `stopping` stays false. Every clause of the
    // gate is therefore satisfied for the gather instance on every block, and
    // its last clause is provably false for the reference instance. The premise
    // check below is what stops the comparison being vacuously true of a module
    // that decorrelated nothing.
    juce::ScopedNoDenormals noDenormals; // FTZ, exactly like the real audio thread

    auto sameBits = [] (float a, float b) noexcept
    {
        std::uint32_t ua, ub;
        std::memcpy (&ua, &a, sizeof (ua));
        std::memcpy (&ub, &b, sizeof (ub));
        return ua == ub;
    };

    // 24576 = 768*32 = 192*128 = 48*512 = 6*4096, so every block size below
    // divides the run exactly and both instances see the same block boundaries.
    constexpr int kTotal = 24576;

    for (const double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        // Deterministic stimulus, generated ONCE per rate and fed to both
        // instances: a noise bed plus a tone, so the Mid history is broadband
        // and a mis-indexed tap cannot land on a value that happens to match.
        std::vector<float> inL ((size_t) kTotal), inR ((size_t) kTotal);
        {
            std::mt19937 rng (13579);
            std::uniform_real_distribution<float> d (-0.5f, 0.5f);
            for (int i = 0; i < kTotal; ++i)
            {
                const float tone = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                    * 220.0f * (float) i / (float) sr);
                inL[(size_t) i] = tone + d (rng);
                inR[(size_t) i] = tone - d (rng);
            }
        }

        // `prepBlock` is the ONLY difference between the two instances, and it
        // is what selects the path: prepBlock >= runBlock gathers, prepBlock <
        // runBlock cannot and falls through to the per-sample loop.
        auto run = [&] (int prepBlock, int runBlock, float density)
        {
            anamorph::VelvetNoise v;
            v.setDensity (density);
            v.setAmount (0.8f);
            v.prepare (sr, prepBlock);   // snaps current := target, builds the weights
            v.setTransportPlaying (true);
            v.reset();

            std::vector<float> outL ((size_t) kTotal), outR ((size_t) kTotal);
            std::vector<float> bufL ((size_t) runBlock), bufR ((size_t) runBlock);
            for (int start = 0; start < kTotal; start += runBlock)
            {
                std::copy (inL.begin() + start, inL.begin() + start + runBlock, bufL.begin());
                std::copy (inR.begin() + start, inR.begin() + start + runBlock, bufR.begin());
                v.processBlock (bufL.data(), bufR.data(), runBlock);
                std::copy (bufL.begin(), bufL.end(), outL.begin() + start);
                std::copy (bufR.begin(), bufR.end(), outR.begin() + start);
            }
            return std::pair<std::vector<float>, std::vector<float>> { outL, outR };
        };

        auto compare = [&] (const char* what, int runBlock, float density)
        {
            const auto gathered  = run (runBlock,     runBlock, density); // gather path
            const auto perSample = run (runBlock - 1, runBlock, density); // per-sample loop

            int    firstDiff = -1;
            double worst     = 0.0;
            for (int i = 0; i < kTotal; ++i)
            {
                const bool same = sameBits (gathered.first [(size_t) i], perSample.first [(size_t) i])
                               && sameBits (gathered.second[(size_t) i], perSample.second[(size_t) i]);
                if (! same)
                {
                    worst = std::max (worst, (double) std::abs (gathered.first [(size_t) i] - perSample.first [(size_t) i]));
                    worst = std::max (worst, (double) std::abs (gathered.second[(size_t) i] - perSample.second[(size_t) i]));
                    if (firstDiff < 0) firstDiff = i;
                }
            }
            check (firstDiff < 0, what);
            if (firstDiff >= 0)
                std::printf ("  [FAIL] %.0f Hz %s: first difference at sample %d "
                             "(block %d of %d); worst |delta| %.3e\n",
                             sr, what, firstDiff, firstDiff / runBlock, runBlock, worst);
        };

        // THE PREMISE, CHECKED FIRST so nothing below is vacuously true: the
        // gather must actually have decorrelated something. A module that
        // returned its input unchanged would satisfy every bit comparison
        // perfectly (TESTING_POLICY rule 4). Measured on the second half of the
        // run, so the ring is long past the zero-fill `reset()` leaves behind --
        // a tap reading a wrong index into an all-zero ring still reads 0.0f,
        // which is why the FIRST blocks after a reset cannot discriminate.
        {
            const auto engaged = run (512, 512, 0.5f);
            float maxSideDelta = 0.0f;
            for (int i = kTotal / 2; i < kTotal; ++i)
            {
                const float inSide  = (inL[(size_t) i]           - inR[(size_t) i])            * 0.5f;
                const float outSide = (engaged.first[(size_t) i] - engaged.second[(size_t) i]) * 0.5f;
                maxSideDelta = std::max (maxSideDelta, std::abs (outSide - inSide));
            }
            check (maxSideDelta > 1.0e-3f, "the gather really decorrelates (premise)");
            std::printf ("  %6.0f Hz: gathered max |side change| = %.4f\n", sr, (double) maxSideDelta);
        }

        // SWEPT OVER BLOCK LENGTH, and the sweep is chosen for what it puts the
        // tap arithmetic through rather than for tidiness. At 32 the ring
        // portion covers the whole block for all but the shallowest taps; at
        // 4096 the block exceeds `decorrSamps` at 44.1 and 48 kHz (1985 and
        // 2160), so EVERY tap splits into a ring run plus a `midBlk` tail --
        // the regime Test 39 cannot reach at all, since its largest block is
        // 512. 512 and 128 sit between the two.
        compare ("gather == per-sample loop at 32 samples",   32,   0.5f);
        compare ("gather == per-sample loop at 128 samples",  128,  0.5f);
        compare ("gather == per-sample loop at 512 samples",  512,  0.5f);
        compare ("gather == per-sample loop at 4096 samples", 4096, 0.5f);

        // AND AT FULL DENSITY, because density decides how many taps are active
        // and WHICH: at the 0.5 default exactly 32 of the 64 taps run, and they
        // are the SHALLOW half (pos spans 3-982 at 44.1 kHz against a 1985-sample
        // window). The deep half -- where a ring read is likeliest to cross the
        // ring origin -- is not exercised by any other configuration in this
        // suite.
        compare ("gather == per-sample loop at 512 samples, density 1.0", 512, 1.0f);
    } // sample-rate sweep
}


// ---------------------------------------------------------------------------
static void testA79ParkedPathsReachableAfterStall()
{
    std::printf ("Test 41: A7-9 -- the parked fast paths are REACHED after an Amount ramp-down\n");

    // WHAT THIS TEST EXISTS TO CATCH, and why the suite could not catch it for
    // two waves. `VelvetNoise`, `HaasProcessor` and `ChorusEngine` each have a
    // parked fast path for "Amount is 0". Every one of them was gated on the
    // wet glide having reached EXACTLY 0 -- and it never does. With a 0 target
    // the update is `a -= k*a`, and under FTZ the DECREMENT underflows before
    // `a` does, so the glide stalls just under FLT_MIN/k and every later
    // decrement is exactly 0. The gates therefore stayed false forever after a
    // user turned Amount down, which is the ONLY route that reaches the state
    // they were written for -- and nothing observed it, because on ordinary
    // real signal `x + 1e-35*(d - x)` is bit-exactly `x` (the absorption needs
    // |x| >= 2^24 * |residual|; Test 42 covers the near-silent class where it
    // fails). A7-9 moves the gates to a FIXPOINT
    // test ("can the glide still move") from a value test ("is it at zero").
    //
    // THE ORACLE IS A SECOND INSTANCE. `S` is driven the way a user drives it:
    // engaged, then turned down and left to stall. `P` sees the identical input
    // with Amount at 0 from `prepare()`, so it is genuinely parked (every module
    // snaps current := target there). Both rings therefore hold the SAME history
    // -- all three modules record the input, not their own output -- so any
    // difference between them is the residual and nothing else.
    //
    // The three checks per module are three different claims:
    //   * real signal -> EXACTLY equal. The "A7-9 changed no audible bit" claim.
    //     It passes before AND after the fix; it is the guard that the change
    //     stayed confined to path selection.
    //   * silence -> within the stall ceiling. The Class-B budget (below).
    //   * silence -> EXACTLY 0. This is the gate: it FAILS before the fix (the
    //     stalled module emits the residual on digital silence, where the dry
    //     term is +0 and cannot absorb it) and passes after. Verified to fire on
    //     all three modules against the pre-A7-9 sources.
    //
    // THE BOUND IS DERIVED HERE, NOT QUOTED. The A7 investigation recorded
    // 4.476e-36 as the worst-case residual; that figure is the maximum its own
    // harness observed, and it is NOT a stimulus-independent bound -- this test,
    // driving +/-0.7 noise, measures 7.145e-36 (Velvet, 48 kHz), 8.043e-36
    // (Haas, 48 kHz) and 1.563e-35 (Chorus, 192 kHz) against the pre-fix
    // sources: 3.5x the recorded figure. The residual is `a_stall * (wet term)`,
    // so what actually bounds it is FLT_MIN/k, computed per module below, times
    // the module's wet gain. The factor of 2 is headroom for that gain (Velvet's
    // normalised tap sum measured 1.30x its input peak) -- this assertion is a
    // regression guard against a residual orders of magnitude larger, and the
    // PROOF that the fix removes it entirely is the exact-zero check, which is
    // stronger than any bound.
    //
    // UNDER ANAMORPH_TESTS_NO_FTZ (valgrind) the stall still happens -- just
    // lower. Without a flush mode the DECREMENT k*a underflows to zero once it
    // drops below half the smallest subnormal, so the glide fixpoints at a
    // ~7e-43 SUBNORMAL rather than at ~FLT_MIN/k (measured: 6.99e-43 for
    // k = 0.001). An earlier version of this comment claimed the glide "walks
    // down through the denormals to a true zero" and that the checks "pass
    // without discriminating"; both halves are false (platform-coverage audit,
    // F-2). The fixpoint gate parks at the subnormal, so post-fix silence is
    // still exactly 0 and all three checks pass -- and against the PRE-fix
    // sources the exact-zero check would still fire, because the old value
    // test stays false at 7e-43. No discrimination is lost without FTZ; only
    // the stall VALUE moves.
    juce::ScopedNoDenormals noDenormals; // FTZ, exactly like the real audio thread

    constexpr int   block         = 512;
    constexpr int   engageBlocks  = 20;  // glide up: the module is genuinely wet
    constexpr int   compareBlocks = 6;
    const     float fltMin        = std::numeric_limits<float>::min();

    std::mt19937 rng (0xA7900001u);
    std::uniform_real_distribution<float> dist (-0.7f, 0.7f);

    // `rampBlocks` must outlast the slowest glide at the rate under test. The
    // stall needs ln(FLT_MIN)/ln(1-k) samples, which is ~155k for ChorusEngine
    // at 192 kHz (k = 1/1920) -- the reason the 192 kHz pass below is longer.
    auto exercise = [&] (const char* name, double sr, int rampBlocks, float stallCeiling,
                         auto& S, auto& P, auto setAmt)
    {
        float maxSigDiff = 0.0f, maxSilDiff = 0.0f, maxSilAbs = 0.0f;
        std::vector<float> sL ((size_t) block), sR ((size_t) block),
                           pL ((size_t) block), pR ((size_t) block);

        auto pump = [&] (int nBlocks, bool silence, bool measure)
        {
            for (int b = 0; b < nBlocks; ++b)
            {
                for (int i = 0; i < block; ++i)
                {
                    const float L = silence ? 0.0f : dist (rng);
                    const float R = silence ? 0.0f : dist (rng);
                    sL[(size_t) i] = pL[(size_t) i] = L;
                    sR[(size_t) i] = pR[(size_t) i] = R;
                }
                S.processBlock (sL.data(), sR.data(), block);
                P.processBlock (pL.data(), pR.data(), block);

                if (! measure) continue;

                for (int i = 0; i < block; ++i)
                {
                    const float dl = std::abs (sL[(size_t) i] - pL[(size_t) i]);
                    const float dr = std::abs (sR[(size_t) i] - pR[(size_t) i]);
                    if (silence)
                    {
                        maxSilDiff = std::max (maxSilDiff, std::max (dl, dr));
                        maxSilAbs  = std::max (maxSilAbs,
                                               std::max (std::abs (sL[(size_t) i]),
                                                         std::abs (sR[(size_t) i])));
                    }
                    else
                    {
                        maxSigDiff = std::max (maxSigDiff, std::max (dl, dr));
                    }
                }
            }
        };

        setAmt (S, 0.8f);                    // P keeps the 0 it was prepared with
        pump (engageBlocks,  false, false);
        setAmt (S, 0.0f);                    // the ramp-down that used to strand the gate
        pump (rampBlocks,    false, false);
        pump (compareBlocks, false, true);   // real signal
        pump (compareBlocks, true,  true);   // digital silence, warm history

        std::printf ("  %-13s %6.0f Hz  real-signal diff = %.6e  silence diff = %.6e  "
                     "silence peak = %.6e  (ceiling %.3e)\n",
                     name, sr, (double) maxSigDiff, (double) maxSilDiff,
                     (double) maxSilAbs, (double) stallCeiling);

        char msg[176];
        std::snprintf (msg, sizeof msg,
                       "%s @ %.0f Hz: stalled matches parked BIT-FOR-BIT on real signal", name, sr);
        check (! (maxSigDiff > 0.0f), msg);
        std::snprintf (msg, sizeof msg,
                       "%s @ %.0f Hz: silence residual within the FLT_MIN/k stall ceiling", name, sr);
        check (! (maxSilDiff > 2.0f * stallCeiling), msg);
        std::snprintf (msg, sizeof msg,
                       "%s @ %.0f Hz: parked path REACHED after the stall (silence is exactly 0)", name, sr);
        check (! (maxSilAbs > 0.0f), msg);
    };

    {
        const double sr = 48000.0;
        anamorph::VelvetNoise S, P;              // targetAmount defaults to 0
        S.prepare (sr, block); P.prepare (sr, block);
        S.setTransportPlaying (true); P.setTransportPlaying (true);
        S.reset(); P.reset();
        exercise ("VelvetNoise", sr, 300, fltMin / 0.0015f, S, P,
                  [] (anamorph::VelvetNoise& v, float a) { v.setAmount (a); });
    }

    {
        const double sr = 48000.0;
        anamorph::HaasProcessor S, P;            // amount defaults to 0
        S.prepare (sr, block); P.prepare (sr, block);
        S.setDelayMs (20.0f);  P.setDelayMs (20.0f);   // 960 samples of warm history
        S.setSide (true);      P.setSide (true);
        exercise ("HaasProcessor", sr, 300, fltMin / 0.001f, S, P,
                  [] (anamorph::HaasProcessor& h, float a) { h.setAmount (a); });
    }

    // ChorusEngine at BOTH ends of the supported range. Its smoothing
    // coefficient is the only rate-dependent one of the three -- wSmooth is
    // 1/(0.01*workingRate) -- so the stall value, and with it the residual the
    // fix removes, scales with the sample rate. 192 kHz is where the programme's
    // worst case lives, which is exactly why it is asserted here and not
    // extrapolated from the 48 kHz pass.
    for (const double sr : { 48000.0, 192000.0 })
    {
        anamorph::ChorusEngine S, P;
        S.setAmount (0.0f); P.setAmount (0.0f);  // amount defaults to 0.5 here
        S.prepare (sr);     P.prepare (sr);
        S.setWorkingRate (sr); P.setWorkingRate (sr);
        exercise ("ChorusEngine", sr, sr > 100000.0 ? 460 : 300,
                  fltMin * (float) (0.01 * sr), S, P,
                  [] (anamorph::ChorusEngine& c, float a) { c.setAmount (a); });
    }
}

// ---------------------------------------------------------------------------
static void testA79ParkedNearSilentIdentity()
{
    std::printf ("Test 42: A7-9 -- parked paths are BIT-EXACT identity on near-silent NONZERO input\n");

    // WHY THIS TEST EXISTS WHEN TEST 41 ALREADY COMPARES STALLED TO PARKED.
    // Test 41's stimulus is +/-0.7 noise and digital silence -- nothing in
    // between -- and the "on real signal `x + 1e-35*(d - x)` is bit-exactly
    // `x`" claim it rests on is amplitude-scoped: the addition absorbs the
    // stalled residual only while |x| >= 2^24 * |residual|. A 2026-08-30
    // review pass asked what happens to near-silent NONZERO input, and the
    // measured answer (worklogs/performance/PERF_AUDIT_A7-9_NEARSILENT_SCOPE.md)
    // is that the pre-A7-9 stalled paths DID move it: driving the pre-fix
    // sources against the current ones, tails at 1e-25..1e-37 amplitude with
    // warm loud history differ by up to 1.204e-35 (Chorus, 192 kHz) inside the
    // delay-history window, while every tail at 1e-20 and above is bit-exact.
    // The residual the fix removes therefore lands not on "digital silence
    // only" but on any sample too small to absorb it -- silence is just the
    // everyday member of that class. This test pins the ACCEPTED side of that
    // scope correction: after A7-9, the parked paths are exact identity on
    // those tails (HaasProcessor and ChorusEngine leave the buffers untouched;
    // VelvetNoise reproduces its usual MS round-trip, which is bit-exact for
    // the mono stimulus used here). Against the pre-A7-9 sources these
    // assertions fail inside the delay-history window -- the residual is
    // exactly what the old path added there.
    //
    // TWO TAIL AMPLITUDES, because the discriminating window is posture-
    // dependent (platform-coverage audit F-1): under FTZ the glide stalls just
    // under FLT_MIN/k, so the window is |x| <~ 2^24 * FLT_MIN/k ~ 1e-28; under
    // ANAMORPH_TESTS_NO_FTZ the stall is a ~7e-43 subnormal and only the
    // 1e-35 tail still discriminates (measured, loud history in both cases:
    // 242..550 samples differ per module against the pre-fix sources at 1e-35
    // without FTZ; none at 1e-30). Each tail therefore starts from a re-warmed
    // loud history. The identity assertion itself is posture-independent --
    // parked is parked.
    //
    // THE TAIL AVOIDS SUBNORMAL INPUT SAMPLES deliberately: under DAZ the
    // VelvetNoise MS round-trip reads a subnormal as zero and reconstructs +0,
    // so subnormal INPUT bits do not survive its parked path (they never did --
    // the engaged loop does the identical arithmetic). That is mix-path
    // behaviour outside this test's claim, so tail samples are snapped away
    // from (0, FLT_MIN) and the identity stays a statement about the parked
    // gates alone.
    juce::ScopedNoDenormals noDenormals; // FTZ, exactly like the real audio thread

    constexpr int   block        = 512;
    constexpr int   engageBlocks = 20;
    constexpr int   tailBlocks   = 8;    // covers every delay-history window (<= 3.2k samples)
    const     float fltMin       = std::numeric_limits<float>::min();

    std::mt19937 rng (0xA7900002u);
    std::uniform_real_distribution<float> dist (-0.7f, 0.7f);

    auto exercise = [&] (const char* name, double sr, int rampBlocks,
                         auto& M, auto setAmt)
    {
        std::vector<float> inL ((size_t) block), inR ((size_t) block),
                           L ((size_t) block), R ((size_t) block);

        // Mono stimulus throughout: VelvetNoise's parked path reconstructs
        // L/R from mid/side, and with side == +0 that round-trip is bit-exact.
        auto pump = [&] (int nBlocks, float amp, bool assertIdentity, int* probes)
        {
            bool identical = true;
            for (int b = 0; b < nBlocks; ++b)
            {
                for (int i = 0; i < block; ++i)
                {
                    // The snap keys off the UNSCALED draw: at the 1e-35 tail the
                    // scaling multiply itself underflows (FTZ flushes it to +/-0
                    // before the magnitude test can see a subnormal), and a -0.0
                    // built that way is the same DAZ-erased class as a subnormal
                    // -- the MS round-trip canonicalizes it to +0 (see above).
                    const float draw = dist (rng);
                    float v = draw * (amp / 0.7f);
                    if (std::abs (draw) > 0.0f && std::abs (v) < fltMin)
                        v = draw > 0.0f ? fltMin : -fltMin; // no subnormal/flushed input
                    inL[(size_t) i] = inR[(size_t) i] = v;
                    if (probes != nullptr && std::abs (v) > 0.0f && std::abs (v) < 1e-30f)
                        ++*probes;
                }
                L = inL; R = inR;
                M.processBlock (L.data(), R.data(), block);
                if (assertIdentity
                    && (std::memcmp (L.data(), inL.data(), sizeof (float) * (size_t) block) != 0
                     || std::memcmp (R.data(), inR.data(), sizeof (float) * (size_t) block) != 0))
                    identical = false;
            }
            return identical;
        };

        setAmt (M, 0.8f);
        pump (engageBlocks, 0.5f, false, nullptr);   // charge the delay histories
        setAmt (M, 0.0f);
        pump (rampBlocks, 0.5f, false, nullptr);     // ramp down; the glide stalls

        int  probes = 0;
        char msg[160];
        const bool id30 = pump (tailBlocks, 1e-30f, true, &probes);
        // Re-warm the delay histories before the second tail: 8 blocks of
        // 1e-30 content have flushed the loud material through every delay
        // line, and a residual scaled by a 1e-30-magnitude delayed sample
        // underflows to nothing -- the 1e-35 tail would discriminate against
        // the pre-fix sources only by luck. The glide stays parked throughout
        // (the target is still 0; recording input is exactly what the parked
        // paths do), so each tail probes a warm-history window.
        pump (engageBlocks, 0.5f, false, nullptr);
        std::snprintf (msg, sizeof msg,
                       "%s @ %.0f Hz: 1e-30 tail with warm history is BIT-EXACT identity", name, sr);
        check (id30, msg);
        const bool id35 = pump (tailBlocks, 1e-35f, true, &probes);
        std::snprintf (msg, sizeof msg,
                       "%s @ %.0f Hz: 1e-35 tail with warm history is BIT-EXACT identity", name, sr);
        check (id35, msg);
        std::snprintf (msg, sizeof msg,
                       "%s @ %.0f Hz: the tails actually probed the residual window (nonzero < 1e-30)",
                       name, sr);
        check (probes > 0, msg);
    };

    {
        const double sr = 48000.0;
        anamorph::VelvetNoise v;
        v.prepare (sr, block); v.setTransportPlaying (true); v.reset();
        exercise ("VelvetNoise", sr, 300, v,
                  [] (anamorph::VelvetNoise& m, float a) { m.setAmount (a); });
    }
    {
        const double sr = 48000.0;
        anamorph::HaasProcessor h;
        h.prepare (sr, block); h.setDelayMs (20.0f); h.setSide (true);
        exercise ("HaasProcessor", sr, 300, h,
                  [] (anamorph::HaasProcessor& m, float a) { m.setAmount (a); });
    }
    for (const double sr : { 48000.0, 192000.0 })
    {
        anamorph::ChorusEngine c;
        c.setAmount (0.0f); c.prepare (sr); c.setWorkingRate (sr);
        exercise ("ChorusEngine", sr, sr > 100000.0 ? 460 : 300, c,
                  [] (anamorph::ChorusEngine& m, float a) { m.setAmount (a); });
    }
}

// ---------------------------------------------------------------------------
// Regression for the engineering-review finding ER-DSP-01: AnamorphEngine::process
// trusted prepareToPlay's maxBlockSize absolutely, so a host block larger than the
// prepared maximum overran every maxBlock-sized scratch buffer (release-build heap
// overflow; JUCE documents this host class as real and says to defend against it).
// The guard chunks such a block into <= maxBlock slices. This test pins BOTH
// halves of the contract: the oversized call must be safe (under ASan the old
// code faults here), and it must be bit-exact against the same audio pushed
// through a twin engine in conforming slices of the same sizes.
static void testOversizedBlockChunked()
{
    std::printf ("Test 43: a block beyond the prepared maximum is chunked, bit-exact vs sliced twin\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int prepared = 512;
    const int big = 4 * prepared + 37; // deliberately not a multiple of the slice

    using namespace anamorph;
    EngineParameters p;
    p.algorithm  = Algorithm::Chorus;
    p.oversample = OversampleFactor::x2; // oversamplers sized for `prepared` too
    p.algoAmount = 0.7f;
    p.driveDb    = 8.0f;
    p.width      = 1.6f;
    p.mix        = 0.8f;
    p.mbEnable   = true;
    p.monoMakerEnable = true;

    // Heap, not stack: sizeof(AnamorphEngine) is 138,576 bytes (measured under
    // the pinned Clang 22), so a pair of them is a ~271 KB frame -- the largest
    // in this suite, and what MSVC's PREfast flags on the Windows analysis job.
    // The engines' own behaviour is identical either way.
    auto wholePtr = std::make_unique<AnamorphEngine>();
    auto slicedPtr = std::make_unique<AnamorphEngine>();
    auto& whole = *wholePtr;
    auto& sliced = *slicedPtr;
    whole.prepare (sr, prepared);  sliced.prepare (sr, prepared);
    whole.setParameters (p);       sliced.setParameters (p);
    whole.reset();                 sliced.reset();

    bool identical = true, allFinite = true;
    for (int nb = 0; nb < 6; ++nb)
    {
        juce::AudioBuffer<float> a (2, big), b (2, big);
        if (nb < 5) { fillNoise (a, (unsigned) (nb * 11 + 3)); }
        else        { a.clear(); }               // one silent oversized block too
        for (int ch = 0; ch < 2; ++ch)
            b.copyFrom (ch, 0, a, ch, 0, big);

        whole.setParameters (p);
        whole.process (a); // guard splits internally: (512, 512, 512, 512, 37)

        // Reference: the identical slice sequence fed as conforming host blocks.
        for (int start = 0; start < big; start += prepared)
        {
            const int len = juce::jmin (prepared, big - start);
            float* ptrs[2] = { b.getWritePointer (0) + start, b.getWritePointer (1) + start };
            juce::AudioBuffer<float> slice (ptrs, 2, len);
            sliced.setParameters (p);
            sliced.process (slice);
        }

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < big; ++i)
            {
                const float va = a.getSample (ch, i);
                if (! std::isfinite (va)) allFinite = false;
                // Exact equality is the assertion here (bit-identity against the
                // sliced twin), so it goes through JUCE's designated helper
                // rather than a bare `!=` the -Wfloat-equal gate would flag.
                if (! juce::exactlyEqual (va, b.getSample (ch, i))) identical = false;
            }
    }
    check (allFinite, "oversized-block output is finite (no scratch overrun)");
    check (identical, "oversized-block output bit-matches the conforming-slice twin");
}

// ---------------------------------------------------------------------------
// Regression for ER-DSP-02: prepare() stomped the continuous smoothers to
// neutral constants and re-armed their targets from the live snapshot WITHOUT
// settling them, so the first ~5-20 ms after every prepareToPlay of a
// non-default session GLIDED from neutral (a Mix=0 session opened wet, Output
// Gain -24 dB opened hot, inverted polarity ramped through +1) -- violating the
// DSP_POLICY invariant-7 bit-exact null in the first blocks. prepare() now
// snaps the smoothers to their just-armed targets (inaudible: all delay/filter
// state was just cleared).
static void testPrepareSettlesSmoothers()
{
    std::printf ("Test 44: re-prepare keeps a Mix=0 session bit-null from sample 0\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    using namespace anamorph;
    EngineParameters p;              // OS off -> zero latency, dry == input
    p.algorithm  = Algorithm::Velvet;
    p.algoAmount = 0.7f;             // wet path engaged and audibly different...
    p.driveDb    = 8.0f;
    p.mix        = 0.0f;             // ...but Mix=0 must be a bit-exact null (ADR-0005)

    AnamorphEngine engine;
    engine.prepare (sr, block);
    engine.setParameters (p);
    engine.reset();
    for (int nb = 0; nb < 40; ++nb)  // settle the initial discrete adoption
    {
        juce::AudioBuffer<float> buf (2, block);
        fillNoise (buf, (unsigned) (nb + 1));
        engine.setParameters (p);
        engine.process (buf);
    }

    // The host re-activates: prepareToPlay again, same spec. Before the fix,
    // mixSmooth's CURRENT value was the stomped neutral 1.0 gliding to 0 --
    // the first ~20 ms played WET.
    engine.prepare (sr, block);

    juce::AudioBuffer<float> buf (2, block), ref (2, block);
    fillNoise (buf, 77u);
    for (int ch = 0; ch < 2; ++ch)
        ref.copyFrom (ch, 0, buf, ch, 0, block);
    engine.setParameters (p);
    engine.process (buf);

    bool nullFromSampleZero = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < block; ++i)
            if (! juce::exactlyEqual (buf.getSample (ch, i), ref.getSample (ch, i)))
                nullFromSampleZero = false;
    check (nullFromSampleZero, "first block after re-prepare is a bit-exact Mix=0 null");
}

// ---------------------------------------------------------------------------
// Regression for ER-DSP-04: CorrelationMeter had no NaN/Inf guard (ADR-0009
// decision bullet 3 was implemented only in LevelMeters). One non-finite sample
// reaching the tap drove an accumulator to Inf; the next finite sample turned it
// NaN (Inf - Inf), and the unguarded one-poles latched NaN until re-prepare --
// the INC-004 defect class, left open in the sibling meter. publish() now
// flushes non-finite accumulators back to 0 (the documented idle value).
static void testCorrelationMeterRecoversFromNaN()
{
    std::printf ("Test 45: correlation meter self-heals after a non-finite sample\n");
    anamorph::CorrelationMeter m;
    m.prepare (48000.0);

    for (int i = 0; i < 4800; ++i) m.process (0.5f, 0.45f);
    m.publish();
    check (m.getFast() > 0.9f, "correlated input reads near +1 before the poisoning");

    m.process (std::numeric_limits<float>::infinity(), 0.5f); // one bad sample
    m.publish();
    // Recovery restarts the flushed accumulators from 0 while the unpoisoned one
    // keeps its history, so give the 120 ms fast one-poles ~4 time constants to
    // re-converge before asserting the tracked value.
    for (int i = 0; i < 24000; ++i) m.process (0.5f, -0.5f);  // finite, anti-phase
    m.publish();

    const bool finite = std::isfinite (m.getFast()) && std::isfinite (m.getSlow())
                     && std::isfinite (m.getBalance()) && std::isfinite (m.getEnergy());
    check (finite, "all published values finite after a non-finite sample");
    check (m.getFast() < -0.9f, "meter recovered and tracks the anti-phase input");
}

// ---------------------------------------------------------------------------
// Regression for ER-DSP-10: EXTREME BUT FINITE audio broke the phase meter.
//
// This is NOT the Test 45 class and the two must not be confused. There, a
// NON-FINITE sample poisoned an accumulator and `publish()`'s sanitize is the
// cure. Here every value the guard can see is FINITE -- the samples, the three
// per-sample products, and all six accumulators -- so sanitize accepts the whole
// state and never fires; the overflow happens AFTER it, inside `correlation()`,
// where `ll * rr` is a float multiply of two mean-square values. Above
// sqrt(FLT_MAX) ~ 1.844e19 that product is +Inf, sqrt(+Inf) is +Inf, +Inf is not
// below the 1e-12 small-signal floor, and `lr / +Inf` is 0 -- so a PERFECTLY
// CORRELATED mono signal published 0.0, "fully decorrelated", and its anti-phase
// twin published -0.0 instead of -1.
//
// THE TEST IS BUILT AROUND THE THRESHOLD, not around "big numbers", because that
// is what makes it a proof of the mechanism: 4.0e9 and 5.0e9 are one binade
// apart and differ in exactly one respect -- whether ll * rr overflows -- and
// the pre-fix build reads +1.000 at the first and 0.000 at the second.
static void testCorrelationMeterExtremeFiniteInput()
{
    std::printf ("Test 50: extreme finite input does not break the phase meter (ER-DSP-10)\n");

    // Long enough for the 600 ms slow one-pole to converge at 48 kHz.
    constexpr int kSettle = 200000;

    struct Read { float fast, slow, energy, balance; };
    auto steady = [] (float l, float r) -> Read
    {
        anamorph::CorrelationMeter m;
        m.prepare (48000.0);
        for (int i = 0; i < kSettle; ++i) m.process (l, r);
        m.publish();
        return { m.getFast(), m.getSlow(), m.getEnergy(), m.getBalance() };
    };

    // ---- the mechanism: one binade either side of the overflow threshold ----
    // sqrt(FLT_MAX) = 1.84467435e19, so ll = rr = l^2 overflows their product
    // once |l| > 4.29496723e9. Both amplitudes below are finite, both produce
    // finite per-sample products, and both leave every accumulator finite.
    const Read below = steady (4.0e9f, 4.0e9f);   // ll*rr = 2.559e38, finite
    const Read above = steady (5.0e9f, 5.0e9f);   // ll*rr = +Inf
    std::printf ("  correlated mono: 4.0e9 -> fast %.4f | 5.0e9 -> fast %.4f (threshold |l| = 4.295e9)\n",
                 (double) below.fast, (double) above.fast);

    // The accumulators are finite on BOTH sides -- this is what separates this
    // defect from the Test 45 poison class, and it is asserted rather than
    // assumed. `energy` is llFast + rrFast, i.e. the accumulator state itself:
    // if sanitize had fired it would read 0, and it reads ~5e19 / ~7.8e19.
    check (std::isfinite (above.energy) && above.energy > 1.0e19f,
           "extreme finite input leaves the accumulators FINITE and non-zero (sanitize never fires)");

    // Below the threshold the meter was always right -- so amplitude alone is
    // not the complaint, and a fix that merely rejected loud audio fails here.
    check (below.fast > 0.99f && below.slow > 0.99f,
           "just BELOW the overflow threshold, correlated mono already read +1");

    // Above it, the pre-fix build reads 0.0 for a perfectly correlated signal.
    check (above.fast > 0.99f, "just ABOVE it, correlated mono still reads +1 (fast)");
    check (above.slow > 0.99f, "just ABOVE it, correlated mono still reads +1 (slow)");

    // Not merely finite: 0.0 IS finite, and 0.0 is the exact wrong answer.
    check (! (std::abs (above.fast) < 0.5f),
           "the extreme reading is not the decorrelated 0.0 the overflow produced");

    // ---- the sign survives too: -0.0 is finite, and is not -1 ----
    const Read anti = steady (1.0e10f, -1.0e10f);
    std::printf ("  anti-phase 1.0e10 -> fast %.4f slow %.4f\n", (double) anti.fast, (double) anti.slow);
    check (anti.fast < -0.99f && anti.slow < -0.99f,
           "extreme finite anti-phase reads -1, not the -0.0 the overflow produced");

    // ---- the contract the overflow violated, stated directly ----
    // Correlation is SCALE-INVARIANT: the same waveform at any amplitude is the
    // same correlation. Two amplitudes eleven orders apart must agree.
    const Read quiet = steady (0.5f, 0.5f);
    const Read loud  = steady (1.0e10f, 1.0e10f);
    check (std::abs (quiet.fast - loud.fast) < 1.0e-6f,
           "correlation is scale-invariant: 0.5 and 1.0e10 agree to 1e-6");

    // A correlated pair at DIFFERENT extreme amplitudes is still correlated
    // (r = 0.3 l), and its balance must still describe the real imbalance.
    const Read uneven = steady (1.0e10f, 3.0e9f);
    check (uneven.fast > 0.99f, "extreme finite, unequal but correlated, still reads +1");
    check (uneven.balance < -0.5f, "and the L/R balance still reports the real imbalance");

    // ---- normal-range control: ordinary behaviour is unchanged ----
    // Not a formality. Every assertion above is satisfied by "always return +1",
    // and these three are what refuse it.
    const Read ctlCorr = steady (0.5f, 0.45f);
    const Read ctlAnti = steady (0.5f, -0.5f);
    check (ctlCorr.fast > 0.99f,  "control: ordinary correlated input still reads +1");
    check (ctlAnti.fast < -0.99f, "control: ordinary anti-phase input still reads -1");

    // Decorrelated control -- alternating L-only / R-only frames have zero
    // cross-product and non-zero energy in both channels, so the meter must sit
    // near 0. This is the assertion "always +1" cannot pass.
    {
        anamorph::CorrelationMeter m;
        m.prepare (48000.0);
        for (int i = 0; i < kSettle; ++i)
            m.process ((i & 1) ? 0.5f : 0.0f, (i & 1) ? 0.0f : 0.5f);
        m.publish();
        std::printf ("  decorrelated control -> fast %.4f\n", (double) m.getFast());
        check (std::abs (m.getFast()) < 0.1f, "control: decorrelated input still reads ~0");
    }

    // ---- the poison contract (Test 45's) is untouched by this fix ----
    // A genuinely NON-finite sample must still flush the accumulator to the
    // documented idle value rather than be rescued by the new branch.
    {
        anamorph::CorrelationMeter m;
        m.prepare (48000.0);
        for (int i = 0; i < 4800; ++i) m.process (0.5f, 0.5f);
        m.process (std::numeric_limits<float>::infinity(), 0.5f);
        m.publish();
        const bool allFinite = std::isfinite (m.getFast()) && std::isfinite (m.getSlow())
                            && std::isfinite (m.getBalance()) && std::isfinite (m.getEnergy());
        check (allFinite, "a genuinely non-finite sample still self-heals (poison contract preserved)");
    }
}

// ---------------------------------------------------------------------------
// Regression for ER-TST-04: the stage-1 input-conditioning block (channelMode,
// swapLR, inputBalance, polarity) and the character parameters (chorusRate,
// chorusDepth, dimMode) had ZERO behavioural coverage -- a swapped-channel,
// inverted-polarity or reversed-balance regression passed every gate, and the
// character params were exercised by no assertion-bearing test even at module
// level. Part A pins conditioning semantics on the transparent-default chain
// (invariant 8: output == conditioned input). Part B pins that each character
// parameter actually changes the engaged output (the dsp_dump self-check
// pattern, brought into the assertion-bearing suite).
static void testInputConditioningAndCharacterParams()
{
    std::printf ("Test 46: input-conditioning semantics + character-parameter discrimination\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    using namespace anamorph;

    // ---- Part A: conditioning on the transparent-default chain ----
    // Feed L = 1 kHz tone, R = 2.7 kHz tone (distinct content per channel),
    // settle 40 blocks (discrete conditioning arrives under the duck; the
    // continuous smoothers finish well inside that), measure RMS over 20 more.
    auto measure = [&] (auto configure, double& rms0, double& rms1)
    {
        AnamorphEngine engine;
        engine.prepare (sr, block);
        EngineParameters p; // transparent defaults
        configure (p);
        engine.setParameters (p);
        engine.reset();

        double ph0 = 0.0, ph1 = 0.0;
        const double inc0 = 2.0 * 3.14159265358979 * 1000.0 / sr;
        const double inc1 = 2.0 * 3.14159265358979 * 2700.0 / sr;
        double sq0 = 0.0, sq1 = 0.0; int counted = 0;
        for (int nb = 0; nb < 60; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                buf.setSample (0, i, 0.5f * (float) std::sin (ph0)); ph0 += inc0;
                buf.setSample (1, i, 0.5f * (float) std::sin (ph1)); ph1 += inc1;
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 40)
                for (int i = 0; i < block; ++i)
                {
                    const double l = buf.getSample (0, i), r = buf.getSample (1, i);
                    sq0 += l * l; sq1 += r * r; ++counted;
                }
        }
        rms0 = std::sqrt (sq0 / juce::jmax (1, counted));
        rms1 = std::sqrt (sq1 / juce::jmax (1, counted));
    };

    double rms0 = 0.0, rms1 = 0.0;

    measure ([] (EngineParameters& p) { p.channelMode = ChannelMode::LeftOnly; }, rms0, rms1);
    check (rms0 > 0.3 && rms1 < 0.02, "LeftOnly keeps L and kills R");

    measure ([] (EngineParameters& p) { p.channelMode = ChannelMode::RightOnly; }, rms0, rms1);
    check (rms1 > 0.3 && rms0 < 0.02, "RightOnly keeps R and kills L");

    measure ([] (EngineParameters& p) { p.swapLR = true; p.channelMode = ChannelMode::LeftOnly; }, rms0, rms1);
    // conditioning order: the kill happens BEFORE the swap, so L's tone lands on R
    check (rms0 < 0.02 && rms1 > 0.3, "swapLR routes the kept channel to the other side");

    measure ([] (EngineParameters& p) { p.inputBalance = 1.0f; }, rms0, rms1);
    check (rms0 < 0.02 && rms1 > 0.3, "inputBalance +1 fully attenuates the far (left) side");

    measure ([] (EngineParameters& p) { p.inputBalance = -1.0f; }, rms0, rms1);
    check (rms1 < 0.02 && rms0 > 0.3, "inputBalance -1 fully attenuates the far (right) side");

    // Polarity: identical mono tone on both channels; after the 5 ms ramp the
    // smoothed sign is EXACTLY -1 / +1, and the default chain is bit-transparent,
    // so the settled output must be the exact negation on the flipped channel.
    {
        AnamorphEngine engine;
        engine.prepare (sr, block);
        EngineParameters p;
        p.polarityL = true;
        engine.setParameters (p);
        engine.reset();
        double ph = 0.0; const double inc = 2.0 * 3.14159265358979 * 1000.0 / sr;
        bool exactFlip = true;
        for (int nb = 0; nb < 60; ++nb)
        {
            juce::AudioBuffer<float> buf (2, block), in (2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = 0.5f * (float) std::sin (ph); ph += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
                in.setSample (0, i, s);  in.setSample (1, i, s);
            }
            engine.setParameters (p);
            engine.process (buf);
            if (nb >= 40)
                for (int i = 0; i < block; ++i)
                {
                    if (! juce::exactlyEqual (buf.getSample (0, i), -in.getSample (0, i))) exactFlip = false;
                    if (! juce::exactlyEqual (buf.getSample (1, i),  in.getSample (1, i))) exactFlip = false;
                }
        }
        check (exactFlip, "polarityL settles to an exact per-channel sign flip");
    }

    // ---- Part B: character parameters must change the engaged output ----
    // Two engines in lockstep on identical noise; sum |a-b| over the settled
    // tail. A dead character parameter reads ~0 here and fails.
    auto engagedDiff = [&] (auto configA, auto configB)
    {
        auto eaPtr = std::make_unique<AnamorphEngine>(); // 135 KB each -- see Test 43
        auto ebPtr = std::make_unique<AnamorphEngine>();
        auto& ea = *eaPtr;
        auto& eb = *ebPtr;
        ea.prepare (sr, block); eb.prepare (sr, block);
        EngineParameters pa, pb;
        pa.algoAmount = pb.algoAmount = 0.7f;
        pa.mix = pb.mix = 0.8f;
        configA (pa); configB (pb);
        ea.setParameters (pa); eb.setParameters (pb);
        ea.reset(); eb.reset();
        double diff = 0.0;
        for (int nb = 0; nb < 40; ++nb)
        {
            juce::AudioBuffer<float> a (2, block), b (2, block);
            fillNoise (a, (unsigned) (nb * 13 + 5));
            for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, a, ch, 0, block);
            ea.setParameters (pa); ea.process (a);
            eb.setParameters (pb); eb.process (b);
            if (nb >= 20)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < block; ++i)
                        diff += std::abs ((double) a.getSample (ch, i) - (double) b.getSample (ch, i));
        }
        return diff;
    };

    const double dRate = engagedDiff (
        [] (EngineParameters& p) { p.algorithm = Algorithm::Chorus; p.chorusRate = 0.2f; },
        [] (EngineParameters& p) { p.algorithm = Algorithm::Chorus; p.chorusRate = 2.0f; });
    check (dRate > 1.0e-3, "chorusRate audibly changes the engaged Chorus output");

    const double dDepth = engagedDiff (
        [] (EngineParameters& p) { p.algorithm = Algorithm::Chorus; p.chorusDepth = 0.15f; },
        [] (EngineParameters& p) { p.algorithm = Algorithm::Chorus; p.chorusDepth = 0.9f; });
    check (dDepth > 1.0e-3, "chorusDepth audibly changes the engaged Chorus output");

    bool dimAllDistinct = true;
    for (int m1 = 1; m1 <= 4 && dimAllDistinct; ++m1)
        for (int m2 = m1 + 1; m2 <= 4 && dimAllDistinct; ++m2)
        {
            const double d = engagedDiff (
                [m1] (EngineParameters& p) { p.algorithm = Algorithm::DimensionD; p.dimMode = m1; },
                [m2] (EngineParameters& p) { p.algorithm = Algorithm::DimensionD; p.dimMode = m2; });
            if (! (d > 1.0e-3)) dimAllDistinct = false;
        }
    check (dimAllDistinct, "all four Dimension-D voicings are pairwise distinct engaged");
}


// ---------------------------------------------------------------------------
//  Opt-in probe (round 12, ER-STATE-13 on AArch64): the ENGINE half of the
//  per-slot Level-Match question, so it can be cross-built with nothing but
//  AnamorphDSP and run under qemu. Matched counterfactual, same design as round
//  9's --legacy-match-probe: two engines with identical parameters and identical
//  noise streams; one receives a stale injected match at its switch, the other
//  does not. Everything ISA-dependent in the mechanism is exercised here -- the
//  relaxed atomic hand-off of the injected value, its consumption at the silent
//  bottom of the duck, the smoother, and the loudness re-measure that supersedes
//  it. Prints; asserts nothing.
// ---------------------------------------------------------------------------
static int runMatchInjectProbe()
{
#if defined(__aarch64__)
    const char* isa = "AArch64";
#elif defined(__x86_64__)
    const char* isa = "x86-64";
#else
    const char* isa = "other";
#endif
    std::printf ("Level-Match injection probe -- ISA %s\n", isa);
    std::printf ("  std::atomic<float>::is_always_lock_free = %d, sizeof(float[2]) = %d, alignof = %d\n",
                 (int) std::atomic<float>::is_always_lock_free, (int) sizeof (float[2]), (int) alignof (float[2]));

    constexpr double sr = 48000.0; constexpr int bs = 512;
    auto fill = [] (juce::AudioBuffer<float>& b, juce::Random& r)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, (r.nextFloat() * 2.0f - 1.0f) * 0.25f);
    };
    auto rms = [] (const juce::AudioBuffer<float>& b)
    {
        double acc = 0.0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i) { const double v = b.getSample (ch, i); acc += v * v; }
        return std::sqrt (acc / (double) (b.getNumChannels() * b.getNumSamples()));
    };

    auto scenario = [&] (int settleBlocks, int chain, float stale, const char* label)
    {
        anamorph::AnamorphEngine c, k;                 // c = contaminated, k = control
        anamorph::EngineParameters p;
        p.autoGainMatch = true;
        p.width = chain == 0 ? 1.9f : 1.5f;
        if (chain == 1) { p.driveDb = 6.0f; p.algoAmount = 0.5f; }
        if (chain == 2)                                // the v0.2 FIXTURE chain round 9 restored, field for field
        {
            p.driveDb = 6.0f; p.algorithm = static_cast<anamorph::Algorithm> (2); p.width = 1.5f;
            p.mix = 0.8f; p.haasDelayMs = 20.0f; p.outputGainDb = -3.0f;
        }
        if (chain == 3)                                // a chain that SETTLES far from the stale value:
        {                                              // reproduces the processor probe's ~6 dB delta
            p.driveDb = 18.0f; p.algorithm = static_cast<anamorph::Algorithm> (2); p.width = 2.0f;
            p.mix = 1.0f; p.algoAmount = 1.0f;
        }
        c.prepare (sr, bs); k.prepare (sr, bs);
        c.primeParameters (p); k.primeParameters (p);
        juce::Random rc (20260901), rk (20260901);     // identical streams
        juce::AudioBuffer<float> bc (2, bs), bk (2, bs);
        for (int i = 0; i < settleBlocks; ++i)
        {
            fill (bc, rc); c.setParameters (p); c.process (bc);
            fill (bk, rk); k.setParameters (p); k.process (bk);
        }
        const float settledC = c.getMatchGainDb(), settledK = k.getMatchGainDb();
        c.requestDuck(); c.injectMatchGainDb (stale);  // exactly what abSwitchTo does with abMatchGain[slot]
        k.requestDuck();
        std::printf ("\n  --- %s: settled match c %+.3f / k %+.3f dB; inject %+.1f dB into c at the switch ---\n",
                     label, settledC, settledK, stale);
        std::printf ("  blk |  match c   match k  |  rms c     rms k    | ratio dB\n");
        double worstAfterDuck = 0.0; int injectSeenAt = -1;
        for (int i = 1; i <= 12; ++i)
        {
            fill (bc, rc); c.setParameters (p); c.process (bc);
            fill (bk, rk); k.setParameters (p); k.process (bk);
            const double rc_ = rms (bc), rk_ = rms (bk);
            const double ratioDb = 20.0 * std::log10 (juce::jmax (1.0e-12, rc_) / juce::jmax (1.0e-12, rk_));
            if (injectSeenAt < 0 && std::abs (c.getMatchGainDb() - stale) < 2.0f) injectSeenAt = i;
            if (i >= 5) worstAfterDuck = juce::jmax (worstAfterDuck, std::abs (ratioDb));
            std::printf ("  %3d | %+8.3f  %+8.3f  | %.5f  %.5f  | %+7.3f\n",
                         i, c.getMatchGainDb(), k.getMatchGainDb(), rc_, rk_, ratioDb);
        }
        std::printf ("  injected value visible in c's match at block %d (%s); worst |ratio| after the duck (blocks 5-12): %.3f dB\n",
                     injectSeenAt, injectSeenAt > 0 ? "mechanism REAL" : "never surfaced", worstAfterDuck);
    };
    scenario (80, 0, +9.0f, "TRANSPARENT chain, settled; inject +9");
    scenario (80, 1, +9.0f, "ENGAGED chain (drive 6, amount 0.5, width 1.5), settled; inject +9");
    scenario (1,  1, +9.0f, "ENGAGED chain, WORST CASE (no settle); inject +9");
    scenario (80, 2, -1.0f, "FIXTURE chain (drive 6, algo 2, width 1.5, mix 0.8, out -3), settled; inject the REAL stale value -1.0");
    scenario (80, 2, +9.0f, "FIXTURE chain, settled; inject +9 (is it the chain or the magnitude?)");
    scenario (80, 3, -1.0f, "LARGE-DELTA chain (settles far from the stale value); inject the REAL stale -1.0");
    return 0;
}

// ---------------------------------------------------------------------------
//  Test 49 -- a restored non-default session must not GLIDE into its own sound.
//
//  ER-DSP-09 (round 20). Every one of these modules already snaps its internal
//  smoothed values to their targets inside its OWN prepare(). The defect was
//  ordering: `AnamorphEngine::prepare()` prepares the modules FIRST and only then
//  runs `updateDerived()`, which is what installs the restored snapshot's targets
//  -- so each module snapped to whatever targets existed beforehand (a fresh
//  instance's defaults, or the previous session's on a reused one), and the
//  engine's own `reset()` then re-zeroed the chorus blend outright. A restored
//  session therefore opened at its DEFAULTS and glided into its stored sound.
//
//  HOW THIS ISOLATES THE GLIDE FROM THINGS THAT ARE NOT BUGS. A delay-based
//  module starting from cleared state necessarily produces less wet signal in its
//  first milliseconds -- the delay line holds silence and no fix can invent past
//  audio -- so an end-to-end "first block vs settled" ratio conflates that with
//  the parameter glide. Each leg here instead compares the subject against a
//  REFERENCE whose delay/filter state is identically cleared and differs ONLY in
//  having its smoothed values already at target, so the comparison sees the glide
//  and nothing else. For three of the four the reference is the module's own
//  correct path (targets set BEFORE prepare); the chorus zeroes its blend in
//  reset() regardless of order, so its reference is settled by running silence
//  with the LFO rate at zero -- which leaves phase at 0 and the delay line full of
//  the same silence, keeping the two bit-comparable.
// ---------------------------------------------------------------------------
static void testRestoredModulesDoNotGlideIn()
{
    std::printf ("Test 49: a restored non-default session opens at its own sound (ER-DSP-09)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    constexpr int n = 512;   // constexpr, so the lambdas below need no capture

    auto burst = [] (juce::AudioBuffer<float>& b, unsigned seed)
    {
        b.setSize (2, n, false, false, true);
        juce::Random rng ((int) seed);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                b.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.25f);
    };
    auto maxDiff = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        float d = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                d = juce::jmax (d, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
        return d;
    };
    // Non-vacuity: the module must actually DO something to this input, or an
    // "identical" verdict would be identical silence.
    auto isLive = [] (const juce::AudioBuffer<float>& out, const juce::AudioBuffer<float>& in)
    {
        float d = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                d = juce::jmax (d, std::abs (out.getSample (ch, i) - in.getSample (ch, i)));
        return d > 1.0e-4f;
    };

    // TOLERANCES. The subject is EXACT -- snapToTargets assigns the target -- so any
    // residual belongs to the REFERENCE, whose settled-by-silence legs approach their
    // targets asymptotically (a one-pole never quite arrives). Measured at 200 blocks
    // the residual is ~1e-8; the 1e-5 bound leaves four orders of headroom over it and
    // still sits four orders BELOW the defect it must catch, which moves the first
    // block by ~0.2 of full scale.
    juce::AudioBuffer<float> dry; burst (dry, 4242u);
    auto copyOf = [&dry] { juce::AudioBuffer<float> c; c.makeCopyOf (dry); return c; };

    // --- Haas -------------------------------------------------------------
    {
        // The reference cannot simply set the targets before prepare here:
        // setDelayMs scales by the module's OWN sample rate, which is still the
        // 44.1 kHz default until prepare runs, so a pre-prepare delay would be
        // 28 ms at the wrong rate. Settle it with silence instead -- Haas has no
        // LFO, so the delay line ends up holding the same zeros the subject's
        // cleared one does and the two stay bit-comparable.
        anamorph::HaasProcessor ref, sub;
        ref.prepare (sr, n);
        ref.setDelayMs (28.0f); ref.setSide (true); ref.setAmount (1.0f);
        ref.reset();   // snaps currentSamples exactly, so only the amount is left to settle
        {
            juce::AudioBuffer<float> quiet (2, n);
            for (int k = 0; k < 200; ++k)   // see the tolerance note below
            {
                quiet.clear();
                ref.processBlock (quiet.getWritePointer (0), quiet.getWritePointer (1), n);
            }
        }
        sub.prepare (sr, n);                                              // the engine's order
        sub.setDelayMs (28.0f); sub.setSide (true); sub.setAmount (1.0f);
        sub.reset();
        sub.snapToTargets();

        auto a = copyOf(), b = copyOf();
        ref.processBlock (a.getWritePointer (0), a.getWritePointer (1), n);
        sub.processBlock (b.getWritePointer (0), b.getWritePointer (1), n);
        check (isLive (a, dry), "Haas: the reference really processes this input (non-vacuity)");
        check (maxDiff (a, b) < 1.0e-5f, "Haas: a restored amount opens settled, not gliding");
    }

    // --- Velvet -----------------------------------------------------------
    {
        anamorph::VelvetNoise ref, sub;
        ref.setDensity (0.9f); ref.setAmount (1.0f);
        ref.prepare (sr, n);
        sub.prepare (sr, n);
        sub.setDensity (0.9f); sub.setAmount (1.0f);
        sub.reset();
        sub.snapToTargets();

        auto a = copyOf(), b = copyOf();
        ref.processBlock (a.getWritePointer (0), a.getWritePointer (1), n);
        sub.processBlock (b.getWritePointer (0), b.getWritePointer (1), n);
        check (isLive (a, dry), "Velvet: the reference really processes this input (non-vacuity)");
        check (maxDiff (a, b) < 1.0e-6f, "Velvet: a restored density/amount opens settled");
    }

    // --- Mono Maker -------------------------------------------------------
    {
        anamorph::MonoMaker ref, sub;
        ref.setFrequency (60.0f);
        ref.prepare (sr, n);
        sub.prepare (sr, n);
        sub.setFrequency (60.0f);
        sub.reset();
        sub.snapToTargets();

        auto a = copyOf(), b = copyOf();
        ref.process (a.getWritePointer (0), a.getWritePointer (1), n);
        sub.process (b.getWritePointer (0), b.getWritePointer (1), n);
        check (isLive (a, dry), "Mono Maker: the reference really processes this input (non-vacuity)");
        check (maxDiff (a, b) < 1.0e-6f, "Mono Maker: a restored crossover opens at its frequency");
    }

    // --- Chorus -----------------------------------------------------------
    {
        anamorph::ChorusEngine ref, sub;
        auto arm = [sr] (anamorph::ChorusEngine& c)
        {
            c.setWorkingRate (sr);
            c.setVoice (anamorph::ChorusEngine::Voice::Chorus);
            c.setRate (0.0f);          // LFO parked: phase stays 0 in both instances
            c.setDepth (0.9f);
            c.setAmount (1.0f);
        };
        ref.prepare (sr); arm (ref);
        // Settle the reference's wet blend by running silence. The delay line ends
        // up holding the same zeros the subject's cleared one does, and with the
        // rate parked the phase is 0 in both -- so the only surviving difference
        // is the blend this test is about.
        {
            juce::AudioBuffer<float> quiet (2, n);
            for (int k = 0; k < 200; ++k)   // see the tolerance note below
            {
                quiet.clear();
                ref.processBlock (quiet.getWritePointer (0), quiet.getWritePointer (1), n);
            }
        }
        sub.prepare (sr); arm (sub);
        sub.reset();
        sub.snapToTargets();

        auto a = copyOf(), b = copyOf();
        ref.processBlock (a.getWritePointer (0), a.getWritePointer (1), n);
        sub.processBlock (b.getWritePointer (0), b.getWritePointer (1), n);
        check (isLive (a, dry), "Chorus: the reference really processes this input (non-vacuity)");
        check (maxDiff (a, b) < 1.0e-5f, "Chorus: a restored wet blend opens settled, not fading in");
    }

    // --- The ENGINE actually performs the snap -----------------------------
    // The four legs above pin each module's own contract; on their own they would
    // still pass if AnamorphEngine::prepare() never called snapToTargets at all
    // (verified: it did). This leg closes that gap, and it uses the Mono Maker
    // because it is the one affected module with NO delay line -- a crossover is
    // pure filter state, zero in both instances -- so an engine-level comparison
    // is free of the delay-fill difference that makes the other three unusable
    // here (a cleared delay line holds silence, and no fix can invent past audio).
    {
        anamorph::EngineParameters e;      // a "restored" non-default snapshot
        e.monoMakerEnable = true;
        e.monoMakerFreq   = 60.0f;         // the default is 120: a full octave away
        e.algoAmount      = 0.0f;          // widener identity, so only the crossover acts
        e.mix             = 1.0f;

        anamorph::AnamorphEngine ref, sub;
        ref.primeParameters (e);
        ref.prepare (sr, n);
        ref.setParameters (e);
        {   // settle the crossover glide on silence: filter state stays at zero,
            // so the reference ends up in exactly the state the subject starts in
            // apart from the frequency this leg is about.
            juce::AudioBuffer<float> quiet (2, n);
            for (int k = 0; k < 200; ++k) { quiet.clear(); ref.process (quiet); }
        }
        sub.primeParameters (e);
        sub.prepare (sr, n);
        sub.setParameters (e);             // the real activation path, nothing else

        juce::AudioBuffer<float> a, b;
        a.makeCopyOf (dry); b.makeCopyOf (dry);
        ref.process (a);
        sub.process (b);
        check (isLive (a, dry), "engine: the Mono Maker really acts on this input (non-vacuity)");
        check (maxDiff (a, b) < 1.0e-5f,
               "engine: prepare() leaves the restored crossover settled, not gliding");
    }

    // --- CONTROL: ordinary live edits after prepare STILL smooth -----------
    // The fix must not have turned the smoothers off. A parameter moved AFTER
    // preparation has to ramp, which is what keeps edits click-free.
    {
        anamorph::HaasProcessor h;
        h.setDelayMs (28.0f); h.setSide (true); h.setAmount (0.0f);
        h.prepare (sr, n);                       // settled at amount 0 == identity
        h.setAmount (1.0f);                      // a LIVE edit, no prepare, no snap

        auto first = copyOf();
        h.processBlock (first.getWritePointer (0), first.getWritePointer (1), n);
        auto settled = copyOf();
        for (int k = 0; k < 20; ++k)             // let the live edit finish ramping
        {
            juce::AudioBuffer<float> quiet (2, n); quiet.clear();
            h.processBlock (quiet.getWritePointer (0), quiet.getWritePointer (1), n);
        }
        h.processBlock (settled.getWritePointer (0), settled.getWritePointer (1), n);
        check (maxDiff (first, settled) > 1.0e-4f,
               "CONTROL: a live amount edit after prepare still RAMPS (smoothing intact)");
    }
}

int main (int argc, char* argv[])
{
    if (argc > 1 && std::strcmp (argv[1], "--match-inject-probe") == 0)
        return runMatchInjectProbe();

    std::printf ("=== Anamorph DSP self-tests ===\n");

    // A RELAXED RUN MUST SAY SO, in the same run whose result it changes. The
    // escape hatch is legitimate (see `isBad`), but read once from the
    // environment it left no trace: a stale export or an inherited CI variable
    // produced the same "ALL TESTS PASSED" line as a full run, with the
    // denormal invariant not asserted. Announced twice on purpose -- at the top
    // where the reader starts and beside the verdict where they stop -- and as
    // a `::warning::` so a CI run surfaces it the way the Rosetta step surfaces
    // its own lost coverage.
    if (ftzUnavailable)
        std::printf ("::warning::ANAMORPH_TESTS_NO_FTZ=1 -- the DENORMAL half of the "
                     "NaN/Inf/denormal invariant is NOT asserted in this run (NaN and Inf "
                     "still are). Set only by the valgrind step; unset it for a full gate.\n");

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
    testHaasParkedWarmHistory();
    testMonoSumInputConditioning();
    testMsSoloInputIsolation();
    testMatchInjectRestore();
    testProcessIsAllocationFree();
    testVelvetBlockLengthInvariance();
    testVelvetGatherEqualsPerSampleLoop();
    testA79ParkedPathsReachableAfterStall();
    testA79ParkedNearSilentIdentity();
    testOversizedBlockChunked();
    testPrepareSettlesSmoothers();
    testCorrelationMeterRecoversFromNaN();
    testCorrelationMeterExtremeFiniteInput();
    testInputConditioningAndCharacterParams();
    testRestoredModulesDoNotGlideIn();
    testResetClearsPendingForcedDuck();
    testPendingDuckDoesNotSurviveActivation();
    testAbActiveClampOnCorruptState(); // state-restoration robustness (not a DSP test)

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    if (ftzUnavailable)
        std::printf ("(ANAMORPH_TESTS_NO_FTZ=1 was set: the denormal invariant was NOT asserted)\n");
    if (failures == 0) { std::printf ("ALL TESTS PASSED\n"); return 0; }
    std::printf ("TESTS FAILED\n");
    return 1;
}
