# ADR-0009 — Crossover Nyquist clamp + engine-wide NaN/Inf self-heal; no output clipper

**0.10.0 processor-boundary clarification (Accepted ADR-0067, 2026-10-30):**
a non-finite APVTS raw value is now mapped to its declared finite default before the engine
sees it. Processor Density therefore targets 0.5, with the corresponding Level-Match
invalidation, rather than inheriting the engine's prior held Density. Direct engine/module
NaN protections and DSP tests 64/74 remain unchanged. Historical measurements below retain
their original scope; State tests 128/138 and 146/147 cover the approved processor behavior.

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
  (`src/dsp/AnamorphEngine.cpp:2139-2140`); every finite reading is unchanged. Test 65. The gain the
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
- `src/dsp/AnamorphEngine.cpp:2275-2327` (NaN/Inf self-heal)
- `src/dsp/LevelMeters.h:98-102, 167` (`sanitize`)

Evidence [Verified]:
- Source: src/dsp/MultibandWidth.cpp:55-71; src/dsp/AnamorphEngine.cpp:2275-2327; src/dsp/LevelMeters.h
- Tests: testCrossoverAutomationSafe, testMeterRecoversFromNaN, testNoBadSamples,
  testNonFiniteBurstSelfHeals (Test 59), testAHostNanParameterDoesNotLatchTheChain (State test 123),
  testNonFiniteGlideTargetsDoNotLatch (Test 64), testNonFiniteBurstKeepsLevelMatchAudible (Test 65),
  testANonFiniteVelvetDensityDoesNotFreezeTheDensity (State test 128)
- History [Partially Verified]: CHANGELOG.md [0.8.2], [0.8.3]
- Related incidents: `../../POSTMORTEMS.md` INC-003 (crossover explosion), INC-004 (meter NaN-latch)


## T-01 implementation correction — validate raw delay readback

The established output-finiteness requirement has no approved raw-bypass exception.
Historical ER-DSP-04 fixed the correlation-meter latch, while noting that bypass
could reintroduce non-finite raw samples after the processed-output guard. Test 59's
original Stereo/non-bypassed matrix did not exercise that path. Its assertions are
preserved and extended by `testNonFiniteBypassBoundary`.

A non-finite sample can be captured by the raw delay, then discarded by Left/Right
Only input conditioning. The processed output is finite, so the existing guard
correctly performs no reset; its conditional bypass-scratch clear never runs. Every
read from the existing raw delay into `bypassDryScratch` now substitutes zero for a
non-finite value. This covers both bypass and forced-duck consumers and history
recorded by the unchanged write-only path. The ring can retain invalid bits until
overwritten, but no consumer receives them: all reads pass through this boundary.
Finite values, including signed zero and headroom above unity, copy unchanged.

Alternatives considered: normalizing on every raw-ring write would also close the
path, but adds work to the common write-only branch; normalizing only in the final
bypass blend leaves the earlier forced-duck consumer uncovered; moving/duplicating
the processed-output guard would alter stage placement and require Architecture
Review. The selected check is within the existing raw-capture/readback node, with
no new node, signal-order change, thread path, buffering or reset-order change. It
implements this accepted requirement rather than choosing a new contract.

D-01's endpoint selection is independent: coefficient zero must retain processed
bits, interior coefficients keep their blend, and coefficient one must select the
validated raw sample. Tests cover the unmodified blend and combined correction.
D-02 remains distinct: an extreme finite sample (about `1e38` at +24 dB Output Gain)
can overflow the processed path and trigger its existing whole-scratch clear,
muting an unaffected channel. A finite-only readback check deliberately does not
change that reset or its Level-Match consequences. No broader scratch/reset
redesign or finite-audio limiter is authorized here.

## D-02 implementation correction — retain independent raw bypass data

Baseline: merged main `d428cc0a4d21c6a4804c9aad8b85d7fd68eb5c61`, after #233/#234.
A finite left sample near `1e38`, +24 dB Output Gain, Bypass On and a 64-sample
block reproduced 64/64 zero samples in the otherwise valid right channel (`0.25`).
The processed-output guard cleared both channels of the already finite raw scratch,
then discarded raw-delay history. Its stateful wet resets were necessary; those
raw clears were not, because T-01 validates every raw read before either consumer.

The same existing guard now retains raw scratch and ring contents. Stateful module,
Level-Match, conditioned dry and OS compensation resets retain their order and
scope. Non-finite raw history remains inaccessible without validation; inactive
scratch is refilled before any consumer can read it. Prepare/explicit reset keep
clearing raw history. This restores the existing valid-audio requirement inside
nodes 0b/6b without a graph, stage, threading, latency or parameter change.

Alternatives: retaining scratch alone still failed 140/1,512 focused assertions
because later latency reads lost valid history. Selectively scanning or clearing
raw poison adds work already performed at every read. Retaining the limitation
would preserve a reproducible valid-audio mute despite the small local correction.
Moving/adding a guard or redesigning self-heal is unnecessary and would need the
Architecture Review Gate. R-01 is an independent interpolation failure.

Evidence [Verified]: `testSelfHealRetainsRawBypass`, Test 59 extension, also
`AnamorphTests --bypass-selfheal-probe`; 1,512 assertions, 252 failures on the
baseline and zero with this correction. The independent R-01 regression still
fails 54/756 assertions with D-02 alone. These are production-engine witnesses,
not native-DAW incidence evidence. Earlier implementation notes retain their
historical scope; no accepted Decision is amended.
