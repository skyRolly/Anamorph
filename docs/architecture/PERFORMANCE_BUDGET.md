# PERFORMANCE_BUDGET.md

Performance and resource budget. **Numeric targets are intentionally left as explicit TODOs:
no benchmark/profiling data exists in the repository, and inventing numbers is prohibited
(constraint C2).** Only structurally-provable facts are stated as Verified.

## Structurally-provable facts (Verified)

| Claim | Evidence |
|---|---|
| **`processBlock` performs no heap allocations.** All scratch buffers and DSP state are allocated in `prepare()`; the audio path uses only pre-sized buffers and scalar state. | DSP audit of all 12 modules; src/dsp/AnamorphEngine.cpp:36-131 (prepare allocations) vs the whole of `process()` (no `new`/resize/lock — line range re-verified Wave 3 after the function grew) |
| **No locks / mutexes / file IO on the audio thread.** | `REALTIME_SAFETY_AUDIT.md`; no `mutex`/`lock`/IO in `src/dsp/**` audio paths |
| **`ScopedNoDenormals` is active for the whole block.** | src/PluginProcessor.cpp:182 |
| **Oversampling only runs when nonlinear work exists** (Drive>0 or Chorus/Dim-D); linear chains skip it → no needless CPU. **Unchanged by ADR-0034**, which decoupled the reported LATENCY from this predicate without touching the run gate: where the wrap is skipped, a 2-channel integer ring supplies its group delay instead. Measured at 48 kHz / 128 on the `working` chain with Drive 0 and a linear algorithm — OS Off 153.27, 2× 152.31, 4× 159.52, 8× 155.91 ns/sample, i.e. inside each other's 6–11 % run-to-run spread and far below the engaged rows (211.12 / 265.46 / 380.00). The ring's own cost is below this instrument's floor. | src/dsp/AnamorphEngine.cpp (`osActiveFor`, `osCompDelayBuffer`); `AnamorphBench` §"Oversampling SELECTED but skipped, Drive 0" |
| **GUI redraw is bounded, idle-gated (0.8.8) AND display-rate-adaptive (0.8.10)**. The editor timer stays 24 Hz and the editor's meter/micro-anim VBlank stays per-frame; the four visualizers (Vectorscope, LevelMeter, StereoMeter, SpectrumImager), formerly fixed 60 Hz `juce::Timer`s, now refresh from `gui::FrameClock` — a `VBlankAttachment` paced to the display and capped near 120 Hz (executes every `ceil(rate/126)`-th vblank: 60→60, 120→120, 144→72, 240→120; 60 Hz wall-clock fallback when the cadence is unmeasurable). Every temporal ease/decay was re-expressed in `dt` form (matching the old 60 Hz curves to within the display quantum), so smoothness scales to the panel while the S1/S2/S3/H15 idle gates and once-per-block audio ballistics are unchanged. The *work* inside each tick still runs only while something visible can change: the Vectorscope, Spectrum analyser and meters repaint only on real content change, the analyser's FFT runs only on a changed/non-silent window (and not at all while hidden), and since Wave 3 computes only the non-negative-frequency magnitudes its consumers read (`ignoreNegativeFreqs`, ~half the per-transform magnitude work, identical visuals), the micro-animation poll skips when provably static — and since Wave 2 (H15) it decides "provably static" from three relaxed change-generation counters (sound params, view params, InternalState) instead of hashing every tracked widget's value at 60 Hz, which was 68-87 % of the remaining idle editor instructions in the Round-2 attribution — and the 24 Hz signature strings rebuild only on a parameter change. Idle GUI cost drops to ~0. Active repaint cost is also bounded (0.8.9 / H2 + H13 + N2): the Vectorscope's static layer (background gradient, rounded panel, glass edges, grid, labels) and each StereoMeter's static layer (glass panel, centre tick) — pure functions of size/scale/look — are rendered once into cached physical-resolution images and blitted per frame; only signal-dependent drawing (point cloud, clip ring, meter pointer, meter end labels for z-order) is rasterized live. These components are opaque (N2): the cached layers are RGB with the editor's flat `colours::bg` backdrop baked into the rounded corners, so the blit is an opaque copy (not an alpha composite) and the parent never re-renders beneath them. The Spectrum analyser caches its bottom layer the same way (H17: panel + band tints + grid, keyed on its exactly-converging eased inputs) but stays translucent — it sits on the editor's semi-transparent Multiband panel, so the N2 opacity pattern does not apply there. The `FrameClock` refresh (0.8.10) stops the Advanced-only Imager while hidden and debounces its rate cap against single-vblank jitter, so idle stays ~0 on any display. Wave 6 (0.8.12) removed the last avoidable GPU cost: the Spectrum analyser's per-band solo-headphone glyph no longer allocates a **plot-sized transparency-layer offscreen every Advanced-mode frame** — the layer is clipped to the glyph (~200× smaller offscreen) and skipped entirely at full opacity, pixel-identical (worklogs/performance/WAVE6_GPU_RENDER_INVESTIGATION.md). Since Wave 4 the **LevelMeter** carries the same opaque static-layer cache as the other three visualizers (panel, IN/OUT + L/R headers, the four recessed bar slots and the centre dB ruler blit from a cached RGB image; only the numbers, fills, peak blocks and live glass edges rasterize per frame — measured −29…−31 % per meter frame, pixel-identical), the **Vectorscope** tick early-returns while the whole editor is hidden (parity with the S3 gates; it was the one visualizer still scanning the ring unseen), the **Spectrum analyser** converts bins to dB once per NEW transform instead of per decay tick (the multi-second release tail after audio stops re-ran ~4k log10s per tick on identical input; measured −92 % of that loop) and reuses its spectrum/fill/clip-quad `Path` storage across paints (no per-paint heap growth), and the editor's 24 Hz tick memoises its three remaining unconditional jobs: preset-name shaping re-runs only when (name, dirty, slot width) changed, the combo hover poll runs only while the cursor is inside the editor or a box is still lit, and the match readout re-formats only when the raw published float changed (bitwise compare). | src/gui/Vectorscope.cpp, SpectrumImager.cpp, LevelMeter.cpp, CorrelationMeter.cpp, FrameClock.h; src/PluginEditor.cpp `stepMicroAnims` + `timerCallback` + `refreshPresetDisplay`; CHANGELOG [0.8.8], [0.8.9], [0.8.10], [0.8.11], [0.8.12]; worklogs/performance/WAVE4_INVESTIGATION.md + WAVE6_GPU_RENDER_INVESTIGATION.md |
| **Scope transfer is O(1) amortised** (lock-free SPSC ring, fixed 16384 capacity, no alloc); the write index is published **once per block** (`pushBlock`, 0.8.8), so readers observe whole committed blocks. | src/dsp/ScopeBuffer.h:28-80 |

## Known per-sample costs (Verified, qualitative)

- **Crossover-move cost (0.8.10; shared coefficients Wave 3).** `MonoMaker` calls
  `LR4Xover::setCutoffFrequency` per sample during its cutoff glide (recomputes coefficients in
  place, no allocation). `MultibandWidth` and `SoloMonitor` do the same while a split tracks
  under the R(f) = 4·max(1, f/300) oct/s cap (ADR-0015 final + slow-drag fix) — per-sample `tan`
  recomputes on the moving splits for the duration of the drag plus ≤ ~1 s of worst-case
  catch-up — and additionally run BOTH banks for one ~12 ms crossfade per discrete jump (2× the
  stage's filter ticks for that fade only). Since Wave 3 the four filters of one Multiband split
  (x, dx, ax, dax — they always share one cutoff) compute the `tan` prewarp ONCE and the twins
  adopt the coefficients (`LR4Xover::copyCoefficientsFrom`, bit-identical): the worst dry-aligned
  drag drops from 12 to 3 tan/sample, wet-only from 6 to 3; the never-processed `ax[0]`/`dax[0]`
  are no longer updated at all, and the per-aligned-block dx/dax resync and `setBankCutoffs`
  (jump fades / resets) copy instead of recomputing. Session-local: −35…−50 % engine cost on
  continuous-drag scenarios (with the solo-monitor gate below).
  Evidence [Verified]: src/dsp/MultibandWidth.cpp (glide + resync + setBankCutoffs);
  src/dsp/LR4Xover.h (copyCoefficientsFrom); MonoMaker.cpp:32-36; SoloMonitor.cpp;
  worklogs/performance/WAVE3_INVESTIGATION.md.
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
- **Multiband allpass phase compensation is a direct 2nd-order section (Wave 3 — the 0.8.10
  follow-up, done).** The flat-recombination fix runs the multiband low-sum through `bands−2`
  phase-compensation allpasses per bank (wet, and dry when Mix is partial). The consumers only
  ever used the `lo+hi` SUM of the full 4th-order `LR4Xover` ladder, and that sum telescopes to
  the ladder's FIRST 2nd-order TPT section — the entire second section cancels except for one
  float subtract/add rounding pair. `LR4Xover::processSampleAllpass` now computes the surviving
  first-section expression directly: half the allpass arithmetic and state traffic. Class B:
  output equal to the shipped 0.8.10 arithmetic within ~1 ulp (twin-dump measured max 1.19e-7,
  2–24 differing samples per 204,800-sample scenario; 2-band paths and everything outside the
  compensation bit-exact), validated by `testMultibandFlatRecombination` +
  `testMultibandMonoCompat` + the 12-scenario full-engine twin dump. The former micro-item is
  folded in: the never-processed `ax[0]`/`dax[0]` are no longer updated anywhere (glide,
  resync, or `setBankCutoffs`). Session-local: settled 3/4-band multiband −9…−17 % engine cost.
  Evidence [Verified]: src/dsp/LR4Xover.h (`processSampleAllpass` + invariant comment);
  src/dsp/MultibandWidth.cpp (`runWet`/`runDry`); worklogs/performance/WAVE3_INVESTIGATION.md.
- **SoloMonitor runs only while it can be heard (0.8.9 / H1; cutoff-decoupled Wave 3).** With
  nothing soloed and every crossfade gain fully settled, the monitor's per-sample work (6 LR4
  `processSample` + 5 smoother ticks) is skipped entirely — previously ~half of the transparent
  engine floor (callgrind 0.8.8: ~49 % of instructions in the default state). The bank goes cold
  and is reset + snapped on re-engage under the ~12 ms crossfade (the engine's `mbRunning`
  warm/cold pattern). Since Wave 3 the cold gate hinges on the GAINS only: a split drag with
  nothing soloed no longer wakes the bank (it used to run the full per-sample loop — ~22 % of the
  engine's drag profile in the Wave-3 attribution — to compute a provable 1·in + 0·bands
  passthrough); re-engage still snaps to the freshest targets (regression Test 33, bit-untouched
  passthrough asserted). Evidence [Verified]: src/dsp/SoloMonitor.cpp (settled fast path + gate
  comment); tests/dsp_tests.cpp (`testSoloColdThroughDrag`); CHANGELOG [0.8.9], [0.8.11].
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
- **The parked Haas path skips its dead read + blend (Wave 4).** With Haas selected and the
  wet glide settled at exactly 0 (the audio thread's `ScopedNoDenormals` flushes the
  asymptotic amount tail to true zero), the per-sample interpolated delay read and the
  `x + 0·(d−x)` blend are skipped; the delay lines KEEP recording (a re-engage reads
  history written while parked — the same reasoning that rejected the Velvet env freeze)
  and the delay glide keeps tracking retargets. Bit-exact except that a `-0.0f` input
  sample keeps its zero sign where the old blend normalised it to `+0.0f` (no consumer
  distinguishes ±0). Guarded by Test 34 (`testHaasParkedWarmHistory`: parked blocks
  bit-untouched, re-engage plays parked-era history, re-park returns to transparency).
  Session-local: haas-parked scenario −12.4 % whole-run instructions (callgrind).
  Evidence [Verified]: src/dsp/HaasProcessor.cpp (parked fast path + invariant comment);
  tests/dsp_tests.cpp; worklogs/performance/WAVE4_INVESTIGATION.md.
- **The NaN self-heal scan is detector-gated, and the scope/bypass ring fills are
  segmented copies (Wave 4).** The per-sample `isfinite` branches are preceded by a
  branch-free exponent-mask max-reduction (auto-vectorized); the original zeroing loop —
  and the reset cascade — run only when the detector fires, bit-identically (proven on
  NaN-injection twin dumps). `ScopeBuffer::pushBlock` and the bypass ring's write-only
  fill write ≤2 contiguous copy segments instead of per-sample masked/wrapped stores —
  identical ring bytes, the identical single release-store publication (readers copy only
  strictly below the acquired index, so intra-block store order was never observable);
  the bypass READ-BACK branch stays per-sample (its reads can overlap the same block's
  writes when the latency is shorter than the block). Session-local: transparent floor
  −4.9 % whole-run instructions (callgrind). Evidence [Verified]: src/dsp/AnamorphEngine.cpp
  (detector + segmented fill), src/dsp/ScopeBuffer.h (pushBlock);
  worklogs/performance/WAVE4_INVESTIGATION.md.
- **Per-block parameter adoption and settled-state bookkeeping are gated (Wave 5).** The
  wrapper hands the engine a snapshot every block; a BITWISE-identical one (steady
  playback) no longer re-runs `updateDerived`'s two `decibelsToGain` pow calls + ~25
  module setters (`AnamorphEngine::sameParameters` — measured ~250 → ~91 instructions
  per unchanged-snapshot call). The parked VelvetNoise loop (Amount settled at 0, the
  default) skips its dead per-sample density/amount/stop bookkeeping while the presence
  env/gate and history writes keep running (the W3-9 contract) and the write-back keeps
  the verbatim multiplier chain — bit-identical output. The settled global-Width stage
  hoists the smoother call (JUCE's settled `getNextValue()` returns `target`
  mutation-free). The meter publish reuses its `db(peakHold)` conversions and caches the
  bar-fall factor per block size; Level-Match memoises `estBoostDb` on the bitwise
  (Drive, Mix) pair, caches its two MEASURE smoothing coefficients per block size, and
  computes the LUFS conversion only on non-silent blocks (its only consumer). All
  Class A: 19-scenario twin dump bit-exact. Session-local callgrind: transparent floor
  −4.5 %, 64-sample host-like blocks −5.5 %. Rejected on conservatism grounds:
  load-first gating of the per-block atomic exchanges (THREADING_POLICY documents the
  exchange-consume pattern), a generation-keyed whole-snapshot cache (incomplete
  change-tracking contract). Evidence [Verified]: src/dsp/AnamorphEngine.cpp
  (`sameParameters` + width hoist), src/dsp/VelvetNoise.cpp (parked fast path),
  src/dsp/LevelMeters.h, src/dsp/LoudnessMatch.cpp;
  worklogs/performance/WAVE5_INVESTIGATION.md.
- **v0.8.11 final pass: the remaining named candidates are closed as no-op, with
  measurements (no code change).** The long-open **GUI fresh-eyes sweep** was
  carried in-line (the Workflow lens was lost to the org token limit a third
  time) and found the whole GUI paint + message-thread surface already
  exhaustively gated/cached across Waves 1–4 — the only residual (per-call
  `juce::Path`/`Font` locals in the `LookAndFeel` slider draws) is transient
  (gesture-only) and not worth restructuring a shared stateless LookAndFeel.
  **W3-10** (skip `applyWidth` at settled Width==1) stays deferred: a 50 M-sample
  probe shows `applyWidth(·,·,1.0f)` differs from identity in **15.5 %** of
  samples (~1 ULP), so it is **Class B** — a bit-changing DSP change not worth
  taking into a release for ~9 flops/sample the W5-C hoist already vectorised.
  **W5-D** (K-weighting lane-parallel bank) was **prototyped** (`scratchpad/
  kwbench.cpp`): bit-exact vs the scalar chains (0/80 M mismatches) but only
  **1.10×** at the frozen baseline SSE2 flags (2-wide doubles); the 4-wide win
  needs `-march`/AVX2, itself a numerics-frozen build-contract change that would
  also break the prototype's bit-exactness via FMA. `loudness.process()` was also
  confirmed **intentionally unconditional** (feeds the always-live match readout;
  must stay warm for automation-driven engage) — not dead work. Net: ~0.5–1 % of
  floor available behind an AVX decision, deferred. Evidence [Verified]:
  worklogs/performance/FINAL_PASS_v0.8.11_INVESTIGATION.md.
- **The Velvet decorrelation window is carried forward, not rebuilt per block (A7-1, 0.9.5).** The
  H5 gather's linear history image was refilled from the ring on EVERY block by a walk of
  `decorrSamps = round(0.045 * sr)` samples — 2160 at 48 kHz, 8640 at 192 kHz — independent of
  `numSamples`. That made it a FIXED per-block cost that grows with the sample rate and is divided
  by the block length, and it was the single largest per-block item in the engine: measured 20,369
  of the engine's 24,287 Ir/block at 48 kHz (84 %), and **62.3 % of the whole engine at 192 kHz with
  32-sample buffers**. The tail is now slid from the previous block's image (`std::copy` leftwards,
  the same floats), guarded by an offset that `processBlock` clears on entry so every non-gather
  path invalidates it by construction. Class A, and measured rather than argued: bit-identical on
  the 32-scenario committed twin dump AND across 180 configurations (9 scenarios × 5 block sizes ×
  4 sample rates). Engine cost: **−14.3 % at 48 kHz/32, −8.5 % at 48 kHz/64, −4.7 % at 48 kHz/128,
  −2.5 % at 48 kHz/256, −7.5 % at 44.1 kHz/128, −8.7 % at 96 kHz/128, −32.3 % at 192 kHz/32,
  −15.0 % at 192 kHz/128.** The per-block term fell 24,302 → 13,502 Ir at 48 kHz while the marginal
  per-sample term stayed at 1596.6 Ir, which is the signature of a change confined to the refill.
  Guarded by Test 39 (`testVelvetBlockLengthInvariance`: the same audio at 512 and at 32 samples
  must be bit-identical, at four sample rates), proven to fire on both a wrong slide and a missing
  invalidation. Evidence [Verified]: worklogs/performance/PERF_AUDIT_v0.9.5_IMPLEMENTATION.md.
  **SUPERSEDED BY A7-2B (next entry), which deleted the image and the slide together** — this entry
  is kept because the measurement is the reason the next one was scoped.
- **A7-2B (Velvet gather reads the ring in place)** removed the linear history image entirely. H5
  built it so each tap could read one unit-stride run; but the ring is ALREADY unit-stride in `i`
  and merely wraps, so each tap is now 1–3 unit-stride runs read straight from `midHist`, plus this
  block's own Mids for the samples the ring does not hold yet. `linHist` and the A7-1 slide offset
  are gone, and with them the module's only cross-block scratch state. Class A, measured on both
  committed instruments: **32/32 twin-dump scenarios identical** and **0 mismatches across 180
  configurations**, plus Test 40, which compares the gather against the per-sample loop directly.
  Engine cost (callgrind Ir/block, startup-subtracted, scenario `working`): **−12.2 % at 48 kHz/32,
  −3.3 % at 48 kHz/128, −0.8 % at 48 kHz/512, −37.2 % at 192 kHz/32, −13.5 % at 192 kHz/128,
  −3.8 % at 192 kHz/512, −12.8 % at 96 kHz/64.** The fixed per-block term fell **12,957 → 5,546 Ir
  at 48 kHz and 39,373 → 6,104 Ir at 192 kHz**, and the shape of that is the point: the A7-1 term
  was proportional to `decorrSamps` and therefore grew with the sample rate, and A7-2B's does not —
  at a 32-sample block the rate penalty from 48 to 192 kHz falls from **39.8 % to 0.04 %**.
  **The measured trade, stated because it is real:** the win narrows as the rate falls, and at
  **44.1 kHz** — where the image A7-1 slid was smallest — the change is neutral to slightly
  negative: **−0.2 % at 44.1 kHz/32 at the default density 0.5, +1.0 % at density 1.0** (64 active
  taps, where the per-tap preamble is paid 64 times against one small `std::copy`). The no-wrap fast
  path in the split is what holds that at 1 %: without it the same points measure +0.9 % and +3.0 %.
  **That +1.0 % corner is ACCEPTED (maintainer decision, 2026-08-22) and A7-2B is not reverted** —
  it is the lowest supported rate × the smallest block × maximum density, the default-density point
  at the same rate and block is negative, and the alternative reintroduces the cross-block state
  A7-2B deleted.
  Evidence [Verified]: src/dsp/VelvetNoise.cpp (the split + its no-aliasing argument);
  worklogs/performance/PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md;
  worklogs/performance/A7_DECISION_PACKET.md (Decision 4).
- **The three Amount-0 parked fast paths are actually REACHABLE (A7-9, 0.9.5).** `VelvetNoise`,
  `HaasProcessor` and `ChorusEngine` each gate a cheap Amount-0 path on the wet glide having reached
  **exactly** 0 — and it never does. With a 0 target the update is `a -= k*a`, and under the block's
  `ScopedNoDenormals` the DECREMENT underflows before `a` does, so the glide stalls just under
  `FLT_MIN/k` (7.837e-36 in Velvet, 1.175e-35 in Haas, rate-dependent in Chorus because
  `wSmooth = 1/(0.01·sr)`) and every later decrement is exactly 0. The gates therefore stayed false
  forever after a user turned Amount down — the ONLY route to the state they were written for — so
  every Wave-4/Wave-5 parked-path claim held only for a session in which the control was never
  touched. The three gates now test the **fixpoint** (`aNext == currentAmount`: can the glide still
  move) instead of the **value**, which is the test `VelvetNoise` has always used for its density
  glide; each keeps the pre-A7-9 condition as a second disjunct, so the gates can only ever admit
  more than before. No DSP state is snapped, frozen or mutated, and re-engage is bit-identical.
  Recovered, post-A7-2B: `ChorusEngine` **+14,220 Ir/block** (48 kHz/128), `HaasProcessor` **+5,635**,
  `VelvetNoise` **+4,019** (48 kHz/32). **Class B**, on the **silence-region sample class** — digital silence, plus near-silent
  samples (≲ 2–4e-28 of full scale under FTZ) co-occurring with a louder delay history; a
  2026-08-30 measurement corrected the earlier "digital silence only" wording
  (`PERF_AUDIT_A7-9_NEARSILENT_SCOPE.md`): the stalled
  multiplier leaked `a_stall × (wet term)` wherever the dry term could not absorb it — measured
  1.563e-35 (Chorus, 192 kHz ≈ −696 dBFS), 8.043e-36 (Haas, 48 kHz), 7.145e-36 (Velvet, 48 kHz)
  on silence against the pre-fix sources, up to 1.204e-35 on near-silent tails, bounded by
  `FLT_MIN/k` times the module's wet gain. On real signal the
  output is bit-identical (0 of 102,400 samples differ, every module, every rate — re-verified
  bit-exact at every tail amplitude down to 1e-20), and after the fix
  the silence output is an **exact zero**. Explicit maintainer approval recorded 2026-08-22. Guarded
  by **Test 41**, proven to fail on all four cases against the pre-A7-9 sources, and **Test 42**
  (near-silent parked identity, likewise fire-proven); the committed twin
  dump does NOT cover it (`tests/dsp_dump.cpp` holds `algoAmount` at 0.7 and never ramps down, which
  is how the defect survived from Wave 4).
  Evidence [Verified]: src/dsp/VelvetNoise.cpp, src/dsp/HaasProcessor.cpp, src/dsp/ChorusEngine.cpp
  (the three gates); tests/dsp_tests.cpp (`testA79ParkedPathsReachableAfterStall`);
  worklogs/performance/PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md.
- **The x86-64 build targets AVX2 (A7-5 / W5-D, 0.9.5, ADR-0031).** `-march=haswell
  -ffp-contract=off` on the GCC/Clang x86-64 builds — Linux x86-64 and the macOS `x86_64` slice via
  `-Xarch_x86_64`; arm64 carries nothing, deliberately (ADR-0031 option 4). ~~MSVC carries
  nothing~~ **— superseded: since ADR-0032 (2026-08-30) the Windows/MSVC x64 build carries
  `/arch:AVX2` at the default non-contracting `/fp:precise`, so every shipped x86-64 binary now has
  an AVX2 baseline. No Windows Ir figure exists; the benefit there is stated as mechanism-shared.
  The `−17.2 %` below is the Linux GCC-13 measurement and is not a Windows number.**
  **−17.2 % engine-wide** (1704.9 → 1412.2 Ir/sample,
  48 kHz/128, scenario `working`). **Class A**: 32/32 twin-dump scenarios identical to the baseline,
  0 mismatches across 180 configurations, and **0 FMA instructions emitted** against 707 with
  contraction left at its default — which is the whole reason it is Class A. The frozen baseline had
  no FMA instruction at all, so the permissive `-ffp-contract=fast` default was inert; `-march`
  introduces the instruction and `-ffp-contract=off` keeps it unused. The GCC/Clang cross-check
  survives for the same reason. The cost is an **ISA floor** — Haswell (2013) / Excavator (2015),
  `SIGILL` below it — recorded in `COMPATIBILITY_POLICY` before the flag landed. This is what
  W5-D (the K-weighting lane-parallel bank, above) was deferred behind; it is now unblocked as a
  Class-A candidate, and unscheduled.
  Evidence [Verified]: CMakeLists.txt (the `AnamorphHardening` x86-64 baseline block);
  docs/architecture/design-decisions/ADR-0031-x86-64-isa-baseline.md;
  worklogs/performance/PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md §6.
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
and `maxBlockSize` at prepare() time (src/dsp/AnamorphEngine.cpp:36-39) with no hard-coded SR
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

## How to produce those numbers — required benchmark procedure

This section was written when **no repeatable benchmark was committed to this repository**:
`scripts/` had no bench entry point, `tests/` measured correctness only, and every number quoted
in the Wave 3-6 worklogs came from a *session-local* scratch harness that was never checked in
(see e.g. `worklogs/performance/WAVE3_INVESTIGATION.md`). Anyone could therefore reproduce a
measurement, but no two people would reproduce the *same* one. What follows fixes the method so
results are comparable across sessions and machines; the section itself deliberately added **no
infrastructure** — closing RISK-002 needs measurements, not a framework. **The harness landed
afterwards and is described below**, so the three TODOs above are no longer open for want of one:
the reason they are still open is given at the end of this section.

**What to measure.** `anamorph::AnamorphEngine` alone, not the plug-in wrapper and not the GUI.
It is a plain library target (`AnamorphDSP`, `CMakeLists.txt`) with no JUCE plug-in host required,
and `tests/dsp_tests.cpp` already demonstrates the whole calling pattern: construct,
`prepare(sr, block)`, `setParameters(EngineParameters)`, then
`process(juce::AudioBuffer<float>&)` on a pre-sized stereo buffer in a loop
(`AnamorphEngine.h:60`; see e.g. `dsp_tests.cpp:118`). A bench is that loop with a timer around
it — a single scratch `.cpp` linked against `AnamorphDSP`. Allocate the buffer once, outside the
timed region: the point is to measure the engine, not `AudioBuffer`'s constructor. Note that
`AnamorphDSP` is an **INTERFACE** library, so the bench needs its own target that compiles the
sources — add it behind an OFF-by-default option (mirroring `ANAMORPH_BUILD_TESTS`) so it never
enters a release build.

**The harness now exists: `tests/bench.cpp`, behind `ANAMORPH_BUILD_BENCH` (OFF by default).**
It implements exactly the procedure below — shipped-Release flags including
`juce_recommended_lto_flags`, the full SR × block × algorithm × oversampling × multiband matrix,
median ns/sample plus worst single block, ≥5 repetitions with the spread reported, the buffer
allocated once outside the timed region, and a deterministic LCG-plus-tone signal so two runs feed
the engine the same samples. The `AnamorphDSP` INTERFACE library means it compiles the sources into
its own console target; that **is** a Build System change under
`docs/policies/ARCHITECTURE_REVIEW_GATE.md`, and it landed under the maintainer approval covering
this roadmap item (2026-08-18) rather than by treating the gate as inapplicable. The procedure was
settled by this document beforehand and is unchanged by the implementation; the sibling product's
`tests/bench.cpp` was read as a worked example of the same method, not copied.

**Constraint C2 is enforced by the harness, not merely stated by this document.** If it cannot
identify the CPU and `ANAMORPH_BENCH_CPU` is unset, it **exits 2 and prints nothing** — an
unattributable number is worse than no number, so it declines to produce one. `ANAMORPH_BENCH_SECONDS`
(default 10) and `ANAMORPH_BENCH_REPS` (default 5) are the two knobs, and their defaults are the
minimums this section requires rather than arbitrary values.

**CI builds and smoke-runs it; CI does not gate on its numbers, and that is a measurement rather
than a preference.** Across independent invocations on an otherwise idle machine, the median
ns/sample varied by **7.2%** and the worst-block figure by **65.4%**. A threshold on the worst-block
number would be pure noise, and a threshold on the median would sit inside its own run-to-run
variance — either one is the gate-that-cries-wolf this repository's testing policy is written
against, and the first red run would teach everyone to re-run it. What CI *does* catch by building
and smoke-running the target is the regression that actually happens to benchmark harnesses: one
that silently stops compiling against the engine it measures, so the numbers are unobtainable on the
day someone needs them. The step lives in `linux-lto-tests`, which already builds shipped-class
optimized code.

**The rows above are still TODO, and the reason has changed.** It is no longer "there is no
harness" — it is that a defensible number needs a machine this project can name and hold still. A
shared cloud runner is precisely the "shared or thermally-throttled machine" this section says is
not a datum, and the 65.4% worst-block spread measured on one is the evidence for that sentence
rather than a hypothetical. Run the harness on a known desktop, record the CPU/OS/compiler beside
every cell, and populate the rows in that change.

**Attempted again 2026-08-22, after A7-1 shipped, and declined again on the same grounds — recorded
so the next attempt does not repeat it.** The harness builds and runs end to end; what is missing is
still only the machine. Four things disqualified the one available, and they are the checklist for
the one that will not be:

* **The CPU cannot be named.** `/proc/cpuinfo` reports `Intel(R) Xeon(R) Processor @ 2.80GHz` — a
  masked virtual model string with no SKU. `AnamorphBench` prints it and therefore satisfies
  constraint C2's letter while identifying no actual processor, which is the one failure mode C2
  exists to prevent.
* **It is a container** (`systemd-detect-virt` → `docker`), not a desktop.
* **The clock cannot be seen or held.** No `cpufreq` is exposed, so neither the governor nor any
  thermal or host-side throttling can be observed, let alone pinned.
* **It is not idle.** Load average was 0.44 / 2.10 / 1.69 while the bench ran.

The medians happened to be stable across three consecutive runs here (working reference 193.79 /
193.31 / 194.09 ns/sample, 0.4 %), and that is **not** sufficient: the column RISK-002 needs is the
worst single block, and it measured 104.0 / 128.5 / 118.2 µs across those same three runs — a
**23.6 %** spread on the one figure the open question turns on.

**One gap for whoever does run it:** `AnamorphBench` records the CPU string, the core count and the
compiler, but not the OS version or the build configuration. This section asks for CPU/OS/compiler
beside every cell, so those two must be written down by hand alongside the table (or the harness
taught to print them) — otherwise the recorded datum is missing half of what makes it reproducible.

**Build it the way users get it.** `-DCMAKE_BUILD_TYPE=Release` only. The `AnamorphHardening`
flags and `juce::juce_recommended_lto_flags` are part of the shipped configuration, so a bench
that does not link them is measuring a different binary.

**The matrix.** Each cell is one number; record every axis with the result or the number is
meaningless:

| Axis | Points to cover |
|---|---|
| Sample rate | 44.1, 48, 96, 192 kHz |
| Block size | 32, 64, 128, 256 samples |
| Algorithm | Haas, Velvet Noise, Chorus, Dimension-D |
| Oversampling | Off; the engaged case (2×/4×/8× **with Drive > 0**, since the wrap only engages for nonlinear/modulation work — see the Verified row above); and, since ADR-0034, the **selected-but-skipped** case (2×/4×/8× with Drive 0 and a linear algorithm), which is where the latency stand-in ring runs and where the CPU saving must remain visible |
| Multiband | off; 4 bands static; 4 bands with a split **dragging** (the RISK-002 hot path) |

**What to record per cell.** Wall-clock **nanoseconds per sample**, derived from a run long
enough to dominate timer noise (≥ 10 s of audio), plus the *worst single block* in the run —
peak matters more than average for a real-time thread. Take the **median of ≥ 5 repetitions** and
report the spread; a single run on a shared or thermally-throttled machine is not a datum.
Report the CPU model, OS, compiler and version alongside, and state whether the machine was
otherwise idle. For attribution rather than totals, `valgrind --tool=callgrind` over the same
harness gives instruction counts that are stable across machines — that is what the Wave 3-6
worklogs used, and it is the right tool for comparing two candidate implementations.

**Memory.** Per-instance footprint is a `prepare()`-time question, not a steady-state one: measure
RSS before and after `prepare()` at the largest supported sample rate and block size. There is
nothing to measure during `process()` — that it allocates nothing is the invariant below.

**The pass/fail question these numbers must answer** — the reason RISK-002 is open:

> On a defensibly modest machine, at 48 kHz / 128 samples, how many Anamorph instances can run
> before the engine consumes a whole core — and does a fast Multiband split drag produce a
> transient cost that would drop a buffer at that instance count?

Until that has an answer, the CPU/memory rows above stay TODO. **Do not populate them with
estimates** (constraint C2); a number without the recorded matrix position, machine and
methodology is worse than an honest TODO.

## Invariant

> `processBlock` must remain allocation-free, lock-free, and IO-free (see
> `docs/policies/REALTIME_AUDIO_POLICY.md`). Any change that could introduce an unbounded or
> per-block allocation requires Architecture Review.
