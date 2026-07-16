# ADR-0015 — Split-movement transitions: zero-latency LR4 retained, bounded rate-capped follower

**Status:** Accepted

## Context

Moving a Multiband split (or a whole band via its Solo handle) modulates a **minimum-phase IIR
crossover**, and the v0.8.10 pre-merge testing (PR #59) established, by measurement, that this is
inherently artifact-bound: the LR4 reconstruction carries a fixed 2π of allpass phase per split at
every width setting, so a cutoff sweeping at `R` oct/s shifts every frequency near it by
`0.312·R` Hz (`dφ/dt / 2π`). No transition scheme removes that energy — it only redistributes it
between pitch shift, sidebands, magnitude dips, and response lag. All measurements below used a
pure-sine protocol (instantaneous frequency of the fundamental via narrowband demodulation, max
spur outside ±30 Hz of the tone, wideband envelope) on a bit-exact model of `LR4Xover`, at drag
speeds 1–24 oct/s with 60 Hz UI-cadence targets.

## Problem

Deliver split movement that a user perceives as responsive and artifact-free, under the binding
product constraints: zero added latency, LR4 crossover behaviour preserved, MultibandWidth's
pure-side-gain width semantics preserved (widths must stay phase-free gain ramps — "width purity",
0.7.0 #1), flat recombination preserved, SoloMonitor unchanged, automation behaviour unchanged.

## Options (the full investigation history — preserve; do not re-run)

- **A. Immediate / fast-tracking coefficient movement** (one-pole τ≈15 ms). Rejected: FM at the
  full drag rate — +23…+66 cents measured at 16–24 oct/s; ~50 cents at a fast crossing. This was
  briefly shipped as the second design and failed user testing ("the sine fundamental moves").
- **B. Block-boundary parameter commit.** Already the case (targets are read per block); it is
  orthogonal to the artifact and solves nothing by itself.
- **C. Fixed control-rate updates (30–120 Hz).** Rejected: identical FM plus stepping spurs at the
  control cadence (raw steps zipper; smoothed steps degenerate to A).
- **D. GUI/DSP decoupling with background preparation + safe swap.** The "safe swap" must itself be
  a crossfade, so this collapses into E.
- **E. Dual filter-bank crossfading** (ping-pong between complete state-copied banks). Rejected for
  continuous movement: a crossfade between two phase-different allpasses is amplitude/phase
  modulation — chained ~12 ms fades sprayed −25…−28 dBc sidebands around a pure tone (the first
  shipped design, failed user testing: "frequencies that should not exist"); consolidated 1.5–2.5
  oct fades land in the worst phase-delta zone (Δφ ≈ π: −6…−11 dB dips, ±100+ cent demodulation
  glitches). The LR4 phase transition spans ~6 octaves, so the mod-2π phase wrap that makes big
  fades cheap never engages at drag-sized deltas. **Retained for genuinely discrete jumps** (target
  steps > 1.5 oct in one block), where it measurably beats a glide (−18 dBc vs −4.7 dBc at 4 oct).
- **F. Deferred DSP commit (apply on mouse release / timeout).** Rejected on UX: no audible
  feedback while dragging.
- **G. Modulation-safe topology (SVF/TPT).** Rejected as a misdiagnosis: `LR4Xover` already *is* a
  TPT (trapezoidal) ladder — the JUCE `LinkwitzRileyFilter` clone. TPT/SVF structures solve
  modulation *stability* artifacts, not transfer-function-motion FM; the measured FM persists on
  our TPT ladder exactly as theory predicts, and any minimum-phase complementary crossover has the
  same 2π-per-crossing phase trajectory.
- **H1. Fixed-carrier decomposition** (whole-signal allpass moves slowly; width-difference bands
  track fast). Rejected by measurement: the difference terms compose correctly only against a
  carrier at the same cutoffs — while the carrier lags, band-kill leaks (+79…+109 dB relative to
  the ideal below the split) or notches (−35…−39 dB).
- **H3. Gain-domain reformulation** (Mid untouched; Side through a shelf cascade realizing the
  width curve — "dynamic-phase" style, cf. FabFilter Pro-MB's publicly documented mode). Rejected
  by hostile review, decisively: shelf phase moves **with gain**, so *width changes* become phase
  modulation (+10 cents measured for a −6 dB ramp at today's 20 ms smoothing; clean fast width
  automation impossible at any smoothing constant) — breaking width purity, the product's core
  guarantee; the transition-shape fit collapses at strong widths (−27…+43 dB vs the current
  curves); W = 0 loses the exact full-mono guarantee or reintroduces crossover-class phase
  (−78 cents measured at a −40 dB shelf); and the settled sound changes in every session
  (W=1 output A(x) → x). See PR #59 discussion for the full hostile-review record.
- **H2. Linear-phase (FIR) crossovers.** The only architecture that is simultaneously instant and
  artifact-free for split *and* width movement (a moving linear-phase split at unit width is
  exactly a delay; width ramps remain pure gains). **Deferred, not rejected**: it adds ~30–80 ms of
  reported latency (an `ARCHITECTURE_REVIEW_GATE` Hard Stop), significant CPU, and PDC/dry-
  alignment rework. Recorded as the future roadmap direction (an opt-in "linear phase" mode, the
  documented industry pattern).

## Decision (v0.8.10)

Keep the zero-latency LR4 architecture and transition via a **bounded rate-capped follower**:

1. **Continuous movement**: per-sample multiplicative glide under a hard **~1.25 oct/s** cap —
   shift bounded at ~0.39 Hz = 4.5 cents at a 150 Hz crossing (below the pure-tone JND and the
   5-cent regression bound), spurs at the analysis floor, direction-symmetric by construction.
   1.25 also leaves closing margin over typical slow manual drags (≤ 1 oct/s), so a gap formed by
   an earlier flick drains *during* continued slow dragging instead of freezing (the "stuck
   follower" defect of a cap ≈ drag speed).
2. **Discrete jumps** (target steps > 1.5 oct between consecutive blocks): one ~12 ms crossfade to
   the state-copied idle bank.
3. **Release consolidation** (the follower-refinement of this ADR): when the targets have been
   quiet for ≥ 0.25 s and the residual lag still exceeds 1.5 oct, the residue is landed as one
   discrete jump. Convergence after any gesture is therefore **bounded**: ≤ ~0.26 s for large
   residues, ≤ 1.5 oct / 1.25 oct/s = 1.2 s for crawled ones — never the unbounded distance-
   proportional catch-up of a pure rate limiter (a 6-octave flick previously took ~5.7 s).

Follower trajectories are deterministic and closed-form: lag grows at `(v − R)` while the drag
outruns the cap, drains at `(R − v)` otherwise, and resolves in ≤ max(0.26 s, lag/R) after release,
with lag > 1.5 oct short-circuited by consolidation. Manual drags and host automation share the
identical path (parameter → per-block target → follower); block size does not affect the
per-sample follower; exactly one smoothing stage exists (verified: no GUI/parameter smoothing
upstream).

## Consequences

- The audible crossover position can trail a fast drag by design (KI-012), but always lands within
  ~1.2 s worst-case (typically ≤ 0.5 s), with at most one bounded ~12 ms fade event per gesture.
- The pure-sine protocol is enforced by regression (`testMultibandSplitDragNoPitchShift`, Test 29):
  < 5 cents worst 100 ms chunk through drags, crawls, and the tone crossing; spur < −31 dBc at a
  60 Hz drag cadence; discrete jumps and released drags land fast and click-free.
- Any future attempt to make fast tracking artifact-free must change the crossover class
  (linear-phase, H2) — a reported-latency change requiring a new ADR + Architecture Review.

Evidence [Verified]: src/dsp/MultibandWidth.{h,cpp}, src/dsp/SoloMonitor.{h,cpp} (design comments
carry the per-mechanism measurements); tests/dsp_tests.cpp Test 29; CHANGELOG [0.8.10]; PR #59
review thread (architecture comparison + H3 hostile review, with sources).
