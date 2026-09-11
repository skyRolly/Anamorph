# ADR-0049 — A transaction proves both ends of every value it moves

**Status:** Accepted (maintainer instruction, 2026-09-09 — *final topology and gesture review round*,
review finding *"band removal discards concurrent edits"* at `SpectrumImager.cpp:1088`).

**Completes [ADR-0040](ADR-0040-a-gesture-stores-only-what-it-still-owns.md) and
[ADR-0042](ADR-0042-a-store-is-committed-only-when-the-parameter-says-so.md); supersedes nothing, and
deliberately does not disturb [ADR-0044](ADR-0044-a-transaction-owns-everything-it-has-written.md).**
ADR-0042 wrote the rule — *a plan is applied only to the world it was computed from* — and ADR-0040
placed the proof adjacent to the store. Both were implemented for the slot each store **writes**. A
removal does not write values; it **moves** them, and the slot each value is **taken from** was never
part of the proof.

## Context

`removeBand` computes its plan from one snapshot of the four widths and three splits, then applies it
as a burst of `setValueNotifyingHost` calls, each of which dispatches every listener synchronously on
this thread. The plan shifts everything at or above the removed index **down** one slot
(`SpectrumImager.cpp:1036-1038`):

```cpp
for (int k = 0, j = 0; k < N;     ++k) if (k != b)     nw[j++] = wd[k];   // nw[j] = wd[j+1] for j >= b
for (int k = 0, j = 0; k < N - 1; ++k) if (k != dropX) nf[j++] = fr[k];   // nf[j] = fr[j+1] for j >= dropX
```

so the store at iteration `k` writes slot `k` with what slot `k + 1` held at plan time. The guard in
front of it proved the destination and nothing else:

```cpp
if (bandCount() != expectedBands || ! juce::exactlyEqual (bandWidth (k), wd[k])) return;
```

## Problem

Each store's world has two ends and only one was proved.

* **A mid source** (slot `k + 1`, `k + 1 <= N - 2`) is proved by the *next* iteration's destination
  check — one store **after** the store that already consumed it, with that store's dispatch in
  between. The transaction does abandon, but the stale value has already landed.
* **The top source** — slot `N - 1` for the widths, slot `N - 2` for the splits — is never a
  destination in either loop, so it is proved **nowhere**. `bandWidth (N - 1)` is compared with
  `wd[N - 1]` at no point in the function.

The window is **reentrant**, not cross-thread-only: `setSoloMask`'s gesture open and value store come
first, and every `setParam` in the loops dispatches, so a single-threaded harness enters it with an
ordinary `AudioProcessorParameter::Listener`. That makes this the most reachable member of the family
— ADR-0046, ADR-0047 and ADR-0048 all close windows bounded by pure reads.

**The charge is misattribution, not loss.** The count store puts the top slot above the live topology,
where `MultibandWidth` (`MultibandWidth.h:53-56`, `.cpp:140`) and `SoloMonitor` do not read it: the
host's value is **parked** — inert while hidden, exact if the count returns — exactly as ADR-0039
records for the parked solo bit. What is wrong is what the **surviving** band receives: the pre-write
snapshot, moved down underneath a host edit that had already replaced it.

## Options

| | |
|---|---|
| **A. Prove the source in the same window as the store** (chosen) | Two comparisons, in the loops that already carry four. Nothing between them and `setParam`. |
| B. Re-prove the whole snapshot before every store | This is the *wide* fix ADR-0044's audit measured and rejected: it re-proves the written **prefix** too, which contradicts ADR-0042's measured disposition and makes legs B and C assert the wrong behaviour. |
| C. Copy the sources into locals and write from those | Changes nothing: the plan already holds copies. The defect is not a stale read, it is a store applied to a world that has moved. |
| D. Accept and document | The window is reentrant and a deterministic test enters it. An accepted residual is for what cannot be reached, not for what can. |

## Decision

> **A transaction proves both ends of every value it moves. A store that copies a value from one slot
> to another proves the destination still holds what the plan was computed against AND the source
> still holds what the plan took, in the same window as the store, with nothing between the
> comparisons and the write.**

Implemented as one comparison per loop, placed after the elision test — below the removal the plan
*is* the world, the store is elided, and there is no source to speak of, which is also why `k + 1` is
the right index wherever the line is reached at all.

**This does not reopen ADR-0044's asymmetry.** State test 76 legs B and C are controls: a width or a
split replaced mid-burst must **not** abandon, and the newer value must stand. Both write a slot the
transaction has **already** written — the prefix — and no guard added here re-reads the prefix. A
source is the opposite end: a slot the plan has not touched and is about to read. Both legs pass
unchanged.

**The sibling is deliberately left alone, and not because it is structurally safe.** `addBandAt`
shifts **up** and its loop runs up, so iteration `i`'s destination guard proves slot `i` at the last
instant it still holds what iteration `i + 1` will take from it — the source is covered at the only
moment it exists. What is not covered is a host write landing on slot `i - 1` *after* the
transaction's own store to it: `nw[i] = (i <= ins) ? wd[i] : wd[i - 1]` re-attributes that slot to a
different band. The symmetric guard **cannot** be added — `bandWidth (i - 1) == wd[i - 1]` fails on
the transaction's own store and would abandon every non-elided add — so it stays ADR-0042's measured
disposition, recorded here as an accepted residual rather than implied by silence.

## Consequences

* Two new abandoning slots per removal (`widthP[N - 1]` and `freqP[N - 2]`), in the fail-safe
  direction: the newer authority stands and the stale plan stops.
* Abandonment still leaves the stores already issued (ADR-0042/0044, unchanged), and an abandoned
  transaction never stores the count, so its residue glides in on the continuous path.
* An uninterrupted removal is untouched: State test 76 leg D still commits with every new guard
  passing.

## Related code

`src/gui/SpectrumImager.cpp` — `removeBand` (`:1117` onward, the two loops).

## Evidence + confidence

**Verified.** State test 76 **leg I** (a width source replaced from inside the first width store) and
**leg J** (a split source replaced from inside the first split store). Mutation record:

| Mutation | Killed |
|---|---|
| the width source proof removed | leg I only, 2 checks |
| the split source proof removed | leg J only, 2 checks |

State 2 840 / 0. `worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §45.
