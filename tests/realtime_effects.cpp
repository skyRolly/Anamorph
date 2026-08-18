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
//  to an allocating helper inserted into it fails with
//  "function with 'nonblocking' attribute must not call non-'nonblocking'
//  function". Clean signal, and it fires -- which is the bar a gate has to pass.
//
//  SCOPE, deliberately narrow: the JUCE-free first-party leaves only. Adding a
//  header that calls into JUCE would reintroduce the 52-warning noise, so the
//  include list below is the contract. Modules that DO call JUCE
//  (AnamorphEngine, the oversampled stages) stay covered by the runtime tiers.
//
//  It is compiled `-fsyntax-only` by the `realtime` job -- no link, no run.
// ============================================================================

#include "dsp/MidSide.h"
#include "dsp/LR4Xover.h"
#include "dsp/ScopeBuffer.h"
#include "dsp/Correlation.h"
#include "dsp/LevelMeters.h"
#include "dsp/RealtimeAnnotations.h"

namespace
{
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
    }
}

// Never linked or run; the compile IS the check.
int main() { return 0; }
