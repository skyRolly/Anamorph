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
