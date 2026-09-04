# ADR-0036 — Program state is message-thread-owned; host threads exchange immutable snapshots

**Status:** Accepted (Thread Model change — maintainer instruction 2026-09-03: resolve D-2 / RISK-007).
**Amended 2026-09-03, round 2** (the PR review's two findings): decisions 5 and 8 — the generation
travels *inside* the snapshot, and the engine-facing oversampling is a generation-tagged word where
the latest restore wins. **Round 3** (one further finding): decision 9 — a Settings edit made after
a restore *arrived* is the newer arrival and survives that restore's adoption. **Round 4**: decision
10 — adopting a restore installs its sound *and* its metadata, so a message-thread action taken in
the handoff window can never be left half-applied under the new session's identity; and decision 11
— the host-serialization contract this design rests on, verified against every wrapper this
repository builds.

> **Architecture Review Gate: APPROVED** (human architecture review, 2026-09-03). This ADR records
> a **Thread Model change** (`ARCHITECTURE_REVIEW_GATE.md`), which that policy forbids merging on a
> green build and `AI_AGENT_POLICY.md` classes as an agent Hard Stop that only human review clears.
> That review has been given for the architecture below. **What was approved** is the model these
> decisions define: message-thread ownership of all program metadata; the two single-object
> exchange cells (`pendingRestore`, `programMailbox`) with ownership transferring on the exchange;
> the generation carried inside each immutable object rather than in a separate atomic; the
> generation-tagged engine-config word with its latest-arrival-wins compare-exchange; and the
> precedence rules for a user action that overlaps a restore (§9, §10, §12). Later work stays
> covered while it stays inside those decisions; anything that adds a thread, a cross-thread path
> or an ordering-critical atomic beyond them is a new gated change and must be flagged as one
> rather than treated as covered. Round 5 (§12) is inside the boundary: it re-keys an existing
> guard onto a new *relaxed* counter that carries no payload and no ordering role — the same class
> as `soundParamGen`, which predates D-2 — and adds no path and no ordering. **Round 6 (§13) is
> inside it too**: it changes only *how* that counter's value is captured — from a read-back to the
> value the allocating `fetch_add` returns — introducing no thread, no cross-thread path, no
> ownership mechanism and no ordering-critical atomic. There is no architectural delta to review.
> **Round 7 (§14) likewise**: it moves an existing relaxed counter's increment from the start of an
> operation to its end and brackets the operation with a second read of the same counter. No thread,
> no blocking mechanism, no cross-thread ownership change; the delta is *when* an existing counter is
> incremented. **Round 8 (§15, §16) as well**: it makes an existing drain run to a fixed point and
> reorders an existing save so its baseline and its bytes come from one session. No thread, no
> blocking mechanism, no new cross-thread ownership; both changes are on the message thread.
> **Round 9 (§17) as well, and it REMOVES machinery rather than adding any**: it deletes a retry loop
> and one of two reads of the live sound, so the save derives its bytes and its baseline from a single
> `apvts.copyState()`; and it canonicalises an existing signature onto the grid a preset file can
> already hold. No thread, no lock, no wait, no allocation on any audio path, no new cross-thread
> reader, no serialization-format change and no parameter or latency change. Everything it touches
> runs on the message thread. There is no architectural delta to review.
> **Round 12 (§21) as well**: it publishes one more message-thread fact inside the snapshot that
> already crosses — the per-field Settings edit generations — so a host-thread save can apply §9's
> existing per-field rule instead of a whole-object one. No new thread, cell, lock, wait or
> allocation on any audio path, no new cross-thread reader (the same single object crosses the same
> single cell), no serialization-format change, no parameter or latency change. There is no
> architectural delta to review.
> **Round 11 (§19, §20) as well, and it too removes machinery**: it deletes a tolerance and the
> comparison that used it from one signature builder, and deletes a second tolerance of the same
> shape from the restore path's per-parameter write gate, leaving a single tolerance-free
> definition and a restore that writes what the session stored. No thread, no lock, no wait, no
> allocation on any audio path, no new cross-thread reader, no serialization-format change, no
> parameter or latency change. There is no architectural delta.
> **Round 10 (§18) as well**: it moves an existing derivation (the A/B toggle's destination) from the
> editor to the processor, after the drain the processor already performs; narrows an existing
> publication from the whole Settings tree to the one field that changed; and replaces a read-back
> with a computation from the values already being written. No thread, no lock, no wait, no
> allocation on any audio path, no new cross-thread reader, no serialization-format change, no
> parameter or latency change. There is no architectural delta to review.

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

10. **Session coherence: an adoption installs the sound too (round 4).** A restore handed over
   from a host thread applies its sound *there*, at decode time (decision 2 — a `prepareToPlay`
   behind it must prime the restored session), and its metadata lands at the adoption. Between the
   two there is a window the message thread cannot see into: from the decode installing the sound
   to `pendingRestore.put`, the cell is still empty, so every entry point's drain finds nothing and
   an A/B switch, a Copy, an undo or a preset load runs — against parameters that are already the
   incoming session's. Such an action stores those parameters into the *outgoing* session's slot
   and applies another, so the live sound is then neither session's. Round 3's tail wrote metadata
   only, so the adoption stamped the restore's name, identity and slots over that sound: one saved
   session assembled from two.
   The rule is now that **adopting a restore commits sound and metadata together** — the decode
   keeps the parameter tree it installed (`RestoreDecode::soundParams`) and the adoption
   re-installs it. Two guards keep that from costing anything or going backwards: the decode also
   records `soundParamGen` as it stood immediately afterwards, so an adoption whose sound nothing
   has touched re-installs nothing (every ordinary restore, no redundant `setValueNotifyingHost`
   burst); and it re-installs only while the engine-config word still carries *this* restore's
   generation, so a restore that a newer one has superseded never resurrects its own sound
   (decision 8's rule, applied to the sound). An inline restore applies its sound on the message
   thread with nothing able to run in between and needs neither guard.
   **Precedence is unchanged and explicit:** an action the message thread takes before a restore is
   in the cell ran against the outgoing session, and a session restore replaces that session — the
   action is superseded, exactly as an inline restore replaces everything (this is *not* decision
   9's case: the Settings are orthogonal preferences a session load carries, the A/B slots and
   preset identity *are* the session). What decision 10 forbids is not the supersession but the
   split: after the adoption the sound, the slots and the identity are one session's.
   **The inline path drains first (round 4).** A restore arriving *on* the message thread now
   adopts a pending one before it decodes, because decoding applies the incoming sound: draining
   afterwards published the older session's metadata while the newer session's parameters were
   already live. State tests 45 and 46 pin both halves.

11. **The host serializes its own state calls — disposition D, "unsupported host concurrency"
   (rounds 4–5).** The host-side
   members (`hostRestoreGen` and the two views) are plain because only `getStateInformation` and
   `setStateInformation` touch them, and a host runs at most one of those at a time. That is the
   contract JUCE's whole `AudioProcessor` state API already rests on, and it was checked against
   every wrapper this repository builds (`ANAMORPH_FORMATS`: VST3, AU, Standalone) at the pinned
   JUCE 9.0.1:
   - **VST3** — `JuceVST3Component::setState` calls `assertHostMessageThread()` (the SDK annotates
     both calls `[UI-thread]`); `getState` reaches `getStateInformation` on the caller's thread with
     no lock of its own. In spec both are the message thread, so they are serialized *and* serialized
     against the editor; out of spec they are the D-2 case this ADR exists for.
   - **AU** — `SaveState` / `RestoreState` are `MusicDeviceBase` overrides that call
     `getStateInformation` / `setStateInformation` straight through on the caller's thread. Neither
     takes `getCallbackLock()` (the wrapper takes it for `processBlock` and offline-mode changes
     only) and neither takes a `MessageManagerLock`. Serialization is the host's, via the
     `kAudioUnitProperty_ClassInfo` property mechanism — the real-world autosave case, and what
     pluginval's `BackgroundThreadStateTest` exercises.
   - **Standalone** — both calls are made from `StandaloneFilterWindow` on the message thread.
   **Round 8 re-verified the whole disposition mechanically against the current tree** and found no
   new evidence: no Anamorph caller of either state function exists (both names appear in `src/`
   only as their own definitions); the only schedulers in `src/` are the editor's 24 Hz and the
   processor's 20 Hz `juce::Timer`s, both on the message thread, with no `std::thread`,
   `juce::Thread`, `callAsync` or thread pool anywhere; `hostRestoreGen` and the two host views are
   read or written at exactly four sites, all inside the off-message-thread branches of the two
   state functions; and the tripwire is constructed at exactly those same two branches. The
   disposition below therefore stands unchanged.
   **Round 6 re-derived this from the complete set of callers**, which is the evidence a wrapper
   inspection alone does not give: across the three formats `ANAMORPH_FORMATS` builds, the ONLY
   code that reaches `AudioProcessor::get/setStateInformation` is the host-facing entry points
   themselves — VST3 `IComponent::getState`/`setState` (plus the VST2-compat readers *inside*
   `setState`), AU `SaveState`/`RestoreState`, and the standalone's `savePluginState()` /
   `reloadPluginState()`. **No JUCE timer, async callback, audio callback or background thread
   calls either function on any of them**; the standalone's own 500 ms timer scans MIDI devices and
   touches no state, and its two calls run from `init()` and the close button, both on the message
   thread. So the concurrency question reduces entirely to whether the *host* issues two overlapping
   calls — there is no JUCE-side scheduler that could produce one behind the host's back.
   The **primary evidence** for the VST3 half is the pinned SDK header itself, which annotates both
   halves of the pair on the host's UI thread — `IComponent::setState`: *"\note [UI-thread &
   (Initialized | Connected | Setup Done | Activated | Processing)]"*, and `IComponent::getState`
   identically (`format_types/VST3_SDK/pluginterfaces/vst/ivstcomponent.h`). Two calls both pinned
   to one thread cannot overlap, so on VST3 the ordering is contractual, not conventional. On AU no
   clause pins them and the wrapper adds nothing, so serialization there is the host's practice.
   No wrapper serializes save against restore *for* the plug-in, and none of them can: the guarantee
   is the host's. Anamorph therefore relies on **exactly** what JUCE relies on and nothing stronger.
   **The disposition is therefore D — concurrent host state calls are possible only from a host that
   is already violating its format's contract, and supported operation excludes them.** Not A,
   because the AU half rests on practice rather than a citable clause; not B, because no wrapper
   provides the serialization; not C, because nothing in supported operation produces the
   concurrency, and adding synchronisation would mean paying for a broken host on every save.
   **The debug tripwire, audited (round 6).** `OffThreadStateCall` detects exactly one condition:
   two *off-message-thread* state calls active at once. It cannot fire in a supported scenario — on
   VST3 an off-thread state call is already out of spec and two overlapping ones doubly so; on AU
   the calls may legitimately be off-thread but the host serializes them; on the standalone they are
   on the message thread and never counted at all. It also cannot fire on the supported shapes that
   look similar: an off-thread save concurrent with the message thread's own timer adoption (not a
   state call), or with a message-thread `getStateInformation` (not counted). It is **purely
   diagnostic** — the counter is read only by the `jassert`, which compiles out in release, so no
   release-build behaviour depends on it and it is not standing in for a correctness mechanism in
   any supported case. If it ever fires, the conclusion above is what needs revisiting, which is the
   point of having it.
   Rather than synchronise a case the formats forbid, the two off-message-thread branches count
   themselves in (`offThreadStateCalls`) and a debug build asserts if a second one ever overlaps —
   a tripwire, never a lock, never blocking, with no effect on the result. A host that trips it is
   broken in a way that breaks every JUCE plug-in, and the assertion says so where it happens.

12. **A sound edit made while a restore is pending survives it (round 5).** §10 made the adoption
   re-install the restored sound, so that an action which had replaced the live parameters with
   another session's could not be left half-applied under the restored identity. Its guard asked
   "has anything touched the parameters since the decode", read from `soundParamGen` — which every
   knob turn bumps. So an ordinary sound edit made in the pending window was treated as another
   session's sound and erased on adoption.
   Those two are different things and the rule now names the difference. A **wholesale replacement**
   — an A/B apply, an undo or redo, a preset load — means the live sound is some *other* session's,
   and the adoption re-installs its own (§10, unchanged). An **edit** means the restored session is
   live with a newer mutation in it: the parameters the user turned are the ones the decode had
   just installed, so the edit is an edit *of the restored session*, and keeping it is the same
   rule the rest of the design already follows — a user action inside the pending window lands on
   top of the restore, never under it (§9 for the Settings, adopt-before-use for every entry point
   that can drain). The adoption leaves it alone.
   **The discriminator** is `soundSetGen`, bumped once per wholesale replacement of the live
   parameters — at `applyStatePreservingView` (A/B, undo/redo), at `applySoundTree` (a restore's
   own install) and at the preset load hook, which installs its session one parameter at a time
   rather than through `replaceState` — and *not* by an individual parameter change. The decode
   records it immediately after installing its sound; the adoption re-installs only if it has
   moved. It is a relaxed staleness counter with no payload and no ordering role, exactly like
   `soundParamGen`: the value that matters travels inside the `RestoreDecode`, whose cell provides
   the ordering.
   **Why sound parameters differ from the A/B slots and the preset identity**, which §10 supersedes:
   those *are* the session — a restore replaces them by definition, and an A/B navigation of the
   outgoing session means nothing in the incoming one. A parameter value is not session identity;
   it is the thing the user is currently editing, and after the decode the thing they are editing
   is the restored session. The Settings (§9) reach the same answer by the same reasoning.
   State test 47 pins it: one edit, several edits across two parameters, and an untouched parameter
   still holding the restored value. Mutation-tested — with the guard keyed on `soundParamGen`
   again, all of it is erased.

13. **A restore's token identifies its own replacement, not a moment (round 6).** §12's
   discriminator is a counter of wholesale sound replacements, and §10's adoption re-installs the
   restored sound when that counter has moved since the decode. Round 5 recorded the decode's
   reference value by **reading the counter back** after applying the sound — which answers a
   different question than the one the adoption asks. The counter names the *latest* replacement,
   so a replacement that landed between the decode's own install and that read-back was recorded as
   the restore's own reference: the counter then already equalled it, the adoption concluded
   "nothing has replaced my sound", and the restored metadata was published over the *other*
   operation's sound. The split §10 exists to prevent, reached through the token instead of through
   the handoff window.
   **The token is now the value the allocating `fetch_add` returns.** `noteWholeSoundReplaced()`
   hands each replacement a value no other replacement can be handed, `applySoundTree` returns the
   one it was handed, and the decode records *that* — so "is the live sound still the one I
   installed?" is answered by comparing the counter against an operation's own identity rather than
   against a snapshot of shared state. Any replacement after the restore's own, whenever it lands,
   leaves the counter above the token and the adoption re-installs; nothing else can be mistaken
   for the restore's sound. Allocation and capture are one atomic operation, so there is no window
   between them to interleave with — the defect is not narrowed, it is unrepresentable.
   **Individual mutation versus wholesale replacement**, by state semantics rather than by
   function name — this is the classification §12's rule rests on:

   | operation | what it does to the live sound | class |
   |---|---|---|
   | a knob turn, a gesture, host automation | changes one parameter of the session that is live | **individual** — belongs to the live session (§12), survives the adoption |
   | A/B apply / switch (`applyStatePreservingView`) | installs the other slot's whole state set | **wholesale** — another session's sound |
   | undo / redo (`applyStateSet` → `applyStatePreservingView`) | installs a stored state set | **wholesale** |
   | preset load (`PresetManager::applySoundTree`, via the load hook) | installs the preset's whole sound, one parameter at a time, and changes the preset identity with it | **wholesale** — the per-parameter mechanism is why the hook bumps the counter explicitly; the classification follows the semantics, not the call shape |
   | a restore's own sound install (`applySoundTree`) | installs the restored session's sound | **wholesale**, and the one that allocates the token it later checks |

   State test 48 pins all three: an A/B apply and a preset load overlapping the decode are each
   superseded by the adoption, and an ordinary edit *at the identical instant, through the same
   seam* survives it. Mutation-tested — with the read-back restored, both replacement legs mix.

14. **A replacement's token is allocated at COMPLETION, and its writes are bracketed (round 7).**
   §13 gave each wholesale replacement an identity no other replacement can be handed. It allocated
   that identity at the operation's **start**, so the counter ordered replacements by when they
   *began* — and begin order is not completion order. Completion order is the one that matters:
   every wholesale replacement writes *every* sound parameter, so the live sound belongs to whichever
   finished last.
   The interleaving that separates them: an A/B apply (or an undo, or a preset load) begins and takes
   token *n+1*; the restore's own sound install then runs to completion and takes *n+2*, which the
   decode records; the A/B apply then finishes writing, so the live parameters are **its**, while the
   counter still reads *n+2*. At the adoption `counter == d.soundSetGen`, the guard concludes
   "nothing has replaced my sound", the re-install is skipped, and the restored metadata is published
   over the other operation's sound — §10's split again, now through the *ordering* of the token
   rather than its ownership.
   **Two rules fix it, and both are needed.**
   - **Completion.** Every wholesale replacement bumps the counter *after* its last sound write,
     never before — `applySoundTree` after `reassertParameters`, `applyStatePreservingView` after the
     view-parameter restore, the preset load in `onLoaded` rather than `onAboutToLoad` (its writes go
     through `PresetManager::applySoundTree` one parameter at a time, so the hook is where its
     completion is observable; the two hooks are paired on every path that can succeed). The rule is
     uniform on purpose: a scheme where one path increments at the beginning and another at the end
     orders nothing.
   - **Bracketing.** A caller that will *keep* its token samples the counter into `begin` before its
     first write and allocates after its last: exactly one bump in between (`token == begin + 1`)
     proves no other wholesale replacement began *and* finished inside ours, and therefore that ours
     is the replacement the live sound belongs to. Anything else means the two interleaved — their
     per-parameter writes are not mutually excluded, so the live parameters may hold values from
     both — and `soundReplacementToken` returns **0**, "no owner provable". The counter starts at 1
     and only rises, so 0 is never a real token and can never compare equal: the adoption
     re-installs, which is the conservative answer and the one that restores a coherent session.
   Together these close the residual window completely rather than narrowing it. There is no
   interleaving in which another replacement's writes land inside the restore's without the counter
   showing it: a replacement that finishes after ours raises the counter above our token, one that
   finishes inside our bracket breaks `token == begin + 1`, and one that finishes entirely before our
   first write wrote parameters we then overwrote.
   **This does not reclassify anything.** An individual mutation (a knob, a gesture, host automation)
   still bumps nothing, so §12's rule is untouched: an ordinary edit after the restore's install
   leaves `counter == token` and survives the adoption. State test 49 pins the ordering for the three
   replacements that share the mechanism — an A/B apply, an undo and a preset load, each held before
   its writes while the restore completes inside it — and State tests 47 and 48 keep the edit control.

15. **The drain runs to a FIXED POINT (round 8).** Every message-thread entry point calls
   `adoptPendingHostState()` before it reads or mutates program state, and what it needs from that
   call is not "one restore was adopted" but **"nothing is pending any more"** — only then is the
   state it goes on to edit the state of every restore that has arrived, and only then does §10's
   precedence rule (an action after a restore's arrival lands on top of it) actually hold for that
   caller. The drain took exactly one restore, so a second arriving *during* the adoption stayed in
   the cell: the caller edited a session that was already superseded, and the later adoption
   wholesale-overwrote the edit — even though the edit came after that restore's arrival, which is
   precisely the case §10 says must survive. The loop closes it: when `adoptPendingHostState()`
   returns, the cell is empty, so any mutation the caller then makes is after every arrived restore
   and is superseded only by one that arrives strictly later.
   It is a **drain, not a wait**: it stops as soon as the cell is empty and never blocks. It cannot
   spin indefinitely in supported operation because a host serializes its own state calls (§11), so
   a further restore can only appear after a whole `setStateInformation` has returned.
   State test 50 pins it through the adoption seam — two seam fires inside one `abSwitchTo`, and the
   switch applied to the *second* restore's slot set. State tests 43 and 44 measured their
   intermediate instants between two drains; those instants now fall inside one drain, so both take
   the same measurements at the seam's second fire instead, assertion for assertion.

16. **A preset's clean baseline and its bytes come from one session (round 8).** `isDirty()` is
   `current sound != sigAtLoad`, and the user-visible meaning of **clean** is *reloading the
   selected preset reproduces the sound you are hearing*. `saveUser` wrote the file, **then** ran
   `onAboutToSave` (which drains a pending restore, replacing the live sound and metadata), **then**
   read the live sound as `sigAtLoad`. The restore sat between the bytes and the baseline: the file
   held the outgoing session's sound while the baseline came from the restored one, so the preset
   was marked clean against a sound its own file does not contain — reloading it changed what you
   heard while the indicator said nothing had changed. A durable mislabel, in a file.
   Two changes, and the rule they implement is that **the baseline is the signature of the sound the
   file was written from**:
   - **The drain runs first**, before the sound is captured, so the save writes the same session the
     rest of the program is on — the adopt-before-use every other message-thread entry point does.
   - **The signature and the state are captured as one coherent pair**, the signature read *before*
     the state copy so the two can only disagree in the safe direction (a sound that moved between
     them leaves the baseline describing the earlier state, which reads as *dirty* rather than as a
     false clean), and a sound-generation check across both reads confirms they describe one sound,
     retrying the two cheap in-memory reads — never the file write — when it does not.
     **This second bullet is SUPERSEDED by §17 (round 9), and its stated safety argument was wrong.**
     The retry gave up after eight attempts and used the unproven pair anyway, and "disagreeing only
     in the safe direction" does not hold: a baseline naming an *earlier* sound reads clean again the
     moment the sound returns to it, which is what cycling automation does by definition. The first
     bullet — the drain runs before the capture — stands unchanged and is still required.
   State test 51 checks the invariant the way a user meets it: save, then load the very file just
   written, and require the sound to be unchanged and still clean.
   **`load`'s baseline is not the same defect.** It also reads `sigAtLoad = soundSig()` after
   applying the preset's sound, so a restore landing in between would baseline the preset against
   the restore's sound — but nothing durable is written, and the restore's own adoption overwrites
   `current`, `sel` and `sigAtLoad` outright (`setMeta` / `adoptRestoredState`), so the mislabel
   cannot outlive the pending window it was created in. Transient and self-correcting, where the
   save's was durable: recorded, not changed.

17. **One capture. The bytes and the baseline are the same object (round 9).** §16 established that a
   preset's clean baseline must describe its own file, and implemented it by taking two reads of the
   live sound — a signature, then the state copy the file is written from — and trying to prove them
   coherent with a sound-generation check that retried up to eight times. Under **sustained
   automation** that check never settles: the loop fell through and wrote the unproven pair, so the
   file held one sound and `sigAtLoad` described another. The consequence is the one §16 exists to
   prevent, reached by the other route — and it is a **false clean**, not merely a stale baseline,
   because a baseline naming an earlier sound compares equal again as soon as the sound returns to
   it. Cycling automation does exactly that, which is why the finding is phrased as *busy automation
   saves false baseline*: the preset sits there marked clean while reloading it changes what you hear.
   Measured on the round-8 implementation with a parameter cycling between two values across the
   save: the baseline named width `0.24000` while the bytes on disk held `0.76000`, and the preset
   read **clean** at `0.24` while loading its own file moved the sound to `0.76`.

   **The rule is now structural rather than checked.** `saveUser` takes **one** capture —
   `apvts.copyState()`, which flushes the live parameters into the APVTS tree under JUCE's own lock
   and returns a private copy — and derives *everything* from that single object: the bytes on disk,
   and the clean baseline via `PresetManager::soundSignatureForSavedTree`. Nothing reads the live
   parameters a second time, so there is no "between" for a mutation to land in. The retry is
   **deleted, not bounded harder**: no number of retries can establish an invariant that one capture
   gets for free, and a retry that gives up silently is worse than none. `saveUser` now contains no
   loop of its own; the one it still reaches, through `onAboutToSave`, is §15's restore drain, whose
   termination rests on §11's supported-host boundary rather than on anything in the save.

   **What clean means after `saveUser` returns**, stated for a user: *loading the selected preset
   would change nothing.* And therefore **the mutation-during-save semantic is (A)**: the preset
   represents the state captured at the one capture instant, and any parameter change after it —
   including one that lands while the file is still being written — leaves the preset **dirty**,
   correctly and immediately, because the live sound has moved away from what the file holds. The
   plug-in never retries, never waits for automation to settle, and never silently saves a state the
   user cannot get back.

   **A torn capture is still one snapshot, and that is the right answer.** JUCE's
   `flushParameterValuesToValueTree` writes adapters one at a time, so a parameter moved from the
   audio thread mid-flush can leave the tree holding parameter A at one instant and parameter B at
   another. No non-blocking design can prevent that, and it does not weaken the invariant: whatever
   mixture of values the flush captured **is** the preset, the baseline is computed from that same
   object, and the two therefore agree by construction. The invariant this ADR owes the user is
   "clean means reloading changes nothing", not "the file is an instantaneous photograph".

   **One rule for what a saved tree means.** `soundSignatureForSavedTree` and
   `PresetManager::applySoundTree` resolve every parameter through the same helper
   (`normalisedFromSavedTree`): value present and usable → `convertTo0to1(plain)`; absent, value-less,
   malformed or non-finite → the parameter default. The baseline is therefore the signature the
   *loader* will produce, by construction rather than by argument, and a hand-edited file cannot mean
   one thing to the apply path and another to the baseline.

   **The signature is the sound as the plug-in renders it.** `normalisedAsRendered`
   (`PluginParameters.h`) maps a normalised value onto the grid the plug-in can actually render and
   keep: `convertTo0to1(convertFrom0to1(v))`. This is a **no-op for every stock parameter** —
   `juce::AudioParameterFloat` stores the denormalised value and reports `convertTo0to1` of it, so its
   `getValue()` is already on that grid. It matters for Anamorph's `RawChoice` / `RawBool`
   (`src/PluginParameters.cpp`), which deliberately keep the *exact* normalised value in `getValue()`
   so pluginval's state-restoration test reads back what it wrote; the session format preserves that
   bit-for-bit through the `raw` attribute, but **a preset has no `raw`** and stores only the snapped
   value. Signing the live unsnapped value therefore described a sound no preset file can hold: a
   4-choice automated to `0.66` was written as index 2 and reloaded as `0.66667`, so §16's invariant
   failed for every discrete parameter carrying an automated sub-step value — **with no concurrency
   involved at all**. Measured: baseline `0.66000`, bytes `0.66667`, reload `0.66667`.
   What this deliberately stops counting as a modification is movement *within* one step (`0.66` →
   `0.67`, both index 2). That is not a sound change — the DSP reads `getRawParameterValue()`, i.e.
   the snapped value — and the preset format cannot record the difference, so a modified-marker for it
   would report a change the user can neither hear nor keep.

   **APPLY IT EXACTLY ONCE, and the file side already has.** This is the sharp edge, and the first
   implementation of this section got it wrong. `apvts.copyState()` writes the **denormalised** value
   into the tree, so `normalisedFromSavedTree` returning `convertTo0to1(plain)` is already
   `convertTo0to1(convertFrom0to1(live))` — character for character the expression the live side
   computes. Canonicalising the file side *again* makes it that mapping twice, and while the mapping
   is the identity in real arithmetic it is **not idempotent in float** for the four frequency
   parameters whose ranges are built from custom log/exp conversion lambdas with no interval to snap
   to (`logFreqRange`, `logFreqRangeCentred`). The two sides then part company in the last bits and,
   at a 5-decimal rounding boundary, in the printed signature — so a **freshly saved preset reads
   MODIFIED**, deterministically for that value, on a preset nobody has touched. Measured at 4 sweep
   points in 20001 before the second application was removed. The rule is therefore: the live side
   applies `normalisedAsRendered` once; the file side applies it zero times, because the tree gave it
   one for free.

   **One definition of "the sound", across the plug-in.** `AnamorphAudioProcessor::soundSignature`
   (the undo / A-B coalescer) is built from `normalisedAsRendered` too. The two signatures answer
   "has the sound changed?" for different purposes and over different parameter sets, but they must
   not answer it *differently for the same movement*: signing the raw normalised value in one and the
   rendered value in the other would record an undo step for a sub-step move on a discrete parameter
   that the modified-marker simultaneously declares to be no change — an undo entry that, pressed,
   moves neither the sound nor the star.

   **Compatibility.** No file format changes: neither the preset format nor the session schema gains,
   loses or reinterprets a field. The one visible edge is that `presetBaseline` strings written by an
   older build are compared against a signature computed the new way, so a session saved by an older
   build can restore showing a modified-marker it did not show before. It applies to **two** cases,
   not one: a discrete parameter carrying a host-automated sub-step value (always), and any of the
   four custom-mapped frequency parameters at a value where the float round trip crosses a
   5-decimal boundary (measured at roughly 1 value in 500). Cosmetic, no sound change, and
   self-correcting on the next save or preset load — both of which recompute the baseline under the
   new definition. Recorded as rule 6 in `SESSION_COMPATIBILITY_POLICY.md`.

   **AN OPEN SIBLING, NOT CLOSED BY THIS ROUND: `load` and `loadFile` still baseline by re-reading.**
   Both apply the file's sound and then take `sigAtLoad` from a second, live read
   (`sigAtLoad = soundSig()`), which is the structure this section condemns — and the consequence is
   the same false clean: with automation cycling a parameter, a write landing between the apply and
   the read makes the baseline describe the automated value, so the menu reads *unmodified* while
   reloading the preset would move the sound. It is durable. §16 recorded `load`'s baseline as a
   self-correcting transient, but that judgement was made about a **restore** landing in the window
   (the restore's own adoption overwrites `sigAtLoad`); an **automation** write is overwritten by
   nothing, and that case was not considered.
   It is **not fixed here, deliberately**, because every remedy examined costs more than it saves and
   the choice is a product decision that wants numbers rather than a reflex:
   - Baseline from the tree (`soundSignatureForSavedTree(apvts, sound)`, the obvious symmetry with
     the save) removes the window entirely, but a load applies `convertTo0to1(plain)` and the
     parameter then reports what it *stores*, so the post-load sound passes through the range mapping
     once more than the baseline did. For the four custom-mapped frequency ranges that extra pass is
     not idempotent, and the preset then reads **modified immediately after being loaded**. Measured
     by State test 52 leg (i): 2 divergences in 3000 round trips, i.e. of order **1 preset load in
     1500** — a spurious dot on the single most common preset action, traded for a window that needs
     an automation write inside a few instructions.
   - Re-reading each parameter inside the apply loop shrinks the window by roughly the length of the
     loop but does not close it, which is a smaller lie rather than a true statement.
   - A generation check across the two reads is round 8's retry, deleted above for the reason above.
   The residual is therefore **reported, measured and left open**: a rare false clean on the load
   path, against a measured ~1-in-1500 false *dirty* as the price of the symmetric fix. The direction
   matters — this ADR treats a false clean as the dangerous one — so it should be closed, but by a
   decision that accepts the cost, not by this round's momentum.

   State test 52 pins the rest, through the `beforeStateCapture` seam so the interleaving is exact
   rather than a race to lose: one mutation in the window, sustained cycling automation, several
   parameters at once, a real concurrent automation thread, and the sub-step discrete case that needs
   no concurrency. The assertion is the user-visible invariant — *if the preset reads clean, reloading
   it changes nothing* — checked at **both** values the automation cycles through, so a baseline that
   named either of them is caught without the test having to guess which. Three further legs pin what
   the arguments above assert: a **sweep** measures the signature equality instead of arguing it —
   20001 normalised points × 33 parameters, uniform then pseudo-random, since a uniform grid alone
   never meets a rounding boundary (measured 0 mismatches; 4 with the canonicaliser applied twice,
   199 of 201 with it removed altogether, and it asserts that every preset-carried parameter is
   ranged, which pins `soundSignatureForSavedTree`'s one approximate branch as dead); a
   **freshly-saved-preset-reads-clean** leg at a custom-mapped frequency value, the same property as
   a user meets it; and a **coherence** leg proving a sub-step move on a discrete parameter moves
   neither the modified-marker nor the undo history.

18. **A decision is derived from the authoritative session at the moment it commits; a publication
   carries only what is authoritative at its generation; a baseline is computed from what is written
   (round 10).** Three review findings, one rule seen three times: a value was derived from state
   read *before* the drain or adoption that made the state authoritative, and then committed *after*
   it. The bounded ordering audit this round opened with (§13 of the worklog) found no fourth
   shared mechanism to fix at a lower boundary — each is a caller getting the rule wrong at its own
   site, so each is fixed at its own site and the rule is what this section records. The round's
   own entry-point audit then found a **fourth** instance, fixed the same way (below).

   **The A/B toggle (finding 1).** The editor computed the toggle's destination itself —
   `abSwitchTo (abActiveSlot() == 0 ? 1 : 0)` — from a read taken before `abSwitchTo`'s own drain
   adopted a pending restore. A restore that flipped the active slot made the computed target the
   slot the restore had just made active, `slot == abActive` held, and the switch returned without
   switching: the toggle did nothing, silently. **The rule:** an A/B action chooses its target from
   the session the plug-in is on once every arrived restore has been adopted — never from a slot a
   caller observed earlier. `abToggle()` implements it on the processor: drain, then derive the
   other slot of the post-drain `abActive`; the editor calls that. `abSwitchTo (int)` remains the
   primitive for an **explicit** target, which is intent rather than a stale derivation: "go to B"
   after a restore that already put the session on B is correctly a no-op. Nothing about the slot
   contents, §10's precedence or rounds 4–7 changes; the round moves one derivation across a
   drain. State test 53 pins both flips and the explicit-target distinction.
   **The preset prev/next step (the audit's fourth instance).** `PresetManager::step` computed
   "the row after this one" from `currentIndex()` and then called `load`, whose drain (through
   `onAboutToLoad`) adopts a pending restore — *after* the index was chosen. A restore that moved
   the selection had Next land on the row after the **old** selection. Same shape, same fix: a new
   `adoptPending` hook (wired to `adoptPendingHostState`) is fired before the index is read, so the
   step is relative to the authoritative row. `onAboutToSave` is the save path's instance of the
   same hook; the two are documented as one rule at two sites. State test 56 pins Next and Prev
   across a restore that moves the selection.

   **Settings publication (findings 2 and 3, one mechanism).** While a host restore is *pending* —
   arrived on its thread, not yet adopted — the Settings tree is only partly authoritative: the
   restore has already published its Oversampling into the engine-config word under its own
   generation (§8), and the tree still holds the outgoing value until the adoption writes it.
   `valueTreePropertyChanged` republished the **whole tree** on every property change, tagged with
   the latest arrival's generation. So an unrelated Settings edit — Meters on — re-published the
   tree's *stale* Oversampling under the pending restore's own generation, which the
   compare-exchange accepts as "that arrival again": the engine dropped back to the old factor
   until the adoption put the restored one back. A publication borrowed a generation for a value
   that was not that generation's.
   **The publication invariant:** *a publication to the engine carries exactly the fields whose
   values are authoritative at the generation it is tagged with.* A single property change is
   authoritative for that property and nothing else, so it publishes that one field
   (`publishField`: the animation mirror for `uiAnimations`, the tagged word for `oversample`,
   nothing for the GUI-only fields). The whole tree is published only where the whole tree has just
   been made coherent for a generation — the end of `writeResolved`, and the constructor. An edit to
   Meters can no longer say anything about Oversampling because it does not carry it. The same rule
   makes a restore's field-by-field write independent of the table's order; with the current table
   (Oversampling first) that was never observable, so it is a guarantee, not a closed defect.
   **When an edit happens, for ordering against restores** (finding 3), is defined as the instant
   its callback reads the engine-config word's generation: the latest restore that had *arrived* as
   far as the message thread can observe, and the edit is ordered after it — recorded against its
   field at that generation, so `adoptResolved` keeps it against that restore and every older one
   (§9), and a restore that arrives later carries a higher generation and replaces it. Both
   observable orderings are exact and State test 54 pins them: a restore published *before* the
   edit's callback is superseded by the edit at its adoption; one published *after* replaces it.
   **The boundary, stated rather than hidden:** the binding's tree write precedes the callback's
   read by JUCE's listener dispatch, and a restore landing inside that gap is observed as having
   arrived *before* the edit, so the edit stands. That is the user-favouring resolution of an
   instant the message thread cannot observe more finely without a lock — the user's explicit
   action is never silently undone by a restore that landed within microseconds of it — and with
   the publication invariant in force its only consequence is the edited field itself. The review's
   framing of that boundary as "the earlier edit defeats the later restore" is the same observation
   from the other side; nothing this ADR promises is violated by it, and this section makes the
   choice explicit. One more interleaving is a transient rather than a boundary: a restore whose
   `publishEngineConfig` lands between the edit callback's generation read and `publishField`'s
   compare-exchange leaves the tree holding the edit's Oversampling and the word the restore's
   until the next adoption — a few instructions to at most one timer period, after which the
   adoption writes the tree and the whole-tree publish is idempotent. Nothing is lost and nothing
   persists; recorded so it is not re-raised as new.

   **The load-side baseline (finding 4, KI-029 — closed).** `load` and `loadFile` applied the
   preset's sound and then took `sigAtLoad` from a second, live read — the two-read shape §17
   removed from the save. Host automation writing a sound parameter between the apply and that read
   made the baseline describe the automated value, so the preset read clean whenever the automation
   returned to it while reloading it would move the sound. §17 left it open because the symmetric
   remedy (the save-side `soundSignatureForSavedTree`) made a just-loaded preset read *modified* at
   a measured 2 in 3000. **The reason was found and removed:** a save's baseline describes live
   state the tree was captured *from*, but a load's describes live state written *from* the tree
   and then read back through the parameter's own store/report pair — `setValue` stores
   `convertFrom0to1(x)` and `getValue` reports `convertTo0to1` of it — which is one range mapping
   deeper, and not idempotent in float for the four log-mapped frequency ranges. Modelling that pass
   — `soundSignatureAfterLoading` applies `normalisedAsRendered` **twice** to the resolved value —
   is the same arithmetic as the post-load `soundSignatureFor`, and on the reference Release build
   the two agree at all 3000 sweep points where the save-side formula misses 2. **But "the same
   arithmetic" is only bit-exact if the compiler emits it identically at both sites, and it need
   not**: the prediction is one inlined expression chain, the live value passes through a store to
   `std::atomic<float>`, and fused-multiply-add contraction can differ between them. The
   ThreadSanitizer build disagreed at ≥ 1 of 3000 points and failed the first version of State test
   55. So the load paths **reconcile** the prediction against one read-back, per parameter, at the
   signature's own resolution (`loadBaselineFromTree`): where predicted and live agree to within a
   few float ulps — five hundred times below the smallest real parameter step — they are the same
   value and the *live* form is taken, because it is what `soundSignatureFor` will keep producing;
   where they differ by more, something other than this load wrote the parameter and the
   *predicted* value stands, so the preset reads dirty rather than absorbing the write. The
   read-back therefore has no window to lose: a foreign write inside it is detected by magnitude
   and rejected. The factory path does the same over its override table. The result holds on any
   toolchain: the baseline a load sets equals what the parameters report (a just-loaded preset is
   exactly clean — measured 0 in 3000 on both builds), and automation landing during the load leaves
   the preset dirty. The pure prediction is kept as a public function for measurement and is
   asserted to be within float tail of the live value everywhere; its exact string agreement is
   printed, not asserted, because it is the toolchain-dependent quantity the reconciliation exists
   to absorb.
   **KI-029 is resolved as MUST FIX, fixed**, and the invariant is now uniform across save and load:
   *the clean baseline is derived from the artifact — the bytes written or the bytes applied — and
   a mutation that lands during the operation leaves the preset dirty rather than being absorbed
   into its baseline.* The load side reaches it through a prediction reconciled at signature
   resolution rather than a bare prediction, for the reason above.

   **Where the seam sits.** `beforeStateCapture` fires immediately before a baseline is fixed: before
   the save's one capture, and after a load's apply. State tests 52 and 55 mutate a parameter there;
   in the fixed code the mutation leaves the preset dirty, in the read-back code it was absorbed.

   **Host serialization**, re-checked for new evidence on this tree and found none: the two state
   functions still appear in `src/` only as two declarations and two definitions with no Anamorph
   call site; the only timers are the editor's 24 Hz and the processor's 20 Hz, both message-thread;
   `hostRestoreGen` and the two host views are touched at the same four sites inside the two
   off-message-thread branches; the tripwire is read only by a `jassert`; and the four additions of
   this round (`abToggle`, `publishField`, `soundSignatureAfterLoading`, the `adoptPending` hook)
   are message-thread-only and unreachable from either state function. Disposition D stands.

19. **One equivalence, no tolerance layered on it (round 11).** §18 closed KI-029 by deriving the
   load's clean baseline from the values the load writes rather than reading the parameters back.
   It then added a **reconciliation** on top: the predicted baseline was compared against one live
   read-back, and where the two agreed to within `1e-6` the *live* value was taken. The
   justification was that the prediction is not bit-exact across toolchains — the ThreadSanitizer
   build had failed the exact-equality assertion in State test 55.

   **That justification was wrong, and the tolerance it bought was a hole.** A tolerance cannot
   distinguish compiler noise from a real automation write of the same magnitude. So an automation
   write landing in the load window, if smaller than `1e-6`, was **absorbed into the baseline**: the
   preset then read *clean* against a sound its own file does not hold — the exact failure §17 and
   §18 exist to prevent, reintroduced by the mechanism meant to protect §18.

   **Re-measured, and the premise does not hold.** With a temporary probe: 20 000 random normalised
   values × 33 parameters comparing the prediction against the live signature after
   `setValueNotifyingHost`, and 3 000 complete save → `copyState` → XML text → re-parse → `loadFile`
   round trips — **zero** differences, at float level and at the five-decimal string level, in **both**
   the Release and the ThreadSanitizer build. The XML text round-trips every value exactly.

   **Round 10's named mechanism is refuted outright.** It was fused-multiply-add contraction
   differing between the inlined prediction chain and the value routed through the parameter. Every
   x86-64 target in this project — the ThreadSanitizer build among them — is compiled under ADR-0031
   with `-march=haswell -ffp-contract=off`, measured there at **0 FMA instructions emitted**. No
   interleaving of that build could have exhibited the mechanism it was blamed on.

   **What produced the red run is a candidate, not a finding, and is recorded as one.** Two suite
   instances sharing a fixed probe path in the temp folder reproduce the same shape of failure on
   demand: run concurrently, this comparison shows hundreds of mismatches with value differences up
   to ~1.9e4 — one process reading the other's file — and round 10 itself root-caused exactly that
   collision later in the same round, for a different test, recording the one-instance-at-a-time rule
   in `TESTING.md`. But round 10's own reproduction listed failures in tests 10, 12, 13, 24, 28 and
   52, **not** 55. So the collision is available as a mechanism and is not claimed as the measured
   cause. What *is* established is enough to decide: the arithmetic is exonerated in the build that
   failed, and the tolerance bought with that failure was unjustified. Round 11 removes the fixed
   shared temp paths from the suite so the mechanism cannot recur (`juce::File::createTempFile`); the
   preset **folder** is still shared, which is what the one-instance rule covers.

   **The reconciliation and its constant are deleted.** The signature has exactly one equivalence,
   stated once and applied identically by every producer:

   > Two sounds are the same when their **rendered normalised values agree to five decimal places**.

   "Rendered" is `normalisedAsRendered` (§17): the value the plug-in actually renders and stores —
   the DSP reads the denormalised, interval-snapped value, and both the preset format and the session
   tree keep that same number. Nothing is layered on top of it, and no comparison anywhere in the
   preset/undo/A-B signature machinery uses a tolerance.

   **What the quantisation itself absorbs — stated, not implied.** One signature bucket is `1e-5` of
   normalised range. That is a *resolution*, so it must be measured against the parameter set rather
   than asserted to be harmless:

   | domain | count | smallest step | vs one `1e-5` bucket |
   |---|---|---|---|
   | on a grid (interval-snapped) | 29 | 8.08e-5 (Chorus Rate, top of range) | 8.08 × |
   | gridless (log-mapped frequency) | 4 | — | no grid at all |

   For a **grid** parameter no two legal settings can share a bucket, so a real edit always shows.
   The binding case is Chorus Rate — `{0.05, 5.0, interval 0.001, skew 0.4}`, the set's only skewed
   range — whose top-of-range step is 8.08e-5; every other grid parameter is wider (Output Gain
   2.08e-4, Haas Delay 2.94e-4, Drive 4.17e-4, the 0.001-interval controls 5e-4 to 1e-3). The margin
   is **~8 ×, not the ~50 × an earlier draft of this section recorded** — 50 × is the Width family
   alone, and a future range change checked against that number could narrow a cell below one bucket.
   State test 57 therefore *asserts* the margin over the whole parameter set instead of quoting it.

   For the four **gridless** ranges the bucket is the only resolution there is: at worst **1.38 Hz**
   at the 20 kHz end of the three multiband splits and **0.013 Hz** on Mono Maker Freq. A move
   smaller than that, at a value that does not straddle a bucket boundary, is not reported. It does
   not hide indefinitely either — successive sub-bucket writes cross a boundary, after which the
   preset reads dirty and stays dirty (State test 57's accumulation leg drives exactly that, and
   asserts that more than one nudge was needed, so the leg is about accumulation and not about a
   single lucky crossing).

   **The two parameter domains, and why they differ on purpose.** A grid parameter absorbs movement
   *inside one cell* — §17's deliberate rule, since such a move changes neither the DSP input nor
   anything a file can hold — and reports movement across a cell boundary, which is a whole step. It
   is **not** true that a tiny write can only reach the signature on the gridless ranges: `snapToLegalValue`
   rounds, so a sub-`1e-6` write that straddles a snap boundary moves a whole step and is correctly an
   edit. State test 57 asserts all of it — inside a cell is not an edit, one cell is, a sub-`1e-6`
   write across a snap boundary is — for Amount *and* for Chorus Rate, the tightest margin in the set.

   **Why a boundary is the only place the tolerance could ever have shown.** A tolerance finer than
   the signature's own resolution can change the answer only where the two values straddle a
   five-decimal rounding boundary. State test 57 therefore *searches* a deterministic grid for such a
   value and its sub-`1e-6` nudge, and asserts it found one before using it — in both directions, on
   both log mappings, and each leg **confined to its own band** of the normalised range, because an
   unconstrained search always terminates within a few steps of zero and four legs then measure one
   neighbourhood. Its baseline oracle is an undisturbed load of the same file, not the function under
   test. Restoring the reconciliation fails **ten** checks across State tests 55 and 57.

   **Nothing else moves.** §17's single-capture save is untouched; §18's load baseline is still
   derived from the tree with no live read — in fact more purely, since the read-back is now gone
   entirely. The two loaders now fire the `beforeStateCapture` seam at the same instant (after the
   apply, before the baseline), so they are interchangeable fixtures, and State test 57 covers the
   sub-`1e-6` write through both. State test 55's equality assertion, which round 10 had weakened to
   "within float tail" with the count merely printed, is restored to exact string equality — asserted
   because it *is* the product invariant (a just-loaded preset must read clean), so a toolchain that
   broke it would be reporting a real defect on that toolchain, to be fixed by making the two sides
   agree and never by widening the comparison.

   **Recorded, not changed — one sibling of the same SHAPE, outside this round's finding.**
   `viewOfRestore` also predicts `presetBaseline`, and for the two cases where the restore carries no
   usable one (absent, or present-but-empty) it predicts the live sound's signature *at decode time*
   while the adoption's `setMeta`/`adoptRestoredState` fallback recomputes it *at adoption time*. A
   sound edit between the two therefore moves it, exactly as the Settings moved before this round. It
   is narrower — it needs a session with no stored baseline, i.e. pre-0.6 or one saved on a nameless
   slot — it predates round 12, and no review finding names it, so it is recorded here rather than
   changed inside a closure round.

   **Recorded, not changed.** Two live reads survive inside otherwise tree-derived signatures: the
   non-ranged fallbacks in `soundSignatureAfterLoading` and `soundSignatureForSavedTree`. They are
   unreachable in this plug-in — every parameter `createAnamorphLayout` builds is a
   `RangedAudioParameter`, which State test 52 asserts rather than assumes — but a future non-ranged
   preset-carried parameter would reopen the KI-029 load window for it, not merely approximate it.
   Separately, `lastPolledSig` (`PluginProcessor.h`) is assigned on every poll and read nowhere; it is
   dead state, not a comparison, and is left for a cleanup outside this round.

20. **A restore writes what the session stored, and is a fixed point (round 11).** The sibling audit
   §19 called for found one more comparison of the deleted shape, on the restore path rather than a
   signature path: `reassertParameters`'s per-parameter write gate was
   `! (std::abs (norm - rp->getValue()) <= 1.0e-6f)`. The value it declined to write is the value the
   **session** stored, while the baseline travelling with that session is adopted verbatim by
   `adoptRestoredState` — so the combination the gate protected is "live value from before, baseline
   from the file", which is the shape that makes an untouched preset show the modified star after a
   project reopen, an A/B toggle or an undo. Round 11's own argument applies verbatim: a tolerance
   cannot tell a float tail from a real difference of the same size.

   **It is exact now**, written as a negated `==` so a NaN on either side still counts as "differs"
   and gets repaired, exactly as the negated `<=` did.

   **The property that had to be measured first** is that a restore is a **fixed point** — a host may
   apply one chunk any number of times, and a per-application float-tail nudge would walk the sound,
   which is what the tolerance was silently buying. Measured with the gate exact: 2 000 random sounds
   × 20 re-applications of their own chunk move **no parameter at all** (worst delta 0.0) and move no
   signature; 3 000 project save/reopen round trips leave the preset marker clean on a fresh instance
   and on the same one. The tolerance had been declining 23 writes per 198 000 — all float tails on
   the four gridless ranges, none of them needed to reach the fixed point. State test 58 asserts the
   fixed point (values, signature, and byte-identical re-serialization) and the clean reopen.

   **Stated honestly:** no test discriminates the two gates, because none can — the two agree on every
   observable in that measurement space. The change is justified as removing an epsilon that had no
   argument behind it and sat on a coherence path, not as fixing a reproduced failure. State test 58
   guards the property that makes the exact form safe.

21. **A save describes the session the adoption will produce, field by field (round 12).** A host
   save taken while a restore was pending wrote the restore's Settings **as decoded**, so a Setting
   the user had already changed inside that window vanished from the project. Reproduced
   deterministically: State test 59 fails 7 checks against the round-11 tree, one per Settings field
   plus the all-six leg.

   **The mechanism is two counters that do not move together.** A restore has two generations. Its
   **arrival** is the engine-config word's CAS on the restoring thread — the one instant the message
   thread can see a restore before adopting it — and that is what a Settings edit records against its
   field (§9) and what `adoptResolved` compares against. Its **adoption** is `adoptedGeneration`,
   which moves at exactly one site inside the drain, and that is what stamps
   `ProgramSnapshot::generation` and what `covered` compares against. A Settings edit can therefore
   *never* raise the generation of the snapshot it publishes: inside the pending window that snapshot
   is guaranteed to be rejected, however many edits it carries.

   **`covered` is right, and stays.** In that window the message thread still holds the OUTGOING
   project's preset name, baseline, selection and A/B slots while the live parameters already hold
   the restored sound. Accepting the snapshot would rebuild exactly the mixed state §5 forbids and
   State test 42 pins. Raising the snapshot's generation on an edit is the same mistake wearing a
   different hat, and is not the fix.

   **The defect is one field of the other branch.** `viewOfRestore` is documented as *what the
   message thread will own once it adopts this restore*, and every field honours that — the name's
   absent-vs-empty rule, the baseline fallback, invalid slots left for save-time resolution. Except
   one: it takes the Settings as `d.internalResolved`, which models `applyResolved` — the inline
   restore's *write every field* rule — while the adoption that will actually run is `adoptResolved`,
   which **keeps** a field edited at or after this restore arrived. Since round 3 those have been two
   different functions and the host-side view was never moved onto the second. The view is also
   immutable and built before the edit exists, so nothing can repair it in place.

   **The invariant.**

   > A host save must describe the session the plug-in **will hold once every restore that has
   > arrived has been adopted**: the restore's program, with each Settings field carrying the value
   > that field will hold after the adoption. Equivalently — a save taken inside the pending window
   > must equal the save the owner takes immediately after it.

   Its falsifiable form: **a completed Settings edit is absent from a save only if a strictly later
   restore has arrived, or the save never observed the edit's publication.** The second clause is not
   a new boundary — it is §18's, restated for the save: a save reflects every edit whose publication
   it observed and none it did not.

   **Stated for the Settings, and only for them.** The equivalent "a save inside the window equals the
   save the owner takes after the adoption" does NOT hold as a general statement about the whole blob,
   and this section does not claim it does. The sound is read live, so an *edit* to it is in the save
   and survives the adoption (§12) — consistent. But a wholesale replacement that overlaps a decode is
   deliberately re-installed by the adoption (§13's token rule, State tests 48–49), so a save taken
   between the two describes the replacement's sound where the adoption will describe the restore's.
   That case belongs to §13 and is pinned there; it is named here so §21's invariant is not read wider
   than it is.

   **Whole-session precedence and per-field precedence are different rules, and one comparison cannot
   serve both.** Preset name, baseline, selection, the A/B slot set and the active index are the
   session: a newer arrival replaces them wholesale, which is what `covered` implements. The six
   Settings are orthogonal preferences the session carries, and since §9 they are per-field,
   arrival-ordered state. The save now decides them separately, per field.

   **The mechanism, and why it is the existing one.** The snapshot carries the per-field edit
   generations **alongside the tree they describe** — `ProgramSnapshot::settingsEditGen`, published
   by `ownedProgram()` from `InternalState::editGenerations()`. The host side reads both out of the
   one immutable object in hand, so a publication landing between two reads can never pair a tree
   with someone else's generations: the same rule round 2 established for the object-wide generation,
   applied to the field-wise ones. The merge is `InternalState::resolvedWithEdits`, which is
   `adoptResolved`'s own predicate — `editIsNewerThan`, now named and used by both — applied to
   values instead of in place. There is no second notion of "the current Settings": there is one
   predicate, called from the adoption and from the save.

   No new thread, cell, lock, wait or allocation on any audio path; no serialization-format change
   (the blob's Settings child is written exactly as before, from a tree resolved one step later);
   no parameter or latency change. §18's publication invariant is intact — a publication still
   carries exactly what is authoritative at its generation; it now says so per field rather than for
   the object as a whole.

   **Why "drain in the edit path" is not the fix**, though it looks smaller. An edit can land between
   the restoring thread's engine-config CAS and its `pendingRestore.put`: the word already carries
   the new generation, so the edit records it and will survive the adoption, yet the cell is still
   empty and there is nothing for any drain to adopt. That sub-case is immune to draining, and it is
   inside the reported window — and it is not a nanosecond-wide gap either: `viewOfRestore` runs in
   it, formatting all 33 parameters into a signature, and so does the `RestoreDecode` copy. Reordering
   to put-then-publish would only move the problem: an edit landing between the put and the publish
   would read the OLD arrival, store its own oversampling index under it, and be overwritten by the
   restore's publication at the higher generation — the §8 CAS ordering the word exists to keep.
   Draining would also move a full restore adoption — a sound re-install, an undo-history clear, a
   preset-metadata swap — into a `ValueTree` property-changed callback raised by an editor binding.

   **Residuals, stated.** An edit whose publication lands after the save's `programMailbox.take()` is
   invisible to that save — §18's boundary, not a new one, and State test 59's leg (h) pins the
   behaviour rather than hiding it. The listener-dispatch gap inside `valueTreePropertyChanged` is
   unchanged in either direction. Only the six Settings are decided per field: a preset rename, an
   A/B copy or an undo inside the window is still discarded by `covered` in favour of the restore's
   session, which is §5/§10's deliberate answer. And the whole of `hostRestoreView` remains a
   PREDICTION — if the processor is destroyed before the drain runs, those bytes describe a state
   that was never live. That residual belongs to §14's mechanism and is not touched here.

   **The bounded audit of the same family found no sibling.** Every other message-thread mutation of
   program state drains first, by construction (`adoptPendingHostState` at the top of the A/B switch,
   the A/B copy, `step`, the preset load and save hooks, the undo poll, the editor's construction,
   Level-Match apply, and both message-thread state calls), so by the time it publishes,
   `adoptedGeneration` already equals the arrival and `covered` is true. The Settings edit path is the
   only message-thread mutation that deliberately does **not** drain — §9 exists because it cannot —
   and it is the only one that needed this. The sound needs nothing: `writeState` reads the live APVTS,
   so a knob turned inside the window is already in the blob, and `adoptRestoreTail` re-installs the
   decode's sound only for a wholesale replacement, never for an edit (§12).

   State test 59 pins all of it: the reported case per field and all six at once; an edit before the
   arrival, which the restore correctly replaces; an edit after two arrivals, which stands over the
   later restore; an edit between two arrivals, which the later restore supersedes and the overlay
   must not resurrect; a save whose snapshot was taken before the edit published, which correctly
   carries the restore's values while the very next save carries the edit; saves on both sides of the
   adoption; and the identity half — the overlaid save still names the restore's session, never the
   outgoing one. Reverting the merge fails **14** checks.

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
- **A save describes one session.** The metadata comes from one immutable snapshot (or, inside the
  pending window, from the view built with that restore) and the sound from one JUCE-locked
  `copyState()`; an adoption installs both halves of a restore together (decision 10). A save that
  observed the sound of one restore with the metadata of another would have to run *while* a
  `setStateInformation` was between its two publications — that is, two host state calls at once,
  which decision 11 establishes cannot happen on any wrapper this repository builds.
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
- `src/PluginProcessor.h` — `RestoreDecode::soundParams` / `soundGen`, `OffThreadStateCall`.
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
  interleaving deterministically through a seam; State test 44 (round 3); State tests 45–46
  (round 4), through the handoff-window and adoption seams.

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
- Round 4: State test 45 fails with the sound re-install removed and State test 46 with the inline
  drain moved back after the decode (each mutation-tested in isolation); the wrapper reading behind
  decision 11 is `juce_audio_plugin_client_VST3.cpp`, `juce_audio_plugin_client_AU_1.mm` and
  `Standalone/juce_StandaloneFilterWindow.h` at the pinned JUCE. Figures in the worklog's §D-2
  round 4. **The Architecture Review Gate is open** (status block above).
