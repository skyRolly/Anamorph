# ADR-0047 — A snapshot is one reading: the value a gesture plans from and the value it proves ownership against are the same measurement

**Status:** Accepted (maintainer instruction, 2026-09-08 — *split-snapshot review round*).

**Completes [ADR-0046](ADR-0046-a-derivation-answers-under-the-topology-it-was-given.md); supersedes
nothing.** ADR-0046 settled this question for the band COUNT: a handler reads `mbBands` once, and the
derivation and the proof are both made of that one reading. It said so in as many words — *"One
reading, used by the derivation and by the proof, has neither failure."* This ADR applies the same
rule one level down, to the split and width VALUES, where every ownership predicate in the class was
still paired with a second read of the same parameter.

## Context

`crossover (i)` is `freqP[i]->convertFrom0to1 (freqP[i]->getValue())` and `bandWidth (i)` is the same
shape for `widthP[i]`, so a call to either is a read of exactly the parameter that `gestureX[i]` and
`gestureW[i]` stamp. Three threads write those parameters: the message thread (this component), the
audio thread (host automation through the format wrapper) and the host state thread.

Nothing in the spans below dispatches. `crossover`, `bandWidth`, `getValue` and `convertFrom0to1` are
plain reads and pure arithmetic; only `setValueNotifyingHost` and `beginChangeGesture` dispatch, and
neither appears between the paired reads. **So reentrancy cannot cross these windows — only another
thread can**, which is the same shape ADR-0046 established for the count and the same reason the
class is invisible to a single-threaded test.

## Problem

A gesture keeps two things about each split: **where it plans from** (`dragOrigX`, an `xs[]` array, a
`was[]` baseline) and **what it owns** (`gestureX`, the value `ownsSplit` compares against). Every one
of those pairs was built from two separate reads. The review finding named the drag capture:

```cpp
void SpectrumImager::captureDragOrigins() noexcept
{
    for (int k = 0; k < (int) std::size (dragOrigX); ++k)
        dragOrigX[k] = freqToX (crossover (k));   // read A -> the PLAN
    captureGestureSound();                        // read B -> the STAMP, inside
}
```

An automation write landing **between** the loops leaves the plan holding the old position and the
stamp holding the new value. `ownsSplit (k)` then compares the stamp against the live parameter,
finds them equal, and says the gesture owns a world it never measured. The drag proceeds and
`writeCrossovers` pushes the stale plan back out — so the automation value is overwritten, inside the
change gesture the drag opened, and therefore into the automation lane and the undo stack.

This is precisely the failure ADR-0039 and ADR-0040 exist to prevent. What is new is that the
**ownership stamp itself is what hides it**: the stamp is fresher than the plan, so every later check
agrees. The order is the one that cannot work, exactly as in ADR-0046 — there the stamp was newer
than the derivation, here the stamp is newer than the plan.

### Measured, because a window this narrow is worth a number rather than an argument

`tests/state_tests.cpp --split-snapshot-probe` drives an automation thread against a drag on split 0,
and counts the message thread writing a split it should not touch. On a 4-core box, 300 drags per
spin setting:

| | spin 0 | spin 40 | spin 120 | spin 400 | total |
|---|---|---|---|---|---|
| before this ADR | 18 | 17 | 6 | 51 | **92 / 1200 (7.7 %)** |
| after | 0 | 0 | 0 | 0 | **0 / 1200** |

Repeated twice more on the same box after unrelated edits to the probe's spin loop: **84** and **72** of 1200. The rate varies with scheduling; what does not vary is that it is never zero before the change and always zero after it.

With a 200 µs sleep inserted between the two reads as a diagnostic, one write timed into the window
laundered **200/200** before the fix and **0/200** after it — after the fix there is no second read
left to straddle. The large "drags that wrote nothing" count the probe also reports is the safe path
working: a lane that moves during the drag makes `gestureIsStale()` true and the gesture abandons.
The laundered count is the residue that guard cannot see, because the stamp agreed with the world.

**The class had to be fixed together, and that was measured too.** An independent verification pass
applied ONLY the `captureDragOrigins` change to HEAD and measured **267 laundered splits in 8000
drags (3.3 %)**, down from 885 (11.1 %) — a 70 % reduction, not a closure. The residual was the
sibling window in `writeCrossovers`, where the ownership proof and the write-worth test were their
own pair of reads. With every site converted, **0 in 8000** on the same box.

**The most user-visible instance is not a value at all — it is `addBandAt`'s insertion index.** The
plan array `xs[]` decides which side of the click the new split lands on (`while (ins < M && xs[ins] <
clickX) ++ins;`), and `fr[]` — the array every store proves itself against — was a *second* read of
the same splits. A foreign write that crosses the click point inside that window flips `ins`. Worked
case from the verification pass: two bands, split 0 at 200 Hz, a click at 500 Hz, an automation write
to 2000 Hz landing between the two reads. `ins` computes to 1 from the stale 200 Hz where a fresh
read gives 0, the ownership guard passes (it compares the post-write world against `fr[0]`, which is
also post-write), and the band is inserted on the wrong side of the pointer. One reading closes it:
`fr[k]` is read once and `xs[k]` is `freqToX (fr[k])`.

## Decision

**A snapshot is one reading.** Wherever this class takes a value to plan from and a value to prove
ownership against, it takes **one** read of the parameter and derives both from it. `convertFrom0to1`
is pure arithmetic, so with nothing racing the derived value is bit-identical to what the second read
returned — the change removes reads, it does not add them.

Two overloads carry the reading the caller already has:

```cpp
bool ownsSplit (int k, float norm) const noexcept;
bool ownsWidth (int b, float norm) const noexcept;
```

Applied at every paired site:

| site | was | is |
|---|---|---|
| `captureDragOrigins` | `dragOrigX` from `crossover`, then `gestureX` from `getValue` | stamp first, `dragOrigX` derived from `gestureX` |
| `mouseDown` handle press / add branch | `dragGrabDX` from a third read | `dragGrabDX = p.x - dragOrigX[h]` |
| `mouseWheelMove` | tick target from a fresh `crossover` | target from `dragOrigX[scrollHandle]` |
| width drag engage | `ownsWidth` reads, then `bandWidth` reads again for the anchor | one `getValue`, proved and anchored |
| `addBandAt` | `crossover (k)` twice in one statement | once, converted twice |
| `resetCrossover`, `commitFreqEditor` | `xs[k]` from `crossover`, `was[k]` from `getValue` | `was[k]` first, `xs[k]` derived |
| `writeCrossovers`, `spreadSplits` | proof reads, then the write-worth test reads again | one reading for both |

## Consequences

* The class is closed **by construction**, not by a new guard: there is no second read to straddle.
  Nothing needs to detect the race, so nothing can fail to detect it.
* Three reads become two and two become one at every site; the width engage loses one, the wheel tick
  loses one, `addBandAt` loses `M` of them. No path gains a read.
* **The wheel tick and the add-branch anchor stop failing lossily.** Those two reads were taken AFTER
  the stamp, so a foreign write between them made `ownsSplit` refuse — safe, but a user edit dropped
  for no visible reason. Derived from the capture, they cannot disagree with it.
* Behaviour is unchanged with nothing racing, which the whole suite confirms unchanged at 2804
  checks, and which is why the deterministic tests could not have caught the defect. That limit is
  recorded rather than papered over: State test 81 pins the two corners either side of the window,
  and reverting this ADR leaves all 2804 checks green. `--split-snapshot-probe` is what sees it.
* RISK-010 is **not** the same finding and is unchanged. That risk is the AUDIO-side reader tearing a
  ten-load snapshot in `PluginParameters::toEngine`; this was the GUI-side snapshot tearing under a
  foreign write. Opposite direction, different code, and the register carried no row for this one.

## Alternatives considered

* **Lock or seqlock the parameter reads.** A threading-model change (`ARCHITECTURE_REVIEW_GATE`) for a
  window that a free reordering closes completely. Rejected.
* **Re-read and compare after the capture.** Detection rather than prevention: it narrows the window
  instead of removing it, and it costs a read where the accepted fix saves one. Rejected — this is
  the same argument ADR-0046 made against ordering the count reads instead of passing the count in.
* **Accept it as a tradeoff and record a risk row.** Defensible on rarity alone until the rate was
  measured at 7.7 % against a moving lane. A defect this cheap to close is not a tradeoff.
