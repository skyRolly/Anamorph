# Non-finite parameter state, and F13 (Level Match carry) — 2026-09-24

Round after PR #155. Scope: the non-finite-parameter failure class (the Mono Maker NaN latch found
in R6 §V, `MultibandWidth::setCrossovers` admitting NaN, and their relation to R7's Haas / Velvet
fix), then a measurement of F13. Investigation preceded every change; each finding ends in one of
confirm-and-fix / confirm-but-defer / investigate further / refute / preserve / owner decision.

Scratch evidence (harnesses, logs, per-lens reports) was produced outside the repository and is
summarised here with the numbers that decide each item.

## A. Baseline

- `main` at `659ca0a` (the PR #155 merge); this branch was restarted from it. No other branch is
  merged in.
- Linux x86-64, GCC, Release, the repository's own flags. DSP suite **518/0**, state suite
  **4791/0** before any change of this round.
- Platforms, hosts and formats not run this round: macOS / Windows / AArch64, any AU host, any
  commercial DAW, Standalone. A minimal VST3 host driving the built `.so` was used for reachability
  only (§B2); the `.so` predates `4767091`, and the code it exercised (parameters, processor,
  multiband, solo, Mono Maker) is unchanged since.

## B. Non-finite investigation

Five independent lenses (Mono Maker, Multiband, reachability, census of every continuous
parameter, contract), a synthesis, and an adversarial refutation that rebuilt every fix variant
itself. Unless stated, 48 kHz / 256-sample blocks; "muted" = a block the self-heal zeroed.

### B1. Mono Maker

- **Trigger.** A NaN cutoff present at any `prepare()`.
- **State transition** (not `reset()`, as the task warned):
  1. `MonoMaker::setFrequency` (`MonoMaker.h:41-44`) is a `jlimit`, which passes NaN: `targetFreq`
     becomes NaN. ±Inf are clamped (+Inf → 0.45·sr, −Inf → 20 Hz).
  2. A **live** NaN is harmless: the glide test `abs (currentFreq − targetFreq) > 0.05f`
     (`MonoMaker.cpp:37`) is false, so the filter keeps its last finite cutoff (0 muted).
  3. `snapToTargets()` — run by `MonoMaker::prepare` and by `AnamorphEngine::prepare` (`:180`) —
     copied the NaN into `currentFreq` and the crossover coefficients.
  4. From then on the glide test is false for every target, so a finite value never reaches the
     filter; `MonoMaker::reset()` clears only the filter state. Every sample is NaN, the self-heal
     zeroes every block (and, via the same path, the dry buffer Bypass plays from).
- **Measured (engine).** NaN at prepare: 20/20 muted → a finite 200 Hz 200/200 → host reset 50/50
  → forced swap 50/50 → a cutoff move 50/50 → a finite re-prepare 0/50. Live NaN then a re-prepare:
  20/20, then 200/200. Mono Maker off at prepare, switched on later: 198/200.
- **Reachability — engine API only.** `monoMakerFreq`'s centred log range turns NaN into 1.0
  (`PluginParameters.cpp:125-131`: `jmax (0, NaN)` = 0), and the APVTS adapter stores
  `convertFrom0to1 (getValue())`, so the raw atomic holds **500 Hz**. Measured: a host NaN (four bit
  patterns) and +Inf → 500 Hz, −Inf → 20 Hz, typed "nan" / "-nan" / "inf" → 500 Hz, a session with
  `value="nan" raw="nan"` → 120 Hz, the built VST3 (controller before activate, a 0-sample flush,
  the live queue) → 500 Hz. Only writing NaN straight into the raw atomic — no host path does —
  reproduced the mute (and silenced Bypass).
- **Impact.** Engine-API callers (tests, bench code) only. In production the laundering has its own
  side effect: a host NaN plays and saves as the range **maximum**, 500 Hz, while a NaN in a saved
  session restores as the default, 120 Hz (§F).
- **Confidence.** High (two lenses and the refuter reproduced it; four NaN bit patterns identical).
- **Decision: confirm and fix** (engine contract), §D.

### B2. MultibandWidth and Band Solo

- **Contract first.** ADR-0009 decision bullet 1 clamps every crossover to `[20, 0.45·sr]` so
  *finite* extreme automation cannot lift a cutoff past Nyquist — the incident it was written for
  (INC-003). It does not promise that a NaN is clamped (`jlimit` passes it), and no document states
  what a NaN split should do. The contract that does cover it is the self-heal: a non-finite sample
  is zeroed and the nodes reset, and "the plugin self-heals instead of needing a Multiband off/on".
- **Trigger and state.** `setCrossovers` (`MultibandWidth.cpp:104-106`) stores the NaN target. A
  live NaN is inert — every comparison against it is false, so neither the step detector nor the
  glide moves the bank. `setBankCutoffs` copies it into the coefficients on a reset, a prepare,
  Multiband enabled, Band Solo engaged, another split stepping > 1.5 octaves, or 4 → 3 bands; the
  output is then NaN and the self-heal zeroes it. Widths take a NaN into the smoothed width at the
  first sample.
- **Reachability — production.** A host NaN reaches the raw atomic; measured through the processor
  and the built VST3 (NaN before activation: 20/20 muted).
- **Impact.** Muted while NaN (Mix 0 and Bypass included); recovers exactly **one block** after a
  finite value, because the self-heal's `multiband.reset()` / `soloMonitor.reset()` re-snaps the
  filter to the now-finite target (removing either reset from the self-heal turned 1/200 into
  200/200). Side effects: the self-heal's full `loudness.reset()` discards the Level-Match gain
  (−6.546 → 0.000 dB, −17 dB then +1.1 dB against a control over 2 s), and the first large finite
  step after a NaN glides (586.7 ms instead of 16 ms). No latch survives a finite value.
- **Confidence.** High (lens + census + refuter, four rate/block configurations).
- **Decision: preserve.** It is inside ADR-0009's self-heal. Whether a NaN split should mute at all
  is the ingress question of §C, an owner decision; `setCrossovers` itself is not changed.

### B3. Census of every continuous parameter

39 rows × {NaN, +Inf, −Inf} × 4 scenarios × 3 contexts, 1,404 runs: **0 non-finite samples reached
the host.** Classes:

| class | fields | behaviour |
|---|---|---|
| ignored | discrete params (→ index 0 / off), balances, `monoMakerFreq` (→ 500 Hz) | no mute |
| transient, 1 block after | `haasDelay`, `mbWidth*` | muted while NaN |
| transient, reset-activated | `mbFreq*` | inert live; muted after a reset / prepare |
| transient, engine smoother | `width`, `mix` (plays 100% wet), `drive` | muted or altered while NaN; the 20–32 ms ramp after |
| transient, gain 0 | `outputGain` | exact zeros while NaN |
| transient, R7 fix holds | `amount` | 1 block, then Amount 0 |
| **latched, silent** | **`velvetDensity`** | **frozen until a finite re-prepare** |

### B4. Velvet density — a finding this round

- **Trigger.** One non-finite density.
- **State transition.** `currentDensity += k·(target − currentDensity)` absorbs NaN;
  `updateWeights` runs only when `abs (currentDensity − weightsDensity) > 0` (`VelvetNoise.cpp:351`),
  which is false for NaN; the output stays finite (the weights of the last finite density keep
  playing), so the self-heal never fires; `VelvetNoise::reset()` reseeds only `currentAmount`.
- **Reachability — production, without a buggy host.** A host NaN, and the text "nan" typed into the
  **editor's value box** (the parameter's own text parser `pctFrom` returns NaN; JUCE's VST3
  `fromString` returns it too).
- **Impact.** Live: output sample-identical to a processor that never left the old density while the
  twin moved (|A − B| 0.707) — the Density knob is dead. It survives a host reset, an algorithm
  switch, a forced duck and every knob move. A re-prepare while NaN builds **zero taps**: Velvet
  silent at any Amount. Only a re-prepare that sees a finite value clears it. CPU: the fast path is
  never taken again (11.3 → 14.8 µs per 256-sample block, rough).
- **Confidence.** High.
- **Decision: confirm and fix**, §D.

### B5. Other non-finite findings (not fixed here; §F)

- **Typed "nan" is production ingress for 11 knobs** (drive, amount, width, Haas delay, Velvet
  density, chorus rate / depth, mix, output gain, both balances): measured mutes width 20/20, output
  gain 16/20, chorus depth 18/20, mix 12/20, drive 11/20. The earlier premise — "a host that sends
  NaN is buggy" (State test 123's header) — is incomplete: the plug-in's own editor produces it.
- **Level Match "Apply" writes NaN into Output Gain** when the published match gain is NaN
  (`PluginProcessor.cpp:469`, the `jlimit` at `:515` passes it) — F13's third item; measured in §E.
- **Restore window.** A session PARAM with `value="nan"` and a usable `raw` leaves the raw atomic NaN
  inside the restore (one NaN host notification) and is saved again as `value="nan"` on every load.
- **Float → int UB on the audio thread**: UBSan float-cast-overflow at `HaasProcessor.cpp:49`
  (haasDelay NaN) and `ChorusEngine.cpp:48` (chorusRate / depth NaN). No crash on x86-64 (the index
  is masked); AArch64 not run.
- **A NaN parameter saves as `value="nan"`** and reloads as the default.

## C. Cross-module conclusion

**Two failure classes, not one invariant.**

1. **Latch — a glide that absorbs NaN and that no recovery path reseeds.** The shape is
   `current += k·(target − current)` (or a snap that copies the target), plus a `reset()` that does
   not touch it. Instances: R7's Haas / Velvet `amount` (fixed in R7 by a reseed in `reset()`,
   which works because a NaN amount produces a NaN sample and so fires the self-heal), **Velvet
   density** and **Mono Maker cutoff** (neither reachable by R7's route: density never produces a
   non-finite sample, and Mono Maker's `reset()` has no finite value to return to). This class
   violates the contract ADR-0009's consequence states — "self-heals instead of needing a Multiband
   off/on"; each needed a re-prepare. It is a module defect wherever it is, independent of ingress.
2. **Transient — a non-finite value changes the sound while it is present.** Multiband / Solo,
   Haas delay, Chorus, the engine smoothers, Output Gain. The self-heal bounds it and it ends one
   block (or one ramp) after a finite value. No document says a NaN parameter must not mute, and
   deciding what it should do instead is a parameter-semantics decision.

Per the task's classification:

| state | handled by | status |
|---|---|---|
| Haas / Velvet `amount` glide | reset reseeds (R7) | holds |
| Velvet density glide | module sanitizes (this round) | fixed |
| Mono Maker cutoff | module sanitizes (this round) | fixed (engine API) |
| Mono Maker Freq parameter | ingress sanitized (its range → 500 Hz) | holds; the value it picks is §F |
| Multiband / Solo crossovers and widths | self-heal handles | preserved |
| Haas delay, Chorus rate / depth | self-heal handles (Chorus reset zeroes its glides) | preserved; UB noted |
| width, mix, drive, output gain, balances | engine smoothers recover after their ramp | preserved |
| discrete parameters | read as index 0 / off | intentionally unsupported (finite but wrong) |
| extreme finite values | ADR-0009 "never alters valid audio" | intentionally unsupported |
| what NaN should mean on ingress | — | **unresolved: owner decision** |

**The narrowest reliable enforcement point for class 2** is `ParamPointers::toEngine`
(`PluginParameters.cpp:326+`): every host value passes through it into `EngineParameters`, on all
three of its callers (`PluginProcessor.cpp:231/234` prepare, `:267` message thread, `:447` audio).
A stateless variant (non-finite → the member default) was measured: 0 muted blocks in every NaN
leg, the Level-Match gain kept, the Velvet latch gone, all finite and ±Inf output hashes identical,
state suite 4791/0. It is **not applied**, because it decides what a NaN parameter means — an
Output Gain NaN would play at 0 dB (up to +24 dB above a −24 dB setting), Mix at 100% wet — and
ADR_POLICY lists "parameter semantics (meaning of a host-visible param)" as an ADR decision. The
alternatives each cross a harder line: holding the last finite value in `toEngine` needs per-field
state shared by three threads (threading-model hard stop); sanitising in the parameter classes'
`setValue` changes what the host reads back (ADR-0013's exact `getValue()`; Architecture Review
Gate). The owner decision is stated in §G.

Class 1 needs no such decision: the fix restores the finite contract after a finite value and does
not choose a meaning for NaN beyond "ignored", which is what `MonoMaker::process()` already did.

## D. Fixes

### Fix boundary

| criterion | Mono Maker `snapToTargets` guard | Velvet `setDensity` guard |
|---|---|---|
| all production paths | n/a in production (ingress launders); every engine path (both prepares, the forced-swap snap) | every path: the setter is the only writer of `targetDensity`; the snap and the glide read only it |
| finite unchanged | yes — the guard is true for every finite value | yes — same |
| deterministic NaN/Inf | yes: NaN keeps the current cutoff; ±Inf still clamped by `setFrequency` | yes: NaN and ±Inf keep the last finite target |
| no duplication | one line, where the NaN entered | one line, where the NaN entered |
| realtime-safe | no allocation, lock or wait (`std::isfinite`) | same |
| parameter semantics | unchanged: the parameter still holds and reports NaN | unchanged |

Rejected alternatives, measured: a reseed in `MonoMaker::reset()` (the R7 pattern) still zeroes
output, because `reset()` runs only after the self-heal has seen a NaN sample — 20/20 blocks while
NaN when it reseeds only toward a finite target (the Mono Maker lens's variant), 1 block per event
when it reseeds to a fixed 120 Hz (mutant below); a reseed of `currentDensity` in
`VelvetNoise::reset()` fails both new tests, because a NaN density never produces a NaN sample, so
nothing calls `reset()`. Forwarding only finite cutoffs from the engine would also drop the ±Inf
clamp.

### Code

- `src/dsp/MonoMaker.cpp:21` — `snapToTargets()` copies the target only if it is finite.
- `src/dsp/VelvetNoise.h:38-40` — `setDensity()` stores the target only if it is finite (+ `<cmath>`).

### Tests (each fails before the fix)

- **Test 64** (`testNonFiniteGlideTargetsDoNotLatch`, DSP suite, engine API — documents the engine's
  own contract, since production cannot deliver a NaN cutoff). Six legs (bad at the first prepare;
  live then a re-prepare; Mono Maker off at prepare then switched on; the same three for Velvet) ×
  five spellings (quiet NaN, payload NaN `0x7fc00001`, −NaN, +Inf, −Inf), each followed by a finite
  value, a host reset and a forced swap; bit-identical to a twin that kept the last finite target,
  plus an audibility control. Pre-fix: 24 of 30 legs differ — Mono Maker NaN muted 260/260, 270/320
  and 248/260 blocks; Velvet differs from the finite move on (NaN) or from the bad block (±Inf).
  Post-fix: 30/30 bit-identical, 0 muted.
- **State test 128** (`testANonFiniteVelvetDensityDoesNotFreezeTheDensity`, the real parameter
  lifecycle): host NaN live, the value-box text "nan" live, NaN before `prepareToPlay`, NaN then a
  re-prepare; then 0.9 and a host reset. Premise controls: the NaN reaches the raw parameter, "nan"
  parses to NaN, Mono Maker Freq lands finite (500 Hz). Pre-fix: all four legs differ (from the
  finite move, or from the re-prepare). Post-fix: bit-identical.

### Mutants

| mutant | DSP suite | state suite |
|---|---|---|
| Mono Maker guard removed | Test 64 fails | passes (production cannot reach it — as designed) |
| Velvet guard removed | Test 64 fails | State test 128 fails |
| Velvet: reseed `currentDensity` in `reset()` instead | Test 64 fails | State test 128 fails |
| Mono Maker: reseed in `reset()` instead | Test 64 fails | passes (as above) |

### Finite behaviour, bit-identical

A separate harness hashed the output of 186 finite legs on the pre-fix and post-fix builds: Velvet
density {0, 0.001, 0.3, 0.5, 1, −0, 1e-30, 2, −1} and Mono Maker {20, 120, 500, 0, −5, 1e-30, 3e38,
21600, 30000, −0, 200} Hz with Mono Maker on and off, at 44.1 / 48 / 96 kHz × 256 / 64 blocks, each
through prepare, a live jump (glide), a host reset, a re-prepare, a forced swap, another glide and a
re-prepare at 96 kHz / 64. **All 186 hashes identical** (all distinct from each other). Every other
line the DSP suite prints is identical before and after; the state suite differs only in its
known thread-timing counters. No smoothing constant changed.

## E. F13 — Level Match carry

F13_PENDING

## F. Remaining findings

F_PENDING

## G. Road map

G_PENDING
