# ADR-0003 — Oversampling wraps nonlinear stages only; minimum-phase IIR; exact PDC

**Status:** Accepted — **latency clause amended by [ADR-0034](ADR-0034-latency-follows-the-oversampling-factor.md)** (2026-09-03)

## Context
Only nonlinear/modulation stages (Drive's tanh, Chorus, Dimension-D) generate aliasing that
oversampling mitigates. Oversampling adds latency and CPU.

## Problem
Oversampling the whole chain would add needless latency/CPU and force PDC even when nothing
nonlinear is active. The anti-aliasing filter choice also affects transient/phase behaviour.

## Options
- **A. Oversample the entire chain.** Simple; wasteful; always-on latency.
- **B. Oversample only Drive + Chorus/Dim-D; linear stages stay at base rate.** Chosen.
- **Filter: linear-phase FIR vs minimum-phase IIR.** IIR chosen.

## Decision
The OS wrap engages only when `oversample != Off && (driveDb > 0.01 || isModAlgorithm(algorithm))`.
Linear stages (Haas, Velvet, Width, MS, Mono Maker, crossovers) run outside it. Oversamplers are
JUCE **minimum-phase polyphase IIR** half-band filters (orders 1/2/3 for 2×/4×/8×), constructed
with the integer-latency flag so **PDC is exact**. ~~When nothing nonlinear is active, reported
latency is **0**.~~ **AMENDED by [ADR-0034](ADR-0034-latency-follows-the-oversampling-factor.md):**
the reported latency is a function of the selected FACTOR alone — when nothing nonlinear is active
the wrap is still skipped, but `osCompDelayBuffer` supplies its group delay so the number does not
move. Reporting 0 there made an ordinary Drive or Algorithm move a host-graph restart. Everything
else in this Decision stands: the engagement predicate, which stages are wrapped, and the filter
choice are unchanged. OS engagement is *latched* (changes only at reset / silent duck bottom) so
latency never changes mid-block; an OS-path change is routed through the duck.

## Consequences
- ~~No latency/CPU when the chain is linear.~~ **No CPU** when the chain is linear — the wrap is
  still skipped, which is the whole saving. The latency half is reversed by ADR-0034: a selected
  factor reports its latency whether or not the wrap runs.
- IIR trade-off: mild phase response, but **no linear-phase pre-ringing / waveform misalignment**
  (the prioritised property).
- Chorus buffers are sized for the max (8×) rate so an OS-factor change never reallocates.

## Related code
- `src/dsp/AnamorphEngine.cpp:24-35` (engagement), `:52-66` (IIR + integer latency)
- `:293-329` (latched OS + latency), `:494-509` (OS-path change routed through duck)
- `src/dsp/ChorusEngine.cpp:14-19` (buffers sized for max rate)

Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.cpp:24-35, 52-66, 374-448
- Test: tests/dsp_tests.cpp :: testBypassNullAndLatency
- History [Partially Verified]: docs/architecture/LATENCY_MODEL.md
