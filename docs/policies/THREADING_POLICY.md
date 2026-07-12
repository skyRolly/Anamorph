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

## Atomic usage rules

- Published meter/correlation/match values: `memory_order_relaxed` (monotonic display data).
- `soundParamGen`: `memory_order_relaxed` — a generation / staleness counter only. It gates a
  message-thread cache rebuild and transfers no payload, so it is deliberately **not** an
  ordering/publication primitive (unlike the scope index below).
- Scope ring index: `release` on write, `acquire` on read (the one ordering-critical pair). The
  index is published **once per block** (`pushBlock`), so a reader that acquires it sees a whole
  committed block — never a partially written one.
- The OpenGL context is attached only on macOS/Windows; all Linux/BSD rendering is on the
  message thread (`docs/architecture/design-decisions/ADR-0011`).

Evidence [Verified]:
- Source: src/dsp/ScopeBuffer.h:28-80; src/dsp/LevelMeters.h:125-198; src/dsp/Correlation.h:50-95;
  src/PluginProcessor.cpp:52-62,128; src/InternalState.h:67-72,125-134

## Enforcement

A change to the thread model, a new shared-state path, or a new atomic ordering triggers the
**Architecture Review Gate** and an **AI Agent Hard Stop**. Changing this policy requires an ADR.
