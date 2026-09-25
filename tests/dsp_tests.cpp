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
#include <vector>
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
    // Drive is kept at 6 dB so this leg measures the ENGAGED wrap. Since ADR-0034
    // it is no longer what makes the latency non-zero -- the selected factor is --
    // and the skipped-wrap half of the matrix is Test 52's leg B.
    for (auto factor : { anamorph::OversampleFactor::x2, anamorph::OversampleFactor::x4, anamorph::OversampleFactor::x8 })
    {
        anamorph::EngineParameters p;
        p.bypass = true;
        p.oversample = factor;
        p.driveDb = 6.0f; // the wrap actually RUNS in this leg
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
// Regression for ER-DSP-11: extreme but FINITE levels erased the channel
// imbalance. A DIFFERENT operation from Test 50's, and the two contracts are
// kept independent on purpose. Test 50 is the phase meter's `ll * rr` PRODUCT
// inside correlation(); this is the balance's `ll + rr` SUM in publish(), and
// fixing the product did nothing for the sum.
//
// The mechanism, all of it downstream of a guard that never fires: both
// accumulators are finite (so `sanitize` accepts them), the NUMERATOR `rr - ll`
// cannot overflow either -- it lies in [-ll, rr] for non-negative operands, so
// it stays finite and carries the whole imbalance -- but the float SUM leaves
// float once it passes FLT_MAX, `+Inf` sails past the 1e-12 small-signal guard,
// and finite/+Inf is a perfectly well-formed 0. The meter reported PERFECTLY
// CENTRED for a badly lopsided pair.
//
// THE TEST IS BUILT AROUND THE OVERFLOW EDGE, not around "big numbers", because
// only that makes it a proof of the mechanism: 1.8e19/0.2e19 is MORE lopsided
// than 1.8e19/1.0e19 and reads correctly in BOTH builds, because its energies
// sum to 3.277e38 and stay under FLT_MAX. Level is not the variable; the
// overflow is.
static void testCorrelationBalanceExtremeFiniteInput()
{
    std::printf ("Test 51: extreme finite levels do not erase the channel balance (ER-DSP-11)\n");

    constexpr int kSettle = 400000;          // ~50 time constants of the 600 ms slow pole

    struct Read { float balance, fast, energy; };
    auto steady = [] (float l, float r) -> Read
    {
        anamorph::CorrelationMeter m;
        m.prepare (48000.0);
        for (int i = 0; i < kSettle; ++i) m.process (l, r);
        m.publish();
        return { m.getBalance(), m.getFast(), m.getEnergy() };
    };
    // The mathematical expectation, from the same inputs, in double.
    auto expected = [] (float l, float r)
    {
        const double ll = (double) l * (double) l, rr = (double) r * (double) r;
        return (rr - ll) / (rr + ll);
    };

    // ---- normal-range controls: three distinct values, so a degenerate fix dies ----
    // "always 0" fails the unequal legs, "always non-zero" fails the balanced leg,
    // "always one-sided" fails whichever direction it is not.
    const Read nBal  = steady (0.5f,  0.5f);
    const Read nLeft = steady (0.5f,  0.25f);   // L louder -> negative
    const Read nRight= steady (0.25f, 0.5f);    // R louder -> positive
    std::printf ("  normal: balanced %.4f | L louder %.4f | R louder %.4f\n",
                 (double) nBal.balance, (double) nLeft.balance, (double) nRight.balance);
    check (std::abs (nBal.balance) < 1.0e-4f, "control: balanced normal-range input reads centred");
    check (std::abs (nLeft.balance  - (float) expected (0.5f, 0.25f)) < 1.0e-3f,
           "control: unequal normal-range input reads its true balance (L louder)");
    check (std::abs (nRight.balance - (float) expected (0.25f, 0.5f)) < 1.0e-3f,
           "control: unequal normal-range input reads its true balance (R louder)");

    // ---- the premise: at extreme levels the accumulators are still HEALTHY ----
    // `sanitize` flushes a poisoned accumulator to 0, and correlation() of zeros
    // is 0. These inputs are perfectly correlated (r = k*l), so a +1 reading is
    // proof that nothing was flushed -- and it is also Test 50's contract still
    // holding at this level, which this test must not disturb.
    const Read xUneq = steady (1.8e19f, 1.0e19f);       // ll+rr = 4.236e38 -> OVERFLOWS
    check (xUneq.fast > 0.99f,
           "premise: the accumulators are healthy at extreme level (correlated input still reads +1)");
    check (xUneq.energy > 0.0f, "premise: the meter is not reporting silence (sanitize never fired)");

    // ---- the defect: an overflowing SUM erased the imbalance ----
    std::printf ("  extreme unequal 1.8e19/1.0e19 -> balance %.6f (expected %.6f)\n",
                 (double) xUneq.balance, expected (1.8e19f, 1.0e19f));
    check (std::abs (xUneq.balance - (float) expected (1.8e19f, 1.0e19f)) < 2.0e-3f,
           "extreme unequal channels report their TRUE balance, not centred");
    // Not merely non-zero, and not merely finite: -0.0 is both finite and the
    // exact wrong answer, so the value itself is the assertion above. This one
    // refuses the degenerate "push extreme readings to an endpoint" fix.
    check (std::abs (xUneq.balance) < 0.9f,
           "...and is the real figure, not a clamped endpoint");

    // Mirrored: the other direction is asserted on its own VALUE as well as on
    // the symmetry, because the symmetry alone is satisfied by the defect (-0.0
    // and +0.0 mirror each other perfectly).
    const Read xMirror = steady (1.0e19f, 1.8e19f);
    std::printf ("  extreme unequal 1.0e19/1.8e19 -> balance %.6f (expected %.6f)\n",
                 (double) xMirror.balance, expected (1.0e19f, 1.8e19f));
    check (std::abs (xMirror.balance - (float) expected (1.0e19f, 1.8e19f)) < 2.0e-3f,
           "the R-louder extreme pair reports its TRUE balance too");
    check (std::abs (xMirror.balance + xUneq.balance) < 1.0e-6f,
           "swapping L and R flips the balance sign exactly");

    // ---- balanced extreme input must STILL read centred ----
    const Read xBal = steady (1.5e19f, 1.5e19f);        // ll+rr = 4.5e38 -> also OVERFLOWS
    std::printf ("  extreme balanced 1.5e19/1.5e19 -> balance %.6f\n", (double) xBal.balance);
    check (std::abs (xBal.balance) < 1.0e-4f, "extreme BALANCED input still reads centred");

    // ---- the discriminator: MORE lopsided, but the sum does not overflow ----
    // 1.8e19/0.2e19 sums to 3.277e38, under FLT_MAX, and read correctly even
    // before the fix. If this leg ever changes, the fix has reached beyond the
    // overflow it was written for.
    const Read xSafe = steady (1.8e19f, 0.2e19f);
    std::printf ("  extreme unequal 1.8e19/0.2e19 (sum does NOT overflow) -> balance %.6f (expected %.6f)\n",
                 (double) xSafe.balance, expected (1.8e19f, 0.2e19f));
    check (std::abs (xSafe.balance - (float) expected (1.8e19f, 0.2e19f)) < 2.0e-3f,
           "a MORE lopsided pair whose sum stays finite is unchanged (level is not the variable)");

    // ---- ER-DSP-10 remains intact: its own contract, asserted here too ----
    {
        anamorph::CorrelationMeter m;
        m.prepare (48000.0);
        for (int i = 0; i < 200000; ++i) m.process (1.0e10f, 1.0e10f);   // Test 50's case
        m.publish();
        check (m.getFast() > 0.99f && m.getSlow() > 0.99f,
               "ER-DSP-10 intact: the phase meter still reads +1 where ll*rr overflows");
    }

    // ---- the poison contract (Test 45's) is untouched by this fix ----
    {
        anamorph::CorrelationMeter m;
        m.prepare (48000.0);
        for (int i = 0; i < 4800; ++i) m.process (0.5f, 0.25f);
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

// ---------------------------------------------------------------------------
//  Opt-in probe (0.9.7, ADR-0034): the reported and the ACTUAL latency of the
//  chain across the whole {factor} x {algorithm} x {drive} grid. Prints the
//  matrix Test 52 asserts, and is what measured the defect: before ADR-0034 the
//  `predict` column read 0 at drive 0 / 0.005 with a linear algorithm and 4 (2x)
//  or 6 (4x, 8x) at drive 6 -- a reported-PDC step on an ordinary knob move.
//  The `actual` column is an impulse peak through the full chain, so in the
//  ENGAGED rows it reads lat or lat+1: the half-band IIR is minimum-phase and
//  smears the peak by a sample at 2x and 8x. That is not a misalignment and it
//  is unchanged by ADR-0034 -- Test 52's exact leg measures the bypass path,
//  where the delay is a pure integer shift. Prints; asserts nothing.
// ---------------------------------------------------------------------------
static int runOsLatencyProbe()
{
    std::printf ("Oversampling latency-stability probe\n");
    constexpr double sr = 48000.0; constexpr int bs = 512;

    // The processor's own order -- prime, prepare, set -- so the OS wrap is
    // latched for the snapshot under test rather than for the defaults.
    auto measureDelay = [] (anamorph::EngineParameters p) -> int
    {
        anamorph::AnamorphEngine e;
        e.primeParameters (p);
        e.prepare (sr, bs);
        e.setParameters (p);
        constexpr int N = 8192;
        juce::AudioBuffer<float> buf (2, N);
        buf.clear();
        buf.setSample (0, 64, 1.0f);
        buf.setSample (1, 64, 1.0f);
        for (int off = 0; off < N; off += bs)
        {
            float* ch[2] = { buf.getWritePointer (0) + off, buf.getWritePointer (1) + off };
            juce::AudioBuffer<float> sub (ch, 2, juce::jmin (bs, N - off));
            e.setParameters (p);
            e.process (sub);
        }
        int peak = -1; float best = 0.0f;
        for (int i = 0; i < N; ++i)
            if (std::abs (buf.getSample (0, i)) > best) { best = std::abs (buf.getSample (0, i)); peak = i; }
        return peak - 64;
    };

    auto reported = [] (anamorph::EngineParameters p) -> int
    {
        anamorph::AnamorphEngine e;
        e.primeParameters (p);
        e.prepare (sr, bs);
        e.setParameters (p);
        return e.getLatencySamples();
    };

    auto predicted = [] (anamorph::EngineParameters p) -> int
    {
        anamorph::AnamorphEngine e;
        e.prepare (sr, bs);
        return e.predictLatency (p);
    };

    const char* osName[4] = { "Off", "2x", "4x", "8x" };
    const anamorph::OversampleFactor osv[4] = { anamorph::OversampleFactor::Off,
                                                anamorph::OversampleFactor::x2,
                                                anamorph::OversampleFactor::x4,
                                                anamorph::OversampleFactor::x8 };
    const char* algoName[4] = { "Haas", "Velvet", "Chorus", "DimensionD" };

    std::printf ("  OS   algorithm   drive |  predict  report  actual\n");
    for (int o = 0; o < 4; ++o)
        for (int a = 0; a < 4; ++a)
            for (float drive : { 0.0f, 0.005f, 6.0f })
            {
                anamorph::EngineParameters p;
                p.oversample = osv[o];
                p.algorithm  = static_cast<anamorph::Algorithm> (a);
                p.driveDb    = drive;
                std::printf ("  %-4s %-11s %5.3f | %7d %7d %7d\n",
                             osName[o], algoName[a], drive,
                             predicted (p), reported (p), measureDelay (p));
            }
    return 0;
}

// ---------------------------------------------------------------------------
//  Test 52 -- the reported latency is a function of the OVERSAMPLING SELECTION
//  alone, and the chain really carries that number (ADR-0034).
//
//  THE DEFECT. The oversampling wrap is skipped when it has no nonlinear or
//  modulation work to do -- Drive parked at 0 with a linear algorithm -- and that
//  CPU saving is deliberate: the resampling round trip is the single largest cost
//  in the engine. What used to travel with it was the wrap's LATENCY. With a
//  factor selected, Drive crossing 0.01 dB (or Algorithm crossing into or out of
//  Chorus / Dimension-D) moved the reported PDC between 0 and the factor's
//  latency, and a host answers a latency change by restarting its graph. The
//  user hears an ordinary knob move as a dropout. Measured with
//  `--os-latency-probe` on the pre-fix build: predict 0 -> 4 at 2x, 0 -> 6 at 4x
//  and 8x, for a Drive move of 0.005 dB -> 6 dB.
//
//  WHAT THE FIX MUST NOT DO, and what each leg is here to catch:
//    * report a constant by making the oversampler ALWAYS run. That would delete
//      the CPU saving. Leg C fails such a build: it requires the drive-0 output to
//      be the OS-off output BIT-FOR-BIT (delayed), which a half-band IIR round
//      trip cannot be.
//    * report a latency the chain does not have. Leg B fails such a build: the
//      bypass path's impulse must land at exactly the reported sample.
//    * report a constant per factor but let the factor's own number move. Leg A
//      pins the whole {factor} x {algorithm} x {drive} grid, and Leg A2 pins it
//      across a LIVE Drive sweep -- the user's actual gesture, including the duck
//      the threshold crossing still opens.
// ---------------------------------------------------------------------------
static void testOversamplingLatencyIsFactorOnly()
{
    std::printf ("Test 52: reported latency follows the Oversampling factor alone (ADR-0034)\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;
    const int block = 256;

    const anamorph::OversampleFactor osv[4] = { anamorph::OversampleFactor::Off,
                                                anamorph::OversampleFactor::x2,
                                                anamorph::OversampleFactor::x4,
                                                anamorph::OversampleFactor::x8 };
    const char* osName[4] = { "Off", "2x", "4x", "8x" };

    // ---- LEG A: the whole grid, statically -------------------------------
    int perFactor[4] = { 0, 0, 0, 0 };
    {
        anamorph::AnamorphEngine ref;
        ref.prepare (sr, block);
        for (int o = 0; o < 4; ++o)
        {
            anamorph::EngineParameters q;
            q.oversample = osv[o];
            perFactor[o] = ref.predictLatency (q);
        }
        std::printf ("  per-factor latency: Off=%d 2x=%d 4x=%d 8x=%d\n",
                     perFactor[0], perFactor[1], perFactor[2], perFactor[3]);
        check (perFactor[0] == 0, "oversampling Off reports zero latency");
        check (perFactor[1] > 0 && perFactor[2] > 0 && perFactor[3] > 0,
               "every selected factor reports a latency");
        // NON-VACUITY for the whole test: "constant per factor" would be trivially
        // true if every factor reported the same number.
        check (perFactor[1] != perFactor[2],
               "non-vacuity: the factors do not all report the same number");

        int moved = 0;
        for (int o = 0; o < 4; ++o)
            for (int a = 0; a < 4; ++a)
                for (float drive : { 0.0f, 0.005f, 0.011f, 6.0f, 24.0f })
                {
                    anamorph::EngineParameters q;
                    q.oversample = osv[o];
                    q.algorithm  = static_cast<anamorph::Algorithm> (a);
                    q.driveDb    = drive;
                    q.algoAmount = 0.7f;
                    if (ref.predictLatency (q) != perFactor[o]) ++moved;
                }
        check (moved == 0,
               "no algorithm or drive value moves the predicted latency of a factor (80 combinations)");
    }

    // ---- LEG A2: a LIVE drive sweep across the engagement threshold -------
    // The gesture the user reported. The OS path still switches here (the duck
    // still opens -- osActiveFor is still a discreteDiffers term), so this also
    // proves the latency holds THROUGH the duck, not merely on either side of it.
    for (int o = 1; o < 4; ++o)
    {
        anamorph::EngineParameters p;
        p.oversample = osv[o];
        p.driveDb    = 6.0f;                     // wrap engaged
        anamorph::AnamorphEngine engine;
        engine.primeParameters (p);
        engine.prepare (sr, block);
        engine.setParameters (p);

        juce::AudioBuffer<float> buf (2, block);
        int worstLat = perFactor[o], bestLat = perFactor[o];
        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * 220.0 / sr;
        for (int n = 0; n < 400; ++n)
        {
            // 6 dB -> 0 over the first 120 blocks, then hold: the threshold is
            // crossed mid-sweep and the duck runs to completion inside the hold.
            p.driveDb = juce::jmax (0.0f, 6.0f - 6.0f * (float) n / 120.0f);
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.25f * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            engine.setParameters (p);
            engine.process (buf);
            worstLat = juce::jmax (worstLat, engine.getLatencySamples());
            bestLat  = juce::jmin (bestLat,  engine.getLatencySamples());
        }
        std::printf ("  %s: drive 6 -> 0 live, reported latency stayed in [%d, %d] (expected %d)\n",
                     osName[o], bestLat, worstLat, perFactor[o]);
        check (worstLat == perFactor[o] && bestLat == perFactor[o],
               "a live drive sweep across the engagement threshold never moves the reported latency");
    }

    // ---- LEG B: reported == ACTUAL, in the skipped-wrap state -------------
    // The state the tree had no coverage for: a factor selected with the wrap
    // skipped. Bypass makes the measurement exact -- the output is the raw input
    // read from the ring at the reported offset, with no filter to smear the peak.
    for (int o = 1; o < 4; ++o)
    {
        anamorph::EngineParameters p;
        p.bypass     = true;
        p.oversample = osv[o];
        p.driveDb    = 0.0f;                     // wrap SKIPPED -- the new path
        anamorph::AnamorphEngine engine;
        engine.primeParameters (p);
        engine.prepare (sr, block);
        engine.setParameters (p);

        const int lat = engine.getLatencySamples();
        check (lat == perFactor[o], "the skipped-wrap state reports the factor's own latency");

        const int N = 4096;
        juce::AudioBuffer<float> buf (2, N);
        buf.clear();
        buf.setSample (0, 0, 1.0f);
        buf.setSample (1, 0, 1.0f);
        for (int off = 0; off < N; off += block)
        {
            const int len = juce::jmin (block, N - off);
            float* chans[2] = { buf.getWritePointer (0) + off, buf.getWritePointer (1) + off };
            juce::AudioBuffer<float> sub (chans, 2, len);
            engine.setParameters (p);
            engine.process (sub);
        }
        int peakPos = -1; float peak = 0.0f;
        for (int i = 0; i < N; ++i)
            if (std::abs (buf.getSample (0, i)) > peak) { peak = std::abs (buf.getSample (0, i)); peakPos = i; }
        std::printf ("  %s, drive 0: reported %d, impulse peak at %d\n", osName[o], lat, peakPos);
        check (peakPos == lat, "the chain really carries the latency it reports with the wrap skipped");
    }

    // ---- LEG C: the CPU optimisation survives, exactly ---------------------
    // Twin instances fed the identical noise stream: one with oversampling Off,
    // one with the factor selected and Drive at 0. If the wrap is genuinely still
    // skipped, the second is the first delayed by exactly `lat` and NOTHING else,
    // bit for bit. A build that "fixed" the latency by always running the
    // oversampler fails here by a wide margin -- the half-band IIR round trip is a
    // real filter, not an integer shift. Multiband on so Mix parks in the
    // full-wet gate: the m=1 blend is then skipped and the output is the exact
    // wet, rather than a one-ULP re-blend against dry read at two different offsets.
    //
    // WHY BIT-EXACT IS THE RIGHT ASSERTION *HERE*, AND MUST NOT BE COPIED BLINDLY.
    // The chain is shift-invariant in this configuration -- the widener is parked
    // (`algoAmount` at its 0 default, so all three modules take their A7-9 fixpoint
    // path), the multiband bank is LTI from zero state, and Width / Output are
    // memoryless -- so feeding it the same samples `lat` later produces the same
    // samples `lat` later, exactly. That is NOT a property of the engine at large:
    // `HaasProcessor` computes `readPos = widx - delaySamps` with `delaySamps` a
    // float, so its fractional interpolation coefficient depends on the ABSOLUTE
    // write index and a shifted input can round differently (measured with
    // `algoAmount = 0.7`: 6.5e-05 at 20 ms / 48 kHz, 6.4e-05 at the default 12 ms /
    // 44.1 kHz, and exactly 0 at 12.3 ms / 48 kHz -- it depends where the value
    // lands in the ULP grid). Any new leg that ENGAGES a widener must therefore use
    // a bound, not equality. That non-shift-invariance is a pre-existing
    // `HaasProcessor` property, unrelated to and untouched by ADR-0034.
    for (int o = 1; o < 4; ++o)
    {
        anamorph::EngineParameters a, b;
        a.mbEnable = b.mbEnable = true;          // parks Mix in the H4 full-wet gate
        a.oversample = anamorph::OversampleFactor::Off;
        b.oversample = osv[o];                   // drive stays at its 0 default

        anamorph::AnamorphEngine ea, eb;
        ea.primeParameters (a); ea.prepare (sr, block); ea.setParameters (a);
        eb.primeParameters (b); eb.prepare (sr, block); eb.setParameters (b);
        const int lat = eb.getLatencySamples();
        check (ea.getLatencySamples() == 0 && lat == perFactor[o],
               "the twins report 0 and the factor's latency respectively");

        const int N = 32 * block;
        juce::AudioBuffer<float> src (2, N), ba (2, N), bb (2, N);
        fillNoise (src, (unsigned) (4000 + o));
        ba.makeCopyOf (src); bb.makeCopyOf (src);
        for (int off = 0; off < N; off += block)
        {
            float* pa[2] = { ba.getWritePointer (0) + off, ba.getWritePointer (1) + off };
            float* pb[2] = { bb.getWritePointer (0) + off, bb.getWritePointer (1) + off };
            juce::AudioBuffer<float> sa (pa, 2, block), sb (pb, 2, block);
            ea.setParameters (a); ea.process (sa);
            eb.setParameters (b); eb.process (sb);
        }

        float worst = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < N - lat; ++i)
                worst = juce::jmax (worst, std::abs (bb.getSample (ch, i + lat) - ba.getSample (ch, i)));
        std::printf ("  %s, drive 0: |OS-selected output shifted by %d - OS-off output| = %.3e\n",
                     osName[o], lat, worst);
        check (worst == 0.0f,
               "with the wrap skipped the output is the OS-off output delayed, bit for bit "
               "(so the oversampler is genuinely NOT running)");
    }

    // ---- LEG D: the click this ALSO removes, while TRUE-BYPASSED --------
    // A second, independent consequence of a moving `lat`, found while auditing
    // the change and measured rather than assumed. In true bypass the output IS
    // the raw-input ring read at `-lat`, and the Bypass crossfade is applied
    // AFTER the switch duck's gain -- so while fully bypassed the duck does not
    // attenuate anything, and a `lat` that moved on the Drive threshold jumped
    // the ring's read position by 4-6 samples at FULL LEVEL. Measured on the
    // pre-ADR-0034 engine with a 220 Hz / 0.5 sine, whose largest possible
    // sample-to-sample step is 2*A*sin(pi*f/fs) = 0.01440: worst step 0.06821
    // (2x) and 0.09445 (4x and 8x), i.e. 4.7x and 6.6x a smooth signal's bound.
    // With the reported latency constant the read position cannot move, and the
    // measured worst step is exactly the smooth bound.
    //
    // THE START PHASE IS SWEPT, and that is not decoration. A 4-6 sample jump in
    // the read position steps the output by |x(t) - x(t+lat)|, which is near ZERO
    // if the jump lands at a peak of the sine and near maximal at a zero crossing.
    // A single fixed phase therefore measures whatever the block arithmetic
    // happens to line up: the first draft of this leg ran at one phase and passed
    // against the DEFECTIVE engine, while the same code at a different block size
    // measured 0.068 / 0.094. Sixteen offsets cover the cycle, and the worst is
    // taken -- so at least one lands where the jump is visible, whatever the block
    // size and fade timing do.
    for (int o = 1; o < 4; ++o)
    {
        const double sineHz = 220.0, amp = 0.5;
        const double inc = 2.0 * 3.14159265358979 * sineHz / sr;
        const float bound = (float) (2.0 * amp * std::sin (3.14159265358979 * sineHz / sr));

        float worstStep = 0.0f;
        for (int ph = 0; ph < 16; ++ph)
        {
            anamorph::EngineParameters p;
            p.bypass     = true;
            p.oversample = osv[o];
            p.driveDb    = 6.0f;                 // wrap engaged; the sweep disengages it
            anamorph::AnamorphEngine engine;
            engine.primeParameters (p);
            engine.prepare (sr, block);
            engine.setParameters (p);

            juce::AudioBuffer<float> buf (2, block);
            double phase = 2.0 * 3.14159265358979 * (double) ph / 16.0;
            float prev = 0.0f; bool started = false;
            for (int n = 0; n < 120; ++n)
            {
                if (n == 60) p.driveDb = 0.0f;   // cross the threshold while bypassed
                for (int i = 0; i < block; ++i)
                {
                    const float v = (float) (amp * std::sin (phase)); phase += inc;
                    buf.setSample (0, i, v); buf.setSample (1, i, v);
                }
                engine.setParameters (p);
                engine.process (buf);
                // Skip the first 40 blocks: the ring is still filling from silence,
                // so its own zero-to-signal edge is not the thing under test.
                for (int i = 0; i < block; ++i)
                {
                    const float sm = buf.getSample (0, i);
                    if (started && n >= 40) worstStep = juce::jmax (worstStep, std::abs (sm - prev));
                    prev = sm; started = true;
                }
            }
        }
        std::printf ("  %s bypassed, drive 6 -> 0: worst sample step over 16 phases %.5f "
                     "(smooth bound %.5f)\n", osName[o], worstStep, bound);
        check (worstStep <= bound * 1.05f,
               "crossing the engagement threshold while true-bypassed no longer jumps the "
               "delay-aligned read position at full level");
    }

    // ---- LEG E: the knob move must not INTERRUPT the sound either -------
    // The reported number holding still (legs A / A2) is only half of what the
    // report asked for. Crossing the Drive threshold with a factor selected is
    // still a discrete PATH change, so it still opens the click-free duck -- and
    // an ordinary duck fades to SILENCE. That is an interruption on an ordinary
    // knob move, which is the thing being complained about, and it is invisible to
    // every other leg here. Measured before the dry-fill branch existed: the output
    // fell to -52.6 / -53.3 / -53.3 dB at 2x / 4x / 8x and spent 6.7 ms more than
    // 20 dB down, inside a ~34 ms envelope.
    //
    // OVERSAMPLING OFF IS THE CONTROL, and it is what makes this leg honest: the
    // identical knob move with no factor selected opens no duck at all, so its
    // shallow dip (-0.6 dB, the drive blend itself easing out) is the floor any
    // correct build must match. Asserting against a fixed dB number instead would
    // have to guess how much of the dip belongs to the drive blend.
    {
        const double sineHz = 440.0, amp = 0.7;
        const double inc = 2.0 * 3.14159265358979 * sineHz / sr;

        auto worstDipRatio = [&] (anamorph::OversampleFactor f)
        {
            anamorph::EngineParameters p;
            p.oversample = f;
            p.algorithm  = anamorph::Algorithm::Haas;
            p.algoAmount = 0.5f;
            p.driveDb    = 0.4f;                 // just above the 0.01 dB threshold
            anamorph::AnamorphEngine engine;
            engine.primeParameters (p);
            engine.prepare (sr, 64);
            engine.setParameters (p);

            juce::AudioBuffer<float> buf (2, 64);
            double phase = 0.0, settled = 0.0, minRms = 1.0e9;
            for (int n = 0; n < 200; ++n)
            {
                // a smooth knob move 0.4 -> 0 dB over ~27 ms, crossing at n ~= 59
                if (n >= 40 && n <= 60) p.driveDb = 0.4f * (1.0f - (float) (n - 40) / 20.0f);
                else if (n > 60)        p.driveDb = 0.0f;
                for (int i = 0; i < 64; ++i)
                {
                    const float v = (float) (amp * std::sin (phase)); phase += inc;
                    buf.setSample (0, i, v); buf.setSample (1, i, v);
                }
                engine.setParameters (p);
                engine.process (buf);
                double acc = 0.0;
                for (int i = 0; i < 64; ++i) { const double sm = buf.getSample (0, i); acc += sm * sm; }
                const double rms = std::sqrt (acc / 64.0);
                if (n == 35) settled = rms;
                if (n >= 40) minRms = juce::jmin (minRms, rms);
            }
            return minRms / juce::jmax (1.0e-12, settled);
        };

        const double control = worstDipRatio (anamorph::OversampleFactor::Off);
        std::printf ("  drive 0.4 -> 0 dB: OS Off keeps %.4f of its settled level (the control)\n", control);
        check (control > 0.5, "CONTROL: with no factor selected the same knob move does not dip");

        for (int o = 1; o < 4; ++o)
        {
            const double r = worstDipRatio (osv[o]);
            std::printf ("  drive 0.4 -> 0 dB at %s: keeps %.4f of settled (%+.1f dB)\n",
                         osName[o], r, 20.0 * std::log10 (juce::jmax (1.0e-12, r)));
            check (r > control * 0.8,
                   "an ordinary Drive move through the engagement threshold no longer "
                   "interrupts the sound (the duck dry-fills instead of muting)");
        }
    }
}

// A/B swap / preset recall / undo: what does the listener actually get?
// ---------------------------------------------------------------------------
//  Test 53 -- crossing the Drive threshold with Oversampling selected must be
//  indistinguishable from crossing it with Oversampling Off (0.9.7).
//
//  WHAT WENT WRONG, AND WHY THE EARLIER TESTS DID NOT SEE IT. Engaging or
//  disengaging the oversampling wrap swaps the whole nonlinear region between the
//  resampled and the base-rate path. That used to be a latched swap at the silent
//  bottom of the switch duck -- and THE DUCK COULD NOT MASK IT, because the duck's
//  gain is applied at the output stage, DOWNSTREAM of Haas (12-35 ms) and Velvet
//  (~21 ms). The handover's discontinuity went into their delay lines at full
//  level and re-emerged after the ~28 ms fade-in was over, unmasked. Measured on a
//  220 Hz tone as a multiple of the settled sample-to-sample step, arriving at
//  duck bottom + the widener's own delay:
//
//      Haas   0 -> 6 dB   2.6x      Velvet 0 -> 6 dB   5.8x
//      Haas   6 -> 0 dB   1.2x      Velvet 6 -> 0 dB   2.4x
//
//  Test 52 leg E measures the same gesture and passes throughout: it reads BLOCK
//  RMS, and a few-sample discontinuity 19-28 ms downstream does not move a block's
//  RMS. Nothing in the suite inspected the signal at sample resolution across this
//  transition, which is why the defect survived the round that fixed the latency.
//
//  THE CONTROL IS OVERSAMPLING OFF, and that is what makes this test honest. The
//  same knob move with no factor selected engages no path swap at all, so whatever
//  it measures is the Drive change ITSELF -- the tanh shaping arriving, the drive
//  blend easing in. Requiring the oversampled runs to match that control asks the
//  only question worth asking: can you tell, from the signal, that the wrap was
//  switched? Asserting a fixed threshold instead would have to guess how much of
//  the number belongs to Drive.
//
//  BOTH GESTURES, because they fail differently. An instantaneous step is what
//  automation and preset recall deliver; a 300 ms sweep is what a knob delivers,
//  and it is what was reported. The instantaneous case additionally caught a second
//  defect the sweep does not: the drive envelope advances once per sample, so the
//  wrapped path ran it `factor` times faster than the base-rate path and the two
//  diverged mid-crossfade -- 2.0x / 4.0x / 7.3x at 2x / 4x / 8x, scaling with the
//  factor, which is the signature.
// ---------------------------------------------------------------------------
namespace
{
    struct CrossResult { double stepRatio = 0.0, holeRatio = 0.0; };

    // One Drive crossing, measured against its OWN settled window so the numbers
    // are comparable across factors and algorithms.
    static CrossResult measureDriveCrossing (anamorph::OversampleFactor f,
                                             anamorph::Algorithm alg,
                                             bool zeroToSix, int sweepBlocks)
    {
        constexpr double sr = 48000.0; constexpr int bs = 64;
        const int settle = 400, tail = 400, moveAt = 400;

        anamorph::EngineParameters p;
        p.algorithm  = alg;
        p.algoAmount = 0.7f;
        p.width      = 1.4f;
        p.oversample = f;
        p.driveDb    = zeroToSix ? 0.0f : 6.0f;
        const float after = zeroToSix ? 6.0f : 0.0f;

        anamorph::AnamorphEngine e;
        e.primeParameters (p); e.prepare (sr, bs); e.setParameters (p);

        std::vector<float> out; out.reserve ((size_t) (settle + tail) * bs);
        juce::AudioBuffer<float> buf (2, bs);
        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * 220.0 / sr;
        for (int nb = 0; nb < settle + tail; ++nb)
        {
            if (sweepBlocks <= 0) { if (nb == moveAt) p.driveDb = after; }
            else if (nb >= moveAt && nb <= moveAt + sweepBlocks)
            {
                const float t = (float) (nb - moveAt) / (float) sweepBlocks;
                p.driveDb = zeroToSix ? 6.0f * t : 6.0f * (1.0f - t);
            }
            for (int i = 0; i < bs; ++i)
            {
                const float v = 0.5f * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            e.setParameters (p);
            e.process (buf);
            for (int i = 0; i < bs; ++i) out.push_back (buf.getSample (0, i));
        }

        const int mv = moveAt * bs;
        double refStep = 0.0, refSq = 0.0; int refN = 0;
        for (int i = mv - 100 * bs + 1; i < mv; ++i)
        {
            refStep = juce::jmax (refStep, (double) std::abs (out[(size_t) i] - out[(size_t) i - 1]));
            refSq += (double) out[(size_t) i] * out[(size_t) i]; ++refN;
        }
        const double refRms = std::sqrt (refSq / juce::jmax (1, refN));

        CrossResult res;
        double worstStep = 0.0;
        for (int i = mv; i < (int) out.size(); ++i)
            worstStep = juce::jmax (worstStep, (double) std::abs (out[(size_t) i] - out[(size_t) i - 1]));
        res.stepRatio = worstStep / juce::jmax (1.0e-9, refStep);

        double minRms = 1.0e9;
        for (int i = mv; i + 32 < (int) out.size(); ++i)
        {
            double sq = 0.0;
            for (int k = 0; k < 32; ++k) { const double v = out[(size_t) (i + k)]; sq += v * v; }
            minRms = juce::jmin (minRms, std::sqrt (sq / 32.0));
        }
        res.holeRatio = minRms / juce::jmax (1.0e-12, refRms);
        return res;
    }
}

static void testDriveCrossingIsSeamlessWithOversampling()
{
    std::printf ("Test 53: a Drive crossing with Oversampling on matches the Oversampling-Off control\n");
    juce::ScopedNoDenormals noDenormals;

    const anamorph::OversampleFactor osv[3] = { anamorph::OversampleFactor::x2,
                                                anamorph::OversampleFactor::x4,
                                                anamorph::OversampleFactor::x8 };
    const char* osName[3] = { "2x", "4x", "8x" };

    for (int alg = 0; alg < 2; ++alg)
        for (int dir = 0; dir < 2; ++dir)
            for (int gesture = 0; gesture < 2; ++gesture)
            {
                const auto algorithm = alg == 0 ? anamorph::Algorithm::Haas
                                                : anamorph::Algorithm::Velvet;
                const bool zeroToSix = (dir == 0);
                const int  sweep     = (gesture == 0) ? 0 : 225;   // 0 = step, 225 blocks = 300 ms
                const char* algName  = alg == 0 ? "Haas  " : "Velvet";
                const char* gName    = gesture == 0 ? "step " : "sweep";

                const auto ctl = measureDriveCrossing (anamorph::OversampleFactor::Off,
                                                       algorithm, zeroToSix, sweep);
                // NON-VACUITY: the control must be a real measurement, not a
                // degenerate one -- a settled window that measured no slope, or a
                // run that fell silent, would make every comparison below free.
                check (ctl.stepRatio > 0.1 && ctl.holeRatio > 0.05,
                       "CONTROL: the Oversampling-Off run of this gesture is well-formed");

                for (int o = 0; o < 3; ++o)
                {
                    const auto r = measureDriveCrossing (osv[o], algorithm, zeroToSix, sweep);
                    std::printf ("  %s %s %s drive %s : step x%.2f (control x%.2f) | level %.4f (control %.4f)\n",
                                 osName[o], algName, gName, zeroToSix ? "0->6" : "6->0",
                                 r.stepRatio, ctl.stepRatio, r.holeRatio, ctl.holeRatio);
                    // 1.25x and 0.75x: the measured margins are far outside them in
                    // both directions -- a correct build matches the control to
                    // ~1 % on the step and ~0.1 % on the level, while the defective
                    // one ran 1.3x to 4.4x the control's step, and the build before
                    // the latency work fell to a level ratio of 0.0000 (true digital
                    // silence). Nothing sits near either bound.
                    check (r.stepRatio <= ctl.stepRatio * 1.25,
                           "no discontinuity beyond the Oversampling-Off control across the Drive crossing");
                    check (r.holeRatio >= ctl.holeRatio * 0.75,
                           "no level hole beyond the Oversampling-Off control across the Drive crossing");
                }
            }
}

// ---------------------------------------------------------------------------
// Shared instrument for the Oversampling SWITCH (a factor change, which ducks).
//
// The observable is **H3/H1**, the third-harmonic ratio at a 1 kHz probe, by
// Hann-windowed Goertzel. Drive is an odd nonlinearity, so this is its signature:
// drive present -> the settled value, drive replaced by the raw input -> collapse.
// It is a RATIO of two bins measured in the same window, which is what makes it
// usable here at all -- the switch duck's fade is a large, TIME-VARYING gain
// sitting on top of everything in this window, so a waveform-difference or a bare
// level metric is dominated by the envelope exactly where the defect lives. (A
// first attempt using a best-fit residual `rms(out - g*in)/rms(out)` is invariant
// only to a CONSTANT gain and showed nothing.)
//
// Modulation cannot be read directly across this transition and is not tried: the
// chorus is reset at every duck bottom by design, so with a mono probe its side/mid
// ratio reads 0.000 there on a factor->factor CONTROL exactly as it does on the
// legs. The drive stage and the mod algorithms share the single wrapped buffer and
// stand or fall with it, so a mod scenario is run WITH drive and read the same way.
namespace
{
    struct OsSwitchTrace
    {
        std::vector<float> l, r;
        int bottom = 0;      // index of the duck's silent bottom (min-RMS after the change)
        int change = 0;      // index of the parameter change
        int latMoves = 0;    // how many times the reported latency changed during the run
        bool latValuesOk = true; // ... and whether it only ever held the two expected values
    };

    static OsSwitchTrace runOsSwitch (anamorph::OversampleFactor from,
                                      anamorph::OversampleFactor to,
                                      anamorph::Algorithm alg, float driveDb)
    {
        constexpr double sr = 48000.0; constexpr int bs = 64;
        constexpr int settle = 400, tail = 400;
        anamorph::EngineParameters p;
        p.algorithm  = alg;
        p.algoAmount = 0.7f;
        p.driveDb    = driveDb;
        p.oversample = from;
        anamorph::AnamorphEngine e;
        e.primeParameters (p); e.prepare (sr, bs); e.setParameters (p);

        anamorph::EngineParameters probeTo = p; probeTo.oversample = to;
        const int latFrom = e.predictLatency (p);
        const int latTo   = e.predictLatency (probeTo);

        OsSwitchTrace t;
        t.l.reserve ((size_t) (settle + tail) * bs);
        t.r.reserve ((size_t) (settle + tail) * bs);
        juce::AudioBuffer<float> buf (2, bs);
        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * 1000.0 / sr;
        int lastLat = e.getLatencySamples();
        for (int nb = 0; nb < settle + tail; ++nb)
        {
            if (nb == settle) p.oversample = to;
            for (int i = 0; i < bs; ++i)
            {   // MONO on purpose: every bit of L-R below was made by the engine
                const float v = 0.6f * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            e.setParameters (p); e.process (buf);
            const int lat = e.getLatencySamples();
            if (lat != lastLat) { ++t.latMoves; lastLat = lat; }
            if (lat != latFrom && lat != latTo) t.latValuesOk = false;
            for (int i = 0; i < bs; ++i) { t.l.push_back (buf.getSample (0, i)); t.r.push_back (buf.getSample (1, i)); }
        }
        t.change = settle * bs;

        // The duck's bottom, found rather than assumed: the quietest 128-sample
        // window after the change. Everything the blend does happens from here on,
        // because this is where the deferred discrete state is adopted.
        double best = 1.0e30;
        for (int i = t.change; i + 128 < (int) t.l.size() && i < t.change + (int) (0.05 * sr); i += 8)
        {
            double sq = 0.0;
            for (int k = 0; k < 128; ++k) { const double v = t.l[(size_t) (i + k)]; sq += v * v; }
            if (sq < best) { best = sq; t.bottom = i; }
        }
        return t;
    }

    // Hann-windowed Goertzel magnitude at `hz` over `w` samples from `at`.
    static double goertzelMag (const std::vector<float>& v, int at, int w, double hz, double sr)
    {
        const double k = 2.0 * std::cos (2.0 * 3.14159265358979 * hz / sr);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < w; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / (w - 1));
            const double s0 = win * v[(size_t) (at + i)] + k * s1 - s2;
            s2 = s1; s1 = s0;
        }
        return std::sqrt (s1 * s1 + s2 * s2 - k * s1 * s2);
    }

    // H3/H1 of a 1 kHz probe in a 256-sample (5.3 ms) window -- short enough to sit
    // INSIDE the 12 ms blend rather than straddle it, long enough for 5.3 cycles.
    static double h3Over1 (const OsSwitchTrace& t, int at)
    {
        const double h1 = goertzelMag (t.l, at, 256, 1000.0, 48000.0);
        const double h3 = goertzelMag (t.l, at, 256, 3000.0, 48000.0);
        return h1 > 1.0e-12 ? h3 / h1 : 0.0;
    }

    static double blockRms (const std::vector<float>& v, int at, int w)
    { double s = 0.0; for (int i = 0; i < w; ++i) { const double x = v[(size_t) (at + i)]; s += x * x; } return std::sqrt (s / w); }

    // `metric` averaged over the settled tail. A modulating algorithm makes any
    // spectral observable breathe, so a SINGLE settled window is not a reference;
    // 32 of them are. The chorus is reset at the duck bottom in every run here, so
    // its LFO phase is aligned across leg and control and the breathing cancels.
    template <typename Fn>
    static double settledMean (const OsSwitchTrace& t, Fn metric)
    {
        double acc = 0.0; int cnt = 0;
        for (int at = (int) t.l.size() - 8192; at + 512 < (int) t.l.size(); at += 256)
        { acc += metric (t, at); ++cnt; }
        return cnt > 0 ? acc / cnt : 0.0;
    }

    // How much of the processing survived the handoff, as a fraction of what the
    // SAME duck keeps on the control -- worst window over the blend's own span
    // (the duck bottom, where the new discrete state is adopted, to +20 ms, which
    // covers the 12 ms ramp with room to spare). Each side is normalised by its own
    // settled level first, so a legitimate steady-state difference between the two
    // end states does not read as a loss. 1.0 = the leg gave up nothing the control
    // did not give up too.
    template <typename Fn>
    static double blendKeepVsControl (const OsSwitchTrace& leg, const OsSwitchTrace& ctl, Fn metric)
    {
        const double legS = settledMean (leg, metric), ctlS = settledMean (ctl, metric);
        if (legS <= 1.0e-9 || ctlS <= 1.0e-9) return 0.0;
        double worst = 1.0e30;
        for (int off = 0; off <= 960; off += 32)
        {
            const double a = metric (leg, leg.bottom + off) / legS;
            const double b = metric (ctl, ctl.bottom + off) / ctlS;
            if (b > 1.0e-3) worst = juce::jmin (worst, a / b);
        }
        return worst;
    }
}

static void testOversamplingOffHandoffKeepsProcessing()
{
    std::printf ("Test 54: switching Oversampling to Off does not lose the processing during the handoff\n");
    juce::ScopedNoDenormals noDenormals;

    const anamorph::OversampleFactor osv[3] = { anamorph::OversampleFactor::x2,
                                                anamorph::OversampleFactor::x4,
                                                anamorph::OversampleFactor::x8 };
    const char* osName[3] = { "2x", "4x", "8x" };

    // TWO SCENARIOS, ONE OBSERVABLE. The oversampled region holds the drive stage
    // AND the modulation algorithms in a single buffer: if that buffer is not
    // computed, everything in it is gone together, which is why one probe answers
    // for both. The probe is the drive's third harmonic, because it is the only
    // one of the two that can be read WHILE the handoff is happening: the chorus
    // is deliberately reset at every duck bottom (stale audio would otherwise
    // replay as the fade lifts), so its output is dry for its own delay length
    // afterwards no matter which path ran -- measured, the side/mid ratio reads
    // 0.000 there on the factor->factor control exactly as it does on the legs.
    // Scenario B therefore runs Chorus WITH drive: `osActiveFor` is then true
    // through both of its predicates, the mod algorithm really is inside the
    // wrapped buffer, and H3/H1 still reports whether that buffer was computed.
    struct Scenario { const char* name; anamorph::Algorithm alg; float drive; };
    const Scenario scen[2] = { { "Haas  +drive", anamorph::Algorithm::Haas,   18.0f },
                               { "Chorus+drive", anamorph::Algorithm::Chorus, 18.0f } };

    auto h3 = [] (const OsSwitchTrace& t, int at) { return h3Over1 (t, at); };

    auto stepRatio = [] (const OsSwitchTrace& t)
    {
        double ref = 0.0, worst = 0.0;
        for (int i = t.change - 6400 + 1; i < t.change; ++i)
            ref = juce::jmax (ref, (double) std::abs (t.l[(size_t) i] - t.l[(size_t) (i - 1)]));
        for (int i = t.change + 1; i < (int) t.l.size(); ++i)
            worst = juce::jmax (worst, (double) std::abs (t.l[(size_t) i] - t.l[(size_t) (i - 1)]));
        return worst / juce::jmax (1.0e-9, ref);
    };

    for (const auto& sc : scen)
    {
        // THE CONTROL IS THE SAME DUCK. A factor->factor switch (2x -> 4x) opens the
        // identical click-free duck, moves the reported latency the same way, and
        // resets the same oversamplers, the same chorus and the same stand-in ring
        // -- it differs from the legs below in ONE respect: the wrap runs on both
        // sides of it, so the path crossfade has nothing to hand over. Whatever the
        // duck itself costs therefore shows up here too, and every threshold below
        // is calibrated against a measurement instead of a guessed constant.
        const auto ctl = runOsSwitch (anamorph::OversampleFactor::x2,
                                      anamorph::OversampleFactor::x4, sc.alg, sc.drive);
        const double ctlSettled = settledMean (ctl, h3);
        const double ctlStep    = stepRatio (ctl);
        const double ctlSettledRms = blockRms (ctl.l, (int) ctl.l.size() - 4096, 2048);
        double ctlFloor = 1.0e30;
        for (int off = 0; off <= 960; off += 32) ctlFloor = juce::jmin (ctlFloor, h3 (ctl, ctl.bottom + off));
        std::printf ("  CONTROL %s 2x -> 4x (wrap runs throughout) : settled H3/H1 %.3f | worst in window %.3f | step x%.2f\n",
                     sc.name, ctlSettled, ctlFloor, ctlStep);
        // NON-VACUITY: the control has to be a real measurement -- a probe that saw
        // no drive at all, or a denominator at the numerical floor, would make every
        // comparison below free. (The control's OWN dip is not disqualifying and is
        // not asserted against: the chorus is reset at every duck bottom, so with a
        // mod algorithm the control dips there too, which is exactly why the legs
        // are measured AGAINST it rather than against a constant.)
        check (ctlSettled > 0.05 && ctlFloor > 0.01 && ctlStep > 0.1,
               "CONTROL: the factor->factor switch is a well-formed measurement");

        for (int o = 0; o < 3; ++o)
        {
            const auto d = runOsSwitch (osv[o], anamorph::OversampleFactor::Off, sc.alg, sc.drive);
            const double keep = blendKeepVsControl (d, ctl, h3);

            // Level: each run is normalised by its own settled RMS first, then
            // compared window-for-window against the control's envelope, so this
            // asks "did the output dip below what this very duck costs anyway".
            const double dSettledRms = blockRms (d.l, (int) d.l.size() - 4096, 2048);
            double levelKeep = 1.0e30;
            for (int off = 0; off <= 960; off += 32)
            {
                const double x = blockRms (d.l,   d.bottom   + off, 128) / dSettledRms;
                const double y = blockRms (ctl.l, ctl.bottom + off, 128) / ctlSettledRms;
                if (y > 1.0e-4) levelKeep = juce::jmin (levelKeep, x / y);
            }
            const double dStep = stepRatio (d);

            std::printf ("  %s %s -> Off : processing %.3f | level %.3f | step x%.2f (control x%.2f)\n",
                         sc.name, osName[o], keep, levelKeep, dStep, ctlStep);

            // 0.85 of the control, on both. The margins are not close: against the
            // DEFECTIVE engine these read 0.358 / 0.558 (Haas) and 0.371 / 0.547
            // (Chorus), against the FIXED one 0.998 / 0.991 and 0.950 / 0.969.
            // Nothing sits near the bound from either side.
            check (keep >= 0.85,
                   "the oversampled region keeps processing through the Oversampling -> Off handoff");
            check (levelKeep >= 0.85,
                   "the output does not collapse below what the same duck costs anyway");
            check (dStep <= ctlStep * 1.25,
                   "no click or transient beyond the factor->factor control");

            // ADR-0034 is untouched by this: the reported number is a function of
            // the Oversampling SELECTION, so it moves exactly once here -- at the
            // switch -- and only ever holds the two values that selection implies.
            check (d.latValuesOk,
                   "the reported latency only ever holds the two selections' own values");
            check (d.latMoves == 1,
                   "the reported latency moves exactly once, at the Oversampling switch");
        }
    }
}

// ---------------------------------------------------------------------------
//  Regression (R4 / F11): a discrete change that reaches NO module must not duck.
//
//  `discreteDiffers` opens a duck-to-silence -- ~6 ms out, ~28 ms in (ADR-0004).
//  That is the right trade for a change that genuinely rewires the graph, because
//  the swap then happens at silence and no stale tail survives it. It is the wrong
//  trade for `dimMode`, which is read by exactly one line -- `chorus.setDimMode`,
//  inside `else if (p.algorithm == Algorithm::DimensionD)` -- so under any other
//  algorithm the value reaches nothing at all and the duck buys nothing.
//
//  MEASURED, on this engine, before the fix: a host lane toggling dimMode between
//  two adjacent steps held the steady output 42.2 dB below the un-automated same
//  engine at 48 kHz / block 128, and the governing variable was the toggle INTERVAL
//  in ms, not the block count -- 2.667 ms gave -42.21 dB at 48k/256 crossings, at
//  96k/512 and at 192k/1024 alike, to two decimals. An automation lane crossing a
//  step boundary every few milliseconds is ordinary; nothing rate-limits it.
//
//  WHAT THIS ASSERTS. Not a state variable: the OUTPUT SAMPLES, against the same
//  engine with no automation at all. The inert case is bit-exact post-fix (measured
//  at 44.1 / 48 / 96 kHz and blocks 64 / 128 / 512, zero differing samples), so the
//  assertion is equality, not a threshold. Three positive controls follow it, and
//  they are the point of the test: the duck must still be there wherever the change
//  can be heard, and the value must still be adopted.
static void testInertDiscreteChangeDoesNotDuck()
{
    std::printf ("Test 55: an inert discrete change does not open a duck\n");
    juce::ScopedNoDenormals noDenormals;

    struct Rendered { std::vector<float> s; double rms = 0.0; };

    // toggleEvery <= 0 means no automation (the control). `field` picks what the
    // lane moves; everything else is held identical between the two renders.
    enum class Lane { None, DimMode, MbBands };
    auto render = [] (double sr, int block, int blocks, anamorph::Algorithm alg,
                      int startDimMode, Lane lane, int toggleEvery) -> Rendered
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.algorithm = alg; p.algoAmount = 0.7f; p.width = 1.4f; p.mix = 1.0f;
        p.mbEnable = true; p.mbBands = 2; p.dimMode = startDimMode;
        engine.setParameters (p);
        engine.reset();

        juce::AudioBuffer<float> buf (2, block);
        Rendered out; out.s.reserve ((size_t) (blocks * block));
        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * 1000.0 / sr;
        bool alt = false; int since = 0; double sumSq = 0.0;

        for (int b = 0; b < blocks; ++b)
        {
            if (toggleEvery > 0 && ++since >= toggleEvery)
            {
                since = 0; alt = ! alt;
                if (lane == Lane::DimMode) p.dimMode = alt ? startDimMode + 1 : startDimMode;
                else if (lane == Lane::MbBands) p.mbBands = alt ? 3 : 2;
            }
            engine.setParameters (p);
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase);
                phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s * 0.85f);
            }
            engine.process (buf);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                { const double v = buf.getSample (ch, i); sumSq += v * v; }
            for (int i = 0; i < block; ++i) out.s.push_back (buf.getSample (0, i));
        }
        out.rms = std::sqrt (sumSq / (2.0 * blocks * block));
        return out;
    };

    auto dbBetween = [] (double a, double ref)
    { return 20.0 * std::log10 (std::max (a, 1e-12) / std::max (ref, 1e-12)); };

    // --- THE ASSERTION. Under Haas, dimMode reaches no module, so a lane moving it
    //     at ANY cadence must leave the stream untouched -- sample for sample.
    const double rates[3]  = { 44100.0, 48000.0, 96000.0 };
    const int    blocks_[3] = { 64, 128, 512 };
    for (int r = 0; r < 3; ++r)
        for (int bi = 0; bi < 3; ++bi)
        {
            const auto ctl = render (rates[r], blocks_[bi], 300, anamorph::Algorithm::Haas, 1, Lane::None, 0);
            for (const int every : { 1, 2, 4 })
            {
                const auto aut = render (rates[r], blocks_[bi], 300, anamorph::Algorithm::Haas, 1,
                                         Lane::DimMode, every);
                size_t differing = 0;
                for (size_t i = 0; i < ctl.s.size(); ++i)
                    if (std::memcmp (&ctl.s[i], &aut.s[i], sizeof (float)) != 0) ++differing;
                if (differing != 0)
                    std::printf ("  [%.0f Hz, block %d, every %d] %zu/%zu samples differ, level %+.2f dB\n",
                                 rates[r], blocks_[bi], every, differing, ctl.s.size(),
                                 dbBetween (aut.rms, ctl.rms));
                check (differing == 0,
                       "an inert dimMode automation lane leaves the output bit-exact");
            }
        }

    // --- CONTROL 1: an AUDIBLE discrete change at the same cadence must STILL duck.
    //     The duck is not being removed, only its membership narrowed; if this stops
    //     ducking the fix has gone too far. Measured at -42.2 dB; asserted at -20.
    {
        const double sr = 48000.0; const int block = 128;
        const auto ctl = render (sr, block, 300, anamorph::Algorithm::Haas, 1, Lane::None, 0);
        const auto aut = render (sr, block, 300, anamorph::Algorithm::Haas, 1, Lane::MbBands, 1);
        const double d = dbBetween (aut.rms, ctl.rms);
        std::printf ("  control: mbBands lane (audible) still ducks to %+.2f dB\n", d);
        check (d < -20.0, "an audible discrete change (mbBands) still ducks");
    }

    // --- CONTROL 2: the SAME dimMode lane under the one algorithm that READS
    //     dimMode must still duck. This is what makes the exclusion an exclusion
    //     rather than a deletion. Measured at -34.4 dB; asserted at -20.
    {
        const double sr = 48000.0; const int block = 128;
        const auto ctl = render (sr, block, 300, anamorph::Algorithm::DimensionD, 1, Lane::None, 0);
        const auto aut = render (sr, block, 300, anamorph::Algorithm::DimensionD, 1, Lane::DimMode, 1);
        const double d = dbBetween (aut.rms, ctl.rms);
        std::printf ("  control: dimMode lane under Dimension D still ducks to %+.2f dB\n", d);
        check (d < -20.0, "dimMode still ducks under the algorithm that reads it");
    }

    // --- CONTROL 3: the value is still ADOPTED while inert. Not ducking must not
    //     mean not adopting: `sameParameters` still compares dimMode, so the change
    //     takes the ordinary continuous path and a later switch to Dimension D has
    //     to hear the NEW value. Asserted externally: move dimMode under Haas, then
    //     switch to Dimension D, and require the settled output to match an engine
    //     that carried the new value all along -- and NOT to match one that kept the
    //     old value. Two renders that must agree and one that must not.
    {
        const double sr = 48000.0; const int block = 128;
        auto switchToDimD = [&] (int dimBefore, int dimAfter) -> std::vector<float>
        {
            anamorph::AnamorphEngine engine;
            engine.prepare (sr, block);
            anamorph::EngineParameters p;
            p.algorithm = anamorph::Algorithm::Haas; p.algoAmount = 0.7f; p.width = 1.4f;
            p.mix = 1.0f; p.mbEnable = true; p.mbBands = 2; p.dimMode = dimBefore;
            engine.setParameters (p);
            engine.reset();

            juce::AudioBuffer<float> buf (2, block);
            std::vector<float> out; out.reserve ((size_t) (240 * block));
            double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * 1000.0 / sr;
            for (int b = 0; b < 240; ++b)
            {
                if (b == 40) p.dimMode = dimAfter;                              // inert here
                if (b == 80) p.algorithm = anamorph::Algorithm::DimensionD;     // now it matters
                engine.setParameters (p);
                for (int i = 0; i < block; ++i)
                {
                    const float s = (float) std::sin (phase);
                    phase += inc;
                    buf.setSample (0, i, s); buf.setSample (1, i, s * 0.85f);
                }
                engine.process (buf);
                for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
            }
            return out;
        };
        // Compare only the settled tail, well past the algorithm swap's own fade.
        // `from` is a PARAMETER rather than a capture: with `block` declared
        // `const int` the expression `160 * block` is a constant expression, so
        // `block` would not be odr-used and a `[block]` capture is dead -- which
        // Clang reports as -Wunused-lambda-capture and the first-party warning
        // gate rejects. Passing the index states the dependency instead of
        // leaving it resting on `block` happening to stay constexpr-usable.
        auto tailDiff = [] (const std::vector<float>& a, const std::vector<float>& b, size_t from)
        {
            double worst = 0.0;
            for (size_t i = from; i < a.size(); ++i)
                worst = std::max (worst, (double) std::abs (a[i] - b[i]));
            return worst;
        };
        const size_t settledFrom = (size_t) (160 * block);
        const auto moved   = switchToDimD (1, 3);   // 1 -> 3 while inert, then Dimension D
        const auto carried = switchToDimD (3, 3);   // 3 throughout
        const auto stale   = switchToDimD (1, 1);   // never moved: the wrong mode
        std::printf ("  control: adopted-while-inert tail |moved-carried| = %.3e, |moved-stale| = %.3e\n",
                     tailDiff (moved, carried, settledFrom), tailDiff (moved, stale, settledFrom));
        check (tailDiff (moved, carried, settledFrom) < 1e-6,
               "a dimMode moved while inert is adopted -- Dimension D hears the new mode");
        check (tailDiff (moved, stale, settledFrom) > 1e-3,
               "the adoption control is sharp: the stale mode sounds different");
    }
}

// ---------------------------------------------------------------------------
//  Regression (R4 / F8): the Multiband Enable crossfade must not STEP THE DRY
//  SOURCE at partial Mix.
//
//  ADR-0005 mixes the dry as a phase-matched `A(dry)` reconstructed through the
//  same crossovers as the wet. `A(dry)` is produced only while the multiband bank
//  is running, and `dryAligned` -- the flag the Mix stage picks its dry source from
//  -- is a BLOCK constant derived from `mbActive`. So when the enable crossfade
//  finished, the dry term jumped from `A(dry)` to the clean dry in ONE SAMPLE at
//  the next block boundary, with nothing fading it: a step of (1-Mix)*|A(dry)-dry|.
//
//  WHY TEST 23 DOES NOT COVER THIS. Test 23 runs at the default Mix of 1.0, where
//  the H4 gate drops the dry term altogether and no dry source exists to switch.
//  The defect lives strictly at 0 < Mix < 1, and only with more than one band (with
//  one band there is no crossover, so A(dry) == dry and the step is identically 0).
//
//  MEASURED before the fix, 48 kHz, 4 bands, crossovers 200/900/3500 Hz: +3.9 dBFS
//  and 18x the signal's own slew (1 kHz, Mix 0.05); -13.4 dBFS and 24x (100 Hz,
//  Mix 0.25, block 256); up to 89x at block 64. After it, every one of those cases
//  sits at 1.3-1.6x -- the signal's own slew, i.e. nothing.
//
//  WHAT THIS ASSERTS. The worst single-sample delta anywhere in the transition,
//  against the same signal's own MEDIAN delta in a steady window before it. A
//  crossfaded transition scores ~1; a one-sample source switch scores tens. The
//  bound is 3.0: far above every post-fix reading and far below every pre-fix one.
static void testMultibandEnableDrySourceNoStep()
{
    std::printf ("Test 56: Multiband Enable does not step the dry source at partial Mix\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;

    // Returns worstStep / medianSlew across the whole transition.
    auto ratio = [sr] (int block, int bands, float mix, bool startEnabled) -> double
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (sr, block);
        anamorph::EngineParameters p;
        p.algorithm = anamorph::Algorithm::Haas; p.algoAmount = 0.7f; p.width = 1.4f;
        p.mix = mix; p.mbEnable = startEnabled; p.mbBands = bands;
        // The bands must actually do something, or the wet path is identity and
        // there is no A(dry) to differ from the clean dry.
        p.mbWidthLow = 1.6f; p.mbWidthMid = 0.6f; p.mbWidthHiMid = 1.5f; p.mbWidthHigh = 0.7f;
        p.mbFreqLow = 200.0f; p.mbFreqMid = 900.0f; p.mbFreqHigh = 3500.0f;
        engine.setParameters (p);
        engine.reset();

        const int toggleAt = 40;
        const int blendSamples = (int) std::lround (0.012 * sr);   // mbEnableBlend, ~12 ms
        const int spanBlocks = blendSamples / block + 3;           // the blend AND the boundary after it
        const int blocks = toggleAt + spanBlocks + 4;

        juce::AudioBuffer<float> buf (2, block);
        std::vector<float> out; out.reserve ((size_t) (blocks * block));
        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * 100.0 / sr; // 100 Hz: a slow
                                                                                    // signal, so a step
                                                                                    // cannot hide in slew
        for (int b = 0; b < blocks; ++b)
        {
            if (b == toggleAt) p.mbEnable = ! startEnabled;
            engine.setParameters (p);
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase);
                phase += inc;
                buf.setSample (0, i, s); buf.setSample (1, i, s * 0.8f);
            }
            engine.process (buf);
            for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
        }

        std::vector<double> steady;
        for (int i = (toggleAt - 8) * block; i < (toggleAt - 2) * block; ++i)
            steady.push_back (std::abs ((double) out[(size_t) i] - out[(size_t) (i - 1)]));
        std::sort (steady.begin(), steady.end());
        const double medianSlew = std::max (steady[steady.size() / 2], 1e-9);

        double worst = 0.0;
        for (int i = toggleAt * block; i < (toggleAt + spanBlocks) * block; ++i)
            worst = std::max (worst, std::abs ((double) out[(size_t) i] - out[(size_t) (i - 1)]));
        return worst / medianSlew;
    };

    // Both directions, both interesting band counts, across Mix and block size.
    for (const bool startEnabled : { true, false })
        for (const int bands : { 2, 4 })
            for (const float mix : { 0.05f, 0.25f, 0.5f, 0.75f })
            {
                const double r = ratio (256, bands, mix, startEnabled);
                if (r >= 3.0)
                    std::printf ("  [%s, %d bands, mix %.2f] worst step is %.1fx the signal's own slew\n",
                                 startEnabled ? "disable" : "enable", bands, mix, r);
                check (r < 3.0, "Multiband Enable at partial Mix does not step the dry source");
            }

    // Block size decides WHERE the boundary after the blend falls, so it decides
    // how much of the fade has run when the source used to switch. Pre-fix this
    // row read 88.8 / 74.1 / 23.7 / 72.6 / 47.9.
    for (const int block : { 64, 128, 256, 512, 1024 })
    {
        const double r = ratio (block, 4, 0.25f, true);
        std::printf ("  block %4d: worst step %.2fx the signal's own slew\n", block, r);
        check (r < 3.0, "the dry source does not step at any block size");
    }

    // CONTROL: one band has no crossover, so A(dry) == dry and no step was ever
    // possible there. It must read the same before and after the fix -- if this
    // moved, the change is doing something beyond closing the switch.
    const double one = ratio (256, 1, 0.25f, true);
    std::printf ("  control: one band (A(dry) == dry) reads %.2fx\n", one);
    check (one < 3.0, "the one-band control is unaffected");
}

// ---------------------------------------------------------------------------
//  Regression (R4 / Part 6): an algorithm change that arrives DURING a fade-out
//  must still clear the outgoing algorithm's state at the silent bottom.
//
//  `pendingAlgoReset` is what makes `haas/velvet/chorus.reset()` run at the duck
//  bottom. It was recomputed at all three duck ENTRIES but not on the fourth path
//  into `pendingP` -- a plain retarget while the fade-out is already running,
//  which the FadeIn-only re-arm guard lets fall straight through. So
//      block N   : change the band count  -> duck opens, flag = false
//      block N+1 : change the algorithm   -> pendingP retargeted, flag NOT refreshed
//  adopted the new algorithm without clearing the old one's state.
//
//  WHICH PAIRS THIS CAN BE HEARD ON. Only a change between Chorus and Dimension D.
//  A module is processed only while it IS the selected algorithm (:1281-1282 and
//  the `isModAlgorithm` gate at :859), so every other incoming module starts from
//  silence and a skipped reset costs nothing. Chorus and Dimension D are two
//  VOICES OF ONE ChorusEngine, so there the incoming voice inherits a delay line
//  full of the outgoing voice's audio. Measured through AnamorphAudioProcessor
//  with host-style parameter writes: peak 0.587 (Chorus -> Dimension D, block 64)
//  and 1.519 (Dimension D -> Chorus, block 128) against a 0.7-amplitude source --
//  and 0.000 for Haas -> Velvet / Chorus / Dimension D and for Chorus -> Haas,
//  which is why the pair matters and the others are kept here as controls.
//
//  WHAT THIS ASSERTS. Not the flag: the SAMPLES. The same end state is reached by
//  two routes -- the algorithm moving WITH the band count (which sets the flag at
//  the duck entry) and the algorithm moving one block into the fade-out (which did
//  not) -- and the two streams have to be identical, because a duck exists exactly
//  so that what happens inside it is not heard.
static void testAlgoResetSurvivesMidFadeRetarget()
{
    std::printf ("Test 57: an algorithm change retargeted mid-fade-out still resets the modules\n");
    juce::ScopedNoDenormals noDenormals;
    const double sr = 48000.0;

    // route 0 -- both changes at once (the flag is set at the duck ENTRY)
    // route 1 -- band count first, algorithm `delayBlocks` later (the retarget path)
    //
    // primeParameters() THEN prepare() is the production order (AnamorphEngine.h:87-92,
    // PluginProcessor.cpp:232-233) and it matters here: prepare() is what runs
    // updateDerived(), so priming afterwards would leave the modules configured from
    // defaults and the engine would never reach the state this test is about.
    auto render = [sr] (int block, int route, anamorph::Algorithm from,
                        anamorph::Algorithm to, int delayBlocks)
    {
        anamorph::AnamorphEngine engine;
        anamorph::EngineParameters p;
        p.algorithm = from; p.algoAmount = 0.8f; p.width = 1.4f; p.mix = 1.0f;
        p.mbEnable = true; p.mbBands = 2;
        engine.primeParameters (p);
        engine.prepare (sr, block);
        engine.setParameters (p);

        juce::AudioBuffer<float> buf (2, block);
        std::vector<float> out; out.reserve ((size_t) (200 * block));
        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * 400.0 / sr;
        for (int b = 0; b < 200; ++b)
        {
            if (b == 60) { p.mbBands = 3; if (route == 0) p.algorithm = to; }
            if (route == 1 && b == 60 + delayBlocks) p.algorithm = to;
            engine.setParameters (p);
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s * 0.7f); buf.setSample (1, i, s * 0.55f);
            }
            engine.process (buf);
            for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
        }
        return out;
    };

    const struct { anamorph::Algorithm from, to; const char* name; } pairs[] = {
        { anamorph::Algorithm::Chorus,      anamorph::Algorithm::DimensionD, "Chorus -> Dimension D" },
        { anamorph::Algorithm::DimensionD,  anamorph::Algorithm::Chorus,     "Dimension D -> Chorus" },
        { anamorph::Algorithm::Haas,        anamorph::Algorithm::Velvet,     "Haas -> Velvet (control)" },
        { anamorph::Algorithm::Haas,        anamorph::Algorithm::DimensionD, "Haas -> Dimension D (control)" },
        { anamorph::Algorithm::Chorus,      anamorph::Algorithm::Haas,       "Chorus -> Haas (control)" },
    };

    // The fade-out is ~6 ms (ADR-0004): at block 64 that is 4.5 blocks and at
    // block 128 it is 2.25, so +1 and +2 land inside it at both sizes.
    for (const int block : { 64, 128 })
        for (const auto& pr : pairs)
        {
            const auto entry = render (block, 0, pr.from, pr.to, 0);
            for (const int d : { 1, 2 })
            {
                const auto late = render (block, 1, pr.from, pr.to, d);
                double worst = 0.0;
                for (size_t i = (size_t) (60 * block); i < entry.size(); ++i)
                    worst = std::max (worst, (double) std::abs (entry[i] - late[i]));
                if (worst > 0.0)
                    std::printf ("  [block %d, %s, +%d blk] routes differ by %.4f\n",
                                 block, pr.name, d, worst);
                check (worst == 0.0,
                       "a mid-fade-out algorithm retarget lands exactly as an entry-route one");
            }
        }
    std::printf ("  every pair, both block sizes, both retarget delays: routes identical\n");
}

// ---------------------------------------------------------------------------
//  Regression (R8 / review): an INERT dimMode move must not re-arm Level Match.
//
//  R4 gave `discreteDiffers` a Dimension-D relevance guard on `dimMode` (Test 55
//  above) because `chorus.setDimMode (p.dimMode)` is the field's only reader and it
//  sits inside `else if (p.algorithm == Algorithm::DimensionD)`. `processingDiffers`
//  asks the NARROWER question -- "did the signal path change?" -- and still compared
//  `dimMode` unconditionally. Its one consumer is the silent duck bottom:
//
//      const bool procChanged = processingDiffers (pendingP, p);
//      ...
//      if (procChanged) loudness.softReset();
//
//  THE REPORTED SCENARIO DOES NOT REPRODUCE, and that is worth a control rather than
//  a correction in prose. A plain Dim-D Style move under Haas opens NO duck at all
//  after R4, so this function is never consulted and the measurement is untouched.
//  Leg 1 asserts exactly that, so a future widening of `discreteDiffers` cannot make
//  this test pass for the wrong reason.
//
//  TWO ROUTES DO REACH IT, both measured:
//    * a FORCED duck -- A/B, preset load, undo, all of which call
//      `AnamorphEngine::requestDuck()` (PluginProcessor.cpp) -- ducks whatever
//      differs, so a slot or preset whose only processing delta is dimMode landed on
//      that line.
//    * `autoGainMatch` is the ONE field `discreteDiffers` lists and `processingDiffers`
//      does not, so toggling Level Match opens a duck of its own; an inert dimMode in
//      the same snapshot then made `procChanged` true. That defeats the rule written
//      at the call site -- "Toggling Level Match / Bypass must NOT re-measure, or
//      enabling Match with a big boost slams loud for a moment".
//
//  WHAT THIS ASSERTS, and why it is not a state variable. `softReset()` clears the
//  K-weighting filters and the energy integrators and KEEPS the published gain, and
//  the silence gate is judged FROM those integrators (`meanSq < 1e-6`, tau = 0.4 s).
//  So converge, go silent, make the change, and keep feeding silence:
//      analysis PRESERVED -> stale integrators hold energy -> `silent` reads false ->
//                            MEASURE keeps gliding -> the published gain DRIFTS
//      analysis RE-ARMED  -> integrators at 1e-9 -> `silent` true -> gain FROZEN
//  Measured on this engine: 0.030446 dB of drift when preserved against 0.000454 dB
//  when re-armed (one block of pre-bottom drift, and then nothing) -- a 67x
//  separation, so the 1e-3 threshold sits nowhere near either number.
//
//  THE ATTRIBUTION LEGS ARE THE POINT. Each defect leg is paired with the SAME duck
//  and dimMode held still; both of those preserved before the fix, so the re-arm was
//  attributable to dimMode and to nothing else in the snapshot.
static void testInertDimModeDoesNotReArmLevelMatch()
{
    std::printf ("Test 58: an inert dimMode move does not re-arm Level Match\n");
    juce::ScopedNoDenormals noDenormals;

    constexpr double sr = 48000.0;
    constexpr int    bs = 256;

    auto base = [] (anamorph::Algorithm algo)
    {
        anamorph::EngineParameters p;
        p.algorithm     = algo;
        p.algoAmount    = 0.4f;
        p.haasDelayMs   = 12.0f;
        p.width         = 0.3f;
        p.driveDb       = 8.0f;
        p.mix           = 1.0f;
        p.autoGainMatch = true;          // Level Match engaged
        p.dimMode       = 1;
        return p;
    };

    // Converge on noise, go silent, apply `next` (optionally behind a forced duck),
    // then keep feeding silence and report how far the published gain moved.
    auto moveAfter = [&] (const anamorph::EngineParameters& start,
                          const anamorph::EngineParameters& next, bool forced)
    {
        anamorph::AnamorphEngine engine;
        engine.primeParameters (start);                 // production order (Test 55's note)
        engine.prepare (sr, bs);
        engine.setParameters (start);

        juce::AudioBuffer<float> buf (2, bs);
        juce::Random rng (777);
        for (int b = 0; b < (int) std::ceil (3.0 * sr / bs); ++b)   // 3 s to converge
        {
            for (int i = 0; i < bs; ++i)
            {
                buf.setSample (0, i, rng.nextFloat() * 1.2f - 0.6f);
                buf.setSample (1, i, rng.nextFloat() * 1.2f - 0.6f);
            }
            engine.process (buf);
        }
        auto runSilence = [&] (int blocks, float from)
        {
            float worst = 0.0f;
            for (int b = 0; b < blocks; ++b)
            {
                buf.clear();
                engine.process (buf);
                worst = juce::jmax (worst, std::abs (engine.getMatchGainDb() - from));
            }
            return worst;
        };
        runSilence (8, engine.getMatchGainDb());        // the transport stops
        const float at = engine.getMatchGainDb();

        if (forced) engine.requestDuck();               // A/B, preset recall, undo
        engine.setParameters (next);
        return runSilence ((int) std::ceil (1.4 * sr / bs), at);
    };

    auto leg = [&] (const char* what, float move, bool wantPreserved)
    {
        const bool preserved = move > 1.0e-3f;
        std::printf ("  %-50s moved %.6f dB -> %s\n", what, move,
                     preserved ? "analysis PRESERVED" : "analysis RE-ARMED");
        check (preserved == wantPreserved, what);
    };

    const auto haas = base (anamorph::Algorithm::Haas);
    const auto dimD = base (anamorph::Algorithm::DimensionD);

    // --- The baseline this test reads everything against: no change at all.
    leg ("R8 control: no change -- the ordinary silent drift", moveAfter (haas, haas, false), true);

    // --- Leg 1: the reported scenario. It opens no duck at all after R4, so nothing
    //     here is consulted -- asserted so it cannot start passing for another reason.
    {
        auto p = haas; p.dimMode = 3;
        leg ("R8: a plain Dim-D Style move under Haas", moveAfter (haas, p, false), true);
    }

    // --- Leg 2: the forced-duck route (A/B / preset / undo).
    {
        auto p = haas; p.dimMode = 3;
        leg ("R8: dimMode alone through a FORCED duck", moveAfter (haas, p, true), true);
    }
    {   // its attribution control: the same forced duck, dimMode held still.
        leg ("R8 control: a forced duck that changes nothing",
             moveAfter (haas, haas, true), true);
    }

    // --- Leg 3: the Level-Match-toggle route.
    {
        auto p = haas; p.dimMode = 3; p.autoGainMatch = false;
        leg ("R8: dimMode + Level Match toggled, one snapshot", moveAfter (haas, p, false), true);
    }
    {   // its attribution control: the same toggle, dimMode held still.
        auto p = haas; p.autoGainMatch = false;
        leg ("R8 control: Level Match toggled alone", moveAfter (haas, p, false), true);
    }

    // --- AND THE RE-ARM MUST STILL HAPPEN WHEREVER THE PATH REALLY MOVED. These are
    //     the legs a fix that simply deleted dimMode from the list would also pass, so
    //     they are what pins the GUARD rather than the removal.
    {
        auto p = dimD; p.dimMode = 3;                     // audible: Dimension D is live
        leg ("R8: dimMode WHILE Dimension D is active", moveAfter (dimD, p, false), false);
    }
    {
        auto p = haas; p.algorithm = anamorph::Algorithm::DimensionD;
        leg ("R8: switching TO Dimension D", moveAfter (haas, p, false), false);
    }
    {
        auto p = haas; p.haasSide = anamorph::HaasSide::Right;
        leg ("R8: haasSide (reaches a module unconditionally)", moveAfter (haas, p, false), false);
    }
    {
        auto p = haas; p.dimMode = 3; p.haasSide = anamorph::HaasSide::Right;
        leg ("R8: dimMode riding a real discrete change", moveAfter (haas, p, false), false);
    }
}

// ---------------------------------------------------------------------------
//  Test 59 -- A NON-FINITE BURST FROM THE HOST SELF-HEALS (ADR-0009; F14, R7)
//
//  ADR-0009's second decision bullet -- "an engine-wide per-sample NaN/Inf guard
//  replaces only non-finite samples with 0 and resets the stateful nodes" -- and its
//  stated consequence, "the plugin self-heals instead of needing a Multiband off/on",
//  were entered by no test: measured under gcov, every line of the guard's scrub and
//  reset block ran ZERO times across this suite. Test 2 asserts the chain PRODUCES no
//  NaN; Tests 19 and 45 feed NaN to the meters, never to `AnamorphEngine::process`.
//  Nothing upstream of the guard scrubs host input, so a host NaN reaches every
//  stateful node in the chain.
//
//  WHAT RECOVERY MEANS HERE, and nothing ADR-0009 does not say: the host never receives
//  a non-finite sample; the chain is not left latched (a NaN held in an IIR state makes
//  every later block non-finite, which the guard then zeroes -- silence for good); and
//  the published Level-Match gain is not left non-finite. Twin engines on the heap
//  (sizeof ~138 KB each): A receives one block with NaN / +Inf on every 7th sample, B the
//  same block with those samples at 0.
//
//  THE COMPARISON IS PER-BLOCK RMS, NOT SAMPLE-EXACT, because the guard's own reset is a
//  state change the twin never makes. Chorus and Dimension D restart their LFO phase and
//  then differ from the twin by up to 1.8 dB for as long as the modulation runs; with
//  Level Match on, A re-converges from a cleared matcher. Haas and Velvet carry no such
//  phase, and with Level Match off they return to the twin to within 0.1 dB -- the
//  stronger claim, made only where it holds.
//
//  MEASURED, 4 algorithms x Oversampling Off / 2x, Multiband and Mono Maker on, Level
//  Match off and on: no non-finite output block anywhere; from 10 blocks after the burst
//  the worst per-block distance from the twin is 2.83 dB (Dim-D, Level Match on) against a
//  6 dB bound -- a latched chain reads -180 dB, so the bound sits ~170 dB from the failure
//  and ~3 dB from the widest healthy leg; Haas / Velvet within 0.1 dB after 4-13 blocks.
//  AND WHAT EACH ASSERTION CATCHES, measured by deleting pieces of the guard:
//      every module reset removed      -> silent (-180 dB) for good, gain NaN
//      only `loudness.reset()` removed -> gain NaN for good; with Level Match on, silence
//      the scrub removed               -> every block after the burst non-finite
//  The first is exactly the latched channel ADR-0009 was written against.
//
//  NOT CLAIMED: extreme FINITE input. ADR-0009 passes valid audio "however loud"
//  untouched and the guard does not fire; the chain itself recovers within 0.1 s, but the
//  Level-Match analysis stays displaced for seconds (worklog R7, §F14). That belongs to
//  Level Match, not to this guard.
static void testNonFiniteBurstSelfHeals()
{
    std::printf ("Test 59: a non-finite burst from the host self-heals (ADR-0009, F14)\n");
    juce::ScopedNoDenormals noDenormals;

    constexpr double sr = 48000.0;
    constexpr int    bs = 256;
    constexpr int    before = 60, after = 120;   // ~0.32 s in, one burst block, ~0.64 s out

    auto rmsDb = [] (const juce::AudioBuffer<float>& b)
    {
        double s = 0.0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double v = b.getSample (c, i);
                s += v * v;
            }
        const double r = std::sqrt (s / (2.0 * b.getNumSamples()));
        return r > 1.0e-9 ? 20.0 * std::log10 (r) : -180.0;
    };
    auto allFinite = [] (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (c, i))) return false;
        return true;
    };

    using anamorph::Algorithm;
    using anamorph::OversampleFactor;
    const Algorithm algos[] = { Algorithm::Haas, Algorithm::Velvet, Algorithm::Chorus, Algorithm::DimensionD };
    const char* names[]     = { "Haas", "Velvet", "Chorus", "Dim-D" };

    bool outFinite = true, gainFinite = true, tracks = true, returns = true;
    for (int lm = 0; lm <= 1; ++lm)
        for (int a = 0; a < 4; ++a)
            for (auto os : { OversampleFactor::Off, OversampleFactor::x2 })
            {
                anamorph::EngineParameters p;
                p.algorithm = algos[a]; p.algoAmount = 0.7f; p.oversample = os;
                p.driveDb = 8.0f; p.width = 1.6f; p.mix = 0.8f;
                p.mbEnable = true; p.monoMakerEnable = true;
                p.autoGainMatch = (lm == 1);

                auto aPtr = std::make_unique<anamorph::AnamorphEngine>();   // heap: see the note above
                auto bPtr = std::make_unique<anamorph::AnamorphEngine>();
                for (auto* e : { aPtr.get(), bPtr.get() })
                {
                    e->primeParameters (p);                     // production order (Test 55's note)
                    e->prepare (sr, bs);
                    e->setParameters (p);
                }

                juce::AudioBuffer<float> A (2, bs), B (2, bs);
                juce::Random rng (4242);
                bool legFinite = true, legGain = true;
                double worst = 0.0;                  // max |dB| vs the twin, from burst + 10 on
                int settled = -1;                    // first block after which A stays within 0.1 dB
                for (int b = 0; b <= before + after; ++b)
                {
                    for (int i = 0; i < bs; ++i)
                    {
                        const float l = rng.nextFloat() - 0.5f, r = rng.nextFloat() - 0.5f;
                        const bool bad = (b == before && (i % 7) == 3);
                        A.setSample (0, i, bad ? std::numeric_limits<float>::quiet_NaN() : l);
                        A.setSample (1, i, bad ? std::numeric_limits<float>::infinity()  : r);
                        B.setSample (0, i, bad ? 0.0f : l);
                        B.setSample (1, i, bad ? 0.0f : r);
                    }
                    aPtr->process (A);
                    bPtr->process (B);

                    legFinite = legFinite && allFinite (A);
                    legGain   = legGain && std::isfinite (aPtr->getMatchGainDb());
                    if (b > before)
                    {
                        const double d = std::abs (rmsDb (A) - rmsDb (B));
                        if (b >= before + 10) worst = juce::jmax (worst, d);
                        if (d < 0.1) { if (settled < 0) settled = b - before; }
                        else settled = -1;
                    }
                }

                // Haas / Velvet have no LFO phase to lose; with Level Match off they must
                // come back to the twin, not merely near it.
                const bool strict = (lm == 0) && (a == 0 || a == 1);
                const bool legReturns = ! strict || (settled >= 0 && settled <= 40);
                char note[48] = "";
                if (strict)
                    std::snprintf (note, sizeof note, settled >= 0 ? " (within 0.1 dB from block +%d)"
                                                                   : " (never within 0.1 dB)", settled);
                std::printf ("  %-6s OS %s  Level Match %-3s: output finite %d, gain finite %d, "
                             "worst |dB| vs twin %5.2f%s\n",
                             names[a], os == OversampleFactor::Off ? "off" : "2x ", lm ? "on" : "off",
                             (int) legFinite, (int) legGain, worst, note);
                outFinite  = outFinite  && legFinite;
                gainFinite = gainFinite && legGain;
                tracks     = tracks     && worst < 6.0;
                returns    = returns    && legReturns;
            }

    check (outFinite,  "F14: the host never receives a non-finite sample, burst block included");
    check (gainFinite, "F14: the published Level-Match gain is never left non-finite");
    check (tracks,     "F14: the chain is not left latched -- within 6 dB of its twin from 10 blocks on");
    check (returns,    "F14: Haas and Velvet return to the twin within 0.1 dB (Level Match off)");
}

// ---------------------------------------------------------------------------
//  Test 60 -- THE ENGAGED WRAP CARRIES EXACTLY THE LATENCY THE HOST IS TOLD (PDC; R7, F15)
//
//  The oversamplers are built with JUCE's integer-latency flag "so PDC is exact"
//  (LATENCY_MODEL.md), and no test measured the PROCESSED path with the wrap running.
//  Test 3+4 and Test 52 measure through the bypass ring and the skipped-wrap stand-in
//  ring, and both of those are delayed BY the reported number -- they check a ring
//  against the number, never the oversampler against it. Measured by building the three
//  oversamplers without the flag (JUCE's default for that argument): the reported latency
//  moved from 4 / 6 / 6 to 3 / 4 / 5, the engaged wrap sat 0.137 / 0.433 / 0.049 samples
//  off the number it reported, and both suites passed. A reported-latency change is a
//  hard-stop class (CLAUDE.md); a wet path off its reported delay combs against the dry
//  path the engine aligns by that number (Mix) and against every other track.
//
//  WHAT THIS ASSERTS, 2x / 4x / 8x at 44.1, 48 and 96 kHz:
//    * the reported latency is 4 / 6 / 6 samples at every rate -- LATENCY_MODEL.md's
//      current values. Changing them is the hard-stop above; this is the check that notices;
//    * with the wrap RUNNING (Drive 6 dB), the chain's phase delay at 300 Hz equals the
//      reported latency within 0.01 samples. Measured within 3e-4 on this build; the
//      mutant above misses by 0.049 at best, five times the bound;
//    * CONTROL, so that cannot pass through the stand-in ring: at 0.35 fs the engaged
//      chain's delay differs from the skipped chain's (Drive 0, the integer ring) by more
//      than 0.1 samples -- the half-band IIR's own phase. Measured 0.88 / 0.44 / 0.84.
//      It is also the only check in either suite that Drive engages the wrap at all:
//      with the predicate broken so Drive never does (the shaper then runs without
//      oversampling), both suites passed and this control alone failed.
//  Phase delay by single-bin DFT over whole periods of the second half of 200 blocks,
//  input at -60 dBFS so the Drive shaper is linear. Engines on the heap (~138 KB each).
static void testEngagedWrapCarriesTheReportedLatency()
{
    std::printf ("Test 60: the engaged oversampling wrap carries exactly the reported latency (PDC)\n");
    juce::ScopedNoDenormals noDenormals;

    constexpr int bs = 256, blocks = 200;
    using anamorph::OversampleFactor;

    // Delay in samples of the chain at `cycles / period` of the sample rate, over whole
    // periods (`period` samples hold exactly `cycles` cycles), wrapped to [0, period / cycles).
    auto delayAt = [] (OversampleFactor f, float driveDb, double sr, int cycles, int period, int& reported)
    {
        anamorph::EngineParameters p;
        p.oversample = f;
        p.driveDb    = driveDb;
        const auto e = std::make_unique<anamorph::AnamorphEngine>();
        e->primeParameters (p);
        e->prepare (sr, bs);
        e->setParameters (p);
        reported = e->getLatencySamples();

        const double w = 2.0 * juce::MathConstants<double>::pi * cycles / period;
        const int from = blocks * bs / 2;
        const int len  = (blocks * bs - from) / period * period;
        double inRe = 0.0, inIm = 0.0, outRe = 0.0, outIm = 0.0;
        juce::AudioBuffer<float> buf (2, bs);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < bs; ++i)
            {
                const float v = (float) (1.0e-3 * std::sin (w * (b * bs + i)));
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            const juce::AudioBuffer<float> in (buf);
            e->setParameters (p);
            e->process (buf);
            for (int i = 0; i < bs; ++i)
                if (const int n = b * bs + i; n >= from && n < from + len)
                {
                    const double c = std::cos (w * n), s = std::sin (w * n);
                    inRe  += in.getSample (0, i) * c;  inIm  -= in.getSample (0, i) * s;
                    outRe += buf.getSample (0, i) * c; outIm -= buf.getSample (0, i) * s;
                }
        }
        const double twoPi = 2.0 * juce::MathConstants<double>::pi;
        const double d = std::atan2 (inIm, inRe) - std::atan2 (outIm, outRe);
        return (d - twoPi * std::floor (d / twoPi)) / w;
    };

    const OversampleFactor fs[3] = { OversampleFactor::x2, OversampleFactor::x4, OversampleFactor::x8 };
    const int expected[3] = { 4, 6, 6 };
    bool pinned = true, exact = true, engaged = true;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int o = 0; o < 3; ++o)
        {
            int rep = 0, repSkipped = 0;
            const int lfPeriod = (int) (sr / 300.0);                       // 147 / 160 / 320: whole
            const double lf = delayAt (fs[o], 6.0f, sr, 1, lfPeriod, rep);
            const double hfOn  = delayAt (fs[o], 6.0f, sr, 7, 20, rep);     // 0.35 fs
            const double hfOff = delayAt (fs[o], 0.0f, sr, 7, 20, repSkipped);
            const double hfPeriod = 20.0 / 7.0;
            double sig = hfOn - hfOff;
            sig -= hfPeriod * std::round (sig / hfPeriod);
            std::printf ("  %5.1f kHz %dx: reported %d (skipped %d) | 300 Hz delay %.4f | 0.35 fs vs skipped %+.3f\n",
                         sr / 1000.0, 2 << o, rep, repSkipped, lf, sig);
            pinned  = pinned  && rep == expected[o] && repSkipped == expected[o];
            exact   = exact   && std::abs (lf - rep) < 0.01;
            engaged = engaged && std::abs (sig) > 0.1;
        }

    check (engaged, "R7 PDC control: Drive 6 dB really runs the wrap (its IIR phase is measurable at 0.35 fs)");
    check (pinned,  "R7 PDC: the reported latency is 4 / 6 / 6 samples at 2x / 4x / 8x at every rate");
    check (exact,   "R7 PDC: the engaged wrap's delay equals the reported latency within 0.01 samples");
}

// ---------------------------------------------------------------------------
//  Test 61 -- THE SCOPE RING HANDS THE GUI THE NEWEST FRAMES, OLDEST FIRST (R7, F15)
//
//  `ScopeBuffer::readLatest (dst, n)` is the only way audio reaches the Vectorscope and
//  the SpectrumImager, and both freshness scans depend on its exact contract: they ask
//  for the `fresh` newest frames or for the 8192-frame FFT window, and read what comes
//  back as the newest frames in time order (`SpectrumImager::pushFFT` scans the window's
//  last `freshN` frames for its silence tracker). Measured under gcov, `readLatest` ran
//  ZERO times across both suites, and so did `pushBlock`'s second segment -- the copy for
//  a block that straddles the ring's end -- because every block size the suites use
//  divides the 16384-frame capacity. A host at 441 or 480 frames per block (10 ms at
//  44.1 / 48 kHz), or one with variable blocks, straddles it every few dozen blocks.
//
//  WHAT THIS ASSERTS, single-threaded: across 2.7 laps of 441-frame blocks of a ramp
//  (frame k carries L = k, R = -k, exact in float), after every block `readLatest` for 1,
//  441, 8192 and 16384 frames returns min(n, written, capacity) frames, and they are the
//  newest ones, oldest first, with `writeCount` equal to the frames written; a request
//  beyond the capacity is clamped to it; one block larger than the ring keeps only its
//  newest `capacity` frames.
//
//  NOT ASSERTED: the cross-thread half. The index is published by one release-store per
//  block and acquired by the reader, so a reader never copies a frame above the index it
//  read; no deterministic test can observe that ordering. A concurrent test would not be
//  a sound substitute: once the writer laps a reader, its overwrite of frames the reader
//  has copied has no happens-before edge back to the writer (worklog R7, §F15-ScopeBuffer).
static void testScopeRingHandsTheNewestFramesOldestFirst()
{
    std::printf ("Test 61: the scope ring hands the GUI the newest frames, oldest first\n");

    using anamorph::ScopeBuffer;
    constexpr int cap = ScopeBuffer::capacity;
    const auto ring = std::make_unique<ScopeBuffer>();        // 128 KB of frames: heap, not stack
    std::vector<float> srcL (2 * (size_t) cap), srcR (2 * (size_t) cap);
    std::vector<float> dstL ((size_t) cap), dstR ((size_t) cap);
    std::uint64_t written = 0;

    auto fill = [&] (int n)
    {
        for (int i = 0; i < n; ++i)
        {
            srcL[(size_t) i] = (float) (written + (std::uint64_t) i);
            srcR[(size_t) i] = -srcL[(size_t) i];
        }
    };
    // dst[0, got) must be ramp frames [written - got, written), oldest first.
    auto newest = [&] (int got)
    {
        for (int i = 0; i < got; ++i)
        {
            const float k = (float) (written - (std::uint64_t) got + (std::uint64_t) i);
            if (! juce::exactlyEqual (dstL[(size_t) i], k) || ! juce::exactlyEqual (dstR[(size_t) i], -k))
                return false;
        }
        return true;
    };

    check (ring->readLatest (dstL.data(), dstR.data(), 8192) == 0 && ring->writeCount() == 0,
           "R7 scope: an empty ring returns nothing");

    constexpr int block = 441;
    const int blocks = (int) (2.7 * cap / block);
    int straddles = 0;
    bool counts = true, frames = true;
    for (int b = 0; b < blocks; ++b)
    {
        fill (block);
        if ((int) (written & (std::uint64_t) ScopeBuffer::mask) + block > cap)
            ++straddles;
        ring->pushBlock (srcL.data(), srcR.data(), block);
        written += (std::uint64_t) block;
        counts = counts && ring->writeCount() == written;
        for (int n : { 1, block, 8192, cap })
        {
            const int got = ring->readLatest (dstL.data(), dstR.data(), n);
            counts = counts && (std::uint64_t) got == std::min<std::uint64_t> ({ (std::uint64_t) n, written,
                                                                               (std::uint64_t) cap });
            frames = frames && newest (got);
        }
    }
    std::printf ("  %d blocks of %d frames, %d of them straddling the ring's end: counts %s, frames %s\n",
                 blocks, block, straddles, counts ? "exact" : "WRONG", frames ? "newest, in order" : "WRONG");
    check (straddles > 0, "R7 scope control: blocks really straddled the ring's end (the second segment ran)");
    check (counts, "R7 scope: readLatest returns min(n, written, capacity) frames; writeCount the frames written");
    check (frames, "R7 scope: ...and they are exactly the newest frames, oldest first");

    const int clamped = ring->readLatest (dstL.data(), dstR.data(), cap + 5);
    check (clamped == cap && newest (clamped), "R7 scope: a request beyond the capacity is clamped to it");

    const int big = cap + 1000;                                // a pathological host block
    fill (big);
    ring->pushBlock (srcL.data(), srcR.data(), big);
    written += (std::uint64_t) big;
    const int got = ring->readLatest (dstL.data(), dstR.data(), cap);
    check (ring->writeCount() == written && got == cap && newest (got),
           "R7 scope: a block larger than the ring keeps only its newest frames");
}

// ---------------------------------------------------------------------------
//  Test 62 -- THE HOST RESET'S CHORUS RE-SEED: AFTER THE FLUSH, FINITE, MODULATION ONLY
//  (Devin review of PR #155, "Chorus fades in after host reset"; State test 126 is the
//  user-facing half, through the processor)
//
//  `AnamorphEngine::reset (audioTailsOnly)` ends by re-seeding the Chorus / Dimension-D wet
//  and depth glides (`chorus.snapToTargets()`), as prepare() does. State test 126 proves the
//  defect and the fix. This pins the three decisions behind that line, each of which an
//  innocent-looking edit could undo without failing anything else:
//    C1. it runs AFTER the in-flight duck flush. A forced swap (A/B, preset, undo) holds the
//        new Amount in pendingP until `p = pendingP`, so a host reset landing inside the
//        fade-out must start at the NEW Amount -- bit-identical to a fresh engine at it;
//    C2. an ordinary duck in flight (Haas -> Dimension-D) is adopted and starts at the new
//        algorithm's configured sound, which also pins that the algorithm test reads the
//        FLUSHED snapshot;
//    G.  a NaN Amount pending at the reset is not seeded: when the host sends a finite value
//        again, no block after the reset is zeroed by the self-heal (ADR-0009, R7);
//    H.  outside Chorus / Dimension-D the idle chorus is left alone: a Haas session that is
//        host-reset and then switched to Chorus sounds exactly like the same session with no
//        reset.
//  Mutants and their failures: worklog R6_HOST_RESET_SCOPE_AND_STATE_COVERAGE.md §U.
static void testHostResetChorusSeedIsScoped()
{
    std::printf ("Test 62: the host reset's chorus re-seed -- after the flush, finite, modulation only\n");
    juce::ScopedNoDenormals noDenormals;

    constexpr double sr = 48000.0;
    constexpr int    bs = 256;
    using anamorph::Algorithm;
    using Scope = anamorph::AnamorphEngine::ResetScope;

    auto activate = [] (const anamorph::EngineParameters& p)
    {
        auto e = std::make_unique<anamorph::AnamorphEngine>();   // heap: ~138 KB
        e->primeParameters (p);                                 // the wrapper's order
        e->prepare (sr, bs);
        e->setParameters (p);
        return e;
    };
    // `blocks` blocks of `n` samples of seeded noise (silence when `quiet`); outputs appended.
    auto run = [] (anamorph::AnamorphEngine& e, const anamorph::EngineParameters& p, int blocks, int n,
                   juce::Random& rng, std::vector<float>* out, bool quiet = false)
    {
        juce::AudioBuffer<float> buf (2, n);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < n; ++i)
            {
                const float v = quiet ? 0.0f : rng.nextFloat() - 0.5f;
                buf.setSample (0, i, v);
                buf.setSample (1, i, quiet ? 0.0f : 0.6f * v + 0.2f * (rng.nextFloat() - 0.5f));
            }
            e.setParameters (p);
            e.process (buf);
            if (out != nullptr)
                for (int i = 0; i < n; ++i)
                {
                    out->push_back (buf.getSample (0, i));
                    out->push_back (buf.getSample (1, i));
                }
        }
    };
    auto same = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (! juce::exactlyEqual (a[i], b[i])) return false;
        return true;
    };
    const int half = (int) (0.5 * sr / bs), probe = (int) (0.15 * sr / bs);

    // --- C1 / C2: a host reset landing inside a duck -------------------------------
    // C1 changes ONLY the Amount, so it pins the seed's placement and nothing else;
    // Test 63 pins how the rest of a forced swap lands at a host reset.
    struct Swap { const char* name = ""; bool forced = false; anamorph::EngineParameters from, to; };
    Swap swaps[2];
    swaps[0].name = "C1 forced swap, Chorus 0.3 -> 0.9"; swaps[0].forced = true;
    swaps[0].from.algorithm = Algorithm::Chorus; swaps[0].from.algoAmount = 0.3f;
    swaps[0].to = swaps[0].from; swaps[0].to.algoAmount = 0.9f;
    swaps[1].name = "C2 ordinary duck, Haas -> Dimension-D"; swaps[1].forced = false;
    swaps[1].from.algorithm = Algorithm::Haas; swaps[1].from.algoAmount = 0.7f;
    swaps[1].to = swaps[1].from; swaps[1].to.algorithm = Algorithm::DimensionD; swaps[1].to.dimMode = 3;
    for (const auto& s : swaps)
    {
        const auto e = activate (s.from);
        const auto twin = activate (s.to);
        juce::Random rng (11);
        run (*e, s.from, half, bs, rng, nullptr);
        if (s.forced) e->requestDuck();
        run (*e, s.to, 1, 64, rng, nullptr);                    // 1.3 ms into the fade-out
        e->reset (Scope::audioTailsOnly);                       // the host's request lands here
        std::vector<float> a, b;
        juce::Random r1 (99), r2 (99);
        run (*e, s.to, probe, bs, r1, &a);
        run (*twin, s.to, probe, bs, r2, &b);
        const bool ok = same (a, b);
        std::printf ("  %-38s: after the host reset %s a fresh engine at the new settings\n",
                     s.name, ok ? "bit-identical to" : "DIFFERENT from");
        check (ok, s.forced ? "host reset in a forced swap starts at the NEW Amount (the seed runs after the flush)"
                            : "host reset in a duck starts at the new algorithm's configured sound");
    }

    // --- G: a NaN Amount pending at the reset is not seeded -------------------------
    {
        anamorph::EngineParameters p; p.algorithm = Algorithm::Chorus; p.algoAmount = 0.7f;
        auto nanP = p; nanP.algoAmount = std::numeric_limits<float>::quiet_NaN();
        const auto e = activate (p);
        juce::Random rng (5);
        run (*e, p, half, bs, rng, nullptr);
        run (*e, nanP, 1, bs, rng, nullptr);                    // the host's bad point (self-heals, R7)
        e->reset (Scope::audioTailsOnly);                       // ...and a stop while it is still pending
        std::vector<float> out;
        run (*e, p, 40, bs, rng, &out);                         // the host is finite again
        int zeroedBlocks = 0; bool finite = true;
        for (int b = 0; b < 40; ++b)
        {
            bool allZero = true;
            for (int i = 0; i < 2 * bs; ++i)
            {
                const float v = out[(size_t) (b * 2 * bs + i)];
                finite = finite && std::isfinite (v);
                allZero = allZero && juce::exactlyEqual (v, 0.0f);
            }
            if (allZero) ++zeroedBlocks;
        }
        std::printf ("  G  NaN Amount pending at the reset: %d self-healed (all-zero) block(s) after it, output finite %d\n",
                     zeroedBlocks, (int) finite);
        check (finite && zeroedBlocks == 0,
               "a NaN Amount pending at a host reset is not seeded -- no self-healed block when the host recovers");
    }

    // --- H: outside Chorus / Dimension-D the idle chorus is left alone -------------
    {
        anamorph::EngineParameters chorusP; chorusP.algorithm = Algorithm::Chorus; chorusP.algoAmount = 0.7f;
        auto haasP = chorusP; haasP.algorithm = Algorithm::Haas;
        std::vector<float> outs[2];
        for (int withReset = 0; withReset < 2; ++withReset)
        {
            const auto e = activate (chorusP);
            juce::Random rng (21);
            run (*e, chorusP, half, bs, rng, nullptr);
            run (*e, haasP, half, bs, rng, nullptr);            // switch to Haas (duck, then Haas)
            run (*e, haasP, 20, bs, rng, nullptr, true);        // ~107 ms of silence: every tail gone
            if (withReset == 1) e->reset (Scope::audioTailsOnly);
            run (*e, chorusP, half, bs, rng, &outs[withReset]); // back to Chorus
        }
        const bool ok = same (outs[0], outs[1]);
        std::printf ("  H  Haas session host-reset, then switched to Chorus: %s the same session without the reset\n",
                     ok ? "bit-identical to" : "DIFFERENT from");
        check (ok, "a host reset outside Chorus / Dimension-D leaves the idle chorus alone");
    }
}

// ---------------------------------------------------------------------------
//  Test 63 -- A HOST RESET INSIDE A FORCED SWAP LANDS WHERE THE SWAP'S OWN BOTTOM WOULD
//  (PR #155; State test 127 is the same contract through the processor's four routes)
//
//  A forced swap -- A/B, preset load, undo, redo (`requestDuck`) -- keeps the OLD state live
//  through its ~6 ms fade-out and applies the new one at the silent bottom: adopted, smoothers
//  SNAPPED, every node cleared (ADR-0004, decision 1). A host reset (`audioTailsOnly`) landing
//  before that bottom used to adopt the target AFTER the node resets and without the snap, so
//  Mix / Width / Output / balance (20 ms), Drive (32 ms) and polarity (5 ms) glided in, and
//  the Haas delay and the multiband crossovers and widths -- which their own reset() snaps --
//  were snapped to the OLD values and glided from there, the delay stalling short for good.
//
//  ORACLE: a fresh engine at the target. Completing the swap and clearing every tail IS a
//  clean start (leg A2 proves that on the pre-fix engine too), for every field the bottom
//  lands. The fields its bottom leaves gliding -- the Haas / Velvet amount, Velvet density and
//  Mono Maker cutoff -- are deliberately not moved by any leg here (worklog §V).
//    L1  engine smoothers, both directions, reset 1 / 64 / 289 samples into the fade-out
//        (289 = the fade has reached silence and the bottom has not run yet);
//    L2  Haas delay, both directions;  L3  multiband crossover, band width and Band Solo;
//    L4  Oversampling Off -> 2x with Drive (a latency-changing swap: no dry fill);
//    L5  into Chorus / Dimension-D with the smoothers, so the Chorus re-seed (Test 62) and
//        this landing compose;
//    L6  the other three ways into a forced fade-out: an ordinary duck upgraded to forced,
//        a forced re-arm from the fade-in, a forced swap retargeted mid fade-out.
//  And what must NOT change (each passes on the pre-fix engine as well):
//    A2  a forced swap that completed before the reset lands on a clean start;
//    A3  a reset in the fade-in lands on a clean start (the bottom has run);
//    A4  a live edit's glide that no reset() lands keeps gliding: the engine's smoothers, the
//        Haas amount, Velvet density and Mono Maker cutoff (the Haas delay, crossovers and the
//        Chorus are landed by their own reset() in any case, duck or none);
//    A5  an ordinary duck's live riders keep gliding the same way;
//    A7  a duck request the engine has not consumed yet commutes with the reset.
//  A4 / A5's oracle is the same control sequence on SILENT history with no reset: its delay
//  lines hold only zeros, which is what the reset leaves, so only control state can differ.
//  Mutants and their failures: worklog R6_HOST_RESET_SCOPE_AND_STATE_COVERAGE.md §V.
static void testHostResetInAForcedSwapLandsSettled()
{
    std::printf ("Test 63: a host reset inside a forced swap lands where the swap's bottom would\n");
    juce::ScopedNoDenormals noDenormals;

    constexpr double sr = 48000.0;
    constexpr int    bs = 256;
    using anamorph::Algorithm;
    using Params = anamorph::EngineParameters;
    using Engine = anamorph::AnamorphEngine;
    constexpr auto host = Engine::ResetScope::audioTailsOnly;
    const int history = (int) (0.5 * sr / bs), window = (int) (1.0 * sr / bs) + 1;

    auto activate = [] (const Params& p)
    {
        auto e = std::make_unique<Engine>();                    // heap: ~138 KB
        e->primeParameters (p);                                 // the wrapper's order
        e->prepare (sr, bs);
        e->setParameters (p);
        return e;
    };
    // `blocks` blocks of `n` samples of noise from `rng` (silence when `quiet`); outputs appended.
    auto run = [] (Engine& e, const Params& p, int blocks, int n, juce::Random& rng,
                   std::vector<float>* out, bool quiet = false)
    {
        juce::AudioBuffer<float> buf (2, n);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < n; ++i)
            {
                const float v = quiet ? 0.0f : rng.nextFloat() - 0.5f;
                buf.setSample (0, i, v);
                buf.setSample (1, i, quiet ? 0.0f : 0.6f * v + 0.2f * (rng.nextFloat() - 0.5f));
            }
            e.setParameters (p);
            e.process (buf);
            if (out != nullptr)
                for (int i = 0; i < n; ++i)
                {
                    out->push_back (buf.getSample (0, i));
                    out->push_back (buf.getSample (1, i));
                }
        }
    };
    // Bit-exact comparison; prints the worst difference and where the last one sits.
    auto compare = [] (const char* name, const std::vector<float>& a, const std::vector<float>& b)
    {
        double worst = 0.0; long last = -1;
        for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
            if (! juce::exactlyEqual (a[i], b[i]))
            {
                worst = juce::jmax (worst, (double) std::abs (a[i] - b[i]));
                last = (long) (i / 2);
            }
        const bool ok = a.size() == b.size() && last < 0;
        if (ok) std::printf ("  %-58s: bit-identical\n", name);
        else    std::printf ("  %-58s: max|d| %.4f, last difference %.2f ms after the reset\n",
                             name, worst, (double) last * 1000.0 / sr);
        return ok;
    };
    // The clean-start oracle: a fresh engine at `to`, fed the same input from the reset on.
    auto fresh = [&] (const Params& to)
    {
        std::vector<float> out;
        const auto f = activate (to);
        juce::Random rng (99);
        run (*f, to, window, bs, rng, &out);
        return out;
    };
    // Settle at `from`, run `drive` (which ends inside a forced fade-out), host reset, then
    // one second at `to`.
    auto afterReset = [&] (const Params& from, const Params& to, const std::function<void (Engine&, juce::Random&)>& drive)
    {
        const auto e = activate (from);
        juce::Random pre (3), mid (4), post (99);
        run (*e, from, history, bs, pre, nullptr);
        drive (*e, mid);
        e->reset (host);
        std::vector<float> out;
        run (*e, to, window, bs, post, &out);
        return out;
    };
    auto forcedAt = [&] (const Params& to, int samples)
    {
        return [&run, to, samples] (Engine& e, juce::Random& rng) { e.requestDuck(); run (e, to, 1, samples, rng, nullptr); };
    };
    auto with = [] (Params p, const std::function<void (Params&)>& edit) { edit (p); return p; };

    Params haas; haas.algorithm = Algorithm::Haas; haas.algoAmount = 0.3f;
    const auto smooth = with (haas, [] (Params& p) { p.mix = 0.5f; p.width = 1.8f; p.outputGainDb = -6.0f;
                                                     p.inputBalance = 0.5f; p.outputBalance = -0.5f;
                                                     p.driveDb = 6.0f; p.polarityL = true; });
    char name[96];

    // --- L1: the engine's smoothers, both directions, early and late in the fade-out ---
    for (int dir = 0; dir < 2; ++dir)
        for (const int k : { 1, 64, 289 })
        {
            const auto& from = dir == 0 ? haas : smooth;
            const auto& to   = dir == 0 ? smooth : haas;
            std::snprintf (name, sizeof name, "L1 Mix/Width/Output/balances/Drive/polarity %s, reset @%d",
                           dir == 0 ? "on " : "off", k);
            check (compare (name, afterReset (from, to, forcedAt (to, k)), fresh (to)),
                   "a host reset inside a forced swap lands the engine smoothers snapped, as the bottom does");
        }

    // --- L2: the Haas delay, which haas.reset() snaps -- to whatever target it holds -------
    for (const auto& [dFrom, dTo] : { std::pair<float, float> { 12.0f, 30.0f }, std::pair<float, float> { 20.0f, 9.0f } })
    {
        const auto from = with (haas, [d = dFrom] (Params& p) { p.haasDelayMs = d; });
        const auto to   = with (haas, [d = dTo]   (Params& p) { p.haasDelayMs = d; });
        std::snprintf (name, sizeof name, "L2 Haas delay %.0f -> %.0f ms, reset @64", dFrom, dTo);
        check (compare (name, afterReset (from, to, forcedAt (to, 64)), fresh (to)),
               "a host reset inside a forced swap snaps the Haas delay to the NEW target");
    }

    // --- L3: multiband crossover, band width, Band Solo -- snapped by their own reset() ---
    {
        const auto from = with (haas, [] (Params& p) { p.mbEnable = true; });
        const auto to   = with (from, [] (Params& p) { p.mbFreqMid = 1500.0f; p.mbWidthLow = 1.8f; p.mbSolo = 0x2; });
        check (compare ("L3 multiband crossover + band width + Band Solo, reset @64",
                        afterReset (from, to, forcedAt (to, 64)), fresh (to)),
               "a host reset inside a forced swap lands the crossovers and band widths on the NEW targets");
    }

    // --- L4: a latency-changing swap (no dry fill): Oversampling Off -> 2x with Drive ------
    {
        const auto to = with (haas, [] (Params& p) { p.oversample = anamorph::OversampleFactor::x2; p.driveDb = 6.0f; });
        check (compare ("L4 Oversampling Off -> 2x + Drive 6 dB, reset @64", afterReset (haas, to, forcedAt (to, 64)), fresh (to)),
               "a host reset inside a latency-changing forced swap lands settled");
    }

    // --- L5: into the modulation voices, where Test 62's re-seed runs too -------------
    for (const auto alg : { Algorithm::Chorus, Algorithm::DimensionD })
    {
        const auto to = with (smooth, [alg] (Params& p) { p.algorithm = alg; p.algoAmount = 0.7f; p.dimMode = 3; });
        std::snprintf (name, sizeof name, "L5 Haas -> %s with the smoothers, reset @64",
                       alg == Algorithm::Chorus ? "Chorus" : "Dimension-D");
        check (compare (name, afterReset (haas, to, forcedAt (to, 64)), fresh (to)),
               "a host reset inside a forced swap into Chorus / Dimension-D starts at the configured sound");
    }

    // --- L6: the other three ways into a forced fade-out -------------------------------
    {
        const auto to = with (smooth, [] (Params& p) { p.haasDelayMs = 25.0f; });
        const auto velvet = with (haas, [] (Params& p) { p.algorithm = Algorithm::Velvet; });
        const auto midway = with (haas, [] (Params& p) { p.mix = 0.7f; p.width = 1.3f; });
        const auto early  = with (haas, [] (Params& p) { p.mix = 0.2f; p.width = 0.5f; });
        check (compare ("L6 ordinary duck upgraded to forced, reset in the fade-out",
                        afterReset (haas, to, [&] (Engine& e, juce::Random& rng)
                        { run (e, velvet, 1, 32, rng, nullptr); e.requestDuck(); run (e, to, 1, 32, rng, nullptr); }),
                        fresh (to)),
               "a host reset inside an ordinary duck upgraded to forced lands settled");
        check (compare ("L6 forced swap re-armed from its fade-in, reset in the fade-out",
                        afterReset (haas, to, [&] (Engine& e, juce::Random& rng)
                        { e.requestDuck(); run (e, midway, 1, 289, rng, nullptr);   // fade reaches silence
                          run (e, midway, 1, 200, rng, nullptr);                    // bottom, then 200 into the fade-in
                          e.requestDuck(); run (e, to, 1, 64, rng, nullptr); }),
                        fresh (to)),
               "a host reset inside a forced swap re-armed from the fade-in lands settled");
        check (compare ("L6 forced swap retargeted mid fade-out, reset",
                        afterReset (haas, to, [&] (Engine& e, juce::Random& rng)
                        { e.requestDuck(); run (e, early, 1, 32, rng, nullptr); run (e, to, 1, 32, rng, nullptr); }),
                        fresh (to)),
               "a host reset inside a retargeted forced swap lands on the LATEST target, settled");
    }

    // --- A2 / A3: the swap's bottom has already run ------------------------------------
    {
        const auto to = with (smooth, [] (Params& p) { p.haasDelayMs = 25.0f; });
        check (compare ("A2 forced swap completed, then a host reset",
                        afterReset (haas, to, [&] (Engine& e, juce::Random& rng)
                        { e.requestDuck(); run (e, to, 12, bs, rng, nullptr); }),     // 64 ms: back to Normal
                        fresh (to)),
               "a forced swap that completed before a host reset lands on a clean start");
        check (compare ("A3 host reset 10 ms into the forced fade-in",
                        afterReset (haas, to, [&] (Engine& e, juce::Random& rng)
                        { e.requestDuck(); run (e, to, 1, 289, rng, nullptr); run (e, to, 1, 480, rng, nullptr); }),
                        fresh (to)),
               "a host reset in a forced fade-in lands on a clean start");
    }

    // --- A4 / A5: live glides are not snapped ------------------------------------------
    // The same control sequence on silent history, with no reset: its lines hold only zeros.
    auto noResetTwin = [&] (const Params& reached, const Params& from)
    {
        const auto t = activate (from);
        juce::Random pre (3), mid (4), post (99);
        run (*t, from, history, bs, pre, nullptr, true);
        run (*t, reached, 1, 64, mid, nullptr, true);
        std::vector<float> out;
        run (*t, reached, window, bs, post, &out);
        return out;
    };
    {
        const auto rider = with (haas, [] (Params& p) { p.mix = 0.5f; p.width = 1.8f; p.outputGainDb = -6.0f; });
        check (compare ("A4 live Mix/Width/Output edit gliding at the reset",
                        afterReset (haas, rider, [&] (Engine& e, juce::Random& rng) { run (e, rider, 1, 64, rng, nullptr); }),
                        noResetTwin (rider, haas)),
               "a host reset does not snap a live edit's glide");
        const auto mono = with (haas, [] (Params& p) { p.monoMakerEnable = true; });
        const auto moduleRider = with (mono, [] (Params& p) { p.algoAmount = 0.9f; p.monoMakerFreq = 300.0f; });
        check (compare ("A4 live Haas amount / Mono Maker cutoff edit gliding at the reset",
                        afterReset (mono, moduleRider, [&] (Engine& e, juce::Random& rng) { run (e, moduleRider, 1, 64, rng, nullptr); }),
                        noResetTwin (moduleRider, mono)),
               "a host reset does not snap a live Haas amount or Mono Maker cutoff glide");
        const auto velvet = with (haas, [] (Params& p) { p.algorithm = Algorithm::Velvet; p.algoAmount = 0.6f; });
        const auto dense = with (velvet, [] (Params& p) { p.velvetDensity = 0.9f; });
        check (compare ("A4 live Velvet density edit gliding at the reset",
                        afterReset (velvet, dense, [&] (Engine& e, juce::Random& rng) { run (e, dense, 1, 64, rng, nullptr); }),
                        noResetTwin (dense, velvet)),
               "a host reset does not snap a live Velvet density glide");
        const auto ducked = with (rider, [] (Params& p) { p.mbBands = 3; });   // discrete, inaudible: Multiband is off
        check (compare ("A5 ordinary duck with live riders at the reset",
                        afterReset (haas, ducked, [&] (Engine& e, juce::Random& rng) { run (e, ducked, 1, 64, rng, nullptr); }),
                        noResetTwin (rider, haas)),
               "a host reset does not snap an ordinary duck's live riders");
    }

    // --- A7: a duck request not yet consumed commutes with the reset -------------------
    {
        const auto to = with (smooth, [] (Params& p) { p.haasDelayMs = 25.0f; });
        std::vector<float> outs[2];
        for (int requestFirst = 0; requestFirst < 2; ++requestFirst)
        {
            const auto e = activate (haas);
            juce::Random pre (3), post (99);
            run (*e, haas, history, bs, pre, nullptr);
            if (requestFirst == 1) e->requestDuck();
            e->reset (host);
            if (requestFirst == 0) e->requestDuck();
            run (*e, to, window, bs, post, &outs[requestFirst]);
        }
        check (compare ("A7 unconsumed duck request before vs after the reset", outs[1], outs[0]),
               "a duck request the engine has not consumed commutes with a host reset");
    }
}

// ---------------------------------------------------------------------------
//  Test 64 -- A NON-FINITE GLIDE TARGET DOES NOT LATCH THE MONO MAKER CUTOFF OR THE VELVET
//  DENSITY (ADR-0009; the engine's own contract -- State test 128 is the parameter path)
//
//  R7 found one glide shape that a single NaN latched: `current += k * (target - current)`
//  cannot leave NaN, and a module whose reset() never reseeds it stays poisoned after the
//  target is finite again. Two more glides had it, measured on the pre-fix engine:
//    * Mono Maker. `setFrequency` is a `jlimit`, which passes NaN; process()'s glide already
//      ignores a NaN target (`abs (current - NaN) > 0.05` is false), but `snapToTargets()` --
//      run by every prepare() -- copied it. A NaN cutoff at a prepare muted the output
//      (the self-heal zeroed every block) through a finite 200 Hz, a host reset, a forced
//      swap and a knob move, until a prepare that saw a finite value: 20/20 blocks, then
//      200/200. Engine API only: the parameter's own range maps a host NaN to 500 Hz before
//      the engine sees it (worklog NONFINITE_PARAMETERS_AND_F13.md §B1), so this documents the
//      ENGINE's contract.
//    * Velvet density. The glide absorbed NaN, `updateWeights` never ran again
//      (`abs (NaN - w) > 0` is false) and the output stayed finite, so the self-heal never
//      fired: the density froze until a finite re-prepare, and a re-prepare while NaN built
//      zero taps (Velvet silent at any Amount). Reachable from a host and from the editor's
//      value box (State test 128).
//  The rule both modules now follow is the one Mono Maker's live glide already had: a
//  non-finite target is ignored, so the module keeps what it had -- Mono Maker its current
//  cutoff (`snapToTargets`), re-clamped for the rate being prepared, and Velvet its last
//  finite density target (`setDensity`).
//
//  ORACLE: a twin engine driven identically except that it never receives the non-finite
//  value -- it keeps the finite value A held before (for a fresh engine: the module's own
//  initial target, Mono Maker 120 Hz, density 0.5). Output must be BIT-IDENTICAL from the
//  first block, through the finite value, a host reset, a re-prepare and a forced swap. A
//  control twin that never moves proves the finite move is audible, so the equality is not
//  the trivial one. Five non-finite spellings (quiet NaN, a payload NaN, -NaN, +Inf, -Inf)
//  must all land identically: Mono Maker's clamp turns +/-Inf into its range ends (finite, so
//  those twins hold +/-FLT_MAX, which the clamp maps to the same ends), Velvet ignores all
//  five. Two legs pin the details: a NaN that lands on a density glide in flight (the glide
//  goes on to its last finite target), and a re-prepare from 96 to 44.1 kHz with a 30 kHz
//  cutoff -- kept unclamped, that cutoff sat above Nyquist and the LR4 went unstable (finite
//  output up to 2.5e38, INC-003's class, measured on the first version of this guard).
static void testNonFiniteGlideTargetsDoNotLatch()
{
    std::printf ("Test 64: a non-finite glide target does not latch Mono Maker or the Velvet density (ADR-0009)\n");
    juce::ScopedNoDenormals noDenormals;

    constexpr double sr = 48000.0;
    constexpr int    bs = 256;
    using anamorph::Algorithm;
    using Params = anamorph::EngineParameters;
    using Engine = anamorph::AnamorphEngine;
    constexpr auto host = Engine::ResetScope::audioTailsOnly;

    auto bits = [] (std::uint32_t u) { float f; std::memcpy (&f, &u, sizeof f); return f; };
    const float bad[]      = { std::numeric_limits<float>::quiet_NaN(), bits (0x7FC00001u), bits (0xFFC00000u),
                               std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
    const char* badName[]  = { "NaN", "NaN 0x7fc00001", "-NaN", "+Inf", "-Inf" };

    auto activate = [] (Engine& e, const Params& p, double rate)
    {
        e.primeParameters (p);                                  // the wrapper's order (Test 55)
        e.prepare (rate, bs);
        e.setParameters (p);
    };

    // One scripted session run on three engines at once: A (receives the bad value), B (the
    // twin, receives `held` instead) and C (the control: `held` throughout, never moves).
    //   phase 0  `pre`   blocks at `start` (A and B identical; skipped when start is bad)
    //   phase 1  `bad`   blocks with the target bad in A, `held` in B -- entered through a
    //                    prepare when `prepareInBad` (A primed with the bad value, as the
    //                    wrapper primes whatever the parameter holds), at `reRate`; when
    //                    `midGlide`, the three first glide toward `held` for 3 blocks, so
    //                    the bad value lands on a glide in flight
    //   phase 2  the finite `to` for A and B, then a host reset, then a forced swap to
    //            `swapTo` -- C stays at `held` throughout.
    struct Leg
    {
        const char* name;
        bool        monoMaker;                     // else: Velvet density
        bool        freshBad;                      // bad value present at the FIRST prepare
        bool        prepareInBad;                  // a re-prepare while the value is bad
        bool        enableLate;                    // Mono Maker off at prepare, switched on at phase 2
        bool        midGlide;                      // the bad value lands on a glide toward `held`
        float       start, held, to;
        double      rate, reRate;                  // the first prepare's rate, the re-prepare's
    };
    // The rate-drop leg: a cutoff legal at 96 kHz (30 kHz) is above Nyquist at 44.1 kHz. The
    // kept cutoff must be re-clamped for the new rate, as `setFrequency` clamps a finite one --
    // unclamped it made the LR4 unstable (finite output up to 2.5e38: INC-003's class).
    const Leg legs[] = {
        { "Mono Maker: bad at the first prepare",      true,  true,  false, false, false, 0.0f,   120.0f,   200.0f, sr, sr },
        { "Mono Maker: live bad, then a re-prepare",   true,  false, true,  false, false, 150.0f, 150.0f,   200.0f, sr, sr },
        { "Mono Maker: bad while off, switched on",    true,  true,  false, true,  false, 0.0f,   120.0f,   200.0f, sr, sr },
        { "Mono Maker: bad, re-prepare 96 -> 44.1 kHz", true, false, true,  false, false, 30000.0f, 30000.0f, 200.0f, 96000.0, 44100.0 },
        { "Velvet density: live bad",                  false, false, false, false, false, 0.3f,   0.3f,   0.9f,   sr, sr },
        { "Velvet density: bad at the first prepare",  false, true,  false, false, false, 0.0f,   0.5f,   0.9f,   sr, sr },
        { "Velvet density: live bad, then a re-prepare", false, false, true, false, false, 0.3f,  0.3f,   0.9f,   sr, sr },
        { "Velvet density: bad mid-glide",             false, false, false, false, true,  0.3f,   0.9f,   0.5f,   sr, sr },
    };

    bool allSame = true, allFinite = true, allAudible = true;
    for (const auto& leg : legs)
        for (int k = 0; k < 5; ++k)
        {
            Params base;
            base.algorithm = leg.monoMaker ? Algorithm::Haas : Algorithm::Velvet;
            base.algoAmount = 0.8f;
            base.monoMakerEnable = leg.monoMaker && ! leg.enableLate;
            auto with = [&] (float v) { Params q = base; (leg.monoMaker ? q.monoMakerFreq : q.velvetDensity) = v; return q; };

            // Mono Maker clamps +/-Inf to a finite cutoff before any glide sees it: the twin for
            // those two holds +/-FLT_MAX, which the same clamp maps to the same end of the range
            // at every rate -- the clamp is unchanged, only NaN reaches the snap.
            float heldB = leg.held;
            if (leg.monoMaker && std::isinf (bad[k]))
                heldB = bad[k] > 0.0f ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max();

            auto a = std::make_unique<Engine>(), b = std::make_unique<Engine>(), c = std::make_unique<Engine>();
            activate (*a, leg.freshBad ? with (bad[k]) : with (leg.start), leg.rate);
            activate (*b, leg.freshBad ? with (heldB)  : with (leg.start), leg.rate);
            activate (*c, leg.freshBad ? with (leg.held) : with (leg.start), leg.rate);

            juce::AudioBuffer<float> A (2, bs), B (2, bs), C (2, bs);
            juce::Random rng (6464);
            bool same = true, finite = true;
            double moved = 0.0;                    // max |B - C| after the finite move (the control)
            int firstDiff = -1, blockNo = 0, muted = 0;   // muted: A silent where the twin is not
            auto step = [&] (const Params& pa, const Params& pb, const Params& pc, bool afterMove)
            {
                for (int i = 0; i < bs; ++i)
                {
                    const float l = rng.nextFloat() - 0.5f, r = 0.3f * (rng.nextFloat() - 0.5f);
                    A.setSample (0, i, l); A.setSample (1, i, r);
                    B.setSample (0, i, l); B.setSample (1, i, r);
                    C.setSample (0, i, l); C.setSample (1, i, r);
                }
                a->setParameters (pa); b->setParameters (pb); c->setParameters (pc);
                a->process (A); b->process (B); c->process (C);
                if (A.getMagnitude (0, bs) <= 0.0f && B.getMagnitude (0, bs) > 0.0f) ++muted;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < bs; ++i)
                    {
                        const float x = A.getSample (ch, i), y = B.getSample (ch, i);
                        if (! std::isfinite (x)) finite = false;
                        if (std::memcmp (&x, &y, sizeof x) != 0 && firstDiff < 0) firstDiff = blockNo;
                        if (afterMove) moved = juce::jmax (moved, (double) std::abs (y - C.getSample (ch, i)));
                    }
                ++blockNo;
            };

            const Params pBad = with (bad[k]), pHeldB = with (heldB), pHeld = with (leg.held);
            if (! leg.freshBad)
                for (int n = 0; n < 40; ++n) step (with (leg.start), with (leg.start), with (leg.start), false);
            if (leg.midGlide)
                for (int n = 0; n < 3; ++n) step (pHeld, pHeld, pHeld, false);
            for (int n = 0; n < 10; ++n) step (pBad, pHeldB, pHeld, false);
            if (leg.prepareInBad)
            {
                activate (*a, pBad, leg.reRate); activate (*b, pHeldB, leg.reRate); activate (*c, pHeld, leg.reRate);
                for (int n = 0; n < 20; ++n) step (pBad, pHeldB, pHeld, false);
            }
            Params pTo = with (leg.to), pCtl = pHeld;
            if (leg.enableLate) { pTo.monoMakerEnable = true; pCtl.monoMakerEnable = true; }
            for (int n = 0; n < 150; ++n) step (pTo, pTo, pCtl, true);
            a->reset (host); b->reset (host); c->reset (host);
            for (int n = 0; n < 40; ++n) step (pTo, pTo, pCtl, true);
            Params pSwap = pTo, pSwapC = pCtl;                        // a forced swap: the A/B, preset, undo route
            pSwap.algorithm = pSwapC.algorithm = leg.monoMaker ? Algorithm::Velvet : Algorithm::Haas;
            a->requestDuck(); b->requestDuck(); c->requestDuck();
            for (int n = 0; n < 60; ++n) step (pSwap, pSwap, pSwapC, true);

            same = firstDiff < 0;
            char at[32] = "";
            if (! same) std::snprintf (at, sizeof at, " from block %d", firstDiff);
            std::printf ("  %-44s %-15s %s the twin%s, muted blocks %3d/%d, output finite %d, control moved %.3f\n",
                         leg.name, badName[k], same ? "bit-identical to" : "DIFFERENT from", at,
                         muted, blockNo, (int) finite, moved);
            allSame    = allSame && same;
            allFinite  = allFinite && finite;
            allAudible = allAudible && moved > 1.0e-3;
        }

    check (allFinite,  "a non-finite Mono Maker / Velvet density target never reaches the output");
    check (allSame,    "a non-finite glide target is ignored: every leg is bit-identical to the twin that kept "
                       "the last finite target, through the finite value, a host reset, a re-prepare and a forced swap");
    check (allAudible, "control: the finite move is audible in every leg (the equality is not the trivial one)");
}

// ---------------------------------------------------------------------------
//  Test 65 -- A SUSTAINED NON-FINITE INPUT BURST DOES NOT SILENCE THE OUTPUT WITH LEVEL MATCH ON
//  (ADR-0009; F13's third item -- Test 59 is one bad block, this is a burst)
//
//  ADR-0009's self-heal zeroes non-finite samples and resets the stateful nodes, so "a stray
//  non-finite sample must not poison the output". `LoudnessMatch` has no guard of its own: a NaN
//  in the block makes it publish NaN until the self-heal's `loudness.reset()` later in the same
//  `process()` call -- and in between, the engine turned that reading into its match TARGET,
//  `decibelsToGain (NaN)` = 0. That target is finite, so the self-heal never sees it, and it does
//  not reset `matchGainSmooth`. One bad block costs a dip; a burst re-sets the target to 0 every
//  block and ramps the applied gain to silence. MEASURED on the pre-fix engine, a NaN in every
//  97th sample for 1 s (this test's configs): Level Match on, 164-165 of 187 blocks exact silence,
//  the burst 13.7-14.5 dB below where it should play; Level Match off, 0-2 blocks. A NaN reading
//  is no measurement, so the target now stays where it was -- ADR-0007's own rule for a reading
//  it cannot trust ("holds the last trusted value").
//
//  ASSERTED per config (Haas, Velvet, Chorus x Oversampling Off / 2x; Drive 6 dB so the match gain
//  is not unity): the output stays finite; with Level Match on, the burst leaves no more silent
//  blocks than with it off (+2); and the burst's level with Level Match on is the Level-Match-off
//  level moved by the pre-burst match gain, within 3 dB -- so the burst is not quietly turned down
//  either. What the burst does to the gain AFTERWARDS (the self-heal's full `loudness.reset()`
//  discards it) is ADR-0009's recorded owner question and is not asserted here.
static void testNonFiniteBurstKeepsLevelMatchAudible()
{
    std::printf ("Test 65: a sustained non-finite input burst does not silence the output with Level Match on (ADR-0009)\n");
    juce::ScopedNoDenormals noDenormals;

    constexpr double sr = 48000.0;
    constexpr int    bs = 256;
    using anamorph::Algorithm;
    using anamorph::OversampleFactor;
    const Algorithm algos[] = { Algorithm::Haas, Algorithm::Velvet, Algorithm::Chorus };
    const char* names[]     = { "Haas", "Velvet", "Chorus" };
    const int settle = (int) (2.0 * sr / bs), burst = (int) (1.0 * sr / bs);

    bool finite = true, notSilenced = true, levelKept = true;
    for (int a = 0; a < 3; ++a)
        for (auto os : { OversampleFactor::Off, OversampleFactor::x2 })
        {
            int zeroBlocks[2] = { 0, 0 };
            double burstSq[2] = { 0.0, 0.0 };
            float matchDb = 0.0f;
            for (int lm = 0; lm <= 1; ++lm)
            {
                anamorph::EngineParameters p;
                p.algorithm = algos[a]; p.algoAmount = 0.6f; p.oversample = os;
                p.driveDb = 6.0f; p.autoGainMatch = (lm == 1);
                auto e = std::make_unique<anamorph::AnamorphEngine>();   // heap: Test 59's note
                e->primeParameters (p);
                e->prepare (sr, bs);
                e->setParameters (p);

                juce::AudioBuffer<float> buf (2, bs);
                juce::Random rng (6565);
                for (int b = 0; b < settle + burst; ++b)
                {
                    const bool inBurst = b >= settle;
                    for (int i = 0; i < bs; ++i)
                    {
                        const float l = 0.5f * (rng.nextFloat() - 0.5f);
                        const bool bad = inBurst && ((b * bs + i) % 97) == 0;
                        buf.setSample (0, i, bad ? std::numeric_limits<float>::quiet_NaN() : l);
                        buf.setSample (1, i, 0.6f * l);
                    }
                    if (b == settle && lm == 1) matchDb = e->getMatchGainDb();
                    e->process (buf);
                    bool allZero = true;
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < bs; ++i)
                        {
                            const float v = buf.getSample (c, i);
                            if (! std::isfinite (v)) finite = false;
                            if (! juce::exactlyEqual (v, 0.0f)) allZero = false;
                            if (inBurst) burstSq[lm] += (double) v * v;
                        }
                    if (inBurst && allZero) ++zeroBlocks[lm];
                }
            }
            const auto db = [&] (double sq) { return 10.0 * std::log10 (juce::jmax (1.0e-30, sq / (2.0 * burst * bs))); };
            const double offDb = db (burstSq[0]), onDb = db (burstSq[1]);
            std::printf ("  %-6s OS %s: silent burst blocks  Level Match off %3d / on %3d of %d;  burst level off %6.2f dB,"
                         " on %6.2f dB (pre-burst match %+.2f dB)\n",
                         names[a], os == OversampleFactor::Off ? "off" : "2x ", zeroBlocks[0], zeroBlocks[1], burst,
                         offDb, onDb, (double) matchDb);
            notSilenced = notSilenced && zeroBlocks[1] <= zeroBlocks[0] + 2;
            levelKept   = levelKept && std::abs (onDb - (offDb + matchDb)) < 3.0;
        }

    check (finite,      "a sustained non-finite burst never reaches the output");
    check (notSilenced, "with Level Match on, a non-finite burst leaves no more silent blocks than with it off");
    check (levelKept,   "...and plays at the Level-Match-off level moved by the pre-burst match gain (within 3 dB)");
}

// ---------------------------------------------------------------------------
//  Test 66 -- LEVEL MATCH ENGAGES AT THE LEVEL IT MEASURED WHEN THE SWITCH CHANGES ONLY ITS GAIN
//  (ADR-0007, Amendment 2026-09-24 -- owner ruling O4g; resolves KI-031, Case B pends on F13(2) / KI-030.
//  State test 130 is the production-path half.)
//
//  THE CLAIM. A duck that turns Level Match on and changes nothing the measurement reads -- only Level
//  Match itself, Output Gain, Output Balance, Bypass or Band Solo (all after the tap), or a tolerant field by
//  a representation-sized amount (relative 1e-5) -- starts its fade-in at the published value: no swell, no
//  dip (Case A). The landing happens at the silent bottom, right after that block's loudness.process, on the
//  value that block publishes. The same engage carrying anything the measurement reads (the wet at the
//  tap, the dry reference, the predict's Drive and Mix, compared EXACTLY because any rise pre-ducks), or a
//  discrete edit that re-arms it, keeps the old start at unity and the glide (Case B, pending F13(2)).
//  Level Match already on, an A/B injection, Apply, Redo, disengage and a host reset are unchanged.
//
//  MEASURED BEFORE THE FIX (engine e9deabe, this test): every Case-A engage glided in from unity. D at the
//  first full-level block was +2.95 / +4.85 / +5.79 dB for the Undo of Apply at Drive 4 / 8 / 10, -4.95 dB
//  for the positive match (the mirror dip), +4.85 dB for each hand engage, +6.28 dB with the post-tap
//  riders and +5.08 / +3.85 / +6.66 dB for the ulp legs; wherever the pre-switch gain sat below the
//  published value (Undo of Apply, re-engage, from -12 dB, riders) the fade-in also overshot [pre-switch
//  gain, published] by +3.0 to +7.2 dB; and the (11) bottom block started from unity (D -18.8 / -18.3 dB).
//  6 of the 29 checks fail there (553 / 6 for the whole suite): LAND, the (3) re-duck (max|D| 1.29 dB),
//  (11), the sweep's iff, the sweep's post-tap rows and the armed-landing count. The Case-B,
//  unchanged-path, defensive-consumer and premise checks pass on both engines by design.
//
//  THE ORACLE (M1). Each run has an event-matched twin: the same seeded noise and history, the SAME duck
//  (requestDuck at the same block, the same discrete change, or -- for an ordinary engage whose only
//  discrete change is Level Match -- an inert opener: a band-count move with Multiband off, a Haas-side
//  move under Velvet), and Level Match OFF at a known Output Gain g_t. Per 256-sample block the
//  least-squares gain g^ = sum(run * twin) / sum(twin^2) gives the run's applied gain g = 20 log10 g^ + g_t,
//  and D = g - the run's published value after that block. The residual a pure gain leaves must be <= 1e-3
//  wherever D is judged (measured <= 2e-9 landed, <= 1e-4 gliding).
//    LAND      max|D| <= 0.1 dB over 0.6 s from the first full-level block; from the event on, the applied
//              gain never leaves [pre-switch gain, published] by more than 0.2 dB (a swell or a dip inside
//              the fade-in); and an un-ducked -20 dB injection after the window moves the run to -20 dB, so
//              Level Match is really on (with it off, the Undo-of-Apply shape still reads D = m - pub, 0.11 dB).
//    NOT LAND  glide fraction phi = D_F / (0 dB - pub_F) >= 0.5 (0.797-0.806 measured, before and after).
//    UNCHANGED bit-identical, from the first full-level block, to a twin in which no landing can happen.
//  Premise: every published value judged is >= 3 dB from unity, where the old glide started. The timing
//  (bottom event+2, first full-level block event+8 at 48 kHz / 256) is computed from the duck's documented
//  lengths and re-derived from the engine's output: an ordinary-duck twin against the lane with no event --
//  its last exact zero, and the first block from which the two are bit-identical.
//
//  LEGS, and the engine variants they reject (each built from this tree and run through this test; phi and
//  D quoted under a variant are its failing values):
//   (1) Undo of Apply, forced, Drive 4 / 8 / 10 (Level Match off at Output Gain m -> on, Output Gain -3).
//   (2) The positive match (Width 0 on anti-correlated input, m +7.4 dB): the mirror dip.
//   (3) Ordinary (hand) engages: re-engage at Output Gain m; from -12 / 0 / +6; and one snapshot carrying
//       Output Gain, Output Balance 0.3, Band Solo band 2 (Multiband on) and Bypass on -> off (measured once
//       its crossfade settles). Every lane's history opens one DIRTY ordinary duck (msMode + Drive) long
//       before its event, so an ordinary entry that ORs the flag instead of assigning it fails here; so does
//       a predicate comparing Output Gain ((1)-(3)) or Band Solo, or comparing mbSolo in M/S Solo's place.
//       And a hand re-engage four blocks into the fade-in of a DIRTY disengage (Level Match off with Width
//       +1e-4): a FadeIn re-duck, whose fresh fade-out must clear the flag it inherits. Rejects a re-duck
//       entry that keeps it (max|D| 1.29 dB).
//   (4) Every tolerant field in ONE forced engage (a single field over its tolerance refuses it): width, Haas
//       delay, amount, input balance and four band widths at +/-1 ulp, and the three crossovers and the Mono
//       Maker frequency -- the log-mapped fields -- at +/-2e-6 relative, just over the 1.6e-6 preset round
//       trip drift that sizes the 1e-5 tolerance (Haas, Multiband, Mono Maker); then rate, depth, width,
//       amount and balance under Chorus, and density, width, amount and balance under Velvet, at +/-1 ulp.
//       Rejects an all-exact compare, an exact velvetDensity compare (Velvet D_F +6.66 dB), a tolerance
//       tightened below the drift (1e-6 and 2e-7: Haas D_F +5.08 dB), and a forced entry that keeps a stale
//       flag.
//   (5) Case B, forced and ordinary-same-snapshot: Drive +0.01 dB and +1 ulp, Width +0.001, Mix -0.001 and
//       -1 ulp, algorithm, M/S solo, msMode, band count (Multiband on), Mono Maker, Oversampling factor, Drive
//       8 -> 10; a Drive edit arriving mid-fade-out, and one that returns to its start before the bottom; an
//       ordinary Drive 8 -> 10 engage upgraded to forced one block into its fade-out (Multiband off); and
//       the discrete edits that change no sample but re-arm (Haas side under Velvet, band count with
//       Multiband off). Rejects, in turn: no dirty flag (ordinary legs), no mid-duck marking (mid-fade-out
//       legs), no measurement predicate (forced legs), a tolerant Drive compare (Drive +1 ulp), a tolerant
//       Mix compare alone (Mix -1 ulp), Width not compared, an upgrade to forced that clears the dirty flag
//       (the upgrade leg), and no !procChanged term (re-arm legs). phi was <= 0.008 on every leg a variant
//       broke.
//   (6) A/B shape (requestDuck + inject m-5 / m+4 + Level Match on): bit-identical from the first full-level
//       block to the same switch between two Level-Match-on slots. And the DEFENSIVE consumer: an ordinary
//       engage whose bottom block also takes an injection (m-5, no forced duck), bit-identical from the
//       first full-level block to the same injection with Level Match on throughout. A landing that
//       overrides the injection is never identical inside the window: rejects either consumer leaving the
//       landing armed (the defensive one alone: identical only from event+122). Non-vacuity: each injection
//       is >= 1 dB off the published value.
//   (7) Redo (forced on -> off) and Apply (ordinary on -> off): bit-identical to a twin already off.
//   (8) Level Match on on both sides, forced Drive 0 -> 10: phi >= 0.3 (0.755). This PINS CURRENT BEHAVIOUR
//       pending F13(2); it does not claim the glide is right.
//   (9) A host reset one block into the fade-out and one block into the fade-in: bit-identical from the
//       reset block to the same reset inside a Level-Match-on-both duck, and max|D| <= 0.1 dB.
//  (10) FIELD SWEEP, derived from the engine rather than from a name list. For each base and each
//       EngineParameters member (the structured binding below makes the member count a compile error to get
//       wrong), a pair of Level-Match-OFF engines sharing one forced duck and differing only in that field
//       decides "measurement-inert" by a bit-identical published trajectory, and the engaging engine must
//       land (phi < 0.25) exactly when it is. Bases: H (Haas, Multiband off, Mono Maker off), C (Chorus,
//       Multiband 4 bands, Mono Maker on, Mix 1), and M / M2 / M3 (Multiband on with ONE / TWO / THREE
//       bands; Multiband rows only, where the band-count guards decide). The post-tap fields are the
//       contract's declared exclusions: they must land, and their published effect must be nil or -- Bypass
//       and the switch itself, with Multiband on at Mix 100% -- the documented H4 reference switch
//       (<= 0.0064 dB; 2e-4 measured). Rejects unguarded Haas fields (C), unguarded Multiband fields (H), a
//       missing band-count guard (M), a guard threshold one band too high (the low crossover / mid width
//       at >= 3 bands: they move yet land in M2; the mid crossover / hi-mid width at >= 4: in M3), an
//       unguarded dimMode, and every predicate variant above.
//  (11) WHEN the landing happens. An ordinary gain-only engage while the matcher converges fast: Width 0,
//       amount 0, Drive 0, Multiband off, the input anti-phase (R = -L: the wet vanishes and the match climbs
//       past +22 dB) until 1 / 2 blocks before the event, then correlated, so the published value moves
//       ~0.65 dB across the bottom block itself (non-vacuity: >= 0.3 dB). The twin opens the same duck with
//       an inert band-count move. The bottom block must play at the value that block publishes, |D| <= 0.1
//       dB (0.000 measured). After the bottom the applied gain follows the moving published value through
//       Level Match's smoother and trails it (D(bot+1) +0.66, D_F +3.5 dB here), so only the bottom block is
//       judged. Rejects landing on the previous block's published value, before the measurement (D(bot)
//       +0.63 / +0.66 dB).
//  The allocation guard (tests/AllocationGuard.h, Test 38's pattern) is armed around setParameters + process
//  from every event to its first full-level block, and a landing counts only when |D| <= 0.1 dB at that
//  armed block: the gain could only have left unity at the bottom inside the armed run of blocks. (The
//  (11) bottom blocks lie inside the same armed run.)
//
//  NOT REJECTED, by construction: dropping a DISCRETE field from the predicate (each is also a re-arm
//  trigger, so !procChanged refuses it first -- Mono Maker Enable measured). A sound change made live
//  BEFORE the engage duck opens (the F13(2) boundary) is not asserted here.
static void testLevelMatchEngagesAtTheLevelItMeasured()
{
    std::printf ("Test 66: Level Match engages at the level it measured when the switch changes only its gain (ADR-0007)\n");
    juce::ScopedNoDenormals noDenormals;

    using anamorph::AnamorphEngine;
    using anamorph::Algorithm;
    using anamorph::HaasSide;
    using anamorph::OversampleFactor;
    using anamorph::SoloMode;
    using Params = anamorph::EngineParameters;
    constexpr double sr = 48000.0;
    constexpr int    bs = 256, nch = 2, blk = bs * nch;

    // THE DUCK'S TIMING, from its documented lengths (AnamorphEngine::prepare: ~6 ms out, ~28 ms in) and
    // re-derived from the engine's own output in group A (premise): 288 samples of fade-out put the silent
    // bottom at event + 2, and 1344 samples of fade-in from there make event + 8 the first full-level block.
    const int fadeOut = (int) std::lround (0.006 * sr), fadeIn = (int) std::lround (0.028 * sr);
    const int kBot  = fadeOut / bs + 1;
    const int kFull = kBot + (fadeIn + bs - 1) / bs;
    const int kWin  = (int) std::lround (0.6 * sr / bs);           // the 0.6 s evaluation window
    const int hist  = (int) std::lround (0.1 * sr / bs);           // the dirty history duck (below)

    // ONE FIELD COUNT, CHECKED BY THE COMPILER. The sweep (group S) needs a row for every EngineParameters
    // member; this binding stops compiling the day the struct gains or loses one.
    {
        const Params probe;
        [[maybe_unused]] const auto& [f01, f02, f03, f04, f05, f06, f07, f08, f09, f10, f11, f12,
                                      f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24,
                                      f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36] = probe;
    }

    const auto guard = anamorph::testing::selfCheck();
    const bool guardLive = guard.newLive || guard.mallocLive;
    if (! guardLive)
        std::printf ("::warning::the allocation guard is compiled out in this build -- Test 66's landings are "
                     "NOT allocation-checked by it in this run (RealtimeSanitizer, where present, covers them).\n");

    // ---- lanes: engines run in lockstep on one seeded input stream ------------------------------------
    struct Lane
    {
        Params pre;                                              // the pre-event snapshot
        std::function<void (int, AnamorphEngine&, Params&)> at;  // per block, before setParameters
        int last = 0, keep = 0;
        std::unique_ptr<AnamorphEngine> e;
        juce::AudioBuffer<float> buf;
        std::vector<float> pub, y;                               // published dB per block; output from `keep`
    };
    // Every lane's history opens one DIRTY ordinary duck -- msMode and Drive in one snapshot -- long before
    // its event, so a flag that outlived that duck would refuse every landing below.
    const auto applyHistory = [hist] (int b, Params& s) { if (b < hist) { s.msMode = ! s.msMode; s.driveDb += 1.0f; } };

    long worstNew = 0, worstMalloc = 0;
    int  armedCalls = 0;
    // anti: anti-correlated input throughout. antiPhaseUntil (group G): the blocks before it carry R = -L
    // exactly, the blocks from it the ordinary correlated input.
    const auto runLanes = [&] (std::vector<Lane>& lanes, int ev, bool anti, int seed, int antiPhaseUntil = -1)
    {
        int blocks = 0;
        for (auto& ln : lanes)
        {
            Params s0 = ln.pre;
            applyHistory (0, s0);
            ln.e = std::make_unique<AnamorphEngine>();   // heap: Test 59's note (1 MB-stack lane)
            ln.e->primeParameters (s0);
            ln.e->prepare (sr, bs);
            ln.e->setParameters (s0);
            ln.keep = ev - 1;
            ln.buf.setSize (nch, bs);
            ln.pub.reserve ((size_t) ln.last + 1);
            ln.y.reserve ((size_t) (ln.last + 1 - ln.keep) * blk);
            blocks = juce::jmax (blocks, ln.last + 1);
        }
        juce::Random rng (seed);
        juce::AudioBuffer<float> in (nch, bs);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < bs; ++i)
            {
                const float v = rng.nextFloat() - 0.5f, w = rng.nextFloat() - 0.5f;
                in.setSample (0, i, v);
                in.setSample (1, i, b < antiPhaseUntil ? -v : anti ? -0.5f * v + 0.5f * w : 0.6f * v + 0.2f * w);
            }
            for (auto& ln : lanes)
            {
                if (b > ln.last) continue;
                Params snap = ln.pre;
                applyHistory (b, snap);
                ln.at (b, *ln.e, snap);
                for (int c = 0; c < nch; ++c) ln.buf.copyFrom (c, 0, in, c, 0, bs);
                if (b >= ev && b <= ev + kFull)          // the event, its bottom (the landing) and the fade-in
                {
                    anamorph::testing::resetCounts();
                    {
                        anamorph::testing::Armed arm;
                        ln.e->setParameters (snap);
                        ln.e->process (ln.buf);
                    }
                    ++armedCalls;
                    worstNew    = juce::jmax (worstNew,    anamorph::testing::newCount.load());
                    worstMalloc = juce::jmax (worstMalloc, anamorph::testing::mallocCount.load());
                }
                else
                {
                    ln.e->setParameters (snap);
                    ln.e->process (ln.buf);
                }
                ln.pub.push_back (ln.e->getMatchGainDb());
                if (b >= ln.keep)
                    for (int i = 0; i < bs; ++i) { ln.y.push_back (ln.buf.getSample (0, i)); ln.y.push_back (ln.buf.getSample (1, i)); }
            }
        }
    };

    // ---- M1: the applied gain of run `r`, read against an event-matched twin `t` --------------------
    struct Fit { double gDb = 0.0, resid = 0.0; bool ok = false; };
    const auto fit = [&] (const Lane& r, const Lane& t, int b) -> Fit
    {
        const float* x = r.y.data() + (size_t) (b - r.keep) * blk;
        const float* z = t.y.data() + (size_t) (b - t.keep) * blk;
        double num = 0.0, den = 0.0, ex = 0.0;
        for (int i = 0; i < blk; ++i) { num += (double) x[i] * z[i]; den += (double) z[i] * z[i]; ex += (double) x[i] * x[i]; }
        if (! (den > 1.0e-20 && ex > 1.0e-20)) return { 0.0, 0.0, false };
        const double g = num / den;
        double res = 0.0;
        for (int i = 0; i < blk; ++i) { const double d = (double) x[i] - g * z[i]; res += d * d; }
        return { 20.0 * std::log10 (juce::jmax (1.0e-12, std::abs (g))), res / ex, true };
    };
    const auto sameBlock = [&] (const Lane& a, const Lane& c, int b)
    {
        return std::memcmp (a.y.data() + (size_t) (b - a.keep) * blk, c.y.data() + (size_t) (b - c.keep) * blk,
                            sizeof (float) * (size_t) blk) == 0;
    };
    // First block from which `a` and `c` are bit-identical through `to` (to + 1: not even the last one).
    const auto identicalFrom = [&] (const Lane& a, const Lane& c, int from, int to)
    {
        int k = to + 1;
        while (k > from && sameBlock (a, c, k - 1)) --k;
        return k;
    };

    struct Judged { double gPre = 0.0, pubF = 0.0, dF = 0.0, phi = 0.0, maxD = 0.0, exc = -1.0e9, resid = 0.0; bool ok = true; };
    // gT: the twin's applied gain (its Output Gain; Level Match off). wasOn: Level Match was on before the event.
    const auto judge = [&] (const Lane& r, const Lane& t, double gT, int ev, bool wasOn, int to) -> Judged
    {
        Judged j;
        const Fit f0 = fit (r, t, ev - 1);
        j.ok   = f0.ok;
        j.gPre = f0.gDb + gT;                                     // the run's applied gain before the event
        const int F = ev + kFull;
        j.pubF = r.pub[(size_t) F];
        for (int b = ev; b <= to; ++b)
        {
            const Fit f = fit (r, t, b);
            const double g = f.gDb + gT, pb = r.pub[(size_t) b];
            if (f.ok && b <= ev + kWin)
                j.exc = juce::jmax (j.exc, juce::jmin (j.gPre, pb) - g, g - juce::jmax (j.gPre, pb));
            if (b >= F)
            {
                j.ok = j.ok && f.ok;
                if (b == F) j.dF = g - pb;
                j.maxD  = juce::jmax (j.maxD, std::abs (g - pb));
                j.resid = juce::jmax (j.resid, f.resid);
            }
        }
        j.phi = j.dF / ((wasOn ? j.gPre : 0.0) - j.pubF);          // off: the smoother held unity (0 dB)
        return j;
    };
    const auto lands = [] (const Judged& j) { return j.ok && j.maxD <= 0.1 && j.exc <= 0.2 && j.resid <= 1.0e-3; };

    int landLegs = 0, armedLandings = 0;
    const auto countArmed = [&] (const Judged& j) { ++landLegs; if (std::abs (j.dF) <= 0.1) ++armedLandings; };
    const auto printLand = [] (const char* name, const Judged& j)
    {
        std::printf ("  %-58s: pub_F %+6.2f  D_F %+6.3f  max|D| %.3f  excursion %+6.3f  resid %.1e  phi %.3f\n",
                     name, j.pubF, j.dF, j.maxD, j.exc, j.resid, j.phi);
    };

    const auto base = [] (Algorithm a, float drive)
    {
        Params p;
        p.algorithm = a; p.algoAmount = 0.5f; p.width = 1.3f; p.driveDb = drive;
        return p;
    };
    const auto matchOn = [] (Params& s) { s.autoGainMatch = true; s.outputGainDb = -3.0f; };

    // =====================================================================================================
    //  GROUP A -- Haas, Drive 8. Legs (1) at Drive 8, (3), (6), (7), (9), and the timing derivation.
    // =====================================================================================================
    const int U = (int) std::lround (2.0 * sr / bs), A = (int) std::lround (1.5 * sr / bs);
    const int probeB = U + kFull + kWin + 1;                      // the injection probe, read one block later
    const int lastL  = probeB + 1;
    const int winTo = U + kFull + kWin;

    bool allLand = true, premises = true, probesLive = true;
    const auto probeAt = [probeB] (int b, AnamorphEngine& e) { if (b == probeB) e.injectMatchGainDb (-20.0f); };
    const auto probeOk = [&] (const Lane& r, const Lane& t, double gT)
    {
        const Fit f = fit (r, t, probeB + 1);
        return f.ok && std::abs (f.gDb + gT + 20.0) <= 0.5;
    };

    // An Apply-then-Undo lane pair (Level Match on until A, then off at Output Gain m; forced Undo at U).
    struct ApplyMemo { float m = 0.0f; bool set = false, agree = true; };
    const auto applied = [] (ApplyMemo& memo, int aBlock, int b, AnamorphEngine& e, Params& s)
    {
        if (b < aBlock) return;
        if (b == aBlock)
        {
            const float v = e.getMatchGainDb();
            if (memo.set && ! juce::exactlyEqual (v, memo.m)) memo.agree = false;
            memo.m = v; memo.set = true;
        }
        s.autoGainMatch = false;
        s.outputGainDb  = memo.m;
    };

    ApplyMemo mA;
    {
        const Params h8 = base (Algorithm::Haas, 8.0f);
        Params onPre = h8; matchOn (onPre);
        std::vector<Lane> L;
        L.reserve (24);
        const auto add = [&L, lastL] (const Params& pre, std::function<void (int, AnamorphEngine&, Params&)> fn)
        { Lane ln; ln.pre = pre; ln.at = std::move (fn); ln.last = lastL; L.push_back (std::move (ln)); };

        add (onPre, [&] (int b, AnamorphEngine& e, Params& s) { applied (mA, A, b, e, s); if (b == U) e.requestDuck(); });   // 0 tForced
        add (onPre, [&] (int b, AnamorphEngine& e, Params& s)                                                              // 1 rUndo
             { applied (mA, A, b, e, s); if (b >= U) { if (b == U) e.requestDuck(); matchOn (s); } probeAt (b, e); });
        for (const float off : { -5.0f, 4.0f })
        {
            add (onPre, [&, off] (int b, AnamorphEngine& e, Params& s)                                                     // 2/4 rInj
                 {
                     applied (mA, A, b, e, s);
                     if (b == U) { e.requestDuck(); e.injectMatchGainDb (mA.m + off); }
                     if (b >= U) matchOn (s);
                 });
            add (onPre, [&, off] (int b, AnamorphEngine& e, Params&)                                                       // 3/5 tInj
                 { if (b == U) { e.requestDuck(); e.injectMatchGainDb (mA.m + off); } });
        }
        add (onPre, [&] (int b, AnamorphEngine& e, Params& s)                                                              // 6 rRedo
             { if (b >= U) { if (b == U) e.requestDuck(); s.autoGainMatch = false; s.outputGainDb = mA.m; } });
        add (onPre, [&] (int b, AnamorphEngine&, Params& s)                                                                // 7 rApplyShape
             { if (b >= U) { s.autoGainMatch = false; s.outputGainDb = mA.m; } });
        add (onPre, [&] (int b, AnamorphEngine& e, Params& s) { applied (mA, A, b, e, s); });                              // 8 tNone
        add (onPre, [&] (int b, AnamorphEngine& e, Params& s)                                                              // 9 rReengage
             { applied (mA, A, b, e, s); if (b >= U) s.autoGainMatch = true; probeAt (b, e); });
        add (onPre, [&] (int b, AnamorphEngine& e, Params& s) { applied (mA, A, b, e, s); if (b >= U) s.mbBands = 3; });   // 10 tOrd
        for (const float og : { -12.0f, 0.0f, 6.0f })                                                                      // 11-13 rHand
        {
            Params offPre = h8; offPre.outputGainDb = og;
            add (offPre, [&] (int b, AnamorphEngine& e, Params& s) { if (b >= U) s.autoGainMatch = true; probeAt (b, e); });
        }
        for (const int at : { U + 1, U + kBot + 1 })                                                                       // 14-19 reset
        {
            const auto duckAndReset = [U, at] (int b, AnamorphEngine& e)
            {
                if (b == U)  e.requestDuck();
                if (b == at) e.reset (AnamorphEngine::ResetScope::audioTailsOnly);
            };
            add (onPre, [&, duckAndReset] (int b, AnamorphEngine& e, Params& s)       // run: Apply, then the engaging Undo
                 { applied (mA, A, b, e, s); duckAndReset (b, e); if (b >= U) matchOn (s); });
            add (onPre, [duckAndReset] (int b, AnamorphEngine& e, Params&) { duckAndReset (b, e); });   // Level Match on both
            add (onPre, [&, duckAndReset] (int b, AnamorphEngine& e, Params& s)       // M1 twin: Level Match off at m
                 { applied (mA, A, b, e, s); duckAndReset (b, e); });
        }
        // (6) the DEFENSIVE consumer: an ordinary engage whose bottom block also takes an injection (no forced
        // duck). Twin: Level Match on throughout, the same injection, no duck.
        add (onPre, [&] (int b, AnamorphEngine& e, Params& s)                                                              // 20 rInjOrd
             { applied (mA, A, b, e, s); if (b >= U) s.autoGainMatch = true; if (b == U + kBot) e.injectMatchGainDb (mA.m - 5.0f); });
        add (onPre, [&] (int b, AnamorphEngine& e, Params&) { if (b == U + kBot) e.injectMatchGainDb (mA.m - 5.0f); });    // 21 tInjOrd
        // (3) a DIRTY ordinary disengage (Width +1e-4 with Level Match off), then a hand re-engage four blocks
        // into its fade-in -- a FadeIn re-duck. Twin: Level Match off at -3, the same two ducks opened by inert
        // band-count moves (Multiband off).
        add (onPre, [&] (int b, AnamorphEngine&, Params& s)                                                                // 22 rReduck
             { if (b >= U) { s.autoGainMatch = false; s.width += 1.0e-4f; } if (b >= U + kBot + 4) s.autoGainMatch = true; });
        {
            Params offM3 = h8; offM3.outputGainDb = -3.0f;
            add (offM3, [&] (int b, AnamorphEngine&, Params& s)                                                            // 23 tReduck
                 { if (b >= U) { s.mbBands = 3; s.width += 1.0e-4f; } if (b >= U + kBot + 4) s.mbBands = 2; });
        }
        runLanes (L, U, false, 6601);
        const double m = mA.m;

        // Timing, derived: the ordinary-duck twin (an inert band-count move, Multiband off) against the lane
        // with no event. Last exact-zero sample -> the bottom; first bit-identical block -> full level.
        const int fullAt = identicalFrom (L[10], L[8], U, winTo);
        int lastZero = -1;
        for (int b = U; b < fullAt; ++b)
            for (int i = 0; i < blk; ++i)
                if (juce::exactlyEqual (L[10].y[(size_t) (b - L[10].keep) * blk + (size_t) i], 0.0f)) lastZero = b;
        std::printf ("  %-58s: bottom event%+d, first full-level block event%+d (documented: %+d / %+d)\n",
                     "duck timing, derived from the engine's output", lastZero + 1 - U, fullAt - U, kBot, kFull);
        check (lastZero + 1 == U + kBot && fullAt == U + kFull,
               "premise: the duck's bottom and first full-level block, read from the engine's output, are event+2 / event+8");
        check (mA.agree, "premise: every Apply lane locked the same value (the lanes share one history)");

        // (1) forced engage in the Undo-of-Apply shape, Drive 8
        {
            const Judged j = judge (L[1], L[0], m, U, false, winTo);
            char nm[96]; std::snprintf (nm, sizeof nm, "(1) Undo of Apply, forced, Drive 8 (m %+.2f)", m);
            printLand (nm, j);
            allLand = allLand && lands (j); premises = premises && std::abs (j.pubF) >= 3.0;
            probesLive = probesLive && probeOk (L[1], L[0], m);
            countArmed (j);
        }
        // (3) ordinary engages: re-engage at Output Gain m, and from Output Gain -12 / 0 / +6
        {
            const Judged j = judge (L[9], L[10], m, U, false, winTo);
            printLand ("(3) hand re-engage after Apply, ordinary (Output Gain m)", j);
            allLand = allLand && lands (j); premises = premises && std::abs (j.pubF) >= 3.0;
            probesLive = probesLive && probeOk (L[9], L[10], m);
            countArmed (j);
            const float ogs[] = { -12.0f, 0.0f, 6.0f };
            for (int k = 0; k < 3; ++k)
            {
                const Judged jh = judge (L[(size_t) (11 + k)], L[10], m, U, false, winTo);
                char nm[96]; std::snprintf (nm, sizeof nm, "(3) hand engage, ordinary, from Output Gain %+.0f", (double) ogs[k]);
                printLand (nm, jh);
                allLand = allLand && lands (jh) && std::abs (jh.gPre - ogs[k]) <= 0.01;
                premises = premises && std::abs (jh.pubF) >= 3.0;
                probesLive = probesLive && probeOk (L[(size_t) (11 + k)], L[10], m);
                countArmed (jh);
            }
            // the FadeIn re-duck: its fresh fade-out clears the dirty flag of the duck it interrupts
            const int ev2 = U + kBot + 4;
            const Judged jr = judge (L[22], L[23], -3.0, ev2, false, winTo);
            printLand ("(3) hand re-engage 4 blocks into a dirty disengage's fade-in", jr);
            premises = premises && std::abs (jr.pubF) >= 3.0;
            check (lands (jr), "a hand re-engage that re-ducks a dirty duck's fade-in lands (the fresh fade-out clears "
                               "the flag it inherited)");
        }
        // (6) A/B: requestDuck + inject + Level Match on == the same switch between two Level-Match-on slots
        bool abSame = true, abVisible = true;
        for (int k = 0; k < 2; ++k)
        {
            const Lane& r = L[(size_t) (2 + 2 * k)];
            const int from = identicalFrom (r, L[(size_t) (3 + 2 * k)], U, winTo);
            const Judged j = judge (r, L[0], m, U, false, winTo);
            std::printf ("  %-58s: bit-identical to the Level-Match-on twin from event%+d; D_F %+.3f dB\n",
                         k == 0 ? "(6) A/B shape, injection m-5 dB" : "(6) A/B shape, injection m+4 dB", from - U, j.dF);
            abSame = abSame && from <= U + kFull;
            abVisible = abVisible && std::abs (j.dF) >= 1.0;
        }
        {   // the defensive consumer, at an ORDINARY engaging bottom
            const int from = identicalFrom (L[20], L[21], U, winTo);
            const Judged j = judge (L[20], L[10], m, U, false, winTo);
            std::printf ("  %-58s: bit-identical to the Level-Match-on twin from event%+d; D_F %+.3f dB\n",
                         "(6) ordinary engage, injection m-5 dB at its bottom block", from - U, j.dF);
            abVisible = abVisible && std::abs (j.dF) >= 1.0;
            check (from <= U + kFull, "an injection consumed at an ORDINARY engaging bottom (the defensive consumer) keeps "
                                      "priority: bit-identical from the first full-level block to the same injection with "
                                      "Level Match on throughout");
        }
        check (abSame, "an A/B injection consumed at a Level-Match-engaging bottom keeps priority: bit-identical from "
                       "the first full-level block to the same switch between two Level-Match-on slots");
        check (abVisible, "non-vacuity: the injected gain is >= 1 dB from the published value at the first full-level block");
        // (7) Redo (forced on -> off) and Apply (ordinary on -> off): unchanged
        {
            const int redo = identicalFrom (L[6], L[0], U, winTo), apply = identicalFrom (L[7], L[8], U, winTo);
            std::printf ("  %-58s: bit-identical to the twin already off from event%+d / event%+d\n",
                         "(7) Redo (forced on->off) / Apply (ordinary on->off)", redo - U, apply - U);
            check (redo <= U + kFull && apply <= U + kFull,
                   "a switch that turns Level Match OFF is unchanged: bit-identical from the first full-level block to a twin already off");
            check (! sameBlock (L[6], L[0], U - 1) && ! sameBlock (L[7], L[8], U - 1),
                   "non-vacuity: before the switch the Level-Match-on run and its twin differ");
        }
        // (9) host reset one block into the fade-out and one block into the fade-in
        {
            bool same = true, near = true, live = true;
            const int ats[] = { U + 1, U + kBot + 1 };
            for (int k = 0; k < 2; ++k)
            {
                const Lane& r = L[(size_t) (14 + 3 * k)];
                const int from = identicalFrom (r, L[(size_t) (15 + 3 * k)], ats[k], winTo);
                double maxD = 0.0;
                for (int b = ats[k]; b <= ats[k] + kWin; ++b)
                {
                    const double g = fit (r, L[(size_t) (16 + 3 * k)], b).gDb + m;
                    maxD = juce::jmax (maxD, std::abs (g - r.pub[(size_t) b]));
                }
                std::printf ("  %-58s: bit-identical to the Level-Match-on-both reset from event%+d; max|D| %.3f dB\n",
                             k == 0 ? "(9) host reset one block into the fade-out" : "(9) host reset one block into the fade-in",
                             from - U, maxD);
                same = same && from <= ats[k];
                near = near && maxD <= 0.1;
                live = live && ! sameBlock (r, L[1], ats[k]);
            }
            check (same, "a host reset inside an engaging duck lands where the same reset inside a Level-Match-on-both duck does");
            check (near, "...and the applied gain is the published value from the reset block on (max|D| <= 0.1 dB)");
            check (live, "liveness: the reset reached the engine (its block differs from the same engage without the reset)");
        }
    }

    // =====================================================================================================
    //  GROUPS B / C -- (1) at Drive 4 and 10, (2) the positive match (Width 0 on anti-correlated input)
    // =====================================================================================================
    struct Shape { const char* name = nullptr; Params p; bool anti = false; };
    Params pos = base (Algorithm::Haas, 0.0f); pos.algoAmount = 0.0f; pos.width = 0.0f;
    const Shape shapes[] = { { "(1) Undo of Apply, forced, Drive 4", base (Algorithm::Haas, 4.0f), false },
                             { "(1) Undo of Apply, forced, Drive 10", base (Algorithm::Haas, 10.0f), false },
                             { "(2) Undo of Apply, forced, positive match (Width 0)", pos, true } };
    for (const auto& sh : shapes)
    {
        ApplyMemo memo;
        Params onPre = sh.p; matchOn (onPre);
        std::vector<Lane> L (2);
        L[0].pre = onPre; L[0].last = lastL;
        L[0].at  = [&] (int b, AnamorphEngine& e, Params& s) { applied (memo, A, b, e, s); if (b == U) e.requestDuck(); };
        L[1].pre = onPre; L[1].last = lastL;
        L[1].at  = [&] (int b, AnamorphEngine& e, Params& s)
                   { applied (memo, A, b, e, s); if (b >= U) { if (b == U) e.requestDuck(); matchOn (s); } probeAt (b, e); };
        runLanes (L, U, sh.anti, 6602);
        const Judged j = judge (L[1], L[0], memo.m, U, false, winTo);
        char nm[96]; std::snprintf (nm, sizeof nm, "%s (m %+.2f)", sh.name, (double) memo.m);
        printLand (nm, j);
        allLand = allLand && lands (j);
        premises = premises && std::abs (j.pubF) >= 3.0 && (! sh.anti || memo.m >= 3.0f);
        probesLive = probesLive && probeOk (L[1], L[0], memo.m);
        countArmed (j);
    }

    // =====================================================================================================
    //  GROUP D -- (3) post-tap riders in the engaging snapshot: Output Gain, Output Balance 0.3, Band Solo,
    //  Bypass (on before the event, off in the snapshot, measured once its crossfade has settled).
    // =====================================================================================================
    {
        Params v = base (Algorithm::Velvet, 8.0f); v.mbEnable = true;
        v.bypass = true; v.outputGainDb = -12.0f;
        std::vector<Lane> L (2);
        L[0].pre = v; L[0].last = lastL;                          // twin: the same riders, opened by an inert Haas-side move
        L[0].at  = [&] (int b, AnamorphEngine&, Params& s)
                   { if (b >= U) { s.haasSide = HaasSide::Right; s.outputBalance = 0.3f; s.mbSolo = 0x2; s.bypass = false; } };
        L[1].pre = v; L[1].last = lastL;
        L[1].at  = [&] (int b, AnamorphEngine& e, Params& s)
                   {
                       if (b >= U)
                       { s.autoGainMatch = true; s.outputGainDb = 6.0f; s.outputBalance = 0.3f; s.mbSolo = 0x2; s.bypass = false; }
                       probeAt (b, e);
                   };
        runLanes (L, U, false, 6603);
        const Judged j = judge (L[1], L[0], -12.0, U, false, winTo);
        printLand ("(3) ordinary engage + Output Gain/Balance, Band Solo, Bypass", j);
        allLand = allLand && lands (j);
        premises = premises && std::abs (j.pubF) >= 3.0;
        probesLive = probesLive && probeOk (L[1], L[0], -12.0);
        countArmed (j);
    }

    // =====================================================================================================
    //  GROUP E -- (4) representation tolerance: every tolerant field in ONE forced engage (a single field over
    //  the tolerance would refuse it), at +/-1 ulp -- the log-mapped crossovers and Mono Maker frequency at
    //  +/-2e-6 relative, the preset round trip's drift. Haas + Multiband + Mono Maker, Chorus, and Velvet.
    // =====================================================================================================
    {
        const auto ulp = [] (float& x, float dir) { x = std::nextafter (x, dir * std::numeric_limits<float>::infinity()); };
        Params hm = base (Algorithm::Haas, 8.0f); hm.inputBalance = 0.2f; hm.mbEnable = true; hm.monoMakerEnable = true;
        Params ch = base (Algorithm::Chorus, 10.0f); ch.inputBalance = 0.2f;
        const auto hmUlp = [ulp] (Params& s, float d)
        {
            ulp (s.width, d); ulp (s.haasDelayMs, -d); ulp (s.algoAmount, d); ulp (s.inputBalance, -d);
            // the log-mapped fields by the preset round trip's measured drift (1.6e-6 relative), not 1 ulp
            s.mbFreqLow *= 1.0f + d * 2.0e-6f; s.mbFreqMid *= 1.0f - d * 2.0e-6f; s.mbFreqHigh *= 1.0f + d * 2.0e-6f;
            s.monoMakerFreq *= 1.0f + d * 2.0e-6f;
            ulp (s.mbWidthLow, d); ulp (s.mbWidthMid, -d); ulp (s.mbWidthHiMid, d); ulp (s.mbWidthHigh, -d);
        };
        const auto chUlp = [ulp] (Params& s, float d)
        { ulp (s.chorusRate, d); ulp (s.chorusDepth, -d); ulp (s.width, d); ulp (s.algoAmount, -d); ulp (s.inputBalance, d); };
        Params vv = base (Algorithm::Velvet, 8.0f); vv.inputBalance = 0.2f;
        const auto vvUlp = [ulp] (Params& s, float d)
        { ulp (s.velvetDensity, d); ulp (s.width, -d); ulp (s.algoAmount, d); ulp (s.inputBalance, -d); };
        struct UlpLeg { const char* name = nullptr; Params p; std::function<void (Params&, float)> f; };
        const UlpLeg legs[] = { { "(4) 1 ulp / 2e-6: width haasDelay amount balance 3 xover 4 mbWidth MM", hm, hmUlp },
                                { "(4) +/-1 ulp under Chorus: rate depth width amount balance",          ch, chUlp },
                                { "(4) +/-1 ulp under Velvet: density width amount balance",            vv, vvUlp } };
        for (const auto& lg : legs)
        {
            std::vector<Lane> L (4);
            for (int k = 0; k < 2; ++k)
            {
                const float d = k == 0 ? 1.0f : -1.0f;
                Lane& t = L[(size_t) (2 * k)];
                Lane& r = L[(size_t) (2 * k + 1)];
                t.pre = r.pre = lg.p;
                t.last = r.last = lastL;
                t.at = [&, d] (int b, AnamorphEngine& e, Params& s) { if (b == U) e.requestDuck(); if (b >= U) lg.f (s, d); };
                r.at = [&, d] (int b, AnamorphEngine& e, Params& s)
                       { if (b == U) e.requestDuck(); if (b >= U) { s.autoGainMatch = true; lg.f (s, d); } probeAt (b, e); };
            }
            runLanes (L, U, false, 6604);
            for (int k = 0; k < 2; ++k)
            {
                const Judged j = judge (L[(size_t) (2 * k + 1)], L[(size_t) (2 * k)], 0.0, U, false, winTo);
                char nm[96]; std::snprintf (nm, sizeof nm, "%s %s", lg.name, k == 0 ? "(+)" : "(-)");
                printLand (nm, j);
                allLand = allLand && lands (j);
                premises = premises && std::abs (j.pubF) >= 3.0;
                probesLive = probesLive && probeOk (L[(size_t) (2 * k + 1)], L[(size_t) (2 * k)], 0.0);
                countArmed (j);
            }
        }
    }
    check (premises, "premise: every LAND leg's published value is >= 3 dB from unity, where the old glide started "
                     "(and the positive-match leg's match is >= +3 dB)");
    check (allLand, "a Level-Match switch that changes only the gain starts at the published value: max|D| <= 0.1 dB over "
                    "0.6 s from the first full-level block, no excursion beyond [pre-switch gain, published] by > 0.2 dB "
                    "anywhere in the switch, residual <= 1e-3");
    check (probesLive, "liveness: Level Match is engaged in every LAND run after its switch (an un-ducked -20 dB "
                       "injection after the window puts the next block at -20 dB within 0.5 dB)");

    // =====================================================================================================
    //  GROUP F -- (5) Case B: the same engage carrying a change the measurement reads. NOT LAND: the fade-in
    //  starts at unity and glides (pending F13(2)). A short pre-roll: the verdict is read at the first
    //  full-level block, and the predict has put the published value >= 3 dB from unity from the first block.
    // =====================================================================================================
    const int UB = (int) std::lround (0.4 * sr / bs);
    bool caseBGlides = true, caseBPremise = true;
    {
        struct Chg { const char* name = nullptr; std::function<void (Params&)> f; };
        const Chg hmChanges[] = {
            { "Drive +0.01 dB",           [] (Params& s) { s.driveDb += 0.01f; } },
            { "Drive +1 ulp",             [] (Params& s) { s.driveDb = std::nextafter (s.driveDb, 100.0f); } },
            { "Width +0.001",             [] (Params& s) { s.width += 0.001f; } },
            { "Mix -0.001",               [] (Params& s) { s.mix -= 0.001f; } },
            { "Mix -1 ulp",               [] (Params& s) { s.mix = std::nextafter (s.mix, 0.0f); } },
            { "algorithm Haas->Velvet",   [] (Params& s) { s.algorithm = Algorithm::Velvet; } },
            { "M/S solo Mid",             [] (Params& s) { s.solo = SoloMode::Mid; } },
            { "msMode on",                [] (Params& s) { s.msMode = true; } },
            { "mbBands 4->3, Multiband on",[] (Params& s) { s.mbBands = 3; } },
            { "Mono Maker on",            [] (Params& s) { s.monoMakerEnable = true; } },
            { "Oversampling Off->2x",     [] (Params& s) { s.oversample = OversampleFactor::x2; } },
            { "Drive 8->10",              [] (Params& s) { s.driveDb = 10.0f; } },
        };
        Params hm = base (Algorithm::Haas, 8.0f); hm.mbEnable = true;
        std::vector<Lane> L;
        L.reserve (64);
        std::vector<juce::String> names;
        const auto addPair = [&] (juce::String name, std::function<void (int, AnamorphEngine&, Params&)> twinAt,
                                  std::function<void (int, AnamorphEngine&, Params&)> runAt, const Params& pre)
        {
            Lane t; t.pre = pre; t.last = UB + kFull; t.at = std::move (twinAt); L.push_back (std::move (t));
            Lane r; r.pre = pre; r.last = UB + kFull; r.at = std::move (runAt);  L.push_back (std::move (r));
            names.push_back (std::move (name));
        };
        // One Case-B pair: the twin makes the change with Level Match off, the run makes it with the engage.
        // (The change is held by pointer: the tables outlive the lanes.)
        const auto addChange = [&] (const Chg& c, bool forced, const Params& pre)
        {
            const auto at = [fn = &c.f, forced, UB] (bool engage)
            {
                const int mode = (forced ? 1 : 0) | (engage ? 2 : 0);
                return [fn, UB, mode] (int b, AnamorphEngine& e, Params& s)
                {
                    if (b == UB && (mode & 1) != 0) e.requestDuck();
                    if (b >= UB) { (*fn) (s); if ((mode & 2) != 0) s.autoGainMatch = true; }
                };
            };
            addPair (juce::String (forced ? "(5) forced + " : "(5) ordinary, same snapshot + ") + c.name, at (false), at (true), pre);
        };
        for (const auto& c : hmChanges)
            for (const bool forced : { true, false })
                addChange (c, forced, hm);
        addPair ("(5) ordinary; Drive 8->10 arrives mid-fade-out",
                 [UB] (int b, AnamorphEngine&, Params& s) { if (b > UB) s.driveDb = 10.0f; },
                 [UB] (int b, AnamorphEngine&, Params& s) { if (b >= UB) s.autoGainMatch = true; if (b > UB) s.driveDb = 10.0f; }, hm);
        addPair ("(5) ordinary; Drive 8->10->8 mid-fade-out, back before the bottom",
                 [UB] (int b, AnamorphEngine&, Params& s) { if (b == UB + 1) s.driveDb = 10.0f; },
                 [UB] (int b, AnamorphEngine&, Params& s) { if (b >= UB) s.autoGainMatch = true; if (b == UB + 1) s.driveDb = 10.0f; },
                 hm);
        // An ordinary engage carrying Drive 8 -> 10 (a dirty duck), upgraded to forced one block into its
        // fade-out (Multiband off). The twin opens the same ordinary duck with an inert band-count move and
        // takes the same upgrade.
        {
            const Params h = base (Algorithm::Haas, 8.0f);
            addPair ("(5) ordinary Drive 8->10 + engage, upgraded to forced at +1",
                     [UB] (int b, AnamorphEngine& e, Params& s) { if (b >= UB) { s.mbBands = 3; s.driveDb = 10.0f; } if (b == UB + 1) e.requestDuck(); },
                     [UB] (int b, AnamorphEngine& e, Params& s) { if (b >= UB) { s.autoGainMatch = true; s.driveDb = 10.0f; } if (b == UB + 1) e.requestDuck(); },
                     h);
        }
        // Discrete edits that change no sample the measurement reads but re-arm it (processingDiffers): Velvet.
        const Params vo = base (Algorithm::Velvet, 8.0f);
        const Chg rearm[] = { { "Haas side under Velvet (re-arms)",      [] (Params& s) { s.haasSide = HaasSide::Right; } },
                              { "mbBands 4->3, Multiband off (re-arms)", [] (Params& s) { s.mbBands = 3; } } };
        for (const auto& c : rearm)
            for (const bool forced : { true, false })
                addChange (c, forced, vo);
        runLanes (L, UB, false, 6605);
        for (size_t k = 0; k < names.size(); ++k)
        {
            const Judged j = judge (L[2 * k + 1], L[2 * k], 0.0, UB, false, UB + kFull);
            std::printf ("  %-58s: pub_F %+6.2f  D_F %+6.3f  phi %.3f  resid %.1e\n", names[k].toRawUTF8(), j.pubF, j.dF, j.phi, j.resid);
            caseBGlides  = caseBGlides && j.ok && j.phi >= 0.5 && j.resid <= 1.0e-3;
            caseBPremise = caseBPremise && std::abs (j.pubF) >= 3.0;
        }
    }
    check (caseBPremise, "premise: every Case-B leg's published value is >= 3 dB from unity");
    check (caseBGlides, "a Level-Match engage that also changes what the measurement reads keeps the glide from unity "
                        "(phi >= 0.5 at the first full-level block; F13(2) pending)");

    // =====================================================================================================
    //  GROUP G -- (11) WHEN the landing happens: right after the bottom block's loudness.process, on the value
    //  that block publishes. An ordinary gain-only engage while the matcher converges fast: Width 0, amount 0,
    //  Drive 0, Multiband off; the input is anti-phase (R = -L: the wet vanishes and the match climbs to its
    //  clamp) until `lead` blocks before the event, then correlated, so the published value still moves
    //  ~0.65 dB across the bottom block itself. The twin opens the same duck with an inert band-count move.
    // =====================================================================================================
    {
        Params w0 = base (Algorithm::Haas, 0.0f); w0.algoAmount = 0.0f; w0.width = 0.0f;
        bool atBottom = true, whenPremise = true, stepSeen = true;
        for (const int lead : { 1, 2 })
        {
            std::vector<Lane> L (2);
            L[0].pre = w0; L[0].last = U + kFull;
            L[0].at  = [U] (int b, AnamorphEngine&, Params& s) { if (b >= U) s.mbBands = 3; };
            L[1].pre = w0; L[1].last = U + kFull;
            L[1].at  = [U] (int b, AnamorphEngine&, Params& s) { if (b >= U) s.autoGainMatch = true; };
            runLanes (L, U, false, 6608, U - lead);
            const int bot = U + kBot;
            const Fit f = fit (L[1], L[0], bot);
            const double pubBot = L[1].pub[(size_t) bot], step = pubBot - L[1].pub[(size_t) bot - 1];
            const double dBot = f.gDb - pubBot;
            const double dNext = fit (L[1], L[0], bot + 1).gDb - L[1].pub[(size_t) bot + 1];
            const double dF = fit (L[1], L[0], U + kFull).gDb - L[1].pub[(size_t) (U + kFull)];
            char nm[96]; std::snprintf (nm, sizeof nm, "(11) ordinary engage, input turns correlated %d blk before", lead);
            std::printf ("  %-58s: pub(bot) %+6.2f  step %+6.3f  D(bot) %+6.3f  resid %.1e | D(bot+1) %+6.3f  D_F %+6.3f\n",
                         nm, pubBot, step, dBot, f.resid, dNext, dF);
            atBottom    = atBottom && f.ok && std::abs (dBot) <= 0.1 && f.resid <= 1.0e-3;
            whenPremise = whenPremise && std::abs (pubBot) >= 3.0;
            stepSeen    = stepSeen && std::abs (step) >= 0.3;
        }
        check (whenPremise, "premise: the (11) engages publish >= 3 dB from unity at the bottom block");
        check (stepSeen, "non-vacuity: the published value moves >= 0.3 dB across the bottom block itself (a landing "
                         "on the previous block's value would miss by more than the 0.1 dB tolerance)");
        check (atBottom, "the landing happens right after the bottom block's measurement: the bottom block plays at the "
                         "value that block publishes (|D| <= 0.1 dB, residual <= 1e-3)");
    }

    // =====================================================================================================
    //  GROUP H -- (8) Level Match on on both sides, forced Drive 0 -> 10: PINS current behaviour (F13(2)).
    // =====================================================================================================
    {
        const Params h0 = base (Algorithm::Haas, 0.0f);
        Params on0 = h0; on0.autoGainMatch = true;
        std::vector<Lane> L (2);
        L[0].pre = h0;  L[0].last = UB + kFull;
        L[0].at  = [UB] (int b, AnamorphEngine& e, Params& s) { if (b >= UB) { if (b == UB) e.requestDuck(); s.driveDb = 10.0f; } };
        L[1].pre = on0; L[1].last = UB + kFull;
        L[1].at  = L[0].at;
        runLanes (L, UB, false, 6606);
        const Judged j = judge (L[1], L[0], 0.0, UB, true, UB + kFull);
        std::printf ("  %-58s: pre %+6.2f  pub_F %+6.2f  D_F %+6.3f  phi %.3f  resid %.1e\n",
                     "(8) Level Match on both sides, forced Drive 0->10", j.gPre, j.pubF, j.dF, j.phi, j.resid);
        check (std::abs (j.pubF - j.gPre) >= 2.0, "premise: the Drive swap moves the published value >= 2 dB");
        check (j.ok && j.phi >= 0.3 && j.resid <= 1.0e-3,
               "Level Match already on: a forced sound change still glides (phi >= 0.3) -- current behaviour, F13(2)");
    }

    // =====================================================================================================
    //  GROUP S -- (10) FIELD SWEEP. Per base and field: a pair of Level-Match-OFF engines sharing one forced
    //  duck, differing only in that field, decides "measurement-inert" by a bit-identical published
    //  trajectory; the engaging engine must land exactly when it is.
    // =====================================================================================================
    {
        struct Row
        {
            const char* name = nullptr;
            std::function<void (Params&)> set;
            bool postTap = false, bypassPre = false, isSwitch = false;
        };
        const auto otherAlgorithm = [] (Params& s) { s.algorithm = s.algorithm == Algorithm::Haas ? Algorithm::Velvet : Algorithm::Haas; };
        const Row rows[] = {
            { "channelMode",     [] (Params& s) { s.channelMode = anamorph::ChannelMode::LeftOnly; }, false, false, false },
            { "monoSum",         [] (Params& s) { s.monoSum = true; },                  false, false, false },
            { "swapLR",          [] (Params& s) { s.swapLR = true; },                   false, false, false },
            { "inputBalance",    [] (Params& s) { s.inputBalance = 0.3f; },             false, false, false },
            { "polarityL",       [] (Params& s) { s.polarityL = true; },                false, false, false },
            { "polarityR",       [] (Params& s) { s.polarityR = true; },                false, false, false },
            { "msMode",          [] (Params& s) { s.msMode = true; },                   false, false, false },
            { "driveDb",         [] (Params& s) { s.driveDb += 1.0f; },                 false, false, false },
            { "algorithm",       otherAlgorithm,                                         false, false, false },
            { "algoAmount",      [] (Params& s) { s.algoAmount += 0.2f; },              false, false, false },
            { "haasDelayMs",     [] (Params& s) { s.haasDelayMs = 18.0f; },             false, false, false },
            { "haasSide",        [] (Params& s) { s.haasSide = HaasSide::Right; },      false, false, false },
            { "velvetDensity",   [] (Params& s) { s.velvetDensity = 0.8f; },            false, false, false },
            { "chorusRate",      [] (Params& s) { s.chorusRate = 2.0f; },               false, false, false },
            { "chorusDepth",     [] (Params& s) { s.chorusDepth = 0.8f; },              false, false, false },
            { "dimMode",         [] (Params& s) { s.dimMode = 3; },                     false, false, false },
            { "width",           [] (Params& s) { s.width += 0.3f; },                   false, false, false },
            { "mbEnable",        [] (Params& s) { s.mbEnable = ! s.mbEnable; },         false, false, false },
            { "mbBands",         [] (Params& s) { s.mbBands = s.mbBands == 3 ? 2 : 3; }, false, false, false },
            { "mbSolo",          [] (Params& s) { s.mbSolo = 0x2; },                    true,  false, false },
            { "mbFreqLow",       [] (Params& s) { s.mbFreqLow = 300.0f; },              false, false, false },
            { "mbFreqMid",       [] (Params& s) { s.mbFreqMid = 1200.0f; },             false, false, false },
            { "mbFreqHigh",      [] (Params& s) { s.mbFreqHigh = 5000.0f; },            false, false, false },
            { "mbWidthLow",      [] (Params& s) { s.mbWidthLow = 1.6f; },               false, false, false },
            { "mbWidthMid",      [] (Params& s) { s.mbWidthMid = 1.6f; },               false, false, false },
            { "mbWidthHiMid",    [] (Params& s) { s.mbWidthHiMid = 1.6f; },             false, false, false },
            { "mbWidthHigh",     [] (Params& s) { s.mbWidthHigh = 1.6f; },              false, false, false },
            { "monoMakerEnable", [] (Params& s) { s.monoMakerEnable = ! s.monoMakerEnable; }, false, false, false },
            { "monoMakerFreq",   [] (Params& s) { s.monoMakerFreq = 250.0f; },          false, false, false },
            { "mix",             [] (Params& s) { s.mix = 0.8f; },                      false, false, false },
            { "outputGainDb",    [] (Params& s) { s.outputGainDb = -6.0f; },            true,  false, false },
            { "outputBalance",   [] (Params& s) { s.outputBalance = 0.3f; },            true,  false, false },
            { "autoGainMatch",   [] (Params&) {},                                        true,  false, true  },
            { "solo",            [] (Params& s) { s.solo = SoloMode::Mid; },            false, false, false },
            { "oversample",      [] (Params& s) { s.oversample = OversampleFactor::x2; }, false, false, false },
            { "bypass",          [] (Params& s) { s.bypass = false; },                  true,  true,  false },
        };
        constexpr int nRows = (int) (sizeof (rows) / sizeof (rows[0]));
        check (nRows == 36, "premise: the sweep has one row per EngineParameters member (36, the binding above)");

        const int US = (int) std::lround (0.3 * sr / bs), lastS = US + kFull + 8;
        Params sh = base (Algorithm::Haas, 8.0f);
        Params sc = base (Algorithm::Chorus, 10.0f); sc.mbEnable = true; sc.monoMakerEnable = true;
        Params sm = base (Algorithm::Haas, 8.0f);    sm.mbEnable = true; sm.mbBands = 1;
        Params sm2 = sm; sm2.mbBands = 2;
        Params sm3 = sm; sm3.mbBands = 3;
        // H and C are the two states the contract names; M, M2 and M3 (Multiband on with ONE, TWO and THREE
        // bands) run the Multiband rows only, where the band-count guards decide: with one band no crossover
        // and no upper width is heard, with two the low crossover and the mid width are, with three the mid
        // crossover and the hi-mid width too -- so each guard's threshold, not only its presence, is pinned.
        struct SweepBase { const char* name = nullptr; Params p; bool mbRowsOnly = false; int minInert = 0, minLive = 0; };
        const SweepBase bases[] = { { "H", sh, false, 3, 15 }, { "C", sc, false, 3, 15 }, { "M", sm, true, 3, 2 },
                                    { "M2", sm2, true, 3, 2 }, { "M3", sm3, true, 2, 2 } };
        constexpr int nBases = (int) (sizeof (bases) / sizeof (bases[0]));
        const auto mbRow = [] (const char* n) { return std::strncmp (n, "mb", 2) == 0; };
        bool iff = true, postTapOk = true, sweepPremise = true, classesSeen = true;
        char verdict[nBases][nRows][48] = {};
        for (int bi = 0; bi < nBases; ++bi)
        {
            std::vector<Lane> L;
            L.reserve (2 * nRows + 4);
            const auto lane = [&L, lastS] (Params pre, std::function<void (int, AnamorphEngine&, Params&)> fn)
            { Lane ln; ln.pre = pre; ln.last = lastS; ln.at = std::move (fn); L.push_back (std::move (ln)); return (int) L.size() - 1; };
            const auto forcedAt = [US] (const std::function<void (Params&)>* fn, bool on)   // fn: a row's, held by pointer
            {
                return [fn, US, on] (int b, AnamorphEngine& e, Params& s)
                { if (b >= US) { if (b == US) e.requestDuck(); if (fn != nullptr) (*fn) (s); if (on) s.autoGainMatch = true; } };
            };
            const Params pb = bases[bi].p;
            Params pbBy = pb; pbBy.bypass = true;
            const int ref   = lane (pb,   forcedAt (nullptr, false));
            const int refBy = lane (pbBy, forcedAt (nullptr, false));
            int twinOf[nRows] = {}, runOf[nRows] = {}, refOf[nRows] = {};
            for (int k = 0; k < nRows; ++k)
            {
                if (bases[bi].mbRowsOnly && ! mbRow (rows[k].name)) continue;
                const Params pre = rows[k].bypassPre ? pbBy : pb;
                refOf[k]  = rows[k].bypassPre ? refBy : ref;
                twinOf[k] = rows[k].isSwitch ? ref : lane (pre, forcedAt (&rows[k].set, false));
                runOf[k]  = lane (pre, forcedAt (&rows[k].set, true));
            }
            runLanes (L, US, false, 6607);
            int inertN = 0, liveN = 0;
            double minPub = 1.0e9;
            for (int k = 0; k < nRows; ++k)
            {
                char* v = verdict[bi][k];
                if (bases[bi].mbRowsOnly && ! mbRow (rows[k].name)) { std::snprintf (v, sizeof verdict[bi][k], "-"); continue; }
                const Lane& r  = L[(size_t) runOf[k]];
                const Lane& t  = L[(size_t) twinOf[k]];
                const Lane& rf = L[(size_t) refOf[k]];
                // the oracle: the field's own change, Level Match off on both sides (for the switch row, the
                // switch itself against the lane that never engages)
                const Lane& changed = rows[k].isSwitch ? r : t;
                bool inert = true; double dPub = 0.0;
                for (size_t b = 0; b < rf.pub.size(); ++b)
                {
                    if (std::memcmp (&changed.pub[b], &rf.pub[b], sizeof (float)) != 0) inert = false;
                    dPub = juce::jmax (dPub, (double) std::abs (changed.pub[b] - rf.pub[b]));
                }
                Params dst = rows[k].bypassPre ? pbBy : pb; rows[k].set (dst);        // the twin's Output Gain
                const Judged j = judge (r, t, (double) dst.outputGainDb, US, false, lastS);
                const bool landed = j.ok && j.phi < 0.25;
                sweepPremise = sweepPremise && std::abs (j.pubF) >= 3.0 && j.ok && j.resid <= 1.0e-3;
                if (rows[k].postTap)
                    postTapOk = postTapOk && landed && (inert || (bases[bi].p.mbEnable && dPub <= 0.0064));
                else
                {
                    iff = iff && (landed == inert);
                    (inert ? inertN : liveN) += 1;
                }
                if (landed) countArmed (j);
                const char* how = landed ? "LANDS" : "glide";
                if (inert) std::snprintf (v, sizeof verdict[bi][k], "inert        %-5s phi %6.3f", how, j.phi);
                else       std::snprintf (v, sizeof verdict[bi][k], "moves %.0e %-5s phi %6.3f", dPub, how, j.phi);
                minPub = juce::jmin (minPub, std::abs (j.pubF));
            }
            classesSeen = classesSeen && inertN >= bases[bi].minInert && liveN >= bases[bi].minLive;
            std::printf ("  (10) sweep base %s: %d inert and %d measurement-reading real changes (post-tap rows apart); "
                         "min |pub_F| %.2f dB\n", bases[bi].name, inertN, liveN, minPub);
        }
        for (int k = 0; k < nRows; ++k)
            std::printf ("  (10) %-15s H: %s | C: %s | M: %s | M2: %s | M3: %s\n", rows[k].name, verdict[0][k], verdict[1][k],
                         verdict[2][k], verdict[3][k], verdict[4][k]);
        check (sweepPremise, "premise: every sweep engage publishes >= 3 dB from unity, and its twin explains it (residual <= 1e-3)");
        check (classesSeen, "non-vacuity: the sweep meets both classes in every base (H and C: >= 3 inert, >= 15 "
                            "measurement-reading; M and M2: >= 3 inert, >= 2 reading; M3: >= 2 inert, >= 2 reading)");
        check (iff, "field sweep: for every real change the engaging engine lands IFF a Level-Match-off engine pair shows "
                    "the change leaves the published trajectory bit-identical");
        check (postTapOk, "field sweep: the post-tap fields (Output Gain/Balance, Band Solo, Bypass, the switch) always land; "
                          "their published effect is nil, or the documented H4 reference switch (<= 0.0064 dB) with Multiband on");
    }

    // ---- the allocation guard, around every event-to-full-level window above ----------------------------
    std::printf ("  allocation guard: %d armed setParameters+process calls (event to first full-level block), "
                 "%d of %d landings observed inside them; worst per call: new=%ld malloc=%ld\n",
                 armedCalls, armedLandings, landLegs, worstNew, worstMalloc);
    check (landLegs > 0 && armedLandings == landLegs,
           "liveness: every landing was observed inside the armed blocks (|D| <= 0.1 dB at the first full-level block, "
           "itself armed like every block back to the event)");
    if (guardLive)
    {
        check (worstNew == 0, "no operator-new allocation while a Level-Match landing is processed");
        if (guard.mallocLive)
            check (worstMalloc == 0, "no malloc-family allocation while a Level-Match landing is processed");
    }
}

// ---------------------------------------------------------------------------
//  Test 67 -- AN A/B INJECTION RE-ARMS THE LEVEL-MATCH ANALYSIS WHEN THE SLOTS DIFFER IN WHAT IT READS,
//  AND A SAME-RATE RE-PREPARE KEEPS THE PUBLISHED RESULT (ADR-0007, Amendment of 2026-09-24, F13(2) Q1 and
//  Q5; KI-030. State test 131 is the production-path half.)
//
//  THE CLAIM. LoudnessMatch has an ANALYSIS half (the K-weighting biquads and the two 0.4 s integrators) and a
//  RESULT half (displayedGainDb, prevPredictedGainDb, the published value); softReset() clears the first and
//  keeps the second.
//   Q1  An A/B injection consumed at a duck bottom -- the forced consumer, or the defensive one when it meets
//       an ordinary bottom -- re-arms the analysis iff the switch changes something the measurement reads
//       (measurementInputsDiffer, or a live change during an ordinary duck's fade-out: duckMeasDirty). The
//       injected value then plays on the destination slot's own sound instead of being dragged back toward
//       the source slot's for seconds. Slots that differ only after the tap, in a guarded-out field, or not at
//       all keep the converged analysis; an injection consumed without a bottom never re-arms;
//       processingDiffers still re-arms a discrete path change, injection or not.
//   Q2  UNCHANGED: a forced swap WITHOUT an injection (preset, undo, redo) does not re-arm and tracks the
//       same edit made live.
//   Q5  prepare() at the SAME rate, for a snapshot whose measurement inputs primeParameters() saw unchanged,
//       keeps the result bit-exact (any block size) and re-arms the analysis; with Level Match on, prepare()
//       starts the applied gain on it (Test 68 pins that for quiet audio). A new rate, a primed snapshot that
//       moves a measurement input, and a first prepare still flush (published 0 dB, then the predict floor).
//
//  THE PROBE (State tests 118 / 120's technique). A re-armed analysis starts from near-zero integrators, so
//  on SILENCE the matcher's gate closes at once and the published value HOLDS exactly; a stale one keeps the
//  gate open for seconds (the 0.4 s integrators decay from the old energy, their ratio -- the old sound's
//  target -- unchanged) and the value MOVES toward it. Each probe injects 6 dB away from the published value;
//  HOLD is a move < 1e-6 dB over 0.3 s of silence (0 measured), MOVE > 1 dB (1.88-4.67 measured). The silence
//  starts 8 blocks (43 ms) before the event, so no processing tail (Haas <= 35 ms) reaches the tap after the
//  bottom even on an ordinary duck, which clears nothing. Every probe first proves its injection was consumed
//  in the block judged: the published value is within 1 dB of it there and >= 4 dB from it one block earlier.
//  TRANSIENT ACCURACY: the run against a FRESH engine prepared at the destination state and fed the identical
//  seeded input from sample 0 (converged), as published difference D(t) and as a per-block least-squares
//  gain of run output against fresh output (residual <= 1e-9 measured), for 3 s after the event.
//
//  LEGS (48 kHz / 256, Haas / Amount 0.5 / Width 1.3 / Level Match on unless stated; bottom at event + 2):
//   (1) THE PREDICATE, per field: an A/B-shaped forced swap (requestDuck, the slot's snapshot, an injection
//       6 dB below the published value) changing ONE EngineParameters member (all 36; the structured binding
//       makes the count a compile error to get wrong) under four bases -- H (Haas, Multiband off, Mono Maker
//       off), V (Velvet, Multiband 2 bands, Mono Maker on), C (Chorus, Multiband 4 bands) and D (Dimension D,
//       Multiband 1 band, Mono Maker on) -- plus the identical slot. HOLD iff the field is a measurement input
//       for that base ("meas") or a processingDiffers path change ("m+p" / "path": these hold before and
//       after the change -- haasSide off Haas and mbBands with Multiband off are "path" only); the post-tap
//       fields ("post": Output Gain / Balance, Band Solo, Bypass, the Level Match switch), the guarded-out
//       ones ("off": Haas fields off Haas, Chorus fields off Chorus -- Dimension D included --, Velvet density
//       off Velvet, dimMode off Dimension D, a crossover or band width with too few bands or Multiband off,
//       Mono Maker Freq with it off) and the identical slot MOVE. (1b) The re-arm clears the ANALYSIS only:
//       every row above injects below the predict floor, so an injection ABOVE it (-0.5 dB against Drive 8's
//       -4.06, slots differing in Width) must be published exactly at the bottom and hold -- a re-arm through
//       reset() loses prevPredictedGainDb and the next predict floors the slot's gain.
//   (2) Continuous-only A/B, Drive 2 -> 8 and 8 -> 2, injecting what a fresh destination engine converged to
//       (the slot's remembered gain, -6.08 / -2.78 dB): published D(t) and output gain within 0.1 dB of the
//       fresh engine over 3 s (0.018 / 0.015 and 0.017 / 0.012 dB measured); its silence twin holds the
//       injected value bit-exact from the bottom block.
//   (3) Drive 0 <-> 10: the same A/B (0.019 / 0.014 dB); and the forced swap WITHOUT an injection (the undo /
//       preset shape), which must NOT re-arm -- its silence probe moves 3.13 / 1.88 dB (on the rise the predict
//       floor at the bottom opens the gap; on the fall, where the predict leaves the value alone, an un-ducked
//       injection one block before the event displaces it first) -- and must track the same edit made live:
//       published trajectories within 0.5 dB from the first full-level block for 3 s (0.348 / 0.167 measured;
//       at the bottom block itself 0.742 on the rise, where the live edit's predict fired two blocks earlier).
//       Both still lag the fresh engine by the measure's design (+4.0 / -7.1 dB at 0.1 s; Q2, pinned as is).
//   (4) Ordinary ducks, opened by the Level Match switch (neither a path nor a measurement change): upgraded
//       to forced by an A/B one block in, the injection re-arms when a Drive edit went live DURING the
//       fade-out (duckMeasDirty; p already carries it, so the bottom's own comparison sees nothing), and when
//       the upgrading snapshot carries it; not when nothing the measurement reads moved. The DEFENSIVE
//       consumer: an injection meeting an ordinary bottom re-arms iff that duck changed a measurement input;
//       the same injection one block before or after the bottom, or with no duck at all, never does.
//   (5) Re-prepare, engine level (the processor's primeParameters -> prepare -> setParameters sequence):
//       the audible run at the same rate and block keeps -6.0838 dB bit-exact; its applied gain -- read
//       against a Level-Match-off twin re-prepared alike (residual 2e-10) -- is -6.080 dB in the first block
//       against the converged fresh engine's -6.084, and applied, published and output stay within 0.1 dB of
//       it for 3 s (0.025 / 0.019 / 0.017 measured; output from block 3, once the 12 ms Haas line refilled).
//       Silence probes, displaced 6 dB UP (above the Drive-8 predict floor, so a keep that lost
//       prevPredictedGainDb would floor it): same rate and block, block 256 -> 512, and a primed snapshot
//       differing only in Output Gain KEEP the value bit-exact through the first block after prepare; from
//       there silence holds (re-armed) where the same run without the re-prepare moves 4.60 dB. 48 -> 44.1 kHz
//       and a primed Drive 8 -> 10 FLUSH: exactly 0 dB after prepare, then a trajectory bit-identical to a
//       fresh engine first-prepared there on the same input (which publishes exactly 0 dB too). (d) A FIRST
//       prepare at 44.1 kHz -- the rate an unprepared engine reads -- from the default snapshot, the sound set
//       afterwards, measures within 0.3 dB of an engine primed with the sound after 2 s (0.016 measured): only
//       "prepared before" (os2) tells it from a same-rate keep, which would skip loudness.prepare and leave the
//       matcher publishing the predict floor for ever.
//  The allocation guard (tests/AllocationGuard.h, Test 38's pattern) is armed around setParameters + process
//  from every A/B / injection event to the block after its bottom in legs (1)-(4) (664 calls, zero
//  allocations measured).
//
//  MEASURED BEFORE THE CHANGE (engine daa6809, this test): 13 of the 37 checks fail -- the "meas" rows of (1)
//  move 2.72-4.67 dB and (1b)'s value drifts; the (2) / (3) A/B runs sit up to 1.33 / 1.63 dB (Drive 2 -> 8 / 8 -> 2) and 3.46 / 4.92
//  dB (0 -> 10 / 10 -> 0) off the fresh engine, their silence twins drift 1.8-5.8 dB off the injected value;
//  the (4) upgrade and defensive legs move; and every re-prepare flushed (the audible run's first block played
//  -4.066 dB against -6.084, 2.02 dB off; each keep read 0 dB, then the predict floor). The path rows, the MOVE rows, the
//  forced-without-injection legs of (3), the controls of (4), (5)'s flushes and its re-arm check, and every
//  premise pass on both engines by design.
//  ENGINE VARIANTS REJECTED (each built from this tree and run through this test): a re-arm at every
//  injection (P1a: (1)'s post-tap / guarded-out / identical rows, (4)'s controls, and (3)'s fall probe, whose
//  displacing injection then re-arms); an injection rule without duckMeasDirty ((4) upgrade and defensive);
//  a forced bottom that re-arms without an injection (P1: (3) no-re-arm and tracks-the-live-edit); a
//  fallback consumer that always re-arms ((3), (4)); no defensive re-arm ((4)); a keep across a rate change
//  and a keep that ignores the primed snapshot ((5) flushes); a keep through loudness.reset() plus a restored
//  published value, which loses prevPredictedGainDb, and a keep that does not re-arm ((5) keeps, re-arm); an
//  injection re-arm through loudness.reset() ((1b)); a keep decided without "prepared before" ((5)(d)).
static void testLevelMatchAbRearmAndSameRateReprepare()
{
    std::printf ("Test 67: an A/B injection re-arms the Level-Match analysis when the slots differ in what it reads; "
                 "a same-rate re-prepare keeps the result (ADR-0007, F13(2))\n");
    juce::ScopedNoDenormals noDenormals;

    using anamorph::AnamorphEngine;
    using anamorph::Algorithm;
    using anamorph::HaasSide;
    using anamorph::OversampleFactor;
    using anamorph::SoloMode;
    using Params = anamorph::EngineParameters;
    constexpr double sr = 48000.0;
    constexpr int    bs = 256, nch = 2, blk = bs * nch;

    // THE DUCK'S TIMING, from its documented lengths (~6 ms out, ~28 ms in; Test 66 re-derives both from the
    // engine's output): the silent bottom at event + 2, the first full-level block at event + 8.
    const int fadeOut = (int) std::lround (0.006 * sr), fadeIn = (int) std::lround (0.028 * sr);
    const int kBot  = fadeOut / bs + 1;
    const int kFull = kBot + (fadeIn + bs - 1) / bs;
    const int sec   = (int) std::lround (sr / bs);                 // blocks per second

    // ONE FIELD COUNT, CHECKED BY THE COMPILER (Test 66's binding): leg (1) needs a row for every member.
    {
        const Params probe;
        [[maybe_unused]] const auto& [f01, f02, f03, f04, f05, f06, f07, f08, f09, f10, f11, f12,
                                      f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24,
                                      f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36] = probe;
    }

    const auto guard = anamorph::testing::selfCheck();
    const bool guardLive = guard.newLive || guard.mallocLive;
    if (! guardLive)
        std::printf ("::warning::the allocation guard is compiled out in this build -- Test 67's re-arms are "
                     "NOT allocation-checked by it in this run (RealtimeSanitizer, where present, covers them).\n");

    // ---- one seeded input stream: block b is the same samples in every lane that reads it ----------------
    const int E  = 3 * sec, post = 3 * sec;                          // the long lanes: 3 s in, 3 s after
    const int streamBlocks = E + post + 2;
    std::vector<float> stream ((size_t) streamBlocks * blk);
    {
        juce::Random rng (6701);
        for (size_t i = 0; i < stream.size(); i += 2)
        {
            const float v = rng.nextFloat() - 0.5f, w = rng.nextFloat() - 0.5f;
            stream[i] = v;
            stream[i + 1] = 0.6f * v + 0.2f * w;
        }
    }

    struct Lane
    {
        Params pre;                                                  // the snapshot from block 0
        std::function<void (int, AnamorphEngine&, Params&)> at;      // per block, before setParameters
        int    blocks = 0, silentFrom = 1 << 30, offset = 0;             // 1 << 30: never
        int    keepFrom = 1 << 30, armFrom = 1 << 30, armTo = -1;
        double rate = 48000.0;
        float  pub0 = 0.0f;                                          // published right after the first prepare
        std::unique_ptr<AnamorphEngine> e;
        std::vector<float> pub, y;                                   // published dB per block; output from keepFrom
    };

    long worstNew = 0, worstMalloc = 0;
    int  armedCalls = 0;
    const auto run = [&] (Lane& ln)
    {
        ln.e = std::make_unique<AnamorphEngine>();                   // heap: Test 59's note (1 MB-stack lane)
        ln.e->primeParameters (ln.pre);
        ln.e->prepare (ln.rate, bs);
        ln.e->setParameters (ln.pre);
        ln.pub0 = ln.e->getMatchGainDb();
        ln.pub.assign ((size_t) ln.blocks, 0.0f);
        ln.y.assign (ln.keepFrom < ln.blocks ? (size_t) (ln.blocks - ln.keepFrom) * blk : 0, 0.0f);
        juce::AudioBuffer<float> buf (nch, bs);
        for (int b = 0; b < ln.blocks; ++b)
        {
            Params snap = ln.pre;
            if (ln.at) ln.at (b, *ln.e, snap);
            const float* src = stream.data() + (size_t) (b + ln.offset) * blk;
            const bool quiet = b >= ln.silentFrom;
            for (int i = 0; i < bs; ++i)
            {
                buf.setSample (0, i, quiet ? 0.0f : src[2 * i]);
                buf.setSample (1, i, quiet ? 0.0f : src[2 * i + 1]);
            }
            if (b >= ln.armFrom && b <= ln.armTo)
            {
                anamorph::testing::resetCounts();
                {
                    anamorph::testing::Armed arm;
                    ln.e->setParameters (snap);
                    ln.e->process (buf);
                }
                ++armedCalls;
                worstNew    = juce::jmax (worstNew,    anamorph::testing::newCount.load());
                worstMalloc = juce::jmax (worstMalloc, anamorph::testing::mallocCount.load());
            }
            else
            {
                ln.e->setParameters (snap);
                ln.e->process (buf);
            }
            ln.pub[(size_t) b] = ln.e->getMatchGainDb();
            if (b >= ln.keepFrom)
            {
                float* out = ln.y.data() + (size_t) (b - ln.keepFrom) * blk;
                for (int i = 0; i < bs; ++i) { out[2 * i] = buf.getSample (0, i); out[2 * i + 1] = buf.getSample (1, i); }
            }
        }
    };

    // The largest move of the published value over blocks (from, to], relative to block `from`.
    const auto moveAfter = [] (const Lane& ln, int from, int to)
    {
        double m = 0.0;
        for (int b = from + 1; b <= to; ++b)
            m = juce::jmax (m, std::abs ((double) ln.pub[(size_t) b] - (double) ln.pub[(size_t) from]));
        return m;
    };
    // Per-block least-squares gain of `r` against `t` (both recorded at block b): 20 log10 g^ and the residual
    // a pure gain leaves, normalised by the run's energy.
    struct Fit { double gDb = 0.0, resid = 1.0; bool ok = false; };
    const auto fit = [&] (const Lane& r, const Lane& t, int b) -> Fit
    {
        const float* x = r.y.data() + (size_t) (b - r.keepFrom) * blk;
        const float* z = t.y.data() + (size_t) (b - t.keepFrom) * blk;
        double num = 0.0, den = 0.0, ex = 0.0;
        for (int i = 0; i < blk; ++i) { num += (double) x[i] * z[i]; den += (double) z[i] * z[i]; ex += (double) x[i] * x[i]; }
        if (! (den > 1.0e-20 && ex > 1.0e-20)) return {};
        const double g = num / den;
        double res = 0.0;
        for (int i = 0; i < blk; ++i) { const double d = (double) x[i] - g * z[i]; res += d * d; }
        return { 20.0 * std::log10 (juce::jmax (1.0e-12, std::abs (g))), res / ex, true };
    };

    const auto base = [] (Algorithm a, float drive)
    {
        Params p;
        p.algorithm = a; p.algoAmount = 0.5f; p.width = 1.3f; p.driveDb = drive; p.autoGainMatch = true;
        return p;
    };

    // The short probe lanes: 0.5 s of audio, silence from S1, the event at E1, judged over 0.3 s after it.
    const int E1 = sec / 2, S1 = E1 - 8, B1 = E1 + kBot, P1 = B1 + (int) std::lround (0.3 * sr / bs);
    constexpr double kHold = 1.0e-6, kMove = 1.0, kInjOff = 6.0;
    // A probe's verdict. `ib`: the block whose process consumed the injection `v`.
    struct Probe { double v = 0.0, atInj = 0.0, before = 0.0, move = 0.0; };
    const auto probeOf = [&] (const Lane& ln, double v, int ib)
    {
        Probe q;
        q.v      = v;
        q.atInj  = std::abs ((double) ln.pub[(size_t) ib] - v);
        q.before = std::abs ((double) ln.pub[(size_t) ib - 1] - v);
        q.move   = moveAfter (ln, ib, P1);
        return q;
    };
    const auto consumed = [] (const Probe& q) { return q.atInj <= 1.0 && q.before >= 4.0; };

    // =====================================================================================================
    //  LEG (1) -- THE PREDICATE, per field, under four bases
    // =====================================================================================================
    {
        enum : int { kOff = 0, kMeas = 1, kPath = 2, kPost = 4 };
        struct Row
        {
            const char* name = nullptr;
            void (*set) (Params&) = nullptr;
            int  (*kind) (const Params&) = nullptr;     // for the base: kMeas / kPath / kPost / kOff (guarded out)
        };
        static const Row rows[] = {
            { "channelMode",     [] (Params& s) { s.channelMode = anamorph::ChannelMode::LeftOnly; },
                                 [] (const Params&) { return kMeas | kPath; } },
            { "monoSum",         [] (Params& s) { s.monoSum = true; },               [] (const Params&) { return kMeas | kPath; } },
            { "swapLR",          [] (Params& s) { s.swapLR = true; },                [] (const Params&) { return kMeas | kPath; } },
            { "inputBalance",    [] (Params& s) { s.inputBalance = 0.3f; },          [] (const Params&) { return (int) kMeas; } },
            { "polarityL",       [] (Params& s) { s.polarityL = true; },             [] (const Params&) { return (int) kMeas; } },
            { "polarityR",       [] (Params& s) { s.polarityR = true; },             [] (const Params&) { return (int) kMeas; } },
            { "msMode",          [] (Params& s) { s.msMode = true; },                [] (const Params&) { return kMeas | kPath; } },
            { "driveDb",         [] (Params& s) { s.driveDb += 1.0f; },              [] (const Params&) { return (int) kMeas; } },
            { "algorithm",       [] (Params& s) { s.algorithm = s.algorithm == Algorithm::Haas ? Algorithm::Velvet : Algorithm::Haas; },
                                                                                      [] (const Params&) { return kMeas | kPath; } },
            { "algoAmount",      [] (Params& s) { s.algoAmount += 0.2f; },           [] (const Params&) { return (int) kMeas; } },
            { "haasDelayMs",     [] (Params& s) { s.haasDelayMs = 18.0f; },
                                 [] (const Params& b) { return b.algorithm == Algorithm::Haas ? (int) kMeas : (int) kOff; } },
            { "haasSide",        [] (Params& s) { s.haasSide = HaasSide::Right; },
                                 [] (const Params& b) { return b.algorithm == Algorithm::Haas ? kMeas | kPath : (int) kPath; } },
            { "velvetDensity",   [] (Params& s) { s.velvetDensity = 0.8f; },
                                 [] (const Params& b) { return b.algorithm == Algorithm::Velvet ? (int) kMeas : (int) kOff; } },
            { "chorusRate",      [] (Params& s) { s.chorusRate = 2.0f; },
                                 [] (const Params& b) { return b.algorithm == Algorithm::Chorus ? (int) kMeas : (int) kOff; } },
            { "chorusDepth",     [] (Params& s) { s.chorusDepth = 0.8f; },
                                 [] (const Params& b) { return b.algorithm == Algorithm::Chorus ? (int) kMeas : (int) kOff; } },
            { "dimMode",         [] (Params& s) { s.dimMode = 3; },
                                 [] (const Params& b) { return b.algorithm == Algorithm::DimensionD ? kMeas | kPath : (int) kOff; } },
            { "width",           [] (Params& s) { s.width += 0.3f; },                [] (const Params&) { return (int) kMeas; } },
            { "mbEnable",        [] (Params& s) { s.mbEnable = ! s.mbEnable; },      [] (const Params&) { return kMeas | kPath; } },
            { "mbBands",         [] (Params& s) { s.mbBands = s.mbBands == 3 ? 2 : 3; },
                                 [] (const Params& b) { return b.mbEnable ? kMeas | kPath : (int) kPath; } },
            { "mbSolo",          [] (Params& s) { s.mbSolo = 0x2; },                 [] (const Params&) { return (int) kPost; } },
            { "mbFreqLow",       [] (Params& s) { s.mbFreqLow = 300.0f; },
                                 [] (const Params& b) { return b.mbEnable && b.mbBands >= 2 ? (int) kMeas : (int) kOff; } },
            { "mbFreqMid",       [] (Params& s) { s.mbFreqMid = 1200.0f; },
                                 [] (const Params& b) { return b.mbEnable && b.mbBands >= 3 ? (int) kMeas : (int) kOff; } },
            { "mbFreqHigh",      [] (Params& s) { s.mbFreqHigh = 5000.0f; },
                                 [] (const Params& b) { return b.mbEnable && b.mbBands >= 4 ? (int) kMeas : (int) kOff; } },
            { "mbWidthLow",      [] (Params& s) { s.mbWidthLow = 1.6f; },
                                 [] (const Params& b) { return b.mbEnable ? (int) kMeas : (int) kOff; } },
            { "mbWidthMid",      [] (Params& s) { s.mbWidthMid = 1.6f; },
                                 [] (const Params& b) { return b.mbEnable && b.mbBands >= 2 ? (int) kMeas : (int) kOff; } },
            { "mbWidthHiMid",    [] (Params& s) { s.mbWidthHiMid = 1.6f; },
                                 [] (const Params& b) { return b.mbEnable && b.mbBands >= 3 ? (int) kMeas : (int) kOff; } },
            { "mbWidthHigh",     [] (Params& s) { s.mbWidthHigh = 1.6f; },
                                 [] (const Params& b) { return b.mbEnable && b.mbBands >= 4 ? (int) kMeas : (int) kOff; } },
            { "monoMakerEnable", [] (Params& s) { s.monoMakerEnable = ! s.monoMakerEnable; },
                                 [] (const Params&) { return kMeas | kPath; } },
            { "monoMakerFreq",   [] (Params& s) { s.monoMakerFreq = 250.0f; },
                                 [] (const Params& b) { return b.monoMakerEnable ? (int) kMeas : (int) kOff; } },
            { "mix",             [] (Params& s) { s.mix = 0.8f; },                   [] (const Params&) { return (int) kMeas; } },
            { "outputGainDb",    [] (Params& s) { s.outputGainDb = -6.0f; },         [] (const Params&) { return (int) kPost; } },
            { "outputBalance",   [] (Params& s) { s.outputBalance = 0.3f; },         [] (const Params&) { return (int) kPost; } },
            { "autoGainMatch",   [] (Params& s) { s.autoGainMatch = false; },        [] (const Params&) { return (int) kPost; } },
            { "solo",            [] (Params& s) { s.solo = SoloMode::Mid; },         [] (const Params&) { return kMeas | kPath; } },
            { "oversample",      [] (Params& s) { s.oversample = OversampleFactor::x2; }, [] (const Params&) { return kMeas | kPath; } },
            { "bypass",          [] (Params& s) { s.bypass = true; },                [] (const Params&) { return (int) kPost; } },
        };
        constexpr int nRows = (int) (sizeof (rows) / sizeof (rows[0]));
        check (nRows == 36, "premise: leg (1) has one row per EngineParameters member (36, the binding above)");

        Params bH = base (Algorithm::Haas, 8.0f);
        Params bV = base (Algorithm::Velvet, 8.0f);     bV.mbEnable = true; bV.mbBands = 2; bV.monoMakerEnable = true;
        Params bC = base (Algorithm::Chorus, 8.0f);     bC.mbEnable = true;
        Params bD = base (Algorithm::DimensionD, 8.0f); bD.mbEnable = true; bD.mbBands = 1; bD.monoMakerEnable = true;
        const Params* const bases[] = { &bH, &bV, &bC, &bD };
        const char* const baseNames[] = { "H", "V", "C", "D" };
        constexpr int nBases = 4;

        // each probe's move, [base][row]; row nRows is the identical slot
        std::vector<double> moves ((size_t) nBases * (nRows + 1));
        bool allConsumed = true, measHold = true, pathHold = true, restMove = true, classes = true;
        for (int bi = 0; bi < nBases; ++bi)
        {
            int nMeasOnly = 0, nPathOnly = 0, nOff = 0;
            for (int k = 0; k <= nRows; ++k)
            {
                const Row* row = k < nRows ? &rows[k] : nullptr;
                double v = 0.0;
                Lane ln;
                ln.pre = *bases[bi];
                ln.blocks = P1 + 1; ln.silentFrom = S1; ln.armFrom = E1; ln.armTo = B1 + 1;
                ln.at = [&] (int b, AnamorphEngine& e, Params& s)
                {
                    if (b >= E1 && row != nullptr) row->set (s);
                    if (b == E1) { v = e.getMatchGainDb() - kInjOff; e.requestDuck(); e.injectMatchGainDb ((float) v); }
                };
                run (ln);
                const Probe q = probeOf (ln, v, B1);
                moves[(size_t) (bi * (nRows + 1) + k)] = q.move;
                allConsumed = allConsumed && consumed (q);
                const int kind = row != nullptr ? row->kind (*bases[bi]) : (int) kOff;
                if ((kind & kPath) != 0)      pathHold = pathHold && q.move < kHold;
                else if ((kind & kMeas) != 0) measHold = measHold && q.move < kHold;
                else                          restMove = restMove && q.move > kMove;
                nMeasOnly += kind == kMeas ? 1 : 0;
                nPathOnly += kind == kPath ? 1 : 0;
                nOff      += kind == kOff && row != nullptr ? 1 : 0;
            }
            classes = classes && nMeasOnly >= 8 && nPathOnly == 1 && nOff >= 4;
        }
        const auto tag = [] (int kind)
        {
            return kind == (kMeas | kPath) ? "m+p " : kind == kMeas ? "meas" : kind == kPath ? "path" : kind == kPost ? "post" : "off ";
        };
        for (int k = 0; k <= nRows; ++k)
        {
            std::printf ("  (1) %-15s", k < nRows ? rows[k].name : "identical slot");
            for (int bi = 0; bi < nBases; ++bi)
            {
                const double mv = moves[(size_t) (bi * (nRows + 1) + k)];
                const int kind = k < nRows ? rows[k].kind (*bases[bi]) : (int) kOff;
                std::printf (" | %s %s %4.2f %s", baseNames[bi], k < nRows ? tag (kind) : "--  ", mv,
                             mv < kHold ? "HOLD" : "move");
            }
            std::printf ("\n");
        }
        double worstHold = 0.0, leastMove = 1.0e9;
        for (int bi = 0; bi < nBases; ++bi)
            for (int k = 0; k <= nRows; ++k)
            {
                const double m = moves[(size_t) (bi * (nRows + 1) + k)];
                const int kind = k < nRows ? rows[k].kind (*bases[bi]) : (int) kOff;
                if ((kind & (kMeas | kPath)) != 0) worstHold = juce::jmax (worstHold, m);
                else                               leastMove = juce::jmin (leastMove, m);
            }
        std::printf ("  (1) move: the largest change of the published value over the 0.3 s of silence after the bottom (dB); "
                     "largest on a HOLD row %.1e, smallest on a move row %.2f\n", worstHold, leastMove);
        check (allConsumed, "premise: every leg-(1) injection was consumed in its bottom block (the published value "
                            "reaches it there, within 1 dB, and not one block earlier, >= 4 dB)");
        check (classes, "non-vacuity: every base meets >= 8 measurement-only rows, exactly one path-only row and >= 4 "
                        "guarded-out rows");
        check (measHold, "an A/B injection re-arms the analysis when the slots differ in a measurement input the path "
                         "re-arm does not see (continuous fields, polarity, the guarded fields their module hears): the "
                         "injected value holds on silence (move < 1e-6 dB)");
        check (pathHold, "a discrete path change (processingDiffers) still re-arms at an A/B bottom: holds");
        check (restMove, "slots that differ only after the tap (Output Gain / Balance, Band Solo, Bypass, Level Match), "
                         "in a guarded-out field, or not at all keep the converged analysis: the injected value moves "
                         "(> 1 dB) toward the source's measurement on silence");

        // (1b) THE RE-ARM CLEARS THE ANALYSIS ONLY. Every row above injects BELOW the destination's predict floor,
        // where min (v, floor) = v whatever the predict remembers; an injection ABOVE it (-0.5 dB against Drive 8's
        // -4.06) is published exactly only if the re-arm kept prevPredictedGainDb -- a re-arm through reset()
        // would make the next predict read a rise and floor the slot's gain.
        {
            const float vUp = -0.5f;                                 // >= 4 dB from the -4.96 published before it
            Lane ln;
            ln.pre = bH; ln.blocks = P1 + 1; ln.silentFrom = S1;
            ln.at = [&] (int b, AnamorphEngine& e, Params& s)
            {
                if (b >= E1) s.width = 1.6f;                         // a measurement input; the predict is unchanged
                if (b == E1) { e.requestDuck(); e.injectMatchGainDb (vUp); }
            };
            run (ln);
            const Probe q = probeOf (ln, (double) vUp, B1);
            std::printf ("  (1b) injection above the Drive-8 predict floor: %+.4f -> bottom %+.4f (v %+.4f), then silence "
                         "moves %.1e dB\n", (double) ln.pub[(size_t) B1 - 1], (double) ln.pub[(size_t) B1], (double) vUp, q.move);
            check (consumed (q) && juce::exactlyEqual (ln.pub[(size_t) B1], vUp) && q.move < kHold,
                   "the injection re-arm clears the analysis only: a slot gain above the predict floor is published "
                   "exactly at the bottom and holds (the predict's memory survives the re-arm)");
        }
    }
    // =====================================================================================================
    //  LEGS (2) and (3) -- continuous-only A/B transients against a fresh destination engine (Drive 2 <-> 8,
    //  0 <-> 10), and at 0 <-> 10 the forced swap WITHOUT an injection against the same edit made live
    // =====================================================================================================
    {
        struct Tr { float from = 0.0f, to = 0.0f; bool forced = false; };
        const Tr trs[] = { { 2.0f, 8.0f, false }, { 8.0f, 2.0f, false }, { 0.0f, 10.0f, true }, { 10.0f, 0.0f, true } };
        const int B = E + kBot, F = E + kFull, last = E + post;
        const int cps[] = { B + (int) std::lround (0.1 * sr / bs), E + (int) std::lround (0.5 * sr / bs), E + sec, E + 2 * sec };
        bool abConsumed = true, abHolds = true, abPub = true, abOut = true, abFit = true;
        bool fMoves = true, fLive = true, fTracks = true;
        for (const auto& tr : trs)
        {
            Lane fresh;
            fresh.pre = base (Algorithm::Haas, tr.to); fresh.blocks = last + 1; fresh.keepFrom = F;
            run (fresh);
            const float vDest = fresh.pub[(size_t) E - 1];               // the slot's remembered gain

            Lane ab;
            ab.pre = base (Algorithm::Haas, tr.from); ab.blocks = last + 1; ab.keepFrom = F; ab.armFrom = E; ab.armTo = B + 1;
            ab.at = [&] (int b, AnamorphEngine& e, Params& s)
            { if (b >= E) s.driveDb = tr.to; if (b == E) { e.requestDuck(); e.injectMatchGainDb (vDest); } };
            run (ab);
            Lane abS;                                                      // its silence twin
            abS.pre = base (Algorithm::Haas, tr.from); abS.blocks = P1 + 1; abS.silentFrom = S1; abS.armFrom = E1; abS.armTo = B1 + 1;
            abS.at = [&] (int b, AnamorphEngine& e, Params& s)
            { if (b >= E1) s.driveDb = tr.to; if (b == E1) { e.requestDuck(); e.injectMatchGainDb (vDest); } };
            run (abS);

            double maxD = 0.0, maxG = 0.0, maxRes = 0.0;
            for (int b = B; b <= last; ++b) maxD = juce::jmax (maxD, std::abs ((double) ab.pub[(size_t) b] - fresh.pub[(size_t) b]));
            for (int b = F; b <= last; ++b)
            {
                const Fit f = fit (ab, fresh, b);
                abFit  = abFit && f.ok;
                maxG   = juce::jmax (maxG, std::abs (f.gDb));
                maxRes = juce::jmax (maxRes, f.resid);
            }
            const double atBot = std::abs ((double) ab.pub[(size_t) B] - vDest), gap = std::abs ((double) ab.pub[(size_t) B - 1] - vDest);
            double holdS = 0.0;
            for (int b = B1; b <= P1; ++b) holdS = juce::jmax (holdS, std::abs ((double) abS.pub[(size_t) b] - vDest));
            char nm[96];
            std::snprintf (nm, sizeof nm, "(%d) A/B Drive %g -> %g, inject the fresh %+.2f", tr.forced ? 3 : 2,
                           (double) tr.from, (double) tr.to, (double) vDest);
            std::printf ("  %-50s: pub(bot-1) %+6.2f pub(bot) %+6.2f | max|D| %.3f  max|g_out| %.3f (resid %.1e) | silence twin "
                         "max|pub-v| %.1e\n", nm, (double) ab.pub[(size_t) B - 1], (double) ab.pub[(size_t) B], maxD, maxG, maxRes, holdS);
            abConsumed = abConsumed && atBot <= 1.0 && gap >= 2.0;
            abHolds    = abHolds && holdS < kHold;
            abPub      = abPub && maxD <= 0.1;
            abOut      = abOut && maxG <= 0.1;

            if (! tr.forced) continue;
            // the undo / preset shape: requestDuck + the snapshot, no injection; and the same edit made live
            Lane fs, lv;
            fs.pre = lv.pre = base (Algorithm::Haas, tr.from);
            fs.blocks = lv.blocks = last + 1;
            fs.at = [&] (int b, AnamorphEngine& e, Params& s) { if (b >= E) s.driveDb = tr.to; if (b == E) e.requestDuck(); };
            lv.at = [&] (int b, AnamorphEngine&, Params& s)   { if (b >= E) s.driveDb = tr.to; };
            run (fs);
            run (lv);
            double maxBot = 0.0, maxFL = 0.0;                              // from the bottom / from full level
            for (int b = B; b <= last; ++b)
            {
                const double d = std::abs ((double) fs.pub[(size_t) b] - lv.pub[(size_t) b]);
                maxBot = juce::jmax (maxBot, d);
                if (b >= F) maxFL = juce::jmax (maxFL, d);
            }
            // its silence probe: on the fall the predict leaves the value alone, so an un-ducked injection one block
            // before the event displaces it first (consumed outside any bottom: no re-arm of its own)
            double v = 0.0;
            const bool fall = tr.to < tr.from;
            Lane fp;
            fp.pre = base (Algorithm::Haas, tr.from); fp.blocks = P1 + 1; fp.silentFrom = S1; fp.armFrom = E1; fp.armTo = B1 + 1;
            fp.at = [&] (int b, AnamorphEngine& e, Params& s)
            {
                if (b >= E1) s.driveDb = tr.to;
                if (b == E1) e.requestDuck();
                if (fall && b == E1 - 1) { v = e.getMatchGainDb() - kInjOff; e.injectMatchGainDb ((float) v); }
            };
            run (fp);
            const double mv = moveAfter (fp, B1, P1);
            const bool live = fall ? consumed (probeOf (fp, v, E1 - 1))
                                   : (double) fp.pub[(size_t) B1 - 1] - fp.pub[(size_t) B1] >= 3.0;   // the predict floor at the bottom
            std::snprintf (nm, sizeof nm, "(3) forced Drive %g -> %g, no injection", (double) tr.from, (double) tr.to);
            std::printf ("  %-50s: vs live max|D| %.3f from the bottom, %.3f from full level; forced / live - fresh at "
                         "0.1/0.5/1/2 s", nm, maxBot, maxFL);
            for (const int c : cps)
                std::printf (" %+.2f/%+.2f", (double) fs.pub[(size_t) c] - fresh.pub[(size_t) c],
                             (double) lv.pub[(size_t) c] - fresh.pub[(size_t) c]);
            std::printf (" | silence probe move %.2f (%s)\n", mv, fall ? "displaced 6 dB first" : "predict floor at the bottom");
            fMoves  = fMoves && mv > kMove;
            fLive   = fLive && live;
            fTracks = fTracks && maxFL <= 0.5;
        }
        check (abConsumed, "premise: each A/B run's injection was consumed at its bottom (published there within 1 dB of "
                           "the injected value, >= 2 dB from it one block earlier)");
        check (abFit, "premise: every A/B output block from the first full-level block on has a least-squares gain against "
                      "the fresh engine");
        check (abHolds, "a continuous-only A/B re-arms the analysis: its silence twin holds the injected value bit-exact from "
                        "the bottom block (Drive 2 <-> 8, 0 <-> 10)");
        check (abPub, "a continuous-only A/B plays the slot's own measurement: published within 0.1 dB of the fresh "
                      "destination engine from the bottom for 3 s (Drive 2 <-> 8, 0 <-> 10)");
        check (abOut, "...and its output level within 0.1 dB of the fresh destination engine's from the first full-level "
                      "block for 3 s (per-block least-squares gain)");
        check (fLive, "liveness: each forced-swap probe's route was reached (the rise's predict floor lowers the value >= 3 dB "
                      "in the bottom block; the fall's displacement was consumed where injected)");
        check (fMoves, "a forced swap WITHOUT an injection (undo / preset) does not re-arm: its silence probe moves (> 1 dB)");
        check (fTracks, "...and it tracks the same edit made live: published trajectories within 0.5 dB from the first "
                        "full-level block for 3 s (Drive 0 <-> 10)");
    }

    // =====================================================================================================
    //  LEG (4) -- ordinary ducks: the upgrade to forced, and the defensive consumer
    // =====================================================================================================
    {
        struct L4
        {
            const char* name = nullptr;
            int  ib = 0;                // the block whose process consumes the injection
            int  cls = 0;               // 0: control (must move), 1: upgrade (must hold), 2: defensive (must hold)
            std::function<void (int, AnamorphEngine&, Params&)> at;
        };
        double v = 0.0;
        const auto inject = [&] (AnamorphEngine& e) { v = e.getMatchGainDb() - kInjOff; e.injectMatchGainDb ((float) v); };
        const L4 legs[] = {
            { "upgrade; Drive 8->10 went live mid-fade-out", B1, 1, [&] (int b, AnamorphEngine& e, Params& s)
              {
                  if (b == E1) s.autoGainMatch = false;                          // the opener: an ordinary duck
                  if (b == E1 + 1)
                  {
                      Params mid = s; mid.autoGainMatch = false; mid.driveDb = 10.0f;
                      e.setParameters (mid);                                     // live: p carries Drive 10 now
                      e.requestDuck(); inject (e);                               // then the A/B upgrades the duck
                  }
                  if (b >= E1 + 1) s.driveDb = 10.0f;
              } },
            { "upgrade; Drive 8->10 in the upgrading snapshot", B1, 1, [&] (int b, AnamorphEngine& e, Params& s)
              {
                  if (b == E1) s.autoGainMatch = false;
                  if (b == E1 + 1) { e.requestDuck(); inject (e); }
                  if (b >= E1 + 1) s.driveDb = 10.0f;
              } },
            { "upgrade; nothing the measurement reads", B1, 0, [&] (int b, AnamorphEngine& e, Params& s)
              {
                  if (b == E1) s.autoGainMatch = false;
                  if (b == E1 + 1) { e.requestDuck(); inject (e); }
              } },
            { "defensive; Level Match off + Drive 10, inject at bottom", B1, 2, [&] (int b, AnamorphEngine& e, Params& s)
              { if (b >= E1) { s.autoGainMatch = false; s.driveDb = 10.0f; } if (b == B1) inject (e); } },
            { "defensive; Level Match off only, inject at bottom", B1, 0, [&] (int b, AnamorphEngine& e, Params& s)
              { if (b >= E1) s.autoGainMatch = false; if (b == B1) inject (e); } },
            { "defensive; the dirty duck, inject at bottom-1", B1 - 1, 0, [&] (int b, AnamorphEngine& e, Params& s)
              { if (b >= E1) { s.autoGainMatch = false; s.driveDb = 10.0f; } if (b == B1 - 1) inject (e); } },
            { "defensive; the dirty duck, inject at bottom+1", B1 + 1, 0, [&] (int b, AnamorphEngine& e, Params& s)
              { if (b >= E1) { s.autoGainMatch = false; s.driveDb = 10.0f; } if (b == B1 + 1) inject (e); } },
            { "no duck; Drive 8->10 live, inject 2 blocks later", E1 + kBot, 0, [&] (int b, AnamorphEngine& e, Params& s)
              { if (b >= E1) s.driveDb = 10.0f; if (b == E1 + kBot) inject (e); } },
        };
        bool live4 = true, upHold = true, defHold = true, ctlMove = true;
        for (const auto& lg : legs)
        {
            Lane ln;
            ln.pre = base (Algorithm::Haas, 8.0f); ln.blocks = P1 + 1; ln.silentFrom = S1; ln.armFrom = E1; ln.armTo = B1 + 1;
            ln.at = lg.at;
            run (ln);
            const Probe q = probeOf (ln, v, lg.ib);
            std::printf ("  (4) %-52s: injected %+6.2f at bot%+d, pub there %+6.2f | move %.2f %s\n", lg.name, q.v, lg.ib - B1,
                         (double) ln.pub[(size_t) lg.ib], q.move, q.move < kHold ? "HOLD" : "move");
            live4 = live4 && consumed (q);
            if (lg.cls == 0)      ctlMove = ctlMove && q.move > kMove;
            else if (lg.cls == 1) upHold  = upHold && q.move < kHold;
            else                  defHold = defHold && q.move < kHold;
        }
        check (live4, "premise: every leg-(4) injection was consumed in the block judged (within 1 dB there, >= 4 dB one "
                      "block earlier)");
        check (upHold, "an ordinary duck upgraded to forced by an A/B re-arms at the injection when a measurement input "
                       "changed -- made live during the fade-out (duckMeasDirty) or carried by the upgrading snapshot");
        check (defHold, "the DEFENSIVE consumer: an injection meeting an ordinary bottom that changed a measurement input "
                        "re-arms (holds)");
        check (ctlMove, "...and no re-arm without one: an upgrade or ordinary bottom that changed nothing the measurement "
                        "reads, and an injection one block before / after the bottom or with no duck at all, move");
    }

    // =====================================================================================================
    //  LEG (5) -- re-prepare: primeParameters -> prepare -> setParameters (the processor's prepareToPlay)
    // =====================================================================================================
    {
        struct Prep { float before = 0.0f, after = 0.0f; };
        const auto reprepare = [] (AnamorphEngine& e, const Params& s, double rate, int block, Prep& pr)
        {
            pr.before = e.getMatchGainDb();
            e.primeParameters (s);
            e.prepare (rate, block);
            e.setParameters (s);
            pr.after = e.getMatchGainDb();
        };
        const Params h8 = base (Algorithm::Haas, 8.0f);
        Params h8Off = h8; h8Off.autoGainMatch = false;
        const int last = E + post;

        // (a) the audible run, its Level-Match-off twin re-prepared alike, and the converged fresh engine
        Prep prRun, prTwin;
        Lane fresh, rn, tw;
        fresh.pre = h8; fresh.blocks = last + 1; fresh.keepFrom = E;
        rn.pre = h8;    rn.blocks = last + 1;    rn.keepFrom = E;
        tw.pre = h8Off; tw.blocks = last + 1;    tw.keepFrom = E;
        rn.at = [&] (int b, AnamorphEngine& e, Params& s) { if (b == E) reprepare (e, s, sr, bs, prRun); };
        tw.at = [&] (int b, AnamorphEngine& e, Params& s) { if (b == E) reprepare (e, s, sr, bs, prTwin); };
        run (fresh); run (rn); run (tw);
        double dPub = 0.0, dApp = 0.0, gOut = 0.0, resTw = 0.0, resFr = 0.0;
        for (int b = E; b <= last; ++b)
        {
            dPub = juce::jmax (dPub, std::abs ((double) rn.pub[(size_t) b] - fresh.pub[(size_t) b]));
            const Fit a = fit (rn, tw, b);                                   // the applied gain (twin at 0 dB)
            dApp  = juce::jmax (dApp, std::abs (a.gDb - fresh.pub[(size_t) b]));
            resTw = juce::jmax (resTw, a.resid);
            if (b >= E + 3)                                                  // the Haas line (12 ms) has refilled
            {
                const Fit g = fit (rn, fresh, b);
                gOut  = juce::jmax (gOut, std::abs (g.gDb));
                resFr = juce::jmax (resFr, g.resid);
            }
        }
        const Fit first = fit (rn, tw, E);
        std::printf ("  (5) same rate and block, audible: published %+.4f -> %+.4f across the re-prepare (bit-identical: %s); "
                     "first block applied %+.3f vs fresh pub %+.3f\n", (double) prRun.before, (double) prRun.after,
                     std::memcmp (&prRun.before, &prRun.after, sizeof (float)) == 0 ? "yes" : "no", first.gDb,
                     (double) fresh.pub[(size_t) E]);
        std::printf ("  (5)   over 3 s: max|pub - fresh| %.3f  max|applied - fresh pub| %.3f (twin resid %.1e)  "
                     "max|g_out vs fresh| %.3f from block 3 (resid %.1e)\n", dPub, dApp, resTw, gOut, resFr);
        check (std::memcmp (&prRun.before, &prRun.after, sizeof (float)) == 0 && std::abs (prRun.before) >= 3.0f,
               "a same-rate, same-block re-prepare keeps the published Level-Match value bit-exact (premise: >= 3 dB from 0)");
        check (resTw <= 1.0e-3, "premise: the Level-Match-off twin explains the run as a pure gain (residual <= 1e-3)");
        check (dApp <= 0.1 && dPub <= 0.1,
               "...and the first audible block lands the applied gain on it: applied and published within 0.1 dB of the "
               "converged fresh engine's value from the first block for 3 s");
        check (gOut <= 0.1, "...and the output level within 0.1 dB of the converged fresh engine's from the moment the Haas "
                            "line has refilled (block 3) for 3 s");

        // (b) silence probes: displaced 6 dB UP by an un-ducked injection one block before -- above the Drive-8 predict
        // floor (-4.06 dB), so a keep that lost the predict's memory (prevPredictedGainDb) would floor it in the first
        // block -- then re-prepared, and silent from the re-prepare on
        struct Keep { const char* name = nullptr; int block = 0; bool og = false, prep = true; };
        const Keep keeps[] = { { "same rate, block 256", bs, false, true }, { "same rate, block 256 -> 512", 2 * bs, false, true },
                               { "same rate, primed Output Gain -6 dB", bs, true, true }, { "control: no re-prepare", bs, false, false } };
        bool keepBits = true, keep512 = true, keepOg = true, probeHold = true, probeLive = true, ctrlMove = true;
        for (const auto& kp : keeps)
        {
            Prep pr;
            double v = 0.0;
            Lane ln;
            ln.pre = h8; ln.blocks = P1 + 1; ln.silentFrom = E1;
            ln.at = [&] (int b, AnamorphEngine& e, Params& s)
            {
                if (kp.og && b >= E1) s.outputGainDb = -6.0f;
                if (b == E1 - 1) { v = e.getMatchGainDb() + kInjOff; e.injectMatchGainDb ((float) v); }
                if (b == E1 && kp.prep) reprepare (e, s, sr, kp.block, pr);
            };
            run (ln);
            const Probe q = probeOf (ln, v, E1 - 1);
            // the analysis: silence from the first block after the re-prepare on (a flush's first block applies the
            // predict floor, a result matter judged below)
            const double mv = moveAfter (ln, E1, P1);
            // the result: kept bit-exact across prepare() AND through the first block
            const bool bits = std::memcmp (&pr.before, &pr.after, sizeof (float)) == 0
                           && std::memcmp (&pr.after, &ln.pub[(size_t) E1], sizeof (float)) == 0;
            if (kp.prep)
                std::printf ("  (5) %-40s: published %+.4f -> %+.4f -> first block %+.4f (bit-identical: %s); then silence "
                             "moves %.1e dB\n", kp.name, (double) pr.before, (double) pr.after, (double) ln.pub[(size_t) E1],
                             bits ? "yes" : "no", mv);
            else
                std::printf ("  (5) %-40s: silence moves %.2f dB (the analysis still live)\n", kp.name, mv);
            probeLive = probeLive && consumed (q);
            if (! kp.prep) { ctrlMove = ctrlMove && mv > kMove; continue; }
            probeHold = probeHold && mv < kHold;
            if (kp.og)                 keepOg  = keepOg && bits;
            else if (kp.block != bs)   keep512 = keep512 && bits;
            else                       keepBits = keepBits && bits;
        }
        check (probeLive, "premise: each re-prepare probe's displacing injection was consumed where injected");
        check (keepBits, "a same-rate, same-block re-prepare keeps the (displaced) published value bit-exact, predict memory "
                         "included: the first block after it publishes the same value");
        check (keep512, "a same-rate re-prepare at a new block size (256 -> 512) keeps it the same way: a block size is no "
                        "input to the measurement");
        check (keepOg, "a primed snapshot that differs only after the tap (Output Gain) keeps it the same way");
        check (probeHold && ctrlMove, "a re-prepare re-arms the analysis: from the first block after it, silence holds the "
                                      "published value (< 1e-6 dB) where the same run without the re-prepare moves (> 1 dB)");

        // (c) the flushes: a new rate, a primed measurement input, a first prepare -- each a first prepare
        struct Flush { const char* name = nullptr; double rate = 48000.0; float drive = 8.0f; };
        const Flush flushes[] = { { "48 -> 44.1 kHz", 44100.0, 8.0f }, { "primed Drive 8 -> 10", 48000.0, 10.0f } };
        const int nAfter = (int) std::lround (0.3 * sr / bs);
        bool flushZero = true, flushFirst = true, flushPremise = true, firstZero = true;
        for (const auto& fl : flushes)
        {
            Prep pr;
            Lane ln, ref;
            ln.pre = h8; ln.blocks = E1 + nAfter;
            ln.at = [&] (int b, AnamorphEngine& e, Params& s)
            {
                if (b >= E1) s.driveDb = fl.drive;
                if (b == E1) reprepare (e, s, fl.rate, bs, pr);
            };
            ref.pre = base (Algorithm::Haas, fl.drive); ref.rate = fl.rate; ref.offset = E1; ref.blocks = nAfter;
            run (ln); run (ref);
            bool same = true;
            for (int k = 0; k < nAfter; ++k)
                same = same && std::memcmp (&ln.pub[(size_t) (E1 + k)], &ref.pub[(size_t) k], sizeof (float)) == 0;
            std::printf ("  (5) %-40s: published %+.4f -> %+.4f; then %+.3f, %+.3f ... bit-identical to a first prepare "
                         "there: %s\n", fl.name, (double) pr.before, (double) pr.after, (double) ln.pub[(size_t) E1],
                         (double) ln.pub[(size_t) E1 + 1], same ? "yes" : "no");
            flushPremise = flushPremise && std::abs (pr.before) >= 3.0f;
            flushZero    = flushZero && std::memcmp (&pr.after, &ref.pub0, sizeof (float)) == 0 && juce::exactlyEqual (pr.after, 0.0f);
            flushFirst   = flushFirst && same;
            firstZero    = firstZero && juce::exactlyEqual (ref.pub0, 0.0f);
        }
        check (flushPremise, "premise: each flushing re-prepare had a published value >= 3 dB from 0 to flush");
        check (firstZero, "a first prepare publishes exactly 0 dB");
        check (flushZero, "a re-prepare at a new rate, or after a primed snapshot that moves a measurement input, flushes: "
                          "published exactly 0 dB right after prepare");
        check (flushFirst, "...and is a first prepare: the published trajectory is bit-identical to a fresh engine's first-"
                           "prepared there on the same input");

        // (d) THE FIRST PREPARE AT THE RATE AN UNPREPARED ENGINE READS. Before any prepare the engine's rate is
        // 44.1 kHz, and a fresh processor primes a default snapshot, which moves no measurement input -- so only
        // "prepared before" keeps that first prepare from passing for a same-rate keep that skips
        // loudness.prepare, leaving the matcher unconfigured: its integrators never leave their floor and it
        // publishes the predict floor for ever. A default engine first prepared at 44.1 kHz and then set to the
        // leg's sound must measure it as an engine primed with that sound does.
        {
            const int n2 = 2 * sec;
            Lane cold, warm;
            cold.pre = Params{}; cold.rate = 44100.0; cold.blocks = n2;
            cold.at = [&] (int, AnamorphEngine&, Params& s) { s = h8; };
            warm.pre = h8;       warm.rate = 44100.0; warm.blocks = n2;
            run (cold); run (warm);
            const double wMoved = std::abs ((double) warm.pub[(size_t) n2 - 1] - warm.pub[0]);
            const double dCold  = std::abs ((double) cold.pub[(size_t) n2 - 1] - warm.pub[(size_t) n2 - 1]);
            std::printf ("  (5) first prepare at 44.1 kHz: primed with the sound %+.4f -> %+.4f; default, then the sound "
                         "%+.4f -> %+.4f (|diff| %.3f dB)\n", (double) warm.pub[0], (double) warm.pub[(size_t) n2 - 1],
                         (double) cold.pub[0], (double) cold.pub[(size_t) n2 - 1], dCold);
            check (wMoved >= 1.0, "premise: (5) a primed first prepare at 44.1 kHz measures: its published value leaves "
                                  "the predict floor by >= 1 dB within 2 s");
            check (dCold <= 0.3, "a first prepare at 44.1 kHz from the default snapshot prepares the matcher: after 2 s it "
                                 "publishes within 0.3 dB of the primed engine's measurement (not the predict floor)");
        }
    }

    // ---- the allocation guard, around every A/B / injection event to the block after its bottom ---------
    std::printf ("  allocation guard: %d armed setParameters+process calls (legs 1-4: event to bottom+1); worst per call: "
                 "new=%ld malloc=%ld\n", armedCalls, worstNew, worstMalloc);
    check (armedCalls > 0, "liveness: the allocation guard was armed around the re-arming bottoms");
    if (guardLive)
    {
        check (worstNew == 0, "no operator-new allocation while an injection re-arms the Level-Match analysis");
        if (guard.mallocLive)
            check (worstMalloc == 0, "no malloc-family allocation while an injection re-arms the Level-Match analysis");
    }
}

// ---------------------------------------------------------------------------
//  Test 68 -- A KEPT LEVEL-MATCH RESULT IS THE APPLIED GAIN FROM THE FIRST BLOCK AFTER A SAME-RATE
//  RE-PREPARE, AT ANY INPUT LEVEL (ADR-0007, Amendment of 2026-09-24, F13(2) Q5. Test 67 leg (5) is the
//  published half.)
//
//  THE CLAIM. When prepare() keeps the published result (prepared before, the same rate, no measurement input
//  moved by primeParameters(), the value finite) and the snapshot reset() settles has Level Match on, the
//  applied match gain IS the kept value from the first sample of the first block, whatever the level of the
//  audio that resumes. prepare() writes matchGainSmooth to unity and snapSmoothers() leaves it alone
//  (ADR-0007 decides where it lands); updateDerived() only TARGETS the kept value. What used to land it was
//  the silence->audio snap at the end of the level-match stage -- prevInputSilent (true after reset()) and
//  ! inSilentNow, inSilentNow = the block's sum of L^2 + R^2 of the CONDITIONED input < 1e-6 * n (~-60
//  dBFS) -- so quieter resumed audio played the smoother's 0.12 s linear glide from 0 dB to the kept value.
//  Unchanged, and pinned here as such: Level Match off rests at unity (a later Case-B engage glides from it,
//  a Case-A engage lands); a new rate, a primed measurement change and a non-finite result flush (published
//  exactly 0 dB, the applied gain at unity; the predict floor then glides in on quiet audio and snaps on
//  loud); an injection wins; a host reset leaves the applied gain alone.
//
//  THE ORACLE. Each run has an event-matched TWIN: the same snapshots, events and seeded input from sample 0,
//  re-prepared at the same block, with autoGainMatch = false at Output Gain 0 dB. Level Match writes only the
//  output stage (and a forced duck's dry-fill level, read here only after its fade-in), so the float entering
//  the output stage is the same in both; the twin's settled unity path skips the multiply, the run multiplies
//  by the applied match gain. run / twin per sample (|twin| > 1e-30) IS the applied gain, to one rounding
//  (~5e-7 dB); the per-block least-squares gain and its residual (Test 66's M1) confirm it -- residual
//  <= 1e-6 wherever the gain is constant (<= 9e-16 measured), 2e-5 to 9e-4 inside the old glide. The twin
//  also shares the run's measurement (bit-identical published value at the re-prepare).
//
//  LEGS (48 kHz / 256; Haas, Amount 0.5, Width 1.3, Level Match on unless stated; the suite's noise, L = v,
//  R = 0.6 v + 0.2 w, as a loud pre-roll -- 3 s for (1)-(5) and (10), 0.5 s for (6)-(9) -- then the
//  processor's primeParameters -> prepare -> setParameters, then J = 28 blocks (0.149 s: the old ramp and 5
//  blocks wholly past it) of the same noise scaled to the stated level, the RMS of the louder, left channel):
//   (1)-(5) KEPT, Level Match on. Drive 8 (retained -6.087 dB: (4)) at -70 dBFS (the Devin review's example:
//       (1)), -90 and -125 (3); Drive 20 at -90 (-10.41 dB), Drive 24 at -125 (-10.95) and Drive 2 at -70
//       (-2.79, the small one) (5); Width 0 and Amount 0 on anti-correlated input (R = -0.5 v + 0.5 w, Test
//       66 leg (2)) at -70: +7.71 dB (2). CLAIM: the applied gain within 0.02 dB of the retained value at the
//       first sample, at every sample and in every block's least-squares gain (residual <= 1e-6) for 0.149 s.
//       Premises, each asserted: the keep (published bit-identical across prepare, >= 3 dB from 0 -- 0.5 dB
//       for the small leg -- and the Drive-8 legs within 0.5 dB of -6 dB); the twin shares the measurement;
//       the published value holds bit-exact through the window (the re-armed gate stays closed); every
//       resumed block's conditioned input (identity conditioning: the samples fed) has mean(L^2 + R^2)
//       < 0.5e-6, half the snap's rule (1.5e-7 / 1.5e-9 / 4.6e-13 measured); the twin explains the run past
//       the old ramp; Level Match is engaged (the applied gain >= 3 dB from unity at the window's end, on
//       either engine); and the re-prepare happened -- the first resumed block's output energy is >= 30 dB
//       below the same lane's without it, which still plays the pre-roll's Haas tail (48.6 dB measured).
//   (6) Level Match OFF across a kept re-prepare (-4.93 dB kept bit-exact), engaged in the FIRST block after it
//       by an ordinary duck, at -70 dBFS. With Drive 8 -> 10 in the same snapshot (Case B, duckMeasDirty) the
//       fade-in still glides from unity: phi = D_F / (0 - pub_F) >= 0.5 at the first full-level block
//       (0.765); alone (Case A) it lands on the published value from the bottom block (max|D| 0.0000). The
//       twins open the same duck with an inert band-count move (Multiband off). The first block because a
//       smoother left at the kept value would glide back to unity (Level Match off targets 1.0) and differs
//       from one left at unity only while that glide is young.
//   (7) 48 -> 44.1 kHz with Level Match on, a FLUSH by decision: published exactly 0 dB after prepare, the
//       Drive-8 predict floor (-4.057 dB) in the first block. At -70 / -90 / -125 dBFS the applied gain starts
//       at unity (first sample -0.0006 dB), is mid-glide at 60 ms (applied / published 0.44; window 0.3-0.7)
//       and lands by the ramp's end: the mechanism the kept legs used to show, and the behavioural proof that
//       each quiet level is below the snap's detector -- at the pre-roll's own level the same flush snaps at
//       its first sample (-4.067 dB, that block's published value).
//   (8) An INVALID result falls back to unity. A NON-FINITE published value does reach prepare() through the
//       engine's API: injectMatchGainDb (+Inf) -- inj > kNoInject + 1 admits it -- consumed un-ducked with
//       Level Match off while the matcher's gate is open sets displayedGainDb = +Inf; MEASURE then computes
//       displayedGainDb += c * (target - Inf) = NaN, clampd passes NaN, the output stage plays Output Gain so
//       no self-heal runs, and NaN is absorbing (the measure, the silent hold and the predict floor all keep
//       it). With Level Match on, the same injection makes the output x * Inf and the self-heal resets the
//       matcher to 0 dB; the processor never injects +Inf (abMatchGain is 0 or a getMatchGainDb() reading,
//       and a NaN injection is refused). (8a) that NaN, re-prepared with Level Match primed on: keepMatch is
//       false by its isfinite term alone (autoGainMatch is no measurement input). The nearest other invalid
//       cases: (8b) a NaN input burst whose self-heal flushes the matcher to exactly 0 dB in its own process()
//       call, re-prepared on the next block (the keep keeps that 0 dB); (8c) a primed Drive 8 -> 10 (a flush).
//       Each: published exactly 0 dB after prepare, the applied gain within 0.01 dB of unity at the first
//       sample while the first block publishes the predict floor (<= -3 dB).
//   (9) An injection right after a kept re-prepare still wins: un-ducked (retained - 5 dB), published bit-exact
//       and applied from its first sample; A/B-shaped (requestDuck + inject retained + 4 dB, the same
//       snapshot), published bit-exact from the bottom block and applied from the first full-level block.
//  (10) A host reset (audioTailsOnly) 3 blocks after a kept re-prepare leaves the applied gain alone: per sample
//       within 1e-4 dB (8e-7 measured) of the same lane without it; liveness: the reset changed that block's
//       output, and the published value held through it.
//  (11) THE ENGINE API, UNPRIMED: prepare() while an ordinary duck that turns Level Match on (nothing else) is in
//       flight and nothing was primed (keepMatch holds: same rate, finite, no prime). reset() adopts pendingP, so
//       the Level-Match state that decides is the one it settles: the kept value (-4.93 dB) is applied from the
//       first sample. The twin opens the same duck with an inert band-count move.
//  F13(1b) Case A / Case B and F13(2) P1b stay Tests 66 / 67's and State tests 130 / 131's.
//
//  MEASURED BEFORE THE FIX (engine a7d2b88, this test): 5 of the 30 checks fail -- the claims of (1)(4),
//  (3)(4), (5), (2) and (11). Every kept leg started at unity: first sample -0.0008 dB against a kept -6.0873 at
//  -70, -90 AND -125 dBFS alike (no snap at any of them), -0.0011 against -10.41 / -10.95, -0.0004 against
//  -2.79, +0.0022 against +7.71; the review's example played -0.001 / -0.373 / -1.170 / -2.521 / -6.087 dB
//  at 0 / 10 / 30 / 60 / 120 ms (the ramp's last step lands at 120 ms), per-block residual up to 1.6e-4 inside
//  the glide. Every premise and legs (6)-(10) pass on both engines by design: they pin what the fix keeps.
//  ENGINE VARIANTS REJECTED (each built from this tree and run through this test): the silence detector
//  lowered to 1e-12 * n (-120 dB) in the fix's place -- the -70 / -90 legs snap and pass, the -125 dBFS legs
//  still start at unity ((3)(4), (5): D +6.09 / +10.95) and the -70 / -90 flushes snap to the predict floor
//  ((7), (8)); a snap on prevInputSilent alone (the rule without ! inSilentNow) -- every kept leg passes, but
//  (6)'s Case B lands (phi 0.000) and every quiet flush snaps ((7), (8): first sample -4.057 / -5.016 dB);
//  the write without its Level-Match gate (if (keepMatch)) -- (6)'s Case B phi 0.094, the smoother's glide
//  from the kept value back to unity barely begun at the bottom; the keep without its isfinite term -- (8a)
//  keeps the NaN, the write lands decibelsToGain (NaN) = 0 and the run is silent from its first sample ((8)).
//  The write placed BEFORE reset() (replacing the unity write), or gated on the Level-Match state read before
//  it: identical on the processor's path, where primeParameters() has written p and pendingP alike -- (11)'s
//  unprimed engine rejects both (first sample -0.0007 dB against a kept -4.9348).
//  NOT REJECTED, equivalent by construction: the published value read before reset() (only softReset() runs
//  in between, and it keeps the result); the write on every Level-Match-on prepare (a flush publishes 0 dB,
//  whose gain is the unity already written); a snap to the smoother's target on a keep (updateDerived() set that
//  target to the same value, or to 1 with Level Match off).
static void testLevelMatchKeptResultIsTheAppliedGainFromTheFirstBlock()
{
    std::printf ("Test 68: a kept Level-Match result is the applied gain from the first block after a same-rate "
                 "re-prepare, at any input level (ADR-0007, F13(2) Q5)\n");
    juce::ScopedNoDenormals noDenormals;

    using anamorph::AnamorphEngine;
    using anamorph::Algorithm;
    using Params = anamorph::EngineParameters;
    constexpr double sr = 48000.0, sr2 = 44100.0;
    constexpr int    bs = 256, nch = 2, blk = bs * nch;

    // THE DUCK'S TIMING, from its documented lengths (~6 ms out, ~28 ms in; Test 66 re-derives both from the
    // engine's output): the silent bottom at event + 2, the first full-level block at event + 8.
    const int fadeOut = (int) std::lround (0.006 * sr), fadeIn = (int) std::lround (0.028 * sr);
    const int kBot  = fadeOut / bs + 1;
    const int kFull = kBot + (fadeIn + bs - 1) / bs;
    // THE OLD RAMP: matchGainSmooth glides linearly over 0.12 s (AnamorphEngine::prepare). J judged blocks
    // after the re-prepare cover it and 5 blocks wholly past it (0.149 s at 48 kHz, 0.163 s at 44.1 kHz).
    const auto rampBlocks = [] (double rate) { return (int) std::ceil (0.12 * rate / bs); };
    const int J  = rampBlocks (sr) + 5;
    const int RL = (int) std::lround (3.0 * sr / bs);        // the long loud pre-roll: legs (1)-(5), (10)
    const int RS = (int) std::lround (0.5 * sr / bs);        // the short one: legs (6)-(9)

    // ---- one seeded stream: block b is the same (v, w) pairs in every lane that reads it -----------------
    std::vector<float> stream ((size_t) (RL + J) * blk);
    {
        juce::Random rng (6801);
        for (auto& x : stream) x = rng.nextFloat() - 0.5f;
    }
    // A resumed level is the RMS of the louder, left channel: the noise is uniform on [-0.5, 0.5), RMS
    // 1/sqrt(12) (-10.8 dBFS, the pre-roll's level).
    const auto scaleFor = [] (double dBFS) { return (float) (std::pow (10.0, dBFS / 20.0) * std::sqrt (12.0)); };

    struct Lane
    {
        Params pre, post;              // before block rp / primed, prepared and set at the re-prepare, and after
        std::function<void (int, AnamorphEngine&, Params&)> at;    // events, per block before setParameters
        int    rp = 0;                 // the re-prepare, before block rp's setParameters + process
        bool   reprep = true;          // false: the no-re-prepare control
        bool   prime = true;           // false: the snapshot reaches the engine by setParameters BEFORE prepare()
        bool   anti = false;           // anti-correlated input (R = -0.5 v + 0.5 w; Test 66 leg (2))
        double rate = 48000.0;         // the re-prepare's sample rate
        float  scale = 1.0f;           // the input's scale from block rp on (the pre-roll plays at 1)
        int    nanAt = -1;             // a block whose first 8 samples are NaN on both channels
        float  before = 0.0f, after = 0.0f;   // published just before / after the re-prepare
        double inMs = 0.0;             // the largest per-block mean of L^2 + R^2 fed from block rp on
        std::unique_ptr<AnamorphEngine> e;
        std::vector<float> pub, y;     // published per block; output of the J blocks from rp (interleaved)
    };

    const auto run = [&] (Lane& ln)
    {
        const int rp = ln.rp, blocks = rp + J;
        ln.e = std::make_unique<AnamorphEngine>();                  // heap: Test 59's note (1 MB-stack lane)
        ln.e->primeParameters (ln.pre);
        ln.e->prepare (sr, bs);
        ln.e->setParameters (ln.pre);
        ln.pub.assign ((size_t) blocks, 0.0f);
        ln.y.assign ((size_t) J * blk, 0.0f);
        juce::AudioBuffer<float> buf (nch, bs);
        for (int b = 0; b < blocks; ++b)
        {
            Params snap = b < rp ? ln.pre : ln.post;
            if (b == rp)
            {
                ln.before = ln.e->getMatchGainDb();
                if (ln.reprep)                                       // the processor's prepareToPlay sequence
                {
                    if (ln.prime) ln.e->primeParameters (snap);
                    else          ln.e->setParameters (snap);      // opens its ordinary duck; prepare()'s reset() adopts it
                    ln.e->prepare (ln.rate, bs);
                    ln.e->setParameters (snap);
                }
                ln.after = ln.e->getMatchGainDb();
            }
            if (ln.at) ln.at (b, *ln.e, snap);
            const float k = b < rp ? 1.0f : ln.scale;
            const float* src = stream.data() + (size_t) b * blk;
            double ms = 0.0;
            for (int i = 0; i < bs; ++i)
            {
                const float v = src[2 * i], w = src[2 * i + 1];
                const float l = k * v, r = k * (ln.anti ? -0.5f * v + 0.5f * w : 0.6f * v + 0.2f * w);
                buf.setSample (0, i, l);
                buf.setSample (1, i, r);
                ms += (double) l * l + (double) r * r;
            }
            if (b >= rp) ln.inMs = juce::jmax (ln.inMs, ms / bs);
            if (b == ln.nanAt)
                for (int i = 0; i < 8; ++i)
                {
                    buf.setSample (0, i, std::numeric_limits<float>::quiet_NaN());
                    buf.setSample (1, i, std::numeric_limits<float>::quiet_NaN());
                }
            ln.e->setParameters (snap);
            ln.e->process (buf);
            ln.pub[(size_t) b] = ln.e->getMatchGainDb();
            if (b >= rp)
            {
                float* out = ln.y.data() + (size_t) (b - rp) * blk;
                for (int i = 0; i < bs; ++i) { out[2 * i] = buf.getSample (0, i); out[2 * i + 1] = buf.getSample (1, i); }
            }
        }
        ln.e.reset();                                               // judged from its record alone
    };

    // ---- THE APPLIED GAIN: run / twin, per sample and per block -------------------------------------------
    // jb: a judged block (0 = the first block after the re-prepare); i: an interleaved index in it.
    const auto sampleDb = [] (const Lane& r, const Lane& t, int jb, int i, double& dB)
    {
        const size_t k = (size_t) jb * blk + (size_t) i;
        const double z = t.y[k];
        if (! (std::abs (z) > 1.0e-30)) return false;
        const double g = (double) r.y[k] / z;
        if (! (g > 0.0)) return false;
        dB = 20.0 * std::log10 (g);
        return true;
    };
    // The applied gain `sec` seconds after the re-prepare (the left sample, else the right; NaN: no reading).
    const auto atTime = [&] (const Lane& r, const Lane& t, double sec, double rate)
    {
        const int n = (int) std::lround (sec * rate);
        double d = 0.0;
        if (sampleDb (r, t, n / bs, 2 * (n % bs), d) || sampleDb (r, t, n / bs, 2 * (n % bs) + 1, d)) return d;
        return std::numeric_limits<double>::quiet_NaN();
    };
    // Per-block least-squares gain of run against twin, and the residual a pure gain leaves (normalised by
    // the run's energy): Test 66's M1.
    struct Fit { double gDb = 0.0, resid = 1.0; bool ok = false; };
    const auto fit = [] (const Lane& r, const Lane& t, int jb) -> Fit
    {
        const float* x = r.y.data() + (size_t) jb * blk;
        const float* z = t.y.data() + (size_t) jb * blk;
        double num = 0.0, den = 0.0, ex = 0.0;
        for (int i = 0; i < blk; ++i)
        {
            num += (double) x[i] * z[i];
            den += (double) z[i] * z[i];
            ex  += (double) x[i] * x[i];
        }
        if (! (den > 1.0e-30 && ex > 1.0e-30)) return {};
        const double g = num / den;
        double res = 0.0;
        for (int i = 0; i < blk; ++i) { const double d = (double) x[i] - g * z[i]; res += d * d; }
        return { 20.0 * std::log10 (juce::jmax (1.0e-12, std::abs (g))), res / ex, true };
    };
    struct Judged
    {
        double first = 0.0;            // the applied gain (dB) at the first reading of block `from`
        int    firstAt = -1;           // its interleaved index (0: the left sample of the very first frame)
        double maxErr = 0.0;           // max |applied - ref| over every sample of the judged blocks
        double lsErr = 0.0;            // max |least-squares gain - ref| over the judged blocks
        double resid = 0.0;            // max residual over the judged blocks
        double residSettled = 0.0;     // ... over those wholly past the old ramp (the gain is constant there)
        double endDb = 0.0;            // the least-squares gain of the last judged block
        bool   ok = true;              // every block read, and >= 90 % of the samples
    };
    const auto judge = [&] (const Lane& r, const Lane& t, double ref, int from, int to, double rate)
    {
        Judged j;
        const int settled = rampBlocks (rate);
        int readings = 0;
        for (int jb = from; jb < to; ++jb)
        {
            for (int i = 0; i < blk; ++i)
            {
                double d = 0.0;
                if (! sampleDb (r, t, jb, i, d)) continue;
                if (j.firstAt < 0) { j.firstAt = (jb - from) * blk + i; j.first = d; }
                j.maxErr = juce::jmax (j.maxErr, std::abs (d - ref));
                ++readings;
            }
            const Fit f = fit (r, t, jb);
            j.ok    = j.ok && f.ok;
            j.lsErr = juce::jmax (j.lsErr, std::abs (f.gDb - ref));
            j.resid = juce::jmax (j.resid, f.resid);
            if (jb >= settled) j.residSettled = juce::jmax (j.residSettled, f.resid);
            j.endDb = f.gDb;
        }
        j.ok = j.ok && j.firstAt >= 0 && readings * 10 >= (to - from) * blk * 9;
        return j;
    };
    // The published value holds bit-exact over blocks [from, to) of a lane, at `v`.
    const auto holdsAt = [] (const Lane& ln, int from, int to, float v)
    {
        bool h = true;
        for (int b = from; b < to; ++b) h = h && std::memcmp (&ln.pub[(size_t) b], &v, sizeof (float)) == 0;
        return h;
    };
    const auto same = [] (float a, float b) { return std::memcmp (&a, &b, sizeof (float)) == 0; };

    const auto haas = [] (float drive, bool lm)
    {
        Params p;
        p.algorithm = Algorithm::Haas; p.algoAmount = 0.5f; p.width = 1.3f; p.driveDb = drive; p.autoGainMatch = lm;
        return p;
    };
    const auto offOf = [] (Params p) { p.autoGainMatch = false; return p; };
    using At = std::function<void (int, AnamorphEngine&, Params&)>;
    // A run and its event-matched twin (Level Match off, otherwise the same snapshot, events and input).
    struct Pair { Lane r, t; };
    const auto pairOf = [&] (const Params& pre, const Params& post, int rp, float scale, bool anti, const At& at,
                             const At& twinAt = nullptr)
    {
        auto pr = std::make_unique<Pair>();
        pr->r.pre = pre;          pr->r.post = post;          pr->r.at = at;
        pr->t.pre = offOf (pre);  pr->t.post = offOf (post);  pr->t.at = twinAt ? twinAt : at;
        for (Lane* ln : { &pr->r, &pr->t }) { ln->rp = rp; ln->scale = scale; ln->anti = anti; }
        return pr;
    };
    const auto runPair = [&] (Pair& pr) { run (pr.r); run (pr.t); };

    const float sc70 = scaleFor (-70.0);
    constexpr double kTol = 0.02;                                  // dB: the claim's tolerance

    // =====================================================================================================
    //  LEGS (1)-(5) -- KEPT with Level Match on: the retained value is the applied gain from the first sample
    // =====================================================================================================
    struct Mag { const char* name = nullptr; float drive = 0.0f; bool positive = false; double dBFS = 0.0, minAbs = 3.0; };
    const Mag mags[] = { { "(1)(3)(4) Drive 8, -70 dBFS (the review's example)", 8.0f, false, -70.0, 3.0 },
                         { "(3)(4) Drive 8, -90 dBFS",                           8.0f, false, -90.0, 3.0 },
                         { "(3)(4) Drive 8, -125 dBFS",                          8.0f, false, -125.0, 3.0 },
                         { "(5) Drive 20, -90 dBFS (large)",                     20.0f, false, -90.0, 3.0 },
                         { "(5) Drive 24, -125 dBFS (larger)",                   24.0f, false, -125.0, 3.0 },
                         { "(5) Drive 2, -70 dBFS (small)",                      2.0f, false, -70.0, 0.5 },
                         { "(2) Width 0 on anti-correlated input, -70 dBFS",     0.0f, true, -70.0, 3.0 } };
    constexpr int nMags = (int) (sizeof (mags) / sizeof (mags[0]));
    std::vector<std::unique_ptr<Pair>> kept;
    bool keepOk = true, twinShares = true, holdOk = true, belowDet = true, explains = true, engaged = true;
    bool around6 = true, claim70 = true, claimLow = true, claimMag = true, claimPos = true;
    for (int m = 0; m < nMags; ++m)
    {
        Params p = haas (mags[m].drive, true);
        if (mags[m].positive) { p.algoAmount = 0.0f; p.width = 0.0f; }
        kept.push_back (pairOf (p, p, RL, scaleFor (mags[m].dBFS), mags[m].positive, nullptr));
        Pair& pr = *kept.back();
        runPair (pr);
        const Judged j = judge (pr.r, pr.t, (double) pr.r.after, 0, J, sr);
        const bool keep = same (pr.r.before, pr.r.after) && std::abs (pr.r.after) >= mags[m].minAbs;
        const bool hold = holdsAt (pr.r, RL, RL + J, pr.r.after);
        const bool claim = j.ok && j.firstAt < 2 && std::abs (j.first - pr.r.after) <= kTol && j.maxErr <= kTol
                        && j.lsErr <= kTol && j.resid <= 1.0e-6;
        std::printf ("  %-52s: published %+.4f -> %+.4f (kept: %s) | applied: first %+.4f (D %+.4f)  max|D| %.4f  "
                     "LS max|D| %.4f  resid %.1e (settled %.1e) end %+.3f | input ms %.1e\n", mags[m].name,
                     (double) pr.r.before, (double) pr.r.after, keep ? "yes" : "no", j.first, j.first - pr.r.after,
                     j.maxErr, j.lsErr, j.resid, j.residSettled, j.endDb, pr.r.inMs);
        keepOk     = keepOk && keep;
        twinShares = twinShares && same (pr.r.after, pr.t.after);
        holdOk     = holdOk && hold;
        belowDet   = belowDet && pr.r.inMs < 0.5e-6;
        explains   = explains && j.ok && j.residSettled <= 1.0e-6;
        engaged    = engaged && std::abs (j.endDb) >= mags[m].minAbs;
        if (m < 3) around6 = around6 && std::abs (pr.r.after + 6.0f) <= 0.5f;
        if (m == 0)      claim70  = claim;
        else if (m < 3)  claimLow = claimLow && claim;
        else if (m < 6)  claimMag = claimMag && claim;
        else             claimPos = claim;
    }
    // THE REVIEW'S EXAMPLE, and the control that the re-prepare happened: the same lane without it
    {
        const Pair& d = *kept[0];
        const double t[] = { 0.0, 0.010, 0.030, 0.060, 0.120 };
        std::printf ("  (1) Drive 8, -70 dBFS: retained %+.4f; applied at 0 / 10 / 30 / 60 / 120 ms:", (double) d.r.after);
        for (const double s : t) std::printf (" %+.3f", atTime (d.r, d.t, s, sr));
        std::printf (" dB\n");
        Lane ctl;
        ctl.pre = ctl.post = haas (8.0f, true); ctl.rp = RL; ctl.scale = sc70; ctl.reprep = false;
        run (ctl);
        double eRun = 0.0, eCtl = 0.0;
        for (size_t i = 0; i < (size_t) blk; ++i)
        {
            eRun += (double) d.r.y[i] * d.r.y[i];
            eCtl += (double) ctl.y[i] * ctl.y[i];
        }
        const double ratio = 10.0 * std::log10 (juce::jmax (1.0e-300, eCtl) / juce::jmax (1.0e-300, eRun));
        std::printf ("  (1) the first resumed block's output energy: re-prepared %.2e, no-re-prepare control %.2e "
                     "(%.1f dB above: the loud Haas tail the re-prepare cleared)\n", eRun, eCtl, ratio);
        check (ratio >= 30.0, "non-vacuity (1)-(5): the re-prepare happened -- the first resumed block's output energy is "
                              ">= 30 dB below a no-re-prepare control's, which still carries the loud Haas tail");
    }
    check (keepOk, "premise (1)-(5): each same-rate re-prepare kept the published value bit-exact, >= 3 dB from 0 dB "
                   "(0.5 dB for the small leg)");
    check (twinShares, "premise (1)-(5): each twin shares its run's measurement (bit-identical published value at the "
                       "re-prepare): the two differ only in the output stage");
    check (holdOk, "premise (1)-(5): the published value holds bit-exact through the judged 0.149 s (the re-armed "
                   "analysis's gate stays closed)");
    check (belowDet, "premise (1)-(5): every resumed block's conditioned input has mean(L^2 + R^2) < 0.5e-6, half the "
                     "engine's silence->audio rule (1e-6 * n): its snap never fires");
    check (explains, "premise (1)-(5): the twin explains the run -- a pure gain, residual <= 1e-6 in every block past the "
                     "old ramp");
    check (engaged, "premise (1)-(5): Level Match is engaged -- the applied gain at the end of the window is >= 3 dB "
                    "from unity (0.5 dB for the small leg)");
    check (around6, "premise (4): the Drive-8 legs retain a result within 0.5 dB of -6 dB");
    check (claim70, "(1)(4) Drive 8, -70 dBFS: a same-rate re-prepare with Level Match on applies the kept NEGATIVE "
                    "result (~-6 dB) from the first sample of the first block -- every sample and every block's "
                    "least-squares gain within 0.02 dB of it for 0.149 s (residual <= 1e-6)");
    check (claimLow, "(3)(4) Drive 8, -90 and -125 dBFS: the same, from the first sample, far below any silence "
                     "detector the snap could be re-tuned to");
    check (claimMag, "(5) Drive 20 (-90 dBFS), Drive 24 (-125 dBFS) and Drive 2 (-70 dBFS): the same for two larger "
                     "retained magnitudes and a small one");
    check (claimPos, "(2) Width 0 on anti-correlated input, -70 dBFS: the same for a POSITIVE retained match");

    // =====================================================================================================
    //  LEG (10) -- a host reset 3 blocks after a kept re-prepare leaves the applied gain alone
    // =====================================================================================================
    {
        const Pair& ref = *kept[0];
        const At hostReset = [RL] (int b, AnamorphEngine& e, Params&)
        { if (b == RL + 3) e.reset (AnamorphEngine::ResetScope::audioTailsOnly); };
        auto pr = pairOf (haas (8.0f, true), haas (8.0f, true), RL, sc70, false, hostReset);
        runPair (*pr);
        double maxDiff = 0.0, maxToKept = 0.0;
        bool read = true;
        for (int jb = 0; jb < J; ++jb)
            for (int i = 0; i < blk; ++i)
            {
                double a = 0.0, c = 0.0;
                const bool ra = sampleDb (pr->r, pr->t, jb, i, a), rc = sampleDb (ref.r, ref.t, jb, i, c);
                if (ra && rc)
                {
                    maxDiff   = juce::jmax (maxDiff, std::abs (a - c));
                    maxToKept = juce::jmax (maxToKept, std::abs (a - pr->r.after));
                }
                else if (ra != rc) read = false;
            }
        const bool live = std::memcmp (pr->r.y.data() + (size_t) 3 * blk, ref.r.y.data() + (size_t) 3 * blk,
                                       sizeof (float) * (size_t) blk) != 0;
        const bool hold = holdsAt (pr->r, RL, RL + J, pr->r.after) && same (pr->r.after, ref.r.after);
        std::printf ("  %-52s: applied vs the same lane without the reset max|diff| %.1e dB; vs the kept value %.4f dB; "
                     "output changed at the reset: %s\n", "(10) host reset 3 blocks after the kept re-prepare", maxDiff,
                     maxToKept, live ? "yes" : "no");
        check (live && hold, "liveness (10): the host reset reached the engine (its block's output differs from the lane "
                             "without it) and the published value held through it");
        check (read && maxDiff <= 1.0e-4, "(10) a host reset after a kept re-prepare leaves the applied gain alone: per "
                                          "sample within 1e-4 dB of the same lane without the reset for 0.149 s");
    }

    // =====================================================================================================
    //  LEG (6) -- Level Match OFF across a kept re-prepare, engaged in the first block after it
    // =====================================================================================================
    {
        const Params h8off = haas (8.0f, false);
        auto b6 = pairOf (h8off, h8off, RS, sc70, false,
                          [RS] (int b, AnamorphEngine&, Params& s)
                          { if (b >= RS) { s.autoGainMatch = true; s.driveDb = 10.0f; } },
                          [RS] (int b, AnamorphEngine&, Params& s) { if (b >= RS) { s.mbBands = 3; s.driveDb = 10.0f; } });
        auto a6 = pairOf (h8off, h8off, RS, sc70, false,
                          [RS] (int b, AnamorphEngine&, Params& s) { if (b >= RS) s.autoGainMatch = true; },
                          [RS] (int b, AnamorphEngine&, Params& s) { if (b >= RS) s.mbBands = 3; });
        runPair (*b6); runPair (*a6);
        const Fit fB = fit (b6->r, b6->t, kFull);
        const double pubF = b6->r.pub[(size_t) (RS + kFull)], dF = fB.gDb - pubF, phi = dF / (0.0 - pubF);
        const Judged jA = judge (a6->r, a6->t, (double) a6->r.after, kBot, J, sr);
        const bool keep = same (b6->r.before, b6->r.after) && same (a6->r.before, a6->r.after)
                       && std::abs (a6->r.after) >= 3.0f;
        const bool hold = holdsAt (a6->r, RS, RS + J, a6->r.after);
        std::printf ("  %-52s: published %+.4f kept (%s); pub_F %+.3f  applied_F %+.3f  phi %.3f  resid %.1e\n",
                     "(6) Level Match off, kept; Case-B engage (Drive 8->10)", (double) b6->r.after, keep ? "yes" : "no",
                     pubF, fB.gDb, phi, fB.resid);
        std::printf ("  %-52s: from the bottom block max|D| %.4f  (first %+.4f vs %+.4f)\n",
                     "(6) Level Match off, kept; Case-A engage (alone)", jA.maxErr, jA.first, (double) a6->r.after);
        check (keep && hold, "premise (6): a same-rate re-prepare with Level Match OFF keeps the published value bit-exact "
                             "(>= 3 dB from 0), and it holds through the engage");
        check (fB.ok && fB.resid <= 1.0e-3 && phi >= 0.5,
               "(6) a same-rate re-prepare with Level Match OFF leaves the applied gain at unity: a Case-B engage (Drive "
               "8 -> 10 in the same snapshot) in the first block after it still glides from unity (phi >= 0.5 at the "
               "first full-level block)");
        check (jA.ok && jA.maxErr <= kTol, "(6) ...and a Case-A engage (Level Match alone) there still lands on the "
                                           "published value from the bottom block (every sample within 0.02 dB)");
    }

    // =====================================================================================================
    //  LEG (7) -- a NEW rate flushes: 0 dB after prepare, the applied gain at unity; quiet -> glide, loud -> snap
    // =====================================================================================================
    {
        const struct { double dBFS; bool loud; } lv[] = { { -70.0, false }, { -90.0, false }, { -125.0, false },
                                                          { 0.0, true } };        // loud: the pre-roll's own level
        bool zero = true, glides = true, snaps = true, premise = true;
        for (const auto& l : lv)
        {
            const bool loud = l.loud;
            const double d = l.dBFS;
            auto pr = pairOf (haas (8.0f, true), haas (8.0f, true), RS, loud ? 1.0f : scaleFor (d), false, nullptr);
            pr->r.rate = pr->t.rate = sr2;
            runPair (*pr);
            const Judged j = judge (pr->r, pr->t, (double) pr->r.pub[(size_t) RS], 0, J, sr2);
            const double pubR = pr->r.pub[(size_t) RS];
            const double mid = atTime (pr->r, pr->t, 0.06, sr2), phi = mid / pubR;
            premise = premise && std::abs (pr->r.before) >= 3.0f && pubR <= -3.0 && j.ok;
            zero = zero && juce::exactlyEqual (pr->r.after, 0.0f);
            if (loud)
                snaps = snaps && j.firstAt < 2 && std::abs (j.first - pubR) <= kTol;
            else
                glides = glides && j.firstAt < 2 && std::abs (j.first) <= 0.01 && phi >= 0.3 && phi <= 0.7
                      && holdsAt (pr->r, RS, RS + J, pr->r.pub[(size_t) RS]) && std::abs (j.endDb - pubR) <= kTol;
            char nm[80];
            if (loud) std::snprintf (nm, sizeof nm, "(7) 48 -> 44.1 kHz, the pre-roll's level (loud)");
            else      std::snprintf (nm, sizeof nm, "(7) 48 -> 44.1 kHz, %.0f dBFS", d);
            std::printf ("  %-52s: published %+.4f -> %+.4f, first block %+.4f | applied: first %+.4f  60 ms %+.3f "
                         "(phi %.2f)  end %+.3f | at 0/10/30/60/120 ms", nm, (double) pr->r.before, (double) pr->r.after,
                         pubR, j.first, mid, phi, j.endDb);
            for (const double s : { 0.0, 0.010, 0.030, 0.060, 0.120 }) std::printf (" %+.3f", atTime (pr->r, pr->t, s, sr2));
            std::printf ("\n");
        }
        check (premise, "premise (7): each flushed lane had a result >= 3 dB from 0 dB to lose, and its first block "
                        "publishes the predict floor (<= -3 dB)");
        check (zero, "(7) a re-prepare at a NEW rate (48 -> 44.1 kHz) with Level Match on still flushes: published exactly "
                     "0 dB after prepare");
        check (glides, "(7) ...the applied gain starts at unity (|first sample| <= 0.01 dB) and, at -70 / -90 / -125 dBFS, "
                       "glides (phi 0.3-0.7 at 60 ms) to the published value by the end of the ramp: no snap -- each "
                       "quiet level is below the silence detector");
        check (snaps, "(7) ...and at the pre-roll's level the same flush snaps at its first sample (the detector's "
                      "silence->audio edge)");
    }

    // =====================================================================================================
    //  LEG (8) -- an invalid result falls back to unity
    // =====================================================================================================
    {
        const Params h8 = haas (8.0f, true);
        const float inf = std::numeric_limits<float>::infinity();
        // (8a) +Inf injected un-ducked, Level Match off, on audio: NaN at the boundary; primed Level Match on
        auto a8 = pairOf (offOf (h8), h8, RS, sc70, false,
                          [RS, inf] (int b, AnamorphEngine& e, Params&) { if (b == RS - 8) e.injectMatchGainDb (inf); });
        // (8b) a NaN burst whose self-heal flushes the matcher, re-prepared on the next block
        auto b8 = pairOf (h8, h8, RS, sc70, false, nullptr);
        b8->r.nanAt = b8->t.nanAt = RS - 1;
        // (8c) a primed measurement change (Drive 8 -> 10)
        auto c8 = pairOf (h8, haas (10.0f, true), RS, sc70, false, nullptr);
        runPair (*a8); runPair (*b8); runPair (*c8);
        bool nanHeld = true;
        for (size_t b = (size_t) RS - 8; b < (size_t) RS; ++b)
            nanHeld = nanHeld && std::isnan (a8->r.pub[b]) && std::isnan (a8->t.pub[b]);
        bool unity = true, zero = true;
        const struct { const char* name; const Pair* p; } legs8[] = {
            { "(8a) +Inf injected, Level Match off -> NaN", a8.get() },
            { "(8b) NaN burst, self-heal, re-prepare", b8.get() },
            { "(8c) primed Drive 8 -> 10", c8.get() } };
        for (const auto& lg : legs8)
        {
            const Judged j = judge (lg.p->r, lg.p->t, 0.0, 0, 1, sr);
            const double pubR = lg.p->r.pub[(size_t) RS];
            double e0 = 0.0;                                             // the run's first-block energy (0: silent)
            for (int i = 0; i < blk; ++i) e0 += (double) lg.p->r.y[(size_t) i] * lg.p->r.y[(size_t) i];
            std::printf ("  %-52s: published %+.4f -> %+.4f, first block %+.4f | applied at the first sample ", lg.name,
                         (double) lg.p->r.before, (double) lg.p->r.after, pubR);
            if (j.firstAt >= 0) std::printf ("%+.4f  at 60 ms %+.3f\n", j.first, atTime (lg.p->r, lg.p->t, 0.06, sr));
            else                std::printf ("none (first-block output energy %.1e: the run is %s)\n", e0,
                                             e0 > 0.0 ? "not a gain of its twin" : "silent");
            zero  = zero && juce::exactlyEqual (lg.p->r.after, 0.0f);
            unity = unity && j.ok && j.firstAt < 2 && std::abs (j.first) <= 0.01 && pubR <= -3.0;
        }
        const bool healed = juce::exactlyEqual (b8->r.pub[(size_t) RS - 1], 0.0f) && b8->r.pub[(size_t) RS - 2] <= -3.0f;
        check (nanHeld && std::isnan (a8->r.before),
               "premise (8a): a NON-FINITE published value reaches prepare() through the engine API -- +Inf injected "
               "un-ducked on audio with Level Match off reads NaN on every block after it (no self-heal: the output "
               "plays Output Gain)");
        check (healed, "premise (8b): the NaN burst's self-heal flushed the matcher in its own process() call (published "
                       "exactly 0 dB there, from <= -3 dB)");
        check (std::isfinite (c8->r.before) && c8->r.before <= -3.0f,
               "premise (8c): the primed-change lane had a result (<= -3 dB) to flush");
        check (zero, "(8) each invalid result is not applied: published exactly 0 dB after the re-prepare (the NaN and the "
                     "primed change flush; the self-heal's 0 dB is what there is to keep)");
        check (unity, "(8) ...and the applied gain falls back to unity at the first sample (<= 0.01 dB) while the first "
                      "block publishes the predict floor (<= -3 dB): Level Match on, nothing applied from the invalid "
                      "value");
    }

    // =====================================================================================================
    //  LEG (9) -- an injection right after a kept re-prepare still wins
    // =====================================================================================================
    {
        const Params h8 = haas (8.0f, true);
        float vA = 0.0f, vB = 0.0f, vAt = 0.0f, vBt = 0.0f;
        // In the first block after the re-prepare: inject (the engine's own published value + off) into `v`,
        // behind a forced duck when `duck` (the A/B shape). Each lane records its own value.
        const auto injectAt = [RS] (float& v, float off, bool duck) -> At
        {
            return [RS, &v, off, duck] (int b, AnamorphEngine& e, Params&)
            {
                if (b != RS) return;
                v = e.getMatchGainDb() + off;
                if (duck) e.requestDuck();
                e.injectMatchGainDb (v);
            };
        };
        auto a9 = pairOf (h8, h8, RS, sc70, false, injectAt (vA, -5.0f, false), injectAt (vAt, -5.0f, false));
        auto b9 = pairOf (h8, h8, RS, sc70, false, injectAt (vB, 4.0f, true), injectAt (vBt, 4.0f, true));
        runPair (*a9); runPair (*b9);
        const Judged jA = judge (a9->r, a9->t, (double) vA, 0, J, sr);
        const Judged jB = judge (b9->r, b9->t, (double) vB, kFull, J, sr);
        const bool pubA = holdsAt (a9->r, RS, RS + J, vA);
        const bool pubB = holdsAt (b9->r, RS + kBot, RS + J, vB) && same (b9->r.pub[(size_t) (RS + kBot - 1)], b9->r.after);
        std::printf ("  %-52s: injected %+.4f (kept %+.4f); published holds it: %s; applied first %+.4f  max|D| %.4f\n",
                     "(9) un-ducked injection in the first block", (double) vA, (double) a9->r.after, pubA ? "yes" : "no",
                     jA.first, jA.maxErr);
        std::printf ("  %-52s: injected %+.4f (kept %+.4f); published from the bottom: %s; applied from full level "
                     "max|D| %.4f\n", "(9) A/B shape (requestDuck + inject) in the first block", (double) vB,
                     (double) b9->r.after, pubB ? "yes" : "no", jB.maxErr);
        check (same (vA, vAt) && same (vB, vBt) && same (a9->r.after, a9->r.before) && same (b9->r.after, b9->r.before),
               "premise (9): both re-prepares kept the result, and each twin injected the identical value");
        check (pubA && jA.ok && jA.firstAt < 2 && jA.maxErr <= kTol,
               "(9) an un-ducked injection right after a kept re-prepare wins: published bit-exact and applied (every "
               "sample within 0.02 dB) at the injected value from its first sample");
        check (pubB && jB.ok && jB.maxErr <= kTol,
               "(9) an A/B-shaped injection right after a kept re-prepare wins: published bit-exact from its bottom "
               "block, applied from its first full-level block");
    }
    // =====================================================================================================
    //  LEG (11) -- the Level-Match state that decides is the one reset() SETTLES (the engine API, unprimed)
    // =====================================================================================================
    // prepare() may run with a duck in flight and nothing primed: reset() adopts pendingP, so p.autoGainMatch
    // is final only after it. Here Level Match was off; setParameters (Level Match on, nothing else) opened its
    // ordinary duck, and prepare() (same rate, keepMatch: nothing primed, the value finite) resolves it. The
    // settled state has Level Match on, so the kept value is applied from the first sample. The twin opens the
    // same duck with an inert band-count move. Kills the write placed before reset() (p still off there: the
    // smoother parks at unity and glides the 0.12 s ramp at -70 dBFS).
    {
        Params t11 = haas (8.0f, false);
        t11.mbBands = 3;
        auto e11 = pairOf (haas (8.0f, false), haas (8.0f, true), RS, sc70, false, nullptr);
        e11->t.post = t11;
        e11->r.prime = e11->t.prime = false;
        runPair (*e11);
        const Judged j = judge (e11->r, e11->t, (double) e11->r.after, 0, J, sr);
        const bool keep = same (e11->r.before, e11->r.after) && same (e11->r.after, e11->t.after)
                       && std::abs (e11->r.after) >= 3.0f;
        std::printf ("  %-52s: published %+.4f -> %+.4f (kept: %s) | applied: first %+.4f  max|D| %.4f  LS max|D| %.4f\n",
                     "(11) unprimed, a Match-on duck in flight at prepare()", (double) e11->r.before,
                     (double) e11->r.after, keep ? "yes" : "no", j.first, j.maxErr, j.lsErr);
        check (keep && j.ok && j.firstAt < 2 && std::abs (j.first - e11->r.after) <= kTol && j.maxErr <= kTol,
               "(11) a kept re-prepare that resolves an in-flight Level-Match engage (unprimed engine API) applies the "
               "kept value from the first sample: the Level-Match state reset() settles decides");
    }
}

static int runForcedSwapAuditProbe()
{
    std::printf ("Forced-swap audit (A/B, preset recall, undo). 220 Hz, block 64, 48 kHz.\n");
    std::printf ("  level = min 128-smp RMS after the swap / settled RMS\n");
    std::printf ("  side  = min 128-smp SIDE rms after the swap / settled side rms  (1.0 = image intact)\n");
    std::printf ("  step  = worst |x[n]-x[n-1]| after / worst in a settled window\n\n");
    constexpr double sr = 48000.0; constexpr int bs = 64;

    struct Case { const char* name; anamorph::EngineParameters from, to; };
    std::vector<Case> cases;
    {
        anamorph::EngineParameters base;
        base.algorithm = anamorph::Algorithm::Haas; base.algoAmount = 0.7f;
        base.width = 1.4f; base.mix = 0.85f;

        auto mk = [&] (const char* nm, auto fromFn, auto toFn)
        { Case c; c.name = nm; c.from = base; c.to = base; fromFn (c.from); toFn (c.to); cases.push_back (c); };

        mk ("A/B sound only, OS Off",            [](auto&){},
            [](auto& q){ q.width = 1.8f; q.algoAmount = 0.35f; q.mix = 0.6f; });
        mk ("A/B sound only, OS 4x + drive 6",   [](auto& q){ q.oversample = anamorph::OversampleFactor::x4; q.driveDb = 6.0f; },
            [](auto& q){ q.oversample = anamorph::OversampleFactor::x4; q.driveDb = 6.0f;
                         q.width = 1.8f; q.algoAmount = 0.35f; q.mix = 0.6f; });
        mk ("A/B sound only, OS 4x + drive 0",   [](auto& q){ q.oversample = anamorph::OversampleFactor::x4; },
            [](auto& q){ q.oversample = anamorph::OversampleFactor::x4;
                         q.width = 1.8f; q.algoAmount = 0.35f; q.mix = 0.6f; });
        mk ("A/B crossing drive 0<->6, OS 4x",   [](auto& q){ q.oversample = anamorph::OversampleFactor::x4; q.driveDb = 0.0f; },
            [](auto& q){ q.oversample = anamorph::OversampleFactor::x4; q.driveDb = 6.0f; });
        mk ("A/B changing the FACTOR Off->4x",   [](auto&){},
            [](auto& q){ q.oversample = anamorph::OversampleFactor::x4; });
        mk ("A/B changing the FACTOR 2x->4x",    [](auto& q){ q.oversample = anamorph::OversampleFactor::x2; q.driveDb = 6.0f; },
            [](auto& q){ q.oversample = anamorph::OversampleFactor::x4; q.driveDb = 6.0f; });
        mk ("A/B changing the ALGORITHM",        [](auto&){},
            [](auto& q){ q.algorithm = anamorph::Algorithm::Velvet; });
    }

    for (const auto& c : cases)
    {
        anamorph::AnamorphEngine e;
        auto p = c.from;
        e.primeParameters (p); e.prepare (sr, bs); e.setParameters (p);
        std::vector<float> l, r; l.reserve (51200); r.reserve (51200);
        juce::AudioBuffer<float> buf (2, bs);
        double phase = 0.0; const double inc = 2.0 * 3.14159265358979 * 220.0 / sr;
        const int settle = 400, tail = 400, swapAt = 400;
        for (int n = 0; n < settle + tail; ++n)
        {
            for (int i = 0; i < bs; ++i)
            {   // a slightly decorrelated stereo source so SIDE energy exists to lose
                const float a = 0.5f * (float) std::sin (phase);
                const float b = 0.5f * (float) std::sin (phase * 1.5 + 0.7);
                phase += inc;
                buf.setSample (0, i, a); buf.setSample (1, i, b);
            }
            if (n == swapAt) { e.requestDuck(); p = c.to; }
            e.setParameters (p); e.process (buf);
            for (int i = 0; i < bs; ++i) { l.push_back (buf.getSample (0, i)); r.push_back (buf.getSample (1, i)); }
        }
        const int sw = swapAt * bs;
        auto winRms = [&] (const std::vector<float>& v, int at, int w)
        { double s = 0.0; for (int k = 0; k < w; ++k) { const double x = v[(size_t)(at + k)]; s += x * x; } return std::sqrt (s / w); };
        auto winSide = [&] (int at, int w)
        { double s = 0.0; for (int k = 0; k < w; ++k)
          { const double d = 0.5 * (l[(size_t)(at + k)] - r[(size_t)(at + k)]); s += d * d; } return std::sqrt (s / w); };

        double refRms = 0.0, refSide = 0.0, refStep = 0.0;
        for (int i = sw - 100 * bs; i < sw - 128; i += 32)
        { refRms = juce::jmax (refRms, winRms (l, i, 128)); refSide = juce::jmax (refSide, winSide (i, 128)); }
        for (int i = sw - 100 * bs + 1; i < sw; ++i)
            refStep = juce::jmax (refStep, (double) std::abs (l[(size_t) i] - l[(size_t) i - 1]));

        double minRms = 1e9, minSide = 1e9, worstStep = 0.0;
        for (int i = sw; i + 128 < (int) l.size(); ++i)
        { minRms = juce::jmin (minRms, winRms (l, i, 128)); minSide = juce::jmin (minSide, winSide (i, 128)); }
        for (int i = sw; i < (int) l.size(); ++i)
            worstStep = juce::jmax (worstStep, (double) std::abs (l[(size_t) i] - l[(size_t) i - 1]));

        std::printf ("  %-32s level %.4f (%+6.1f dB) | side %.4f (%+6.1f dB) | step x%.2f\n", c.name,
                     minRms / refRms,  20.0 * std::log10 (juce::jmax (1e-12, minRms  / refRms)),
                     minSide / refSide, 20.0 * std::log10 (juce::jmax (1e-12, minSide / refSide)),
                     worstStep / juce::jmax (1e-9, refStep));
    }
    return 0;
}

// Oversampling 2x/4x/8x -> Off: does the PROCESSING survive the handoff?
// The human-readable trace behind Test 54, on the same instrument (see the notes
// on OsSwitchTrace / h3Over1 above). The last two rows are CONTROLS: the identical
// switch duck, with the identical reported-latency step and the identical resets,
// between two states that BOTH run the wrap -- so anything the duck itself costs
// appears on them too.
static int runOsOffHandoffProbe()
{
    std::printf ("Oversampling -> Off handoff: is the DRIVE still processing?\n");
    std::printf ("  H3/H1 by Hann-windowed Goertzel (256 smp) -- a ratio, so the duck's fade cancels.\n");
    std::printf ("  1 kHz mono sine at 0.6, Drive 18 dB, Haas. ms is relative to the duck's silent bottom.\n\n");
    constexpr double sr = 48000.0;
    // Every direction the OS path can be switched in, not only the reported one:
    // out of the wrap, INTO it, and between two factors (the controls).
    const anamorph::OversampleFactor from[7] = { anamorph::OversampleFactor::x2,
                                                 anamorph::OversampleFactor::x4,
                                                 anamorph::OversampleFactor::x8,
                                                 anamorph::OversampleFactor::Off,
                                                 anamorph::OversampleFactor::Off,
                                                 anamorph::OversampleFactor::x2,
                                                 anamorph::OversampleFactor::x4 };
    const anamorph::OversampleFactor to[7]   = { anamorph::OversampleFactor::Off,
                                                 anamorph::OversampleFactor::Off,
                                                 anamorph::OversampleFactor::Off,
                                                 anamorph::OversampleFactor::x2,
                                                 anamorph::OversampleFactor::x8,
                                                 anamorph::OversampleFactor::x4,
                                                 anamorph::OversampleFactor::x2 };
    const char* name[7] = { "2x -> Off", "4x -> Off", "8x -> Off",
                            "Off -> 2x", "Off -> 8x",
                            "2x -> 4x  (control)", "4x -> 2x  (control)" };

    std::printf ("      %-20s", "ms from bottom");
    for (int ms = -4; ms <= 24; ms += 2) std::printf (" %+5d", ms);
    std::printf ("\n");
    for (int o = 0; o < 7; ++o)
    {
        const auto t = runOsSwitch (from[o], to[o], anamorph::Algorithm::Haas, 18.0f);
        double ref = 0.0, worst = 0.0;
        for (int i = t.change - 6400 + 1; i < t.change; ++i)
            ref = juce::jmax (ref, (double) std::abs (t.l[(size_t) i] - t.l[(size_t) (i - 1)]));
        for (int i = t.change + 1; i < (int) t.l.size(); ++i)
            worst = juce::jmax (worst, (double) std::abs (t.l[(size_t) i] - t.l[(size_t) (i - 1)]));
        std::printf ("  H3/H1 %-20s", name[o]);
        for (int ms = -4; ms <= 24; ms += 2)
        {
            const int at = t.bottom + (int) (ms * sr / 1000.0);
            std::printf (" %5.3f", (at >= 0 && at + 256 < (int) t.l.size()) ? h3Over1 (t, at) : 0.0);
        }
        std::printf ("\n  rms   %-20s", "");
        for (int ms = -4; ms <= 24; ms += 2)
        {
            const int at = t.bottom + (int) (ms * sr / 1000.0);
            std::printf (" %5.3f", (at >= 0 && at + 256 < (int) t.l.size()) ? blockRms (t.l, at, 256) : 0.0);
        }
        std::printf ("   worst step x%.2f\n", worst / juce::jmax (1.0e-9, ref));
    }
    return 0;
}

int main (int argc, char* argv[])
{
    // A CRASH MUST NOT TAKE THE LOG WITH IT (D-2 round 13). Windows' CRT buffers
    // stdout fully when it is a pipe -- which every CI runner is -- so a suite that
    // dies mid-run loses everything written since the last flush. That is exactly how
    // a round-12 Windows failure arrived: one truncated line, no summary, and no way
    // to tell which test had been running. Unbuffered output costs nothing measurable
    // for a few thousand short lines and makes every future failure readable at the
    // point it happened, on every platform, without a `stdbuf` wrapper the Windows job
    // cannot use anyway.
    std::setvbuf (stdout, nullptr, _IONBF, 0);

    if (argc > 1 && std::strcmp (argv[1], "--match-inject-probe") == 0)
        return runMatchInjectProbe();

    if (argc > 1 && std::strcmp (argv[1], "--os-latency-probe") == 0)
        return runOsLatencyProbe();

    if (argc > 1 && std::strcmp (argv[1], "--forced-swap-probe") == 0)
        return runForcedSwapAuditProbe();

    if (argc > 1 && std::strcmp (argv[1], "--os-off-probe") == 0)
        return runOsOffHandoffProbe();

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
    testCorrelationBalanceExtremeFiniteInput();
    testInputConditioningAndCharacterParams();
    testRestoredModulesDoNotGlideIn();
    testResetClearsPendingForcedDuck();
    testPendingDuckDoesNotSurviveActivation();
    testOversamplingLatencyIsFactorOnly();
    testDriveCrossingIsSeamlessWithOversampling();
    testOversamplingOffHandoffKeepsProcessing();
    testInertDiscreteChangeDoesNotDuck();
    testMultibandEnableDrySourceNoStep();
    testAlgoResetSurvivesMidFadeRetarget();
    testInertDimModeDoesNotReArmLevelMatch();
    testNonFiniteBurstSelfHeals();
    testEngagedWrapCarriesTheReportedLatency();
    testScopeRingHandsTheNewestFramesOldestFirst();
    testHostResetChorusSeedIsScoped();
    testHostResetInAForcedSwapLandsSettled();
    testNonFiniteGlideTargetsDoNotLatch();
    testNonFiniteBurstKeepsLevelMatchAudible();
    testLevelMatchEngagesAtTheLevelItMeasured();
    testLevelMatchAbRearmAndSameRateReprepare();
    testLevelMatchKeptResultIsTheAppliedGainFromTheFirstBlock();
    testAbActiveClampOnCorruptState(); // state-restoration robustness (not a DSP test)

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    if (ftzUnavailable)
        std::printf ("(ANAMORPH_TESTS_NO_FTZ=1 was set: the denormal invariant was NOT asserted)\n");
    if (failures == 0) { std::printf ("ALL TESTS PASSED\n"); return 0; }
    std::printf ("TESTS FAILED\n");
    return 1;
}
