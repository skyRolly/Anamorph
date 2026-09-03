# THREADING_POLICY.md

**Priority: 2.** System Policy — derived from the implemented threading model
(`docs/architecture/THREAD_MODEL.md`).

## Threads

Audio · Message/GUI · OpenGL render (macOS/Windows only) · (no worker threads).

## Allowed communication paths (only these)

| Direction | Mechanism | Rule |
|---|---|---|
| GUI → Audio (automatable params) | APVTS `std::atomic<float>*` | Read once per block into `EngineParameters`. |
| GUI → Audio (host-hidden) | `InternalState` ValueTree + the engine-config word | Only Oversampling crosses to audio, read as the low byte of one `std::atomic<uint64>` (`oversampleIndex()`, relaxed). Its writers — the message thread from the tree, a host-thread restore (D-2) — publish through one compare-exchange tagged with the generation of the arrival, and a publication lands only if no higher generation stands: the latest restore wins, an older restore's completion never overwrites it (ADR-0036 §8). A Settings edit is an arrival too: it publishes under the generation of the latest restore that had arrived, lands over it, and its field survives that restore's adoption (ADR-0036 §9). |
| GUI → Audio (momentary solo) | `std::atomic<int> soloPreviewMask` | −1 = use the param; relaxed. |
| GUI → Audio (meter reset) | `std::atomic<int> resetReq` | `exchange` consumed on the audio thread. |
| Audio → GUI (scope) | `ScopeBuffer` SPSC ring | Exactly one producer + one reader **thread** (message thread; stateless read sites: Vectorscope, SpectrumImager, read-only `writeCount`); release/acquire on the write index. |
| Audio → GUI (meters/correlation/match) | published `std::atomic<float>` (relaxed) | Audio writes in `publish()`; GUI reads via getters. |
| Audio → GUI (sound-param change generation) | `std::atomic<uint32> soundParamGen` (relaxed) | A monotonic staleness hint, **not** payload sync: bumped on any sound-param value change (the per-parameter listener, on whichever thread changes the value) and on host restore; the GUI compares it to skip rebuilding its 24 Hz signature caches. Carries no payload — the values themselves cross via the APVTS atomics above — so relaxed is sufficient (no ordering/publication role). |
| Audio/host → Message (latency re-report request, D-1) | `std::atomic<int> latencyUpdateRequest` — **release** store, **acquire** `exchange` — plus the engine's `latency2/4/8` (relaxed `std::atomic<int>`) | The one ordering-critical pair besides the scope ring: the flag PUBLISHES the parameter, oversampling or (round 15) prepare write that raised it, and a processor-owned 20 Hz timer delivers `setLatencySamples` on the message thread. Raised by `requestLatencyUpdate()` from any non-message thread — the APVTS listener under host automation, `setStateInformation`'s tail, an off-message-thread `prepareToPlay` — and served synchronously when the caller IS the message thread. |
| Audio → GUI (view-param / InternalState generations, Wave 2 / H15) | `std::atomic<uint32> viewParamGen`; `InternalState::gen` (relaxed) | The identical staleness-hint pattern, extended so the editor's 60 Hz micro-anim poll re-arms on counter loads instead of hashing every animated widget per frame: `viewParamGen` is bumped by a dedicated no-gesture listener on the view params (Bypass), `InternalState::gen` by its property-change callback (Settings values, incl. session restore). No payload, no ordering role. |
| Host state thread → Message (a restore's metadata tail, D-2 / ADR-0036) | `ExchangeCell<RestoreDecode> pendingRestore` — one `std::atomic<T*>`, `exchange` with **acq_rel** on both sides | An off-message-thread `setStateInformation` applies the sound (the APVTS, JUCE-locked) and the engine-config word (tagged with this restore's generation) on the caller's thread, then publishes the DECODED metadata tail as one immutable object carrying that generation; the message thread adopts it (`adoptPendingHostState`) from the processor's 20 Hz timer and at the top of every entry point that mutates program state. Ownership transfers with the exchange — whichever side's exchange returns the pointer frees it — so at most one object exists and nothing is freed while reachable elsewhere. On the message thread the tail is adopted inline; nothing is deferred. |
| Message → Host state thread (the program snapshot, D-2 / ADR-0036) | `ExchangeCell<ProgramSnapshot> programMailbox` (same cell); the snapshot carries the generation of the last restore the message thread had adopted when it published | The message thread republishes an immutable snapshot of the program state it owns after every mutation (`PresetManager::onMetaChanged`, `InternalState::onChanged`, the A/B paths); an off-message-thread `getStateInformation` takes the latest into its own view and serializes from it plus the JUCE-locked `copyState()`. While the newest snapshot it holds carries a generation older than its own last restore, it serializes from the view it built from that restore, so a save after a restore on the same host thread describes the sound it applied — and because the generation is part of the snapshot, the decision is about the object in hand, never about a generation read at another moment (round 2, review finding 1). |

## Forbidden cross-thread access

- No painting, allocation, locking, or IO on the audio thread.
- No direct access to non-atomic shared state across threads (the only synchronisers are the
  listed atomics + the SPSC ring).
- No second producer on `ScopeBuffer`, and no reads off the message thread (one writer + one
  reader thread by construction; reads are stateless `const` peeks — `readLatest` / `writeCount`
  never mutate, so multiple message-thread read sites are safe).
- PDC/latency must be recomputed on the **message thread** via the `const`, race-free
  `predictLatency` — never by mutating audio-thread state from the message thread.
  **KI-027 (filed 2026-08-31, RESOLVED 2026-09-01 under decision D-1, approved by the maintainer):**
  under VST3 host automation of Drive/Algorithm the APVTS listener delivers `parameterChanged` on
  the **audio** thread, and it used to call `setLatencySamples` from there. It now calls
  `requestLatencyUpdate()`, which delivers synchronously only when the caller IS the message
  thread and otherwise stores one atomic request that a processor-owned 20 Hz timer serves on the
  message thread — so the rule above holds by construction rather than by assumption
  (`docs/architecture/LATENCY_MODEL.md`; State tests 22 and 27).
  **Round 15 (2026-09-02, ER-STATE-19) routed `prepareToPlay` through the same request:** a host
  that activates the plug-in off the message thread (JUCE's Linux VST3 wrapper before — or
  without — a host `IRunLoop`; FL Studio's Patcher; an AU `Initialize` off main, which is how
  pluginval, the macOS release gate, calls it) used to deliver
  from that thread concurrently with the timer; now the message thread is the only writer, and the
  engine's `latency2/4/8` are relaxed atomics whose ordering rides on the flag. State test 30;
  `AnamorphStateTests --reprepare-race-probe` under ThreadSanitizer.

## Host state calls: a covered path (D-2 / ADR-0036)

Host-driven `getStateInformation`/`setStateInformation` may arrive on **any** thread: on VST3 the
pinned SDK annotates both calls `[UI-thread]` (JUCE debug-asserts it for `setState`), on the **macOS
AU** no spec forbids off-main-thread `SaveState`/`RestoreState` (host autosave is the real-world
case, and pluginval's AU background-thread state test produces exactly that window), and JUCE's
Linux VST3 wrapper services the plug-in's messages from its own thread until the host registers an
`IRunLoop`. Since D-2 the rule is: **every piece of program metadata this plug-in owns — the preset
name, identity and dirty baseline, the A/B slot set, the undo history, the committed baseline and
gesture bookkeeping, and `InternalState`'s Settings tree — is message-thread state; only the message
thread writes it, only the message thread reads it directly, and a host thread reaches it through
the two exchange cells in the table above.** The audio thread's inputs are unchanged: the APVTS
parameter atomics, `InternalState`'s oversampling atomic and `soloPreviewMask`, published exactly as
before. RISK-007 is closed; `AnamorphStateTests --state-thread-probe`, `--state-prepare-race-probe`
and `--d2-stress-probe` under ThreadSanitizer, in the `tsan` CI lane, keep it closed.

`prepareToPlay` is unchanged by D-2: the tables cover the paths it creates, not the thread it
arrives on. Its engine body relies on the format contract that no `processBlock` runs concurrently,
which every wrapper guarantees; its latency report does not depend on the thread at all (round 15,
ER-STATE-19). It reads the oversampling atomic, which an off-message-thread restore stores
synchronously before it returns — the ordinary setState-then-activate order therefore primes the
engine from the restored Setting from the first sample, as it did before D-2.

The adoption never runs inside a JUCE listener callback. `AudioProcessorParameter::Listener`
callbacks (`parameterValueChanged`, `parameterGestureChanged`) are delivered under the parameter's
`listenerLock`, and adopting a restore takes the APVTS lock (`syncCommitted` → `copyState`) — the
reverse of the order a host-thread `replaceState` takes the two (the APVTS lock, then `listenerLock`
through `setValueNotifyingHost`). That cycle is a real deadlock between a host-thread restore and a
gesture start on the message thread; `--d2-stress-probe` reported it as a lock-order inversion when
the first cut drained in the gesture callback, and the rule is now: **nothing that takes the APVTS
lock runs from a parameter listener callback.** A restore landing mid-gesture is adopted by the next
poll, which zeroes the gesture count exactly as an inline restore always has.

One contract is load-bearing and is stated rather than assumed: **the host serializes its own state
calls** (never two at once). The host-side views of the two cells are read and replaced by the
single caller that contract implies — the same contract JUCE's `AudioProcessor` state API already
relies on.

## Atomic usage rules

- Published meter/correlation/match values: `memory_order_relaxed` (monotonic display data).
- `soundParamGen`: `memory_order_relaxed` — a generation / staleness counter only. It gates a
  message-thread cache rebuild and transfers no payload, so it is deliberately **not** an
  ordering/publication primitive (unlike the scope index below).
- Scope ring index: `release` on write, `acquire` on read (the one ordering-critical pair). The
  index is published **once per block** (`pushBlock`), so a reader that acquires it sees a whole
  committed block — never a partially written one.
- `latencyUpdateRequest`: `release` on the store, `acquire` on the `exchange` — the second
  ordering-critical pair (D-1). The engine's `latency2/4/8` are `relaxed`: their ordering rides on
  that flag, and they carry no ordering of their own.
- The two D-2 cells (`pendingRestore`, `programMailbox`): `exchange` with `acq_rel` on both sides —
  the third ordering-critical pair. The exchange PUBLISHES the immutable object's contents to the
  side that takes it and TRANSFERS ownership in the same operation. The generations ride INSIDE
  the objects (a decode's, a snapshot's), so no separate atomic pair is read at a different moment
  than the object it is about. The one relaxed load is the empty-check fast path, which only decides
  whether to attempt the exchange.
- The engine-config word (`InternalState::engineConfig`): `compare_exchange` with `acq_rel` on
  the writers (a host-thread restore, the message thread's tree writes), `relaxed` on the audio
  reader — a value with no payload behind it. The tag is the generation of the arrival, and the
  CAS refuses a lower one: "latest arrival wins" is decided and stored in one operation.
- The OpenGL context is attached only on macOS/Windows; all Linux/BSD rendering is on the
  message thread (`docs/architecture/design-decisions/ADR-0011`).

Evidence [Verified]:
- Source: src/dsp/ScopeBuffer.h:28-80; src/dsp/LevelMeters.h:125-198; src/dsp/Correlation.h:50-190;
  src/PluginProcessor.cpp:94-116, 307; src/InternalState.h:175, 441-464
- D-2: src/PluginProcessor.h (the ownership boundary comment, `ExchangeCell`, the cells and
  generations); src/PluginProcessor.cpp (`adoptPendingHostState`, `setStateInformation`,
  `getStateInformation`); ADR-0036; State tests 37–41; the `tsan` job in
  `.github/workflows/build.yml`

## Enforcement

A change to the thread model, a new shared-state path, or a new atomic ordering triggers the
**Architecture Review Gate** and an **AI Agent Hard Stop**. Changing this policy requires an ADR.
