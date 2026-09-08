# SpectrumImager — topology transaction consistency audit (v0.9.8)

**Head at the start of the round:** `91e20d9`. **Preceding chain:** ADR-0038 (a gesture is void once
its topology moves), ADR-0039 (a gesture owns the world it was latched in), ADR-0040 (a gesture
stores only what it still owns), ADR-0041 (a coupled update is all of it or none of it), ADR-0042
(a store is committed only when the parameter says so), ADR-0043 (a commit carries intent, and a
plan is computed where it is used).

## 1. Running work, inventoried before anything else

| Handle | What it was | Disposition |
|---|---|---|
| `whlv0zass` | `local_workflow` — *"Audit every SpectrumImager write and preview path against the ADR-0040/0042 ownership invariant, and rule the two review findings"*, the previous round's RO-1 fan-out | **Redundant — stopped.** Its question was answered and shipped as ADR-0043; every conclusion it existed to reach is already in `worklogs/SPECTRUMIMAGER_REMAINING_OWNERSHIP_AUDIT_v0.9.8.md` |
| `wn9z13hsv` | an earlier round's workflow | Already gone — lost to a container restart; `TaskStop` reports no such task, so there is no handle and nothing consuming resources |
| `btqalfv71`, `b7ljd47ih`, `bceeuq71w`, `bk2n9gf12`, `b1eth7hd9`, and the CI/valgrind monitors | last round's background bash tasks | All completed, all results consumed and reported |
| sub-agents | — | `ListAgents`: none. No other session on this machine |

`ps` showed no compute-bound process. One workflow was launched for **this** round
(`wf_17153265-ac9`) after the inventory.

## 2. F1 — topology edits commit mixed layouts. CONFIRMED, and measured

The reviewer's mechanism is *"a listener can replace `mbSolo` or the recently written parameter
inside `setParam`; later iterations only validate the next store"*. The second half is not quite
what the code does, and the distinction matters:

* Validating only the next store is **correct** for the slot it guards. Every slot's plan comes from
  the entry snapshot, never from the previous slot's committed value, so there is no stale plan to
  continue from. The audit's independent reconstruction reached the same verdict.
* The real hole is that `soloMask()` is read **exactly twice** in each burst — the entry snapshot and
  the guard immediately before `setSoloMask` — and **never again**. Between `setSoloMask` returning
  true and the count store sit up to six further synchronous dispatches, plus `setBands`'s own
  `beginChangeGesture`. Nothing in that window reads the mask.

**Reproduced on `91e20d9`, State test 76, a removal at N = 4:**

```
[leg A] the count committed over a mask the transaction no longer owned: Bands 3 with mask 0x8
[leg F] the transaction ran on past the divergence: split0 2000.0 (untouched is 200.0), Bands 4
[leg G] the count committed from inside its own gesture open: Bands 3 with mask 0x8
```

Leg A is F1 exactly. `SoloMonitor::process` then masks `0x8` with `((1 << 3) - 1) = 0b0111`, so the
soloed band is silently *gone* while `mbSolo` still reads `0x8` and the button stays lit.

**The in-source comment was false.** `addBandAt` claimed *"The transaction cannot go on to change
the band count with the mask still in the old numbering — that is the half-applied commit T3
measured."* True only of a change landing **before** the mask store, which is what its guard tests.
Corrected in place.

## 3. What the round's own audit did to the round's own first fix

The first implementation was wider: record every store's read-back, re-prove the whole prefix
(mask, widths **and** splits) before each subsequent store, and convert the width and split leaves
from `setParam` to `storeOwned` so the read-backs existed. It passed its own tests — legs A, B and C
all asserted "the count must not commit". The audit then refuted two things about it, and both are
kept here rather than quietly dropped:

* **SP-6 — it contradicted an Accepted ADR on a *measured* point.** ADR-0042 keeps `setParam` `void`
  because aborting at a width leaf was measured to be **worse**: `Bands 3 mask 0x5 wLo 1.750`, the
  intended layout with the newer authority's width standing. Aborting because a host wrote a width
  leaves the **old count** with an **already-remapped mask** — the very incoherence F1 is about.
  Two of my own regression legs were asserting that wrong behaviour.
* **SP-7 — it left open the window it most needed to close.** The prefix proof sat immediately
  before `setBands`, *outside* its gesture bracket. `setBands` calls `beginChangeGesture` before its
  guard, and that dispatch reaches every listener, so a host answering the gesture open still
  committed a count over a mask the transaction no longer owned. That is leg G, and it fails on the
  wide fix.

**Why the asymmetry is right, stated once so it is not "fixed" again.** `mbWidthLow` means band 0's
width under **either** topology; `mbFreqLow` is split 0 under either. A host writing them mid-burst
is a newer authority on a value the count does not reinterpret, and ADR-0036 §25 says the newer
authority wins. `mbSolo` is the exception because `SoloMonitor` masks it *with the count*: the count
store is what changes its meaning. Legs B and C are now **controls** asserting that a width or split
replaced mid-burst does **not** abandon the transaction and that the newer value stands.

## 4. The decision, and how the two mechanisms were told apart

ADR-0044. Two changes, and the mutation matrix is what showed they cover different windows:

| Mutation | Killed by | What that proves |
|---|---|---|
| Q1 — the in-loop `soloMask() != nm` checks removed | **leg F only** | The in-loop checks buy **residue reduction**, not commit correctness: `setBands`'s own guard still catches leg A |
| Q2 — `setBands` stops proving `expectedMask` | **leg G only** | The gesture-open window is real and no caller-side check reaches it |
| Q3 — the call sites pass `-1` | **leg G only** | Same window, from the other side |
| Q4 — both removed | **legs A, F and G** | Leg A is doubly covered; only removing both reproduces the original measurement |

## 5. F2 — abandonment retains partial remapping. TRUE, and it stays true

Partial application is unchanged and remains deliberate: the alternative is a compensating rollback,
which writes a stale value over a newer authority (ADR-0036 §25) and cannot terminate
deterministically, since its own stores dispatch and can be answered again. What changes is the
**shape** and the **size** of what is left behind:

| | before | after |
|---|---|---|
| worst case | **new count** over a solo word in the old numbering — the layout is reinterpreted wrongly and a soloed band goes silent | **old count** over a partial prefix — every value is read under the topology it was written for |
| stores landing after the divergence is observable | the rest of the transaction (measured: three, leg F's shape) | none — the next check returns |

**The recorded store census is one too high, and this is the second time it has been wrong.** The
in-source comment says *"an add at N = 3 issues nine stores and a removal at N = 4 issues seven, so
the largest residue is eight and six"* — itself an ADR-0042 correction of an earlier *"at most one"*.
The audit re-counted from the shipped code: an add at N = 3 issues **eight** value stores, because
the first `ins + 1` width slots are always elided (`nw[i] == wd[i]` below the insertion), so the
largest residue is **seven**, not eight. Corrected in place, and the correction is called out rather
than substituted silently.

**Why the residue is survivable rather than dangerous**, re-derived from the DSP rather than assumed:
`MultibandWidth::setCrossovers` clamps every split to `[20 Hz, 0.45·sr]` and forces strict `1.1×`
ordering *before* use; `SoloMonitor::process` masks the solo word with the live count;
`setBandCount` clamps to `[1, 4]`. A partial layout is audibly wrong and repaired by the user's next
edit — never illegal, never NaN, never unbounded.

## 6. F3 — the held-audition guard, and a second reason it has no test

The previous round recorded the reason as *"`tick()` is driven only by `juce::VBlankAttachment` and
the harness never gets a vblank"*. That is true and **incomplete**. `SpectrumImager::tick` opens
with

```
if (! isShowing()) { wasShowing = false; return; }
```

and `tests/state_tests.cpp` constructs the editor but never **shows** it — no peer, no desktop — so
`isShowing()` is false on all three CI platforms. Even a public `tick(dt)` called directly from a
test would return at that line without reaching the guard. The disclosure understated the depth of
the gap, and that is corrected in `TESTING.md`.

**Determination.** The guard itself is correct and minimal — one call to the same predicate
`mouseDrag` and `mouseUp` already consult. Reaching it from a test needs one of:

* **(a) make `tick` public** — insufficient on its own, per the above;
* **(b) split the hold promotion into its own method and make that public** — a production seam that
  exists only for the test. It is the smallest change that yields *real* coverage, and it is a
  public-API change to production for test reasons, which is the maintainer's call and not one to
  slip into a review round;
* **(c) show the editor in the harness** (`addToDesktop`) — changes the harness's contract on every
  platform, including headless macOS and Windows runners, for one guard;
* **(d) leave it under ADR-0025** with the disclosure corrected. **Chosen for this round**, with
  (b) named concretely as ADR-0025 §5's "what would close it".

**One observation the investigation turned up and did not act on.** A correctness guard living
behind a *visibility* gate is a shape worth noticing: if the editor is hidden while a solo is held
(Simple mode hides the imager), `tick` returns early and the staleness question is never asked. The
press is orphaned rather than stale-audited. Recorded, not chased — it is outside this round's brief
and needs its own reproduction before anyone calls it a defect.

## 7. F4 — cross-thread partial layouts. Re-evaluated, not expanded

Unchanged, and the reason is the one ADR-0042 established rather than a restatement of ADR-0041's:
`PluginParameters::toEngine` reads the ten multiband atomics with **ten separate `load()` calls**
per block, with no seqlock or generation guard, so the **reader tears**. A write-side atomic
topology commit would be re-torn on the audio side and would buy nothing. Making it real means
replacing the *read* — a DSP-parameter and threading-model change, and so an
`ARCHITECTURE_REVIEW_GATE` item. Recorded; not attempted; no rewrite begun.

## 8. Options, as the brief put them

| | Verdict |
|---|---|
| **A. Keep the conditional-store model, document residuals** | Rejected for the mask. The three measurements are not a residual — they are a **completed** transaction publishing a layout that never existed |
| **B. Stronger transaction ownership** | Chosen, but only after being cut down: the wide form contradicted ADR-0042 on a measured point (§3) |
| **C. Rollback** | Rejected. Writes a stale value over a newer authority (ADR-0036 §25); cannot terminate deterministically |
| **D. Snapshot / version architecture** | Rejected **here**, on reach rather than taste: pointless while the reader tears (§7). An `ARCHITECTURE_REVIEW_GATE` item |
| **E. Other** | What shipped: one integer comparison on the one value whose meaning the count changes, carried into the count store's own bracket |

## 9. Validation

State **2 733 / 0** · DSP **396 / 0** · `check-realtime` 47/0 · `check-portability` 57/0 ·
`check-docs` 131 clean · `check-citations` clean · `git diff --check` clean. No lock, no allocation,
no blocking, no audio-path change, no parameter-model change, no sanitizer finding suppressed.

---

# Final review round (2026-09-08, second half) — the sub-agent audit, consumed and acted on

## 10. Which agents were checked, what they held, and what was done

| Workflow | State | Contents | Action |
|---|---|---|---|
| `wf_17153265-ac9` (this round's) | running, 12 area results + verify verdicts landed | the reconstructions below | **consumed in full** |
| `wf_b53560a1-266` | stopped last round | RO-1 write-path audit; conclusions shipped as ADR-0043 | no unresolved item; left stopped |
| `wf_5743c961-dad` | ended without a synthesis (120/122) | the reentrant-store round; its reconstructions were read and acted on at the time | sampled; nothing unconsumed |
| `wf_893cff2c-94b` | complete | `kSplitMovedPx`, writer inventory, wheel anchors, solo semantics — all shipped as ADR-0040/0041 | no action |
| sub-agents / other sessions | none (`ListAgents`) | — | — |

The three verify agents on F1-A all **refuted** it — correctly: the brief named `91e20d9`, the tree
was two commits ahead at `8a00946`, and each independently established that the finding was exact
against the named baseline and closed on the shipped head. That is the outcome a verify phase is
for, and it is recorded rather than treated as noise.

## 11. Every consumed result, classified and acted on

### Confirmed defects — fixed, with coverage
| # | Finding | Action |
|---|---|---|
| **Finding B** / W1 / RT-2 | the wheel's `scrollHandle`/`scrollBand` latch survives a band-count change | **ADR-0045**: stamped with `scrollBands`. State test 77 leg A; mutation R1 measured `band 1 moved 1.060 -> 1.120 after the count changed under a hand that never moved` |
| RT-1 | `resetParam` has a gesture bracket with no topology proof inside it | **ADR-0045**: `expectedBands`. Leg C; mutation R2 measured `1.600 / 1.000 / 1.600` |
| TH-1 | `addBandAt`'s ADR-0044 mask re-proof had **zero** coverage — every leg drove `removeBand` | State test 76 **leg H**; mutation R3 measured `Bands 3 with mask 0x8` on the add path |
| TH-2 | leg F did not enforce its own claim: removing the mask check from the WIDTH loop alone left both its assertions true | leg F gains the width assertion; mutation R4 kills it and nothing else |

### Documentation / process — corrected
| # | Finding | Action |
|---|---|---|
| F2-C1…C8 | the store census is wrong in six places; an add at N = 3 issues **eight** stores, not nine, so the standing residue is **seven** | corrected in `ADR-0042`, `ADR-0044` and the in-source comments; both halves now counted the same way (stores that stand), which is the discrepancy that let it drift |
| DC-6 | an **abandoned** transaction gets no duck: its residue arrives on the continuous path and stands, where a completed mixed layout would at least have been ducked into | recorded in ADR-0044 — this is the sharper statement of F2's cost, and the reason the in-loop checks earn their integer read |
| RT-3 | the previous worklog's *"removes its only single-threaded trigger"* overstates the ADR-0043 fix | corrected in place, §7 |
| F3-2 / F3-3 / TH-5 | the ADR-0025 disclosure understated its own gap, and the seam it named would not work | corrected in `TESTING.md` in the previous half of this round; re-confirmed here |
| F2-D1 | *"ADR-0044 has no row in ADR_INDEX"* | **false positive** — the agent read a stale snapshot; the row is present |

### Architecture trade-offs — decided
| # | Finding | Decision |
|---|---|---|
| F4-A…D | the DSP's ten-load multiband snapshot | **accept the trade AND escalate**, which is the answer to a question posed as A-or-B: it is now **RISK-010** in `FUTURE_RISKS.md`, named as an `ARCHITECTURE_REVIEW_GATE` item, because the only real fix replaces the READER. F4-D is new and worth keeping: store-count-last plus read-count-first makes "new count over old values" unreachable, so only the benign direction remains (wording corrected 2026-09-08, §31: the count is not read *first* -- `mbEnable` precedes it -- but it is read before every parameter it reinterprets, which is what the argument needs) |
| F3-1/F3-4 | the held-audition guard's coverage | **B — accepted GUI-only gap.** Option (b), extracting the promotion into a public method, is the only thing that would work (`tick` returns at `isShowing()` before the guard), and it is a production seam existing solely for a test. ADR-0025 §5 keeps it revisitable |

### False positives — no action, reason recorded
| # | Finding | Why not |
|---|---|---|
| **W2** | *"`soundMovedUnderGesture` loops the full arrays and voids a gesture for a slot the topology does not use"* | Implemented, and it **broke State test 71 leg G**, which deliberately requires a foreign write to slot 2 at two bands to stop the drag. The plan is `projectFromOrig` over ALL slots from a capture that seeded ALL of them, and the count can rise at any moment. Reverted; the rejection is recorded beside the predicate so it is not re-proposed |
| **Finding A** | *"`commitFreqEditor` only rejects disappeared handles; a surviving index may refer to a different topology"* | **Not a defect**, and the audit's own area agent (RT-4) says so independently. The ADR-0044 asymmetry decides it: `mbFreqLow` is split 0 under every topology, so a surviving index names the same parameter — unlike `removeBand`'s band index, which ADR-0039 had to refuse because a surviving index there RETARGETED the operation. What must be proved is that the split still exists (the bounds line) and that the plan comes from the live layout (ADR-0043 moved it inside the bracket, where `M` is re-read and `i < M` proves the handle a second time). The reasoning is now in the source at that line |
| PR-5, TH-6, DC-1, DC-7, W4, W5, U5 | various | each states plainly that the shipped code is correct; no change |

### Informational — recorded, unchanged
* **U4** — a mouse-wheel Width edit produces **no undo step at all** (`setParam` opens no gesture). Real
  and user-visible, and deliberately **not** fixed here: the brief says *preserve existing wheel
  behaviour*, and adding gesture brackets to the wheel changes it. Recorded for a future round.
* **U1/U2/U3** — `undo()`/`redo()` pass a member by const reference into `applyStateSet`, and a
  re-entrant poll can split one topology transaction into two undo steps whose midpoint is a layout
  the user never had. In `PluginProcessor`, outside this round's subsystem; recorded, not chased.
* **DC-3/DC-4** — widths are consumed by slot and crossovers are force-ordered, so a numbering
  mismatch puts a wrong-but-legal width on a real band. Already the accepted residue's shape.
* **cancelled spread handle ordering**, **parameter-space ownership comments** — reviewed, unchanged.

## 12. A correction to this round's own reporting

I read *"2749 checks, 0 failure(s)"* from a **stale binary** after restoring a mutation, and reported
leg H as passing when it had never passed. It was found by re-running, not by anything else. The leg
needed the add area located by tooltip rather than assumed at mid-plot; it now kills mutation R3.
Recorded because the same class of mistake — trusting a test binary that had not been relinked —
has now happened twice in this PR.

**And a second one, in the other direction: a false alarm I raised myself.** The final valgrind run
reported `390 checks, 1 failures` on the DSP suite — *"engine output free of NaN/Inf/denormals"* —
while the same binary passed natively and `memcheck` reported `ERROR SUMMARY: 0 errors from 0
contexts`. It is not a defect and it is not this branch's: `AnamorphTests` links **zero**
`SpectrumImager` symbols and the DSP sources are byte-identical to the merge base. valgrind emulates
floating point and does not honour the FTZ/DAZ bits `juce::ScopedNoDenormals` sets, so denormals
survive into the output and that one assertion fails on a build that is correct on every real CPU.
`.github/workflows/build.yml` sets **`ANAMORPH_TESTS_NO_FTZ=1`** for exactly this step, and
`TESTING.md`'s own recipe row says so — **my local invocation just did not follow it**. Re-run as CI
runs it: `ALL TESTS PASSED`, and the binary announces its own relaxation
(*"the denormal invariant was NOT asserted"*). No repository change is warranted; what this cost was
one round of investigation, and the earlier valgrind DSP runs in this PR that passed without the
variable did so by luck of denormal production, not by method.

---

# Closure round (2026-09-08) — CI resolution, remaining findings, merge decision

## 13. Workflow inventory, with the decision recorded rather than assumed

| Id | Purpose | State | Results | Consumed? | Remaining value | **Decision + reason** |
|---|---|---|---|---|---|---|
| `wf_17153265-ac9` | this PR's topology-transaction audit: 13 areas → 3-lens adversarial verify → A–E judging | running; 12 area results + verify verdicts | yes, all read | the verify/judge tail on findings **already ruled** | **Consume and stop.** Benefit of continuing: re-verification of findings whose disposition is already implemented, tested and mutation-proved. Cost: a 4-core box at 2 concurrent agents for hours, against a round whose purpose is a merge decision. The area results — the load-bearing half — are all in. Stopped |
| `wf_b53560a1-266` | RO-1 write-path audit | stopped last round | 45 | yes → ADR-0043 | none | leave stopped |
| `wf_5743c961-dad` | reentrant-store round; **ended without a synthesis** (120/122) | complete-but-unsynthesised | 120 | yes, at the time | none — its reconstructions shipped as ADR-0042 | no action; recorded because "ended without a synthesis" is exactly the case worth re-checking |
| `wf_893cff2c-94b` | `kSplitMovedPx`, writer inventory, wheel anchors, solo semantics | complete | 24 | yes → ADR-0040/0041 | none | no action |
| background tasks | TSan, valgrind, CI monitors | all completed | — | yes | none | none running |
| sub-agents / sessions | — | none (`ListAgents`) | — | — | — | — |

## 14. The CI failure — code regression, mine, fixed

**Exactly one failing step in the whole matrix**, and it was never the platforms: `linux` step 20,
*"Gate first-party Clang warnings"* —

```
-Wsign-conversion in tests/state_tests.cpp: 8 site(s), baseline allows 0.
```

`tsan`, `sanitizers` (ASan+UBSan **and** valgrind), `realtime`, `linux-lto-tests`, `fuzz`, `docs`,
`source-lint`, `windows`, `windows-avx2-ab`, `macos`, `macos-intel` and `macos-crossslice` all
succeeded.

**Classification: code regression, and mine.** All eight sites are `std::array` indexed with `int` in
**State test 77**, which I added last round. Reproduced locally by extracting the TU's own compile
line from ninja and re-running it under `clang++-18` with `-Wsign-conversion`: 8 sites before, 0
after. The remaining first-party Clang warnings in both changed TUs are exactly the baseline's
(`src/PluginEditor.h` `-Wshadow-field` 1; `src/dsp/ScopeBuffer.h` `-Wsign-conversion` 2).

**Why it escaped me.** My local gate is GCC, and `check-gcc-warnings.py` gates five flags that do not
include `-Wsign-conversion`; the Clang gate has a different, larger set and is the one that runs on
`linux`. `check-clang-warnings.py` pins `--clang-major 22` and this container has 18, so the script
itself declines locally — which is correct behaviour and not a substitute for compiling the changed
TU with the flag. **The procedure that would have caught it is the one used to fix it**: take the
TU's compile line from the build and re-run it under clang with the gated flags. Worth doing on any
change that touches `tests/` or `src/`.

**The `pull_request`-event run is not evidence.** For the same SHA the PR run reported success and
the push run reported failure. Every heavy job carries
`if: github.event_name != 'pull_request' || …head.repo.full_name != github.repository`, so on a
same-repo PR they are **skipped** — the PR run is green because it did nothing. The push run is the
one that gates. Recorded because reading the PR run as the answer is an easy and expensive mistake.

## 15. The remaining review findings, ruled

* **§3A topology stamps protect stale targets** (`:2101`, `:2368-2372`) — **already fixed** by
  ADR-0045, and the cited lines on this head contain the fixes. But the review's wording named a
  window the first implementation did **not** close: `resetParam (widthP[b], bandCount())` derives
  the index and *then* reads the count, and `mbBands` is written by the **audio thread** too, so a
  write landing between the two makes the guard agree while `b` is stale. **Tightened**: both call
  sites read the count first and derive from it, so that window fails as a refusal. No UX change.
* **§4 cross-thread partial layouts** (`:725`) — trade-off remains correct and **RISK-010 is the
  right instrument**: the reader is what tears, so no writer-side change is honest, and replacing the
  reader is a threading-model change. Escalated, not silently accepted. No local fix added.
* **§5 held-audition guard** (`:1327`) — **accept the GUI-only gap.** The only seam that reaches it
  is a public promotion method existing solely for a test (`tick` returns at `isShowing()` before the
  guard, so making `tick` public does nothing). Regression risk is low — the guard is one call to a
  predicate two tested siblings already use — and the maintenance cost of a permanent production seam
  is not. ADR-0025 §5 keeps it revisitable when a shown-editor harness lands.
* **§6 informational** — `:354` cancelled spreads: accepted; the DSP force-orders, `handleNearX` is
  order-independent, and the only cost is an unreachable affordance for one band until the next edit.
  `:398` parameter-space equality: intentional and explained at the definition (ADR-0041) — exact
  comparison of a read-back, no epsilon to invent. `:725` partial edits: ADR-0044's residual,
  accepted with its size now counted correctly and its no-duck asymmetry recorded.
* **§7 U4** (a wheel Width edit produces no undo step) — **future work, not a blocker.** Fixing it
  means bracketing the wheel in gestures, which changes wheel behaviour; the round that found it was
  told to preserve that behaviour. It is a pre-existing limitation, not a regression of this PR.
* **§7 U1–U3** (undo re-entrancy in `PluginProcessor`) — **architecture item, future work.** Outside
  this PR's subsystem, unproven by any test here, and a fix touches the undo model. Recorded so it
  is not lost; not a blocker for a PR that does not change that code.

---

# Round 10 — topology identity (ADR-0046)

## 16. Workflow and sub-agent audit, decided on evidence

**What was inspected.** The container had restarted (PID 1 uptime 17 s at the start of the round),
so every workflow, background task and sub-agent from the previous rounds was already gone with it —
`ListAgents` reported none, the task list was 94/94 completed, and `ps` showed no compute-bound
process. There was nothing running to consume, continue or stop, and nothing left unconsumed: the
scratch directory held only the previous rounds' task outputs, all of which had been read.

**What was started, and why.** With no inherited work, the decision was whether to *create* parallel
audit capacity. Two things argued for it: the round asks for previous conclusions to be re-verified
rather than preserved, and it warns against following any single agent. One four-track read-only
workflow (`wf_92fb0c32-f6e`) was launched: an exhaustive ordering sweep, a re-verification of six
recorded conclusions, a consequence analysis of a mis-stamped wheel latch in DSP terms, and a
re-examination of the five accepted residuals — each followed by adversarial verifiers instructed to
default to *refuted*.

**What it found.** 50 claims from the four tracks. The primary finding was independently confirmed
(track 1, W1: the wheel stamp was read after the derivation), and four claims were classed
`needs-action`. Every one of them was then checked in the code by hand before being acted on:

| Claim | What it said | What the code said | Action |
|---|---|---|---|
| V7 | `mouseDown` still lets `handleNearX`/`bandAtX` take their own count reads | true of the file the agent read (pre-edit) | already closed by this round's fix |
| W4 | the wheel's write EXTENT comes from a second live read inside `dragCrossoverTo`, so "one reading decides the tick" was not actually achieved | **true** — `const int M = bandCount() - 1` | **fixed**: `dragCrossoverTo (handle, x, n)` |
| W12 | the wheel's width store proves the topology but not the VALUE, unlike the crossover stores beside it | true, and correct as designed — the wheel is a read-modify-write on the live value, so it *builds on* a foreign write rather than overwriting one; `cancelActiveDrag()` waives ownership for exactly that reason (ADR-0043) | **ruled a false positive**, reason recorded |
| M5 | `bandAddTarget` reads the count after its band index was derived | true; `lo` cannot be affected and the only affected term (`hi`) can only NARROW the range, and `addBandAt` re-proves the count anyway | **no action**, inert |

**And one correction to this round's own work,** which is the reason the workflow paid for itself:
claim W8 caught that the comment I had just written at the wheel's width store copied ADR-0045's
wording — *"an automation touch and an undo step for a band that is not there"* — onto a store that
opens **no gesture**. `parameterGestureChanged` counts gesture opens and `pollUndoCoalesce` turns
the return to zero into the undo entry (`PluginProcessor.cpp:812-825`), so a bare `setParam` makes
**no** undo step at all. That is worse than the claim, not better: the value still reaches the host
and is folded into the committed baseline with nothing to reverse it. The comment now says so, and
says why `resetParam`'s wording is right where it stands.

**The decision to stop it.** At 80 minutes the workflow had produced all 50 claims and 29 of the
~50 verifier verdicts (13 refuted, 16 survived), and was working through verifiers on a snapshot of
`SpectrumImager.cpp` that the fix had since replaced. Continuing would have re-adjudicated claims
already adjudicated by hand, against code that no longer exists, while holding 2 of this box's 4
cores — the same two the TSan and valgrind runs need. **Consumed and stopped** (`TaskStop`), with
the tally recorded above rather than discarded. The judgement is not that the tail was worthless; it
is that its marginal information value had fallen below the cost it was imposing on the gating
validation.

## 17. Finding A — confirmed defect, and worse than the wording suggested

The review located it at `:2377` with `:2368-2372` related. On the head it arrived at, the tick read
the count **three** times and let the derivation take a fourth:

```
const int N = bandCount();                          // 1 -- the write BOUND
if (scrollBands >= 0 && bandCount() != scrollBands) // 2 -- the staleness test
const int h = handleNearX (x);                      // 3 -- inside the helper
scrollBands  = bandCount();                         // 4 -- the STAMP, taken LAST
```

There is **no dispatch** anywhere in that span — `bandAtX`, `handleNearX`, `crossover` and
`bandCount` are plain `getValue()` reads — so reentrancy cannot cross it and only another thread
can. That is exactly the window the review named.

Two failures, in opposite directions:

* **fail-open (the stamp).** An index derived at three bands and stamped with the four that arrived
  a few instructions later claims a topology it was never derived in. `scrollBands` is not
  re-derived for the rest of the burst, so every later tick compares against that claim and
  **passes** — a stamp naming the wrong topology is worse than no stamp, because it silences the
  check that exists to catch this. ADR-0039 already refused this shape for `removeBand`.
* **fail-safe but lossy (the bound).** Reading 1 is taken before the staleness test; a count that
  *rises* in that window leaves a latch re-derived under the new topology bounded by the old,
  smaller one, and the tick writes nothing.

Verdict: **A — confirmed defect requiring implementation.** `mouseDown` never had the fail-open half
because it reads the count at the very top, which is what `SpectrumImager.cpp:2331` relies on
(*"handleNearX and addBandAt both return an index inside the count they read"*) — a sentence that is
only usable if the count they read is the count the caller proved.

## 18. The fix, and what it deliberately does not do

ADR-0046: **a derivation answers under the topology it is given, and a handler reads that topology
once.** `bandAtX`/`handleNearX` take the count to answer under; `mouseWheelMove`, `mouseDown` and
`mouseDoubleClick` each take one reading and use it for the staleness test, the derivations, the
stamp, the write bounds, `gestureBands`, the plan extent inside `dragCrossoverTo`, and the store's
own proof. `setParam` gains `expectedBands`, closing the one store in the class that had no topology
proof at all.

Making the derivation and the stamp *atomic* in the strong sense would need a lock, which ADR-0038
already rejected because `mbBands` is written from the audio thread. Making them **the same read**
needs nothing. Not a threading-model change; not a gate item.

**Not done, in writing:** `soloHit` and `deleteHit` still re-read the count, because threading a
topology into them means threading it through `deleteBox`, `soloBox`, `bandLeftX` and `bandRightX`
as well — six signatures for the fail-safe half only. The line is: fail-open is fixed; fail-safe-
but-lossy is fixed where it costs one argument, and named where it does not.

## 19. Coverage, and the part of it that is honestly untestable

State test 78 (four legs, 12 checks) holds the contract: at four bands four probes steer four
different bands in order; at two bands no probe reaches a band the topology has not got **and** every
probe still steers the band it is over; an alt-click resets no band outside the topology; and the
control still resets exactly one.

| Mutation | Killed by |
|---|---|
| Q1 — the wheel derives under a topology the tick did not prove | State test 77 leg A **and** State test 78 leg B (`steered nothing: 0 1 -1 -1`) |
| Q2 — the Alt reset derives under an unproved topology, bound kept | **nothing** (the bound refuses it first) |
| Q2b — the Alt reset derives *and* bounds under an unproved topology | State test 78 leg C (`reset a width for band 3, which a two-band topology does not have`) |
| Q3 — the wheel's width store loses its topology proof | **nothing** |
| Q4 — the wheel stamps with a later read (the exact pre-fix shape) | **nothing** |

**Q3 and Q4 surviving is the measured result, not an oversight.** The window holds no dispatch, so a
deterministic test cannot enter it: it would have to move `mbBands` from another thread inside a few
instructions and then observe the store and the live count atomically. A stress probe was considered
and rejected — it can assert no invariant across that window that is sound without observing both
together, and a probe that cannot fail for the right reason is worse than none. Q3 and Q4 were run
to establish that claim by measurement instead of asserting it.

Leg B was tightened during the round: its first form asserted only `hit < 2`, which a tick that
steered **nothing** satisfies, and Q1 is caught by the bound as silence rather than as a wrong band.
Both halves are now asserted.

---

# Round 11 — coupled-commit consistency (ADR-0044 amendment)

## 20. Workflow and sub-agent audit

**Inspected.** The container had restarted again (PID 1 uptime 5 m 22 s at the start of the round).
`ListAgents`: none. Task list: 95/95 completed. `ps`: no compute-bound process. No workflow
transcript directories survive, and no task output files remain. **Nothing was running, and nothing
was left unconsumed** — the previous round's workflow (`wf_92fb0c32-f6e`) had already been consumed
and its 50 claims and 29 verdicts recorded in §16 before it was stopped, so its loss with the
container costs nothing.

**Decision: start nothing.** The finding is one precisely-located asymmetry in two adjacent
functions with five call sites; the work is a trace and a measurement, not a search. Fanning it out
would have cost wall-clock on a 4-core box and returned claims about code I can read in full. Recorded
because the alternative — spawning a workflow reflexively because the previous round did — is the
failure mode this audit step exists to prevent.

## 21. The finding, measured

`setBands` and `setSoloMask` each prove BOTH the count and the mask on the **near** side, adjacent to
the store (ADR-0044), and each re-reads only its **own** parameter on the **far** side
(`SpectrumImager.cpp:639`, `:662`). Both halves of the review's wording are true of the code.

A temporary probe drove the real component with `WriteFromInsideAStore` on each parameter:

```
[leg 1] solo click, count dropped inside the mask store   -> Bands 2, mask 0x8, live-solo 0x0
[leg 2] removal, mask re-asserted inside the count store  -> Bands 3, mask 0x8, live-solo 0x0
[leg 3] control, unprobed removal                         -> Bands 3, mask 0x4, live-solo 0x4
```

`live-solo 0x0` looked at first like the `Bands 3 with mask 0x8` incoherence ADR-0044 named, and I
recorded it as such before finishing the measurement. **That reading was wrong**, and the round trip
is what showed it:

```
[leg 1] far-side window   -> Bands 2, mask 0x8, live 0x0  -> count returns -> mask 0x8, live 0x8
[leg 1] ADR-0039 control  -> Bands 2, mask 0x8, live 0x0  -> count returns -> mask 0x8, live 0x8
```

Identical, and the control uses **no reentrancy anywhere** — it is a plain count drop. So the state
the far-side window publishes is ADR-0039's deliberately parked solo bit: inert while hidden
(`SoloMonitor.cpp:85` and the painter both mask with `((1 << bands) - 1)`) and exact when the count
returns. The window publishes nothing the plug-in does not already publish by design.

## 22. Classification: **B — already prevented; invariant documented**

Two things cover the far side, neither inside the two functions:

1. **The callers.** Every caller that ACTS on the result re-proves the other parameter on its next
   line with only pure reads in between (`addBandAt:839`, `removeBand:953`). The rest discard it
   (`removeBand:965`, both solo-click sites) or use it only for mask-independent values
   (`addBandAt:867` → `resultingBands`, `ins`).
2. **ADR-0039's parked bit**, measured above.

**Cross-reading the other parameter on the far side was evaluated and rejected on evidence**, not
preference: it would return `false` when the count *did* commit, so `addBandAt` would abandon a
successful add because a foreign writer touched the mask afterwards — the "aborting at a leaf is
measured worse than completing" class ADR-0042 recorded at `Bands 3 mask 0x5 wLo 1.750`. A more
precise report bought with worse behaviour.

Shipped: the invariant stated at both functions, ADR-0044 amended, and **State test 79** (four legs,
16 checks) pinning it. Leg C is the guard on point 1.

**Mutation record, reported as defence in depth rather than dressed up as a single-line proof:**

| Mutation | Result |
|---|---|
| M1 — remove the one re-proof leg C names (`addBandAt:839`) | **survives**; `setBands`' near-side `expectedBands` is a second layer |
| M2 — remove both loop count re-proofs *and* that guard | **kills leg C**: `the add committed its count (Bands 3) after the topology it proved had already moved to 4 inside the mask store`, and kills State test 76 leg H alongside |

No single-line mutation proof exists here and none is claimed.

**No CHANGELOG entry.** `CHANGELOG_POLICY` scopes the file to user-visible changes; this round ships
comments, an ADR amendment and a test, and changes no behaviour. Recorded so the absence reads as a
decision rather than an oversight.

## 23. The re-evaluations the round required

* **ADR-0046** — **confirmed correct.** Untouched by this finding: ADR-0046 is about a handler taking
  one topology reading for its derivations; this is about what a store re-reads after its own
  dispatch. Different mechanism, no interaction.
* **RISK-010** — **confirmed correct, unchanged.** This is a message-thread *reentrancy* window
  (listener dispatch); RISK-010 is the audio thread's ten-load *cross-thread* read. Different
  mechanism. The round does reinforce RISK-010's existing statement that the DSP-side masking is load
  bearing — legs A and B measure exactly that — but the risk text already says so and needs no edit.
* **Held-audition gap** — **remains accepted.** Re-verified at this head: `tick()` returns at
  `if (! isShowing())` (`SpectrumImager.cpp:1286-1289`) before reaching the guard at `:1352`, and the
  harness never shows the editor. No new evidence; no production seam added.
* **U4 (wheel Width undo)** — **remains an accepted residual; does not block merge.** Measured for
  the first time, side by side on the same parameter: a wheel tick moved `mbWidthLow` 1.000 → 1.180
  with `canUndo()` **false**; a width drag moved it 1.000 → 1.750 with `canUndo()` **true**. So it is
  a real, everyday, race-free inconsistency between two ways of editing one control. Deferred anyway,
  on evidence: it is **pre-existing** (the merge base carries the same gesture-less `setParam`), it is
  a *missing* undo entry rather than a wrong value or an incoherent state, and it is already recorded
  completely in `KNOWN_ISSUES.md` with a scope decision and the open UX question a fix must answer
  (a gesture per tick or per burst). That question belongs to its own change; the KI entry gained the
  measurement and nothing else, because the entry was already correct.
* **Informational items** — `:390` cancelled spreads, `:435` ownership parameter equality, `:725`
  partial transaction residue: **reviewed, unchanged.** Confirmed byte-identical to `5c7f225`; this
  round's two hunks are both comment insertions at `:639` and `:682` and touch none of them.

## 24. The test's own TSan regression, and why it was not suppressed

The first version of State test 79 **failed the `tsan` lane**, and the full-suite run is what caught
it — not review:

```
WARNING: ThreadSanitizer: lock-order-inversion (potential deadlock)
  Cycle in lock order graph: M0 => M1 => M0
  ... WriteFromInsideAStore::parameterValueChanged -> setSoloMask -> toggleSoloBit -> mouseUp
```

Legs A and C nest the two parameters' JUCE `listenerLock`s **solo → bands**; leg B nested them
**bands → solo**. Both orders on the main thread close a cycle in TSan's lock-order graph. It is the
RISK-009 shape exactly — harmless, because a single thread takes both orders at different times and
no deadlock is possible — but `halt_on_error=1` stops the job, and the one entry in
`tests/tsan-suppressions.txt` names `WriteFromInsideAGestureOpen`, so it does not match
`WriteFromInsideAStore`.

**Widening the suppression was rejected.** The round that reduced that file to a single entry did so
because a second one *matched nothing* and would only widen what a future report can be absorbed by;
adding an entry now that it does match would trade a detector for a green lane. Instead leg B was
given **its own processor**, constructed while the first is still alive so the mutex addresses cannot
be recycled. Distinct mutexes, no cycle, nothing suppressed, and the leg keeps its full assertion set.

Recorded because the tempting fix was the wrong one, and because it is a second instance this round
of the same lesson: the measurement, not the reading, is what settles these.

## 25. The `macos` CI failure — infrastructure, and the latent fragility behind it

Run 34235865436 attempt 1 failed one job, `macos`, with all four pluginval steps dying in **under a
second each**:

```
Fetching pluginval (pluginval_macOS.zip)...
100  157k  100  157k    0     0  1394k
  End-of-central-directory signature not found.
unzip: cannot find zipfile directory in one of pluginval.zip or ...
##[error]Process completed with exit code 9.
```

**Classified infrastructure, on evidence rather than convenience:**

* the download *succeeded* and returned **157 KB** — `pluginval_macOS.zip` is megabytes, so what
  landed was an HTML error or rate-limit page from the release CDN, not an archive;
* the plug-in is never loaded — the failure is in fetching an external tool;
* `macos-intel` ran **the same four pluginval steps** against the same plug-in and passed;
* `linux` and `windows` pluginval passed;
* `macos`'s own steps 9 and 10 — the self-tests on arm64 **and** the x86_64 slice under Rosetta —
  passed, so the code is fine on that platform;
* the previous run (`5c7f225`) passed `macos` twenty minutes earlier on the same code paths;
* this round's diff is comments plus one test, which cannot affect a `curl` of a release asset.

One re-run was taken, which is the whole allowance for a failure that is not this PR's: attempt 2
returned **success**. Confirmed transient.

**The latent fragility is real and is NOT fixed here.** `scripts/run-pluginval.sh:477-478` is

```bash
curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/$PV_ZIP" -o "$TOOLS_DIR/pluginval.zip"
(cd "$TOOLS_DIR" && unzip -o pluginval.zip >/dev/null)
```

`curl -L` **without `--fail`** exits 0 on an HTTP error page, so the error page is written to
`pluginval.zip` and the job reports a confusing *"End-of-central-directory signature not found"*
instead of *"the download failed"*. A `--fail` plus a size or magic-byte assertion would turn a
misleading archive error into an accurate one, and the repository already has the matching
self-test culture (the pluginval verdict self-test, the warning-gate self-test, the ABI floor
self-test). It is deliberately left for its own change: it is CI tooling, it needs a self-test of
its own to meet the standard those siblings set, and bolting it onto a topology-consistency round
is the scope creep this series has been avoiding. Recorded here with the exact mechanism so the
next person does not have to re-derive it from a one-second job failure.

---

# Round 12 — wheel gesture semantics and the suppression match assertion

## 26. Workflow and sub-agent audit

**Inspected.** Container restarted again (PID 1 uptime 1 m 01 s). `ListAgents`: none. `ps`: nothing
compute-bound. No workflow transcript directories, no task output files. Task list 96/96 completed.
**Nothing running, nothing unconsumed.**

**Decision: start one read-only audit workflow** (`wf_366cbb73-956`, four tracks + adversarial
verifiers) — the opposite of the previous round's decision, and for a reason rather than a mood.
Round 11's finding was one asymmetry in two adjacent functions with five call sites: a trace, not a
search. This round's Finding 1 asks what a single statement does to *four* separate subsystems —
host automation gestures, the undo coalescer, drag continuity, and interaction semantics against the
merge base — which is exactly the shape that repays independent readers. Recorded because "audit
workflows" is not a ritual: it earns its place when the question is wide, and does not when it is
narrow.

## 27. Finding 1 — wheel input closes active gestures: **B, intentional; the consequences were the
gap, not the behaviour**

`cancelActiveDrag()` is the first statement of `mouseWheelMove` (cited by function: the line moved
three times inside this PR, §31 and §35) and it
calls `endGesture()` on the dragged parameter.

**Provenance, measured.** It is NOT in the merge base — `git show <merge-base>:src/gui/SpectrumImager.cpp`
shows `mouseWheelMove` opening straight at `const int N = bandCount();`. It was introduced by
**ADR-0041** (`e247c11`), an early round of this PR, **not** by the recent ADR-0045/0046 topology and
wheel work the review suspected. ADR-0041 §Consequences already states it as a product decision:
*"A wheel tick during a drag ends the drag. New, deliberate, and stated as a product decision."*

**So the behaviour is intentional and documented. What was NOT documented is what it costs** — and
that is precisely what the review asked for. Measured on the real component:

| | wheel tick mid-drag | control, no tick |
|---|---|---|
| `mbWidthLow` | 1.000 → 1.375 → **1.495**, further drag leaves it **1.495** | 1.375 → **2.000** |
| host gestures | 1 open / 1 close, **closing at the tick** | 1 open / 1 close at mouseUp |
| `canUndo()` | 0 mid-drag → **1 at the tick** | 1 at release |

So: the host sees the automation touch released early; the drag so far is committed as its **own undo
step**; and the held press is **dead** until release and re-press. All three intended, none written
down.

**Shipped:** the three consequences stated at the call site and in an ADR-0041 amendment; a
**CHANGELOG `### Changed` entry**, because this is a user-visible interaction change against the
merge base and `[0.9.8]` had none — the two existing wheel entries are about stale topology targets,
not about scrolling ending a drag; and **State test 80** pinning all three directly.

**Mutation M1** (delete the `cancelActiveDrag()`) kills three checks: State test 73 leg A reproduces
ADR-0041's original measurement verbatim — `a wheel tick adopted the installed width 1.700, and the
drag then wrote 0.650 from an anchor taken before it` — plus State test 80's gesture-timing and
undo-step assertions. **Reported honestly: leg A's "the press is dead" assertion SURVIVES M1**,
because with the press alive the next drag write is refused by ADR-0040's `ownsWidth` check instead.
The three assertions have different sensitivities and only two catch this removal.

**A test defect of my own, caught by the test:** leg B first shared leg A's processor and failed,
because `canUndo()` is cumulative — leg A had already left an entry, so leg B could never show the
mid-drag → after-release transition it exists to assert. It was measuring leg A's history. Leg B now
runs on its own processor.

## 28. Finding 2 — TSan suppression match assertion: **A, added; but the review's mechanism was
refuted first**

The review asked whether "helper rename or pattern drift could silently disable the intended
suppression". **Measured, and it cannot** — that mode is loud:

| mode | measured result | |
|---|---|---|
| entry renamed / pattern drifts, so the inversion resurfaces | **exit 66**, report returned | **LOUD** |
| a **dead** entry sits alongside the live one | **exit 0**, `Matched 1 suppressions` with **2** entries in the file | **SILENT** |

So the review's stated worry is a false positive, *and* there is a real silent mode next to it — the
one `tests/tsan-suppressions.txt` itself calls dangerous in its own header: *"An entry that matches
nothing is not free — it silently widens what a future report can be absorbed by."* The file stated
the rule and nothing enforced it.

**Shipped:** a `tsan` job step asserting **match-count == entry-count** (not "at least one", which
would miss exactly the silent mode). Self-tested against all three measured logs before shipping:
passes on the live file (1/1), fails on the loud mode (1 expected / 0 matched) and on the silent mode
(2 expected / 1 matched). This is the house standard — every other gate here carries a canary or
self-test proving it can fail.

## 29. Verify-only items — all unchanged

* **RISK-010** — unchanged. This round touched no reader and no threading; the ten-load snapshot and
  its accept-and-escalate disposition stand.
* **Held-audition guard** — unchanged. Re-verified at this head: `tick()` still returns at
  `if (! isShowing())` before reaching the guard, and the harness never shows the editor. No
  production seam added.
* **U4** — unchanged, and *reinforced* rather than reopened. This round's Finding 1 measurement shows
  the wheel's own width store still creates no undo step (the undo entry at the tick belongs to the
  **drag** the wheel finished, not to the wheel's edit). That is consistent with U4 and with
  `KNOWN_ISSUES`; the wheel gesture design is not reopened.
* **Informational items** — cancelled spreads, ownership parameter equality, partial transaction
  residue: reviewed, no new evidence, unchanged.

## 30. Two things the audit workflow added, and one honest limit on the new assertion

**A fourth consequence of Finding 1, which I had not measured.** The workflow's track 1 asked what a
tick does to a held *solo* or *delete-x* press, not just a drag. `cancelActiveDrag()` clears
`soloPressBand` and `pressDeleteBand` as well, so the tick **swallows that click**. Measured: press a
solo, scroll, release → mask `0x0`, against `0x1` for the uninterrupted press. Ruled the **same
intentional rule**, not a second finding and not a defect: ADR-0041 leg (b) already establishes that a
solo click whose world moved under it writes nothing, and a wheel tick moves the world — so letting
the toggle fire after the tick is the defect this ADR closed. Documented at the call site and in the
ADR amendment; behaviour unchanged, per the brief's instruction not to change behaviour without
evidence, and the evidence points the other way.

**A residual of the new suppression assertion, stated rather than glossed.** Match-count ==
entry-count catches the two modes measured in §28 — a dead entry (silent) and a drifted one (already
loud). It does **not** catch a third: an entry *broadened* by a future edit that still matches exactly
one report, but a wider class of them. The data-race canary cannot see that either, because it proves
only that data races are still reported, and this is a `deadlock:` entry. What guards it today is the
file's own header — *"a wider entry is worse than none"* — and review. Recorded as a known limit of
the assertion rather than left for someone to discover by trusting it too far.

**Workflow disposition.** `wf_366cbb73-956` produced 34 claims across four tracks. Its two
`needs-action` items that survived verification are the two above; its provenance track independently
confirmed that `cancelActiveDrag()` entered at `e247c11` (ADR-0041) and appears as unchanged *context*
in the ADR-0045 and ADR-0046 diffs, which is the direct refutation of the review's hypothesis that the
recent topology work introduced it. Two claims were refuted on verification (a USER_MANUAL
contradiction, and a report of the suppression file being mutated — that was this round's own
temporary measurement, since restored). Consumed in full.

## 31. The audit workflow's late result, and the one §29 conclusion it overturned

`wf_366cbb73-956` returned its full 34-claim result after §30 was written from the streamed
partials. Reading it end to end turned up two verified items that §29 had disposed of as "reviewed,
no new evidence, unchanged", plus two stale anchors of my own. All four are documentation; no
behaviour changed and no test changed.

**§29 was WRONG about the ownership-equality informational item.** I recorded it as needing no
action. It does. `ownsSplit` compares exactly and is correctly described at its own definition
(`SpectrumImager.cpp:398-408`) and in the header (`SpectrumImager.h:384-390`) -- but the DEFINITION
COMMENT of `kSplitMovedPx` (`SpectrumImager.cpp:14-17`) still said `soundMovedUnderGesture` compares
against that constant and that "the two must be the same number". ADR-0041 removed the coupling;
`git log -L 14,18` shows that paragraph unchanged since `6e37e6e` (the ADR-0039 round that wrote
it). It is the **third** copy of a defect this PR already found and corrected by name twice, at
`:398-401` and `SpectrumImager.h:384-387` -- both of which end with "a reader arriving here was told
the opposite of what the code does" -- and it states the falsehood more strongly than either, at the
site a reader reaches first. Corrected in place, line-count-neutral so no citation anchor into that
file moves.

**A second header sentence that is literally false, fail-safe rather than defective.**
`SpectrumImager.h:351` said "Every write this gesture makes refreshes these" of `gestureX`. Two of
the class's own stores do not: `resetCrossover` (`:774`) and `spreadSplits` (`:387-388`) take the
read-back into a local, so an Alt-click reset inside a latched gesture makes `ownsSplit` report the
gesture's own store as foreign and the timer cancels it. The direction is safe and the cost is
already documented at the timer (`:1380-1382`: one int store, fires once, no repaint), so this is a
comment correction, not a code change. Recorded rather than quietly fixed, because the previous
round asserted the invariant without qualification.

**RISK-010's bound was one word stronger than the code.** `FUTURE_RISKS.md` said `mbBands` is "read
**first** by `toEngine`". It is not: `e.mbEnable` loads at `PluginParameters.cpp:365`, one line
ahead of the count. Harmless -- no topology transaction writes `mbEnable` (its only writers repo-wide
are the toggle attachment and preset load), so every parameter the count reinterprets is still read
strictly after it -- but the accurate wording is "read before every parameter the count
reinterprets". Corrected in `FUTURE_RISKS.md` and at the worklog's own echo (§ Architecture
trade-offs). The accept-and-escalate decision is untouched.

**Two anchors of mine went stale inside my own commit.** `DOCUMENTATION_COVERAGE.md` and §27 both
cited the wheel's `cancelActiveDrag()` at `SpectrumImager.cpp:2433`; the 22-line comment I added
above it in the same commit pushed the call to `:2455`. The citation gate could not catch these --
they are new anchors, unverifiable against `origin/main` -- which is the same blind spot recorded in
§18. Re-aimed by hand.

**One workflow claim declined.** W-A's precision note that a band move closes "0, 1 or 2 gestures,
not always 2" (`beginBandMove:708-709` sets `soloMoveLeft`/`soloMoveRight` to `-1` at the ends) is
correct about the code, but no shipped text of mine claims two, so there is nothing to correct.

## 32. Workflow and sub-agent audit (split-snapshot round)

**Collected first.** No workflow was running (`ps` shows no agent, no build, no probe). Three journals
exist from this session's earlier rounds:

| run | state | results in journal | recorded disposition | what I did with it now |
|---|---|---|---|---|
| `wf_366cbb73-956` | finished; its result arrived LATE and overturned a shipped conclusion (§31) | 38 | "consumed in full", then corrected | nothing left; §31 closed it |
| `wf_17153265-ac9` | stopped mid-run by decision, tail unread | 70 | "consume and stop", tail judged redundant | **re-opened for audit** — see below |
| `wf_5743c961-dad` | ended without a synthesis | 120 | "sampled; nothing unconsumed" | **re-opened for audit** |

**The decision, and why it is not the same as last round's.** §31 established that "recorded as
consumed" is not evidence of having been consumed: a workflow whose result file is zero bytes was read
only through its streamed partials, and one such result later contradicted a shipped conclusion. Both
remaining journals are in exactly that state. Re-reading them costs one agent; the failure mode it
guards against has already happened once in this session, on this PR. **Decision: start ONE new
read-only audit workflow (`wf_d3bf30b3-246`) whose sixth track re-reads both journals end to end and
reports only the items that are still unaddressed at HEAD.** Continuing the stopped run was rejected
for the reason it was stopped — its remaining agents re-verify findings already implemented and
mutation-proved — and doing nothing was rejected on the §31 evidence.

## 33. The finding, measured before it was classified

**"Split snapshots can launder automation"** (`SpectrumImager::captureDragOrigins`). The plan basis and
the ownership stamp were two separate reads of the same parameter:

```cpp
for (int k = 0; k < 3; ++k) dragOrigX[k] = freqToX (crossover (k));  // read A -- the PLAN
captureGestureSound();                                               // read B -- the STAMP
```

`crossover (i)` is `freqP[i]->convertFrom0to1 (freqP[i]->getValue())`, so both reads hit the same
parameter object. Neither dispatches, so **reentrancy cannot cross the window; only another thread
can** — which makes the class invisible to every deterministic test in the suite and is why it needed
a probe rather than an argument.

**The consequence chain, traced and then measured.** A write landing between the reads leaves the plan
holding the old position and the stamp holding the new value. `ownsSplit (k)` compares the stamp with
the live parameter, finds them equal, and reports the gesture owns a world it never measured; the drag
proceeds; `writeCrossovers` finds the plan more than half a pixel from live and writes it. The
automation value is overwritten **inside the change gesture the drag opened** — automation lane, undo
stack, the lot.

**`--split-snapshot-probe`, added this round.** An automation thread alternates `mbFreqMid` while the
message thread presses, drags split 0 six pixels and releases. The discriminator is the THREAD, not
the value: only the drag can write that parameter from the message thread, and a laundered split is
written back to the lane's *previous* value, which the lane itself wrote a moment earlier, so no value
comparison can separate them.

| | spin 0 | spin 40 | spin 120 | spin 400 | total |
|---|---|---|---|---|---|
| before | 18 | 17 | 6 | 51 | **92 / 1200 (7.7 %)** |
| after | 0 | 0 | 0 | 0 | **0 / 1200** |

Repeated twice more on the same box: **84** and **72** of 1200. The rate varies with scheduling; what does not vary is that it is never zero before the change and always zero after it.

**Two corrections to my own first attempt, recorded because both changed the answer.** The first probe
reset only `mbFreqMid` between iterations, so after the first drag the handle had walked away from the
swept x and no drag happened at all — 3000 iterations of nothing, reported as "no laundering". And the
first writer fired ONE write per drag: that reaches the window about once in 4000 drags, enough to
prove the class and useless as a detector, because a lane is only straddled at a TRANSITION and a
one-shot lane has one. A continuously alternating lane has thousands.

**The mechanism proof is separate from the rate.** Inserting a 200 µs sleep between the two reads and
timing one write into it gives **200/200** laundered before the fix and **0/200** after. That build
edits production code and is not shipped; the recipe is in `TESTING.md` so it can be redone in one
line.

**Classification: A — confirmed defect.** Not a tradeoff: the window is reachable on this box without
help, the damage is a silent overwrite of automation inside the user's own gesture, and the fix removes
reads rather than adding them.

## 34. The fix is ADR-0046's own rule, one level down

ADR-0046 settled this for the band COUNT and wrote the principle out: *"One reading, used by the
derivation and by the proof, has neither failure."* It was never applied to the split and width VALUES.
Eight sites carried the paired-read shape; the review named four of them and the audit found the rest:

| site | was | is |
|---|---|---|
| `captureDragOrigins` | plan from `crossover`, stamp from `getValue` | stamp first, plan derived from it |
| `mouseDown` (handle press, add branch) | `dragGrabDX` from a third read | `dragGrabDX = p.x - dragOrigX[h]` |
| `mouseWheelMove` | tick target from a fresh `crossover` | from `dragOrigX[scrollHandle]` |
| width drag engage | `ownsWidth` reads, `bandWidth` reads again for the anchor | one `getValue`, proved and anchored |
| `addBandAt` | `crossover (k)` **twice in one statement** | once, converted twice |
| `resetCrossover`, `commitFreqEditor` | `xs[]` from `crossover`, `was[]` from `getValue` | `was[]` first, `xs[]` derived |
| `writeCrossovers`, `spreadSplits` | proof read, then the write-worth read | one reading for both |

`ownsSplit (int, float)` and `ownsWidth (int, float)` carry the caller's reading. `convertFrom0to1` is
pure arithmetic, so with nothing racing every derived value is bit-identical to what the second read
returned — which is why the whole suite is unchanged.

**Fixing the named site alone would not have closed it, and that is measured rather than asserted.**
An independent verification pass applied ONLY the `captureDragOrigins` change to HEAD and measured
**267 laundered splits in 8000 drags (3.3 %)** against 885 (11.1 %) unfixed — a 70 % reduction. The
residual was `writeCrossovers`' own pair of reads. With every site converted the same probe measures
**0 in 8000**.

**Two of those sites were failing SAFE, not open, and were still worth fixing.** The wheel tick's
target and the add branch's anchor were third reads taken AFTER the stamp, so a foreign write between
them made `ownsSplit` refuse. Safe — but a refusal is a user edit dropped for no reason the user can
see, the same lossy trade ADR-0046 closed one branch up for the count.

**Coverage, with its limit stated rather than implied.** State test 81 pins the two corners either side
of the window (a write before the capture is owned; a write after it makes the gesture refuse and write
nothing). Its mutation record is honest: **reverting ADR-0047 leaves all 2804 checks green.** The
window holds no dispatch, so no deterministic test can enter it. The probe is the coverage. A further
mutation run corrected the test's own comment: leg B is owned by the ADR-0038/0039 staleness gate at
the top of `mouseDrag`, not by the per-slot `ownsSplit` proof I first credited — disabling the proof
fails State test 71 legs B and H and leaves leg B green, disabling the gate fails leg B.

## 35. Verify-only items, and one documentation gap the audit found

* **RISK-010 — unchanged, and specifically NOT this finding.** RISK-010 is the AUDIO-side reader
  tearing a ten-load snapshot in `toEngine`; this was the GUI-side snapshot tearing under a foreign
  write. Opposite direction, different code. The register carried no row for the GUI direction, and
  now needs none: the defect is closed rather than accepted. The wording corrected on 2026-09-08
  ("read before every parameter the count reinterprets") re-checked against
  `src/PluginParameters.cpp:365-374` and still accurate.
* **Held-audition guard — unchanged.** `tick()` still returns at `if (! isShowing())` before reaching
  the guard, and the harness still never shows the editor. No production seam added this round either;
  the new probe drives public mouse entry points only.
* **Wheel gesture closure — unchanged, B (intentional).** ADR-0041 owns it, State test 80 pins it, and
  the automation/undo/host consequences measured last round still hold. This round CHANGED the line it
  sits on for the third time in one PR, which is the second half of this entry.
* **U4 — unchanged, accepted residual.** The wheel's width store still goes through gesture-less
  `setParam`; nothing this round touched it, and the wheel gesture design is not reopened.
* **TSan suppression — unchanged and still harness-scoped, verified rather than assumed.** The single
  entry names `WriteFromInsideAGestureOpen`, and a repository-wide grep finds that symbol in exactly
  one place: `tests/state_tests.cpp:2715`, a test double. No production symbol carries the name, so no
  production lock inversion can be absorbed by it, and the data-race canary proves data races are still
  reported. The match-count == entry-count assertion ran green in CI on `03a6e39` (job `tsan`, step
  "Every TSan suppression still matches something"). Its recorded residual — a pattern BROADENED to
  still match exactly one report — is unchanged.
* **Informational items — reviewed, unchanged.** Cancelled spreads and partial transaction residue:
  no new evidence, no action.

**The anchor that drifted three times.** `docs/DOCUMENTATION_COVERAGE.md` and §27 both cited the
wheel's `cancelActiveDrag()` by line: `:2433` when written, `:2455` after that commit's comment, and
`:2510` after this one's. The citation gate cannot catch this — new anchors have no `origin/main`
counterpart to compare against (§18, §31). Rather than re-aim a fourth time, both now cite the
FUNCTION, which cannot drift. The one gate-visible drift this round, `dragCrossoverTo`'s declared
re-aim, was caught by `check-citations.py` and re-derived to `:587`.

**A documentation gap the audit surfaced, corrected at its smallest.** `THREADING_POLICY.md`'s table of
allowed communication paths had a row for GUI → Audio (automatable params) and no row for the reverse:
host automation writing those same parameters from the AUDIO thread through the format wrapper, and
`setStateInformation` writing them from the HOST STATE thread. The fact was in the file — the KI-027
note states it for the latency path — but not as a path. That absence is why a GUI-side snapshot could
be written without anyone asking what writes underneath it. One row added, describing the model as it
already is; no threading-model change, and none proposed.

## 36. The suppression assertion I shipped yesterday was wrong, and two claims beside it with it

The audit's TSan track did what §35's verify-only pass did not: it read the assertion's PARSE rather
than its intent. Three corrections follow, all measured on a purpose-built two-inversion binary under
the same clang, not argued.

**The assertion was wrong in BOTH directions.** `ThreadSanitizer: Matched N suppressions` is the
**sum of hit counts**, not the number of entries that matched:

| file | reports | summary | breakdown lines |
|---|---|---|---|
| one entry | two distinct | `Matched 2` | **1** |
| two entries | one each | `Matched 2` | **2** |
| two entries, one dead, one hit twice | two | `Matched 2` | **1** |

So the step I added yesterday would have **failed a correct file** whose single entry absorbed two
reports, and **passed the exact file it exists to reject** — a dead entry beside one hit twice. It
returned green on `03a6e39` only because the real suite produces one report. The fix counts the
per-entry breakdown lines, which is one line per entry that matched at least once. Self-tested
against five logs: 1-entry/1-hit pass, 1-entry/2-hits pass (the old parse failed it), 2-entries with
one dead fail (the old parse passed it), the real suite log pass, no-summary-at-all fail.

**One consequence found while measuring, and recorded in the file:** two entries that both match the
SAME report are credited as ONE — and in the scratch run the credited entry was the SECOND in the
file, so it is not "the first entry wins". The new assertion therefore reads a redundant entry as
dead and fails. That is the intended answer, but it is a surprise if met cold.

**The suppression file's safety claim was stronger than the truth.** It said no production stack can
contain the symbol "so this entry cannot mask that class". A TSan suppression is REPORT-scoped: if
any frame of any stack matches, the whole report is absorbed. Measured: an inversion whose stacks are
ALL production frames is still reported (exit 66 with the entry loaded); a MIXED cycle pairing a
production edge with the harness edge **is** absorbed. The residual is real and now recorded rather
than claimed away — bounded only by the fact that the double is not compiled into a shipped build.
The same overstatement was echoed in `DOCUMENTATION_COVERAGE.md` and is corrected there too.

**And the reason the second entry was dropped was false.** The file said `WriteFromInsideAStore`
"matched nothing" because `print_suppressions=1` reported `Matched 1`, not two. Given the measurement
above, that observation cannot distinguish "matched nothing" from "matched the same report and was
not credited" — and the latter is what happened. The entry was REDUNDANT, not dead. The conclusion
(one entry) survives; the stated mechanism did not, and a rule kept for a wrong reason is one edit
away from being dropped for a wrong reason.

**A dependence nothing recorded:** the entry names the double reached from State test 75 leg D only
because leg D precedes leg H. Re-aiming or removing leg D turns the report into an H-vs-G cycle the
pattern does not name. One sentence in the file now says so.

## 37. What the audit found that this round did NOT act on

The sixth track re-read the two journals §32 re-opened. These items are real enough to record and
outside this round's finding; none is a hard-stop category, and none is claimed as handled:

| item | where | why not now |
|---|---|---|
| `applyStateSet` takes its `StateSet` by const reference and is called with the member `committed`, so a re-entrant `pollUndoCoalesce` can retarget a restore mid-burst | `src/PluginProcessor.cpp` | a one-token fix (by value), but it is the undo/restore path, not this round's class, and `FUTURE_RISKS` RISK-011 claims to cover U1-U3 while its text describes a different mechanism. Needs its own round with its own regression |
| `undo()`/`redo()` re-establish `committedSig` after the restore burst but never `committed` | `src/PluginProcessor.cpp` | same path, same reasoning |
| a whole-session restore landing before the count store lets `setBands` commit a count over the restored session — with no recorded disposition anywhere | `SpectrumImager::setBands` | ADR-0042 legs B and C would have to be re-run before ruling it; that is a round, not a patch |
| State test 76 has ADR-0042's asymmetry controls for `removeBand` but not for `addBandAt` | `tests/state_tests.cpp` | controls, not defect finders; worth adding, not worth wedging into this round |
| the abandonment-residue store census is checkable and unchecked | `tests/state_tests.cpp` | same |
| ~10 stale figures and cross-references (an ADR-0042 residue count, a `[0.9.7]`→`[0.9.8]` block, a `FrameClock::fire` seam that cannot work, `soloPreviewMask` as an eleventh publication channel, THREAD_MODEL's blanket no-locking sentence) | docs | documentation-only drift found by a track that was looking for something else; a documentation-sync round should take them together rather than have each round take one |

Recorded here rather than silently dropped, which is the whole point of §32 having re-opened those
journals in the first place.
