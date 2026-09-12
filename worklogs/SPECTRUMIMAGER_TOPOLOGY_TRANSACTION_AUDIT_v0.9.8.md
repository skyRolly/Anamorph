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

## 38. Workflow audit, and the CI failure that was mine

**The CI failure first, because it was blocking.** `source-lint` failed on `ea82eab` at step 10,
"Check documentation evidence anchors": one drifted anchor,
`docs/DOCUMENTATION_COVERAGE.md: src/gui/SpectrumImager.cpp:398-408 -> :410-420`, moved by the twelve
lines ADR-0047's overloads added above `ownsSplit`.

**Classification: a code issue in the change set — a stale documentation anchor — not a formatting or
tooling problem.** The gate did exactly its job. The reason I did not see it locally is worth more
than the fix: **CI compares against the PREVIOUS COMMIT, and I had verified against `origin/main`.**
Those are different questions, and only the first sees drift a commit introduces and then re-anchors
within itself. The last three rounds each re-anchored against a base chosen by hand; this is the
round where that habit finally cost a red build. The rule from here: run
`check-citations.py --check --base HEAD` as the final step before committing, which is what CI runs.

**Workflow audit.** `wf_d3bf30b3-246` (last round's) had returned all six of its tracks — the
load-bearing half — and eleven of roughly forty-five adversarial verifiers.

| option | evidence | decision |
|---|---|---|
| let it finish | the remaining verifiers re-verify claims whose disposition is already implemented, tested and pushed; the eleven read so far refuted NOTHING and only corrected details the shipped fix already covered (the sibling window, the five-site scope, the insertion index) | rejected |
| stop it | ~34 verifiers left at 2 concurrent agents on a 4-core box, during a round that needs the box for builds, TSan and valgrind; the same calculus recorded for `wf_17153265-ac9` in §25 | **taken** |
| start a replacement | the two new findings are a different question and need their own tracks | **also taken** (`wf_7a4ee1f6-cc3`) |

Its journals stay on disk and its six track results are consumed in §§33-37 and here. No unread
result is being carried forward silently — §31 is why that sentence is written every round now.

## 39. Finding 1 — "release-time automation loses a band": ALREADY PREVENTED, and measured six times over

The mechanism the review describes is real and I did not have to argue about it: `mouseUp` proves the
count with `gestureIsStale()`, latches `pressBands`, clears `gestureBands`, and then calls
`endGesture (freqP[dragHandle])` — which DISPATCHES — before reaching `removeBand (dragHandle + 1,
pressBands)`. A host recording automation can move `mbBands` from inside that dispatch. The window is
not hypothetical and it is not cross-thread-only: it is REENTRANT, which makes it the most reachable
of any window this series has examined.

**And nothing gets through it.** The mutation run is the evidence:

| mutation | checks killed |
|---|---|
| M1 — delete `removeBand`'s entry `N != expectedBands` | **none** (2 814 / 0) |
| M1 + M4 — also delete the `bandCount() != expectedBands` before the solo store | **none** |
| M1 + M4 + M-solo — also pass `-1` for `setSoloMask`'s `expectedBands` | **none** |
| **M-I2 — the OUTWARD-DRAG call site passes `bandCount()` instead of `pressBands`** | **2**, with the round's own diagnostic: `Bands 4 -> 3: the release read a count the check never saw` |
| the same substitution at the DELETE-X call site alone | none |
| the same substitution at the two SOLO call sites alone | none |

**The first three readings were mine and the fourth came from the audit, and the fourth is the one
that settles it.** Peeling the callee's comparisons one at a time never fires, because the width loop,
the split loop and `setBands` each still refuse — so I first wrote M1 down as a *coverage hole*, a
guard no test defends. It is not. What is load-bearing is the `pressBands` ARGUMENT, and **State test 69**
leg C pins it precisely: remove the contract at the call site rather than one comparison inside the
callee and the test fires twice, with the exact `Bands 4 -> 3` the source comment cites.

**And it is ONE call site, not all four.** The audit measured the substitution per site: the
outward-drag site kills two checks, the delete-x site none, the two solo sites none. That is not one
guard and three holes — the outward-drag site is the only tail store with a synchronous DISPATCH in
front of it (`endGesture` on the dragged split, which is what leg C's `BandsMoveOnGestureEnd` listener
fires inside), so it is the only one a single-threaded harness can reach. The others are
cross-thread-only. **That corrects §39's own "two coverage holes" line below**: the delete-x site is an
unreachable window, not an undefended guard. Also corrected: the test is State test **69** leg C, not
71 — the audit's verifier caught that misattribution in the comment I had just written.

The distinction matters because the two readings call for opposite actions — write a missing test
versus write down why a single-line mutation cannot fire — and I would have taken the wrong one. It is
also the third time this round a first reading needed correcting by measurement rather than argument;
the other two are the probe's parked constant and its breakdown test in §40.

**Disposition: already prevented; invariant documented at the guard** with the full ladder, so the
next reader does not rediscover "this line kills no test" and delete it.

**Two coverage holes the audit found while proving that, recorded rather than closed.** The delete-x
call site's `pressBands` has no mutation that kills anything — the same change there (`removeBand (dB,
bandCount())`) leaves 2 814 green, because the suite has no adversary for that path's narrower window.
And `mouseUp`'s own staleness gate can be disabled entirely with a green build: what it protects is
gesture hygiene (pairing `endChangeGesture`, tearing down the preview), which nothing in the state
suite observes. Neither is a defect; both are places where a future edit would go unnoticed.

**One thing the ladder does NOT cover, stated rather than implied:** a SAME-COUNT layout change (splits
moved, count unchanged) landing between `removeBand`'s `fr[]`/`wd[]` snapshot and its first store is
caught only when the split loop reaches it — after the width loop has already written. That is the
recorded ADR-0042/0044 partial-transaction residue, unchanged and still accepted.

## 40. Finding 2 — "concurrent topology changes misplace additions": CONFIRMED, fixed, measured

`mouseDown` derives the band under the pointer from the latched count (`bandAtX (p.x, gestureBands)`)
and then asks `bandAddTarget` where that band ENDS — and that function read `mbBands` for itself. Two
readings, three writers, and the `b < N - 1` ternary between them decides whether the band's right
edge is a split or the plot edge. Every call in the span is a pure read, so this one IS cross-thread
only, unlike Finding 1.

Measured with `--add-target-probe`, added this round: **55 misplacements in 4800 clicks (1.1 %) before,
0 after**, control placing the split at 15030.7 Hz throughout, and the abandoned-add rate unchanged
(1534-1582 before, 1569-1588 after) so the fix suppresses no add that previously succeeded.

**The fix is one argument**, in ADR-0046's own shape: `bandAddTarget (b, x, outX, n)` with `mouseDown`
passing `gestureBands` and `updateHover` passing the `N` it already read. A third reading of `mbBands`
at the call site, which `addBandAt` overwrites before the caller can use it, is gone with it.

**And the symmetric change on the other side of the call was implemented, measured and REMOVED.**
Giving `addBandAt` the `expectedBands` contract `removeBand` has carried since ADR-0039 looked
obviously right — the asymmetry between an add that proves nothing and a removal that proves
everything reads like a gap. Three things settled it against:

* it closed nothing (`--add-target-probe`: 0 misplacements with and without it, 4800 clicks each);
* it is wrong in principle, and the principle was already written down — `removeBand` takes a BAND
  INDEX, which a count change RETARGETS, while `addBandAt` takes a FREQUENCY, which means the same
  thing under every topology. That is ADR-0045's own asymmetry, ruled there for `commitFreqEditor`;
* it cost the user a click, turning an add into a no-op whenever a lane moved the count in the instant.

Recorded at both call sites so the asymmetry reads as a decision rather than an oversight.

## 40b. A NEW finding the audit turned up inside `removeBand`, escalated rather than patched

Track 1 of `wf_7a4ee1f6-cc3` was asked whether ADR-0039's fix left any window uncovered, and found one
that is not the same-count residue already on record: **the plan's SOURCE slot is never proved, only
its destination.** `removeBand` builds `nf[]`/`nw[]` by shifting values down past the removed band, and
each loop iteration proves the slot it is about to WRITE against the snapshot (`exactlyEqual (bandWidth
(k), wd[k])`). It does not prove the slot the value was TAKEN FROM. So a same-count foreign write to
the top live split or width — a slot the shift reads but never writes — is silently discarded and the
transaction COMPLETES, rather than abandoning as it does for a write to any other slot.

Measured by the track on its own scratch copy: 3 bands, splits 200/2000, widths 0.500/1.500/1.900,
mask 6, delete-x on band 0, with a one-shot listener firing from inside `setSoloMask`'s store and
writing `mbFreqMid` 2000 -> 5000 (split N-2, a plan SOURCE): the burst runs to completion and the
5000 is gone.

**Escalated, not patched, and the reason is not scope.** The fix — prove the source slot as well as
the destination in both loops — changes when a topology transaction ABANDONS, which is ADR-0042's and
ADR-0044's recorded disposition, and those were reached by measuring the alternatives against each
other. Changing that from inside a round whose brief says *"avoid broad redesign"* would be exactly the
move this series keeps refusing. It belongs in its own round with ADR-0042's legs re-run, and it is on
the record here so it is not rediscovered as new.

## 41. Verify-only items

* **RISK-010 — unchanged.** Neither finding touches it: both are GUI-side topology readings, and
  RISK-010 is the AUDIO-side reader tearing a ten-load snapshot in `toEngine`. No new evidence.
* **Held-audition guard — unchanged.** `tick()` still returns at `if (! isShowing())` before the
  guard; the harness still cannot show the editor; no production seam added this round either.
* **Wheel input closes active gestures — no behaviour decision is required, and the options are laid
  out anyway because the brief asked.** (A) keep ADR-0041's rule: the tick finishes the press, then
  acts — the host gesture closes at the tick, the drag so far becomes its own undo step, the press is
  dead, and a pending solo/delete click is swallowed; all four measured last round, all intended,
  pinned by State test 80 and described in the CHANGELOG in user-facing terms. (B) make the wheel a
  no-op while a press is held — preserves drag continuity and adds no undo step, but trades one silent
  surprise for another and would need its own CHANGELOG entry reversing the last one. (C) defer the
  tick until release — queued state and a delayed effect the user cannot predict; rejected outright.
  **Recommendation: keep (A).** No defect is outstanding, (B) has no evidence in its favour beyond
  taste, and reversing a shipped, documented, tested user-visible rule for taste is the one change
  this series has consistently refused to make.
* **U4 / wheel width undo — unchanged, accepted residual.** The wheel's width store still goes through
  gesture-less `setParam`; nothing this round touched it and no new evidence arrived.
* **TSan suppression — unchanged, and the corrected assertion held.** Still harness-scoped (the
  symbol exists only in `tests/state_tests.cpp`); production inversions still visible (an all-production
  cycle exits 66, measured); the assertion now counts per-entry breakdown lines rather than the summary
  and ran green in CI on `ea82eab`.

## 42. Two corrections the audit made to the ADR-0048 change itself

Both were found in the WORKING TREE, before the round closed, which is the first time that has
happened rather than a round later.

**The hover fix recreated the defect one line up.** `updateHover` reads `N` at the top of the pass and
uses it for the delete target, while `handleNearX` and `bandAtX` re-read for themselves. Threading `N`
into `bandAddTarget` ALONE therefore left the band index coming from a fresh reading and the band's
edges from `N` — the exact mismatch ADR-0048 is about, moved rather than closed. `N` is now threaded
into all three. Display only, so nothing there fails open; but a cursor that offers one band's
affordance while naming another's is the same defect one severity band down, and it costs two reads to
remove rather than to reason about.

**A figure in my own comment was not reproducible.** The `bandAddTarget` comment quoted "5
misplacements in 800 clicks" — the FIRST version of the probe, before its geometry reset and its
listener verdict were fixed. The shipped probe measures 55 in 4800. Corrected everywhere it appears.
A number that cannot be reproduced from the tree that carries it is worse than no number.

**And a residual the count fix does not reach, now stated in the source and the ADR:** `lo` and `hi`
still read `crossover()` live, so the edges can be split VALUES a same-count layout change has already
repositioned. That needs ADR-0047's instrument — one capture of the split array shared by the
derivation and the target — not this one. Only the RISING count direction misplaces; falling widens
the clamp, and rising past four drops the click (fail-safe but lossy).

## 43. The audit's track 4, and the one thing in it that was actionable

Track 4 re-audited the standing decisions and corroborated every one of §41's conclusions
independently — RISK-010 unchanged and not this round's direction, the held-audition guard unchanged
with no production seam added, U4 not reopened, and the TSan suppression harness-scoped with the
corrected assertion right in both directions AND in the no-summary case (it is pipefail-agnostic by
construction, `|| true`).

**On the wheel it produced a better argument than mine.** I recommended keeping ADR-0041's rule partly
on the grounds that option (B) — a wheel that does nothing while a press is held — had no evidence in
its favour. The track MEASURED the cost instead: making the wheel a no-op fires State test 73 leg A
with `a wheel tick adopted the installed width 1.700, and the drag then wrote 0.650 from an anchor
taken before it`, because `captureDragOrigins` can re-seed the ownership record but not `dragGrabDX`
/`dragGrabDY`, the anchors a held press computes its next write from. So (B) reintroduces the
ADR-0041 defect unless those anchors are re-seeded too — a larger change than it looks. The
recommendation is unchanged and now rests on a measurement rather than on taste.

**And it found one actionable gap, which is now closed.** ADR-0041's rule has four consequences; State
test 80 pinned three. The fourth — a tick swallowing a pending solo or delete-x click — was measured
when the wheel round found it and then only documented. Stopping `cancelActiveDrag` clearing
`soloPressBand` left all 2814 checks green. **Leg C** now runs the control and the interrupted press
on its own processor; mutation M-W2 kills exactly one check, leg C's second, and nothing else.

Documenting a measurement is not the same as pinning it, and this is the second time this session that
distinction mattered — the first was §39's guard, where a survived mutation meant the opposite of what
it looked like.

## 44. The audit's full result, and five findings escalated from it

`wf_7a4ee1f6-cc3` finished all five tracks and 34 agents. Tracks 1, 2 and 4 are consumed in §§39-43.
What tracks 3 and 5 add is below. **None of it is patched here**, and that is a decision rather than
fatigue: each item is the same class this round just fixed, which means each deserves the same
treatment — a measurement, a minimal fix, a mutation record — and doing four of those inside a round
whose brief named two findings is how a round stops being reviewable.

**Two corrections to my own records came out of the same run and ARE applied** (§39): the mutation
kill belongs to State test **69** leg C, not 71, and the per-site attribution shows the delete-x call
site is an unreachable window rather than the coverage hole I called it.

| # | finding | evidence | why not now |
|---|---|---|---|
| A | **Every per-store re-proof in BOTH `removeBand` loops is uncovered** — the whole ADR-0040 loop guard can be deleted and the suite stays green | mutations M4/M4b/M5/M5b/M4c, all surviving | a real coverage gap with a concrete fix (two legs on State test 76, modelled on leg F with `poke.target = bandsP` and a value target). It is test work, not a code change, and it belongs with the §40b source-slot finding since both live in the same loops |
| B | **`moveBand` sizes its plan from a reading its own proof never sees** — `const int M = bandCount() - 1` while `writeCrossovers` proves against `gestureBands` | the ADR-0046 shape, in the sibling one line from `dragCrossoverTo` which that ADR fixed | same class as this round's Finding 2 and the same one-argument fix, but the band-move path carries two pins and a `bandTmin`/`bandTmax` pair; it needs its own measurement rather than an argument by analogy |
| C | **`beginBandMove` derives every band-move identifier and bound from its own reading**, two of them used across its own gesture dispatches | same track | fixing B without C leaves the plan's extent proved and its pins still derived under an unproved topology — so they are one change, not two. **CORRECTED BY MEASUREMENT in §55: they are not.** Reverting `moveBand` alone fires 7/1200; reverting `beginBandMove` alone fires 0/1200. The extent is load-bearing for the observable; the pins are threaded on the rule, not on evidence, and §55 says so |
| D | **`mouseUp`, `cancelActiveDrag` and `endBandMove` clear their latched identifiers AFTER the `endGesture` dispatch**, so a reentrant cancel can fire `endChangeGesture` twice | static, three sites | the fix (copy to a local, null the member, then dispatch) is small but it is a reentrancy-semantics change in the gesture bracket, which is where this series has been most careful |
| E | `bandSoloed` is dead code — the last helper that re-reads `soloMask` to interpret a caller's index, with a comment already warning against calling it | no callers | trivial, and trivial changes still need a round that is looking at that file |

**One thing the audit says about the whole `removeBand` ensemble is worth carrying forward as a rule
rather than a finding:** no single layer of its six-layer count proof is measurable, and no three
layers are — only the whole set. So a surviving single-line mutation there is not evidence of a hole,
and a surviving ENSEMBLE mutation on the loops (item A) is. That is the general form of the mistake I
made twice this round.

## 45. Finding — "band removal discards concurrent edits": CONFIRMED, fixed (ADR-0049)

This is §40b, escalated last round and returned by the review as a MUST-RESOLVE. Two readings of the
same evidence, one round apart, and the second one is the review's.

**The mechanism, from the index arithmetic rather than from the prose.** `removeBand` builds its plan
as `nw[j] = wd[j]` for `j < b` and `wd[j + 1]` for `j >= b`, and `nf[j] = fr[j]` for `j < dropX` and
`fr[j + 1]` for `j >= dropX`. So a store is a MOVE: slot `k` is written with what slot `k + 1` held at
plan time. The guards proved `bandWidth (k) == wd[k]` and `crossover (k) == fr[k]` — the destination —
and nothing else. `bandWidth (N - 1)` is compared with `wd[N - 1]` at no point in the function, and
`crossover (N - 2)` with `fr[N - 2]` at no point either. A mid source IS proved, but by the NEXT
iteration's destination check: one store after the store that consumed it, with that store's dispatch
in between.

**REENTRANT, and that is what separates this from the ADR-0046/0047/0048 family.** `setSoloMask` and
every `setParam` in the loops dispatch, so a single-threaded harness enters the window with an
ordinary listener. Those three ADRs all close windows bounded by pure reads, which is why each of them
records honestly that reverting it leaves the whole suite green. This one does not have that excuse,
and it does not need it.

**Where the first draft of this section was wrong, corrected by the round's own verifier.** I wrote
that the host's value was "discarded in silence". It is not: the count store puts the top slot ABOVE
the live topology, where `MultibandWidth` (`.h:53-56`, `.cpp:140`) does not read it, so the value is
PARKED — inert while hidden, exact if the count returns, which is the same disposition ADR-0039
records for the parked solo bit. The accurate charge is MISATTRIBUTION: the surviving band inherits
the pre-write snapshot under a host edit that had already replaced it. Saying "discarded" makes the
defect sound worse than the evidence supports, which is precisely the failure this file's
measurements exist to prevent.

**What the fix does not disturb.** ADR-0044's audit removed a WIDE fix that re-proved the whole
written PREFIX, because ADR-0042 measured aborting there and found it worse; State test 76 legs B and
C are the controls that keep it. Both legs write a slot the transaction has already written, and no
guard added here re-reads the prefix. Both pass unchanged, and leg D — the uninterrupted positive
control — still commits with every new guard passing.

**And the sibling stays alone, for a reason I got wrong first.** I argued `addBandAt` was
structurally safe because the destination guard at iteration `i` is also the source proof for
iteration `i + 1`. Half true: it proves the source at the last instant it exists, which is the
strongest statement available, and that half survives. What it does NOT cover is a host write landing
on slot `i - 1` AFTER the transaction's own store to it — `nw[i] = (i <= ins) ? wd[i] : wd[i - 1]`
re-attributes that slot to a different band, so the newer value stands on a band the user did not aim
it at, the same misattribution shape one severity band down. The symmetric guard cannot be added:
`bandWidth (i - 1) == wd[i - 1]` fails on the transaction's OWN store and would abandon every
non-elided add. So it stays ADR-0042's measured disposition, now NAMED at the site as a residual
rather than implied by silence.

| Mutation | Killed |
|---|---|
| M-A1 — the width source proof removed | State test 76 leg I only, 2 checks |
| M-A2 — the split source proof removed | State test 76 leg J only, 2 checks |

## 46. Finding — "release-time automation loses a band": CONFIRMED, fixed (ADR-0050)

Last round I recorded this finding as ALREADY PREVENTED and spent six measurements proving the
`pressBands` argument was load-bearing. That was true and it was the wrong half of the question. The
review re-asked it and the answer is different, because the two halves are different:

* **the COUNT half** — a `mbBands` move inside `endGesture`'s dispatch — IS caught, by `pressBands`
  and `removeBand`'s `expectedBands`, and State test 69 leg C measures it (`Bands 4 -> 3: the release
  read a count the check never saw`);
* **the SOUND half** — a different layout at the SAME count — was caught by nothing, and the reason
  is one line: `gestureBands = -1` ran BEFORE `endGesture`, and `gestureIsStale()` returns false
  unconditionally when `gestureBands < 0` (both `topologyMovedUnderGesture` and
  `soundMovedUnderGesture` short-circuit there). The window was not merely unchecked; it was
  un-CHECKABLE.

`ownsSplit`/`ownsWidth` are the only things in the class that see a same-count install, and they had
been disarmed one line above the dispatch that lets one in. The pending removal then merged a band of
a layout the press had never seen.

**The fix is where the clear goes, and both halves of it are load-bearing** — measured separately,
because "move the line AND add the check" is two changes and a round that does not tell them apart
cannot say which one matters:

| Mutation | Killed |
|---|---|
| M-B1 — the post-dispatch `! gestureIsStale()` removed | State test 69 leg G only, 1 check |
| M-B2 — the clear restored above the branches, re-proof left in place | State test 69 leg G only, 1 check |

Nothing in the two early-return branches reads `gestureBands` (`setParam`, `setBands`, `setSoloMask`,
`toggleSoloBit` and `endBandMove` do not), so they keep clearing it exactly where they did and their
behaviour is unchanged. An uninterrupted release still removes: `writeCrossovers` stores THROUGH
`gestureX[k]`, so the stamp equals the world the drag just wrote. State test 69 leg E is that control.

## 47. Finding — "concurrent split moves misplace new bands": CONFIRMED, fixed (ADR-0051)

The residual ADR-0048 recorded in its own source comment and index row, returned by the review as a
MUST-RESOLVE. The COUNT was one reading per pass; the split ROW was not. `bandAtX`, `handleNearX` and
`bandAddTarget` each read `crossover()` for themselves, so one press took THREE readings of a
three-element row three threads write, and `updateHover`, `mouseDoubleClick` and `mouseWheelMove` did
the same.

`mouseDown` carried a second instance of the same shape that the review did not name and the audit
found: the press stamps the whole row at the top (`captureGestureSound`) and the handle branch then
called `captureDragOrigins()`, which stamps AGAIN — the index from the first stamp, the drag's grab
anchor from the second. `seedDragOrigins()` derives the origins from the stamp the press already
holds, so the press is now one reading end to end and one reading SHORTER than before.

**The instrument had to be aimed three times, and that is the part of this section worth keeping.**

1. Lane alternating split 0 between 500 Hz and 6 kHz, click at 15 kHz, detector on the upward clamp:
   **0 misplaced before, 0 after**, across 3200 clicks. The lane never crossed the click, so the
   band's left edge stayed left of the pointer under both readings and `jlimit` never clamped.
2. Lane crossing the click (500 Hz ↔ 16 kHz), same upward detector, and a 2 000 000-iteration
   volatile spin inserted between the two readings to widen the window by ~1 ms: **still 0 before,
   0 after**. The upward direction requires `bandAtX` to answer "the band above" and
   `bandAddTarget` to then read the split above the click — and in this geometry the other direction
   is the one that fires.
3. The same lane with the detector watching BOTH clamp directions, natural window, no spin:

   ```
   before   15, 11, 22, 27  --  75 of 1600 (4.7%)
   after     0,  0,  0,  0  --   0 of 1600
   ```

   with the correctly-placed column unchanged (856 before, 847 after), so no add that previously
   succeeded is suppressed: the 75 become adds `addBandAt` abandons on its own ADR-0040 proofs, which
   is the fail-safe direction.

Attempts 1 and 2 are recorded in the probe's header rather than deleted, because **a verdict of 0
from an instrument that cannot see the defect is indistinguishable from a verdict of 0 from a defect
that is not there**, and shipping the first as if it were the second is the same false-confidence
failure the TSan suppression assertion was corrected for on 2026-09-08. Had I stopped at attempt 1 I
would have reported "the class does not reproduce" and shipped a CI gate that can never fail.

## 48. Finding — "ignored wheel input ends held presses": CONFIRMED, fixed (ADR-0052)

The review filed this alongside "wheel input closes active gestures" and warned against assuming they
share a classification. They do not, and the split is clean:

* **8.2 — a REAL tick ends a held press: REFUTED as a defect.** ADR-0041's recorded decision,
  re-measured last round (making the wheel a no-op during a held press fires State test 73 leg A,
  because `captureDragOrigins` re-seeds the ownership record but cannot re-seed
  `dragGrabDX`/`dragGrabDY`), and pinned by State test 80 legs A–C. Unchanged, and re-affirmed.
* **8.1 — an IGNORED event ends a held press: CONFIRMED defect.** `cancelActiveDrag()` ran at the
  first line of `mouseWheelMove` and the delta threshold forty lines later. `deltaX` is read NOWHERE
  in this class, so a horizontal trackpad scroll arrives with `deltaY == 0` and is ignored — after
  ending the drag, closing its host gesture early (committing the drag so far as its own undo step)
  and swallowing a pending solo or delete click. ADR-0041's rule is about OWNERS, and an event that
  writes nothing is not one.

Window class: **none** — this is a single-threaded message-thread ordering defect needing no second
thread and no listener, which is why State test 80 leg D is an ordinary deterministic test.

**The gate question was asked rather than assumed.** `docs/policies/ARCHITECTURE_REVIEW_GATE.md`
lists *conflict with an Accepted ADR* as a hard stop. ADR-0052 narrows ADR-0041's SCOPE, and the
reasoning for calling that a clarification rather than a conflict is written into the ADR itself:
ADR-0041's premise is its own first sentence — *the wheel is its own instantaneous edit* — and every
event it reasoned about behaves identically after the change. The in-source ADR-0041 comment block
was reconciled rather than left stranded, and ADR-0041 carries a `Scoped by` cross-link.

| Mutation | Killed |
|---|---|
| M-C1 — `cancelActiveDrag()` restored in front of the threshold | State test 80 leg D only, all 3 checks |

## 49. The CI failure on `e45b3e4`, and the gate that could not have caught it locally

`linux` step 22, *Gate first-party Clang warnings*, exit 1:

```
tests/state_tests.cpp:17084:68: warning: [-Wunused-variable]
-Wunused-variable in tests/state_tests.cpp: 1 site(s), baseline allows 0.
```

`kClickHz`, left behind when State test 82's probe changed from a fixed click frequency to a swept
one. **A CODE ISSUE IN THE CHANGE SET**, not a formatting or tooling one — the gate did its job, and
the fix is to delete the constant.

**Why it escaped locally, which is the durable half.** `check-clang-warnings.py` refuses to run when
the log's compiler major differs from the baseline's, and it is right to: diagnostic counts move
between majors. This box has clang-18 and gcc-13 against a pinned clang-22 and gcc-16, so BOTH
warning gates decline locally on every run and a brand-new first-party warning is invisible until the
push builds. That is the second CI cycle this PR has spent on a class that a local compiler would
have named instantly.

`scripts/preflight.sh` now carries a **local first-party warning sweep**: `-fsyntax-only -Wall
-Wextra` over `src/gui/SpectrumImager.cpp` and `tests/state_tests.cpp` with whatever compiler is
installed, reporting anything it says about a file under `src/` or `tests/`. It is **advisory and
never fatal**, and the comment says why in the file: the authoritative gate is still CI's pinned
major, and this must not become a second baseline to argue with. It is a smoke alarm, not a gate.
`-Wunused-variable` is version-independent and in `-Wall`, so it would have fired.

## 50. Two attributions in the tree, re-measured and corrected

Both were found by this round's adversarial verifier rather than by the review, and both are the same
class of error I corrected twice last round: a leg named from memory instead of from a mutation.

**"State test 71 leg C and State test 73 leg A already fail if the press survives"** — the sentence
in `mouseWheelMove`'s ADR-0041 block and in State test 80's own header. Measured by removing
`cancelActiveDrag()` from the handler entirely:

```
[FAIL] leg A: a refresh that cannot refresh the anchor does not adopt the value either   (State test 73)
[FAIL] leg A: the host's change gesture closes AT the tick, not at mouseUp               (State test 80)
[FAIL] leg A: ...and the drag so far is committed as its own undo step                   (State test 80)
```

Three checks, in **73 leg A and 80 leg A**. State test 71 leg C is *"a Bands change from INSIDE the
removal burst stands"* and has nothing to do with the wheel; State test 72 leg C *is* a wheel leg
(*"a wheel tick mid-drag must not re-seed ownership"*) and does **not** fail either, so the
verifier's proposed correction was wrong too and is recorded here as such. Corrected at both sites.

**`docs/DOCUMENTATION_COVERAGE.md` still carried "State test 71 leg C"** in two places for the
`pressBands` mutation, where last round corrected the source and the worklog to **69** leg C and
missed the coverage document. Corrected.

## 51. A mutation that used to fire and no longer does, which is a strengthening

Re-measuring §50 turned up a third thing. Last round's headline evidence for `removeBand`'s
`pressBands` argument was that substituting a live `bandCount()` at the OUTWARD-DRAG call site kills
State test 69 leg C twice. On this tree it kills **nothing**.

The reason is ADR-0050: `! gestureIsStale()` now sits in front of that call, and
`topologyMovedUnderGesture` compares the live count against the **still-latched** `gestureBands` — so
leg C's `mbBands` move is refused one level earlier, whatever `expectedBands` says. Removing **both**
gives the measurement that now stands:

```
[FAIL] leg C: a Bands move inside mouseUp removes no band
[FAIL] leg C: ...and rewrites no split or width of the topology it never saw
[FAIL] leg G: a same-count sound install inside mouseUp removes no band
```

This is the third time in two rounds that reading a mutation result at face value would have produced
the wrong action, and the third distinct shape of the mistake:

| | what it looked like | what it was |
|---|---|---|
| round 3 | `removeBand`'s entry guard survives every single-line mutation → a coverage hole | an unreachable guard behind five siblings |
| round 3 | the delete-x call site survives → a second coverage hole | a cross-thread-only window no harness can enter |
| **this round** | the outward-drag mutation no longer fires → coverage LOST | a stronger guard moved in front of it |

`pressBands` is kept as defence in depth, and the guard's comment now says why, so the next round does
not delete it as dead: it is the only cover if the staleness gate ever moves again, and the delete-x
and solo call sites have no gate in front of them at all.

## 52. The cost of keeping the latch alive, and how ADR-0050 pays it

Named by the round's own verifier, not by the review, and worth the two lines it costs.

Keeping `gestureBands` latched across `endGesture` is what makes the sound re-proof possible. It is
also what makes `tick`'s reconcile — `if (gestureIsStale()) cancelActiveDrag();` at 24 Hz — have
something to find inside that dispatch, if a host pumps the message loop from a gesture callback.
With `dragHandle` still set, that reconcile calls `endGesture` on the SAME parameter a second time
while the first call is on the stack: a negative open-gesture count in the processor and a spurious
undo entry. That is escalated finding (D) from section 44, and widening its reachability inside a
round whose whole subject is ownership would be the wrong trade.

So the identifier is latched into a local and the member cleared BEFORE the dispatch. A reentrant
reconcile then takes `cancelActiveDrag`'s cheap exit and closes nothing twice — but that exit clears
`gestureBands` and does NOT clear `dragRemovePending`, so the removal would proceed under a gesture
something else had just cancelled, with `gestureIsStale()` answering `false` because the latch is
gone. `gestureBands == pressBands` refuses instead.

**That third comparison is UNMEASURED and the source says so.** No harness reaches a host that pumps
the message loop from inside `endChangeGesture`, so removing it leaves all 2 840 checks green. It is
the same disposition as `removeBand`'s delete-x `pressBands`, recorded the same way: a guard whose
window is real and whose test does not exist, named rather than dressed up as measured.

**An earlier shape of this fix was measured and discarded.** Clearing `dragHandle` WITHOUT the latch
comparison closes the double-close and opens the stale removal; keeping `dragHandle` set closes the
stale removal (`cancelActiveDrag` clears `dragRemovePending` on that path) and leaves the
double-close. Only the pair closes both, which is why both are in the code and why the mutation table
has a row that kills nothing.

## 53. ADR-0050 applied to ONE branch of three, and the review caught it

Filed as a Bug at `src/gui/SpectrumImager.cpp:2562` in the same review that approved ADR-0052:
*"Automation can replace a same-count layout after the release check but before a pending delete or
solo action. Both paths clear `gestureBands` before acting, so `gestureIsStale()` cannot stop the
action from targeting the replacement layout."*

**Confirmed, and it is my own ADR's title used against my own implementation.** ADR-0050 is *"a
gesture's ownership ends with its LAST on-release action, not with its first"*. I moved the clear off
the shared line, wrote three paragraphs about why the press's ownership has to outlive the actions
that depend on it — and then put `gestureBands = -1` straight back at the top of the delete branch
and the top of the solo branch. Two of the three exits kept the exact shape the ADR was written to
remove. The paragraph I added even asserted the opposite as a justification: *"nothing else in the
two early branches reads `gestureBands` ... so moving the clear costs them nothing"*. True and
irrelevant: the point was never what those branches read, it was what they could no longer be
ASKED.

**Reachability, stated before the fix rather than after.** Neither remaining window holds a dispatch:

* the delete branch runs `deleteHit (e.position)` — `bandCount()`, `deleteBox`, `bandLeftX`,
  `bandRightX`, `crossover()`, all pure — between the handler's gate and `removeBand`;
* the solo branch's stores are the `else` of the branch that calls `endBandMove()`, so the one
  dispatch in that branch (`endGesture` on the two moved splits) and the stores are mutually
  exclusive. `onClearSoloPreview` is a relaxed atomic store into the processor
  (`PluginProcessor.h:137`) and dispatches nothing.

So both are **cross-thread only**. That is the class ADR-0046, ADR-0047, ADR-0048 and ADR-0051 all
CLOSED this PR rather than accepted, and the structural argument is the stronger half anyway: with
the latch cleared the question is not merely unasked, it is unanswerable.

**The double-close trap, again.** Keeping the latch alive across the held-solo path makes `tick`'s
reconcile reachable there, and `cancelActiveDrag` re-runs BOTH `onClearSoloPreview` and
`endBandMove` while `soloPressBand`/`soloMovedBand` are still set — two change gestures closed
twice. Both branches now take every latched identifier into locals and clear the members before
anything dispatches, exactly as the drag branch already did.

**WHAT IS MEASURED, AND WHAT IS NOT.** The defect direction is unmeasurable and no probe is shipped
for it. A lane moving the sound continuously makes the handler's own gate refuse nearly every
release, so the few-instruction window contributes nothing an instrument could separate — a probe
here would print a number that means nothing, which is precisely what section 47 exists to warn
about. The RISK direction of adding a gate is measurable and was measured: forcing each new gate to
refuse always kills **25 checks** (delete) and **6 checks** (solo) across State tests 69, 71, 74, 75,
76, 79 and 80. The actions are heavily covered, so an over-refusing fix would have failed loudly.

**The pattern worth naming.** Three rounds running, the mistake has been the same shape: a rule
stated correctly and applied to the instance in front of me. Round 3 recorded a mutation ladder for
one call site of four and called it four. This round wrote *"ends with its LAST on-release action"*
and shipped it for one branch of three. A rule that names a class has to be walked over the whole
class before the round closes, and "the review will find the rest" is not a method.

## 54. The whole class, walked — every gesture-close site in the file

Section 53 ends with "a rule that names a class has to be walked over the whole class", so here is
the walk rather than the promise. Every site that closes a change gesture or clears a latched
identifier:

| Site | Ordering | Disposition |
|---|---|---|
| `mouseUp` delete branch | identifier cleared before the action; `gestureIsStale()` proved at it | **fixed** (§53) |
| `mouseUp` solo branch | all four identifiers cleared before `endBandMove`; staleness proved at the stores | **fixed** (§53) |
| `mouseUp` drag branch (`:2643`) | `dragHandle` cleared before `endGesture` | fixed in the first pass |
| `mouseUp` tail (`:2685`, the WIDTH drag) | `dragBand` cleared before `endGesture` | **fixed here** |
| `cancelActiveDrag` (`:2713-2718`) | `gestureBands = -1` is its FIRST statement, so a reentrant reconcile finds `gestureIsStale()` false and cannot recurse | **self-protected**, unchanged |
| `endBandMove` (`:836-840`) | clears `soloMoveLeft`/`Right` AFTER its own two `endGesture`s | escalated (§44 item D), and now **unreachable by reentrancy from both of its callers** |

**The width-drag tail was opened by this ADR, not found by it.** The clear used to happen at the top
of the handler, which disarmed `tick`'s reconcile for the whole of `mouseUp`; keeping the latch alive
to the end is what made the reconcile reachable at `endGesture (widthP[dragBand])`. Closing it is
paying for what the ADR opened. The width drag fires no on-release action, so it needs no staleness
re-proof — only the two lines that stop it being closed twice.

**`endBandMove`'s own ordering is materially narrowed rather than fixed.** Its two callers are
`mouseUp`'s solo branch, which now clears `soloPressBand`/`soloMovedBand` before calling it (so a
reconcile takes `cancelActiveDrag`'s cheap exit), and `cancelActiveDrag` itself, which clears
`gestureBands` first (so the reconcile's predicate is false). The internal ordering is still worth
tidying, and stays escalated — but it is no longer reachable from either path.

**Mutation record after the amendment**, re-run against the final shape:

| Mutation | Killed |
|---|---|
| the drag branch's post-dispatch re-proof removed | State test 69 leg G only, 1 check |
| the delete branch always refuses | 25 checks, State tests 69, 71, 74, 75, 76, 79 |
| the solo branch always refuses | 6 checks, State tests 76, 79, 80 |

## 55. Finding — "band drags adopt replacement layouts": CONFIRMED, fixed (ADR-0046 completed)

Escalated as §44 items B and C, returned by the review as the round's one confirmed defect. The
classification did not need re-deriving — §44 already had it, and ADR-0046's own comment on
`dragCrossoverTo` states both the mechanism and the ABA consequence in the ADR's own words. What was
missing was the measurement §44 itself asked for ("it needs its own measurement rather than an
argument by analogy"), so that is what this section is.

**Three readings, one proof.** `beginBandMove` reads `bandCount()` for the two pins and the T range;
`moveBand` reads it again for the plan's EXTENT; `writeCrossovers` proves each store against
`gestureBands`, the PRESS's latch. Readings 1 and 2 are separated by `beginBandMove`'s own
`beginGesture` calls, which dispatch — so a host answering the gesture open moves them apart
deterministically. Nothing proves reading 2 at all.

**Why the count check does not save it.** A plan sized HIGH is refused by the first store's
`bandCount() != gestureBands` — unless the count has come BACK by then. That is the ABA, it is
cross-thread only, and ADR-0046 wrote it down a year of rounds ago as the reason `dragCrossoverTo`
takes `n` from its caller.

**Reproduced before fixing, and the instrument had to be corrected twice** — recorded because both
corrections changed the answer:

1. **Geometry.** With `mbFreqHigh` parked at 15 kHz the min-gap packing in `projectFromOrig` never
   reaches it, so `out[2]` equals its origin and `writeCrossovers` elides the store: **0 before and
   0 after**, including with a 300 000-iteration spin widening the ABA window. That is an instrument
   pointed at the wrong place, and it is indistinguishable from a defect that is not there. Parked at
   5.6 kHz — just above split 1, so a rightward move pushes it — the signature fires.
2. **A false positive in the verdict.** Running the lane across `mouseDown` lets some presses latch
   `gestureBands = 4`, after which writing `freqP[2]` is entirely CORRECT. That is what left the
   first post-fix run at 1/1200 rather than 0, and a CI gate built on it would have flaked for a
   reason no reader could reconstruct. The lane is now quiet for the press; the window under test
   lies entirely inside the drag events, so nothing is lost.

**Measured**, pooled over 3600 band moves at four lane spacings: **40 before (1.1%), 0 after**, with
the control writing `freqP[2]` never.

**AND §44 ITEM C IS WRONG, WHICH THE MEASUREMENT SAYS AND THE ANALOGY DID NOT.** That note asserted
`moveBand` and `beginBandMove` were "one change, not two". Mutated separately:

| Mutation | out-of-range writes |
|---|---|
| only `moveBand` reverted (the EXTENT) | 7 / 1200 |
| only `beginBandMove` reverted (the PINS and T range) | **0 / 1200** |
| both reverted (the shipped code) | 40 / 3600 |
| neither | 0 / 3600 |

The extent is load-bearing for this signature; the pins are not. They are threaded anyway — one
reading for the derivation and the proof is ADR-0046's rule, it removes a read rather than adding
one, and a wrong T range is a wrong CLAMP that `projectFromOrig`'s safety pass re-clamps — but that
half is **unmeasured, and the probe header and the ADR both say so** rather than borrowing the
extent's evidence. This is the fourth time this series has had to separate "argued" from "measured",
and the first time the argument came from my own escalation note.

**No new ADR.** This changes no accepted decision: it applies ADR-0046's own rule to the one site
that ADR named and skipped, so it is recorded as an amendment to ADR-0046 rather than as ADR-0053.
No gate item — two private member functions gain a defaulted argument.

## 56. Remaining-review audit — the seven standing residuals, against THIS round's change

The production change is six lines: two private signatures gain a defaulted `int n`, two reads become
`n >= 0 ? jlimit (1, 4, n) : bandCount()`, and two call sites pass `gestureBands`. Nothing else in
`src/` moves. So the question for each residual is not "is it still accurate" — the previous round
verified that across all seven with its own workflow — but "does a band-move extent fix bear on it".

| Residual | Bears on it? | Status |
|---|---|---|
| **RISK-010**, cross-thread topology READER | No. This is a message-thread WRITER in `SpectrumImager`; RISK-010 is `toEngine`'s ten independent `load()`s in `PluginParameters.cpp`, which this PR does not touch at all | unchanged, reopen condition as recorded |
| **`addBandAt` re-attribution window** (ADR-0042) | No. Different function, different transaction; the add's loops are untouched | unchanged, named at the site |
| **held-audition vblank gap** | No. `tick` is untouched and still returns at `isShowing()` before the guard | unchanged, no clean production seam |
| **wheel closure** (ADR-0052) | No. `mouseWheelMove` is untouched; State test 80 legs A–D all pass | unchanged, approved 2026-09-09 |
| **U4**, wheel width undo | No. The wheel's width store is untouched and still opens no gesture | unchanged |
| **partial transaction residue** (ADR-0044) | Not the same residue. ADR-0044's figures are the `addBandAt`/`removeBand` bursts; `moveBand`'s burst is `writeCrossovers`, which is not in that record. The fix does bound `writeCrossovers`' residue to the proved topology, but ADR-0044's statement needs no edit because it never covered this path | unchanged |
| **TSan suppression scope** | No. The file is untouched and the run still reports `Matched 1 suppressions` with one breakdown line | unchanged |

**None reopened, and none needed a change.** The one thing worth carrying forward is that a residual
list is only as good as the question asked of it: "is it still true" and "did what I just did move it"
are different questions, and only the second is this round's to answer.

## 57. The `macos-intel` failure on `6356cc6` — not this PR's, and how that was established

```
Fetching pluginval (pluginval_macOS.zip)...
curl: (6) Could not resolve host: github.com
##[error]Process completed with exit code 6.
```

**Exit 6 is curl's, not pluginval's** — `CURLE_COULDNT_RESOLVE_HOST`. The step died fetching the
validator over the network; pluginval never started, the bundle was never loaded, and nothing about
the plug-in was tested. That is the "died before any test body ran" category, not a validation
result.

**Established rather than assumed, from the same job's own evidence:**

* the failing step is `pluginval VST3 (deterministic x3)`, and the very next step,
  `pluginval VST3 (randomise x3)`, fetched pluginval successfully and passed **3/3 on the identical
  bundle**;
* the two AU steps then passed **3/3 and 3/3** on the same build;
* so nine validation passes succeeded on the artifact the failed step never got as far as opening;
* and the job's own state suite reported **2805 checks, 0 failures** before any of it. (2805, not
  2840: `Editor lifetime` is skipped off Linux under KI-007, which is the recorded platform
  difference and not an anomaly.)

**Action: one re-run, which is the whole allowance.** The rule this file keeps is that "flake" is not
a root cause — a re-run is justified only for a failure that dies before a test body runs, and only
once. This is that case, the root cause is named above rather than shrugged at, and if the re-run
fails again the failure is real and belongs to this PR.

**What is NOT concluded from this:** nothing about `run-pluginval.sh`. The script's own
`classify_pass_exit` never saw an exit code, because the failure happened in the fetch that precedes
it. There is no gap in the classifier to fix here, and inventing one would be the wrong lesson.

**Outcome, 2026-09-09.** The allowance was not spent on a re-run: pushing `031c7f4` (documentation
only — no source, test or workflow file differs from `6356cc6`) superseded the queued re-run and
sent the full matrix at the head that would actually be merged. Run `34352056376` on `031c7f4`
returned **14/14 with nothing red** — 13 jobs `success`, `merge-check` `skipped` because a push
event runs the matrix directly. `macos-intel` passed, including the step that had died in the fetch,
with its four pluginval gates (VST3 deterministic ×3, VST3 randomise ×3, AU deterministic ×3, AU
randomise ×3) and its 2805-check state suite. Same tree, same job, different network: the failure
was in the fetch, exactly as diagnosed, and this PR owns no part of it.

The same run is also the head-of-branch evidence for the ADR-0046 change itself: `linux` reports all
four topology probe gates green — ADR-0047 (the split snapshot is one reading), ADR-0048 (the add
target answers under one topology), ADR-0051 (its edges answer under one reading too) and **ADR-0046
(a band move's plan is sized by the topology the press proved)** — alongside the Clang warning gate,
the ABI floor assertion and both Linux pluginval gates.

## 58. Cleanup round — the two escalated double-close sites, and the dead solo helper

Scope given: audit `cancelActiveDrag` and `endBandMove`'s latch-clear ordering, fix it only if it is
safe, and delete `bandSoloed` if nothing calls it. RISK-010, U4 and the accepted ADR trade-offs were
explicitly out of bounds and were not touched.

### 58a. Workflow audit before any edit

No background agent, workflow or build was running at the start of this round. The two CI monitors
from §57 (`bczeocj85` on `6356cc6`, `b7zp3mbdx` on `031c7f4`) had both ended, and their last events
were consumed rather than re-derived: `sanitizers: cancelled` and `macos: cancelled` on the
superseded run are the concurrency group doing its job, not failures. Nothing was started that
existing evidence already answered — in particular the reachability question here is a *static* one
about two predicates and a *deterministic* one about a listener, so no probe and no sub-agent could
have answered it better than reading the two call sites.

### 58b. Item 1 — `cancelActiveDrag` and `endBandMove`: CONFIRMED, and it is safe to fix

The review filed it as a Bug: *"`cancelActiveDrag` leaves drag identifiers live while `endGesture`
notifies the host. Reentrant cancellation closes the same gesture again."* Confirmed, and the
mechanism is not the one the early clear was thought to cover.

`cancelActiveDrag` clears `gestureBands` at the top, before its cheap exit. **That protects exactly
one of the two reentrant paths into it:**

* `SpectrumImager::tick` — `if (gestureIsStale()) cancelActiveDrag();`. The predicate reads the
  latch, which is already `-1`, so a reentrant tick takes the cheap exit. Covered.
* `AnamorphAudioProcessorEditor`'s stuck-drag reconcile — `if (isMouseButtonDownAnywhere() &&
  ! anyPhysicalMouseButtonDown()) { …; imager->cancelActiveDrag(); }`. **Not covered.** That
  predicate never reads `gestureBands`; it reads the mouse. And KI-013 is why it can still be true on
  the re-entry: the macOS realtime query does not refresh JUCE's cached button state, so the gate
  does not go quiet by itself there the way the comment above it describes for other platforms.

Reached that way from inside `endChangeGesture`, with `dragBand`/`dragHandle`/`soloPressBand` still
live, the nested call **closes the same parameter's gesture a second time** and then clears the
identifiers — so the outer call, on resumption, **skips the sibling gesture it had not reached yet
and leaves it open**. The review named the first half; the second is worse and was found by walking
the function rather than by reading the finding.

**Reproduced before patching**, because the class is reentrant and therefore deterministic — no
thread, no sleep, no test-only production seam. State test 83 arms a real
`AudioProcessorParameter::Listener` that calls `cancelActiveDrag()` from the close it is watching:

```
pre-fix   leg A (split drag)  closed 2 times
          leg B (width drag)  closed 2 times
          leg C (band move)   closed 2 times
          leg D (control)     closed 1 time      <- correct before AND after
post-fix  all four legs       closed 1 time
```

**Safe:** both functions make the same calls, on the same parameters, in the same order, and end in
the same state; only the point at which the members are cleared moves. No ownership predicate changes
its answer on any non-reentrant path — leg D is the control that says so, and the whole 2 851-check
suite is the wider one. The one behaviour that changes is the reentrant path, from a double close
plus a leaked-open sibling to a cheap exit. No new ADR: this is ADR-0050's own rule applied to the
two sites that ADR escalated and carried, so it is an amendment to an Accepted decision, not a new
one, and no `ARCHITECTURE_REVIEW_GATE.md` item is triggered.

**Mutations — and they correct what I first wrote at both sites:**

| Mutation | Killed |
|---|---|
| `cancelActiveDrag`'s clear moved back after the dispatches | legs A + B, 2 checks |
| `endBandMove`'s clear moved back after the dispatches | **nothing** |
| both (the pre-fix tree) | legs A + B + C, 3 checks |

The first draft of both source comments read the second row alone and called `endBandMove`'s half
"defence in depth with no reachable test". The third row says otherwise: leg C is held by the
**pair**, and `endBandMove`'s clear is the layer that still refuses the double close once
`cancelActiveDrag`'s cheap exit is gone. Both comments are corrected in place. This is the same "no
single layer is measurable, only the ensemble" shape §44 records for the `removeBand` count proof,
and the reason a surviving single-line mutation at either site is not evidence of a hole.

### 58c. Item 2 — `bandSoloed` removed

No production caller, no test caller, no doc that depended on the symbol existing. Declaration
(`SpectrumImager.h`) and definition (`SpectrumImager.cpp`) both deleted. The one comment that named
it — the ADR-0041 note in `mouseUp`'s Alt-click solo branch explaining why that branch reads
`soloMask()` **once** instead of asking a helper — is kept and reworded: the argument is about the
double read, not about a symbol, and the note now records that the helper existed, lost its last
caller when the branch was rewritten, and was removed rather than left as a pattern to reach for.
`docs/DOCUMENTATION_COVERAGE.md`'s escalation line is updated from "is dead code" to closed.

### 58d. What this round did NOT do, and why

* **RISK-010, U4, and the ADR-0042/0044 accepted trade-offs** — explicitly out of scope, untouched,
  and re-verified as unmoved by this change: it edits two release/cancel paths and deletes a private
  helper, and touches no store, no plan, no proof and no audio-side reader.
* **The review's Investigate item at `SpectrumImager.cpp:795`** (*"audit history obscures
  invariants"* — the large historical comment blocks) is a real style question and is deliberately
  NOT acted on here. Shrinking those blocks is exactly the kind of edit that loses the measurement a
  later round needs, and this file has twice had a claim restored from a comment that a tidier
  version would have dropped. It belongs to a round that is doing that on purpose, with the
  measurements moved into the ADRs first rather than deleted.
* **The Bug the review filed at `SpectrumImager.cpp:829`** is separately confirmed and is NOT fixed
  here — see §59.

## 59. The review's other Bug, at `SpectrumImager.cpp:829` — CONFIRMED, and deliberately not fixed here
> **CLOSED by §60 (2026-09-09).** The measurement this section asked for exists
> (`--band-move-adopt-probe`, 148 / 18000 before and 0 after) and the fix shipped. Kept as written
> because the reasoning for deferring it — and the one-word fix it identified in advance — is the
> record of how the finding was carried rather than dropped.

*"Band drags adopt later automation. A same-count write between the ownership check and
`captureDragOrigins` replaces the press snapshot. `moveBand` then moves automation the gesture never
owned."*

Verified against the tree rather than taken from the finding, and it is real. It is a **different**
defect from §55's, at the same function: §55 was about the plan's EXTENT being sized by an unproved
reading, and is fixed. This one is about the gesture's ownership RECORD being replaced mid-gesture.

**The chain, with the line each step is on.** `mouseDrag` proves the gesture at its entry
(`if (gestureIsStale()) { cancelActiveDrag(); return; }`), then on the first movement past the 4 px
threshold calls `beginBandMove (soloPressBand, gestureBands)`, which calls `captureDragOrigins()`.
That helper is `captureGestureSound(); seedDragOrigins();` — and `captureGestureSound()` **re-stamps**
all seven `gestureX`/`gestureW` slots from the live parameters. So a same-count write landing between
the gate and that line is written INTO the record the gesture proves against, and `writeCrossovers`
can never refuse it afterwards: it compares against the adopted stamp. `bandAnchorX` is still
`soloDownX`, the press's x, so the projection is anchored to the press while its origins come from
the replacement.

**Why the count half does not save it:** `writeCrossovers` also tests `bandCount() != gestureBands`,
and `gestureBands` is NOT re-stamped, so a count move in that window is still refused. Only a
same-count sound change is adopted — precisely what the review says.

**Class: cross-thread only.** The window runs from `mouseDrag`'s gate to `beginBandMove`'s
`captureDragOrigins()` and contains `plot()` and two integer assignments — no dispatch, no store.
Same class as ADR-0047's, ADR-0048's and ADR-0051's windows, all of which this series CLOSED rather
than accepted, and reachable by a probe rather than by a deterministic test.

**The fix is one word, and is already designed.** `SpectrumImager.h` says of `seedDragOrigins` that
it is *"the origin half of `captureDragOrigins`, on its own, for the one caller that has"* a stamp
already — ADR-0051 split the helper for exactly this shape and converted `mouseDown`'s handle branch.
`beginBandMove` is the same kind of caller: it runs mid-gesture, after `mouseDown` stamped and after
`mouseDrag`'s gate proved that stamp still current, so it should derive its origins from the stamp it
holds instead of taking a new one. And the conversion is provably inert when nothing is racing:
`soundMovedUnderGesture` compares all seven slots in normalised units **exactly**, so past the gate
the stamp equals the live values bit-for-bit and `seedDragOrigins()` yields identical `dragOrigX`.

**Not applied in this round, and that is a scope decision rather than a disposition.** The brief for
this round named two items and listed what not to touch; a third production change to the band-move
path — the same path §55 changed — belongs with its own measurement (`--band-move-probe` extended to
count adopted layouts, before and after) rather than as a rider on a cleanup pass. It is carried as
an OPEN confirmed finding, not as an accepted residual, and it is the one review blocker this round
leaves standing.

## 60. §59 closed — "band drags adopt later automation" is fixed (ADR-0051, applied again)

§59 recorded this as the one OPEN confirmed finding and said the fix "belongs with its own
measurement". That measurement now exists and the finding is closed.

### 60a. Workflow audit before any edit

Nothing was running: no subagent, no workflow, no monitor, no build. One unconsumed artefact existed
and was consumed rather than re-derived — CI on `7610588`, push run `34365192043`, **13 success / 1
skipped** (`merge-check`, by design on a push event), with all four PR-event workflows green on the
same SHA. That is the baseline this round started from.

**What was started, and why it was not redundant.** A nine-agent workflow ran the *invariant* question
— three independent derivations from different lenses, two adversarial writer audits, and four
refuters each attacking a different claim of the proposed fix. It was not a duplicate of anything:
§59 named the mechanism and the candidate fix but proved neither, and the standing instruction was to
establish the invariant *before* applying `seedDragOrigins()`. Result: **0 of 4 refuters refuted**,
all three derivations converged on the same invariant, and both writer audits returned
`SpectrumImager.cpp:828` as the ONLY mid-gesture blanket write in the file. Four of its residual
findings were acted on and are marked below; one — a claim that ADR-0051 names only `mouseDown`'s
handle branch and that extending it is therefore an amendment needing a human gate — was checked
against the ADR itself and **rejected**: see §60c.

### 60b. The invariant

> **A gesture's ownership record — `gestureBands`, `gestureX[0..2]`, `gestureW[0..3]` — is stamped at
> a gesture START and is thereafter immutable except through the gesture's OWN confirmed stores
> (`storeOwned`, which advances a slot only after read-back proves the store landed). Nothing else
> may write it while it is in force.**

That is what makes `exactlyEqual (freqP[k]->getValue(), gestureX[k])` a decision procedure for "it
moved and I did not move it". A blanket copy of the live row into the record destroys the procedure
rather than refreshing it.

Answering the four questions the brief posed, in order:

* **Which snapshot has been proved?** The press's. `mouseDown` stamps at the top of the handler for
  every branch; the solo branch stores nothing; and `mouseDrag`'s first statement is
  `if (gestureIsStale()) { cancelActiveDrag(); return; }`, which compares all three splits and all
  four widths against the record with `juce::exactlyEqual` in normalised units. `beginBandMove` is
  reached a few instructions later.
* **What must stay immutable?** `gestureX` and `gestureW`, from that proof until the gesture ends,
  except through `storeOwned`.
* **Which helper captures origins without re-stamping?** `seedDragOrigins()`, which derives
  `dragOrigX[k]` FROM `gestureX[k]` and writes nothing back. Everything `beginBandMove` computes
  (`bandStartLeftX`/`bandStartRightX`, then `bandTmin`/`bandTmax`) comes from `dragOrigX`, so after
  the change the function reads no live parameter at all — the window does not narrow, it **stops
  existing**.
* **Locks or DSP-thread changes?** None, and none permissible. One token on the message thread.

### 60c. No human approval is required, and that was checked rather than assumed

ADR-0051's Decision is general — *"a pass that has already stamped the row derives from the stamp
rather than stamping again"* — and names exactly ONE exception, the add branch, because `addBandAt`
has just changed the count and written the row. `beginBandMove` is a pass that has already stamped
and is not that branch, so this is **the rule applied to its second site**, not a narrowing of it.
That is the distinction from ADR-0052, which genuinely narrowed accepted ADR-0041's scope and was
correctly raised as a hard-stop gate item. Nothing in the `ARCHITECTURE_REVIEW_GATE.md` list is
touched: no parameter ID, no serialization schema, no threading model, no DSP signal order, no
reported latency, and no conflict with an Accepted ADR.

The wheel's `captureDragOrigins()` was checked too and is a third, legitimate case:
`cancelActiveDrag()` has cleared `gestureBands`, so no record is in force when it stamps.

### 60d. Evidence — proved before it was patched

The window is **cross-thread only** (`plot()` and three scalar assignments; no store, so no
dispatch), so a probe and not a state test. The existing `--band-move-probe` cannot see it: its lane
drives `mbBands`, and this is a value-half defect. `--band-move-adopt-probe` was built for it — the
lane writes `mbWidthHigh` once per iteration, because a band move never writes a width.

```
before   148 / 18000 band moves
after      0 / 18000
```

one second either way, with the mandatory control line printing *late crossover stores SEEN* in both.

**Three instrument corrections, all recorded in the probe's own header because each changed the
answer:**

1. the detector watched `freqP[1]` while the discovery loop pressed **band 0**'s solo button — a
   band-0 move pins only its right edge, so `freqP[1]` never moved and the control was silent;
2. the lane was released **after** `mouseDown`, and a blocked thread's wakeup latency alone carried
   the write past the whole of drag event 1 — 0 before AND after;
3. the first handshake spun on a level flag: it starved the message thread badly enough that 1200
   iterations did not finish in forty-five minutes, and its "wait for the flag to fall" edge could be
   skipped by the next iteration raising it again, which **deadlocked** the probe.

**And the reading is load-sensitive**, which is stated in the header rather than left for someone to
trip over: the same instrument measured **4 / 1200** on a box busy with another build and
**148 / 18000** on an idle one. The direction is never wrong — a non-zero total is always a real
adoption — but a small total is not evidence of a small defect.

### 60e. Regression and mutation

* **State test 84**, four legs, with the same honest scope as State tests 81 and 82: it *cannot* fail
  on the defect, and says so in its header. Leg A pins reversibility (cursor back to the press point
  restores the press's own splits — a direct assertion about `dragOrigX`); legs B and C that a
  foreign width and a foreign split during the move still void the gesture; leg D the positive
  control that an undisturbed move keeps committing.
* **Leg A's first draft asserted the wrong contract** and is recorded rather than replaced quietly:
  it claimed a rigid pixel translation leaves the two edges' frequency ratio invariant. It does not
  (10.000 → 9.357), because the three split parameters have their own ranges and quantisation.
* **Mutation — the one-word revert.** The probe goes 0 → 148 / 18000. The state suite stays at
  **2 860 / 0**, and leg A reports the same frequencies **to the digit**. That is not a coverage hole:
  it is the inertness claim measured rather than argued, and it is the reason the deterministic
  legs are labelled as contract cover rather than as proof of the fix.
* **CI gate** wired into the `linux` job beside the ADR-0046/0047/0048/0051 probes, at 3000 × 6 =
  18 000 moves — chosen for *power*, not habit: at that count the pre-fix defect produces ~148 hits,
  so a regression cannot slip through as a lucky zero.

### 60f. Residuals from the panel that were acted on

* `beginBandMove`'s `n` **lost its default argument**. The function now depends on its caller having
  proved the record; a defaulted parameter would let a future second caller reach that dependency
  with nothing proved and no diagnostic.
* **Three comments went stale the moment the call changed** and were corrected in the same edit, per
  `DOCUMENTATION_LIFECYCLE_POLICY`: `captureGestureSound`'s "called at every gesture start" (now says
  *and nowhere else*, with why); `captureDragOrigins`' header in the `.h` ("call this at every drag
  start"); and `seedDragOrigins`' "the ONE caller that has already stamped", which is now two.
* **`--band-move-probe`'s own comment was wrong** and is corrected: it says it presses band 1's solo
  button and it presses band 0's — its inline run-skipping loop advances past the second run before
  the `seen == 1` test is evaluated, so the fallback takes the first button. Verified by measurement,
  not by reading. Its *measurement* is unaffected (a band-0 move with an over-sized extent does write
  `freqP[2]`, which is the signature it counts), so the label was fixed and the discovery left alone
  — rewriting it would move the instrument.
* **What it does NOT do** is stated at the call site and in the ADR: the fix makes the race
  adoption-free, not write-free. A foreign width landing in the old window is refused at the NEXT
  event's gate, so the crossover burst already in flight still goes out.

### 60g. Not touched

RISK-010, the `addBandAt` re-attribution window, the held-audition vblank gap, wheel gesture closure,
U4, ADR-0044's partial-transaction residue, cancelled-spread visual ordering and the TSan suppression
scope were all out of bounds by instruction and none was reopened; this change edits one call in one
function and touches no store, no plan, no proof and no audio-side reader. The historical-comment
question at `SpectrumImager.cpp:794` was likewise left alone — no correctness or documentation error
was found in that block; the three stale comments listed above are elsewhere and were fixed on their
own merits.

## 61. The wheel latch's ROW half (ADR-0045 applied again), and the removeBand coverage gap closed

### 61a. Workflow audit and lifecycle decision

Nothing was running: no subagent, no workflow, no monitor, no build; one zero-byte task stub with
nothing in it. One unconsumed artefact, consumed rather than re-derived: CI on `20647a9` —
**20 successful check runs, 14 skipped, none red** across every workflow on that SHA.

**No workflow was started, and that is the recorded decision.** Ultracode is off for this round, so
the Workflow tool's standing opt-in does not apply; and the two questions here — the lifecycle of one
latch in one handler, and whether an existing suite covers one guard — are answered by reading the
code and by running mutations against the suite, both of which are direct and exact. A fan-out would
have duplicated reading with less precision. The previous round's nine-agent panel was justified
because the invariant was contested; this one is not.

### 61b. The finding: CONFIRMED

*"Wheel bursts target stale bands."* Established from the code rather than assumed:

* **What `scrollBands` stores:** `bandCount()` at latch creation, and nothing else.
* **What the latch IS:** `scrollHandle`/`scrollBand`, **indices**, derived from the pointer x under
  one reading of the count and one of the split row. `scrollAnchor` stores the pointer position, not
  the target's.
* **Created:** on the first tick of a burst, when both indices are negative.
* **Invalidated:** by `mouseMove` more than 3 px from `scrollAnchor`, by `mouseExit`, and by
  `bandCount() != scrollBands`. **By nothing else.**
* **Which automation invalidates it without changing the count:** any write to `mbFreqLow/Mid/High`.
  The split row alone decides where every handle and every band boundary is drawn, so a same-count
  split move re-lays the display out under a stationary hand — and none of the three invalidations
  above can see it. `scrollAnchor` cannot, because it tests the POINTER, and the pointer has not
  moved.

The ADR's own bullet argues this case in its own words for the count: a change "re-lays the whole
display out under a hand that has not moved, so the next tick steered a split the pointer was no
longer over". Only the count half was implemented.

**Reproduced before patching**, and unlike the ADR-0046/0047/0051 windows this one is deterministic:
it lies **between two wheel ticks**, i.e. user time, so a single-threaded test enters it directly.
State test 77 leg E on the pre-fix tree:

```
[leg E] the wheel steered a band its latch named under another split row:
        band 1 moved 1.060 -> 1.120 after a same-count split move under a hand that never moved
```

### 61c. What the next tick should do, and why

Four options were considered against the existing wheel UX:

* **Re-hit-test every tick** — REJECTED. The wheel's own edits move the split it is steering, so the
  burst would jump to a neighbour mid-burst. This is precisely why the latch exists.
* **Invalidate the burst** — REJECTED, for the reason the count case already gives: dropping and
  re-deriving costs the user nothing, because the re-derivation happens in the SAME tick and the tick
  still edits.
* **Revalidate by re-deriving at `scrollAnchor`** — REJECTED. The burst's own edits walk the target
  away from the anchor, so an ordinary split burst would invalidate itself after one tick.
* **Stamp the row beside the count, and refresh it with the burst's own confirmed stores** — CHOSEN.
  It is ADR-0045's rule with "topology" read as ADR-0039 and ADR-0051 define it.

### 61d. Implementation

`scrollFx[3]` records the split row the latch was derived in, seeded from the **same single**
`captureSplits` reading the derivation uses (ADR-0051 — the read is hoisted so the staleness test and
any re-derivation share one reading). A tick whose live row differs drops the latch, and the existing
re-derivation block re-aims it in the same tick.

After a split store the stamp is **derived** from `gestureX`, which `writeCrossovers` maintains
through `storeOwned`, rather than re-read from the parameters — one reading per pass, and no window
in which a foreign write could be adopted, which is the defect §60 closed one function away. A
refused store leaves the stamp at the pre-store row, so the next tick retargets: a store this burst
did not land is not a row this burst owns.

ADR-0041 and ADR-0052 are untouched — the delta test and `cancelActiveDrag()` still run first and
first, so an input that performs no edit still has no side effects. No lock, no audio-thread change.

### 61e. Mutations, including one that survives

| Mutation | Killed |
|---|---|
| the row comparison removed (the pre-fix, count-only shape) | leg E, 1 check |
| the stamp refresh after a split store removed | leg F, 1 check |
| the seed at latch creation removed | **nothing** |

The third is recorded as unkilled rather than hidden or deleted. Without the seed a **width** burst
re-derives its latch every tick, and over an unchanged row that is idempotent, so nothing observable
differs; what it really costs is that `scrollAnchor` is re-stamped each tick, so the 3 px pointer test
stops measuring drift from where the burst began. No test here separates those.

**Leg F had to be re-aimed, and the reason is recorded in the test.** At the 0.20 delta the rest of
the test uses, one tick moves the split 5.6 px — inside `handleNearX`'s 7 px grab radius — so a
wrongly dropped latch re-derives onto the SAME handle and the leg passes on broken code. Measured: at
0.20 it killed neither stamp mutation. At 1.0 the handle walks 28 px clear and the leg discriminates.

### 61f. The `removeBand` per-store re-proof gap — measured, then closed

Re-measured open on this tree first: deleting the destination re-proof from **both** loops left all
2 866 checks green. Leg F of State test 76 kills the ADR-0044 **mask** re-proof, legs B and C are the
ADR-0042 controls, and legs I/J cover the spread — none of them touches this one.

New legs K (width loop) and L (split loop). **They are not the opposite of legs B and C**, which is
the question to ask before adding them: B and C poke a slot the transaction has ALREADY finished with,
and ADR-0042 rules that such a write is a newer authority and must stand — it does, and they still
pass. K and L poke a slot the plan has NOT yet reached, from inside the store one iteration earlier.
Already-written versus about-to-be-written is exactly the line ADR-0042 draws. Both are isolated from
the ADR-0049 source proof, which at the relevant iteration reads a slot the legs never touch.

| Mutation | Killed |
|---|---|
| the width loop's destination re-proof removed | leg K, 3 checks |
| the split loop's destination re-proof removed | leg L, 2 checks |
| both | 5 checks |

**No production line changed for this**, per the instruction: the legs assert shipped behaviour.

**Two test-construction mistakes are recorded because each changed the answer.** Leg K's "the newer
width is not overwritten" check was **vacuously true** under its own mutation at the shared fixture's
values — `nw[1]` equalled `wd[1]`, so the k = 1 store was elided by the loop's own "the plan IS the
world here" line. Band 2's width is now set explicitly in the leg and the check discriminates.
And leg L, which must poke the same parameter PAIR as leg C in the opposite direction, gave
ThreadSanitizer a **lock-order cycle** (M0 ⇒ M1 ⇒ M0) through `setValueNotifyingHost`'s listener
lock — a real red TSan run on this tree, single-threaded and impossible to deadlock, but red. The
choice was a second `deadlock:` suppression or removing the second lock; removing it won.
`WriteFromInsideAStoreQuietly` uses `setValue`, which changes exactly what the guard reads and takes
no listener lock, so the suppression file stays at ONE entry and the CI count assertion is untouched.

### 61g. Not touched

RISK-010, the `addBandAt` re-attribution window, the held-audition vblank gap, wheel gesture closure,
U4, ADR-0044's partial-transaction residue, cancelled-spread visual ordering, the TSan suppression
scope and the historical comments at `SpectrumImager.cpp:794` were all out of bounds by instruction.
None was reopened, and no new evidence about any of them appeared. The `:794` block was read while
tracing the latch and contains no correctness or documentation error.

## 62. Release actions and wheel ticks — the proof-to-action boundary

### 62a. Workflow audit and lifecycle decisions

Nothing was running. Three workflow transcripts exist and all three are COMPLETE — 15, 34 and 9
agents, no unfinished agent, no empty result — and all three had their findings consumed and shipped
in earlier rounds. **Decision: close all three.** Resuming would replay cached answers to questions
that are no longer open. CI on `f9a5266` (20 successful check runs, 14 skipped, none red) was
consumed as the baseline rather than re-derived.

**One new workflow WAS started** — five agents, for independent invariant derivation only, not to
duplicate reading I did directly: two reachability derivations for the release finding (one of them a
deliberate skeptic), one for the wheel, and a judge/refuter pair on "one invariant or two" plus the
gate. It earned its cost twice over: the skeptic could not refute the release finding, and one agent
found something neither the review nor I had — see §62c.

### 62b. Finding — the wheel's WITHIN-TICK window: CONFIRMED (ADR-0047)

A tick reads the row once at the top and everything deciding *which* control it steers derives from
that reading; the split branch then called `captureDragOrigins()`, a **second** reading. A same-count
write between the two is proved absent by the first and adopted by the second, so the tick steers the
latched handle from its new position with every store proved against the row it just adopted.
Distinct from `scrollFx`, which closes the window *between* ticks. Window bounded by pure reads —
cross-thread only.

### 62c. Finding — the release actions: CONFIRMED, and it is TWO windows, not one

The review described one window. It is two, with different reachability classes, and the panel's
skeptic could not refute either.

* **Delete leg — cross-thread only.** `removeBand` takes its OWN second reading of the row and the
  widths at entry and plans from it; every later guard compares against that reading, so an install
  landing between the caller's gate and it is baked in and invisible. The COUNT never had this
  problem because `expectedBands` makes the two reads one. Fixed by taking the snapshot in normalised
  units, deriving the plan's Hz from it, and proving the press's ownership from it through
  `ownsSplit (k, norm)` / `ownsWidth (b, norm)` — the overloads ADR-0047 added for exactly this.
* **Solo leg — REENTRANT, and the file said the opposite.** `mouseUp`'s ADR-0050 comment asserted the
  solo store paths "are CROSS-THREAD ONLY and no deterministic test enters them". True of the
  handler's body; false of the window that matters, which continues into `setSoloMask`, whose
  `beginChangeGesture()` dispatches **ahead of its guard** — and that guard proves count and mask
  only. This is ADR-0045's own second sentence ("a store whose gesture bracket dispatches before it
  proves the topology inside the bracket") with the sound half missing. Fixed by one clause,
  `&& ! soundMovedUnderGesture()`, inside the bracket. The comment is corrected in place.

### 62d. One invariant or two — the panel split, and the honest answer is "neither extreme"

The judge said one rule (ADR-0047 generalised); the refuter said two and that a new ADR was needed.
Resolved from the ADR texts: **three sites, two already-accepted rules, no new ADR.** The wheel and
`removeBand` are both ADR-0047 ("one read, derive the plan and the proof from it"). The solo store is
ADR-0045's bracket sentence. Forcing all three into one story would have required widening ADR-0047's
scope — which is exactly the kind of move ADR-0052 needed human approval for, and it is not needed
here because each site falls inside a rule as written.

### 62e. Evidence, stated at its real strength

| Guard | Class | Mutation |
|---|---|---|
| `setSoloMask`'s `! soundMovedUnderGesture()` | reentrant | **kills State test 79 leg C** |
| `removeBand`'s entry ownership proof | cross-thread only | kills nothing |
| the wheel's proved-row stamp | cross-thread only | kills nothing |

The last two are defence in depth with no reachable test, on the same footing as `mouseUp`'s
`gestureBands == pressBands` and `removeBand`'s delete-x call site. Both are correct by ADR-0047's
own rule and inert with nothing racing (the caller's gate has just proved the same values), and the
whole 2 879-check suite is unchanged by either.

### 62f. A probe was built for the wheel window and WITHDRAWN

`--wheel-adopt-probe` reached the tree, measured, and was removed rather than shipped. Recorded
because the failure is instructive:

1. **A control-only bug first.** `oneBurst` reset the `laneLanded` flag at its top, which cleared the
   flag the control had just set — so the control reported "tick 2 does not steer split 1" on a tree
   where `mbFreqMid` plainly moved 2000 → 2172.7 across the two ticks. The instrument was broken, not
   the code, and the mandatory control is what caught it.
2. **Then the real defect in the instrument.** With the control passing it read 38/1200 pre-fix — and
   **34/1200 post-fix**, which is not a signal. A diagnostic on the counted iterations showed
   `mbFreqMid` ending at 2172.7, the ordinary two-tick value, not the lane's 8000: the detector gated
   on a flag the lane set *after* its store returned, with no ordering against the message-thread
   store, so it was counting benign ticks.

A gate that cannot fail is worse than no gate, so it is not in the tree. The wheel guard therefore
ships unmeasured and is labelled as such above.

### 62g. Architecture-review gate: NOT triggered

Walked against `docs/policies/ARCHITECTURE_REVIEW_GATE.md`, and both panel agents — including the
refuter, who was arguing for a new ADR — reached the same conclusion independently. No DSP graph, no
signal-flow, no parameter registry, no serialization, no latency, no plugin format. **Thread model:**
the policy defines it as "new thread, new cross-thread path, new atomic ordering"; all three changes
*remove* parameter reads and add no lock, atomic, allocation or shared object, so the cross-thread
surface strictly shrinks. **Build system:** defined as "CMake structure, JUCE version/pin, dependency
set" — this round changes no workflow file at all, and adding CI *steps* in earlier rounds was none of
those three. No accepted ADR is conflicted: each site falls inside ADR-0045 or ADR-0047 as written.
**No human approval is required, and none is manufactured.**

### 62h. Residuals — re-verified, none reopened

RISK-010 (audio-side reader, untouched), the `addBandAt` re-attribution window, the held-audition
vblank gap, wheel gesture closure under ADR-0041/0052 (the delta test and `cancelActiveDrag()` still
run first and first — unchanged by any edit here), U4, ADR-0044's partial-transaction residue,
cancelled-spread ordering, and the TSan suppression scope (still exactly one entry, still
harness-scoped, count assertion untouched). The historical comment block was read while tracing both
windows; the only error found in it is the solo reachability sentence, which is §62c and is corrected.

### 62i. CI failure on `22a5e1d` — a `-Wshadow` in the change set, and why preflight missed it

`linux` step 25 and `linux-lto-tests` step 9 both failed on
`src/gui/SpectrumImager.cpp:1208: warning: declaration of 'int b' shadows a parameter [-Wshadow]`.
The new width-ownership loop used `b`, which is `removeBand (int b, int expectedBands)`'s own
parameter. A **code issue in the change set**; the gate did its job. Renamed to `w`.

**Why it escaped the local sweep, which is the part worth keeping.** `scripts/preflight.sh`'s advisory
first-party sweep runs `-Wall -Wextra`, and `-Wshadow` is in NEITHER of those — it has to be asked for
by name. The pinned CI gates carry it and the local smoke alarm did not, so the sweep was green on a
tree the gate rejects. `-Wshadow` is now in the sweep, verified to run clean after the rename. This is
the second time this sweep has been widened by a warning that reached CI first (the first was
`-Wunused-variable`, which it did catch once added), and the pattern is the same: the advisory sweep
is only as good as the flags it is given.

### 62j. A false measurement in the shipped source, found on the final re-read and corrected

The in-source comment at the wheel's within-tick stamp ended *"Measured at 38 in 1200 ticks, 0 after
(`--wheel-adopt-probe`)."* **That claim is false**, and it is the exact failure this round spent §62f
documenting: the probe read 38/1200 before and **34/1200 after**, the diagnostic showed it counting
benign ticks, and it was withdrawn. ADR-0047's own "Applied again" section, `TESTING.md` and the
coverage pass all say so correctly; only the source comment — written before the probe was
measured and not revised when it was withdrawn — carried the number as if the probe had confirmed
the fix.

Found by re-reading the round's own production diff against the final head rather than by any gate,
which is worth stating: no lint, no test and no CI job can see a comment that claims a measurement
nobody took. The comment now says the guard is UNMEASURED, gives the two readings and the reason the
instrument was withdrawn, and points at `TESTING.md` and this worklog.

The same re-read corrected the wrapping of the `mouseUp` comment edit from §62c and made its referent
explicit: the sentence about the cross-thread-only class ADR-0046/0047/0048/0051 closed belongs to the
DELETE half, since the solo half is now the reentrant one, with State test 79 leg C named as the test
that enters it.

Comment-only in both cases: object code identical, and the suite re-ran at 2 879 / 0.

## 63. The press hit-test — ADR-0046's two named exceptions, and the one that was open

### 63a. Workflow audit and lifecycle decision

Nothing was running: no workflow, no monitor, no background task, no unfinished agent transcript,
no process but the session itself. Every task on the checklist was complete. One unconsumed artefact,
consumed rather than re-derived: CI on `5fe9f23` — push run `34426995984` **13/13 success** with
`merge-check` skipped by design, and all four PR-event workflows green on the same SHA.
**Decision: start nothing that duplicates direct reading, and start exactly one agent for
independent derivation.**

**One agent was started, and it earned its cost three times over.** Its brief was adversarial — try
to refute the finding — and it (i) **refuted my own geometric conclusion**, see §63e; (ii) supplied
the three independent mechanisms that make `deleteHit` safe, two of which I had not found; and
(iii) found a new actionable item, `nearWidthLine`, which went on the checklist as **G6 before any
code was written**, per the round's own rule.

### 63b. The finding: CONFIRMED, and it is the one exception ADR-0046 got wrong

`mouseDown` stamps `gestureBands = bandCount()` and `captureGestureSound()` on its first two lines,
derives `pressF[]` from that stamp, and then called `soloHit (p)` — which read `bandCount()` for
itself and let `soloBox` → `bandLeftX`/`bandRightX` read `crossover()` for themselves. Counted: **9
live parameter reads for a two-band hit, 21 for a four-band one**, none of them the reading the press
proved. `soloHit` was not even internally consistent — its loop bound `N` and the `bandCount()`
inside every `bandRightX` are different readings.

**Every statement between the stamp and `soloPressBand = sh` is a pure read**, so nothing on the
message thread can dispatch into the gap: **CROSS-THREAD ONLY**, roughly 50–200 ns wide.

### 63c. Why this one was fail-OPEN when the file said it was fail-safe

`soloPressBand` is consumed by three places — `tick`'s hold audition, `mouseDrag`'s band move,
`mouseUp`'s toggle — and **none re-derives it**. Each is guarded by `gestureIsStale()`, which proves
the world still MATCHES the press. It cannot prove the index was DERIVED in that world, because the
reading `soloHit` used is recorded nowhere. An **ABA return** therefore erases the disagreement
before any guard runs.

Nothing downstream can catch it either: `toggleSoloBit` range-checks `b` **not at all**, and
`setSoloMask` deliberately admits a bit above the live count because a **parked** solo bit is a
designed, tested state (State test 79 leg A). The index had to be right at derivation or not at all.

`mbBands` is a four-valued `RawInt`, so "returns to the same value" is an ordinary automation shape
rather than a coincidence — and **no split or width write is needed**, so `soundMovedUnderGesture()`
is false by construction throughout.

**Consequence:** `0x8` stored at two bands is masked to nothing by `SoloMonitor` — the user clicks a
headphone, hears no solo, and still pays one automation write and one undo entry. That is verbatim
the failure ADR-0043 recorded and believed it had closed with `gestureIsStale()`; it is reached by a
route that gate cannot see, because the index was wrong *at derivation* rather than made wrong later.

### 63d. The file contradicted itself, 1 600 lines apart

`mouseDown`'s comment and ADR-0046's "What was NOT done" both said these two derivations are "the
fail-safe half only … a disagreement makes `gestureIsStale()` refuse". ADR-0046's **own amendment**,
one section above, argues the opposite for `beginBandMove`:

> …an ABA return to the stamped count between the two reads would let a plan sized under the wrong
> topology through.

Same shape, same reasoning, opposite conclusion. That internal contradiction is the strongest
evidence the finding is real, and it is what makes this an **amendment to an accepted ADR's scope
paragraph** rather than a new decision.

### 63e. I had it wrong, and the agent proved it

I concluded the review's worked example — a press aimed at band 1 of a two-band layout returning band
3 of a four-band one — was **geometrically unreachable**, because band 3's headphone centre is
`½(x(f₂) + R)` against band 1's `½(x(f₀) + R)`, needing `|x(f₂) − x(f₀)| < 36 px` where the UI keeps
splits `kMinGapPx = 46` apart.

**That reasoning is wrong, and the error is worth keeping.** `kMinGapPx` is applied **only** by
`projectGaps`, to plans this component computes. The three split parameters are independent,
unconstrained `logFreqRange` parameters; `MultibandWidth`'s ordering clamp is applied to a **local
copy** and never written back; `reassertParameters` applies restored values verbatim. So
`mbFreqLow == mbFreqMid == mbFreqHigh` is legally installable — and there bands 1 and 2 have **zero
width**, fail the 30 px gate that hides a headphone, and are skipped, putting band 3's centre exactly
on band 1's. **The review's example is exact, not approximate.** Measured on the shipped layout at
x = **677.5**, which the probe then discovered independently.

### 63f. The fix, and why it is the smallest one

Six private member functions gain defaulted arguments (`bandLeftX`, `bandRightX`, `soloBox`,
`deleteBox`, `soloHit`, `deleteHit`), and `nearWidthLine` a seventh. `mouseDown` passes the reading
it already took; `updateHover` passes the one ADR-0048/0051 already made it hoist; `mouseUp`'s delete
release derives `relF` from `gestureX` — pure arithmetic on a value already read, so **no parameter
reads are added anywhere and many are removed**. The hit-tests resolve the count **once** and hand it
down, so even callers that pass nothing lose `bandRightX`'s internal re-read. `paint` never calls any
of them (it draws from the eased `dispLeftX`/`dispRightX`), so the display is untouched.

### 63g. `deleteHit` was NOT open, for reasons the ADR never gave

Three independent mechanisms, none of them `gestureIsStale()`: `deleteBox` depends on `bandLeftX`
alone, which reads **no** count, so a count rise can only *append* candidates above the first match;
`removeBand` rejects `b >= expectedBands` outright; and `mouseUp` re-runs the hit-test and requires it
to name the same band. It is threaded because one reading per pass is the rule and it removes reads —
**not claimed as a defect fixed**. `nearWidthLine` likewise: a wrong answer starts or fails to start a
width drag on the band the *proved* row put under the cursor, and every store is refused by
`ownsWidth` against that same record.

### 63h. Evidence

| Mutation | Killed |
|---|---|
| M1 — `mouseDown` takes the live hit-test again (the exact pre-fix shape) | **nothing** deterministic; `--solo-alias-probe` 491/1200 |
| M2 — the press hit-test answers under an unproved count of 4 | State test 85 legs B, C, E — 5 checks |
| M3 — `bandRightX` ignores the threaded count | **nothing** |
| M4 — `soloBox` ignores the threaded split row | **nothing** |
| M5 — the release-time delete confirmation reads live again | **nothing** |
| M6 — that confirmation answers under an unproved count | leg D + 27 checks across the delete tests |

M1/M3/M4/M5 surviving is the honest result: the window holds no dispatch, so no single-threaded test
can enter it. State test 85 holds the **contract**; the probe measures the **window**.

### 63i. The probe CAN fail, and that was proved before it was shipped

`--solo-alias-probe`, unlike `--wheel-adopt-probe` withdrawn in §62f:

| | presses that soloed a band the press's layout has not got |
|---|---|
| before | **491 / 1 200 (41 %)** — spins 0/40/120/400: 166, 191, 132, 2 |
| after | **0 / 1 200** |

Two design decisions make it a signature rather than a coincidence. The **lane runs across
`mouseDown`**, because that is where the window is. And it is **stopped, with the count pinned to 2,
before every release** — so a press that legitimately latched four bands is refused by
`topologyMovedUnderGesture()` and writes nothing, instead of being counted as a defect. That is the
false-positive class `--band-move-probe`'s own header records having been caught by. The **control
line is mandatory and aborts**: on a clean tree the aliased x must solo band 1, and it does, at both
ends of the comparison.

### 63j. Gate: NOT triggered

No DSP graph, signal flow, parameter registry, serialization, latency or plugin format. **Thread
model** — no new thread, no new cross-thread path, no new atomic ordering; the change removes
parameter reads, so the cross-thread surface strictly shrinks. **Build system** — one CI *step* added,
which is none of "CMake structure, JUCE version/pin, dependency set". No accepted ADR is conflicted:
ADR-0046's Decision authorises this verbatim, and only its scope paragraph is amended. **No human
approval is required, and none is manufactured.**

## 64. The record a release action owns, dropped by something else (ADR-0050, applied again)

### 64a. Workflow audit and lifecycle decision

Nothing was running: no workflow, no monitor, no background task, no unfinished transcript, no
process but the session. Every checklist item was complete. One unconsumed artefact, consumed rather
than re-derived: CI on `169a9a1` — push run `34431318103` **13/13 success**, `merge-check` skipped by
design, and all four PR-event workflows green on that SHA.

**One workflow was started**, three independent lenses (call chain, disarm surface, skeptic) each
adversarially refuted by three more, then a four-option design panel and a judge — for independent
derivation, not to duplicate reading. **Its verdict is not what settled this round**: the finding was
reproduced deterministically from the code before the panel returned, and the reproduction is the
evidence of record.

### 64b. The finding: CONFIRMED, and reproduced on one thread

`cancelActiveDrag()` opens with an unconditional `gestureBands = -1;` and only *then* takes the cheap
exit for "no identifier is latched". A release branch in `mouseUp` clears its identifiers **before**
it dispatches — which is ADR-0050's own instruction — so a re-entrant call finds nothing to close,
takes the cheap exit, and **still drops the record on the way past**. Every ownership predicate in
this class self-disables at `gestureBands < 0`.

`setSoloMask` calls `beginChangeGesture()` **before** its guard, so the window is **REENTRANT**: a
host answering the gesture open by pumping the message loop re-enters `cancelActiveDrag` with the
store on the stack. **Two vectors, and the adversarial pass found the stronger one after this section
was first drafted** — recorded as a correction rather than smoothed over. The primary vector is
`SpectrumImager::tick`'s OWN reconcile, `if (gestureIsStale()) cancelActiveDrag();` (`:1739`), whose
gate is true *precisely because of* the same-count install that lands inside the bracket, and which a
nested pump delivers as a message-thread VBlank callback — no mouse state, no KI-013. The editor's
stuck-drag reconcile (`PluginEditor.cpp:1538-1543`) is the weaker one: JUCE updates the source's
button state *before* dispatching `mouseUp` and the native peers clear the button bit, so for a real
in-window release that gate is normally already false, and it survives only through KI-013's stale
macOS cache.

**State test 79 leg E** is leg C with that cancellation added ahead of the identical install:

```
[leg E] a re-entrant cancellation disarmed the record and the solo bit was written anyway:
        mask 0x8, split 1 2000.0 -> 6500.0 Hz
```

Leg C refuses that bit; leg E is the same defect reached around the same guard. **Only leg E failed**
on the unfixed tree — 2 896 checks, 1 failure — so the reproduction is isolated.

### 64c. The class already knew, at one branch

The handle-drag branch defends itself with a bespoke `gestureBands == pressBands` compare and states
the mechanism in its own comment: *"clearing `dragHandle` above sends a reentrant reconcile down
`cancelActiveDrag`'s cheap exit, which clears `gestureBands` … and `gestureIsStale()` would answer
`false` because the latch is gone."* That was **one branch's local defence against a general
defect**, and this is the same "applied to one branch of three" shape this ADR already had to correct
once.

**Per branch, at its real strength — and the first draft of this section overstated the delete
branch, corrected here.** Solo: the live defect, and the only branch where the disarm becomes a wrong
write; its count and mask guards survive as locals, so exactly the sound half ADR-0050 added is what
is lost. Handle drag: fail-safe but **lossy** — the compare turns the nested cancel into a refusal, so
a legitimate outward-drag removal is silently dropped. Delete: **not reachable by reentrancy at all**
— no dispatch sits between `pressDeleteBand = -1` and its gate, and `removeBand`'s entry proof is
preceded only by pure reads; the one residual sub-window is `removeBand`'s later `setSoloMask`, which
dispatches before its own sound clause but sits *after* the transaction's ownership proof and behind
its count and mask checks. Width drag: inert.

### 64d. The fix, and the two it rejects

A scoped flag: `mouseUp` owns the record for the duration of its release action; `cancelActiveDrag`
declines outright while it is set. Two production lines and an RAII helper.

**Rejected — move the clear after the cheap exit.** It is deliberately in front (ADR-0039):
`mouseDown` latches `gestureBands` on branches that latch no identifier at all, and nothing else ever
clears those.

**Rejected — copy the handle-drag compare to the other branches.** Three more sites of the same
ad-hoc guard, and it still leaves `removeBand`'s mid-transaction per-store proofs disarmed. Making
the record survive fixes every consumer in one place.

### 64e. Evidence

| Mutation | Killed |
|---|---|
| `cancelActiveDrag` no longer declines during a release action | leg E |
| `mouseUp` no longer claims the record | leg E |
| the handle-drag branch's bespoke `gestureBands == pressBands` compare removed | **nothing** |

The third is the honest result: that compare was always labelled unmeasured and is now redundant
defence in depth. Kept — one integer compare per release — and **not** claimed as load-bearing.

### 64f. A route considered and left open, on purpose

A nested `mouseUp` would find every identifier cleared, skip all four branches, and reach the tail,
which drops the record; the flag does not stop that, because the nested frame's own scope guard is a
plain set/clear rather than a re-entrancy counter. No evidence in this repository says a host does
this, no test reaches it, and closing it would mean restructuring the exit ordering this ADR exists to
fix. **Recorded as open and unmeasured rather than fixed on a guess.**

### 64g. Gate: NOT triggered

No DSP graph, signal flow, parameter registry, serialization, latency or plugin format. **Thread
model** — one `bool` written and read on the message thread only; no thread, lock, atomic or
cross-thread path is added, and the change removes a state transition. **Build system** — untouched.
This applies ADR-0050's Decision to the one function able to defeat it. **No human approval is
required, and none is manufactured.**

### 64h. Residuals — re-verified only where this change can reach them

| Residual | Disposition |
|---|---|
| RISK-010 (audio-side reader) | **Unchanged** — this change touches `SpectrumImager.{h,cpp}` and the tests only; `PluginParameters.cpp` is not in this PR's diff at all |
| `addBandAt` re-attribution window | **Unchanged** — reached from `mouseDown`; the flag is set only in `mouseUp` |
| held-audition vblank coverage | **Affected, and intentionally so** — `tick`'s `if (gestureIsStale()) cancelActiveDrag();` is now suppressed *while a release action runs*. That is the fix, not a side effect: the release action clears every identifier it owns and drops the record itself one line later. The gap the residual names — `tick` returning at `isShowing()` before the guard — is untouched |
| wheel gesture closure (ADR-0041/0052) | **Unchanged** — `mouseWheelMove`'s delta test and `cancelActiveDrag()` are outside `mouseUp`, so the flag is false there; verified still first and first |
| U4 wheel width undo | **Unchanged** |
| ADR-0044 partial-transaction residue | **Unchanged** |
| cancelled-spread visual ordering | **Unchanged** |
| TSan suppression scope | **Unchanged** — still exactly one entry. Leg E adds no new lock-order shape: it writes `midP` from inside `soloP`'s gesture dispatch exactly as leg C already does, and the nested `cancelActiveDrag` takes the cheap exit and no parameter lock |
| historical comment blocks | **One correction, caused by this fix** — see §64i |

### 64i. A comment this fix made false, corrected in the same commit

The handle-drag branch's `gestureBands == pressBands` compare carried a comment saying that *without*
it "the line below would remove a band under a gesture something else has just cancelled". That was
true when written and is **no longer true**: the record now survives the nested cancel, and the
measurement says so — removing the compare fails nothing, while reverting either half of the fix
fails leg E.

The comment is rewritten to record the **change of status** rather than deleted: the compare is kept
as redundant defence in depth, still unmeasured, and explicitly not presented as the thing holding
the line. Fixing a defect can make a neighbouring comment false, and that is exactly the class of
error this file has had to correct twice in this series — so it is corrected in the same commit that
causes it, not left for a later reader.

### 64j. `macos-intel` red on `7c31452` — pluginval's own teardown, and it is NOT this PR's

The documentation-only commit `7c31452` failed `macos-intel` step 15, *"pluginval AU (randomise x3),
native Intel"*. Diagnosed rather than assumed, and it is **not this PR's** by three independent
arguments:

**1. The diff cannot cause it.** `8a520fd` → `7c31452` is **three Markdown files** — no source, no
build file, no workflow. The binaries are byte-identical, and the same job at the same step passed on
`8a520fd` minutes earlier. As a check on that reasoning rather than an assumption, the two runs'
self-test counts were compared: both report **394** DSP and **2 861** state checks on this job, so the
red run built the same tree the green one did (those figures differ from the Linux tree's 396 / 2 896
because this job's platform gating differs — established from the green log, not supposed).

**2. Every test passed.** The log ends `Completed tests in pluginval / Audio processing` → `SUCCESS`,
and only *then*:

```
libc++abi: terminating due to uncaught exception of type std::__1::bad_function_call
pluginval received Abort trap: 6, exiting immediately
pluginval: CRASHED (au randomise pass 3/3, ... exit 9)
```

The crash is in **pluginval's own teardown**, after the plug-in has already been validated.

**3. This exact signature is the case this repository's pluginval work exists for.** It is quoted
verbatim in `scripts/run-pluginval.sh` (both the header reasoning and the classifier's self-test
fixture) and in `CI_CD.md`, `TESTING.md` and this register — first observed on **PR #141, run
34019453055, job `macos`, AU randomise pass 2/3**, with the same "SUCCESS first, then the teardown
crash" shape. The PV round's fix was to stop calling it *"a plug-in that failed validation"* and start
calling it `CRASHED`; it deliberately did **not** move the gate, and deliberately does **not** retry on
macOS (`the retry exists for the Linux X11/XEmbed flake only`).

**Disposition.** Established as not this PR's, so the one re-run this category allows is spent on it
rather than a code change. Nothing here is skipped, disabled or quarantined, and no empty commit is
pushed. If it reproduces identically on the re-run, that is new evidence about the *harness* — not
about this change — and belongs to the pluginval crash record, not to ADR-0050.

**Outcome of the re-run.** `rerun-failed-jobs` was refused with **HTTP 403 `This workflow is already
running`** while the sibling `macos` job was still in flight, so the re-run was delivered the only way
left that changes no code: pushing this diagnosis commit, `4ccedba`, which supersedes the `7c31452`
run and re-runs every job on the new head. Push run **`34495243892` on `4ccedba` is 13 / 13 success**
(`merge-check` skipped by design), **`macos-intel` included** — job `102932477492`, step 15 *"pluginval
AU (randomise x3), native Intel"* **passed**. The tree under it is byte-identical in every source and
build file to the one that went red, which settles the classification: the `std::bad_function_call`
abort is **non-deterministic, in pluginval's own shutdown**, and is not a property of this change. The
one re-run this category allows is now spent, and it came back green; a *third* occurrence would be
new evidence about the harness and belongs to the pluginval crash record above, not to ADR-0050.

---

## §65. Round 8 — the band-move startup, interrupted between its two gesture opens

### 65a. Workflow audit and lifecycle decision

| Artefact | State found | Decision |
|---|---|---|
| Round-7 adversarial Workflow (17 agents) | Completed, result consumed in `7c31452` | **Not restarted.** Its subject was `cancelActiveDrag`'s first two lines; this round's is a different window, and nothing it produced bears on the startup path |
| Monitor on run `34495243892` (`4ccedba`) | Stream ended, 13/13 read | Closed |
| Monitor on run `34497783641` (`afb128f`) | Stream ended, 13/13 read | Closed |
| Tasks #1–#118 | All `completed` | No unconsumed results |
| Scheduled check-ins / PR-activity subscriptions | None | None created — `CLAUDE.md` forbids both |
| Round-8 Workflow (7 dimensions × 3 skeptics + 5 option evaluators) | Launched this round | Consumed; corrections applied below |

### 65b. The finding: CONFIRMED, reproduced, and its wording corrected

`beginBandMove` is the **only** function in `SpectrumImager` that brackets more than one parameter.
An audit of every `beginChangeGesture` call site in the file found two in that function
(`SpectrumImager.cpp` — the two `beginGesture` statements that close it) and exactly one in every
other: `mouseDown`'s three press branches, `resetParam`, `resetCrossover`, `commitFreqEditor`,
`setBands` and `setSoloMask`. A single-open bracket has no interior for a re-entry to land in. This
one does, and `mouseDrag` publishes `soloMovedBand = true` before calling in.

**The review's wording is right, and a correction of mine is retracted below (§65b-ii).** Both
directions are real, on different pins: the **second** pin is closed having never been opened (an
`endChangeGesture` with no matching `beginChangeGesture`, gesture count −1 for it, and the outer
frame then skips its own open because the members are already cleared), and the **first** pin *does*
remain open, permanently. Reachability, ordering and `soloPressBand == -1` in the resumed handler
were exact throughout.

**Three consequences, measured rather than argued** (State test 83 leg E, on the pre-fix tree — one
sequence, three observables):

```
[leg E] the second pin was closed 1 time(s) having been opened 0
[leg E] the resumed handler auditioned mask 0x80000000
[leg E] the host's split at 6500.0 Hz was written back to 2000.0 Hz, with -1 gesture(s) open
[leg E] a properly bracketed edit after the interrupted startup recorded NO undo step
        -- the gesture count never came back down
```

The second is `1 << soloPressBand` with `soloPressBand == -1` — undefined behaviour, and
`AnamorphAudioProcessor::setSoloPreview` masks it with `& 0x0F`, so what reaches the engine is a
band set the press never named. The third is the ADR-0040 / ADR-0047 failure exactly, reached not by
a race but by the record being dropped mid-startup: with `gestureBands == -1` every ownership
predicate self-disables and `writeCrossovers` skips its count proof, so the burst writes the
pre-press origins back over the host's install — outside any change gesture, both having just been
closed.

All four were the **only** failures in the suite, so the reproduction is isolated.

### 65b-ii. The fourth consequence, and a correction of my own retracted

The fourth is the worst and the first draft of this section missed it entirely — it was surfaced by
the adversarial pass and then **measured** rather than accepted. JUCE walks `listeners` in
**reverse** — `for (int i = listeners.size(); --i >= 0;)`, `juce_AudioProcessorParameter.cpp` — and
the processor registers itself at construction (`PluginProcessor.cpp`, `p->addListener (this)`), so
a host listener added later is notified **first**. The whole nested cancellation therefore runs, and
both of its `endChangeGesture`s reach `AnamorphAudioProcessor::parameterGestureChanged` while
`openGestures` is still 0 — where its `else if (openGestures > 0 …)` makes them no-ops — **before**
the outer `beginChangeGesture` reaches the processor and takes the count to 1. Nothing can bring it
down: every identifier is already cleared, so no later `endBandMove`, `mouseUp` or
`cancelActiveDrag` closes that pin. `pollUndoCoalesce` refuses to commit while `openGestures > 0`,
so **undo silently stops recording every subsequent sound edit** until an A/B switch, preset load,
undo or redo zeroes the count. Measured: a properly bracketed Width edit after one interrupted
startup records no undo step at all.

**So the review's *"the gesture remains open"* was right, and my correction of it is retracted.** I
had argued the leak could only be an unmatched close, because `endBandMove` closes both pins. That
is true of the **second** pin and false of the **first**, and the first is the one with the lasting
consequence. The claim reached the ADR, the CHANGELOG and the pull request before it was measured,
which is why the retraction is recorded here rather than quietly dropped. The fix was already
correct — the assertion passes on the fixed tree without any code change — but the account of what
it prevents was incomplete and, on this point, wrong.

### 65b-i. The reconcile that is live here — a claim of my own, corrected

The first draft of this section and of the in-source comment said the re-entry arrives from *"`tick`'s
reconcile, or the editor's stuck-drag one"*. **The editor's is inert here**, and the reason is a fix
this repository already shipped: during a DRAG the button is genuinely down, and round 4 gave
`anyPhysicalMouseButtonDown()` the OS's real button state (KI-013, resolved), so
`isMouseButtonDownAnywhere() && ! anyPhysicalMouseButtonDown()` is false throughout. On the RELEASE
side, where the button is up, it is the live one — the ranking is inverted between the two windows,
and round 7's record has it the other way round for its own window, correctly.

**`tick`'s gate is also false on entry**, which makes this a precondition rather than a free-standing
re-entry: `mouseDrag`'s first statement has just proved the record, and everything between that proof
and the dispatch is a pure computation. The only sequence production can produce is *host writes a
parameter from inside the gesture open → the gate becomes true → the loop it pumps runs the
reconcile*. Leg E performs exactly that order; an earlier draft of the fixture had the write and the
cancel the wrong way round, which modelled a call production would not make. The direct
`cancelActiveDrag()` in the leg stands in for `tick`, which a headless fixture cannot drive — it
returns at `isShowing()` before the reconcile, the standing residual already recorded for the
held-audition guard — so the leg exercises the reconcile's **body** with its gate made true one line
above. Re-measured after the correction: identical results, all three.

### 65c. Which band, and why leg C could not have found this

`soloMoveLeft = (b > 0) ? b - 1 : -1` and `soloMoveRight = (b < N - 1) ? b : -1`. Both pins are live
only for a **middle** band, `0 < b < N - 1`, which needs `N >= 3`. State test 83 leg C — the existing
coverage — presses the **last** band deliberately (its comment says so: *"so mbFreqHigh carries the
whole count"*), where `soloMoveRight == -1` and only one `beginGesture` ever runs. The window is
structurally invisible from there. Leg E presses band 1 at four bands, found by walking the solo
lane and taking the second contiguous run of the "Solo this band" tooltip rather than by hardcoded
geometry, so the leg does not depend on the split values the fixture happens to install.

### 65d. The fix, and the three it rejects

`beginBandMove` takes the same ownership claim `mouseUp` has taken since round 7, for the duration of
its startup. Two lines: a `ScopedGestureAction` at the top of the function, and the guard it feeds in
`cancelActiveDrag` (already present, now reading a depth).

* **B — publish `soloMovedBand` only after both opens.** Turns an unmatched close into an unmatched
  **open**: the nested cancel would see `soloMovedBand == false`, close nothing, and leave both pins
  open forever with the identifiers gone. Consequence (3) survives untouched.
* **C — track which pins actually opened, close only those.** Fixes consequence (1) alone. (2) and
  (3) survive, because the members are still cleared under the outer frame.
* **D — open from locals, assign the members afterwards.** Same failure as B: the nested cancel finds
  `soloMoveLeft/Right` still `-1`. Ordering alone cannot fix this. The record has to survive.

Nothing here needs the cancellation to happen *now*: `moveBand`, one statement later, re-proves the
count and every split it writes, and `tick`'s next reconcile still fires. Declining costs a few
instructions of latency.

### 65e. A depth, not a flag — and it is unmeasured

Round 7 shipped `bool releaseActionActive` with the note that a nested `mouseUp` would clear it
early. Adding a **second** user makes that hazard newly reachable: `beginBandMove` is called from
`mouseDrag`, so a host pumping the loop from the first pin's open can deliver a queued mouse-up into
`mouseUp` while the startup's claim stands, and a `bool` would have had the inner scope's exit clear
the outer one's. `int gestureActionDepth` counts instead.

**Degrading the counter back to a set/clear flag kills nothing** — no test nests the two sites. It is
kept because this site creates the hazard and the counter costs the same instruction, and it is
labelled unmeasured at the declaration rather than claimed. The nested-`mouseUp` route recorded in
§64f **stays open**: the counter stops the inner scope clearing the outer claim, not a nested
`mouseUp` running its tail and dropping the record there.

### 65e-ii. The adversarial pass — what it settled, and a process error of mine

A 33-agent workflow ran seven independent readings of the path, three skeptics per reading, and five
option evaluators. What it settled:

* **The mechanism and the uniqueness claim held** across all seven readings: `beginBandMove` is the
  only site in the file that opens more than one gesture, and the interior is reachable.
* **It found the fourth consequence** (§65b-ii), which I had missed, and which I then measured rather
  than accepted. One skeptic named my error exactly: my "correction" of the review was *"an artefact
  of a per-parameter call-counter probe that is blind to the fact that the interrupted begin's
  dispatch is a LISTENER LOOP"*. That is right, and the retraction is §65b-ii.
* **It narrowed reachability the same way I had**: a reading that claimed several live re-entry
  vectors was refuted for *"a re-entry vector that the code proves inert, an omitted precondition
  that turns 'reachable' into a much narrower conjunction"* — the editor's reconcile and the missing
  staleness precondition, i.e. §65b-i independently.
* **The option evaluators, measuring rather than arguing:** A **rejected** — one of its two readings
  is *"measurably WORSE than the unfixed code"*; B **rejected** — it *"converts a transient unmatched
  CLOSE into two permanently leaked OPEN change gestures"*; C **rejected** — it *"closes one of four
  harms and replaces it with a strictly worse, ordering-independent one"*; D **viable, closes all
  four, four lines** — and *"it converges on the shipped fix and is a distinct option in name only"*.
  A sixth option the evaluators invented themselves — make the pin members mean "gesture OPEN"
  rather than "gesture WANTED" — is viable but does not close all four, and they judged it *"a policy
  change (abandon, not decline)"* that *"collides with an ADR section the repo already Accepted"*.
  Recorded rather than dropped.

**The process error is mine.** I launched the workflow and then implemented and committed the fix
while it ran, so the tree moved three times under the agents. Almost every skeptic spent its pass
refuting the *tree* rather than the finding — *"the reading refutes the finding using, as its
evidence, the commit that FIXES the finding"* — and one reading even declared the finding refuted
because it had read my own fix. The pass still produced its most valuable result, but most of its
adversarial budget was spent on an artefact I created. **The rule this round adds: pin the audited
revision for the agents (a `git show <sha>:path` extract, which one skeptic did unprompted) or do not
touch the tree until the pass returns.**

### 65f. Evidence

| Mutation | Killed |
|---|---|
| `beginBandMove` no longer claims the record | leg E's **four** checks, and nothing else |
| `cancelActiveDrag` no longer declines | those four **and** State test 79 leg E — **5 checks** |
| `mouseUp` no longer claims the record | State test 79 leg E only — **1 check** |
| the depth degraded to a set/clear flag | **nothing** — §65e |

The first three are the orthogonality proof: each claiming site is measured on its own, neither
subsumes the other, and the shared decline is measured by both. Leg F is the positive control.

No probe is shipped. The window is reentrant and deterministic, so a stress probe would be a gate
that cannot fail — the rule §62j's withdrawn wheel probe exists to enforce.

### 65g. Gate: NOT triggered

No DSP graph, signal flow, parameter registry, serialization, latency or plugin format. **Thread
model** — one message-thread `int` replacing one message-thread `bool`; no thread, lock, atomic or
cross-thread path is added. **Build system** — untouched. This applies ADR-0050's Decision to the one
startup able to defeat it, and adds no ADR. **No human approval is required, and none is
manufactured.**

### 65h. Residuals — re-verified only where this change can reach them

| Residual | Disposition |
|---|---|
| RISK-010 (audio-side reader) | **Unchanged** — `PluginParameters.cpp` is not in this PR's diff at all |
| `addBandAt` re-attribution window | **Unchanged** — reached from `mouseDown`; the claim is taken in `mouseUp` and `beginBandMove` |
| held-audition vblank coverage | **Affected, and intentionally so** — `tick`'s `if (gestureIsStale()) cancelActiveDrag();` is now also suppressed *while a band move is starting up*, two statements' worth. `moveBand` re-proves immediately after, and the next tick still fires. The gap the residual names — `tick` returning at `isShowing()` before the guard — is untouched |
| wheel gesture closure (ADR-0041/0052) | **Unchanged** — `mouseWheelMove` calls `cancelActiveDrag()` outside both claiming scopes; verified still first and first |
| U4 wheel width undo | **Unchanged** |
| ADR-0044 partial-transaction residue | **Unchanged** |
| cancelled-spread visual ordering | **Unchanged** |
| TSan suppression scope | **Unchanged** — still exactly one entry, `Matched 1 suppressions` with one breakdown line. Legs E and F add no new lock-order shape: they write nothing from inside a gesture open that State test 79 leg E did not already |
| historical comment blocks | **One rename propagated** — `releaseActionActive`/`ScopedReleaseAction` are now `gestureActionDepth`/`ScopedGestureAction`, and the declaration comment says why the type changed. No claim in the round-7 text became false |

---

## §66. Round 9 — the mouse wheel, and what an interaction is

**The brief (maintainer, 2026-09-12).** Change the mouse-drag, mouse-wheel and Undo/Redo behaviour
for every knob and slider, the numeric value below each knob, the Multiband split, the Multiband
bandwidth, the Band Solo interaction and the Settings slider. Wheel input during a drag must ADD to
the drag rather than cancel it, the drag must continue from the combined value, and the whole
interaction must be one undo step. A standalone scroll must be one undo step that a later scroll of
the same control EXTENDS, preserving the value the first scroll started from; another editing method
must start a new step. Holding a Band Solo button and scrolling must move the band, not its width.
The Settings slider must gain the interaction and keep its exclusion from Undo. And, explicitly:
*"if previous behavior in the code or existing documentation conflicts with this request, the
behavior specified in this task is the latest behavior and takes precedence."*

### 66a. What was actually there — three behaviours, none of them the one asked for

| While the mouse is held | Before |
|---|---|
| knob / slider / value box | **nothing.** `juce::Slider::Pimpl::mouseWheelMove` is wrapped in `! e.mods.isAnyMouseButtonDown()`; the event is consumed and discarded |
| Multiband split or width drag | **the press ended** (ADR-0041): gesture closed at the notch, the drag so far committed as its own undo step, further movement dead |
| a held Band Solo button | the press ended, the click was swallowed, and the wheel edited whatever band the pointer was over |

| With no button held | Before |
|---|---|
| knob / slider / value box | **one undo step per notch** — JUCE wraps each in its own `ScopedDragNotification` |
| Multiband split or width | **no undo step at all** — every store was a bare `setValueNotifyingHost` outside any gesture (KI-010's second path) |

### 66b. The one thing that makes this hard, and it is ADR-0041's own measurement

Not one of these drags reads the live parameter again after it starts. A split drag stores
`cursor.x - dragGrabDX`; a width drag stores `yToWidth (cursor.y - dragGrabDY)`; a band move stores
two pins at `bandStart{Left,Right}X + clamp (cursor.x - bandAnchorX)`; the value box stores
`downProp + (-dragY) / 180`; and `juce::Slider::handleAbsoluteDrag` recomputes
`valueOnMouseDown + mouseDiff / 250` on **every** mouse move. So a notch that writes the value and
stops is erased by the next mouse event — which is exactly what ADR-0041 measured
(*"a wheel tick adopted the installed width 1.700, and the drag then wrote 0.650 from an anchor taken
before it"*) and answered, with the tools it had, by ending the press.

The answer this round takes is to move the **anchor**, not to refresh anything:

| Interaction | The single variable | Set to |
|---|---|---|
| split press | `dragGrabDX` | `cursor.x - clamp (target)` |
| width press | `dragGrabDY` | `cursor.y - widthToY (target)` — the exact form the 3 px engage uses |
| band move | `bandAnchorX` | `cursor.x - clamp (T + notch)` |
| value box | `downProp` | `+=` the proportion that actually fitted |
| `juce::Slider` | `Knob::wheelDragProp` | `+=` the proportion that fitted, re-applied after every `Slider::mouseDrag` |

ADR-0041's DECISION is therefore untouched and relied on: these branches perform no refresh, so its
rule has nothing to refuse. Only its Consequences line is superseded, and ADR-0053 says so in its
header, with a reciprocal note in ADR-0041 and in ADR-0052.

**`juce::Slider`'s drag baseline is unreachable** — `valueOnMouseDown`, `mouseDragStartPos`,
`valueWhenLastDragged` and `lastAngle` are private `Pimpl` members with no public setter, and
`getThumbBeingDragged()` is the only part of that state a subclass can read. The recon pass concluded
from that that no public API can re-anchor a drag; the conclusion is wrong and the premise is right.
The question is not *"can I write `valueOnMouseDown`?"* but *"can I correct the value after JUCE has
computed it?"*, and the answer is yes, every drag event, for the price of one `double`.

### 66c. Why there is no inactivity timer, which is what the brief describes

An undo entry holds the state from BEFORE its step. So *"preserve the value the scroll started from
and replace only the ending value"* is precisely *do not push another entry, and move the committed
baseline on*. The step therefore exists from the FIRST notch and is extended by every notch after it,
which makes *"one Undo returns the control to the value it had before the scroll"* true at **every
instant** rather than only after a dwell — a strictly stronger guarantee than the mechanism the brief
describes, obtained with no timer, no poll and nothing held open.

And the alternative is the worst option available to this code base. A single gesture held open
across the scroll and closed on a timeout is the shape of the defect measured in this very release:
`pollUndoCoalesceAdopted` records nothing while `openGestures > 0`, so a gesture that fails to close
stops undo recording SILENTLY (ADR-0050, §65). Recorded here as a deviation in MECHANISM, not in
behaviour, so a later reader can see it was a decision rather than an omission.

### 66d. Two defects this round's own adversarial pass found, before any review did

The round ran an eight-reader read-only reconnaissance with three critics over the result. Most of
the map was stale by the time it returned — see §66f — but two of the critics' findings were real,
against code that had already landed, and both are fixed here with their own regression legs.

1. **A notch with no band to move still swallowed the solo click.** At one band `beginBandMove`
   leaves both pins at `-1` and `moveBand` returns at `M <= 0` having written nothing and opened
   nothing — so the notch performs NO EDIT — yet the press was converted into a "move" anyway, and
   the release then took the move's branch instead of the toggle's. That is ADR-0052's own rule
   broken in a branch written three hours earlier. `if (gestureBands < 2) return;` and State test 87
   leg E.
2. **Two gestures finishing inside one poll period took the LAST gesture's name.** The poll runs on
   the editor's 24 Hz tick, so a drag released and a notch taken within the same ~42 ms have always
   collapsed into one step; what was new was the name on it. Taking the notch's name made that step
   EXTEND the scroll the notch belonged to, folding the drag into it and leaving the drag with no
   undo point of its own. A disagreeing name inside an already-pending batch now clears the name.
   State test 86 leg G is the one leg in the suite that deliberately does not poll between two edits.

### 66e. What is stated rather than implied

* **ONE gesture, up to three parameters, on the split path.** `dragCrossoverTo` pushes neighbouring
  splits aside and those stores are outside any bracket of their own, so a host recording touch/latch
  sees them as automation. Undo is unaffected (`openGestures` is one global count) and this is
  exactly what the DRAG path has done since 0.6.x. Matching it is deliberate.
* **A typed value force-committed by a notch is attributed to the scroll.** JUCE's wheel handler
  calls `valueBox->hideEditor (false)` before its own gesture and that commit opens a gesture inside
  the naming scope. Both belong to one user action.
* **A notch delivered to a component other than the one being dragged still edits that component.**
  JUCE routes by POINTER, not by capture. Unchanged, and left unchanged deliberately: a mouse-button
  gate on the multiband display would also silence a notch during one of its own presses that latched
  no identifier (an Alt-click reset, an add the count refused). Measured as a real consequence of
  this: with a knob drag open, such a notch's multiband edit lands in the knob's undo step.
* **`monoFreqK` and the Settings bar snap to the cursor** (`LinearHorizontal`, `snapsToMousePos`
  default `true`), so for those two a wheel offset means the thumb deliberately stops sitting exactly
  under the pointer. That is what "additive" means for a snap-to-cursor control; State test 88 leg C
  drives that path.
* **The velocity-drag path (Ctrl/Alt/Cmd held) is NOT measured.** `applyWheelDragOffset` runs after
  `Slider::mouseDrag` whatever branch it took, so it composes with `handleVelocityDrag` by
  construction — but nothing in the suite holds a modifier, and that is said here rather than left to
  be assumed.
* **`monoFreqK`'s and the Settings bar's value boxes are not draggable**, and were not before this
  round: `ValueBox::mouseDown` requires a ROTARY parent. Their wheel still reaches the slider. Not a
  regression and not fixed here.

### 66g. Mutation — sixteen applied one at a time, fifteen killed

The full table is in `docs/procedures/TESTING.md`. Three entries are worth repeating here.

**M5 SURVIVED, and finding out why is the most useful thing this round's mutation pass did.** The
split leg's deciding check compared the final split frequency against the frequency the press
started from, and a split drag cannot reproduce that bit-for-bit through `freqToX`/`xToFreq` — so
`! exactlyEqual` passed whether or not the notch had survived. The leg was green, it looked like
coverage, and deleting the line it exists to protect changed nothing. Rewritten to take its baseline
with a drag event at the SAME cursor position the deciding check uses, it kills M5 with the measured
message `the very next drag event at the same cursor position put the split back to 199.9998 Hz`.
That figure is also the proof the first version could not work: `199.9998` is not `200.0`.

**M8 reaches outside this round's own legs**, which is the useful half of it: extending the undo step
on every commit rather than only on a matching NAME breaks five preset-identity and undo/redo checks
that predate this change entirely. The name is load-bearing for behaviour this round did not write.

**M15 survives and is kept.** The `ownsWidth` proof it removes sits behind the press branches'
`gestureIsStale()` gate, two lines above, which compares every split and width exactly — so a
persistent foreign write is caught before the proof is reached, and what the proof covers is a write
landing between the gate and the store, with no dispatch in that stretch for a single-threaded
harness to enter. Same class as the surviving mutations recorded for ADR-0046, ADR-0047, ADR-0048 and
ADR-0051, kept for the same reason and said to be unmeasured rather than presented as covered.

### 66f. A process note, and a correction to §65e-ii's rule

§65 recorded the rule *"pin the audited revision, or do not touch the tree until the pass returns"*
after a round in which agents spent their passes refuting a moving tree. This round obeyed it for as
long as the pass was auditing, and then broke it deliberately: the recon was still running four
agents deep when implementation started, on a 4-core box where the remaining agents would have cost
another half hour. The cost was exactly what §65 predicts — four of the eight readers describe code
that no longer existed, and one critic opened by saying so.

What was NOT lost is the part that mattered, and that is the correction: the three critics read the
DIGEST, not the tree, and both of the real findings above came from them. A reconnaissance pass ages
badly against a tree that moves; an adversarial pass over a written claim does not. The rule is
therefore narrower than §65 stated: **pin the tree for a pass that must cite it, and let a pass that
argues from a written claim run beside the work.**

## §67. Round 10 — the review of round 9: three corrections to the wheel

A review of `6d12c2c..e38ab37` returned three defects and one investigate item. Each was reproduced
as a failing check against the tree before any of it was touched, and the numbers below are the ones
the failing runs printed.

### 67a. The pointed control was silent where the multiband display was not

`Slider::Pimpl::mouseWheelMove` wraps its entire body in `! e.mods.isAnyMouseButtonDown()`. Round 9's
in-press branch claims a notch only when the slider it lands on owns the drag
(`getThumbBeingDragged() >= 0`), so a notch delivered to a knob while ANOTHER control held the press
fell through to JUCE and was discarded. That is not an edge case: JUCE routes wheel events by
pointer, not by capture — `getTargetForGesture` hit-tests the peer at the event position whether or
not a drag is in flight — and a rotary drag travels up to 250 px, which leaves the knob. The same
gesture over the multiband display edited it, because that class answers its own wheel events.

Measured: *"the notch was dropped: Width stayed at 1.0000 while another control held the press"*
(State test 88 leg F), and the Settings bar the same (leg G).

The fix hands JUCE the same event with `mods.withoutMouseButtons()` and every other field copied
verbatim. Restating JUCE's wheel arithmetic locally was rejected: the delegation keeps its amount,
its `jmax(interval, |delta|)` floor, `snapValue`, the duplicate-event filter on `e.eventTime`, the
value-box editor hide and the `ScopedDragNotification` that opens the host gesture — five behaviours
that would each have to be re-derived and kept in step. It is gated on exactly what JUCE needs to
act (`isEnabled() && isScrollWheelEnabled()`), so a disabled slider still forwards the UNTOUCHED
event to its ancestors rather than a synthetic one.

**And the fix's own first version was wrong for the value box.** Worth recording because the review
did not ask for it and the round's own re-read of the diff found it: the box drags by steering
`downProp`, and it maps 180 px of travel across a box under 20 px tall, so the cursor leaves the box
within a few pixels and JUCE delivers the rest of that drag's notches to the KNOB. Letting the knob
write the value there turned "nothing happens" into something worse — the value jumped and the box's
next drag event recomputed from `downProp` and erased it (*"the box's next drag event erased it:
back to 2.6700"*, State test 88 leg I). A notch must reach the anchor the press is steering, so the
knob asks any child holding a drag gesture to take it first, through `DragGestureOwner` — the same
named interface the editor's release-outside reconcile already uses to reach a control that lives in
an anonymous namespace. The child never forwards the event, so the ask cannot recurse, and the loop
is behind `isAnyMouseButtonDown()` so an ordinary scroll pays nothing.

### 67b. An empty press split a scroll in two

Gesture closes are batched until the 24 Hz poll, so a click that opens and closes a gesture without
moving a value can share a batch with the next notch. Round 9's mixed-batch guard then read that
nameless close as a disagreement and cleared the batch's name, and the notch after it started a
second undo step. Measured: *"one Undo stopped at 1.8000: the empty click between the two scrolls
made the second one its own step"* (State test 86 leg L).

The poll already held the principle — it writes `lastStepWheelKey` only where it records a step,
because a gesture that changed nothing has not interrupted the scroll — so the close now applies the
same test rather than a new one. The cheap instrument was already there: `soundParamGen`, the counter
the S10 poll skip maintains, is bumped by every parameter value change. One relaxed load when the
batch's first gesture opens, one when it closes. No signature rebuild, no timer, no allocation.

### 67c. A refused burst attributed the host's write to the wheel

The standalone multiband branches must open their gesture before their store, and the open
dispatches. A host that answers it by writing the same parameter makes the store refuse (ADR-0047) —
but the gesture still closes, the poll still sees a moved signature, and a NAMED close extended the
previous scroll's step with a value the scroll never produced. Measured: *"one Undo stopped at
1.0000, not at the 1.1200 the previous scroll ended on: the refused burst extended it"* (State test
86 leg I). Both branches now name the step only when their own store committed.

**The correction that came out of measuring it.** Fixing 67b turned out to fix leg I as well, and it
would have been easy to file 67c as redundant. Instrumenting the generation counter said why, and
said what is left: `juce::ListenerList` calls listeners in REVERSE order of registration, and this
processor registers in its constructor, so a host write made from a parameter listener during the
gesture-open is delivered BEFORE the coalescer samples the generation — open `gen=63`, close
`gen=63`, no name, whatever the store did. What the store result covers is the other ordering: a
change arriving AFTER the sample. Cross-thread, that is ADR-0047's own case and unreachable here;
single-threaded, it is reachable on the split branch, because `writeCrossovers` proves EVERY split in
the row and not only the ones it moves, so a probe writing the next split from inside the first
split's store aborts the transaction after part of it has landed. That is State test 86 leg M, and it
kills the split half (mutation M19). The width branch has one store and nothing dispatching before
it: M18 survives, recorded as unmeasured defence for the cross-thread ordering, the same class and
the same disposition as M15.

**What was NOT fixed, and is written down instead.** A burst that wrote nothing of its own still
leaves an undo step for whatever the host wrote inside its gesture. Unnaming it stops the
misattribution — the previous scroll keeps the value it ended on — but the gesture did close over a
changed signature. That is the generic property of gesture coalescing, not something ADR-0053
introduced, and closing it would need a gesture able to withdraw its own commit request.

### 67d. The investigate item: the velocity drag branch

Round 9's own part 12 listed the Ctrl/Alt/Cmd velocity drag as composing with `applyWheelDragOffset`
*by construction* and untested. State test 88 leg H now measures it, and opens with a positive
control — the same 20 px with no modifier moves the knob by a different amount — so the leg cannot
quietly degrade into a second copy of leg A if the modifier ever stopped selecting the branch.
Mutation M23 (`applyWheelDragOffset` returns unconditionally) kills legs A and H together.

**And writing it found something in JUCE, which is why the leg does not use a knob.** The first
version held Ctrl over Drive and turned the `sanitizers` job red:
`juce_Slider.cpp:929:86: runtime error: division by zero`. `Slider::Pimpl::resized` assigns
`sliderRegionSize` only for horizontal and vertical styles, and the slider's own constructor takes
that branch once with JUCE's DEFAULT `LinearHorizontal` style against empty bounds -- so every
rotary slider carries `sliderRegionSize == 0` for life. Measured from outside rather than inferred:
`getPositionOfValue(max) - getPositionOfValue(min)` is `pos * sliderRegionSize`, and it reads
**0.000 for Drive against 246.000 for the mono-maker slider**. Velocity mode is chosen by
`(normRange.end - normRange.start) / sliderRegionSize < normRange.interval`, so a velocity drag of a
knob divides by zero there; `+inf` is not less than the interval, the velocity branch is taken
exactly as intended, and no behaviour is wrong -- but the division is real and the gate is right to
report it.

Three ways out were available and two were refused. Adding `float-divide-by-zero: src:*juce-src/*`
to `scripts/ubsan-ignorelist.txt` is refused by that file's own text, which says the scope is the
point and that `float-divide-by-zero` still instruments the vendored tree in full -- silencing a
whole sub-check across all of JUCE to admit one test is precisely the trade it exists to refuse.
Guarding the leg out under sanitizers is a test that cannot fail where it matters. What shipped
drives the same JUCE branch through a slider whose region size is real, and records the knob
finding here and in `TESTING.md` so the next person to try it does not rediscover it as a red CI
run. **Verified in both directions** on a local clang-18 `-fsanitize=undefined,float-divide-by-zero`
build with the CI ignorelist: the rotary drag reproduces the report at the same line, and the leg as
it stands runs the whole suite clean.

### 67e. Validation

State 3 057 / 0, DSP 396 / 0. Nine new mutations: M17, M19-M25 killed, M18 surviving as above.
UBSan (local clang-18, `undefined,float-divide-by-zero`, the CI ignorelist): the state suite clean.
TSan over the whole suite: 0 warnings, one matched suppression
(`deadlock:WriteFromInsideAGestureOpen`). valgrind memcheck: 0 errors from 0 contexts on both suites
under `ANAMORPH_TESTS_NO_FTZ=1`. Docs 141 clean, citations 450 anchors clean against `origin/main`,
the merge base and `HEAD~1`, realtime 47 / 0, portability 57 / 0.


## §68. Round 11 — the review of round 10: five defects, one refusal, two found on the way

Six findings arrived. Five are real and fixed; one -- `ScopedWheelStep`'s destructor -- is refuted
with its reasoning recorded in the source. Three more were found while reading for them (one of
them created by this round's own first fix), and two documentation defects were found: one in the
changelog and one in a comment the previous round made false. Every defect below was reproduced as a
FAILING check before anything was touched, and every fix has a mutation that brings the failure
back.

### 68a. The crux of finding 1 was a question about JUCE, and the answer went the review's way

The claim was that a small in-drag notch can be erased by snapping while JUCE's own path floors the
movement to one interval. The premise is only true if the slider's `normRange.interval` is
non-zero, and an attachment-driven slider looked like the case where it would be zero. It is not:
`juce_ParameterAttachments.cpp` copies the parameter's interval onto the slider
(`newRange.interval = range.interval`), and this plug-in declares a real interval on nearly every
parameter -- 0.001 for Amount, Width, Mix and the percentages, 0.01 for Drive and the two gains --
while the Settings Persistence bar sets its own (`setRange (0, 1, 0.001)`). Only the three
log-frequency splits and the mono-maker frequency have none.

So JUCE moves a slider by at least one interval per notch (`jmax (normRange.interval,
std::abs (delta))`) and the in-drag paths, which restate JUCE's arithmetic because its whole wheel
body sits behind `! e.mods.isAnyMouseButtonDown()`, did not. The gap only opens for a notch smaller
than one interval, which a mouse wheel never sends and a macOS trackpad sends constantly: JUCE
scales precise scrolling by `0.5/256`, so one unit is `deltaY = 0.00195`, which asks for 0.0003 of
Amount -- and `Slider::setValue` snapped it straight back to the value already there. Measured, 20
notches at that size: **0.0000 inside a press against 0.0200 with no button held**. The same
physical gesture, two answers, which is the asymmetry ADR-0053 exists to remove and which its own
in-source comment already claimed to deliver.

The fix is one function, `anamorph::gui::wheelTargetValue`, next to the `DragGestureOwner` interface
the two in-drag callers already share -- direction, scale, rails and JUCE's floor, in one place --
and both callers bank what the control ACTUALLY moved rather than what they asked for. That last
part is not cosmetic: a request banked past a rail is dead travel the next mouse move applies as a
jump. Finding 6A is answered by the same function: the duplication was real and it HAD diverged, so
the answer is not "record why two copies are fine" but "make it one".

### 68b. Two chain defects, and the guard that keeps the third from being a regression

Finding 2: a drag that returns to where it started leaves the scroll chain standing, because the
poll records the chain's name only where it records a step -- correct for a gesture that changed
nothing, wrong for one that changed something and put it back. *"the round-trip drag left the chain
standing: one Undo jumped past it to 0.0000"*.

Finding 3: a host write landing in the commit window is folded into the step the poll records, and
with the name matching it EXTENDED a step the user had already finished. *"the batch carrying the
host write EXTENDED the first scroll: one Undo jumped straight back to 0.0000"*.

Both are the same rule -- a step belongs to a scroll only if the batch that asked for it holds
nothing else -- and both are decided from the generation counter the S10 poll skip already
maintains. The one thing that had to be got right is the *guard*: two notches of ONE scroll can land
in a single poll period and net exactly zero, and a rule phrased as "edited and the signature did
not move" would split a continuous scroll into two undo steps. The name distinguishes them, and leg
P is the leg that fails if it stops doing so.

The foreign test is taken at every gesture EDGE rather than at the close alone, and that came from
the round's own investigation rather than from the review: a write that lands before the NEXT
batch opens is in exactly the same poll period and just as foreign, and by the time that batch
closes the counter has moved for its own writes too. Leg S is that side of the window; leg O is the
close side; M29 removes the open half and kills leg S alone.

**What is NOT fixed, and why it is a decision rather than an omission.** The automation's VALUE is
still inside the step the poll records, so one Undo takes it back with the scroll. Separating it
requires the state as it stood when the gesture closed, and the only place to snapshot that is
inside `parameterGestureChanged`, where D-2/ADR-0036 forbids the APVTS lock -- the lock-order
inversion `--d2-stress-probe` has already reported against a host-thread `replaceState`. The
lock-free alternative is to patch the pushed baseline per parameter from a foreign-write set, which
changes what an undo entry MEANS for every gesture in the plug-in and would want its own ADR and
its own round. Leg O prints the measurement every run rather than asserting it away.

### 68b-ii. And the fix broke the guarantee it serves, which is why fixes get verified too

The first version of the foreign test re-synced the edge generation only at the poll tail. Every
program state jump polls FIRST and applies its parameters AFTER -- and `applyStateSet` notifies the
host, so `soundParamGen` advances once per sound parameter with no poll behind it. The next gesture
open therefore compared against a generation stale by N and called a batch nothing foreign touched
somebody else's write; unnamed, and the notch after it started a second step. *"the scroll after the
Undo was split in two: one Undo stopped at 1.8000, the value between its two notches"* -- a
two-notch scroll right after an Undo becoming TWO undo steps, which is the exact guarantee ADR-0053
exists to give. In a host the 24 Hz tick usually lands between the Undo click and the first notch
and hides it; the leg is deterministic because it does not poll there.

Found by this round's own adversarial verification of its own fix, not by the suite -- every
existing leg opens with `while (canUndo()) undo(); pollUndoCoalesce();`, and that trailing poll is
precisely what re-syncs the edge. A suite that tidies up between steps cannot see a defect whose
whole shape is "the step after an untidy one". Every jump now re-syncs after its own writes (State
test 86 leg T, mutation M39), and the window that remains -- a write between one gesture's own open
and its own close -- is stated at the open-side test rather than left to be rediscovered.

### 68c. Findings 4 and 5, and the two the reading found

Finding 4: a notch at the end of a band's travel converted the solo press into a move -- opening one
or two host gestures and starting the audition -- and the release then took the move branch and
swallowed the click. *"it swallowed the click: mask 0x0 where the uninterrupted press gives 0x2"*.
The travel limits were knowable only by SETTING them, which is why the test could not be written
first; `bandMovePlan` is now the pure half of `beginBandMove` and both derive from it.

Finding 5: both standalone multiband branches opened their change gesture before discovering the
clamped target was the value already there. Each now asks first, from the reading the tick has
already taken and proved, against `writeCrossovers`' own half-pixel threshold.

Found while reading for them: the **in-press width branch** engaged the width drag on a notch at a
rail, so a press that had not crossed the 3 px threshold would then write widths on any one-pixel
tremor; and the **Alt-click reset** bracketed `doReset` in a change gesture whether or not there was
anything to reset, punching in a touch/latch write region for a control that never moved. Both are
ADR-0052, both are three lines, both have a leg.

And one documentation defect, in the changelog rather than the code: the `[0.9.8]` band-move bullet
had been spliced INTO the middle of the solo-click bullet, leaving a truncated sentence and an
orphaned fragment. `check-docs.py` passes it -- a continuation line that follows a complete entry is
indistinguishable from a wrapped one by the grammar it checks -- so it is repaired by hand and
recorded here.

### 68d. Finding 6 is refuted, and the refusal is the interesting part

`ScopedWheelStep`'s destructor writes 0 rather than restoring what it found, and 0 IS what it found
at every construction site this code can reach: the imager's two scopes are mutually exclusive
branches of one handler, `juce::Component::mouseWheelMove` forwards UP only, and no `Knob` is a
descendant of another. The one non-structural interleaving -- a host pumping the message loop from
inside `beginChangeGesture` and delivering a queued notch over another control -- leaves the outer
step unnamed either way, because the disagreement rule already unnames a batch holding two
differently-named gestures. It would take the host ALSO running the 24 Hz poll inside that pumped
loop for a restored key to name anything, and the whole cost of the difference is one extra undo
step. Recorded at the destructor; not hardened, because the hardening is three lines no reachable
path exercises and no test can fail.

### 68f. Residuals -- re-verified, and one register correction

Rounds 9 and 10 shipped no residual table, breaking the §64h/§65h practice; this round's own
read-only audit found the omission and the drift it caused. The table is restored here, and the U4
row is corrected rather than repeated.

| Residual | Where recorded | Disposition against this round |
|---|---|---|
| RISK-010 — the DSP's multiband snapshot is ten independent loads | `docs/FUTURE_RISKS.md:109`, `:249-302` | **UNCHANGED.** The reader (`src/PluginParameters.cpp`) is not in this round's diff, store order is untouched, and none of this round's new paths can write `mbBands`: the reopen conditions at `FUTURE_RISKS.md:288-296` are each checked and none fire. |
| `addBandAt` re-attribution window | in-source, `src/gui/SpectrumImager.cpp` (`nw[i] = (i <= ins) ? wd[i] : wd[i - 1]`) | **UNCHANGED.** `addBandAt` is untouched; ADR-0042's measured disposition stands. |
| Held-audition vblank coverage | worklog §64h / §65h rows | **UNCHANGED.** This round adds no `ScopedGestureAction` decline and no `tick` change; the audition now simply starts LESS often (a blocked notch no longer promotes it). |
| Wheel gesture semantics accepted by ADR-0041 / ADR-0052 | `ADR-0041`, `ADR-0052` | **CHANGED, by decision rather than by evidence.** ADR-0052 now also covers an event whose delta is real but whose target clamps to the value already held; four sites were brought under it and a fifth (the pointer latch) was examined and deliberately left. Recorded in that ADR's own new section. |
| U4 — a mouse-wheel Width edit produces no undo step | worklog `:2546`, `:2818`; `docs/KNOWN_ISSUES.md:132`, `:363` | **CLOSED, and the register was wrong.** The substance ceased to exist in round 9, when the standalone multiband branches gained `beginGesture`/`endGesture`; `KNOWN_ISSUES.md` records the closure with a date and an ADR, while the worklog's last residual table still carried it as "Unchanged". Superseded, not reopened -- and the register over-counted standing residuals by one until this row. |
| ADR-0044 partial transaction residue | ADR-0044, worklog §64h | **UNCHANGED.** `addBandAt` / `removeBand` / `setBands` / `spreadSplits` are untouched. |
| Cancelled-spread visual ordering | worklog §64h | **UNCHANGED.** Same reason; no spread path is in this diff. |
| TSan suppression scope | `tests/tsan-suppressions.txt` | **UNCHANGED in scope, one citation refreshed.** No new lock is taken: the listener callbacks gain relaxed loads and message-thread scalar writes only. The file's own line citations for those callbacks had drifted (they named `PluginProcessor.h:171-174`, which has been `:239-242` for some rounds) and are corrected; the entry itself is not touched, and the `tsan` job's matched-entry assertion still passes. |
| Historical comment blocks | in-source, `src/gui/SpectrumImager.cpp` | **ONE CORRECTED.** Round 10 made a sentence in the standalone width branch false -- it still said `storeOwned`'s result "is discarded deliberately rather than dropped" while the round had just started consuming it to decide the step's name. Fixed in place, which is the §64i obligation the register itself states. The surrounding blocks are otherwise left alone, for the reason §64i gives: they carry measurements a later round needs. |

### 68e. Validation

State 3 114 / 0, DSP 396 / 0. Fourteen new mutations: M26-M35 and M37-M39 killed, M36 surviving
with the reason recorded in `TESTING.md`. M34 SURVIVED its first run and that is recorded too: leg
R pressed where no width drag is latched, so it passed against the mutation it exists to kill --
the leg now opens with a control that fails if the press latches nothing. TSan over the whole suite: 0 warnings, one matched suppression
(`deadlock:WriteFromInsideAGestureOpen`). valgrind memcheck: 0 errors from 0 contexts on both suites
under `ANAMORPH_TESTS_NO_FTZ=1`. UBSan (clang-18, `undefined,float-divide-by-zero`, the CI
ignorelist): clean. Docs 141 clean, citations clean against all three bases, realtime 47 / 0,
portability 57 / 0, preflight exit 0.

### 68g. One more defect, found by CI, and it was in the leg rather than in the plug-in

The first push carrying leg L failed the `macos` self-test -- on the **arm64** host only, while the
same binary under Rosetta reported `3079 checks, 0 failure(s)`. The failing check was
`leg L: the box takes the first notch`, which is the leg's own premise rather than its assertion.

The cause is the leg's input. Leg L measures what a pair of events **sharing one timestamp** does,
so it cannot stamp its events with the `++seq * 7 ms` step every other leg uses -- but `juce::Time`
is millisecond-resolution, and on the faster host the leg's `getCurrentTime()` landed in the same
millisecond as leg I's. Leg I delivers its notch to the KNOB, which forwards it to whichever child
holds the drag; that child is the very value box leg L then scrolls, and the forward stamps the
box's `lastNotchTime`. So the duplicate filter this round added did exactly what it is written to
do and discarded leg L's FIRST notch, and the leg failed against working code.

The fix is in the fixture, and it closes the class rather than the instance. Every leg of test 88
now stamps its events from ITS OWN instant, five seconds past the leg before (`legStamp`, declared
once above leg A), so a stamp that must be shared inside a leg is still unique to that leg. The
pair within each of leg L's stanzas still shares one instant, which is the whole input the leg
exists to send.

**The hazard was never leg L's alone, and the probe says so.** Collapsing `legStamp` to a single
instant -- `return suiteBase;`, the limit case of a host fast enough to run every leg inside one
millisecond -- fails THREE checks, not one:

    [FAIL] leg I: a notch delivered to the knob during its value box's drag still lands
    [FAIL] leg I: ...and the drag's own next event does not erase it
    [FAIL] leg L: the box takes the first notch
    3114 checks, 3 failure(s)

The middle line is the one CI reported; the two above it are leg I failing for the same reason one
leg earlier, which no host has been fast enough to reach yet. That collapse is the standing probe
for this rule. M30, M35, M37 and M38 still kill legs J, K and L with the offsets in place.

Nothing under test compares an event time to the wall clock or to another event's -- JUCE's
`Slider` (`juce_Slider.cpp:1147`) and both in-drag handlers only ever ask whether two stamps are
EQUAL -- so a base in the future is indistinguishable from "now" to everything the legs exercise.
The general rule, now recorded in `TESTING.md`: a stamp that must be shared has to be unique to the
leg sharing it, or running the legs faster changes what they measure.

