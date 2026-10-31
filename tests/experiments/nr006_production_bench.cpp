// Nonshipping serial cost/code-generation probe. Both builds use the same TU;
// select the hash-verified historical fixture with ANAMORPH_SCOPE_HEADER.
#ifndef ANAMORPH_SCOPE_HEADER
#define ANAMORPH_SCOPE_HEADER "../../src/dsp/ScopeBuffer.h"
#endif
#include ANAMORPH_SCOPE_HEADER
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

extern "C" void scope_push_assembly_canary (anamorph::ScopeBuffer*, const float*, const float*, int);
extern "C" int scope_read_assembly_canary (const anamorph::ScopeBuffer*, float*, float*, int);
extern "C" void scope_push_assembly_canary (anamorph::ScopeBuffer* ring, const float* l, const float* r, int n)
{ ring->pushBlock (l, r, n); }
extern "C" int scope_read_assembly_canary (const anamorph::ScopeBuffer* ring, float* l, float* r, int n)
{ return ring->readLatest (l, r, n); }

int main()
{
    constexpr int cap = anamorph::ScopeBuffer::capacity;
    std::vector<float> l (cap + 17, 0.25f), r (l.size(), -0.5f), outL (cap), outR (cap);
    volatile std::uint32_t sink = 0;
    std::printf ("bytes,block,push_min_ns,push_median_ns,push_max_ns,read8192_min_ns,read8192_median_ns,read8192_max_ns\n");
    for (int block : { 32, 64, 512, cap + 17 })
    {
        auto ring = std::make_unique<anamorph::ScopeBuffer>();
        std::array<double, 7> pushes {}, reads {};
        for (int rep = 0; rep < 7; ++rep)
        {
            constexpr int loops = 2000;
            const auto a = std::chrono::steady_clock::now();
            for (int i = 0; i < loops; ++i) ring->pushBlock (l.data(), r.data(), block);
            const auto b = std::chrono::steady_clock::now();
            for (int i = 0; i < loops; ++i)
            {
                if (ring->readLatest (outL.data(), outR.data(), 8192) != 8192) return 3;
                sink = std::bit_cast<std::uint32_t> (outL[static_cast<std::size_t> (i % 8192)]);
            }
            const auto c = std::chrono::steady_clock::now();
            pushes[rep] = std::chrono::duration<double, std::nano> (b - a).count() / loops;
            reads[rep] = std::chrono::duration<double, std::nano> (c - b).count() / loops;
        }
        std::sort (pushes.begin(), pushes.end()); std::sort (reads.begin(), reads.end());
        std::printf ("%zu,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n", sizeof (*ring), block,
            pushes[0], pushes[3], pushes[6], reads[0], reads[3], reads[6]);
    }
    return sink == std::bit_cast<std::uint32_t> (0.25f) ? 0 : 3;
}
