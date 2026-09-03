// ============================================================================
//  AnamorphBench — the measurement procedure PERFORMANCE_BUDGET.md prescribes.
//
//  WHY IT EXISTS. That document's CPU/memory rows have been explicit TODOs
//  since they were written, for one stated reason: "no repeatable benchmark is
//  committed to this repository ... every number quoted in the Wave 3-6
//  worklogs came from a session-local scratch harness that was never checked
//  in". This is that harness, committed. It changes no shipped behaviour and
//  builds only when asked (`-DANAMORPH_BUILD_BENCH=ON`).
//
//  IT MEASURES THE ENGINE, NOT THE PLUG-IN. `anamorph::AnamorphEngine` is the
//  unit the budget document names: no wrapper, no GUI, no host. The calling
//  pattern is `tests/dsp_tests.cpp`'s — prepare, setParameters, process on a
//  pre-sized stereo buffer.
//
//  THE BUFFER IS ALLOCATED ONCE, OUTSIDE THE TIMED REGION, and the stimulus is
//  written into it before the clock starts. The point is to measure the engine,
//  not `AudioBuffer`'s constructor or a noise generator.
//
//  A NUMBER WITHOUT ITS MACHINE IS NOT A MEASUREMENT (constraint C2). This
//  refuses to print a table it cannot label: if the CPU model cannot be read
//  and `ANAMORPH_BENCH_CPU` is unset, it exits 2 rather than emitting a
//  complete-looking result that identifies nothing.
//
//  WHY THE MATRIX IS NOT THE FULL CROSS PRODUCT. The prescribed axes (4 sample
//  rates x 4 block sizes x 4 algorithms x 4 oversampling factors x 3 multiband
//  states) multiply out to 768 cells, which is a number nobody reads and an hour
//  nobody spends. Each SECTION below instead varies ONE axis around a fixed
//  48 kHz / 128-sample reference, which is what makes a row interpretable: the
//  difference between two rows in a section is the axis, because nothing else
//  moved. The two paths the budget document calls out by name -- oversampling
//  engaged with Drive > 0, and the RISK-002 multiband split drag -- get their
//  own sections.
//
//  DETERMINISTIC STIMULUS: a fixed-seed LCG plus a fixed tone, regenerated
//  identically for every repetition, so two runs differ only by the machine.
// ============================================================================

#include "dsp/AnamorphEngine.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#if JUCE_LINUX || JUCE_BSD
 #include <fstream>
#elif JUCE_MAC || JUCE_IOS
 #include <sys/sysctl.h>
#elif JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
    // ---- C2: identify the machine, or refuse ------------------------------
    std::string cpuModel()
    {
       #if JUCE_LINUX || JUCE_BSD
        std::ifstream in ("/proc/cpuinfo");
        std::string line;
        while (std::getline (in, line))
            if (line.rfind ("model name", 0) == 0)
                return line.substr (line.find (':') + 2);
       #elif JUCE_MAC || JUCE_IOS
        std::size_t len = 0;
        if (sysctlbyname ("machdep.cpu.brand_string", nullptr, &len, nullptr, 0) == 0 && len > 1)
        {
            std::string out (len - 1, '\0');
            if (sysctlbyname ("machdep.cpu.brand_string", out.data(), &len, nullptr, 0) == 0)
                return out;
        }
       #elif JUCE_WINDOWS
        HKEY key {};
        if (RegOpenKeyExA (HKEY_LOCAL_MACHINE,
                           "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                           0, KEY_READ, &key) == ERROR_SUCCESS)
        {
            char buf[256] {};
            DWORD size = sizeof (buf), type = 0;
            const bool ok = RegQueryValueExA (key, "ProcessorNameString", nullptr, &type,
                                              reinterpret_cast<LPBYTE> (buf), &size) == ERROR_SUCCESS
                            && type == REG_SZ;
            RegCloseKey (key);
            if (ok) return std::string (buf);
        }
       #endif
        return {};
    }

    // How much audio each repetition processes. The budget document asks for
    // >= 10 s; `ANAMORPH_BENCH_SECONDS` shortens it for the CI lane, where the
    // question is "did this regress" rather than "what is the number".
    double audioSeconds()
    {
        if (const char* s = std::getenv ("ANAMORPH_BENCH_SECONDS"); s != nullptr && *s != 0)
        {
            const double v = std::atof (s);
            if (v > 0.0) return v;
        }
        return 10.0;
    }

    int repetitions()
    {
        if (const char* s = std::getenv ("ANAMORPH_BENCH_REPS"); s != nullptr && *s != 0)
        {
            const int v = std::atoi (s);
            if (v > 0) return v;
        }
        return 5;
    }

    using anamorph::Algorithm;
    using anamorph::OversampleFactor;

    const char* algoName (Algorithm a)
    {
        switch (a)
        {
            case Algorithm::Haas:       return "Haas";
            case Algorithm::Velvet:     return "Velvet";
            case Algorithm::Chorus:     return "Chorus";
            case Algorithm::DimensionD: return "Dim-D";
        }
        return "?";
    }

    const char* osName (OversampleFactor f)
    {
        switch (f)
        {
            case OversampleFactor::Off: return "Off";
            case OversampleFactor::x2:  return "2x";
            case OversampleFactor::x4:  return "4x";
            case OversampleFactor::x8:  return "8x";
        }
        return "?";
    }

    // A benchmark cell: one configuration, one row of output.
    struct Cell
    {
        std::string label;                  // what this row varies
        double sr           = 48000.0;
        int    block        = 128;
        anamorph::EngineParameters p {};
        bool   dragSplits   = false;        // RISK-002: move a crossover per block
    };

    struct Result { double medianNsPerSample; double worstBlockUs; double spreadPct; };

    // The transparent default configuration: every stage at identity.
    anamorph::EngineParameters defaultsParams()
    {
        return anamorph::EngineParameters {};
    }

    // A representative "working" configuration -- the one a user who bought the
    // plug-in for its effect actually runs: widening engaged, multiband on,
    // level match on. This is the reference for every section below.
    anamorph::EngineParameters workingParams()
    {
        anamorph::EngineParameters p;
        p.algorithm     = Algorithm::Velvet;
        p.algoAmount    = 0.6f;
        p.width         = 1.4f;
        p.mix           = 0.85f;
        p.mbEnable      = true;
        p.mbBands       = 4;
        p.monoMakerEnable = true;
        p.autoGainMatch = true;
        return p;
    }

    Result measure (const Cell& c)
    {
        const int reps       = repetitions();
        const double seconds = audioSeconds();
        const int blocks     = juce::jmax (1, (int) (seconds * c.sr / c.block));

        std::vector<double> nsPerSample;
        nsPerSample.reserve ((size_t) reps);
        double worstUs = 0.0;

        for (int r = 0; r < reps; ++r)
        {
            anamorph::AnamorphEngine engine;
            engine.prepare (c.sr, c.block);
            anamorph::EngineParameters p = c.p;
            engine.setParameters (p);
            engine.reset();

            // Allocated ONCE, before the clock: see the header note.
            juce::AudioBuffer<float> buf (2, c.block);

            std::uint32_t rng = 0x12345u;
            double sumNs = 0.0;
            std::int64_t samples = 0;

            for (int b = 0; b < blocks; ++b)
            {
                for (int n = 0; n < c.block; ++n)
                {
                    rng = rng * 1664525u + 1013904223u;
                    const float noise = ((float) (rng >> 8) / 8388608.0f - 1.0f) * 0.05f;
                    const float tone  = 0.2f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                         * 220.0f * (float) (b * c.block + n)
                                                         / (float) c.sr);
                    buf.setSample (0, n, tone + noise);
                    buf.setSample (1, n, tone - noise);
                }

                // RISK-002: a crossover that MOVES is the documented hot path --
                // per-sample coefficient recompute under the rate cap. Sweeping
                // it here is the difference between this row and the static one.
                if (c.dragSplits)
                {
                    const float t = (float) (b % 120) / 120.0f;
                    p.mbFreqMid = 400.0f + 3000.0f * t;
                    engine.setParameters (p);
                }

                const auto t0 = std::chrono::steady_clock::now();
                engine.process (buf);
                const auto t1 = std::chrono::steady_clock::now();

                const double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>
                                      (t1 - t0).count();
                sumNs   += ns;
                samples += c.block;
                worstUs  = std::max (worstUs, ns / 1000.0);
            }

            nsPerSample.push_back (sumNs / (double) samples);
        }

        std::sort (nsPerSample.begin(), nsPerSample.end());
        const double median = nsPerSample[nsPerSample.size() / 2];
        const double lo = nsPerSample.front(), hi = nsPerSample.back();
        // Spread is reported so a reader can tell a real change from noise --
        // which is the whole reason a single run is not a datum.
        const double spread = median > 0.0 ? (hi - lo) / median * 100.0 : 0.0;
        return { median, worstUs, spread };
    }

    void row (const Cell& c)
    {
        const Result r = measure (c);
        // Percentage of ONE core at this sample rate: a block must be finished
        // within block/sr seconds, so cost/budget = (ns/sample * sr) / 1e9.
        const double pct = r.medianNsPerSample * c.sr / 1.0e7;
        std::printf ("| %-22s | %6.0f | %4d | %8.2f | %7.1f | %6.1f%% | %5.2f%% |\n",
                     c.label.c_str(), c.sr, c.block,
                     r.medianNsPerSample, r.worstBlockUs, r.spreadPct, pct);
        std::fflush (stdout);
    }

    void header (const char* section)
    {
        std::printf ("\n### %s\n\n", section);
        std::printf ("| config | SR | block | ns/sample | worst blk us | spread | %% core |\n");
        std::printf ("|---|---|---|---|---|---|---|\n");
    }
}

int main()
{
    std::string machine = cpuModel();
    if (machine.empty())
        if (const char* forced = std::getenv ("ANAMORPH_BENCH_CPU"); forced != nullptr && *forced != 0)
            machine = forced;

    if (machine.empty())
    {
        std::fprintf (stderr,
                      "AnamorphBench: could not identify this CPU, so the run would violate "
                      "PERFORMANCE_BUDGET.md constraint C2 (a number without its machine is not "
                      "a measurement).\nAdd a lookup for this platform in cpuModel(), or set "
                      "ANAMORPH_BENCH_CPU to the model string and re-run.\n");
        return 2;
    }

    std::printf ("AnamorphBench -- machine: %s, %d cores, %s %s\n",
                 machine.c_str(), (int) std::thread::hardware_concurrency(),
#if defined (__clang__)
                 "clang", __clang_version__
#elif defined (__GNUC__)
                 "gcc", __VERSION__
#else
                 "cc", "?"
#endif
                 );
    std::printf ("method: %d reps/cell, %.1f s of audio each; ns/sample is the MEDIAN rep, "
                 "worst block is the max single process() across all reps, spread is "
                 "(max-min)/median across reps. %% core = cost / (block/SR) budget.\n",
                 repetitions(), audioSeconds());

    const auto working = workingParams();

    header ("Reference and idle paths");
    { Cell c; c.label = "transparent defaults"; c.p = defaultsParams();      row (c); }
    { Cell c; c.label = "working (reference)";  c.p = working;               row (c); }
    { Cell c; c.label = "bypass";               c.p = working; c.p.bypass = true; row (c); }

    header ("Algorithm (48 kHz / 128, working)");
    for (auto a : { Algorithm::Haas, Algorithm::Velvet, Algorithm::Chorus, Algorithm::DimensionD })
    { Cell c; c.label = algoName (a); c.p = working; c.p.algorithm = a; row (c); }

    header ("Sample rate (128 samples, working)");
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    { Cell c; c.label = "SR sweep"; c.sr = sr; c.p = working; row (c); }

    header ("Block size (48 kHz, working)");
    for (int b : { 32, 64, 128, 256 })
    { Cell c; c.label = "block sweep"; c.block = b; c.p = working; row (c); }

    // The wrap only engages for nonlinear/modulation work, so Drive > 0 is what
    // makes these rows mean anything -- an oversampling row at Drive 0 measures
    // the bypassed wrapper (PERFORMANCE_BUDGET.md, the Verified row).
    header ("Oversampling, engaged with Drive > 0 (48 kHz / 128)");
    for (auto f : { OversampleFactor::Off, OversampleFactor::x2,
                    OversampleFactor::x4,  OversampleFactor::x8 })
    { Cell c; c.label = std::string ("OS ") + osName (f); c.p = working;
      c.p.driveDb = 8.0f; c.p.oversample = f; row (c); }

    // The state ADR-0034 created and the reason it is cheap. A factor SELECTED
    // with the wrap SKIPPED (Drive 0, linear algorithm) now carries the factor's
    // latency through `osCompDelayBuffer` instead of through the resampling round
    // trip. These rows are what says the CPU saving survived the latency change:
    // they belong beside the OS Off row above, not beside the engaged rows -- if
    // they ever start tracking the engaged rows, the wrap has started running.
    header ("Oversampling SELECTED but skipped, Drive 0 -- the ADR-0034 stand-in (48 kHz / 128)");
    for (auto f : { OversampleFactor::Off, OversampleFactor::x2,
                    OversampleFactor::x4,  OversampleFactor::x8 })
    { Cell c; c.label = std::string ("OS ") + osName (f) + ", drive 0"; c.p = working;
      c.p.driveDb = 0.0f; c.p.algorithm = Algorithm::Haas; c.p.oversample = f; row (c); }

    header ("Multiband, including the RISK-002 split drag (48 kHz / 128)");
    { Cell c; c.label = "multiband off";      c.p = working; c.p.mbEnable = false; row (c); }
    { Cell c; c.label = "4 bands, static";    c.p = working;                        row (c); }
    { Cell c; c.label = "4 bands, DRAGGING";  c.p = working; c.dragSplits = true;   row (c); }

    std::printf ("\nAnamorphBench: done.\n");
    return 0;
}
