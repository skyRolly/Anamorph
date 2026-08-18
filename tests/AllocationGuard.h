#pragma once

// ============================================================================
//  AllocationGuard.h — counts heap allocations while the audio path runs.
//
//  WHY IT EXISTS ALONGSIDE RTSan (ADR-0029 §7). RealtimeSanitizer is the
//  stronger tool and the primary gate, but it is Clang-only and
//  Linux/macOS-only: the shipped Windows binary is built by MSVC, which RTSan
//  never runs on. This guard is `operator new` replacement -- a facility the
//  C++ standard guarantees on EVERY conforming implementation, MSVC included --
//  plus glibc malloc interposition where that is available. It is the portable
//  tier, not a replacement for RTSan where RTSan exists.
//
//  WHAT IT COVERS, and the split matters. Measured on this project: a single
//  `prepare()` allocates 102 times through `operator new` and 663 times through
//  the malloc family -- JUCE's `AudioBuffer`/`HeapBlock` take the raw-malloc
//  route. So `operator new` alone would miss the allocation JUCE actually
//  performs most, which is why the malloc half exists and why the guard reports
//  the two counts separately rather than summing them.
//
//  ARMED ONLY AROUND `process()`. `prepare()` is *required* to allocate
//  (REALTIME_AUDIO_POLICY permits it and the engine depends on it), so an
//  always-on counter would measure the wrong thing. `Armed` is a scope guard.
//
//  IT PROVES IT CAN COUNT BEFORE IT REPORTS ZERO (TESTING_POLICY rule 4).
//  "No allocations were observed" and "nothing was observing" print the same
//  way, and the configurations below genuinely differ in what is live:
//
//    GCC / Clang Release      operator new live, malloc live
//    ASan (+UBSan)            operator new live, malloc COMPILED OUT
//    RealtimeSanitizer        WHOLE GUARD COMPILED OUT  (see below)
//    valgrind memcheck        WHOLE GUARD COMPILED OUT  (see below)
//
//  The malloc half is compiled out under ASan deliberately: an
//  executable-defined `malloc` fights ASan's own allocator and the process
//  dies. Under ASan the `operator new` half still counts, and ASan itself
//  tracks the malloc route, so the configuration loses no coverage it had.
//
//  `selfCheck()` performs one known allocation of each kind and reports which
//  halves actually moved; the caller decides what to assert on that basis. A
//  half that is not live is DISCLOSED and skipped, never silently passed.
//
//  TWO CONFIGURATIONS MUST COMPILE THE WHOLE GUARD OUT, for different reasons.
//
//  1. REALTIMESANITIZER, and this one is a CORRECTNESS requirement rather than
//     a tidiness one. RTSan detects allocations by intercepting the allocation
//     entry points; a definition in the program's own object files takes
//     precedence over the interceptor in the sanitizer runtime archive, so the
//     guard's `malloc` -- and its `operator new`, which routes through
//     `std::malloc` -- reach glibc without ever passing RTSan. Measured on the
//     real suite with one escaping `malloc` seeded into `AnamorphEngine::process`:
//
//         guard compiled in   RTSan reports 0, exit 1 (only the guard's assert)
//         guard compiled out  RTSan reports the malloc at AnamorphEngine.cpp:668,
//                             exit 43
//
//     Left in, the guard would BLIND the lane it shares a binary with -- and the
//     liveness canary would not notice, because it is compiled standalone
//     without the guard. Detected here with `__has_feature(realtime_sanitizer)`
//     rather than by a flag on the job, so a local `-fsanitize=realtime` build
//     cannot reintroduce the conflict by forgetting it. Nothing is lost: under
//     RTSan the stronger detector is already present and covers allocations,
//     locks and blocking calls with a symbolized stack.
//
//  2. VALGRIND, via `-DANAMORPH_NO_ALLOC_GUARD` on that job's build (memcheck
//     leaves no compile-time marker to test, so this one is a flag; no CMake
//     structure change is involved). memcheck tracks which allocator produced
//     each block and intercepts the `new`/`delete` and `malloc`/`free` families
//     SEPARATELY, so an `operator new` that hands back `std::malloc` memory is
//     reported as "Mismatched free() / delete / delete []" on every subsequent
//     delete -- measured against the real JUCE-linked suite under the pipeline's
//     exact invocation, where it fails the step outright. (A small standalone probe
//     does NOT reproduce it, which is why this note names the binary that does.)
//
//  In both, `selfCheck()` reports both halves not-live and the test discloses
//  and skips rather than asserting a vacuous zero.
// ============================================================================

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

// MSVC ANNOTATION PARITY. `vcruntime_new.h` declares the replaceable operators
// with SAL annotations, and `/analyze` reports "Inconsistent annotation for
// 'new': this instance has no annotations" when a replacement omits them --
// which this guard did, producing six new code-scanning alerts on the ONE
// platform where it is the sole realtime tier (RTSan does not run there).
// The annotations carry no codegen; they tell the analyser what the function
// returns, which is exactly what it was missing. Empty off MSVC.
#if defined(_MSC_VER)
  #include <sal.h>
  #define ANAMORPH_GUARD_RET_NOTNULL(sz)   _Ret_notnull_ _Post_writable_byte_size_(sz)
  #define ANAMORPH_GUARD_RET_MAYBENULL(sz) _Ret_maybenull_ _Post_writable_byte_size_(sz)
#else
  #define ANAMORPH_GUARD_RET_NOTNULL(sz)
  #define ANAMORPH_GUARD_RET_MAYBENULL(sz)
#endif

#if defined(__has_feature)
  #if __has_feature(address_sanitizer)
    #define ANAMORPH_GUARD_ASAN 1
  #endif
#endif
#if defined(__SANITIZE_ADDRESS__)
  #define ANAMORPH_GUARD_ASAN 1
#endif

// RealtimeSanitizer: the guard must stand down entirely, or it shadows RTSan's
// own allocation interceptors and blinds the realtime lane (see the header note
// above for the measurement). Self-detected rather than flag-driven so a local
// `-fsanitize=realtime` build cannot reintroduce the conflict.
#if defined(__has_feature)
  #if __has_feature(realtime_sanitizer)
    #define ANAMORPH_NO_ALLOC_GUARD 1
  #endif
#endif

// The malloc half needs a way to reach the REAL allocator without recursing.
// glibc exports `__libc_malloc` for exactly this; it is the mechanism this
// guard is built on, and its absence is what makes the half unavailable rather
// than broken elsewhere.
#if defined(__linux__) && defined(__GLIBC__) && !defined(ANAMORPH_GUARD_ASAN) && !defined(ANAMORPH_NO_ALLOC_GUARD)
  #define ANAMORPH_GUARD_MALLOC 1
  extern "C" void* __libc_malloc (std::size_t);
  extern "C" void* __libc_calloc (std::size_t, std::size_t);
  extern "C" void* __libc_realloc (void*, std::size_t);
#endif

namespace anamorph::testing
{
    inline std::atomic<bool>     guardArmed { false };
    inline std::atomic<long>     newCount   { 0 };
    inline std::atomic<long>     mallocCount{ 0 };
    // Escape hatch for the self-check's aligned probe; see selfCheck().
    inline void* volatile        alignedSink = nullptr;

    inline void resetCounts() noexcept { newCount.store (0); mallocCount.store (0); }

    // Scope guard: counts only while it is alive. Deliberately not nestable --
    // the audio path is entered from one place at a time in these tests.
    struct Armed
    {
        Armed()  noexcept { guardArmed.store (true,  std::memory_order_relaxed); }
        ~Armed() noexcept { guardArmed.store (false, std::memory_order_relaxed); }
        Armed (const Armed&) = delete;
        Armed& operator= (const Armed&) = delete;
    };

    struct SelfCheck { bool newLive; bool mallocLive; bool alignedNewLive; };

    // One known allocation of each kind, so "zero" downstream means "nothing
    // allocated" rather than "nothing was watching".
    inline SelfCheck selfCheck()
    {
        SelfCheck r { false, false, false };
        {
            resetCounts();
            Armed arm;
            volatile auto* probe = new double[64];   // operator new[]
            probe[0] = 1.0;
            delete[] const_cast<double*> (probe);
            r.newLive = newCount.load() > 0;
        }
        {
            resetCounts();
            Armed arm;
            void* raw = std::malloc (4096);
            if (raw != nullptr) std::free (raw);
            r.mallocLive = mallocCount.load() > 0;
        }
        {
            // The over-aligned route is a SEPARATE set of replaceable operators,
            // so a live plain `operator new` does not imply a live aligned one.
            // The pointer ESCAPES through a volatile sink: C++14 permits eliding
            // a new/delete pair the optimizer can see is unused, and at -O3 it
            // does exactly that -- which would report this half dead while it
            // works perfectly. Same reason `realtime_canary.cpp` escapes its
            // allocation.
            resetCounts();
            Armed arm;
            struct alignas (128) Wide { float v[16]; };
            auto* wide = new Wide;
            alignedSink = wide;
            delete wide;
            r.alignedNewLive = newCount.load() > 0;
        }
        resetCounts();
        return r;
    }
}

// ---------------------------------------------------------------------------
//  The interposers. `operator new` forwards to plain `std::malloc` and
//  `operator delete` to `std::free`, which keeps the pair CONSISTENT -- that
//  consistency is what lets the guard run cleanly under valgrind memcheck
//  (measured: 0 errors under the pipeline's exact invocation). Replacing only
//  one side of the pair is what produces "Mismatched free() / delete" reports.
// ---------------------------------------------------------------------------
#if !defined(ANAMORPH_NO_ALLOC_GUARD)
ANAMORPH_GUARD_RET_NOTNULL(n)
void* operator new (std::size_t n)
{
    if (anamorph::testing::guardArmed.load (std::memory_order_relaxed))
        anamorph::testing::newCount.fetch_add (1, std::memory_order_relaxed);
    void* p = std::malloc (n != 0 ? n : 1);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

ANAMORPH_GUARD_RET_NOTNULL(n)
void* operator new[] (std::size_t n) { return ::operator new (n); }

ANAMORPH_GUARD_RET_MAYBENULL(n)
void* operator new (std::size_t n, const std::nothrow_t&) noexcept
{
    if (anamorph::testing::guardArmed.load (std::memory_order_relaxed))
        anamorph::testing::newCount.fetch_add (1, std::memory_order_relaxed);
    return std::malloc (n != 0 ? n : 1);
}

ANAMORPH_GUARD_RET_MAYBENULL(n)
void* operator new[] (std::size_t n, const std::nothrow_t& t) noexcept { return ::operator new (n, t); }

// Every form frees with `std::free`, matching the `std::malloc` above. GCC's
// `-Wmismatched-new-delete` fires on this and is a FALSE POSITIVE by
// construction rather than a shape worth changing: GCC attributes the block to
// the replaced `operator new[]` and does not follow it through to the
// `std::malloc` that actually produced the memory, so it reports `free` on
// "new[] memory" no matter how the deallocators forward among themselves
// (measured both ways). That is why the GCC warning gate's set excludes it --
// see `scripts/gcc-warning-baseline.txt`.
void operator delete (void* p) noexcept                 { std::free (p); }
void operator delete[] (void* p) noexcept               { std::free (p); }
void operator delete (void* p, std::size_t) noexcept    { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept  { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept   { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }

// --- C++17 over-aligned forms ------------------------------------------------
//  These matter MOST where the malloc half is unavailable. On MSVC and macOS
//  `ANAMORPH_GUARD_MALLOC` is never defined, so `operator new` replacement is
//  the whole guard there -- and an over-aligned allocation that fell through to
//  the default implementation would reach `aligned_alloc`/`_aligned_malloc`
//  uncounted, in exactly the platform gap Test 38 exists to close. JUCE's SIMD
//  types are over-aligned, so this is a route the audio path can plausibly take.
//
//  THE ALLOCATOR AND DEALLOCATOR MUST BE THE MATCHING PAIR, which is why this
//  is not simply `std::malloc`: memory from `_aligned_malloc` MUST go back
//  through `_aligned_free`, and mixing them is undefined behaviour rather than
//  a leak. `aligned_alloc` blocks are ordinary `free` blocks, so the POSIX side
//  pairs with `std::free` like the rest of the guard.
//
//  `aligned_alloc` also requires the size to be a multiple of the alignment
//  (C11); the round-up below keeps that contract, since a caller asking for 4
//  bytes at 64-byte alignment is legal C++ but not a legal `aligned_alloc` call.
namespace anamorph::testing
{
    inline void* alignedAlloc (std::size_t n, std::size_t align) noexcept
    {
        if (n == 0) n = 1;
      #if defined(_MSC_VER)
        return _aligned_malloc (n, align);
      #else
        const std::size_t rounded = ((n + align - 1) / align) * align;
        return std::aligned_alloc (align, rounded);
      #endif
    }

    inline void alignedFree (void* p) noexcept
    {
      #if defined(_MSC_VER)
        _aligned_free (p);
      #else
        std::free (p);
      #endif
    }
}

ANAMORPH_GUARD_RET_NOTNULL(n)
void* operator new (std::size_t n, std::align_val_t a)
{
    if (anamorph::testing::guardArmed.load (std::memory_order_relaxed))
        anamorph::testing::newCount.fetch_add (1, std::memory_order_relaxed);
    void* p = anamorph::testing::alignedAlloc (n, static_cast<std::size_t> (a));
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

ANAMORPH_GUARD_RET_NOTNULL(n)
void* operator new[] (std::size_t n, std::align_val_t a) { return ::operator new (n, a); }

ANAMORPH_GUARD_RET_MAYBENULL(n)
void* operator new (std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept
{
    if (anamorph::testing::guardArmed.load (std::memory_order_relaxed))
        anamorph::testing::newCount.fetch_add (1, std::memory_order_relaxed);
    return anamorph::testing::alignedAlloc (n, static_cast<std::size_t> (a));
}

ANAMORPH_GUARD_RET_MAYBENULL(n)
void* operator new[] (std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept
{
    return ::operator new (n, a, t);
}

void operator delete (void* p, std::align_val_t) noexcept   { anamorph::testing::alignedFree (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { anamorph::testing::alignedFree (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept   { anamorph::testing::alignedFree (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { anamorph::testing::alignedFree (p); }
void operator delete (void* p, std::align_val_t, const std::nothrow_t&) noexcept   { anamorph::testing::alignedFree (p); }
void operator delete[] (void* p, std::align_val_t, const std::nothrow_t&) noexcept { anamorph::testing::alignedFree (p); }
#endif // !ANAMORPH_NO_ALLOC_GUARD

#if defined(ANAMORPH_GUARD_MALLOC)
extern "C" void* malloc (std::size_t n)
{
    if (anamorph::testing::guardArmed.load (std::memory_order_relaxed))
        anamorph::testing::mallocCount.fetch_add (1, std::memory_order_relaxed);
    return __libc_malloc (n);
}

extern "C" void* calloc (std::size_t count, std::size_t size)
{
    if (anamorph::testing::guardArmed.load (std::memory_order_relaxed))
        anamorph::testing::mallocCount.fetch_add (1, std::memory_order_relaxed);
    return __libc_calloc (count, size);
}

extern "C" void* realloc (void* p, std::size_t n)
{
    if (anamorph::testing::guardArmed.load (std::memory_order_relaxed))
        anamorph::testing::mallocCount.fetch_add (1, std::memory_order_relaxed);
    return __libc_realloc (p, n);
}
#endif
