# v0.8.11 Final Performance Pass & Release-Readiness Audit (worklog)

> The last engineering pass before the v0.8.11 release. Deliberately **not** an
> open-ended optimisation phase: it closes the three remaining named candidates
> (GUI fresh-eyes sweep, W3-10 Width==1, W5-D K-weighting SIMD) with measured
> verdicts and audits release readiness. Numbers are session-local (constraint
> C2). Read `WAVE5_INVESTIGATION.md` first — this pass evaluates its handover
> list. **Outcome: zero code changes — every candidate is already-done, Class-B,
> or low-benefit-behind-a-frozen-flag. The correct release-readiness result is a
> documented "no safe win remains," not a manufactured change.**

- **Date:** 2026-07-20
- **Branch:** `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `4aac4eb`
  (PR #76 = Waves 4+5, now **merged**; the branch fast-forwards onto it and
  carries no unmerged history, so this pass is a fresh change — a new PR, per
  the merged-PR handover rule).
- **Environment / method:** as Waves 4–5 (same container, gcc 13.3.0, Release
  flags `-O3 -DNDEBUG`, **no `-march`** → baseline x86-64, SSE2, no FMA). The
  Release flag set is numerics-frozen by policy (CMakeLists:62; `-O3`/LTO/
  `-ffast-math`/`-march` are build-contract changes needing an ADR + Review).

## 1. GUI fresh-eyes sweep — COMPLETE (no new findings)

The sweep that was lost to the org token limit in Waves 4 and 5 was attempted a
third time via a two-lens Workflow (paint path + message thread) and **again**
lost both lenses to the org spend limit. It was then carried **in-line** against
source, reading every paint()/tick() consumer in full:

| Surface | State found |
|---|---|
| `Vectorscope`, `CorrelationMeter`/`StereoMeter`, `LevelMeter` | Opaque static-layer cache (N2/H2/H13) + `isShowing()` tick gate + bitwise "drawn value changed?" repaint gate. Nothing left. |
| `SpectrumImager` | S2 `isShowing()` gate, `magsSettled`/`redSettled` settle flags, `magsDb` per-transform dB cache, reused `Path` storage, snap-to-zero tails. Nothing left. |
| `PluginEditor::timerCallback` (24 Hz) | Every unconditional job memoised: advanced/meters/tooltips/ms toggles compare-gated, preset display keyed on (name,dirty,width), combo-hover behind the S11 cursor-inside pre-gate, match readout bitwise-gated. |
| `PluginEditor::stepMicroAnims` (per vblank) | H15 generation pre-gate (sound/view/InternalState counters) → FNV fingerprint → early-out; the 44-widget poll runs only when something provably moved. |

- **Only residual identified:** the `LookAndFeel` rotary/linear-slider draws
  construct `juce::Path`/`juce::Font` locals per call (LookAndFeel.cpp:55/66/108/
  206/537/607). Cost is real but **transient** — it occurs only while a control
  actually repaints (a hover or a preset/A-B/undo *sweep*, ≤ ~0.45 s), never in
  steady state (the repaint gates above hold the knobs still when idle). The
  `LookAndFeel` is a single shared, stateless instance; caching Paths there would
  need per-component keying or scratch and adds real complexity/risk for a saving
  visible only during active gestures. **Rejected** — wrong trade for a low-risk
  release pass.
- **Verdict:** the GUI paint + message-thread surface is exhaustively optimised
  across Waves 1–4; the long-open fresh-eyes sweep is now **closed with no
  actionable Class-A finding.**

## 2. W3-10 Width==1 gate — DEFER (Class B, empirically confirmed)

Candidate: when the global Width smoother is settled at exactly `1.0`, skip
`applyWidth` entirely (mathematical identity) instead of running the (already
W5-C-hoisted, vectorised) `applyWidth(L,R,1.0f)` kernel per sample.

- **Bit-exactness — measured, not assumed.** `applyWidth(L,R,1.0f)` is
  `mid=(L+R)*0.5; side=(L-R)*0.5*1; L=mid+side; R=mid-side` — a mid/side
  round-trip whose two rounded sums do **not** reconstruct the input bit-for-bit.
  A 50 M random-sample probe: **7 767 363 / 50 000 000 samples (15.5 %) differ**
  from identity, max abs 5.96e-08 (~1 ULP). Skipping the kernel would therefore
  **change the DSP output bits → Class B.**
- **Why defer:** the entire Wave 1–5 program held a strict **Class-A (bit-exact)**
  standard. Introducing a bit-changing DSP output change on the eve of a release,
  to save ~9 flops/sample **only** when Width is settled at exactly 1.0 (and W5-C
  already hoisted+vectorised that loop, so the residual is small), is precisely
  the release risk the pass is meant to avoid ("No approximate DSP"; "Only
  implement if evidence supports it"). Evidence does not support it. Unchanged
  Class-B deferral, now with a hard number attached.

## 3. W5-D K-weighting 4-lane bank — DEFER (prototyped: bit-exact but 1.10×)

Candidate: the four K-weighting chains (`kDryL/R`, `kWetL/R`) are independent
2-biquad cascades that **share identical coefficients** (all four get the same
`setSampleRate(sr)`), differing only in state + input — a clean lane-parallel
(SLP) target. Prototyped in `scratchpad/kwbench.cpp`: the exact scalar 4-chain
path vs a lane-parallel array bank (shared scalar coeffs, per-lane state), 20 M
samples × 4 lanes, at the project's Release flags.

| Metric | Result |
|---|---|
| Bitwise mismatches (scalar vs bank) | **0 / 80 000 000**, maxabs 0.0 |
| Kernel speedup | **1.10×** |

- **Why only 1.10×:** at baseline x86-64 (no `-march`) the compiler can pack at
  most **2 doubles** (SSE2 `__m128d`), not 4 — so the "4-lane" restructure buys a
  2-wide fold, not a 4-wide one. The big win the Wave-5 worklog imagined needs
  AVX2 (`-march=haswell`+) for 4-wide doubles, and enabling `-march`/FMA is a
  **numerics-frozen build-contract change** (Hard-Stop; own ADR + Review) — and
  would itself introduce FMA, breaking the very bit-exactness the prototype relies
  on. So the reachable benefit here is a 1.10× fold on the K-weighting kernel.
- **Sizing the impact:** K-weighting is the LoudnessMatch inner loop, ≈ the
  worklog's "~6–12 % of the transparent floor." A 1.10× fold on that band is
  ≈ **0.5–1 % of floor** overall — for a **medium restructure of a contractual
  DSP loop** whose cross-platform bit-exactness is proven only at baseline SSE2
  (an AVX/ARM/`-march=native` build would take the FMA path and could diverge).
- **Also confirmed not-dead-work:** `loudness.process()` runs **every block even
  when Auto-Gain-Match is off** by design — it feeds the always-live match-gain
  readout so engaging Match never lurches (feedback #16/#23) and stays warm for
  automation-driven engage regardless of editor state. Gating it on
  `autoGainMatch` (or editor-open) would break the readout and couple the audio
  path to GUI state (a realtime-guarantee violation). Contractual always-on.
- **Verdict:** **DEFER** to a dedicated post-release pass, now backed by a real
  measurement (bit-exact ✓, 1.10× at frozen flags, ~0.5–1 % floor). Not worth a
  contractual-loop restructure on release eve.

## 4. Release-readiness audit

| Area | Finding |
|---|---|
| **Build** | Release build green; `AnamorphTests` green. |
| **Tests** | **140 checks, 0 failures** (Tests 1–34 + the A/B state-restoration clamp guard). Matches the docs' "33 self-tests + guard / 140 checks" convention — **no test-count drift.** |
| **CI (main)** | `Build & Validate` + `CodeQL Advanced` were **green** on `912a755` (parent); the merge-commit `4aac4eb` re-run was in flight at audit time (`Microsoft C++ Code Analysis` already green on it). No failing gate observed. |
| **Version drift** | `CMakeLists.txt` 0.8.11, README 0.8.11, CHANGELOG `[0.8.11] — 2026-07-20`, HANDOVER 0.8.11 — consistent. |
| **CHANGELOG** | This pass makes **no user-visible change**, so per CHANGELOG_POLICY rule 3 it gets **no entry** and no new version. The consolidated `[0.8.11] — 2026-07-20` already covers all shipped work. Deliberate, not an omission. |
| **Known limitations** | KI-001…012 all Low/Medium, none release-blocking (KI-009 REAPER focus host-specific; KI-011 tooltip-corner fix pending a hardware re-test; KI-002 macOS not notarized). RISK-003: no git release tags. macOS ad-hoc signed only. |
| **Human sign-off** | The Level-5 manual audition remains the human gate per RELEASE_POLICY — the one open non-code item before shipping. |

**Release blockers: none.** Non-blocking follow-ups: KI-011 hardware re-test,
git release tags (RISK-003), macOS notarization (KI-002, gated on the RELEASE_
HARDENING_PLAN business decisions). Release risks introduced by this pass: **none**
(no code change).

## 5. Handover — remaining performance candidates (post-release)

Unchanged priority, now all evidence-backed:

1. **W5-A** — lat==0 mix-ring round-trip elimination (highest blast radius; the
   fix sketch + overlap caveat are in the Wave-5 Workflow findings journal).
2. **W5-D** — K-weighting lane-parallel bank, but only worthwhile **coupled to an
   AVX2 build decision** (own ADR): 1.10× at baseline SSE2, ~4-wide only with
   `-march`. Prototype (`scratchpad/kwbench.cpp`) proves bit-exactness at current
   flags; FMA divergence must be re-checked per target arch.
3. **W3-10** — Class-B width==1 identity skip; only if the program ever adopts a
   Class-B tier for the width path (15.5 % of samples differ by ~1 ULP).
4. Category-C gates (H7 + a LevelMeters gate) still await the maintainer's
   Architecture Review; then Multiband LR4 SIMD, then OSD-2 (OS-only).

The transparent floor is unchanged from the Wave-5 record (LevelMeters ~22 %,
LoudnessMatch ~24 %, VelvetNoise parked ~7–8 %, engine inline ~11 %). No cheap
Class-A item remains; the next real wins need either an Architecture Review or an
AVX build decision.
