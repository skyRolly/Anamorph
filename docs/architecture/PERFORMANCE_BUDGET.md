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
| **GUI redraw is bounded AND idle-gated (0.8.8)**. The timer cadence is unchanged (24 Hz editor timer; 60 Hz component timers; per-frame VBlank), but the *work* inside each tick now runs only while something visible can change: the Vectorscope, Spectrum analyser and meters repaint only on real content change, the analyser's FFT runs only on a changed/non-silent window (and not at all while hidden), the micro-animation poll skips when provably static, and the 24 Hz signature strings rebuild only on a parameter change. Idle GUI cost drops to ~0. Active repaint cost is also bounded (0.8.9 / H2 + H13): the Vectorscope's static layer (background gradient, rounded panel, glass edges, grid, labels) and each StereoMeter's static layer (glass panel, centre tick) — pure functions of size/scale/look — are rendered once into cached physical-resolution images and blitted per frame; only signal-dependent drawing (point cloud, clip ring, meter pointer, meter end labels for z-order) is rasterized live. | src/gui/Vectorscope.cpp, SpectrumImager.cpp, LevelMeter.cpp, CorrelationMeter.cpp; src/PluginEditor.cpp `stepMicroAnims`; CHANGELOG [0.8.8], [Unreleased] |
| **Scope transfer is O(1) amortised** (lock-free SPSC ring, fixed 16384 capacity, no alloc); the write index is published **once per block** (`pushBlock`, 0.8.8), so readers observe whole committed blocks. | src/dsp/ScopeBuffer.h:28-80 |

## Known per-sample costs (Verified, qualitative)

- **Per-sample IIR coefficient recompute while gliding.** `MonoMaker`, `MultibandWidth`, and
  `SoloMonitor` call `LinkwitzRileyFilter::setCutoffFrequency` per sample during a crossover
  glide (recomputes coefficients in place, no allocation). This is the dominant variable cost
  when crossovers are moving. Evidence [Verified]: src/dsp/MultibandWidth.cpp:113-123;
  MonoMaker.cpp:33-37; SoloMonitor.cpp.
- **SoloMonitor runs only while it can be heard (0.8.9 / H1).** With nothing soloed and every
  crossfade gain fully settled, the monitor's per-sample work (6 LR4 `processSample` + 5 smoother
  ticks) is skipped entirely — previously ~half of the transparent engine floor (callgrind 0.8.8:
  ~49 % of instructions in the default state). The bank goes cold and is reset + snapped on
  re-engage under the ~12 ms crossfade (the engine's `mbRunning` warm/cold pattern). Evidence
  [Verified]: src/dsp/SoloMonitor.cpp (settled fast path); CHANGELOG [Unreleased].
- **VelvetNoise** has an O(maxTaps=64) sparse-FIR inner loop per sample, plus a full-buffer
  `std::fill` on the transport-stop completion (no alloc). As of 0.8.8 the surrounding per-sample
  work is gated without changing output: the 64-tap weight rebuild + `sqrt` normalisation runs only
  while the Density glide is actually moving (skipped on an exact bit-compare once settled), and the
  tap accumulation is skipped when its contribution is provably exactly zero (Amount 0 — the default
  — or the presence gate fully closed, outside any stop fade). Bit-identical; the history writes and
  all envelopes/glides still run every sample. Evidence [Verified]: src/dsp/VelvetNoise.cpp
  (`updateWeights` gate; the tap-loop zero-skip); CHANGELOG [0.8.8].

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
