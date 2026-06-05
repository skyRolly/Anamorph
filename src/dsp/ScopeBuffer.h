#pragma once

#include <atomic>
#include <array>
#include <cstdint>

namespace anamorph
{

// ============================================================================
//  ScopeBuffer
//
//  Lock-free single-producer / single-consumer ring buffer of stereo samples.
//  The audio thread (producer) only writes; the GUI thread (consumer) reads a
//  decimated view for the vectorscope. No locks, no allocation on either side.
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
    inline void push (float l, float r) noexcept
    {
        const auto w = write.load (std::memory_order_relaxed);
        left [w & mask] = l;
        right[w & mask] = r;
        write.store (w + 1, std::memory_order_release);
    }

    // --- gui thread ------------------------------------------------------
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
