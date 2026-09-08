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
