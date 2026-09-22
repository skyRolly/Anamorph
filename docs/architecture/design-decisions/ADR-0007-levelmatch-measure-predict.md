# ADR-0007 — Level Match = BS.1770 Measure + absolute Predict

**Status:** Accepted

## Context
A fair A/B needs the processed output loudness-matched to the dry in real time. A pure measured
loudness lags and drifts on silence; a pure accumulator ratchets.

## Problem
Early Level Match drifted on silence (slammed loud on the next play) and could ratchet toward the
−24 dB floor when Drive was cranked up/down while paused; raising Mix with Drive cranked slammed.

## Options
- **A. Continuous adaptive AGC.** Rejected — not transparent; constantly moving.
- **B. Measured loudness only.** Lags; drifts on silence.
- **C. BS.1770 Measure + absolute feed-forward Predict.** Chosen (0.8.1).

## Decision
Two cooperating estimators publishing `matchGainDb = LUFS(dry) − LUFS(wet)`:
- **Measure** — ITU-R BS.1770 K-weighted ground truth; on silence it **holds** the last trusted
  value (no drift).
- **Predict** — an **absolute** (non-accumulating) feed-forward estimate of the wet's boost as a
  pure function of **Drive and Mix**; floor-only (only ever lowers gain). A silence→audio edge
  snaps the applied gain so the first audible block is compensated even if the host never ran the
  plugin while paused. Reads ≈0 at unity (no bias).

Dry reference is the phase-matched `A(dry)` (ADR-0005), not the raw input, so the multiband
allpass ripple cancels. "Apply" locks the measured gain into Output Gain as a fixed value.

## Note, 2026-09-21 — a host reset does not clear the measure

R5/F2 added the missing `AudioProcessor::reset()` override, which gives this ADR a **new entry point
that did not exist before**: a host issues a reset on a transport stop (VST3 `setProcessing(false)`,
AU `Reset()`), and a transport stop is the canonical silence this ADR's "holds the last trusted
value" rule is about.

The override therefore calls `AnamorphEngine::reset (ResetScope::audioTailsOnly)`, not the wholesale
flush: it clears the delay lines, filter banks, oversamplers, rings and any in-flight duck, and
deliberately leaves `loudness` alone. The wholesale flush — which `prepare()` still performs, because
a new sample rate invalidates the measurement too — zeroes `matchGainDb`, and measured on this engine
that turns a converged −5.158 dB into 0.000 dB on every transport stop: the "slammed loud on the next
play" symptom in the Context above, and a contradiction of `testLevelMatchSilenceFreeze` (Test 16).

Anything that adds another flush path must make the same distinction. State test 118's last leg
asserts it.

## Correction, 2026-09-22 — "leaves `loudness` alone" was half of the answer

The note above is right that the PUBLISHED gain must survive a host reset and wrong that the whole
matcher should be untouched, and the second half is a real contract break rather than untidiness.
`LoudnessMatch` holds two separable things: the ANALYSIS (the four K-weighting biquads and the two
energy integrators) and the RESULT (`displayedGainDb`, `prevPredictedGainDb`, the published
`matchGainDb`). R5's `audioTailsOnly` path called neither `reset()` nor `softReset()`, so the
analysis kept describing audio that had stopped.

That is observable because **the silence gate is judged from the integrators themselves** —
`meanSqDry < 1e-6 && meanSqWet < 1e-6`, against a τ = 0.4 s window. Stale energy makes `silent`
read FALSE for seconds of real silence, and MEASURE keeps gliding the published gain toward a
target computed from pre-reset audio. Measured through the wrapper, Drive 8 / Mix 1 / Width 0.3 /
Amount 0.4, 3 s of noise then a host reset then silence: the gain moved **0.0242 dB** and was still
moving 4 s later. Small, and exactly what "No drift on silence" below forbids.

The path now calls **`softReset()`** — the semantic the duck bottom already uses when the
processing changed (`if (procChanged) loudness.softReset()`): filters and integrators cleared, the
result carried across untouched. Post-fix the same measurement moves **0.0000 dB**, with the value
at the reset bit-identical to the converged one.

Driving the four failure modes apart, `LoudnessMatch` alone, 4 s of true silence after convergence
(Drive told to PREDICT 8 dB, floor −4.0572 dB; real wet-vs-dry difference +2 dB):

| host-reset variant | at reset | after 4 s | max move |
|---|---|---|---|
| nothing (what R5 shipped) | −2.0673 | −2.0008 | 0.0666 |
| `softReset()` (this correction) | −2.0673 | −2.0673 | **0.0000** |
| integrators cleared, filters left warm | −2.0673 | −2.0202 | 0.0471 |
| `softReset()` + `prevPredictedGainDb` zeroed | −2.0673 | −4.0572 | 1.9898 |
| `softReset()` + `displayedGainDb` zeroed | −2.0673 | +0.0000 | 2.0673 |
| `reset()` (the wholesale flush) | +0.0000 | −4.0572 | 4.0572 |

Row 3 is why one observation covers all four fields: warm biquads RING into the silence and push
even cleared integrators back over the gate. Rows 4 and 5 are why the published half must survive —
zeroing `prevPredictedGainDb` lets the PREDICT floor slam the gain down on the next block.

**The general rule this settles**, for `audioTailsOnly` and for anything that adds another flush
path: a host reset clears **audio**, and leaves **display and the user's own latches** alone. The
same pass found `levels.reset()` on this path wiping `peakHoldL/R` — a held peak `LevelMeters.h`
documents as "never falls", with its reset rule stated in the same header: *"on a number click or a
playback restart"*. The product has exactly two such paths and a transport stop is neither
(`src/gui/LevelMeter.h:27`, `src/PluginProcessor.cpp:442`). Measured, a transport stop turned a held
−0.92 dB into −100.00 dB — destroying the reading at the moment the user stopped to read it.
`correlation` and `levels` are now cleared under `ResetScope::everything` only, and the play-edge
and seek clears are measured intact (−0.92 dB kept across the stop, −33.98 dB after the next play).

State test 120 asserts all of it — including a fourth leg driving a real `AudioPlayHead`, so a
narrowing that kept the peak by breaking the play-edge clear could not pass — and fails against the
pre-fix engine (0.0242 dB of drift; −100.00 dB held peak).
`scripts/check-state-coverage.py` holds the per-module decision in place.

## Consequences
- No drift on silence; no ratchet; no Mix=100% slam; unbiased at unity.
- Deliberately **not** a continuously-adapting AGC.

## Related code
- `src/dsp/LoudnessMatch.cpp:15-43` (K-weighting), `:74-95` (predict), `:131-156` (measure/hold)
- `src/dsp/AnamorphEngine.cpp:1121-1154` (A(dry) ref + silence-edge snap)
- `src/PluginProcessor.cpp:402-424` (`applyAutoGain`)

Evidence [Verified]:
- Source: src/dsp/LoudnessMatch.cpp:15-156
- Tests: testLevelMatchUnity, testLevelMatchNoRatchet, testLevelMatchMixCouplingNoSlam,
  testLevelMatchSilenceFreeze, testMultibandUnityMatch
- History [Partially Verified]: CHANGELOG.md [0.8.1]
- Related incident: `../../POSTMORTEMS.md` INC-002 (Level Match ratchet / Mix=100% slam)
