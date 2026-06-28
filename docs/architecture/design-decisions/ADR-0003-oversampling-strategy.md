# ADR-0003 — Oversampling wraps nonlinear stages only; minimum-phase IIR; exact PDC

**Status:** Accepted

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
with the integer-latency flag so **PDC is exact**. When nothing nonlinear is active, reported
latency is **0**. OS engagement is *latched* (changes only at reset / silent duck bottom) so
latency never changes mid-block; an OS-path change is routed through the duck.

## Consequences
- No latency/CPU when the chain is linear.
- IIR trade-off: mild phase response, but **no linear-phase pre-ringing / waveform misalignment**
  (the prioritised property).
- Chorus buffers are sized for the max (8×) rate so an OS-factor change never reallocates.

## Related code
- `src/dsp/AnamorphEngine.cpp:14-23` (engagement), `:42-54` (IIR + integer latency)
- `:293-329` (latched OS + latency), `:494-509` (OS-path change routed through duck)
- `src/dsp/ChorusEngine.cpp:14-19` (buffers sized for max rate)

Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.cpp:14-23,42-54,293-329
- Test: tests/dsp_tests.cpp :: testBypassNullAndLatency
- History [Partially Verified]: README:466-474
