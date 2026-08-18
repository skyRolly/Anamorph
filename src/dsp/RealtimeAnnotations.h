#pragma once

// ============================================================================
//  RealtimeAnnotations.h — the one spelling of the realtime function-effect
//  attribute, guarded so it costs nothing on the compilers that lack it.
//
//  WHAT IT IS. `ANAMORPH_NONBLOCKING` expands to Clang's
//  `[[clang::nonblocking]]` where the compiler has it, and to nothing
//  everywhere else. On a function it declares the REALTIME_AUDIO_POLICY
//  contract in the type system: no allocation, no lock, no blocking syscall.
//  Under `-fsanitize=realtime` (the `realtime` CI job, ADR-0029) the runtime
//  enforces that contract and aborts at the offending call with a symbolized
//  stack; in every other build it is inert.
//
//  IT IS A TYPE ATTRIBUTE, NOT A PREFIX ATTRIBUTE, and the difference is a
//  hard error rather than a warning -- `[[clang::nonblocking]] void f()` is
//  rejected with "attribute cannot be applied to a declaration". It goes
//  AFTER the parameter list and after `noexcept`:
//
//      void process (juce::AudioBuffer<float>& b) noexcept ANAMORPH_NONBLOCKING;
//
//  WHY THE GUARD IS `__has_cpp_attribute` AND NOT A COMPILER-VERSION TEST.
//  Three of this project's four shipped toolchains are not the pinned Clang:
//  GCC builds the shipped Linux binary, MSVC the Windows one, AppleClang the
//  macOS one. `__has_cpp_attribute` is the C++20-mandated feature test, so
//  each of them answers for itself instead of this header guessing from a
//  version number. GCC 13 accepts the guarded macro and warns
//  `-Wattributes: scoped attribute directive ignored` on the RAW spelling,
//  which is precisely the warning the guard exists to avoid -- the Clang
//  warning gate would otherwise take it as a new first-party diagnostic.
//
//  IT CHANGES NO CODE. The attribute carries a compile-time effect
//  declaration and, under RTSan, instrumentation. Outside an RTSan build it
//  affects neither mangling nor codegen -- verified by object comparison, see
//  ADR-0029 §Evidence. That is what lets it sit on a DSP_POLICY-frozen audio
//  path at all.
//
//  ANNOTATE DELIBERATELY, NOT BROADLY. `-Wfunction-effects` verifies the
//  contract at COMPILE time, but only for callees whose definitions the
//  translation unit can see; every JUCE call is opaque to it and warns. JUCE
//  9.0.1 carries no annotations of its own (measured: zero occurrences in the
//  pinned checkout), so annotating the call tree transitively produces dozens
//  of warnings about correct code. ADR-0029 records the resulting rule: the
//  annotation marks ENTRY POINTS for the runtime tool, and
//  `-Wfunction-effects` stays off until the dependency can answer it.
// ============================================================================

#if defined(__has_cpp_attribute)
  #if __has_cpp_attribute(clang::nonblocking)
    #define ANAMORPH_NONBLOCKING [[clang::nonblocking]]
  #endif
#endif

#ifndef ANAMORPH_NONBLOCKING
  #define ANAMORPH_NONBLOCKING
#endif
