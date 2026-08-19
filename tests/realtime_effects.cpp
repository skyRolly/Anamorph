// ============================================================================
//  realtime_effects.cpp — COMPILE-TIME realtime enforcement for the first-party
//  leaf layer, via Clang's `-Wfunction-effects`.
//
//  WHY THIS EXISTS BESIDE THREE OTHER TIERS (ADR-0029). RTSan and the allocation
//  guard are RUNTIME tools: they see what the suite executes.
//  `scripts/check-realtime.py` is a TEXT scan: it sees forbidden constructs
//  written literally inside an audio-path body. Neither follows a CALL:
//
//      void helper (int n) { sink.resize (n); }          // allocates
//      void leaf (int n) noexcept [[clang::nonblocking]]
//      { helper (n); }                                   // <- nothing above sees this
//
//  `-Wfunction-effects` does, because it verifies the effect through the call
//  graph. That is the gap this file closes, and it is the whole reason for it.
//
//  WHY IT IS A SEPARATE TRANSLATION UNIT AND NOT A BUILD-WIDE FLAG. Measured on
//  this tree: annotating `AnamorphEngine::process` and enabling the flag over
//  that TU produces 52 warnings, essentially all of them JUCE calls whose
//  definitions the TU cannot see (`Oversampling::reset` x9,
//  `FloatVectorOperations::copy` x6). Clang can only infer a callee's effects
//  from a VISIBLE DEFINITION, and JUCE 9.0.1 carries no annotations of its own,
//  so those warnings are about correct code. ADR-0029 records the decision not
//  to enable the flag build-wide.
//
//  What IS clean is the layer below JUCE: this project's own header-only DSP
//  leaves, whose bodies are pure arithmetic and pre-sized state. Measured: the
//  driver below compiles with ZERO `-Wfunction-effects` warnings, while a call
//  to an ALLOCATING helper -- the `ANAMORPH_EFFECTS_CANARY` block below -- fails
//  with "function with 'nonblocking' attribute must not call non-'nonblocking'
//  function". Clean signal, and it fires -- which is the bar a gate has to pass.
//
//  WHAT MAKES THE CANARY FAIL IS THE EFFECT, NOT THE MISSING ANNOTATION, and the
//  difference is visible in this very file. Clang INFERS a callee's effects
//  wherever it can see the definition, so `anamorph::applyWidth` -- unannotated,
//  `inline`, and called by the driver below -- is inferred nonblocking and
//  diagnoses nothing. An earlier version of this note offered that call as the
//  proof that the gate fires, which cannot be right: the gated compile contains
//  it and must stay clean. The helper that fails is the one whose body actually
//  blocks.
//
//  SCOPE, deliberately narrow: the JUCE-free first-party leaves only. Adding a
//  header that calls into JUCE would reintroduce the 52-warning noise, so the
//  include list below is the contract. Modules that DO call JUCE
//  (AnamorphEngine, the oversampled stages) stay covered by the runtime tiers.
//
//  It is compiled `-fsyntax-only` by the `realtime` job -- no link, no run.
//
//  IT IS COMPILED TWICE, and the second compile is the LIVENESS PROOF
//  (`TESTING_POLICY.md` rule 4). A clean compile is this gate's entire output,
//  and it is also exactly what a DEAD gate prints: Clang treats an unrecognised
//  `-Werror=<name>` as a mere `-Wunknown-warning-option` WARNING, so the day
//  `function-effects` is renamed or dropped, the step keeps exiting 0 while
//  checking nothing. Measured on the pinned Clang 22.1.8: with the option
//  misspelled, a translation unit carrying a REAL seeded violation compiles
//  with exit status 0.
//
//  So the `realtime` job compiles this same file a second time with
//  `-DANAMORPH_EFFECTS_CANARY`, which seeds the violation below, and asserts
//  that compile FAILS with a `-Wfunction-effects` diagnostic -- the same shape
//  as `tests/realtime_canary.cpp` proves the RTSan lane can fail. Seeding it
//  into THIS file rather than a separate canary TU is deliberate: it proves the
//  diagnostic on the exact translation unit, include set and flags the gate
//  actually uses, and leaves nothing to drift out of step with them.
// ============================================================================

#include "dsp/MidSide.h"
#include "dsp/LR4Xover.h"
#include "dsp/ScopeBuffer.h"
#include "dsp/Correlation.h"
#include "dsp/LevelMeters.h"
#include "dsp/RealtimeAnnotations.h"

#if defined(ANAMORPH_EFFECTS_CANARY)
  #include <vector>
#endif

namespace
{
#if defined(ANAMORPH_EFFECTS_CANARY)
    // THE SEEDED VIOLATION, compiled only for the liveness proof. Not annotated,
    // and it grows a `std::vector` -- which is how this project's DSP modules
    // actually allocate -- so a `nonblocking` caller of it is exactly the defect
    // `-Wfunction-effects` exists to report. The diagnostic comes from the CALL
    // GRAPH, which is this tier's whole reason to exist: the error is reported
    // at the CALL below, and Clang's notes walk down from there through
    // `vector::resize` and `_M_default_append`. The effect it finally names is
    // "throws or catches exceptions" rather than the allocation itself -- both
    // are blocking, both are what the attribute forbids, and the walk is the
    // property being proven live.
    std::vector<float> canarySink;

    void canaryAllocatingHelper (int n) { canarySink.resize ((std::size_t) n); }
#endif

    // The driver is annotated, so every leaf routine it calls must be inferable
    // as nonblocking. Clang walks the call graph from here.
    void leafAudioPath (float* l, float* r, int n,
                        anamorph::LR4Xover& xover,
                        anamorph::ScopeBuffer& scope,
                        anamorph::CorrelationMeter& corr,
                        anamorph::LevelMeters& meters) noexcept ANAMORPH_NONBLOCKING
    {
        for (int i = 0; i < n; ++i)
        {
            float mid, side, lo, hi;
            anamorph::MidSide::encode (l[i], r[i], mid, side);
            anamorph::applyWidth (l[i], r[i], 1.25f);
            anamorph::MidSide::decode (mid, side, l[i], r[i]);
            xover.processSample (0, l[i], lo, hi);
            l[i] = lo + hi;
            corr.process (l[i], r[i]);
        }

        meters.input.process (l, r, n);
        meters.output.process (l, r, n);
        meters.publish();
        corr.publish();
        scope.pushBlock (l, r, n);

#if defined(ANAMORPH_EFFECTS_CANARY)
        canaryAllocatingHelper (n);   // the gate must report this, or it is dead
#endif
    }
}

// Never linked or run; the compile IS the check.
int main() { return 0; }
