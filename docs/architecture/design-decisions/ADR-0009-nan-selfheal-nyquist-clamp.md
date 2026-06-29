# ADR-0009 — Crossover Nyquist clamp + engine-wide NaN/Inf self-heal; no output clipper

**Status:** Accepted

## Context
Automating a Multiband split toward Nyquist (4 bands crowded high) pushed the ordered separation
above Nyquist, where Linkwitz-Riley coefficients blow up — a "+600 dB" burst that stuck one
channel and killed the other. A single NaN could also latch a meter envelope at NaN forever.

## Problem
The instability must be fixed at the source (not masked by limiting), and a stray non-finite
sample must not poison the output or meters. Adding a 0 dBFS clipper would harm dynamics/headroom.

## Options
- **A. Add an output limiter/clipper.** Rejected — destroys headroom; masks the real cause.
- **B. Clamp crossover frequencies Nyquist-safe + per-sample NaN/Inf self-heal.** Chosen.

## Decision
- Every crossover (Multiband, dry-align bank, Solo monitor, Mono Maker) clamps cutoffs to
  `[20, 0.45·sr]` with the 1.1× ordering enforced **top-down**, so separation can never lift a
  cutoff past Nyquist (the source fix).
- An engine-wide **per-sample** NaN/Inf guard replaces only non-finite samples with 0 and resets
  the stateful nodes (self-heal) — it is **not** a level limiter and never alters valid audio.
- Meters clamp every sample finite and flush any non-finite envelope back to its floor.
- **Confirmed: there is no 0 dBFS clipper anywhere** — dynamics and headroom are fully preserved.

## Consequences
- Extreme crossover automation stays bounded and NaN-free (test).
- Meters always recover after a non-finite burst (test).
- The plugin self-heals instead of needing a Multiband off/on.

## Related code
- `src/dsp/MultibandWidth.cpp:55-71` (clamp+order); `SoloMonitor.cpp:41-57`; `MonoMaker.h:36-39` (setFrequency clamp)
- `src/dsp/AnamorphEngine.cpp:847-870` (NaN/Inf self-heal)
- `src/dsp/LevelMeters.h:73-77,142` (`sanitize`)

Evidence [Verified]:
- Source: src/dsp/MultibandWidth.cpp:55-71; src/dsp/AnamorphEngine.cpp:847-870; src/dsp/LevelMeters.h
- Tests: testCrossoverAutomationSafe, testMeterRecoversFromNaN, testNoBadSamples
- History [Partially Verified]: CHANGELOG.md [0.8.2], [0.8.3]
- Related incidents: `../../POSTMORTEMS.md` INC-003 (crossover explosion), INC-004 (meter NaN-latch)
