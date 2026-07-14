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
| **GUI redraw is bounded, idle-gated (0.8.8) AND display-rate-adaptive (0.8.10)**. The editor timer stays 24 Hz and the editor's meter/micro-anim VBlank stays per-frame; the four visualizers (Vectorscope, LevelMeter, StereoMeter, SpectrumImager), formerly fixed 60 Hz `juce::Timer`s, now refresh from `gui::FrameClock` — a `VBlankAttachment` paced to the display and capped near 120 Hz (executes every `ceil(rate/126)`-th vblank: 60→60, 120→120, 144→72, 240→120; 60 Hz wall-clock fallback when the cadence is unmeasurable). Every temporal ease/decay was re-expressed in `dt` form (matching the old 60 Hz curves to within the display quantum), so smoothness scales to the panel while the S1/S2/S3/H15 idle gates and once-per-block audio ballistics are unchanged. The *work* inside each tick still runs only while something visible can change: the Vectorscope, Spectrum analyser and meters repaint only on real content change, the analyser's FFT runs only on a changed/non-silent window (and not at all while hidden), the micro-animation poll skips when provably static — and since Wave 2 (H15) it decides "provably static" from three relaxed change-generation counters (sound params, view params, InternalState) instead of hashing every tracked widget's value at 60 Hz, which was 68-87 % of the remaining idle editor instructions in the Round-2 attribution — and the 24 Hz signature strings rebuild only on a parameter change. Idle GUI cost drops to ~0. Active repaint cost is also bounded (0.8.9 / H2 + H13 + N2): the Vectorscope's static layer (background gradient, rounded panel, glass edges, grid, labels) and each StereoMeter's static layer (glass panel, centre tick) — pure functions of size/scale/look — are rendered once into cached physical-resolution images and blitted per frame; only signal-dependent drawing (point cloud, clip ring, meter pointer, meter end labels for z-order) is rasterized live. These components are opaque (N2): the cached layers are RGB with the editor's flat `colours::bg` backdrop baked into the rounded corners, so the blit is an opaque copy (not an alpha composite) and the parent never re-renders beneath them. The Spectrum analyser caches its bottom layer the same way (H17: panel + band tints + grid, keyed on its exactly-converging eased inputs) but stays translucent — it sits on the editor's semi-transparent Multiband panel, so the N2 opacity pattern does not apply there. The `FrameClock` refresh (0.8.10) stops the Advanced-only Imager while hidden and debounces its rate cap against single-vblank jitter, so idle stays ~0 on any display. | src/gui/Vectorscope.cpp, SpectrumImager.cpp, LevelMeter.cpp, CorrelationMeter.cpp, FrameClock.h; src/PluginEditor.cpp `stepMicroAnims`; CHANGELOG [0.8.8], [0.8.9], [0.8.10] |
| **Scope transfer is O(1) amortised** (lock-free SPSC ring, fixed 16384 capacity, no alloc); the write index is published **once per block** (`pushBlock`, 0.8.8), so readers observe whole committed blocks. | src/dsp/ScopeBuffer.h:28-80 |

## Known per-sample costs (Verified, qualitative)

- **Per-sample IIR coefficient recompute while gliding.** `MonoMaker`, `MultibandWidth`, and
  `SoloMonitor` call `LR4Xover::setCutoffFrequency` per sample during a crossover
  glide (recomputes coefficients in place, no allocation). This is the dominant variable cost
  when crossovers are moving. Evidence [Verified]: src/dsp/MultibandWidth.cpp:110-120;
  MonoMaker.cpp:32-36; SoloMonitor.cpp.
- **The Drive waveshaper's tanh is a minimax rational kernel (Wave 2 / H3).** The two per-sample
  libm `tanh` calls (~55 % of every oversampling delta in the Round-2 attribution; their range
  reduction owned 35.8 % of engine branch mispredicts) are an odd degree-9/8 rational with
  clamped input/result — call-free, predictable, measured 15.2 → 3.9 ns/sample (3.9×) on the
  kernel bench. Class B: max relative error 3.5e-7 (~3 ulp) vs double `std::tanh` on a 4M-point
  sweep; exact 0 at 0; saturates to exactly ±1; the same-kernel makeup keeps full-scale peak
  mapping exact by construction. The Mix=0 bit-exact null (DSP_POLICY inv. 7) re-verified on the
  twin dump. Evidence [Verified]: src/dsp/AnamorphEngine.cpp (`driveTanh` + invariant comment);
  CHANGELOG [0.8.9].
- **The multiband dry-align bank is gated in the settled-full-wet state (Wave 2 / H4).** With the
  Mix glide parked at exactly 1, Match off (and not mid-engage), and no enable/bypass crossfade in
  flight, the A(dry) reconstruction (6 LR4 calls/sample — half the multiband cost, ~20 µs on the
  shipped default in the Round-2 attribution) and the m=1 blend pass are skipped. Class B: the
  gated output is the exact wet instead of its m=1 float re-blend (measured ≤2.4e-10); the live
  Measure readout follows the delay-aligned clean dry while gated, so a Match engage right after a
  gated stretch starts from a reference without the multiband reconstruction ripple (measured
  0.53 dB worst-case level offset on a near-crossover synthetic, converging over the loudness
  window; always duck+glide smoothed). Both dry delay rings stay warm; re-engage is comb-free
  (`testDryAlignGateRecomb`). Evidence [Verified]: src/dsp/AnamorphEngine.cpp (gate + invariant
  comment); CHANGELOG [0.8.9].
- **The LR4 crossovers are a local flat-state clone (Wave 2 / H6).** All ten
  `juce::dsp::LinkwitzRileyFilter<float>` instances (MultibandWidth wet + dry-align banks,
  SoloMonitor, MonoMaker) are replaced by `LR4Xover` — the same coefficient derivation and TPT
  ladder expression-for-expression, with four flat per-channel floats instead of four heap
  `std::vector`s (whose per-sample indexing was 4.5-7 % of every multiband/solo row in the
  Round-2 attribution). Bit-identical: proven byte-exact on the 33-scenario full-engine dump,
  including 4-band solo engage/clear cycles (cold re-entry) and per-sample split/mono-freq
  glides. Evidence [Verified]: src/dsp/LR4Xover.h (invariant comment); CHANGELOG [0.8.9].
- **Multiband allpass phase compensation (0.8.10) — the top remaining DSP optimisation candidate.**
  The flat-recombination fix runs the multiband low-sum through `bands−2` extra `LR4Xover`
  allpass sections per bank (wet, and dry when Mix is partial), correctly removing the
  close-crossover magnitude dip but adding roughly **half again** to the multiband stage's
  per-block cost (isolated-harness measurement, session-local) — and multiband is the #2 DSP
  hotspot in the shipped 4-band default, so this is now the most prominent single cost there.
  Two non-Category-C follow-ups (deferred — no DSP-behaviour change): (a) replace the full
  `LR4Xover` dual-output (a 4th-order TPT ladder computing both LP and HP) used purely as an
  allpass with a dedicated **2nd-order allpass** biquad — `bands−2` sections × 2 banks, expected
  to recover about half the added cost; (b) skip the currently-set-but-never-processed `ax[0]` /
  `dax[0]` glide coefficient updates (index 0 is never read; a glide-only `tan`/`sqrt` waste).
  Both keep the recombination bit-for-audible-purposes identical and are validated by the
  existing `testMultibandFlatRecombination` + `testMultibandMonoCompat` + the full-engine twin
  dump. Evidence [Verified]: src/dsp/MultibandWidth.cpp (allpass banks, `.cpp:150-166,184-199`;
  the `ax[0]/dax[0]` set at `:126-127`); CHANGELOG [0.8.10].
- **SoloMonitor runs only while it can be heard (0.8.9 / H1).** With nothing soloed and every
  crossfade gain fully settled, the monitor's per-sample work (6 LR4 `processSample` + 5 smoother
  ticks) is skipped entirely — previously ~half of the transparent engine floor (callgrind 0.8.8:
  ~49 % of instructions in the default state). The bank goes cold and is reset + snapped on
  re-engage under the ~12 ms crossfade (the engine's `mbRunning` warm/cold pattern). Evidence
  [Verified]: src/dsp/SoloMonitor.cpp (settled fast path); CHANGELOG [0.8.9].
- **Level-meter envelopes are branchless (0.8.9 / H8).** The per-sample envelope coefficient
  picks in `StereoLevel::process` use a bit-select instead of data-dependent branches (which
  owned ~87 % of all engine branch mispredicts on real audio). Bit-identical values for every
  input incl. NaN/Inf; slight fixed ALU cost on perfectly-predictable (all-silence) input in
  exchange for the active-signal win. Evidence [Verified]: src/dsp/LevelMeters.h (`sel`);
  CHANGELOG [0.8.9].
- **Chorus/Dimension-D LFO is a quadrature recurrence (Wave 2 / H11).** The two per-sample libm
  sines (≈9 % of the active chorus rows; ~15-20 µs inside everything-on-os4 in the Round-2
  attribution) are replaced by one double-precision `(sin, cos)` rotation, re-seeded from the
  iterated `phase` each block. Class B numerics: sub-0.1-sample delay wobble at the depth
  extremes (measured ≤8.2e-4 output delta on the 25-scenario dump, chorus-active blocks only);
  the float `phase` state and the amount-0 idle path (H12) are bit-identical, so nothing drifts
  across blocks or re-engages. Evidence [Verified]: src/dsp/ChorusEngine.cpp (recurrence +
  invariant comment); CHANGELOG [0.8.9].
- **VelvetNoise** has an O(maxTaps=64) sparse-FIR inner loop per sample, plus a full-buffer
  `std::fill` on the transport-stop completion (no alloc). As of 0.8.8 the surrounding per-sample
  work is gated without changing output: the 64-tap weight rebuild + `sqrt` normalisation runs only
  while the Density glide is actually moving (skipped on an exact bit-compare once settled), and the
  tap accumulation is skipped when its contribution is provably exactly zero (Amount 0 — the default
  — or the presence gate fully closed, outside any stop fade). Bit-identical; the history writes and
  all envelopes/glides still run every sample. As of Wave 2 (ALG-4) the fixed ±1 tap sign is folded
  into the stored weight at rebuild time, so the gather does one multiply per tap instead of two and
  reads one array less — bit-identical (`w·(±1)` is an exact sign flip; evaluation order unchanged;
  Round-2 estimate −2-3 µs on the velvet-1.0 row). With the density glide settled and no stop fade
  in flight, the gather itself runs tap-outer over a linear history image (Wave 2 / H5): one
  contiguous unit-stride streaming run per tap instead of 64 random-index ring reads per sample
  (45.6 % of the row's D1 read misses in the Round-2 attribution; estimate −25-30 % on the
  velvet-1.0 row), accumulating in the original ascending-tap order — bit-identical; the glide,
  stop-fade and parked paths keep the original loop. Evidence [Verified]: src/dsp/VelvetNoise.cpp
  (`updateWeights` gate + sign fold; the H5 fast path + eligibility comment; the tap-loop
  zero-skip); CHANGELOG [0.8.8], [0.8.9].

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
