// Standalone old/fixed contract regression. Historical source gets scheduling
// hooks only; all operations are on ONE thread, so no data race is executed.
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

void legacyReadHook (std::uint64_t, bool);
void legacyWriteHook();
#define LEGACY_SCOPE_READ_HOOK(i, after) legacyReadHook (i, after)
#define LEGACY_SCOPE_WRITE_HOOK() legacyWriteHook()
#define anamorph legacy_scope
#include ANAMORPH_OLD_SCOPE_HEADER
#undef anamorph
#include ANAMORPH_CURRENT_SCOPE_HEADER

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
};
}

namespace
{
constexpr int cap = anamorph::ScopeBuffer::capacity;
legacy_scope::ScopeBuffer* oldRing = nullptr;
const float* replacementL = nullptr;
const float* replacementR = nullptr;
bool halfPair = false, scheduled = false;
bool readHooks = true, inFlight = false, inFlightWrong = false;
float* oldOutL = nullptr;
float* oldOutR = nullptr;

bool correct (const std::vector<float>& l, const std::vector<float>& r, int n, std::uint64_t end)
{
    for (int i = 0; i < n; ++i)
    {
        const float value = static_cast<float> (end - static_cast<std::uint64_t> (n) + static_cast<std::uint64_t> (i));
        if (std::bit_cast<std::uint32_t> (l[static_cast<std::size_t> (i)]) != std::bit_cast<std::uint32_t> (value)
            || std::bit_cast<std::uint32_t> (r[static_cast<std::size_t> (i)]) != std::bit_cast<std::uint32_t> (-value))
            return false;
    }
    return true;
}
}

void legacyReadHook (std::uint64_t i, bool after)
{
    if (readHooks && ! scheduled && i == 0 && after != halfPair)
    {
        scheduled = true;
        oldRing->pushBlock (replacementL, replacementR, cap);
    }
}

void legacyWriteHook()
{
    if (! inFlight) return;
    scheduled = true;
    const int got = oldRing->readLatest (oldOutL, oldOutR, 1);
    const float expected = static_cast<float> (cap - 1);
    inFlightWrong = got != 1 || std::bit_cast<std::uint32_t> (oldOutL[0]) != std::bit_cast<std::uint32_t> (expected)
        || std::bit_cast<std::uint32_t> (oldOutR[0]) != std::bit_cast<std::uint32_t> (-expected);
}

int main (int argc, char** argv)
{
    if (argc != 2 || (std::strcmp (argv[1], "--old") != 0 && std::strcmp (argv[1], "--new") != 0)) return 2;
    const bool old = std::strcmp (argv[1], "--old") == 0;
    int failures = 0;
    for (bool half : { true, false })
    {
        auto previous = std::make_unique<legacy_scope::ScopeBuffer>();
        auto current = std::make_unique<anamorph::ScopeBuffer>();
        std::vector<float> l (cap), r (cap), outL (cap), outR (cap);
        for (int i = 0; i < cap; ++i) { l[static_cast<std::size_t> (i)] = static_cast<float> (i); r[static_cast<std::size_t> (i)] = -l[static_cast<std::size_t> (i)]; }
        if (old) previous->pushBlock (l.data(), r.data(), cap);
        else current->pushBlock (l.data(), r.data(), cap);
        for (int i = 0; i < cap; ++i) { l[static_cast<std::size_t> (i)] = static_cast<float> (cap + i); r[static_cast<std::size_t> (i)] = -l[static_cast<std::size_t> (i)]; }
        const int count = half ? 1 : cap;
        bool valid = true;
        int copied = count;
        std::uint64_t end = cap;
        scheduled = false;
        if (old)
        {
            oldRing = previous.get(); replacementL = l.data(); replacementR = r.data(); halfPair = half;
            copied = previous->readLatest (outL.data(), outR.data(), count);
        }
        else
        {
            const auto read = anamorph::ScopeBufferTestAccess::read (*current, outL.data(), outR.data(), count,
                [&] (int i, bool after)
                {
                    if (! scheduled && i == 0 && after != half)
                    { scheduled = true; current->pushBlock (l.data(), r.data(), cap); }
                });
            valid = read.valid; copied = read.copied; end = read.end;
        }
        if (! scheduled || (valid && ! correct (outL, outR, copied, end)))
        {
            ++failures;
            std::fprintf (stderr, "wrong committed positions: %s %s\n", old ? "old" : "new", half ? "frame" : "window");
        }
    }
    {
        auto previous = std::make_unique<legacy_scope::ScopeBuffer>();
        auto current = std::make_unique<anamorph::ScopeBuffer>();
        std::vector<float> l (cap), r (cap), outL (cap), outR (cap);
        for (int i = 0; i < cap; ++i) { l[static_cast<std::size_t> (i)] = static_cast<float> (i); r[static_cast<std::size_t> (i)] = -l[static_cast<std::size_t> (i)]; }
        if (old) previous->pushBlock (l.data(), r.data(), cap);
        else current->pushBlock (l.data(), r.data(), cap);
        for (int i = 0; i < cap; ++i) { l[static_cast<std::size_t> (i)] = static_cast<float> (cap + i); r[static_cast<std::size_t> (i)] = -l[static_cast<std::size_t> (i)]; }
        scheduled = false;
        bool wrong = false;
        if (old)
        {
            oldRing = previous.get(); readHooks = false;
            oldOutL = outL.data(); oldOutR = outR.data(); inFlight = true;
            previous->pushBlock (l.data(), r.data(), cap);
            inFlight = false;
            wrong = inFlightWrong;
        }
        else
        {
            anamorph::ScopeBufferTestAccess::write (*current, l.data(), r.data(), cap,
                [&] (std::uint64_t pos)
                {
                    if (pos == 2 * static_cast<std::uint64_t> (cap) - 1)
                    {
                        scheduled = true;
                        const auto read = current->readLatestSnapshot (outL.data(), outR.data(), 1);
                        wrong = read.valid && ! correct (outL, outR, read.copied, read.end);
                    }
                });
        }
        if (! scheduled || wrong)
        {
            ++failures;
            std::fprintf (stderr, "wrong committed positions: %s in-flight\n", old ? "old" : "new");
        }
    }
    std::printf ("scope snapshot control: %s, cases=3, failures=%d\n", old ? "old" : "new", failures);
    return failures == 0 ? 0 : 1;
}
