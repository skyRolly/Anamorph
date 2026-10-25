// Nonshipping single-threaded Test 61 characterization; not a product repair.
#include <juce_dsp/juce_dsp.h>
#include "dsp/ScopeBuffer.h"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cinttypes>
#include <cstdio>
#include <memory>
#include <vector>
#if defined(__SSE__)
 #include <xmmintrin.h>
#endif

struct ScopeSnapshot
{
    std::uint64_t written;
    int got, index;
    float left, right, expectedLeft, expectedRight;
    bool leftEqual, rightEqual;
    const void* ring;
    const float* leftDestination;
    const float* rightDestination;
};

extern "C" __attribute__((noinline)) void scope_mismatch (const ScopeSnapshot& value)
{
    unsigned long long control = 0;
   #if defined(__SSE__)
    control = _mm_getcsr();
   #elif defined(__aarch64__)
    __asm__ volatile ("mrs %0, fpcr" : "=r" (control));
   #endif
    std::printf ("SCOPE_MISMATCH_BITS written=%llu got=%d index=%d ringIndex=%llu "
                 "left=%08" PRIx32 " right=%08" PRIx32 " expectedLeft=%08" PRIx32
                 " expectedRight=%08" PRIx32 " leftEqual=%d rightEqual=%d "
                 "ring=%p dstL=%p dstR=%p capacity=%d fpControl=%llx rounding=%d\n",
                 (unsigned long long) value.written, value.got, value.index,
                 (unsigned long long) ((value.written - (std::uint64_t) value.got + (std::uint64_t) value.index) & anamorph::ScopeBuffer::mask),
                 std::bit_cast<std::uint32_t> (value.left), std::bit_cast<std::uint32_t> (value.right),
                 std::bit_cast<std::uint32_t> (value.expectedLeft), std::bit_cast<std::uint32_t> (value.expectedRight),
                 value.leftEqual, value.rightEqual, value.ring, (const void*) value.leftDestination,
                 (const void*) value.rightDestination, anamorph::ScopeBuffer::capacity, control, std::fegetround());
    std::fflush (stdout);
}

int main()
{
    using anamorph::ScopeBuffer;
    constexpr int cap = ScopeBuffer::capacity;
    auto ring = std::make_unique<ScopeBuffer>();
    std::vector<float> sourceL (2 * (size_t) cap), sourceR (2 * (size_t) cap);
    std::vector<float> destinationL ((size_t) cap), destinationR ((size_t) cap);
    std::uint64_t written = 0;
    bool mismatchReported = false;
    auto fill = [&] (int count)
    {
        for (int i = 0; i < count; ++i)
        {
            sourceL[(size_t) i] = (float) (written + (std::uint64_t) i);
            sourceR[(size_t) i] = -sourceL[(size_t) i];
        }
    };
    auto newest = [&] (int got)
    {
        if (got < 0 || got > cap || (std::uint64_t) got > written)
            return false;
        for (int i = 0; i < got; ++i)
        {
            // Capture each operand once. Print those values, their bit patterns
            // and the comparison results; never reload just for diagnostics.
            const float left = destinationL[(size_t) i];
            const float right = destinationR[(size_t) i];
            const float expectedLeft = (float) (written - (std::uint64_t) got + (std::uint64_t) i);
            const float expectedRight = -expectedLeft;
            const bool leftEqual = juce::exactlyEqual (left, expectedLeft);
            const bool rightEqual = juce::exactlyEqual (right, expectedRight);
            if (! leftEqual || ! rightEqual)
            {
                if (! mismatchReported)
                {
                    scope_mismatch ({ written, got, i, left, right, expectedLeft, expectedRight,
                                      leftEqual, rightEqual, ring.get(), destinationL.data(), destinationR.data() });
                    mismatchReported = true;
                }
                return false;
            }
        }
        return true;
    };
    bool counts = ring->readLatest (destinationL.data(), destinationR.data(), 8192) == 0 && ring->writeCount() == 0;
    bool frames = true;
    constexpr int block = 441;
    const int blocks = (int) (2.7 * cap / block);
    int straddles = 0;
    for (int b = 0; b < blocks; ++b)
    {
        fill (block);
        if ((int) (written & ScopeBuffer::mask) + block > cap)
            ++straddles;
        ring->pushBlock (sourceL.data(), sourceR.data(), block);
        written += block;
        counts = counts && ring->writeCount() == written;
        for (int count : { 1, block, 8192, cap })
        {
            const int got = ring->readLatest (destinationL.data(), destinationR.data(), count);
            counts = counts && (std::uint64_t) got == std::min<std::uint64_t> ({ (std::uint64_t) count, written, (std::uint64_t) cap });
            frames = frames && newest (got);
        }
    }
    const int clamped = ring->readLatest (destinationL.data(), destinationR.data(), cap + 5);
    const bool clamp = clamped == cap && newest (clamped);
    const int big = cap + 1000;
    fill (big);
    ring->pushBlock (sourceL.data(), sourceR.data(), big);
    written += big;
    const int got = ring->readLatest (destinationL.data(), destinationR.data(), cap);
    const bool oversized = ring->writeCount() == written && got == cap && newest (got);
    std::printf ("SCOPE_DIAGNOSTIC_COMPLETE counts=%d frames=%d straddles=%d clamp=%d oversized=%d\n",
                 counts, frames, straddles, clamp, oversized);
    return counts && frames && straddles > 0 && clamp && oversized ? 0 : 1;
}
