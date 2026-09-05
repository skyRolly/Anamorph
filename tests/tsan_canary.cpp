// ============================================================================
//  tsan_canary.cpp -- the liveness proof for the `tsan` CI lane (TESTING_POLICY
//  rule 4: a checker must prove it is live before its silence is trusted).
//
//  The lane's entire output on a healthy tree is "no reports", which is also
//  exactly what a lane whose instrumentation stopped working prints. This
//  program commits one deliberate, unmistakable data race -- two threads
//  writing one plain int with no synchronisation of any kind -- and the lane
//  fails unless ThreadSanitizer reports it: a non-zero exit AND the report
//  signature, because a canary that died for some unrelated reason must not
//  pass for a live gate (the RTSan canary learned that the hard way).
//
//  Deliberately no JUCE, no atomics, no mutex: the race must be the only thing
//  in the program, so the only way it can be missed is the instrumentation.
// ============================================================================

#include <thread>
#include <cstdio>

namespace
{
    int shared = 0;   // plain, unsynchronised, written from two threads

    void hammer()
    {
        for (int i = 0; i < 100000; ++i)
            ++shared;
    }
}

int main()
{
    std::thread a (hammer), b (hammer);
    a.join();
    b.join();
    std::printf ("tsan canary: the seeded race ran (shared = %d); if this is all you see, the lane is NOT live\n",
                 shared);
    return 0;
}
