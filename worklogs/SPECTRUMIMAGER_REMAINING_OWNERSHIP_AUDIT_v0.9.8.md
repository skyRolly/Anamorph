# SpectrumImager — remaining ownership and stale-gesture audit (v0.9.8)

**Head at the start of the round:** `ae86963`. **Preceding rounds:** ADR-0038 (a gesture is void once
its topology moves), ADR-0039 (a gesture owns the world it was latched in), ADR-0040 (a gesture
stores only what it still owns), ADR-0041 (a coupled update is all of it or none of it; ownership is
a parameter question), ADR-0042 (a store is committed only when the parameter says so).

**The invariant under test, unchanged:**

> A gesture stores an authoritative value only while that value is still the one its plan was
> computed from.

## 1. What the review raised

| # | Cited | What it says |
|---|---|---|
| F1 | `SpectrumImager.cpp:662`, `commitFreqEditor` `:881-885` | Primary crossover stores lose host writes: the store path does not verify that the parameter still holds the value the plan was computed from, so *gesture starts → host changes crossover → GUI commits old value → newer host value is overwritten* |
| F2 | `:1206`, `tick()` `:1162-1168` | A held solo gesture can survive a topology change, and `tick()` then auditions a band by a stale positional identifier |
| D1 | `SpectrumImager.h:350` | The comment still describes ownership as a pixel tolerance |
| I1 | `:354` | A cancelled spread can leave the handles visually disordered — investigate only |

**Anchor drift, reported rather than silently followed.** Three of the four line numbers do not name
what the finding describes on `ae86963`: `:662` is the snapshot loop inside `resetCrossover`, not a
store; `:881-885` is the `TextEditor` colour setup inside `openFreqEditor`; `:1162-1168` is the
analyser's release-decay loop, not `tick()`'s solo block. The **substance** of each finding is real
and is audited below at the lines that actually carry it — `resetCrossover`'s store at `:673`,
`commitFreqEditor`'s at `:926`, and the hold promotion at `:1207-1212`. `SpectrumImager.h:350` and
`SpectrumImager.cpp:354` are exact.

## 2. Every write and preview path, and whether the invariant is applied

| Operation | Cached state | Ownership check | Reentrant safe | Stale behaviour |
|---|---|---|---|---|
| crossover drag (`writeCrossovers`, `:327-344`) | `dragOrigX`, `dragGrabDX`, `gestureX`, `gestureBands` | count re-proved + `ownsSplit(k)` immediately before each store; `storeOwned` after | **yes** | returns false → `mouseDrag` cancels the gesture |
| band move (`moveBand` → `writeCrossovers`) | `dragOrigX`, `bandAnchorX`, `bandTmin/Tmax`, `gestureX` | as above | **yes** | as above |
| crossover wheel (`mouseWheelMove` `:2228-2248`) | none — `cancelActiveDrag()` runs first, so `gestureBands < 0` and `ownsSplit` is waived | `storeOwned` far side only | **yes** | n/a: the target is read live one statement before the store and nothing dispatches between |
| **`resetCrossover` primary (`:673`)** | `xs[]`/`was[]` snapshot taken **before** `beginChangeGesture` | far side only (`storeOwned`) | **partial** | **F1: the stored value can depend on the neighbours' snapshot — see §4** |
| `resetCrossover` spread (`spreadSplits`) | `xs[]`, `was[]` | count + `was[k]` before each store, `storeOwned` after | **yes** | returns false → the rest of the spread is abandoned |
| **`commitFreqEditor` primary (`:926`)** | the text box, `xs[]`/`was[]` snapshot before `beginChangeGesture` | `i >= M` refusal at entry; far side only | **partial** | **F1: two defects — see §3 and §4** |
| `commitFreqEditor` spread | as `resetCrossover` | as `resetCrossover` | **yes** | as `resetCrossover` |
| `addBandAt` mask (`:741`) | `oldMask`, `N` | `expectedBands` + `expectedMask` between gesture open and store; `soloMask()` re-read after close | **yes** | returns −1; nothing further is stored |
| `addBandAt` widths (`:747`) | `wd[]`, `N` | count + exact `bandWidth(i) == wd[i]` before each; store elided when the plan equals the snapshot | **yes** | returns −1 |
| `addBandAt` splits (`:765`) | `xs[]`, `fr[]`, `N` | count + exact `crossover(i) == fr[i]` before each; elided when unmoved | **yes** | returns −1 |
| `addBandAt` count (`:768`) | `N` | `expectedBands` before, `bandCount() == want` after the gesture closes | **yes** | returns −1; the caller latches no identifier |
| `removeBand` mask / widths / splits / count | `oldMask`, `fr[]`, `wd[]`, `expectedBands` | same shape as `addBandAt`; splits compared exactly in parameter space | **yes** | returns; the stores already issued stand (ADR-0042 §41) |
| width drag (`:2087-2098`) | `gestureW`, `dragGrabDY` | `ownsWidth` before the anchor **and** before the store; `storeOwned` after | **yes** | `cancelActiveDrag()` |
| width wheel (`:2249`) | none — press ended first | none needed | **yes** | n/a: single store, nothing follows |
| width reset (`resetParam`, `:540-543`) | none — the target is the parameter's own default | none | **yes** | n/a: single store, nothing follows |
| solo click (`toggleSoloBit` / `setSoloMask`, `:2137-2141`) | `pressBands`, the mask read at `:2136` | `expectedBands` + `expectedMask` between open and store; `soloMask()` after close | **yes** | the store is refused; nothing else follows |
| **momentary solo audition (`tick()` `:1207-1212`)** | `soloPressBand`, `soloPressMs`, `soloHoldActive` | **none** | n/a | **F2: nothing here asks `gestureIsStale()` — see §5** |
| momentary solo audition (`mouseDrag` `:2050`) | as above | `gestureIsStale()` at `:2044` | n/a | `cancelActiveDrag()` clears the preview |
| preview clear (`mouseUp` `:2107`, `cancelActiveDrag` `:2195-2199`) | as above | `gestureIsStale()` at `:2107` | n/a | fires `onClearSoloPreview` |
| crossover band-pass preview (`soloCurveBand`, `:1254`) | `soloPressBand` | **none** — it copies `soloPressBand` whenever `soloHoldActive` | n/a | drawn against whatever layout is live (F2's visual half) |

**Answer to "is the ADR-0040/0042 invariant fully applied?" — no, in exactly two places**, and both are
the ones the review named: the two *primary* stores of the reset / text-commit pair, and the solo
hold promoted by `tick()`. Every other row is covered, and several of them are covered by a guard
that would be a **false refusal** if it were added elsewhere — see §6.

## 3. F1, first half: a commit that carries no user intent

`openFreqEditor` seeds the text box from the live split at `:896`:

```
freqEditor->setText (freqText (crossover (i)), juce::dontSendNotification);
```

and `commitFreqEditor` stores whatever is in the box. The editor is committed by **Return** (`:889`),
by **focus loss** (`:891`) and by **any `mouseDown` in the component** (`:1950`). So opening the chip
and clicking away re-commits the value the split had when the editor opened — a store the user never
asked for, carrying a value from the past.

**Measured on `ae86963`, State test 75 leg A:**

```
[leg A] a dismissed editor wrote its opening snapshot over a newer host value:
        5000.0 Hz was installed and 200.0 Hz was written over it
```

That is the review's sequence exactly. And leg C measures the same defect with nothing moving at
all: `dismissing an untouched editor issued 1 store(s)` — an automation touch and an undo step for
an edit that never happened.

Leg B is the line a fix must not cross: a **typed** value must still replace a host write made while
the box was open, because the user's own action is the newer authority (ADR-0036 §25). It is green
before and must stay green after.

## 4. F1, second half: a projection that depends on state the store never proves

`projectGaps` (`:283-303`) pins the edited split, packs the neighbours to the minimum gap, and then
slides **the whole cluster — the pin included** — back inside the plot edges:

```
if (xs[0] < lo)         { const float d = lo - xs[0];         for (...) xs[i] += d; }
if (xs[count - 1] > hi) { const float d = xs[count - 1] - hi; for (...) xs[i] -= d; }
```

The slide amount is a function of the **neighbours'** snapshot. So the value the primary store writes
is not always the user's typed value or the parameter's default: when the slide fires it is a
projection of the world as it was at snapshot time. And `beginChangeGesture` at `:672` / `:925`
dispatches to the host **before** the store, so a host answering the gesture-open by moving a
neighbour is inside the window.

**Measured, State test 75 leg D.** Splits `200 / 18000 / 19500`, type `15 kHz` into the first chip:

```
uninterrupted:  8440.1 / 11407.5 / 15122.0     <- the projection doing its job
interrupted:    8440.1 /  3000.0 / 19500.0     <- the pin stored against splits that had moved
```

In the interrupted case the spread correctly refuses (`was[k]` catches the moved neighbour, so the
host's 3000 Hz stands), but the pin was already stored at a position computed to make room for
splits at 18000/19500 that no longer exist — leaving the **first split above the second**. The pin's
value is justified by nothing.

## 5. F2: the hold that auditions a band nobody pressed

`tick()` promotes a press to a hold on a purely time-based condition (`:1207-1212`):

```
if (soloPressBand >= 0 && ! soloHoldActive && ! soloMovedBand
    && juce::Time::getMillisecondCounter() - soloPressMs > 200u)
{ soloHoldActive = true; if (onSoloPreview) onSoloPreview (1 << soloPressBand); }
```

`soloPressBand` names a band by **position**, latched at `mouseDown` against the topology recorded in
`gestureBands`. `mouseDrag` (`:2044`) and `mouseUp` (`:2107`) both ask `gestureIsStale()` before
acting on it. **`tick()` asks nothing.** So:

* press the headphone on band 3 of four → a host lane drops Bands to 2 → 200 ms later the audition
  fires with mask `0x8`, which `SoloMonitor::process` masks to `0x8 & 0x3 = 0`: the user holds a
  solo button and hears **no solo at all**;
* press band 1 of two → a host lane raises Bands to 4 → the audition is band 1 of the **new** layout,
  a different frequency range from the one the user pressed on;
* and `soloCurveBand = soloPressBand` at `:1254` draws the band-pass curve for that same stale index.

The **release** already handles it (`mouseUp` cancels and clears the preview), so the wrong audition
is bounded by how long the user holds — which is unbounded.

## 6. Where an ownership check would be a FALSE REFUSAL, and why it is not added

The review asks whether ownership checking is required on the primary stores. **For the value itself,
no — and adding it would break a legitimate edit.** An Alt-click reset targets the parameter's
*default*; a typed commit targets the *user's typed value*. Neither is computed from the split's
current value, and the user's action is itself the newer authority at the moment of the commit
(ADR-0036 §25). A guard of the form "refuse if the split moved since the snapshot" would abandon a
deliberate user edit whenever an automation lane happened to touch that split — the exact class of
false refusal this series has been careful to avoid (ADR-0042: `setParam` stays `void` for the same
reason). State test 75 leg B holds that line.

What **is** required is narrower and is what §4 establishes: the store must prove the state its
*projection* depended on. That is a different claim from "the target must not have moved", and it is
the one the invariant actually makes.

## 7. I1: the disordered layout, ruled

`spreadSplits` abandoning part-way can leave the splits out of order **on screen** — leg D's
interrupted case is a live example (`8440.1 / 3000.0 / 19500.0`). Asked whether that is a real
correctness problem:

* **The DSP is unaffected.** `MultibandWidth::setCrossovers` and `SoloMonitor::setCrossovers` clamp
  and force strict `1.1×` ordering on whatever they read (`MultibandWidth.cpp:102-112`,
  `SoloMonitor.cpp:68-77`), so no filter ever sees an inverted split.
* **No invariant in the repository requires ordered drawn handles**, and `handleNearX` (`:249-259`)
  is order-independent — it picks the nearest split by pixel distance, so every handle stays
  grabbable and one drag restores the order.
* **But two hit-tests do assume ordering, and degrade.** `bandAtX` (`:241-248`) scans ascending and
  returns the first band whose upper split exceeds the frequency, so the band between an inverted
  pair can never be returned — its width line and its add area become unclickable. `soloHit`
  (`:272-278`) requires `bandRightX(b) - bandLeftX(b) > 30`, which is **negative** for that band, so
  its headphone is not hittable either. Nothing is destroyed and nothing is deleted wrongly; the
  affordances of one band are simply unreachable until any edit re-projects the layout.

**Ruling: not a correctness defect, and not fixed directly.** Fixing it in the drawing or the
hit-testing would mean either sorting what the parameters say (drawing a layout the plug-in does not
have) or refusing to abandon a spread (writing a stale value over a newer authority — the thing this
whole series exists to stop). What the round *does* do is remove its only single-threaded trigger:
§4's fix means a spread is no longer computed from a world that has already moved, so the
same-thread reentrant route to a disordered layout closes. What remains is the genuinely concurrent
route — a host lane writing a split from another thread between our snapshot and our store — which
is the bounded, documented trade ADR-0042 §41 already rules on. Recorded here as accepted behaviour
with its blast radius named.

## 8. Rejected approaches

| Approach | Why it loses |
|---|---|
| **Ownership check on the primary store's target** ("refuse if the split moved since the snapshot") | A **false refusal**. The target of a reset is the parameter's default and the target of a typed commit is the user's typed value; neither is computed from the split's current value, and the user's action is the newer authority. It would abandon a deliberate edit whenever automation touched that split. State test 75 leg B is the guard against ever adding it. |
| **Refuse the commit whenever `projectGaps` slid the pin and any neighbour moved** | Correct but unnecessarily lossy: it throws away a legitimate user edit in order to avoid a projection that could simply have been computed later. It also needs a new predicate; §9's choice needs none. |
| **Clamp the typed value instead of sliding the cluster** | A product behaviour change to `projectGaps`, which the drag path also uses, in a round whose brief is the smallest correct fix. The slide is documented behaviour ("slide the whole cluster back inside the edges, keeping the gaps it just set") and leg E holds it. |
| **Compare the committed text against the value at open, rather than tracking whether it was edited** | Cannot distinguish "the user retyped the same number" from "the user typed nothing", and mis-handles equivalent spellings (`0.2 kHz` vs `200 Hz`). `juce::TextEditor::onTextChange` answers the real question exactly and costs one flag. |
| **A stale-hold check inside `paint()` or `effectiveSoloMask()`** | Painting must not have side effects, and the audition has to stop whether or not a repaint happens. |
| **A new mechanism for stale positional identifiers (architecture option C)** | There is already one — `gestureIsStale()` — and `mouseDrag` and `mouseUp` both use it. A third consumer that does not is a missing call, not a missing mechanism. |

## 9. Decision — architecture option A, reuse, unchanged

**All three fixes are applications of primitives that already exist. Nothing is extended and nothing
new is built.**

1. **F1a — a commit carries intent, or it carries nothing.** `openFreqEditor` clears a flag after
   seeding the box; `TextEditor::onTextChange` sets it; `commitFreqEditor` returns without storing
   when it is clear. This is not an ownership question at all — a commit that carries no user change
   has nothing to own — which is why it needs no ownership primitive. It also removes the spurious
   automation touch and undo step that a dismissal produced even when nothing had moved.
2. **F1b — compute the plan after the gesture opens.** The snapshot, `projectGaps` and the store move
   inside the `beginChangeGesture` / `endChangeGesture` bracket, in that order. `projectGaps` is
   pure, so between the snapshot and the store **nothing dispatches**: the projection is a function
   of the world as it is one statement before the store, and `storeOwned` still proves the far side.
   No refusal, no new predicate, no behaviour change in the ordinary case — the plan is simply
   computed later. `spreadSplits` is untouched and its `was[]` is now captured from the same fresh
   world.
3. **F2 — `tick()` asks the question the other two consumers ask.** `if (gestureIsStale())
   cancelActiveDrag();` immediately before the hold promotion. `cancelActiveDrag` clears
   `gestureBands`, so the check fires once rather than every frame, and it early-returns before any
   repaint when no identifier is latched — so an Alt-click reset, which latches none, costs one
   integer store. That also retires the residual ADR-0042 §45 recorded: `resetCrossover` leaves
   `gestureX` stale, which used to be harmless "only by luck".

**Why not B or C.** B (extend the primitives) has nothing to extend: `ownsSplit`/`ownsWidth`/
`storeOwned`/`spreadSplits` are already exactly right for what they guard, and the two gaps are a
plan computed too early and a consumer that never asked. C (a separate mechanism) would put a second
answer to "is this gesture still valid?" beside `gestureIsStale()`, which is the mistake ADR-0038
was written to end.

**Realtime and threading:** message thread only. No lock, no wait, no allocation, no audio-path
change, no parameter-model change. Not an Architecture Review Gate item — no parameter ID, range,
default, automation flag or serialization field moves, and no Accepted ADR is contradicted.

**Known coverage limit, stated before the work rather than after.** F2's trigger lives in `tick()`,
which is driven only by `juce::VBlankAttachment` (`FrameClock::start`, `FrameClock.h:44-58`) and has
no headless surface: `tick` is private, no test in the suite drives it, and a component with no peer
receives no vblank. §12 records what is testable, what is not, and the disclosures ADR-0025 requires.

## 10. Implementation chronology

1. The write-path table (§2) built by reading every store and preview site, before any edit.
2. State test 75 written against `ae86963` with legs A, C and D red and B and E green, and the two
   measurements quoted in §3 and §4 taken from that run.
3. The decision record above written and committed before any production edit, per the round's gate.
4. `openFreqEditor` records the seeded text and clears the edited flag; `TextEditor::onTextChange`
   sets it; `commitFreqEditor` returns without storing when neither says the user changed anything.
5. `resetCrossover` and `commitFreqEditor` restructured so the snapshot, `projectGaps` and the store
   all sit inside the change-gesture bracket, in that order.
6. `tick()` gains `if (gestureIsStale()) cancelActiveDrag();` before the hold promotion.
7. Leg F added for the reset path after measuring that it slides the cluster the other way
   (`21 / 25 / 19000` resets its third split to `4732.0` Hz, not its `3000` Hz default).
8. **A correction the first attempt forced.** The intent gate was first written against
   `onTextChange` alone. That broke four existing legs — State test 74 legs C, G and I and the new
   leg B — because `TextEditor::onTextChange` is delivered through `postCommandMessage`
   (`juce_TextEditor.cpp:594-599`) and never arrives without a running message loop. An asynchronous
   signal is a poor basis for a correctness gate in any case; the synchronous comparison against the
   seeded text was added beside it, and either is now sufficient. Recorded rather than quietly
   amended: the first design would have shipped a gate that only works when a message loop happens
   to be pumping.

## 11. Mutation proofs

| # | Mutation | Killed by |
|---|---|---|
| N1 | `commitFreqEditor` drops the intent gate | leg A **and** leg C |
| N2 | `commitFreqEditor` computes the plan before the gesture opens | leg D |
| N3 | `resetCrossover` computes the plan before the gesture opens | leg F |
| N4 | `tick()` drops the stale check | **nothing** — see §12 |

## 12. The one thing that has no test, and the four disclosures ADR-0025 requires

F2's fix is one call to an existing predicate, and its **trigger** cannot be driven headlessly.

1. **Why no reliable test exists.** The promotion is in `SpectrumImager::tick (double)`, private
   (`SpectrumImager.h:66`) and driven only by `juce::VBlankAttachment` (`FrameClock::start`,
   `FrameClock.h:44-58`). The suite constructs the editor but never shows it
   (`tests/state_tests.cpp:6-11`: "no peer, no message loop, no interaction"), a component with no
   peer receives no vblank, and no existing test drives a tick. Every other way in was checked:
   `mouseDrag`'s promotion at `:2050` is already behind `gestureIsStale()`, so a test through it
   cannot discriminate the fix; `paint` is drivable but must not have side effects; and
   `cancelActiveDrag()` is public but calling it directly would test the test.
2. **What replaced it.** A source-level proof plus the two sibling consumers. The promotion is the
   third reader of `soloPressBand`; `mouseDrag` (`:2044`) and `mouseUp` (`:2107`) already ask
   `gestureIsStale()` and cancel through `cancelActiveDrag()`, and both are covered by State tests
   68–74. The fix is the same call, in the same shape, at the third site. What is uncovered is only
   *that a tick, rather than a mouse event, is what notices*.
3. **Where the gap is tracked.** `docs/procedures/TESTING.md` §"Gaps in the automated coverage",
   as the new `FrameClock` entry, cross-referenced from ADR-0043.
4. **Whether infrastructure could close it.** Yes, concretely: a seam that lets the suite step one
   frame — a test-only `FrameClock::fire (double)`, or a shown editor with a driven message loop —
   would reach this and every other per-frame behaviour in the imager, none of which has coverage
   today. Not done here because adding a production seam to test a one-line guard inverts the cost,
   and because the driven-message-loop harness is the same infrastructure the GUI-lifetime entry in
   the same register is waiting on. Revisited when that lands, per ADR-0025 §5.
