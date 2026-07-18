# Wave 3 — Runtime Performance Investigation (worklog)

> Working notes + handover record for the Wave-3 optimisation pass. This is a **worklog**,
> not architecture documentation: numbers here are session-local measurements on the
> environment below and are NOT release claims (constraint C2 — `PERFORMANCE_BUDGET.md`
> stays qualitative). Future agents: read this before re-profiling; do not restart the
> Wave 1/2 investigations (their conclusions were re-verified at this HEAD, see below).

- **Date:** 2026-07-18
- **Branch:** `performance/wave3-runtime-optimization` (from `main` @ `bc5f852`, post-PR #61)
- **Environment:** Linux x86_64 container, Intel Xeon @ 2.10 GHz (4 cores), gcc 13.3.0,
  `-O3 -DNDEBUG` (the repo's Release config), JUCE 8.0.14 (pinned).
- **Method:** scratchpad harnesses linked against the repo's prebuilt Release objects
  (never part of the build system): an isolated MultibandWidth/SoloMonitor bench, a
  full-`AnamorphEngine` scenario bench (48 kHz, 512-sample blocks, deterministic noise,
  median of repeated passes), a per-scenario float32 output **twin dump** for
  before/after bit-comparison, and callgrind instruction attribution (function- and
  line-level, `-g` rebuild of the DSP TUs only). GUI numbers were not re-measured this
  wave (no display in this environment); the GUI findings below are code-inspection
  results building on the recorded Wave 1/2 GUI measurements.

## Prior state (Wave 1 + Wave 2, re-verified at this HEAD)

`docs/architecture/PERFORMANCE_BUDGET.md` + CHANGELOG 0.8.8–0.8.10 record the completed
optimisations (settled fast paths H1/H4/H12/S4/S5/S7, static-layer caches H2/H13/H17/N2,
FrameClock pacing, H3 rational tanh, H6 flat LR4 clone, H5 velvet gather, H11 quadrature
LFO, H8 branchless meters, S9 scope publish). An independent re-verification pass at this
HEAD confirmed all of them are intact (verdicts STILL-TRUE), with two record-only drifts:

- The budget's `processBlock` no-allocation evidence cites stale line ranges
  (`process()` now spans AnamorphEngine.cpp:593–1168); substance unchanged.
- The recorded `ax[0]`/`dax[0]` micro-item understated the waste: `dax[0]` also glides
  per sample while align is on, and is reassigned every aligned block by the resync.
- `Vectorscope.h`'s header comment still describes the pre-0.8.10 60-fps-timer design
  (comment drift only).

The recorded open candidate (the multiband phase-compensation allpass running a full
LR4 ladder for a `lo+hi` sum) was confirmed still true and is implemented this wave.

## Baseline measurements (this environment, ns/sample)

### Full engine (48 kHz, 512-block, noise; median of 9 passes)

| scenario              | baseline |
|-----------------------|---------:|
| default-transparent   |    71.4  |
| mb4-settled           |   198.7  |
| mb4-mix50             |   302.8  |
| mb3-settled           |   151.3  |
| mb4-glide             |   466.7  |
| mb4-glide-mix50       |   622.0  |
| mb4-solo1             |   275.5  |
| velvet-1.0            |    75.8  |
| chorus-os4            |   248.2  |
| everything-on-os4     |   486.1  |

(`mb4-glide` = continuous ±0.5-oct sinusoidal retargeting of all three splits per block —
the drag-emulation case; widths non-unit in all mb rows; mix=1 except the mix50 rows.)

### Isolated MultibandWidth / SoloMonitor

| scenario            | baseline |
|---------------------|---------:|
| mb2-settled-wet     |    29.4  |
| mb3-settled-wet     |    67.8  |
| mb4-settled-wet     |   109.0  |
| mb3-settled-align   |   116.7  |
| mb4-settled-align   |   188.1  |
| mb4-glide-wet       |   212.7  |
| mb4-glide-align     |   392.0  |
| mb4-fade-wet        |   112.9  |
| solo-active (mask 1)|    66.2  |
| solo-settled (cold) |     0.2  |

### Callgrind attribution (Ir share of the collected scenario pass)

- **default-transparent floor:** LevelMeters.h 22.3 % (the `sel()` mask combine alone
  7.5 %), LoudnessMatch 23.4 % (19.0 % of it the four double K-weighting biquad chains),
  engine inline 13.6 % (mix blend, rings, smoothers, correlation 2.9 %, scope 1.5 %),
  VelvetNoise parked 11.2 % (amount/density/env/gate glides + history writes),
  bench noise-fill 11.4 % (harness artifact, excluded from conclusions).
- **mb4-settled:** runWet lambda 42.1 %, engine 23.5 %, LoudnessMatch 11.9 %.
- **mb4-mix50:** runWet 29.6 % + runDry 27.5 % — the dry-align bank costs as much as the wet.
- **mb4-glide:** SoloMonitor::process 22.1 %, runWet 19.4 %, libm `__tan_fma` ~21.8 %
  (incl. fenv), MultibandWidth glide/update 13.0 %.

### Key derived facts

- One `LR4Xover::setCutoffFrequency` (double `tan` + divide) ≈ 17 ns here. A 3-split
  glide currently performs **6** of them per sample wet-only (x + ax), **12** with the
  dry bank aligned — which is why the glide rows double/triple their settled cost.
- The full-ladder allpass (`lo+hi`) kernel measures 14.4 ns per stereo sample pair; the
  first-2nd-order-section equivalent 10.0 ns (1.44×). The telescoping identity
  `lo + hi = yL2 + (((yL − R2·yB) + yH) − yL2) ≈ (yL − R2·yB) + yH` was verified
  numerically: max |Δ| 1.19e-7 on ±0.7 noise for both fixed and per-sample-gliding
  cutoffs (the second TPT section cancels except for one float rounding pair).
- With **nothing soloed**, a split drag wakes SoloMonitor's H1 fast path (its settled
  gate requires cutoffs within 0.05 Hz of targets), so the whole bank — 6 LR4
  `processSample` + 5 smoother ticks + up to 3 `tan` updates per sample — runs for the
  entire drag + catch-up, producing a provable `1·in + 0·bands` passthrough. The gains
  alone prove the output; cold re-entry snaps cutoffs anyway, so the cutoff term of the
  gate only forces wasted work.

## Hypotheses → decisions

| id | hypothesis | class | decision |
|----|-----------|-------|----------|
| W3-1 | SoloMonitor: decouple the H1 cold gate from cutoff proximity (stay cold through no-solo target moves; track targets cheaply while cold) | A | **P0 — implement** |
| W3-2 | Per-split LR4 coefficient sharing: x/ax/dx/dax of one split always share the same cutoff → compute `tan` once per split, adopt into the twins; skip the never-processed `ax[0]`/`dax[0]`; copy (not recompute) the per-block dx/dax resync | A | **P0 — implement** |
| W3-3 | Phase-compensation allpass = first LR4 TPT section only (the recorded 0.8.10 candidate): dedicated 2nd-order allpass path instead of the full dual-output ladder | B (≈1-ulp/sample injection) | **P0 — implement** |
| W3-4 | Output-stage settled fast path: hoist the settled gain product per block; skip the loop at exact unity | A | **P1 — implement** |
| W3-5 | Settled-Mix hoist: `m`, smoothstep `ts` are block constants when `mixSmooth` is settled | A | **P1 — implement** |
| W3-6 | SpectrumImager FFT `ignoreNegativeFreqs=true` (JUCE computes 4097 instead of 8192 magnitudes; upper half is never read) | A | **P1 — implement** |
| W3-7 | LoudnessMatch gated when Match off | C | **REJECTED** — `getMatchGainDb()` is a live contract with Match off: the editor's Measure readout updates every timer tick and the Apply button writes the measured value into Output Gain. Freezing the measurement breaks both (DSP_POLICY inv. 10 "Measure+Predict"). An editor-closed gate would also change readout freshness on reopen and add GUI→audio coupling — maintainer decision, not a Wave-3 unilateral change. |
| W3-8 | LevelMeters gated when editor closed | C | **REJECTED** — held peak numbers are specified to persist until clicked/replay (#15); gating while closed would lose peaks/clips that occurred with the editor closed. The remaining per-sample cost (16 branchless `sel` picks across the two meters) is the H8 tradeoff working as recorded. |
| W3-9 | VelvetNoise parked env/gate freeze | C | **REJECTED** (same conclusion as Wave 1/2): env/gate must keep tracking the input while parked so a re-engage opens with the correct presence state; freezing changes engage envelopes audibly. |
| W3-10 | Width==1 exact gate on the global Width stage | B | **DEFERRED** — ~7 flops/sample on the floor, but it injects deltas into every downstream stage for a micro win; not worth a Class-B review this wave. |
| W3-11 | Haas amount-0 fast path (S5/H12 family) | A | **DEFERRED** — real but only affects the non-default Haas-selected-at-zero auditioning state; keep the PR focused. Recorded for a future wave. |
| W3-12 | NaN-scan two-pass vectorisation; segmented ring copies (scope/bypass) | A | **DEFERRED** — low single-digit % each; mechanical but churn-heavy. |
| W3-13 | GUI: LevelMeter static-layer cache (H2/H13 recipe; the one visualizer without it) | A | **DEFERRED** — needs the Wave-1 GUI harness to size + validate; meters are hidden by default. Top GUI candidate for a Wave 4. |
| W3-14 | GUI: SpectrumImager dB-conversion cache across decay ticks; paint Path reuse; 24 Hz timer memoisation (preset-name shaping, combo hover poll, match-readout format); Vectorscope hidden-editor `isShowing` gate | A | **DEFERRED** — all message-thread; individually small. Recorded with file:line in the Wave-3 findings for a future GUI wave. |

## Results (after implementation)

**Measurement caveat discovered mid-wave:** the container's effective CPU speed drifted
between the first baseline run and the post-change runs (untouched code paths "sped up"
~40 % on their own). All before/after deltas below therefore come from **interleaved
reruns of a frozen pre-change binary vs the post-change binary on the same machine
state**, minutes apart — the first-run tables above remain valid for *attribution ratios*
but not for absolute before/after deltas. Future agents: always keep a frozen baseline
binary and interleave.

### After: full engine (ns/sample, median of 9, fair interleaved A/B)

| scenario              | baseline | after | Δ |
|-----------------------|---------:|------:|------:|
| default-transparent   |    40.4  |  37.8 |  −6.6 % |
| mb4-settled           |    98.7  |  85.1 | −13.8 % |
| mb4-mix50             |   146.3  | 122.2 | −16.5 % |
| mb3-settled           |    75.7  |  69.2 |  −8.6 % |
| mb4-glide             |   249.0  | 130.1 | −47.7 % |
| mb4-glide-mix50       |   336.2  | 166.7 | −50.4 % |
| mb2-mix50             |    64.7  |  62.0 |  −4.2 % |
| mb2-glide-mix50       |   147.3  |  95.4 | −35.2 % |
| mb4-solo1             |   138.3  | 124.3 | −10.1 % |
| velvet-1.0            |    45.6  |  43.4 |  −4.7 % |
| chorus-os4            |   151.7  | 150.1 |  −1.0 % (noise; untouched path) |
| everything-on-os4     |   299.4  | 278.9 |  −6.8 % |

### After: isolated MultibandWidth / SoloMonitor (fair interleaved A/B)

| scenario            | baseline | after | Δ |
|---------------------|---------:|------:|------:|
| mb2-settled-wet     |    22.6  |  22.0 |  −2 % (noise; untouched path) |
| mb3-settled-wet     |    48.3  |  33.6 | −30.4 % |
| mb4-settled-wet     |    61.9  |  54.3 | −12.3 % |
| mb3-settled-align   |    67.2  |  53.0 | −21.1 % |
| mb4-settled-align   |   104.7  |  84.7 | −19.1 % |
| mb4-glide-wet       |   127.4  |  96.4 | −24.3 % |
| mb4-glide-align     |   220.3  | 136.6 | −38.0 % |
| mb4-fade-wet        |    60.5  |  51.2 | −15.4 % |
| solo-active         |    39.2  |  40.7 |  +3.8 % (noise; hot path untouched) |
| solo-settled (cold) |     0.12 |  0.12 | — |

### Output-equivalence validation

- **Twin dump** (12 engine scenarios × 200 blocks × stereo float32, deterministic noise):
  every Class-A row **bit-exact for all 204,800 samples** — including the two 2-band
  discriminator rows (`mb2-mix50`, `mb2-glide-mix50`, which exercise the coefficient
  sharing, the settled-Mix hoist, the output-stage fast path and the align-resync copy
  with NO compensation allpass in the path, so any W3-1/2/4/5 flaw would show there) and
  `default-transparent`, `mb4-solo1`, `velvet-1.0`, `chorus-os4`. The AP2 (W3-3, Class B)
  delta appears only where the compensation allpass runs: `mb4-settled` 2/204800 samples,
  max |Δ| 5.96e-8; `mb4-mix50` 4 samples; the glide rows 7–9; `everything-on-os4`
  24 samples, max |Δ| 1.19e-7 (~1 ulp at signal scale). `mb3-settled` happened to be
  bit-exact over the whole dump (the single-allpass rounding pair cancels there).
- **DSP self-test suite:** 32 tests + A/B guard, **136 checks, 0 failures** (Test 33
  added: solo monitor stays cold through a no-solo split drag, buffer bit-untouched,
  re-engage snaps to the freshest targets; proven to fail on pre-Wave-3 source — the
  stayedCold and freshest-snap checks fail exactly as predicted, everything else passes).
- **Full plugin build (VST3, Release):** green, no new warnings (8 pre-existing warning
  lines before and after, byte-compared).
- **pluginval:** cannot run locally — the release download is egress-blocked in this
  sandbox (the proxy returns a JSON error payload instead of the zip; the same constraint
  recorded in HANDOVER for previous sessions). It runs blocking at strictness 10 on the CI
  Linux gate.

## Handover notes for the next wave

- The scratchpad harnesses (bench_mb.cpp, bench_engine.cpp with dump/compare modes) are
  session-local; recreate them from this worklog's description. Compile the bench TU with
  the AnamorphTests flags from build.ninja and link the prebuilt
  `CMakeFiles/AnamorphTests.dir` objects (minus dsp_tests.cpp.o) — no CMake changes.
- Top remaining candidates, in recorded priority order: GUI LevelMeter static-layer cache
  (W3-13), the message-thread micro-set W3-14, Haas amount-0 idle path (W3-11), NaN-scan /
  ring-copy vectorisations (W3-12), Width==1 gate (W3-10, Class B). The transparent floor
  is now dominated by LevelMeters (~22 % — inherent, W3-8) and LoudnessMatch (~19–23 % —
  contractual, W3-7): neither is gateable without a behaviour decision by the maintainer.
