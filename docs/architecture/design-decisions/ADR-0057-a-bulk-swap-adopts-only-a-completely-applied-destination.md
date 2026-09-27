# ADR-0057 — A forced bulk swap adopts only a completely applied destination

**Status:** **Proposed** (2026-09-27; revised the same day by the O8(3) architecture round, worklog §U).
**Architecture Review Gate: TRIGGERED. Implementation deferred. No production file implements any part
of this protocol.**

**The gate record.**
- **What is gated.** `ARCHITECTURE_REVIEW_GATE.md`, *Thread Model change — new thread, new cross-thread
  path, new atomic ordering*. The protocol below adds:
  - a new atomic ordering to the existing message → audio request word: the completion is published
    with `release` and taken with `acquire`, where every access is relaxed today;
  - a new message on that path: a sequence number and a completion;
  - a second per-block take of the word, before the parameter snapshot is read.

  `THREADING_POLICY.md` (*Enforcement*): *"A change to the thread model, a new shared-state path, or a
  new atomic ordering triggers the Architecture Review Gate and an AI Agent Hard Stop. Changing this
  policy requires an ADR."* This is that ADR.
- **The Accepted records it amends.**
  - ADR-0007, Amendment of 2026-09-25 (A/B provenance): *"The handoff: one existing atomic, no new
    path"* and its gate record, *"The thread model gains no thread, no direction and no ordering"*.
  - ADR-0036 §24 and §25 item 5: the residual those sections accepted, for forced swaps.
  - ADR-0004, decision 1: a forced swap still defers everything to its bottom, but its duck starts at the
    completion, not at the request.
- **What approval is required.** `ARCHITECTURE_REVIEW_GATE.md` §Procedure, steps 2–3: a human reviewer
  with DSP/audio context reviews this ADR against `THREADING_POLICY.md`, `REALTIME_AUDIO_POLICY.md` and
  the three ADRs above. The decision is recorded by setting this ADR's **Status** to **Accepted**, with
  the date and the reviewer, in the change that implements it. That change must not be auto-merged.
  Step 4 (the release compatibility checklist) is not triggered: no parameter, serialization or
  reported-latency change.
- **What does not approve it.**
  - The round's delegation. It is *"authorized to make owner-level semantic/product decisions"*, and
    *"NOT authorized to silently override an existing threading-model hard stop"*.
  - A green build, or the prototype evidence below. `AI_AGENT_POLICY.md`: *"A passing
    build/test/pluginval does not clear a Hard Stop — only human review does."*
- **Until it is approved:** `KNOWN_ISSUES.md` KI-032 stays open.

## Context

A **forced bulk swap** replaces the whole sound behind a masking duck:
- an A/B switch (`AnamorphAudioProcessor::abSwitchToAdopted`, `src/PluginProcessor.cpp:2177`);
- undo and redo (`src/PluginProcessor.cpp:2075`, `src/PluginProcessor.cpp:2106`);
- a user or factory preset load (`presets.onAboutToLoad`, `src/PluginProcessor.cpp:83`, then the write loop,
  `src/PresetManager.cpp:845` to `src/PresetManager.cpp:898`).

Each raises the duck first and then writes the sound one parameter at a time. The A/B switch requests
the switch (`src/PluginProcessor.cpp:2186`), captures the slot it leaves (`src/PluginProcessor.cpp:2187`)
and applies the destination (`src/PluginProcessor.cpp:2189`). The apply is JUCE's `replaceState`: one
store per parameter whose value changes, then `reassertParameters` (`src/PluginProcessor.cpp:1113`).

The contract the duck exists for (feedback #1, ADR-0004) is that a bulk swap is applied *entirely* at
the silent bottom, continuous controls included, with the smoothers snapped. The engine keeps the source
live through the ~6 ms fade-out. Every block, `processBlock` reads the parameters into a snapshot
(`src/PluginProcessor.cpp:447`), then `setParameters` takes the request word
(`src/dsp/AnamorphEngine.cpp:743`). On every block of the fade-out the engine records that snapshot
(`pendingP = np`, `src/dsp/AnamorphEngine.cpp:826`). At the bottom it adopts the last one
(`p = pendingP`, `src/dsp/AnamorphEngine.cpp:1317`) and judges the destination's A/B Level-Match record
against it (`src/dsp/AnamorphEngine.cpp:1407`). A host reset adopts it too
(`src/dsp/AnamorphEngine.cpp:240`), and the prime of `prepareToPlay` adopts its own snapshot wholesale
(`src/dsp/AnamorphEngine.h:123`).

The request word is **relaxed** and carries no completion (`THREAD_MODEL.md`, the request-word row). So
the adoption point is fixed in *audio* time, two blocks after the request is taken at 256 samples. The
application's end is fixed in *wall* time: ~0.1–0.3 ms here, and unbounded behind host notifications,
contention or preemption. Nothing relates the two.

ADR-0036 §24 accepted that *"a reader that samples parameters while a replacement is running still sees
part of the outgoing sound and part of the incoming one — JUCE offers no block-atomic parameter
snapshot and never has … What changes is that the mixture cannot SETTLE."* The Devin review reports the
forced-swap form as a defect: *"A/B switches adopt incomplete slot states"* (`src/PluginProcessor.cpp:2186`;
first raised on `9e38310`). It is one.

## Problem (measured on the branch head `e49a90e`, whose `src/` is `9e38310`'s)

**Reproduced through the processor** (worklog §U1; x86-64 Linux, Release, 48 kHz / 256). The message
thread calls `abSwitchTo` while an audio thread runs `processBlock` at a set multiple of real time.

The probe records per trial:
- the request's publication and every destination write, timestamped where JUCE stores the raw value the
  audio thread reads;
- the block that took the request and the forced bottom's snapshot, compared field by field with both
  slots;
- the restored Level-Match answer, and the output against a twin switched completely at the same block.

Six combinations of changed parameters, 40 trials per combination and speed, 1,680 trials. Each cell is
complete / hybrid / source-only:

| combination | 1× / 4× | 16× | 64× | 112× | 256× | free-running |
|---|---|---|---|---|---|---|
| Drive + Width | 40 / 0 / 0 | 40 / 0 / 0 | 39 / 0 / 1 | 3 / 8 / 29 | 0 / 0 / 40 | 0 / 0 / 40 |
| Mix + Amount | 40 / 0 / 0 | 40 / 0 / 0 | 40 / 0 / 0 | 16 / 7 / 17 | 1 / 2 / 37 | 1 / 0 / 39 |
| Haas delay + side | 40 / 0 / 0 | 40 / 0 / 0 | 38 / 0 / 2 | 1 / 0 / 39 | 0 / 0 / 40 | 1 / 0 / 39 |
| Haas → Chorus + rate + depth | 40 / 0 / 0 | 40 / 0 / 0 | 36 / 0 / 4 | 10 / 5 / 25 | 0 / 1 / 39 | 1 / 0 / 39 |
| Multiband bands + split + widths | 40 / 0 / 0 | 39 / 0 / 1 | 40 / 0 / 0 | 39 / 1 / 0 | 39 / 0 / 1 | 39 / 0 / 1 |
| Eight fields | 40 / 0 / 0 | 40 / 0 / 0 | 39 / 1 / 0 | 3 / 11 / 26 | 0 / 0 / 40 | 2 / 0 / 38 |

The Multiband combination stays mostly complete because its blocks are heavier: its fast speeds are
much lower multiples of real time.

- **Totals.** 573 of 1,680 trials adopted an incomplete destination: 36 hybrids and 537 source-only.
- **Timings (medians).** The request is published +51–92 µs after the call and the first destination
  write lands +117–166 µs after it. The request is taken at the next block and the bottom's snapshot is
  read two blocks later:
  - at 1× this is +13.3 ms, after the writes;
  - at 112×, +0.12 ms, inside them;
  - at 256×, +0.07 ms, before the first write.
- **The Devin case.** At 112×, Drive + Width: the request was taken at +27.5 µs and the bottom read its
  snapshot at +122.5 µs. B's Drive had been written at +122.4 µs; B's Width was written only at +131.0 µs.
  The bottom adopted Drive 12 with Width 1.
- **At real time.** One 12 ms message-thread stall after the first destination write gave a hybrid in 40
  of 120 trials at 1×: 6–7 of 20 in every combination.
- **Level Match.** Every incomplete adoption restored B's measured record as not measured: 573 of 573,
  and 40 of 40 under the stall. The record was judged against the adopted state. A same-rate
  `prepareToPlay` then threw the published level away in 368 of the 573.
- **Output.** Against the complete twin, the fade-in differs by −19.4 to +3.1 dB (the maximum, relative
  to the twin's signal). It was never bit-identical again within 24 blocks after the bottom.
- **Every adoption path, deterministically** (worklog §U4). Audio blocks, host resets and re-prepares were
  run inside the writes at every write position. The head adopted a state neither side holds on every
  path: the A/B switch, undo, redo, a factory preset, a host reset, the prime, a supersession inside a
  fade, and a Copy followed by a switch.
- **Not a data race.** Every shared access is a `std::atomic`. The defect is **snapshot consistency**:
  every intermediate snapshot is a valid state, and the bottom adopts one neither slot holds.

**Root cause.** The request is the only message, and it precedes the writes. The engine adopts, at a
point it chooses in audio time, the snapshot of whichever block reaches that point. Nothing tells it
the destination is complete.

## The invariant (owner decision, 2026-09-27)

**A forced bulk swap adopts the complete destination or nothing.** Wherever a pending forced swap is
adopted (its bottom, a host reset that completes it, the prime), the state adopted is one of two:
- the destination exactly as its application wrote it; or
- the source, still live, with the swap still pending.

It is never a mixture, and never the source taken for the destination. The destination's Level-Match
provenance is judged against the complete destination.

## Options compared

Every correct option needs the audio thread to learn, with *acquire* semantics, that the application
ended. No existing message → audio path gives it one:
- the three ordering-critical pairs of `THREADING_POLICY.md` run audio → GUI (the scope ring), audio /
  host → message (D-1) and host ↔ message (D-2);
- the engine-config word is relaxed on the audio side;
- the APVTS values are sequentially consistent, but each orders only itself, and no parameter marks the
  end of an application.

- **B2 (recommended): the two-phase request word; the swap starts at its completion.** The existing
  word carries a 4-bit sequence and a completion stored with `release`. The audio thread takes the word
  with `acquire` twice per block, before and after reading the snapshot. While a swap's completion is
  absent the engine adopts nothing. The forced duck starts only on a snapshot read between two acquires
  after that completion, so its bottom never waits. The exact protocol is under *Decision*.
- **B2-H: the same word, with the bottom holding.** This is the version of this ADR at `9eea7c4`. The
  duck starts at the request, as today, and the bottom holds (silent or dry-filled) until a snapshot read
  after the completion; a cap aborts to the source.
- **A: a separate completion atomic** (the sequence of the last complete swap, `release` store /
  `acquire` load), with the request word unchanged.
- **C: an immutable destination snapshot handed over**, through a D-2-style exchange cell; the engine
  adopts that object at the bottom.
- **D: an existing mechanism.** None qualifies. These were each measured or argued (worklog §T4):
  - the engine holds while the snapshot keeps changing (no guarantee);
  - the destination's A/B record as the reference (fails after a Copy, a restore or a first visit);
  - requesting after the writes (unmasked writes, a mixed leaving capture);
  - `suspendProcessing` (a lock on the audio thread);
  - a second duck (the first bottom still adopts);
  - a content hash (automation defeats it).

  Each fails the invariant or real-time safety, and a correct D would need a new ordering anyway.

| criterion | B2 (recommended) | B2-H | A | C |
|---|---|---|---|---|
| correctness | proved below; 0 incomplete adoptions measured | proved the same way; 0 measured (A/B only) | as B2, if both atomics are read in the right order | the object is complete, but the live parameters are still written one by one: a C that does not also freeze adopts the late writes as edits |
| memory ordering | release on the completion RMW, acquire on both takes, one existing word | the same, one take | a second atomic, plus a required read order between two atomics | acq_rel exchange of a pointer, plus the completion |
| ownership | ~24 bytes of audio-thread engine state; the word keeps its writers | ~16 bytes | + one atomic | an object crosses threads and must be returned to be freed |
| real-time safety | O(1) per block, no lock, wait or allocation | the same | the same | the same, but only with a pool or a return channel |
| offline / burst | the source plays through the writes; the swap lands at its completion, bit-identical to a switch made there | the silent or dry bottom lengthens (median 1–7 blocks at ≥64×, 66 max at 256×) | as B2 or B2-H | as B2 |
| normal real time | bit-identical to today when the writes end between blocks (every existing test) | the same | the same | the same |
| repeated requests | one stash: the first source, the newest destination | re-arms at each request | needs a sequence too | one object per request; superseded ones must be reclaimed |
| supersession | the newest sequence is awaited; older completions are ignored | the same | the same, as a counter | the exchange replaces the pending object |
| host reset | unchanged: it adopts `pendingP`, which is always trusted | must not complete; keeps the source pending | as B2 or B2-H | adopts the object if one is pending |
| prepare (prime) | adopts only a trusted snapshot; keeps the source while a swap is written | keeps the source | as B2 | could adopt the object |
| undo / redo / preset | the same protocol at their three call sites | the same | the same | the processor builds the object at each site |
| Copy | writes nothing, requests nothing: unaffected | unaffected | unaffected | unaffected |
| S5 A/B evidence | the A/B bookkeeping runs at the start, against the unchanged source: the same record | at the request, as today | as B2 | as B2 |
| Level-Match provenance | judged against the complete destination; measured records stay measured | the same | the same | the same |
| CPU cost | one more acquire RMW per block, and branches | the exchange becomes acquire | one acquire load per block | an acquire exchange per block, a copy at adoption |
| memory cost | ~24 bytes | ~16 bytes | + 4 bytes | ~150 bytes per request, plus a pool |
| allocation | none (measured: 0) | none | none | per request on the message thread, and a deferred free |
| testability | a single-threaded injection covers every interleaving; all five negative controls observable | the same, plus the cap's timing | similar | more states |
| failure behaviour | a missing completion stops parameter adoption, with the source playing, until the next completion; a scope guard pairs every request | the cap aborts to the source, and the swap stays pending: the same dependence | the same | the same |
| complexity | ~130 engine lines, ~30 processor lines | ~150, with the cap's fade-back and re-arm | B2 plus a second atomic | the largest |

**Selected: B2.** It is the narrowest design that satisfies the invariant:
- one existing word on one existing path;
- no new thread, object, lock or cap;
- the only added ordering is the pair the invariant needs.

Moving the wait from the bottom to before the fade-out removes B2-H's silent hold and its cap: the
source is the fallback for the whole pending interval, not only after a timeout.

**Two intermediate designs were rejected** (worklog §U3).
- **Starting the fade at the completion, with the snapshot read before the only acquire.** The
  completion's own block cannot be trusted, so a host reset in that block had to defer its adoption. That
  changed established behaviour. 22 State checks failed: 13 of State test 139's, 7 of State test 127's
  (*"a host reset inside an A/B, preset, undo or redo swap lands settled"*) and 2 of State test 130 (7).
- **Arming the fade one block after the completion instead of reading between two acquires.** It delays
  every swap by a block and moves the timing of every existing test.

## Decision (recommended architecture; implementation deferred by the gate)

### The word

`duckRequest` stays one `std::atomic<int>`, and every modification stays a read-modify-write:
- bits 0–11 as today: duck, switch, forget, and the two slot indices;
- **bit 12:** the completion flag;
- **bits 13–16:** the request's sequence *s* (1–15);
- **bits 17–20:** the completion's sequence.

Sequence 0 is the engine API: complete by construction, because the caller hands the whole state to
`setParameters`, and it behaves exactly as today.

### The protocol (the eleven points)

Message thread, for every bulk apply (an A/B switch, undo, redo, a preset load):
1. **Request publication (R).** `requestAbSwitch (from, to, s)` / `requestDuck (s)`: today's relaxed
   CAS, which also stores the processor's next *s* (1–15, wrapping). It is published where it is today:
   before the leaving capture and before the first write.
2. **The application begins** at the first parameter store: the first `setValueNotifyingHost` inside
   `replaceState`, or the preset's first write.
3. **The application completes** at the last store:
   - the end of `reassertParameters` (A/B, undo, redo; `src/PluginProcessor.cpp:1113`);
   - the preset write loops (`src/PresetManager.cpp:871` for a factory preset, and
     `src/PresetManager.cpp:575`, the end of `applySoundTree`).
4. **Completion publication (C).** `completeBulkApply (s)`: a CAS that stores the flag and *s* with
   **`memory_order_release`**. A scope guard publishes it after the last store on every exit path,
   exceptions included.

Audio thread (every `processBlock`; the prime of `prepareToPlay` the same):

5. **Acquisition.** `acquireRequests()` takes the word with `exchange (0, memory_order_acquire)` before
   `toEngine` reads the snapshot (E<sub>a</sub>). `setParameters (np)` / `primeParameters (np)` take it
   again the same way after the read (E<sub>b</sub>). Each take processes the word:
   - a request with *s* ≠ 0 sets `awaitSeq = s`;
   - an A/B switch is stashed, keeping the first source slot and taking the newest destination;
   - a completion flag whose sequence equals `awaitSeq` clears it and sets `startPending`;
   - sequence 0 is applied as today, or joins the stash while a swap is awaited.

   **The snapshot is trusted** only if no swap was awaited after E<sub>a</sub> and E<sub>b</sub> took
   no new request.
6. **When the forced bottom may adopt: always.** A forced duck is only ever started on a trusted
   snapshot. At the first block whose snapshot is trusted while `startPending`, the engine applies the
   stash and begins the forced duck with that snapshot as its target. The stash is the A/B bookkeeping:
   the leaving slot is recorded against the source, unchanged since the request. A mid-duck retarget is
   also a trusted snapshot. So the bottom adopts a complete state, and so does a host reset, which adopts
   `pendingP`.
7. **The source is retained while the completion is absent.** While a swap is awaited, or the snapshot
   is untrusted, `setParameters` returns right after E<sub>b</sub>: no live edit, no duck entry, no
   `pendingP` write. The engine renders the adopted source, and any duck already in flight lands on its
   trusted target. The prime keeps the source the same way.
8. **The hold limit is zero at the forced bottom.** The wait happens before the fade-out, with the
   source at full level. The pending interval is the application's wall time converted to blocks, plus
   at most one block.
9. **When a bound is reached: there is none, by design.** An audio-time cap can only do one of two
   things when it fires:
   - adopt the snapshot, which may be incomplete, which the invariant forbids; or
   - keep the source, which the pending state already does.

   B2-H's *"abort to the source"* is therefore the pending state itself. Liveness is guaranteed on the
   message thread instead: every request is paired with its completion (point 4).
10. **A completion that arrives late** (after any length of pending interval) starts a fresh forced swap
    from the source to the complete destination. The A/B bookkeeping runs at that start.
11. **A newer request supersedes the old one.**
    - The engine awaits the newest *s*.
    - An older completion is ignored. The word keeps one completion slot, the latest, and the message
      thread's swaps are sequential (the admission gate queues a re-entrant one), so a stale completion
      never carries the awaited *s*.
    - The stash keeps the first source slot and takes the newest destination.
    - There is one start, from the source to the newest complete state.
    - A forced duck that an earlier completed swap already started continues to its trusted target and
      lands. The new start re-ducks from the fade-in, or retargets during the fade-out, as today.

**The real-time bound, exactly.** Per block the audio thread performs two `exchange`s and a constant
number of branches. It never waits, spins, locks or allocates. The output during a pending interval is
the source, or the duck already in flight. It is bit-identical to a twin that received the swap at its
completion (measured, below).

**Liveness.** A scope guard pairs each request with its completion:
- the processor's A/B switch, undo and redo use a local guard;
- a preset load publishes its completion from a guard around the write loop in `PresetManager`, extending
  its existing contract *"never an onAboutToLoad() with no matching onLoaded()"*
  (`src/PresetManager.cpp:794`).

A request whose completion never came would freeze parameter adoption (the source keeps playing) until
the next bulk swap's completion, which supersedes it.

### Why the ordering is sufficient (proof)

Notation:
- the message thread M performs R (relaxed RMW), then W<sub>1</sub> … W<sub>n</sub>, then C (release
  RMW);
- the W<sub>j</sub> are JUCE's stores to `ParameterAdapter::unnormalisedValue`, a `std::atomic<float>`
  assigned with the default `seq_cst`;
- the audio thread A performs, per block, E<sub>a</sub> (acquire RMW), then L (`toEngine`'s
  `->load()`s, `seq_cst`), then E<sub>b</sub> (acquire RMW).

**Claim 1 (completeness).** If E<sub>a</sub> of this block, or an earlier take of A, reads C's value or
any later value of the word, every W<sub>j</sub> happens-before L.
- Every modification of the word is a read-modify-write (CAS, `fetch_or`, `exchange`; no plain store
  exists). So every later modification is in C's **release sequence** ([intro.races]).
- An acquire operation that reads a value from that sequence synchronizes with C ([atomics.order]).
- W<sub>j</sub> is sequenced before C, and the take is sequenced before L. So W<sub>j</sub> happens
  before each load in L.
- By write-read coherence, each load reads W<sub>j</sub>'s value or a later one in that parameter's
  modification order. A later value is a later live edit or a newer swap's write; Claim 2 excludes the
  newer swap's.

**Claim 2 (nothing newer).** If a load in L reads a store W′ of a newer swap, whose request R′ is
sequenced before W′, then this block's E<sub>a</sub> or E<sub>b</sub> takes R′.
- W′ is a `seq_cst` store (a release) and the load is `seq_cst` (an acquire). Reading W′ makes W′
  synchronize with the load, so R′ happens-before the load, and hence before E<sub>b</sub>.
- By write-read coherence, E<sub>b</sub> reads R′ or a later value.
- Only A's own takes clear a request. Every writer keeps a pending request's duck bit and sequence; the
  forget CAS drops only a switch's slot indices, by design.
- So either E<sub>a</sub> already read R′, and the swap was awaited after E<sub>a</sub>, or
  E<sub>b</sub> sees R′'s bits. Either way the snapshot is untrusted.

**Claim 3 (trusted means complete).** Take a trusted snapshot. No swap is awaited after E<sub>a</sub>,
so every swap whose request was taken has had its completion taken, at or before E<sub>a</sub>. By
Claim 1 all their writes are in L, and by Claim 2 no newer swap's write is.

A superseded swap's writes are in L too. Its completion precedes the next request on M, and every later
completion is a read-modify-write in its release sequence. What remains are live edits, which is today's
semantics.

**Claim 4 (only trusted snapshots are adopted).** By construction:
- `setParameters` and the prime return before any use of an untrusted `np`;
- `pendingP` is only ever assigned a trusted snapshot;
- the bottom and a host reset adopt `pendingP`.

**Why a relaxed completion (or a relaxed take) is insufficient.**
- **In the C++ model.** A relaxed RMW is not a release, and a relaxed take is not an acquire, so no
  synchronizes-with edge exists. E<sub>a</sub> may read C's flag while L still reads the pre-W<sub>j</sub>
  values. The model permits that outcome, and a compiler may move the later relaxed RMW above the
  earlier stores, even on x86-64.
- **On ARMv8** (Apple Silicon, a shipped target), where `seq_cst` stores compile to `STLR` and loads to
  `LDAR`:
  - `STLR` orders the accesses **before** it, but does not order itself before a **later** plain store.
    A relaxed C (`STXR`, or a relaxed `CAS`) can become visible to A before W<sub>n</sub>.
  - An `LDAR` in L orders only the accesses **after** it. So with a relaxed E<sub>a</sub> (`LDXR`), L's
    loads may be satisfied before E<sub>a</sub>'s.
  - With C as `STLXR` / `CASL` and E<sub>a</sub> as `LDAXR` / `SWPAL`, both orders hold.
  - Claim 2 holds on ARMv8 because `STLR` orders R′ before W′; the architecture is other-multi-copy
    atomic; and L's `LDAR` orders E<sub>b</sub> after it.
- **x86-64 hides all of this.** It is TSO, and every RMW is a locked instruction, so no x86-64 run can
  confirm or refute the argument. The evidence below does not rely on one: the orders are argued here,
  and the logic is tested with the orders removed.

**The preconditions**, to be pinned in `THREADING_POLICY.md` on acceptance:
1. Every store of a bulk apply is sequenced before its completion, on the thread that publishes it. The
   applications are synchronous on the message thread.
2. Every audio-thread read of a swap-carried value is an acquire or stronger: `toEngine`'s `->load()`.
   A relaxed read there would break Claim 2.
3. Every modification of the word is a read-modify-write.
4. Exactly one completion follows each request, after its last store, on every exit path.
5. The processor takes the word before building each snapshot it hands to `setParameters` or
   `primeParameters`, and never reuses one across two engine calls. `prepareToPlay`'s trailing
   `setParameters` re-acquires and re-reads; today it reuses the prime's snapshot.

The oversampling factor is read relaxed (`src/InternalState.h:175`), but it is a Setting that never
swaps with a slot (#13/#15), so no swap-carried value depends on it.

## Evidence (scratch prototype, not in the tree; worklog §U2–§U6)

The prototype is B2 exactly as above, built from a copy of `src/`: ≈160 lines of protocol code (the
engine ≈130, the processor ≈30), plus counters and the negative controls' switches. It covers:
- the engine;
- `processBlock` and `prepareToPlay`;
- the A/B switch, undo, redo, and both preset hooks.

Every correctness check, deterministic and threaded, asserts after **every block, host reset and
re-prepare** that the engine's adopted state is one of the command's complete states.

- **The existing suites.** DSP 935 / 0, with output identical to the head's.
  - State: 5547 checks, 13 failures. All 13 are State test 139 checks, each describing an adoption inside
    the writes. The rest of the output is identical to the head's, except the thread-timing counters and
    wall-clock timings of State tests 38, 39, 41, 62, 113 and 116.
  - State test 127 and State test 130 (7) are unchanged.
  - In (B), B's record is kept, measured, at every write position (−8.8130 dB).
- **Deterministic, every interleaving** (single thread; blocks, resets, re-prepares and snapshot reads
  run inside the writes at every write position):

  | build | cases | with an incomplete adoption |
  |---|---|---|
  | head (`e49a90e`) | 1,245 | 689 |
  | B2 prototype | 1,245 | **0** |

  - Every prototype case also ends on the right state and leaves no swap awaited or pending.
  - Every A/B landing restores the destination's measured record as measured.
  - Output bit-identical to a twin that made the command atomically at its completion:
    458 of 458 twin comparisons (A/B, slow writes, undo, redo, preset).
  - Coverage: A/B at every write position with 1–8 blocks there; a block after every write (slow writes);
    repeated alternation; undo; redo; two factory presets at every write; a host reset and a re-prepare at
    every write; a snapshot read before the completion handed in after it (then processing, a host reset,
    or the prime); a completion and the next request in one word; a supersession inside a fade; Copy.
- **The Phase-1 probe itself, run on the prototype** (the same 6 combinations × 7 speeds × 40 trials):
  - 1,680 of 1,680 bottoms adopted the complete destination; the head's run had 573 incomplete;
  - every trial was bit-identical to the twin that switched completely at the block that started it;
  - B's record came back measured in every trial, and a same-rate `prepareToPlay` kept it in every
    trial; the head flushed 368;
  - behind the 12 ms stall at 1×: 120 of 120 complete; the head had 40 hybrids;
  - the longest pending interval was 53 blocks (unpaced).
- **Threaded** (1×, 4×, 16×, 64×, 112×, 256× and free-running; A/B, repeated, back-to-back superseding
  switches, undo, redo, a preset, Copy, and host resets and re-prepares on the audio thread):
  - without a stall: 4,620 trials, 0 incomplete adoptions, 0 wrong final states, 0 stuck;
  - with a random stall of up to 12 ms at a random write: 4,620 trials, the same;
  - the head: 567 of 2,310 trials with an incomplete adoption.

  The longest pending interval was 648 blocks, under a 12 ms stall at free-running speed. The
  source played throughout, and no block waited.
- **Negative controls**, each the prototype with one rule broken. Each makes the race observable:

  | control | cases with an incomplete adoption |
  |---|---|
  | the completion published before the writes | 689 of 1,245, exactly the head's cases; every one of the 458 output twins differs (threaded: 544 of 2,310 trials) |
  | no sequence: any completion ends the wait | 146: a completion and the next request in one word (42 of 42), supersession (34), repeated switching (70) |
  | the snapshot read before the first acquire | 84: the host reset (42 of 42) and the prime (42 of 42) after a partial read |
  | no re-check after the read | 15 of 19: a read landing inside the next swap's writes |
  | `prepareToPlay`'s trailing `setParameters` reusing the prime's snapshot (precondition 5) | 42 of 42: a re-prepare, then a host reset (the AU order), after a partial read; the prototype 0 of 42, the head 42 of 42 |

- **Allocation.** Engine-only, with the allocation guard armed around every engine call (message-thread
  calls included): 60 swaps, 135 frozen blocks, supersessions, a forget, host resets and primes. Zero
  `operator new`, zero `malloc`.
- **The happens-before edge (ThreadSanitizer witness, not proof).** A plain `int` was written before the
  completion and read after the take. With release / acquire TSan reported nothing (200 completions, 0 stale reads); with both relaxed
  it reported a data race on the witness read (exit 66). This shows the compiled protocol has the edge. It proves nothing about
  snapshot consistency, which is Claims 1–4.

## The adoption paths (audit)

| path | today | under B2 |
|---|---|---|
| A/B switch | requests, then writes; the bottom adopts whichever snapshot it reads | request, writes, completion; the swap starts on a trusted snapshot |
| undo, redo | `requestDuck` then `applyUndoEntry` (`src/PluginProcessor.cpp:2075`, `src/PluginProcessor.cpp:2083`) | the same protocol |
| preset (factory, user file, file chooser) | `onAboutToLoad` → the write loop → `onLoaded` (`src/PresetManager.cpp:845`, `src/PresetManager.cpp:1010`) | request in `onAboutToLoad`; completion after the loop |
| host reset | adopts `pendingP` (`src/dsp/AnamorphEngine.cpp:240`) | unchanged; `pendingP` is always trusted |
| prime / prepare | adopts its snapshot wholesale (`src/dsp/AnamorphEngine.h:123`) | adopts only a trusted snapshot; keeps the source while a swap is written |
| Copy | no write, no request | unaffected (measured: the word is unchanged by a Copy) |
| session restore | no duck: ordinary automation, ADR-0036 §24 | out of scope; its forget bit keeps the sequence and the completion |

## Consequences (on acceptance)

- **Real time without a stall: no change.** The writes end between blocks, so the request and its
  completion are taken in the same block: the same start, the same bottom, the same output. Every existing
  test outside State test 139 is unchanged.
- **Burst processing and a stalled message thread.** The source plays through the writes, and the swap
  lands at its completion.
  - In an offline render the switch is placed later in the rendered timeline, by the application's wall
    time. The swap is never silent longer, and never lands a mixture.
  - Live edits made during the pending interval are adopted with the swap. Today they are already
    deferred to the forced bottom.
  - The pending interval runs from the request to the completion, so for an A/B switch it includes the
    leaving capture (`src/PluginProcessor.cpp:2187`) as well as the writes.
  - Publishing the request after the capture would shorten the interval. The capture writes nothing, so
    the invariant does not depend on that order. The proposal keeps today's order.
- **Level Match.** The destination's A/B record is judged against the complete destination, so a
  measured record stays measured.
- **Cost:** one more acquire `exchange` per block (a locked instruction on x86-64; `SWPAL` or an
  `LDAXR` / `STXR` pair on ARMv8), a few branches, and ~24 bytes of engine state.
- **Not affected:** the reported latency (a swap cannot change the oversampling factor), the DSP order,
  parameter IDs and serialization.

## Risks

- **A missing completion** freezes parameter adoption until the next bulk swap completes. Mitigated by
  the scope guards (point 4), and tested by an exception injected inside an apply.
- **Precondition 2 is invisible to an x86-64 test.** A future relaxed read in `toEngine` would break
  Claim 2 only on weakly ordered hardware. Pinned in `THREADING_POLICY.md` and at `toEngine`, with a
  static check that its loads use the default order.
- **Precondition 5.** Reusing a snapshot across two engine calls re-opens the window. `prepareToPlay`
  does exactly that today, and the negative control that keeps the reuse adopts a partial snapshot
  (worklog §U3, §U4). Pinned at `prepareToPlay`.
- **The 4-bit sequence** needs only two distinct values in flight, because the message thread's swaps
  are sequential. Fifteen gives margin for a host that nests a command through the admission gate's
  queue.
- **Hosts that call `processBlock` on the message thread inside a parameter notification** get frozen
  blocks until the completion, which is the correct behaviour.

## Required changes on acceptance (the implementation plan; the smallest file set)

| file | change |
|---|---|
| `src/dsp/AnamorphEngine.h` | the word's new bits; `requestAbSwitch (from, to, seq = 0)`, `requestDuck (seq = 0)`, `completeBulkApply`, `acquireRequests`; the audio-thread handshake state; the prime's trust gate |
| `src/dsp/AnamorphEngine.cpp` | the writers keep the sequence and completion bits; the release CAS; the two takes and the trust rule; `setParameters` returns while untrusted and starts a completed swap on a trusted snapshot |
| `src/PluginProcessor.h` / `.cpp` | the sequence counter and a scope guard; `acquireRequests` before the snapshot in `processBlock` and `prepareToPlay`; `prepareToPlay` re-reads for its trailing `setParameters`; the guard at the A/B switch, undo and redo; the preset hooks |
| `src/PresetManager.h` / `.cpp` | a guard that fires the completion after the write loop on every exit path |
| `tests/state_tests.cpp` | State test 139 rewritten as the regression, and the brief's legs A–H: 2 / 4 / 8 / 16 fields at every write position; accelerated threaded switching; real time; repeated and superseding switches; undo, redo and preset; a host reset and a re-prepare in the writes and at the completion's block; Level Match (measured kept, S5 evidence intact, never measured for a partial); a completion delayed far past a fade (the source stays, no block waits, the late completion starts the swap) |
| `tests/dsp_tests.cpp` | the engine's word protocol through the engine API, with the allocation guard (Test 38's pattern); sequence 0 unchanged |
| `THREAD_MODEL.md`, `THREADING_POLICY.md` | the request-word row; a fourth ordering-critical pair, and preconditions 1–5 |
| ADR-0007, ADR-0036, ADR-0004 | the handoff and gate-record sentences named in the gate record above; the §24 / §25 residual closed for forced swaps; decision 1's duck starting at the completion |
| `API_REFERENCE.md`, `KNOWN_ISSUES.md`, `CHANGELOG.md`, `TESTING.md`, `DOCUMENTATION_COVERAGE.md` | the engine's request API; KI-032 resolved; a *Fixed* entry in the release that ships it; the tests; the coverage pass |

## Related code

- `src/PluginProcessor.cpp:2177` (`abSwitchToAdopted`), `src/PluginProcessor.cpp:2186` (the request),
  `src/PluginProcessor.cpp:2187` (the leaving capture), `src/PluginProcessor.cpp:2189` (the apply).
- `src/PluginProcessor.cpp:1087` (`beforeSoundReplacementWrites`), `src/PluginProcessor.cpp:1113`
  (`reassertParameters`), `src/PluginProcessor.cpp:2075`, `src/PluginProcessor.cpp:2106` (undo, redo).
- `src/PluginProcessor.cpp:83`, `src/PluginProcessor.cpp:88` (the preset hooks); `src/PresetManager.cpp:845`,
  `src/PresetManager.cpp:871`, `src/PresetManager.cpp:898`, `src/PresetManager.cpp:575` (the load, its
  write loops and completion points).
- `src/PluginProcessor.cpp:231`–`src/PluginProcessor.cpp:234` (`prepareToPlay`: the prime's snapshot, reused),
  `src/PluginProcessor.cpp:447`, `src/PluginProcessor.cpp:450` (`processBlock`: read, then take).
- `src/dsp/AnamorphEngine.cpp:611` (`requestAbSwitch`), `src/dsp/AnamorphEngine.cpp:628`
  (`forgetAbMatchMemory`), `src/dsp/AnamorphEngine.cpp:743` (the exchange),
  `src/dsp/AnamorphEngine.cpp:826` (`pendingP = np` mid-duck), `src/dsp/AnamorphEngine.cpp:1286` and
  `src/dsp/AnamorphEngine.cpp:1317` (the bottom and its adoption), `src/dsp/AnamorphEngine.cpp:1407` (the
  A/B restore), `src/dsp/AnamorphEngine.cpp:240` (a host reset adopting `pendingP`).
- `src/dsp/AnamorphEngine.h:123` (the prime), `src/dsp/AnamorphEngine.h:198` (`requestDuck`),
  `src/dsp/AnamorphEngine.h:377` (the request word); `src/PluginParameters.cpp:326` (`toEngine`).

## Evidence + confidence

- **The defect:** State test 139 (committed), the threaded reproduction of 1,680 trials, the real-time
  stall, and the deterministic enumeration on the head (worklog §U1, §U4). **Verified (measured).**
- **The protocol's correctness:** Claims 1–4, reasoned from the C++ memory model and the ARMv8 ordering
  rules. **Reasoned, not observable on x86-64.**
- **The protocol's logic:** the deterministic enumeration, the threaded runs, the five negative controls
  and the suites, all on the scratch prototype. **Verified (measured).**
- **The happens-before edge:** the TSan witness. **Verified**, as a witness only.
- **Production:** none. The protocol is **not implemented**; KI-032 is **open**.
