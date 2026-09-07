# ADR-0041 — A coupled update is all of it or none of it, and ownership is a parameter question

**Status:** Accepted (maintainer instruction 2026-09-07: three SpectrumImager review findings — wheel
input reviving stale width drags, solo clicks targeting replaced layouts, failed solo remaps still
changing bands — plus a ruling on the half-pixel ownership threshold and on the cross-thread topology
transaction, with the instruction to find the common invariant before patching any of them).

**Completes [ADR-0040](ADR-0040-a-gesture-stores-only-what-it-still-owns.md); supersedes nothing.**
ADR-0040 moved the ownership question from handler entry to the store. It left two things unstated
that this ADR settles: what a *refusal* obliges the caller to do, and what units ownership is
measured in.

**Not an Architecture Review Gate item.** No parameter ID, range, default, automation flag or
serialization field changes; `mbSolo` and `mbBands` keep exactly the meaning they had. Three private
member functions change return type and two gain a parameter.

## Context

Three findings, and they are one shape: **part of a coupled change was applied and the rest was not,
and nothing downstream could tell.**

* **T1, a partial refresh.** `captureDragOrigins()` re-seeds the projection origins and the ownership
  record; a wheel tick calls it. During a *width* drag that adopts an outside width change into
  `gestureW` while `dragGrabDY` — the anchor the next width store computes from — still points at
  the sound before it. **Measured:** `a wheel tick adopted the installed width 1.700, and the drag
  then wrote 0.650 from an anchor taken before it`. This also corrects ADR-0040's claim that the
  refresh re-seeds "everything together": true for splits, false for widths.
* **T2, a commit with no precondition.** `mouseUp` clears `gestureBands` before the solo branch, so
  `setSoloMask` ran at its `expectedBands = -1` default. **Measured:** `the click soloed band 3 of a
  four-band layout, Bands became 2 inside the store, and the mask was written as 0x8 anyway`.
* **T3, a refusal nobody heard.** `setSoloMask` could refuse its store and say nothing, and both
  topology transactions carried on. **Measured:** `the mask store was refused and the transaction
  carried on: Bands 3 with the mask left in the old numbering (0x5)`.

## Problem

ADR-0040 gave every conditional store a precondition but gave the caller no way to learn that the
precondition failed, and it measured ownership in **display pixels** — a GUI quantisation tolerance
doing a correctness job.

## Decision

> **A coupled update — a commit or a refresh — is all of it or none of it. A conditional store
> reports whether it committed, and a caller that derived state from a precondition abandons the rest
> of its plan the moment any store does not commit. A refresh that cannot bring every piece of state
> the next write depends on to the same authoritative sound refreshes none of it. And ownership is
> compared in the parameter's own units, exactly — never in pixels.**

**Ownership moves to parameter space.** `gestureX[3]` and `gestureW[4]` hold the **normalised**
value, compared with `exactlyEqual`. `storeOwned` stores and then confirms that the parameter holds
this store's result, computing the expected value with the same two conversions the parameter itself
performs (`AudioParameterFloat::setValue` is `value = convertFrom0to1 (newValue)`, `getValue()` is
`convertTo0to1 (value)`, `juce_AudioParameterFloat.cpp:97-98`), so it is bit-identical when nothing
else wrote and differs at *any* magnitude when something did. `kSplitMovedPx` keeps its one real job
— deciding whether a write is worth making — and `kWidthQuantum` is deleted.

**Refusals are heard.** `setBands`, `setSoloMask` and `toggleSoloBit` return `bool`. `addBandAt` and
`removeBand` abandon the transaction when the mask store is refused.

**The solo click names its topology.** `mouseUp` passes `pressBands` and the mask it read to both
solo paths, so the store proves the topology the band index belongs to and the word it is replacing.

**A wheel tick ends an in-flight press.** Two gestures cannot own the same state at once, and the
wheel's refresh cannot re-anchor `dragGrabDY`/`dragGrabDX`. The press ends; the wheel then acts with
nothing in flight. **The wheel is not disabled** — the press is finished.

## The half-pixel threshold, measured

Against the real axis (`kAxis` plus its Fritsch–Carlson map) at the harness's 902 px plot, half a
display pixel is:

| Hz | Δ Hz | % |
|---|---|---|
| 30 | 0.19 | 0.642 % |
| 200 | 0.70 | 0.352 % |
| 1 000 | 3.53 | 0.353 % |
| 10 000 | 32.31 | 0.323 % |
| 18 000 | 54.79 | 0.304 % |

Worst **0.646 % at 27 Hz**; worst absolute **61.24 Hz at 19 905 Hz**. The crossover parameters are
`logFreqRange (20, 20000)` with **no interval** (`PluginParameters.cpp:238-240`), resolving about
**0.00055 Hz at 1 kHz** — so a 32 Hz change at 10 kHz is roughly **59 000 times** the parameter's
resolution, fully automatable, and was being read as the gesture's own. **Outcome C**: pixel space is
the wrong ownership primitive.

## The cross-thread topology transaction

| Option | Verdict |
|---|---|
| **A** per-store conditional ownership | **Kept**, and materially tightened by T3: a refused store now stops the transaction. |
| **B** single conditional commit from one snapshot | Rejected as *unreachable*, not undesirable: the layout lives in eight separate automatable parameters, so there is no single commit point; approximating one is exactly A. |
| **C** versioned topology | Rejected: the **silent writer** (`reassertParameters` with `notifyHost = false`) advances no version yet moves what the imager reads, so the version would have to be added to the restore path — a processor change to reach what the value comparison already gives. |
| **D** lock | Prohibited: `mbBands` is written from the audio thread (`REALTIME_AUDIO_POLICY`, a Thread Model change). `soundReplacement` (ADR-0036 §24) is never taken by the audio thread but does not close reentrancy at all and is scoped to whole-sound replacements. |
| **E** other | Nothing the measurements support. |

**Ruling: a bounded, documented concurrency trade — not a defect and not an architectural
violation.** What *was* a defect is T3, and it is fixed. What remains is a store landing between our
comparison and the `setValueNotifyingHost` on the next line, from another thread; its blast radius is
one parameter, and under a lower new count the parameters left behind are ones the DSP does not read
(`SoloMonitor.cpp:85`, `MultibandWidth.h:53`).

## Consequences

- **A wheel tick during a drag ends the drag.** New, deliberate, and stated as a product decision.
- **A solo click whose topology moved under it writes nothing** — the conservative answer, and the
  same one a release lost outside the window already gives.
- **A topology transaction stops at a refused store** instead of finishing against a precondition
  that no longer holds.
- **The gesture now stops for changes it used to absorb** — anything down to the parameter's own
  resolution rather than only what a display pixel can show.
- **`kWidthQuantum` is gone**; the width path needs no epsilon once ownership is exact.

## Related code

- `src/gui/SpectrumImager.h` — `gestureX`/`gestureW` as normalised values, `storeOwned`, the `bool`
  returns on `setBands`/`setSoloMask`/`toggleSoloBit`.
- `src/gui/SpectrumImager.cpp` — `ownsSplit`, `ownsWidth`, `storeOwned`, `captureGestureSound`,
  `writeCrossovers`, the `mouseDrag` width store, `mouseUp`'s solo branch, `addBandAt`, `removeBand`,
  `mouseWheelMove`.
- `tests/state_tests.cpp` — State test 73 (`testACoupledUpdateIsAllOfItOrNone`).

## Evidence + confidence

**Verified.** All three findings reproduced against `799113f` and green after: State test 73 legs
(a), (b), (c), with leg (g) for the threshold. Mutations, each killed by exactly one named leg —
**P1** (the wheel leaves the press running) → (a); **P2** (the solo click carries no topology) → (b);
**P3** (`removeBand` carries on after a refused mask store) → (c); **P4** (ownership back in pixel
space) → (g). Positive controls held: legs (d), (e), (f), and State tests 66–72 unchanged and green.

**Confidence: high** for the three findings and for the threshold, which is arithmetic against the
real axis. **Bounded, and stated as such** for the cross-thread window, which is ruled an accepted
trade rather than claimed closed.
