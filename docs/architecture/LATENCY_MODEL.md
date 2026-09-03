# LATENCY_MODEL.md

Plugin delay compensation (PDC) model.

## Latency sources

| Source | Latency | Condition |
|---|---|---|
| Oversampling (2×/4×/8×) | `latency2/4/8` = integer samples from `Oversampling::getLatencyInSamples`, rounded | Whenever that **factor is selected** — see below |
| Everything else (Haas, Velvet, Chorus, Width, crossovers, Mono Maker, Level Match, Solo) | **0** | Always (all linear/IIR, no lookahead) |

The oversamplers are minimum-phase polyphase IIR half-band filters, constructed with the
"integer latency" flag so PDC is exact.
Evidence [Verified]: src/dsp/AnamorphEngine.cpp:57-71 (`latency2` — the three oversamplers built
with the integer-latency flag, and their latencies stored).

## The reported latency is a function of the SELECTED FACTOR (ADR-0034)

**Selecting the factor is the only thing that moves the number.** `predictLatency(e)` reads
`e.oversample`; `getLatencySamples()` reads `p.oversample`; both go through one helper so they
cannot drift apart. No parameter — Drive, Algorithm or any other — can change it, and since
Oversampling is an `InternalState` Setting (ADR-0010) rather than an APVTS parameter, host
automation cannot change it either.

**The wrap is still SKIPPED when it has no work to do**, on the unchanged predicate:

```cpp
osActiveFor(e) = e.oversample != Off && (e.driveDb > 0.01f || isModAlgorithm(e.algorithm));
isModAlgorithm(a) = (a == Chorus || a == DimensionD);
```

That predicate is the CPU saving — the resampling round trip is the largest single cost in the
engine — and it keeps its other three jobs: gating `currentOversampler()`, latching into
`osEngaged`, and forcing a duck through `discreteDiffers`. What it no longer decides is the
latency. In the one state where a factor is selected but the wrap is skipped (Drive at 0 with a
linear algorithm), a 2-channel integer ring, **`osCompDelayBuffer`**, supplies the wrap's group
delay, in the wrap's own place in the chain. So the chain genuinely carries the number it reports
in every combination, and the number does not move when Drive or Algorithm does.

**Why it works this way.** Reporting 0 in the skipped state made an ordinary knob move a host-graph
restart: hosts respond to a latency change by re-priming the graph, which the user hears as a
dropout. Measured on the pre-0.9.7 build (`AnamorphTests --os-latency-probe`, 48 kHz), a Drive move
of 0.005 dB → 6 dB with a linear algorithm reported 0 → 4 at 2× and 0 → 6 at 4× and 8×. Reporting
the factor's latency *without* the stand-in ring would have been worse still — the host would
compensate for a delay the chain did not have. See ADR-0034 for the options and the measurements.

Evidence [Verified]: src/dsp/AnamorphEngine.cpp; tests `testBypassNullAndLatency` (Test 3+4:
latency==0 with OS off; the engaged wrap's bypass delay matches the reported latency) and
`testOversamplingLatencyIsFactorOnly` (Test 52: the whole {factor}×{algorithm}×{drive} grid; a live
Drive sweep across the engagement threshold; reported == actual with the wrap skipped; and the
skipped state's output bit-identical to the OS-off output delayed, which is what proves the wrap is
genuinely not running).

## Reported latency (current values)

`getLatencySamples()` returns `latency2/4/8` for the selected factor, and 0 for Off. The concrete
sample counts depend on JUCE's half-band filter orders (1/2/3 for 2×/4×/8×) and the sample rate;
they are computed at `prepare()` time, not hard-coded. Measured at 48 kHz on the pinned JUCE 9.0.1:
**2× = 4, 4× = 6, 8× = 6** samples (Test 52 prints the row; Test 38's landing census records the
same three numbers and notes that 4× and 8× are equal, so an x4 → x8 switch moves no latency).

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:69-71 (`latency2` / `latency4` / `latency8`, the
only writes, made at `prepare()` time).

`TODO: tabulate the measured latency2/4/8 sample counts at 44.1/96/192 kHz from a built
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
  itself would be unserviced while the editor is closed, making the interval there "until the editor
  next opens". That is a mechanism the pinned wrapper permits, not observed behaviour: the one real
  Linux host tested, REAPER, updates the reported latency with the editor open AND closed.)
- **A restore re-derives the report from the final state.** `setStateInformation` ends with a
  latency request because two things inside it can move a latency-bearing parameter without the
  listener hearing the final value: `apvts.replaceState` adopts a malformed `@value` by CLAMPING it
  to a range endpoint (and re-reports for it), and `reassertParameters` then repairs it with
  `setValue()` plus a direct atomic store, notifying nobody by design. Regression coverage: State
  test 24.
- The OS engagement is **no longer latched** (ADR-0035): it is the live target of a 12 ms
  crossfade, so the wrap ⇄ stand-in-ring handover happens while both paths are audible and mixed,
  not at a silent duck bottom. That does not touch the reported number, which follows `oversample`
  — a discrete control still adopted only at a duck bottom or a reset — so the reported latency
  still cannot change mid-block. An oversampling FACTOR change is still routed through the duck,
  because that one does move the number — and that duck settles the blend on the state it adopts,
  since two paths of different latency are not alignable and so not mixable (Test 54).
- **A parameter move now re-derives the same number.** `parameterChanged` still requests a latency
  update on a Drive or Algorithm move, and `deliverLatency()` still recomputes — but the value is
  unchanged, and JUCE's `setLatencySamples` notifies the host only when the value actually differs,
  so nothing reaches the host and none of the notification chain's cost is paid. This retires the
  reachable half of KI-027; the D-1 mechanism below is kept in full, because a host writing the
  Oversampling Setting inside an off-thread `setStateInformation` (RISK-007) can still change the
  value from a non-message thread.

Evidence [Verified]: src/PluginProcessor.cpp:188-212 (`deliverLatency` + `updateLatency`), :110-115 (`parameterChanged`); src/dsp/AnamorphEngine.cpp:277-282,
:293-329, :494-509.

## INVARIANT (binding)

> **Reported-latency changes require an ADR.** Any change to latency sources, the engagement
> condition, or the reported value must be recorded as an ADR and pass Architecture Review
> (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`, `docs/policies/ADR_POLICY.md`). Latency
> reporting must remain exact (integer) so host PDC stays sample-accurate.
>
> **And the chain must carry what it reports.** ADR-0034 added the second half explicitly: a
> constant reported number is only correct while some element actually supplies that delay. When
> the oversampling wrap is skipped, `osCompDelayBuffer` is that element; removing it while leaving
> `predictLatency` keyed on the factor would make the plug-in claim a delay it does not have, and
> every other track in the session would sit early by that many samples. Test 52 leg B is the
> assertion.
>
> **Changing it must not cost the CPU saving.** `osActiveFor` decides whether the wrap RUNS and is
> not a latency question. Test 52 leg C pins this by requiring the skipped state's output to be the
> OS-off output delayed, bit for bit — which a build that simply ran the wrap all the time cannot
> satisfy.
