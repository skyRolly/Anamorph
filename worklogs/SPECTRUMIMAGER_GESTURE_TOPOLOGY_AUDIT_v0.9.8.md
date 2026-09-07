# SpectrumImager gesture state vs band topology — audit, v0.9.8

**Round:** follow-up to PR #143 (three stale-topology defects fixed one consumer at a time).
**Question put:** is local validation at each consumer the right architecture, or does gesture state
need centralized invalidation?
**Answer:** centralized, plus one primitive made safe. Recorded as
[ADR-0038](../docs/architecture/design-decisions/ADR-0038-gesture-state-is-void-once-topology-moves.md).

---

## 1. The two findings, and two more the audit added

| # | Site | Claim | Verdict |
|---|---|---|---|
| 1 | `SpectrumImager.cpp:1775` | concurrent band change deletes the wrong band — the topology can move between the call-site check and `removeBand`'s own read, and the clamp retargets | **Real.** A TOCTOU that PR #143's check narrows but cannot close. |
| 2 | `SpectrumImager.cpp:349` | a stale band move overwrites restored splits — `projectFromOrig` returns early with `out[]` still holding drag-start positions, and `writeCrossovers` writes them | **Real, and measured:** `the restored split 900.0 Hz was overwritten with 200.0 Hz`. |
| 3 | *(this audit)* | a topology **rise** also lets a gesture keep writing | **Real.** State test 68 leg C failed before the fix. |
| 4 | *(this audit)* | a void gesture can write splits it never named | **Real.** State test 68 leg E failed before the fix. |

Findings 3 and 4 are why this is an architecture question rather than two more patches: the review
named the two consumers it could see, and the same premise was violated at consumers it did not.

## 2. Reproduction

All four are driven by State test 68 in `tests/state_tests.cpp`, through the real editor with real
`juce::MouseEvent`s (the harness State test 66 established). Against `aa55f20` — the head with both
previous rounds' fixes — legs A, C and E fail:

```
State test 68: a gesture whose topology moved writes nothing further (ADR-0038)
  [leg A] the restored split 900.0 Hz was overwritten with 200.0 Hz
  [FAIL] leg A: a band move stale against a restore does not overwrite the restored split
  [FAIL] leg C: a rise voids the drag too -- no further write
  [FAIL] leg E: no band the gesture never named is modified
2544 checks, 3 failure(s)
```

Leg B — the same scenario driven through a **crossover** drag rather than a band move — **passed**
before the fix. That asymmetry is the evidence for the architecture: `dragCrossoverTo` validates and
returns, so it was already safe; `moveBand` validates its *pins* and then writes the *unpinned*
splits from drag-start origins, so it was not. Consumer-by-consumer validation had covered one and
missed the other, which is the failure mode of option A stated as a measurement.

## 3. The field map

Every cached gesture identifier, what it is defined relative to, and what its consumers did with it
**before** this round:

| Field | Defined relative to | Latched at | Consumer | Revalidated? | If stale |
|---|---|---|---|---|---|
| `dragHandle` | split position | `mouseDown` | `dragCrossoverTo` | yes (`handle >= M` returns) | nothing |
| `dragHandle` | split position | `mouseDown` | `mouseUp` → `removeBand` | call-site check only (racy) | **wrong band deleted** |
| `dragBand` | band index | `mouseDown` | `mouseDrag` width write | no | writes the vanished band's own Width |
| `dragOrigX[3]` | split x at press | `captureDragOrigins` | `projectFromOrig` | n/a (all slots seeded) | — |
| `soloPressBand` | band index | `mouseDown` | `mouseUp` solo actions | no | mask bit for a vanished band |
| `soloMoveLeft/Right` | split positions | `beginBandMove` | `projectFromOrig` pins | yes | move does less… |
| — the same, via `writeCrossovers` | split x at press | `beginBandMove` | `moveBand` | **no** | **…but writes drag-start positions over a restore** |
| `pressDeleteBand` | band index | `mouseDown` | `mouseUp` | yes (`deleteHit == dB`) | nothing |
| `scrollHandle` / `scrollBand` | index at hover | `updateHover` | `mouseWheelMove` | yes (`< N-1` / `< N`) | nothing |
| `bandStartLeftX/RightX`, `bandTmin/Tmax`, `bandAnchorX`, `dragGrabDX/DY` | pixels at press | press | the move/drag maths | no | positions from the old geometry |

## 4. The topology-change paths, and which ones race

| Path | Thread | Can land *inside* a mouse handler? |
|---|---|---|
| click-to-add (`addBandAt`), delete-x, outward-drag removal | message | no — same handler, synchronous |
| **host automation on `mbBands`** | host / audio | **yes** |
| **state restore (`setStateInformation`)** | host thread; the sound half is applied synchronously (ADR-0036) | **yes** |
| preset load, A/B apply, undo/redo, factory reset | message | no — between handlers only |

This split is the whole design question. For the message-thread paths a check at handler entry is
**exact**: nothing can move between the check and the writes, because the mutator would have to run
on the same thread. For the two host-thread paths it is **not**, and no check can be — which is why
the decision has two halves.

## 5. Options considered

**A — local validation at every consumer.** What PR #143 built. Rejected on evidence: it is what
missed findings 2, 3 and 4, it scales with the number of consumers rather than with the number of
invariants, and every future consumer is a new chance to forget. It also cannot close finding 1 at
all, since a check and the operation it guards are two separate reads of a live parameter.

**B — centralized invalidation.** Snapshot the topology when a press begins; void the whole gesture
the moment the live count differs. One rule, two call sites, and it covers every identifier at once
because they are all defined against the same topology. Exact for the message-thread paths. Leaves
the host-thread window open.

**C — B, plus making the racy primitive safe.** As B, and `removeBand` **refuses** a non-live index
instead of clamping it into a live one. The clamp was the mechanism that turned "stale" into "wrong
target"; removing it means that even inside the window a check cannot reach, the worst outcome is
that nothing happens.

**Rejected outright:** taking a lock around the gesture (the topology is written from the audio
thread — `REALTIME_AUDIO_POLICY` forbids it, and it would be a Thread Model change); marshalling
parameter writes through the message thread (a far larger change than the defect warrants, and
ADR-0036 deliberately chose snapshot exchange over marshalling); polling in the 24 Hz reconcile
(42 ms of continued writing, and the event handlers are the exact points where the state is used).

**Decision: C.** Neither half is sufficient alone — B leaves the wrong-target outcome reachable in
the window, and the refusal alone does nothing about findings 2, 3 and 4, which are write-through
rather than retargeting.

## 6. What the decision costs

A gesture is now **cancelled** when the topology moves under it, where before it continued. That is
a deliberate behaviour change and it supersedes what State test 66 legs A–D asserted: they were
written to prove the gesture *continued correctly* across a rise, and now prove it *stops*. Leg A's
liveness check inverted accordingly, with the reason recorded in the test. This is the same
principle ADR-0036 §25 settled for the processor — an obsolete writer must not reassert over a newer
authority — applied to the gesture layer.

`captureDragOrigins()` (ADR-0038's predecessor round) becomes **defence in depth**: with the gesture
voided on any change, `moveBand`'s live count always equals the snapshot, so the slots beyond it are
never read. It is kept as the second layer, not removed.

## 7. What could not be proven, and is recorded rather than claimed

The **refusal in `removeBand` has no reachable test.** The centralized guard returns from `mouseUp`
before `removeBand` is called on every path a single-threaded suite can build, so the refusal is
shadowed. Measured: restoring the clamp *and* deleting the call-site guard leaves all 2544 checks
green. It exists for the window a check cannot enter, and that window needs a real concurrent write.
An earlier draft of State test 68 leg D claimed to prove it; the claim was wrong and was corrected in
the test rather than left standing.

**Not fixed, with reasons.** Two rows of §3 write a *vanished object's own parameter* rather than
another object's: `dragBand`'s Width write and `soloPressBand`'s mask bit. Both are now unreachable
through a gesture (the guard voids it first), and both remain reachable with **no gesture at all** —
a host lowering Bands while a solo is latched leaves the same phantom bit. That is Bands/parameter
coherence, a separate question from gesture lifetime, and it is left to whoever takes it on.

**Also unfixed, and narrower than the findings:** a restore that changes the crossover *values*
while leaving Bands **unchanged** does not move the snapshot, so the gesture continues and its writes
win. Closing it needs a different signal — the drag comparing the live crossovers against what it
last wrote, or the imager observing the processor's `soundSetGen`. Neither finding claims it, no test
here exercises it, and it is recorded so a future round starts from the design rather than the
symptom.
