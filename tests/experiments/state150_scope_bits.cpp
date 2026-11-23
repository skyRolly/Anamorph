#include "dsp/ScopeBuffer.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#if defined(_MSC_VER)
 #define PROBE_NOINLINE __declspec(noinline)
#else
 #define PROBE_NOINLINE __attribute__((noinline))
#endif

#if defined(_M_X64) || defined(__x86_64__)
 #include <immintrin.h>
#endif

namespace anamorph
{
struct ScopeBufferTestAccess
{
    static PROBE_NOINLINE std::uint64_t pack (const float& l, const float& r)
    {
        return ScopeBuffer::pack (l, r);
    }

    static PROBE_NOINLINE std::uint64_t payload (const ScopeBuffer& ring, std::uint64_t pos)
    {
        return ring.slots[pos & ScopeBuffer::mask].stereo.load (std::memory_order_acquire);
    }
};
}

using anamorph::ScopeBuffer;
using Access = anamorph::ScopeBufferTestAccess;
constexpr std::array<std::uint32_t, 8> words { 0, 0x80000000u, 0x3f800000u, 1,
    0x00800000u, 0x7f800000u, 0x7fc12345u, 0x7f812345u };
constexpr auto sign = 0x80000000u;
constexpr auto quiet = 0x00400000u;

static PROBE_NOINLINE std::uint32_t storedBits (const float& value)
{
    std::uint32_t word;
    std::memcpy (&word, &value, sizeof word);
    return word;
}

static PROBE_NOINLINE bool originalFixture()
{
    constexpr int cap = ScopeBuffer::capacity;
    auto ring = std::make_unique<ScopeBuffer>();
    std::vector<float> l (cap + 17), r (l.size()), outL (cap), outR (cap);
    constexpr auto bits = words;
    for (std::size_t i = 0; i < bits.size(); ++i)
    { l[i] = std::bit_cast<float> (bits[i]); r[i] = std::bit_cast<float> (bits[i] ^ sign); }
    ring->pushBlock (l.data(), r.data(), static_cast<int> (bits.size()));
    const auto read = ring->readLatestSnapshot (outL.data(), outR.data(), static_cast<int> (bits.size()));
    bool preserved = read.valid;
    for (std::size_t i = 0; i < bits.size(); ++i)
        preserved = preserved && std::bit_cast<std::uint32_t> (outL[i]) == bits[i]
            && std::bit_cast<std::uint32_t> (outR[i]) == (bits[i] ^ sign);
    return preserved;
}

enum class Seed { fixture, runtimeValue, bytes, wrongRightQuietBit };

template<Seed seed>
static PROBE_NOINLINE unsigned trace (const char* name)
{
    constexpr int cap = ScopeBuffer::capacity;
    constexpr int count = static_cast<int> (words.size());
    auto ring = std::make_unique<ScopeBuffer>();
    std::vector<float> l (cap + 17), r (l.size()), outL (cap), outR (cap);

    for (std::size_t i = 0; i < words.size(); ++i)
    {
        if constexpr (seed == Seed::fixture)
        {
            l[i] = std::bit_cast<float> (words[i]);
            r[i] = std::bit_cast<float> (words[i] ^ sign);
        }
        else if constexpr (seed == Seed::runtimeValue)
        {
            volatile std::uint32_t input = words[i];
            const auto word = input;
            l[i] = std::bit_cast<float> (word);
            r[i] = std::bit_cast<float> (word ^ sign);
        }
        else
        {
            const auto left = words[i];
            auto right = left ^ sign;
            if constexpr (seed == Seed::wrongRightQuietBit)
                if (i == words.size() - 1) right ^= quiet;
            std::memcpy (&l[i], &left, sizeof left);
            std::memcpy (&r[i], &right, sizeof right);
        }
    }

    std::array<std::uint32_t, words.size()> preL {}, preR {};
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        preL[i] = storedBits (l[i]);
        preR[i] = storedBits (r[i]);
    }
    const auto start = ring->writeCount();
    ring->pushBlock (l.data(), r.data(), count);
    const auto read = ring->readLatestSnapshot (outL.data(), outR.data(), count);

    std::printf ("case=%s valid=%d copied=%d end=%llu\n", name, static_cast<int> (read.valid),
        read.copied, static_cast<unsigned long long> (read.end));
    unsigned mismatches = 0;
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        const auto packed = Access::payload (*ring, start + i);
        const auto direct = Access::pack (l[i], r[i]);
        const std::uint32_t expected[] { words[i], words[i] ^ sign };
        const std::uint32_t before[] { preL[i], preR[i] };
        const std::uint32_t unpacked[] { static_cast<std::uint32_t> (packed), static_cast<std::uint32_t> (packed >> 32) };
        const std::uint32_t output[] { storedBits (outL[i]), storedBits (outR[i]) };
        const std::uint32_t observed[] { std::bit_cast<std::uint32_t> (outL[i]), std::bit_cast<std::uint32_t> (outR[i]) };
        for (std::size_t channel = 0; channel < 2; ++channel)
        {
            const auto e = expected[channel];
            const auto o = observed[channel];
            mismatches += static_cast<unsigned> (e != o);
            std::printf ("  sample=%zu channel=%c input=0x%08x prepack=0x%08x pack_call=0x%016llx "
                         "atomic=0x%016llx decoded=0x%08x returned=0x%08x observed=0x%08x xor=0x%08x\n",
                i, channel == 0 ? 'L' : 'R', static_cast<unsigned> (e), static_cast<unsigned> (before[channel]),
                static_cast<unsigned long long> (direct), static_cast<unsigned long long> (packed),
                static_cast<unsigned> (unpacked[channel]), static_cast<unsigned> (output[channel]),
                static_cast<unsigned> (o), static_cast<unsigned> (e ^ o));
        }
    }
    std::printf ("case=%s mismatches=%u\n", name, mismatches);
    if (! read.valid || read.copied != count || read.end != start + words.size()) return count * 2 + 1;
    return mismatches;
}

int main()
{
#if defined(_MSC_VER)
    std::printf ("_MSC_FULL_VER=%ld\n", static_cast<long> (_MSC_FULL_VER));
#endif
#if defined(_M_X64) || defined(__x86_64__)
    std::printf ("mxcsr=0x%08x\n", _mm_getcsr());
#endif
    const bool original = originalFixture();
    std::printf ("original_fixture=%s\n", original ? "pass" : "fail");
    const auto fixture = trace<Seed::fixture> ("fixture");
    const auto runtime = trace<Seed::runtimeValue> ("runtime-value");
    const auto bytes = trace<Seed::bytes> ("bytes");
    const auto negative = trace<Seed::wrongRightQuietBit> ("negative");
    if (negative != 1) return 2;
    return (original && fixture == 0 && runtime == 0 && bytes == 0) ? 0 : 1;
}
