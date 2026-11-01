#pragma once

// Opt-in, nonshipping extension of AnamorphBench. No production code or build
// target changes. The legacy overlay is SERIAL ONLY; it must never be lapped
// by a concurrent reader. Reader traffic below is not a GUI or a DAW callback.
#ifndef ANAMORPH_NATIVE_SCOPE_LEGACY
 #define ANAMORPH_NATIVE_SCOPE_LEGACY 0
#endif
#if ! ANAMORPH_NATIVE_SCOPE_LEGACY
 #include "../src/gui/ScopeSnapshotCache.h"
#endif
#include <atomic>
#include <bit>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#if JUCE_MAC
 #include <mach/mach.h>
#elif JUCE_WINDOWS
 #include <psapi.h>
#endif

namespace native_validation
{
using Clock = std::chrono::steady_clock;
constexpr bool legacy = ANAMORPH_NATIVE_SCOPE_LEGACY != 0;
inline const void* volatile escaped = nullptr;

// Outside the clock. Keep the engine's writes observable without reading its
// scope after every block (which would introduce hidden reader/cache traffic).
inline void escape (const void* pointer) noexcept
{
    escaped = pointer;
    std::atomic_signal_fence (std::memory_order_seq_cst);
   #if defined (__GNUC__) || defined (__clang__)
    asm volatile ("" : : "g" (pointer) : "memory");
   #endif
}

inline void quoted (const std::string& text)
{
    std::putchar ('"');
    for (char character : text)
    {
        const auto c = static_cast<unsigned char> (character);
        if (c == '"' || c == '\\') std::printf ("\\%c", c);
        else if (c < 32) std::printf ("\\u%04x", static_cast<unsigned> (c));
        else std::putchar (c);
    }
    std::putchar ('"');
}

inline std::uint64_t rssBytes()
{
   #if JUCE_LINUX
    std::ifstream file ("/proc/self/status");
    std::string key;
    while (file >> key)
    {
        if (key == "VmRSS:") { std::uint64_t kb = 0; file >> kb; return kb * 1024; }
        std::string rest; std::getline (file, rest);
    }
   #elif JUCE_MAC
    mach_task_basic_info_data_t info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO,
                   reinterpret_cast<task_info_t> (&info), &count) == KERN_SUCCESS)
        return static_cast<std::uint64_t> (info.resident_size);
   #elif JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS info {};
    if (K32GetProcessMemoryInfo (GetCurrentProcess(), &info, static_cast<DWORD> (sizeof (info))) != 0)
        return static_cast<std::uint64_t> (info.WorkingSetSize);
   #endif
    return 0; // Reported as unavailable, never interpreted as a zero footprint.
}

struct Options
{
    std::string kind = "engine", scenario = "working", reader = "none";
    double rate = 48000, seconds = 10;
    int block = 128, window = 8192, iterations = 2000, repetition = 0;
    bool smoke = false, paced = false;
};

inline Options parse (int argc, char* argv[])
{
    Options o;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--smoke") { o.smoke = true; continue; }
        if (arg == "--paced") { o.paced = true; continue; }
        if (++i == argc) throw std::runtime_error ("missing option value");
        const std::string value = argv[i];
        const auto number = [&value]
        {
            std::size_t used = 0;
            const double v = std::stod (value, &used);
            if (used != value.size() || ! std::isfinite (v))
                throw std::runtime_error ("invalid finite number");
            return v;
        };
        const auto integer = [&number]
        {
            const double v = number();
            if (v < 0 || v > std::numeric_limits<int>::max() || std::floor (v) != v)
                throw std::runtime_error ("invalid nonnegative integer");
            return static_cast<int> (v);
        };
        if      (arg == "--kind")       o.kind = value;
        else if (arg == "--case")       o.scenario = value;
        else if (arg == "--reader")     o.reader = value;
        else if (arg == "--rate")       o.rate = number();
        else if (arg == "--seconds")    o.seconds = number();
        else if (arg == "--block")      o.block = integer();
        else if (arg == "--window")     o.window = integer();
        else if (arg == "--iterations") o.iterations = integer();
        else if (arg == "--rep")        o.repetition = integer();
        else throw std::runtime_error ("unknown option");
    }
    if (o.kind != "scope-push" && o.kind != "scope-read" && o.kind != "engine" && o.kind != "memory")
        throw std::runtime_error ("unknown measurement kind");
    if (o.reader != "none" && o.reader != "active" && o.reader != "stalled")
        throw std::runtime_error ("unknown reader mode");
    if (legacy && o.reader != "none")
        throw std::runtime_error ("legacy ScopeBuffer is serial-only: concurrent reads would be a data race");
    if (o.reader != "none" && o.kind != "engine")
        throw std::runtime_error ("reader load belongs to the corrected engine probe only");
    if (o.block < 1 || o.block > 65536 || o.window < 1 || o.window > anamorph::ScopeBuffer::capacity
        || o.rate < 8000 || o.rate > 192000
        || o.seconds <= 0 || o.seconds > 600 || o.iterations < 1 || o.iterations > 1000000)
        throw std::runtime_error ("operating point outside bounded probe limits");
    if (! o.smoke && (o.seconds < 10 || o.iterations < 2000))
        throw std::runtime_error ("acceptance collection requires >=10 seconds of audio and >=2000 primitive calls");
    if (o.reader != "none" && o.seconds < 1.1)
        throw std::runtime_error ("reader probe needs >=1.1 seconds to exercise pause and recovery");
    if (o.reader != "none" && ! o.paced)
        throw std::runtime_error ("reader load requires real-time pacing outside process()");
    return o;
}

inline Cell cellFor (const Options& o)
{
    Cell c; c.label = o.scenario; c.sr = o.rate; c.block = o.block; c.p = workingParams();
    if      (o.scenario == "defaults") c.p = defaultsParams();
    else if (o.scenario == "bypass") c.p.bypass = true;
    else if (o.scenario == "haas") c.p.algorithm = anamorph::Algorithm::Haas;
    else if (o.scenario == "velvet") c.p.algorithm = anamorph::Algorithm::Velvet;
    else if (o.scenario == "chorus") c.p.algorithm = anamorph::Algorithm::Chorus;
    else if (o.scenario == "dimensional") c.p.algorithm = anamorph::Algorithm::Dimensional;
    else if (o.scenario == "mb-off") c.p.mbEnable = false;
    else if (o.scenario == "mb-drag") c.dragSplits = true;
    else if (o.scenario == "os2" || o.scenario == "os4" || o.scenario == "os8")
    {
        c.p.driveDb = 8;
        c.p.oversample = o.scenario == "os2" ? anamorph::OversampleFactor::x2
                      : o.scenario == "os4" ? anamorph::OversampleFactor::x4 : anamorph::OversampleFactor::x8;
    }
    else if (o.scenario == "selected2" || o.scenario == "selected4" || o.scenario == "selected8")
    {
        c.p.algorithm = anamorph::Algorithm::Haas;
        c.p.driveDb = 0;
        c.p.oversample = o.scenario == "selected2" ? anamorph::OversampleFactor::x2
                      : o.scenario == "selected4" ? anamorph::OversampleFactor::x4 : anamorph::OversampleFactor::x8;
    }
    else if (o.scenario != "working" && o.scenario != "silence")
        throw std::runtime_error ("unknown engine case");
    return c;
}

inline void stimulus (juce::AudioBuffer<float>& buffer, std::uint32_t& rng,
                      std::int64_t frame, double rate, bool silent)
{
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        rng = rng * 1664525u + 1013904223u;
        const float noise = (static_cast<float> (rng >> 8) / 8388608.0f - 1) * 0.05f;
        const float tone = 0.2f * std::sin (2 * juce::MathConstants<float>::pi * 220
                                          * static_cast<float> (frame + n) / static_cast<float> (rate));
        buffer.setSample (0, n, silent ? 0 : tone + noise);
        buffer.setSample (1, n, silent ? 0 : tone - noise);
    }
}

inline std::uint64_t hashAudio (const juce::AudioBuffer<float>& buffer, std::uint64_t hash)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float value = buffer.getSample (channel, n);
            if (! std::isfinite (value)) throw std::runtime_error ("nonfinite engine output");
            hash = (hash ^ std::bit_cast<std::uint32_t> (value)) * 1099511628211ULL;
        }
    return hash;
}

struct Reader
{
    std::atomic<bool> stop { false };
    std::thread worker;
    std::uint64_t accepted = 0, rejected = 0, pauseBefore = 0, pauseAfter = 0;
    std::uint64_t checksum = 14695981039346656037ULL;
    ~Reader() { stop.store (true, std::memory_order_release); if (worker.joinable()) worker.join(); }
    void join() { stop.store (true, std::memory_order_release); if (worker.joinable()) worker.join(); }
    void start (anamorph::ScopeBuffer& scope, const std::string& mode)
    {
       #if ! ANAMORPH_NATIVE_SCOPE_LEGACY
        if (mode == "none") return;
        // One serialized reader actor models the two peeks. It owns these
        // caches exclusively until join, as in the existing concurrency tests.
        auto vectorscope = std::make_unique<anamorph::gui::ScopeSnapshotCache> (8000);
        auto spectrum = std::make_unique<anamorph::gui::ScopeSnapshotCache> (8192);
        worker = std::thread ([this, &scope, mode, a = std::move (vectorscope), b = std::move (spectrum)]
        {
            const auto startTime = Clock::now();
            auto next = startTime;
            bool paused = false;
            while (! stop.load (std::memory_order_acquire))
            {
                if (mode == "stalled" && ! paused
                    && Clock::now() - startTime >= std::chrono::milliseconds (100))
                {
                    pauseBefore = scope.writeCount();
                    std::this_thread::sleep_for (std::chrono::milliseconds (750));
                    pauseAfter = scope.writeCount(); paused = true;
                    next = Clock::now();
                }
                for (auto* cache : { a.get(), b.get() })
                {
                    const auto result = cache->readLatest (scope, cache == a.get() ? 8000 : 8192);
                    if (cache->accept (result))
                    {
                        ++accepted;
                        // Keep every accepted sample observable under LTO.
                        // This models copy + consumption, not FFT/render cost.
                        for (int i = 0; i < result.copied; ++i)
                        {
                            const auto index = static_cast<std::size_t> (i);
                            checksum = (checksum ^ std::bit_cast<std::uint32_t> (cache->left()[index])) * 1099511628211ULL;
                            checksum = (checksum ^ std::bit_cast<std::uint32_t> (cache->right()[index])) * 1099511628211ULL;
                        }
                    }
                    else ++rejected;
                }
                next += std::chrono::microseconds (16667);
                std::this_thread::sleep_until (next);
            }
        });
       #else
        (void) scope; (void) mode;
       #endif
    }
};

inline void samples (const std::vector<std::int64_t>& values)
{
    std::printf ("\"block_ns\":[");
    for (std::size_t i = 0; i < values.size(); ++i)
        std::printf ("%s%lld", i == 0 ? "" : ",", static_cast<long long> (values[i]));
    std::putchar (']');
}

inline int run (int argc, char* argv[], const std::string& cpu)
{
    try
    {
        const auto o = parse (argc, argv);
        const auto c = cellFor (o);
        std::vector<std::int64_t> timings;
        std::uint64_t hash = 14695981039346656037ULL;
        std::uint64_t inputHash = hash;
        int latencySamples = 0;
        std::uint64_t rssBefore = 0, rssConstructed = 0, rssPrepared = 0;
        Reader reader;
        double measuredSamples = 0;
        // Report an un-subtracted empty clock-pair calibration. Clock cost
        // belongs to the observations, particularly very short primitive calls.
        std::int64_t clockMin = std::numeric_limits<std::int64_t>::max(), clockMax = 0;
        for (int i = 0; i < 1000; ++i)
        {
            const auto a = Clock::now(), b = Clock::now();
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds> (b - a).count();
            clockMin = std::min (clockMin, static_cast<std::int64_t> (ns));
            clockMax = std::max (clockMax, static_cast<std::int64_t> (ns));
        }
        if (o.kind == "scope-push" || o.kind == "scope-read")
        {
            auto ring = std::make_unique<anamorph::ScopeBuffer>();
            std::vector<float> left (static_cast<std::size_t> (std::max (o.block, anamorph::ScopeBuffer::capacity)), 0.25f);
            std::vector<float> right (left.size(), -0.5f), outL (static_cast<std::size_t> (o.window)), outR (outL.size());
            ring->pushBlock (left.data(), right.data(), anamorph::ScopeBuffer::capacity);
            timings.resize (static_cast<std::size_t> (o.iterations));
            for (auto& duration : timings)
            {
                escape (ring.get());
                const auto a = Clock::now();
                if (o.kind == "scope-push") ring->pushBlock (left.data(), right.data(), o.block);
                else if (ring->readLatest (outL.data(), outR.data(), o.window) != o.window)
                    throw std::runtime_error ("serial read did not return a complete window");
                const auto b = Clock::now();
                duration = std::chrono::duration_cast<std::chrono::nanoseconds> (b - a).count();
                escape (ring.get()); escape (outL.data());
            }
            if (ring->readLatest (outL.data(), outR.data(), o.window) != o.window)
                throw std::runtime_error ("missing final scope window");
            for (std::size_t i = 0; i < outL.size(); ++i)
                if (std::bit_cast<std::uint32_t> (outL[i]) != std::bit_cast<std::uint32_t> (0.25f)
                    || std::bit_cast<std::uint32_t> (outR[i]) != std::bit_cast<std::uint32_t> (-0.5f))
                    throw std::runtime_error ("scope data control failed");
            measuredSamples = static_cast<double> (o.kind == "scope-read" ? o.window : o.block) * o.iterations;
        }
        else
        {
            rssBefore = rssBytes();
            auto engine = std::make_unique<anamorph::AnamorphEngine>();
            rssConstructed = rssBytes();
            engine->prepare (o.rate, o.block); engine->setParameters (c.p); engine->reset();
            engine->setTransportPlaying (o.scenario != "silence");
            rssPrepared = rssBytes();
            if (o.kind == "engine")
            {
                juce::AudioBuffer<float> buffer (2, o.block);
                std::uint32_t rng = 0x12345u;
                const auto blocks = static_cast<int> (std::ceil (o.seconds * o.rate / o.block));
                timings.resize (static_cast<std::size_t> (blocks));
                // A full second of untimed settling, with identical stimuli in
                // each variant. The idle row is actually stopped/silent.
                const int warm = static_cast<int> (std::ceil (o.rate / o.block));
                for (int b = 0; b < warm; ++b)
                {
                    stimulus (buffer, rng, static_cast<std::int64_t> (b) * o.block, o.rate, o.scenario == "silence");
                    engine->process (buffer);
                }
                reader.start (engine->getScopeBuffer(), o.reader);
                const auto wallStart = Clock::now();
                for (int b = 0; b < blocks; ++b)
                {
                    stimulus (buffer, rng, static_cast<std::int64_t> (warm + b) * o.block, o.rate, o.scenario == "silence");
                    if (c.dragSplits)
                    {
                        auto p = c.p; p.mbFreqMid = 400 + 3000 * static_cast<float> (b % 120) / 120;
                        engine->setParameters (p);
                    }
                    inputHash = hashAudio (buffer, inputHash);
                    escape (engine.get());
                    const auto a = Clock::now(); engine->process (buffer); const auto end = Clock::now();
                    timings[static_cast<std::size_t> (b)] = std::chrono::duration_cast<std::chrono::nanoseconds> (end - a).count();
                    escape (engine.get());
                    hash = hashAudio (buffer, hash); // Outside the timed region.
                    if (o.paced)
                        std::this_thread::sleep_until (wallStart + std::chrono::nanoseconds (
                            static_cast<std::int64_t> ((b + 1.0) * o.block * 1.0e9 / o.rate)));
                }
                reader.join();
                if (o.reader != "none" && reader.accepted == 0)
                    throw std::runtime_error ("reader load never accepted a window");
                if (o.reader == "stalled" && reader.pauseAfter <= reader.pauseBefore)
                    throw std::runtime_error ("producer made no progress during reader stall");
                measuredSamples = static_cast<double> (blocks) * o.block;
                if (o.scenario == "working" && hash == inputHash)
                    throw std::runtime_error ("working engine control is inert (output equals its input)");
            }
            latencySamples = engine->getLatencySamples();
        }
        std::printf ("{\"schema\":1,\"variant\":\"%s\",\"kind\":", legacy ? "original" : "corrected"); quoted (o.kind);
        std::printf (",\"case\":"); quoted (o.scenario); std::printf (",\"reader\":"); quoted (o.reader);
        std::printf (",\"cpu\":"); quoted (cpu);
        std::printf (",\"rate\":%.17g,\"block\":%d,\"window\":%d,\"paced\":%s,\"seconds\":%.17g,\"rep\":%d,\"smoke\":%s,"
                     "\"samples\":%.17g,\"engine_bytes\":%zu,\"scope_bytes\":%zu,"
                     "\"rss_before\":%llu,\"rss_constructed\":%llu,\"rss_prepared\":%llu,\"rss_available\":%s,"
                     "\"reader_accepted\":%llu,\"reader_rejected\":%llu,\"stall_frames\":%llu,"
                     "\"clock_min_ns\":%lld,\"clock_max_ns\":%lld,\"latency_samples\":%d,"
                     "\"input_hash\":\"%016llx\",\"reader_hash\":\"%016llx\",\"audio_hash\":\"%016llx\",",
                     o.rate, o.block, o.window, o.paced ? "true" : "false", o.seconds, o.repetition,
                     o.smoke ? "true" : "false", measuredSamples,
                     sizeof (anamorph::AnamorphEngine), sizeof (anamorph::ScopeBuffer),
                     static_cast<unsigned long long> (rssBefore), static_cast<unsigned long long> (rssConstructed),
                     static_cast<unsigned long long> (rssPrepared), rssPrepared > 0 ? "true" : "false",
                     static_cast<unsigned long long> (reader.accepted), static_cast<unsigned long long> (reader.rejected),
                     static_cast<unsigned long long> (reader.pauseAfter - reader.pauseBefore),
                     static_cast<long long> (clockMin), static_cast<long long> (clockMax),
                     latencySamples, static_cast<unsigned long long> (inputHash),
                     static_cast<unsigned long long> (reader.checksum), static_cast<unsigned long long> (hash));
        samples (timings); std::printf ("}\n");
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf (stderr, "native validation probe: %s\n", e.what()); return 2;
    }
}
} // namespace native_validation
