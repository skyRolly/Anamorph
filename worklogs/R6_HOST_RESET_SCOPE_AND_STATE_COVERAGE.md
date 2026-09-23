# R6 — The host-reset scope, and mechanical enforcement for hand-maintained state lists

**Base:** branch `claude/anamorph-comprehensive-review-90tpty` at `1310b65` (PR #155).
**Version:** 0.9.9 (`CMakeLists.txt:14`). **Date:** 2026-09-22.

---

## A. Baseline

`origin/main` is `bfd0e06`. **PR #155 is open, not merged**, so R4's and R5's work is on this branch
and not in `main`. This round was explicitly authorised to continue on the branch rather than
restart from `main`; the unmet prerequisite was reported first and the round stopped until then. No
other development branch was read, fetched or used.

Local suites at the start of the round: DSP **469 / 0**, state **4677 / 0**. CI on `1310b65` was
green: `Build & Validate` run 35690826509 (the `push` event, which is the one that runs the real
matrix) concluded **success**, as did CodeQL, PREfast and dependency-review. The `pull_request`
run's matrix jobs read *skipped* by design — only `merge-check` runs there — which is why the
push run is the one cited.

Also done in this round, on instruction and before the engineering work: every unreleased entry was
folded into **`[0.9.9] — 2026-09-22`** and the `[Unreleased]` heading and its link definition were
removed (`1310b65`).

---

## B. The reported finding — "Host reset retains LoudnessMatch filter and energy state"

### Confirmed, independently, and quantified

R5 gave `AudioProcessor::reset()` a destination and drew the scope line with
`ResetScope::audioTailsOnly`. On that path `AnamorphEngine::reset` called **neither**
`loudness.reset()` nor `loudness.softReset()`, so for the matcher a host reset and no reset at all
were the same event.

That is not merely untidy, and the mechanism is specific: **the silence gate is judged from the
integrators themselves** —

```cpp
const double kSilence = 1.0e-6;
const bool silent = (meanSqDry < kSilence && meanSqWet < kSilence);   // LoudnessMatch.cpp
```

— against a τ = 0.4 s window. With stale energy in `meanSqDry/Wet`, `silent` reads **false** for
seconds of real silence, and MEASURE keeps gliding `displayedGainDb` toward a target computed from
pre-reset audio. ADR-0007's Consequences say "No drift on silence".

Measured through `AnamorphAudioProcessor` (Drive 8, Mix 1, Width 0.3, Amount 0.4; 3 s of noise, a
host reset, then pure silence):

| | worst movement over 1.4 s of silence |
|---|---|
| pre-fix (`audioTailsOnly` touches nothing) | **0.024158 dB**, still moving at 4 s |
| post-fix (`softReset()`) | **0.000000 dB** |

The magnitude is small. It is also unbounded within the window and is exactly the property the ADR
names, so it is a contract break rather than a rounding artefact.

### The suggested patch was not adopted as-is

Calling `loudness.reset()` on this path is the R5 mistake in reverse: the wholesale flush zeroes
`displayedGainDb`, `prevPredictedGainDb` and the published `matchGainDb`, which turns a converged
value into 0.000 dB on every transport stop — the "slammed loud on the next play" symptom in
ADR-0007's Context, and what State test 118's last leg already forbids.

The fix is **`softReset()`**: filters and integrators cleared, published result carried across. It
is not a new semantic — it is the one the duck bottom already uses when the processing changed
(`if (procChanged) loudness.softReset();`).

---

## C. Part 5's inventory — what else `audioTailsOnly` was getting wrong

The round's instruction was to inventory every component the scope touches and separate state that
must be **cleared**, **preserved**, or **recomputed**, rather than take the report at face value.
Doing that found a **second defect of the same class, in the same function, in the opposite
direction** — reported by nobody.

`AnamorphEngine::reset` called `levels.reset()` unconditionally. `LevelMeters::reset()` clears
`peakHoldL/R`, which `LevelMeters.h` documents as *"a held PEAK number (max sample peak since the
last reset, **never falls**)"* — and whose reset rule the same header states outright: *"on a number
click or a playback **restart**"*.

**Correcting my own first reading of this:** I initially wrote that the GUI click is its *only*
reset path, citing `PluginProcessor.cpp:443`. That is wrong twice over. There are **two** paths, and
:443 is not the GUI one: `src/gui/LevelMeter.h:27` is `mouseDown` on the readout, and
`src/PluginProcessor.cpp:442` clears it on a transport **play edge or a seek**. The correction makes
the finding *sharper* rather than weaker: the product's design is **hold through the stop, clear on
the next play**, and stopping in order to *look at* the number is exactly what the latch is for.
R5's override broke the half that matters and left the other half alone.

Before R5 this flush had one caller — `prepare()`, where clearing the meters is right — and R5's
override made it reachable from every transport stop.

Measured through the wrapper: a held peak of **−0.92 dB** became **−100.00 dB** on the stop.
Measured again with a real `juce::AudioPlayHead`, post-fix, so the other half is not taken on
inspection: **−0.92 dB kept** across the stop, **−33.98 dB** (the quiet material's own peak) after
the next play edge, and the same after a seek.

`correlation` was checked the same way and is running averages with no latch; it is display-only for
the same reason and is moved with `levels`. Both are written by the audio path and read only by the
editor — no gain or routing decision reads them back — and both decay on silence by their own
ballistics, so nothing is lost by not clearing them.

### The state inventory, and the one sentence it produced

| component | `everything` | `audioTailsOnly` | why |
|---|---|---|---|
| `haas` `velvet` `chorus` `multiband` `monoMaker` `soloMonitor` | clear | clear | delay lines and filter banks: audio |
| `os2/4/8`, the four delay rings, `prevInputSilent` | clear | clear | audio |
| the in-flight duck (`switchState`, `pendingP`, `dryDuck*`, `pendingForced`) | flush to target | flush to target | a duck describes audio that has stopped |
| `loudness` **analysis** (K filters, integrators) | clear | **clear** | audio that has stopped, and the silence gate is computed from it |
| `loudness` **result** (`displayedGainDb`, `prevPredictedGainDb`, `matchGainDb`) | clear | **preserve** | ADR-0007: on silence the measure holds the last trusted value |
| `correlation`, `levels` | clear | **preserve** | display-only, and the held peak is the user's latch |
| `scope` (`ScopeBuffer`) | — | — | has no `reset()` at all; out by construction |

> **Correction (§U).** The first row is true of `chorus`'s buffers and false of its wet blend and
> modulation depth, which `ChorusEngine::reset()` zeroes too. Those are the user's sound; a host
> reset now re-seeds them, as `prepare()` does (ER-DSP-09).

**The rule: a host reset clears AUDIO, and leaves DISPLAY and the user's own latches alone.**
`everything` still clears both, because a re-prepare really does invalidate every readout.

This was checked against the R5 failure mode the round named — *"a seemingly correct broad reset
accidentally cleared state that was intentionally required to survive"* — and the answer is that
both findings are instances of it, pointing in opposite directions.

---

## D. Why one observation proves four fields

State test 120's first leg asserts one externally visible thing: **after a host reset, feeding
silence must not move the published match gain at all.** That single observation fails if any of
four pieces of state is handled wrongly. Measured by driving `LoudnessMatch` directly through four
variants, 4 s of true silence after a 3 s convergence (Drive told to PREDICT 8 dB → floor
−4.0572 dB; real wet-vs-dry difference +2 dB):

| host-reset variant | at reset | after 4 s | max move |
|---|---|---|---|
| nothing (what R5 shipped) | −2.0673 | −2.0008 | 0.0666 |
| `softReset()` (what R6 ships) | −2.0673 | −2.0673 | **0.0000** |
| integrators cleared, **filters left warm** | −2.0673 | −2.0202 | 0.0471 |
| `softReset()` + `prevPredictedGainDb` zeroed | −2.0673 | −4.0572 | 1.9898 |
| `softReset()` + `displayedGainDb` zeroed | −2.0673 | +0.0000 | 2.0673 |
| `reset()` (the wholesale flush) | +0.0000 | −4.0572 | 4.0572 |

Row 3 is the one that decided the test's design: **warm biquads ring into the silence** and push
even freshly-cleared integrators back over the gate, so the silence-hold observation is a real test
of the K-weighting filter clearing and not only of the integrators. Rows 4 and 5 are why the
published half must survive — zeroing `prevPredictedGainDb` lets the PREDICT floor slam the gain
down on the very next block.

The test's parameter set was chosen from measurement, not taste: at Drive 8 / Mix 1 / Width 0.3 /
Amount 0.4 the chain converges to **−2.76 dB** while the PREDICT floor sits at **−4.06 dB**, a
1.30 dB margin. At the more obvious Width 1.4 / Amount 0.8 the measurement converges *below* the
floor, `min()` is a no-op, and rows 4 and 6 would silently test nothing.

---

## E. R6's changes

### Code

`src/dsp/AnamorphEngine.cpp`, in `reset (ResetScope)`:

```cpp
if (resetScope == ResetScope::everything) loudness.reset();
else                                      loudness.softReset();
...
if (resetScope == ResetScope::everything)
{
    correlation.reset();
    levels.reset();
}
```

`src/dsp/AnamorphEngine.h`: the `ResetScope` comment now states the AUDIO/DISPLAY rule.

Nothing else in the function changed. The scope was **not** broadened: the two edits move exactly
two components, each in the direction the inventory decided, and `everything` is untouched.

### Test

**State test 120** — *a host reset clears audio and keeps the user's state* — three legs:

1. **Level Match.** A control leg (no reset → the gain drifts, which is also the pre-fix behaviour
   of the reset path); then the assertion (host reset → the published gain is carried across
   `exactlyEqual`, and 1.4 s of silence moves it by < 1e-6 dB).
2. **`everything` still flushes the matcher** — `prepareToPlay` zeroes the published gain.
3. **The held peak** — a transient, a host reset, the held peak unchanged; then `prepareToPlay`
   clears it.
4. **The other half of the latch's contract** — a real `AudioPlayHead` stub: the peak is kept
   across a transport stop, and still cleared by the next play edge and by a seek. Without this leg
   a narrowing that kept the peak by *breaking* the play-edge clear would pass every other check.
   It is also the first test in either suite to drive an `AudioPlayHead`, which is one of R7's named
   holes; the rest of the transport machine is still uncovered and stays R7's.

Against the pre-fix engine the test reports:

```
  Level Match converged -2.7638 dB; without a reset it moves 0.023005 dB over 1.40 s of silence
  after a host reset the gain moves 0.024158 dB over the same silence
  [FAIL] R6: a host reset clears the analysis state -- silence moves nothing (ADR-0007)
  held peak -0.92 dB -> after a host reset -100.00 dB -> after a block -100.00 dB
  [FAIL] R6: a host reset leaves the user's held peak alone
  [FAIL] R6: a transport STOP keeps the held peak
  transport: -0.92 dB held across the stop, -33.98 dB after the next play
```

— three failures, and leg 4's play-edge and seek checks pass on both engines, which is the point of
having them: they pin the behaviour the narrowing had to leave alone.

and post-fix all thirteen checks pass. Its stack frame is **544 bytes**, so the suite maximum is
unchanged at 709,760 B (`testSettingsPublicationIsFieldLevelAndOrderedByObservation`) and the
1 MB-stack parity step's documented figure stays correct.

### Gate

**`scripts/check-state-coverage.py`** — see §G.

---

## F. Parts 6–7 — the mechanical-enforcement candidate inventory

The recurring pattern the global review names is *a correctness invariant held by a hand-maintained
list that nothing compares to its subject*. Every candidate below was evaluated on the same axes.

### C1 — `EngineParameters` × `sameParameters` (36 fields, total coverage) — **IMPLEMENTED**

- **Invariant:** `sameParameters` must compare every field. Its own comment says so.
- **Current enforcement:** the comment. Nothing else.
- **Failure mode:** a field added to the struct and forgotten here has its edits **ignored whenever
  nothing else moves** — the parameter silently stops working.
- **Existing coverage:** none. The state suite enumerates `getParameters()` for round-trips, which
  is a different question (does the value persist), not this one (does the edit reach the engine).
- **Protection:** require the exact comparison form `a.X == b.X` / `sameF (a.X, b.X)`, per field.
- **False positives:** none possible — the requirement is total and the function's shape is fixed.
- **Maintenance:** one line per new field, which is the line that was being forgotten.
- **ADR:** none needed (see Part 11 below).

### C2 — `EngineParameters` × `discreteDiffers` / `processingDiffers` / `copyContinuous` — **IMPLEMENTED**

- **Invariant:** each is a *selection*, so the invariant is not coverage but that a **decision
  exists** for every field and the code matches it.
- **Failure mode, measured:** R4 found `dimMode` ducking when no module could observe the change
  (a field in a list it did not belong in) and `pendingAlgoReset` set on one of two paths.
- **Protection:** one declared `DISCRETE` partition, cross-checked against `copyContinuous` in both
  directions, plus per-function exclusion tables carrying the reason the source already gives.
- **False positives:** a new field always fires until classified. That is the point, not a defect.
- **Maintenance:** one classification per new field; two words for a new exclusion.

### C3 — `AnamorphEngine` modules × `reset (ResetScope)` — **IMPLEMENTED**

- **Invariant:** every DSP module has a decided answer for each scope.
- **Failure mode, measured:** both of this round's findings, in the same function, in opposite
  directions.
- **Protection:** a derived subject (members whose type declares its own `reset()`), a declared
  answer per module, and a structural check of where the call sits relative to the
  `ResetScope::everything` guard.
- **Gap, stated:** **scalar** engine state (`dryDelayWrite`, `pendingForced`, `prevInputSilent`) is
  cleared by assignment, not by a call, and is not covered. ER-DSP-07 was exactly such a scalar. It
  stays covered by tests (State tests 57, 118–120), and widening the subject to all 62 members would
  produce a 50-row table of `never` that nobody would read.

### C4 — the stack-local rule (Part 9's specific question) — **NO ACTION, with the reason**

- **Invariant:** the total stack frame of any one test function must fit the Windows main thread's
  1 MB.
- **Would a grep rule represent it?** **No, in both directions, and the repository's own evidence
  says so.**
  - *Not necessary:* one stack-local `AnamorphAudioProcessor` is ~138 kB — 13 % of the reserve and
    perfectly safe. A textual ban would reject correct code.
  - *Not sufficient:* the overflow comes from a **frame**, not a token. `tests/dsp_tests.cpp`
    declares `anamorph::AnamorphEngine engine;` as an automatic in dozens of tests and its largest
    frame is 289,440 B with no `AnamorphAudioProcessor` anywhere in the file. And State test 59's own
    note records that **splitting the legs into separate functions was measured not to help** —
    the compiler inlines them back into one frame — so the rule is not even per-source-function.
- **What already enforces it, semantically:** the `linux` job re-runs **both** suites under
  `ulimit -s 1024`, and the `sanitizers` job catches the ASan-inflated case (which is how this
  round's predecessor's regression was caught, before merge). Between them the class is covered at
  the real invariant.
- **Decision:** adding a grep rule here would be a checker whose assumptions are weaker than the
  invariant it claims to enforce — the exact thing this round was told not to build.

### C5 — R6a, a lint asserting every state-replacing entry point takes a `StateCommandGate` — **DEFERRED, with the reason**

- The invariant currently **holds at 14/14**; the risk is a future fifteenth entry point.
- There is **no measured defect** in this class, unlike C1–C3, and `check-dispatch.py` already
  enforces a structurally identical rule for the parameter-dispatch bracket, so the shape is proven
  but the need is speculative.
- Deferred rather than refuted: it remains the correct next gate if a fifteenth entry point is ever
  added. Adding two lints in one round doubles the review surface for one round's evidence.

### C6 / C7 — `gestureActionDepth`'s four writers; `processingDiffers`' two questions — **DEFERRED**

Both are **behaviour** changes to shipped state machinery rather than enforcement, and the road map
itself says to defer them if they need an ADR amendment. Neither was measured to be wrong this
round, and neither is what "mechanical enforcement" means. Filed, not started.

### The alternative that was considered and not taken

A **runtime** test would be stronger than a lint for C1: enumerate `proc.getParameters()`, move one,
and assert the engine's adopted snapshot followed. It was rejected on three measured obstacles — the
duck defers adoption, a memberwise comparison needs the very field list under test, and a byte
comparison is defeated by struct padding — and because it covers none of C2 or C3. It is recorded
here as the honest strengthening, not manufactured as a finding.

---

## G. `scripts/check-state-coverage.py`

One lint, two targets, the `check-dispatch.py` shape: module docstring stating what it claims and
what it cannot see, the same apostrophe/comment stripper the other two lints carry, `::error
file=,line=` annotations, and a `--self-test` that runs first in the job.

**What it claims, precisely:** (1) every member of the subject has an answer in the declaration
table; (2) the code agrees with that answer. It does **not** claim the answers are right — the
judgement stays with the reviewer, the ADR and the tests. The docstring says so in those words.

**Why a text scan is right for target 1:** all four functions are single-expression enumerations
with a fixed shape, and the lint looks for the exact **comparison**, both operands pinned, not for
a field name anywhere in the body. `a.mix == b.width` satisfies neither `mix` nor `width`.

**The obvious objection, answered in the docstring:** the table is a list too. It is one list
instead of four; it is cross-checked against `copyContinuous` exactly, in both directions; and it is
total over the struct, which is how every measured defect in this class arrived.

**Validation, in both directions, against the real tree** (not only synthetic input):

| violation reintroduced | result |
|---|---|
| `loudness` left out of the host-reset path (the reported finding) | fires, names `loudness`, exit 1 |
| `levels`/`correlation` cleared on the host-reset path (the found one) | fires, names both, exit 1 |
| a new continuous field added to `EngineParameters` | fires on `sameParameters`, exit 1 |
| a new **discrete** field, classified but missing from two lists | fires 3×, exit 1 |
| the tree as shipped | `36 EngineParameters field(s) answered by 4 list(s), 12 engine module(s) answered for both reset scopes`, exit 0 |

**Self-test: 64 cases**, in both directions, including both Round-6 reset defects in their original
form, the parser cases that would make the lint fail *open* (an access specifier eating the next
member, a brace initialiser hiding three, a digit separator swallowing the file, an `enum class`
read as a resettable type), and the cases an over-eager revision would flag.

Wired into `source-lint` (self-test immediately before the lint, per `TESTING_POLICY` rule 4) and
into `scripts/preflight.sh` (seven checkers → eight).

---

## H. Part 10 — cross-check against what already exists

| candidate | already covered by | verdict |
|---|---|---|
| C1 `sameParameters` totality | nothing — no lint, no compiler diagnostic, no test | new gate justified |
| C2 the three selections | nothing | new gate justified |
| C3 reset × scope | State tests 57 / 118 / 120 cover *instances*, not *coverage* | new gate justified |
| C4 stack locals | `linux` `ulimit -s 1024` (both suites) **and** `sanitizers` | **already enforced; no action** |
| C5 `StateCommandGate` | `check-dispatch.py` enforces the structurally identical rule elsewhere | deferred |
| — | `-Wshadow`, `-Wfloat-equal` and the two version-fenced warning baselines | unrelated axes |

No existing gate's success condition was changed, weakened or disabled.

---

## I. Part 11 — ADR and policy boundaries

`ADR_POLICY` rule 1 makes an ADR mandatory for nine categories. **None is altered:** not DSP signal
flow (stage order is untouched), not parameter semantics, not the threading model, not format
support, not build architecture, not state serialization, not latency, not oversampling, and no DSP
algorithm was replaced. The `ARCHITECTURE_REVIEW_GATE` list is likewise untouched, and no Accepted
ADR is contradicted — ADR-0007 is **extended**, in the dated-correction form this repository already
uses, because its R5 note said "deliberately leaves `loudness` alone" and that was half wrong.

**No new ADR.** Documentation synced instead, per `DOCUMENTATION_LIFECYCLE_POLICY`:

- `ADR-0007` — *Correction, 2026-09-22*: the analysis/result split, the six-row measurement table,
  the AUDIO/DISPLAY rule, and the `levels` finding.
- `THREAD_MODEL.md` — the **Host reset** row now carries the scope rule and points at the
  *Meter hold reset* row for the latch's real owner.
- `CI_CD.md` — the source-lint job row, the lint description list and the local-reproduction block.
  While there: **`check-dispatch.py` had never been added to that description list** (the R8 road-map
  item "CI_CD.md's missing lint"). Drift reported and the smallest correction applied — it is now
  entry (d), marked with the date it was added to the document rather than to the job.
- `TESTING.md` — the `source-lint` reproduction row.
- `CHANGELOG.md` — two `Fixed` entries under `[0.9.9]`, both user-visible with their measured
  numbers. The lint gets none: the changelog records user-visible change, and a CI gate is not one.

---

## J. Part 12 — validation

**Ran locally, on this tree:**

- `scripts/preflight.sh` → **exit 0**. `check-docs` 148 files clean; `check-portability`,
  `check-realtime`, `check-dispatch`, `check-state-coverage` (64 cases) and `check-citations`
  (242 cases) all self-test green and then run clean; `check-linux-abi --self-test` 19 cases;
  `setup-llvm-apt --self-test` 9; `run-pluginval --self-test` 22.
- **DSP suite 469 / 0**; **state suite 4690 / 0**, up from 4677 — State test 120's **thirteen**
  checks, measured by running the suite with and without it rather than by counting `check(` calls.
- Both suites green under **`ulimit -s 1024`**, the Windows-parity stack.
- `check-citations` re-anchored the documents twice, because the engine edit shifted
  `AnamorphEngine.cpp` by a uniform **+61** lines past line 199 (the second pass followed the
  comment correction in §C). Every **glossed** anchor was then re-derived and verified **by hand**
  against the final source, per the standing rule that `--fix`'s line map may not be trusted for a
  gloss — `process` at 947–1875, `levels.input.process` 1128, `applyInputConditioning` 1204,
  `dryScratch` 1220–1221, `applyWidth` in 1386–1400, `monoMaker.process` 1638,
  `loudness.process` in 1650–1655, `soloMonitor.process` 1784, `bypassBlend` in 1838–1863,
  `scope.pushBlock` in 1865–1874 — and each range boundary lands on the comment or brace its label
  names.
- Compiler diagnostics on every touched TU: clean. The four `-Wfloat-equal` sites the first draft of
  State test 120 introduced were rewritten to `juce::exactlyEqual`, the repository's own idiom.

**NOT run locally, and named rather than implied:**

- The two warning gates need a build log from the **pinned** major (gcc-16 / clang-22); locally
  only gcc-13 / clang-18 exist, so only their self-tests ran.
- The Linux ABI floor needs linked, stripped artifacts.
- **No platform other than Linux was exercised.** Windows, macOS, macOS-Intel, the sanitizer, TSan,
  fuzz, realtime and pluginval lanes are CI's, and nothing here claims their result.

---

## L. Follow-up, same round: the reset left the transport edge detector stale

A second review finding on the same entry point, investigated after the above shipped and
**confirmed** — but the suggested reading of it needed correcting, and the fix is not the obvious one.

### The finding, and what it actually is

`PluginProcessor::reset()` cleared the engine's audio tails and left `prevPlaying` and `prevPosValid`
alone. The held peak is cleared on a "playback restart", which `processBlock` finds as the rising
edge `playing && ! prevPlaying`. **An edge needs a falling half**, and the falling half exists only
if the plug-in SAW a non-playing block.

The host class this override was added for is precisely the one that sends none. VST3
`setProcessing(false)` calls `reset()` and then stops calling `process`
(`juce_audio_plugin_client_VST3.cpp:3475-3479`); `setProcessing(true)` simply resumes; and neither
side re-prepares — `preparePlugin` is called with `CallPrepareToPlay::no` at `:3469`. AU `Reset()`
is the same shape (`juce_audio_plugin_client_AU_1.mm:255-263`). Anamorph ships VST3, AU and
Standalone (`CMakeLists.txt:413-418`), so both plug-in formats are affected.

### Measured, before deciding anything

Through `AnamorphAudioProcessor` with a real `juce::AudioPlayHead`, a held peak of −0.92 dB,
`reset()` with **no `processBlock` in between**, then a resume:

| lifecycle | held peak after the resume | |
|---|---|---|
| resume where the transport left off | **−0.92 dB** | restart MISSED |
| resume at the last block's own start | **−0.92 dB** | restart MISSED |
| resume at 0 (host returned to start) | −33.98 dB | cleared |
| a host that kept calling `process` while stopped | −33.98 dB | cleared |

Rows 3 and 4 are why this is a real inconsistency rather than a theory, and neither is the play edge
doing its job: row 3 is the **seek** detector firing because that host moved the playhead to zero,
and row 4 is a host that never took this path at all. The same user action — stop, then play —
therefore cleared the number or did not, decided by the host's processing model.

### The decision: which of the four candidate fixes, and why

| candidate | verdict |
|---|---|
| clear the hold **in `reset()`** | **Rejected.** That clears the number at the STOP — §C's finding in reverse. Stopping to read it is what the latch is for. |
| change the **play-edge detection** | **Rejected.** Wider blast radius, and it does not address the cause: stale state carried across a lifecycle boundary. |
| clear `prevPosValid` **only** | **Refuted.** `prevPlaying` would still be true, so the edge still never occurs. It does not fix either failing row. |
| clear **`prevPlaying`** in `reset()` | **Adopted.** What is stale is the EDGE DETECTOR, not the meter: the next playing block becomes a genuine rising edge and the existing path does the clearing, at the restart. |
| no change | **Refuted by measurement** — rows 1 and 2. |

`prevPosValid` and `prevPosSamples` are deliberately left alone, and that is measured rather than
assumed. `prevPlaying` already reaches the seek detector (it is the `? :` in its `expected`), so this
write changes that arithmetic too — what it cannot change is any OUTCOME, because `seeked` is read
only by `(playing && seeked)` and `(playing && ! prevPlaying)` is now true on the same block. The
test carries both directions: a resume that IS a seek and one that is not land on the same single
clear.

### The differential, whole

Nine lifecycles, pre-fix and post-fix, same harness. **Exactly two rows change.**

| | pre-fix | post-fix | required |
|---|---|---|---|
| A resume where the transport left off | HELD | **CLEARED** | cleared |
| B resume at the last block's own start | HELD | **CLEARED** | cleared |
| C resume at 0 (also a seek) | CLEARED | CLEARED | cleared |
| D non-playing blocks, then reset, then play | CLEARED | CLEARED | cleared |
| E stopped and reset, still stopped (§C's rule) | HELD | HELD | held |
| F 400 more blocks of continuous playback | HELD | HELD | held |
| G a seek while playing | CLEARED | CLEARED | cleared |
| H the GUI readout click | CLEARED | CLEARED | cleared |
| I no playhead at all | HELD | HELD | held |
| ADR-0007: gain across the reset | −2.7638 dB, Δ 0.000000000 | identical | identical |
| ADR-0007: movement over 1.4 s of silence | 0.000000 dB | identical | identical |

### Related, checked and deliberately not changed

`prepareToPlay` leaves `prevPlaying` stale too — the same class. It is **benign and left alone**:
`prepare()` runs `reset (ResetScope::everything)`, which calls `levels.reset()` and clears the hold
outright, so a missed edge afterwards has nothing left to clear. Recorded here rather than fixed,
because a minimal diff is the point and there is no defect to answer for.

### Coverage

**State test 121** — *a host reset arms the next restart* — carries all nine lifecycles plus the two
ADR-0007 legs, each rig on the heap so no leg inherits another's transport memory (which is the very
state under test). **18 checks**, frame **512 bytes**, so the suite maximum is unchanged at
709,760 B (`testSettingsPublicationIsFieldLevelAndOrderedByObservation`) and the 1 MB-stack parity
step's documented figure stays correct. The state suite goes **4690 → 4708**.

---

## M. Validation of the follow-up

`scripts/preflight.sh` **exit 0** — `check-docs` 149 clean (self-test 464), portability 120,
realtime 93, dispatch 52, state-coverage 64, citations 242, linux-abi 19. **DSP 469 / 0**,
**state 4708 / 0**. Both suites green under **`ulimit -s 1024`**.

The header edit added 51 lines at `PluginProcessor.h:66`, which moved five anchors. Four were
**declared re-aims** (`DELIBERATE_REAIMS`) and one a glossed citation, so all five were re-derived
**by hand from their own symbols** rather than by the line map, and both the document and the
declaration table were updated together:
`parameterValueChanged` 567-570 → **618-621**, `ViewGenWatcher::parameterValueChanged` 772 → **823**,
`UndoStacks` 682 → **733** (two entries), `StateSet::isValid` 612-625 → **663-676** (the struct opens
at 663 and closes at 676). Two further ordinary-drift anchors were re-anchored by `--fix` and then
verified by hand: the class span 25-537 → **25-588**, and `StateSet::selection` 612-624 →
**663-675** (`selection` sits at 674). 534 anchors intact.

**Not run:** Windows, macOS, MSVC, AppleClang, TSan, RTSan, `pluginval`, the sanitizer lanes, and
the two warning gates — they need the pinned gcc-16 / clang-22 and only gcc-13 / clang-18 are local.
Linux is the whole of the local coverage. **No host was exercised**: the lifecycle claims above come
from the pinned JUCE wrapper sources and from a stub `juce::AudioPlayHead`, not from a DAW.

### One intermittent state-suite failure, observed and NOT attributed

Reported rather than waved away, because it is unresolved. **One** local run of the state suite
reported `4708 checks, 1 failure(s)`. That run's stdout was piped through `tail -2`, so the `[FAIL]`
line was discarded and **the failing check is not known**.

It has not reproduced in **nine** subsequent full runs: four on an idle box, three under a
deliberate six-way CPU spin on a four-CPU container, and two under a concurrent 4-way `ninja`
rebuild of the 2 MB `tests/state_tests.cpp` — which is what was running when the failure appeared.
Every one returned `4708 checks, 0 failure(s)`.

What can be said without overclaiming:

* **It is not State test 120 or 121.** Both are deterministic — fixed buffers, fixed transport
  positions, fixed RNG seeds, one heap processor per leg, no threads and no pumped timers — and
  both passed in all ten runs.
* The suite carries **six pre-existing wall-clock assertions** (`elapsedMs < 400.0`,
  `pollMillis < 500`, `millis < 500`, and three `ms < 2000.0`), which is the shape a load-induced
  intermittent failure would take. The one with the tightest budget was measured across six runs
  and is **not** marginal: it reports **51 ms against its 400 ms bound**, unchanged under load.
* This change writes a single `bool` in a host callback the format contract says is not concurrent
  with `processBlock`. There is no new thread, lock, timer or ordering.

So: **not reproduced, not attributed, and explicitly not called a flake** — "flake" is a verdict
this round has not earned. CI captures the whole log, so if it recurs there the failing check will
be named; nothing here suppresses, retries or loosens anything to hide it.



---

## N. Second follow-up: an inert dimMode move re-armed the Level-Match measure

A third review finding on the same branch. **Confirmed** — but its stated scenario does not
reproduce, and the routes that do are different ones.

### What the two lists actually said

R4 gave `discreteDiffers` a Dimension-D relevance guard on `dimMode` (§ADR-0004's Correction):
`chorus.setDimMode (p.dimMode)` is the field's only reader and it sits inside
`else if (p.algorithm == Algorithm::DimensionD)`, so under any other algorithm the value reaches no
module. `processingDiffers` — the narrower question, *did the signal path change* — still compared
it unconditionally. Its one consumer is the silent duck bottom:

```cpp
const bool procChanged = processingDiffers (pendingP, p);
...
if (procChanged) loudness.softReset();
```

### The reported scenario does not reproduce, and that had to be measured

A plain Dim-D Style move under Haas opens **no duck at all** after R4, so `processingDiffers` is
never consulted. Measured; it is leg 1 of the test, asserted rather than assumed, so a future
widening of `discreteDiffers` cannot make the test pass for the wrong reason.

### The two routes that do reach it

| route | before | after |
|---|---|---|
| a plain Dim-D Style move under Haas *(the reported scenario)* | preserved | preserved |
| a **forced duck** — A/B, preset recall, undo, all via `requestDuck()` — whose only processing delta is `dimMode` | **thrown away** | preserved |
| `dimMode` in the same snapshot as a **Level Match toggle** | **thrown away** | preserved |
| `dimMode` riding a real discrete change | thrown away | thrown away |
| `dimMode` **while Dimension D is live** | thrown away | thrown away |
| switching **to** Dimension D; `haasSide` | thrown away | thrown away |

The second route is the pointed one: `autoGainMatch` is the **one** field `discreteDiffers` lists
and `processingDiffers` does not, so toggling Level Match opens a duck of its own — and an inert
`dimMode` riding along made `procChanged` true, defeating the rule written three lines below the
call: *"Toggling Level Match / Bypass must NOT re-measure."*

### The discriminator, and the attribution

`softReset()` clears the K-weighting filters and the energy integrators and keeps the published
gain; the silence gate is judged **from** those integrators. So: converge, go silent, make the
change, keep feeding silence. Analysis preserved → stale integrators → `silent` false → the gain
drifts (**0.030446 dB**). Analysis re-armed → integrators at 1e-9 → frozen (**0.000454 dB**, one
block of pre-bottom drift and then nothing). A **67×** separation, so the 1e-3 threshold sits
nowhere near either number.

Each defect leg is paired with an **attribution control** — the same duck with `dimMode` held still.
Both preserved *before* the fix, so the re-arm was attributable to `dimMode` and to nothing else in
the snapshot. Without those two controls the legs would only show that *a* duck re-arms.

### The fix, and what pins it

The same guard `discreteDiffers` already carries, symmetric and conservative: if either side is
DimensionD it still fires, and when only one side is, `algorithm` already differs a line above.
**Test 58** (10 checks) carries all six rows plus the two attribution controls; **two fail** against
the pre-fix tree, and they are exactly the two reachable routes. Four of its legs must still
RE-ARM — those are what pin the guard rather than a deletion of the term.

---

## O. Part 3: the checker's blind spot — extended, narrowly, and the case against

The informational finding: `check-state-coverage.py` cannot validate the *conditional meaning* of
`dimMode`'s membership. True, and this defect is the proof — the lint was green for four rounds
while the two lists disagreed, because "the field is named" was satisfied either way.

**Decision: extend, with an escape hatch.** Both of the bars set for it are met and measurable.

* **Measured benefit.** R4 introduced the asymmetry on 2026-09-21 and it survived to round 8 under
  a green lint. The rule fails the build at the moment it is introduced, in seconds, on every push.
* **A clear mechanical rule.** *Where the two selection lists both name a field, they must attach
  the same condition to it.* The lists ask different questions, but a field's condition in both is
  the same test — does the value reach a module — so a difference is a decision. The guard is
  extracted by balanced-paren walk from a fixed term shape and compared with whitespace collapsed.
* **It cannot become wrong**, because a legitimate divergence is *declared* in `GUARD_DIVERGENCE`
  with a reason rather than forbidden — the same idiom as the existing exclusion tables. Empty today.

**The case against, recorded because it is real.** The rule governs exactly one field today, and
Test 58 already pins the behaviour by measurement with ten legs, four of them specifically on the
guard. A test that measures output is strictly stronger than a lint that compares text: rewriting
`Algorithm::DimensionD` to the wrong enumerator keeps both lists in agreement and leaves this check
silent. That limit is now written into the lint's own docstring rather than left for a reader to
discover — the lint's third claim is stated as consistency, never correctness.

**Not done:** anything that would have the lint reason about what a guard *means*. That needs real
analysis, not a text scan, and it is the boundary the docstring has drawn since round 6.

Self-test 64 → **79 cases**, both directions, including the R8 defect's own shape, a differing
guard, a re-wrapped guard (must be the same guard), the declared-divergence hatch and a stale
declaration. Demonstrated firing against the **real tree** for both violation shapes.

---

## P. Part 4: the intermittent state-suite failure — still unattributed, and now much better bounded

§M recorded one local run reporting `4708 checks, 1 failure(s)` whose failing check was lost to a
`tail`. This round adds the evidence that matters most, and it is not local.

**CI on `caef45a` completed with every job green** (run 35703670784), and seven of those jobs run
the full state suite independently:

| job | toolchain / platform |
|---|---|
| `linux` | GCC Release, and again under `ulimit -s 1024` |
| `linux-lto-tests` | GCC + `-flto` |
| `sanitizers` | Clang ASan + UBSan + vptr, **and** valgrind memcheck |
| `tsan` | Clang ThreadSanitizer (plus the D-2 probes ×5) |
| `windows` | MSVC |
| `macos` | AppleClang universal, **and** the x86_64 slice under Rosetta |
| `macos-intel` | native Intel |

Plus three more local runs on the current tree. So the count since the single observed failure is
now **nine local runs** (four idle, three under a six-way CPU spin, two under a concurrent 4-way
rebuild) **plus three more here plus the CI matrix** — across five toolchains and three operating
systems, with sanitizers and valgrind — and none reproduced it.

Against the three questions asked:
* **Caused by this branch?** No evidence for it, and now substantial evidence against: the branch
  head passes the suite on every platform and sanitizer CI runs.
* **Deterministic?** No. Not once in the reproductions above.
* **Otherwise?** **Unattributed.** The failing check is still unknown, because that run's stdout was
  piped through `tail`. Nothing has been retried, loosened, suppressed or marked flaky. The suite
  carries six pre-existing wall-clock assertions, and the tightest was measured at **51 ms against
  its 400 ms bound**, unchanged under load — so the obvious hypothesis is not supported either.

It does not block this work, and CI captures whole logs, so a recurrence there will name the check.

---

## Q. Third follow-up: a host reset froze the live meters (R9)

A fourth review finding on this branch, and the third on the host-reset entry point. **Confirmed**,
and wider than reported: the same premise froze the correlation and balance display, and it
defeated the clip-latch clears.

### Baseline, from the tree rather than from this log

- **HEAD at the start of the round was `d9f6c88`** (R8). The round's brief named `dd00eb9`, which is
  R6; `caef45a` (R7) and `d9f6c88` (R8) sit on top of it. The tree was clean and level with origin.
- Suites on `d9f6c88`, local GCC 13 Release: DSP **479 checks, 0 failures**; state **4708 checks,
  0 failures**.
- **CI on `d9f6c88` itself**, read from the Actions API this round rather than assumed from the
  `caef45a` result §P reports: push run **35713608895**, 14 jobs — 13 `success` (`source-lint`,
  `docs`, `linux` with pluginval and the 1 MB-stack pass, `linux-lto-tests`, `realtime`, `tsan`,
  `sanitizers` with valgrind, `fuzz`, `windows`, `windows-avx2-ab`, `macos`, `macos-intel`,
  `macos-crossslice`) and `merge-check` `skipped`, as it is on every push event. The pull-request
  runs on the same SHA — Build & Validate 35713617159, CodeQL, Microsoft C++ Code Analysis,
  Dependency review — all `success`. This branch had introduced no CI failure to fix.

### The lifecycle, traced from the code

Every writer, clearer and publisher of the meter state, and who reads it:

| state | written by | cleared by (before this round) | reaches the GUI through |
|---|---|---|---|
| `pkDim`, `msBri`, `msNum` (the three envelopes) | `process()`, per sample | `reset()` only | `publish()` → `publishAll()` |
| `barPeak` + `hold` (bar tick, 1 s hold) | `process()` / `publish()` | `reset()` only | `publishAll()` |
| `rmsNum` + `rmsHold` (RMS number, 1.2 s hold) | `publish()` → `stepRmsNumber` | `reset()`; `resetReq` clears the hold only | `publishAll()` |
| `blockPeak` | `process()`, rewritten every block | nothing (unobservable) | read by `publish()` |
| `peakHold` (the held peak) | `process()`, max | `reset()`; `resetReq` | `publishAll()`; the peak clip is `db(peakHold) > 0` |
| `rmsClip` | `stepRmsNumber`, `num > 0` | `reset()`; `resetReq` | `publishAll()` |
| `resetReq` (a pending request, not state) | `resetHold()` — the readout click, and `processBlock`'s play / seek edge | `process()`'s `exchange` | — |
| `Correlation` fast / slow `lr`, `ll`, `rr` | `process()`, per sample | `reset()`, which did **not** publish | `publish()`, per block |

The GUI adds nothing to any of it. `LevelMeter::tick` snapshots the atomics and draws them —
`gui/LevelMeter.h`: "ballistics are all audio-side". `StereoMeter` (gui/CorrelationMeter.cpp) runs
its own glide to centre, on its own clock, only when it reads `energy < 6e-9`. So every falling
value in the meter falls only while `processBlock` runs. `reset()` was reached from `prepare()`
alone, since R6 took it off the host-reset path.

The host sequence that matters is the one R7 already established: VST3 `setProcessing(false)`
calls `reset()` and then stops calling `process`, and `setProcessing(true)` resumes without a
prepare (`juce_audio_plugin_client_VST3.cpp:3469-3479`). AU `Reset()` has the same shape.

### Measured, before the fix

Through the wrapper, `AnamorphAudioProcessor` on the heap, −6 dBFS noise with one 0.9 transient,
host reset with no block after it (output meter, L; R identical):

| moment | dim | bright | bar | RMS number | held peak |
|---|---|---|---|---|---|
| active | −6.13 | −11.17 | −0.92 | −10.87 | −0.92 |
| straight after `reset()` | −6.13 | −11.17 | −0.92 | −10.87 | −0.92 |
| 2 s later, nothing run | −6.13 | −11.17 | −0.92 | −10.87 | −0.92 |
| resumed, 4 blocks at −33.98 dB | −6.75 | −11.88 | **−0.92** | **−10.87** | −33.98 (play edge) |

The resumed bar tick and RMS number are the pre-stop values, because their hold timers had not
advanced while nothing ran. For comparison, the same host resetting and then **continuing** to call
`processBlock` with silence decayed normally: dim −20.64 and bar −35.20 at 0.5 s, the bar at −100 by
1.5 s, dim −93.22 and the RMS number −25.21 at 3.0 s.

**The correlation meter froze the same way**, which the finding did not name. With the right channel
at 0.4× the left: `energy` 9.663e-02, phase +1.000, balance −0.724, unchanged after the reset — and
the GUI decides silence from `energy` alone, so both pointers stayed put. Three silent blocks later
`energy` was still 8.457e-02: `CorrelationMeter::reset()` cleared the accumulators and published
nothing.

**The clip latches were affected too**, found by State test 122 rather than by the finding.
`stepRmsNumber` latches the RMS clip on the NUMBER, and `resetReq` does not clear the number. After
+3.52 dB material and a host reset:

| then | peak clip | RMS clip | held peak | RMS number |
|---|---|---|---|---|
| a click, one block of silence | 0 | **1** | −100.00 | +3.30 |
| a play edge, four blocks at −34 dB | 0 | **1** | −33.98 | +3.30 |
| control — no reset, 3 s of silence, then the click | 0 | 0 | −100.00 | −11.04 |

The first block the host ran cleared the RMS clip and re-latched it from pre-stop state.

**Out of scope, recorded:** the vectorscope holds its last frame when the ring stops moving — the
same class of display (audio that has ended, still shown). `Vectorscope::tick` names the case, "the
ring is FROZEN (the host stopped calling processBlock)", but only to skip repaints of a picture that
cannot change; it does not establish that holding the frame is the intended look. It is not in this
path: `ScopeBuffer` has no `reset()`, and giving it one here would make `reset()`'s thread a second
producer on a single-producer ring — a threading-model change. Whether a stop should blank the scope
is a UX decision this round does not take.

### The semantics, and what was rejected

The state splits cleanly in two, and the two need opposite answers on a host reset:

- **Live display** — the three envelopes, the bar tick and its hold, the RMS number and its hold,
  `blockPeak`, and all of `Correlation`. Each describes audio that has ENDED.
- **Latches** — `peakHoldL/R` and `rmsClipL/R` (the peak clip is derived from `peakHold`). They are
  the user's, cleared "on a number click or a playback restart" (`LevelMeters.h`), and a stop is
  neither. R6's fix for them stands.

Chosen: `StereoLevel::resetLive()` — clear the live half and publish — built with `reset()` on one
private partition, `clearLive()` + `clearLatches()`, which between them name every field
`process()` / `publish()` write. `reset()` is now `clearLive(); clearLatches(); publishAll();`.
`CorrelationMeter::reset()` publishes. `AnamorphEngine::reset` calls `correlation.reset()` on both
scopes and `levels.reset()` or `levels.resetLive()` by scope. Rejected:

- **Clearing the fields from `AnamorphEngine`.** Moves `StereoLevel`'s invariants out of the class,
  and nothing would then keep the two halves total.
- **`resetHold()` from `reset()`.** Clears the latches at the stop: State test 120's defect again,
  and R7's "WHAT THE FIX IS NOT".
- **Decay or a staleness timeout in the GUI.** A second clock and a second set of ballistics for the
  same meter, against "ballistics are all audio-side", and a much larger change.
- **`levels.reset()` on the host path.** The R6 regression.

### The reset-state matrix, filled in from the code after the fix

| state | host reset (`audioTailsOnly`) | re-prepare (`everything`) | readout click | playback restart |
|---|---|---|---|---|
| Level Match published gain (`displayedGainDb`, `prevPredictedGainDb`, `matchGainDb`) | kept (`softReset`) | cleared | — | — |
| Level Match analysis (K-weighting biquads, integrators) | cleared | cleared | — | — |
| held peak `peakHoldL/R` (and the peak clip) | **kept** | cleared | cleared, next block | cleared, next block |
| RMS clip `rmsClipL/R` | **kept** | cleared | cleared, next block | cleared, next block |
| envelopes `pkDim`, `msBri`, `msNum` | cleared **(new)** | cleared | — | — |
| RMS number `rmsNum` | cleared **(new)** | cleared | — | — |
| bar tick `barPeak` / `blockPeak` | cleared **(new)** | cleared | — | — |
| hold timers: bar `hold` / RMS `rmsHold` | cleared **(new)** | cleared | — / cleared | — / cleared |
| `Correlation` accumulators | cleared **(new)**, published | cleared, published **(new)** | — | — |
| `prevPlaying` (R7) | cleared | untouched (benign, §L) | — | — |

"Next block" is exact: the click and the restart only raise `resetReq`, and `process()` consumes
it.

### Publication from `reset()`'s thread

`publishAll()` and `CorrelationMeter::publish()` are relaxed stores into the same atomics the audio
thread publishes every block and the GUI reads relaxed from any thread. The non-atomic writes before
them are safe under the same format contract that already covers every other write `reset()` makes:
no `processBlock` runs concurrently with a host reset (THREAD_MODEL.md, Host reset row). Nothing new
is added to the audio path, and there is no new ordering, lock, wait or `callAsync`. The prepare
thread already published the level meters through `levels.reset()`, and R5 did so from this thread
too; the correlation meter's publication is new on both threads. THREAD_MODEL.md and
THREADING_POLICY.md now name the extra writers.

### The trade-off

A host that resets **and keeps** calling `processBlock` with silence used to see the live meters fall
over a few seconds (figures above). It now sees them reach the floor at the stop. The end state is
the same, and silence with **no** reset is unchanged: 3 s of silent blocks give dim −93.21, bar −100,
held peak −0.92, before and after the fix.

### Tests — State test 122, `testAHostResetClearsTheLiveMeters`

Through the wrapper, with a real `AudioPlayHead`, heap-allocated processors, 24 checks, all on the
published atomics the GUI reads:

- **A**, a reset with no block after it: every live readout on both meters exactly −100 (`db()`'s
  floor, so equality rather than a threshold), and `energy` exactly 0.
- **B**, the held peak exactly as before, and both clip latches on both channels.
- **C**, the resume: bar and RMS number describe the new audio, the play edge still clears and the
  peak re-latches new material to within 0.05 dB. A click after the reset, and a play edge after the
  reset, each leave both clip latches clear.
- **D**, a re-prepare: the whole meter, latches included, and a silent correlation meter.
- **E**, what must not move: the click with no host reset, and plain decay on silence.

**Against the pre-fix engine, 9 of the 24 fail**: the three A checks, the two C resume checks, the B
"live readouts beside them are cleared", both clip-latch C checks, and D's correlation publication.
Against the fix, 0 of 24 fail (the suite runs **4732 checks, 0 failures**, up from 4708).

The GUI-click leg began life in group E as an "unaffected path". Against the pre-fix engine it
failed. The reason was the frozen RMS number re-latching the clip, which is the defect, not a
regression in the click. It is labelled C now, and E carries the true control: the click with no
host reset, which passes before and after the fix.

Frame: `testAHostResetClearsTheLiveMeters` is **464 B** (`-fstack-usage`, static). The suite's
largest frame is unchanged at **709,760 B** (`testSettingsPublicationIsFieldLevelAndOrderedByObservation`).

### `scripts/check-state-coverage.py`

- `correlation` moved from `everything` to `both`, with the reason.
- `levels` stays `everything`, **deliberately**: `resets_in` does not count `resetLive()` or
  `resetHold()` as a reset. Counting `resetLive()` would make R6's defect — a full `levels.reset()` on
  the host path — read `both` against a `both` declaration and pass. Measured in a scratch mirror
  of the tree: putting `levels.reset()` back on the host path still fails the lint.
- Self-test: 83 cases, among them `resetLive` and `resetHold` not counting as a reset, and
  `correlation` moved back to a re-prepare only firing. The fixtures that encoded R6's shape
  (correlation under `everything`) were updated to the new one.
- **A blind spot, measured rather than assumed.** The lint sees WHETHER a module is reset under a
  scope, not WHICH reset. Swapping `loudness.reset()` and `loudness.softReset()` between the scopes
  passes it. In the real tree, byte-restored afterwards: the whole swap fails State tests 118, 120
  and 121 (5 checks, all on the host-reset half). **The re-prepare half alone** — `everything`
  taking `softReset()` — **fails nothing**: the lint passes, and so do all 4732 state and 479 DSP
  checks. *[Corrected in §T: this round read that as "a re-prepare keeps the published gain", and it
  does not. `prepare()` zeroes the matcher through `loudness.prepare()` before it ever reaches
  `reset (everything)`, so the mutant changed no behaviour. Removing both flushes is what keeps the
  gain, and State test 120 leg 2 fails on it. There is no coverage gap here.]*

### Documentation changed, and why each

- **ADR-0007** — a dated Correction. R6's rule "leaves display and the user's own latches alone" is
  refined to "leaves the latches and the Level-Match result alone, and clears the live display". The
  ADR is append-only, so R6's text stands.
- **THREAD_MODEL.md** — the Host reset row (scope rule and the publication from that thread), the
  prepare row (what `everything` adds), and three handoff rows. *Level meters* and *Correlation* now
  name the reset writers. *Meter hold reset* now names the play / seek edge as its second writer:
  that is **reported drift, pre-existing**, because the row named only the GUI. Its anchor
  `LevelMeters.h:58,62` was re-derived to `:85, 110`. `:62` already pointed at the bit-select comment
  at HEAD, not at the `exchange`, so that is **drift reported, not silent**.
- **THREADING_POLICY.md** — the Audio → GUI row now names the reset writers, and the prepare one
  was missing too.
- **DSP_ALGORITHMS.md** — one sentence each: `CorrelationMeter::reset()` publishes, and the two
  `StereoLevel` resets.
- **CHANGELOG `[0.9.9]`** — R6's bullet said "Transport stops now leave the meters alone", which is
  no longer true. It now says the peak numbers and their clip colours. A new Fixed bullet covers the
  freeze. That the freeze is user-visible relative to 0.9.8 rests on a **code audit, not a
  measurement**. The 0.9.8 release commit `27920b3` and the merge base `bfd0e06` both have no
  `reset()` override, the same audio-side-only meter ballistics, and the same `energy < 6e-9`
  correlation glide. So a VST3 suspend froze the meters there as well. The bullet makes no claim
  about when the defect began.
- **USER_MANUAL.md** — one clause: a stop keeps the peak numbers and clip colours, and the bars and
  RMS numbers fall.
- **CI_CD.md** and the lint docstring — the blind spot above, and why `resetLive()` is not counted.

**Citations.** The `reset()` comment moved everything below it in `AnamorphEngine.cpp` by +7 lines.
The ten glossed `SIGNAL_FLOW.md` anchors were re-derived by hand: 60 range boundaries were checked
against HEAD's text at the old line, and every gloss was found inside its new range. The other 34 are
ordinary drift, handled by `--fix` (0 need a human). Six more anchors are in the class the gate does
not check (bare filenames, `:NNN` continuations). They were correct at HEAD, this change broke them,
and they were re-derived by symbol: `PluginProcessor.h:73` and State test 121's header
(`LevelMeters.h:82-84`), `TROUBLESHOOTING.md` (`:117-125`, `:204`), `POSTMORTEMS.md:51` (`:204`), and
`REALTIME_SAFETY_AUDIT.md` (`Correlation.h:48-107`, `LevelMeters.h:85-192`).

**Found and reported, not changed** — anchors in that same unchecked class that already pointed at
unrelated text at HEAD. They split two ways, and the split decides whose drift each one is.

- **Broken by this PR's earlier rounds.** ADR-0039:126, ADR-0040:41 and `SpectrumImager.cpp:662` cite
  `AnamorphEngine.cpp:609` / `:616` for "the engine only reads these parameters". At the merge base
  `bfd0e06` those lines ARE `multiband.setCrossovers` / `soloMonitor.setCrossovers`. R4–R8 moved them,
  and the gate does not read bare filenames. They are at `:770` / `:777` today. The recommendation is
  to correct them in this PR, since this PR introduced them. It is left to the documentation pass
  because this round's brief confines docs to the host-reset semantics.
- **Already wrong at the merge base, so older than the PR.** ADR-0009's `LevelMeters.h:98-102, 167`
  (`sanitize`; the bit-select helper and a blank line — `LevelMeters.h` is unchanged between the merge
  base and HEAD). `PARAMETER_REFERENCE.md`'s `:614-617`, `:442-469`, `:648-653`.
  `REALTIME_SAFETY_AUDIT.md:14`'s `:660-1339`. `POSTMORTEMS.md:87`'s `:513` (the merge base's line is
  about `osBlend`, not `mbEnableBlend`) and `:920-921`. The NaN self-heal anchors `1256-1300` /
  `1269-1313` in `TROUBLESHOOTING.md`, `DSP_POLICY.md` and `DEVELOPMENT.md`: the self-heal sat at
  `:1625` at the merge base and is at `:1819` now.

`DSP_POLICY.md`'s other bare anchors (`AnamorphEngine.cpp:761-766`, `:878-894`, `:726-759`,
`:768-785`) were **not checked** this round.

**Also reported:** `DOCUMENTATION_LIFECYCLE_POLICY.md` maps "New/changed test" to `TESTING.md` and
`DOCUMENTATION_COVERAGE.md`. Neither has an entry for State tests 118–122 or Test 58: R6, R7 and R8
did not add them, and this round did not add one for 122 alone. It belongs to the standalone
documentation pass.

### Validation — local, this machine only

| check | result |
|---|---|
| DSP suite, GCC 13 Release | 479 checks, 0 failures; the same under `ulimit -s 1024` |
| state suite, GCC 13 Release | 4732 checks, 0 failures; the same under `ulimit -s 1024` |
| `scripts/preflight.sh` | exit 0. Every lint and self-test passes; citations match `origin/main` (531 anchors) and `d9f6c88` (534). Its local warning sweep and suite half did not run here (no `./build`); the suites above are the same targets built in a scratch tree |
| GCC gate, approximated | the gate's flag set on every translation unit that sees the changed code, GCC **13** (CI pins 16): only the two baselined sites (`PluginProcessor.cpp` `-Wshadow`, `AnamorphEngine.cpp` `-Wmisleading-indentation`) |
| Clang gate, approximated | JUCE's Clang warning set, Clang **18** (CI pins 22), same translation units: every first-party site is a baselined one, plus four `-Wmissing-prototypes` in `tests/AllocationGuard.h`, a file this round did not touch. No site lands on a changed line |
| stack | State test 122's frame is 464 B; the suite's largest is unchanged at 709,760 B |

Not exercised locally, and **not claimed**: MSVC, AppleClang, the pinned Clang 22 and GCC 16, TSan,
ASan/UBSan, RTSan, valgrind and pluginval. Those are CI's, on the commit that carries this work.

---

## R. Part 9 — the conditional-membership limitation: leave it test-only

Recorded in §O: `check-state-coverage.py` requires the two selection lists to attach the same
condition to a field they both name. It does not check that the condition is right, because
conditional membership lives in `updateDerived`'s control flow. Three options:

| option | what it would take | benefit, on evidence |
|---|---|---|
| a static check of the condition | reading which branch of `updateDerived` reaches each module — control-flow understanding of C++ from a text scan | one field (`dimMode`) has a condition today; the lint would be fragile for a single row |
| an executable check | driving each field under each algorithm and asking whether output changes — Test 58's method, generalised | that is a test, and belongs in a test suite |
| **leave it test-only** (chosen) | nothing | Test 58 already pins the one condition, with four must-still-re-arm legs and the must-not legs. The guard-parity rule (R8) holds the two lists to each other |

Nothing measured this round argues for more. The defect this round found was not in conditional
membership. It was in the lint's per-method blindness (§Q), and that is recorded as its own gap. The
note stays informational.

---

## S. Part 10 — the road map, reassessed on the evidence

"Decision" is what should happen next, not work done here; nothing below was implemented. The
criteria are the brief's: severity, reproducibility, impact, likelihood, regression risk, existing
safeguards, dependencies and the cost of postponing. Coverage claims are from grep of the current
tree, not from the original review.

| item | severity · likelihood | safeguards today | cost of postponing | decision |
|---|---|---|---|---|
| **R7 — production-reachable unexercised paths** (F15, F14) | medium · the paths run in every host | F15's transport hole is now partly covered: State tests 120–122 install an `AudioPlayHead` and reach the sample-clock path, the play edge, the seek and the no-playhead path. Still reached by **no test**: the ppq fallback in `processBlock` (0 test files call `setPpqPosition`), bus-layout negotiation and the mono up-mix (0 files), `ScopeBuffer::readLatest` (0 files), and the engine-wide NaN/Inf self-heal (F14 — Tests 19 and 45 feed non-finite samples to the meters, never to `AnamorphEngine::process`) | high, and measured by this branch: the host-lifecycle defects fixed in R5, R6, R7 and R9 were each found by review rather than by a test, and each fix had to add the test that was missing (State tests 118, 120, 121, 122) | **proceed — the next priority.** Additive tests, the lowest regression risk on the list, and the class that keeps producing findings. Start with the re-prepare leg below, then F14's self-heal, then F15's holes in the order listed |
| ~~**A re-prepare keeps the Level-Match gain — untested**~~ *(refuted in §T: State test 120 leg 2 already pins it)* | low · a sample-rate change is rare | none: measured, the mutation passes the lint and all 5211 checks | small but silent — the next edit to `reset()` has nothing to stop it | **proceed**, as R7's first item: one State-test leg (converge, re-prepare at a new rate, assert the published gain is 0). A per-method column in the lint is a table-shape change, and the test is cheaper and exact |
| **F13** — Level Match state that outlives its validity | medium · every preset / undo / A/B with Level Match on | ADR-0007; Tests 16, 58; State tests 118, 120, 121 | medium: (1) `matchGainSmooth` not snapped on a forced duck carries the previous state's match gain through a preset load, undo or redo (review: high → medium; not measured); (2) continuous-only swaps not re-arming is design debt; (3) `LoudnessMatch`'s missing non-finite guard is low (the review's verifier: the self-heal fires first) | (1) **investigate** — measure the carried gain through a preset load before choosing a fix; (2) **architecture decision** — it is R6c's question and changes ADR-0007's re-arm contract; (3) **defer**, and let it ride with F14's test, which is the one that can reach it |
| **F10** — re-entrant `mouseUp` leaks two gestures | high · low, needs a re-entrant host | none for three of `gestureBands`' four writers (source-readable: `SpectrumImager.cpp:3254` is the only consult) | medium: a host sees an automation touch that never ends | **investigate**: the root cause is readable in source, and the state suite already drives imager presses (State test 105). A synthetic re-entrant `mouseUp` would turn "likelihood unverified" into a measurement before R6b is sized |
| **F9** — adoption under the held `soundReplacement` lock | high · unverified, needs a host holding the other edge | the TSan lane and canary, which cannot hold the other edge | unknown; the review marked it overstated | **investigate reachability** before any fix (open question 6 of the global review); no change on this round's evidence |
| **F12** — `advancedMode` host-writable, synchronous resize | high · low | none: no test transitions it with an editor open | unmeasured: the review records no reproduction in a shipping host | **defer**, unchanged. It needs a host-write test with an editor, which is R7-shaped work once R7's lifecycle items are done |
| **R6c / R6d** — `processingDiffers`' two questions; one prologue for the five program-state jumps | behaviour changes | R8's guard parity reduces R6c's exposure | low | **defer**, per the road map's own instruction: each needs an ADR amendment |
| **R6a / R6b** — the `StateCommandGate` lint; `gestureActionDepth` consulted by all four writers | enforcement / F10's fix | — | R6b is gated on F10's measurement | **defer** R6a (trigger unchanged); R6b **after** F10's investigation |
| **Scalar-state gap**, and the new **per-method blindness**, in `check-state-coverage.py` | enforcement gaps, both documented | tests (State tests 57, 118–122; and the leg above) | low once the re-prepare leg exists | **preserve** as documented; revisit only on a second method-level defect |
| **Conditional membership** (Part 9) | informational | Test 58 | none | **preserve** test-only (§R) |
| **F16 / road-map R8** — documentation drift | low · certain | the citation gate covers path-qualified anchors only | rises with every round: this one found five unchecked anchors this PR's own earlier rounds broke, eleven older ones, and the missing test entries (§Q) | **proceed** as a documentation-only pass, separate from code rounds. Do not widen it into an ADR evidence audit, which R8's own text rules out |
| **Intermittent state failure** (§M, §P) | unattributed | whole CI logs name any recurrence | none this round: nine full local runs, every failure in them a deliberate pre-fix or mutation check | **investigate on recurrence only**; no retry, no loosening |
| **Vectorscope frozen frame** | UX question, not established as a defect | none; the idle gate treats a frozen ring as a static picture | low | **architecture decision** if blanking is wanted: it needs a second producer on the SPSC `ScopeBuffer` (a threading-model change), so it is not a follow-on to this fix |

**The next engineering priority is R7**, entered through its cheapest item, the re-prepare leg *(which §T shows is already covered)*. The
reasons: it is the only open item whose absence produced findings on this branch, round after round;
its work is additive tests, so it cannot regress the product; and every higher-severity item (F9,
F10, F12) needs a measurement first that the same test harness provides.

---

## T. PR #155 finalization, after the architecture approval

### Baseline and CI, read for the current head

- **HEAD `e13664f`** (R9's worklog correction on top of `c5f3d8f`, R9's code), level with origin,
  tree clean. PR #155 open, `mergeable_state: clean`.
- **CI on `e13664f`, complete.** Push run 35778680286: all 13 jobs `success` — `source-lint`,
  `docs`, `linux` (Clang 22 build and first-party warning gate, pluginval ×3 deterministic and ×3
  randomised, the suites again under a 1 MB stack), `linux-lto-tests` (GCC 16 and its warning gate),
  `sanitizers` (ASan + UBSan + vptr, then valgrind on both suites), `tsan`, `realtime` (RTSan),
  `fuzz`, `windows` (MSVC, pluginval), `windows-avx2-ab`, `macos` (AppleClang universal, the x86_64
  slice under Rosetta, pluginval VST3 and AU), `macos-intel`, `macos-crossslice`; `merge-check`
  skipped, as on every push. The pull-request run 35778689255 ran `merge-check` — the merge with
  `main`, both suites — `success`. CodeQL, Microsoft C++ Code Analysis and Dependency review on the
  same SHA: `success`.
- **`c5f3d8f` never completed a run of its own**: both Build & Validate runs were cancelled by the
  branch's `cancel-in-progress` when `e13664f` was pushed. `e13664f` changes only the worklog, so its
  run is the first complete validation of R9's code.
- **The architecture approval** is the owner's ruling of 2026-09-22. It is recorded in ADR-0007 in
  the form ADR-0008 established: "Architecture Review Gate: APPROVED by the owner", with its
  four-step table. GitHub holds no approving review object for it — the reviews on the PR are CodeQL
  bot comments — so the ADR entry and the PR body are the record.

### The five anchors this PR broke, repaired

| where | was | now | the claim |
|---|---|---|---|
| ADR-0039:126 | `AnamorphEngine.cpp:609`, `:616` | `src/dsp/AnamorphEngine.cpp:770` (`multiband.setCrossovers`) and `src/dsp/AnamorphEngine.cpp:777` (`soloMonitor.setCrossovers`) | "the engine only reads them" |
| ADR-0040:41 | the same two | the same two | the same |
| `src/gui/SpectrumImager.cpp:662` | `AnamorphEngine.cpp:609` | `src/dsp/AnamorphEngine.cpp:770` (`multiband.setCrossovers`) | the same |

At the merge base `bfd0e06` the old numbers were exactly those two `setCrossovers` calls; R4–R8
moved them by 161 lines. The claim itself was re-checked, not assumed: in `src/dsp/` the split
parameters are compared (`sameParameters`) and read into the two crossover banks, and nothing
writes them.

**Why the full path.** A bare filename is invisible to the gate, which is how all five drifted
unseen. The repository's own precedent (round 48, `TESTING.md`) is to rewrite such anchors "in
full-path form, which puts them under the gate from now on". Each now carries a gloss naming the
call on its line, so a reader can check the aim too. `SpectrumImager.cpp` kept its line count, since
it is a tracked file cited from elsewhere.

**Verified with the gate's own code, both ways.**
- *Content.* `glossed_problems_in` — the function the gate runs on its opted-in documents — over
  the three files: all five anchors claimed, 0 problems. Five mutants, each anchor moved onto a
  neighbouring line or the other call: all five caught ("names 'multiband.setCrossovers', which is
  not there").
- *Drift* — the failure that broke them in the first place. With these anchors committed as the
  base, two lines were inserted above `:770` and two blank lines removed after `:777`, so that only
  that window moved. `--check --base HEAD` then exited 1 on exactly these five: "DRIFTED …
  `src/dsp/AnamorphEngine.cpp:770 -> …:772`" (and `:777 -> :779`) in ADR-0039, ADR-0040 and
  `SpectrumImager.cpp`, with `--fix` offering the moved calls. The gate's checked-anchor count rose
  from 535 to 540. The source was restored byte-identically afterwards.
- *What CI will not do.* These three files are not in `GLOSS_CHECKED_DOCS`, so CI runs the drift
  half on them and not the gloss half. The list admits a document only after every citation in it
  has been read, and that is the ADR-anchor audit this round's brief rules out.

### The final reset-semantics pass — no inconsistency, no code change

> **Correction (§U).** This pass read the meter and Level-Match halves of the contract, not the
> modules' sound state: a host reset restarted the Chorus / Dimension-D wet and depth from zero.

Read from the code, not from §Q: `AnamorphEngine::reset` has exactly two callers, `prepare()`
(`everything`) and `PluginProcessor::reset()` (`audioTailsOnly`).

| requirement | where it holds | pinned by |
|---|---|---|
| `audioTailsOnly` clears the live meters | `levels.resetLive()` | State test 122 A |
| peak hold and clip latches survive it | `resetLive()` touches neither | State tests 120 (leg 3), 122 B |
| the Level-Match published gain survives it | `loudness.softReset()` | State tests 118, 120 (leg 1), 121 |
| the Level-Match analysis is cleared | `softReset()` clears the biquads and integrators | State test 120 leg 1 (silence moves 0.000000 dB) |
| correlation is cleared and published | `correlation.reset()` on both scopes | State test 122 A and D |
| `everything` is the full reset | `loudness.reset()`, `levels.reset()` | State tests 120 (leg 2), 122 D |
| a re-prepare is distinct and complete | `prepare()` → `loudness.prepare()` (→ `LoudnessMatch::reset()`), `levels.prepare()`, `correlation.prepare()`, then `reset (everything)` | the same |
| the readout click stays distinct | `resetHold()` → next block: peak hold, RMS clip, RMS hold | State tests 121, 122 C/E |
| the playback restart stays distinct | play / seek edge → `resetHold()` | State tests 121, 122 C |
| the thread-model record matches | THREAD_MODEL host rows, *Level meters*, *Correlation*, *Meter hold reset*; THREADING_POLICY Audio → GUI | read against the code above |

### A correction to R9: the re-prepare "gap" was not one

§Q and §S recorded that "a re-prepare that keeps the Level-Match gain passes the lint and both
suites", and made closing it R7's first item. **Wrong.** The mutation behind it — `reset
(everything)` taking `softReset()` — never made a re-prepare keep the gain. `prepare()` zeroes the
matcher through `loudness.prepare()` → `LoudnessMatch::reset()` before it reaches `reset
(everything)`, and that is the only caller of the `everything` scope. Measured on the state suite,
restored byte-identically afterwards:

| mutant | what it removes | state suite |
|---|---|---|
| M1 | the `everything` branch's `loudness.reset()` (→ `softReset()`) | 4732 / 0 — nothing changed |
| M3 | `LoudnessMatch::prepare()`'s own `reset()` | 4732 / 0 — nothing changed |
| M2 | both | **1 failure**: State test 120, "ResetScope::everything still flushes the matcher, published gain included" |

The contract is pinned already, by State test 120 leg 2, beside leg 1's host-reset half — which is
exactly the pair R7's first item was meant to add. The two flushes are redundant, and either one
alone keeps the behaviour, so no single-flush mutant can be observed at all. No test was added, and
no product code changed. The lint docstring and `CI_CD.md` now say this, and §Q and §S carry
correction markers. The R9 commit message (`c5f3d8f`) keeps the mistaken sentence; it is pushed
history and is not rewritten.

## U. The Devin finding: "Chorus fades in after host reset"

HEAD `8b0850b`. The finding reached this round as a title and three locations:
`src/PluginProcessor.h:68` (the host-reset override), the `AnamorphEngine::reset` path, and
`ChorusEngine::reset`. PR #155 has no review thread for it, so there is no thread to reply to. It
was treated as a hypothesis. The code was left alone until the mechanism had been measured.

### The original hypothesis

Stated as the finding implies it: `ResetScope::audioTailsOnly` runs `chorus.reset()`, which zeroes
the chorus's wet blend and modulation depth along with its delay line. Nothing on the host path
re-seeds them, so after every host reset a Chorus or Dimension-D session fades in from dry.

This round asked four questions:
1. Does a host reset restart the Chorus wet, the Dimension-D wet, the modulation depth, or any
   other user-visible sound state from zero?
2. How does that compare with `prepare()`, which already calls `snapToTargets()`?
3. What is the intended contract?
4. Is the fix the finding implies the right one?

### The mechanism, read from the code

- **Two callers.** `AnamorphEngine::reset` has exactly two: `prepare()` (`everything`) and
  `PluginProcessor::reset()` (`audioTailsOnly`, `src/PluginProcessor.h:66-68`, added in R5 by
  `d5a0a0e`). VST3 `setProcessing(false)` and AU `Reset()` reach the second (THREAD_MODEL, *Host
  reset*). The merge base has no override, so the host path exists only in this unreleased PR.
- **`ChorusEngine::reset()`** clears the buffers and write indices. It also sets `phase`,
  `currentWet` and `currentDepth` to 0 (`src/dsp/ChorusEngine.cpp:28-30`). One engine serves both
  Chorus and Dimension-D, so both voices share this state.
- **`prepare()`** follows its `reset (everything)` with `chorus.snapToTargets()`
  (`src/dsp/AnamorphEngine.cpp:179`, ER-DSP-09). That sets `currentWet = amount` and arms
  `snapDepthPending`. The depth snap is consumed at the next `processBlock`, after `setWorkingRate`
  (`src/dsp/ChorusEngine.h:42`).
- **The host path had no such re-seed.** Both glides therefore restart from 0 at the one-pole's
  ~10 ms time constant.
- **The depth glide never reaches its target.** It stalls at a float fixpoint short of the target:
  239 ULP at 48 kHz, 1919 ULP (0.23 samples) at 8x. This is ordinary one-pole behaviour; the defect
  is the rewind to 0 that exposes it.
- **On AU this also undid ER-DSP-09 at session start.** JUCE's AU `Reset()` is
  `if (! prepared) prepareToPlay(); juceFilter->reset();` (`juce_audio_plugin_client_AU_1.mm:255-263`).
- **The other `chorus.reset()` callers are a different case.** They call the module directly and
  never call `AnamorphEngine::reset`:
  - the duck bottoms (`AnamorphEngine.cpp:1048`, `:1058`, `:1108`);
  - the wrap's cold-to-warm restart (`:1321`);
  - the NaN self-heal (`:1875`).

  At each of these a fade masks the rewind, or the glide is meant to go. R5 added the first caller
  where nothing masks it. The prepare() comment explaining why the snap is not inside `reset()`
  claimed that `reset()` itself runs at the duck bottom and on the self-heal. That is not true; the
  comment is corrected in this change.

### Measured behaviour, before any change

**Method: twins.**
- **A:** settle 1 s of seeded noise, host reset, probe.
- **B:** a fresh instance prepared at the same settings. At processor level the parameters are set
  *before* `prepareToPlay`; setting them after it opens a ~34 ms discrete duck and confounds the
  comparison.
- A and B are compared bit-exactly. The measurements were taken at engine level and through
  `AnamorphAudioProcessor::reset()`, at 48 kHz with 256-sample blocks.

**Residual against the fresh twin (Chorus, Amount 1.0).**

| window after the reset | 0-5 ms | 20-50 ms | 50-200 ms | persistent |
|---|---|---|---|---|
| Chorus, Amount 1.0 | **+308 dB** | 0.5 dB | −16 dB | **−47 dB** |

- **0-5 ms.** The prepared twin is exact silence: 100 % wet, empty delay line. The reset instance
  leaks the dry signal.
- **Dimension-D** (mode 3) has the same shape; its persistent residual is −53 dB.
- **The persistent term is the depth stall.** Across the oversampling factors it measured −49, −42,
  −36 and −30 dB at Off, 2x, 4x and 8x.

**Effective wet**, measured with a DC probe into the empty line as `1 − out/x`:

| time after the reset | 0.02 ms | 1 ms | 2 ms | 5 ms | 10 ms | 13 ms |
|---|---|---|---|---|---|---|
| Amount 1.0 (the twin: 1.000 throughout) | 0.004 | 0.097 | 0.183 | 0.395 | 0.633 | 0.728 |

At Amount 0.7 the effective wet was 0.068 at 1 ms and 0.277 at 5 ms, against the twin's 0.7.

**Isolating the two halves.** Each half has its own audible cause, so both must be re-seeded.

| variant | 0-5 ms | persistent |
|---|---|---|
| re-seed the wet only | −inf (exact) | −46.7 dB |
| re-seed the depth only | +308 dB | −92.9 dB |

**Other paths measured.**
- **AU order**, through the processor: `prepareToPlay` then `reset()` differed from `prepareToPlay`
  alone by −5.2 dB (Chorus) and −2.7 dB (Dimension-D) over the first 43 ms.
- **Census of 240 random configurations** (OS 2x/4x/8x, Drive with OS, Multiband, Mix, Bypass).
  115 of 115 Chorus / Dimension-D configurations differed from a fresh prepare; 0 of the 125 others
  did.
- **Existing coverage: none.** At HEAD the DSP suite (492 / 0) and the state suite (4760 / 0) pass
  with and without the fix, and State tests 118-123 print identical output either way.

**The four questions, answered.**

| state | host reset at HEAD | `prepare()` |
|---|---|---|
| Chorus wet (`currentWet`) | **restarts at 0, glides back** | snapped to the Amount |
| Dimension-D wet (the same `currentWet`) | **restarts at 0, glides back** | snapped |
| modulation depth (`currentDepth`) | **restarts at 0, stalls 239 ULP short** | snapped (deferred one block) |
| LFO phase | restarts at 0 | restarts at 0: identical |
| Haas amount / delay, Velvet wet, Mono Maker, Multiband widths, solo, Drive + OS, Width / Mix / Output | unchanged: bit-identical to the fresh twin | — |
| Level-Match published gain | kept (ADR-0007) | cleared |
| meter latches | kept (display, §Q) | cleared |

The chorus is the only module whose `reset()` destroys a sound-state glide:
- Haas snaps its delay and keeps a finite wet.
- Velvet keeps its wet.
- Mono Maker keeps its cutoff.
- MultibandWidth snaps its widths.

### The contract, and whether it changes

**The finding contradicts the contract as already written.** The contract itself does not change.

**What the documents say:**
- **ADR-0007's rule:** "a host reset clears audio and the live display that describes audio that
  has ended, and leaves the user's latches and the Level-Match result alone."
- **THREAD_MODEL's *Host reset* row** says the same.
- **`ResetScope::audioTailsOnly`** (`src/dsp/AnamorphEngine.h`) lists buffers, filters, rings, the
  duck, the matcher's analysis and the live display. It does not list smoothed sound state.
- **The duck-flush comment in `reset()`** promises "a clean steady state".
- **CHANGELOG `[0.9.9]`** says "Normal processing after a stop is unchanged".

**What that means for the code.** A host reset must be bit-identical to a clean start, with two
exceptions: the Level-Match published gain and the meter latches.

**The wet blend and the depth are the user's sound, not audio.** At HEAD the code broke the contract
for Chorus and Dimension-D and nowhere else. The code is wrong and the documents are right. No ADR
changes.

### The final decision, and why it is correct

The code, appended as the last statement of `AnamorphEngine::reset()`:

```cpp
if (resetScope == ResetScope::audioTailsOnly
    && isModAlgorithm (p.algorithm)
    && std::isfinite (p.algoAmount))
    chorus.snapToTargets();
```

**Why this is the correct fix:**
- It re-seeds exactly the two values the census found wrong, from the targets the user set, with
  the method `prepare()` already uses for the same reason (ER-DSP-09).
- `snapToTargets()` is two plain stores, `noexcept`. It adds no allocation, lock or wait, and adds
  nothing to `process()`.
- It leaves unchanged the parameters, the state schema, the signal order, the reported latency and
  the threading model. It also leaves `prepare()`, `ChorusEngine`, `snapSmoothers()` and the order
  of every existing statement in `reset()` as they were.
- It is not an Architecture Review Gate class: it makes the approved R9 contract hold for the one
  module that broke it.

Each of the four conditions is pinned by a mutant (the table below):

- **Last, after the duck flush.** A forced swap (A/B, preset, undo) holds the new Amount in
  `pendingP` until the flush's `p = pendingP; updateDerived();`. A snap taken beside `chorus.reset()`
  at the top of `reset()` seeds the *old* Amount: −3.4 dB and max |d| 0.174 for Chorus 0.3 → 0.9,
  and Haas 0.3 → Chorus 0.9 landed on 0.3.
- **`audioTailsOnly` only.** `everything` is `prepare()`'s own flush, and `prepare()` snaps after
  it. Dropping this condition passes every check but changes existing DSP output through the direct
  `engine.reset()` callers in the suite: Test 55's dimMode control moves from −24.66 to −33.71 dB.
  The fix may change nothing but the defect.
- **Chorus / Dimension-D only.** In any other algorithm the chorus is idle. At HEAD a host reset
  leaves it exactly as it was: it was already reset at the duck bottom that left the modulation
  voice. Without this condition the reset arms `snapDepthPending`, and a later Haas → Chorus switch
  differs from the same session with no reset, from 222 ms on.
  - An investigator preferred the unguarded form, so that the switch matches a twin prepared in
    Haas (that twin differs by −12.7 dB). That −12.7 dB is the existing history-dependence of the
    one-shot depth snap: a Haas session that ever ran Chorus already differs from one prepared in
    Haas, with no host reset involved.
  - The guarded form matches the continuation with no reset, which is the contract. It is identical
    to the unguarded form in every finite Chorus / Dimension-D case.
- **Finite Amount only.** Seeding a NaN target defeats R7's reseed rule (ADR-0009, Implementation
  note 2026-09-22), where a non-finite target parks at the 0 that `reset()` gave it. Measured
  without the guard, the first finite block after a host reset taken during a NaN Amount is zeroed
  by the self-heal (256 samples), and the self-heal's `loudness.reset()` drops the Level-Match gain
  to 0.000 dB. HEAD shows neither.

**Alternatives rejected on measurement:**

| alternative | why not |
|---|---|
| snap beside `chorus.reset()` at the top of `reset()` | seeds the pre-flush Amount inside a forced swap (above) |
| change `ChorusEngine::reset()`, or add a tails-only reset to the module | changes the duck-bottom, wrap and self-heal paths. A tails-only module method failed 4 of the scenarios measured, among them a forced swap, a reset mid-`osBlend` and one mid-Amount glide; skipping `chorus.reset()` on the host path instead leaves the tail ringing (V6 below) |
| snap all four modules | a wider behaviour change with no defect behind it; the others are already bit-identical |
| no scope guard / no `isModAlgorithm` / no `isfinite` | each measured above |

**The mutants.** Each was applied to the engine alone, against the final tests:

| variant | failures |
|---|---|
| V0 — HEAD, no fix | **10**: Test 62 C1, C2; State test 126 A ×4, B ×2, E ×2 |
| V2 — no `isfinite` | Test 62 G |
| V3 — no `isModAlgorithm` | Test 62 H |
| V4 — snap before the duck flush | Test 62 C1, C2 |
| V5 — no scope guard | 0, but changes Test 55's printed control (−24.66 → −33.71 dB). Kept out on the no-side-effect rule, not caught by a check |
| V6 — skip `chorus.reset()` on the host path instead | State test 126 C ×2 (the tail survives), A ×4, B ×2; Test 62 C1 |

**After the fix.**
- The 240-configuration census gives 0 of 240 differing from a fresh prepare.
- The effective wet is the Amount from the first sample.

**One consequence, stated so it is not mistaken for a dropout.** At 100 % wet the first
milliseconds after a host reset are exact silence, until the delay line's shortest tap fills. That
is longer than the 5.3 ms State test 126 A asserts. A fresh `prepare()` does exactly the same.

### Regression coverage

- **Test 62** (`testHostResetChorusSeedIsScoped`, `tests/dsp_tests.cpp`, engine level, 4 checks)
  pins where the seed sits and when it must not run:
  - **C1:** a forced swap Chorus 0.3 → 0.9 in flight at the reset.
  - **C2:** an ordinary duck Haas → Dimension-D in flight at the reset.
  - **G:** a NaN Amount pending at the reset.
  - **H:** a Haas session host-reset and then switched to Chorus.
  - C1 and C2 are bit-identical to a fresh engine at the new settings; H is bit-identical to the
    same session without the reset.
  - **Pre-fix: 2 failures (C1, C2). Post-fix: 0.**
- **State test 126** (`testAHostResetKeepsTheConfiguredChorusSound`, `tests/state_tests.cpp`,
  16 checks) is driven through `AnamorphAudioProcessor::reset()`, the call the wrappers make, for
  Chorus and Dimension-D:
  - **A:** Amount 1.0 and 0.7: the configured wet is there from the first sample. On the first
    block the worst |out − x(1 − Amount)| is below 1e-6; measured 0 and 2.98e-8. Pre-fix: 0.499
    and 0.349.
  - **B:** OS Off / 2x × Amount 1.0 / 0.7: bit-identical to a fresh processor for 0.5 s. Pre-fix,
    47 608-47 616 of 47 616 samples differ.
  - **C:** audio tails are still cleared: silence after loud material is exactly 0 after the reset.
    A control without the reset peaks at 0.495 (Chorus) and 0.479 (Dimension-D).
  - **D:** `prepare()` is unchanged: a fresh prepare opens at the configured wet, and a re-prepare
    at OS 2x equals a fresh processor bit-exactly.
  - **E:** the AU order, `prepareToPlay()` then `reset()`, is bit-identical to `prepareToPlay()`
    alone.
  - **Pre-fix: 8 failures (A ×4, B ×2, E ×2). Post-fix: 0.** C and D pass on both, as they must:
    they pin what the fix must not change.
- **Both prove the pre-fix failure against the final test text.** HEAD's engine with the final
  tests gives DSP 496 / 2 and state 4776 / 8. The tree gives 496 / 0 and 4776 / 0.
- **Existing coverage of `prepare()`**: Test 49 (`testRestoredModulesDoNotGlideIn`), unchanged and
  passing.

### A separate finding, recorded and not fixed

**A host reset inside a forced swap's fade-out does not land in "a clean steady state".** The flush
adopts `pendingP` through `updateDerived()` only. It calls neither `snapSmoothers()` nor the other
modules' snaps, so the new Mix, Width and Output targets glide in:
- over 20 ms, max |d| 0.053, 0.131 and 0.245;
- a Haas Amount change glides for ~200 ms (max |d| 0.21).

The comment above the flush ("bit-exact transparent from sample 0") is false in that case.

**Why it is not fixed here:**
- It needs a forced duck (A/B, preset, undo) in flight in the ~6 ms before a stop.
- It is not the Devin finding.
- Fixing it changes which values `reset()` snaps: a behaviour decision beyond the smallest fix.

It is recorded for the road map. Test 62 C1 changes only the Amount, so it pins this fix without
depending on that one.

### Where earlier passes missed it

- **R5** added the host-reset caller without checking it against ER-DSP-09, the defect class
  `prepare()`'s snap exists for.
- **R6's state inventory** (above) classed `chorus` as "delay lines and filter banks: audio". That
  is true of its buffers and false of its wet and depth glides.
- **§T's "final reset-semantics pass"** read the meter and Level-Match halves of the contract and
  not the modules' sound state.

Each carries a correction marker pointing here.

### Documentation

**Changed:**
- the `prepare()` comment in `AnamorphEngine.cpp` (corrected, same line count);
- `procedures/TESTING.md` (Test 62, State test 126);
- `DOCUMENTATION_COVERAGE.md` (the 74th pass);
- this section and its correction markers;
- 61 citations (118 line numbers) across 19 documents and one source comment, re-anchored by
  `check-citations.py --fix`. The fix block shifted every line after `AnamorphEngine.cpp:307` by 19.
  Each of the 118 was verified mechanically as exactly +19, past 307, with no text change.

**Not changed, on purpose:**
- **No ADR.** The documented contract (ADR-0007, THREAD_MODEL, `ResetScope`) is unchanged, and the
  code now meets it.
- **No CHANGELOG entry.** The defect never shipped: the merge base has no reset override
  (CHANGELOG_POLICY, "never a `Fixed` bullet for a fix that only ever existed in an unreleased
  branch"). The existing `[0.9.9]` sentence "Normal processing after a stop is unchanged" was false
  at HEAD and is now true, pinned by State test 126.

**Drift reported, not fixed:**
- `architecture/API_REFERENCE.md`, the `reset` row: it gives the signature as `void ()` (stale since
  R5 added `ResetScope`) and says "settles smoothers". `reset()` settles the three crossfades, not
  the Width / Mix / Output smoothers.
- The bare same-file anchors in `AnamorphEngine.cpp`'s own comments (`:433`, `:445`, `:480`,
  `:590`, `:603`, `:894`, `:269`) were already stale at `8b0850b`. The citation gate does not see
  the bare form.

### Validation, local

- **Suites, GCC 13 Release:** DSP **496 / 0**, state **4776 / 0**, and the same under
  `ulimit -s 1024`.
- **ASan + UBSan** (local Clang 18, RelWithDebInfo, CI's sanitizer flags): DSP and state pass with
  no report. The state binary needs `ulimit -s unlimited` locally: under this compiler `main`'s
  frame is ~13.6 MB, and an earlier head overflowed the same way at 8 MB. That is a property of the
  local toolchain, not of this change.
- **Lints:** `check-docs`, `check-portability`, `check-realtime`, `check-dispatch` and
  `check-state-coverage` all pass.
- **Citation gate:** passes against `HEAD`, `HEAD~1` and the merge base.
- **Warnings:** the GCC gate flags and Clang 18's warning set, over the changed translation units,
  give nothing on an added line. Three found on State test 126's first draft were fixed:
  - an unneeded lambda capture, twice;
  - a float `==` comparison, now `juce::exactlyEqual`.

---

---

## K. Remaining findings, and what the next item is

> **Updated after R9 (§Q–§S).** The R6 list below stands, with four changes. **R7 is now the next
> priority**, entered through the re-prepare Level-Match leg (§S) — which §T then found already
> covered by State test 120 leg 2; R7's record is `worklogs/R7_PRODUCTION_PATH_COVERAGE.md`. **R6b** is sequenced after an F10
> measurement. The lint's **per-method blindness** joins the scalar-state gap as a documented
> enforcement gap. The **documentation pass** gains the unchecked-anchor drift and the missing test
> entries listed in §Q.

Nothing new was manufactured. The round leaves:

- **R6a** (`StateCommandGate` lint) — deferred with its reason in §F/C5; the correct next gate when
  a fifteenth state-replacing entry point appears.
- **R6c / R6d** (`processingDiffers`' two questions; a shared prologue for the five program-state
  jumps) — behaviour changes, not enforcement; defer per the road map's own instruction.
- **R7** (coverage for production-reachable unexercised paths) — unchanged, and now the cheapest
  high-value item on the list.
- **R8** (the standalone documentation pass) — one item less: `CI_CD.md`'s missing lint is done.
- **The scalar-state gap** in `check-state-coverage.py`'s target 2 — stated in the docstring and in
  §F/C3, covered by tests rather than by the lint. Closing it is a table-shape question, not a
  defect.
