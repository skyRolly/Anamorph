# ADR-0006 — Strict serial chain: Mono Maker post-Mix, Band Solo post-everything

**Status:** Accepted

## Context
Earlier versions placed Mono Maker *before* the widener and wove Band Solo into the Multiband
DSP. Both fought the Dry/Wet Mix, producing the 0.7.3–0.7.5 round of solo/low-cut bugs.

## Problem
Stages with feedback into routing decisions (solo-dependent paths, pre-widener low split) make
the Mix interaction ambiguous: "Solo leaks lows," "Mix=0 breaks Solo," low-cut under solo.

## Options
- **A. Keep solo/low-frequency special-casing inside the effect stages.** The bug source.
- **B. Rebuild as a strictly serial chain; make Mono Maker and Band Solo plain downstream
  stages.** Chosen (0.8.0).

## Decision
The chain is strictly serial: **Processing → Dry/Wet Mix → Mono Maker → Output → Band Solo
monitor**. Mono Maker collapses the lows of the **mixed** signal in place (post-Mix), so the low
end is mono at any Mix. Band Solo is **post-everything and monitoring-only**: it band-passes the
produced output and never changes any DSP stage; with nothing soloed the true output passes
through. The effect engine is **solo-agnostic** (always sums every band).

## Consequences
- The solo/low-cut bug class is structurally eliminated (no solo-dependent routing).
- Level Match measures the post-Mono-Maker output.
- See `DSP_GRAPH_REFERENCE.md` — Mono Maker post-Mix and Band Solo post-everything are hard
  reorder prohibitions.

## Related code
- `src/dsp/AnamorphEngine.cpp:761-766` (Mono Maker post-Mix), `:831-845` (Band Solo last)
- `src/dsp/MultibandWidth.h:29-32` (solo-agnostic)

Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.cpp:761-845
- Tests: testMonoMakerPostMix, testSoloMonitor, testLevelMatchAndSolo
- History [Partially Verified]: CHANGELOG.md [0.8.0]; commit 018dcdd
