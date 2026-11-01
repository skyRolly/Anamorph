#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <limits>

namespace anamorph
{

// Fixed-capacity single-audio-producer display history (Accepted ADR-0070).
// Reusable payload/tag accesses are atomic. GUI readers are serialized by the
// message thread / JUCE paint lock. An attempt returns exact committed positions
// or rejects once on reuse. No waiting, reservation, allocation or retry.
class ScopeBuffer
{
public:
    static constexpr int capacity = 1 << 14;
    static constexpr int mask = capacity - 1;
    static_assert (sizeof (float) == sizeof (std::uint32_t));
    static_assert (std::numeric_limits<float>::is_iec559);
    static_assert (std::atomic<std::uint64_t>::is_always_lock_free,
                   "ScopeBuffer requires native lock-free 64-bit atomic loads/stores");

    struct ReadResult
    {
        bool valid = false;
        int copied = 0;
        std::uint64_t end = 0;
    };

    // Keep newest min(n,capacity), advance by original n. Three release stores
    // per retained frame; one index publication. n is nonnegative. No RMW.
    void pushBlock (const float* l, const float* r, int n) noexcept
    {
        pushImpl (l, r, n, NoWriteHook {});
    }

    std::uint64_t writeCount() const noexcept { return write.load (std::memory_order_acquire); }

    // Rejected scratch may be partially written: never consume it. Valid empty
    // and rejected copies are distinct. end is the SAME publication used by copy.
    ReadResult readLatestSnapshot (float* l, float* r, int count) const noexcept
    {
        return copyAt (l, r, count, write.load (std::memory_order_acquire), NoReadHook {});
    }

    // Tail length comes from this publication, not an earlier writeCount hint.
    ReadResult readLatestSince (float* l, float* r, std::uint64_t since, int limit) const noexcept
    {
        const auto end = write.load (std::memory_order_acquire);
        const auto fresh = end >= since ? end - since : 0;
        const auto count = std::min (fresh, static_cast<std::uint64_t> (std::clamp (limit, 0, capacity)));
        return copyAt (l, r, static_cast<int> (count), end, NoReadHook {});
    }

    // Legacy serial-peek API for Test 61/diagnostics. Concurrent GUI consumers
    // use the explicit result so rejection cannot be interpreted as silence.
    int readLatest (float* l, float* r, int count) const noexcept
    {
        const auto result = readLatestSnapshot (l, r, count);
        return result.valid ? result.copied : 0;
    }

private:
    friend struct ScopeBufferTestAccess; // test-only deterministic scheduling
    struct Slot { std::atomic<std::uint64_t> tag { 0 }, stereo { 0 }; };
    struct NoWriteHook { void operator() (std::uint64_t) const noexcept {} };
    struct NoReadHook { void operator() (int, bool) const noexcept {} };

    static std::uint64_t pack (float l, float r) noexcept
    {
        return static_cast<std::uint64_t> (std::bit_cast<std::uint32_t> (l))
            | (static_cast<std::uint64_t> (std::bit_cast<std::uint32_t> (r)) << 32);
    }

    template<class Hook>
    void pushImpl (const float* l, const float* r, int n, Hook&& afterPayload) noexcept
    {
        const auto old = write.load (std::memory_order_relaxed);
        if (n <= 0) { write.store (old, std::memory_order_release); return; }
        const auto end = old + static_cast<std::uint64_t> (n);
        const int retained = std::min (n, capacity);
        const int skip = n - retained;
        for (int i = 0; i < retained; ++i)
        {
            const auto pos = end - static_cast<std::uint64_t> (retained) + static_cast<std::uint64_t> (i);
            auto& slot = slots[pos & mask];
            slot.tag.store (0, std::memory_order_release);
            slot.stereo.store (pack (l[skip + i], r[skip + i]), std::memory_order_release);
            afterPayload (pos); // NoWriteHook compiles away on production path
            slot.tag.store (pos + 1, std::memory_order_release);
        }
        write.store (end, std::memory_order_release);
    }

    template<class Hook>
    ReadResult copyAt (float* l, float* r, int count, std::uint64_t end, Hook&& hook) const noexcept
    {
        const int copied = static_cast<int> (std::min (end,
            static_cast<std::uint64_t> (std::clamp (count, 0, capacity))));
        for (int i = 0; i < copied; ++i)
        {
            const auto pos = end - static_cast<std::uint64_t> (copied) + static_cast<std::uint64_t> (i);
            const auto& slot = slots[pos & mask];
            if (slot.tag.load (std::memory_order_acquire) != pos + 1) return { false, 0, end };
            hook (i, false);
            const auto stereo = slot.stereo.load (std::memory_order_acquire);
            if (slot.tag.load (std::memory_order_acquire) != pos + 1) return { false, 0, end };
            l[i] = std::bit_cast<float> (static_cast<std::uint32_t> (stereo));
            r[i] = std::bit_cast<float> (static_cast<std::uint32_t> (stereo >> 32));
            hook (i, true);
        }
        return { true, copied, end };
    }

    // Unique tags exclude uint64 epoch rollover within a processor lifetime
    // (>1.5 million years at 384 kHz). Ring wrap is supported. Prepare/reset do
    // not clear these while GUI readers may exist.
    std::array<Slot, capacity> slots {};
    std::atomic<std::uint64_t> write { 0 };
};
} // namespace anamorph
