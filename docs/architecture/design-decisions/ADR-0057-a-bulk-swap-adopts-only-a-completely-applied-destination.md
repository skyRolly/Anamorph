# ADR-0057 — A forced bulk swap adopts only a completely applied destination

**Status:** **Accepted** (2026-09-27, on the **repository owner's approval** given in the implementing task;
implemented the same day in PR #156; `KNOWN_ISSUES.md` KI-032 fixed in 0.9.9). Proposed 2026-09-27 and revised
the same day by the O8(3) architecture round (worklog §U); implemented and validated per worklog §V.
**Amends:** ADR-0004 decision 1 (the forced duck starts at the swap's completion), ADR-0007's Amendment of
2026-09-25 (the A/B provenance handoff and its gate record), ADR-0036 §24 and §25 item 5 (the residual is closed
for forced swaps).

**The gate record.**
- **What was gated.** `ARCHITECTURE_REVIEW_GATE.md`, *Thread Model change — new thread, new cross-thread
  path, new atomic ordering*. The protocol below adds:
  - a new atomic ordering to the existing message → audio request word: the completion is published
    with `release` and taken with `acquire`, where every access was relaxed before;
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
- **The approval.** `ARCHITECTURE_REVIEW_GATE.md` §Procedure, steps 2–3 require a human decision recorded as
  this **Status**. It was given by the **repository owner** on 2026-09-27, in the task that implemented this
  ADR: B2 as specified below (not B2-H), with the instruction not to stop for a further architecture
  approval. This record names no other reviewer. The implementing change is not to be auto-merged, and the
  pull request that carries it (PR #156) is merged by the owner, not by this change. Step 4 (the release
  compatibility checklist) was not triggered: no parameter, serialization or reported-latency change.
- **What did not approve it**, and would not have. The earlier round's delegation (*"NOT authorized to
  silently override an existing threading-model hard stop"*), a green build, or the prototype evidence
  (`AI_AGENT_POLICY.md`: *"A passing build/test/pluginval does not clear a Hard Stop — only human review
  does."*). Until the owner's approval the ADR stood **Proposed** and KI-032 open.
- **The approval marker in code:** `src/PluginProcessor.h`, the *THE BULK-SWAP HANDSHAKE* block
  ("ARCHITECTURE REVIEW GATE: APPROVED by the repository owner, 2026-09-27").

## Context

*This section and the next describe the code as it was measured at `e49a90e` (whose `src/` is `9e38310`'s), before
this decision was implemented. Their anchors are pinned to that revision.*

A **forced bulk swap** replaces the whole sound behind a masking duck:
- an A/B switch (`AnamorphAudioProcessor::abSwitchToAdopted`, `e49a90e:src/PluginProcessor.cpp:2177`);
- undo and redo (`e49a90e:src/PluginProcessor.cpp:2075`, `e49a90e:src/PluginProcessor.cpp:2106`);
- a user or factory preset load (`presets.onAboutToLoad`, `e49a90e:src/PluginProcessor.cpp:83`, then the write loop,
  `e49a90e:src/PresetManager.cpp:845` to `e49a90e:src/PresetManager.cpp:898`).

Each raises the duck first and then writes the sound one parameter at a time. The A/B switch requests
the switch (`e49a90e:src/PluginProcessor.cpp:2186`), captures the slot it leaves (`e49a90e:src/PluginProcessor.cpp:2187`)
and applies the destination (`e49a90e:src/PluginProcessor.cpp:2189`). The apply is JUCE's `replaceState`: one
store per parameter whose value changes, then `reassertParameters` (`e49a90e:src/PluginProcessor.cpp:1113`).

The contract the duck exists for (feedback #1, ADR-0004) is that a bulk swap is applied *entirely* at
the silent bottom, continuous controls included, with the smoothers snapped. The engine keeps the source
live through the ~6 ms fade-out. Every block, `processBlock` reads the parameters into a snapshot
(`e49a90e:src/PluginProcessor.cpp:447`), then `setParameters` takes the request word
(`e49a90e:src/dsp/AnamorphEngine.cpp:743`). On every block of the fade-out the engine records that snapshot
(`pendingP = np`, `e49a90e:src/dsp/AnamorphEngine.cpp:826`). At the bottom it adopts the last one
(`p = pendingP`, `e49a90e:src/dsp/AnamorphEngine.cpp:1317`) and judges the destination's A/B Level-Match record
against it (`e49a90e:src/dsp/AnamorphEngine.cpp:1407`). A host reset adopts it too
(`e49a90e:src/dsp/AnamorphEngine.cpp:240`), and the prime of `prepareToPlay` adopts its own snapshot wholesale
(`e49a90e:src/dsp/AnamorphEngine.h:123`).

The request word is **relaxed** and carries no completion (`THREAD_MODEL.md`, the request-word row). So
the adoption point is fixed in *audio* time, two blocks after the request is taken at 256 samples. The
application's end is fixed in *wall* time: ~0.1–0.3 ms here, and unbounded behind host notifications,
contention or preemption. Nothing relates the two.

ADR-0036 §24 accepted that *"a reader that samples parameters while a replacement is running still sees
part of the outgoing sound and part of the incoming one — JUCE offers no block-atomic parameter
snapshot and never has … What changes is that the mixture cannot SETTLE."* The Devin review reports the
forced-swap form as a defect: *"A/B switches adopt incomplete slot states"* (`e49a90e:src/PluginProcessor.cpp:2186`;
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
ended. No message → audio path before this decision gave it one:
- the three ordering-critical pairs of `THREADING_POLICY.md` run audio → GUI (the scope ring), audio /
  host → message (D-1) and host ↔ message (D-2);
- the engine-config word is relaxed on the audio side;
- the APVTS values are sequentially consistent, but each orders only itself, and no parameter marks the
  end of an application.

- **B2 (selected): the two-phase request word; the swap starts at its completion.** The existing
  word carries a 4-bit sequence and a completion stored with `release`. The audio thread takes the word
  with `acquire` twice per block, before and after reading the snapshot. While a swap's completion is
  absent the engine adopts nothing. The forced duck starts only on a snapshot read between two acquires
  after that completion, so its bottom never waits. The protocol as implemented is under *Decision*.
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

| criterion | B2 (selected) | B2-H | A | C |
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

## Decision

**Selected: B2** (the owner's approval, 2026-09-27), as specified here and implemented in PR #156. **Not B2-H.**

### The word

`duckRequest` stays one `std::atomic<int>`, and every modification stays a read-modify-write:
- bits 0–11 as before: duck, switch, forget, and the two slot indices;
- **bit 12:** the completion flag;
- **bits 13–16:** the request's sequence *s* (1–15);
- **bits 17–20:** the completion's sequence.

Sequence 0 is the engine API: complete by construction, because the caller hands the whole state to
`setParameters`, and it behaves exactly as before.

### The protocol, as implemented

Message thread, for every bulk apply (an A/B switch, undo, redo, a preset load):
1. **The sequence.** A scoped `AnamorphAudioProcessor::BulkApply` takes the next sequence *s* from
   `beginBulkApply()` (1–15, wrapping; `AnamorphEngine::kBulkSeqCount`). A swap begun while another is open
   **joins** it: the same *s*, and only the outermost end completes, so a completion always follows every write
   of every swap in flight. (The admission gate runs state commands one at a time, so production never nests;
   joining keeps the rule true without depending on that.) A preset load takes its sequence in the processor's
   `onAboutToLoad` hook.
2. **Request publication (R).** `engine.requestAbSwitch (from, to, s)` (A/B) or `engine.requestDuck (s)` (undo,
   redo, a preset load): a relaxed CAS that stores *s* in bits 13–16. It is published where it was before —
   before the leaving capture and the first write. Sequence 0 (the engine API: `requestDuck()` is still a
   `fetch_or`) is complete by construction and behaves exactly as before.
3. **The application** writes the destination one parameter at a time; its **last store** is:
   - A/B, undo, redo: the end of `applyStatePreservingView` — the Bypass / view-parameter write-back that
     follows `reassertParameters` (the design text named `reassertParameters` itself; the write-back after it
     is later, and `BulkApply::complete()` follows `abApplySlot` / `applyUndoEntry`, which covers both);
   - a factory preset: the override loop inside the §24 scope;
   - a user preset (`load` of a file row, or `loadFile`): the end of `applySoundTree`.
4. **Completion publication (C).** `endBulkApply()` → `engine.completeBulkApply (s)`: a CAS that stores the flag
   (bit 12) and *s* (bits 17–20) with **`memory_order_release`**. The processor calls `BulkApply::complete()`
   right after the last store; its destructor publishes it on every other exit path, an exception included. A
   preset load publishes it from PresetManager's new `onSoundApplied` hook, fired exactly once for every
   `onAboutToLoad` — at `SoundAppliedGuard::fire()` after the last store, or by the guard's destructor on any
   other path.

Audio thread (every `processBlock`), and the prepare path the same:

5. **Acquisition.** The processor no longer builds the snapshot itself. It hands the engine a reader:
   `engine.setParametersFrom (read)` takes the word with `exchange (0, memory_order_acquire)` (E<sub>a</sub>,
   `acquireBeforeSnapshot`), calls `read()` (the processor's `readEngineSnapshot()`, `toEngine`'s `seq_cst`
   loads, plus the momentary solo audition), then takes the word again the same way (E<sub>b</sub>,
   `acquireAfterSnapshot`). `prepareToPlay` calls `engine.prepareFrom (sr, bs, read)`: `primeParametersFrom
   (read)` (the same two takes around its own read), `prepare`, then `setParametersFrom (read)` — a SECOND fresh
   read. The design's separate `acquireRequests()` call was replaced by this callable-reader form, so the order
   "take, read, take" and "never reuse a snapshot" (precondition 5) are structural rather than conventions each
   caller must keep. Each take (`takeRequestWord`) processes the word:
   - a forget clears every A/B record and any switch not yet started, and drops the stash;
   - a request with *s* ≠ 0 sets `awaitSeq = s` and reports a new request; its A/B switch is stashed, keeping the
     first source slot and taking the newest destination (and a forget in the same word records no source);
   - a request with *s* = 0 (the engine API) is kept in `legacyReq`, applied with the next trusted snapshot;
   - a completion flag whose sequence equals `awaitSeq` clears it and sets `startPending`; any other is ignored.

   **The snapshot is trusted** only if no swap was awaited after E<sub>a</sub> and E<sub>b</sub> took no new
   request.
   **The engine API's one-take path.** `setParameters (np)` / `primeParameters (np)` receive a snapshot read
   before the call, so they make ONE take (`acquireForEarlierSnapshot`) and trust `np` only if no swap was
   awaited before the take and the take found no new request. Their contract, stated in `AnamorphEngine.h`: the
   snapshot was read after the engine's previous call returned (precondition 5). The processor never uses them.
6. **When the forced bottom may adopt: always.** A forced duck is only ever started on a trusted snapshot. At the
   first trusted snapshot while `startPending` (`startTakenRequests`), the engine applies the engine-API requests
   first, then the stash — the A/B bookkeeping (`switchAbSlots`), recording the leaving slot against the state being played: the source, or the trusted target of a duck
   already in flight at the request, which may land during the wait (point 7) — and begins the forced duck with that snapshot as its target. A mid-duck retarget
   is also a trusted snapshot. So the bottom adopts a complete state, and so does a host reset, which adopts
   `pendingP`. A prime that takes a completion with a snapshot it cannot trust leaves the swap pending, and the
   trailing `setParametersFrom` of `prepareFrom`, or the first block, starts it.
7. **The source is retained while the completion is absent.** While a swap is awaited, or the snapshot is
   untrusted, `adoptSnapshot` returns right after E<sub>b</sub>: no live edit, no duck entry, no `pendingP`
   write. The engine renders the adopted source, and any duck already in flight lands on its trusted target. The
   prime keeps the source the same way (`primeSnapshot`).
8. **The hold limit is zero at the forced bottom.** The wait happens before the fade-out, with the source at
   full level. The pending interval is the application's wall time converted to blocks, plus at most one block.
9. **When a bound is reached: there is none, by design.** An audio-time cap can only do one of two things when it
   fires: adopt the snapshot, which may be incomplete, which the invariant forbids; or keep the source, which the
   pending state already does. Liveness is guaranteed on the message thread instead: every request is paired
   with exactly one completion (point 4).
10. **A completion that arrives late** (after any length of pending interval) starts a fresh forced swap from the
    source to the complete destination. The A/B bookkeeping runs at that start.
11. **A newer request supersedes the old one.**
    - The engine awaits the newest *s*; an older completion is ignored. The word keeps one completion slot, the
      latest, and the message thread's swaps are sequential, so a stale completion never carries the awaited *s*.
    - The stash keeps the first source slot and takes the newest destination; there is one start, from the
      source to the newest complete state.
    - A forced duck that an earlier completed swap already started continues to its trusted target and lands.
      The new start re-ducks from the fade-in, or retargets during the fade-out, as before.

**The real-time bound, exactly.** Per block the audio thread performs two `exchange`s and a constant number of
branches. It never waits, spins, locks or allocates (Test 75 counts zero allocations with the guard armed around
every engine call). The output during a pending interval is the source, or the duck already in flight. It is
bit-identical to a twin that received the swap at its completion (State test 139; State test 140 S1t).

**Liveness.** A request whose completion never came would freeze parameter adoption (the source keeps playing)
until the next bulk swap's completion, which supersedes it. The scope guards make that unreachable on every path
that returns or throws: `BulkApply` for the A/B switch, undo and redo; `SoundAppliedGuard` for a preset load,
extending PresetManager's existing contract *"never an onAboutToLoad() with no matching onLoaded()"*.

### Why B2-H was rejected

B2-H — the draft of this ADR at `9eea7c4` — started the duck at the request, as before, and held the silent (or
dry-filled) bottom until a snapshot read after the completion, with an audio-time cap that aborted to the source.
- **It masks the source while the destination is written.** The duck is already down for the whole pending
  interval: under a burst or a stall the bottom's silence or dry fill lengthens (median 1–7 blocks at ≥ 64×,
  66 at 256×, worklog §U3). B2 keeps the source at full level for that interval and ducks only once, around a
  complete destination.
- **Its cap has no correct action.** When it fires it can adopt the snapshot it has — which may be incomplete,
  the very thing the invariant forbids — or keep the source, which is B2's pending state reached by a longer
  route, now with a fade-back and a re-arm to get there (~150 engine lines against B2's ~130).
- **The bottom would wait.** Every existing forced-duck path (the tighten branch, the re-duck from the fade-in,
  a host reset landing the duck) would have to learn a new "bottom held" state; B2 changes none of them, because
  a forced duck is only ever started on a trusted snapshot.
B2 moves the wait from the bottom to before the fade-out, which removes the hold, the cap and the new state
together.

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
- Only A's own takes clear a request. Every writer keeps a pending request's duck bit and a non-zero
  sequence (a newer bulk request replaces the sequence with its own, which E<sub>b</sub> reports as new all the
  same); the forget CAS drops only a switch's flag and slot indices, by design.
- So either E<sub>a</sub> already read R′, and the swap was awaited after E<sub>a</sub>, or
  E<sub>b</sub> sees R′'s bits. Either way the snapshot is untrusted.

**Claim 3 (trusted means complete).** Take a trusted snapshot. No swap is awaited after E<sub>a</sub>,
so every swap whose request was taken has had its completion taken, at or before E<sub>a</sub>. By
Claim 1 all their writes are in L, and by Claim 2 no newer swap's write is.

A superseded swap's writes are in L too. Its completion precedes the next request on M, and every later
completion is a read-modify-write in its release sequence. What remains are live edits, which is the pre-existing
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
  - With C as `STLXR` / `CASL` and E<sub>a</sub> as `LDAXR` / `SWPA`, both orders hold.
  - Claim 2 holds on ARMv8 because `STLR` orders R′ before W′; the architecture is other-multi-copy
    atomic; and L's `LDAR` orders E<sub>b</sub> after it.
- **x86-64 hides all of this.** It is TSO, and every RMW is a locked instruction, so no x86-64 run can
  confirm or refute the argument. The evidence below does not rely on one: the orders are argued here,
  and the logic is tested with the orders removed.

**The preconditions**, pinned in `THREADING_POLICY.md` (*Atomic usage rules*, the request word) and at the code:
1. Every store of a bulk apply is sequenced before its completion, on the thread that publishes it. The
   applications are synchronous on the message thread (`BulkApply::complete()` after `abApplySlot` /
   `applyUndoEntry`; `SoundAppliedGuard::fire()` after a preset's last write).
2. Every audio-thread read of a swap-carried value is an acquire or stronger: `toEngine`'s default `seq_cst`
   `->load()`. A relaxed read there would break Claim 2. Pinned by a comment at `ParamPointers::toEngine` and by
   `check-realtime.py`, which rejects `memory_order_relaxed` / `consume` in `toEngine`'s body (with self-test
   cases in both directions); the AArch64 disassembly of `toEngine` shows 36 `LDAR` for its 36 loads.
   **The other half of the pair is binding too** (made explicit by the pre-merge audit, 2026-09-27; the proof's
   notation always assumed it): every message-thread store of a swap-carried value is a release or stronger.
   The stores are JUCE's — `ParameterAdapter::parameterValueChanged` assigns the `std::atomic<float>`
   `unnormalisedValue` with the default `seq_cst` (`juce_AudioProcessorValueTreeState.cpp:155` at the pinned
   JUCE 9.0.2, `72782788`) — plus the processor's silent re-assert, whose `atom->store` is `seq_cst` too. A
   relaxed store there breaks Claim 2 exactly as a relaxed read does. No lint can see it (it is JUCE's source), so
   a JUCE bump re-verifies it (`DEPENDENCY_POLICY.md`, upgrade rule 2).
3. Every modification of the word is a read-modify-write (the request-word comment in `AnamorphEngine.h`).
4. Exactly one completion follows each request, after its last store, on every exit path (`BulkApply`,
   `SoundAppliedGuard`).
5. The snapshot is read between the two takes and never reused across two engine calls: structural in
   `setParametersFrom` / `prepareFrom`; for the engine API's `setParameters (np)` / `primeParameters (np)`, a
   stated contract (read after the previous call returned) backed by their one-take trust rule. Before this
   change `prepareToPlay` handed the prime's snapshot to its trailing `setParameters`; it now re-reads.

The oversampling factor is read relaxed (`e49a90e:src/InternalState.h:175`), but it is a Setting that never
swaps with a slot (#13/#15), so no swap-carried value depends on it.

## Evidence

### Before implementation (scratch prototype, not in the tree; worklog §U2–§U6)


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

### The implementation, on the final head (worklog §V)

Every figure below is from the production code of this change — no prototype, no scratch copy of `src/` — built
Release, x86-64 Linux, GCC 13, 48 kHz / 256, under a 1 MB stack.
- **The suites.** DSP 944 checks, 0 failures; State 5,573 checks, 0 failures (5,593 with State test 141, which the
  pre-merge audit added to pin precondition 4's refusal and exception paths; worklog §W). New: DSP Test 75
  (7,791 engine-level interleavings; 0 incomplete, 0 allocations), State test 139 rewritten as the regression,
  State test 140 (1,358 interleavings through the processor on x86-64 Linux — 1,352 on arm64 macOS, whose preset loads change fewer parameters — every path; 0 failures). F13, O8(1) and O8(2) — State
  tests 130–138 and Tests 66–74 — pass unchanged; State test 127 (*"a host reset inside an A/B, preset, undo or
  redo swap lands settled"*) and State test 130 (7) pass unchanged.
- **The deterministic enumeration of worklog §U4**, ported to the final API (scratch harness, the same 1,245 cases):

  | build | cases | with an incomplete adoption | twin comparisons bit-identical |
  |---|---|---|---|
  | pre-fix head (`f12cc80`, `src/` = `e49a90e`'s) | 1,245 | **689** | — |
  | final code | 1,245 | **0** | 458 of 458 |

  Every final case ends on the right state with nothing pending, and every A/B landing restores the destination's
  measured record as measured. The harness's later sub-scenario S9d (a partial snapshot primed, activated, then a
  host reset) adds 42 cases: pre-fix 42, final 0. Allocation, engine only, guard armed around every call (60
  swaps, supersessions, a forget, host resets, primes while awaiting and after a completion): 0 `operator new`,
  0 `malloc`. Through the processor, with the guard armed around every `processBlock` of the enumeration, every
  allocation it counted was attributed by stack: on JUCE's timer thread (another thread; the guard is
  process-wide) or in the guard's own start-up self-check — none on the thread calling `processBlock`.
- **The Phase-1 probe, permanent** (`AnamorphStateTests --bulk-swap-probe 40`; six combinations × seven speeds × 40
  trials): **0 of 1,680** trials failed — a block holding neither slot 0, a landing not
  measured 0, a wrong final state 0, a same-rate `prepareToPlay` flushing B's level
  0. The pre-fix head's run of the same measurement (worklog §U1): 573 incomplete adoptions, 368 flushes.
- **At real time behind one 12 ms stall** (`--bulk-swap-probe 20 12000`): **0 of 120** failed; the
  pre-fix head had 40 hybrids.
- **Threaded stress** (`--bulk-swap-stress 20` and `--bulk-swap-stress 20 12000`; A/B, repeated, superseding,
  undo, redo, preset, Copy, and host resets and re-prepares on the audio thread, 1×–256× and free-running):
  9,240 trials, **0 with a state no swap completed, 0 wrong final states,
  0 stuck**; the pre-fix head had 567 of 2,310 trials with an incomplete adoption.
- **The negative controls, each the final code with one rule broken** (worklog §V; scratch mutants, both suites
  built and run for each). Every one fails the committed suites:

  | control | fails |
  |---|---|
  | N1 the completion published before the writes | State test 139 (13 checks: every (A) position, (B)'s landings, (C), (D), (E), (F1)); State test 140 (1,066 cases) |
  | N2 no sequence: any completion ends the wait | Test 75 (4); State test 140 (S3 70, S10 42, S11 34 cases) |
  | N3 the snapshot read before the first take | Test 75 ((1) 141, (3) 12 cases); State test 140 (S9e 12) |
  | N4 no second take | Test 75 ((1) 564, (2) 490, (3) 24); State test 140 (S9e 28, S13b 15) |
  | N5 `prepareFrom` re-using the prime's snapshot | Test 75 ((3) 15); State test 140 (S9e 14) |
  | N6 trust after a first take that left a swap awaited | Test 75 (3,704 cases, two (4) checks); State test 139 (13 checks); State test 140 (1,075) |
  | N7 the one-take path ignoring an awaited swap | Test 75 ((1) 391); State test 140 (S9b–d 84) |
  | N8 the release and the acquire both relaxed | State test 139 (F2) under ThreadSanitizer: a ThreadSanitizer data race reported on the witness (the final code: none) |
  | N9 the pre-fix behaviour (every request sequence 0) | State test 139 (12 checks); State test 140 (1,120 of 1,358 cases); the DSP suite is unaffected (the change is the processor's) |
  | N10 no completion published | 257 State checks: all 1,358 of State test 140's cases (each command left pending), State test 139 ×8, and the Level-Match, undo and preset tests that follow a swap (State tests 31, 35, 127, 130–138) |
  | N11 one take per block (`setParameters` of a snapshot read before the call, in `processBlock`) | no incomplete adoption — one take is conservative — but every swap starts a block late: 96 checks of the timing-exact tests (State tests 35, 127, 130–138); State tests 139 and 140 pass. The two takes are what keep real-time behaviour unchanged |

- **The happens-before edge (ThreadSanitizer, clang 18).** The final State suite under TSan: 5,573 / 0 and no data race; its four reports are the pre-existing lock-order inversions of State tests 75, 100, 103 and 113, each matched by `tests/tsan-suppressions.txt`, which the CI lane applies. N8 above
  is the control. A witness, not a proof: it shows the compiled protocol has the edge, and nothing about snapshot
  consistency, which is Claims 1–4.
- **AArch64.** Cross-built with `aarch64-linux-gnu-g++` 13.3 (Release, the shipped flags minus the x86 ISA
  baseline) and disassembled: `completeBulkApply` calls the release CAS helper, `CASL` with LSE or
  `LDXR`/`STLXR`; `takeRequestWord` the acquire swap helper, `SWPA` or `LDAXR`/`STXR`; `toEngine` 36 `LDAR` for
  its 36 loads. The DSP suite (Test 75 included) ran under `qemu-aarch64` user-mode emulation: 944 / 0 (Test 75: 7,791 cases, 0 incomplete, 0 allocations).
  qemu on an x86-64 host executes the guest's memory accesses with the host's TSO ordering, so this shows the
  AArch64 code's logic, not weak-ordering behaviour. The only AArch64 hardware run is CI's `macos` job
  (`macos-latest`, Apple Silicon), which runs both suites natively on the arm64 slice: its result on the pushed head is recorded in the PR #156 description.

## The adoption paths (audit)

| path | before (`e49a90e`) | implemented |
|---|---|---|
| A/B switch | requests, then writes; the bottom adopts whichever snapshot it reads | `BulkApply` + `requestAbSwitch (…, s)`, the writes, `complete()`; the swap starts on a trusted snapshot (State test 139; State test 140 S1–S3) |
| undo, redo | `requestDuck` then `applyUndoEntry` | the same protocol (`BulkApply` + `requestDuck (s)`; State test 140 S4, S5) |
| preset (factory, user file, file chooser) | `onAboutToLoad` → the write loop → `onLoaded` | request in `onAboutToLoad`; completion from `onSoundApplied` after the last store (State test 140 S6, S6f) |
| host reset | adopts `pendingP` | unchanged: `pendingP` is always trusted; a swap still awaited is not landed (State test 139 (D); State test 140 S7) |
| prime / prepare | adopts its snapshot wholesale; the trailing `setParameters` re-uses it | `prepareFrom`: adopts only a trusted snapshot, then re-reads (State test 139 (D); State test 140 S8, S9c–e; Test 75 (3)) |
| Copy | no write, no request | unaffected: the word is unchanged by a Copy (State test 140 S12) |
| session restore | no duck: ordinary automation, ADR-0036 §24 | out of scope; its forget keeps the duck, the sequence and the completion |
| engine API (sequence 0) | `requestDuck()` / `requestAbSwitch (from, to)` + `setParameters (np)` | unchanged behaviour while no bulk sequence is pending (a sequence-0 request raised while one is folds into that swap, `AnamorphEngine.cpp:616-618`); one take with its own trust rule (Test 75 (1) and (4); Test 70) |
| other multi-store actions (Apply's two stores, the imager's band transactions) | no request | not bulk swaps: they raise no forced duck, so the handshake does not apply; a block between their stores adopts the intermediate state as a live edit, as before (pre-merge audit, 2026-09-27) |

## Consequences

- **Real time without a stall: no change.** The writes end between blocks, so the request and its completion are
  taken in the same block: the same start, the same bottom, the same output. Every existing test outside State
  test 139 passes unchanged on the final code (DSP and State suites, above).
- **Burst processing and a stalled message thread.** The source plays through the writes, and the swap lands at
  its completion.
  - In an offline render the switch is placed later in the rendered timeline, by the application's wall time.
    The swap is never silent longer, and never lands a mixture.
  - Live edits made during the pending interval are adopted with the swap. Before, they were already deferred to
    the forced bottom.
  - The pending interval runs from the request to the completion, so for an A/B switch it includes the leaving
    capture as well as the writes. The capture writes nothing, so the invariant does not depend on that order;
    the implementation keeps it.
- **Level Match.** The destination's A/B record is judged against the complete destination, so a measured record
  stays measured (State test 139 (B); State test 140 S1, S11).
- **Cost:** one more acquire `exchange` per block (a locked instruction on x86-64; `SWPA` or an `LDAXR` / `STXR`
  pair on ARMv8), a few branches, and ~24 bytes of engine state.
- **Not affected:** the reported latency (a swap cannot change the oversampling factor), the DSP order, parameter
  IDs and serialization.

## Risks

- **A missing completion** freezes parameter adoption until the next bulk swap completes. Mitigated by the scope
  guards (point 4); a completion is published on every path that returns or throws.
- **Precondition 2 is invisible to an x86-64 test.** A future relaxed read in `toEngine` would break Claim 2 only on
  weakly ordered hardware. Pinned in `THREADING_POLICY.md`, by a comment at `toEngine`, and by `check-realtime.py`
  (a weak order in `toEngine` is a violation). A load moved OUT of `toEngine` into a helper in another translation
  unit would escape the lint; the comment names the rule for that case.
- **Precondition 5.** Reusing a snapshot across two engine calls re-opens the window. The processor's entries take a
  reader and cannot; the engine API's one-take path states the contract and trusts a snapshot only when no swap was
  awaited before its take.
- **The 4-bit sequence** needs only two distinct values in flight, because the message thread's swaps are
  sequential. Fifteen gives margin; a swap begun inside another joins it and shares its sequence.
- **Hosts that call `processBlock` on the message thread inside a parameter notification** get frozen blocks until
  the completion, which is the correct behaviour.
- **Pre-existing, not changed here.** A host-pumped restore or GUI transaction nested inside a swap's window is
  ADR-0036 §24's examined residual; it raises no duck and is not a bulk swap.

## Implementation record (PR #156)

| file | change |
|---|---|
| `src/dsp/AnamorphEngine.h` | the word's new bits and the RMW-only rule; `requestAbSwitch (from, to, bulkSeq = 0)`, `requestDuck (bulkSeq = 0)`, `completeBulkApply`, `kBulkSeqCount`; `setParametersFrom`, `primeParametersFrom`, `prepareFrom`; the handshake state and its takes (`acquireBeforeSnapshot`, `acquireAfterSnapshot`, `acquireForEarlierSnapshot`); read-only observers for tests |
| `src/dsp/AnamorphEngine.cpp` | the writers keep the sequence and completion bits; the release CAS; `takeRequestWord` (the acquire take); `startTakenRequests`; `switchAbSlots` (the A/B record, split out of `takeRequests`); `setParameters` → `adoptSnapshot`, the prime → `primeSnapshot`, each returning while untrusted |
| `src/PluginProcessor.h` / `.cpp` | `readEngineSnapshot`; `beginBulkApply` / `endBulkApply` and the scoped `BulkApply` (join semantics); the guard at the A/B switch, undo and redo; the preset hooks; `processBlock` → `setParametersFrom`; `prepareToPlay` → `prepareFrom` |
| `src/PresetManager.h` / `.cpp` | `onSoundApplied` and `SoundAppliedGuard` in `loadAdopted` and `applyParsedFile` |
| `src/PluginParameters.cpp` | the precondition-2 comment at `toEngine` |
| `scripts/check-realtime.py` | `setParametersFrom` and `takeRequestWord` seeded; a weak load order in `toEngine` is a violation; self-test cases |
| `tests/dsp_tests.cpp` | Test 75 |
| `tests/state_tests.cpp` | State test 139 rewritten as the regression; State test 140; `--bulk-swap-probe`, `--bulk-swap-stress` |
| docs | `THREAD_MODEL.md`, `THREADING_POLICY.md`, `REALTIME_AUDIO_POLICY.md`, ADR-0004, ADR-0007, ADR-0036, `ADR_INDEX.md`, `API_REFERENCE.md`, `ARCHITECTURE.md`, `SIGNAL_FLOW.md`, `REALTIME_SAFETY_AUDIT.md`, `FUTURE_RISKS.md` (RISK-010), `KNOWN_ISSUES.md` (KI-032), `CHANGELOG.md`, `TESTING.md`, `DOCUMENTATION_COVERAGE.md`, the worklog §V |

Deviations from the design text, each recorded above: the callable-reader API instead of `acquireRequests()`; the
engine API's one-take trust rule; join semantics for a nested swap; `onSoundApplied` / `SoundAppliedGuard` as the
preset completion point; the A/B completion point after the Bypass / view write-back rather than at
`reassertParameters`.

## Related code

- `src/PluginProcessor.cpp:2210-2229` (`abSwitchToAdopted`: the guard, the request, the capture, the apply, the
  completion); `src/PluginProcessor.cpp:2085-2126`, `src/PluginProcessor.cpp:2128-2153` (undo, redo);
  `src/PluginProcessor.cpp:2071-2083` (`beginBulkApply`, `endBulkApply`); `src/PluginProcessor.cpp:86-87` (the
  preset hooks); `src/PluginProcessor.cpp:240` (`prepareFrom`); `src/PluginProcessor.cpp:455-461` (`setParametersFrom`).
- `src/PluginProcessor.h:185` (`readEngineSnapshot`); `src/PluginProcessor.h:970-1001` (the handshake block and
  `BulkApply`).
- `src/PresetManager.cpp:64-84` (`SoundAppliedGuard`); `src/PresetManager.cpp:867`, `src/PresetManager.cpp:895`,
  `src/PresetManager.cpp:867`, `src/PresetManager.cpp:895`, `src/PresetManager.cpp:912`, `src/PresetManager.cpp:1035-1037` (each loader's guard and its explicit completion points: `loadAdopted`'s factory and user-file branches, `applyParsedFile`'s one);
  `src/PresetManager.h:326-331` (`onSoundApplied`).
- `src/dsp/AnamorphEngine.h:130-172` (the prime, `setParameters`, `setParametersFrom`, `prepareFrom`),
  `src/dsp/AnamorphEngine.h:228-246` (`completeBulkApply`, the observers), `src/dsp/AnamorphEngine.h:439-486` (the word
  and the handshake state).
- `src/dsp/AnamorphEngine.cpp:611-631` (`requestAbSwitch`), `src/dsp/AnamorphEngine.cpp:633-658` (`requestDuck`,
  `completeBulkApply`), `src/dsp/AnamorphEngine.cpp:672-727` (`takeRequestWord`), `src/dsp/AnamorphEngine.cpp:729-790`
  (`startTakenRequests`, `takeRequests`, `switchAbSlots`), `src/dsp/AnamorphEngine.cpp:854-878` (`setParameters`,
  `primeSnapshot`, `adoptSnapshot`).
- `src/PluginParameters.cpp:330` (`toEngine`, precondition 2); `scripts/check-realtime.py` (the seeds, `WEAK_ORDER`).

## Evidence + confidence

- **The defect:** the pre-fix measurements (worklog §T, §U1, §U4) and, on the final code, the negative control N9 and
  the ported enumeration's pre-fix run (689 of 1,245). **Verified (measured).**
- **The fix's logic:** Test 75, State tests 139 and 140, the ported enumeration (0 of 1,245), the probes and the
  threaded stress, all on the final code, with each rule's negative control failing the committed suites.
  **Verified (measured).**
- **The protocol's ordering:** Claims 1–4, reasoned from the C++ memory model and the ARMv8 ordering rules; the TSan
  witness and its relaxed control; the AArch64 disassembly. **Reasoned; not observable on x86-64 or under qemu.**
- **Production:** implemented in PR #156; KI-032 **fixed** in 0.9.9.
