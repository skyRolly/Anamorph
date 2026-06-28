# PERFORMANCE_BUDGET.md

Performance and resource budget. **Numeric targets are intentionally left as explicit TODOs:
no benchmark/profiling data exists in the repository, and inventing numbers is prohibited
(constraint C2).** Only structurally-provable facts are stated as Verified.

## Structurally-provable facts (Verified)

| Claim | Evidence |
|---|---|
| **`processBlock` performs no heap allocations.** All scratch buffers and DSP state are allocated in `prepare()`; the audio path uses only pre-sized buffers and scalar state. | DSP audit of all 12 modules; src/dsp/AnamorphEngine.cpp:26-113 (prepare allocations) vs :472-899 (process — no `new`/resize) |
| **No locks / mutexes / file IO on the audio thread.** | `REALTIME_SAFETY_AUDIT.md`; no `mutex`/`lock`/IO in `src/dsp/**` audio paths |
| **`ScopedNoDenormals` is active for the whole block.** | src/PluginProcessor.cpp:66 |
| **Oversampling only runs when nonlinear work exists** (Drive>0 or Chorus/Dim-D); linear chains skip it → no needless CPU. | src/dsp/AnamorphEngine.cpp:19-23 |
| **GUI redraw is bounded** (24 Hz editor timer; 60 Hz component timers; per-frame VBlank). | src/PluginEditor.cpp:613; gui/*.cpp |
| **Scope transfer is O(1) amortised** (lock-free SPSC ring, fixed 16384 capacity, no alloc). | src/dsp/ScopeBuffer.h:55-57 |

## Known per-sample costs (Verified, qualitative)

- **Per-sample IIR coefficient recompute while gliding.** `MonoMaker`, `MultibandWidth`, and
  `SoloMonitor` call `LinkwitzRileyFilter::setCutoffFrequency` per sample during a crossover
  glide (recomputes coefficients in place, no allocation). This is the dominant variable cost
  when crossovers are moving. Evidence [Verified]: src/dsp/MultibandWidth.cpp:113-123;
  MonoMaker.cpp:33-37; SoloMonitor.cpp.
- **VelvetNoise** runs an O(maxTaps=64) inner loop per sample while active, plus a full-buffer
  `std::fill` on the transport-stop completion (no alloc). Evidence [Verified]:
  src/dsp/VelvetNoise.cpp:92,117,129-139.

## Target sample rates / buffer sizes

`TODO: confirm and record validated operating points. The code adapts to the host sample rate
and `maxBlockSize` at prepare() time (src/dsp/AnamorphEngine.cpp:26-29) with no hard-coded SR
assumption; chorus buffers are sized for 8× the base rate (:34). Common targets to validate:
44.1k / 48k / 96k / 192k sample rates; 32 / 64 / 128 / 256 buffer sizes. Requires profiling.`

## CPU budget

`TODO: Peak and Average CPU per instance are not measured in the repository. Requires
profiling a built binary across the sample-rate × buffer-size × algorithm × oversampling
matrix. Do not populate with estimated numbers.`

## Memory budget

`TODO: Per-instance memory is not measured. Structurally, allocation is bounded and occurs
only in prepare() (delay lines sized to max latency + max block; chorus buffers to 8× rate;
ScopeBuffer fixed at 16384 stereo frames). Requires measurement for a concrete figure.`

## Invariant

> `processBlock` must remain allocation-free, lock-free, and IO-free (see
> `docs/policies/REALTIME_AUDIO_POLICY.md`). Any change that could introduce an unbounded or
> per-block allocation requires Architecture Review.
