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

## K. Remaining findings, and what the next item is

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
