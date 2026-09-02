# THREADING_POLICY.md

**Priority: 2.** System Policy — derived from the implemented threading model
(`docs/architecture/THREAD_MODEL.md`).

## Threads

Audio · Message/GUI · OpenGL render (macOS/Windows only) · (no worker threads).

## Allowed communication paths (only these)

| Direction | Mechanism | Rule |
|---|---|---|
| GUI → Audio (automatable params) | APVTS `std::atomic<float>*` | Read once per block into `EngineParameters`. |
| GUI → Audio (host-hidden) | `InternalState` ValueTree + atomic mirror | Only Oversampling crosses to audio (via `osAtomic`). |
| GUI → Audio (momentary solo) | `std::atomic<int> soloPreviewMask` | −1 = use the param; relaxed. |
| GUI → Audio (meter reset) | `std::atomic<int> resetReq` | `exchange` consumed on the audio thread. |
| Audio → GUI (scope) | `ScopeBuffer` SPSC ring | Exactly one producer + one reader **thread** (message thread; stateless read sites: Vectorscope, SpectrumImager, read-only `writeCount`); release/acquire on the write index. |
| Audio → GUI (meters/correlation/match) | published `std::atomic<float>` (relaxed) | Audio writes in `publish()`; GUI reads via getters. |
| Audio → GUI (sound-param change generation) | `std::atomic<uint32> soundParamGen` (relaxed) | A monotonic staleness hint, **not** payload sync: bumped on any sound-param value change (the per-parameter listener, on whichever thread changes the value) and on host restore; the GUI compares it to skip rebuilding its 24 Hz signature caches. Carries no payload — the values themselves cross via the APVTS atomics above — so relaxed is sufficient (no ordering/publication role). |
| Audio/host → Message (latency re-report request, D-1) | `std::atomic<int> latencyUpdateRequest` — **release** store, **acquire** `exchange` — plus the engine's `latency2/4/8` (relaxed `std::atomic<int>`) | The one ordering-critical pair besides the scope ring: the flag PUBLISHES the parameter, oversampling or (round 15) prepare write that raised it, and a processor-owned 20 Hz timer delivers `setLatencySamples` on the message thread. Raised by `requestLatencyUpdate()` from any non-message thread — the APVTS listener under host automation, `setStateInformation`'s tail, an off-message-thread `prepareToPlay` — and served synchronously when the caller IS the message thread. |
| Audio → GUI (view-param / InternalState generations, Wave 2 / H15) | `std::atomic<uint32> viewParamGen`; `InternalState::gen` (relaxed) | The identical staleness-hint pattern, extended so the editor's 60 Hz micro-anim poll re-arms on counter loads instead of hashing every animated widget per frame: `viewParamGen` is bumped by a dedicated no-gesture listener on the view params (Bypass), `InternalState::gen` by its property-change callback (Settings values, incl. session restore). No payload, no ordering role. |

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

## Host state calls: a documented assumption, not a covered path

The tables above are exhaustive for the paths the plug-in *creates*. Host-driven
`getStateInformation`/`setStateInformation` are additionally **assumed to arrive on the message
thread**: their Anamorph-owned tail (A/B slots, preset metadata, `InternalState`, undo
signatures) is non-atomic message-thread state with no lock or marshalling. On VST3 the pinned
SDK annotates both calls `[UI-thread]` (JUCE debug-asserts it for `setState`), so the assumption
is the format contract; on the **macOS AU** no spec forbids off-main-thread
`SaveState`/`RestoreState`, and that unguarded exposure is tracked as **RISK-007**
(`docs/FUTURE_RISKS.md`) — any lock/hop guard is itself an Architecture-Review-Gate change.
`prepareToPlay` is in the same position: the tables cover the paths it creates, not the thread it
arrives on. Its engine body relies on the format contract that no `processBlock` runs
concurrently, which every wrapper guarantees; its latency report no longer depends on the thread
at all (round 15, ER-STATE-19, above). An off-message-thread prepare against an OPEN editor's
reads of engine state is RISK-007's exposure class and is recorded there, not closed here.

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
- The OpenGL context is attached only on macOS/Windows; all Linux/BSD rendering is on the
  message thread (`docs/architecture/design-decisions/ADR-0011`).

Evidence [Verified]:
- Source: src/dsp/ScopeBuffer.h:28-80; src/dsp/LevelMeters.h:125-198; src/dsp/Correlation.h:50-108;
  src/PluginProcessor.cpp:59-77, 249; src/InternalState.h:87-92, 194-203

## Enforcement

A change to the thread model, a new shared-state path, or a new atomic ordering triggers the
**Architecture Review Gate** and an **AI Agent Hard Stop**. Changing this policy requires an ADR.
