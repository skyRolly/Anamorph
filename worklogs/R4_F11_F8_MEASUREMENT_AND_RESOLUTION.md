# R4 — Measure and resolve F11 and F8

**Baseline:** `main` at `bfd0e06` (the merge of PR #154). **Branch:**
`claude/anamorph-comprehensive-review-90tpty`. **Date:** 2026-09-21.

Both findings entered this round *implementation-Verified, impact-Unverified*
(`GLOBAL_REVIEW_v0.9.9_INVESTIGATION.md` §7). The round therefore began with measurement, and the
measurement changed both findings — one in its detail, the other in its verdict, twice.

---

## A. Baseline verification, and a defect in this round's own instrument

### A.1 The baseline

`main @ bfd0e06` builds clean (GCC 13, CMake + Ninja, Release, `-march=haswell -ffp-contract=off`
per ADR-0031) and is green: **396 DSP checks / 0 failures**, **4654 state checks / 0 failures**,
`check-realtime` 50 files / 0 violations, `check-dispatch` every dispatch bracketed,
`check-portability` 61 files / 0 violations, `check-docs` 146 files clean, `check-citations` 531
anchors intact.

### A.2 The instrument defect, and what it cost

**This must be read before any number below, because the first version of every engine-level
harness in this round was invalid, and one of them produced a refutation I published in chat.**

`AnamorphEngine::primeParameters` (`AnamorphEngine.h:87-92`) assigns `p` and `pendingP` and does
**not** call `updateDerived()`. `prepare()` is what calls it (`AnamorphEngine.cpp:144`). The
production order is therefore prime **then** prepare, which is what `PluginProcessor.cpp:232-233`
does and what the header comment at `:84` documents.

The first harnesses called `prepare()` first and primed afterwards. The consequence is specific and
silent: `prepare()` configured every module from **default** parameters, `primeParameters` then
replaced `p` without pushing it to the modules, and the `setParameters(p)` that followed hit the
steady-state no-change gate (`:465`) and skipped `updateDerived()` too. An engine set up that way
runs with default module settings until something forces an adoption.

That corrupts a control-vs-automated comparison asymmetrically: the **automated** run forces
`updateDerived()` on its first toggle and becomes correctly configured, while the **control** never
does.

What it cost, precisely:

| claim | status |
|---|---|
| F11's magnitude (−42.2 dB at 48 kHz / block 128) | **survived** re-measurement unchanged (−42.21 dB with a valid control) |
| F11's post-fix residual "+0.02/+0.03 dB" | **wrong** — the corrected figure is **0.00 dB, bit-exact** |
| "F8 REFUTED" | **wrong, and withdrawn.** F8 is confirmed; see §C |
| "`updateDerived()` is not idempotent" (an intermediate conclusion while chasing the residual) | **wrong** — it was the same instrument defect; `updateDerived()` run every block is bit-exact against running it once |

Every figure in this document was taken with the order corrected. The harnesses are
`scratchpad/f11_v2.cpp`, `f11_exact.cpp`, `f8_v2.cpp`, `algoreset.cpp` and `algoreset_prod.cpp`;
each carries the correction in its header comment. `f11_production.cpp` drove
`AnamorphAudioProcessor` and was never affected, which is why its agreement with the engine-level
figure was not the corroboration it appeared to be — two instruments agreeing does not make either
correct when only one of them can be wrong in the relevant way.

---

## B. F11 — confirmed, with two corrections to the finding

### B.1 What was measured

The engine is driven with a 1 kHz sine (L, 0.85·R) under Haas at amount 0.7, width 1.4, multiband
on with 2 bands. One discrete parameter is toggled every *K* blocks and the **output level** is
measured as RMS over a settled window, expressed in dB against the identical engine with no
automation at all. The primary probe is `dimMode`, which under Haas reaches no module, so the duck
is the only route by which it can change the audio.

### B.2 The finding is confirmed

| toggle interval | steady output vs control |
|---|---|
| 2.667 ms | **−42.21 dB** |
| 5.333 ms | −30.30 dB |
| 10.667 ms | −18.82 dB |
| 21.333 ms | −9.01 dB |

Confirmed through the production path as well — `setValueNotifyingHost` → APVTS → `toEngine` →
`setParameters` → `processBlock` — to within 0.03 dB. Nothing rate-limits a discrete parameter write
anywhere between the host and the state machine.

### B.3 Correction 1 — the cadence is a duration, not a block count

The finding says "faster than ~1 per 3 blocks". Block count is not the governing variable; the
**interval in milliseconds** is. A 2.667 ms interval reads −42.21 dB at 48 kHz / block 256, at
96 kHz / block 512 and at 192 kHz / block 1024 alike, to two decimals, because the fade lengths are
defined in milliseconds (`switchIncOut`/`switchIncIn`, `AnamorphEngine.cpp:87-88`). Audibility
thresholds, at 48 kHz:

| attenuation | interval at which it is first *not* exceeded |
|---|---|
| within 1 dB | ≥ ~115–130 ms |
| −3 dB | ~45–53 ms |
| −6 dB | ~29–32 ms |
| −20 dB | ~11–16 ms |

### B.4 Correction 2 — it is not perpetual

The finding says "holds the plug-in in a perpetual duck". It does not. Once the automation stops the
level is back within 0.5 dB in **8–10 blocks (21–27 ms)** at 48 kHz / block 128 — the fade-in's own
length. The defect is a sustained attenuation *while the lane moves*, not a latch. This matters for
severity: it is a level defect, not a dropout.

### B.5 Why the mechanism is real (state-machine trace)

The re-arm guard is
`if (switchState == SwitchState::FadeIn && (forceDuck || discreteDiffers (np, p)))`. It fires **only
from `FadeIn`**, and its body does not write `switchPhase`. So a lane that keeps crossing boundaries
restarts the ~6 ms fade-out from wherever the ~28 ms fade-in had reached, and the asymmetry of the
two rates is what sets the equilibrium level. A change arriving during `FadeOut` re-arms nothing at
all — it only retargets `pendingP`. (That second fact is §E.)

### B.6 The fix, and why this one

`dimMode` is read by exactly one line — `chorus.setDimMode (p.dimMode)` at `:603`, inside
`else if (p.algorithm == Algorithm::DimensionD)`. Under any other algorithm the value reaches no
module, so the duck has nothing to swap at silence: the whole cost, none of the purpose. The fix
narrows `discreteDiffers`' `dimMode` term to fire only when either side is Dimension D.

Three properties made this the right fix rather than the smallest one:

- **It is the designed trade, correctly applied.** For a change that genuinely rewires the graph —
  band count, algorithm, oversampling factor — ducking under fast automation is ADR-0004's
  deliberate choice, so stale tails clear at silence. Changing the *re-arm* would weaken that for
  every transition. Narrowing membership weakens it for none.
- **Membership has been narrowed before**, on exactly this reasoning: Bypass, Multiband Enable and
  Band Solo (ADR-0004), and the oversampling wrap's engagement (ADR-0035). This is the fourth entry
  in an existing pattern, not a new mechanism.
- **The test is "does the field reach a module", not "does the algorithm use it".** `haasSide` looks
  like a candidate and is not: `haas.setSide` at `:590` runs unconditionally. It was measured
  alongside the others and is unchanged.

Nothing is lost by not ducking: `sameParameters` still compares `dimMode` (`:269`), so the value is
adopted through the continuous path, and a later switch **to** Dimension D is an `algorithm`
difference that ducks, adopts the whole snapshot at the bottom and runs `chorus.setDimMode` with the
value already in `p`.

### B.7 Post-fix

Bit-exact. Zero differing samples against the un-automated engine at 44.1 / 48 / 96 kHz and blocks
64 / 128 / 512, at toggle cadences of every 1, 2 and 4 blocks. The five audible probes (band count,
algorithm, oversampling factor, Band Solo, `haasSide`) are **byte-identical** to their pre-fix
sweep output across 4 rates × 6 block sizes × 8 cadences; a `dimMode` lane under Dimension D still
ducks (−34.4 dB at one crossing per block).

---

## C. F8 — refuted, then confirmed. The refutation was mine and it was wrong

### C.1 The correction

Measured with the invalid harness of §A.2, F8 looked refuted: ratios of 0.92–1.18 against the
signal's own slew across 180 conditions. That result was an artefact. With the engine unconfigured
before the toggle the wet path was effectively identity, so `A(dry)` equalled the clean dry and
there was no step to find. **I published that refutation in chat. It is withdrawn.**

### C.2 F8 is confirmed, and the mechanism is exactly as the finding states

`dryAligned` is set only inside `if (mbActive)` and `mbActive` (`:1315`) is a **block** constant.
The Mix stage picks its dry source straight off that flag (`:1367-1368`) and blends
`dryL = cleanL + ts * (alignL - cleanL)` (`:1449-1453`) with `ts = 1` for any Mix ≥ `kAlignMix`
(0.05). So the dry term steps by `(1 − Mix) · |A(dry) − dry|` in one sample at the block boundary
where `mbActive` changes. On a **disable** that boundary is the first block after the ~12 ms blend
reaches 0; on an **enable** it is the blend's first sample.

### C.3 Magnitude

48 kHz, crossovers 200/900/3500 Hz, widths 1.6/0.6/1.5/0.7; "ratio" is the worst single-sample delta
over the transition divided by the same signal's own **median** delta in a steady window.

| case | step | ratio | after the fix |
|---|---|---|---|
| 1 kHz, 4 bands, Mix 0.05, disable | **+3.9 dBFS** | 18.1× | −18.6 dBFS, 1.3× |
| 100 Hz, 2 bands, Mix 0.05, disable | −6.2 dBFS | 51.8× | −36.7 dBFS, 1.6× |
| 100 Hz, 4 bands, Mix 0.25, disable | −13.4 dBFS | 23.7× | −37.1 dBFS, 1.5× |
| 100 Hz, 4 bands, Mix 0.25, block 64 | −1.8 dBFS | **88.8×** | −37.1 dBFS, 1.5× |
| 100 Hz, 4 bands, Mix 0.05, enable | −4.8 dBFS | 63.5× | −36.9 dBFS, 1.6× |

The step follows `(1 − Mix)` as predicted: at 1 kHz / 4 bands it reads 1.759 at Mix 0.05, 1.376 at
0.25, 0.898 at 0.50 and 0.419 at 0.75 — a ratio of 1.96 between the first and third against the
1.9 the law predicts.

Three conditions produce **no** step, before or after, and each is a control that identifies the
mechanism rather than a gap in it: **Mix = 0** (the smoothstep is 0, so `A(dry)` is never read — the
ADR-0005 null holds), **Mix = 1** (the H4 gate drops the dry term), and **one band** (no crossover,
so `A(dry) == dry`; it reads 1.43× both before and after).

White noise is a poor probe here and is recorded as such: its own slew is so large that every
condition including the one-band control reads ~3×. The tonal probes carry the result.

### C.4 The fix

Inside the loop that already crossfades the wet contribution on `mbEnableBlend`, glide the dry
reconstruction toward the clean dry on the **same per-sample value**. That moves the source switch
to the instant at which the two are equal: a disable reaches the `mbActive` boundary with the buffer
already holding the clean dry, and an enable starts the blend at the clean dry the previous block
was using. Four lines of arithmetic, no new state, no allocation, no branch in the Mix loop, and at
a settled blend the loop does not run at all — so every fully-enabled and fully-disabled state stays
bit-exact.

One consequence, recorded in ADR-0005: `A(dry)` is also the Level-Match reference (ADR-0007), so for
the ~12 ms of the fade that reference follows the dry actually being mixed rather than the pure
reconstruction. Outside the fade nothing changes.

### C.5 Why Test 23 did not catch it

`testMultibandEnableCrossfadeClickFree` runs at the default `Mix = 1`, where the dry term does not
exist. The defect lives strictly at `0 < Mix < 1` with more than one band — exactly the region the
finding named and the test did not enter.

---

## D. Part 6 — the `pendingAlgoReset` companion: **distinct mechanism, same family, confirmed**

**Verdict: distinct.** F11 is about a *re-arm* that does not reset the fade phase.
`pendingAlgoReset` is about a *derived flag* that is computed at three entry points and not
refreshed on a fourth path into the same variable. They share a family — state derived once at an
entry and not re-derived when the target moves — but not a mechanism, and neither fix touches the
other.

**Reachable.** The mid-`FadeOut` retarget (`pendingP = np` with no other effect) is the fourth path.
An ordinary two-step sequence — band count on one block, algorithm on the next — reaches the silent
bottom, adopts the new algorithm and skips `haas/velvet/chorus.reset()`.

**Audible, on one pair.** A module is processed only while it *is* the selected algorithm
(`:1281-1282`, and the `isModAlgorithm` gate at `:859`), so every other incoming module starts from
silence and a skipped reset costs nothing. Chorus and Dimension D are two voices of **one**
`ChorusEngine`, and there the incoming voice inherits a delay line full of the outgoing voice's
audio. Measured through `AnamorphAudioProcessor` with host-style parameter writes, against the
identical end state reached by the entry route:

| pair | block 64 | block 128 |
|---|---|---|
| Chorus → Dimension D | **0.587** | **1.099** |
| Dimension D → Chorus | **1.214** | **1.519** |
| Haas → Velvet / Chorus / Dimension D | 0.000 | 0.000 |
| Chorus → Haas | 0.000 | 0.000 |

on a 0.7-amplitude source. With the flag refreshed, every route is 0.000.

**Isolated, not inferred.** Two controls establish that the flag — not the arrival timing — is the
whole difference: a one-line experimental build that refreshes the flag makes the retarget route
**bit-identical** to the entry route; and narrowing the bottom-of-duck reset to one module at a time
attributes the entire difference to `chorus.reset()`, with `haas` and `velvet` contributing exactly
zero.

**Fix:** recompute the flag on that one path, with the same expression and the same reference the
other three use. It is placed in an explicit `else` so it cannot reach the forced-duck branches,
each of which sets the flag itself.

---

## E. Changes made

| file | change |
|---|---|
| `src/dsp/AnamorphEngine.cpp` | `discreteDiffers`: the `dimMode` term fires only when either side is Dimension D (F11) |
| `src/dsp/AnamorphEngine.cpp` | the `mbEnableBlend` crossfade loop also glides `A(dry)` toward the clean dry (F8) |
| `src/dsp/AnamorphEngine.cpp` | the mid-`FadeOut` retarget branch recomputes `pendingAlgoReset` (Part 6) |
| `tests/dsp_tests.cpp` | Tests 55, 56, 57 |
| `docs/.../ADR-0004-*.md` | Correction, 2026-09-21: the fourth exclusion, the measured cadence and recovery, the retarget flag |
| `docs/.../ADR-0005-*.md` | Correction, 2026-09-21: the dry source crosses over rather than switching; the ADR-0007 consequence |
| `docs/architecture/SIGNAL_FLOW.md`, `PARAMETER_REFERENCE.md` | synced to both corrections |
| `CHANGELOG.md` | `[Unreleased]` → `### Fixed`, three user-visible entries |
| 16 documents | evidence anchors re-aimed by `check-citations.py --fix` (54 anchors; the engine file grew by ~95 lines) |

**Not changed, deliberately:** the re-arm guard, `switchPhase`, the fade rates, `haasSide`'s
membership, the parameter registry, the serialization schema, the threading model, the DSP signal
order, and the reported latency. No `ARCHITECTURE_REVIEW_GATE` hard-stop item is touched — the gate
list names DSP *signal-order* changes, and neither fix reorders the graph. Both are amendments to
Accepted ADRs and are recorded as dated Corrections in those ADRs, which is what
`ARCHITECTURE_REVIEW_GATE` asks for.

---

## F. Regression protection

| test | asserts | pre-fix |
|---|---|---|
| **55** `testInertDiscreteChangeDoesNotDuck` | an inert `dimMode` lane leaves the stream **bit-exact**, at 3 rates × 3 block sizes × 3 cadences; plus three positive controls: an audible discrete change still ducks (< −20 dB), `dimMode` under Dimension D still ducks (< −20 dB), and a `dimMode` moved while inert is still **adopted** (its Dimension D tail matches an engine that carried the value all along, and differs from one that kept the old value) | **27 of its 27 lane checks fail** |
| **56** `testMultibandEnableDrySourceNoStep` | the worst single-sample delta over the whole transition is < 3× the signal's own median slew — both directions, 2 and 4 bands, Mix 0.05–0.75, blocks 64–1024, with the one-band case as a control | **20 of its 22 checks fail** |
| **57** `testAlgoResetSurvivesMidFadeRetarget` | a mid-fade-out algorithm retarget produces output **identical** to the entry route, over Chorus ↔ Dimension D plus three controls, at two block sizes and two retarget delays | **12 of its 20 checks fail** |

All three assert externally meaningful processing behaviour — output samples — not internal state.
The thresholds are derived from measurement: bit-exactness where the post-fix result is bit-exact,
and 3× where post-fix readings are 1.3–1.6× and pre-fix readings are 7×–89×. Nothing sits near a
bound from either side.

**Validation.** DSP suite **469 checks / 0 failures** (396 before this round; the 73 added are Tests
55–57 at 31 / 22 / 20 checks, of which 59 fail against the pre-fix engine), state suite **4654 / 0**, `check-realtime` 0 violations, `check-dispatch` clean,
`check-portability` 0 violations, `check-docs` 146 clean, `check-citations` 531 anchors intact.
Each new test was additionally run against the pre-fix engine and observed to fail, one fix at a
time.

**Not exercised, and said rather than implied:** macOS and Windows, VST3/AU/AAX hosts, `pluginval`,
the sanitizer lanes, and the Clang/GCC warning gates (they need their own build logs). Linux GCC 13
Release only.

---

## G. Remaining findings, and the road map

### G.1 Findings this round touched

| finding | verdict |
|---|---|
| **F8** | **Confirmed and fixed.** Impact now Verified: up to +3.9 dBFS, 89× the signal's own slew. My own earlier refutation is withdrawn — it was an instrument defect |
| **F11** | **Confirmed and fixed**, with two corrections: the cadence is an interval in ms, not a block count; and it is not perpetual (recovery 21–27 ms) |
| **F11 companion** (`pendingAlgoReset`) | **Confirmed and fixed.** Distinct mechanism; audible on the Chorus ↔ Dimension D pair only |

### G.2 The road map

| item | status |
|---|---|
| **R1 / R2 / R3** (F1, F5, F6) | done, round 53 |
| **R4** (F11, F8) | **done, this round** |
| **R5** (F4, F2, F3) — a save that lies, a flush that does nothing, a tail that under-reports | **proceed next.** Unchanged in scope. Three small independent diffs on `AudioProcessor` members other than `processBlock`, each with a clear completion test, no prerequisites. It is now the cheapest high-value work on the list. **Note the hard-stop boundary:** the reported *latency* is gated, the *tail* is not — confirm that reading before starting |
| **R6** (mechanical enforcement of hand-maintained invariants) | **proceed after R5**, and this round strengthens its case for the second time. `discreteDiffers`, `sameParameters`, `processingDiffers` and `copyContinuous` are four hand-maintained field lists over one struct, and this round found *two* defects in that family: a member that should not have been in one list, and a derived flag missing from one of four paths. Add to R6's work list: a lint that every `EngineParameters` field appears in `sameParameters`, and one that every write to `pendingP` is followed by the same derivation |
| **R7** (coverage for unexercised paths, F15/F14) | **proceed after R6.** One item is now partly addressed — Test 56 drives the multiband dry bank at partial Mix across four band counts and five block sizes, which was one of the named holes |
| **R8** (documentation pass, F16) | **proceed, batched.** The ADR-0004/0005 anchors and the two architecture documents are now current; the standalone pass (the `FUTURE_RISKS` rows, the `KNOWN_ISSUES` sync, the stale test counts, the dead `DELIBERATE_REAIMS` sweep) is untouched. **Nothing to add from this round:** every DSP test count in the documents sits inside a dated round record and is correct as written for its round, so the suite growing to 469 does not stale any of them |
| **F7**, **RISK-010**, **RISK-015**, the gzip hypothesis, ADR numbering | **preserve as no-action.** Re-read this round; nothing in R4's evidence bears on any of them |
| "F8 is refuted" | **reject.** Recorded here so the claim cannot be re-adopted from the chat transcript |

### G.3 New, from this round

| item | disposition |
|---|---|
| **The prime/prepare ordering hazard.** `primeParameters` followed by `prepare` is required, documented at `AnamorphEngine.h:84-92` and honoured by `PluginProcessor.cpp:232-233`. The reverse order produces a silently mis-configured engine with no diagnostic. It cost this round a published refutation | **investigate, R6-adjacent, low cost.** Not a product defect — the contract is documented and the one production caller obeys it. But it is a contract held by a comment, which is R6's exact theme. Cheapest credible fix: have `setParameters` treat the first call after `prepare` as an unconditional adoption, or assert the order in a debug build. **Do not** reorder `updateDerived` into `primeParameters` without measuring — `prepare` calls it after sizing every buffer, and that order is load-bearing |
| **White noise is not a step probe.** Its own slew swamps any single-sample discontinuity; the one-band control read the same ~3× as every defective condition | **preserve as a note.** Recorded so a future round does not repeat the measurement and read it as a null result |
