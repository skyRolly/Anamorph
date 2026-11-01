#pragma once

#include "gui/ScopeSnapshotCache.h"
#include <tuple>

namespace anamorph
{
struct ScopeBufferTestAccess
{
    template<class Hook>
    static ScopeBuffer::ReadResult read (const ScopeBuffer& ring, float* l, float* r, int n, Hook hook)
    { return ring.copyAt (l, r, n, ring.writeCount(), hook); }

    template<class Hook>
    static void write (ScopeBuffer& ring, const float* l, const float* r, int n, Hook hook)
    { ring.pushImpl (l, r, n, hook); }

    static std::uint64_t tag (const ScopeBuffer& ring, std::uint64_t p)
    { return ring.slots[p & ScopeBuffer::mask].tag.load (std::memory_order_acquire); }
    static void tag (ScopeBuffer& ring, std::uint64_t p, std::uint64_t value)
    { ring.slots[p & ScopeBuffer::mask].tag.store (value, std::memory_order_release); }
};
}

namespace anamorph::gui
{
struct ScopeVisualTestAccess
{
    static void poll (Vectorscope& view) { view.pollNewSamples(); }
    static auto state (const Vectorscope& view)
    { return std::tuple { view.lastSeenCount, view.lastNonZero, view.lastFrameSilent, view.snapshot.result().end }; }
    static auto state (const SpectrumImager& view)
    { return std::tuple { view.lastSeenCount, view.lastNonZero, view.lastWindowSilent, view.snapshot.result().end }; }
    static bool transform (SpectrumImager& view) { return view.pushFFT(); }
    static const std::vector<float>& points (const Vectorscope& view) { return view.snapshot.left(); }
    static const std::vector<float>& magnitudes (const SpectrumImager& view) { return view.fftData; }
    static bool silent (const SpectrumImager& view) { return view.lastWindowSilent; }
};
}

static void testScopeSnapshotPublication()
{
    std::printf ("State test 150: scope windows reject reuse and visualizers retain accepted state (ADR-0070)\n");
    using anamorph::ScopeBuffer;
    using Access = anamorph::ScopeBufferTestAccess;
    using Visual = anamorph::gui::ScopeVisualTestAccess;
    constexpr int cap = ScopeBuffer::capacity;
    std::printf ("  scope sizeof=%zu processor sizeof=%zu cache header sizeof=%zu\n",
        sizeof (ScopeBuffer), sizeof (AnamorphAudioProcessor), sizeof (anamorph::gui::ScopeSnapshotCache));
    auto ring = std::make_unique<ScopeBuffer>();
    std::vector<float> l (cap + 17), r (l.size()), outL (cap), outR (cap);
    const auto fillFrames = [&] (std::uint64_t start)
    {
        for (std::size_t i = 0; i < l.size(); ++i)
        { l[i] = static_cast<float> (start + i); r[i] = -l[i]; }
    };
    const auto exact = [&] (ScopeBuffer::ReadResult read)
    {
        if (! read.valid) return false;
        for (int i = 0; i < read.copied; ++i)
        {
            const float expected = static_cast<float> (read.end - static_cast<std::uint64_t> (read.copied)
                                                       + static_cast<std::uint64_t> (i));
            if (std::bit_cast<std::uint32_t> (outL[static_cast<std::size_t> (i)]) != std::bit_cast<std::uint32_t> (expected)
                || std::bit_cast<std::uint32_t> (outR[static_cast<std::size_t> (i)]) != std::bit_cast<std::uint32_t> (-expected))
                return false;
        }
        return true;
    };

    auto read = ring->readLatestSnapshot (outL.data(), outR.data(), cap);
    check (read.valid && read.copied == 0 && read.end == 0, "scope: valid empty window is explicit");
    ring->pushBlock (nullptr, nullptr, 0);
    check (ring->writeCount() == 0, "scope: empty push never touches sample pointers");
    fillFrames (0); ring->pushBlock (l.data(), r.data(), cap);
    const auto before = ring->writeCount();
    fillFrames (before);
    read = Access::read (*ring, outL.data(), outR.data(), 1, [&] (int, bool after)
    { if (! after) ring->pushBlock (l.data(), r.data(), cap); });
    check (! read.valid && read.copied == 0 && read.end == before,
           "scope: reuse between first tag and payload rejects, with the original publication");
    read = ring->readLatestSnapshot (outL.data(), outR.data(), cap);
    check (exact (read), "scope: reader recovers on its next ordinary attempt");

    fillFrames (ring->writeCount());
    read = Access::read (*ring, outL.data(), outR.data(), cap, [&] (int i, bool after)
    { if (i == 0 && after) ring->pushBlock (l.data(), r.data(), cap); });
    check (! read.valid, "scope: a stalled reader rejects later-slot reuse after copying an old prefix");
    check (ring->writeCount() == 3 * static_cast<std::uint64_t> (cap), "scope: slow reader never holds back publication");

    // Observe the old published last frame while its replacement payload has
    // been stored but its new tag/index have not. Removing invalidation makes
    // this exact scheduled observation accept the wrong incarnation.
    const auto oldEnd = ring->writeCount();
    fillFrames (oldEnd);
    bool during = false, rejected = false;
    Access::write (*ring, l.data(), r.data(), cap, [&] (std::uint64_t pos)
    {
        if (pos == oldEnd + cap - 1)
        {
            during = true;
            rejected = ! ring->readLatestSnapshot (outL.data(), outR.data(), 1).valid;
        }
    });
    check (during && rejected, "scope: invalidation rejects an in-flight payload before new tag/index publication");

    const auto hint = ring->writeCount();
    fillFrames (hint); ring->pushBlock (l.data(), r.data(), 63);
    read = ring->readLatestSince (outL.data(), outR.data(), hint, cap);
    check (read.copied == 63 && read.end == hint + 63 && exact (read), "scope: fresh tail and end use one acquired publication");
    const auto end = read.end;
    read = ring->readLatestSince (outL.data(), outR.data(), end, cap);
    check (read.valid && read.copied == 0 && read.end == end, "scope: repeated tail peek is valid empty, not consumption");
    for (int n : { 1, 511, cap - 1, cap, cap + 17 })
    {
        fillFrames (ring->writeCount()); ring->pushBlock (l.data(), r.data(), n);
        read = ring->readLatestSnapshot (outL.data(), outR.data(), cap + 5);
        check (read.copied == cap && exact (read), "scope: wrap/clamp/oversized pushes preserve exact newest positions");
    }
    read = ring->readLatestSnapshot (nullptr, nullptr, 0);
    check (read.valid && read.copied == 0, "scope: zero request never touches destination pointers");

    constexpr std::array<std::uint32_t, 8> bits { 0, 0x80000000u, 0x3f800000u, 1,
        0x00800000u, 0x7f800000u, 0x7fc12345u, 0x7f812345u };
    for (std::size_t i = 0; i < bits.size(); ++i)
    { l[i] = std::bit_cast<float> (bits[i]); r[i] = std::bit_cast<float> (bits[i] ^ 0x80000000u); }
    ring->pushBlock (l.data(), r.data(), static_cast<int> (bits.size()));
    read = ring->readLatestSnapshot (outL.data(), outR.data(), static_cast<int> (bits.size()));
    bool preserved = read.valid;
    for (std::size_t i = 0; i < bits.size(); ++i)
        preserved = preserved && std::bit_cast<std::uint32_t> (outL[i]) == bits[i]
            && std::bit_cast<std::uint32_t> (outR[i]) == (bits[i] ^ 0x80000000u);
    check (preserved, "scope: stereo packing preserves zero/subnormal/Inf/NaN payload bits");

    // Prepare/reset must leave the display ring's lifetime/count intact.
    auto engine = std::make_unique<anamorph::AnamorphEngine>();
    engine->prepare (48000, 64);
    std::fill (l.begin(), l.end(), 0.25f); std::fill (r.begin(), r.end(), 0.25f);
    engine->getScopeBuffer().pushBlock (l.data(), r.data(), 64);
    engine->prepare (96000, 128); engine->reset();
    read = engine->getScopeBuffer().readLatestSnapshot (outL.data(), outR.data(), 64);
    check (read.valid && read.end == 64 && read.copied == 64 && outL[63] > 0.24f,
           "scope: re-prepare/reset preserve live ring history, with no reset writer");

    // Real offscreen component paint and activity polling use the same cache.
    auto visualRing = std::make_unique<ScopeBuffer>();
    anamorph::gui::Vectorscope vector (*visualRing);
    vector.setSize (240, 240);
    visualRing->pushBlock (l.data(), r.data(), 8192);
    Visual::poll (vector);
    juce::Image image (juce::Image::ARGB, 240, 240, true);
    juce::Graphics graphics (image);
    vector.paint (graphics);
    const auto oldVector = Visual::state (vector);
    const auto oldPoints = Visual::points (vector);
    std::fill (l.begin(), l.end(), 0.5f); std::fill (r.begin(), r.end(), 0.5f);
    visualRing->pushBlock (l.data(), r.data(), 64);
    const auto badPosition = visualRing->writeCount() - 1;
    const auto goodTag = Access::tag (*visualRing, badPosition);
    Access::tag (*visualRing, badPosition, 0);
    Visual::poll (vector); vector.paint (graphics);
    check (Visual::state (vector) == oldVector && Visual::points (vector) == oldPoints,
           "scope GUI: rejected tail/paint preserves activity and last accepted picture");
    Access::tag (*visualRing, badPosition, goodTag);
    Visual::poll (vector); vector.paint (graphics);
    check (std::get<0> (Visual::state (vector)) == 8256 && std::get<3> (Visual::state (vector)) == 8256,
           "scope GUI: restored validity retries at the same publication");

    auto processor = std::make_unique<AnamorphAudioProcessor>();
    auto& fftRing = processor->getEngine().getScopeBuffer();
    anamorph::gui::SpectrumImager imager (fftRing, processor->getAPVTS());
    std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f);
    fftRing.pushBlock (l.data(), r.data(), 8192);
    check (Visual::transform (imager) && Visual::silent (imager), "scope FFT: valid zero window enters analytical silence");
    const auto silentState = Visual::state (imager);
    const auto silentMagnitudes = Visual::magnitudes (imager);
    std::fill (l.begin(), l.end(), 0.25f); std::fill (r.begin(), r.end(), 0.25f);
    fftRing.pushBlock (l.data(), r.data(), 64);
    const auto wakeTag = Access::tag (fftRing, 100);
    Access::tag (fftRing, 100, 0); // outside valid new tail, inside full FFT window
    check (! Visual::transform (imager) && Visual::state (imager) == silentState
               && Visual::magnitudes (imager) == silentMagnitudes,
           "scope FFT: valid wakeup tail plus rejected full copy commits no state or magnitudes");
    Access::tag (fftRing, 100, wakeTag);
    check (Visual::transform (imager) && ! Visual::silent (imager), "scope FFT: wakeup retries successfully without another producer push");
    const auto activeState = Visual::state (imager);
    const auto activeMagnitudes = Visual::magnitudes (imager);
    std::fill (l.begin(), l.end(), 0.75f); std::fill (r.begin(), r.end(), 0.75f);
    fftRing.pushBlock (l.data(), r.data(), 64);
    const auto activePosition = fftRing.writeCount() - 1;
    const auto activeTag = Access::tag (fftRing, activePosition);
    Access::tag (fftRing, activePosition, 0);
    check (! Visual::transform (imager) && Visual::state (imager) == activeState
               && Visual::magnitudes (imager) == activeMagnitudes,
           "scope FFT: rejected active window preserves prior transform/cache/counters");
    Access::tag (fftRing, activePosition, activeTag);
    check (Visual::transform (imager), "scope FFT: active window retries on next ordinary call");
}

static void testScopeSnapshotConcurrency()
{
    std::printf ("State test 151: genuinely concurrent scope producer and independent readers (ADR-0070)\n");
    using anamorph::ScopeBuffer;
    constexpr int cap = ScopeBuffer::capacity;
    auto ring = std::make_unique<ScopeBuffer>();
    std::vector<float> l (cap + 17), r (l.size());
    for (int i = 0; i < cap; ++i) { l[static_cast<std::size_t> (i)] = static_cast<float> (i); r[static_cast<std::size_t> (i)] = -static_cast<float> (i); }
    ring->pushBlock (l.data(), r.data(), cap);
    std::atomic<int> ready { 0 };
    std::atomic<bool> start { false }, done { false };
    struct Stats { unsigned accepted = 0, rejected = 0, bad = 0, during = 0; };
    std::array<Stats, 2> stats {};
    std::array<std::atomic<std::uint64_t>, 2> observed {};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (10);
    const auto consume = [&] (int id)
    {
        std::vector<float> left (8192), right (8192);
        std::uint64_t seen = 0;
        d2::Pace pace;
        ready.fetch_add (1, std::memory_order_release);
        while (! start.load (std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
            pace.rest();
        do
        {
            const auto read = id == 0 ? ring->readLatestSnapshot (left.data(), right.data(), 8192)
                : ring->readLatestSince (left.data(), right.data(), seen, 8192);
            auto& s = stats[static_cast<std::size_t> (id)];
            if (! read.valid) ++s.rejected;
            else
            {
                seen = read.end;
                if (read.copied > 0)
                {
                    ++s.accepted;
                    if (! done.load (std::memory_order_acquire)) ++s.during;
                    for (int i = 0; i < read.copied; ++i)
                    {
                        const float expected = static_cast<float> (read.end - static_cast<std::uint64_t> (read.copied)
                                                                   + static_cast<std::uint64_t> (i));
                        if (std::bit_cast<std::uint32_t> (left[static_cast<std::size_t> (i)]) != std::bit_cast<std::uint32_t> (expected)
                            || std::bit_cast<std::uint32_t> (right[static_cast<std::size_t> (i)]) != std::bit_cast<std::uint32_t> (-expected)) ++s.bad;
                    }
                    observed[static_cast<std::size_t> (id)].store (read.end, std::memory_order_release);
                }
            }
            // Like the suite's other long-lived workers, relinquish a bounded
            // share of the CPU to the operation being observed. yield() need
            // not schedule another guest thread under serialising memcheck.
            // Pacing is test-only, uniform across lanes, and outside the APIs.
            pace.rest();
        } while (! done.load (std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline);
    };
    std::thread first (consume, 0), second (consume, 1);
    d2::Pace producerPace;
    while (ready.load (std::memory_order_acquire) != 2 && std::chrono::steady_clock::now() < deadline)
        producerPace.rest();
    start.store (true, std::memory_order_release);
    constexpr std::uint64_t final = cap + 1048576;
    std::uint64_t pos = cap;
    int iteration = 0;
    constexpr std::array<int, 4> blocks { 64, 511, cap + 17, 127 };
    while (pos < final && std::chrono::steady_clock::now() < deadline)
    {
        const int n = static_cast<int> (std::min (final - pos,
            static_cast<std::uint64_t> (blocks[static_cast<std::size_t> (iteration) % blocks.size()])));
        for (int i = 0; i < n; ++i)
        { l[static_cast<std::size_t> (i)] = static_cast<float> (pos + static_cast<std::uint64_t> (i)); r[static_cast<std::size_t> (i)] = -l[static_cast<std::size_t> (i)]; }
        ring->pushBlock (l.data(), r.data(), n);
        pos += static_cast<std::uint64_t> (n);
        ++iteration;
        if (iteration == 4)
        {
            // Test-only observation checkpoint: a permanently lapped reader is
            // allowed to reject forever, so nonzero accepted coverage cannot
            // depend on OS fairness. Give both readers one post-advance window,
            // then continue the remaining stress in independent paced loops. No handshake,
            // wait or reader state exists inside the production push/read API.
            while ((observed[0].load (std::memory_order_acquire) < pos
                    || observed[1].load (std::memory_order_acquire) < pos)
                   && std::chrono::steady_clock::now() < deadline)
                producerPace.rest();
        }
        else if ((iteration % 4) == 0) producerPace.rest();
    }
    done.store (true, std::memory_order_release);
    first.join(); second.join();
    std::printf ("  scope producer: position=%llu target=%llu ready=%d observed=%llu/%llu\n",
        static_cast<unsigned long long> (pos), static_cast<unsigned long long> (final), ready.load(),
        static_cast<unsigned long long> (observed[0].load()), static_cast<unsigned long long> (observed[1].load()));
    check (pos == final && ring->writeCount() == final, "scope stress: producer completes with no reader ownership dependency");
    for (std::size_t i = 0; i < stats.size(); ++i)
    {
        const auto& s = stats[i];
        std::printf ("  scope reader %zu: accepted=%u rejected=%u during-writer=%u bad=%u\n", i, s.accepted, s.rejected, s.during, s.bad);
        check (s.during > 0 && s.bad == 0, "scope stress: each independent reader observes exact windows during production");
    }
}
