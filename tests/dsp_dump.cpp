// ============================================================================
//  AnamorphDspDump -- the DSP bit-identity harness, committed.
//
//  WHY IT EXISTS. `DEPENDENCY_POLICY.md` rule 2 makes bit-identical engine
//  output the gate a JUCE bump has to pass, and the 8.0.14 -> 9.0.0 and
//  9.0.0 -> 9.0.1 migrations both passed it -- with a SCRATCHPAD tool that was
//  never committed, "per the `xbench.cpp` precedent" (worklogs/
//  JUCE9_MIGRATION_v0.8.13.md §3). So the gate exists, the evidence exists, and
//  the instrument does not. Every future bump has had to rebuild it.
//
//  REBUILDING IT IS NOT FREE, and the worklog records exactly how it goes
//  wrong: on the FIRST run the scenario set left `algoAmount` at its 0 default,
//  which is identity for the wet path, so the four algorithms hashed the same
//  as each other. Nothing failed. The tool reported 32 matching hashes and
//  proved nothing at all, because the code under test was never reached. That
//  defect was caught by a human noticing two rows that should differ did not --
//  which is not a thing to rely on twice.
//
//  SO THIS TOOL CHECKS ITS OWN SCENARIOS FIRST, and refuses to print a baseline
//  it has not shown to be discriminating: before any hash is emitted it asserts
//  that the scenarios produce DISTINCT output from each other along every axis
//  it varies. A scenario set that collapses is a broken instrument, and it exits
//  3 saying so rather than printing 32 confident and meaningless lines. This is
//  `TESTING_POLICY.md` rule 4 -- a checker must prove it is live before its
//  silence is trusted -- applied to a checker whose silence is a table of equal
//  hashes.
//
//  HOW IT IS USED (the "twin" part). This program prints one deterministic line
//  per scenario: an FNV-1a hash over every output byte, plus the engine's
//  reported latency. Build it against two JUCE checkouts with otherwise
//  identical flags, run both, and `diff` the two outputs. Identical output is
//  the proof; any differing line names the exact scenario to investigate.
//  `docs/procedures/TESTING.md` carries the step-by-step.
//
//  WHY A HASH AND NOT A GOLDEN FILE. A committed golden set of expected hashes
//  would be a golden-master DSP test, which this repository has deliberately
//  rejected: the numbers are legitimately allowed to change when the DSP
//  changes, and a baseline that must be regenerated on every intentional edit
//  trains people to regenerate it without looking. The question here is never
//  "does this match a stored value" -- it is "does build A match build B", and
//  only a diff between two runs answers that. Nothing is stored.
//
//  DETERMINISM IS THE WHOLE PRODUCT. Fixed-seed LCG noise, then digital
//  silence (which catches denormal and tail differences the noise phase hides),
//  fixed block size, fixed sample rate, no wall clock, no threads, no
//  environment reads. Two runs of the same binary must be byte-identical or
//  this tool cannot be used for its purpose -- and `--self-check` asserts
//  exactly that before anything else.
// ============================================================================

#include "dsp/AnamorphEngine.h"

#include <juce_dsp/juce_dsp.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
    using anamorph::Algorithm;
    using anamorph::OversampleFactor;

    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlock      = 512;
    constexpr int    kNoiseBlocks   = 120;
    constexpr int    kSilenceBlocks = 120;

    // ---- FNV-1a over the raw output bytes -------------------------------
    // Bytes, not floats: a difference in the low mantissa bit is a difference,
    // and comparing at any coarser resolution would answer a weaker question
    // than the one DEPENDENCY_POLICY rule 2 asks.
    struct Fnv
    {
        std::uint64_t h = 1469598103934665603ull;
        void feed (const void* data, std::size_t n) noexcept
        {
            const auto* p = static_cast<const unsigned char*> (data);
            for (std::size_t i = 0; i < n; ++i)
            {
                h ^= p[i];
                h *= 1099511628211ull;
            }
        }
    };

    struct Scenario
    {
        std::string name;
        anamorph::EngineParameters p;
    };

    // The stimulus. A fixed-seed LCG rather than <random>: the standard
    // library's generators are specified, but the DISTRIBUTIONS are not, and a
    // libstdc++/libc++ difference in `uniform_real_distribution` would show up
    // as a DSP difference this tool is meant to be measuring.
    struct Lcg
    {
        std::uint32_t s = 0x13579BDFu;
        float next() noexcept
        {
            s = s * 1664525u + 1013904223u;
            return (float) ((double) (s >> 8) / (double) (1u << 24)) * 2.0f - 1.0f;
        }
    };

    std::uint64_t runScenario (const anamorph::EngineParameters& params, int& latencyOut)
    {
        anamorph::AnamorphEngine engine;
        engine.prepare (kSampleRate, kBlock);
        engine.setParameters (params);
        engine.reset();
        latencyOut = engine.getLatencySamples();

        juce::AudioBuffer<float> buf (2, kBlock);
        Fnv hash;
        Lcg lcg;

        for (int b = 0; b < kNoiseBlocks + kSilenceBlocks; ++b)
        {
            const bool silent = (b >= kNoiseBlocks);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int i = 0; i < kBlock; ++i)
                    d[i] = silent ? 0.0f : lcg.next() * 0.5f;
            }

            engine.process (buf);

            for (int ch = 0; ch < 2; ++ch)
                hash.feed (buf.getReadPointer (ch), sizeof (float) * (std::size_t) kBlock);
        }
        return hash.h;
    }

    // The scenario matrix: 4 algorithms x 4 oversampling factors x M/S off/on.
    // Every one of them ENGAGES the wet path -- see the header: a matrix that
    // does not is a matrix that proves nothing.
    std::vector<Scenario> scenarios()
    {
        const Algorithm algos[] { Algorithm::Haas, Algorithm::Velvet,
                                  Algorithm::Chorus, Algorithm::DimensionD };
        const char* algoNames[] { "haas", "velvet", "chorus", "dimd" };
        const OversampleFactor os[] { OversampleFactor::Off, OversampleFactor::x2,
                                      OversampleFactor::x4, OversampleFactor::x8 };
        const char* osNames[] { "os1", "os2", "os4", "os8" };

        std::vector<Scenario> out;
        for (int a = 0; a < 4; ++a)
            for (int o = 0; o < 4; ++o)
                for (int ms = 0; ms < 2; ++ms)
                {
                    anamorph::EngineParameters p;
                    p.algorithm  = algos[a];
                    p.oversample = os[o];
                    p.msMode     = (ms != 0);

                    // These are what make the scenario DISCRIMINATING, and each
                    // is here for a reason rather than for variety:
                    p.algoAmount = 0.7f;   // the wet path is identity at 0
                    p.driveDb    = 8.0f;   // oversampling only wraps nonlinear work
                    p.width      = 1.6f;
                    p.mix        = 0.8f;   // a dry/wet blend exercises the PDC align
                    p.mbEnable   = true;   // the multiband chain
                    p.mbBands    = 4;
                    p.mbWidthLow = 0.7f;
                    p.mbWidthHigh = 1.5f;
                    p.monoMakerEnable = true;
                    p.autoGainMatch   = true;

                    out.push_back ({ std::string (algoNames[a]) + "-" + osNames[o]
                                     + (ms ? "-ms" : "-lr"), p });
                }
        return out;
    }

    // ---- the instrument checks itself before it reports ------------------
    //
    // Two independent properties, because they fail independently:
    //   1. REPEATABLE -- the same scenario run twice must hash the same. If it
    //      does not, every diff this tool produces is noise.
    //   2. DISCRIMINATING -- distinct scenarios must hash differently. If they
    //      do not, every diff this tool produces is empty for the wrong reason,
    //      which is the failure that actually happened in 2026.
    int selfCheck()
    {
        const auto cases = scenarios();
        int lat = 0;

        // 1. Repeatability, on one representative scenario.
        const auto first  = runScenario (cases.front().p, lat);
        const auto second = runScenario (cases.front().p, lat);
        if (first != second)
        {
            std::fprintf (stderr,
                "dsp-dump self-check FAILED: '%s' is not repeatable (%016llx vs %016llx).\n"
                "This tool compares two BUILDS; a scenario that differs from itself makes\n"
                "every such comparison meaningless. Something in the engine or the harness\n"
                "is reading state this program does not control.\n",
                cases.front().name.c_str(),
                (unsigned long long) first, (unsigned long long) second);
            return 3;
        }

        // 2. Discrimination, across the whole matrix.
        std::map<std::uint64_t, std::string> seen;
        std::vector<std::pair<std::string, std::string>> collisions;
        for (const auto& c : cases)
        {
            const auto h = runScenario (c.p, lat);
            const auto it = seen.find (h);
            if (it != seen.end())
                collisions.push_back ({ it->second, c.name });
            else
                seen.emplace (h, c.name);
        }

        if (! collisions.empty())
        {
            std::fprintf (stderr,
                "dsp-dump self-check FAILED: %zu scenario pair(s) produce IDENTICAL output,\n"
                "so those rows cannot detect a difference between two builds:\n",
                collisions.size());
            for (const auto& pair : collisions)
                std::fprintf (stderr, "    %s  ==  %s\n", pair.first.c_str(), pair.second.c_str());
            std::fprintf (stderr,
                "\nThis is the 2026 defect, not a new one: a matrix that leaves the wet path\n"
                "at identity hashes the same everywhere and reports a confident nothing.\n"
                "Fix the scenario set -- do NOT relax this check.\n");
            return 3;
        }

        std::printf ("dsp-dump self-check passed: %zu scenarios, all repeatable and all distinct.\n",
                     cases.size());
        return 0;
    }
}

int main (int argc, char** argv)
{
    const bool selfCheckOnly = (argc > 1 && std::strcmp (argv[1], "--self-check") == 0);

    // ALWAYS, not only on request: a dump whose instrument has not been checked
    // is the failure mode this file exists to prevent, and the check costs one
    // extra pass over a matrix that already runs in seconds.
    const int rc = selfCheck();
    if (rc != 0 || selfCheckOnly)
        return rc;

    std::printf ("\n# AnamorphDspDump -- %.0f Hz, %d-sample blocks, %d noise + %d silence blocks\n",
                 kSampleRate, kBlock, kNoiseBlocks, kSilenceBlocks);
    std::printf ("# scenario                 fnv1a-64            latency\n");

    for (const auto& c : scenarios())
    {
        int latency = 0;
        const auto h = runScenario (c.p, latency);
        std::printf ("%-24s  %016llx  %7d\n",
                     c.name.c_str(), (unsigned long long) h, latency);
    }
    return 0;
}
