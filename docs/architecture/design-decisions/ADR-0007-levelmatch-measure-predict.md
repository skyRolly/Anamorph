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

## Note, 2026-09-22 — what may re-arm the measure, and what may not

The measure is re-armed in exactly one place: the silent duck bottom, `if (procChanged)
loudness.softReset()`, where `procChanged` is `processingDiffers (pendingP, p)`. That function asks
*did the signal path change* — a narrower question than the duck's own — and its answer decides
whether a converged reading survives a switch.

`processingDiffers` compared `dimMode` **unconditionally**, while ADR-0004's Correction of
2026-09-21 had already given `discreteDiffers` a Dimension-D relevance guard for it: the field's
only reader is `chorus.setDimMode`, inside `else if (p.algorithm == Algorithm::DimensionD)`, so
under any other algorithm the value reaches no module and the path did not change.

**The reported scenario did not reproduce**, and that is worth recording. A plain Dim-D Style move
under Haas opens no duck at all after ADR-0004's correction, so this function is never consulted.
Two other routes did reach it, both measured on the engine (Haas, Level Match engaged, converged
then silent — the analysis survives as ~0.030 dB of ordinary drift and is thrown away as a frozen
0.000):

| route | before | after |
|---|---|---|
| a plain Dim-D Style move | preserved | preserved |
| a **forced duck** — A/B, preset recall, undo (`requestDuck`) whose only processing delta is `dimMode` | **thrown away** | preserved |
| `dimMode` in the same snapshot as a **Level Match toggle** (`autoGainMatch` is the one field `discreteDiffers` lists and `processingDiffers` does not, so it opens a duck of its own) | **thrown away** | preserved |
| `dimMode` while Dimension D is live, or any real path change | thrown away | thrown away |

The second route defeats the rule written at the call site itself — *"Toggling Level Match / Bypass
must NOT re-measure, or enabling Match with a big boost slams loud for a moment"*. Each of the two
was paired with an attribution control (the same duck with `dimMode` held still), and both controls
preserved, so the re-arm was attributable to `dimMode` alone.

`processingDiffers` now carries the same guard, symmetric and conservative exactly as in
`discreteDiffers`. Test 58 pins all of it, including four legs that must STILL re-arm — which is
what pins the guard rather than a removal. `scripts/check-state-coverage.py` additionally requires
the two selection lists to attach the same condition to a field they both name; it does not and
cannot check that the condition is right.

## Correction, 2026-09-22 — the live display is not a latch

The rule settled above — a host reset "leaves **display and the user's own latches** alone" — joined
two things that need opposite answers. The **latches** (`peakHoldL/R` and the two clip latches) are
the user's, and that correction is right to keep them. The **live display** (the three meter
envelopes, the bar tick and its hold, the RMS number and its hold, and `correlation`'s running
averages) describes audio that has ended. It was skipped on the premise that it decays on silence by
its own ballistics, which holds only while the host keeps calling `processBlock`: those ballistics
run in `process()` / `publish()` on the audio thread, and the GUI draws the published atomics with
none of its own (`gui/LevelMeter.h`: "ballistics are all audio-side"). The host class this entry
point exists for — VST3 `setProcessing(false)`, AU `Reset()` — calls `reset()` and then stops
calling `process`.

Measured through the wrapper, a host reset with no block after it (−6 dBFS noise, one 0.9 transient):

| readout | active | after the reset, R6 | 2 s later, R6 | after the reset, now |
|---|---|---|---|---|
| dim / bright envelope | −6.13 / −11.17 | −6.13 / −11.17 | −6.13 / −11.17 | −100.00 / −100.00 |
| bar tick / RMS number | −0.92 / −10.87 | −0.92 / −10.87 | −0.92 / −10.87 | −100.00 / −100.00 |
| held peak (a latch) | −0.92 | −0.92 | −0.92 | **−0.92** |

With the right channel at 0.4× the left, `correlation`'s `energy` stayed at 9.663e-02 — above the
GUI's `energy < 6e-9` silence test, so the phase and balance pointers never began their glide to
centre. On resume the bar tick (1 s hold) and the RMS number (1.2 s hold) still carried the pre-stop
values. And because `stepRmsNumber` latches the RMS clip on the NUMBER, which neither the readout
click nor the play edge clears, the first block after either re-latched the clip it had just cleared
(after +3.52 dB material: RMS clip set again, number +3.30 dB).

`audioTailsOnly` now calls `levels.resetLive()` — the live half, published — and `everything` calls
`levels.reset()`, the whole meter. `correlation` has no latch, so it is reset on both scopes, and
its `reset()` now publishes. **The rule is therefore: a host reset clears audio and the live display
that describes audio that has ended, and leaves the user's latches and the Level-Match result
alone.** The trade-off, stated rather than hidden: a host that resets AND keeps calling
`processBlock` with silence now sees the live meters reach the floor at the stop, where they used to
fall there over several seconds (measured before the fix, that same host: the bar tick at −100 dB
after 1.5 s, the RMS number still at −25.21 dB after 3 s). The end state is the same, and silence
with no reset decays exactly as before.

State test 122 asserts all of it through the published atomics — including a click and a play edge
after the reset, and silence with no reset — and 9 of its 24 checks fail against the pre-fix engine.
`scripts/check-state-coverage.py` now reads `correlation` as reset on both scopes. `levels` stays
declared `everything`, deliberately: `resetLive()` is not counted as a reset, so the full
`levels.reset()` the correction above removed from the host path still cannot come back unseen.

**Architecture Review Gate: APPROVED by the owner, 2026-09-22 (R9).** The approval covers the
reset / thread-model change as implemented in `c5f3d8f`: a host reset clears and publishes the live
meter display and the correlation meter from `reset()`'s thread; the held peak, the clip latches
and the published Level-Match gain survive it; a re-prepare still resets all of it.

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | the PR #155 body and the `c5f3d8f` commit message, both naming the two gated classes: a new writer of the meter and correlation atomics, and this Correction |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's ruling of 2026-09-22**: *"Human Architecture Approval has now been granted for the reset/thread-model change introduced in this PR"* |
| 3 | if the change is a decision, an ADR is added/updated | this Correction; `THREAD_MODEL.md` (the two host rows, and the *Level meters*, *Correlation* and *Meter hold reset* rows); `THREADING_POLICY.md` (the Audio → GUI row) |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered** — no parameter ID, range, default, automation flag, serialization field or reported-latency value changes |

## Consequences
- No drift on silence; no ratchet; no Mix=100% slam; unbiased at unity.
- Deliberately **not** a continuously-adapting AGC.

## Related code
- `src/dsp/LoudnessMatch.cpp:15-43` (K-weighting), `:74-95` (predict), `:131-156` (measure/hold)
- `src/dsp/AnamorphEngine.cpp:1177-1210` (A(dry) ref + silence-edge snap)
- `src/PluginProcessor.cpp:402-424` (`applyAutoGain`)

Evidence [Verified]:
- Source: src/dsp/LoudnessMatch.cpp:15-156
- Tests: testLevelMatchUnity, testLevelMatchNoRatchet, testLevelMatchMixCouplingNoSlam,
  testLevelMatchSilenceFreeze, testMultibandUnityMatch
- History [Partially Verified]: CHANGELOG.md [0.8.1]
- Related incident: `../../POSTMORTEMS.md` INC-002 (Level Match ratchet / Mix=100% slam)
