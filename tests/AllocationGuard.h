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
//  the two counts separately rather than summing them. THE TWO ARE ACTUALLY
//  SEPARATE, which needed a fix rather than a claim: `operator new` used to
//  forward to `std::malloc`, and where the malloc interposer exists that name
//  resolves to the interposer in this same file -- so one `new` moved both
//  counters and the same `prepare()` printed 102 and 765, the second figure
//  being 663 + 102. `rawAlloc` (below) takes the `new` route past it, and the
//  measurement above is reproducible from this code again.
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
//     guard's `malloc` -- and its `operator new`, which reaches the real
//     allocator directly -- reach glibc without ever passing RTSan. Measured on the
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
//     SEPARATELY, so an `operator new` that hands back malloc-family memory is
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

#if ! defined(_MSC_VER)
  // posix_memalign is a POSIX declaration, not a std:: one -- <cstdlib> is not
  // required to expose it. See alignedAlloc() for why it is used in preference
  // to C11 aligned_alloc.
  #include <stdlib.h>
#endif

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

// ...AND THE DETECTION ABOVE IS ITSELF CHECKED, because on its own it is a
// single unverified spelling holding up the whole realtime lane. If
// `realtime_sanitizer` is ever renamed, removed, or absent from a future pinned
// Clang, `__has_feature` answers 0, the guard quietly compiles back IN, its
// definitions take precedence over RTSan's interceptors, and the lane loses its
// ALLOCATION detection -- the exact defect ADR-0029 documents and the one shape
// this file's own measurement above was taken to prevent. (Only that class: RTSan
// keeps intercepting locks and blocking calls, verified by seeding a
// `pthread_mutex_lock` into an annotated function with the guard live. Allocation
// is nonetheless the class this suite exists to police.)
//
// Nothing downstream would notice, and the reason is worse than silence. The
// liveness canary is compiled standalone WITHOUT this header, so it keeps
// passing. And `selfCheck()`'s disclosure would flip from "compiled out" to
// "live" -- at which point Test 38 takes its LIVE branch and every one of its
// three liveness assertions PASSES, because the guard genuinely works. It is
// simply in the build where it must not be, reporting a green lane over an
// allocation detector that is no longer running.
//
// So the `realtime` job states the same fact a SECOND time, from outside the
// compiler: `-DANAMORPH_RTSAN_LANE=1` sits on the same `CMAKE_CXX_FLAGS` string
// as `-fsanitize=realtime`, and the two cannot drift apart without someone
// editing that one line. The check below is deliberately NOT keyed on
// `__has_feature` -- a test that consults the signal it is verifying proves
// nothing. It compares the job's declaration against the outcome, so the
// mutation this exists to catch (feature detection stops firing, guard comes
// back) fails the BUILD, at the earliest possible moment, naming the reason.
//
// The reverse direction is deliberately not asserted: a local
// `-fsanitize=realtime` build without the flag is a normal thing to do and is
// already correct, because the feature test stands the guard down by itself.
#if defined(ANAMORPH_RTSAN_LANE) && ! defined(ANAMORPH_NO_ALLOC_GUARD)
  #error "ANAMORPH_RTSAN_LANE is set, so this build declares itself the RealtimeSanitizer lane, but the allocation guard did NOT stand down. Two things produce this and both need checking: __has_feature(realtime_sanitizer) has stopped firing (renamed or removed in this Clang), or -fsanitize=realtime is no longer on this TU's command line while the -D still is -- they are two edits on one line in build.yml. Left as-is the guard shadows RTSan's allocation interceptors and the lane reports a clean run with its allocation detection switched off (ADR-0029 SS7). Fix whichever it is; do NOT silence this by removing the -D."
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
    // Escape hatch for the self-check's probes; see selfCheck(). A probe whose
    // allocation the optimizer can prove unused is a probe the optimizer is
    // allowed to delete, and a deleted probe reports its half DEAD while that
    // half works perfectly.
    inline void* volatile        probeSink = nullptr;

    inline void resetCounts() noexcept { newCount.store (0); mallocCount.store (0); }

    // THE `new` ROUTE MUST NOT PASS THROUGH THIS GUARD'S OWN `malloc`, or the
    // two counters stop being two routes. `operator new` forwarded to
    // `std::malloc`, and where `ANAMORPH_GUARD_MALLOC` is defined that name
    // resolves to the interposer a few hundred lines below -- in this same
    // translation unit -- so ONE `new` incremented `newCount` AND
    // `mallocCount`. Nothing asserted wrongly (Test 38 requires both to be
    // zero), but the `new=N malloc=M` the run prints was not a split: every
    // `new` appeared in both halves, and the per-`prepare()` figures quoted
    // above and in `REALTIME_SAFETY_AUDIT.md` could not be reproduced from the
    // code that printed them.
    //
    // `__libc_malloc` is the same real allocator the interposer itself forwards
    // to, so this takes the identical route with one fewer counter on it, and
    // its blocks are ordinary glibc heap blocks that the unchanged `std::free`
    // in every `operator delete` still frees -- the pairing the interposer
    // already relies on. Where the interposer does not exist (MSVC, macOS, and
    // ASan, which owns `malloc` itself) `std::malloc` IS the real allocator and
    // this is the same call it always was. The malloc half's own liveness probe
    // in `selfCheck()` deliberately calls `std::malloc` and must keep doing so:
    // it is there to prove the interposer fires.
    inline void* rawAlloc (std::size_t n) noexcept
    {
      #if defined(ANAMORPH_GUARD_MALLOC)
        return __libc_malloc (n);
      #else
        return std::malloc (n);
      #endif
    }

    // Scope guard: counts only while it is alive. Deliberately not nestable --
    // the audio path is entered from one place at a time in these tests.
    struct Armed
    {
        Armed()  noexcept { guardArmed.store (true,  std::memory_order_relaxed); }
        ~Armed() noexcept { guardArmed.store (false, std::memory_order_relaxed); }
        Armed (const Armed&) = delete;
        Armed& operator= (const Armed&) = delete;
    };

    // `mallocCompiledIn` is a property of the BUILD, not of the run, and it is
    // reported separately from `mallocLive` so the caller can tell the two
    // apart. They fail for opposite reasons: not-compiled-in is the expected,
    // documented state on MSVC, macOS and under ASan, while compiled-in-but-
    // not-live means the interposition or the probe stopped working -- a defect
    // that must fail rather than degrade to a warning.
    struct SelfCheck { bool newLive; bool mallocLive; bool alignedNewLive; bool mallocCompiledIn; };

    // One known allocation of each kind, so "zero" downstream means "nothing
    // allocated" rather than "nothing was watching".
    inline SelfCheck selfCheck()
    {
      #if defined(ANAMORPH_GUARD_MALLOC)
        SelfCheck r { false, false, false, true };
      #else
        SelfCheck r { false, false, false, false };
      #endif
        {
            resetCounts();
            Armed arm;
            volatile auto* probe = new double[64];   // operator new[]
            probe[0] = 1.0;                          // a volatile STORE into the
                                                     // block: an observable use
                                                     // the optimizer may not drop
            delete[] const_cast<double*> (probe);
            r.newLive = newCount.load() > 0;
        }
        {
            // THE SAME ESCAPE THE OTHER TWO PROBES ALREADY HAD, and it was
            // missing here. `malloc(4096)` immediately followed by `free` with
            // nothing in between is the exact shape GCC's `-fallocation-dce`
            // deletes at -O2 and above, and `linux-lto-tests` compiles this file
            // at `-O3 -flto` where whole-program visibility makes that easier
            // rather than harder.
            //
            // MEASURED, because the honest answer is more interesting than the
            // expected one: on g++-13 at `-O3 -flto` the pair SURVIVES without
            // this line. It survives for a reason that is incidental rather than
            // reassuring -- under `ANAMORPH_GUARD_MALLOC` this very translation
            // unit defines `malloc`, so the compiler stops treating the call as
            // the builtin whose semantics allocation-DCE relies on. That is a
            // property of the configuration where the malloc half exists at all,
            // and it would evaporate the moment the interposer moved to its own
            // TU or the compiler chose to reason about it differently.
            //
            // So this is hardening rather than a bug fix, and it costs one
            // store. What it buys is that the probe no longer depends on an
            // accident: publishing the pointer through a volatile sink makes the
            // allocation observable, which no optimizer may discard.
            resetCounts();
            Armed arm;
            void* raw = std::malloc (4096);
            probeSink = raw;
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
            probeSink = wide;
            delete wide;
            r.alignedNewLive = newCount.load() > 0;
        }
        resetCounts();
        return r;
    }
}

// ---------------------------------------------------------------------------
//  The interposers. `operator new` forwards to `rawAlloc` -- the real
//  allocator, PAST this guard's own `malloc`, see there -- and `operator
//  delete` to `std::free`, which keeps the pair CONSISTENT -- that
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
    void* p = anamorph::testing::rawAlloc (n != 0 ? n : 1);
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
    return anamorph::testing::rawAlloc (n != 0 ? n : 1);
}

ANAMORPH_GUARD_RET_MAYBENULL(n)
void* operator new[] (std::size_t n, const std::nothrow_t& t) noexcept { return ::operator new (n, t); }

// Every form frees with `std::free`, matching the `rawAlloc` above -- its
// blocks are ordinary glibc heap blocks whichever branch it took. GCC's
// `-Wmismatched-new-delete` fires on this and is a FALSE POSITIVE by
// construction rather than a shape worth changing: GCC attributes the block to
// the replaced `operator new[]` and does not follow it through to the
// allocator call that actually produced the memory, so it reports `free` on
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
//  a leak. `posix_memalign` blocks are ordinary `free` blocks, so the POSIX
//  side pairs with `std::free` like the rest of the guard.
//
//  WHY `posix_memalign` AND NOT C11 `aligned_alloc`. This header is compiled
//  into `AnamorphTests` on every platform, and both macOS jobs configure with
//  `-DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`. C11 `aligned_alloc` arrived in the
//  macOS runtime in 10.15, and libc++ honours that availability window by not
//  declaring `std::aligned_alloc` at all below it -- so the previous spelling
//  was a compile error waiting on a toolchain whose libc++ still enforces the
//  guard, on the two jobs that ship the macOS artifacts. `posix_memalign` has
//  been in macOS since 10.6 and in glibc since 2.1.91, carries no availability
//  attribute, and returns ordinary `free`-able memory, so it needs no change to
//  `alignedFree` below. The deployment target is deliberately NOT raised to
//  work around this: 10.13 is the compatibility claim
//  (`docs/architecture/COMPATIBILITY_MATRIX.md`), and a test header is not the
//  place to move it.
//
//  ALIGNMENT MUST BE A POWER OF TWO **AND** A MULTIPLE OF `sizeof(void*)` for
//  `posix_memalign`; C++ over-aligned new only guarantees the first. In
//  practice these operators are reached only when the alignment exceeds
//  `__STDCPP_DEFAULT_NEW_ALIGNMENT__` (16 here), so the second holds already --
//  the clamp below makes that a property of this function rather than of the
//  caller, since an EINVAL return would present as a spurious `bad_alloc`.
namespace anamorph::testing
{
    inline void* alignedAlloc (std::size_t n, std::size_t align) noexcept
    {
        if (n == 0) n = 1;
      #if defined(_MSC_VER)
        return _aligned_malloc (n, align);
      #else
        if (align < sizeof (void*)) align = sizeof (void*);
        void* p = nullptr;
        if (posix_memalign (&p, align, n) != 0) return nullptr;
        return p;
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
