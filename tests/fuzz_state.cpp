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
//  WHAT IT ASSERTS. The restore oracle is the sanitizer. Separately, mandatory
//  filesystem setup/ownership/cleanup invariants fail closed with a classified
//  harness error and abort; those are not automatically memory-safety findings. A
//  crash, a heap overflow, a use-after-free, an uninitialised read or an
//  unhandled exception fails the run; a rejected blob is a PASS, because
//  refusing malformed state is what this path is supposed to do.
//
//  The input is fed to the REAL entry point unmodified. Restore/round-trip
//  sanitizer findings retain those bytes for reproduction. A classified
//  FUZZ_HARNESS_FAILURE also requires filesystem/environment provenance;
//  its abort is not reproduced in a host by the input bytes alone.
// ============================================================================

#include "PluginProcessor.h"
#include "state_filesystem.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <cstring>

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

    // One disposable filesystem per input, destroyed BEFORE returning to the
    // fuzzer. This never runs JUCE's global shutdown and adds no exit handler.
    // A reused process must not compare against the previous input's override.
    anamorph::PresetManager::setTestPresetDirectory ({});
    state_test_filesystem::Profile profile;
    auto parent = juce::File::getSpecialLocation (juce::File::tempDirectory);
    const char* const control = std::getenv ("ANAMORPH_FUZZ_ISOLATION_CONTROL");
    if (control != nullptr && std::strcmp (control, "invalid-parent") == 0)
        parent = parent.getNonexistentChildFile ("invalid-fuzz-parent", "");
    if (! profile.initialise (parent))
    {
        (void) profile.cleanup();
        std::fprintf (stderr, "Fuzz isolation setup failed\n");
        std::fprintf (stderr, "FUZZ_HARNESS_FAILURE=setup\n");
        std::abort();
    }
    if (control != nullptr && std::strcmp (control, "bypass") == 0)
        anamorph::PresetManager::setTestPresetDirectory ({});
    if (anamorph::PresetManager::presetDirectory() != profile.directory().getChildFile ("presets"))
    {
        (void) profile.cleanup();
        std::fprintf (stderr, "Fuzz isolation override is not active\n");
        std::fprintf (stderr, "FUZZ_HARNESS_FAILURE=override\n");
        std::abort();
    }

    {
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
    } // processor teardown is still part of EVERY input, before profile cleanup

    // Report ACTUAL callback progression; libFuzzer's -runs budget is not an
    // exact entry-count oracle. Ordinary campaigns neither count nor pause here.
    const bool held = control != nullptr && std::strcmp (control, "hold") == 0;
    static std::uint64_t callbackSequence = 0;
    const auto sequence = held ? ++callbackSequence : 0;
    const auto witnessRoot = held ? profile.directory().getFullPathName() : juce::String();
    if (held)
    {
        std::printf ("FUZZ_ISOLATION_READY=%llu\t%s\n", (unsigned long long) sequence,
                     witnessRoot.toRawUTF8());
        std::fflush (stdout);
        if (std::getchar() == EOF)
        {
            (void) profile.cleanup();
            std::fprintf (stderr, "Fuzz isolation release failed\n");
            std::fprintf (stderr, "FUZZ_HARNESS_FAILURE=release\n");
            std::abort();
        }
    }
    anamorph::PresetManager::setTestPresetDirectory ({});
    if (! profile.cleanup())
    {
        std::fprintf (stderr, "Fuzz isolation cleanup failed\n");
        std::fprintf (stderr, "FUZZ_HARNESS_FAILURE=cleanup\n");
        std::abort();
    }

    if (held)
    {
        std::printf ("FUZZ_ISOLATION_CLEAN=%llu\t%s\n", (unsigned long long) sequence,
                     witnessRoot.toRawUTF8());
        std::fflush (stdout);
    }
    return 0;
}
