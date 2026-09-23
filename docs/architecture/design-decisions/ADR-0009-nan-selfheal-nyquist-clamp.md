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

## Implementation note — 2026-09-22 (PR #155, road-map R7)
The decision above was not fully implemented until this date; the gap is recorded rather than
closed silently. The guard reset every stateful node **except the wet-amount glides of
`HaasProcessor` and `VelvetNoise`**: `currentAmount += k·(target − currentAmount)` cannot leave
NaN, and neither module's `reset()` touched it. One non-finite `amount` from the host — JUCE's
parameter path passes NaN through — made every later block non-finite, and the guard zeroed
every block. Measured through the processor: **−180 dB** for Haas and Velvet after the host was
finite again, through a stop and play and through a host reset, until a re-prepare. Both
`reset()`s now reseed a non-finite glide to 0, parked identity (`src/dsp/HaasProcessor.cpp:41-42`,
`src/dsp/VelvetNoise.cpp:70-71`), and leave finite state untouched, so every other caller is
bit-identical. The decision is unchanged; the code now matches it. Regression coverage: State
test 123 (the parameter path) and Test 59 (a non-finite audio burst — under gcov the guard's
scrub-and-reset block had run zero times in either suite). Not this ADR's: an extreme but
*finite* burst passes untouched, as decided, and leaves Level Match displaced for seconds — an
ADR-0007 question (worklog `R7_PRODUCTION_PATH_COVERAGE.md`, §F14).

## Related code
- `src/dsp/MultibandWidth.cpp:55-71` (clamp+order); `SoloMonitor.cpp:41-57`; `MonoMaker.h:36-39` (setFrequency clamp)
- `src/dsp/AnamorphEngine.cpp:1842-1892` (NaN/Inf self-heal)
- `src/dsp/LevelMeters.h:98-102, 167` (`sanitize`)

Evidence [Verified]:
- Source: src/dsp/MultibandWidth.cpp:55-71; src/dsp/AnamorphEngine.cpp:1842-1892; src/dsp/LevelMeters.h
- Tests: testCrossoverAutomationSafe, testMeterRecoversFromNaN, testNoBadSamples,
  testNonFiniteBurstSelfHeals (Test 59), testAHostNanParameterDoesNotLatchTheChain (State test 123)
- History [Partially Verified]: CHANGELOG.md [0.8.2], [0.8.3]
- Related incidents: `../../POSTMORTEMS.md` INC-003 (crossover explosion), INC-004 (meter NaN-latch)
