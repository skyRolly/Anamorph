# REALTIME_SAFETY_AUDIT.md

Per-module audit against `docs/policies/REALTIME_AUDIO_POLICY.md`. Scope: does the audio path
(`process`/`processBlock`/`reset`) allocate, lock, or do IO? Allocation in `prepare()` is
permitted and noted separately.

Audit basis: full read of `src/dsp/**` and `src/PluginProcessor.cpp` (two independent passes).

## Audit table

| Module | Audio-path status | Allocation (prepare only) | Evidence |
|---|---|---|---|
| `AnamorphAudioProcessor::processBlock` | **Verified** — `ScopedNoDenormals`; param snapshot is atomic loads; no alloc/lock/IO | n/a (engine.prepare) | PluginProcessor.cpp:64-131 |
| `AnamorphEngine::process` | **Verified** — all scratch pre-sized; no alloc/lock/IO | prepare(): all buffers + oversamplers | AnamorphEngine.cpp:26-113 vs :472-899 |
| `MidSide` | **Verified** — pure arithmetic, `noexcept` | none | MidSide.h:21-42 |
| `HaasProcessor` | **Verified** — `process`/`reset` use pre-sized vectors (`std::fill`, no resize) | prepare(): `bufL/bufR.assign` | HaasProcessor.cpp:14-21,45-62 |
| `VelvetNoise` | **Verified** — no alloc/lock/IO; note O(64) per-sample loop + transport-stop `std::fill` (no alloc) | prepare(): `midHist.assign`, RNG construct | VelvetNoise.cpp:10-17,81-139 |
| `ChorusEngine` | **Verified** — `std::sin` per sample, pre-sized buffers | prepare(): `bufL/bufR.assign` (sized to 8× rate) | ChorusEngine.cpp:11-19,55-112 |
| `MonoMaker` | **Verified** — per-sample `setCutoffFrequency` (in-place coeff recompute, no alloc) | prepare(): scalar only (`LR4Xover` state is flat — no heap since Wave 2 / H6) | MonoMaker.cpp:7-18,25-47 |
| `MultibandWidth` | **Verified** — capped cutoff moves (0.8.10: per-sample coeff recompute while easing at ~1 oct/s; one ~12 ms dual-bank crossfade with 2× filter ticks on a discrete target step), no alloc/lock/IO | prepare(): scalar only (24× `LR4Xover.prepare`, flat state — no heap since Wave 2 / H6) | MultibandWidth.cpp (prepare/reset/glide + fade trigger/processBlock) |
| `SoloMonitor` | **Verified** — capped cutoff moves (0.8.10, as MultibandWidth) + `SmoothedValue`, no alloc | prepare(): 6× flat-state filter + smoother reset | SoloMonitor.cpp (prepare/reset/glide + fade trigger/process) |
| `LoudnessMatch` | **Verified** — fixed nested biquad structs; `pow/log10/tanh`; no alloc | prepare(): coeff compute only | LoudnessMatch.cpp:47-156 |
| `CorrelationMeter` | **Verified** — scalar one-poles only | none | Correlation.h:36-95 |
| `LevelMeters` | **Verified** — scalar envelopes; NaN self-heal per sample | none | LevelMeters.h:60-167 |
| `ScopeBuffer` | **Verified** — fixed `std::array`, lock-free SPSC | none | ScopeBuffer.h:28-57 |

## Cross-cutting findings (Verified)

- **No `new` / `malloc` / `std::vector::resize` / mutex / file IO on any audio path** across
  all 12 DSP modules + the processor. All heap allocation is confined to `prepare()` (via
  `std::vector::assign` or `juce::dsp::*::prepare`).
- **Non-finite guard:** an engine-wide per-sample NaN/Inf check replaces only non-finite
  samples with 0 and resets stateful nodes; it is not a level limiter and never alters valid
  audio. Evidence: AnamorphEngine.cpp:847-870.
- **`reset()` paths run `std::fill`/filter resets** but never allocate, and are invoked at safe
  points (prepare, host reset, the silent duck bottom, NaN self-heal).

## Items needing a non-static check

`TODO: a sanitizer/RT-audit run (e.g. running the built plugin under a real-time-violation
detector, or auditing JUCE's Oversampling::processSamplesUp/Down for internal allocation) would
upgrade the "no allocation inside JUCE's oversampler call" assumption from inferred to measured.
The plugin's own code is allocation-free on the audio path; JUCE internals are trusted by
construction (initProcessing is called in prepare).`

Source for the OS init: src/dsp/AnamorphEngine.cpp:49-51 (`initProcessing` at prepare).
