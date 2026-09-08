# ADR-0044 — A transaction owns everything it has written, not just what it is about to write

**Status:** Accepted (maintainer instruction, 2026-09-08 — *audit all topology transactions, decide
whether the transaction semantics are sufficient, and choose between keeping the conditional-store
model, stronger transaction ownership, rollback, a snapshot/version architecture, or another
design*).

**Completes [ADR-0040](ADR-0040-a-gesture-stores-only-what-it-still-owns.md) and
[ADR-0042](ADR-0042-a-store-is-committed-only-when-the-parameter-says-so.md); supersedes nothing.**
Both were right and both were implemented in one direction only.

## Context

A topology edit is not one write. `addBandAt` issues up to nine stores and `removeBand` up to seven:
the solo word, then the widths, then the splits, and the band count **last**. Every one is a
`setValueNotifyingHost`, which dispatches every listener **synchronously from inside itself**
(`juce_AudioProcessorParameter.cpp:59-63`, `:111-121` → `juce_AudioProcessor.cpp:1467`), so a host
can write any parameter back from inside any of them.

ADR-0040 answered that with *"the burst is re-validated before every store"* and ADR-0042 with
*"a store is committed only when the parameter says so"*. Read together they sound complete. They
are not, because **both look forward**:

* ADR-0040's check proves the store that is **about to happen**, against the snapshot its plan came
  from — `bandCount() != N`, `bandWidth (i) != wd[i]`, `crossover (i) != fr[i]`.
* ADR-0042's check proves the store that **just happened**, against its own target —
  `setBands`/`setSoloMask` re-read after `endChangeGesture`, `storeOwned` re-reads after its store.

Nothing proves a value the transaction committed **earlier**. Once `setSoloMask` has returned true,
no later check reads `mbSolo` again; once a width has been stored, no later check reads it again.
A listener that replaces one of them from inside a **later** store's dispatch is therefore invisible
to every remaining check — and the count, written last, commits on top of it.

## Problem

The count is not just another value. `SoloMonitor::process` masks the solo word with
`((1 << bands) - 1)` (`src/dsp/SoloMonitor.cpp:85`) and `MultibandWidth` reads only the prefix of
splits and widths that the count selects. **The count is what reinterprets everything else**, so
committing it over a prefix the transaction no longer owns is precisely *"a new band count with
values still numbered for the old topology"*.

Reproduced deterministically on `91e20d9`, State test 76, a removal at N = 4 with the probe firing
from inside a **later** store of the same transaction:

```
[leg A] Bands 3 with mask 0x8       -- a word written for FOUR bands; SoloMonitor then masks it
                                       with 0b0111 and the soloed band is simply gone
[leg B] Bands 3 with width0 1.500   -- the plan wrote 0.500; band 0 carries the removed band's width
[leg C] Bands 3 with split0 200.0   -- the plan wrote 2000.0; a three-band count over a four-band row
```

The in-source comment claimed the opposite — *"the transaction cannot go on to change the band count
with the mask still in the old numbering"*. That was true only of a change landing **before** the
mask store, which is the case its guard tests. It is corrected in place rather than deleted.

## Options

* **A. Keep the conditional-store model and document the residual.** Rejected. The three
  measurements above are not a residual: they are a **completed** transaction publishing a layout
  that never existed. Documenting a wrong commit does not make it right.
* **B. Stronger transaction ownership.** Chosen, but only after the round's own audit cut it down:
  the first implementation proved the whole written prefix (mask, widths and splits) and was wrong
  for the widths and splits — see *The first attempt* below.
* **C. Rollback / compensating writes on abandonment.** Rejected, and for the reason ADR-0042
  already gave for option E: a compensating write puts the transaction's own older value back over a
  **newer authority**, which is exactly the stale overwrite ADR-0036 §25 forbids; and it cannot
  terminate deterministically, because the rollback's own stores dispatch and can be answered again.
* **D. Snapshot / version architecture — publish a coherent layout as one versioned unit.**
  Rejected here, not on taste but on reach: `PluginParameters::toEngine` reads the ten multiband
  atomics with ten separate `load()` calls per block (`src/PluginParameters.cpp:365-374`), so the
  **reader tears** and a versioned write-side unit would be re-torn on the audio side. Making it
  real means replacing the read as well — a threading-model and DSP-parameter change, and so an
  `ARCHITECTURE_REVIEW_GATE` item, not a review-round fix. Recorded, not attempted.
* **E. Something narrower than B.** This is what actually shipped: no transaction object, no token,
  no new state, and no change to the width and split leaves — one integer comparison on the one
  value whose meaning the count store changes, carried into the count store's own bracket.

## Decision

> **The solo word is re-proved all the way to the count store, and the count store proves it inside
> its own gesture bracket. The widths and the splits deliberately keep ADR-0042's disposition.**

**Why only the mask.** The count is not just another value: it is the one that *reinterprets*
another. `SoloMonitor::process` masks the solo word with `((1 << bands) - 1)`, so a word written for
four bands and committed under three silently loses a soloed band — the parameter still reads
`0x8`, the UI still shows band 3 soloed, and nothing is soloed. `mbWidthLow`, by contrast, means
band 0's width under **either** topology, and `mbFreqLow` is split 0 under either; a host writing
them mid-burst is a newer authority on a value the count does not reinterpret. That asymmetry is the
whole of this decision.

**Two mechanisms, and they cover different windows.** This was not obvious and the mutation matrix
is what separated them:

* `soloMask() != nm` **before every store** in both loops. Its job is not the commit — see below —
  it is to stop the transaction **at** the divergence instead of running on to the end.
* `setBands (n, expectedBands, expectedMask)`. `setBands` calls `beginChangeGesture` **before** its
  guard, and that dispatch reaches every listener, so a host answering the gesture open is invisible
  to any check the caller makes outside. The guard now proves the mask between the open and the
  store, exactly the shape `setSoloMask` has carried since ADR-0041, and for the same reason: the
  check and the store it guards must have nothing between them.

**What each mutation kills, which is how the two were told apart:**

| Mutation | Killed by |
|---|---|
| Q1 — the in-loop `soloMask() != nm` checks removed | **leg F only.** Leg A still passes: `setBands`'s own guard catches the mask. The in-loop checks buy residue reduction, not commit correctness, and leg F is the only thing that measures it |
| Q2 — `setBands` stops proving `expectedMask` | **leg G only** — the window a caller-side check cannot reach |
| Q3 — the call sites pass `-1` instead of `nm` | **leg G only** — same window, from the other side |
| Q4 — both mechanisms removed | **legs A, F and G.** Leg A is doubly covered, so only removing both reproduces the original measurement |

## The first attempt, and what the audit did to it

The first implementation of this decision was **wider**: it recorded the read-back of every store
and re-proved the whole prefix — mask, widths and splits — before each subsequent store, converting
the width and split leaves from `setParam` to `storeOwned` so their read-backs were available. It
passed its own tests. The round's own adversarial audit then found two things wrong with it, and
both are recorded here rather than quietly dropped:

1. **It contradicted an Accepted ADR on a measured point.** ADR-0042 states that `setParam` stays
   `void` because aborting at a width leaf was *measured* to be worse — `Bands 3 mask 0x5 wLo 1.750`,
   the intended layout with the newer authority's width standing — since the mask is stored first
   and is only correct once the count changes. Aborting the transaction because a host wrote a
   width leaves the **old count** with an **already-remapped mask**, which is the very incoherence
   this ADR is about. The wide fix made two of its own regression legs assert that wrong behaviour.
2. **It left the one window it most needed to close.** The prefix proof sat immediately before
   `setBands`, outside the gesture bracket, so a host answering `beginChangeGesture` still committed
   a count over a mask the transaction no longer owned. That is now leg G, and it fails on the wide
   fix.

Legs B and C were rewritten from assertions into **controls**: a width or a split replaced mid-burst
must *not* abandon the transaction, and the newer value must stand. They are what keeps the
asymmetry from being "fixed" again by a later round.

## What this does NOT close, stated plainly

**Abandonment still leaves the stores already issued**, and that is still deliberate — the
alternative is option C, and completing instead would put the old layout back over a newer
authority. What changes is the shape and the size:

| | before | after |
|---|---|---|
| a foreign **mask** write mid-burst | **new count** committed over a word in the old numbering | count refused; the newer mask stands |
| stores issued after the divergence is observable | the rest of the transaction (measured: three, leg F's shape) | none — the next check returns |
| a foreign **width or split** write mid-burst | transaction completes, newer value stands | **unchanged** — ADR-0042's measured disposition, now covered by legs B and C |

The DSP is what makes the remaining residue survivable, re-derived rather than assumed:
`MultibandWidth::setCrossovers` clamps every split to `[20 Hz, 0.45·sr]` and forces strict `1.1×`
ordering before use; `SoloMonitor::process` masks the solo word with the live count;
`setBandCount` clamps to `[1, 4]`. A partial layout is audibly wrong and repaired by the next edit —
never illegal, never NaN, never unbounded.

## Consequences

* An add or a removal whose solo word is taken over mid-burst now leaves the **old** count standing,
  so the host's word is still read under the topology it was written for.
* The transaction abandons at the divergence, so fewer of its stores reach the host's automation
  lanes and the undo history for an operation that did not complete.
* `setBands` gains a third parameter, defaulted to "do not prove it"; no other caller changes.
* Cost: one integer read per store on the message thread. No lock, no allocation, no blocking, no
  audio-path change, no parameter-model change.
* **Not a gate item:** no parameter ID rename or removal, no serialization-schema change, no
  threading-model change, no DSP signal-order change, no reported-latency change. ADR-0042 is
  **completed and narrowed with the measurement that narrows it** — its leaf reasoning was right for
  the widths and splits and silent about the mask, which this ADR supplies.

## Related code

`src/gui/SpectrumImager.cpp` — `setBands`, `addBandAt`, `removeBand`;
`src/gui/SpectrumImager.h` — `setBands`'s `expectedMask`.

Evidence [Verified]:
- Reproduction: `tests/state_tests.cpp` State test 76 legs A, F and G fail on `91e20d9` and pass
  after — leg A measured `Bands 3 with mask 0x8`, leg F `split0 2000.0` where untouched is 200.0,
  leg G `Bands 3 with mask 0x8` from inside `setBands`'s own gesture open. Legs B, C, D and E are
  the controls (a width or split replaced mid-burst still completes; an uninterrupted removal and
  add still commit) and are green throughout. Mutations Q1–Q4 as tabulated above.
- Dispatch mechanics: `juce_audio_processors_headless/processors/juce_AudioProcessorParameter.cpp:59-63`,
  `:82`, `:111-121`; `juce_AudioProcessor.cpp:1467`.
- DSP clamping: `src/dsp/MultibandWidth.cpp` (`setCrossovers`, `setBandCount`);
  `src/dsp/SoloMonitor.cpp:85`.
- The torn reader that rules out option D: `src/PluginParameters.cpp:365-374`.
- Suites: state 2 733 / 0, DSP 396 / 0.
