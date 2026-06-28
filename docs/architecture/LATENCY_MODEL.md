# LATENCY_MODEL.md

Plugin delay compensation (PDC) model.

## Latency sources

| Source | Latency | Condition |
|---|---|---|
| Oversampling (2×/4×/8×) | `latency2/4/8` = integer samples from `Oversampling::getLatencyInSamples`, rounded | **Only** when the OS wrap is engaged |
| Everything else (Haas, Velvet, Chorus, Width, crossovers, Mono Maker, Level Match, Solo) | **0** | Always (all linear/IIR, no lookahead) |

The oversamplers are minimum-phase polyphase IIR half-band filters, constructed with the
"integer latency" flag so PDC is exact. Evidence [Verified]: src/dsp/AnamorphEngine.cpp:42-54.

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
Evidence [Verified]: src/dsp/AnamorphEngine.cpp:52-54, :306-316.

`TODO: tabulate the measured latency2/4/8 sample counts at 44.1/48/96/192 kHz from a built
binary (requires running the plugin; not statically provable here).`

## Host compensation behaviour

- The wrapper reports latency via `setLatencySamples(predictLatency(...))`.
- `predictLatency` is `const` and race-free, so the **message thread** updates PDC (on Drive /
  Algorithm / Oversampling change) without touching audio-thread state.
- The OS engagement is **latched** (changes only at `reset` or the silent duck bottom), so
  latency never changes mid-block; an OS-path change is routed through the duck.

Evidence [Verified]: src/PluginProcessor.cpp:52-62; src/dsp/AnamorphEngine.cpp:196-201,
:293-329, :494-509.

## INVARIANT (binding)

> **Reported-latency changes require an ADR.** Any change to latency sources, the engagement
> condition, or the reported value must be recorded as an ADR and pass Architecture Review
> (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`, `docs/policies/ADR_POLICY.md`). Latency
> reporting must remain exact (integer) so host PDC stays sample-accurate.
