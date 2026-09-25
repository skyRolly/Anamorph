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
