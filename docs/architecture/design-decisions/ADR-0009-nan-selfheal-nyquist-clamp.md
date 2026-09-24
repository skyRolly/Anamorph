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

## Implementation note — 2026-09-24 (non-finite parameter state)
R7's note above said the code now matches the decision. A census of every continuous parameter
(worklog `NONFINITE_PARAMETERS_AND_F13.md`) found **two more glides of R7's shape** —
`current += k·(target − current)`, with a `reset()` that never reseeds it — and they now **ignore a
non-finite target**, the rule `MonoMaker::process()`'s own glide already followed
(`abs (current − NaN) > 0.05` is false):
- **Velvet density — production-reachable.** A host's NaN, and the text "nan" typed into the value
  box (the parameter's own text parser returns NaN), reach the raw parameter. The glide absorbed
  it, `updateWeights` never ran again, and the output stayed finite, so the self-heal never fired:
  the density froze through a host reset and every later knob move until a re-prepare that saw a
  finite value, and a re-prepare while NaN built zero taps (Velvet silent at any Amount).
  `VelvetNoise::setDensity` now keeps the last finite target (`src/dsp/VelvetNoise.h:38-40`).
- **Mono Maker cutoff — engine API only.** The parameter's range maps a host NaN to 500 Hz before
  the engine sees it (and −Inf to 20 Hz), so no production path delivers one. Through the engine
  API, `snapToTargets()` — run by every `prepare()` — copied a NaN target and the output was muted
  through a finite value, a host reset and a forced swap until a finite re-prepare. It now keeps
  the current cutoff, re-clamped to `[20, max(1000, 0.45·sr)]` for the rate being prepared
  (`src/dsp/MonoMaker.cpp:21-22`) — kept unclamped across a 96 → 44.1 kHz re-prepare, a 30 kHz
  cutoff sat above Nyquist and the output went unstable while finite (INC-003's class, measured
  on the first version of the guard). The ±Inf clamp is unchanged.

Finite values are bit-identical (186 finite legs hashed before and after here; an independent
check found 945 more identical). What a non-finite value produces depends on history: Velvet keeps
its last finite density target, and Mono Maker keeps its current cutoff (a live NaN already froze a
cutoff glide in flight); a fresh engine keeps the module's initial value (density 0.5, 120 Hz).
Regression coverage: Test 64 (the engine) and State test 128 (the parameter path, including the
value-box text).

- **Level Match — a node the guard did not reach.** `LoudnessMatch` publishes NaN from a non-finite
  input sample until the guard's `loudness.reset()` later in the same `process()` call, and in
  between the engine made that reading its match target: `decibelsToGain (NaN)` is 0, finite, so the
  guard never saw it, and it does not reset `matchGainSmooth`. A burst (a NaN in every 97th sample
  for 1 s) therefore ramped the applied gain to silence with Level Match on — 164–165 of 187 blocks
  exact zeros against 0–2 with it off. A NaN reading now keeps the current target
  (`src/dsp/AnamorphEngine.cpp:1736-1737`); every finite reading is unchanged. Test 65. The gain the
  full `loudness.reset()` discards after the burst is the owner question below, not changed.

**Recorded, not changed:**
- **Decision bullet 1 holds for finite values only.** `jlimit` passes NaN, so the crossover clamp
  does not bound a NaN split. A NaN crossover (Multiband, Band Solo) is inert while live, mutes
  after a reset, a prepare or a structural move, is caught by the self-heal, and recovers one block
  after a finite value — it latches nothing, which is this ADR's self-heal working as decided.
- **What a non-finite parameter value should mean is an owner decision** (ADR_POLICY: parameter
  semantics). Today, while a value is non-finite, several controls mute or change the sound
  (Width, Haas Delay, Chorus Rate / Depth, Output Gain, the multiband widths and crossovers mute;
  Mix plays fully wet), and the self-heal's full `loudness.reset()` discards the Level-Match
  gain; Mono Maker Freq's range already turns NaN into 500 Hz. The measured candidate — a
  stateless guard in `ParamPointers::toEngine`, non-finite → the parameter default, covering all
  three callers — removes every mute, but chooses a meaning (an Output Gain NaN would play at
  0 dB), so it is not applied here. The worklog states the options.
- **Stale anchors** in *Related code* below — `MultibandWidth.cpp:55-71` (the clamp is at
  `:104-116`) and `MonoMaker.h:36-39` (`setFrequency` is at `:41-44`) — reported for the
  documentation pass.

## Related code
- `src/dsp/MultibandWidth.cpp:55-71` (clamp+order); `SoloMonitor.cpp:41-57`; `MonoMaker.h:36-39` (setFrequency clamp)
- `src/dsp/AnamorphEngine.cpp:1866-1916` (NaN/Inf self-heal)
- `src/dsp/LevelMeters.h:98-102, 167` (`sanitize`)

Evidence [Verified]:
- Source: src/dsp/MultibandWidth.cpp:55-71; src/dsp/AnamorphEngine.cpp:1866-1916; src/dsp/LevelMeters.h
- Tests: testCrossoverAutomationSafe, testMeterRecoversFromNaN, testNoBadSamples,
  testNonFiniteBurstSelfHeals (Test 59), testAHostNanParameterDoesNotLatchTheChain (State test 123),
  testNonFiniteGlideTargetsDoNotLatch (Test 64), testNonFiniteBurstKeepsLevelMatchAudible (Test 65),
  testANonFiniteVelvetDensityDoesNotFreezeTheDensity (State test 128)
- History [Partially Verified]: CHANGELOG.md [0.8.2], [0.8.3]
- Related incidents: `../../POSTMORTEMS.md` INC-003 (crossover explosion), INC-004 (meter NaN-latch)
