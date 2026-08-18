// ============================================================================
//  realtime_canary.cpp — proof that the RealtimeSanitizer lane can FAIL.
//
//  WHY THIS EXISTS. `TESTING_POLICY.md` rule 4: a checker must prove it is live
//  before its silence is trusted. The `realtime` job's whole output on a healthy
//  tree is "no violations" — which is exactly what a lane whose instrumentation
//  silently stopped working also prints. This translation unit is the
//  difference between those two states: its annotated function commits a real
//  violation, so the job asserts it ABORTS. If it ever exits 0, the lane is
//  dead and the job fails saying so.
//
//  THE ALLOCATION ESCAPES ON PURPOSE. At -O2 Clang may delete a non-escaping
//  malloc/free pair before the RTSan pass runs, so the obvious canary --
//  allocate and immediately free -- can pass while the tool is working
//  perfectly. Publishing the pointer through a volatile sink removes that
//  degree of freedom: the allocation cannot be reasoned away, so a green canary
//  means the instrumentation is genuinely absent rather than merely optimised.
//  (The block is never freed. The process is expected to abort inside the
//  allocation, and a leak in a canary that is supposed to die is not a defect.)
//
//  It is compiled DIRECTLY by the workflow step rather than being a CMake
//  target: it needs no JUCE and no project configuration, and keeping it out of
//  CMakeLists.txt keeps a liveness probe from becoming a Build System change.
// ============================================================================
#include "../src/dsp/RealtimeAnnotations.h"

#include <cstdio>

// The escape hatch the optimiser cannot see through.
volatile float* rtsanCanarySink = nullptr;

static void allocatingAudioCallback (int numSamples) noexcept ANAMORPH_NONBLOCKING
{
    // The violation: a heap allocation inside a function declared nonblocking.
    auto* block = new float[(unsigned) numSamples];
    block[0] = 1.0f;
    rtsanCanarySink = block;   // escapes -> cannot be elided
}

int main()
{
#ifndef __clang__
    std::printf ("realtime canary: not a Clang build -- the annotation is inert here.\n");
    return 2;   // the job runs this only under the pinned Clang; anything else is a wiring error
#else
    std::printf ("realtime canary: calling an allocating nonblocking function...\n");
    allocatingAudioCallback (64);
    // Reaching this line means RTSan did NOT intercept the allocation.
    // NOTE: this message deliberately avoids the token the workflow greps for
    // (the sanitizer's own "ERROR: Realtime..." report signature). An earlier
    // draft carried it here, which made a DEAD lane satisfy the workflow's
    // report-present assertion -- caught by running the step's own logic
    // against an uninstrumented build.
    std::printf ("realtime canary: FAILED -- the allocation was not intercepted; "
                 "this lane is not instrumenting anything.\n");
    return 1;
#endif
}
