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

## Consequences
- No drift on silence; no ratchet; no Mix=100% slam; unbiased at unity.
- Deliberately **not** a continuously-adapting AGC.

## Related code
- `src/dsp/LoudnessMatch.cpp:15-43` (K-weighting), `:74-95` (predict), `:131-156` (measure/hold)
- `src/dsp/AnamorphEngine.cpp:768-801` (A(dry) ref + silence-edge snap)
- `src/PluginProcessor.cpp:135-157` (`applyAutoGain`)

Evidence [Verified]:
- Source: src/dsp/LoudnessMatch.cpp:15-156
- Tests: testLevelMatchUnity, testLevelMatchNoRatchet, testLevelMatchMixCouplingNoSlam,
  testLevelMatchSilenceFreeze, testMultibandUnityMatch
- History [Partially Verified]: CHANGELOG.md [0.8.1]
- Related incident: `../../POSTMORTEMS.md` INC-002 (Level Match ratchet / Mix=100% slam)
