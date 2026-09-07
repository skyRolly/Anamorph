# ADR-0038 — A SpectrumImager gesture is void once the band topology it was defined against moves

**Status:** Accepted (maintainer instruction 2026-09-07: audit SpectrumImager gesture state
invalidation across topology changes and decide between local validation, centralized invalidation,
or another design — recorded here as **centralized, plus one primitive made safe**).

**Extends the principle of [ADR-0036](ADR-0036-program-state-ownership.md) §25** from the processor
to the GUI gesture layer: an obsolete writer must not reassert over a newer authority. Nothing in
ADR-0036 or [ADR-0037](ADR-0037-legacy-ab-slot-baseline-at-the-boundary.md) is superseded — no
ownership, publication, generation, lock or ordering rule changes, and the audio thread is not
touched.

**Not an Architecture Review Gate item.** No parameter ID is renamed or removed, no serialization
field is added or re-interpreted, no thread or cross-thread path is created, no DSP order or reported
latency moves. It changes what a GUI gesture does when the band count moves under it.

## Context

`SpectrumImager` caches a dozen identifiers for the duration of a mouse gesture — `dragHandle`,
`dragBand`, `soloPressBand`, `soloMoveLeft`/`soloMoveRight`, `pressDeleteBand`, `dragOrigX[]` and the
pixel anchors — and every one of them names a split or a band **by position**, latched when the press
begins. `bandCount()` is a live read of the automatable `mbBands` parameter, so those names can stop
meaning what they meant at any instant.

Three rounds of review fixed three consequences one consumer at a time: `projectFromOrig` reading
past its copied prefix, `dragOrigX` seeded only for the splits in use, and `mouseUp` forwarding a
stale handle into `removeBand`. Review then found two more, and the audit
(`worklogs/SPECTRUMIMAGER_GESTURE_TOPOLOGY_AUDIT_v0.9.8.md`) found two beyond those.

## Problem

Two distinct failures, and they need different medicine.

**Write-through.** A consumer that validates part of its input still writes the rest. `moveBand`
validates its *pins* against the live count, so a fallen count makes `projectFromOrig` return early —
with `out[]` still holding the drag-start positions it copied from `dragOrigX`. `writeCrossovers`
then compares those against the live crossovers and writes the ones that differ. When the topology
change was a **restore**, which moves the crossover values as well as the count, the gesture writes
its pre-drag positions straight over the restored ones. Measured: `the restored split 900.0 Hz was
overwritten with 200.0 Hz`.

**Retargeting under a race.** `removeBand` clamped an out-of-range index into the live range. A
caller-side liveness check cannot close that, because `bandCount()` is a live read and the count can
move between the check and `removeBand`'s own read of it. A check never closes a race.

The topology-change paths divide exactly along that line. Click-to-add, the delete x, preset load,
A/B apply and undo are all **message-thread**, so a check at handler entry is exact — the mutator
would have to run on the same thread. Host automation on `mbBands` and the sound half of
`setStateInformation` are **not**, and those are the two that race.

## Options

1. **Local validation at every consumer.** The status quo after PR #143. Rejected on evidence: it is
   what missed the write-through — `dragCrossoverTo` validates and returns, so it was already safe,
   while `moveBand` validates and writes anyway — and it scales with consumers rather than with
   invariants. It cannot close the race at all.
2. **Centralized invalidation.** Snapshot the topology when a press begins; void the gesture the
   moment the live count differs. One rule covering every identifier, because they all share one
   premise. Exact for the message-thread paths, open for the two that race.
3. **Centralized invalidation, plus making the racy primitive safe.** As 2, and `removeBand` refuses
   a non-live index rather than clamping it into a live one.
4. **A lock around the gesture.** Rejected: `mbBands` is written from the audio thread, so this is a
   Thread Model change and forbidden by `REALTIME_AUDIO_POLICY`.
5. **Marshal parameter writes onto the message thread.** Rejected: far larger than the defect
   warrants, and ADR-0036 deliberately chose snapshot exchange over marshalling.
6. **Poll in the editor's 24 Hz reconcile.** Rejected: 42 ms of continued writing, and the event
   handlers are the exact points at which the stale state is used.

## Decision

**Option 3.** A gesture is defined against the topology it began in, and is **void** the moment that
topology moves:

* `mouseDown` snapshots `bandCount()` into `gestureBands`, once, at the top — every branch below is
  covered by the one snapshot.
* `mouseDrag` and `mouseUp` begin with `if (topologyMovedUnderGesture()) { cancelActiveDrag(); … }`.
  `cancelActiveDrag()` already existed for a release lost outside the window and does exactly the
  right thing: close every open parameter gesture, clear every flag, fire **no** on-release action.
* `removeBand` **refuses** an index outside `[0, bandCount())` instead of clamping it.

Neither half is sufficient alone. Without the refusal the wrong-target outcome stays reachable inside
the window; without the invalidation the write-through, the rise and the unrelated-band cases all
stand.

Message thread only. No lock, no allocation, no blocking, nothing added to any audio path — the
predicate is a comparison of two ints.

## Consequences

**A gesture now stops where it used to continue.** If the host moves Bands mid-drag, the drag ends
rather than carrying on against the new topology. This is the intended behaviour and it is the same
rule ADR-0036 §25 applies to the processor. It **supersedes what State test 66 legs A–D asserted** —
they were written to prove the gesture continued *correctly* across a rise; they now prove it stops.
Leg A's liveness check inverted, with the reason recorded in the test.

**`captureDragOrigins()` becomes defence in depth.** With the gesture voided on any change,
`moveBand`'s live count always equals the snapshot, so the slots beyond it are never read. Kept as
the second layer, not removed.

**The refusal has no reachable test, and that is recorded rather than dressed up.** The guard returns
from `mouseUp` before `removeBand` is called on every path a single-threaded suite can build.
Measured: restoring the clamp *and* deleting the call-site guard leaves all 2544 checks green. Its
justification is the code path, not a mutation proof.

**Two residues stay open, both outside gesture lifetime.** A vanished band's own Width and its solo
mask bit are still writable — but only with no gesture in flight, since the guard now voids one
first; they are Bands/parameter coherence. And a restore that changes the crossover *values* while
leaving Bands unchanged does not move the snapshot; closing that needs a value-level signal (the drag
comparing the live crossovers against what it last wrote, or the imager observing `soundSetGen`), and
no finding in this round claims it.

## Related code

* `src/gui/SpectrumImager.h` — `gestureBands`, `topologyMovedUnderGesture()`
* `src/gui/SpectrumImager.cpp` — the snapshot in `mouseDown`, the guard in `mouseDrag` / `mouseUp`,
  the refusal in `removeBand`, `cancelActiveDrag()`
* `tests/state_tests.cpp` — State test 68 (this decision), State tests 66 and 67 (its predecessors)

## Evidence + confidence

**Verified.** State test 68 legs A, C and E fail against `aa55f20` and pass after; leg B passed
before and after, which is the measurement that rejects option 1. Mutation: deleting the centralized
guard fails 3 checks across State tests 66 and 68, printing `the restored split 900.0 Hz was
overwritten with 200.0 Hz`. Inverted mutation: deleting the call-site guard while keeping the refusal
leaves everything green. Full state suite 2 544 / 0; DSP 396 / 0; valgrind memcheck 0 errors;
ThreadSanitizer clean; `check-realtime` 0 violations. Investigation record:
`worklogs/SPECTRUMIMAGER_GESTURE_TOPOLOGY_AUDIT_v0.9.8.md`.
