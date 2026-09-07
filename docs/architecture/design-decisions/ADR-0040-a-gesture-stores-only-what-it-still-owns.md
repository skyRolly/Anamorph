# ADR-0040 — A gesture stores only what it still owns, checked adjacent to the store and recorded at it

**Status:** Accepted (maintainer instruction 2026-09-07: three SpectrumImager write-ownership review
findings — width changes overwritten, concurrent crossover changes reclaimed, concurrent band changes
overwritten — with the instruction to determine the common invariant before patching any of them).

**Completes [ADR-0038](ADR-0038-gesture-state-is-void-once-topology-moves.md) and
[ADR-0039](ADR-0039-a-gesture-owns-the-world-it-was-latched-in.md); supersedes neither.** Those two
decided *when a gesture is void*. Both put the decision at **handler entry**, which is the right place
for a change that arrives between two mouse events and the wrong place for one that arrives between
two stores of the same burst. This ADR moves the decision to the **store**.

**Not an Architecture Review Gate item.** No parameter ID is renamed or removed, no serialization
field is added or re-interpreted, no thread or cross-thread path is created, no DSP order or reported
latency moves, and no Accepted ADR is contradicted. Three private member functions change return
type; one private array is added.

## Context

`gestureIsStale()` is evaluated once, at the top of `mouseDrag` and `mouseUp`
(`SpectrumImager.cpp:1776`, `:1824`), and then up to seven parameter stores follow it with no
re-validation. That is safe only if nothing can run between the check and the stores. Something can.

```
juce_AudioProcessorParameter.cpp:59-63   setValueNotifyingHost: setValue(v); sendValueChangedMessageToListeners(v);
juce_AudioProcessorParameter.cpp:111-121 ... dispatches every listener SYNCHRONOUSLY, on this thread
juce_AudioProcessor.cpp:1467-1475        ParameterChangeForwarder -> AudioProcessorListener::audioProcessorParameterChanged
```

`ParameterChangeForwarder` is attached to every parameter, and every format wrapper registers an
`AudioProcessorListener` behind it (`juce_audio_plugin_client_VST2.cpp:263-266`, `..._AU_1.mm:188`,
`..._AUv3.mm:236`, `..._LV2.cpp:135`). **Every `setParam` the imager makes therefore reaches the host
inside the call**, and a host that writes back re-enters on the message thread before `setParam`
returns. On VST3 the concrete return path is
`performEdit → IEditController::setParamNormalized → Param::setNormalized → setValueNotifyingHost`,
gated by `if (! owner.vst3IsPlaying)` so it is reachable with the transport stopped; AU has no such
gate.

No first-party listener writes a multiband parameter — the four in `src/` bump counters or count
gestures (`PluginProcessor.cpp:308-317`, `:814-825`, `PluginProcessor.h:312-318`) — and the engine
only reads these parameters (`AnamorphEngine.cpp:609`, `:616`). Every writer is therefore the imager
itself, the host, or one of the repository's own whole-sound replacement paths.

## Problem

**W1 — width changes are overwritten.** `gestureX` records splits only, so a width-only change moves
nothing the staleness machinery watches. A width drag anchors `dragGrabDY` at the 3 px engage and
thereafter computes the value from the cursor alone; the live width is never read again. Worse, the
behaviour was inconsistent by threshold timing: a change landing **after** the engage was overwritten,
one landing **before** it was silently adopted as the anchor and re-emitted inside the user's own
change gesture. **Measured:** `the installed width 1.700 was overwritten with 0.650 by a drag anchored
before it`.

**W2 — concurrent crossover changes are reclaimed.** `writeCrossovers` compared the live value against
its own *target*, which a large foreign move satisfies more emphatically, not less — so it overwrote.
And `captureGestureSound()` ran after **all** the stores; being branch-free and provenance-free, it
recorded whatever survived as gesture-owned, so the next `soundMovedUnderGesture()` could not see the
change either. A second, race-free instance of the same laundering: the write loop is bounded by the
live split count while the record was bounded by the array size, so the unused slots were adopted for
free. **Measured:** `the split written from inside the burst (15000.0 Hz) was reclaimed as 10000.0 Hz`.

**W3 — concurrent band changes are overwritten.** `removeBand` reads the count once and then performs
`2N - 1` stores (seven at N = 4: the solo word, three widths, two splits, the count), every one of
them derived from that single read. A host that moves `mbBands` from inside the first store had the
remaining six written over it and the old count restored on top, ending at *neither* the count the
user saw *nor* the one the writer installed. **Measured:** `Bands was moved to 2 from inside the burst
and the rest of it wrote 3 back`. `addBandAt` is the same shape with a weaker precondition.

## Options

1. **A — expanded local checks before each write.** This is the selected mechanism's *shape*, but on
   its own it is what ADR-0038 rejected: per-consumer checks a future consumer forgets. Taken only
   because it is bound to one named invariant with one recording rule, in one place.
2. **B — a shared authoritative-state generation the gesture snapshots.** Rejected on three counts.
   It misses a single automated crossover, an undo step and every width change (`soundSetGen` counts
   *wholesale* replacements, `PluginProcessor.h:267`); it is not reachable from the imager; and,
   decisively, **a silent writer bumps nothing** — `reassertParameters (…, notifyHost = false)` writes
   `rp->setValue (norm)` plus a direct atomic store and fires no listener at all, yet moves exactly
   what `crossover()`, `bandWidth()` and `bandCount()` read. Only a value comparison sees that.
3. **C — the gesture owns a snapshot of exactly what it observed or wrote.** Selected, as the
   *ownership record*.
4. **D — conditional commit adjacent to the mutation.** Selected, as the *store discipline*. C answers
   "what do I own"; D answers "when may I store it". Neither alone suffices: C with a blanket
   post-burst record launders the intruder's value (W2); D with nothing to compare against is not a
   check at all.
5. **E — reuse the repository's existing coordinated-write abstraction.** The candidate is real and
   was evaluated rather than dismissed: `juce::CriticalSection soundReplacement`
   (`PluginProcessor.h:558`, ADR-0036 §24) brackets whole-sound replacements and is explicitly never
   taken by the audio thread (`:554`). Rejected on four grounds: it does not close the reentrancy
   class at all (a host re-entering is on *this* thread, and the section is recursive); taking it
   would block the message thread behind a host-thread restore; the imager holds a `ScopeBuffer&` and
   an `APVTS&` and reaching it needs new cross-component plumbing; and ADR-0036 §24 scoped it to
   whole-sound replacements, so widening it to a GUI band edit is an ADR-0036 change, not a bug fix.
6. **F — defer or marshal the burst.** Rejected: message-thread marshalling used to hide a correctness
   problem, which ADR-0036 already declined for state.
7. **G — reorder the burst so a partial application is inert.** Rejected on inspection: `setSoloMask`
   writes a whole four-bit word, so there is no safe first store, and no ordering makes a truncated
   compaction invisible.

## Decision

> **A gesture stores an authoritative value only while that value is still the one its plan was
> computed from; the comparison is adjacent to the store, with no call between them; and ownership of
> what was stored is recorded at the store, never in a blanket pass afterwards. The first value found
> not to be owned abandons every remaining store of the burst.**

Two owners instantiate the one rule.

**A drag** owns `gestureX[3]` (split positions, within the same half pixel `writeCrossovers` uses to
decide a write is worth making) and `gestureW[4]` (band widths, exactly — the width path performs no
write suppression, so its read-back is bit-identical to its store and no epsilon has to be invented).
`ownsSplit (k)` and `ownsWidth (b)` are the two halves of the question, and `gestureBands < 0` — no
gesture in flight, the wheel path — owns nothing and refuses nothing.

**A topology transaction** (`removeBand`, `addBandAt`) owns `expectedBands` plus the `fr[]`, `wd[]`
and `oldMask` its plan was computed from at entry. The count is re-read and each target re-proved
immediately before every store of the burst.

**Width semantics: void, uniformly.** Any width the gesture does not own voids it, at any point in its
life — which is also what makes the old anchor-vs-store inconsistency go away, since both halves now
ask the same question.

Message thread only. No lock, no allocation, no blocking, no wait; the audio thread is untouched.

## Consequences

- **A drag now stops when a width it does not own moves**, where it used to overwrite it or adopt it
  as its anchor depending on when the 3 px threshold happened to be crossed.
- **A burst abandons the rest of itself** the moment a value stops being owned. For a topology
  transaction this leaves the stores already issued: that is the right trade, because the alternative
  — completing — puts the old count back over the newer one and rewrites the layout under it, while
  abandoning leaves the newer topology standing. A legitimately begun operation truncated by a newer
  authority is ADR-0036 §25's rule, not a stale overwrite.
- **Unused split slots are no longer adopted.** The record is now bounded by what the pass actually
  wrote, not by the array size.
- **`resetCrossover` and `commitFreqEditor` stop carrying their own copy of `0.5f`** and reference
  `kSplitMovedPx`, which the ADR-0039 comment already required and which two sites did not obey.
- **Three private signatures change** (`writeCrossovers`, `dragCrossoverTo`, `moveBand` now return
  `bool`); `addBandAt` returns −1 when it abandons its burst, which its one caller already handles.

**What is left open, stated rather than dressed up.**

1. **A truly concurrent store** — a host thread writing between our comparison and our store — is not
   excluded and cannot be without a lock. Its blast radius is now one parameter, not a burst.
2. **A partially applied topology transaction**, as above.
3. **Undo attribution during a bare width click**: an external width write is bracketed by the user's
   own `begin`/`endChangeGesture` and folds into their undo step. That is attribution, not ownership,
   and it is the same class as the wheel path's gesture-less writes. Out of scope, recorded.
4. **The wheel path** writes with no gesture in flight and bounds-checks its indices against a live
   count in the same statement. No gesture, so no gesture ownership. Out of scope, recorded.
5. **The vanished-band Width and solo-mask residuals** keep the ADR-0039 disposition: preserved,
   deliberately. Nothing this round found makes them user-visible incorrect behaviour.

## Related code

- `src/gui/SpectrumImager.h` — `gestureW[4]`, `ownsSplit`, `ownsWidth`, the `bool` returns.
- `src/gui/SpectrumImager.cpp` — `writeCrossovers`, `ownsSplit`, `ownsWidth`, `captureGestureSound`,
  `soundMovedUnderGesture`, `dragCrossoverTo`, `moveBand`, `addBandAt`, `removeBand`, the `mouseDrag`
  width branch, `kSplitMovedPx` at its two former copies.
- `tests/state_tests.cpp` — State test 71 (`testAGestureWritesOnlyWhatItOwns`).

## Evidence + confidence

**Verified.** All three findings reproduced against `6e37e6e` before any change and green after:
State test 71 legs (a), (b) and (c), plus leg (g) for the laundering half and leg (h) for the add
transaction. Mutations, each killed by exactly one named leg — **N1** (widths out of the detector and
out of the drag branch) → leg (a); **N2a** (no ownership check before the crossover store) → leg (b);
**N2b** (the blanket post-burst capture restored) → leg (g); **N3** (`removeBand`'s unguarded burst)
→ leg (c); **N4** (`addBandAt`'s unguarded burst) → leg (h). Positive controls held throughout: legs
(d), (e) and (f), and State tests 66–70 unchanged and green.

**Confidence: high** for the reentrancy class, which is closed by construction — between the
comparison and the store that follows it there is no call, so no listener can run. **Confidence:
bounded, and stated as such** for the cross-thread class: it is narrowed from a burst to a single
store and is not claimed closed.
