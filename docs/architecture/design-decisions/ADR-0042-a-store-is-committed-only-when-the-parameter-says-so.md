# ADR-0042 — A store is committed only when the parameter says so

**Status:** Accepted (maintainer instruction 2026-09-07: one confirmed SpectrumImager review finding —
reentrant stores corrupting coupled edits at `SpectrumImager.cpp:542` and `:498-501` — with the
instruction to understand the transaction model before patching it, and to re-open the cross-thread
partial topology transaction as an investigation rather than preserve its accepted disposition).

**Completes [ADR-0041](ADR-0041-a-coupled-update-is-all-of-it-or-none-of-it.md); supersedes nothing.**
ADR-0041 wrote the rule — *a conditional store reports whether it **committed***. It implemented that
rule as far as the **precondition** and no further, and it converted one of the three spread loops in
this class to ownership. This ADR finishes both halves and re-rules the cross-thread residual on new
evidence.

**Not an Architecture Review Gate item.** No parameter ID, range, default, automation flag or
serialization field changes; `mbBands` and `mbSolo` keep exactly the meaning they had. No threading
model change: nothing new runs on the audio thread, no lock, no allocation, no blocking, no wait, no
marshalling. Two private member functions change their return expression, one private member function
is added, two gain a snapshot array.

## Context

`setValueNotifyingHost` stores and then dispatches every listener **synchronously on the calling
thread** (`juce_AudioProcessorParameter.cpp:59-63`, `:111-121`), and
`AudioProcessor::ParameterChangeForwarder` (`juce_AudioProcessor.cpp:1467-1475`) hands each one to
the host. ADR-0040's round-3 correction closed the window `beginChangeGesture` opens *before* the
store. The window the value dispatch opens *after* it was left open in four places, and all four were
measured on `e247c11` by State test 74:

* **`setSoloMask` (`:542`)** set `stored = true` the instant the store was issued. *Measured:* `the
  mask store was overwritten from inside its dispatch and the transaction carried on: Bands 3 with
  mask 0x9` — and `SoloMonitor.cpp:85` then masks `0x9 & 0x7`, so the soloed top band disappears.
* **`setBands` (`:519`)** the same, and `addBandAt`'s caller latches `gestureBands`, `dragHandle` and
  an open host gesture on the strength of its answer. *Measured:* `the count store did not stand
  (Bands 2) and the press still latched the add and opened 1 gesture(s) on the new split`.
* **`resetCrossover` (`:605-621`) and `commitFreqEditor` (`:810-829`)** spread their neighbours with
  the predicate ADR-0040 condemned at `:314-316` — *does the live value differ from MY target?* — which
  a large foreign move answers **more** emphatically, not less. *Measured, on both:* `5000.0 Hz was
  installed and 2000.0 Hz was written over it`.

## Problem

Two different failures wear the same clothes, and conflating them produces the wrong fix:

* **A false claim.** A store reports a success the parameter did not keep, and a later store whose
  *meaning depends on it* is performed anyway. (`setSoloMask`, `setBands`.)
* **A stale write.** A later store overwrites a newer authoritative value, because its predicate asks
  "is this different from my target?" instead of "is this still mine?". (The two spreads.)

## Decision

> **A store is committed only when the parameter says so. A conditional store re-reads its own target
> after the last dispatch it can cause and reports what it finds, and a transaction abandons the rest
> of its plan the moment a store whose result a later store depends on did not commit. A plan is
> applied only to the world it was computed from: a slot that no longer holds the value the plan was
> computed against belongs to somebody else, and is not written.**

**The far side is verified in the units the caller reasons in.** `setBands` returns
`stored && bandCount() == want`; `setSoloMask` returns `stored && soloMask() == mask`. Both re-read
**after `endChangeGesture`**, which is the last instant either function can still be believed, so a
write from inside the gesture close is caught as well as one from inside the value dispatch. Neither
can false-refuse a clean store: `bandCount()` and `soloMask()` decode through `std::lround`
(`SpectrumImager.cpp:189-210`) and `RawInt` stores and returns the raw normalised float with no
snapping (`PluginParameters.cpp:71-89`), so the store's own round trip cannot move the integer.

**The two spreads own what they write.** `spreadSplits` captures each neighbour's normalised value
before the primary store, writes a neighbour only while it still holds it, stores through
`storeOwned`, and stops at the first slot that is not ours — the shape `writeCrossovers` has carried
since ADR-0040. The primary store is confirmed the same way and the spread does not run if it did not
land, because every neighbour position was computed to make room for it.

**`setParam` stays `void`, deliberately.** Every one of its callers is either a leaf of a topology
burst — the plan for slot `k+1` is read from the entry snapshot, never from slot `k`'s committed
value, so there is no stale plan to continue from — or a single store with nothing after it (the
wheel, `resetParam`). Measured: a listener echoing a width slot from inside its own store in the
middle of `removeBand` leaves `Bands 3 mask 0x5 wLo 1.750`, the intended layout with the newer
authority's width standing. Aborting there was measured to be **worse**, because the mask is stored
first and is only correct once the count changes.

**Abort semantics: abandon, never retry.** Re-reading and continuing is not available — the whole
plan (solo remap, width shift, split shift, new count) comes from one entry snapshot, so "recompute"
means "start again". Restarting is the uncontrolled retry loop the trigger itself can repeat. The old
value is never reapplied over the newer write.

## The cross-thread partial topology transaction, re-ruled

ADR-0041 ruled this a bounded trade because there is no single write-side commit point. Re-derived
from the final implementation, **the ruling stands and the reason is stronger: even one would not
help.** `PluginParameters::toEngine` reads the ten multiband atomics with **ten separate `load()`
calls** once per block, with no seqlock, generation or coherence guard
(`PluginParameters.cpp:365-374`, from `PluginProcessor.cpp:186`). The **reader tears**, so an atomic
write-side commit would be re-torn on the read side. Making the topology genuinely atomic means
replacing the read with a versioned or double-buffered snapshot — a **threading-model change**, an
Architecture Review Gate item, and out of scope for a review round.

**Option C's recorded reason was false, and this ADR corrects it.** ADR-0041's option table, and
this ADR's first form, rejected a generation counter because the silent writer
"advances no version". `reassertParameters (…, notifyHost = false)` fires no listener
(`PluginProcessor.cpp:743-748`) but **does** bump `soundParamGen` at `:779-780`, under a comment
saying exactly why, added a month before ADR-0041 shipped. The verdict is unchanged and rests on
three obstacles that are each sufficient: the processor listens to every non-view parameter and
bumps unconditionally (`PluginProcessor.cpp:43-47`, `PluginProcessor.h:171-174`), so the
transaction's own stores move the counter it would watch, and one global counter conflates all eight
multiband parameters with every other sound parameter; the silent bump is once per **pass**, not per
store, so it is invisible in the between-adjacent-stores window the whole design exists to close;
and both the bump and the silent stores are relaxed with no release/acquire pairing, so making a
counter trustworthy cross-thread means editing the restore path's synchronisation — a
threading-model change and a hard stop. The parameter's own normalised value is the stronger stamp:
per-parameter, content-addressed, self-stamped at the store, and already atomic. The only thing a
counter buys over it is ABA detection, and an ABA here is a no-op by construction because the guards
re-prove the count, the mask and each value exactly.

**Option D's decisive objection is not the policy but that there is no second acquisition site.**
Every frame between the imager's store and the host's write-back is JUCE-wrapper or host code, so
there is nowhere to put the matching lock; a lock with one acquisition site is not mutual exclusion.
The same-thread re-entry then makes exclusion impossible in principle — `juce::CriticalSection` is
re-entrant, a non-recursive mutex self-deadlocks the message thread inside a host callback, and a
`try_lock` that drops the host's write discards an authoritative store.

Option **E**, evaluated on its merits this round rather than dismissed: a compensating rollback that
restores the stores already issued writes a **stale value over a newer authority**, which ADR-0036
§25 forbids and which every finding in this series has been about; its rollback stores can themselves
be refused and interfered with, so it has no deterministic termination. Rejected.

What makes option A safe is not the size of the window but what the DSP does with anything it reads:
every split is clamped to `[20 Hz, 0.45·sr]` and force-ordered `1.1×` (`MultibandWidth.cpp:102-112`,
`SoloMonitor.cpp:68-77`); the count is clamped to `[1,4]` and only that prefix is used
(`MultibandWidth.h:53-56`); the mask is masked to the live count (`SoloMonitor.cpp:85`). Every
continuous quantity is smoothed — rate-capped crossover targets (ADR-0015), one-pole widths,
`SmoothedValue` solo gains — so a partial layout living for the handful of stores between two
statements never arrives at the output. The one discontinuous quantity, `mbBands`, is a structural
change routed through the engine's silent switch-duck and is written **last** by both bursts, which is
why the store order is kept.

## The adversarial pass over the shipped fix

Once the fix was in the tree and green, a read-only fan-out re-derived every store site from the
shipped code. It found two more, both the same rule applied inconsistently rather than a new class,
and both are closed here:

* **`spreadSplits` re-proved the values but not the count.** `writeCrossovers` has re-proved
  `bandCount()` before every store since ADR-0040's round-3 correction; the new helper did not, and
  `was[k]` cannot stand in for it because `mbBands` is a different parameter. A host lowering the
  count from inside the primary store left every split holding what it held, so the plan — made for
  the old count — was applied anyway, reaching the host as automation and undo entries the user never
  made. Closed by `bandCount() - 1 == count` at the top of each iteration.
* **`addBandAt` owned its splits in pixels.** ADR-0041 ruled ownership a parameter question and
  converted the gesture paths; `removeBand` has compared `juce::exactlyEqual (crossover (k), fr[k])`
  since ADR-0040, and this one guard was left on `kSplitMovedPx` — 32 Hz at 10 kHz, so a fully
  representable, fully automatable host move that size read as "unchanged" and the burst wrote over
  it. Closed by capturing the splits in parameter space alongside the pixel plan.

A third round of the same pass, over the transaction half, corrected two more claims and removed one
more defect. Both burst comments said abandoning leaves *"at most one already-issued store behind"*;
an add at N = 3 issues nine stores and a removal at N = 4 issues seven, so the largest residue is
**eight** and **six**, corrected in place. And below the insertion point — or above the removal —
most of the plan *is* the world it was computed from, yet the burst wrote those slots anyway: for
the widths a redundant dispatch, for the **splits** a value change, because the plan is carried in
pixels and `xToFreq (freqToX (f))` is a bisection over a monotone-spline axis, not the identity.
**Measured on a two-band add: `split0 200.000015259 -> 199.999847412, delta -1.678e-04`** — an
automation and undo entry, with a new value, for a split the user never touched. Both bursts now
skip a store whose plan equals its snapshot exactly, which removes the perturbation and shrinks the
reentrancy surface together.

One defect the audit reached that no earlier round had: **`commitFreqEditor` never re-proved that its
handle still names a live split.** `openFreqEditor` checks it when the editor opens, and nothing
closes the editor when the band count moves afterwards, so a host lane dropping Bands while the user
types left the commit storing into a split the topology no longer uses — an automation and undo entry
the user never made, live again when the count returns — and handing `spreadSplits` a pin outside the
live range. Closed the way ADR-0039 closed the same class in `removeBand`: refuse, never clamp.
`resetCrossover` takes the same guard.

Examined and deliberately not changed: `projectGaps` is a coupled chain, so proving one slot does not
prove the plan *for* that slot, and an abort part-way can leave the splits out of order **on screen**
— the same accepted trade as above, since the alternatives are to overwrite a newer authority or to
commit atomically; the DSP force-orders whatever it reads. `resetCrossover` writes a pixel round trip
of the default (~1e-6 Hz) and does not refresh `gestureX`; both are pre-existing, inert on every
reachable path, and recorded in the worklog rather than changed inside a round about stores.

## Consequences

* A user edit that a host is simultaneously automating on the same parameter now aborts instead of
  completing. That is newer-authority-wins (ADR-0036 §25) and is the intended trade.
* `resetCrossover` and `commitFreqEditor` can now leave their neighbours unspread when somebody else
  moved one. The layout is legal — the DSP re-orders — and the next edit resolves it.
* The partial-application residual is unchanged and is documented above, with the reopening trigger.

## Related code

`src/gui/SpectrumImager.cpp` — `setBands`, `setSoloMask`, `spreadSplits`, `resetCrossover`,
`commitFreqEditor`; `src/gui/SpectrumImager.h`.

## Evidence + confidence

**Verified.** State test 74 legs A, B, C, D, I(ii) and J(ii) fail before and pass after; positive
controls E, F, G, H, I(i), J(i) green throughout; State tests 66–73 unchanged and green. Mutations
M1–M7 each killed by exactly the intended leg. State suite 2 667 / 0, DSP 396 / 0.
`worklogs/SPECTRUMIMAGER_GESTURE_TOPOLOGY_AUDIT_v0.9.8.md` §§36-41.
