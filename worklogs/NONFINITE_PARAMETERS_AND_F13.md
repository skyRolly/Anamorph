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
  item with an audible, everyday trigger.
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
  **State test 129**: a real audio thread on NaN-laced input, Apply called the instant the published
  gain reads NaN. Pre-fix: Output Gain NaN, "nan" saved, output silent (3 checks); post-fix 0. The
  window is only reachable from a second thread and a serialised scheduler rarely reaches it
  (valgrind 1 of 4 runs, pinned CPU 1 of 8), so the window count is printed, not asserted — on such a
  run the leg is vacuous and says so. An earlier version asserted it and would have failed CI's
  valgrind lane on correct code; the synthesis caught it.
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
| **F13(1b) the Undo-of-Apply swell** (+4.0 / +4.8 dB, 128 ms) and the hand re-engage swell | §E3 | **owner decision** | changing the forced-bottom or engage behaviour of the match smoother is ADR-0007 / ADR-0004 territory; no test covers it yet |
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
| **F13(1b) Undo-of-Apply swell** | **owner decision — recommended next** | +4–5 dB above both endpoints on an everyday action; snapping at the forced bottom fixes the undo route (+0.05 dB) with both suites unchanged | an owner ruling (and a test for the swell either way) |
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
- **ASan + UBSan (Clang 18)** and **FMA contraction** (`-march=haswell -ffp-contract=on`): both
  suites passed with the non-finite guards, Test 64 (first 30 legs) and State test 128, 0 runtime
  errors; the final tree is covered by CI's sanitizer and arm64 lanes, not re-run here.
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
