# ADR-0036 — Program state is message-thread-owned; host threads exchange immutable snapshots

**Status:** Accepted (Thread Model change — maintainer instruction 2026-09-03: resolve D-2 / RISK-007).
**Amended 2026-09-03, round 2** (the PR review's two findings): decisions 5 and 8 — the generation
travels *inside* the snapshot, and the engine-facing oversampling is a generation-tagged word where
the latest restore wins. **Round 3** (one further finding): decision 9 — a Settings edit made after
a restore *arrived* is the newer arrival and survives that restore's adoption.

**Resolves decision D-2** (`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md`, deferred in
round 4) and **closes RISK-007** (`docs/FUTURE_RISKS.md`). **Amends `THREADING_POLICY.md`** §Host state
calls, which recorded off-message-thread state calls as an unguarded assumption: they are now a covered
path. **Extends D-1** (KI-027, ADR-less — approved 2026-09-01): the same request/consume shape, with a
payload. Nothing in ADR-0008 (undo), ADR-0010 (InternalState), ADR-0013 (`raw`) or ADR-0024 (identity)
changes; each of those decides *what* the state is, this one decides *who owns it*.

## Context

`getStateInformation` / `setStateInformation` arrive on whatever thread the host uses. On VST3 the
pinned SDK annotates both `[UI-thread]` and JUCE debug-asserts it for `setState`, so there they are
the message thread. On the **macOS AU** nothing pins them (host autosave is the real-world case) and
the JUCE wrapper passes both straight through on the caller's thread; pluginval's AU
`BackgroundThreadStateTest` exercises exactly that window on every green macOS run; and JUCE's Linux
VST3 wrapper services the plug-in's messages from its own thread until the host registers an
`IRunLoop`, so even an in-spec host's UI thread is not JUCE's message thread for a while.

Every piece of *program metadata* this plug-in owns — the preset name, identity and dirty baseline
(`PresetManager`), the A/B slot set (`abSlot[]`, `abActive`, `abMatchGain[]`), the undo history
(`abUndo[]`), the committed baseline and gesture bookkeeping, and the host-hidden Settings tree
(`InternalState`) — was written by the restore on the caller's thread and read by the editor's
24 Hz tick on the message thread, with no lock and no marshalling. `AnamorphStateTests
--state-thread-probe` under ThreadSanitizer measured it in round 2: four data races, on `abActive`,
the `abUndo` vector's internals (twice), and a `juce::String` reference-count exchange. Rounds 15,
20 and 21 re-measured and classified everything else raised against it as the same class.

## Problem

Three properties have to hold at once, and the two candidate fixes recorded for D-2 each broke one:

1. **The audio thread stays lock-free and untouched.** Its inputs are the APVTS parameter atomics,
   `InternalState::osAtomic` and `soloPreviewMask`; a restore reaches it as a burst of per-parameter
   atomics masked by ADR-0004's duck. That contract is JUCE's and must not change.
2. **A restore's sound half must be synchronous on the caller's thread.** The ordinary host order is
   `setState`, then `setActive`/`prepareToPlay`, and `prepareToPlay` primes the engine from the
   parameters and the oversampling atomic (round 20, ER-DSP-09). Deferring the sound would reopen
   the restored-session glide.
3. **The metadata must never be read on one thread while written on another.** `juce::String`,
   `std::vector` and `ValueTree` are not synchronised types; a torn read is undefined behaviour.

A **mutex over the state-set members** (candidate 1) satisfies 1 and 2 and closes 3 only if it is
held across every editor read and every restore write — including `setValueNotifyingHost` calls that
re-enter the host, which is a lock-order hazard against a host that holds its own lock while calling
`getState` from another thread. A **`callAsync` of the whole tail** (candidate 2) satisfies 1 and 3
and breaks 2 for the Settings half (the oversampling atomic would follow the tree, one message-loop
turn late) and leaves a save issued on the host thread right after its restore describing the
*previous* program.

## Options

- **A. Narrow mutex over the metadata, never held across a host callback.** Rejected: the lock
  would have to be released and re-taken around every `setValueNotifyingHost` inside an A/B apply,
  which is exactly the interleaving where a torn view is possible, and it adds a lock to the very
  paths the editor tick runs 24 times a second.
- **B. Make every raced member individually atomic.** Rejected on the task's own terms: `abActive`
  is logically atomic *with* the slots, the match memory and the undo clear — one restore — and
  publishing them independently exposes combinations no restore ever produced. The container and
  the strings cannot be made atomic at all.
- **C. Message-thread ownership, with two lock-free single-object handoffs.** Chosen.
- **D. C, with reference-counted snapshots reclaimed through `std::atomic<std::shared_ptr>` or a
  hazard pointer.** Rejected as the general form: the standard library's atomic `shared_ptr` is
  lock-based on every mainstream implementation, a hazard pointer is bespoke concurrency code the
  repository has no precedent for, and neither is needed once the host contract that state calls
  are serialized is used — the contract JUCE's `AudioProcessor` API already relies on.

## Decision

1. **Ownership.** All program metadata is **message-thread state**: only the message thread mutates
   it, the editor reads it there, and no other thread reads or writes it. The audio thread's inputs
   are unchanged.

2. **The restore handoff (host thread → message thread).** `setStateInformation` decodes the blob
   into a `RestoreDecode` on the caller's thread. The **sound** is applied there (JUCE-owned and
   thread-aware: the APVTS locks its tree, `ParameterAttachment` hops on its own) and so is the
   **engine-facing Settings atomic** (`InternalState::publishEngineConfig`), because a
   `prepareToPlay` that follows on the same host thread reads both. Then:
   - on the message thread (every in-spec VST3 host, the standalone, pluginval VST3, the state
     suite) the tail is adopted **inline**, exactly as before — nothing is deferred;
   - on any other thread the decode is published through `std::atomic<RestoreDecode*>::exchange`
     and the message thread adopts it in `adoptPendingHostState()`. Ownership transfers with the
     exchange: whichever side's exchange returns the pointer owns it, so a restore superseded before
     adoption is freed by the host side that supersedes it and at most one object ever exists.

3. **Adopt-before-use.** The adoption is drained by the processor's own 20 Hz timer (so it runs
   with no editor open), by the editor's tick (through `pollUndoCoalesce`), and at the top of every
   message-thread entry point that mutates program state — undo, redo, A/B switch and copy, preset
   load and save (`PresetManager::onAboutToLoad` / `onAboutToSave`), Apply gain, `createEditor`,
   and get/setState on the message thread. Sequential order is therefore preserved: a user action
   after a restore always sees the restore. **Never from inside a JUCE listener callback:**
   `AudioProcessorParameter::Listener::parameterGestureChanged` is delivered under the parameter's
   `listenerLock`, and an adoption takes the APVTS lock (`syncCommitted` → `copyState`) — the
   reverse of the order a host-thread `replaceState` takes the two. The first cut drained there and
   `--d2-stress-probe` reported the cycle as a lock-order inversion; a restore landing mid-gesture
   is adopted by the next poll, where it zeroes the gesture count exactly as an inline restore does.

4. **The program snapshot (message thread → host thread).** After every mutation of program
   metadata the message thread publishes an immutable `ProgramSnapshot` (name, baseline, selection,
   a copy of the Settings tree, the active index, both slots) through a second exchange cell —
   `PresetManager::onMetaChanged` and `InternalState::onChanged` report theirs, the A/B paths publish
   directly. An off-message-thread `getStateInformation` takes the latest into its own view and
   serializes from that plus the JUCE-locked `copyState()`. A slot the snapshot carries as invalid is
   resolved at save time from the live parameters and the snapshot's own metadata, which is what
   `abEnsureInit()` does on the message thread.

5. **The pending window, decided from the snapshot in hand (amended, round 2).** Between the host
   thread publishing a restore and the message thread adopting it, a host-thread save must describe
   the sound it just applied: the host side keeps the view it built from that restore, and uses it
   whenever the newest snapshot it holds carries a generation **older than its own last restore**.
   The generation is *part of the immutable snapshot* (`ProgramSnapshot::generation`: the last host
   restore the message thread had adopted when it published), so the decision is about the object
   the save is holding, whichever moment it took it. Round 1 read two generation atomics *after*
   taking the mailbox, and an adoption that completed between the take and the reads paired the
   pre-restore snapshot with a "nothing pending" answer — the restored sound serialized around the
   previous program's name, slots and Settings. The two counters are now each one side's plain
   state (`hostRestoreGen` on the host side, `adoptedGeneration` on the message thread) and cross
   the boundary only inside the objects (`RestoreDecode::generation`, `ProgramSnapshot::generation`).
   State test 37 pins the window; State test 42 reproduces the review's interleaving through a seam
   and pins that the save then equals the owner's.

6. **The serialized-text repair moves before `replaceState`.** A malformed value's repaired text used
   to be written into the live `apvts.state` after `replaceState`, outside JUCE's private lock — a
   race against any other thread's locked `copyState()`. It is now written into the private copy the
   caller is about to hand to `replaceState`, on both restore paths and on the A/B/undo apply, so the
   live tree is only ever written under JUCE's lock. One consequence is deliberate and better: the
   host is told the *repaired* value during `replaceState`, where it used to be told the clamped
   garbage first and then silently corrected. The end state, the durability of the repair
   (ER-STATE-25) and every existing state test are unchanged.

7. **The APVTS root type is read once**, at construction, into `apvtsStateType`; the decode no longer
   reads the live `apvts.state` handle, which `replaceState` reassigns under a lock the reader does
   not hold.

8. **The engine-config word: the latest restore wins (round 2).** The one thing a restore publishes
   for the audio side — the oversampling index `processBlock` and `prepareToPlay` read — is one
   `std::atomic<uint64>` carrying the index in its low byte and, in its high 32 bits, the generation
   of the *arrival* that published it. A publication lands only if no higher generation stands, and
   the comparison and the store are one compare-exchange (`InternalState::publishEngineConfig`), so
   there is no check-then-store window. A host-thread restore publishes with its own generation; the
   message thread's tree writes (`applyResolved`, a Settings edit) publish with the last generation
   it adopted (`noteAdoptedGeneration`). Round 1's overwrite — the adoption of restore A storing A's
   oversampling over restore B's, published from the host thread while A's tail was in flight, so
   an activation in that window primed the engine at A's setting — is therefore not representable:
   A's adoption carries A's generation and yields. The semantics chosen are **the latest restore
   supersedes an older one completely**: the sound already did (the APVTS holds the last
   `replaceState`), the cell already did (a superseded decode is freed by the `put` that supersedes
   it), and the word now does. A Settings edit is an arrival too (decision 9): it publishes the word
   under the generation of the latest restore that had arrived when it was made, so it lands over
   that restore and yields only to one that arrives later. The animation flag stays a plain
   message-thread mirror — its one reader is the imager on that thread — and the host thread no
   longer writes it. State test 43 pins the rule on the word alone (generations 1, 2, a delayed
   completion of 1, an edit in the window, a late republication, a newer restore over the edit)
   and the interleaving on the processor through the adoption seam.

9. **Settings edits and pending restores: precedence by arrival (round 3).** The six host-hidden
   Settings are message-thread state written by the editor's `juce::Value` bindings; an off-thread
   restore's values reach that tree only at the adoption. Round 2's adoption wrote all six
   unconditionally, so an edit made inside the pending window — *after* the restore had arrived —
   was replaced by the older restore: the one message-thread mutation not ordered after the restore
   it overlapped, because a binding write reaches this code only through the tree listener, after
   the fact, where every other mutating entry point drains first. The rule the rest of the design
   already has — *a user action inside the pending window lands on top of the restore, never under
   it* — is now enforced at the boundary that can enforce it, the adoption: an edit records, against
   its field, the generation of the latest restore that had arrived when it was made (the tag the
   engine-config word carries — the one place a restore's arrival is visible to the message thread
   before its adoption), and `InternalState::adoptResolved (resolved, generation)` keeps a field
   whose recorded generation is that restore's or later. So an edit made *before* a restore arrived
   carries a lower generation and is replaced — the restore is the newer arrival, exactly as an
   inline restore replaces everything — and one made *after* it stands, through that restore's
   adoption and any older one's. An inline (message-thread) restore is the newest arrival by
   definition and still writes every field (`applyResolved`). The models rejected: "the user edit
   always wins" would let a project load fail to restore a Setting the user had touched minutes
   earlier; "the restore always wins" is the defect; excluding Settings from restores changes the
   file format's meaning. The word follows: the edit publishes under that same generation, so the
   engine takes it at once; the adoption republishes from the whole tree afterwards, so the tree and
   the word agree after every adoption; a restore that arrives later carries a higher generation,
   wins the word, and replaces the field at its adoption. State test 44 pins all of it: each field
   alone, all six, an edit before the arrival, two restores around two edits through the adoption
   seam, the inline restore.

## Consequences

- **Realtime.** `processBlock`, `toEngine`, `setParameters` and `process` are unchanged. The one
  audio-thread read D-2 touches, `InternalState::oversampleIndex()`, is a relaxed 64-bit load masked
  to its low byte (round 2) where it was a relaxed `int` load — lock-free on every target, asserted
  at compile time; no cell, allocation, wait or branch is reachable from the audio thread.
  `check-realtime.py`, RTSan and the allocation guard see exactly what they saw.
- **No mutex, no `callAsync`, no `MessageManagerLock`, no wait**, on any thread. The two cells are
  `std::atomic<T*>::exchange`; the engine-config word is one compare-exchange on its writers (the
  host thread on a restore, the message thread on a tree write — never the audio thread) and a
  relaxed load on its reader; the H-side views are plain `unique_ptr`s a single serialized caller
  replaces. Lifetime is closed: each cell frees on replacement or in the destructor; nothing is freed
  while another thread can still reach it, because a pointer is reachable from exactly one side.
- **The message-thread path is unchanged** apart from one relaxed load at each entry point and one
  small allocation per metadata mutation. Serialization output is byte-identical (State test 3).
- **The off-message-thread path defers the metadata tail by at most one timer period (≤ 50 ms)**, and
  a user action landing inside that window is ordered after the restore — through adopt-before-use
  for every entry point that can drain, and through precedence by arrival (decision 9) for a
  Settings edit, which cannot. Both are inside the timing
  tolerance of a host restore and replace undefined behaviour. With the editor closed and a starved
  message queue (RISK-008's Linux case) the tail waits, the sound and the engine-config word are
  live, and host saves are served from the host side's own view.
- **A host-thread save describes ONE published program plus the live sound.** The metadata comes
  from one immutable snapshot and the parameters from one JUCE-locked `copyState()`, read one after
  the other; a message-thread action that mutates both between the two (an A/B switch: its parameter
  burst, then its index) can leave a save with the earlier program's index around the later sound.
  That is the host-timing class every non-blocking design has — the same as a save taken during a
  parameter burst on the message thread itself, or during host automation — and not the ordering
  class rounds 1 and 2 closed (two programs mixed, or an older restore standing over a newer one).
  Recorded, not fixed: a bounded retry would narrow it without closing it, and a lock would put the
  message thread's A/B path behind a host save.
- **The host contract that state calls are serialized is load-bearing** for the host-side views, as it
  already was for the whole `AudioProcessor` state API. It is stated in the header and asserted where
  it can be.
- **RISK-007 closes**; `THREADING_POLICY.md` and `THREAD_MODEL.md` gain the two paths and lose the
  "documented assumption" paragraph. The `tsan` CI lane keeps the property: the four probes under
  ThreadSanitizer, gated behind a liveness canary, fail the push that reopens it.

## Related code

- `src/PluginProcessor.h` — the ownership boundary comment, `ProgramSnapshot` (with its
  `generation`), `RestoreDecode`, `ExchangeCell`, the cells, the two per-side generation counters
  (`hostRestoreGen`, `adoptedGeneration`), the views, the two test seams.
- `src/PluginProcessor.cpp` — `adoptPendingHostState`, `adoptRestoreTail`, `decodeRestore`,
  `applySoundTree`, `repairSerializedValues`, `viewOfRestore`, `writeState`, `ownedProgram`,
  `publishProgram`; the drains at each entry point.
- `src/InternalState.h` — `resolveRestore` / `resolveLegacy` (any thread), `applyResolved` (the
  inline restore) and `adoptResolved` (an adopted one, keeping the fields edited after it arrived;
  message thread), `publishEngineConfig` / `noteAdoptedGeneration` / `engineConfigGeneration` (the
  generation-tagged engine-config word), `onChanged`.
- `src/PresetManager.{h,cpp}` — `onMetaChanged`, `onAboutToSave`, `soundSignatureFor`.
- `tests/state_tests.cpp` — State tests 37–41 and `--d2-stress-probe`; tests 22 and 27 re-shaped to
  the production off-thread path; State tests 42–43 (round 2), each reproducing its reviewed
  interleaving deterministically through a seam; State test 44 (round 3).

Evidence [Verified]:
- Baseline: `--state-thread-probe` and `--state-prepare-race-probe` under ThreadSanitizer on the
  pre-change tree, 10 runs each — a report in 10/10 runs (the `juce::String` exchange class, the
  others need a denser interleaving on the measuring machine); `--d2-stress-probe` on the pre-change
  tree — 220 reports in 2 runs (one of them hit the 900 s wall), every class the register records
  plus the Settings tree and the slot handles, and 12 of them heap-use-after-free.
- After: the four probes under ThreadSanitizer, repeated — silent; State tests 37–41 green; the
  full state and DSP suites green; `check-realtime.py` green; `preflight.sh` exit 0. Figures in
  `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §D-2.
- Round 2: State tests 42 and 43 fail on the round-1 tree with each fix reverted in isolation
  (mutation-tested: the mailbox chosen regardless of generation; the word stored regardless of
  generation) and pass with it; the four probes and the suite under ThreadSanitizer stay silent.
  Figures in the worklog's §D-2 round 2.
- Round 3: State test 44 fails on the round-2 tree with the adoption writing every field again
  (mutation-tested) and passes with decision 9; the four probes and the suite under ThreadSanitizer
  stay silent. Figures in the worklog's §D-2 round 3.
