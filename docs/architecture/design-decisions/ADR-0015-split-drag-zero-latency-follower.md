# ADR-0015 — Split-movement transitions: zero-latency LR4 retained, rate-capped follower

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

## Decision (v0.8.10, third design — evaluated and refined; see "v0.8.10 final decision" below)

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

## v0.8.10 final decision (refinement — direct interaction over inaudibility; rate law refined again in the slow-drag fix below)

The bounded follower above was evaluated in interactive testing and its **latency was rejected**
as a UX regression: at 1.25 oct/s the audible crossover lagged every ordinary fast drag (a
500 Hz → 2 kHz / 0.5 s drag released with 1.37 oct of lag and glided on for another second), and
the release consolidation — a 0.25 s "wait until the user stopped" heuristic — read as a sudden
delayed jump after the hand had already stopped. The original product intent was never "make split
movement inaudible on a pure sine"; it was "slightly reduce the FM of direct manipulation". The
governing trade is therefore restated: **a small amount of controlled FM is preferable to obvious
interaction latency.**

All architecture-investigation results above remain valid (the physics, the A–H matrix, the H3
hostile review, linear-phase as the future roadmap); the zero-latency LR4 architecture is
retained. Only the follower's operating point changes, and the mechanism gets *simpler*:

1. **Continuous movement**: the same per-sample multiplicative glide, cap raised to a hard
   **~4 oct/s**. Every human drag up to 4 oct/s tracks **exactly** — zero GUI/DSP gap (Case A
   500 Hz → 2 kHz / 5 s and Case B 500 Hz → 2 kHz / 0.5 s both measure 0.00 oct lag, 0.00 s
   settle) — and the crossover feels attached to the mouse. Faster movement bounds the shift at
   `0.312·4 ≈ 1.25` Hz: **~15 cents at a 150 Hz crossing, ~2 cents at 1 kHz** (measured 15.9c /
   2.4c), spurs still at the −41 dBc analysis floor, < 0.1 dB envelope ripple — roughly **half**
   the original uncapped implementation's worst case (+31c / +4.7c at its ~8 oct/s), which is the
   "slight artifact reduction" originally asked for. Even a violent 6-octave flick (Case C) drains
   in ~1.25 s of *continuous* motion after release — no several-second catch-up.
2. **Discrete jumps** (target steps > 1.5 oct between consecutive blocks — automation steps and
   preset snaps, Case D; unreachable by dragging at UI cadence): unchanged, one ~12 ms crossfade
   to the state-copied idle bank. This is the only special handling that is actually needed.
3. **Release consolidation: removed entirely** (quiet detector, 0.25 s timeout, residue fade).
   With the 4 oct/s cap the residue a drag can accumulate is small and short-lived, so the
   consolidation's one job is gone — and its cost (the delayed post-release jump) was the worst
   part of the UX regression. No timers, no intent prediction, no deferred commits remain.

Follower trajectories stay deterministic and closed-form: lag grows at `(v − 4)` oct/s only while
a drag outruns the cap, drains at 4 oct/s otherwise, and resolves in `lag/4` s after release.
Manual drags and host automation share the identical path; exactly one smoothing stage exists.

## Crossover Follower Slow-Drag Regression (v0.8.10 maintenance fix)

**Observed behaviour.** Interactive testing of the flat 4 oct/s cap above reported an inversion:
*very fast* flicks felt acceptable while *slow-to-normal* drags left the audible crossover
trailing the GUI line, with movement continuing for several seconds after release.

**Root cause (measured, not a state bug).** The follower math is correct — a drag at or below its
cap tracks 1:1, and no target update resets or interferes with the glide state. The regression is
a *calibration-vs-geometry* error: the Multiband display maps ~10 octaves onto ~900 px
(≈ 90 px/octave), so ordinary human gestures of 400–2000 px/s are **4–22 oct/s** — above the flat
cap. Trajectory reconstruction through the real pipeline (mouse events → 0.5 px write gate →
block-cadence target reads → glide) shows a 600 px/s "normal" drag pinned 2.4 octaves behind with
0.63 s of post-release crawl, and every faster gesture worse (up to ~4.9 oct behind, ~1.4 s of
crawl — on top of trailing throughout the drag itself). Genuinely slow drags (≤ 360 px/s) tracked
exactly, and violent flicks could escape through the discrete-jump bank fade (a coalesced
> 1.5 oct per-block step lands in 12 ms) — which is precisely why "fast felt better than slow".
The deeper error: the swept-allpass shift at sweep rate R is a **constant 0.312·R Hz wherever the
crossing sits**, so a cap flat in oct/s spends its entire artifact budget protecting low crossings
(where a constant-Hz shift is nearest the bass register's constant-Hz pitch JND) and buys nothing
but lag at high ones.

**Fix (the operating curve, not the architecture).** The glide becomes a **slew-limited
smoother**: per sample each cutoff moves by its ~20 ms one-pole demand toward the target, clamped
to a **frequency-proportional cap `R(f) = 4 · max(1, f/300 Hz)` oct/s**:

- Below 300 Hz the flat 4 oct/s floor is unchanged — shift ≤ 1.25 Hz, the original bound, so a
  150 Hz crossing still measures ~14 cents (Test 29: 14.0–14.2c, same as before the fix).
- Above 300 Hz the cap grows with the cutoff, holding the shift at **0.42 % of the crossing
  (~7 cents)** — 13.3 oct/s at 1 kHz, 160 at 12 kHz. 300 Hz is the measured knee for the spur
  bound: an fref = 150 variant rode 27 oct/s past a 1 kHz tone and sprayed −27 dBc (> the
  −31 dBc bound); fref = 300 measures at the −41 dBc analysis floor.
- The one-pole leg exists for spectral purity, not feel: it filters the 60 Hz UI staircase out of
  the demand and **tapers every arrival** — a bare rate-clamp lands at full speed, a corner in
  the phase trajectory that measured −24 dBc of splatter when it coincided with a tone.

Kinematics after the fix (same reconstruction): drags ≤ 4 oct/s track 1:1 (± the 20 ms ease); the
600 px/s complaint gesture converges **0.01 s** after release (was 0.63 s); a 950 px/s drag 0.09 s
(was 0.94 s); even a full-panel flick lands in ~0.5 s of continuous motion (was ~1.4 s plus the
in-drag trailing). No timers, no prediction, no consolidation — the v0.8.10 final architecture
(zero-latency LR4, per-sample glide + discrete-jump bank fade, flat recombination) is unchanged;
only the glide's rate law and arrival shape moved.

Regression: Test 29 gained a **normal-drag tracking** scenario on both paths (150 Hz → 12 kHz over
0.95 s at a 60 Hz cadence — 6.65 oct/s ≈ 600 px/s): the audible band edge must be at the target
0.1–0.35 s after release (solo monitor ≥ 0.9 of full level, measures 0.99; the multiband width-0
leak ≤ 0.15, measures 0.01). Re-pinned to the flat cap, both checks fail (0.47 and 0.60) —
verified in both directions. All prior Test 29 bounds hold at the same measured values.

## Consequences

- Fast drags carry a small **controlled, bounded FM** (~14 cents at a 150 Hz crossing, ~7 cents
  at any crossing above 300 Hz) instead of interaction latency — the accepted product trade
  (KI-012). Drags within the cap are artifact-bounded *and* track 1:1.
- The pure-sine protocol is enforced by regression (`testMultibandSplitDragNoPitchShift`, Test 29)
  at the final operating curve: < 18 cents worst 100 ms chunk through drags, crawls, and the
  tone crossing (the shipped follower measures ~14; the uncapped original ~28 and the bare
  one-pole ~50 fail); spur < −31 dBc at a 60 Hz drag cadence (measures −41.3); a released 6-oct
  flick lands by plain gliding well under a second; a normal-speed drag's band edge arrives with
  the gesture on both paths (the flat-cap follower fails both checks); discrete jumps land via
  the bank fade; every stream click-free.
- Any future attempt to make fast tracking artifact-free must change the crossover class
  (linear-phase, H2) — a reported-latency change requiring a new ADR + Architecture Review.

Evidence [Verified]: src/dsp/MultibandWidth.{h,cpp}, src/dsp/SoloMonitor.{h,cpp} (design comments
carry the per-mechanism measurements); tests/dsp_tests.cpp Test 29; CHANGELOG [0.8.10]; PR #59
review thread (architecture comparison + H3 hostile review, with sources).
