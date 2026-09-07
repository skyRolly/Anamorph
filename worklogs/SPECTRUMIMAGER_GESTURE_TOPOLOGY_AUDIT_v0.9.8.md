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

---

# Round 2 (2026-09-07) — the two review findings on the ADR-0038 change, and the two residuals

The change this worklog records shipped as `b65ce4e`. Review came back with one finding from **each
direction** of the guard it introduced, and the same review asked for a ruling on the two residuals
§7 had recorded. Everything below is measured against `b65ce4e`, not against the pre-ADR-0038 tree.

## 8. What was open at the start of round 2

| # | Anchor at `b65ce4e` | Finding | Source | Class |
|---|---|---|---|---|
| R1 | `SpectrumImager.cpp:1775` | Concurrent band change deletes wrong band — the liveness check and `removeBand` read Bands separately | review | retarget |
| R2 | `SpectrumImager.cpp:1697` | Newly added splits stop dragging — the add raises Bands after `gestureBands` was captured | review | over-invalidation |
| R3 | §7 residual | A restore that changes crossover VALUES at an unchanged count is not seen | audit residual | write-through |
| R4 | §7 residual | A vanished band's Width and solo bit stay writable | audit residual | coherence |

## 9. Reproduction, before any change

Both review findings and R3 reproduce deterministically in the repository's own harness — the real
editor, real `juce::MouseEvent`s, no mock. Output from `b65ce4e` with the new tests present:

```
State test 69: a gesture owns the topology it created, and none it did not
  [leg A] the new split stayed at 1392.1 Hz (from 1392.1 Hz): the press's own add voided its gesture
  [FAIL] leg A: the newly added split follows the press that created it
  [leg C] Bands 4 -> 3: the release read a count the check never saw
  [FAIL] leg C: a Bands move inside mouseUp removes no band
  [FAIL] leg C: ...and rewrites no split or width of the topology it never saw
State test 70: a sound change under a gesture voids it, count or no count
  [leg A] the restored split 15000.0 Hz was pulled back to 10000.0 Hz by a drag that never named it
  [FAIL] leg A: a value-only sound change is not undone by the drag that outlived it
  [FAIL] leg B: a band move stale against a value-only change does not undo it either
```

**How R1's window is made deterministic without a thread.** JUCE dispatches
`AudioProcessorParameter::Listener::parameterGestureChanged` **synchronously** from
`endChangeGesture` (`juce_AudioProcessorParameter.cpp:101`). `mouseUp` calls `endGesture` on the
dragged split *before* the removal, so a listener attached to `mbFreqLow` writes `mbBands` in exactly
the window between the release's liveness check and `removeBand`'s own read — a real listener on a
real parameter, no sleep, no test-only production code. This is the model of a host recording
automation, a control surface, or the sound half of a restore landing at the instant of release.

## 10. Why a caller-side check cannot be the fix — the measurement

Option 3 of ADR-0039 was tested rather than argued. Mutation **M7** restores the round-3 caller-side
liveness check `dragHandle < bandCount() - 1` *and* removes the new contract:

```
--- M7 caller-side liveness check instead of the contract ---
  [FAIL] leg C: a Bands move inside mouseUp removes no band
  [FAIL] leg C: ...and rewrites no split or width of the topology it never saw
```

The check passes because a stale index can land inside a **new** range perfectly well. Two bands,
drag split 0 outside, the count rises to four inside `mouseUp`: `0 < 4 - 1` is true, and
`removeBand (1)` then merges bands out of a layout the user never saw. A check in the caller cannot
close a window that opens after it; only the operation validating the topology it was aimed at can.

## 11. R3 — the sound is part of the gesture's world

Traced sources of an authoritative crossover change: `setStateInformation` (the sound half), preset
load, A/B apply, undo/redo, and a host automation lane. All of them write the same three parameters.

**Is an existing signal reusable?** `soundSetGen` (`PluginProcessor.h:267`) is the repository's
authoritative counter, but it counts **wholesale replacements** only, so it would miss a single
automated crossover and an undo step; and it is not reachable — the imager is constructed with a
`ScopeBuffer&` and an `APVTS&` and holds nothing else. `isSweeping` is an *animation* window
(`PluginEditor.cpp:571`: `uiAnimOn && knobSweepTime > 0`), gated on a user preference and set by knob
resets, so it is not a correctness signal either.

**Self-comparison is exact here.** Every writer of `mbFreqLow`/`Mid`/`High` is either this class or
somebody outside it: the engine only reads them (`AnamorphEngine.cpp:609`, `:616`), and no processor
path writes them back. So "it moved and I did not move it" needs no plumbing, and it covers
automation and undo as well as restore, which the injected counter would not.

## 12. R4 — the disposition, with the evidence

| Question | Answer, with the source |
|---|---|
| What does `mbBands` own? | The active band count 1..4. It does not own the per-band values. |
| What happens to a vanished band's Width? | Nothing. `MultibandWidth.h:53-56`: *"Only (bandCount - 1) crossovers and bandCount widths are used"*. |
| What happens to its solo bit? | Nothing audible. `SoloMonitor.cpp:85` computes `mask & ((1 << bands) - 1)`, and `:96` holds every band above the count at gain 0 *"so a band-count change settles cleanly"*. |
| Can hidden values become observable again? | Yes — exactly, by raising Bands again. That is the point of retaining them. |
| Does save/restore preserve them? | Yes; all four widths and the mask are registry fields. |
| Can host automation target them? | Yes; they are ordinary automatable parameters. Writing one while it is hidden changes nothing until the count returns. |
| Do the imager's own edits remap them? | Yes. `addBandAt` and `removeBand` both rewrite the width array and renumber the solo mask for the new topology — which is why the retained values matter. |

**Decision: preserve (outcome B).** The values are inert while hidden, exact when the count returns,
and load-bearing for the imager's own add/remove remapping. Clearing them on a count change would
lose the user's settings on an automation dip and would change what `mbWidth*` and `mbSolo` mean
across a count change — a Parameter Registry / Serialization Registry semantics change, and so an
Architecture Review Gate item rather than a bug-fix-round decision.

## 13. The final field sweep

Every topology- or sound-dependent gesture field, its frame of reference, and what a stale value can
do after this round. "Retarget" means *reaching a different live object*; "write-through" means
*writing a live object from stale data*.

| Field | Relative to | Captured | Invalidated by | Consumer behaviour when stale | Retarget? | Write-through? |
|---|---|---|---|---|---|---|
| `dragHandle` | split numbering under `gestureBands` | `mouseDown` (handle branch; add branch, after its own add) | `gestureIsStale()` in `mouseDrag`/`mouseUp`; cleared at the end of `mouseUp` | `dragCrossoverTo` validates against a live `M` and returns; `removeBand` refuses a topology it was not aimed at | **No** | **No** |
| `dragBand` | band numbering | `mouseDown` (width branch) | as above | `setParam (widthP[dragBand], …)` writes a **vanished band's own** Width in the race window — inert (`MultibandWidth.h:53`) | **No** | Its own only |
| `soloPressBand` | band numbering | `mouseDown` (solo branch) | as above | sets a hidden solo bit in the race window — inert (`SoloMonitor.cpp:85`) | **No** | Its own only |
| `soloMoveLeft` / `soloMoveRight` | crossover numbering | `beginBandMove` | as above | `projectFromOrig` validates both pins and returns when neither is live | **No** | **No** |
| `pressDeleteBand` | band numbering | `mouseDown` (delete branch) | as above | `deleteHit` re-proves the position **and** `removeBand` re-proves the topology | **No** | **No** |
| `scrollHandle` / `scrollBand` | split / band numbering | first wheel event | 3 px `mouseMove`, `mouseExit` | bounds-checked against a live `N` in the same statement; a stale one does nothing | **No** | **No** |
| `dragOrigX[3]` | the sound at gesture start | `captureDragOrigins` | `soundMovedUnderGesture()` — **new this round** | was the write-through vector; now the gesture is void before it is read again | **No** | **No** |
| `gestureX[3]` | the sound the gesture last left | `captureDragOrigins`, then every `writeCrossovers` | `cancelActiveDrag`, end of `mouseUp` | it *is* the detector | n/a | n/a |
| `dragRemovePending` | the gesture | `mouseDrag` | as `dragHandle` | `removeBand` refuses unless the topology matches | **No** | **No** |
| `bandAnchorX`, `bandStartLeftX/RightX`, `bandTmin/Tmax` | pixels at `beginBandMove` | `beginBandMove` | as above | feed `moveBand`'s projection, which is voided first | **No** | **No** |
| `handlePressMs/X`, `widthPressY`, `dragGrabDX/DY`, the hold flags | the gesture | `mouseDown`/`mouseDrag` | as above | pure UI anchors; name no object | **No** | **No** |

Two cells are deliberately not "No": a width drag and a solo press can, inside the race window,
write the **vanished band's own** parameter. Neither reaches another band, and both are inert until
the count returns — which is exactly the F4 disposition above. They are recorded as accepted
residuals, not closed.

## 14. What could not be proven, round 2

- **`removeBand`'s topology refusal has a residual by construction.** It reduces the window to
  "between its single read and its writes". A change landing *there* makes its writes and the outside
  writer's writes two concurrent writers to the same parameters — last writer wins. That is a
  coherence property no lock-free design can exclude, and it is **not** a stale-identifier retarget.
- **Mutation M5 is not caught.** Clearing `gestureBands` before `cancelActiveDrag`'s cheap exit is
  invisible to a single-threaded suite: the leaked snapshot is always re-taken by the next
  `mouseDown` before anything can consume it. Recorded rather than dressed up, exactly as ADR-0038
  recorded the refusal having no reachable test.
- **`addBandAt` reporting its own `N + 1`** rather than a second `bandCount()` read is likewise
  untestable here; both reads agree in a single-threaded suite.

---

# Round 3 (2026-09-07) — write ownership: the check and the store are not adjacent

Round 2 shipped as `6e37e6e`. Review returned with three findings that are the same defect in three
places: **the staleness check and the store it guards are separated, and the authoritative value can
move in between.** This is the decision record required before any production edit.

## 15. What was open at the start of round 3

| # | Anchor at `6e37e6e` | Finding | Kind |
|---|---|---|---|
| W1 | `SpectrumImager.h:304` | Width changes are overwritten — `gestureX` tracks crossovers only, so a width-only change is invisible | blind spot |
| W2 | `SpectrumImager.cpp:309` | Concurrent crossover changes are reclaimed — `writeCrossovers` can replace an externally changed split, and `captureGestureSound` runs after **all** stores | check/store gap + laundering |
| W3 | `SpectrumImager.cpp:579` | Concurrent band changes are overwritten — `expectedBands` protects one read, not the store burst that follows | check/store gap |

## 16. Reproduction, before any change

State test 71, against `6e37e6e`:

```
State test 71: a gesture writes only what it owns, and claims only what it wrote
  [leg A] the installed width 1.700 was overwritten with 0.650 by a drag anchored before it
  [FAIL] leg A: an external width change is not overwritten by the drag that outlived it
  [leg B] the split written from inside the burst (15000.0 Hz) was reclaimed as 10000.0 Hz
  [FAIL] leg B: a split written from inside the burst is not overwritten by the rest of it
  [leg C] Bands was moved to 2 from inside the burst and the rest of it wrote 3 back
  [FAIL] leg C: a topology installed from inside the removal burst is not written over
```

Legs (d), (e) and (f) — an uninterrupted width drag, the neighbour push/spring-back, and a steady
delete x — pass throughout and are the positive controls.

## 17. The mechanism, read out of JUCE rather than assumed

This is the fact the whole round turns on, and it changes the class of the defect.

```
juce_AudioProcessorParameter.cpp:59-63
    void AudioProcessorParameter::setValueNotifyingHost (float newValue)
    { setValue (newValue); sendValueChangedMessageToListeners (newValue); }

juce_AudioProcessorParameter.cpp:111-121
    ScopedLock lock (listenerLock);
    for (int i = listeners.size(); --i >= 0;)
        if (auto* l = listeners [i]) l->parameterValueChanged (getParameterIndex(), newValue);

juce_AudioProcessor.cpp:1467-1475
    void AudioProcessor::ParameterChangeForwarder::parameterValueChanged (int index, float value)
    { ... l->audioProcessorParameterChanged (owner, index, value); }
```

`ParameterChangeForwarder` is an `AudioProcessorParameter::Listener` attached to every parameter, and
every plugin-format wrapper registers an `AudioProcessorListener` behind it
(`juce_audio_plugin_client_VST2.cpp:263-266`, `..._AU_1.mm:188`, `..._AUv3.mm:236`,
`..._LV2.cpp:135`). **So every `setParam` the imager makes reaches the host synchronously, inside the
call**, and a host that writes back — a linked-parameter macro, an automation write-back, a control
surface echo — re-enters on the message thread before `setParam` returns.

First-party listeners were checked and none of them writes a multiband parameter:
`AnamorphAudioProcessor::parameterChanged` sets an atomic (`PluginProcessor.cpp:308-317`),
`ViewGenWatcher` bumps a counter (`PluginProcessor.h:312-318`), `parameterGestureChanged` counts
gestures (`PluginProcessor.cpp:814-825`). The engine only **reads** the crossovers and widths
(`AnamorphEngine.cpp:609`, `:616`). Every writer is therefore the imager itself or somebody outside
the plug-in.

**This reclassifies W2 and W3.** They are not an unclosable cross-thread race: they are *synchronous
reentrancy on the message thread*, and a check placed **adjacent** to its store closes them
completely, because straight-line code between a comparison and the store that follows it cannot be
interrupted by a listener. What remains after that is the ordinary cross-thread concurrency window of
a single store, which is irreducible without a lock and which no design here can or should claim.

## 18. The common invariant

> **A gesture stores an authoritative value only while that value is still the one its plan was
> computed from; the comparison is adjacent to the store, with no call between them; and ownership of
> what was stored is recorded at the store, never in a blanket pass afterwards. The first value found
> not to be owned abandons every remaining store of the burst.**

Two owners instantiate the one rule:

* a **drag** owns `gestureX[3]` (split positions) and `gestureW[4]` (band widths) — what it last
  observed or wrote;
* a **topology transaction** (`removeBand`, `addBandAt`) owns `expectedBands` plus the `fr[]`,
  `wd[]` and `oldMask` its plan was computed from at entry.

## 19. Candidate architectures, and why the others lose

| | Design | Verdict |
|---|---|---|
| **A** | Expanded local stale checks before each write | This *is* the selected mechanism's shape, but on its own it is what ADR-0038 rejected: per-consumer checks that a future consumer forgets. Taken only **because** it is bound to one named invariant and one recording rule. |
| **B** | A shared authoritative-state generation the gesture snapshots | Rejected on coverage and reachability, as in ADR-0039: `soundSetGen` counts wholesale replacements only, so it misses a single automated crossover, an undo step and every width change; and it is not reachable from the imager. It also cannot help at all with W2/W3, whose writer is a **host re-entering inside our own store** — no generation bump exists for that. |
| **C** | The gesture owns a snapshot of exactly what it observed or wrote | Selected, as the *ownership record*. Extended from crossovers to widths (W1) and given per-store recording (W2). |
| **D** | Conditional / transactional commit adjacent to the mutation | Selected, as the *store discipline*. C answers "what do I own"; D answers "when may I store it". Neither alone is enough: C with a blanket post-burst record launders the intruder's value (W2); D without an ownership record has nothing to compare against. |
| **E** | Restructure so only the count write can lose; defer the burst; route topology edits through the processor | Rejected. Deferring a burst is message-thread marshalling used to hide a correctness problem, which the brief forbids and which ADR-0036 already decided against for state. No reordering makes a partial burst inert: `setSoloMask` writes a whole 4-bit word, so there is no "safe first store". |

**Rejected outright:** a lock (`mbBands` is written from the audio thread —
`REALTIME_AUDIO_POLICY`, and a Thread Model change); `setValue` without notification (silently
desynchronises the host and breaks automation); post-hoc repair of a burst that lost its precondition
(fights the newer authority, which is the opposite of ADR-0036 §25).

## 20. Why this closes the window, and exactly what it leaves open

**Closed — synchronous reentrancy.** In `writeCrossovers` the comparison `freqToX (crossover (k))` vs
`gestureX[k]` and the `setParam` that may follow are adjacent statements in one iteration; nothing
runs between them. A host re-entering through the notification of store *k−1* is therefore seen by
iteration *k*'s comparison, and the burst is abandoned. Same shape in `removeBand`/`addBandAt`: the
precondition is re-read immediately before each store, so a write landing inside store *n* is caught
before store *n+1*.

**Left open, and stated rather than dressed up:**

1. **A truly concurrent store.** A host thread writing between our comparison and our store is not
   excluded and cannot be without a lock. Its blast radius is now **one parameter**, not a burst.
2. **A partially applied topology transaction.** Aborting mid-burst leaves the stores already issued.
   This is the correct trade: the completed stale operation *restores the old count over the newer
   one* (measured, `wrote 3 back`), whereas the abort leaves the newer topology standing and at most
   some already-issued compaction. A legitimately-begun operation truncated by a newer authority is
   exactly ADR-0036 §25's rule, not a stale overwrite.
3. **Case C of the width analysis** — an external width write during a *bare* click is bracketed by
   the user's own `begin`/`endChangeGesture` and folds into their undo step. That is undo attribution,
   not write ownership, and it is the same class as the wheel path's gesture-less writes. Out of
   scope, recorded.
4. **The wheel path** (`mouseWheelMove`) writes widths and crossovers with no gesture in flight;
   `scrollHandle`/`scrollBand` are bounds-checked against a live count in the same statement. No
   gesture, so no gesture ownership. Out of scope, recorded.

## 21. Width semantics, decided

Today's behaviour is inconsistent purely by threshold timing: an external width change **after** the
3 px engage is overwritten (`:1815` computes from `dragGrabDY`, and the live width is never read
again), while one **before** the engage is silently adopted as the anchor and re-emitted inside the
user's gesture (`:1812`). Both are wrong in the same way and in opposite directions.

**Decision: void the gesture, uniformly.** Any width the gesture does not own, at any point in its
life, voids it — the same answer ADR-0038 gives for the count and ADR-0039 gives for the splits. The
comparison is exact equality against the read-back, so the drag's own store can never trip it, and no
epsilon has to be invented for a path that has no write-suppression epsilon.

## 22. Implementation chronology

1. State test 71 written first, asserting the invariant against unmodified `6e37e6e`; legs (a), (b)
   and (c) fail, legs (d), (e), (f) pass. §16.
2. The JUCE dispatch read out rather than assumed (§17), which reclassified W2 and W3 from
   "unclosable cross-thread race" to "synchronous reentrancy, closable by construction".
3. The decision record above written before any production edit, per the round's own gate.
4. `writeCrossovers` rewritten: `ownsSplit (k)` adjacent to the store, the record taken from the
   read-back of that slot immediately, the blanket `captureGestureSound()` removed, and a `bool`
   returned so the caller abandons the event.
5. `gestureW[4]`, `ownsWidth`, and the width branch of `mouseDrag` — ownership checked before the
   anchor as well as before the store, so the case-A/case-B inconsistency disappears.
6. `removeBand` and `addBandAt`: the count re-read and each target re-proved before every store.
   `addBandAt`'s first draft compared `xs[i]` for all `i < N` and broke State test 69 leg (a) — `xs`
   holds only the `M = N - 1` splits that exist, and slot `M` is the one the add creates, so there is
   nothing there to own. Recorded rather than quietly fixed.
7. Legs (g) and (h) added to separate the laundering half from the overwrite half, and to prove the
   add transaction rather than argue it.
8. The two stray copies of `0.5f` in `resetCrossover` and `commitFreqEditor` pointed at
   `kSplitMovedPx`, which the ADR-0039 comment already required.

## 23. The final stale-write audit

Every store performed while a gesture is active, with what establishes ownership and where it is
validated.

| Store | Writes | Ownership | Validated | What can change it after | Prevented by |
|---|---|---|---|---|---|
| `writeCrossovers` → `freqP[k]` | one split | `gestureX[k]`, within `kSplitMovedPx` | the statement before the store | a host re-entering through store `k−1`'s notification | `ownsSplit (k)`, adjacent; the burst is abandoned |
| `mouseDrag` width → `widthP[dragBand]` | one width | `gestureW[b]`, exactly | the statement before the anchor and before the store | another message-thread event, or a host/audio-thread write between two mouse callbacks | `ownsWidth (b)`, adjacent; the gesture is voided |
| `mouseDrag` width anchor (`dragGrabDY`) | nothing — reads `bandWidth` | as above | same check | as above | same check; the anchor is no longer taken from a value we do not own |
| `removeBand` → `soloP` | the 4-bit mask | `oldMask` + `expectedBands` | immediately before | a host writing from inside an earlier store — there is none, this is first | the count and mask are re-proved before it |
| `removeBand` → `widthP[k]` | one width | `wd[k]` + `expectedBands` | immediately before each | a host re-entering through the preceding store | re-read count + CAS; the rest is abandoned |
| `removeBand` → `freqP[k]` | one split | `fr[k]` + `expectedBands` | immediately before each | as above | as above |
| `removeBand` → `bandsP` | the count | `expectedBands` | immediately before | as above | as above |
| `addBandAt` → the same four kinds | as above | its own `N`, `wd[]`, `xs[]`, `oldMask` | immediately before each | as above | as above |
| `endGesture` / `beginGesture` | nothing | n/a | n/a | notifies the host, so it is a reentrancy *point*, not a write | the store that follows it is checked |
| `mouseWheelMove` → `freqP`/`widthP` | one split or width | none — no gesture is in flight | indices bounds-checked against a live count in the same statement | n/a | out of scope by construction; recorded §20.4 |

Nothing on that list can retarget another live object, and nothing can overwrite a value the gesture
does not own except in the single-store cross-thread window §20.1 names.

## 24. Validation and residuals, round 3

State suite **2 598 / 0**; State tests 66–70 unchanged and green throughout. Mutations N1, N2a, N2b,
N3 and N4 each killed by exactly one named leg. Residuals unchanged from §20 plus §12's
vanished-band Width and solo-mask disposition, which this round re-checked and did not move: the new
machinery touches the same stores, and nothing it found makes those values user-visible incorrect
behaviour.

## 25. The correction the adversarial pass forced, same day

The round's own audit ran a five-way design panel and four adversarial judges per candidate over the
**shipped** shape. Three of the flaws they returned were real, and one of them contradicts a claim
this worklog and ADR-0040 had already made. Reproduced as State test 72 against `3bdc488`:

```
State test 72: the check is adjacent to EVERY store, including the ones that open a gesture
  [leg A] Bands was moved to 2 from inside the gesture that opens the commit, and the commit wrote 3 over it
  [FAIL] leg A: a write inside the commit's own gesture-open is not written over
  [leg B] the echoed value 500.0 Hz was adopted as the gesture's own and then overwritten with 131.3 Hz
  [FAIL] leg B: a same-parameter echo is not adopted as the gesture's own
```

| # | What was wrong | Fix | Mutation |
|---|---|---|---|
| C1 | `setBands`/`setSoloMask` dispatch `parameterGestureChanged(idx, true)` **before** their value store, so the caller's check was not adjacent at either commit point | `expectedBands` (+ `expectedMask`) re-proved between the open and the store | **N6** → leg (a) |
| C2 | The ownership record was a bare read-back, so a listener writing the SAME parameter from inside its own store was adopted | confirm the store landed against what it asked for, at the write path's own tolerance; record only slots this pass wrote | **N5** → leg (b) |
| C3 | `writeCrossovers` re-proved each value but not the count | count re-proved per store | **N7 not caught** — the consequence is inert writes to splits above the live count |

**Why State test 71 could not have found C2:** every one of its legs hooks one parameter and aims the
probe at a *different* one, so the same-parameter case was structurally outside it. That is the kind
of blind spot an adversarial pass exists to find, and it found it in code that had already shipped.

**One flag investigated and rejected.** `mouseWheelMove` was called a laundering hole for re-seeding
ownership mid-drag. Measured: it re-seeds `dragOrigX` and `gestureX` **together**, so the projection
targets move with the record and nothing stale is written. State test 72 leg (c) passes before and
after and is kept as the guard on that pairing.

---

# Round 4 (2026-09-07) — a coupled update is all of it or none of it

Round 3 shipped as `799113f`. Review returned three findings and two deeper questions. This is the
decision record required before any production edit.

## 26. What was open at the start of round 4

| # | Anchor at `799113f` | Finding | Kind |
|---|---|---|---|
| T1 | `SpectrumImager.cpp:2095` | Wheel input revives stale width drags | partial **refresh** |
| T2 | `SpectrumImager.cpp:1985` | Solo clicks target replaced layouts | commit with no precondition |
| T3 | `SpectrumImager.cpp:646` | Failed solo remaps still change bands | **refusal nobody heard** |
| T4 | `SpectrumImager.cpp:366` | `ownsSplit` ignores host changes within half a display pixel | investigate |
| T5 | `SpectrumImager.cpp:638` | Per-store checks cannot make topology edits atomic | investigate |

## 27. Reproduction, before any change

```
State test 73: a coupled update is all of it or none of it
  [leg A] a wheel tick adopted the installed width 1.700, and the drag then wrote 0.650 from an anchor taken before it
  [leg B] the click soloed band 3 of a four-band layout, Bands became 2 inside the store, and the mask was written as 0x8 anyway
  [leg C] the mask store was refused and the transaction carried on: Bands 3 with the mask left in the old numbering (0x5)
```

Leg (a) also refutes a claim round 3 made. State test 72 leg (c) established that
`captureDragOrigins()` re-seeds `dragOrigX` **and** `gestureX` together, and round 3's report
generalised that to "nothing stale is written". That is true for splits and **false for widths**: the
width store computes from `dragGrabDY`, a third piece of state the refresh does not touch. The
correction is recorded here rather than amended away.

## 28. The common invariant

All three findings are the same shape: **part of a coupled change was applied and the rest was not,
and nothing downstream could tell.**

> **A coupled update — a commit or a refresh — is all of it or none of it. A conditional store must
> report whether it committed, and a caller that derived state from a precondition must abandon the
> rest of its plan the moment any store does not commit. A refresh that cannot bring every piece of
> state the next write depends on to the same authoritative sound must refresh none of it.**

* **T3** is a *commit* half-applied: `setSoloMask` refused and said nothing, so the transaction
  changed the band count with the mask still in the old numbering.
* **T2** is a *commit* with no precondition at all: `mouseUp` clears `gestureBands` before the solo
  branch, so `setSoloMask` runs at its `expectedBands = -1` default.
* **T1** is a *refresh* half-applied: `captureDragOrigins()` adopts an outside width into `gestureW`
  while `dragGrabDY` still points at the sound before it.

## 29. T4 — the half-pixel threshold, measured

`kSplitMovedPx = 0.5f` was doing two jobs: deciding whether a write is worth making (its purpose) and
deciding whether a value is the gesture's own (not its purpose). Measured against the real axis
(`kAxis` + the Fritsch–Carlson map, reproduced exactly) at the harness's 902 px plot:

| Hz | +0.5 px | Δ Hz | % |
|---|---|---|---|
| 30 | 30.193 | 0.19 | 0.642 % |
| 200 | 200.703 | 0.70 | 0.352 % |
| 1 000 | 1 003.529 | 3.53 | 0.353 % |
| 10 000 | 10 032.307 | 32.31 | 0.323 % |
| 18 000 | 18 054.795 | 54.79 | 0.304 % |

Worst case **0.646 % at 27 Hz**, worst absolute **61.24 Hz at 19 905 Hz**. The crossover parameters
are `logFreqRange (20, 20000)` with **no interval** (`PluginParameters.cpp:238-240`), so the smallest
representable step is the float itself — about **0.00055 Hz at 1 kHz**. A 32 Hz change at 10 kHz is
therefore roughly **59 000 times** the parameter's resolution: unambiguously representable, fully
observable through host automation, and — until this round — reclaimable by the gesture.

**Decision: outcome C.** Pixel space is the wrong ownership primitive. Ownership moves to the
**normalised parameter value, compared exactly**. That is safe because
`AudioParameterFloat::setValue` is `value = convertFrom0to1 (newValue)` with no snapping and
`getValue()` is `convertTo0to1 (value)` (`juce_AudioParameterFloat.cpp:97-98`), so
`convertTo0to1 (convertFrom0to1 (norm))` is bit-identical to what a clean store leaves — for the
width family too, whatever its 0.001 interval does, because both sides use the same two conversions.
`kSplitMovedPx` keeps its real job and only that job; `kWidthQuantum` is no longer needed.

## 30. T5 — the cross-thread topology transaction

| Option | Verdict |
|---|---|
| **A** per-store conditional ownership | **Kept**, and materially tightened by T3's fix: a refused store now stops the transaction instead of letting it run on. |
| **B** single conditional commit from one snapshot | Rejected as unreachable, not as undesirable. The layout lives in **eight separate automatable parameters**; there is no single commit point to make conditional. Approximating one means exactly the per-store re-proof A already performs. |
| **C** versioned topology | Rejected for ADR-0040 option 2's reason, now sharper: the **silent writer** (`reassertParameters` with `notifyHost = false`) advances no version yet moves what the imager reads, so a version would have to be added to the restore path — a processor change to reach a guarantee the value comparison already gives. |
| **D** lock | Prohibited: `mbBands` is written from the audio thread, so `REALTIME_AUDIO_POLICY` forbids it, and it would be a Thread Model change. The one lock that exists (`soundReplacement`, ADR-0036 §24) is never taken by the audio thread but does not close reentrancy at all, is scoped to whole-sound replacements, and would block the message thread. |
| **E** other | Nothing the measured facts support. |

**Ruling: a bounded, documented concurrency trade — not a defect, and not an architectural
violation.** What *was* a defect is T3, where the transaction continued after its own precondition
had been refused; that is fixed. What remains is a store landing between our comparison and the
`setValueNotifyingHost` on the next line, from another thread. Its blast radius is one parameter, and
under a *lower* new count the parameters left behind are ones the DSP does not read
(`SoloMonitor.cpp:85`, `MultibandWidth.h:53`).

## 31. T5 semantics — what the solo operation owns

`mbSolo` is a positional 4-bit word, and the solo operation owns **a positional band index plus the
topology that numbering belongs to** — which is why the index alone is not enough and why
`expectedBands` is the missing half. Nothing here changes what `mbSolo` or `mbBands` *mean*: no
registry field, range, default or serialization semantics moves, so this is a bug fix and not an
Architecture Review Gate item.

## 32. T1 semantics — what a refresh must refresh

The width write depends on `dragGrabDY`; the crossover write depends on `dragGrabDX`; neither is
refreshable by `captureDragOrigins()`, which has no cursor position to re-anchor against. Two
gestures cannot own the same state at once, so **a wheel tick while a press gesture is in flight ends
the press gesture** and then performs its own action with nothing in flight. The wheel keeps working
— this is not "disable the wheel during a drag" — and the press ends rather than continuing from an
anchor that predates a sound it has just been told is its own.

## 33. Implementation chronology, round 4

1. State test 73 written first, against unmodified `799113f`; legs (a), (b), (c) fail, (d), (e), (f)
   pass. §27.
2. The threshold measured before it was touched, by reproducing `kAxis` and its Fritsch–Carlson map
   exactly and evaluating half a pixel at eight frequencies. §29.
3. The decision record above written before any production edit, per the round's own gate.
4. `setBands`, `setSoloMask` and `toggleSoloBit` return `bool`; `addBandAt` and `removeBand` abandon
   the transaction on a refused mask store.
5. `mouseUp`'s two solo paths pass `pressBands` and the mask they read.
6. `mouseWheelMove` ends an in-flight press before acting.
7. `gestureX`/`gestureW` become normalised values; `ownsSplit`/`ownsWidth` compare exactly;
   `storeOwned` stores and confirms in parameter space; `kWidthQuantum` deleted.
8. Leg (g) added to make the threshold change mutation-visible — the discriminator is whether the
   drag *stops*, because a 20 Hz move at 10 kHz is under the write-suppression threshold and would
   not be overwritten either way.

## 34. The final stale-write and transaction audit

| Operation | Authoritative inputs | Cached gesture state | Ownership condition | Commit point | Reentrancy points | If ownership is lost half way |
|---|---|---|---|---|---|---|
| crossover store (`writeCrossovers`) | `freqP[k]->getValue()`, `bandCount()` | `gestureX[k]` (normalised), `dragOrigX`, `dragGrabDX` | `ownsSplit (k)`, exact | each `storeOwned` | the store's own dispatch | returns false; `mouseDrag` voids the gesture |
| width store (`mouseDrag`) | `widthP[b]->getValue()` | `gestureW[b]` (normalised), `dragGrabDY` | `ownsWidth (b)`, exact, checked before the anchor **and** the store | `storeOwned` | the store's own dispatch | `cancelActiveDrag()` |
| `setSoloMask` | `bandCount()`, `soloMask()` | `expectedBands`, `expectedMask` | both, between the gesture open and the store | the store inside the open | `beginChangeGesture` dispatch | returns false; the caller abandons |
| `toggleSoloBit` | the mask it reads | `pressBands` | as above | as above | as above | returns false; nothing else follows |
| `setBands` | `bandCount()` | `expectedBands` | between the gesture open and the store | the store inside the open | `beginChangeGesture` dispatch | returns false; it is the last store |
| `removeBand` / `addBandAt` | count, mask, `fr[]`, `wd[]` | `expectedBands` + the plan | re-proved before every store | the burst, store by store | every store's dispatch | abandons the rest; stores already issued stand (§30) |
| on-release removal | `pressBands` | `dragHandle`, `dragRemovePending` | `removeBand`'s contract | inside `removeBand` | `endGesture` before it | refused |
| neighbour spring-back | `dragOrigX` | as the crossover store | `ownsSplit` | as above | as above | as above |
| wheel refresh | live values | none — the press is ended first | n/a | n/a | its own stores | n/a |

**The invariant a future gesture consumer must follow**, stated once so it need not be reconstructed:

> Own what you observed or wrote, in the parameter's own units, compared exactly. Check ownership in
> the statement before the store, with no call between. Record what you stored only after confirming
> the parameter holds it. Report a refusal to your caller, and abandon the rest of your plan when you
> hear one. Refresh every piece of state a write depends on together, or refresh none of it.

## 35. Validation and residuals, round 4

State suite **2 632 / 0**; State tests 66–72 unchanged and green. Mutations P1–P4 each killed by
exactly one named leg. Residuals: §30's single cross-thread store, ruled a bounded trade; the
stores already issued when a transaction abandons; and the ADR-0039 disposition of the vanished-band
Width and solo-mask values, unchanged.

## 36. What was open at the start of round 5

One confirmed production defect and one reopened investigation.

| # | Location | Review finding |
|---|---|---|
| R1 | `SpectrumImager.cpp:542` | Reentrant stores corrupt coupled edits. A listener can rewrite the parameter just stored by `setSoloMask` or `setParam` (`:498-501`); neither verifies its result, so add, remove, reset and text-entry bursts continue from stale plans, and host automation can be reassigned or overwritten. |
| R2 | `SpectrumImager.cpp:638` | The cross-thread partial topology transaction, accepted as a bounded trade in §30, reopened as an investigation against the final implementation. |

R1 is the same class ADR-0040's round-3 correction and ADR-0041 closed on the **near** side of a
store — the window `beginChangeGesture` opens between a caller's check and the value going out —
reopened on the **far** side, the window the value dispatch itself opens between the store and the
next statement. The two are one call apart and were closed one round apart.

## 37. Reproduction, before any change

State test 74 drives the real editor and injects real `juce::MouseEvent`s, with the reentrancy
probes round 3 built: `EchoTheSameParameter` writes the parameter whose store is dispatching,
`WriteFromInsideAStore` writes a different one from the same window. All four legs are red on
`e247c11`, and each printed its own measurement:

| Leg | Path | Probe | Measured on `e247c11` |
|---|---|---|---|
| A | `removeBand` → `setSoloMask` | echo `mbSolo` → `0b1001` from inside the mask store | `Bands 3 with mask 0x9` — the transaction lowered the count with a word it had not remapped, and `SoloMonitor.cpp:85` then masks `0x9 & 0x7`, so the soloed top band vanishes |
| B | alt-click → `resetCrossover` | move `mbFreqMid` to 5 kHz from inside the reset's own store | `5000.0 Hz was installed and 2000.0 Hz was written over it` |
| C | text commit → `commitFreqEditor` | same probe, same window | `5000.0 Hz was installed and 2000.0 Hz was written over it` |
| D | click-to-add → `setBands` | echo `mbBands` back to 2 from inside the count store | `the count store did not stand (Bands 2) and the press still latched the add and opened 1 gesture(s) on the new split` |

Legs E–H are the positive controls — an uninterrupted delete, an uninterrupted alt-click reset, an
uninterrupted text commit and an uninterrupted add — and all four are green before the fix, so the
test discriminates the defect rather than the operation.

**Legs B and C are the more serious pair, and they are not the same defect as A and D.** A and D are
a store reporting a success it did not have (a *false claim*). B and C are a later store overwriting
a newer authoritative value (a *stale write*) — and it happens through the exact predicate ADR-0040
condemned at `SpectrumImager.cpp:314-316`: *does the live value differ from MY target?*, which a
large foreign move answers more emphatically, not less. `writeCrossovers` was converted to ownership
in round 3; `resetCrossover` and `commitFreqEditor` compute the same kind of plan, spread the same
kind of neighbours and were left on the old predicate.

Measured separately, and **not** a defect: a listener echoing a *width* slot from inside its own
store in the middle of `removeBand` (`landed=1 Bands=3 mask=0x5 wLo=1.750 …`) or of `addBandAt`
(`landed=1 Bands=3 wLo=1.750 …`). Both bursts completed their intent and left the newer width
standing. Nothing later in either plan reads that slot, so there is nothing stale to continue from;
the probes were temporary and are not kept.

## 38. The reentrancy invariant, and where the previous round stopped short

ADR-0041 already wrote the rule down:

> A conditional store **reports whether it committed**, and a caller that derived state from a
> precondition abandons the rest of its plan the moment any store does not commit.

The implementation of that rule stopped at the *precondition*. `setBands` (`:514-528`) and
`setSoloMask` (`:533-547`) set `stored = true` immediately after `setValueNotifyingHost` and never
look again, so what they return is **whether the store was issued**, not whether it committed. The
one place the round did implement the far side — `storeOwned` (`:373-382`), which stores and then
confirms the parameter holds this store's result — was applied only to `writeCrossovers` and the
width drag. So the gap is not a wrong rule; it is the rule applied to two of four store primitives.

Stated for this round, with the halves separated because the two findings are different failures:

> **P1 — no stale write.** A transaction never writes a slot whose live value is not the one its plan
> was computed from. *(Guarded by the pre-store re-reads; violated by the two neighbour spreads.)*
>
> **P2 — no false claim.** A transaction never reports success, and never takes a decision, on the
> strength of a store the parameter did not keep. *(Violated by `setBands` and `setSoloMask`.)*
>
> **P3 — no incoherent commit.** A transaction never performs a store whose meaning depends on an
> earlier store the parameter did not keep. *(The mask → count dependency; follows from P2 once the
> refusal is audible.)*

The round's proposed single form — *"a coupled transaction may continue only if every committed store
still has the value and topology the remaining plan was derived from"* — is **right for P1 and P3 and
too strong for the leaf stores**, and the measurement in §37 is why. The width and split stores
inside a topology burst are leaves: the plan for slot `k+1` is read from the entry snapshot, never
from slot `k`'s committed value, so there is no stale plan to continue from. Aborting there was
measured to leave a *worse* state than completing, because the mask is stored first and is only
correct once the count changes — an abort at a width store leaves the mask remapped under the old
count, while completing leaves the intended layout with one slot holding the newer authority's value.
So the invariant that actually holds is the dependency-shaped one: **a transaction must hear every
refusal a later store's correctness depends on, and there is exactly one such dependency here —
the solo mask before the band count.**

## 39. The store reporting contract, and the abort semantics

| Primitive | Reports | Why |
|---|---|---|
| `setSoloMask` | `bool` — **committed**, re-read after `endChangeGesture` | the count store's meaning depends on it (P3) |
| `setBands` | `bool` — **committed**, re-read after `endChangeGesture` | `addBandAt`'s caller latches `gestureBands`, `dragHandle` and a host gesture on the strength of it (P2) |
| `storeOwned` | already reports and already re-reads | unchanged |
| `setParam` | stays `void` | every caller is either a leaf of a burst (nothing reads it, §38) or a single store with nothing after it (the wheel, `resetParam`) |

Verification is in **semantic space** for the two integer parameters and in **parameter space** for
the floats, and neither can false-refuse a clean store:

* `bandCount()` and `soloMask()` both round through `std::lround` (`:189-210`), and `RawInt`
  (`PluginParameters.cpp:71-89`) stores and returns the raw normalised float, so the store's own
  round trip cannot move the integer it decodes to.
* `storeOwned` computes `expect` with the same two conversions `AudioParameterFloat` performs
  (`juce_AudioParameterFloat.cpp:97-98`), bit-identical when nothing else wrote — the ADR-0041
  argument, unchanged and already in production on two paths.

**Abort semantics: A — abort the remaining transaction.** The newer authoritative write wins and no
remaining derived store is performed. B (re-read and continue) is wrong because the remaining plan
cannot be recomputed: the whole plan — the solo remap, the width shift, the split shift and the new
count — is derived from one entry snapshot, so "recompute" means "start again", not "continue". C
(restart) is wrong because the trigger is a foreign write that may repeat: a retry is exactly the
uncontrolled loop the round's own instruction forbids, and the operation has no bounded retry model
to reuse. D is unnecessary. **A is also already the code's behaviour on the near side**, so this is
the same answer extended, not a second mechanism.

## 40. The two neighbour spreads — the P1 half

`resetCrossover` (`:605-621`) and `commitFreqEditor` (`:810-829`) compute a plan from a snapshot,
issue one bracketed store, and then spread the neighbours with

```
if (k != i && std::abs (freqToX (crossover (k)) - xs[k]) > kSplitMovedPx)
    setParam (freqP[k], …);
```

which is the predicate ADR-0040 condemned in `writeCrossovers` and quoted at `:314-316`: *does the
live value differ from MY target?* A foreign move makes it **more** true, so the plan reclaims the
newer value. Round 3 converted `writeCrossovers` to ownership and left these two alone; §37 legs B
and C are the same measurement on the two that were left.

The fix is the one already in production one function away: capture the normalised value of each
neighbour before the primary store, write a neighbour only while it still holds it, store through
`storeOwned`, and stop at the first slot that is not ours. The primary store is confirmed the same
way — after `endChangeGesture`, so a write from inside the gesture close is caught too — and the
spread does not run if it did not land, because every neighbour position was computed to make room
for it.

## 41. The cross-thread partial topology transaction, re-ruled from the final implementation

§30 accepted this as a bounded trade. The round's instruction is not to preserve that disposition
automatically, so it was re-derived from the final code. **The ruling is upheld, and the reason
changes: the argument in §30 was that there is no single write-side commit point. The stronger
reason, found this round, is that even one would not help.**

**The writer set, re-enumerated.**

| Writer | Call | Fires listeners | Thread |
|---|---|---|---|
| this class | `setValueNotifyingHost` | yes | message |
| preset load / init | `setValueNotifyingHost` (`PresetManager.cpp:188`, `:203`, `:320`, `:543`) | yes | message |
| undo / redo / A-B | `reassertParameters (…, notifyHost = true)` (`PluginProcessor.cpp:523`, `:737`) | yes | message |
| host state restore | `reassertParameters (…, notifyHost = false)` → `rp->setValue (norm)` (`PluginProcessor.cpp:740`) | **no** | message |
| **host automation (VST3)** | `processParameterChanges` → `setValueAndNotifyIfChanged` → `setValueNotifyingHost` (`juce_audio_plugin_client_VST3.cpp:3496`, `:3591`, `:833`) | yes | **audio** |
| the engine | — reads only (`AnamorphEngine.cpp:608-616`) | — | audio |

§30 already ruled that `mbBands` is written from the audio thread; this round supplies the citation
it asserted without one. JUCE's VST3 wrapper applies automation from inside
`JuceVST3Component::process` (`:3591` calls `processParameterChanges`, which reaches
`setValueNotifyingHost` at `:833`), so a host lane can move any of these parameters between two
stores of a burst on a genuinely different thread — and the listener dispatch for that write runs on
the audio thread too. A topology transaction can therefore be interrupted between any two of its
stores, by that path and by the synchronous reentrancy of §37, and the per-store checks are the
right shape for both because they compare live values without asking who moved them.

**Option B — a single conditional topology commit — is not merely unreachable, it is pointless.**
`PluginParameters::toEngine` reads the ten multiband atomics with **ten separate `load()` calls**
once per block, with no seqlock, generation or coherence guard (`PluginParameters.cpp:365-374`,
called from `PluginProcessor.cpp:186`). **The reader tears.** An atomic write-side commit would be
re-torn on the read side, so it would buy nothing without also replacing the read with a versioned
or double-buffered snapshot — which is a **threading-model change**, an Architecture Review Gate
item and an AI Agent Hard Stop, and would have to be lock-free on the audio thread. The same
argument disposes of the idea that the burst is a special weakness: `reassertParameters` restores a
whole sound the same way, one parameter at a time, and a host writing two automation lanes in one
block tears identically.

**Option C — versioned/generation — loses to the same silent writer as in §30.** The host restore
path writes `rp->setValue (norm)` and fires nothing (`PluginProcessor.cpp:740`), so a generation
driven by listeners cannot see it. A generation derived from *polling the values* is what
`ownsSplit`, `ownsWidth` and `storeOwned` already are, under another name.

**Option D — a lock — is forbidden and would not work.** `REALTIME_AUDIO_POLICY.md` puts
`mutex`/`lock`/blocking waits on the hard red line, and the audio thread is now shown to be a
*writer* of these parameters, so any lock covering the transaction would be taken there. It would
also not close the reentrancy half at all: that is same-thread, and a recursive `CriticalSection`
re-entered by a listener on the message thread excludes nobody.

**Option E — compensating rollback — is rejected on its own merits.** Restoring stores `1..k-1` to
their snapshot values writes a **stale value over a newer authority**, which is exactly what
ADR-0036 §25 forbids and what every finding in this worklog has been about. Each rollback store can
itself be refused, and can itself be interfered with, so the scheme has no deterministic
termination; and the host and undo stack would see the whole excursion.

**Option A — per-store conditional ownership — stands.** What makes it safe is not the size of the
window but what the DSP does with anything it reads:

* **Nothing it reads can be illegal.** `MultibandWidth::setCrossovers` and
  `SoloMonitor::setCrossovers` clamp every split to `[20 Hz, 0.45·sr]` and then force strict `1.1×`
  ordering, top-down and bottom-up (`MultibandWidth.cpp:102-112`, `SoloMonitor.cpp:68-77`) — the
  0.8.2 fix for exactly this class. `setBandCount` clamps to `[1,4]`. `SoloMonitor::process` masks
  the word with `(1 << bands) - 1` (`SoloMonitor.cpp:85`) and holds the gains above the count at 0
  (`:96`).
* **Only the valid prefix is used.** `MultibandWidth.h:53-56`: `(bandCount - 1)` crossovers and
  `bandCount` widths. Values retained in the slots above are inert while the count is low, and
  become observable only when a later, legitimate operation raises the count — the ADR-0039
  disposition, unchanged.
* **Every continuous quantity is smoothed, so a microsecond-lived intermediate never arrives.**
  Crossovers are *targets* eased under a ~4 oct/s rate cap or a single bank crossfade (ADR-0015,
  `MultibandWidth.h:91-94`); widths glide one-pole ~20 ms (`MultibandWidth.h:64-68`); the solo gains
  are `SmoothedValue` crossfades (`SoloMonitor.cpp:94-96`). A partial layout that exists for the
  handful of stores between two statements on the message thread contributes a negligible increment
  to a smoother that needs tens of milliseconds to travel.
* **The one discontinuous quantity is stored last.** `mbBands` is a structural change routed
  through the engine's silent switch-duck with a reset (`AnamorphEngine.cpp:302-306`, `:856`), and
  both bursts write it **after** everything else. So the partial layout always lives inside the
  smoothers and never inside a count change. **This is why the store order is kept**, and it is now
  a measured reason rather than an unexamined default: moving the mask store later would shorten the
  window in which the mask is remapped under the old count by roughly five stores, but the solo
  gains are crossfaded so that window is already inaudible, while moving it would make an abort at
  any earlier store *more* costly — with the mask first, an abort at the very first store applies
  nothing at all.

**Worst case, stated concretely.** Four bands, mask `0b1010`, widths `[0.5, 1.5, 1.0, 1.0]`, splits
`[200, 2000, 10000]`. The user clicks the delete x on band 0. The burst writes `mbSolo = 0b0101`;
between that store and the next, a VST3 automation lane writes `mbFreqMid`. The width loop's
`! juce::exactlyEqual (bandWidth (k), wd[k])` guard does not see a *split* change, so the widths are
written; the split loop's guard does, at `k = 1`, and the burst returns. The layout that stands is
`Bands = 4`, mask `0b0101`, widths `[1.5, 1.0, 1.0, 1.0]`, split 0 rewritten, split 1 the host's.
The user sees bands 1 and 2 soloed instead of 1 and 3, and the wrong widths on bands 0–2, until the
next edit. **Nothing is unsafe** — every value is in range, the order is enforced by the DSP, the
count is untouched, no NaN is reachable — and the user's next click resolves it. That is the trade,
and it is the same trade any host writing two automation lanes in one block already takes.

**What would justify reopening it.** A coherent read on the audio side — a seqlock or a
double-buffered `EngineParameters` snapshot published with release/acquire — would make write-side
atomicity worth having, and only then. That is a threading-model change and belongs to the
Architecture Review Gate, not to a review round. The trigger to raise it would be evidence that a
torn topology is *audible*: a report of a wrong band being soloed or a wrong width being applied for
longer than a smoother's travel, or a DSP change that removes one of the smoothers above.

## 42. Implementation chronology, round 5

1. State test 74 written first, against `e247c11`, with the four legs red and their positive controls
   green. §37 is its measurement, quoted from the run.
2. Two temporary probes measured the `setParam`-in-a-burst case that the review names but that turned
   out not to be a defect (`removeBand` and `addBandAt` under a width echo); recorded in §37 and
   removed.
3. The decision record above written before any production edit, per the round's own gate.
4. `setBands` returns `stored && bandCount() == want`; `setSoloMask` returns
   `stored && soloMask() == mask`; both re-read after `endChangeGesture`.
5. `spreadSplits` added; `resetCrossover` and `commitFreqEditor` capture `was[]` before their primary
   store, confirm that store, and spread through the helper.
6. Legs I and J added — the primary-store confirmation is not observable through legs A–D, and a
   fix without a killing mutation is not a fix. Each leg is two halves so that "the neighbour did not
   move" cannot pass because the spread never moves anything: half (i) proves the push exists.
7. Leg J's threshold retuned from 250 Hz to 225 Hz after the first run measured the real push as
   `200 -> 249.8 Hz` on this axis. The measurement, not the guess, sets the bound.

## 43. The final transaction audit, round 5

Every multi-parameter operation this work touches, stated in the form §15 of the round's brief asks
for.

| Operation | Authoritative snapshot | Derived plan | Stores, in order | Synchronous callback points | Ownership check | If ownership is lost | Remaining plan | Stores already issued | Can a newer value be overwritten afterwards? |
|---|---|---|---|---|---|---|---|---|---|
| `setParam` | none | the caller's | one | the store's own dispatch | none — by design (§38) | n/a | n/a | n/a | no: no caller writes the same slot twice |
| `setBands` | `bandCount()` | `want = jlimit (1,4,n)` | one, inside a gesture | the gesture open, the value dispatch, the gesture close | `expectedBands` between open and store; `bandCount() == want` after close | returns false | the caller's | this store | no |
| `setSoloMask` | `bandCount()`, `soloMask()` | `mask & 0x0F` | one, inside a gesture | as `setBands` | `expectedBands`/`expectedMask` between open and store; `soloMask() == mask` after close | returns false | the caller's | this store | no |
| `toggleSoloBit` | the word it reads | `m ^ (1 << b)` | via `setSoloMask` | as above | as above | returns false | nothing follows | as above | no |
| `storeOwned` | the caller's | one value | one | the store's own dispatch | read-back `== expect`, exact | returns false | the caller's | this store | no |
| `writeCrossovers` | `freqP[k]->getValue()`, `bandCount()` | `xs[]` | up to 3 | each store's dispatch | count + `ownsSplit (k)` before each; `storeOwned` after | returns false | abandoned | stand | no |
| `spreadSplits` | `freqP[k]->getValue()` at plan time (`was[]`), and the split count | `xs[]` from `projectGaps` | up to 2 | each store's dispatch | `bandCount() - 1 == count` **and** `getValue() == was[k]` before each; `storeOwned` after | returns false | abandoned | stand | **no — this is the round's P1 fix** |
| `resetCrossover` | live splits + `was[]` | default for `i`, `projectGaps` for the rest | 1 primary + `spreadSplits` | the gesture open/close, each store's dispatch | primary confirmed after its gesture closes; then `spreadSplits` | returns before spreading | abandoned | the primary stands | no |
| `commitFreqEditor` | live splits + `was[]` | parsed value, `projectGaps` for the rest | 1 primary + `spreadSplits` | as `resetCrossover` | as `resetCrossover` | skips the spread; still closes the editor | abandoned | the primary stands | no |
| `addBandAt` | `bandCount()`, `soloMask()`, `fr[]`, `wd[]` | remapped mask, shifted widths, shifted splits, `N + 1` | mask, ≤5 widths, ≤3 splits, count | every store's dispatch | count + target re-proved before each, splits **exactly, in parameter space** since §45; mask and count also confirmed after | returns −1 | abandoned | stand (§41) | no — every store's target is re-proved against the snapshot immediately before it |
| `removeBand` | as `addBandAt` | remapped mask, shifted widths, shifted splits, `N − 1` | mask, ≤3 widths, ≤2 splits, count | every store's dispatch | as `addBandAt`; the final count store's result is discarded because nothing follows it | returns | abandoned | stand (§41) | no |
| `resetParam` | none | the parameter's default | one, inside a gesture | the gesture open/close, the value dispatch | none — nothing follows it | n/a | n/a | n/a | no |
| wheel width step | live width | `bandWidth + step` | one | the store's dispatch | none — the press was ended first | n/a | n/a | n/a | no |
| `mouseUp` solo click | `pressBands`, the mask it reads | one word | via `setSoloMask` | as `setSoloMask` | both, and now the far side | nothing is written | nothing follows | none | no |
| state / preset / A-B | outside this class | outside | outside | — | — | — | — | — | these are the **newer authority** the checks above defer to |

**No stale transaction can continue because a primitive returned normally**, because the only
primitives that return normally without proving their result are the ones nothing reads afterwards:
`setParam` at a burst leaf, `resetParam`, and the wheel's width step. Every other store either proves
its own result (`storeOwned`, `setBands`, `setSoloMask`) or is immediately preceded by a re-proof of
the exact value its plan assumed.

## 44. Validation and residuals, round 5

State suite **2 673 / 0**; DSP **396 / 0**; State tests 66–73 unchanged and green. Mutations M1–M9
each killed by exactly the intended leg (M6 kills State test 73 leg (c) as well as State test 74 leg
A, which is right: one `if` guards both windows; M8 and M9 come from §45's pass over the shipped
fix). `check-realtime` 47/0 with its self-test 93/93,
`check-portability` 57/0 with 120/120, `check-docs` 128 clean with 464/464, `check-citations` clean
against both bases with self-test 139/139, `git diff --check` clean, `preflight.sh` exit 0.

**Residuals, unchanged and each with its evidence:** §41's partial topology application, ruled a
bounded trade with the reopening trigger named; the stores already issued when a transaction
abandons; and the ADR-0039 disposition of a vanished band's Width and solo bit, which are inert while
hidden (`SoloMonitor.cpp:85`, `MultibandWidth.h:53-56`) and exact when the count returns.

## 45. The adversarial pass over the shipped round-5 code

The same practice as round 3: once the fix was in the tree and green, a read-only fan-out re-derived
every store site from the shipped code rather than from the design. It found two more, both real,
both closed here, and both the *same rule applied inconsistently* rather than a new class.

**D1 — `spreadSplits` re-proved the values but not the COUNT.** `writeCrossovers` has re-proved
`bandCount()` before every store since ADR-0040's round-3 correction 1; the new helper did not.
`was[k]` cannot stand in for it: `mbBands` is a different parameter, so a host lowering the count
from inside the primary store leaves every split holding exactly what it held, and the plan — made
for the old count — is applied anyway. On a lowering the writes land on splits the new topology does
not use, so they are DSP-inert (`MultibandWidth.h:53-56`) but reach the host: an automation lane and
the undo stack record split moves the user never made, and they become live if the count returns.
Closed by re-proving `bandCount() - 1 == count` at the top of each iteration. State test 74 leg K;
mutation M8.

**D2 — `addBandAt` owned its splits in PIXELS.** ADR-0041 ruled ownership a parameter question and
converted the gesture paths; `removeBand` has compared `juce::exactlyEqual (crossover (k), fr[k])`
since ADR-0040. `addBandAt`'s split guard was still
`std::abs (freqToX (crossover (i)) - xs[i]) > kSplitMovedPx`, and half a display pixel is 32 Hz at
10 kHz (§29's table) — so a fully representable, fully automatable host move that size read as
"unchanged" and the burst wrote its own plan over it. This is precisely the defect ADR-0041 named,
surviving in the one guard that round did not convert. Closed by capturing `fr[]` alongside `xs[]`
and comparing exactly. State test 74 leg L; mutation M9.

**Examined and NOT changed, with the reason:**

* **The coupled plan can still be applied in part.** `projectGaps` is a chain — `xs[k]` is a function
  of the *other* slots' snapshot — so proving slot `k` in isolation does not prove the plan *for*
  slot `k` is still valid, and an abort part-way can leave the splits out of order on screen. This is
  the same accepted trade as §41, arrived at from the other direction: the alternatives are to
  overwrite a newer authority (forbidden) or to commit atomically (unavailable). The DSP is unaffected
  — `MultibandWidth::setCrossovers` and `SoloMonitor::setCrossovers` force `1.1×` ordering on whatever
  they read — so the residual is a display artefact until the next edit.
* **`resetCrossover` writes a pixel round trip of the default rather than `getDefaultValue()`**
  (`freqToX` out, `xToFreq` back through a 30-iteration bisection). About 1e-6 Hz at 180 Hz, and
  nothing on any path compares these parameters to their default exactly. Inert; recorded, not
  changed.
* **`resetCrossover` does not refresh `gestureX`.** `mouseDown` latches the gesture snapshot at the
  top for every branch, and the alt-click branch then resets a split without updating it — so
  `soundMovedUnderGesture()` reports true for the slots the reset moved. It is harmless only because
  that branch latches no identifier, so `cancelActiveDrag` takes its early return. Pre-existing,
  unchanged by this round, recorded as a residual rather than fixed inside a round about stores.
* **The neighbour stores are outside any change-gesture bracket**, matching `writeCrossovers` and the
  drag path, which bracket only the split the user is holding. Consistent with the existing design;
  an observation, not a defect.
* **`kSplitMovedPx` inside `spreadSplits` is correct.** It now runs strictly *after* the exact
  ownership compare and `continue`s rather than storing, so it does only the job ADR-0041 left it —
  deciding whether a write is worth making on a slot already proved to be ours. It cannot mistake a
  foreign value for a small delta at any magnitude.

**Two more, both small and both closed:**

* **A comment that told the reader the opposite of the code.** The paragraph above `ownsSplit` still
  described a split as owned "within the same half pixel `writeCrossovers` uses to decide a write is
  worth making" — the rule ADR-0041 removed, sitting directly above the ADR-0041 paragraph that
  replaced it. `CLAUDE.md`'s drift rule applies to comments as much as to documents, and this is
  exactly the misreading ADR-0041 was written to end. Rewritten to state what the code does.
* **The Alt solo branch decided from a second read of `mbSolo`.** `const int m = soloMask();` latches
  the word passed as `expectedMask`, and the branch then called `bandSoloed (soloPressBand)`, which
  re-reads it. `mbSolo` is a `std::atomic<float>` a host can write from another thread at any
  instant, so the word the branch chose from and the word the store names as its precondition were
  not guaranteed to be the same one — "two reads where the rule needs one", which is what this whole
  series has been about. It is unreachable single-threaded (nothing dispatches between the two
  statements) and needs two host writes inside a two-statement window to matter, so it carries no
  test; the fix is to decide from `m`, which is strictly one read.

**Recorded and deliberately not changed:** `writeCrossovers` indexes `freqP[k]` on the caller's
`count` without the `k < std::size (freqP)` bound its new sibling `spreadSplits` carries. Both
callers derive `count` from `bandCount()`, which is `jlimit (1, 4, …)`, so it is unreachable on this
head; the asymmetry is this round's, and it is noted here rather than closed by widening a function
the round did not otherwise touch.

One correction to this round's own ground facts, from the same pass: in this tree the JUCE parameter
sources are under `modules/juce_audio_processors_headless/processors/`, not
`modules/juce_audio_processors/processors/`. Every line number cited in this worklog and in ADR-0042
is correct in the headless copy.
