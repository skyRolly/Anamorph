# Performance audit — v0.9.4 investigation round

**Status: investigation complete, no product code changed.** One optimization was implemented in a
throwaway build to *measure* it (§6.1) and is not in the tree. Everything below is a proposal for a
maintainer to schedule.

**Round ID prefix:** `A7-` (the seventh performance round; Waves 1–6 and the v0.8.11 final pass
precede it). Read alongside `worklogs/POST_v0.8.12_AUDIT_AND_ROADMAP.md` §4, which is the
authoritative disposition table for every candidate open before this round.

---

## 1. Goal and scope

Answer three questions with measurements rather than inspection:

1. **Where does the engine actually spend instructions today**, after six optimization waves?
2. **Which of the remaining costs are worth attacking**, given the program's Class-A (bit-exact)
   standard and the hard-stop classes in `docs/policies/ARCHITECTURE_REVIEW_GATE.md`?
3. **What is the execution order**, with dependencies and expected outcome per stage?

In scope: `anamorph::AnamorphEngine` (the audio thread) and the editor's paint path.
Out of scope: the `juce::Timer` / `VBlankAttachment` tick path (needs a message loop; see §4.4),
GPU rendering (no GPU in this environment — the same limitation `WAVE6_GPU_RENDER_INVESTIGATION.md:128`
records), and everything already disposed of in the roadmap table (§8).

---

## 2. Method

### 2.1 Instruments

| Instrument | Built from | What it answers |
|---|---|---|
| `tests/bench.cpp` → `AnamorphBench` | committed, `-DANAMORPH_BUILD_BENCH=ON`, Release + `juce_recommended_lto_flags` | wall-clock ns/sample over the `PERFORMANCE_BUDGET.md` matrix |
| a session-local scenario driver | not committed; replicates `bench.cpp`'s `workingParams()` exactly, one scenario per run, stimulus generated **before** the measured region | callgrind attribution per scenario, free of harness noise |
| a session-local editor paint driver | not committed; constructs the real processor + editor headlessly (the `tests/state_tests.cpp:1611` pattern), paints into a `juce::Image` | per-component repaint cost |

### 2.2 Why callgrind instruction counts and not wall-clock

`PERFORMANCE_BUDGET.md` is explicit that a shared cloud runner **is not a datum** for the budget
rows, and that *"for attribution rather than totals, `valgrind --tool=callgrind` over the same
harness gives instruction counts that are stable across machines"*. This round therefore reports
**Ir (instructions retired) as its primary unit** and treats every wall-clock figure as indicative
only. The container's own numbers make the case: the committed bench measured **spread up to 34 %**
on some cells and worst-single-block figures varying by more than 4× between neighbouring rows.

### 2.3 Startup subtraction

Every callgrind figure below is the **difference between a 3.0 s run and a 1.0 s run, divided by
two** — process start, dynamic-linker symbol resolution, `prepare()` allocation and `reset()` all
cancel exactly. Without it, `dl-lookup` and `prepare()` inflate the idle scenario by ~8 %.

### 2.4 Cross-validation against the prior rounds

The idle attribution reproduces Wave 4's independently (`WAVE4_INVESTIGATION.md:56-59`, which quoted
its shares including *"~14 % harness noise-fill"*):

| Component | Wave 4 (incl. harness) | Wave 4 rescaled | This round |
|---|---|---|---|
| `LevelMeters.h` | 22.6 % | 26.3 % | **28.6 %** |
| `LoudnessMatch` | 23.8 % | 27.7 % | **30.7 %** |
| VelvetNoise parked | 11.4 % | 13.3 % | **9.7 %** |
| `Correlation.h` | 2.9 % | 3.4 % | **3.8 %** |
| `ScopeBuffer` push | 1.5 % | 1.7 % | ~3.1 % |

Two independent harnesses, two rounds apart, agreeing to a few points. The methodology is sound.

**Machine (constraint C2):** Intel(R) Xeon(R) Processor @ 2.80 GHz, 4 cores, gcc 13.3.0, Linux
container, **not otherwise idle and not held still**. Named because a number without its machine is
not a measurement — and named precisely so that **no wall-clock number here is promoted into
`PERFORMANCE_BUDGET.md`'s TODO rows** (§7, item A7-0).

---

## 3. Baseline: what the engine costs today

### 3.1 Whole-engine cost by scenario

Steady-state instructions per second of 48 kHz stereo audio, 128-sample blocks:

| Scenario | Ir/s | Ir/sample | vs working |
|---|---|---|---|
| transparent defaults (every stage at identity) | 21,853,774 | 455.3 | 0.26× |
| working, multiband off | 39,948,589 | 832.3 | 0.48× |
| Haas algorithm | 73,472,596 | 1530.7 | 0.88× |
| Chorus algorithm | 77,240,151 | 1609.2 | 0.92× |
| **working reference** (Velvet 0.6, width 1.4, mix 0.85, 4-band MB, mono-maker, match on) | **83,952,051** | **1749.0** | 1.00× |
| **bypass** (host bypass parameter engaged) | **85,135,484** | 1773.7 | **1.01×** |
| multiband split DRAGGING (RISK-002) | 93,337,676 | 1944.5 | 1.11× |
| Drive 8 dB, oversampling ×8 | 208,571,353 | 4345.2 | 2.48× |

### 3.2 Attribution — working reference

| Ir/s | share | site |
|---|---|---|
| 34,992,000 | **41.7 %** | `LR4Xover.h` inside `MultibandWidth::processBlock` (the two bank lambdas) |
| 13,340,625 | **15.9 %** | `VelvetNoise::processBlock` |
| 6,476,250 | 7.7 % | `MultibandWidth.cpp` itself |
| 6,251,250 | 7.5 % | `LevelMeters.h` (`StereoLevel::process` ×2) |
| 6,710,250 | 8.0 % | `LoudnessMatch` (`.h` 6.57 % + `.cpp` 1.42 %) |
| 4,249,264 | 5.1 % | `AnamorphEngine.cpp` inline |
| 3,939,000 | 4.7 % | `LR4Xover.h` in the engine (MonoMaker + SoloMonitor) |
| 1,920,000 | 2.3 % | `MidSide.h` inside the multiband |

Multiband as a whole = **44.0 M Ir/s = 52.4 %** of the working reference (confirmed independently by
the `working` − `mb-off` difference).

### 3.3 Attribution — transparent defaults (the idle floor)

| Ir/s | share | site |
|---|---|---|
| 6,251,250 | **28.6 %** | `LevelMeters.h` |
| 6,710,250 | **30.7 %** | `LoudnessMatch` |
| 2,753,390 | 12.6 % | `AnamorphEngine.cpp` inline (NaN scan, bypass ring fill, publish) |
| 2,123,250 | 9.7 % | `VelvetNoise::processBlock` (parked) |
| 834,375 | 3.8 % | `Correlation.h` |
| 682,785 | 3.1 % | scope/ring `memcpy` |

**59.3 % of the transparent idle floor is metering and loudness analysis, not audio processing** —
and `defaults` vs `defaults` with `autoGainMatch = false` measured **byte-identical totals**,
confirming `loudness.process()` is unconditional exactly as `WAVE3_INVESTIGATION.md:113` records.

---

## 4. Findings

### 4.1 A previously-closed question, reopened: per-block fixed cost is dominated by one item

Wave 5 attributed small-buffer per-block cost and **closed** it:
*"+13–20 %… ~4.5k Ir of fixed work per block… **no single dominant item**"* (`WAVE5:27-33`), and the
roadmap carried that closure forward.

Solving `cost(b) = A·48000 + B·(48000/b)` from the 32- and 256-sample runs:

| Configuration | fixed per block (B) | marginal (A) |
|---|---|---|
| transparent defaults | **3,581 Ir** | 464.9 Ir/sample |
| working (algorithm engaged) | **24,287 Ir** | 1558.6 Ir/sample |

Wave 5's figure reproduces **exactly — for the transparent default**. In the configuration a paying
user runs it is **6.8× larger**, and **20,369 Ir of it (84 %) is `VelvetNoise::processBlock` alone**.

The reason the earlier closure missed it is mechanical, not careless: the H5 tap-outer gather is
gated on `currentAmount > 0.0f || targetAmount > 0.0f` (`src/dsp/VelvetNoise.cpp:132-135`), and the
default `algoAmount` is 0. **Measuring per-block cost at the transparent default measures the
configuration in which the dominant per-block item is switched off.**

### 4.2 The mechanism

The H5 fast path rebuilds its linear history image from the ring on **every block**:

```cpp
for (int j = 0; j < decorrSamps; ++j)
    linHist[j] = midHist[(writePos - decorrSamps + j) & histMask];   // VelvetNoise.cpp:139-140
```

`decorrSamps = round(0.045 × sr)` (`VelvetNoise.cpp:26`) — **2160 samples at 48 kHz, 8640 at
192 kHz** — and the loop is independent of `numSamples`. Measured ≈ 9.4 Ir per history sample.
It therefore scales with the sample rate and is divided by the block length:

| SR | block | engine Ir/sample | VelvetNoise | share |
|---|---|---|---|---|
| 48 kHz | 32 | 2317.6 | 755.3 | 32.6 % |
| 48 kHz | 128 | 1749.1 | 277.9 | 15.9 % |
| 192 kHz | 32 | 4140.2 | **2577.9** | **62.3 %** |
| 192 kHz | 128 | 2204.8 | 733.6 | 33.3 % |

Solved per configuration: the fixed term is 20,369 Ir/block at 48 kHz and 78,690 at 192 kHz (3.86×
for a 4× rate, as the mechanism predicts), while the marginal term is **118.8 Ir/sample at both** —
the signature of a cost that belongs entirely to the per-block refill.

**This is the optimization that Wave 2's H5 paid for and nobody re-measured at small buffers.** H5
is still the right structure: it turned 64 random ring reads per sample into unit-stride runs, and
that win is real at 128+ samples. What it also introduced is a per-block term that overtakes the win
as the block shrinks and the sample rate rises.

### 4.3 Bypass costs marginally more than processing — and that is deliberate

`bypass` measured **85.1 M Ir/s against the working reference's 84.0 M**. A host-bypassed instance
costs slightly *more* than an active one, because the entire chain still runs and the bypass ring
read-back is added on top (`AnamorphEngine.cpp` inline rises 4.25 M → 5.51 M Ir/s).

This is not an oversight. `AnamorphEngine.cpp:790-796` states the contract: *"The processing AND the
Level-Match analysis ALWAYS run below — Bypass only changes the audio output path, never the
analysis path, so Measure + Predict keep running while bypassed (Issue 2)."* `loudness.process()` is
handed the **processed** `L, R` (`:1137`), so the chain cannot be skipped without freezing the
Measure readout — the exact live contract `W3-7` was rejected for breaking.

`pid::bypass` is registered as the **host bypass parameter** (`PluginParameters.cpp:271-272`,
`PluginProcessor.cpp:13-14`), so this applies to every bypassed instance in a session. Quantified
here for the first time; the disposition is a product question, not an engineering one (§9).

### 4.4 GUI paint: the cost is JUCE's rasterizer, not Anamorph's drawing

Headless full repaints into an offscreen image (940×720 editor, 68 direct children):

| Target | size | Ir per repaint | vs one second of DSP |
|---|---|---|---|
| whole editor | 940×720 | 28,613,806 | 0.34× |
| Vectorscope | 574×574 | **10,716,856** | 0.13× |
| StereoMeter (correlation) | 574×26 | 396,806 | 0.005× |

Inside the Vectorscope repaint, `Vectorscope::paint` itself is **2.9 %**; 43.3 % is
`ImageFill<PixelARGB, PixelRGB>::handleEdgeTableLine` — the **static-layer blit** — and 20.6 % is the
point cloud's solid-colour fills. The H2/N2 caches are doing exactly what they were built to do; what
remains is JUCE's software renderer converting RGB→ARGB at ~14 Ir/pixel.

**Three limits, stated rather than implied.** This is the **Linux software renderer**; macOS
(CoreGraphics) and Windows (Direct2D) rasterize differently, so the absolute numbers do not
transfer. `paintEntireComponent` forces a full repaint, which is the resize/first-paint case, not
the steady state — JUCE repaints dirty regions. And the `juce::Timer` / `VBlankAttachment` tick path,
where the Wave 1–4 idle gates live, needs a message loop and was **not** measured here; the
budget document's "idle GUI cost ≈ 0" claim is therefore neither confirmed nor challenged by this
round.

`LevelMeter` and `SpectrumImager` measured **0×0**: they are Advanced-mode components and are not
laid out in the default view, which is consistent with the S2/S3 hidden-component gates.

### 4.5 RISK-002: the multiband split drag is not a dropout-class transient

The drag costs **+11.2 %** over the static 4-band state (93.3 M vs 83.9 M Ir/s), of which
`__tan_fma` is 3.26 % — the per-sample coefficient recompute Wave 3 already cut from 12 tan/sample
to 3. The committed bench's wall-clock worst-single-block for the drag row (142.4 µs) came in
*below* the static row's (181.0 µs); given the spread on this machine neither number is a datum, but
nothing in either instrument suggests a transient cliff.

**RISK-002 is not closed by this round** and must not be recorded as closed: closing it needs the
wall-clock instance count on a named, held-still machine (§7, A7-0).

### 4.6 Oversampling ×8: 41 % of the cost is JUCE's resampling filters

At Drive 8 dB / OS ×8, `juce::dsp::Oversampling2TimesPolyphaseIIR` up + down = **85.4 M Ir/s, 40.9 %**
of that scenario — more than the nonlinear work it exists to enable (`processNonlinearRegion`
+ its maths inline = 34.6 M, 16.6 %). The engine's own gate (oversampling runs only when nonlinear
work exists) is what keeps this off the common path.

---

## 5. Candidate evaluation

Every candidate is judged on: is the cost real and measured · what is the expected gain · Class A or
B · does it touch a hard-stop class · implementation and maintenance cost · **and whether the
roadmap table already disposed of it**.

| ID | Candidate | Gain | Class | Risk | Verdict |
|---|---|---|---|---|---|
| **A7-1** | Slide the H5 linear history instead of re-gathering it from the ring every block | **measured −14.3 % at 48 k/32, −32.3 % at 192 k/32, −4.7 % at 48 k/128** | **A (proven)** | low | **Optimize now** |
| A7-2 | Remove the residual per-block term entirely (ring-free double-buffer history) | ~45 % of what A7-1 leaves; unmeasured | A (expected) | medium | **Consider later** |
| A7-3 | Reduce `decorrSamps`-proportional work at high sample rates generally | subsumed by A7-1/A7-2 | — | — | **Do not optimize separately** |
| A7-4 | Skip the DSP chain when Bypass is settled at 1 | ~82 % of a bypassed instance (bounded, not measured) | A for audio, **breaks a live readout contract** | high | **Do not optimize** — maintainer decision (§9) |
| A7-5 | Multiband LR4 bank SIMD | unmeasured | A only without FMA | high | **Consider later — blocked on the AVX2 ADR** |
| A7-6 | Vectorscope static-layer blit format | unmeasured off-Linux | A if pixel-identical | medium | **Do not optimize** on this evidence |
| A7-7 | Replace JUCE's polyphase oversampler | unmeasured | **B** (different filter = different audio) | high | **Do not optimize** |
| A7-8 | Gate meters / loudness on editor state | 59.3 % of the idle floor (measured) | — | — | **Already rejected** (W3-7, W3-8); §9 |

### 5.1 A7-1 — why it is the whole recommendation

The retained tail of `linHist` is, by construction, the previous block's `linHist` shifted left by
that block's length: the ring is written with exactly the mids the image already holds. So the
`decorrSamps`-long masked ring walk can be a `memmove` of the same floats, guarded by a validity
flag that the non-fast paths and `reset()` clear.

**Implemented in a throwaway build and measured**, because a bit-exactness claim has to be earned:

| SR | block | before | after | saved | engine |
|---|---|---|---|---|---|
| 48 kHz | 32 | 2356.0 | 2019.6 | 336.5 | **−14.3 %** |
| 48 kHz | 64 | 1975.4 | 1807.2 | 168.2 | −8.5 % |
| 48 kHz | 128 | 1787.3 | 1703.1 | 84.1 | −4.7 % |
| 48 kHz | 256 | 1691.5 | 1649.5 | 42.1 | −2.5 % |
| 192 kHz | 32 | 4178.7 | 2829.7 | 1349.0 | **−32.3 %** |
| 192 kHz | 128 | 2242.9 | 1905.7 | 337.2 | −15.0 % |

**Bit-exact across 144 configurations** — 9 scenarios (working, mb-off, Haas, Chorus, Dim-D, OS ×8,
defaults, bypass, multiband drag) × 4 block sizes × 4 sample rates, compared by an FNV-1a hash over
**every output sample of both channels**. Zero mismatches. Class A by measurement, not by argument.

Cost: ~20 lines in one file, two new members, one invalidation point in `reset()`. It touches no
parameter ID, no schema, no thread model, no signal order and no reported latency — **no hard-stop
class**. The H5 eligibility comment (`VelvetNoise.cpp:99-123`) gains a paragraph; nothing in it is
invalidated.

**The strongest case against**, stated because it is real: it adds a cross-block cache-validity
invariant to a module whose fast path already has a five-clause eligibility gate. A future edit that
takes the fast path without maintaining the flag would read a stale history — silently, and only in
the first block after the mistake. The mitigation is that the flag is cleared by a scope guard on
every non-fast-path exit rather than by hand at each return, and that a regression test can assert
bit-equality between a run at block 32 and the same audio at block 256.

### 5.2 Why the biggest number is not the recommendation

Multiband is **41.7 %** of the working reference and the obvious target. It is not proposed, for
reasons that are all on the record:

* Wave 2 (H6) already replaced JUCE's filter with a flat-state clone; Wave 3 already cut the glide
  from 12 tan/sample to 3 and halved the allpass compensation. The structure left is a **serial TPT
  recurrence** — `s1→s2→s3→s4` per sample — which cannot be vectorized across samples at all.
* Vectorizing across *banks* is the W5-D shape, already prototyped and measured at **1.10×** under
  the frozen SSE2 flags, with the real win requiring `-march=haswell`+ — *"a numerics-frozen
  build-contract change (own ADR + Architecture Review)"* that *"would itself introduce FMA, breaking
  the very bit-exactness the prototype relies on"*.
* The roadmap already files LR4 SIMD as **"Defer … Only worth opening alongside the AVX2 decision"**.

Proposing it again without an AVX2 decision would be re-litigating a settled question.

---

## 6. What was built to validate, and where it lives

### 6.1 Nothing in this round is in the product tree

The A7-1 implementation exists only as a scratch build outside the repository. It is described in
§5.1 in enough detail to be re-derived, and the numbers above are what it measured. **The change
itself is a separate, reviewable PR** — this round's deliverable is the decision, not the diff.

### 6.2 The committed bench is fit for purpose and was exercised

`AnamorphBench` built clean under Release + LTO and produced the full matrix. Its own honesty
mechanism worked: it refuses to print a table it cannot label. The per-cell wall-clock is recorded
in this round's HTML report for reference and is **not** promoted into the budget document.

---

## 7. Prioritized roadmap

Order is by (measured gain × confidence) ÷ (risk × cost). Dependencies are explicit.

### A7-0 — Close the `PERFORMANCE_BUDGET.md` numeric rows (no code)
**Priority: first, and it blocks nothing.** The harness exists, the procedure is written, and this
round proves both work end to end. What is missing is a machine: run `AnamorphBench` on a named,
otherwise-idle desktop and paste the matrix in with CPU/OS/compiler beside it. That answers RISK-002's
question ("how many instances before a core") and retires three TODOs.
*Depends on:* a maintainer with a desktop. *Outcome:* RISK-002 answerable; every later claim gets a
wall-clock reference point.
*Do **not*** populate those rows from this round's container numbers.

### A7-1 — The VelvetNoise linear-history slide
**Priority: highest code item.** Largest measured gain, Class A proven over 144 configurations,
~20 lines, one file, no hard-stop class, no policy conflict.
*Depends on:* nothing.
*Outcome:* small-buffer and high-sample-rate users get **−14 % to −32 %** of whole-engine cost; the
128-sample common case gets −4.7 %. Per-block fixed cost falls from 24,287 Ir to roughly 12,000.
*Gate:* the twin-dump bit-equality evidence must be regenerated against the real tree and a
block-size-invariance test added, or the Class-A claim is only this round's word.

### A7-2 — Remove the residual per-block term
**Priority: after A7-1, and only if A7-0's numbers say small buffers still hurt.** A7-1 leaves a
`memmove` of `decorrSamps` floats per block. A history kept as a linear double-buffer would remove it
outright, but it restructures the module's storage rather than adding a fast path.
*Depends on:* A7-1 landing first (it establishes the validity-flag invariant A7-2 would build on);
A7-0 for the evidence that the remainder matters.
*Outcome:* the remaining ~45 % of the original per-block term. Unmeasured — size it before scheduling.

### A7-5 — Multiband LR4 SIMD
**Priority: last, and not on its own.** The largest remaining consumer, and structurally blocked.
*Depends on:* an **AVX2 / `-march` ADR + Architecture Review** (hard stop: build-contract change,
FMA divergence), taken jointly with W5-D so one decision serves both.
*Outcome:* unknown until the ADR fixes the target ISA. Do not open before it.

### Not scheduled
A7-3 (subsumed), A7-4 and A7-8 (§9 — product decisions, not engineering), A7-6 (needs per-platform
measurement first), A7-7 (Class B, replaces a shipped filter).

---

## 8. Prior dispositions this round re-confirms

Checked against the code and left alone. None of these is reopened:

* **W3-7 / W3-8** (gate LoudnessMatch / LevelMeters) — the contracts they protect are still in the
  code (`AnamorphEngine.cpp:1137` passes the processed signal; held peaks persist by spec). This
  round adds only the price tag (§9).
* **W3-9** (freeze the Velvet presence env while parked) — still load-bearing; A7-1 deliberately does
  not touch the env/gate or the history writes.
* **W3-10** (Width==1 identity gate) — still Class B (15.5 % of samples differ by ~1 ULP).
* **W3-12 residue** (bypass read-back segmentation) — the overlap hazard is unchanged.
* **W5-4 / W5-8** — thread-model and change-tracking hazards unchanged.
* **W5-D** — the SSE2 ceiling is unchanged; nothing this round found lowers it.
* **All Wave-6 GPU rejects** — this round measured the *software* renderer only and produces no
  evidence that reopens any of them.
* **LookAndFeel per-paint locals** — still gesture-transient; the paint profile confirms Anamorph's
  own drawing code is a rounding error next to JUCE's rasterizer.

**W5-A** (`lat==0` mix-ring round-trip) is filed as *"Revisit only on evidence of real small-buffer
host pain"*. This round produces small-buffer evidence — but it points at VelvetNoise, not at the
mix ring. W5-A stays deferred on its own terms.

---

## 9. Decisions that are the maintainer's, not engineering's

Both are quantified here for the first time. Neither has an engineering answer.

1. **A bypassed instance costs 101 % of an active one** (§4.3). The cause is the documented Issue-2
   contract that Measure + Predict keep running while bypassed. The question is whether a live
   Measure readout *while the plug-in is bypassed* is worth a bypassed instance costing a full one
   in a large session. Changing it is a live-contract change and a consolidated Architecture Review
   item — the same queue as W3-7.

2. **59.3 % of the transparent idle floor is metering and loudness analysis** (§3.3), running
   regardless of Level Match being off and regardless of whether an editor exists. W3-7 and W3-8
   rejected gating these for good reasons — a live Measure readout and peaks that persist while the
   editor is closed — and those reasons stand. What was missing was the price. The roadmap already
   names the right queue position: *"one consolidated Review if idle-CPU ever matters
   commercially."* This round supplies the number that would make that call decidable.

---

## 10. Limits of this round

* **No wall-clock datum.** Shared container; every ns figure here is indicative and none may be
  promoted into `PERFORMANCE_BUDGET.md`.
* **The GUI tick path was not measured** — only paint. The idle-GUI claims from Waves 1–4 are
  neither confirmed nor challenged.
* **Software renderer only.** No GPU, and no macOS/Windows rasterizer.
* **A7-1's Class-A evidence is this round's**, generated by a scratch build. It must be regenerated
  against the product tree before the claim enters the repository.
* **One scenario shape.** The `working` reference is `bench.cpp`'s; a user on Chorus with multiband
  off has a different profile (§3.1 gives the spread).
