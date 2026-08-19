// ============================================================================
//  fuzz_state.cpp — libFuzzer target for the HOST STATE-RESTORE path.
//
//  WHY THIS PATH. `setStateInformation` is the one entry point where a plug-in
//  parses bytes it did not write: a session file from an older version, a
//  preset from another machine, or a host that truncated a chunk. The state
//  suite already drives the *shaped* cases -- three legacy fixtures, a garbage
//  blob, an out-of-range A/B index, unknown fields, a corrupt slot -- and each
//  of those was written after someone thought of it. A fuzzer is the part that
//  does not have to think of it first.
//
//  LIFETIME IS THE DESIGN CONSTRAINT, not a detail, and the first draft of this
//  file got it wrong in a way worth recording. libFuzzer calls `exit()` when it
//  finishes, which runs static destructors. With the initialiser held in a
//  function-local `static`, its destructor ran `shutdownJuce_GUI()` ->
//  `DeletedAtShutdown::deleteAll()` DURING `__run_exit_handlers`, on an array
//  another exit handler had already released -- ASan reported a double-free in
//  `juce_HeapBlock.h:307` off `__run_exit_handlers`, on the EMPTY input, within
//  60 s of fuzzing. That is a static-destruction-ORDER artifact of the harness,
//  not a defect in `setStateInformation`.
//
//  So the initialiser is allocated once and DELIBERATELY NEVER DESTROYED:
//  JUCE's global teardown must not run at `exit()` in a fuzz harness. One
//  process-lifetime object is leaked on purpose, which is why the fuzz job runs
//  with `detect_leaks=0` -- stated here so the next reader does not "fix" it.
//
//    * the initialiser is leaked, so `shutdownJuce_GUI()` never runs;
//    * the processor is constructed and destroyed INSIDE each callback, so
//      every input also exercises construction and teardown, and nothing
//      plug-in-owned outlives the callback.
//
//  Constructing a processor per input is the slow choice and the correct one:
//  restore-then-destroy is where the interesting lifetime bugs are, and a
//  fuzzer that reused one processor would never see them.
//
//  WHAT IT ASSERTS. Nothing, deliberately: the oracle is the sanitizer. A
//  crash, a heap overflow, a use-after-free, an uninitialised read or an
//  unhandled exception fails the run; a rejected blob is a PASS, because
//  refusing malformed state is what this path is supposed to do.
//
//  The input is fed to the REAL entry point unmodified, so a crashing case
//  found here reproduces by handing the same bytes to a host.
// ============================================================================

#include "PluginProcessor.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
    // Constructed on first use and never destroyed -- see the lifetime note
    // above. The `static` here holds a POINTER, so nothing runs
    // `shutdownJuce_GUI()` when libFuzzer calls `exit()`.
    void ensureJuceInitialised()
    {
        static auto* const juceInit = new juce::ScopedJuceInitialiser_GUI();
        (void) juceInit;
    }
}

extern "C" int LLVMFuzzerTestOneInput (const std::uint8_t* data, std::size_t size)
{
    // `setStateInformation` takes an int; a size that does not fit is not a
    // case a host can produce, and truncating would silently test something
    // else.
    if (size > static_cast<std::size_t> (std::numeric_limits<int>::max()))
        return 0;

    ensureJuceInitialised();

    AnamorphAudioProcessor processor;

    // The real restore path, with the bytes exactly as given.
    processor.setStateInformation (data, static_cast<int> (size));

    // Round-trip after restore: a blob that leaves the processor in a state it
    // cannot then SERIALISE is a defect the restore call alone would not
    // surface, and this is where the A/B-index clamp class showed up.
    juce::MemoryBlock out;
    processor.getStateInformation (out);

    // Restoring what we just produced must also be safe -- the repeated-load
    // case, and the one a host performs on every session save/reload cycle.
    if (out.getSize() > 0 && out.getSize() <= static_cast<std::size_t> (std::numeric_limits<int>::max()))
        processor.setStateInformation (out.getData(), static_cast<int> (out.getSize()));

    return 0;
}
