# ADR-0039 — A gesture owns the world it was latched in: the topology it created, and no sound but its own

**Status:** Accepted (maintainer instruction 2026-09-07: close two SpectrumImager review findings —
the concurrent `removeBand` retarget and the newly added split that stopped dragging — decide the
correct invalidation invariant, and rule on the value-only restore and Bands/parameter coherence
follow-ups).

**Amends [ADR-0038](ADR-0038-gesture-state-is-void-once-topology-moves.md); supersedes nothing.**
ADR-0038 decided *centralized invalidation plus one primitive made safe*, and that decision stands.
This ADR sharpens two things it left imprecise and adds one it did not cover:

1. **When** the snapshot is taken — with the gesture's identifiers, not at handler entry.
2. **What** the safe primitive checks — the topology the caller validated against, not a range.
3. **What** counts as the gesture's world — the sound as well as the count.

The amendment pattern follows `ADR_POLICY.md` rule 4 and the precedent of ADR-0028 (amended by
ADR-0033) and ADR-0031 (amended in part by ADR-0032): a refinement adds an ADR and cross-links; only
a reversal marks the old record `Superseded`. Nothing in ADR-0038's Decision is reversed here.

**Not an Architecture Review Gate item.** No parameter ID is renamed or removed, no serialization
field is added or re-interpreted, no thread or cross-thread path is created, no DSP order or reported
latency moves, and no Accepted ADR is contradicted. Two private member functions change signature.

## Context

ADR-0038 put one snapshot, `gestureBands`, at the top of `mouseDown` and made `mouseDrag`/`mouseUp`
void the whole gesture when the live count differed. Review returned with a finding from each
direction of that guard.

**Too broad.** The add-area branch of `mouseDown` *creates* a split and hands the press to it:
`addBandAt` raises `mbBands`, then `dragHandle = idx` is latched. A snapshot taken at the top of the
handler therefore named a topology the press itself had already left, so the first `mouseDrag`
compared 3 against 2, called the gesture void and cancelled it. Click-to-add-and-drag — one of the
imager's primary interactions — stopped following the cursor the instant the split appeared.

**Too narrow.** `bandCount()` is a live read, and the release path reads it more than once: the guard
at the top of `mouseUp`, then the outward-drag liveness check, then `removeBand` itself. Between the
second and the third there is a real window — `endGesture` on the dragged split runs first, and JUCE
dispatches `AudioProcessorParameter::Listener::parameterGestureChanged` **synchronously** from
`endChangeGesture` (`juce_AudioProcessorParameter.cpp:101`), so a host recording automation, a
control surface, or the sound half of a restore can move `mbBands` from inside it.

Separately, ADR-0038 recorded two residuals. Both are ruled on here.

## Problem

**F1 — a range check is not an identity check.** ADR-0038 made `removeBand` refuse an out-of-range
index. That stops a stale index landing *outside* the live range; it does nothing when the index
lands *inside* it. Two bands, drag split 0 outside, the count rises to four inside `mouseUp`:
`removeBand (1)` was in range, so it merged bands out of a four-band layout the user had never seen
and left Bands at 3. **Measured:** State test 69 leg (c) prints `Bands 4 -> 3: the release read a
count the check never saw`, and the same release rewrote `mbFreqMid`, `mbFreqHigh`, `mbWidthLow` and
`mbWidthMid`.

**F2 — the gesture's own topology change read as somebody else's.** Measured: State test 69 leg (a)
prints `the new split stayed at 1392.1 Hz (from 1392.1 Hz): the press's own add voided its gesture`.

**F3 — a whole sound can be installed at an unchanged count.** `gestureBands` watches `mbBands` and
nothing else. A restore, preset, A/B apply, undo or automation lane that changes the crossover
*values* while leaving the count alone moves nothing the gesture watches, so the drag continues —
and it continues from `dragOrigX`, the positions of the sound that was just replaced.
`projectFromOrig` pulls every unpinned split toward those stale origins and `writeCrossovers` pushes
the difference to the host, inside the change gesture the drag opened. **Measured:** State test 70
leg (a) prints `the restored split 15000.0 Hz was pulled back to 10000.0 Hz by a drag that never
named it`, and leg (b) reproduces it through the band-move consumer.

**F4 — a vanished band's Width and solo bit stay writable.** Recorded by ADR-0038 as a residual and
ruled on below.

## Options

1. **Leave `gestureBands` at the top of `mouseDown` and special-case the add branch's consumers.**
   Rejected: it is the per-consumer validation ADR-0038 already rejected on measurement, re-created
   one level up.
2. **Ask who moved the count** — a "self-caused" flag set around `addBandAt`. Rejected: bookkeeping
   that has to be maintained at every future mutation site, and it answers the wrong question. See
   the Decision.
3. **Keep the caller-side liveness check and tighten it.** Rejected **by measurement**: mutation M7
   restores the round-3 check `dragHandle < bandCount() - 1` *and* removes the contract, and State
   test 69 leg (c) still fails. A caller-side check cannot close a window that opens after it.
4. **A lock around the topology read and the removal.** Rejected exactly as in ADR-0038: `mbBands` is
   written from the audio thread, so this is forbidden by `REALTIME_AUDIO_POLICY.md` and would be a
   Thread Model change.
5. **Inject the processor's `soundSetGen` into the imager** for F3, as `setAnimationSource` injects
   the animation flag. Rejected on coverage, not on cost: `soundSetGen` counts **wholesale sound
   replacements** (`PluginProcessor.h:267`), so it would miss a single automated crossover and an
   undo step, and it would add a cross-component dependency to close a defect the class can already
   detect from the parameters it owns.
6. **Re-seed `dragOrigX` when the sound moves**, so the drag continues against the new sound.
   Rejected: `dragOrigX` *is* the spring-back behaviour (#8–#11) — re-seeding it would make a pushed
   neighbour adopt its pushed position as its home and never spring back. Voiding is also the answer
   ADR-0038 already gives when a restore moves the count, and a restore should not behave differently
   because of the incidental fact that its band count matches.
7. **Clear or remap the hidden Width/solo values when Bands falls** (F4). Not taken: see the
   Decision. Both consumers already mask them, and changing what those parameters mean across a
   count change is a Parameter Registry / Serialization Registry semantics change — an Architecture
   Review Gate item, not a bug-fix-round decision.

## Decision

**The invariant.** A gesture is defined against the topology **and the sound** in force at the moment
its **identifiers were latched**, and it is void the moment either differs.

That phrasing is what settles option 2 without any bookkeeping. A topology change the press performs
is synchronous, on the message thread, inside `mouseDown`, and **complete before the identifiers
exist**; every other change is observed after they exist. The two classes are separated **by
construction**, so "authoritative" needs no attribution: it means *observed after the identifiers
were latched*. `mouseDown` takes the snapshot at the top for every branch, and the add branch retakes
it next to `dragHandle = idx`, from the count `addBandAt` itself established rather than from a
second, later read.

**The primitive.** `removeBand (int b, int expectedBands)` reads the live count **once** and refuses
unless it equals `expectedBands` and `b` is in range. Callers pass the topology the press was made
in. The check and the operation then share a single read, which is the only shape that holds when the
count can move between two of them. The round-3 caller-side liveness check is **removed rather than
doubled up**: `removeBand` owns the decision, so there is one place that decides whether a removal is
legitimate. `addBandAt (float hz, int& resultingBands)` reports the count it established for the same
reason — one read, not two.

**The sound half.** `gestureX[3]` records where the gesture last left each split, seeded at every
gesture start (through `captureDragOrigins`) and refreshed after every write the gesture makes
(through `writeCrossovers`, by reading **back** through the same conversion the write went out
through). `soundMovedUnderGesture()` reports a difference larger than `kSplitMovedPx` — the same
0.5 px `writeCrossovers` uses to decide a write is worth making, now a named constant so the two
cannot drift. Self-comparison is exact here because the imager and outside writers are the only
writers of these three parameters: the engine only reads them (`AnamorphEngine.cpp:609`, `:616`).

**F4 — the retained Width and solo values are preserved, deliberately.** Both consumers already mask
to the live count: `SoloMonitor.cpp:85` computes `mask & ((1 << bands) - 1)` and `:96` holds every
band above the count at gain 0 *"so a band-count change settles cleanly"*; `MultibandWidth.h:53-56`
states *"Only (bandCount - 1) crossovers and bandCount widths are used"*. So a hidden value is inert
while hidden and exact when the count returns — and the imager's own edits remap it (`addBandAt` and
`removeBand` both rewrite the width array and the solo mask for the new numbering). Clearing it would
change what `mbWidth*` and `mbSolo` mean across a count change, which is a gated Parameter/
Serialization semantics decision, and would lose the user's settings on an automation dip. **No
change; recorded as intentional.**

Message thread only. No lock, no allocation, no blocking, no wait; the audio thread is untouched.

## Consequences

- **Click-to-add-and-drag works again.** It was broken between `b65ce4e` and this change.
- **A removal is refused when the topology moved inside `mouseUp`.** The user's release does nothing
  instead of merging bands of a layout they never saw. Doing nothing is the conservative answer, and
  the same one a release lost outside the window already gives.
  **This consequence covered only the COUNT half until 2026-09-09, and the sentence read as though it
  covered both.** The mechanism it rests on is `removeBand`'s `expectedBands`, which compares counts;
  a different layout at the SAME count reached the removal untouched, because `mouseUp` cleared
  `gestureBands` before the `endGesture` dispatch that lets one in and `gestureIsStale()` answers
  `false` unconditionally in that state. The sound half is closed by
  [ADR-0050](ADR-0050-a-gestures-ownership-ends-with-its-last-on-release-action.md), and State test 69
  legs C and G are the two halves.
- **A drag stops when an authoritative sound change lands under it**, at any band count. Previously
  it stopped only when the count moved. A user holding a split while a preset loads now loses the
  grip — which is the point: the sound they were editing no longer exists.
- **A false positive would cancel a live drag.** Bounded by construction: the comparison is against
  the drag's own read-back, at the same threshold the write path uses, so nothing the gesture itself
  does can trip it (mutation M4 proves the refresh is load-bearing — State test 70 leg (d) fails
  without it). The only remaining tripwire is a host that echoes a *changed* value back, which is an
  external change and correctly voids.
- **`cancelActiveDrag()` clears `gestureBands` before its cheap exit.** The snapshot is taken on
  branches that latch no identifier (an Alt-click reset, an add the count refused), and the early
  return used to leave it set with nothing in flight.
- **Two private signatures change.** `addBandAt` and `removeBand` are private; no public or
  serialized surface moves.

**What has no reachable test, recorded rather than dressed up.** `addBandAt` reporting `N + 1` from
its own read rather than a second `bandCount()`, and `cancelActiveDrag` clearing the snapshot before
its early return, are both invisible to a single-threaded suite: mutation M5 (the second of these)
leaves all 2 574 checks green. Their justification is the window they close, not a mutation proof.

## Related code

- `src/gui/SpectrumImager.h` — `gestureBands`, `gestureX[3]`, `topologyMovedUnderGesture()`,
  `soundMovedUnderGesture()`, `gestureIsStale()`, the `addBandAt`/`removeBand` signatures.
- `src/gui/SpectrumImager.cpp` — `kSplitMovedPx`, `writeCrossovers`, `captureDragOrigins`,
  `captureGestureSound`, `soundMovedUnderGesture`, `addBandAt`, `removeBand`, `mouseDown`,
  `mouseDrag`, `mouseUp`, `cancelActiveDrag`.
- `tests/state_tests.cpp` — State test 69 (`testGestureOwnsTheTopologyItCreated`), State test 70
  (`testAuthoritativeSoundChangeVoidsAGesture`).
- `src/dsp/SoloMonitor.cpp`, `src/dsp/MultibandWidth.h` — the masking that makes F4 inert.

## Evidence + confidence

**Verified.** All three findings reproduced against `b65ce4e` before any change and green after:
State test 69 leg (a) and leg (c), State test 70 legs (a) and (b). Mutation results — **M1** (drop
the topology contract) kills leg (c); **M2** (drop the add branch's re-snapshot) kills leg (a);
**M3** (drop the sound half of the predicate) kills State test 70 legs (a) and (b); **M4** (stop
recording what the gesture wrote) kills State test 70 leg (d); **M6** (the whole pre-ADR-0038
`removeBand`, clamp and all) kills leg (c) — a mutation the previous round could **not** catch;
**M7** (the round-3 caller-side liveness check instead of the contract) also kills leg (c), which is
the measurement that rejects option 3. **M5** is not caught, and is recorded above as such.
Positive controls held throughout: State test 69 legs (d), (e) and (f), State test 70 legs (c) and
(d), and State test 67 leg (c).

**Confidence: high** for F1–F3 (each reproduced, fixed and mutation-proved) and **high** for F4's
disposition (both consumers' masking read directly from the DSP sources). The residual is stated in
the worklog: a change landing between `removeBand`'s single read and its writes is a plain concurrent
write, not a stale-identifier retarget, and no lock-free design can exclude it.
