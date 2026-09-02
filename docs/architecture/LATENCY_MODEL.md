# LATENCY_MODEL.md

Plugin delay compensation (PDC) model.

## Latency sources

| Source | Latency | Condition |
|---|---|---|
| Oversampling (2×/4×/8×) | `latency2/4/8` = integer samples from `Oversampling::getLatencyInSamples`, rounded | **Only** when the OS wrap is engaged |
| Everything else (Haas, Velvet, Chorus, Width, crossovers, Mono Maker, Level Match, Solo) | **0** | Always (all linear/IIR, no lookahead) |

The oversamplers are minimum-phase polyphase IIR half-band filters, constructed with the
"integer latency" flag so PDC is exact. Evidence [Verified]: src/dsp/AnamorphEngine.cpp:42-56.

## When oversampling is engaged

The OS wrap engages **only** when it has nonlinear/modulation work to do:

```cpp
osActiveFor(e) = e.oversample != Off && (e.driveDb > 0.01f || isModAlgorithm(e.algorithm));
isModAlgorithm(a) = (a == Chorus || a == DimensionD);
```

So with Oversampling selected but Drive at 0 and a *linear* algorithm (Haas/Velvet), the
oversampler is bypassed and reported latency is **0**.

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:14-23; test `testBypassNullAndLatency`
(latency==0 with OS off; latency>0 when OS active; bypass delay matches reported latency).

## Reported latency (current values)

`getLatencySamples()` returns `latency2/4/8` for the selected factor when `osEngaged`, else 0.
The concrete sample counts depend on JUCE's half-band filter orders (1/2/3 for 2×/4×/8×) and
the sample rate; they are computed at `prepare()` time, not hard-coded.
Evidence [Verified]: src/dsp/AnamorphEngine.cpp:54-56, :308-318.

`TODO: tabulate the measured latency2/4/8 sample counts at 44.1/48/96/192 kHz from a built
binary (requires running the plugin; not statically provable here).`

## Host compensation behaviour

- The wrapper reports latency via `setLatencySamples(predictLatency(...))`.
- `predictLatency` is `const` and race-free, so COMPUTING the number never touches audio-thread
  state. **DELIVERING it is the part with a thread requirement** (D-1, approved 2026-09-01):
  `setLatencySamples`' notification chain takes at least three `CriticalSection`s and, when the
  reported value actually changes, appends to a heap container and `write()`s a pipe in the Linux
  wrapper. Under VST3 host automation of Drive / Algorithm the caller is the **audio thread**
  (KI-027), so every re-report is routed through `requestLatencyUpdate()`:
  - on the **message thread** it stays fully synchronous — UI edits, preset loads, undo/redo and
    a message-thread `prepareToPlay` (every in-spec VST3 activation, the standalone, pluginval)
    are unchanged and instantaneous;
  - anywhere else the caller does one atomic store and returns, and a **processor-owned** 20 Hz
    timer performs the delivery. The timer belongs to the processor, not the editor, so it runs
    with no editor open.
  - **The request flag is cleared exactly once per delivery, and before the state it publishes is
    read** (2026-09-01, ER-STATE-14). `timerCallback`'s `exchange(0)` IS that clear, so it calls
    `deliverLatency()` — the delivery half, which does not touch the flag — rather than
    `updateLatency()`, which clears again; anything stored between those two clears was dropped, and
    a dropped request is a permanently stale reported latency, since the audio thread's store is
    unacknowledged and nothing re-raises it until the next unrelated move or a re-prepare. Requests
    landing DURING a delivery survive and are served by the next tick, which is what clearing first
    buys. The store is **release** and the consumers **acquire**, so a consumed request also
    publishes the parameter write that raised it — under `relaxed` on both sides there is no such
    edge, which x86-64's store ordering hides and the AArch64 targets do not. Both are
    correct-by-construction: the window is nanoseconds wide and no external mechanism can place a
    request inside it, so State test 27 does not discriminate them and says so. What that test DOES
    pin, deterministically since round 12, is the adjacent invariant — a request that lands while a
    delivery is running survives to the next tick — using `setLatencySamples`' synchronous listener
    notification as a barrier: a build that clears after delivering fails it.
  - **`prepareToPlay` goes through the same request path** (2026-09-02, round 15, ER-STATE-19).
    It used to call `updateLatency()` directly, on whatever thread the host activated on. That is
    the message thread for every in-spec VST3 host, but not always: JUCE's Linux VST3 wrapper
    services the plug-in's messages — this timer included — from its own background thread until
    the host registers an `IRunLoop`, which a conformant host may do only through `IPlugFrame`
    when an editor first opens — so the whole pre-editor phase in such a host, and the plug-in's
    whole life in one that never provides a run loop; FL Studio's Patcher is known to JUCE to
    ignore `setActive`'s `[UI-thread]` annotation; nothing pins an AU `Initialize` to main, and
    pluginval — the repository's own macOS release gate — calls an AU's `prepareToPlay` on its
    test thread, hopping to the message thread for VST3 only. From such a thread the direct call wrote
    `AudioProcessor::latencySamples` and walked the listener chain while `timerCallback` could be
    doing the same, and let a tick serving an earlier request read `latency2/4/8` while
    `engine.prepare()` rewrote them — two data races ThreadSanitizer reports on the pre-round-15
    code (`AnamorphStateTests --reprepare-race-probe`), with a reachable ending in which the
    timer's older number lands last and nothing is pending to correct it (observable only on an
    instance's FIRST prepare, since the oversamplers' latencies do not depend on the sample rate;
    the undefined behaviour is on every off-thread prepare that overlaps a tick). Now a
    message-thread prepare is unchanged; an off-thread one raises the request and the timer
    reports within one tick, so the message thread is the ONLY writer; and `latency2/4/8` are
    relaxed atomics whose ordering rides on the flag's release/acquire pair. A host that activates
    off-thread therefore learns the activation latency up to 50 ms late, through the same
    `restartComponent(kLatencyChanged)` path automation already uses. Regression coverage: State
    test 30 — a worker-thread prepare must deliver nothing from that thread, the report must still
    be unchanged after the join, and the tick must then serve the PREPARED value, equal to a
    message-thread prepare's; fails 3 checks without the fix, 0 with it.
- **The host may therefore learn of a latency change up to one timer interval (50 ms) after the
  parameter moved.** This is the deliberate cost of keeping locks, allocation and a syscall off the
  audio thread; the VALUE reported is always the one the live state predicts, only its delivery is
  deferred. Regression coverage: State test 22. (RISK-008 records the one wrapper configuration —
  a Linux host handing its run loop over only through `IPlugFrame` — in which the message queue
  itself is unserviced while the editor is closed, so the interval there is "until the editor
  next opens".)
- **A restore re-derives the report from the final state.** `setStateInformation` ends with a
  latency request because two things inside it can move a latency-bearing parameter without the
  listener hearing the final value: `apvts.replaceState` adopts a malformed `@value` by CLAMPING it
  to a range endpoint (and re-reports for it), and `reassertParameters` then repairs it with
  `setValue()` plus a direct atomic store, notifying nobody by design. Regression coverage: State
  test 24.
- The OS engagement is **latched** (changes only at `reset` or the silent duck bottom), so
  latency never changes mid-block; an OS-path change is routed through the duck.

Evidence [Verified]: src/PluginProcessor.cpp:149-173 (`deliverLatency` + `updateLatency`), :110-115 (`parameterChanged`); src/dsp/AnamorphEngine.cpp:218-223,
:293-329, :494-509.

## INVARIANT (binding)

> **Reported-latency changes require an ADR.** Any change to latency sources, the engagement
> condition, or the reported value must be recorded as an ADR and pass Architecture Review
> (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`, `docs/policies/ADR_POLICY.md`). Latency
> reporting must remain exact (integer) so host PDC stays sample-accurate.
