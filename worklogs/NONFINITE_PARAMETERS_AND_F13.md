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
     (`MonoMaker.cpp:38`) is false, so the filter keeps its last finite cutoff (0 muted).
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
  twin moved (|A − B| 0.707 in the contract lens's setup, 0.508 in the guard check's) — the Density
  knob is dead. It survives a host reset, an algorithm
  switch, a forced duck and every knob move. A re-prepare while NaN builds **zero taps**: Velvet
  silent at any Amount. Only a re-prepare that sees a finite value clears it. CPU: the fast path is
  never taken again (11.3 → 14.8 µs per 256-sample block in one measurement, 10.4 → 12.5 µs in
  another; rough).
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

### D1. Non-finite parameters (§B)

### Fix boundary

| criterion | Mono Maker `snapToTargets` guard | Velvet `setDensity` guard |
|---|---|---|
| all production paths | n/a in production (ingress launders); every engine path (both prepares, the forced-swap snap) | every path: the setter is the only writer of `targetDensity`; the snap and the glide read only it |
| finite unchanged | yes — the guard is true for every finite value | yes — same |
| deterministic NaN/Inf | yes: NaN keeps the current cutoff, re-clamped for the rate; ±Inf still clamped by `setFrequency` | yes: NaN and ±Inf keep the last finite target |
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

- `src/dsp/MonoMaker.cpp:21-22` — `snapToTargets()` copies the target only if it is finite, and
  otherwise keeps the current cutoff re-clamped to `[20, max(1000, 0.45·sr)]` for the rate being
  prepared. The first version kept it unclamped; the adversarial guard check found that a 30 kHz
  cutoff set at 96 kHz, then NaN, then a re-prepare at 44.1 kHz sat above Nyquist and made the LR4
  unstable while finite (max |x| up to 2.47e38, the self-heal never fired — INC-003's class; the
  pre-fix code muted instead). Engine API only. The re-clamp closes it and Test 64 now has the leg.
- `src/dsp/VelvetNoise.h:38-40` — `setDensity()` stores the target only if it is finite (+ `<cmath>`).

### Tests (each fails before the fix)

- **Test 64** (`testNonFiniteGlideTargetsDoNotLatch`, DSP suite, engine API — documents the engine's
  own contract, since production cannot deliver a NaN cutoff). Eight legs (Mono Maker: bad at the
  first prepare; live then a re-prepare; off at prepare then switched on; a re-prepare from 96 to
  44.1 kHz with a 30 kHz cutoff. Velvet: bad at the first prepare; live; live then a re-prepare; a
  NaN landing on a density glide in flight) × five spellings (quiet NaN, payload NaN `0x7fc00001`,
  −NaN, +Inf, −Inf), each followed by a finite value, a host reset and a forced swap; bit-identical
  to a twin that kept the last finite value, plus an audibility control. Pre-fix (genuine `659ca0a`
  engine objects): 32 of 40 legs differ — Mono Maker NaN muted 248–270 of 260–320 blocks; Velvet
  differs from the finite move on, or from the bad block itself (±Inf on the live legs, every
  spelling mid-glide). The first, unclamped Mono Maker guard fails the rate-drop leg. Post-fix:
  40/40 bit-identical, 0 muted.
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
| Mono Maker: the first guard, current cutoff kept unclamped | Test 64 fails (rate-drop leg, 3 NaN spellings) | not run |

The four rows above ran the full suites against the 30-leg Test 64; the last row ran the DSP suite
against the 40-leg version. The guard check also built nine plausible wrong fixes: every one that
changes behaviour is caught by Test 64 except three that differ only when a NaN lands mid-glide —
which is why the mid-glide leg was added — and a guard moved into the engine's `updateDerived`,
which is equivalent on every leg run.

### Finite behaviour, bit-identical

A separate harness hashed the output of 186 finite legs on the pre-fix and post-fix builds (re-run
after the re-clamp, still identical): Velvet
density {0, 0.001, 0.3, 0.5, 1, −0, 1e-30, 2, −1} and Mono Maker {20, 120, 500, 0, −5, 1e-30, 3e38,
21600, 30000, −0, 200} Hz with Mono Maker on and off, at 44.1 / 48 / 96 kHz × 256 / 64 blocks, each
through prepare, a live jump (glide), a host reset, a re-prepare, a forced swap, another glide and a
re-prepare at 96 kHz / 64. **All 186 hashes identical.** The adversarial guard check built its own
pre-fix objects from `git archive 659ca0a` and found 945 more finite legs identical (648 engine legs
over 22.05–192 kHz, blocks 1–4096, Oversampling Off–8×; 288 processor legs through A/B, undo / redo,
preset loads, host reset and re-prepare mid-glide; 9 direct module legs). Every other
line the DSP suite prints is identical before and after; the state suite differs only in its
known thread-timing counters. No smoothing constant changed.

### D2. From F13

Apply's NaN guard and the match-target guard — measured and decided in §E3, specified in §E4.

## E. F13 — Level Match carry

ADR-0007 was read first (including its three notes and two corrections). Five lenses — processor
routes, engine decomposition with counterfactual variants, the contract, non-finite Level Match, and
an adversarial check of §D's guards — then a synthesis that re-ran every deciding number, then a
refuter that rebuilt its own harnesses. Oracle: a fresh instance at the destination state, same
input, converged. Stationary seeded noise at two levels, 48 kHz / 256 and 44.1 kHz / 512; no music,
transients or silence gaps; Linux x86-64 only.

### E1. What each transition carries

| transition | integrators / K-filters | published gain | `prevPredictedGainDb` | `matchGainSmooth` |
|---|---|---|---|---|
| forced swap, continuous-only (preset / undo / redo) | **carried** (`procChanged` false, no re-arm) | carried | carried | carried; target = carried published gain |
| forced swap, discrete | cleared (`softReset`) | carried | carried | carried |
| live continuous edit (no duck) | carried | carried | carried | carried |
| A/B (forced + injection) | carried if continuous-only, cleared if discrete | **overwritten** by the slot's value | carried | **snapped** to the slot's value |

### E2. Trajectories (processor, 48 kHz / 256; dB, seconds after the route)

| delta (G*) | route | bottom pub / applied | overshoot 0–0.3 s | t0.5 / t0.1 | residual 6 s |
|---|---|---|---|---|---|
| Drive 0→8 (−6.164) | live | −3.714 / −0.156 | +5.10 | 2.31 / 3.77 | +0.008 |
| | user preset | −3.727 / −0.156 | +4.99 | 2.30 / 3.77 | +0.008 |
| | undo, redo | −3.727 / −0.30 | +4.89 | 2.30 / 3.77 | +0.008 |
| | A/B (inj −6.138) | −5.630 / −6.114 | +2.63 | 2.23 / 3.69 | +0.008 |
| R6 edit→base (−5.456) | live | −7.588 / −7.587 | min −2.13 | 1.88 / 3.36 | −0.006 |
| | preset, undo, redo | −7.59…−7.56 | min −2.9 | 1.87 / 3.35 | −0.005 |
| | A/B (inj −5.428) | −5.611 / −5.436 | min −1.01 | 1.10 / 2.75 | −0.003 |

Preset, undo and redo that move only continuous controls stay within 0.27–0.45 dB of the same live
edit after 0.1 s and land on the same end state. **Factory presets turn Level Match off**: the load
returns every omitted parameter to its default and `autoGainMatch` defaults to off, so "Level Match
on through a preset" needs a user preset.

### E3. Decisions

- **F13(1a) — the published gain carried across a forced swap: preserve (intended).** A
  continuous-only swap re-arms nothing, so the result is untouched; a discrete one `softReset`s,
  which keeps the result by ADR-0007's rule. The forced route tracks the live edit.
- **F13(1b) — `matchGainSmooth` left out of `snapSmoothers()`: owner decision.** It changes nothing
  when Level Match is on in both states (the smoother sits 0.003 dB from its target at the bottom;
  snapping it gives identical output). It matters when a forced swap turns Level Match **on** — in
  production, **Undo of Apply** — where the smoother, parked at unity while Level Match was off,
  swells the output **+4.0 dB (Drive 8) / +4.8 dB (Drive 10) above both endpoints**, peaking 128 ms
  after the undo; a hand re-engage after Apply swells the same way through a non-forced duck. No test
  covers it. The exclusion is a deliberate carve-out in the code (`AnamorphEngine.cpp:724`), ADR-0004
  says "snap smoothers" without naming it, and the documents that call the engage smoothed promise no
  click, not this — so the evidence points one way, but the rule is the owner's. Options and measured
  effects: ADR-0007 note 2026-09-24, question 1. **Reopen:** an owner ruling; this is the one F13
  item with an audible, everyday trigger. Re-measured, widened (any engage without an A/B injection)
  and put to the owner with six measured behaviours in §I (decision record §I6).
- **F13(2) — continuous-only swaps do not re-arm; the "0.56–0.6 dB / ~4 s" claim.** CONFIRMED only for
  R6's A/B setup (0.63 dB off G* at 0.57 s, within 0.1 dB at 2.75 s, 0.023 dB at 4 s); REFUTED as a
  general statement: on A/B it is 0.22–5.8 dB depending on the delta and the programme, and on preset
  / undo / redo it is the live edit's own re-convergence. The one route-specific defect is **A/B
  between slots that differ only in continuous controls**: the injected gain is right (0.03 dB from
  G*) and the stale integrators pull it away within ~100 ms for 1.8–4.5 s. Re-arming at the injection
  (B1) holds it within 0.013–0.042 dB and passes both suites, but it changes ADR-0007's "re-armed in
  exactly one place" rule and reverses the A/B row of its dimMode table (identical slots re-arm) —
  **owner decision, hard stop (conflict with an Accepted ADR)**. Re-arming on every forced duck fails
  Test 58 and is ruled out. **Reopen:** an approved ADR-0007 amendment.
- **F13(3) — no non-finite guard in `LoudnessMatch`.**
  - **(3a) the in-block NaN is published:** preserve. It is never non-finite after `process()`
    returns (0 of 117 configurations, 0 of 12,288 late-NaN runs): the self-heal's full reset follows
    in the same call. Guarding the store instead hides Test 59's check on that reset (measured).
  - **(3b) a NaN seen only by the dry reference:** refuted across 12,288 configurations.
  - **(3c) Apply locks a NaN into Output Gain: confirm and fix** (§D2). A clear violation of
    ADR-0007 ("Apply locks the measured gain") and of ADR-0008's Undo rule.
  - **(3d) the in-block NaN became the match target: confirm and fix** (§D2). `decibelsToGain (NaN)` is
    0 — finite, so the self-heal never saw it, and it never resets `matchGainSmooth`. A sustained
    burst silenced the output with Level Match on (164–165 of 187 blocks against 0–2 off, 13.7–14.5 dB
    under the matched level). The same class R7 fixed: a stateful node the self-heal does not reach.
    One stray NaN costs only a −0.39 dB dip either way.

### E4. Fixes from F13 (§D2)

- `src/PluginProcessor.cpp:469` — `applyAutoGain` returns when the published gain is NaN (`std::isnan`:
  ±Inf cannot be published, and the `jlimit` still bounds it). Every finite Apply bit-identical
  (writes, notifications, undo step, saved-state hash) over 14 gains incl. ±24, ±30, ±1e-30, −0.
  **State test 129** (revised after the Devin review, §I): leg A hands Apply the NaN through the
  processor's test seam `seams.atApplyMeasurement` and requires the seam to fire (Apply ran to its
  measurement) and nothing to be written; leg B is the finite control through the same seam; leg C
  is the real window, reported as corroboration. Pre-fix: leg A fails five checks with its liveness
  satisfied; Apply disabled: A's and B's liveness fail. An earlier version relied on the real window
  alone and could pass vacuously on a serialised scheduler (valgrind 1 of 4 runs reached it, one
  pinned CPU 1 of 8).
- `src/dsp/AnamorphEngine.cpp:1736-1737` — a NaN reading keeps the current match target (TG).
  **Test 65**: pre-fix both level checks fail; post-fix silent blocks equal Level Match off's and the
  burst plays at the off level plus the pre-burst match gain (e.g. −13.71 − 5.12 = −18.83 dB). Test
  59's Level-Match-on lines move 0.51–2.83 → 0.54–2.80 dB (bound 6 dB); nothing else in either suite
  moves. With the self-heal's `loudness.reset()` removed, Test 59 still fails (1 check), so TG keeps
  the detection a `LoudnessMatch`-side guard would lose.
- Four comments that said an A/B swap glides or re-arms now say what the code does (comment-only).

## F. Remaining findings

Each is recorded, not fixed, with the reason.

| finding | evidence | status | why not now |
|---|---|---|---|
| **What a non-finite parameter means on ingress** (the mutes of §B3 class 2, the Level-Match wipe by the self-heal) | §B2, §B3, §C; stateless `toEngine` guard measured | **owner decision** (§G) | chooses a meaning for a host-visible value (ADR_POLICY); the audible cost of "default" is a louder Output Gain |
| **Text entry accepts "nan"** — the value boxes and VST3 `fromString` (`pctFrom`, `hzFrom`, `khzFrom`, `balFrom`, JUCE's default parser) | §B5; State test 128 premise control | owner decision, with the above | one change at the parsers would cover the editor and host text entry, but what "nan" should become is the same question |
| **Mono Maker Freq launders NaN to 500 Hz**, saved as a legitimate value; a NaN in a session restores 120 Hz | §B1 | owner decision, with the above | the range's inverse lambda decides it; the default-vs-maximum choice is the same parameter-semantics question |
| **Restore window**: `value="nan"` with a usable `raw` leaves the raw atomic NaN inside the restore and is re-saved on every load | §B5 (lens-measured with a seam, not under real concurrency) | investigate further | touches the serialized-value repair path (`PluginParameters.h:185`); needs its own measurement under the real restore |
| **Float → int UB** at `HaasProcessor.cpp:49`, `ChorusEngine.cpp:48` for a NaN delay / rate / depth | UBSan float-cast-overflow; no crash on x86-64 | defer | resolved by any ingress rule; a module guard alone would be the "modify every module" pattern the task warned against. AArch64 not run |
| **Discrete parameters read NaN as index 0 / off** (algorithm → Haas, `mbBands` → 1, `advancedMode` → Simple); `roundToInt (NaN)` depends on the bit pattern | §B3 | preserve | finite, bounded, no latch; the same owner decision covers it |
| **The first large step after a NaN crossover glides** (586.7 ms instead of 16 ms) | §B2 | preserve | only after a non-finite episode; audible effect not measured |
| **Stale citations** — ADR-0009 *Related code* (`MultibandWidth.cpp:55-71`, `MonoMaker.h:36-39`), `DSP_POLICY.md:55`, `THREAD_MODEL.md:99` (`toEngine` also runs on the prepare and message threads), State test 123's header premise ("a host that sends one is buggy") | read against the code | documentation pass | reported, not rewritten (no general cleanup this round) |
| **F13(1b) the Undo-of-Apply swell** (+4.0 / +4.8 dB, 128 ms) and the hand re-engage swell — every engage without an A/B injection (§I: +2.3 / +4.5 / +5.7 dB at Drive 4 / 8 / 10) | §E3, §I | **owner decision** (KI-031; record §I6) | changing the forced-bottom or engage behaviour of the match smoother is ADR-0007 / ADR-0004 territory; no test covers it yet |
| **F13(2) A/B between continuous-only slots** re-converges for 1.8–4.5 s after a correct injection | §E3 | **owner decision; hard stop** | B1 conflicts with ADR-0007's re-arm rule and its dimMode table |
| **Level Match gain discarded by the self-heal** (full `loudness.reset()`), after a NaN parameter or a NaN burst | §B2, §E3 | owner decision (ADR-0009's existing question) | unchanged by TG, which only keeps the target during the burst |
| **Level Match convergence after any large Drive change** (+5 dB for ~2.5 s on a Drive 0 → 8 raise, live or forced) | §E2 | preserve | ADR-0007 design: the predict floor anticipates part of the boost, the measure glides at τ 0.9 s |
| **Factory presets turn Level Match off** | §E2 | preserve (record) | presets restore every omitted parameter's default; a product question, not a defect |
| **Engine-API finite overflow** −3e38 → +3e38 still latches the Velvet density (the glide overflows to NaN) | guard check | defer | the parameter range is [0, 1]; no production path |
| **State test 39 failed once** (6 preset-identity checks) in 12 local runs while workflow harnesses ran concurrently; 0 failures in 5 idle runs afterwards | §G note | investigate on recurrence | loads presets by index from a list that includes the shared user folder; consistent with a concurrent writer there, not proven |
| **Stale anchor** `DSP_ALGORITHMS.md` Mono Maker recombine `.cpp:39-45` (the recombine is at `:49-51`; wrong before this round) | read against the code | documentation pass | pre-existing |

## G. Road map

| item | decision | reason | reopen when |
|---|---|---|---|
| Velvet density latch; Mono Maker cutoff latch | **done** (§D1) | module glides that absorbed NaN; finite unchanged | a new glide shape without a reseed |
| Apply locks NaN; NaN match target | **done** (§E4) | clear violations of ADR-0007 / ADR-0009 | — |
| Multiband / Band Solo NaN | **preserve** | inside ADR-0009's self-heal; recovers in one block | the owner rules on ingress |
| **What a non-finite parameter means on ingress** (`toEngine` default vs hold-last vs status quo; text parsers accepting "nan"; Mono Maker's 500 Hz; the self-heal's Level-Match wipe) | **owner decision** | parameter semantics (ADR_POLICY); the measured candidate plays an Output Gain NaN at 0 dB | an owner ruling — then one stateless change in `toEngine`, plus the parsers if chosen |
| **F13(1b) Undo-of-Apply swell** | **owner decision — recommended next** (§I6: O4g recommended, O4 / O2 alternatives) | +2.3–5.7 dB above both endpoints on an everyday action; every candidate that lands the gain leaves both suites and the measurement unchanged | an owner ruling (and a test for the swell either way) |
| **F13(2) A/B continuous-only re-arm (B1)** | **owner decision; hard stop** | conflicts with ADR-0007's re-arm rule and dimMode table | an approved ADR-0007 amendment |
| Restore window (`value="nan"` + usable `raw`) | **investigate** | lens-measured through a seam only | a measurement under the real restore |
| Float → int UB (Haas delay, Chorus) on NaN | **defer** | resolved by any ingress rule | the ingress decision, or an AArch64 run |
| F12 (Advanced Mode host write), F10 (re-entrant mouseUp), F9 (adoption under the held lock), R6a–R6d | **preserve unchanged** | outside this round | as recorded in the R6 / R7 worklogs |
| The standalone documentation pass | **defer** | stale anchors reported here (ADR-0007, ADR-0009, DSP_POLICY, THREAD_MODEL, DSP_ALGORITHMS) | its own round |
| ScopeBuffer race, vectorscope stop-state | **preserve unchanged** | outside this round | as recorded |

## H. Validation

Linux x86-64 only; macOS, Windows, AArch64 and MSVC are CI's. No DAW, AU host or Standalone build
was run.

- **Suites, final tree (Release, GCC):** DSP 524 / 0, state 4802 / 0 — also under `ulimit -s 1024`.
  Every printed line outside the new tests and Test 59's Level-Match-on legs is identical to the
  baseline; the state suite differs only in its thread-timing counters.
- **ASan + UBSan (Clang 18), final tree:** DSP 522 / 0 (this build config runs 2 fewer checks, as in
  earlier rounds), state 4802 / 0, 0 runtime errors. **FMA contraction** (`-march=haswell
  -ffp-contract=on`, the arm64-float stand-in), final tree: DSP 524 / 0, state 4802 / 0.
- **Gates:** `check-realtime`, `check-dispatch`, `check-portability`, `check-docs` (+ self-test),
  `check-state-coverage` (+ self-test), `check-citations` (+ self-test; `--check` against `HEAD`,
  `HEAD~3` and `659ca0a`): all exit 0. GCC with the warning gate's flags and Clang with the
  project's warning set: 0 warnings on any added line.
- **Pre-fix proof:** Test 64 32 / 40 legs, State test 128 4 / 4 legs, State test 129 3 checks,
  Test 65 2 checks — each against the code it guards (genuine `659ca0a` objects for Test 64).
  Mutants and rejected alternatives: §D1, §E4.
- **Finite bit-identity:** 186 legs here and 945 in the independent check (§D1); Apply over 14
  finite gains (§E4); TG leaves every suite line unchanged but Test 59's non-finite legs.
- **State test 39** failed once (6 preset-identity checks) in 12 local runs made while workflow
  harnesses were running on the same machine, and passed in 5 idle runs afterwards; recorded in §F.
- **Hard-stop classes:** none touched — no parameter ID, range, default or schema change, no
  threading or DSP-order change, no latency change; the ADR-0007 re-arm question that would conflict
  with an Accepted ADR is recorded, not implemented.

## I. Devin-review round (2026-09-24): State test 129, the coverage audit, F13(1b)

Round after `57c2967` on the same branch (PR #156). Two review findings first, then F13(1b)
reproduced and put to the owner. No behaviour change in this section.

### I1. Review findings

- **State test 129 could pass without Apply reading a NaN.** The window is inside `engine.process()`
  (between `loudness.process` and the self-heal's reset); a serialised scheduler almost never lands a
  click in it, and the test required only that the audio thread ran. Fixed in the test and a
  message-thread seam (§E4; `docs/procedures/TESTING.md`). Pre-fix: leg A fails five checks with its
  liveness met, leg C a sixth; Apply disabled: A's and B's liveness fail (`reached 0`); one pinned CPU
  twice: 4809 / 0 with A and B reached.
- **No coverage entry for the round.** Added (`DOCUMENTATION_COVERAGE.md` 76th and 77th passes),
  with KI-029 / KI-030 for the two owner-decision limitations. The repository has no gate that
  checks a new test is named in a pass; a scratch cross-check (every `Test N` / `State test N` added
  since `659ca0a` named in `TESTING.md` and in a pass) passes on the tree and reports each entry
  removed from it (Test 65, State test 128).
- **Non-finite fixes on the new head:** DSP 524 / 0, state 4809 / 0; the DSP log is byte-identical to
  the previous round's, the state log differs only in thread-timing counters and State test 129's new
  leg; the 186 finite legs hash identically to the pre-fix build.

### I2. F13(1b) reproduced

Measured through `AnamorphAudioProcessor` on this head: 48 kHz / 256, stereo pink noise at −18 dBFS
RMS, Haas 0.5, Width 1.3, Advanced on, oversampling off. **Excess** = level above BOTH the same run
without the event and a fresh instance at the destination (K-weighted, 100 ms window; this cancels
the programme's own 0.5–0.8 dB fluctuation). **Settle** = time until within 0.1 dB of the fresh
instance.

| route (Drive 8 unless stated) | published match | excess, peak time | settle |
|---|---|---|---|
| Undo of Apply, Drive 4 / 8 / 10 | −3.89 / −6.87 / −8.45 dB | **+2.32 / +4.45 / +5.67 dB**, 128–130 ms | 523 / 610 / 647 ms |
| hand re-engage after Apply | −6.87 | **+4.44**, 128 ms | 606 |
| hand engage from Output Gain −12 dB (no Apply) | −6.80 | **+4.51**, 129 ms | 607 |
| user preset that only turns Level Match on, from Output Gain −3 dB | −6.80 | **+0.72**, 128 ms | 613 |
| hand engage from Output Gain 0 / +6 dB | −6.80 | none (glides down from 0 dB) | 607 |
| Undo of Apply at a positive match (Drive 0, Width 0) | +0.69 | none; a **−0.35 dB dip** below both | 288 |
| Apply; Redo; hand disengage; A/B either way; host reset inside the swap | — | none | 104–127 |

Independent reproductions: 44.1 kHz / 512 with a different signal and metric, +5.14 / +6.55 dB at
Drive 8 / 10 (+3.87 / +4.70 at three times the level — within 0.13 dB of the earlier +4.0 / +4.8);
96 kHz / 64 +5.26 dB; a third harness +4.10–4.62 dB across four rate / block / signal
configurations. The peak time (123–130 ms with a 100 ms window) is robust; the size scales with the
match gain (0.58–0.69 × |match|).

### I3. Mechanism

Trace of the Drive 8 Undo, per block (end-of-block values):

| ms after Undo | state | published | `matchGainSmooth` current → target | `outGainSmooth` | applied |
|---|---|---|---|---|---|
| −5.3 | settled after Apply, Match off | −6.872 | 0.000 → 0.000 | −6.800 | −6.800 (Output Gain) |
| 0.0 / 5.3 | forced fade-out, dry fill latched at −6.80 | −6.871 | 0.000 → 0.000 | −6.800 | −6.800 |
| 10.7 | **bottom**; Match on | −6.871 | −0.214 → −6.871 | −3.000 (snapped) | −0.214 |
| 37.3 | end of fade-in | −6.869 | −1.214 → −6.869 | −3.000 | −1.214 (5.7 dB hot) |
| 128 / 200 / 400 | | −6.86 | −3.80 / −5.11 / −6.50 | | |

1. While Level Match is off, the smoother's target is 1.0 (`AnamorphEngine.cpp:833-835`, `:1736`),
   so after Apply it ramps to 0 dB and parks there. The matcher keeps measuring (`loudness.process`
   is ungated).
2. Undo is a forced duck. At the bottom `procChanged` is false (`autoGainMatch` is not in
   `processingDiffers`), so nothing re-arms: published, displayed and predicted gains and the
   integrators are unchanged. `updateDerived()` sets the target to the published −6.87 dB;
   `snapSmoothers()` lands every other smoother but not this one (`:724`); an Undo carries no A/B
   injection; the input is continuous, so the silence→audio snap (`:1770-1771`) does not fire.
3. The output stage switches its gain from Output Gain (−6.80) to the smoother (0 dB) — a step of
   the match gain's size, which the fade-in reveals.
4. The smoother's 120 ms linear ramp is restarted every block, because the published value moves
   slightly each block and `SmoothedValue::setTargetValue` restarts on any change. It behaves as a
   one-pole with τ ≈ 120 ms, so the excess takes ~0.6 s to decay rather than 120 ms.

The measurement is not involved: every analysis field is continuous across the Undo, and a
diagnostic that lands the smoother at every duck bottom removes all four rows above while leaving
the published value identical. What lands it today: the A/B injection (`:1161-1166`), the
silence→audio edge, and a host reset, which sets `prevInputSilent` so the next audible block snaps
(measured +0.19 dB, no swell).

### I4. The existing contract

| | code (this head) | Accepted ADR / owner ruling | implementation detail | unspecified |
|---|---|---|---|---|
| Apply | ordinary duck; Output Gain := published, Level Match off, one undo step | ADR-0007 "Apply locks the measured gain"; ADR-0008 (one step, Redo restores it) | the duck, the 20 ms Output Gain ramp | how Apply should sound |
| Undo of Apply / any forced swap that turns Match on | fade-in starts from unity, glides to the published value | none decides it; ADR-0004 D1 "snap smoothers there so nothing pops mid-fade" (exclusion unnamed); ADR-0008 "`requestDuck()` masks the level jump" | the `:724` carve-out (origin before this clone's history) | the applied gain an engaging forced fade-in should carry |
| hand engage | ordinary duck, same unity start | ADR-0007: toggling must **not re-measure** (`:124-125`) | CHANGELOG [0.8.9] "always duck- and glide-smoothed, never a click"; `:1816` "toggling Match … is seamless" | the level an engage starts from |
| A/B | injection lands published and smoother | ADR-0007 notes (injection value, dimMode row) | the injection | (F13(2), KI-030) |
| continuous-only forced swap, Match on in both | carried, glides with the measure | ADR-0007 note 2026-09-24: behaves like the live edit (F13(1a), preserved) | | |
| host reset / prepare | reset: published kept, edge snap lands the smoother; prepare: full flush | ADR-0007 notes 2026-09-21/22 and the R9 approval (published survives a reset) | | the applied gain across a reset |
| silence→audio edge | snaps the applied gain | ADR-0007 Decision — the only Accepted sentence about the applied gain | the −60 dBFS gate | whether other silent points (a duck bottom) should snap |

**No owner ruling exists** on how an engage should sound, on the Undo-of-Apply swell, or on landing
the smoother at a bottom (docs, worklogs, CHANGELOG and `git log --all` searched; the repository is a
shallow clone, so the carve-out's origin and the text of feedback #1 / #16 / #23 cannot be
recovered). ADR-0007's note of 2026-09-24 reserves the question for the owner.

### I5. Candidate behaviours, measured

Each is a scratch engine variant built against this head's objects; none is in the repository. For
every variant both suites pass (524 / 0, 4809 / 0) with no changed line other than thread-timing
counters, and the published value, the displayed value and the analysis are identical to this head
in every scenario (none adds a `reset`, `softReset` or `setDisplayedGainDb`). No variant needs a
parameter, schema, signal-order, latency or threading change. Excess in dB (48 kHz / 256, as §I2).

| | O1 keep | O2 snap in `snapSmoothers()` | O3 land after the measure, forced bottom | O4 land after the measure, any Match-on bottom | **O4g** O4 when only Level Match / Output Gain / Output Balance change | O5 start from the heard gain | O6 track the matcher while off |
|---|---|---|---|---|---|---|---|
| Undo of Apply D4 / D8 / D10 | +2.32 / +4.45 / +5.67 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 |
| hand re-engage after Apply | +4.44 | +4.44 | +4.44 | 0 | 0 | 0 | 0 |
| hand engage from −12 dB | +4.51 | +4.51 | +4.51 | 0 | 0 | 0 (monotonic rise, 494 ms) | 0 |
| hand engage from 0 / +6 dB, settle | 607 ms glide down | same | same | lands, 127 ms | lands, 127 ms | 607 / 730 ms (starts at +6: up to 10.2 dB above the destination) | lands, 127 ms |
| preset that only turns Match on | +0.72 | 0 | 0 | 0 | 0 | 0 (glide, 524 ms) | 0 |
| Undo 60 ms after Apply | +2.48 at 100 ms | — | — | 0 | 0 | 0 | — |
| re-engage 80 ms after a disengage (OG 0) | +3.00 at 100 ms | — | — | 0 | 0 | +4.18 | — |
| Undo of Apply at a positive match (+0.69 dB): dip | −0.35 | — | — | +0.02 (none) | +0.02 (none) | — | — |
| **engage + sound change** (user preset, Match on, Drive 10→0), run − fresh at 100 ms / min | −3.59 / −7.72 at 377 ms | not measured (as O4 by reading) | not measured | **−8.90 / −8.90 at 70 ms** | −3.59 / −7.72 (= O1) | −5.69 / −8.07 | not measured |
| same + algorithm change, min | −1.06 | — | — | **−5.53** | −1.06 (= O1) | −2.35 | — |
| engage + Drive 0→10, max | +7.43 | — | — | +5.05 | +7.43 (= O1) | +5.33 | — |
| continuous-only Drive 0→10, Match on both | +6.73 | +6.77 | **+4.55** | +6.73 | +6.73 | +6.73 | +6.73 |
| A/B (both directions, injection) | — | identical | identical | identical | identical | identical | identical |
| host reset inside the fade-in, settle | 120 ms | 126 | 126 | 126 | 126 | 126 | 126 |
| re-prepare | +2.46 (measurement) | identical | identical | identical | identical | identical | identical |
| cost | — | none | none | none | a "same sound" comparison with a float tolerance (a preset round trip moved `chorusRate` by one ulp) | none | ~3 % more engine time while Match is off (fast path lost); part-way after a large match move |
| ADR-0007 note's options | "keep" | listed | listed | listed ("wider change to the engage") | not listed (O4 narrowed) | not listed | not listed |

The rows from "Undo 60 ms after Apply" to "engage + Drive 0→10" (except the positive-match row) come
from the verifier's independent harness: run minus a fresh destination instance, 20 ms least-squares
gain, same programme; "—" is not measured.

**O7, re-measure on engage** (a full `loudness.reset()` at a Match-on bottom, then a snap): Undo of
Apply +2.80 / +4.97 / +6.16 dB at Drive 4 / 8 / 10 (worse than O1) with the snap before the
measure, +1.41 / +2.29 / +2.66 dB with 2.1–2.2 s to settle with it after — a smaller swell traded
for a multi-second one — and it contradicts ADR-0007 (`:101` re-armed in one place; `:124-125` toggling
must not re-measure). Ruled out: a hard stop, and no better.

**Engage + sound change is a different problem.** While Level Match is off the published value
describes the sound being played; a forced swap that turns Match on AND changes the sound therefore
lands (O4) on a stale value, the same staleness a Match-on-both swap shows (continuous-only Drive
10→0 with Match on in both: −8.72 dB against the fresh instance, unchanged by every option). O1's unity start is accidentally
close in one direction (X1) and far in the other (X2). Neither is right; the right answer is a
measurement question (F13(2), KI-030), not this one.

### I6. Owner decision record — F13(1b)

- **Observed.** Turning Level Match on without an A/B switch starts the applied gain at unity instead
  of at the matched gain, so the output steps by the match gain at the silent bottom and returns over
  ~0.6 s. When the level before and the matched level are both below unity — Undo of Apply, a hand
  re-engage after Apply, an engage from a low Output Gain — that is a swell above both: +2.3 / +4.5 /
  +5.7 dB at Drive 4 / 8 / 10 on the measured programme, peaking ~130 ms after the action. When both
  are above unity it is a dip (−0.35 dB at +0.69 dB). An everyday trigger; no test covers it.
- **Contract.** Only the silence→audio snap is decided for the applied gain (ADR-0007 Decision).
  ADR-0004 D1 and `snapSmoothers()`'s comment ("a big level change never swells (#1)") point one way
  but do not name this smoother; the CHANGELOG and a code comment call the engage glide-smoothed and
  seamless. ADR-0007's note of 2026-09-24 reserves the question: "each is a change to this ADR, not a
  defect of it". No owner ruling exists.
- **Question.** At a switch's silent bottom that turns Level Match on, what applied gain should the
  fade-in carry — unity (today), the matcher's value, or the gain that was playing? And should the
  answer hold when the same switch also changes the sound?
- **Allowed choices.** O1 keep and document; O2; O3; O4; O4g; O5; O6 (§I5). Not allowed: O7 (conflicts
  with ADR-0007, and is worse).
- **Recommendation: O4g**, with **O4** as the simpler alternative and **O2** as the narrowest.
  - O4g removes every measured instance: Undo of Apply, the hand re-engage, an engage from a low
    Output Gain, a preset or undo that only turns Level Match on, and the mirror dip.
  - It changes nothing where Level Match is already on (so F13(1a)'s "forced tracks the live edit"
    stands), nothing on A/B, nothing in the measurement, no suite line and no CPU.
  - It leaves an engage that also changes the sound exactly as today. That case needs a measurement
    answer (F13(2)), and O4 makes it worse by up to 4.5 dB (X1b) while improving the opposite
    direction.
  - Its one audible change beyond the fix: an engage from a louder Output Gain lands at the silent
    bottom instead of gliding down over ~0.6 s, the same treatment every other control gets at a
    forced bottom.
  - Its cost is a "same sound except output" comparison that must stay complete as parameters are
    added and must tolerate a preset round trip. A defaulted comparison on `EngineParameters` beside
    `processingDiffers`, plus a test that fails if a field is missed, would carry it.
  - O4 needs no gate and is the ADR note's own "wider" option.
  - O2 is one line and fixes only the forced routes (Undo of Apply, preset); the hand engages keep
    their swell.
  - O1 is defensible only as a documented limitation (KI-031).
- **What changes under O4g (or O4).**
  - Code: `AnamorphEngine.cpp` — a block-local flag set at a Match-on duck bottom (cleared by an
    injection), a landing right after the level-match stage's `setTargetValue`, and for O4g the
    comparison. Comments `:78`, `:155-156`, `:703-705`, `:724`, `:1814-1816`.
  - Tests: a DSP test through the engine (forced and ordinary engage from Output Gain = match; a
    sound-changing engage and an A/B injection unchanged) and a State test through the processor
    (Undo of Apply and the hand re-engage within a stated bound of both endpoints over 0–0.6 s).
  - Docs: an ADR-0007 owner-ruling note and an ADR-0004 note naming the smoother; CHANGELOG
    `[Unreleased]` Fixed; KI-031 resolved; `TESTING.md`; `DOCUMENTATION_COVERAGE.md`;
    `PERFORMANCE_BUDGET.md:50-53` ("always duck+glide smoothed").
  - Hard-stop classes: none mechanical. It is an ADR-0007 amendment, so it needs the owner.
- **What changes under O1.** Correct the comments that are false today (`:703-705`, `:1814-1816`),
  add a test that pins the behaviour, and note the exclusion in ADR-0004 D1. KI-031 stays.

### I7. Recorded, not changed

- ADR-0007 note 2026-09-24 corrected in place (same PR): the smoother's lag at a Match-on-both bottom
  is 0.024–0.063 dB (0.94 dB right after a large live Drive move), not "within 0.003 dB"; the swell's
  size scales with the match gain; the route list and the measured options (this section).
- Drift reported, not edited (pre-existing): `AnamorphEngine.cpp:703-705` ("a big level change never
  swells") and `:1814-1816` ("toggling Match … is seamless") are false for an engage; ADR-0007:101
  "re-armed in exactly one place" is scoped to the switch (the host reset `softReset`s and the
  self-heal `reset`s); ADR-0035 "every other control is already snapped" and ADR-0004 D1 do not name
  the match smoother.
- The per-block ramp restart (§I3 item 4) lengthens every Level-Match glide to ~0.6 s; a separate
  lever, not decided here.
- The measurement family is unchanged by every option (A/B continuous-only +1.59 dB, Drive 0↔10
  +6.73 / −8.72 dB, re-prepare +2.46 dB): F13(1a), F13(2) and §F as already recorded.
- Scope: Linux x86-64, GCC; stationary noise and one programme-like signal; 44.1 / 48 / 96 kHz,
  64–2048-sample blocks for the headline rows only; Haas only for the matrix (one Velvet row). No
  DAW, no music.

## J. F13(1b) implemented: the owner's ruling (O4g), the derived predicate, and its tests

Round after `e9deabe`, same branch (PR #156). The owner ruled on §I6: *"Use O4g as the working
direction for this round"*; *"Derive it from the actual engine/state graph and signal-path
dependencies."*; *"The predicate must tolerate harmless floating-point representation differences"*;
*"Preserve current behavior for sound-changing Level Match engages until F13(2) is explicitly
resolved."* (the same quotations as ADR-0007's review-gate row). §I is not edited; two of its plan
lines are superseded here: the "same sound except output" comparison (§I6) is replaced by the derived predicate below, and
"CHANGELOG `[Unreleased]`" is not available (no tag exists; CHANGELOG_POLICY puts unreleased work in the
dated `[0.9.9]` entry).

### J1. The invariant

At a switch's silent bottom that turns Level Match on, the applied gain `matchGainSmooth` is landed —
current and target — on the value the matcher publishes right after that block's `loudness.process`
(Case A) if and only if:

1. `! measurementInputsDiffer (p, pendingP)` — nothing the measurement reads differs between the state
   heard before the switch and the state adopted at the bottom;
2. `! duckMeasDirty` — no such change was made live during the switch's own fade-out (an ORDINARY duck
   applies continuous controls at once through `copyContinuous`, so by the bottom `p` already carries
   them; measured on the naive version: a Drive change riding the engage left a 4.54 dB error);
3. `! procChanged` — the bottom does not re-arm the measure (a re-armed measure is moving: probe R3
   measured up to 0.277 dB at the bottom and 0.512 dB over 3 s off-Haas / Multiband-off);
4. no A/B injection is consumed in that block (the slot's gain keeps priority);
5. the reading is finite (ADR-0009).

Anything else (Case B) is the pre-ruling behaviour: the smoother starts at unity and glides.

### J2. Deriving `measurementInputsDiffer`

The measurement reads three things: the wet at the tap (L/R after Mono Maker), the dry reference
`loudnessRefScratch` (A(dry), or the delay-aligned clean dry under the H4 gate), and the predict's
inputs (`setDriveDb`, `setMix`). Two independent derivations, then a reconciliation:

- **Code reading**, every reader of every field traced to the tap; checked with a harness comparing the
  whole matcher state bitwise after one field changed (84 rows, 0 disagreements).
- **Measurement**, independently: pairs of engines differing in one field, compared bitwise on the
  matcher's biquads, integrators, published and predicted values and on the exact arrays handed to
  `LoudnessMatch::process` — 240 configurations (4 algorithms × Multiband × Mono Maker × Mix ×
  oversampling × band count, Level Match off, on and switching), 23,744 runs, 288/288 A/A controls
  identical.
- **Reconciliation**: 30 of 36 fields agreed outright; the rest were settled by reading and probes:
  - `driveDb`, `mix` are compared **exactly**: the predict's rise test fires on any rise — a +1 ulp
    Drive rise with bit-identical audio moved the published value 1.08 dB in one block (R4).
  - `haasSide` (off Haas) and `mbBands` (Multiband off) never reach the tap but make `processingDiffers`
    re-arm; the predicate guards them and condition 3 covers the re-arm.
  - Ordinary ducks: condition 2 (above).
  - A Multiband on/off crossfade still running when an ordinary engage opens (≤ 12 ms) reaches the tap;
    a latch for it was prototyped and **not adopted**: without it the worst stale landing measured
    1.65e-4 dB over 3 s (32 probe rows), below the accepted H4 difference.
- **Name traps confirmed**: `solo` (M/S solo, input conditioning) moves the published value by
  0.17–0.24 dB and is compared; `mbSolo` (Band Solo, post-everything) moves nothing and is not.

Result — compared exactly: `channelMode`, `monoSum`, `swapLR`, `polarityL/R`, `msMode`, `solo`,
`algorithm`, `mbEnable`, `monoMakerEnable`, `oversample`, `driveDb`, `mix`; to a relative 1e-5:
`inputBalance`, `algoAmount`, `width`; guarded: `haasDelayMs`, `haasSide` (Haas either side),
`velvetDensity` (Velvet), `chorusRate`, `chorusDepth` (Chorus only — not Dimension D), `dimMode`
(Dimension D), `mbBands`, `mbWidthLow` (Multiband either side), `mbFreqLow`/`mbWidthMid`,
`mbFreqMid`/`mbWidthHiMid`, `mbFreqHigh`/`mbWidthHigh` (and ≥ 2 / 3 / 4 bands), `monoMakerFreq` (Mono
Maker either side); not compared: `outputGainDb`, `outputBalance`, `mbSolo`, `bypass`, `autoGainMatch`.

**Tolerance.** Round trips measured through the processor (3,008 random trials plus curated values,
2,000 × 25-generation drift chains): only the three log-mapped crossovers and `monoMakerFreq` moved
through preset save / load (≤ 14 ulp, relative ≤ 1.6e-6, settling by the second generation), and
`chorusRate` at its default moved 1 ulp on a fresh instance (JUCE's unsnapped initial raw value).
`driveDb`, `mix`, `width` and every other float came back bit-identical through every path. 1e-5 is
six times the largest drift and under a tenth of any snapped parameter's half grid step; the gridless
crossovers' worst hidden change would move the published value by ≈ 4e-6 dB.

**Accepted, bounded:** the H4 reference switch (`bypass` and the Level Match switch itself reach the
dry reference while Level Match is off, Multiband on, Mix exactly 1): ≤ 0.0064 dB at the bottom; the
forced bottom's module restarts: ≤ 0.009 dB over 3 s.

### J3. Implementation (`f20212d`)

`src/dsp/AnamorphEngine.cpp`: `measurementInputsDiffer` beside `processingDiffers`; `duckMeasDirty`
(cleared at every fresh fade-out entry and by `reset()`, set at the ordinary-duck entry and by every
live copy during an ordinary duck); the decision at the bottom; the two injection consumers clear it;
the landing after the level-match stage's `setTargetValue`. No header layout concern beyond one bool
member; audio-thread only; no allocation, lock or wait (Test 66 arms the allocation guard around every
landing). `scripts/check-state-coverage.py` gains `MEASUREMENT_INPUTS` / `MEASUREMENT_INPUTS_EXCLUDED`,
total over the struct, checked for form and exact guard text term by term, with the predict's inputs
derived from the code; its self-test re-creates each defect on the real source (110 cases).

Measured on the implementation (the §I shared matrix, 48 kHz / 256, pink noise):

| route | before | after |
|---|---|---|
| Undo of Apply, Drive 4 / 8 / 10 | +2.32 / +4.45 / +5.67 dB, settle 523–647 ms | 0.00 dB, settle 127 ms |
| hand re-engage after Apply; engage from Output Gain −12 dB | +4.44 / +4.51 | 0.00 / 0.00 |
| user preset that only turns Level Match on | +0.72 | 0.00 |
| Undo of Apply at a positive match (+0.69 dB) | −0.35 dip | +0.02 (none) |
| engage from a louder Output Gain (0 / +6 dB) | 607 ms glide down | lands, 127 ms |
| A/B, Apply, Redo, disengage, re-prepare, Level Match on in both states | — | identical to before (byte-identical rows) |
| preset turning Level Match on with Drive 10→0 / + algorithm / Drive 0→10 (Case B) | −7.72 / −1.06 / +7.43 | identical to before |

The reconciler's probes (G, R1–R4, S) against the implementation are line-identical to its prototype
except the 32 R1 rows the omitted crossfade latch lands on (≤ 1.65e-4 dB, above). Both suites unchanged
(timing lines aside) before the new tests were added.

### J4. The F13(2) boundary, measured

A sound change made shortly **before** a gain-only engage is outside the switch, so the engage lands on a
published value still converging on the new sound (probe S, ordinary engage k blocks after the change,
mean |applied − the destination's settled value| over the first 500 ms):

| before the engage | 16 ms | 500 ms | 1 s | 2 s |
|---|---|---|---|---|
| Drive 12 → 6 live (quieter) | 4.46 dB | 3.33 | 2.22 | 0.83 |
| forced Drive 12 → 6 + Width (quieter) | 5.72 | 3.91 | 2.50 | 0.97 |
| forced Drive 6 → 12 + Width (louder) | 4.62 | 2.63 | 1.62 | 0.56 |

Before the ruling the same engages glided from unity (same probe, same metric, at 16 ms): 2.73 dB (live,
quieter) and 3.37 dB (forced, quieter) — closer than landing — and 6.25 dB (forced, louder) — further.
Neither is right; the lag is the measure's own convergence, which is the F13(2) question (§K). No
convergence guard was added: that would be an F13(2) rule chosen silently.

## K. F13(2): the published value across sound changes — evidence, root cause, owner-decision record

Round after `f155978`, same branch (PR #156). The owner asked for F13(2) after F13(1b): reproduce the
measurement family through the plugin, decide which transitions are measurement-invalid and why, and
either implement what the Accepted text already decides or stop at the evidence and decision-record
boundary. **No behaviour changed in this section.** Every candidate policy below contradicts Accepted
ADR-0007 text (§K5), so each one is a hard stop (`AI_AGENT_POLICY.md:45`) and is recorded, not built.

### K1. Method

Three measurements on the engine of `f20212d` (its code unchanged since), through
`AnamorphAudioProcessor` exactly as a host and the editor drive it, then two adversarial verifiers
(one re-ran the measurements at other seeds and block sizes with its own probe; one re-ran every
variant's suites and harness rows and re-checked every quotation):

- **Shared harness** (the §E / §I harness, extended by one row per transition class): K-weighted
  100 ms trailing windows (`pkX` / `dipX` against the two counterfactual trajectories) plus a 20 ms
  least-squares gain of the run against a fresh instance prepared at the destination state and fed the
  identical input from sample 0 ("run − fresh", normalised over 2.5–3.0 s after the event). Pink noise,
  48 kHz / 256. The four recorded rows are byte-identical to the pre-O4g table; only the O4g engage
  rows differ from it.
- **Independent harness**, written without reading the other harnesses: band-limited noise with a slow
  ±3 dB level modulation, 48 kHz / 256 and 44.1 kHz / 512, its own 20 ms least-squares metric.
- **Root-cause probe**: counterfactual surgery on the running matcher at the event — S1 replaces only the
  ANALYSIS (the four K-weighting biquads and both integrators) with the fresh destination's, S2 only the
  RESULT (`displayedGainDb`, `prevPredictedGainDb`, `matchGainDb`), S3 both; an `m` suffix also sets the
  applied-gain smoother; S4 skips a flush. The half whose replacement removes the error owns it.

Scope: Linux x86-64, GCC; stationary noise programmes only (no music, transients or silence gaps); Haas
for the matrix, Chorus / Velvet for the discrete rows (Chorus rows placed so the fresh instance's LFO
phase matches, since the bottom restarts the LFO).

### K2. The recorded figures, reproduced

| transition (Level Match on in both states) | recorded | shared harness | independent 48 k / 256 | independent 44.1 k / 512 |
|---|---|---|---|---|
| A/B, slots differ only in Drive (2 → 8) | +1.59 dB | +1.59 (`pkX`) | +1.65 at 490 ms | +1.62 |
| the same, 8 → 2 | −1.84 (`dipX`) | −1.84 | −2.02 | −2.08 |
| Undo, Drive 0 → 10 | +6.73 | +6.73 | +7.65 at 50 ms | +7.64 |
| Undo, Drive 10 → 0 | −7.89 / −8.72 | −7.89 | −8.35 | −8.38 |
| re-prepare, same rate and block | +2.46 | +2.46 | +2.48 | +2.61 |

The 20 ms windows read the Drive rows higher and earlier than the 100 ms ones (the shared harness's own
50 ms figure for Undo 0 → 10 is +7.32). Same sign, size and cause everywhere. An adversarial re-run at
two other seeds and at 128- and 512-sample blocks moved the shared-harness figures by at most 0.2 dB
(+1.55 to +1.59, +6.65 to +6.73, −7.87 to −7.91, +2.26 to +2.46), and a third probe on its own input
and metric agreed in sign and cause.

Every class, run − fresh at 100 ms / 500 ms / 1 s / 2 s after the event (shared harness):

| class | transition | 100 ms | 500 ms | 1 s | 2 s |
|---|---|---|---|---|---|
| (a) live edit | Drive 0 → 10 / 10 → 0 | +6.89 / −8.10 | +3.54 / −6.70 | +1.89 / −3.63 | +0.46 / −0.95 |
| (b) forced, continuous-only | user preset 0 → 10 / 10 → 0 | +6.91 / −8.10 | +3.52 / −6.72 | +1.86 / −3.64 | +0.46 / −0.95 |
| | Undo | +6.61 / −7.93 | +3.49 / −6.74 | +1.86 / −3.64 | +0.46 / −0.98 |
| | Redo | +6.58 / −7.89 | +3.60 / −6.62 | +1.95 / −3.57 | +0.48 / −0.96 |
| (c) A/B, continuous-only | Drive 2 → 8 / 8 → 2 | +0.67 / −0.42 | +1.59 / −1.84 | +1.12 / −1.48 | +0.29 / −0.41 |
| (d) discrete | Haas → Chorus live / Undo | −1.77 / −1.80 | −1.10 / −1.24 | −0.60 / −0.67 | −0.13 / −0.15 |
| (e) A/B, discrete | Haas ↔ Chorus | ≤ +0.07 | ≤ +0.05 | ≤ +0.02 | 0.00 |
| (f) re-prepare | same / block 256 → 512 / 48 → 44.1 kHz | +2.35 / +2.29 / +2.30 | +1.29 / +1.29 / +1.31 | +0.67 / +0.68 / +0.68 | +0.15 / +0.15 / +0.14 |
| (g) host reset | settled (control without reset) | +0.23 (+0.22) | +0.15 (+0.13) | +0.06 (+0.07) | +0.01 (+0.02) |
| | 200 ms after a live Drive 0 → 10 (control) | +3.77 (+5.19) | +1.27 (+2.70) | +0.64 (+1.58) | +0.14 (+0.38) |
| (h) Case B engage | preset: Level Match on + Drive 0 → 10 / 10 → 0 | +6.57 / −2.84 | +3.49 / −6.52 | +1.86 / −3.64 | +0.46 / −0.95 |
| (i) Case A engage on a converging value | 16 ms / 500 ms after a live Drive 12 → 6 | −4.14 / −3.50 | −3.67 / −2.52 | −2.30 / −1.53 | −0.60 / −0.36 |

The three forced routes track the live edit within 0.35 dB at every checkpoint: **(b) is (a)**. A host
reset reduces the error of a switch it interrupts rather than causing one (g).

### K3. Root cause, by surgery

| class | category | what owns the error (surgery, 20 ms run − fresh) |
|---|---|---|
| (a) live edit | stale but consistent | both halves describe the old Drive and re-converge with the measure's constants (0.4 s integrators; 60 ms glide above a 2 dB gap, 0.9 s below); the applied-gain smoother (a 120 ms ramp restarted every block, so an exponential of τ ≈ 120 ms) dominates the first ~300 ms. Drive 0 → 10: none +7.99, S3 (both halves) +7.23 at 47 ms then +0.26 at 500 ms, S3m +0.03 |
| (b) forced, continuous-only | stale but consistent — as (a) | `procChanged` false, no re-arm; no injection; no Case-A landing (Level Match is on in both states, so the switch is not an engage); `snapSmoothers()` leaves the applied gain. Undo 0 → 10: none +7.45, S3 +6.59, S3m 0.00; 10 → 0: none −8.74, S3 −5.25, S3m 0.00 |
| (c) A/B, continuous-only | **analysis inconsistent with the result** | the injected result is the slot's remembered value (−6.70 dB against a fresh −6.88; the 0.18 dB is its provenance — the slot was left 3 s after an edit, before the measure had converged) and is landed; the integrators still hold the source slot, so the first measure targets it (−2.80) and the fast glide drags the value away (KI-030). S1 (analysis only) +1.82 → +0.39 (+0.16 before the bottom); S2 (result only) +1.84 — no effect |
| (d) discrete | result stale, analysis re-armed — the mirror of (c), benign | `softReset` re-arms: the analysis is right within a block or two (S1 no effect) and pulls the carried result the right way at the 0.9 s glide. S2 cuts the time above 0.5 dB from 1,365 to 165 ms |
| (e) A/B, discrete | not stale | re-arm, injection and landing in the same bottom: ≤ 0.1 dB in every mode. The injected value is only as good as the slot's convergence when it was left |
| (f) re-prepare, same rate | **a valid result flushed** | `loudness.prepare` and `reset(everything)` zero a result that is still exact: skipping the flush (S4) gives 0.00 dB, restoring only the result +0.03; the result owns ~90 % of the error, the analysis re-arm ~10 % (S1 +2.73 → +2.46). The next block's predict floor (−4.07 against a true −6.80) and the edge snap start the glide |
| (f) re-prepare, new rate | coefficients invalid in kind; the error is still the flush | the old 48 kHz coefficients kept (S4) and the new ones with the old states (S4r) both give −0.09 dB; the pre-prepare result was 0.09 dB from the 44.1 kHz destination |
| (g) host reset, settled | not stale | `softReset`, result kept, edge snap: ≤ 0.03 dB |
| (g) host reset, mid-swap | result stale, analysis re-armed | the reset completes the swap and then re-arms unconditionally, so a continuous-only swap IS re-armed here; the edge snap lands the stale result at once: Undo 0 → 10 + reset +3.05 (without the reset +7.45), S1 no effect, S2 +0.01. A pending A/B injection is consumed after the re-arm — consistent (+0.18, without the reset +1.82) |
| (h) Case B | stale but consistent | the matcher runs while Level Match is off, so both halves describe the sound before the switch; the applied gain starts at unity. Drive 10 → 0: none −7.74, S3 −0.55; 0 → 10: none +7.38, S3 +6.46 (unity starts 8.4 dB off) |
| (i) Case A on a converging value | stale but consistent | the landing copies the lagging value of (a) into the applied gain at full size; with both halves replaced before the bottom it is exact (0.00 dB) |
| (j) NaN self-heal | analysis invalid in kind; a valid result flushed | restoring the pre-NaN matcher (S4) gives +0.01 dB; the flush costs +1.57 dB peak and 1.4 s |

**The Drive 0 → 10 / 10 → 0 asymmetry** has five parts. (1) The predict only lowers: on 0 → 10 it floors the
published value from +0.13 to −5.02 dB in the bottom block; on 10 → 0 it does nothing (removing it makes
the rise worse, +8.46 against +7.45, and leaves the fall unchanged). (2) On the rise the stale target is
more than 2 dB away, so the fast glide undoes most of that pre-duck within ~80 ms. (3) The target is the
dB ratio of two linear-energy integrators: it converges fast on a rise and slowly on a fall, where the
old, larger energy dominates (remaining target error at 101 / 400 / 997 ms: 5.15 / 1.73 / 0.33 dB rising,
8.10 / 5.56 / 1.97 dB falling). (4) Inside the 2 dB band the published value moves on the 0.9 s
coefficient, so on the fall it sits still until ~290 ms. (5) The applied gain is not landed at the forced
bottom. The time constants alone are nearly symmetric (S1m +1.49 / −1.83 dB); the asymmetry is the stale
analysis (S2m +3.82 / −6.15) and the rise-only predict.

### K4. Which transitions are measurement-invalid, and why

- **Invalid in kind** — the state cannot describe the new situation: a **sample-rate change** (the
  K-weighting coefficients, the 0.4 s window and the glide constants are functions of the rate) and a
  **non-finite sample** (a NaN in the integrators). Only the ANALYSIS is invalid in either case: the stale
  coefficients cost < 0.01 dB, and the result survives both (0.09 dB and 0.01 dB from the fresh
  destination). Both flushes are decided: re-prepare by the review gate's scope sentence *"a re-prepare
  still resets all of it"* (ADR-0007:182), the self-heal by ADR-0009 (whose own owner question is the
  gain it discards, ADR-0009:82-83).
- **Analysis inconsistent with the result** — exactly one transition: the **A/B injection between
  slots that differ only in continuous controls** (c). The result is the destination's (within its own
  provenance: the value published when the slot was left); the carried analysis is the source slot's
  and pulls it away. This is the only transition where the matcher's two halves disagree in the wrong
  direction, and the only one where re-arming the analysis removes the error (all but the slot's own
  provenance).
- **Stale but consistent** — the measure's accepted lag (Context "A pure measured loudness lags",
  ADR-0007:6-7; the Decision's measure + floor-only predict): **live continuous edits** (a), **forced
  continuous-only swaps** — preset, undo, redo (b), which are the same thing — **Case B engages** (h),
  and **Case A engages on a value still converging** (i).
- **Result stale, analysis re-armed** — a **discrete change** (d) and a **host reset mid-swap** (g): the
  mirror of (c), but benign, because the correct half is the analysis and it pulls the stale result the
  right way.
- **Not stale** — an A/B with a discrete difference (e), a settled host reset (g), and every change
  after the tap (Output Gain, Output Balance, Bypass, Band Solo, the Level Match switch itself: the
  2026-09-24 Amendment's derived set).

Distinguishing the routes: continuous and discrete differ only in whether the bottom re-arms
(`processingDiffers`); preset, undo and redo are the live edit; A/B differs from them only by the
injection, which is right, and by leaving the analysis alone, which is the defect; a re-prepare and the
self-heal throw away a result that was still valid; a host reset re-arms whatever is in flight.

### K5. Candidate policies, measured (scratch engine variants; none is in the repository)

The re-arm variants (P1–P1c) pass both suites (553 / 0, 4,923 / 0): no existing test discriminates
between them. P2, P3 and P4 each fail exactly the checks that pin the behaviour they change. CF-V2 and
CF-V3 are the independent harness's diagnostics, whose suites were not run (P4 is CF-V3 built as a
candidate). None needs a parameter, schema, threading, signal-order or latency change; the allocation
guard stays at zero under each.

| | definition | effect (HEAD → variant) | Accepted text it contradicts |
|---|---|---|---|
| **P1** | at a forced bottom, also `softReset()` when `measurementInputsDiffer (p, pendingP)` | (c) +1.59 → +0.10 dB, settle 2,448 → 124 ms; (b) Undo 0 → 10 +6.73 → +6.18, 10 → 0 −7.89 → −5.92 — so the forced route **no longer tracks the live edit** (+7.31 / −8.92 unchanged live); the applied gain still glides (published −6.57 against applied −3.52 at 130 ms), so the rise barely improves; forced Case B changes | ADR-0007:101-104, :197-200, :214-215, :279-280, :330-333; the O4g ruling (Case B changes); arguably :124-125 (a preset that turns Level Match on while Drive changes is re-armed; the counter-reading is :122, "any real path change: thrown away") |
| **P1a** (B1) | `softReset()` at every A/B injection | (c) as P1; (b) unchanged; also re-arms an A/B between identical slots and one that changes only Level Match | as P1 (:101-104, :214-217, :279-280, :330); reverses the dimMode table's A/B row (:120); arguably :124-125 |
| **P1b** | P1a only when the slots' measurement inputs differ | (c) as P1; an A/B that also turns Level Match on +1.63 → +0.07; nothing else on the shared harness moves; identical-slot and Level-Match-only A/B are not re-armed; State test 31's remembered slot-B gain prints −1.04 → +2.08 dB (its checks pass; the same under P1–P1c) | :101-104, :214-217, :279-280, :330-333; A/B Case B changes (the O4g ruling); arguably :124-125 (an A/B that turns Level Match on while Drive changes is re-armed) |
| **P1c** | P1 at every bottom (with `duckMeasDirty`) | as P1, plus ordinary Case B engages; re-arms an ordinary duck whose edit returned before the bottom | as P1; arguably :124-125 |
| CF-V2 (O3) | land the applied gain after the measure at a Match-on-both forced bottom | Undo 0 → 10 +7.65 → +4.47 (§I5's O3, +4.55; a 2.6–2.9 dB cut on another programme); 10 → 0 unchanged (the published value itself is stale) | :197-200 (the forced route stops tracking the live edit) |
| CF-V3 / **P4** | keep the result across a re-prepare at an unchanged rate (a changed rate still flushes) | re-prepare `pkX` +2.46 → +0.04 dB, settle 2,215 → 0 ms (+2.48 → +0.055 on the independent harness); a block-size-only re-prepare the same; a rate change unchanged. **Fails State test 120** ("R6: ResetScope::everything still flushes the matcher, published gain included"). Not measured: a same-rate re-prepare no longer clears a Level Match displaced by an extreme finite burst (ADR-0009:44-46) | :39-40; the review gate's scope, :182 "a re-prepare still resets all of it" |
| **P2** | Case A lands only after 1.3 s (the 0.4 s window plus the 0.9 s glide) without a measurement-input change; otherwise it glides from unity (as built, the counter also advances through silence, where the measure holds rather than converges) | mean error over 500 ms after an engage 16 / 500 / 1,000 ms after a change: quieter routes better (live 4.46 → 2.73, forced 5.70 → 3.35 dB), the louder route worse (4.51 → 6.15 dB, peak +5.20 → +8.93) and the overshoot above both endpoints returns (`pkX` up to +0.90 dB); 2 s unchanged. **Fails Test 66** (3 checks: every lane's history holds a measurement-input change 0.2 s before its event) | :268-282 (Case A asks only about the switch), and :319-324 leaves it undecided |
| **P3** (O4) | Case B lands too | Level Match on + Drive 0 → 10 better (`pkX` +6.71 → +4.59, still above both endpoints), Drive 10 → 0 worse (`dipX` −6.88 → −8.09), + an algorithm change worse (−1.06 → −4.45). **Fails Test 66** (3) and **State test 130** (2) — the legs that pin Case B | the O4g ruling (*"Preserve current behavior for sound-changing Level Match engages until F13(2) is explicitly resolved."*) and :283-290 |

No re-arm touches (a), (d), (e), (f), (g), (i) or (j): a re-arm makes the published value converge faster
but does not move the applied gain, which dominates the first ~300 ms of (b) and (h).

### K6. Owner decision record — F13(2)

- **Decided by the current text** (the published value is not "invalid", it is the reading of an
  estimator that lags by design): live edits (a); forced continuous-only swaps behave like the live edit
  (ADR-0007:197-200, a note recording measured behaviour, with a change reserved by question 2 — which is
  why Test 66 leg (8) and State test 130 leg (8) pin it "pending F13(2)" while §E3 calls it "preserve");
  re-arm on a path change (:101-104, Test 58); re-arm plus injection on an A/B with a discrete difference;
  flush on re-prepare (:39-40, :182) and in the self-heal (ADR-0009); host reset re-arms and keeps the
  result (:165-167); the Case B interim and the Case A landing (the Amendment, O4g).
- **Excluded by the current text:** re-measuring on engage (:240-241, :344); re-arming on every forced
  duck (:246, Test 58); re-measuring on a Level Match or Bypass toggle (:124-125).
- **No owner ruling decides the open questions.** The review gate of 2026-09-22 (R9; its scope at
  :179-182, the owner's words at :187) decides two of the transitions — the host reset keeps the
  published gain, a re-prepare resets all of it — and O4g decides the engage predicate and binds Case B
  *"until F13(2) is explicitly resolved"* (:360). Every change in §K5 contradicts Accepted text, and
  `ADR_POLICY.md:27-28` says a reversed decision adds a new ADR; the precedents in this repository
  (ADR-0007's Amendment, ADR-0008's, ADR-0024's) amended in place. Which form applies is the owner's
  call.
- **Questions for the owner**, each with its measured options:
  1. **A/B injection** (question 2, KI-030): leave the analysis (today; +1.59 / −1.84 dB for ~2.5 s on a
     continuous-only switch); re-arm only when the slots' measurement inputs differ (P1b; the one
     targeted fix: +0.10 dB, nothing else moves); re-arm at every injection (P1a; also re-arms identical
     slots and reverses the dimMode row).
  2. **Forced swaps without an injection** (question 2's "whenever any continuous sound field
     differs?"): keep them identical to the live edit (today); re-arm them (P1; a forced route that no
     longer tracks the live edit, and still +6.18 dB on the rise because the applied gain glides); or
     land the applied gain at a Match-on-both forced bottom (O3 / CF-V2; +4.47 on the rise, nothing on the
     fall). Neither makes the live edit itself any better: that is the measure's design (Context :6-7).
  3. **Case B starting gain**: unity (today); land on the published value (P3 / O4 — better in one
     direction, worse in the other two measured); another seed (§I5's O5, the gain that was playing).
  4. **Case A convergence**: land whatever is published (today; −4.14 dB at 16 ms after a live Drive
     change, settling in ~2.6 s); glide from unity until the measure has settled (P2 — better when the
     change made the sound quieter, worse and with the swell back when it made it louder).
  5. **Re-prepare at an unchanged rate** (not raised before; the review gate's scope sentence decides it
     today): flush (today; +2.46 dB for ~2.2 s after every same-rate re-prepare); keep the result (P4;
     +0.04 dB, and State test 120's R6 check, which pins the flush, would change with it).
- **Recommendation**, for the owner to accept or reject: question 1 → **P1b**, the only change that
  fixes a defect of the matcher's own consistency (c) and moves nothing else measured; questions 2–4 →
  keep today's behaviour: every option measured trades one direction for another (P2, P3) or makes the
  forced route diverge from the live edit (P1, O3), and the lag it would address is the Accepted design
  (Context :6-7); question 5 → **P4** if the owner reads the gate's "a re-prepare still resets all of it"
  as describing the flush rather than requiring it, since the result is measured valid across a
  same-rate re-prepare; otherwise keep.

### K7. Recorded, not changed

- Pre-existing drift, reported: ADR-0007:101 "re-armed in exactly one place" holds for the switch path
  only (the host reset `softReset`s, `prepare()` and the self-heal `reset`); `DSP_ALGORITHMS.md:186-192`
  points the measure at the predict lines and does not describe `softReset` / `reset` / the injection;
  `API_REFERENCE.md:35` says a host reset lands a swap "exactly as its silent bottom would", which for
  Level Match is not exact: a pending A/B injection is adopted one block later, by the fallback
  consumer, and the gain lands through the edge snap rather than the Case-A landing (the row already
  says the reset clears the Level-Match analysis); `AnamorphEngine.cpp:222-223` and
  `AnamorphEngine.h:49-51` say a new block size invalidates the K-weighting coefficients (they depend on
  the rate only); ADR-0007's *Related code* anchors (:372-377) and its 2026-09-24 note's comment anchors
  (:253-254: two of its four anchors moved — the re-arm comment is now `AnamorphEngine.cpp:1179-1181`,
  the output-stage comment `:1887-1892`); the lower bound of the re-arm-at-injection figure differs
  between ADR-0007:217 (0.022 dB) and §E3 (0.013 dB) — this round's P1a / P1b rows measure it on
  another metric (+0.10 dB `pkX`; +0.004 dB on the independent harness's 20 ms metric when the slot had
  converged before it was left — otherwise the slot's own provenance error remains, +0.2 dB on the
  verifier's probe).
- Corrected in this round because this PR wrote them: ADR-0007's Case B "(unity)" and ADR-0004's /
  ADR-0035's "otherwise it glides" omitted the A/B engage, where the injected slot gain sets the
  smoother; the engine's output-stage comment and three `KNOWN_ISSUES.md` sentences said the same; §J
  paraphrased the owner.
- The per-block restart of the applied-gain ramp (§I3, §I7) is measured again here as an exponential of
  τ ≈ 120 ms that owns the first ~300 ms of every glide; still a separate lever, deferred.

## L. F13(2) decided and implemented: the A/B re-arm (P1b) and the same-rate re-prepare (P4)

Round after `daa6809`, same branch (PR #156). The owner authorized this round to take the §K6
decisions: *"You are explicitly authorized to make the owner decisions for the unresolved F13(2)
questions based on your own investigation, measured evidence, and the recommendations already
recorded in the worklog."* — with Q1 *"Adopt **P1b**"*, Q2–Q4 *"Keep the existing behavior."* /
*"Keep the current behavior."*, and Q5 *"Adopt **P4**"* limited by *"Do not automatically generalize
this to different-rate re-prepare."* The contract is ADR-0007's Amendment (F13(2)); this section is
the evidence behind it.

### L1. The evidence re-checked against the code before any edit

- **Q1.** The injection is consumed at the forced bottom (`AnamorphEngine.cpp`, inside
  `if (pendingForced)`) and, defensively, by a consumer that runs whenever no forced duck is pending
  — after a host reset that resolved the swap, the pending injection is adopted there on the next
  block, on an analysis the reset has already re-armed (§K3 (g)). A forced duck from Normal makes
  nothing live, so `measurementInputsDiffer (p, pendingP)` at the bottom is the whole change; an
  ordinary duck UPGRADED to forced mid-fade-out (the `forceDuck` branch of `setParameters`) keeps
  `duckMeasDirty` from its ordinary entry, and `p` already carries its live continuous edits — so the
  re-arm reads the same `duckMeasDirty || measurementInputsDiffer (p, pendingP)` the Case-A landing
  reads. §K5's scratch P1b read the comparison alone; the implementation adds `duckMeasDirty`, which
  changes nothing on the shared harness (no upgrade there) and closes the upgrade case.
- **Q5.** `AnamorphAudioProcessor::prepareToPlay` calls `primeParameters (e)`, `prepare`, then
  `setParameters (e)`; `primeParameters` overwrites `p`, so a restore between two prepares (the
  VST3/AU order: setState, then setActive) is invisible to `prepare()` unless `primeParameters`
  records it. §K5's scratch P4 kept the result on any same-rate re-prepare; the implementation keeps
  it only when `primeParameters` saw no measurement-input change (`primeMeasChanged`), so a restore
  that moved Drive still flushes and one that moved only Output Gain keeps. The K-weighting
  coefficients, the 0.4 s window and the glide constants depend on the rate alone
  (`LoudnessMatch::prepare`, `KWeighting::setSampleRate`); the glide coefficients re-key on each
  block's length (`coeffForN`), so a block size is no input. The engine's comments saying a block size
  invalidates the coefficients (`AnamorphEngine.h`, `ResetScope`; `reset()`'s scope comment) were
  wrong and are corrected.
- **The R9 sentence.** ADR-0007:182 (*"a re-prepare still resets all of it"*) is the ADR's statement of
  what the reset / thread-model approval covered at `c5f3d8f`; the owner's own words (:187) approve
  that change and say nothing about the re-prepare, and the note's reason for the flush (:39-40) is
  the sample rate. Read as descriptive, and narrowed by the Amendment.

### L2. Implementation

`src/dsp/AnamorphEngine.{h,cpp}`: `measChangedAtBottom` (block-local, one answer for the Case-A landing
and the re-arm); `if (measChangedAtBottom) loudness.softReset();` before `setDisplayedGainDb (inj)` at
both injection consumers; `primeMeasChanged` (set by `primeParameters`, read and cleared by
`prepare`); `keepMatch` in `prepare()` (prepared before, bit-identical rate, no primed
measurement-input change, a finite published value) → `loudness.prepare` skipped and `reset
(everything)` takes `softReset()` through `keepMatchResult`, a flag set only around `prepare()`'s own
`reset()` call. No parameter, schema, threading, signal-order or latency change; no allocation, lock
or wait; `prepare()` never runs concurrently with `process()` or `reset()`.

### L3. Scope of Q5: a new sample rate keeps the flush, by decision

The first draft of this section argued that a new rate invalidates the *result* because the band the
measurement integrates is `fs / 2`. Measured (§L4), that is not so: carried across a rate change, the
result lands 0.02–0.13 dB from the destination's converged value, and the measurement adds no rate
dependence of its own — the published value equals minus the per-rate K-weighted out / in ratio
within 0.012 dB at every rate. What differs across rates is the sound: at the default 12 ms Haas
delay 44.1 kHz reads a fractional delay (529.2 samples) that the Haas line's linear interpolation
low-passes (0.05 dB quieter on pink noise, 0.20 dB on white noise to 18 kHz; with Amount 0 every rate
agrees within 0.002 dB), and harmonics above the lower rate's Nyquist add at most 0.085 dB. So the
*analysis* is invalid in kind at a new rate (its coefficients are functions of the rate) and the
*result* is merely stale by the sound's own rate dependence — the category of a small live edit.

The flush is kept anyway: the authorization limits P4 to the same rate (*"Do not automatically
generalize this to different-rate re-prepare."*), the evidence covers Haas only on stationary noise
and multisine programmes (not Velvet, Chorus, Dimension D, Multiband, 4× / 8× oversampling, music or
transients), and a rate change is rare next to a same-rate re-prepare. Keeping across a rate change
is a candidate for a later owner decision, with this evidence; the engine's and State test 120's
comments now say "by decision" instead of the band argument.

### L4. Measurements on the implementation

**The family, before (`daa6809`) and after** (the §K shared harness plus §K5's X rows, 48 kHz / 256,
pink noise; `pkX` / `dipX` in dB, `set01f` in ms, and run − fresh at 100 ms / 500 ms / 1 s / 2 s).
Exactly five rows changed, all expected; the other 44 are byte-identical line for line:

| row | before | after |
|---|---|---|
| A/B Drive 2 → 8 (`S7b_AtoB`) | +1.59 / +1.14 / 2,448; +0.67 / +1.59 / +1.12 / +0.29 | +0.10 / +0.07 / 124; +0.07 / +0.09 / +0.06 / +0.02 |
| A/B Drive 8 → 2 (`S7b_BtoA`) | −0.00 / −1.84 / 2,548; −0.42 / −1.84 / −1.48 / −0.41 | +0.06 / +0.04 / 125; +0.05 / +0.06 / +0.04 / +0.01 |
| A/B that turns Level Match on while Drive changes (`X8`) | +1.63 / +1.21 / 2,452 | +0.07 / +0.20 / 124 |
| re-prepare, same rate (`S11`) | +2.46 / +0.04 / 2,215; +2.35 / +1.29 / +0.67 / +0.15 | +0.04 / −0.09 / 0; +0.02 / +0.02 / −0.00 / −0.00 |
| re-prepare, block 256 → 512 | +2.41 / +0.04 / 2,217 | +0.04 / −0.09 / 0 |

Byte-identical, among others: Undo Drive 0 ↔ 10 (`S9a` / `S9b`, +6.73 / −7.89), preset and redo
Drive 0 ↔ 10, the rate change (`f_sr441`, +2.40), both host-reset rows, every O4g engage row (0.00),
the Level-Match-only A/B (`S7a`), the identical-slot A/B (`X1`), the discrete A/B (`X5`, already
re-armed by `processingDiffers`), live edits, and the Case-A-converging and Case-B rows. The kept
re-prepare still shows a −2.63 dB `eMin` at 21 ms: the flushed delay lines refilling, not the gain
(applied −6.795 against the fresh −6.805 dB at that moment); the flush used to hide it under +2.7 dB.
The re-arm's own wobble after a kept re-prepare is 0.042–0.059 dB at ~0.4 s.

**The rate pairs** (Haas, Amount 0.5, Width 1.3, Level Match on; converged published match of fresh
processors, and the transient after `prepareToPlay` at a new rate; a scratch "keep across the rate"
variant rebuilds the coefficients, flushes the analysis, and restores `displayedGainDb` and
`prevPredictedGainDb`):

| | 48 − 44.1 kHz | 96 − 48 kHz | 96 − 44.1 kHz |
|---|---|---|---|
| multisine to 18 kHz | −0.092 dB | 0.000 | −0.093 |
| multisine to 0.45 fs | −0.094 to −0.108 | −0.017 to +0.068 | −0.026 to −0.125 |
| white noise to 18 kHz | −0.221 | −0.003 to −0.014 | −0.224 to −0.237 |

Current flush after 96 → 44.1 or 48 → 44.1 kHz: +2.66 to +2.78 dB at Drive 8 and +3.07 to +3.31 dB at
Drive 10 (−0.45 to −0.64 dB at Drive 0), above 0.1 dB for ~3 s. The keep variant: 0.02–0.13 dB stale,
applied-gain error ≤ 0.143 dB, and never above 0.1 dB in 30 of its 36 rows. Evidence, not a change
(§L3).

**State test 31's remembered slot-B gain** moved from −1.040 dB to +2.078 dB because the switch into
slot B now re-arms. Slot B's converged match, from the same run kept playing and from a fresh
processor at its exact state, is +2.97 dB: the new value is 0.89 dB off it, the old one 4.01 dB and of
the wrong sign — the stale analysis was dragging the value the slot then remembered.

### L5. Tests, and what each rejects

- **DSP Test 67** (`testLevelMatchAbRearmAndSameRateReprepare`, the engine contract; 37 checks, 13
  fail against `daa6809`) and **State test 131** (the processor, as a host and the editor drive it; 97
  checks, 21 fail against `daa6809`); **State test 120**'s leg 2 replaced — it asserted the same-rate
  flush this round removes; it now pins the keep (bit-exact, then re-armed) and the new-rate flush
  (16 checks, 2 fail against `daa6809`). `procedures/TESTING.md` has the legs.
- **Observing a re-arm without reaching inside the matcher.** Silence fed right after the event closes
  the matcher's gate: a re-armed analysis holds the published value exactly (< 1e-6 dB, 0 measured); a
  carried one keeps integrating the tail and moves it (> 1 dB, 1.88–4.67 dB measured). Every probe
  first proves its injection was consumed in the block it judges, so a probe cannot pass on a block
  where nothing happened. Accuracy is read against a fresh engine or processor prepared at the
  destination and fed the identical seeded input from sample 0: D(t) on the published value and a
  per-block least-squares gain of the outputs (residual ≤ 1e-3).
- **The predicate is total.** Test 67's leg 1 swaps one `EngineParameters` member at a time under four
  bases (36 members × 4, plus the identical slot) and requires "re-armed" exactly for the measurement
  inputs and the `processingDiffers` path changes; `check-state-coverage.py`'s `MEASUREMENT_INPUTS`
  table (§J) keeps the member list total.
- **Mutants** (each a one-site change to the implementation, built and run with both full suites):

  | mutant | change | DSP suite | State suite |
  |---|---|---|---|
  | M1 | no re-arm at either injection consumer (`daa6809`'s Q1) | 6 (Test 67 legs 1–4) | 14 (131 a, b control, g) |
  | M2 | re-arm at every injection (P1a) | 3 (leg 1 post / off / identical rows, leg 3 fall probe, leg 4 controls) | 4 (131 b) |
  | M3 | `measChangedAtBottom` without `duckMeasDirty` (re-arm and landing) | 3 (Test 66 Case B, Test 67 leg 4) | 1 (130 (5)) |
  | M3b | the same for the re-arm only | 2 (leg 4) | 0 — the processor cannot reach it |
  | M4 | the fallback consumer re-arms unconditionally | 2 (legs 3, 4) | 0 — idem |
  | M5 | the fallback consumer never re-arms | 1 (leg 4, defensive) | 0 — idem |
  | M6 | never keep (`daa6809`'s Q5) | 6 (leg 5) | 9 (120 leg 2, 131 e) |
  | M7 | keep across a rate change | 2 (leg 5 flushes) | 2 (120, 131 e) |
  | M8 | keep without `! primeMeasChanged` | 1 (leg 5 flush) | 1 (131 e) |
  | M9 | a keep that does not re-arm | 4 (leg 5) | 4 (120, 131 e) |
  | M10 | `primeMeasChanged` on any field (`! sameParameters`), a header mutant | 1 (leg 5, Output-Gain keep) | 1 (131 e) |
  | M11 | keep without `os2 != nullptr` ("prepared before") | **0 → 1** (leg 5 (d), added) | **0 → 1** (131 h, added) |
  | M12 | the injection re-arm through `reset()` | **0 → 1** (leg 1b, added) | 1 (131 b control route) |

  M11 was not equivalent: `sr` reads 44.1 kHz before any prepare, and a fresh processor primes a
  default snapshot that moves no measurement input, so without "prepared before" the first prepare at
  44.1 kHz skips `loudness.prepare` and the matcher never measures (−4.06 dB for ever, against −9.20
  dB). M12 was caught only by one route premise with a 0.37 dB margin because every Test 67 injection
  sat below the predict floor. Both checks were added and each kills its mutant; on `daa6809` the two
  first-prepare checks pass (its `prepare` always prepared the matcher) and leg (1b) fails with the
  rest of Q1. M3b, M4 and M5 are killed by the DSP suite only: the processor consumes its injections
  at the forced bottom.

### L6. Recorded, not changed

- **Keeping the result across a new sample rate** (§L3): measured 0.02–0.13 dB stale against the
  flush's +2.7 to +3.3 dB; not adopted by the authorization's own limit, a candidate for a later owner
  decision with the §L4 evidence and its stated gaps.
- **Haas at 44.1 kHz** reads its default 12 ms as a fractional delay (529.2 samples) through linear
  interpolation, 0.05–0.20 dB quieter than at 48 / 96 kHz on noise (§L3): a product observation about
  the Haas line, outside Level Match.
- **The NaN self-heal** still flushes the whole matcher (ADR-0009's own open owner question); the
  per-block restart of the applied-gain ramp (§I3, §K7) is still a separate lever, deferred.
- **PREfast C6262 on DSP Test 66** (`tests/dsp_tests.cpp`, *"Function uses '20528' bytes of stack"*):
  test-only, introduced with the test in the F13(1b) round, real frame 14,480 B (1.4 % of 1 MiB; the
  `char verdict[5][36][48]` table is 8,640 B), passes the `ulimit -s 1024` suite runs. Not moved to the
  heap and not suppressed; disposition in `procedures/CI_CD.md`. Test 67 (3,328 B) and State test 131
  (1,584 B) add none.
- **Evidence tooling.** The scratch shadow builder used for the scratch variants tested a header
  dependency with `grep -q` under `pipefail`, which can report a miss when `grep` exits early; every
  variant that overrode a header was rebuilt from clean, and the figures above come from those builds.
- **Pre-existing drift, reported:** the Note of 2026-09-24's own comment anchors (the lines of its head)
  and the lower bound of its re-arm-at-every-injection figure (0.022 vs 0.013 dB, a policy not adopted);
  ADR-0004's `:480-562`, ADR-0005's `:726-759` and ADR-0006's `:831-845` (bare anchors, already stale at
  the merge base) and ADR-0005's A(dry) production span.

## M. The Devin review of `a7d2b88`: a kept result was not the applied gain on quiet audio

Round after `a7d2b88`, same branch (PR #156). Finding (Devin): *"Quiet audio loses retained match
level."* Confirmed, and root-caused against the code before any edit.

### M1. Root cause — the lifecycle

The processor's `prepareToPlay` runs `primeParameters (e)`, `prepare`, `setParameters (e)`. Inside
`prepare()`:
1. `keepMatch` is decided (prepared before, bit-identical rate, no primed measurement change, a finite
   published value), and a kept matcher skips `loudness.prepare`.
2. `matchGainSmooth.reset (sr, 0.12)`. JUCE's `SmoothedValue::reset (double, double)` sets the step count
   and snaps the current value to the old target. Every smoother is then written to its neutral value,
   `matchGainSmooth` to **unity**.
3. `updateDerived()` sets the match smoother's **target** to the kept value (Level Match on) or 1.
4. `reset()` adopts an in-flight duck's snapshot (after a prime, `p` already equals it) and, for a keep,
   takes `loudness.softReset()`. The result is authoritative from here on.
5. `snapSmoothers()` settles every smoother **except** `matchGainSmooth` (ADR-0007 decides where it
   lands).

The first `process()` block then moved the applied gain only through the silence→audio snap:
`prevInputSilent` (true after `reset()`) and `! inSilentNow`, where `inSilentNow = inSq < 1e-6·n` over
the conditioned input. Audio resumed below that detector never fired it. The smoother then glided
linearly over 0.12 s from unity to the kept value. The published value was right all along; the audio
was not.

Every re-prepare leg of Test 67, State test 131 and State test 120 resumed on loud audio. There the
snap landed the gain in the first block, so none of them saw this, and the §L4 "+0.04 dB, 0 ms" row was
measured the same way.

### M2. The fix

In `prepare()`, right after `reset()`:
`if (keepMatch && p.autoGainMatch) matchGainSmooth.setCurrentAndTargetValue (decibelsToGain
(published))`.

- **Why after `reset()`.** `p` is final only there, since `reset()` may adopt a duck's snapshot. The
  kept result is authoritative only there, after the `softReset`. The value written is bitwise the
  target `updateDerived()` set, so the smoother is settled.
- **Level Match off.** The smoother stays at unity, so a later engage starts where the engage rules say:
  Case A lands, Case B glides from unity.
- **Every flush is unchanged.** The published value is 0 dB, so unity already agrees with it.

Alternatives were measured and rejected:
- **Writing before `reset()`** (replacing the unity write). This is equivalent on the processor path,
  because the prime makes `p == pendingP`. It differs for an engine-level `prepare()` without a prime
  while a duck is in flight.
- **Inside `reset()` under `keepMatchResult`.** This is equivalent, but it puts a gain write into the
  function that keeps the gain on a host reset.
- **In `snapSmoothers()`.** That function also runs at every forced duck bottom, and a Case-B forced
  engage would then land.
- **The forbidden alternatives:**
  - a lowered detector threshold, which still fails below it (−125 dBFS) and makes quiet flushes snap;
  - a snap on `prevInputSilent` alone, which lands Case B and snaps quiet flushes.

An adversarial review of the lifecycle found the fix correct and found no other path of the same class
(§M5 records the related ones).

### M3. Measured, before (`a7d2b88`) and after

Applied gain as run / twin per sample; the twin is Level Match off at Output Gain 0 dB.

| where | kept value | resumed at | 0 / 10 / 30 / 60 / 120 ms, before | after |
|---|---|---|---|---|
| processor (State test 132 (1)), Drive 8 | −6.0306 dB | −70 dBFS | −0.001 / −0.371 / −1.162 / −2.503 / −6.031 dB | −6.031 dB throughout |
| engine (Test 68 (1)), Drive 8 | −6.0873 dB | −70, −90, −125 dBFS | −0.001 / −0.373 / −1.170 / −2.521 / −6.087 dB, the same at all three levels | −6.087 dB throughout |
| engine, Drive 20 / 24 / 2, positive match | −10.41 / −10.95 / −2.79 / +7.71 dB | −90 / −125 / −70 / −70 dBFS | first sample 0 dB (error = \|kept\|) | kept value from the first sample |

A loud resume is identical before and after: the snap lands it in the first block. The published value
is bit-identical before and after in every leg.

### M4. Tests

- **DSP Test 68** (the engine contract): 30 checks; 5 fail on `a7d2b88`.
- **State test 132** (the processor): 124 checks; 30 fail on `a7d2b88`.

`procedures/TESTING.md` has the legs. Both assert their premises:
- the keep happened;
- the re-prepare happened;
- Level Match is engaged;
- the input is under the detector, numerically and behaviourally (the same quiet level glides after a
  new-rate flush).

Both also pin what must not change: Level Match off (a Case-B engage right after still glides from unity),
the new-rate flush, invalid results, a following injection, and a following host reset.

The existing F13 tests pass unchanged: Tests 66 and 67, State tests 120, 130 and 131.

**Mutants** (each a one-site edit of the fixed engine, built and run through both full suites):

  | mutant | change | DSP suite | State suite |
  |---|---|---|---|
  | no fix (`a7d2b88`) | — | 5 (Test 68's kept claims and (11)) | 30 (State test 132's three kept checks on ten legs) |
  | K2 | the write without its Level-Match gate | 1 (Test 68 (6) Case B, φ 0.094) | 1 (State test 132 (7) Case B) |
  | K3 | the write before `reset()`, replacing the unity write | 1 (Test 68 (11)) | 0 — equivalent on the processor path |
  | K4 | the target only (`setTargetValue`) | 4 | 30 |
  | K5a | no fix; `prevInputSilent = false` after a kept prepare | 6 | 33 |
  | K5b | no fix; the silence→audio snap never fires | 9 (incl. Tests 66, 67) | 35 (incl. State tests 130, 131) |
  | K5c | no fix; the snap on `prevInputSilent` alone | 3 | 9 |
  | K6 | no fix; the detector lowered to 1e-12·n | 4 (the −125 dBFS legs; quiet flushes snap) | 11 |
  | K7 | no fix; `snapSmoothers()` snaps the match smoother | 3 (Test 66: forced Case B lands) | 2 (State tests 130, 131) |
  | K10 | the keep without its `isfinite` term | 2 (Test 68 (8)) | 4 |
  | K11 | the gate read before `reset()` | 1 (Test 68 (11)) | 0 — as K3 |
  | K13 | no fix; the unity write deleted | 2 | 9 |
  | K14 | the fix, plus the loud snap suppressed after a keep | 0 | 2 (State test 132's loud-resume snaps, at 2e-4 dB) |
  | K8 / K9 / K12 | the write on every Level-Match-on prepare; the value read before `reset()`; a snap to the target on any keep | 0 | 0 — equivalent: a flush publishes 0 dB (unity), `softReset()` keeps the result, the target is the same value |

  Where the rows were run:
  - K2, K4, K5a–c, K6, K7, K10, K13 and K14: the DSP suite before leg (11) existed (619 checks). Each of
    them that starts a kept value at unity would also fail leg (11). K14's State count is with the 2e-4
    dB snap tolerance.
  - The no-fix row, K3, K8, K9, K11 and K12: the final suites (620 DSP checks).
  K3 and K11 first survived both suites, and Test 68 leg (11) was added. K3 and K11 are identical on the
  processor path, where the prime makes `p == pendingP`. Leg (11) runs the unprimed engine API with a
  Level-Match engage in flight at `prepare()`, where the fix's placement after `reset()` decides.
  K14 first survived State test 132's 0.02 dB snap tolerance, because the snap moves the kept value only
  one MEASURE step (0.0018 dB). The loud-resume snaps are now judged at 2e-4 dB.

### M5. Recorded, not changed

- **A flush's own quiet glide.** After a first prepare, a new rate or a primed measurement change, the
  published value is 0 dB and the applied gain is unity; the first block's predict floor then publishes
  e.g. −4.06 dB at Drive 8. Below the detector the applied gain glides there over 0.12 s. This is
  measured the same before and after the fix, and Test 68 (7) and State test 132 (8) pin it as
  unchanged. There is no kept result behind it: it is the flushed restart, Case-B-shaped, with its own
  owner question. It is a candidate, not a defect of Q5.
- **A NaN published value can reach `prepare()` through the engine API.**
  - `injectMatchGainDb (+Inf)` passes `inj > kNoInject + 1`.
  - Consumed with Level Match off while the matcher's gate is open, it makes MEASURE compute Inf − Inf =
    NaN, and `clampd` passes it.
  - The output plays Output Gain, so no self-heal runs, and the NaN is absorbing.
  - It survives a host reset (`softReset` keeps the result).

  `keepMatch`'s `isfinite` term flushes it at a re-prepare (Test 68 (8a), State test 132 (9)(iii)).
  The processor cannot inject +Inf: `abMatchGain` is 0 or a `getMatchGainDb()` reading, and a NaN
  injection is refused. Rejecting a non-finite injection in both consumers would close the route. That
  is outside this finding and borders KI-029 (non-finite ingress).
- **`keepMatch` ignores an in-flight duck's `pendingP` when `prepare()` runs without `primeParameters`.**
  This is reachable only through the engine API. The processor always primes, and the prime compares the
  live `p` with the snapshot.
- **Two State-suite runs at once can collide.** Running two at the same time made State test 19 read
  another run's preset file (a shared path outside `HOME`). A single run is clean. This is a test-harness
  property, recorded for whoever parallelises CI.

## N. The Devin review of `0fbce03`: a live edit left the kept result stale

This round follows `0fbce03` on the same branch (PR #156). Devin's finding: *"Live edits retain stale
match gain."* It points at `keepMatch` in `AnamorphEngine::prepare()`. The finding was confirmed
through the processor before any edit.

### N1. Reproduction and root cause

Q5's condition 3 compares the snapshot `primeParameters()` adopts with the engine's live `p`. Three
transitions write `p` as they happen:
- a live continuous edit, on the Normal path (`p = np`);
- an ordinary duck's continuous part (`copyContinuous`);
- a duck bottom (`p = pendingP`).

When the host re-prepares after any of them, the prime compares the new state with itself. So
`primeMeasChanged` stays false, and the result the re-prepare keeps still describes the audio from
before the edit. The analysis is re-armed by then, so nothing is left in the engine that could correct
it quickly: the kept value glides toward the new measurement at the measure's own speed.

The probe ran through the processor: 48 kHz / 256, Haas, Amount 0.5, Drive 8, Width 1.0, Output Gain
−3 dB, Level Match on, stationary noise with seed 901. It converged for 3 s, made the edit as a user
gesture, and called `prepareToPlay` at the same rate *d* blocks later. Three lanes played the same
stream:
- **the live lane:** the edit, then the re-prepare;
- **the restore control:** the same edit made while suspended, which reaches `prepare()` through the prime;
- **the fresh reference:** a new instance at the destination state.

The error is |published − reference|, at the first sample and integrated over 3 s:

| edit | head `0fbce03`, d = 1 block (5 ms) | restore control (flush) |
|---|---|---|
| Width 1.0 → 2.0 (Devin's case) | KEPT −5.471 dB against −7.728: 2.25 dB, 1.84 dB·s | 3.40 dB, 1.81 dB·s |
| Width 1.0 → 1.3 | kept: 0.62, 0.56 | 2.02, 1.78 |
| Drive 8 → 12 | kept: 1.55, 1.37 | 1.55, 1.37 (the predict floor sets both) |
| Drive 8 → 2 | kept: 3.02, 1.66 | 0.43, 0.40 |
| Mix 1.0 → 0.5 | kept: 2.25, 1.63 | 0.74, 0.65 |
| Amount 0.5 → 1.0 | kept: 0.78, 0.75 | 2.18, 1.97 |
| Output Gain −3 → −9 (not a measurement input) | kept: 0.00, 0.02 | kept: 0.00, 0.02 |

On head the live lane kept at every *d* from 5 ms to 8.5 s. Its error decays with the measure: Width
1 → 2 went 2.25 / 1.73 / 1.07 / 0.35 / 0.11 dB at 0.005 / 0.53 / 1.07 / 2.13 / 3.2 s. The modulated
programme (1.3 Hz and 0.37 Hz envelopes, 18 dB deep, with a 220 Hz partial) gave the same pattern.

### N2. The invariant, and what "current" means

The owner set the boundary and authorised the decision: *"A same-rate re-prepare may retain a
published Level Match result only when that result is still valid for the measurement state that the
re-prepared engine will use."* Valid means **current**: the published value is the measure's answer for
the inputs it now reads, within the Level Match settle tolerance (0.1 dB). The cases:
1. An immediate re-prepare after a measurement-input change is not current, however the change arrived,
   so it flushes as the restore route does.
2. A change the measure has caught up with is current again, and the result is kept.
3. A change to a field `measurementInputsDiffer` does not compare invalidates nothing.
4. A non-finite result flushes (condition 4). A flush publishes no measurement of any state, so it
   counts as current.

The measure's own evidence decides. The engine calls `LoudnessMatch::inputsChanged()` whenever it adopts
a change to anything the measurement reads. The call sites:
- the Normal path (a live edit);
- a change heard during a duck: its continuous part goes live at once, and in a fade-in no bottom
  follows to report it;
- every duck bottom with `measChangedAtBottom`, which covers forced swaps, discrete changes and an
  ordinary duck's continuous part (`duckMeasDirty`);
- a host reset that lands an in-flight duck carrying such a change.

An ordinary duck's entry is not a call site. Its continuous part goes live there, but the bottom reports
it within ~6 ms. A re-prepare in between meets `prepare()`'s in-flight-duck test (below). A report at the
entry would be redundant on every path, and no test could tell it apart.

The matcher then snapshots both integrators. They are linear one-poles, so after N samples the
pre-change energy left in them is exactly (1 − c)^N times the snapshot, and the audio heard since the
change has energy `meanSq − (1 − c)^N · snapshot`. The measure's own target formula reads it (the
−0.691 offsets cancel; the same 1e-7 floor and ±24 dB clamp apply).

Each audible block glides the published value by `coeff` toward that block's target. The bookkeeping
splits the step into two parts:
- `postShare` / `postSum` accumulate the step's weight and the post-change target. Both decay by
  (1 − coeff) per step, exactly like the published value. `postSum / postShare` is therefore the
  glide-weighted mean of the post-change measurements, taken with the weights the published value itself
  gives them.
- Everything else in the published value is older: its value at the change, and the targets the
  pre-change energy coloured.

The result is current again when `postShare ≥ ½` and |published − postSum/postShare| ≤ 0.1 dB. The
difference is exactly (1 − postShare) times the distance between the older content and the post-change
mean. It therefore measures the pre-change influence on the published value directly. It does not test
a coincidence between a moving target and a gliding value.

Three rules cover silence, what counts as a post-change measurement, and the predict floor:
- Silent blocks do not glide, so they confirm nothing.
- A block adds share only when the input heard since the change is itself a measurement: its dry energy
  is above the silence gate and at least half of what the dry integrator holds. The linear split is
  exact for the integrators, but pre-change audio still in the pipeline (the dry reference's alignment
  and filters, a delay line emptying into the wet) arrives after the change and counts as post-change
  audio. Against no input, or a sliver of it, those tails are the whole ratio; §N3 has both failures.
  The gate is absolute because the half is relative: `softReset()` empties the pre-change snapshot, and
  after that any input is at least half.
- The predict floor moves the published value only in the block that first reads a raised Drive or Mix.
  That is a measurement input, so `inputsChanged()` has already emptied the share. The check reads the
  value itself either way. A variant that restarts the share when the floor fires produced identical
  output on all 48 probe rows (§N3).

The other operations:
- An A/B injection marks the result current (the slot's own measurement, restored with its state).
- `reset()` clears the question.
- `softReset()` keeps both the currency and the share. It zeroes only the pre-change snapshot, because
  the integrators then hold post-change audio alone.

`prepare()` keeps a result only while it is current. It also refuses while a duck that changes a
measurement input is in flight (`adoptsMeasChange`), because its own `reset()` adopts what that duck's
bottom would have reported. On the processor path that is an ordinary duck's live continuous part. On
the unprimed engine API it is also the pending snapshot: §M5's recorded gap, which the prime covers for
the processor. Nothing published changes: the
bookkeeping writes no value the measure, the predict or the engine reads.

### N3. Strategies compared

| strategy | what it keeps | verdict |
|---|---|---|
| S0: the status quo | every result the prime cannot see changed | fails case 1 (§N1) |
| S1: a sticky dirty flag, cleared only by a flush or an injection | nothing after any edit, for ever | the rule the owner excluded in so many words |
| S2: time since the change | whatever a timeout allows | the design has no time-based validity contract; a timeout would stand in for the measure |
| S3: the engine's smoothers settled | results 20–120 ms after an edit | the smoothers are not the measurement; the measure needs seconds |
| S4: a "measured-for" snapshot or generation | results whose snapshot matches | still needs a caught-up rule, and its revert shortcut (edit and back) is wrong while the analysis still holds the excursion |
| S5a: the post-change energy dominates each integrator (pre-change share ≤ ½) and one block's post-change target agrees within 0.1 dB | results after one agreeing block | **rejected on measurement**, below |
| S5 with share from any post-change audio over the gate, input or wet | results a wet tail agrees with | **rejected on measurement**, below |
| S5 with share from post-change input over the gate, without the half | results a ratio of tails agrees with | **rejected on measurement**, below |
| **S5: the glide split (§N2)** | results the measure has provably caught up with | **adopted** |
| S6: publish the post-change measurement at the re-prepare instead of flushing | always a measurement of the new state | a candidate: it changes what is published, and it would remove the trade-off in §N6 |

S5a was the first implementation, and it passed both suites. An engine-level scan then found two ways it
marked a stale result current. The scan covered 24 scenarios × 8 seeds × {noise, modulated programme}:
- six path changes: algorithm × 3, Multiband on, Mono Maker on, M/S;
- seven continuous edits;
- seven silence gaps of 0.1–5 s after the edit;
- three edits followed at once by a host reset.

The two failure modes were:
1. **A host reset right after the edit.** `softReset()` empties the integrators, so "the post-change
   energy dominates" holds trivially. The first block's short-window target then agreed with the old
   value by chance. The result went current at block 0: Width 1 → 2 on noise, 2.26 dB off the fresh
   reference; Drive 8 → 2 on the modulated programme, 4.15 dB off.
2. **The modulated programme.** The target moves with the programme, because Drive's saturation makes
   the wet/dry ratio level-dependent, and it crossed the gliding value at ~1.3 s after most edits. The
   result went current up to 1.33 dB off (Width 1 → 2, then a 1 s gap).

The glide split went through two gates before the adopted one. Two more sweeps target that gate, each
over 8 seeds × {noise, modulated programme}:
- **a silence sweep:** Width 1 → 2 at Drive 0–24 dB (7 values), with the input silent for 1–8 s from
  the edit (5 values);
- **a burst sweep:** the same Drives, then 2–40 blocks of audio after the edit (5 values), then 3 or 8 s
  of silence.

The two rejected gates:
1. **Share from any post-change audio over the gate, input or wet** (the first S5 build). The silence
   sweep found Drive 24 on the modulated programme with ≥ 3 s of silence. The Haas delay line emptied
   into the wet with no dry behind it, and the gate stayed open on the decaying pre-change energy. That
   tail's ratio to the silence floor agreed with the gliding value, and the result went current 2.7 s
   into the silence, 2.3 dB off.
2. **Share from post-change input over the gate, without the half.** This passed both sweeps (burst:
   ≤ 0.19 dB) but failed the scan on Drive 8 → 2 with 1 s of silence from the edit, noise seed 905. The
   dry reference's tail held the post-change dry energy at 1.9e-6, just over the gate (7 ppm of the
   integrator), for the first 0.26 s of the silence, against 2.9e-3 of Haas tail in the wet. That −24 dB
   target took up to a quarter of the share. 0.43 s after the audio resumed, the post-change mean met the
   gliding value 2.0 dB off.

The adopted gate requires both conditions. Test 69 leg (10) pins each on the matcher directly: (10a) and
(10b) a tail with no input behind it, (10c) tails against a sliver of input.

S5 on the same scan:
- currency came no earlier than 0.63 s after a change, and never at a crossing;
- at the moment of currency the result was within 0.15 dB of the fresh reference on noise, 0.19 dB on
  the modulated programme;
- the host-reset rows went current at 2.3–2.9 s, 0.09–0.15 dB off;
- on the silence and burst sweeps it was within 0.15 dB at currency and 0.16 dB after, and it never failed to go current.

These residuals are the measure's own history dependence. The lane and the reference glide toward the
same audio from different histories, and their difference decays below 0.01 dB by ~5 s.

### N4. Measured on the implementation

**The processor grid** re-prepared *d* after the edit and measured |error| at the first sample. The
grid ran from 0.005 to 8.5 s:

| edit | noise: flushes (as the restore control) for d ≤ | noise: keeps from, error | modulated: flushes for d ≤ | modulated: keeps from, error |
|---|---|---|---|---|
| Width 1.0 → 2.0 | 3.2 s | 4.3 s, 0.04 dB | 3.2 s | 4.3 s, 0.04 dB |
| Width 1.0 → 1.3 | 2.1 s | 3.2 s, 0.03 | 1.1 s | 2.1 s, 0.12 |
| Drive 8 → 12 | 2.1 s | 3.2 s, 0.09 | 3.2 s | 4.3 s, 0.03 |
| Drive 8 → 2 | 3.2 s | 4.3 s, 0.06 | 3.2 s | 4.3 s, 0.06 |
| Mix 1.0 → 0.5 | 3.2 s | 4.3 s, 0.04 | 3.2 s | 4.3 s, 0.06 |
| Amount 0.5 → 1.0 | 2.1 s | 3.2 s, 0.05 | 1.1 s | 2.1 s, 0.14 |
| Output Gain −3 → −9 | never | at once, 0.00 | never | at once, 0.00 |

Every flush row is identical to the restore control, error by error. The S5a build kept on the
modulated programme from 2.1 s, 0.12–0.40 dB off. S5 keeps two rows there, Width 1.0 → 1.3 and Amount,
0.12 / 0.14 dB off: inside the 0.19 dB the scan measured at currency on that programme.

**Nothing changes without a re-prepare.** The F13(1b) route harness gained five rows and a hash mode:
FNV-1a over the output samples, over the published value per block, and over the applied gain's current
and target values per block. It was built against the pre-fix sources (`0fbce03`, with every header
dependent recompiled) and against the fix. 22 of 23 rows are bit-identical in all three hashes:
- S1–S4: Apply, Undo and Redo;
- S5–S6: hand engages and disengage;
- S7: A/B with the injection;
- S8: a preset load;
- S9: a forced Undo;
- S10: host resets in the fade;
- S11: a re-prepare with nothing changed;
- S13: a live script with every measurement-input category, a 60-block drag, a ducked algorithm change,
  Output Gain, Level Match off and on, two Undos, a Redo, two host resets, Apply and an Undo;
- S14: an A/B script with a host reset;
- S15: a re-prepare 6 s after a live edit.

The 23rd row, S12, is the positive control: a live edit with a re-prepare 5 ms later. Its hashes
differ, and the fix publishes −8.622 dB 3 s later against the pre-fix −8.630.

### N5. Tests

- **DSP Test 69** (`testLevelMatchReprepareKeepsOnlyACurrentResult`, the engine contract, and in leg
  (10) the matcher directly): 102 checks. Against `0fbce03`, 29 of the 94 checks of legs (1)–(9) fail
  (re-run on the final test file with leg (10) removed, which drives the matcher's new API).
- **State test 133** (`testLevelMatchReprepareKeepsOnlyACurrentResultThroughTheProcessor`, the processor
  contract): 129 checks; 31 fail against `0fbce03`.

`procedures/TESTING.md` has the legs. On both engines the failures are exactly the flush claims; every
premise, liveness check, keep and control passes. The premises are asserted, never assumed:
- the edit was heard, from its block's output against an event-matched no-edit twin;
- the measure had not caught up (≥ 1 dB or ≥ 0.3 dB off a fresh engine at the destination), or had
  caught up, read through the published value itself (within 0.05 dB for 0.5 s). No test sleeps a fixed
  guess;
- the re-prepare happened, and the flush is exactly 0 dB followed by the restore route bit for bit;
- a width smoother was still gliding at the re-prepare (Test 69 (1d): 1.016 → 1.249 of 2.0 inside the
  edit block).

The existing F13 tests pass unchanged: Tests 66, 67 and 68, and State tests 120, 130, 131 and 132.

**Mutants.** Each is a one-site edit of the final tree (M19: the rejected first design's matcher), built
and run through both full suites. The counts are failures, DSP / State.

| mutant | change | DSP | State | rejected by |
|---|---|---|---|---|
| M01 | `keepMatch` without `isResultCurrent()` | 26 | 29 | every flush claim but the in-flight-duck ones |
| M18 | `inputsChanged()` never marks | 29 | 29 | as M01, and Test 69 (10a)–(10c) |
| M02 | `keepMatch` without the in-flight-duck test | 3 | 1 | Test 69 (1q) and its second re-prepare, (9a); State test 133 (10) |
| M20 | that test without its `pendingP` half (unprimed only) | 1 | 0 | Test 69 (9a) |
| M03 | no report on the live path | 23 | 24 | the live legs, the drag, the half-way points, the host resets |
| M05 | no report for a change heard during a duck | 1 | 1 | Test 69 (1r); State test 133 (11) |
| M06 | no report at a duck bottom | 1 | 3 | Test 69 (1p); State test 133 (3) Undo, Redo, preset |
| M07 | no report when a host reset lands a duck | 1 | 1 | Test 69 (1s); State test 133 (8d) |
| M27 | a report for every changed snapshot | 6 | 3 | Test 69 (3a)–(3f); State test 133 (5) |
| M28 | a report at every duck bottom | 2 | 2 | Test 69 (3g), (1r)'s twin; State test 133 (4), (11)'s control |
| M08 | currency without the half share | 0 | 2 | State test 133 (8b) |
| M09 | a 1 dB agreement instead of 0.1 dB | 5 | 3 | Test 69 (2a)'–(2d)', (10c); State test 133 (6) half-way, (8c) |
| M10 | the post-change target read from the whole integrators | 3 | 0 | Test 69 (4a), (10a), (10c) |
| M23 | the gate without the half | 1 | 0 | Test 69 (10c) |
| M24 | the half without the gate | 1 | 0 | Test 69 (10b) |
| M22 | the half, with share also from a post-change wet over the gate | 1 | 0 | Test 69 (10b) |
| M25 | share from any post-change audio over the gate, no half (the first S5 build) | 2 | 0 | Test 69 (10a), (10b) |
| M15 | share from every audible block | 2 | 0 | Test 69 (10a), (10b) |
| M11 | an A/B injection leaves the result stale | 2 | 1 | Test 69 (5a), (6b); State test 133 (3) A/B |
| M12 | a flush leaves the result stale | 1 | 1 | the second re-prepares after a flush |
| M26 | `softReset()` clears the question | 4 | 6 | Test 69 (1s), (5b), (5c), (10b); State test 133 (8a)–(8d) |
| M16 | `inputsChanged()` keeps the previous share | 1 | 0 | Test 69 (2f) |
| M17 | the pre-change energy never decays | 11 | 12 | every converged keep, and leg (10)'s control and liveness: the result never becomes current |
| M19 | S5a, the rejected first design | 2 | 4 | Test 69 (10a), (10b); State test 133 (8b), (8c) |
| M13 | `softReset()` keeps the pre-change snapshot | 0 | 0 | not rejected: see below |
| M14 | `softReset()` also clears the share | 0 | 0 | not rejected: see below |

Two mutants are not rejected. Both change only what `softReset()` leaves behind, and the false-currency
scan (§N3) bounds both:
- **M13** subtracts pre-change energy that the re-arm has already removed from the integrators, so the
  post-change energies read low until that phantom decays. On the scan it moves currency in 18 of the
  48 rows: later in 13 (by up to 1.53 s), earlier in 5 (all on the modulated programme, by 0.10–0.44 s).
  At currency it is never more than 0.19 dB off the fresh reference, the adopted rule's own bound there.
- **M14** discards the post-change share accrued before a re-arm, so currency can only come later. It is
  identical to the adopted rule on all 48 rows: every re-arm in the scan comes with its own change, when
  the share is still empty.

Pinning either needs the timing of currency after a re-arm, which no invariant here fixes. The suites
pin the invariant (never keep a stale result) and its liveness (keep once caught up).

**Validation on the final tree** (Linux x86-64, GCC, Release):
- both suites under `ulimit -s 1024`: DSP 722 / 0, State 5276 / 0;
- the pre-fix run above: 29 / 31 failures, exactly the flush claims;
- the Devin controls still fail as designed: State test 129 with Apply disabled (69 failing checks) and
  with its NaN guard removed (6); `prepare()` without the kept-result write fails State test 132 (33
  state failures, 6 in DSP);
- the 186 finite-value legs hash identically to the pre-fix build, and the 23 route hashes of §N4 are
  identical to the first build of this fix;
- frames (GCC `-fstack-usage`): Test 69 5,104 B, its largest lambda 992 B; State test 133 2,384 B;
- `check-docs`, `check-dispatch`, `check-portability`, `check-realtime`, `check-state-coverage` and
  their self-tests; `check-citations` against `0fbce03`, `043c7e3`, `a7d2b88` and `659ca0a`, and its
  self-test (255 cases); the GCC gate flags on the changed translation units (no new warning).

### N6. Recorded, not changed

- **The trade-off window.** A re-prepare from ~0.3 s to ~3–4 s after a large measurement-input change
  now flushes where the partly converged value was closer. Width 1 → 2 at 1.07 s: the keep was 1.07 dB
  off, the flush is 3.45 dB off. Condition 3 already accepts the same trade-off for the restore route.
  S6, publishing the post-change measurement at the re-prepare, would remove it; it is a candidate
  because it changes what is published.
- **Automation of a measurement input** keeps the result not current, because every block's change
  restarts it. A re-prepare during such automation flushes, as the same automation arriving through a
  restore always did.
- **A slot's remembered value is restored as current.** An A/B switch injects the destination slot's
  remembered gain (#23), and the matcher takes it as current. The processor probe (Haas / Drive 8 slot A;
  slot B at Drive 12; switch back, then a same-rate `prepareToPlay` d later) measured three cases:
  - **B converged when left:** kept 0.00–0.01 dB off at every d from 0.1 s.
  - **B left 0.3 s after its edit:** kept 1.09 / 0.87 / 0.61 / 0.38 / 0.12 dB off at 0.1 / 0.3 / 0.6 /
    1.1 / 2.1 s. The flush there is 7.61 dB off at its first sample, and 1.41 dB·s over 3 s against the
    keep's 0.97–0.13.
  - **A never-visited slot, and both slots after a session restore,** inject 0.0 dB (ER-STATE-20's
    fresh-instance value) and are taken as current the same way. For the default sound 0.0 dB is right
    (−0.001 dB measured).

  The variant that makes an injection wait for the measure's confirmation (`setDisplayedGainDb` reporting
  `inputsChanged()`) closes both cases. It flushed the converged slot for 0.1–0.3 s after the switch, and
  the stale slot until 2.1 s and beyond. It also failed Test 67 leg (5), 3 checks: its probes displace the
  value by an engine-API injection before a re-prepare. And it failed State test 132 leg (6)'s premise.
  Both tests are to be preserved. Carrying each slot's currency with its remembered value would close the
  gap without that cost. The value crosses from the message thread to the audio thread with the
  injection, so that needs a second cross-thread field: a threading-model change, outside this finding.
  It is a candidate. **Resolved in §O** (Devin review of `20af101`): the record carries whether its value
  was measured, and it stays on the audio thread, so no second cross-thread field was needed.
- **§M5's unprimed in-flight duck is closed.** `prepare()` now refuses to keep when `reset()` will adopt
  a measurement-input change (`adoptsMeasChange`).
- **A documentation drift, corrected.** `THREAD_MODEL.md` cited `LoudnessMatch.h:112` for the published
  atomic, which `0fbce03` already had at `:126`. It now reads `:161`.

## O. The Devin review of `20af101`: an unsettled A/B gain survived a re-prepare

*"Unsettled A/B gain survives re-prepare"* (`src/dsp/LoudnessMatch.h`, `setDisplayedGainDb`). §N6 recorded
it and did not change it; this section decides it, on the owner's authorization of 2026-09-25 (ADR-0007,
Amendment of 2026-09-25, A/B provenance).

### O1. Reproduction and root cause

A processor probe with private access (scratch, not in the repository) was run at 48 kHz / 256 with
Haas, Amount 0.5, Drive 8, Output Gain −3 and Level Match on, over noise (seed 901). The sequence:
1. Slot B, a copy of converged A, is edited Drive 8 → 12 and left *t* later.
2. The engine plays 6 s on A, then switches back to B.
3. A same-rate `prepareToPlay` runs 4 blocks after that switch.

At `20af101`, *t* = 0.3 s, block by block:
- **When B is left**, it publishes −6.0728 dB. It is **not current**: a fresh processor at B measures
  −7.608.
- `abSwitchToAdopted` stores the value, and only the value: `abMatchGain[1] = engine.getMatchGainDb()`.
- **At the return's forced bottom**, the injection writes the value back and `setDisplayedGainDb` sets
  `resultStale = false`. The result is current from that block on, while it glides.
- **The re-prepare** passes condition 5 and keeps −6.0844 against a fresh −7.6109.

| *t* | recorded current | kept error at the first sample | over 3 s |
|---|---|---|---|
| 0.1 s | 0 | 1.596 dB | 1.407 dB·s |
| 0.3 s | 0 | 1.526 | 1.335 |
| 0.6 s | 0 | 1.252 | 1.102 |
| 1.1 s | 0 | 0.801 | 0.708 |
| 2.1 s | 0 | 0.286 | 0.263 |
| 3.2 s | 1 | 0.086 | 0.095 |
| 4.3 s | 1 | 0.025 | 0.050 |
| 6.0 s | 1 | 0.014 | 0.036 |

**A validity-state error, not convergence lag.** The value B remembered is exactly what the measure had
published after *t*. Its label claims a measurement that never happened. The label is the only thing
condition 5 reads, and it is also the only thing the injection changed. A never-visited slot shows the
same error with no edit at all: it injects 0 dB, is made current, and is kept 4.574 dB off.

### O2. The invariant, and the second bit it needs

*A remembered A/B Level-Match gain carries the validity state of the measurement result that produced
it; restoring the value and restoring its validity are separate.*

**Why "current" is not enough.** "Current" (§N2) means the value describes no **previous** state. A
flush is current by that definition. That is right in the flush's own context: its first block's
predict floor lands on the flushed 0 dB. It is **not portable**. The adversarial reviews of §O4 measured
records taken within ~2.7 s of a flush, restored elsewhere and kept:
- A/B 0.3 s / 1.0 s after the first prepare: kept 1.071 / 0.498 dB off;
- the first second silent (the record is the Drive-8 predict floor, −4.057): kept 1.457 dB off;
- A/B before the first block after the first prepare (record 0 dB), returning from Drive 12 to
  Drive 8, where no floor fires: kept 4.704 dB off. A flush plays 1.47 dB off there.

**The predict floor.** The floor can also replace a **measured** restored value in the bottom's own
block, when the destination's Drive or Mix is higher than the source's. That value is then a prediction,
not the slot's measurement.

The record therefore carries `LoudnessMatch::isResultMeasured()`: the measure has **confirmed** the
published value for its current inputs, by §N2's criterion. It implies current.

| event | measured |
|---|---|
| flush (`reset`) | cleared |
| `inputsChanged()` | cleared |
| the predict floor lowering a measured value | cleared, and the bookkeeping restarts from a fresh snapshot |
| `softReset()` | kept |
| the confirmation | set |
| `setDisplayedGainDb (db, true)` | set |

The confirmation bookkeeping now runs while the result is not measured, which includes after a flush. It
moves only the new bit there, so `isResultCurrent()` and condition 5 are unchanged.

**Post-restore convergence.**
- A value restored **not** current becomes current, and measured, only through the confirmation: 2–4 s
  on audible programme; silence confirms nothing.
- A value restored **measured** stays measured while it converges. The exceptions are the floor case
  above and an input change.

### O3. Every capture and restore path, traced

`20af101` had one capture and one restore point, both on the message thread, in `abSwitchToAdopted`:
`abMatchGain[abActive] = engine.getMatchGainDb()` and `engine.injectMatchGainDb (abMatchGain[slot])`.
The injection was consumed at the forced bottom (`process`, with the P1b re-arm) or on the defensive path
(a prime dropped the duck; a host reset completed it). `adoptRestoreTail` zeroed the array
(ER-STATE-20). `abCopyToOther` moved a slot's state and not its gain. The Oversampling index is shared
by both slots.

The windows where the value and the state it describes come apart, measured at `20af101` with the
windows probe:

| window | kept error |
|---|---|
| (a) an edit to B the engine had not adopted when B was left | 1.599 dB |
| (b) an ordinary duck in flight when B was left | 1.715 dB |
| (c0) A → B → A before any block (B's slot got A's live value) | 1.609 dB |
| (c1) the same, with one block between | 1.608 dB |
| (d) Copy onto a visited B | 1.900 dB |
| (e) Oversampling changed on A | 0.565–0.636 dB |
| (f) an edit during the return fade | 1.049 dB |
| a never-visited slot | 4.574 dB |

W7, a switch pending across a new-rate prepare, was carried to the new rate. W9, Apply in the same turn
as a switch, is out of scope: Apply reads the published value and nothing else.

The full path table, after the change, is ADR-0007's (Amendment of 2026-09-25, A/B provenance).

### O4. Strategies compared, and the three adversarial reviews

A scratch workflow ran four stages:
- **Probes:** windows, bit-exactness, a bounded audit, threading.
- **Two designs:** D, the engine keeps the record; B2-exact, a 64-bit published word with a tag re-keyed
  through the tolerance.
- **Three adversarial reviews,** each looking for refutations: provenance; false negatives and
  regressions; threading, realtime and static analysis.

The findings:
- **Bit-exactness.** A/B round trips move `mbFreqLow/Mid/High` and `monoMakerFreq` by up to 12 ulp
  (1.24e-6 relative), below `measurementInputsDiffer`'s 1e-5. A bitwise token calls 4–5.5 % of
  converged round trips "not current". The engine's `p` equals `toEngine (APVTS)` bit for bit
  (3000 / 3000).
- **Strategy A** (a boolean with the value) is wrong in every window. The message thread cannot read
  the matcher's currency without a race (TSan), and a copied flag describes the wrong moment.
- **B1** (a generation) misses (d)–(f).
- **B2-exact** holds against the windows. Its injection can land after the bottom on the defensive path,
  un-re-armed, and drift 0.53–0.55 dB while labelled measured (0.44–0.55 dB kept). Its key needs a second
  list mirroring `measurementInputsDiffer`.
- **C** (every restore waits for confirmation) fails Test 67 (5)(b) and Test 69 (5a) / (6b), and flushes
  converged slots after every switch.
- **D's first prototype** was refuted twice:
  - it recorded `isResultCurrent()`: the flush-currency cases of §O2, and the floor case (20 of 595
    random-scan keeps more than 0.3 dB off, worst 4.716 dB);
  - it ignored `duckMeasDirty`: an ordinary duck opened by a non-measurement discrete change (the
    Level Match switch, or a band count with Multiband off) with a Width or Drive edit, and A/B inside
    its fade-out, was kept 1.32–2.26 dB off.

  Its transport survived. The restore is armed with the duck and consumed at the bottom with P1b, so
  there is no late landing (0.023 dB where B2-exact drifts 0.530).
- **Its new-rate "disown"** wrongly unmeasured a 48 → 44.1 → 48 kHz round trip (1.429 against
  0.045 dB·s). The adopted design stamps the rate instead.

**Adopted: D with the measured bit, the in-flight-duck rule and the rate stamp.** All three reviews
named this hybrid as the strongest option.

### O5. The implementation and the handoff

The request word (`duckRequest`, existing, relaxed) gains a switch bit with two 4-bit slot fields and a
forget bit:
- `requestDuck` is a `fetch_or`;
- `requestAbSwitch` is a CAS that keeps a pending source;
- `forgetAbMatchMemory` is a CAS that keeps a pending duck and drops a pending switch.

`takeRequests()` runs at the top of `setParameters` and in `primeParameters` before `p = np`. It forgets,
records, and arms. `restoreAbSlot()` runs at the forced bottom and on the defensive path, and hands
`adoptRememberedMatch (db, measured, rearm)` the value and the two separate answers.

What the message thread reads for a switch: nothing of the engine's. The records are audio-thread /
prepare-path state, never concurrent with `process()`. `EngineParameters` is 132 bytes and trivially
copyable; the engine grows by ~290 bytes and is heap-allocated in every host and test.

Checked:
- check-realtime reports 0 violations. A seeded lock in `restoreAbSlot` is reported.
  `LoudnessMatch::setDisplayedGainDb` stays header-inline: a callee in another translation unit is the
  lint's documented cross-file gap, and RTSan covers it under `process`.
- The processor asserts `AnamorphEngine::kAbSlots == anamorph::kNumAbSlots`, so the DSP layer does not
  include the processor's header.

### O6. Measured on the implementation

| measurement | result |
|---|---|
| the O1 sweep | *t* ≤ 2.1 s flushes: 1.403–1.418 dB·s over 3 s, against the keep's 0.263–1.407. *t* ≥ 3.2 s keeps, bit-identical to `20af101` (0.086 / 0.025 / 0.014 dB). The published value before every re-prepare is bit-identical; only the label differs |
| windows (a), (b), (d), (e), (f), never-visited | flushed |
| windows (c0) / (c1) | B's own value restored and kept, 0.009 / 0.007 dB, where `20af101` restored A's (the one audible change) |
| control | kept, 0.015 dB, bit for bit |
| processor routes | 23 of 23 bit-identical to `20af101` per block (output, published, applied gain): the live-edit harness of §N4, A/B script included |
| DSP suite output | byte-identical |
| finite-parameter hashes | 186 of 186 unchanged |
| ThreadSanitizer, the handoff under load | an engine-only harness (the DSP sources built with `-fsanitize=thread`): a message thread issues `requestAbSwitch` / `requestDuck` / `forgetAbMatchMemory` and reads the published value (5.2 M operations) while the audio thread runs 4000 blocks of `setParameters` + `process`, with a `primeParameters` + `prepare` every 500 blocks (every second one at a new rate): 0 reports. Positive control, the same harness with `requestAbSwitch` also writing the record from the message thread: 4 reports (`takeRequests` against `requestAbSwitch`) |
| Devin controls on the implementation | State test 129 fails 5 with the NaN guard removed and 5 with Apply disabled; State test 132 fails 30 without the retained-gain seat; State test 133 fails 24 without the live-edit report. Baseline: 265 checks, 0 failures |

State test 134's processor measurements, before (the same test built against `20af101`) and after, are
tabulated in ADR-0007's amendment (A/B provenance). The window cases this section's probes did not reach
all flush where `20af101` kept:
- window (g), 2.13 dB;
- the flush-current captures, 4.63 / 1.06 dB;
- the floored restore, 1.27 dB;
- identical slots with an unmeasured record, 1.48 dB;
- a session restore then a switch, 1.59 dB;
- a switch and an Undo in one turn, 1.91 dB;
- a 44.1 kHz restore of a 48 kHz record, and a new-rate prime.

Their event-matched controls keep. Two audible changes are both provenance corrections:
- (c0) / (c1): B's own value is restored, 1.59 dB closer;
- a switch pending when a session restore lands: dropped, where `20af101` restored the previous project's
  B onto the restored A.

### O7. Tests

Both tests were written by one agent and then verified by another, which tried to refute every premise,
control and reference and added mutants of its own.
- **Test 70** (`testAbLevelMatchMemoryCarriesItsProvenance`, DSP): 62 checks, ~0.6 s. It covers:
  - the matcher's two bits;
  - capture and restore through re-prepare verdicts, including an audible restore of a stale record
    beside the same value injected as measured: bit-identical output, only the verdict differs;
  - the request word;
  - the rate stamp and the prime;
  - the tolerance;
  - the raw API;
  - P1b.

  The API does not exist at `20af101`. The HEAD-equivalent composite (every record measured, every
  restore current) fails 15; `20af101`'s engine behind a six-line shim fails 23.
- **State test 134** (`testLevelMatchAbSlotCarriesTheValidityOfItsResult`, processor): 135 checks,
  ~2.2 s, 63 heap processors. Against `20af101`, 29 fail: the flush claims, (c0) / (c1)'s own-value
  checks, and the pending switch at a session restore. Every keep, premise, route, probe and control passes
  there.
- **Existing tests** pass unchanged, except State test 132 leg (6), whose A/B pair now plays a 4 s
  pre-roll. Its premise was a flush-current capture.
- **Suites:** DSP 784 / 0 and State 5411 / 0.

**Mutants** (each the fix with one change, run through the new test and, where noted, the full suite):

| # | mutant | Test 70 | State test 134 |
|---|---|---|---|
| M1 | every record measured | 6 | 11 |
| M2 | `isResultCurrent()` recorded | 2 | 3 |
| M3 | no in-flight-duck term | 1 | 1 |
| M4 | no `measurementInputsDiffer` at the restore | 2 | 6 |
| M5 | no rate stamp | 2 | 2 |
| M6 | capture while a restore is armed | 1 | 3 |
| M7 | forget keeps the records | 1 | 2 |
| M8 | a pending switch's source replaced | 1 | 4 |
| M9 | `requestDuck` stores over a pending switch | 1 | 3 |
| M10 | the floor does not un-measure | 2 | 1 |
| M11 | a flush counts as measured | 1 | 1 |
| M12 | `setDisplayedGainDb` ignores `measured` | 15 | 21 |
| M13 | the bookkeeping gated on staleness again | 17 | 1 |
| M14 | re-arm only for an unmeasured record | 15 | 9 |
| M15 | the prime clears the word | 3 | 5 |

The verifiers' own variants:
- the record's state read from the pending snapshot;
- the applied gain snapped only for a measured restore;
- a retarget while armed ignored;
- a pending switch keeping its destination;
- `setDisplayedGainDb (v, false)` keeping the earlier bookkeeping;
- forget leaving an armed restore;
- the in-flight term without its switch-state test;
- the prime recording after it adopts;
- `rearm || ! measured`.

Each is rejected, after the verifiers extended the tests with (2f), (2g), (b'), W7's return and Copy, and
the (c0) / (c1) own-bottom checks. Two variants survive, by design:
- **Capture in the word that forgets.** The record would carry its own state and rate, so it could only
  restore as measured where it is a measurement.
- **The floor also marking a measured restore stale.** The floored value is the prediction a flush would
  publish in its first block, so the two verdicts play the same value.

### O8. Recorded, not changed

- **The trade-off.** A slot left before its measure confirms now flushes at a re-prepare right after the
  return. At *t* = 1.1–2.1 s, keeping cost less (0.26–0.71 dB·s against the flush's 1.40). So does a slot
  visited within ~2.7 s of any flush, State test 132 leg (6)'s old setup among them: that leg now gives
  its pair a 4 s pre-roll so its premise is the measure's. With switching faster than confirmation, a
  restored not-measured value never confirms, because nothing carries evidence across visits.
  Conservative; the cost is only at a re-prepare.
- **Rare conservative misses in free-running or burst processing.** Under those conditions a forced
  bottom can adopt a partly written slot, and the record then differs from the adopted state and is
  restored not current. Measured by the second review on the design-1 prototype, whose transport this
  implementation keeps: 1–2 in 120–400 threaded toggles.
- **The predict floor over a measured restore (audio).** Skipping the floor for a measured restore would
  keep the slot's level. That is an audible change, and a candidate.
- **Copy carrying the live record.** It would remove the flush and the lurch after a Copy. It changes
  what is restored, so it is a candidate.
- **Deferred by instruction:** cross-rate retention; a flush's quiet glide; the currency of automation;
  KI-029 and global non-finite ingress; the per-block ramp restart.

## P. The Devin review of `1c22d51`: "Level Match engages on stale compensation"

The finding is at `AnamorphEngine.cpp:1270-1271`. A live edit to something the measurement reads leaves
the result not current. Level Match is then turned on before the measure has caught up. At the bottom,
`p` and `pendingP` both hold the edit, so `measChangedAtBottom` is false, and Case A lands the published
value. The review proposed `&& loudness.isResultCurrent()`. Decided under the owner's authorization of
2026-09-25: **not a defect; the landing is kept** (ADR-0007, Note of 2026-09-25, stale engage). No
production behaviour changes (three comment edits, object code byte-identical) and the tests below.

### P1. Reproduction and trace (processor, then engine)

Processor at 48 kHz / 256: Haas 50 %, Drive 8, Output Gain 0, Level Match off, seeded noise; Width 1.0 →
2.0 by gesture at 6 s; the toggle 0.3 s later. Traced per block:

| moment | P (dB) | current | measured | applied (dB) | fresh (dB) |
|---|---|---|---|---|---|
| block before the edit | −5.5241 | 1 | 1 | 0.0000 | −7.8024 |
| edit block | −5.5241 | 0 | 0 | 0.0000 | −7.8025 |
| the toggle's bottom (block 1186) | −5.7656 | 0 | 0 | **−5.7656** | −7.8027 |
| bottom + 10 blocks | −5.8330 | 0 | 0 | −5.7798 | −7.8020 |

- **At the bottom.** `measChangedAtBottom` 0 and `procChanged` 0, so the landing predicate is true. The
  applied gain equals the value the bottom block publishes.
- **Current control** (the toggle 8 s after the edit): the result is current and measured, lands, and is
  0.001 dB from the fresh value.
- **Currency returns** 3.28 s after the edit (engine reproduction).

The engine reproduction is an independent harness driving `AnamorphEngine` directly. Its objects are
byte-identical to HEAD's. It gives the same trace, and it lands −5.7653 against a fresh −7.8027.

### P2. The question the finding raises

Is the landed value a **validity** error or the measure's **convergence**? A validity error would be a
value carried out of the context its analysis measured. The only fallback the rule offers is Case B's
unity, so the finding is a defect only if the landing plays something other than what Level Match on
throughout would play there.

Two lanes answer it. The same history with Level Match on from the first block publishes the same
trajectory:
- **Engine:** within 1.6e-5 dB at the landing in 85 of 85 rows. The run's and the twin's trajectories are
  bit-identical when the twin takes the same forced ducks, except with Multiband on (≤ 4.2e-4 dB, the H4
  reference switch).
- **Processor:** 665 engages, 505 of them landings on a non-current result.
  - HEAD's 500 ms mean error is never more than 0.07 dB above the twin's, and its peak never more than
    0.12 dB above.
  - In 73 rows HEAD is more than 0.07 dB *below* the twin (up to 0.79 dB), where the twin's own applied
    gain still trails its published value: up to 0.14 dB at the bottom after a Width edit, 0.15–0.51 dB
    after Drive and Mix cuts, and 2.10 dB 50 ms after a Drive 0 → 12 pre-duck (engine set).
  - The NaN self-heal rows lie outside the 505: the flush leaves the result current. They exceed the twin
    by up to +0.25 dB mean and +0.88 dB peak, where the twin's own flush happens with Level Match on.

The not-current value still partly describes the sound before the edit. That is what Level Match on
throughout is publishing too, so the observed error is the measure's own lag, not the landing's.

### P3. Every path that lands the applied gain (audit)

| # | path | lands | not current possible | context |
|---|---|---|---|---|
| L1 | `prepare()` (`:100`, `:114`) | unity | — | no published value |
| L2 | a kept re-prepare (`:175-176`) | the kept result | no: `keepMatch` requires currency | same state, same rate |
| L3 | an A/B restore (`:1375`, `:1413` → `adoptRememberedMatch`, `:682`) | the slot's record | yes (not measured) | **another** context, by design (#23); validity restored separately (the A/B provenance amendment) |
| L4 | the Case-A landing (`:1959-1960`) | this block's published value | yes | **the same** context, still converging |
| L5 | the silence→audio edge snap (`:1993-1994`) | this block's published value | yes | the same trajectory; after a host reset, the value carried across the re-armed analysis |

- `updateDerived` (`:1023`) and `:1958` set only the target (a glide).
- The NaN self-heal lands nothing.
- Only L3 lands a value from another context, and that is by design.
- **Option A's reach.** It changes only L4. L5 re-lands the value on any bottom that meets an audio edge,
  and after every host reset (HEAD and Option A bit-identical there).
- **Thread safety.** Reading `isResultCurrent()` at the bottom would be safe: same thread, a plain bool
  written only on the audio thread and the prepare / reset paths.

### P4. The options, measured

Setup:
- **Engine set:** 75 non-current landings.
  - Algorithms and modules: Haas, Velvet, Chorus, Dimension D, Multiband and Mono Maker.
  - Edits: Width, Drive, Mix, Haas Delay, Multiband band width and Mono Maker frequency.
  - Also forced engages and Undo of Apply.
- **Processor set:** 665 engages, 505 of them non-current landings.
  - Width edits at input correlations 0, 0.3 and 0.6 and at two Drives.
  - Algorithm changes; cuts of Drive, Mix, Amount and Width; Mono Maker off.
  - Edits in silence, host resets, Undo sequences, and A/B with a not-measured record.
  - Correlated, uncorrelated and anti-correlated programmes.
  - Its re-prepare and NaN self-heal rows leave the result current and fall outside the 505.
- **Metrics:** mean |applied − fresh| over 120 ms, 500 ms and 2 s after the bottom, and the signed peak.

**Option A (the review's guard; glide from unity when not current).**

| edit, engaged after | HEAD 120 ms / peak | A 120 ms / peak |
|---|---|---|
| Width 1 → 2, 0.3 s (the review's case) | 2.02 / +2.04 | 5.90 / +7.61 |
| Width 1 → 2 under Velvet, 0.3 s | 2.02 / +2.05 | 7.21 / +9.32 |
| Multiband band width 1 → 2, 0.3 s | 2.00 / +2.02 | 5.87 / +7.58 |
| Drive 0 → 12 (the predict pre-ducks), 0.05 s | 4.68 / +4.68 | 6.57 / +7.51 |
| Haas Delay 15 → 30 ms, 0.3 s | 0.02 / +0.09 | 3.70 / +5.34 |
| Undo of Apply after Width 1 → 2 and its Undo, 0.3 s | 0.22 / −0.31 | 3.63 / +5.33 |
| Drive 8 → 4, 0.05 s (quieter) | 2.33 / −2.34 | 1.36 / +2.99 |
| Mix 1 → 0, 0.05 s (quieter; fresh 0 dB) | 5.50 / −5.51 | 1.81 / −4.78 |
| Drive 12 → 0, 0.05 s (quieter; fresh +0.67) | 8.26 / −8.27 | 3.01 / −6.96 |

- **Engine set, 500 ms mean:** A worse in 54 of 75 rows. It is better in 21, all of them edits that make
  the sound quieter, engaged within ~1–3 s (Mix 1 → 0 is still better at 3 s).
- **Processor set, 500 ms mean, exact comparison:** HEAD better in 246 of 505, A in 225, 34 ties (with a
  0.05 dB tie band: 241 / 221 / 43).
  - A is also better in the positive class (anti-correlated Width 0 → 2 at 0.02 s: 7.22 → 5.62 dB;
    uncorrelated: 5.35 → 4.44).
  - In 149 rows A is better on both mean and peak without being louder.
- **Direction.** HEAD errs in the direction of the edit, by the twin's amount. A errs loud by the whole
  match wherever the match is a cut.
- **Test 66 against A: 6 checks fail.** Every Test 66 lane opens a dirty history duck in its first
  0.1 s. The result is current only 2.13 s after it, so at the 2.0 s event it is not current, 0.134 dB
  from settled. A turns that residual into a glide from unity: D_F +4.85 dB, the pre-O4g numbers.

**Option B (land only when measured).** A measured result is always current, so B is identical to A on
every non-current landing. It differs only on a current value the measure has not confirmed, a flush's
for example. There it refuses the landing: a +7.63 dB peak after a same-rate re-prepare, and O4g's own
Undo of Apply on a flush-current result in State test 130 (see P6).

**Options C and C2 (from the verification).**
- **C** lands a non-current value only at or below the level heard; otherwise it glides from that level.
- **C2** is the narrowest version: it caps the start at max(unity, Output Gain) when not current and
  above the cap.

C2 differs from HEAD only in non-current landings, and is never louder than HEAD at the peak:
- **Better:** 158 rows.
- **Worse:** 23 rows, all on the quiet side (down to −7.45 dB).
- **Suites:** DSP 3 failures, all in Test 66: leg (2)'s positive-match Undo of Apply (D_F −0.165 dB),
  leg (11), and the landing-liveness count. State 0.

### P5. Decision

**Keep the landing; Q4 stands**, re-examined against currency (ADR-0007, Note of 2026-09-25, stale engage).

**The invariant.** At a Case-A bottom the applied gain lands on the value that block publishes: the value
Level Match on throughout publishes there, current or not. Currency decides what a result may be
carried into (a re-prepare, an A/B record), not where the applied gain joins the published trajectory.
The landing validates nothing.

**Why.**
- The finding's premise holds for the code but not for the behaviour. The landed value is still partly the
  previous sound's, because the measure has not caught up. It is the value Level Match on throughout
  publishes, and the landing carries it into no other context.
- By the same argument a Case-B bottom's published value is the on-throughout trajectory too. Its unity
  start is kept by Q3, a decision on measurement (§K5, P3), not by this invariant.
- Every alternative measured either trades direction (A), refuses correct values (B, and A on trivial
  edits), or fails accepted coverage (A, B and C2 against Test 66 / State test 130).
- A reopens KI-031 on Undo of Apply after any recent measurement-input edit.

### P6. Tests and mutants

- **Test 71** (engine, 27 checks) and **State test 135** (processor, 23 checks).
  - **Premises, each asserted.**
    - Current before the edit, through the same-rate re-prepare verdict of Tests 69 / 70 and State tests
      133 / 134.
    - Converged there: the published value moved ≤ 0.02 dB over the 0.5 s before it. HEAD moves 0.002 /
      0.003 dB. A flush is current from its first block, so KEPT alone proves nothing (review finding).
    - The edit reached the engine: its block differs from the lane without it.
    - NOT current after the edit, at the engage and right before the bottom.
    - The bottom at event + 2, read from the twin's output.
  - **Claims.** The bottom block plays its published value (|D| ≤ 0.1 dB, ≥ 3 dB from unity, ≥ 0.5 dB
    from fresh). The on-throughout trajectory (≤ 1e-4 dB; ≤ 0.01 dB across a forced bottom's module
    restarts). The applied gain joining the on-throughout lane's. FLUSHED right after the landing.
    Recovery (KEPT 6 s later, within 0.1 dB of fresh). A new rate flushing.
  - **Controls.** Current (O4g), Output Gain (never stale), Case B (glides; FLUSHED after its fade-in), a flush's
    current value, Undo of Apply on a stale result (no swell), A/B with a not-measured record (not
    promoted) and with a measured one.
  - **HEAD:** DSP 811 / 0, State 5434 / 0.
- **Mutants**, each an engine variant run through both suites:

| mutant | DSP: Test 71 / Test 66 / other | State: State test 135 / 130 / other |
|---|---|---|
| M1 A: land only when current (`&& loudness.isResultCurrent()`) | 9 / 6 / 0 | 6 / 0 / 0 |
| M2 B: land only when measured | 10 / 6 / 1 | 6 / 18 / 2 |
| M3 the landing marks the result current (`setDisplayedGainDb (published, true)`) | 8 / 2 / 0 | 5 / 0 / 0 |
| M4 no landing (Case B everywhere) | 12 / 6 / 1 | 9 / 24 / 3 |
| M5 a landing blind to `measChangedAtBottom` | 1 / 2 / 1 | 1 / 2 / 2 |
| M6 C2: a non-current boost above the level heard starts at that level | 1 / 3 / 0 | 0 / 0 / 0 |
| M7 an ordinary duck's bottom does not report its measurement change (`&& pendingForced`) | 1 / 0 / 0 | 1 / 0 / 0 |

- **"Other."** M2, M4 and M5 also fail Test 68 leg (6). M2 and M5 fail State tests 131 (d) and 132 (7).
  M4 fails those two and State test 133's route (11).
- **Coverage.** Every variant is rejected by Test 71 or State test 135. M6 is rejected only by Test 71
  leg (4), the pinned positive step, and by Test 66.
- **M7 was a coverage gap.** The review built it: before the corrected Case-B verdict it passed both full
  suites (811 / 0, 5434 / 0). Tests 69 / State 133 cover only a forced bottom's report ((1p)). It now
  fails (E) in both tests, and nothing else.
- **Devin regression controls on this tree** (the state suite, each mechanism removed):

  | mechanism removed | fails |
  |---|---|
  | Apply's NaN guard | 6 of State test 129 (5 deterministic, plus the real-window leg C) |
  | Apply itself | 5 of State test 129, 2 of 132, 3 of 133, 2 of 135 and 45 of 130 |
  | the kept result as the applied gain | 30 of State test 132, 3 of 133, 4 of 134 |
  | the live-edit report | 24 of State test 133, 1 of 134, 7 of 135 |
  | `setDisplayedGainDb` honouring `measured` | 21 of State test 134, 2 of 135 |
- **Adversarial review of the tests and documents.** Two checks could not fail and were corrected: Case
  B's verdict (now after the fade-in) and the converged premise (now a stationarity window). (The
  mid-fade-in flush that made the first one vacuous was `duckMeasDirty` outliving its bottom: §Q.) Test 71's
  route check read two blocks, not one. The documents' bounds, tallies and set descriptions were
  corrected against the raw data.

### P7. Recorded, not changed

- **The positive stale step.** An edit that makes the sound louder while the match is boosting (a narrowed
  image widened, Drive 0) leaves a stale boost, and the engage steps up to it:
  - +6.00 dB above the level before, +10.12 dB over fresh −4.12 dB (Test 71 leg (4), anti-correlated,
    21 ms after the edit);
  - +4.25 dB on uncorrelated programme;
  - at correlation ≥ 0.3, at most 0.6 dB more than a glide.

  Level Match on throughout plays the same surge: the predict reads only Drive and Mix. The candidate is
  C2; it fails Test 66.
- **L5, the edge snap**, lands a non-current value too (after a host reset or an edit in silence), as
  designed.
- **A 5 ms boundary.** An edit and the toggle in one block give Case B; one block apart they give a
  landing.
- **Deferred by instruction:** cross-rate retention; a flush's quiet glide; automation currency; the A/B
  residuals of §O8; KI-029; global `toEngine` sanitization; float→int UB; F9, F10, F12; R6a–R6d;
  ScopeBuffer threading; the vectorscope stop-state; historical doc cleanup; the per-block ramp restart.

## Q. The Devin review of `d910a1e`: "Valid A/B gain lost on re-prepare"

The finding is at `AnamorphEngine.cpp:61-65` (`adoptsMeasChange`, `keepMatch`). An ordinary duck is
upgraded to a forced A/B duck during its fade-out. The bottom restores B's measured gain, but
`duckMeasDirty` stays set. A same-rate `prepare()` ~10 ms into the fade-in then reads the duck as still
carrying a change and flushes the gain to 0 dB. **A defect, fixed**: the bottom retires the flag it reports
(ADR-0007, Note of 2026-09-26, the duckMeasDirty lifecycle).

### Q1. Reproduction and trace (processor, then engine)

Processor at 48 kHz / 256: Advanced Mode, Haas 50 %, Width 1, Multiband on, Output Gain −3, Level Match
on, seeded correlated noise.
- Slot B (a Copy, Drive 12) is left converged at 10 s. Its record is −7.6162, measured, at 48 kHz, for the
  state it is restored into (a fresh processor at B: −7.6191).
- 6 s later, on A, Bands 4 → 3 opens an ordinary duck (block 3008). `abSwitchTo (B)` one block later
  upgrades it.

| block | switch state | `duckMeasDirty` | `pendingForced` | P (dB) | current / measured | applied (dB) |
|---|---|---|---|---|---|---|
| 3008 (the duck opens) | Normal → FadeOut | 0 → 1 | 0 | −5.5229 | 1 / 1 | −5.5203 |
| 3009 (the switch, upgraded) | FadeOut | 1 | 0 → 1 | −5.5229 | 1 / 1 | −5.5203 |
| 3010 (the bottom) | FadeOut → FadeIn | **1 → 1** | 1 → 0 | −7.6132 | 1 / 1 | −7.6160 |
| 3011 | FadeIn | 1 | 0 | −7.6100 | 1 / 1 | −7.6157 |

**The prepare.** A `prepareToPlay` before block 3012 (fade-in phase 0.38, 10.7 ms) finds `adoptsMeasChange`
true. The terms are:
- `duckMeasDirty` 1;
- `measurementInputsDiffer (p, pendingP)` 0;
- `primeMeasChanged` 0;
- the result current and measured;
- the record measured, same rate, same inputs.

It **flushes to 0 dB**. The next block applies the predict floor, −6.0097 dB: 1.61 dB off the fresh value,
0.74 dB mean over 2 s.

**The same, at other gaps and on other routes** (processor):
- **Every fade-in gap** (+ 1 … + 6 after the bottom block): flushed, 1.61 dB.
- **Before the bottom:** flushed. The armed restore lands in the first block after it (0.022 dB off).
- **After the fade-in:** kept. The flag is still set, but every reader gates on the switch state.
- **The causal probe.** The same `prepareToPlay` with only `duckMeasDirty` cleared by hand keeps the value,
  0.028 dB off. The record was valid and the result current; the flag's lifetime alone discarded it.
- **Controls.**
  - The switch alone: kept, 0.029 dB.
  - The duck changing no measurement input (Bands with Multiband off) plus the switch: kept.
  - The duck alone: flushed. That result is not current, so the flush is correct.
  - Multiband off with Width 1 → 2 riding the band count: flushed, 1.61 dB, like the main route.

**The other two readers in the fade-in:**
- A host reset `reset()` before block 3012 left the restored value NOT current. It re-reported the retired
  change, and a re-prepare before the measure re-confirmed it would flush.
- `abSwitchTo (A)` there recorded B as NOT measured. Back on B 1 s later, a `prepareToPlay` flushed, 1.60
  dB off. Clearing the flag by hand: current, and measured / kept, 0.023 dB.

**Engine reproduction** (Test 72's lanes, programme N): the same split. The pre-fix engine flushes in all
four fade-in gaps and applies −6.01 against a kept −7.59. It also flushes after the host reset and after
the switch away and back. The switch alone and the no-measurement duck keep.

### Q2. The lifecycle

What each reader asks — *has a measurement-input change gone live during this duck that nothing has
reported yet?* — and who writes it:

| site | role | before this change | after |
|---|---|---|---|
| ordinary entry (`setParameters`, :770) | set: `measurementInputsDiffer (p, np)` before `copyContinuous` | same | same |
| heard mid-duck, not forced (:881) | set (`\|= heard`), and `inputsChanged()` at once | same | same |
| forced entry (:758), fade-in re-duck (:809), `reset()` (:361) | clear | same | same |
| the bottom (:1263) | read: `measChangedAtBottom` → `inputsChanged()`, no Case-A landing, restore / injection re-arm (P1b) | read, **left set** | read, then **cleared** (:1270) |
| `prepare()` (:61-62) | read: no keep | read in FadeOut **and FadeIn** | FadeIn sees it only if a fade-in edit set it (then not current anyway) |
| `reset()` (:239) | read: `inputsChanged()` | FadeIn re-reported the bottom's change | same rule, nothing left to re-report |
| A/B record (:645) | read: not measured | FadeIn recorded a measured restore as not measured | measured iff the result is |

**The invariant.** The flag is set only while such a change has not been reported.
- **Before the bottom** it is the only carrier: the ordinary entry does not call `inputsChanged()`, so
  the currency bit still reads current.
- **At the bottom** it is reported (`inputsChanged()`) and consumed through `measChangedAtBottom`, which is
  block-local and read by the landing, the forced restore, the injection and the defensive consumer.
- **After the bottom** the currency bit carries it, or a restore's provenance (`setDisplayedGainDb (v,
  measured)`) replaces it.
- **Fade-in edits.** A heard edit in the fade-in sets the flag again and calls `inputsChanged()` in the
  same statement, so every fade-in reader sees *not current* through either answer.
- **Into Normal.** The flag can outlive the duck there. Every reader gates on `switchState != Normal`,
  and the next entry overwrites it.

Is anything left for the flag to say after the bottom? No: `reset()` already cleared it when it completed
a duck in the bottom's place.

### Q3. The strategies, measured

Each strategy is an engine variant, run through the probe of Q1 and both suites with Test 72 and State
test 136.

| strategy | Q1 probe (fade-in `prepareToPlay` / host reset / switch away) | Test 72 / State 136 | other suites | verdict |
|---|---|---|---|---|
| **A: retire at every bottom (adopted)** | kept 0.028 dB / current / measured, kept | 0 / 0 | 0 | the invariant; one assignment beside the report |
| A′: retire at a forced bottom only | the same | 0 / 0 | 0 | equivalent here; leaves a reported flag set after ordinary bottoms (below) |
| B: `prepare()`'s guard narrowed to FadeOut | kept, then its own `reset()` marks it NOT current / NOT current / not measured, flushed | 6 / 6 | 0 | a kept result that is stale on arrival; two readers still wrong |
| B3: all three readers narrowed to FadeOut | as A | 0 / 0 | 0 | equivalent; every present and future reader must gate a flag with no meaning left |
| retire at the upgrade | as A | 1 (G1) / 0 | Tests 66, 67 | loses the flag's re-arm and Case B across an upgrade |
| retire before the bottom reads it | as A | 1 (G1) / 0 | Tests 66 67 ×2 68 71, State 130 132 135 | loses the report |
| retire where the fade-in ends | as HEAD | 11 / 8 | 0 | the finding |

**A against A′.** An ordinary bottom that reports a change leaves the result not current. No fade-in is
long enough for the measure to confirm it again: at 16384-sample blocks the whole fade-in fits in the
bottom block and the result is still not current after it. So a keep never depends on the ordinary
bottom's flag. What does depend on it is a host reset in that fade-in:
- **A′ (and HEAD).** The reset reports the change a second time and restarts the confirmation. The
  result is current at block 3127 or 3129, depending on the gap.
- **A.** The confirmation lands at block 3126 from either gap, the block a lane without the reset
  reaches.

A is one rule for every bottom, and the rule `reset()` already follows.

**Decision** (owner authorization delegated, 2026-09-26): **A**.

### Q4. Audit of the paths (bounded)

- **`measChangedAtBottom`.** It is computed from the flag before the retirement and is block-local. Its
  four readers (the Case-A landing, `restoreAbSlot`, the injection, the defensive consumer) run in the same
  `process()` call. Unchanged.
- **`processingDiffers`, `measurementInputsDiffer`.** Unchanged, and neither reads the flag.
  `measurementInputsDiffer (p, pendingP)` is false throughout a fade-in: `p = pendingP` at the bottom,
  and a later snapshot either re-ducks (a discrete change) or is copied into `p` (continuous).
- **`keepMatch`.** The formula is unchanged. Its inputs after a bottom are now the currency bit and the
  prime.
- **Forced transitions.**
  - Entry clears the flag.
  - Tighten does not touch it.
  - A forced re-duck from the fade-in clears it; `takeRequests` runs first, so its record reads the
    retired state.
  - The bottom clears it.
- **Ordinary transitions.**
  - Entry sets it.
  - A retarget in the fade-out ORs in heard edits.
  - The bottom clears it.
  - Heard edits in the fade-in set it and report.
  - A re-duck clears it, then its copy marks it again.
- **The A/B upgrade** (an ordinary fade-out taking a forced request). It keeps the flag, and the bottom
  consumes it:
  - Test 72 (G1): the re-arm comes from the flag alone and holds;
  - Tests 66 and 67: Case B and the re-arm across an upgrade.

  There is no downgrade path: `pendingForced` clears only at a bottom or in `reset()`.
- **Host reset and `prepare()` before the bottom.** Unchanged; they read the flag and clear it themselves.
- **Threads.** The flag is audio-thread state. It is written in `process()` beside the report it retires,
  and read in `setParameters`, `process`, and `prepare` / `reset` (host-serialized with processing). There
  is no new state, no lock and no cross-thread access. The added store is one `bool`.

### Q5. Tests, mutants, controls, hashes

- **Test 72** (engine, 34 checks) and **State test 136** (processor, 18 checks). Premises, legs and
  numbers are in `TESTING.md`. Against `d910a1e`: 11 and 8 fail.
- **HEAD:** DSP 845 / 0, State 5452 / 0.
- **Nothing else moves.** On `d910a1e` and on this tree:
  - the full DSP output is byte-identical apart from Test 72;
  - the State output is identical apart from State test 136 and the thread-timing counters;
  - the 23 processor-route hashes and the 186 finite hashes are identical.

  So no existing test reads a re-prepare, a host reset or an A/B record in the fade-in after an ordinary
  duck carrying a measurement change: the coverage gap the finding lives in.
- **Devin controls on this tree** (the state suite, each mechanism removed; the 1 MB stack):

  | mechanism removed | fails |
  |---|---|
  | Apply's NaN guard | 6 of State test 129 |
  | Apply itself | 5 of 129, 45 of 130, 8 of 131, 2 of 132, 3 of 133, 2 of 135 |
  | the kept result as the applied gain | 30 of 132, 3 of 133, 4 of 134, 1 of 136 (the quiet resume) |
  | the live-edit report | 24 of 133, 1 of 134, 7 of 135 |
  | `setDisplayedGainDb` honouring `measured` | 21 of 134, 2 of 135, 1 of 136 ((F)) |

- **Load sensitivity (recorded).** With four suites run concurrently on this container, State tests 18
  and 39 (preset I/O with audio and a saving thread) failed once each, on the pre-fix binary as well.
  Rerun alone, they pass.

### Q6. Recorded, not changed

- **The fade-in heard edit.** It still sets the flag. That is redundant with its own `inputsChanged()`,
  and harmless, but a host reset in that fade-in restarts the confirmation, as before.
- **Deferred by instruction:** cross-rate retention; a flush's quiet glide; automation currency; KI-029;
  global `toEngine` sanitization; float→int UB; F9, F10, F12; R6a–R6d; ScopeBuffer threading; the
  vectorscope stop-state; historical doc cleanup; the per-block ramp restart; §P7's 5 ms boundary and
  positive stale step.

### Q7. The A/B residuals of §O8, re-measured (bounded; nothing changed)

Processor probes on this tree: seeded correlated noise, 48 kHz / 256, Haas 50 %, Output Gain −3, Level
Match on. Slot B is a Copy of A, edited at 4 s and left *t* s later; 6 s on A; back; `prepareToPlay` 4
blocks after the return. The error is |applied − fresh B| integrated over the 3 s after it.
- **KEEP** is an engine variant that restores every record as measured: the counterfactual.
- **Post mean** is the matcher's post-change mean (`postSum / postShare`) at the moment B is left.

**(1) A slot left before it is measured flushes where keeping can cost less.**

| route (confirmed after) | *t* (s) | HEAD (flush), dB·s | KEEP, dB·s | published at the leave, dB off fresh | post mean, dB off fresh (share) |
|---|---|---|---|---|---|
| Drive 8 → 12 (3.06 s) | 0.3 / 1.1 / 2.1 / 2.7 | 1.61 / 1.60 / 1.61 / 1.60 | 1.54 / 0.82 / 0.31 / 0.18 | 1.53 / 0.81 / 0.29 / — | 0.048 (0.03) / 0.004 (0.60) / 0.001 (0.87) / — |
| Drive 12 → 8 (3.30 s) | 0.3 / 1.1 / 2.1 | 1.46 / 1.45 / 1.46 | 1.44 / 0.99 / 0.34 | 1.94 / 1.06 / 0.38 | 0.081 / 0.027 / 0.009 |
| Drive 8 → 2 (3.78 s) | 0.1 / 0.3 / 1.1 / 2.1 | 0.46 / 0.47 / 0.46 / 0.46 | 1.85 / 1.73 / 1.50 / 0.60 | 3.32 / 3.14 / 1.76 / 0.64 | — (0.00) / 0.129 / 0.037 / 0.011 |
| Drive 0 → 8 (3.76 s) | 0.3 / 1.1 / 2.1 | 1.46 / 1.45 / 1.46 | 1.45 / 1.44 / 0.63 | — | — |

- **Neither policy dominates.** After a rise or a fall, keeping costs less, most at 1–2.7 s. After a
  cut the predict floor lands near the new level, so the flush wins at every *t*: a KEEP would play the
  old level up to 2.4 dB off.
- **Root cause.** The result is confirmed only when the gliding published value comes within 0.1 dB of
  the post-change mean, with that mean at least half the value. The mean itself is already
  0.03–0.13 dB from fresh by 0.3 s, and its share passes 0.5 by ~1 s. The glide takes ~3 s to arrive. The
  record carries the published value and one bit, not the mean.
- **Candidate** (not adopted; ADR-0007 already records the trade-off and the S6 candidate): record the
  post-change mean and its share with the slot, and restore the mean as measured once the share is at
  least 0.5. It would beat both columns on every row above from ~1 s. It changes what a restore and a
  re-prepare publish, so it needs an ADR amendment and the owner.
  *Adopted 2026-09-26 with slow-glide weights (§R).*

**(2) Switching faster than confirmation never re-confirms.** After Drive 8 → 12, toggling every *T*:

| *T* | B at the leave, dB off fresh | B's record measured | re-prepare after the last return |
|---|---|---|---|
| 0.3 s, 12 switches | 0.38 at 3.3 s | never | flushed, 1.59 dB·s |
| 0.6 s, 12 switches | 0.08 at 6.6 s | never | flushed, 1.60 dB·s |
| 1.0 s | 0.03 at 11 s | from the 5th visit | kept, 0.05 dB·s |
| 1.5 s, 2.0 s | — | from the 2nd visit | kept, 0.05 / 0.04 dB·s |

- **Root cause.** Every restore of a not-measured record calls `inputsChanged()`, which zeroes the
  post-change share. When the slots differ, the P1b re-arm also empties the integrators, so nothing
  accumulates across visits, and one visit needs ≳ 0.6 s at best.
- **Same candidate as (1).** Carrying the share and mean across visits would confirm after about two
  0.6 s visits. The ADR already records "evidence across visits" as not adopted.

**(3) Burst processing adopts a partly written slot.** A threaded harness: a free-running audio thread
calls `processBlock` while the main thread toggles `abSwitchTo`. Both slots are measured, and the check
is made 40 blocks after each toggle.
- **The failure.** `abSwitchTo` takes 120–270 µs on the message thread (one locked `replaceState`, then
  the raw values re-asserted one by one). `processBlock` rebuilds `toEngine()` from those atomics every
  block, and the free-running thread runs 9–10 blocks inside that window. The forced bottom, 2 blocks
  after the request, adopts a mixed snapshot. The restore is judged against it (`measuredFor ≠ p`) and
  comes back not current, and the rest of the writes land as live edits in the fade-in.
- **Measured.**

  | processing | toggles left not current |
  |---|---|
  | free-running | 59 of 60, and 291 of 300 |
  | ≥ 50 µs per block (≈ 75× real time) | 0 of 60 |
  | 200 µs or 1 ms per block | 0 of 60 |
  | real time (5.3 ms) | 0 of 60 |
  | free-running, audio paused around the call | 0 of 60 |

  The design-1 figure of §O8 (1–2 in 120–400) came from a different harness.
- **Cost.** Conservative: the restored value is still the slot's, and it plays. Only a same-rate
  re-prepare within the ~3 s before re-confirmation flushes it. This needs a GUI A/B click during a
  faster-than-real-time render.
- **Fixes.** Each changes the processor → engine handoff: the destination snapshot delivered with the
  request, or the bottom held until an apply generation matches. That is a threading-model change (a
  hard stop), so it is recorded, not changed. This round's fix is not involved: the mixed adoption
  happens before the bottom.

## R. The A/B residuals of §O8 and §Q7: O8(1) and O8(2) closed, O8(3) investigated (0.9.9)

Continues PR #156 from `3a779f5`. The owner delegated the decision ("You are authorized to make that owner decision
from the evidence and your own recommendation"); O8(3) is investigation only ("Do NOT implement a new Processor →
Engine threading/state handoff architecture in this round"). Decision record: ADR-0007, A/B provenance, revision of
2026-09-26. All of it is version 0.9.9, and the owner set the 0.9.9 release date to September 27, 2026 ("The 0.9.9
release date must be updated to September 27, 2026"): `CHANGELOG.md`'s `[0.9.9]` heading is the one authoritative
place that carries it.

### R1. Reproduction through the processor (head `3a779f5`)

A scratch probe (not committed) drives `AnamorphAudioProcessor` as State test 134 does: seeded correlated noise
(seed 134), 48 kHz / 256, Advanced Mode, Haas 50 %, Width 100 %, Multiband off, Output Gain −3, Level Match on.
Slot B, a Copy of A (Drive 8), is active from the first block and edited at 4 s; each visit to A lasts 6 s (2 s in
the cycle and visit scripts); `prepareToPlay` 4 blocks after the last return. A fresh processor at B from sample 0
is the reference. Recorded separately at each leave and return: the published value, currency and measured bits,
the glide-weighted post-change share and mean, the record and its bit, and the applied gain; after the return the
integrated |applied − fresh| over 3 s, and the re-prepare verdict.

**O8(1), one visit, Drive 8 → 12** (8 → 2 in brackets):

| left after the edit | published, dB off fresh | measured | post-change share | its mean, dB off fresh | re-prepare, then 3 s |
|---|---|---|---|---|---|
| 0.3 s | 1.532 (3.136) | no | 0.029 | 0.048 (0.129) | flushed 1.61 (0.47) dB·s |
| 0.6 s | 1.262 (2.642) | no | 0.307 | 0.027 (0.071) | flushed 1.59 (0.46) |
| 1.0 s | 0.894 (1.927) | no | 0.556 | 0.006 (0.042) | flushed 1.60 (0.46) |
| 1.1 s | 0.812 (1.760) | no | 0.603 | 0.004 (0.037) | flushed 1.60 (0.46) |
| 2.0 / 2.1 s | 0.321 / 0.288 (0.711 / 0.640 at the return) | no | 0.854 / 0.870 | 0.001 | flushed 1.61 (0.46) |
| 3.2 / 4.3 s | 0.086 / 0.026 | yes | — | — | kept 0.103 / 0.053 |

The return restores the value recorded, not current; the measure re-confirms it 2.51 s after the return (1.35 s
after a cut), whatever *t* was. The post-change mean is within 0.13 dB of fresh from 0.3 s and its share passes
0.5 by 1.0 s; the published glide needs ~3 s to reach it. **Root cause:** the record holds the published value
and one bit, not the measurement behind it.

**O8(2), repeated visits** (toggling every *T* from the edit on, 24 switches): at *T* = 0.3 s the share at every
leave is 0.274 — it restarts at each return — and B is never measured (still 0.111 dB off at 6.9 s; the re-prepare
after the last return flushes: 1.604 dB·s). *T* = 0.6 s: 0.307 then 0.482, never measured. The cycle leave 0.6 s /
back / 0.5 s / leave / back: the second leave's share 0.420, not measured, flushed (1.593 dB·s). **Root cause:**
every restore of an unmeasured record calls `inputsChanged()` (`setDisplayedGainDb (v, false)`), which zeroes the
post-change share; with the slots differing, the P1b re-arm also empties the integrators. Nothing carries across
visits, and a visit shorter than ~0.6 s can never confirm.

### R2. The measurement invariant

A read-only derivation of the bookkeeping (`LoudnessMatch.cpp`, the currency block), confirmed against a model of
the recurrences:
- `postSum / postShare` is exactly the mean of the counted blocks' post-only differences `P_k`, weighted as the
  published value weights its targets (the same `glideCoeff`). Silent blocks move neither.
- Exactly, `D = (1 − S)·L + S·M + R`: `L` the value at the change plus the uncounted steps, `M = Σ / S`, and `R` the
  counted blocks' stale colouring `g_k (T_k − P_k)`, the floor's jumps and the clamp's corrections. `R = 0` only with
  an empty snapshot (after `reset`, `prepare`, `softReset`). The live predicate tests `(1 − S)·|L_eff − M| ≤ 0.1`.
- `S` and `Σ` freeze while measured, and every path that clears `resultMeasured` resets them, so "measured implies
  current" holds structurally.
- Borrowing a recorded `S` into the live predicate would confirm with no new audio (and `S = 0` gives a NaN mean);
  restoring `M` with a GLIDE-weighted share certifies startup-dominated means, because a few fast-glide blocks reach
  half by themselves.

**The answer to the phase's question** — which piece of measurement state best represents the Level Match result
when a slot is left before the published value has converged — is the post-change evidence: the measurements of
the current inputs and their weight, not the published glide that still carries the previous state. Weighted by the
SLOW glide, its share is the live criterion's own "at least half", measured on its slowest path (0.62 s of counted
audio), so its mean is a measurement exactly when the live criterion would call it one.

### R3. Strategies compared

Six engine variants behind the same probe: three programmes (noise; noise amplitude-modulated 0.55 ± 0.45 at 0.7 ×
3.1 Hz with 20 % gaps; 80 ms bursts every 0.47 s with 50 ms tails), eight edits (Drive 8 → 12 / 2 / 9 / 7 / 8.3,
Width 1 → 2 / 1.05 / 0.95), nine leave times (216 routes), plus flush-origin first visits (0.1–1.5 s after the first
prepare), repeated visits (*T* = 0.3 / 0.6 s), and cycles. The full suites were run against S2 and S5.

| strategy | O8(1) | O8(2) | failure mode | full suites |
|---|---|---|---|---|
| S0 head | flushed ≤ 2.7 s | never | — | — |
| S1 glide-weighted mean at share ≥ 0.5, no carry | kept ≥ 1 s | 8 → 12: never at 0.3 / 0.6 s | none new | — |
| S2 S1 + carry | kept ≥ 1 s | from return 1–3 | certifies startup-dominated means after a flush or a 0 dB restore: 0.08–0.19 dB off (noise), 0.33 (modulated), 0.45 (burst) at 0.1–0.5 s | DSP 3 / State 7 failures |
| S3 mean at any share | kept from 0.3 s | from return 0–3 | after small edits on burst programme restores 0.35–0.38 dB off where head plays 0.16–0.21 | — |
| S4 carry only | — | from return 4–10 | slow | — |
| **S5 slow-weighted evidence, certified at 0.5, carried below** | kept ≥ ~0.9 s | from return 1–3 | none found | DSP 1 / State 3 (the intended changes) |

S5 against head on the 216 routes: 78 differ; the median integrated error after the re-prepare improves by
0.778 dB·s. Certified restores sit within the envelope of live confirmations on the same programmes (median / max
dB off fresh at the return — S5 certified: noise 0.007 / 0.041, modulated 0.032 / 0.083, burst 0.011 / 0.264; head
live-confirmed: 0.028 / 0.102, 0.045 / 0.219, 0.026 / 0.219). The burst maximum is the programme moving over the
6 s on A: at the leave the burst-programme evidence mean is within 0.07 dB of fresh from 0.9 s on (a time-series
probe, 0.1 s steps to 5 s). On burst programme after a rise the kept mean plays up to 0.15 dB·s more than the
flush (0.10–0.15 at four of five leave times; the predict floor lands near the level on that route) — the one
route-dependent cost, recorded in ADR-0007.
Flush-origin: S5 certifies nothing before 0.62 s of audio (0.1 / 0.3 / 0.5 s: flushed, as head), then 0.009–0.029 dB
off (noise) at 0.8 s. Visits (S5; head never): *T* = 0.3 s measured from return 3 (8 → 12, 8 → 2, noise), 2
(modulated); *T* = 0.6 s from return 1. Cycles: every head flush that S2 turned into a keep S5 keeps too, except
four where the carried evidence is still under half — 0.3 / 0.3 s on both programmes after a rise and after a
cut (noise 8 → 2, modulated 8 → 12 and 8 → 2) and 0.6 / 0.2 s after a cut (noise) — which stay flushed.

Costs: CPU — four multiply-adds per audible block while not measured; memory — 16 bytes in `LoudnessMatch`, 16 per
record; realtime — no allocation, lock or branch on anything but the block's own state; threading — none (the
evidence is captured and restored by `takeRequests` / `restoreAbSlot`, the audio-thread functions that own the
record); testability — the matcher's evidence is public (`getEvidence`, `restoreUnmeasured`), the engine's observable
through the published value and the re-prepare verdict.

### R4. Decision (S5)

Adopted under the owner's delegation. It is correct from the measurement contract because it certifies nothing the
live criterion would not: the mean of post-change measurements of the exact inputs and rate the record is restored
into, with at least the weight the slowest live certification requires — and the published value then IS that mean,
so the criterion's second half (`|D − M| ≤ 0.1`) holds with equality. It preserves every distinction the phase
names: currency (`isResultCurrent`) and provenance (`isResultMeasured`) unchanged in meaning; the published result is
the mean only when it is measured; the post-change measurement state is the evidence, recorded and restored as such;
the remembered value is still the value recorded, restored not current whenever the evidence is not a measurement;
the applied gain snaps to what is published. A remembered estimate is never made measured by being numerically
close: the value recorded never certifies anything.

### R5. Implementation

- `LoudnessMatch`: `Evidence { share, sum }`; `getEvidence()` (empty while measured); `restoreUnmeasured (db, e)`
  (the mean, measured, when `e.share ≥ kMeasuredShare` and the share and mean are finite; otherwise
  `setDisplayedGainDb (db, false)` and the evidence carried); the evidence decays by `coeffSlow` every audible block
  and adds `coeffSlow` / `coeffSlow · P_k` on counted blocks, beside `postShare`; reset in `inputsChanged`, `reset`
  and the floor's un-measure; `kMeasuredShare = 0.5` names the live criterion's half (bit-identical).
- `AnamorphEngine`: `AbMatchMemory::evidence`; `takeRequests` records it unless a duck in flight carries a live
  measurement change; `restoreAbSlot` passes it only when the record is valid (rate and inputs);
  `adoptRememberedMatch` restores a measured value as before and anything else through `restoreUnmeasured`, and
  snaps the applied gain to what is published (bit-identical wherever the published value is the record's).
- An empty evidence restores exactly what `setDisplayedGainDb (v, false)` did (Test 73 (1d)); measured records and
  injections take the unchanged path. The implementation's DSP-suite output was byte-identical to the S5 prototype's.

### R6. Tests, mutants, controls, hashes

- **Test 73** (the matcher and the engine, 56 checks) and **State test 137** (the processor, 54 checks): see their
  headers and TESTING.md. Every premise is asserted: the record not measured at the leave (a re-prepare there
  flushes), the edit live, the leave and the return (A ≥ 1 dB from fresh B; the bottom's value), the mean's
  representativeness (Test 73 (1a) against an exact target), each re-prepare (FLUSH exactly 0 dB, KEEP bit-exact),
  and each control event-matched (a 0.01 dB Drive move in the return's own turn; the same duck without the Width
  change; the same Copy with and without a measurement-input change).
- **Accepted assertions revised** (both stronger): Test 70 (3b)'s identity check — the double switch's bottom is
  now bit-identical to (3a)'s bottom and ≥ 1 dB from B's record, where it compared with A's recorded value within
  0.05 dB (A's record, unmeasured with 1 s of evidence, now restores its mean, −5.5105, against A converged at
  −5.5092 and the recorded −4.8005); State test 134 (B) 1.1 s — KEEP with the bottom ≥ 0.5 dB from the record and
  within 0.1 dB of fresh, and the kept value within 0.1 dB of fresh, where it asserted FLUSH.
- **Mutants** (each this tree with one change; failing checks in Tests 66–73 / State tests 134, 136, 137). All
  eighteen are rejected:

  | mutant | DSP | State | what catches it |
  |---|---|---|---|
  | M01 no evidence recorded (the head-equivalent composite) | 18 | 25 | Test 73 (2) and its premise's control, (3), (4a)'s control, (4c); Test 70 (3b); State 137 (A) (B) (C) (D2) (E) (F) (G)'s control; State 134 (B) 1.1 s |
  | M02 evidence recorded through a change in flight | 1 | 1 | Test 73 (4a); State 137 (G) |
  | M03 evidence restored for other inputs | 5 | 6 | the (3) / (B) controls, (4b), (C)'s control, (D1) |
  | M04 evidence restored for another rate | 1 | 1 | Test 73 (4c) at 44.1 kHz; State 137 (E2) at 44.1 kHz |
  | M05 certified at any share | 14 | 27 | the 0.3 / 0.6 s flushes, the visits, (1c), (C), (E); Test 70 (2d) (2e); Test 72 (F); State 134's not-measured lanes; State 136 (F) |
  | M06 glide-weighted evidence | 5 | 5 | (1a)'s fast glide, (3), (B); Test 70 (2d); State 134 (F)'s stale lanes |
  | M07 no carry | 14 | 13 | (1c), (3), (B) |
  | M08 the floor keeps the evidence | 1 | 0 | (1f) |
  | M09 a change keeps the evidence | 27 | 30 | (1a)–(1f), (2), (3), (A), (B) and more |
  | M10 a flush keeps the evidence | 1 | 0 | (1f) |
  | M11 decay only on counted blocks | 1 | 0 | (1c)'s formula |
  | M12 no finite-share guard | 1 | 0 | (1e) |
  | M13 the applied gain snapped to the value recorded | 0 | 1 | State 137 (F) (the first full-level block) |
  | M14 certified at 0.75 | 11 | 15 | the 1.1 s legs, the visit bounds, (1b), (1f); Test 70 (3b) |
  | M15 certified at 0.4 | 6 | 2 | (1c); Test 70 (2d); Test 72 (F); State 136 (F); (C) |
  | M16 the certified mean current but not measured | 2 | 0 | (1b), (1f) — **it survived its first run**: (1b)'s matcher was already measured before the restore, so a restore that never set the bit went unseen; (1b) now restores into a matcher neither current nor measured |
  | M17 the certified restore publishes the value recorded | 11 | 16 | the 1.1 s legs, the visits, (1b), Test 73 (2)'s premise control; Test 70 (3b); State 134 (B) 1.1 s |
  | M18 measured records restored through the evidence path | 33 | 37 | Test 70's measured keeps, (2)'s control and premise, and every measured leg |

  M06 first survived (1a) too: the 2× → 3× change keeps the glide slow through the counted blocks, so glide and
  slow weights coincide there; (1a) now adds a 12 dB change whose counted blocks glide fast.
- **Suites** (under `ulimit -s 1024`): DSP 901 / 0, State 5506 / 0.
- **The memcheck lane** (`sanitizers`). `311fa70`'s run was cancelled at its 45-minute cap inside State test 137
  under valgrind (Test 73 took 56 s there; locally, under the lane's flags, 56 s and 69 s). Both tests were made
  cheaper: slot B's edit at 3 s instead of 4 s, fresh lanes stopping at the last block read, and Test 73's
  later-return lane run only where it asserts — 43 s and 57 s locally. Each now asserts that B's result is
  MEASURED before its edit, with a control the same check fails on. Through the processor B is stale from the
  block-0 switch until the measure confirms it, so a keep by `prepareToPlay` the block before the edit is the
  measured bit (1.5 s in: flushed). At the engine neither a keep in place (the first prepare's flush makes B current,
  not measured) nor a keep after a return (a record not measured whose evidence reaches half comes back current
  too) shows it; what the return publishes does: B left the block before the edit for A at Drive 12 and back — the
  slots differ, so the return re-arms and the bottom holds what was restored — its record −5.4711 comes back bit for
  bit (the measured path; an evidence-certified record comes back as the mean) and a re-prepare keeps it; the same
  visit 1.5 s in publishes the mean −5.5151 against the record −5.2410, and with the edit at 1.5 s the premise
  fails. Two cuts before this one were wrong, each caught by an adversarial review: `fba78ec` moved Test 73's edit
  to 1.5 s behind an in-place re-prepare (B there is first measured 2.42 s in) and dropped State test 137 (A)'s
  0.6 s case (six checks); `4b2ed24` read the premise from a keep after a return, which an evidence-certified record
  passes too. Both are undone. `fba78ec`'s lane took 44:37. The lane had run 29–42 minutes on this PR's green heads, so its cap moves
  to 60 minutes (CI_CD.md, "Job timeouts"); command, suites and strictness unchanged.
- **What else moves**, against `3a779f5`: the DSP output is identical outside Test 73 except Test 70 (3b)'s line
  (the bottom now −5.5105, the evidence's mean, where it was −4.8048); the State output is identical outside State
  test 137 (thread-timing counters aside) except three places, all an A/B return of a record left not measured
  with a measurement's worth of evidence, all passing: State test 130 leg (6) (slot B → slot A, the same sound:
  D_F −0.000 dB against +0.012, the published value −6.157 against −5.816), State test 131 (a) (Drive 0 → 10: the
  bottom −7.263 against −7.127, the fresh destination −7.266; D +0.003 dB at 0 s against +0.140) and (b) (the
  sources that follow it, −6.154 against −5.961; every probe unchanged), and State test 134 (B) 1.1 s (revised).
  The 186 finite-value hashes are identical; 22 of the 23 processor-route hashes are identical, and S14 — A/B
  switches between slots that differ in Width and Drive, B left 2 s after its edit — moves (its final published
  value by 0.001 dB), for the same reason.
- **Devin controls** (each Devin mechanism removed from this tree; failing checks by State test; no new Devin
  finding this round): the NaN guard — 129 ×6; Apply disabled — 129 ×5, 130 ×45, 131 ×8, 132 ×2, 133 ×3, 135 ×2;
  the kept-result init — 132 ×30, 133 ×3, 134 ×4, 136 ×1, 137 ×1; the live-edit report — 133 ×24, 134 ×1,
  135 ×7, 137 ×16; `setDisplayedGainDb` honouring `measured` — 134 ×20, 135 ×2, 136 ×1, 137 ×22. Every
  control still fails the tests it was written against (129, 132, 133, 134 and 136 as in §Q5); State test 134's
  count under the last one is 20 where it was 21, because its (B) 1.1 s record is now certified by its evidence,
  a path that does not pass through `setDisplayedGainDb`.
- **Static checks:** `check-docs`, `check-realtime`, `check-dispatch`, `check-portability` and
  `check-state-coverage` pass; `check-citations` passes against `3a779f5` and `659ca0a` (76 anchors re-anchored,
  seven `DELIBERATE_REAIMS` targets re-derived) and its self-test passes. GCC with the gate's flags gives the same
  gated-warning sets as `3a779f5` on both test files, the engine, the matcher and the processor (the ungated,
  structural `-Wmismatched-new-delete` from `AllocationGuard.h` counts more sites, as every new allocation in a
  test does). `-fstack-usage`: Test 73 3,280 B (its largest leg lambda 8,016), State test 137 2,560 B; the engine's
  `restoreAbSlot` 64 → 80 B, `adoptRememberedMatch` 48 → 64 B, `LoudnessMatch::process` 512 → 544 B; the
  suites' largest frames grow by the engine's 48 bytes per automatic (DSP 290,208 → 290,304; State 711,824 →
  712,064), inside the 1 MiB guard both suites pass under.
- **PREfast** (the CI artifact of `311fa70` against `3a779f5`'s): `C6262` 180 → 180, none added or removed, every
  moved claim by an exact multiple of 48 bytes (the engine's growth per automatic); `C26495` identical; two new
  `C26498` on Test 73 (1e)'s `inf` / `nan` locals, made `constexpr` in the next commit (not suppressed).

### R7. O8(3), burst processing: investigated, not changed

A read-only audit of the write sequence against the pinned JUCE 9.0.2 source, and the round-§Q7 measurements.
- **The partial states.** `abSwitchToAdopted` requests the switch first, captures the leaving slot, then
  `applyStateSet` → `replaceState`: JUCE's `valueTreeRedirected` walks the new tree's PARAM children in its order
  (the APVTS adapter map's: `advancedMode` first, `width` last) and stores each raw atomic through
  `setValueNotifyingHost`; the re-assert loop after it normally changes nothing; the Bypass view value is written
  back last. `processBlock` rebuilds `EngineParameters` from 36 independent atomic loads every block. Nothing orders
  the two, so a snapshot can mix old and new values across any two fields (never within one). The mixtures that
  matter are semantic groups: `advancedMode` against the 21 fields it gates (a snapshot right after it can carry
  Advanced-off defaults neither slot holds), the algorithm against its own parameters, the band count and enable
  against the crossovers and band widths, `autoGainMatch` against `outputGainDb`, and a transient Bypass.
- **What the engine does with them.** The request is always taken in the block of the first new value (the CAS is
  sequenced before the parameter stores, which synchronise with the loads), so the leaving slot's record is never
  corrupted. The forced bottom adopts whatever the bottom block read; the restore is judged against it and comes
  back not current (the conservative answer); writes landing in the fade-in go live as edits (continuous) or
  re-duck as ordinary ducks (discrete).
- **Observable outside the harness?** Only when the host processes the ~6 ms fade-out faster than the write window
  (0.12–0.27 ms measured): faster than ~22–50× real time — an offline render with the editor open, a host that
  renders ahead on workers, one that splits a device buffer into back-to-back calls — or a message-thread stall of
  ≳ 6 ms inside the loop. Every route needs a GUI A/B click.
- **Contract.** No Accepted document promises a block-atomic snapshot; ADR-0036 states the opposite ("JUCE offers no
  block-atomic parameter snapshot and never has") and classes the duck consumed before the parameters move as "a
  masking miss, never a click". ADR-0007 already records O8(3). Two textual tensions: ADR-0004's "defer *all*
  params to the bottom" and `AnamorphEngine.h`'s "Call BEFORE changing the parameters" hold in letter only in real
  time; ADR-0036 §24 residual 5 / §25 item 5 give a lock-contention cause and a "two host installs" bound that the
  round-28 admission (the gate held across the whole body) appears to have made stale, and do not name the burst
  cause. Reported here, not edited (no broad historical cleanup this round).
- **Architectures compared.** (A) hand the engine the destination `EngineParameters` with the request: needs a new
  wait-free channel and still sees the half-written atomics after the bottom — dominated by (B). **(B) hold the
  forced bottom, capped, until the apply is complete** — variant B2: the existing request word becomes two-phase
  (requested, complete, with a sequence number in the free bits), the completion a release CAS, the per-block
  exchange an acquire; no new path, no lock, no wait; real time unchanged (the hold never engages when the writes
  finish inside the fade-out). (C) a seqlock snapshot: a data race without per-word atomics and fences, and a
  bounded retry that fails under preemption — rejected. (D) request after the writes: reintroduces unmasked A/B
  pops — rejected. (E) move the capture before the request: shortens the window, proves nothing. (F) judge currency
  at the fade-in's end: cannot tell late apply writes from a user edit — rejected.
- **Recommendation: B2**, as a future owner decision. It strengthens the ordering of an existing atomic and lets the
  bottom hold — a threading-model change and a hard stop under `ARCHITECTURE_REVIEW_GATE.md`, and a change to
  ADR-0004, ADR-0007's "one existing atomic, no new path", ADR-0036 §24, THREAD_MODEL.md and THREADING_POLICY.md.
  Until then the recorded disposition stands: conservative (the slot's value plays; only a same-rate re-prepare in
  the ~3 s before re-confirmation flushes it).

### R8. Copy, and the record's lifecycle (audited)

- **Copy.** `abCopyToOther` never reaches the engine: the destination's state moves, its record does not. The
  invariant needs no change here: the restore judges the record against the state it is restored into, so a Copy
  that moves a measurement input drops the evidence and restores the value not current (State test 137 (D1);
  State test 134 (d)), and one that leaves the inputs as recorded restores the evidence's mean, measured (D2).
  Carrying the source's live record with a Copy stays a candidate (it would need a new request in the word).
- **One provenance.** Every write of the record is in `takeRequests` (capture) or its forget (`AbMatchMemory {}`),
  and every read in `restoreAbSlot`. `measured` and `evidence` cannot drift apart: the evidence is taken from
  `getEvidence()`, which is empty whenever the result is measured, and cleared with `measured` under the in-flight
  rule. No second flag was added. Paths traced: `abSwitchTo` / `abToggle`, the prime, `prepare()` (does not touch
  the records), host `reset()` (an armed restore survives into the defensive consumer), Copy, preset load and
  undo / redo (never touch the records), `setStateInformation` (forgets them), and `injectMatchGainDb` (not a
  record; measured by the caller).
- **Side findings, not changed:** the `duckMeasDirty` comment in `AnamorphEngine.h` says the flag marks a change
  "made live", while the entry test also counts discrete fields still pending (the effect is conservative); a switch
  pending in the word before an off-thread session install could restore the previous project's record into the
  new project's state, measured only if its inputs and rate match (inferred, untested; the later forget cannot undo
  it).

### R9. Recorded, not changed (deferred)

- O8(3) (R7): recommendation B2, gated.
- Copy carrying the live record; the predict floor over a measured restore; cross-rate retention; the P1b
  re-arm's first blocks moving a measured restore 0.3–0.8 dB (all as ADR-0007 records them).
- The route-dependent cost of R3 (burst programme after a rise: the kept mean up to 0.15 dB·s over the flush).
