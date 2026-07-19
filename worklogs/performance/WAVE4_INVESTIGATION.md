# Wave 4 — Runtime Performance Investigation (worklog)

> Working notes + handover record for the Wave-4 optimisation pass. This is a **worklog**,
> not architecture documentation: numbers here are session-local measurements on the
> environment below and are NOT release claims (constraint C2 — `PERFORMANCE_BUDGET.md`
> stays qualitative). Future agents: read `WAVE3_INVESTIGATION.md` first; this wave
> implements its recorded top remaining candidates (W3-11/12/13/14) and re-verifies
> nothing older (Waves 1–3 conclusions were left as recorded).

- **Date:** 2026-07-19
- **Branch:** `claude/beautiful-sagan-JAUFI` (from `main` @ `8ffb9a6`, post-PR #63)
- **Environment:** Linux x86_64 container, Intel Xeon @ 2.10 GHz (4 cores), gcc 13.3.0,
  `-O3 -DNDEBUG` (the repo's Release config), JUCE 8.0.14 (pinned), Xvfb for the GUI
  harnesses (JUCE's Linux "vblank" is a display-rate timer — verified in
  `juce_Windowing_linux.cpp` (`TimedCallback vBlankManager`) — so FrameClock ticks run
  normally under Xvfb).
- **Method:** the Wave-3 scratchpad harnesses, re-frozen at this HEAD and extended:
  the full-`AnamorphEngine` scenario bench (48 kHz, 512-sample blocks, deterministic
  noise, now under `ScopedNoDenormals` like the real audio thread) with 7 new Wave-4
  rows (`haas-parked`, `haas-active`, `haas-reparked`, `bypass-on`, `nan-heal`,
  `nan-heal-haas`, `default-b64`), per-scenario float32 **twin dumps** for before/after
  bit-comparison, callgrind Ir attribution (base vs new binaries — deterministic, immune
  to the container's CPU drift), a new `LevelPix` harness (real `LevelMeter` +
  deterministic audio-driven `anamorph::LevelMeters`, `paintEntireComponent` timing +
  raw-pixel dumps), the existing `PixDump` S12 harness (SpectrumImager pixel identity,
  clip-off and clip-on), a decay-tick micro-bench, and the `EdBench` real-editor harness
  (processor + editor under Xvfb, phased per-thread CPU).
- **Method caveat:** a 4-lens verification/discovery Workflow fan-out was launched and
  lost to an org token-spend limit (all four agents died before returning findings).
  Verification was instead carried in-line: every candidate's preconditions were checked
  against primary sources (JUCE `Path::clear` storage retention, `Decibels::gainToDecibels`
  purity, the Linux vblank timer, the editor's flat `colours::bg` backdrop behind the
  meter, `bypassDelayBuffer` prepare sizing ≥ maxBlock, no `-ffast-math` in the build
  flags), and the mechanical validations below (twin dumps, pixel dumps, warning diff,
  suite) carry the correctness burden. No independent fresh-eyes sweep happened this
  wave — recorded as a follow-up item.

## Scope guard

No DSP behaviour, parameter, serialization, threading, latency or signal-order change.
Every item is Class A (bit-exact audio / pixel-identical GUI); the one caveat is D1's
±0-sign note below. The W3 category-C rejections stand un-revisited: LoudnessMatch
gating (W3-7), LevelMeters gating (W3-8), Velvet env freeze (W3-9) — all contractual;
Width==1 gate (W3-10, Class B) stays deferred.

## Baselines (this environment, HEAD `8ffb9a6`)

Wall-clock ns/sample (median of 9, pinned core; the container's CPU speed drifts
between runs — the Wave-3 caveat — so the numbers that matter below are interleaved
A/B or callgrind, never these absolutes): default-transparent 44.9, haas-parked 43.2,
haas-active 44.6, bypass-on 45.5, default-b64 88.6 (per-block overheads ≈ per-sample
cost at 64-sample blocks), velvet-1.0 51.5, mb4-settled 103.4, nan-heal 124.1 (the
healing resets, not the scan).

Callgrind Ir attribution at HEAD, default-transparent (shares of the whole run incl.
~14 % harness noise-fill): LevelMeters.h 22.6 % (contractual, W3-8), LoudnessMatch
23.8 % (contractual, W3-7), engine inline 11.9 % (includes the NaN scan and bypass
ring fill), VelvetNoise parked 11.4 %, Correlation 2.9 %, ScopeBuffer push 1.5 %.
haas-parked: HaasProcessor::processBlock 8.4 % + 2.6 % libm floor (the parked loop's
interpolated read).

EdBench at HEAD (Xvfb, 12 s phases; software rendering, so absolutes are container-
local): meters shown — P0 no-blocks 11.2 %, P1 idle silence 11.5 %, P2 active signal
40.7 %, P3 active+automation 43.0 %, P4 editor-closed 0.9 %; Advanced — 19.0 / 18.7 /
72.2 / 73.4 / 1.2 %. **Measurement trap recorded:** the FIRST baseline run of the
meters scenario reported ~1.2 % across all phases with its H15 probe finding 0
animated sliders — the editor never fully engaged (cause unestablished; possibly a
first-Xvfb-start artifact) — and was discarded; the numbers above are a re-run under
the same conditions as the A/B batch. Future agents: sanity-check the H15 probe line
(13 sliders expected) before trusting an EdBench phase table.

`LevelPix` at HEAD: 1.12–1.22 ms per full meter frame (154×396, all four bars + 8
numbers + panel/ruler re-rasterized per frame).
Decay-tick micro-bench at HEAD: 39.3 µs per SpectrumImager mags-release tick
(4097 × `gainToDecibels` on unchanged input).

## Candidates → decisions

| id | candidate (W3 ref) | class | decision |
|----|--------------------|-------|----------|
| G1 | LevelMeter static-layer cache + opaque (W3-13, the H2/H13/N2 recipe) | A (pixel-identical) | **implement** |
| G2 | SpectrumImager per-tick dB conversion → per-transform cache (W3-14) | A (value-identical) | **implement** |
| G3 | SpectrumImager paint `Path` reuse (W3-14) | A (pixel-identical) | **implement** |
| G4 | Editor 24 Hz memoisation: preset-name shaping keyed on inputs; combo hover poll pre-gated on an editor-level cursor test; match readout keyed on the raw float (W3-14) | A | **implement** |
| G5 | Vectorscope hidden-editor `isShowing` gate (W3-14; parity with the LevelMeter/StereoMeter S3 gates) | A | **implement** |
| D1 | Haas parked fast path: skip the interpolated read + blend at amount exactly 0; KEEP ring writes + both glides (W3-11) | A (see ±0 note) | **implement** |
| D2 | NaN-scan: branch-free exponent-mask max-reduction detector before the (unchanged) fix-up loop (W3-12) | A | **implement** |
| D3 | Segmented ring copies: `ScopeBuffer::pushBlock` + the bypass ring's write-only fill → ≤2 contiguous copies (W3-12) | A | **implement** |
| — | LoudnessMatch / LevelMeters gating, Velvet freeze (W3-7/8/9) | C | **stand rejected** (contracts unchanged) |
| — | Width==1 exact gate (W3-10) | B | **stay deferred** (micro win, injects deltas downstream) |
| — | Bypass read-back branch segmentation | — | **rejected**: its reads can overlap the same block's writes when `readLat < n`; per-sample stays |

Expected gains stated before implementation: G1 ≈ half of an active meter frame
(the H13 fillPanel precedent); G2 ≈ the whole decay-tail conversion (~40 µs/tick);
G3 = allocation churn only (small); G4 = the shaping/formatting share of the 24 Hz
tick (GlyphArrangement per tick → ~never); G5 = the hidden-editor scan to zero;
D1 ≈ half the parked-Haas loop (~5 % of that scenario); D2+D3 ≈ 3–5 % of the
transparent floor.

## Implementation notes (all in this wave's single commit)

- **G1** `src/gui/LevelMeter.{h,cpp}`: a `Layout` struct is now the single source of
  the row/column/bar/ruler maths (the former in-paint arithmetic verbatim);
  `ensureStaticLayer` renders panel + IN/OUT + L/R headers + the four recessed bar
  slots (gradient + clipped faint ticks) + the centre dB ruler into an opaque RGB
  image at physical resolution with the editor's flat `colours::bg` baked into the
  corners (N2 — same backdrop-coupling caveat as the Vectorscope); `paint()` blits it
  and draws only the numbers, the clipped fills/peak blocks and the live glass edges
  (kept after the fills, preserving draw order; the ruler is geometrically disjoint
  from every dynamic pixel — bars are inset ≥ 3 px from the ruler column, its ticks
  reach 0.5 px past its edge — so caching it beneath is composition-identical).
  During the meter reveal animation the size changes per frame, so the layer rebuilds
  per frame at reveal cost ≈ the old direct draw (H17-style; the reveal is ~0.3 s).
- **G2** `src/gui/SpectrumImager.{h,cpp}`: `magsDb[k]` = `gainToDecibels (fftData[k]
  * norm)` recomputed only when `pushFFT()` returns true (the only writer of
  `fftData`; `norm` is a compile-time function of `fftSize`); the decay loop reads
  the cache. Init `kMinDb` = the conversion of the all-zero ctor `fftData`.
- **G3** `src/gui/SpectrumImager.{h,cpp}`: `specPath`/`specFillPath`/`clipQuadPath`
  members, `clear()`-reused (`Path::clear` → `Array::clearQuick`, storage retained —
  verified in juce_Path.cpp); `fillPath` copy-ctor → `clear()+addPath` (identical
  element sequence).
- **G4** `src/PluginEditor.{h,cpp}`: `refreshPresetDisplay` keyed on
  (`currentName`, `isDirty`, slot width) — both inputs are cached-cheap
  (`PresetManager` S10 signature cache); combo hover loop pre-gated on one
  editor-level `isShowing + contains(getMouseXYRelative())` plus a `comboHoverLit`
  latch (every box lies inside the editor bounds, so cursor-outside ⇒ no box can be
  hovered; a still-lit box keeps polling until cleared — including when the editor
  stops showing); match readout formatted only when the raw float changed, compared
  **bitwise** so a NaN transition still updates rather than freezing the readout.
- **G5** `src/gui/Vectorscope.cpp`: `tick()` early-returns on `! isShowing()`; on
  re-show the accumulated `fresh` delta triggers the same capacity-capped catch-up
  scan the constructor's gate init performs.
- **D1** `src/dsp/HaasProcessor.cpp`: with `amount` and `currentAmount` both exactly
  0 (exact compares, the S4 idiom; FTZ under the processor's `ScopedNoDenormals`
  flushes the glide tail to true 0 in ~1.8 s), the loop keeps the delay-glide tick
  and BOTH ring writes + index advances and skips `readDelayed` + the blend.
  **±0 note:** the old `x + 0·(d−x)` normalised a `-0.0f` input sample to `+0.0f`
  whenever `d > x`; the fast path preserves the input's zero sign. No numeric or
  audible consequence (every consumer treats ±0 identically); no other case differs.
  Regression **Test 34** (`testHaasParkedWarmHistory`) asserts the path-agnostic
  invariants: parked blocks bit-untouched, re-engage plays history recorded WHILE
  parked (fails if a future change stops the parked ring writes), re-parked returns
  to bit-transparency.
- **D2** `src/dsp/AnamorphEngine.cpp`: detection pass = per-sample
  `bits & 0x7f800000` max-reduction (auto-vectorizes; the masked field's max reaches
  `0x7f800000` iff some sample is non-finite — no false positives/negatives, unlike
  an OR-union which false-fires when exponents union to all-ones); the original
  isfinite/zeroing loop runs only when the detector fires, so the healing cascade is
  bit-identical (`nan-heal`/`nan-heal-haas` twin rows).
- **D3** `src/dsp/ScopeBuffer.h` + `src/dsp/AnamorphEngine.cpp`: ≤2 `memcpy` /
  `FloatVectorOperations::copy` segments replace the per-sample masked/wrapped
  stores; identical ring bytes; the scope's single release-store publication is
  unchanged (readers only copy strictly below the acquired index, so intra-block
  store order was never observable — THREADING_POLICY intact). The bypass
  read-back branch is untouched (overlap hazard above).

## Results

### Output equivalence (the gate for everything)

- **Twin dump**: 19 engine scenarios × 200 blocks × stereo float32 (the 12 Wave-3
  rows + the 7 new Wave-4 rows, including both `nan-heal` rows and `default-b64`):
  **every row bit-exact for all 204,800 samples** (`maxAbs 0.0, 0 samples differ`).
- **Pixels**: `LevelPix` 154×396 ARGB — **byte-identical** base vs new (and base
  self-deterministic across runs); `PixDump` SpectrumImager at the harness's widths,
  clip-off (amp 0.02) and clip-on (amp 0.6) — **byte-identical** both.
- **DSP suite**: 33 tests + A/B guard, **140 checks, 0 failures** (Test 34 added:
  parked-Haas warm-history invariants, 4 checks).
- **Warnings**: the pre-change and post-change builds emit the IDENTICAL warning set
  (same 10 unique project lines, displaced only by line-number shifts) — zero new,
  zero removed.
- **pluginval**: cannot run locally (the release download is egress-blocked in this
  sandbox — the recorded standing constraint); it runs blocking at strictness 10 in
  both modes on the CI gate for this push.

### Performance (fair A/B: callgrind Ir on the same inputs, base vs new binaries)

| scenario | Ir delta (whole program) | note |
|---|---:|---|
| default-transparent | **−4.9 %** | D2 + D3 on the always-on floor; ~14 % of the total is harness noise-fill, so the engine-only cut is ≈ −5.7 % |
| haas-parked | **−12.4 %** | D1 + D2 + D3 (engine-only ≈ −14.5 %) |
| bypass-on | **−3.0 %** | D2 + D3a (the bypass fill takes the untouched read-back branch here) |

Wall-clock interleaved medians agreed in direction (haas-parked 45.3 → 40.8 ns/sample,
≈ −10 %) but the container's CPU drift (±10 % between adjacent runs) makes callgrind
the citable instrument for the small rows.

- **G1**: full meter frame 1.12–1.22 → 0.81–0.83 ms (**−29…−31 %** per frame,
  pixel-identical); on a visible meter with active audio that is every displayed
  frame (up to ~120 Hz on a fast panel).
- **G2**: decay tick 39.3 → 3.1 µs (**−92 %** of the mags-release loop) for the
  multi-second release tail after audio stops and every silent-window tick until
  settle.
- **G3**: no measurable paint-time change on the imager (~4.2–4.3 ms/frame both
  sides — the win is the removed per-paint heap alloc/growth, not rasterization);
  kept because it is free, allocation-churn-reducing, and pixel-identical.
- **G4/G5**: not separately instrumented (the 24 Hz shaping/formatting and the
  hidden-tick scans sit below EdBench's tick resolution); the reasoning stands on
  the code inspection above — GlyphArrangement shaping now runs on preset/name/width
  change only, the match readout formats on value change only, the hover poll and
  hidden vectorscope ticks drop to a compare.
- **EdBench end-to-end** (same-conditions A/B, the discarded-first-run caveat above):
  meters-shown ACTIVE phases **P2 40.7 → 34.0 % (−16.4 %)** and **P3 43.0 → 35.9 %
  (−16.5 %)** of a core — consistent with the −30 % LevelPix per-frame cut applied to
  the meter's share of the active editor profile; meters-shown idle P0/P1 at parity
  (~11 %, the meter is S3-idle-gated there so G1 has nothing to remove); Advanced
  phases at parity (−0.5…+0.7 %, within run noise — G2's decay tail and G3's
  allocation churn sit below this harness's resolution).

## Rejected / deferred this wave

- **Bypass read-back segmentation** — overlap hazard (reads of samples written this
  block when `readLat < n`); correctness first.
- **Per-paint `Font` hoists, bandCurve path reuse, further imager micro-items** —
  interaction-gated paths, individually sub-measurable; not worth churn.
- **W3-10 Width==1 gate** — unchanged Class-B verdict.
- **The W3-7/8/9 category-C gates** — contracts; would need the maintainer's
  Architecture Review, deliberately not proposed again.

## Handover notes for the next wave

- Harnesses now in the scratchpad: `bench_engine.cpp` (+ Wave-4 scenarios, FTZ),
  `levelpix/` (LevelMeter pixel+timing), `pixdump/` (imager pixels), `edbench/`
  (+ `EDBENCH_METERS`), `w4/` result logs. Recreate from these descriptions if the
  scratchpad is gone; always keep a frozen baseline binary and interleave (the CPU
  drift is real).
- Remaining ranked candidates: a fresh-eyes discovery sweep (the lost Workflow —
  worth re-running when budget allows), W3-10 (Class B), Haas/Velvet/Chorus
  `processNonlinearRegion` per-block entry costs at tiny buffers (default-b64 shows
  per-block overheads ≈ per-sample cost at 64 samples — unattributed this wave),
  and the two category-C items if the maintainer ever wants an Architecture Review.
- The transparent floor after this wave is still dominated by LevelMeters (~22 %,
  W3-8) and LoudnessMatch (~24 %, W3-7) — both contractual; nothing Class A of
  comparable size remains on the floor.
