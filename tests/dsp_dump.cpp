// ============================================================================
//  AnamorphDspDump -- the DSP bit-identity harness, committed.
//
//  WHY IT EXISTS. `DEPENDENCY_POLICY.md` rule 2 makes bit-identical engine
//  output the gate a JUCE bump has to pass, and the 8.0.14 -> 9.0.0 and
//  9.0.0 -> 9.0.1 migrations both passed it -- with a SCRATCHPAD tool that was
//  never committed, "per the `xbench.cpp` precedent" (worklogs/
//  JUCE9_MIGRATION_0.8.13.md §3). So the gate exists, the evidence exists, and
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
#include <array>
#include <bit>
#include <cmath>
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
                                  Algorithm::Chorus, Algorithm::Dimensional };
        const char* algoNames[] { "haas", "velvet", "chorus", "dimensional" };
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
    // NR-011 is additive: legacy stdout remains the same 32 three-field rows.
    // --transitions emits five separate four-field rows. Both modes always run
    // these same transition controls; default mode keeps their success silent.
    constexpr int kTransitionBlock = 128;
    constexpr int kPhaseBlocks = 32;
    constexpr int kTransitionBlocks = 8 * kPhaseBlocks;
    enum class Transition { BypassMix, AlgorithmOs, Crossover, SoloBands, ResetIdle };
    enum class Control { Normal, Inert, ChangedSample, ChangedLatency };
    enum class Action { None, Bypass, Mix, Algorithm, Os, Low, Mid, High, Bands, Solo,
                        Enable, ResetTails, ResetAll, Transport };
    constexpr std::array<const char*, 14> actionNames { "none", "bypass", "mix", "algorithm", "OS",
        "low split", "mid split", "high split", "band count", "solo", "multiband enable",
        "tail reset", "all reset", "transport" };
    struct OmittedAction
    {
        Action action = Action::None;
        int first = 0, end = 0; // half-open block range; unrelated actions remain live
    };
    struct ActionCase
    {
        OmittedAction omit;
        bool requireAudio = true; // masked parameter changes use actual adoption instead
    };

    std::vector<ActionCase> actionCases (Transition kind)
    {
        std::vector<ActionCase> cases;
        const auto phase = [&] (Action action, int at, bool audible = true)
        { cases.push_back ({ { action, at * kPhaseBlocks, (at + 1) * kPhaseBlocks }, audible }); };
        switch (kind)
        {
            case Transition::BypassMix:
                for (int at : { 2, 4, 6, 7 }) phase (Action::Bypass, at, at != 2); // Mix=0 masks phase 2
                for (int at : { 1, 3, 5, 7 }) phase (Action::Mix, at, at != 3);   // bypass masks phase 3
                break;
            case Transition::AlgorithmOs:
                for (int at = 1; at <= 6; ++at)
                { phase (Action::Algorithm, at); phase (Action::Os, at); }
                break;
            case Transition::Crossover:
                for (int at = 1; at <= 6; ++at)
                    for (auto action : { Action::Low, Action::Mid, Action::High }) phase (action, at);
                break;
            case Transition::SoloBands:
                for (int at = 1; at <= 7; ++at) phase (Action::Solo, at, at != 6); // disabled in phase 6
                for (int at : { 3, 4, 5 }) phase (Action::Bands, at);
                for (int at : { 6, 7 }) phase (Action::Enable, at);
                break;
            case Transition::ResetIdle:
                cases.push_back ({ { Action::ResetTails, 64, 65 }, true });
                cases.push_back ({ { Action::ResetAll, 128, 129 }, true });
                // Only edges call setTransportPlaying. A missing resume stays
                // missing until the next edge; repeated holds cannot repair it.
                for (int block : { 64, 96, 160, 224, 240 })
                    cases.push_back ({ { Action::Transport, block, block + 1 }, true });
                break;
        }
        return cases;
    }
    constexpr std::array<Transition, 5> transitions { Transition::BypassMix, Transition::AlgorithmOs,
        Transition::Crossover, Transition::SoloBands, Transition::ResetIdle };
    constexpr std::array<const char*, 5> transitionNames { "transition-bypass-mix", "transition-algorithm-os",
        "transition-crossover", "transition-solo-bands", "transition-reset-idle" };

    struct TransitionResult
    {
        std::uint64_t audio = 0, latencyTrace = 0, adoptedParameters = 0;
        int finalLatency = 0;
        bool valid = true;
    };

    TransitionResult runTransition (Transition kind, Control control = Control::Normal,
                                    OmittedAction omitted = {})
    {
        anamorph::EngineParameters p;
        p.algorithm = kind == Transition::ResetIdle ? Algorithm::Velvet : Algorithm::Haas;
        p.algoAmount = 0.7f;
        p.width = 1.6f;
        p.mix = 0.8f;
        p.mbEnable = kind != Transition::ResetIdle;
        p.mbWidthLow = 0.4f; p.mbWidthMid = 1.8f;
        p.mbWidthHiMid = 0.6f; p.mbWidthHigh = 1.5f;
        // Bypass/Mix keeps a selected-but-skipped oversampling path: Haas is
        // linear and Drive is zero, but selected factor must still supply PDC.
        if (kind == Transition::BypassMix) p.oversample = OversampleFactor::x4;
        if (kind == Transition::AlgorithmOs) p.driveDb = 8.0f;
        if (kind == Transition::ResetIdle) p.mix = 1.0f;

        anamorph::AnamorphEngine engine;
        engine.primeParameters (p); // only before prepare, never a midstream shortcut
        engine.prepare (kSampleRate, kTransitionBlock);
        juce::AudioBuffer<float> audio (2, kTransitionBlock), empty (2, 0);
        Lcg lcg;
        Fnv samples, latency, adoption;
        TransitionResult result;
        const bool active = control != Control::Inert;
        bool previousTransportTarget = false;
        const auto feedLatency = [&] (int value)
        {
            // Explicit little-endian metadata encoding, separate from samples.
            const auto v = static_cast<std::uint32_t> (value);
            const std::array<unsigned char, 4> bytes { static_cast<unsigned char> (v),
                static_cast<unsigned char> (v >> 8), static_cast<unsigned char> (v >> 16),
                static_cast<unsigned char> (v >> 24) };
            latency.feed (bytes.data(), bytes.size());
        };

        for (int block = 0; block < kTransitionBlocks; ++block)
        {
            const int phase = block / kPhaseBlocks;
            const auto apply = [&] (Action action)
            { return active && ! (omitted.action == action && block >= omitted.first && block < omitted.end); };
            if (active && kind == Transition::BypassMix)
            {
                constexpr std::array<bool, 8> bypass { false, false, true, true, false, false, true, false };
                constexpr std::array<float, 8> mix { 0.8f, 0.0f, 0.0f, 1.0f, 1.0f, 0.35f, 0.35f, 0.8f };
                if (apply (Action::Bypass)) p.bypass = bypass[static_cast<std::size_t> (phase)];
                if (apply (Action::Mix)) p.mix = mix[static_cast<std::size_t> (phase)];
            }
            if (active && kind == Transition::AlgorithmOs)
            {
                constexpr std::array<Algorithm, 8> algo { Algorithm::Haas, Algorithm::Chorus,
                    Algorithm::Dimensional, Algorithm::Velvet, Algorithm::Haas, Algorithm::Chorus,
                    Algorithm::Haas, Algorithm::Haas };
                constexpr std::array<OversampleFactor, 8> factor { OversampleFactor::Off,
                    OversampleFactor::x2, OversampleFactor::x4, OversampleFactor::x8,
                    OversampleFactor::Off, OversampleFactor::x4, OversampleFactor::Off, OversampleFactor::Off };
                if (apply (Action::Algorithm)) p.algorithm = algo[static_cast<std::size_t> (phase)];
                if (apply (Action::Os)) p.oversample = factor[static_cast<std::size_t> (phase)];
            }
            if (active && kind == Transition::Crossover)
            {
                constexpr std::array<float, 8> low { 180, 70, 350, 140, 400, 70, 180, 180 };
                constexpr std::array<float, 8> mid { 800, 600, 1500, 900, 1900, 600, 800, 800 };
                constexpr std::array<float, 8> high { 3000, 2000, 8000, 3500, 10000, 2000, 3000, 3000 };
                const auto at = static_cast<std::size_t> (phase);
                const auto prior = at == 0 ? at : at - 1;
                const float t = static_cast<float> (block % kPhaseBlocks + 1) / static_cast<float> (kPhaseBlocks);
                if (apply (Action::Low)) p.mbFreqLow = low[prior] + t * (low[at] - low[prior]);
                if (apply (Action::Mid)) p.mbFreqMid = mid[prior] + t * (mid[at] - mid[prior]);
                if (apply (Action::High)) p.mbFreqHigh = high[prior] + t * (high[at] - high[prior]);
            }
            if (active && kind == Transition::SoloBands)
            {
                constexpr std::array<int, 8> bands { 4, 4, 4, 2, 3, 4, 4, 4 };
                constexpr std::array<int, 8> solo { 0, 1, 0, 2, 4, 8, 0, 1 };
                if (apply (Action::Bands)) p.mbBands = bands[static_cast<std::size_t> (phase)];
                if (apply (Action::Solo)) p.mbSolo = solo[static_cast<std::size_t> (phase)];
                if (apply (Action::Enable)) p.mbEnable = phase != 6; // clear, disable, repeat cold engage
            }
            const bool idle = kind == Transition::ResetIdle && (phase == 2 || phase == 6);
            const bool silent = kind == Transition::ResetIdle && (phase == 1 || phase == 5);
            if (kind == Transition::ResetIdle && ((block == 64 && apply (Action::ResetTails))
                                                 || (block == 128 && apply (Action::ResetAll))))
                engine.reset (block == 64 ? anamorph::AnamorphEngine::ResetScope::audioTailsOnly
                                         : anamorph::AnamorphEngine::ResetScope::everything);
            // The last stop makes the preceding resume observable as well: if
            // resume is omitted, Velvet sees no subsequent falling edge. No
            // action labels/targets are hashed into the audio evidence.
            const bool playing = ! active || kind != Transition::ResetIdle
                || ! (phase == 2 || phase == 5 || phase == 6 || block >= 240);
            if (block == 0 || playing != previousTransportTarget)
                if (block == 0 || apply (Action::Transport)) engine.setTransportPlaying (playing);
            previousTransportTarget = playing;
            engine.setParameters (p);
            feedLatency (engine.getLatencySamples());
            auto& buffer = idle ? empty : audio;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample (ch, i, silent ? 0.0f : lcg.next() * 0.5f);
            engine.process (buffer);
            // An independent engine observation for deliberately audio-masked
            // writes. Hash named adopted fields, never requested targets/padding.
            const auto& adopted = engine.getAdoptedParameters();
            const std::array<std::uint32_t, 10> fields {
                static_cast<std::uint32_t> (adopted.bypass), std::bit_cast<std::uint32_t> (adopted.mix),
                static_cast<std::uint32_t> (adopted.algorithm), static_cast<std::uint32_t> (adopted.oversample),
                std::bit_cast<std::uint32_t> (adopted.mbFreqLow), std::bit_cast<std::uint32_t> (adopted.mbFreqMid),
                std::bit_cast<std::uint32_t> (adopted.mbFreqHigh), static_cast<std::uint32_t> (adopted.mbBands),
                static_cast<std::uint32_t> (adopted.mbSolo), static_cast<std::uint32_t> (adopted.mbEnable) };
            adoption.feed (fields.data(), fields.size() * sizeof (std::uint32_t)); // internal control only
            // Seeded instrument controls alter evidence only, never engine code.
            if (control == Control::ChangedSample && block == 96)
            {
                auto* sample = buffer.getWritePointer (0);
                sample[0] = std::bit_cast<float> (std::bit_cast<std::uint32_t> (sample[0]) ^ 1u);
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto* data = buffer.getReadPointer (ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    result.valid = result.valid && std::isfinite (data[i]);
                if (buffer.getNumSamples() > 0)
                    samples.feed (data, sizeof (float) * static_cast<std::size_t> (buffer.getNumSamples()));
            }
            const int actual = engine.getLatencySamples();
            result.valid = result.valid && actual >= 0;
            feedLatency (actual + (control == Control::ChangedLatency && block == 96 ? 1 : 0));
            // A full phase is longer than the documented discrete switch duck.
            // Check adoption/PDC after settling without freezing a filter delay number.
            if (block % kPhaseBlocks == kPhaseBlocks - 1)
                result.valid = result.valid && actual == engine.predictLatency (p);
        }
        result.audio = samples.h;
        result.latencyTrace = latency.h;
        result.adoptedParameters = adoption.h;
        result.finalLatency = engine.getLatencySamples();
        return result;
    }

    int transitionSelfCheck (bool quiet)
    {
        std::map<std::uint64_t, const char*> distinct;
        for (std::size_t i = 0; i < transitions.size(); ++i)
        {
            const auto a = runTransition (transitions[i]);
            const auto repeat = runTransition (transitions[i]);
            const auto inert = runTransition (transitions[i], Control::Inert);
            const auto sample = runTransition (transitions[i], Control::ChangedSample);
            const auto metadata = runTransition (transitions[i], Control::ChangedLatency);
            if (! a.valid || ! repeat.valid || ! inert.valid || ! sample.valid || ! metadata.valid
                || a.audio != repeat.audio || a.latencyTrace != repeat.latencyTrace
                || a.finalLatency != repeat.finalLatency || a.audio == inert.audio
                || a.adoptedParameters != repeat.adoptedParameters
                || a.audio == sample.audio || a.latencyTrace != sample.latencyTrace
                || a.audio != metadata.audio || a.latencyTrace == metadata.latencyTrace
                || ! distinct.emplace (a.audio, transitionNames[i]).second)
            {
                std::fprintf (stderr, "dsp-dump transition self-check FAILED: %s is invalid, inert, "
                    "not repeatable or its sample/latency controls are dead.\n", transitionNames[i]);
                return 3;
            }
            for (const auto& action : actionCases (transitions[i]))
            {
                const auto without = runTransition (transitions[i], Control::Normal, action.omit);
                if (! without.valid || (action.requireAudio ? a.audio == without.audio
                                                          : a.adoptedParameters == without.adoptedParameters))
                {
                    std::fprintf (stderr, "dsp-dump action self-check FAILED: %s, %s at block %d "
                        "has no %s effect when omitted alone.\n", transitionNames[i],
                        actionNames[static_cast<std::size_t> (action.omit.action)], action.omit.first,
                        action.requireAudio ? "audio" : "adopted-parameter");
                    return 3;
                }
            }
            if (transitions[i] == Transition::AlgorithmOs && a.latencyTrace == inert.latencyTrace)
            {
                std::fprintf (stderr, "dsp-dump transition self-check FAILED: selected OS latency never changed.\n");
                return 3;
            }
        }
        if (! quiet)
            std::printf ("dsp-dump transition self-check passed: 5 scenarios, all repeatable and discriminating.\n");
        return 0;
    }
}

int main (int argc, char** argv)
{
    const bool transitionMode = argc > 1 && std::strcmp (argv[1], "--transitions") == 0;
    if (transitionMode)
    {
        const int checked = transitionSelfCheck (false);
        if (checked != 0 || (argc > 2 && std::strcmp (argv[2], "--self-check") == 0)) return checked;
        std::printf ("# AnamorphDspDump transitions -- 48000 Hz, 128-sample blocks, 256 blocks\n");
        std::printf ("# scenario fnv1a-64 latency latency-trace-fnv1a-64\n");
        for (std::size_t i = 0; i < transitions.size(); ++i)
        {
            const auto r = runTransition (transitions[i]);
            if (! r.valid) return 3;
            std::printf ("%s %016llx %d %016llx\n", transitionNames[i],
                static_cast<unsigned long long> (r.audio), r.finalLatency,
                static_cast<unsigned long long> (r.latencyTrace));
        }
        return 0;
    }
    const bool selfCheckOnly = (argc > 1 && std::strcmp (argv[1], "--self-check") == 0);

    // ALWAYS, not only on request: a dump whose instrument has not been checked
    // is the failure mode this file exists to prevent, and the check costs one
    // extra pass over a matrix that already runs in seconds.
    const int rc = selfCheck();
    if (rc != 0) return rc;
    const int transientRc = transitionSelfCheck (true);
    if (transientRc != 0 || selfCheckOnly) return transientRc;

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
