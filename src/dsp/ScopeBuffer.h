#pragma once

#include <atomic>
#include <array>
#include <cstdint>

namespace anamorph
{

// ============================================================================
//  ScopeBuffer
//
//  Lock-free single-producer ring buffer of stereo samples. The audio thread
//  (producer) only writes; all reads happen on the GUI (message) thread as
//  stateless peeks (vectorscope + spectrum imager). No locks, no allocation
//  on either side.
//
//  Capacity is a power of two so wrap is a cheap mask.
// ============================================================================
class ScopeBuffer
{
public:
    static constexpr int capacity = 1 << 14; // 16384 stereo frames
    static constexpr int mask     = capacity - 1;

    ScopeBuffer() { write.store (0, std::memory_order_relaxed); }

    // --- audio thread ----------------------------------------------------
    // Writes a whole block and publishes it with ONE release-store on the
    // write index (S9). Readers acquire the index and only copy frames
    // strictly below it, so a block becomes visible atomically -- partially
    // committed frames can never be observed. The synchronisation contract is
    // unchanged: the same single writer, the same single release/acquire pair
    // on the same atomic, just at block cadence instead of per sample.
    inline void pushBlock (const float* l, const float* r, int n) noexcept
    {
        auto w = write.load (std::memory_order_relaxed);
        const auto end = w + (uint64_t) n;
        if (n > capacity) // pathological block: only the newest frames can fit
        {
            l += n - capacity;
            r += n - capacity;
            w = end - (uint64_t) capacity;
        }
        for (; w != end; ++w)
        {
            left [w & mask] = *l++;
            right[w & mask] = *r++;
        }
        write.store (end, std::memory_order_release);
    }

    // --- gui thread ------------------------------------------------------
    // Monotonic total of frames ever written (uint64 -- never wraps in
    // practice). The same acquire load readLatest performs; lets a reader
    // detect "no new frames since last check" without copying any data.
    // Read-only: never mutates the ring and consumes nothing.
    uint64_t writeCount() const noexcept { return write.load (std::memory_order_acquire); }

    // Copies up to `count` most-recent frames (oldest first) into the caller's
    // buffers. Returns the number of frames actually copied.
    int readLatest (float* dstL, float* dstR, int count) const noexcept
    {
        const auto w = write.load (std::memory_order_acquire);
        if (count > capacity) count = capacity;
        const uint64_t available = (w < (uint64_t) count) ? (int) w : count;
        const uint64_t start = w - available;
        for (uint64_t i = 0; i < available; ++i)
        {
            const auto idx = (start + i) & mask;
            dstL[i] = left [idx];
            dstR[i] = right[idx];
        }
        return (int) available;
    }

private:
    std::array<float, capacity> left  {};
    std::array<float, capacity> right {};
    std::atomic<uint64_t>       write { 0 };
};

} // namespace anamorph
