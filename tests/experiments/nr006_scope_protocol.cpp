// Nonshipping NR-006 research. Not included by CMake or production headers.
// Build/run instructions and limits: worklogs/NR006_SCOPE_PUBLICATION_REVIEW.md.
#include "../../src/dsp/ScopeBuffer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <limits>
#include <thread>
#include <vector>

namespace
{
constexpr int cap = anamorph::ScopeBuffer::capacity;
constexpr int mask = cap - 1;
using Word = std::uint64_t;
static_assert (sizeof (float) == sizeof (std::uint32_t));
static_assert (std::numeric_limits<float>::is_iec559);
static_assert (std::atomic<Word>::is_always_lock_free);
static_assert (std::atomic<float>::is_always_lock_free);
static_assert (std::atomic<unsigned>::is_always_lock_free);

Word pack (float l, float r)
{
    return Word { std::bit_cast<std::uint32_t> (l) }
         | (Word { std::bit_cast<std::uint32_t> (r) } << 32);
}
void unpack (Word v, float& l, float& r)
{
    l = std::bit_cast<float> (static_cast<std::uint32_t> (v));
    r = std::bit_cast<float> (static_cast<std::uint32_t> (v >> 32));
}
struct NoHook { void operator() (int) const {} };

// A: race-free samples, without pair or window validation.
struct Split
{
    std::array<std::atomic<float>, cap> left {}, right {};
    std::atomic<Word> count { 0 };
    void pushBlock (const float* l, const float* r, int n)
    {
        const auto old = count.load (std::memory_order_relaxed);
        const auto end = old + static_cast<Word> (n);
        const int skip = std::max (0, n - cap);
        for (int i = skip; i < n; ++i)
        {
            const auto at = (old + static_cast<Word> (i)) & mask;
            left[at].store (l[i], std::memory_order_relaxed);
            right[at].store (r[i], std::memory_order_relaxed);
        }
        count.store (end, std::memory_order_release);
    }
    template<class Hook = NoHook>
    int read (float* l, float* r, int n, Word& end, Hook hook = {}) const
    {
        end = count.load (std::memory_order_acquire);
        n = static_cast<int> (std::min (end, static_cast<Word> (std::clamp (n, 0, cap))));
        for (int i = 0; i < n; ++i)
        {
            const auto at = (end - static_cast<Word> (n) + static_cast<Word> (i)) & mask;
            l[i] = left[at].load (std::memory_order_relaxed);
            hook (i); // deterministic L/R-overlap control, without a plain-memory race
            r[i] = right[at].load (std::memory_order_relaxed);
        }
        return n;
    }
};

// B: indivisible stereo frames, without window validation.
struct Packed
{
    std::array<std::atomic<Word>, cap> data {};
    std::atomic<Word> count { 0 };
    void pushBlock (const float* l, const float* r, int n)
    {
        const auto old = count.load (std::memory_order_relaxed);
        const auto end = old + static_cast<Word> (n);
        const int skip = std::max (0, n - cap);
        for (int i = skip; i < n; ++i)
            data[(old + static_cast<Word> (i)) & mask].store (pack (l[i], r[i]), std::memory_order_relaxed);
        count.store (end, std::memory_order_release);
    }
    template<class Hook = NoHook>
    int read (float* l, float* r, int n, Word& end, Hook hook = {}) const
    {
        end = count.load (std::memory_order_acquire);
        n = static_cast<int> (std::min (end, static_cast<Word> (std::clamp (n, 0, cap))));
        for (int i = 0; i < n; ++i)
        {
            const auto at = (end - static_cast<Word> (n) + static_cast<Word> (i)) & mask;
            unpack (data[at].load (std::memory_order_relaxed), l[i], r[i]);
            hook (i);
        }
        return n;
    }
};

// D: generation-validated atomic stereo frames. An invalid read returns -1;
// partial destination bytes MUST NOT be consumed. No retry or reader ownership.
struct Tagged
{
    struct Slot { std::atomic<Word> tag { 0 }, value { 0 }; };
    std::array<Slot, cap> slots {};
    std::atomic<Word> count { 0 };
    void pushBlock (const float* l, const float* r, int n)
    {
        const auto old = count.load (std::memory_order_relaxed);
        const auto end = old + static_cast<Word> (n);
        const int skip = std::max (0, n - cap);
        for (int i = skip; i < n; ++i)
        {
            const auto pos = old + static_cast<Word> (i);
            auto& slot = slots[pos & mask];
            slot.tag.store (0, std::memory_order_release);
            slot.value.store (pack (l[i], r[i]), std::memory_order_release);
            slot.tag.store (pos + 1, std::memory_order_release);
        }
        count.store (end, std::memory_order_release);
    }
    template<class Hook = NoHook>
    int read (float* l, float* r, int n, Word& end, Hook hook = {}) const
    {
        end = count.load (std::memory_order_acquire);
        n = static_cast<int> (std::min (end, static_cast<Word> (std::clamp (n, 0, cap))));
        for (int i = 0; i < n; ++i)
        {
            const auto pos = end - static_cast<Word> (n) + static_cast<Word> (i);
            const auto& slot = slots[pos & mask];
            if (slot.tag.load (std::memory_order_acquire) != pos + 1) return -1;
            hook (i); // after first tag, before payload and second tag
            const auto v = slot.value.load (std::memory_order_acquire);
            if (slot.tag.load (std::memory_order_acquire) != pos + 1) return -1;
            unpack (v, l[i], r[i]);
        }
        return n;
    }
};

// C: full-window triple-buffer ownership. One logical consumer; returning its
// previous private buffer via middle happens only after that copy is finished.
struct Owned
{
    struct Snapshot { std::array<float, cap> l {}, r {}; Word end = 0; };
    std::array<Snapshot, 3> snapshots {};
    anamorph::ScopeBuffer history;
    std::atomic<unsigned> middle { 1 };
    unsigned writer = 0;
    mutable unsigned reader = 2;
    void pushBlock (const float* l, const float* r, int n)
    {
        history.pushBlock (l, r, n);
        auto& dest = snapshots[writer];
        dest.end = history.writeCount();
        history.readLatest (dest.l.data(), dest.r.data(), cap);
        writer = middle.exchange (writer | 4u, std::memory_order_acq_rel) & 3u;
    }
    int read (float* l, float* r, int n, Word& end)
    {
        if ((middle.load (std::memory_order_acquire) & 4u) != 0)
            reader = middle.exchange (reader, std::memory_order_acq_rel) & 3u;
        const auto& source = snapshots[reader];
        end = source.end;
        const int available = static_cast<int> (std::min (end, static_cast<Word> (cap)));
        n = std::min (available, std::clamp (n, 0, cap));
        std::memcpy (l, source.l.data() + available - n, static_cast<std::size_t> (n) * sizeof (float));
        std::memcpy (r, source.r.data() + available - n, static_cast<std::size_t> (n) * sizeof (float));
        return n;
    }
};

struct Plain : anamorph::ScopeBuffer
{
    int read (float* l, float* r, int n, Word& end) const
    { end = writeCount(); return readLatest (l, r, n); }
};

int failures = 0;
void check (bool good, const char* what)
{
    if (! good) { std::fprintf (stderr, "FAIL: %s\n", what); ++failures; }
}
void fillSequence (std::vector<float>& l, std::vector<float>& r, Word start)
{
    for (std::size_t i = 0; i < l.size(); ++i)
    { l[i] = static_cast<float> (start + i); r[i] = -l[i]; }
}
bool window (const float* l, const float* r, int n, Word end)
{
    for (int i = 0; i < n; ++i)
        if (l[i] != static_cast<float> (end - static_cast<Word> (n) + static_cast<Word> (i)) || r[i] != -l[i])
            return false;
    return true;
}

int controls()
{
    std::vector<float> l (cap), r (cap), outL (cap), outR (cap);
    fillSequence (l, r, 0);
    Word end = 0;
    auto split = std::make_unique<Split>();
    split->pushBlock (l.data(), r.data(), cap);
    fillSequence (l, r, cap);
    split->read (outL.data(), outR.data(), cap, end, [&] (int i)
    { if (i == 0) split->pushBlock (l.data(), r.data(), cap); });
    check (outR[0] != -outL[0], "atomic-sample control must expose split stereo epochs");

    auto packed = std::make_unique<Packed>();
    fillSequence (l, r, 0); packed->pushBlock (l.data(), r.data(), cap);
    fillSequence (l, r, cap);
    packed->read (outL.data(), outR.data(), cap, end, [&] (int i)
    { if (i == 0) packed->pushBlock (l.data(), r.data(), cap); });
    check (! window (outL.data(), outR.data(), cap, end), "packed control must expose mixed window epochs");
    bool pairs = true;
    for (int i = 0; i < cap; ++i) pairs = pairs && outR[i] == -outL[i];
    check (pairs, "packed control must preserve stereo pairs while window fails");

    auto tagged = std::make_unique<Tagged>();
    fillSequence (l, r, 0); tagged->pushBlock (l.data(), r.data(), cap);
    fillSequence (l, r, cap);
    check (tagged->read (outL.data(), outR.data(), cap, end, [&] (int i)
        { if (i == 0) tagged->pushBlock (l.data(), r.data(), cap); }) == -1,
        "tag validation must reject reuse between first tag and payload");
    check (tagged->read (outL.data(), outR.data(), cap, end) == cap
        && window (outL.data(), outR.data(), cap, end), "tagged ring must recover without retry");
    // Single-thread newest/wrap/clamp/oversized behavior, independent of concurrency.
    for (int block : { 0, 1, 63, 511, cap - 1, cap, cap + 17 })
    {
        l.resize (static_cast<std::size_t> (block)); r.resize (l.size());
        fillSequence (l, r, tagged->count.load());
        tagged->pushBlock (l.data(), r.data(), block);
        const int got = tagged->read (outL.data(), outR.data(), cap + 5, end);
        check (got == cap && window (outL.data(), outR.data(), got, end), "tagged serial boundary");
    }
    // Bit representation, including signed zero, infinity and NaN payloads.
    for (std::uint32_t bits : { 0u, 0x80000000u, 0x7f800000u, 0x7fc12345u, 0xffffffffu })
    {
        const float a = std::bit_cast<float> (bits);
        float b = 0, c = 0;
        unpack (pack (a, a), b, c);
        check (std::bit_cast<std::uint32_t> (b) == bits && std::bit_cast<std::uint32_t> (c) == bits,
               "pack must preserve every tested float bit pattern");
    }
    return failures;
}

template<class Ring>
void stress (const char* name, int readers = 1)
{
    auto ring = std::make_unique<Ring>();
    std::atomic<bool> start { false }, done { false };
    std::atomic<unsigned> observed { 0 }, during { 0 }, bad { 0 };
    const auto consume = [&]
    {
        std::vector<float> l (8192), r (8192);
        while (! start.load (std::memory_order_acquire)) std::this_thread::yield();
        do
        {
            Word end = 0;
            const int got = ring->read (l.data(), r.data(), 8192, end);
            if (got > 0)
            {
                observed.fetch_add (1, std::memory_order_relaxed);
                if (! done.load (std::memory_order_acquire)) during.fetch_add (1, std::memory_order_relaxed);
                if (! window (l.data(), r.data(), got, end)) bad.fetch_add (1, std::memory_order_relaxed);
            }
        } while (! done.load (std::memory_order_acquire));
    };
    std::vector<std::thread> consumers;
    for (int i = 0; i < readers; ++i) consumers.emplace_back (consume);
    std::vector<float> l (64), r (64);
    start.store (true, std::memory_order_release);
    for (Word pos = 0; pos < 1048576; pos += 64)
    {
        fillSequence (l, r, pos); ring->pushBlock (l.data(), r.data(), 64);
        if ((pos & 4095) == 0) std::this_thread::yield();
    }
    done.store (true, std::memory_order_release);
    for (auto& consumer : consumers) consumer.join();
    std::printf ("stress,%s,readers=%d,accepted=%u,during-writer=%u,bad=%u\n",
                name, readers, observed.load(), during.load(), bad.load());
    check (during.load() > 0 && bad.load() == 0, "concurrent stress must overlap and preserve accepted windows");
}

volatile Word sink = 0;
template<class Ring>
void bench (const char* name)
{
    auto ring = std::make_unique<Ring>();
    std::vector<float> l (cap + 17, 0.25f), r (l.size(), -0.5f), dl (cap), dr (cap);
    for (int block : { 32, 64, 512, cap + 17 })
    {
        std::array<double, 7> pushes {}, reads {};
        for (int rep = 0; rep < 7; ++rep)
        {
            constexpr int loops = 2000;
            auto t = std::chrono::steady_clock::now();
            for (int i = 0; i < loops; ++i) ring->pushBlock (l.data(), r.data(), block);
            auto u = std::chrono::steady_clock::now();
            Word end = 0;
            for (int i = 0; i < loops; ++i)
            {
                const int got = ring->read (dl.data(), dr.data(), 8192, end);
                sink = pack (dl[static_cast<std::size_t> (i % got)], dr[static_cast<std::size_t> (i % got)]) ^ end;
            }
            auto v = std::chrono::steady_clock::now();
            pushes[rep] = std::chrono::duration<double, std::nano> (u - t).count() / loops;
            reads[rep] = std::chrono::duration<double, std::nano> (v - u).count() / loops;
        }
        std::sort (pushes.begin(), pushes.end()); std::sort (reads.begin(), reads.end());
        std::printf ("bench,%s,bytes=%zu,block=%d,push_ns=%.1f/%.1f/%.1f,read8192_ns=%.1f/%.1f/%.1f\n",
            name, sizeof (Ring), block, pushes[0], pushes[3], pushes[6], reads[0], reads[3], reads[6]);
    }
}
}

int main (int argc, char** argv)
{
    if (controls() != 0) return 1;
    if (argc > 1 && std::strcmp (argv[1], "--bench") == 0)
    {
        bench<Plain> ("plain-serial-only"); bench<Split> ("A-split");
        bench<Packed> ("B-packed"); bench<Owned> ("C-owned"); bench<Tagged> ("D-tagged");
    }
    else
    {
        stress<Tagged> ("D-tagged", 2); stress<Owned> ("C-owned");
    }
    std::printf ("nr006-research: %d failures; production ScopeBuffer unchanged\n", failures);
    return failures == 0 ? 0 : 1;
}
