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

## Note, 2026-09-24 — F13 measured: what a forced swap carries, and Apply with no measurement

F13 of the v0.9.9 global review said Level Match state survives transitions that should invalidate
it. Measured through the processor and the engine (worklog `NONFINITE_PARAMETERS_AND_F13.md` §E),
against a fresh instance at the destination state:

- **Preset load, undo and redo that move only continuous controls behave like the same live edit.**
  The published gain is carried — such a swap does not re-arm at all (`procChanged` is false), so
  nothing touches the result — the predict floor is re-applied, and the measure re-converges with its
  own time constants: within 0.27–0.45 dB of the live edit after 0.1 s, the same end state. The "≈0.6 dB for ≈4 s" figure from R6 is that
  re-convergence for one A/B setup (0.63 dB, within 0.1 dB at 2.75 s), not a fixed drift: across
  deltas and levels it is 0.22–5.8 dB on A/B, and on preset / undo / redo it is the live edit's own.
- **`matchGainSmooth` left out of `snapSmoothers()`** changes almost nothing when Level Match is on in
  both states (the smoother sits 0.02–0.06 dB from its target at the bottom; 0.9 dB right after a
  large live Drive move). It matters when a forced swap turns Level Match ON — in production, **Undo of
  Apply** — and there it swells: the smoother sits at unity while Level Match is off, so the undo
  overshoots both endpoint levels by +4.0 / +4.8 dB (Drive 8 / 10) on the programme first measured and
  +4.5 / +5.7 dB on a second — the size scales with the match gain — peaking ~130 ms after the undo.
  Re-engaging Level Match by hand after Apply (a ducked, non-forced switch) swells the same way, as
  does any engage from an Output Gain below the matched gain (worklog §I; KI-031). `snapSmoothers()`'s own comment gives
  its purpose as "a big level change never swells (#1)"; the documents that call the engage
  smoothed promise no click, not this. No test covers it.
- **An A/B switch whose slots differ only in continuous controls undoes its own injection.** The
  injected slot gain is right (0.03 dB from the converged value), but the analysis is not re-armed
  (`processingDiffers` asks only about discrete fields), so the stale integrators pull the gain away
  within ~100 ms and it takes 1.8–4.5 s to come back. Re-arming at an injection keeps it within
  0.022–0.042 dB. That is a change to this ADR's "re-armed in exactly one place" rule.
- **Apply locked a NaN.** The matcher publishes NaN for the few microseconds between a non-finite
  input sample and the self-heal's `loudness.reset()` (ADR-0009), and `applyAutoGain`'s `jlimit`
  passes NaN: Output Gain became NaN (silence through a host reset and a re-prepare, `value="nan"`
  saved, Undo to 0 dB instead of the user's value). "Apply locks the measured gain" has nothing to
  lock there, so Apply now does nothing (`src/PluginProcessor.cpp:469`); every finite Apply is
  unchanged. State test 129.

**Recorded for the owner, not decided here** (each is a change to this ADR, not a defect of it):
1. At a silent duck bottom that turns Level Match on, what applied gain should the fade-in carry —
   unity (today), the matcher's value, or the gain that was playing? Measured (worklog §I5; every
   candidate leaves the published value, the analysis, A/B and both suites unchanged): keep and
   document (the swell stays, KI-031); snap it in `snapSmoothers()` (forced routes only — Undo of
   Apply and a preset to 0 dB; the hand engage keeps its swell); snap it after `loudness.process` in
   the forced bottom block (the same, and a continuous-only Drive 0 → 10 swap's excess falls from +6.7
   to +4.6 dB, so that route no longer tracks the live edit); land it after `loudness.process` at any
   bottom that turns Level Match on — a wider change to the engage than this ADR's current text
   describes — (every route to 0 dB; an engage from a louder Output Gain lands instead of gliding
   down; an engage that also changes the sound lands on a published value that describes the old
   sound, up to 4.5 dB further off than today); the same only when nothing but Level Match / Output
   Gain / Output Balance changes (every route to 0 dB, sound-changing engages as today; needs a
   complete comparison that tolerates a preset round trip); start it from the gain that was playing
   (every route, by a glide; slower from a boosted Output Gain); track the matcher while Level Match
   is off (every route; ~3 % more engine time while off). Re-measuring on engage is not an option: it
   contradicts the re-arm rule above and measured worse.
2. Should the measure also re-arm when an A/B slot's remembered gain is injected (measured above), or
   whenever any continuous sound field differs? Re-arming at every injection also re-arms an A/B
   switch between identical slots (0.030 → 0.000454 dB, the re-armed signature), which reverses the
   A/B row of the dimMode table in the note above — no suite catches it, because Test 58 never
   injects. Re-arming on every forced duck is ruled out outright: it fails Test 58. (Measured
   further on 2026-09-24 — each transition's root cause, the candidate policies and the owner's
   questions — in worklog `NONFINITE_PARAMETERS_AND_F13.md` §K; nothing decided there.)

Separately, a NaN reading no longer becomes the match target (ADR-0009, note of the same date;
Test 65): the target stays where it was, this ADR's rule for a reading it cannot trust.

The comments that said an A/B swap glides or re-arms (`AnamorphEngine.cpp:78`, `:1111-1113`,
`:1816`; `LoudnessMatch.h:52-54`) now say what the code does. This ADR's *Related code* anchors
(`AnamorphEngine.cpp:1201-1234`, `PluginProcessor.cpp:402-424`, and `LoudnessMatch.cpp:131-156`,
which is the PREDICT block, not measure / hold) are stale; reported for the documentation pass.

## Amendment, 2026-09-24 — an engage that changes only the gain starts at the published value (F13(1b), O4g)

This answers question 1 of the note above, on the owner's ruling (at the end of this section). It
adds one point at which the applied gain lands. It does not change what is measured, when the measure
re-arms, or A/B.

**The rule.** At a switch's silent bottom that turns Level Match **on** — `p.autoGainMatch` false
before the bottom and true after it, whether the switch is forced (A/B, preset, undo, redo) or
ordinary (the toggle itself) — the applied gain `matchGainSmooth` takes one of two paths:

- **Case A — the switch changes nothing the Level-Match measurement reads.** The matcher, which runs
  with Level Match off too, is already measuring the sound that plays after the bottom, so the fade-in
  starts from the value it publishes, converged or still converging on an earlier edit (*Note of
  2026-09-25, stale engage*, below): right after
  that block's `loudness.process` the smoother is landed, current and target, on the target the
  level-match stage has just computed (`src/dsp/AnamorphEngine.cpp:1959`). This is an **alignment
  of an existing result, not a new measurement**: nothing in `LoudnessMatch` is reset, re-armed,
  written or read differently. Case A holds only when all of these do:
  1. nothing the measurement reads differs between the state heard before the switch and the state
     adopted at the bottom — `! measurementInputsDiffer (p, pendingP)` (`:564`);
  2. no such change was made live during the switch's own fade-out — `! duckMeasDirty`: an ORDINARY
     duck applies its continuous controls at once (`copyContinuous`), so by the bottom `p` already
     carries them and the comparison above cannot see them (`:675`, `:785-786`);
  3. the bottom does not re-arm the measure — `! procChanged` (`processingDiffers` still alone
     decides the `softReset()`; a re-armed measure is moving, so it is not landed on);
  4. no A/B injection is consumed in that block — the slot's remembered gain keeps priority (#23);
  5. the reading is a number (ADR-0009): otherwise the stage keeps its target and nothing lands.
- **Case B — anything else.** Exactly the behaviour before this amendment: the smoother starts where
  it rests while Level Match is off (unity) and glides to the matcher's value — or, at an A/B switch
  (condition 4), starts on the slot's injected gain. When the switch also changes the sound, the
  published value describes the sound *before* the switch; landing on it would align the gain to a
  stale result — worklog §I5 measured that at up to 4.5 dB further from a fresh instance than
  gliding. The right gain there is a measurement question (question 2 above, F13(2), KI-030) and is
  not decided here. A host reset or a silence→audio edge still lands the gain in both cases, through
  the Decision's edge snap, as before.

**`measurementInputsDiffer`** is derived from the engine's signal graph (worklog
`NONFINITE_PARAMETERS_AND_F13.md` §J), not from a list of names, and each answer was checked by
measuring the matcher's state bitwise with one field changed. The measurement reads three things: the
wet at the tap (after Mono Maker), the dry reference `loudnessRefScratch`, and the predict's inputs
(`setDriveDb`, `setMix`). A field counts when a change to it can reach one of them, under the
condition that its module's **output** reaches the tap:

| class | fields |
|---|---|
| always, exact | `channelMode`, `monoSum`, `swapLR`, `polarityL/R`, `msMode`, `solo` (M/S solo is input conditioning, before the tap), `algorithm`, `mbEnable`, `monoMakerEnable`, `oversample`; **`driveDb`, `mix`** — the predict's inputs, whose rise test fires on any rise, one ulp included (measured: +1 ulp of Drive moved the published value 1.08 dB) |
| always, tolerant | `inputBalance`, `algoAmount`, `width` |
| guarded | `haasDelayMs`, `haasSide` (Haas on either side); `velvetDensity` (Velvet); `chorusRate`, `chorusDepth` (Chorus); `dimMode` (Dimension D); `mbBands`, `mbWidthLow` (Multiband on either side), `mbFreqLow`/`mbWidthMid`, `mbFreqMid`/`mbWidthHiMid`, `mbFreqHigh`/`mbWidthHigh` (and at least 2 / 3 / 4 bands); `monoMakerFreq` (Mono Maker on either side) |
| not compared | `outputGainDb`, `outputBalance`, `bypass`, `mbSolo` (after the tap), `autoGainMatch` (the switch itself) |

"Tolerant" is a relative 1e-5: six times the largest representation drift a preset round trip
produced (1.6e-6, on the log-mapped crossovers and Mono Maker Freq; State test 8's snap-equivalence),
and under a tenth of any snapped parameter's half grid step, so a round trip is not a change and a
real edit is. It is not an equality over the whole struct: that version refused to land on a
preset whose Chorus Rate — inert under Haas — came back one ulp off. `scripts/check-state-coverage.py`
holds the function to a declaration for every `EngineParameters` field (`MEASUREMENT_INPUTS`), as it
does for the four lists before it, and derives from the code that the predict's inputs are exact.

**Accepted, bounded, and stated rather than hidden** (all measured, worklog §J): `bypass` and the Level
Match switch itself move the H4 dry reference while Level Match is off (the 0.8.9 Class-B difference;
≤ 0.0064 dB at an engage); a Multiband on/off crossfade still running when an ordinary engage opens
(≤ 12 ms) reaches the tap by ≤ 1.7e-4 dB; the forced bottom's module restarts leave ≤ 0.009 dB.

**The boundary with F13(2).** Case A asks about the *switch*. A sound change made **before** the switch
(Level Match off, Drive moved, then Level Match turned on within the measure's settling time) is not
part of it: the published value is still converging on the new sound, and a Case-A engage lands on
it — the value a Level Match that had been on throughout would be following, since the matcher runs
whether or not Level Match is on. That lag is the measure's own time constants (question 2, F13(2)),
measured in worklog §J, and is not decided here. *(Decided by the F13(2) amendment below, Q4; re-examined
against currency and kept by the Note of 2026-09-25, stale engage, below: it lands, current or not.)*

**What this changes in the text above.**
- *Decision* ("A silence→audio edge snaps the applied gain …"): still true, and no longer the only
  point where the applied gain lands — it now lands at a silence→audio edge, at an A/B injection, and
  at a Case-A bottom. Everywhere else it glides.
- *Note of 2026-09-22* ("re-armed in exactly one place"; "must NOT re-measure"): unchanged. A Case-A
  landing re-arms nothing, and `measurementInputsDiffer` is not a re-arm trigger. The two functions ask
  different questions — *did the signal path change* (re-arm) and *does the switch change what the
  measurement reads* (land; the published value may still be converging on an earlier edit, the Note of
  2026-09-25, stale engage).
- *Note of 2026-09-24, second bullet* — "there it swells", "re-engaging Level Match by hand after
  Apply … swells the same way, as does any engage from an Output Gain below the matched gain", "No
  test covers it" — describes the engine before this amendment. For Case A (Undo of Apply, the hand
  re-engage after Apply, an engage from a low Output Gain, a preset or undo that only turns Level
  Match on) the swell is gone: +2.32 / +4.45 / +5.67 dB at Drive 4 / 8 / 10 → 0.00 dB, settling in
  127 ms instead of 523–647 ms (Test 66, State test 130). For Case B it stands, with Case B's own
  reason (above). The bullet's first half, Level Match on in both states, is unchanged — and its
  "within 0.003 dB" was corrected in place to 0.02–0.06 dB.
- *Question 1*: answered with its option "the same only when nothing but Level Match / Output Gain /
  Output Balance changes", with the set derived rather than named (it also admits `bypass` and
  `mbSolo`, which act after the tap). Re-measuring on engage stays excluded.
- *The comment list* (`AnamorphEngine.cpp:78`, the re-arm comment and `:1816` of that note): rewritten
  again, together with the `prepare()` and `snapSmoothers()` comments.

**One audible change beyond the fix**, stated rather than hidden: a Case-A engage from an Output Gain
*above* the matched gain used to glide down from the unmatched level over ~0.6 s; it now lands at the
silent bottom, which is what every other control already does at a forced bottom.

**Architecture Review Gate — owner ruling of 2026-09-24.** The owner selected this behaviour ("O4g")
from the decision record (worklog §I6) and directed that it be implemented and recorded here. Review
of the implemented predicate and landing is the owner's review of PR #156; until then this section
records the ruling and the implementation, not an approval of the code.

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | the PR #156 body and the implementing commit message, naming the gated class: a change to an **Accepted ADR** (`AI_AGENT_POLICY.md` Hard Stop) — the note above reserved the engage for the owner, "each is a change to this ADR, not a defect of it" |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's ruling of 2026-09-24**: *"Use O4g as the working direction for this round"*; *"Derive it from the actual engine/state graph and signal-path dependencies."*; *"The predicate must tolerate harmless floating-point representation differences"*; *"Preserve current behavior for sound-changing Level Match engages until F13(2) is explicitly resolved."*; and *"Because this changes documented Level Match behavior, update the repository's ADR/decision record consistently and preserve the historical reasoning."* Review of the implementation: pending, PR #156 |
| 3 | if the change is a decision, an ADR is added/updated | this Amendment; ADR-0004 and ADR-0035 (notes of the same date) |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered** — no parameter ID, range, default, automation flag, serialization field or reported-latency value changes; no DSP node, stage order, thread or cross-thread path changes |

Related code (this amendment): `src/dsp/AnamorphEngine.cpp:564` (`measurementInputsDiffer`),
`:675` and `:785-786` (`duckMeasDirty`), `:1175` (the decision at the bottom), `:1867` (the landing);
`scripts/check-state-coverage.py` (`MEASUREMENT_INPUTS`).

## Amendment, 2026-09-24 — F13(2): an A/B injection re-arms a stale analysis, and a same-rate re-prepare keeps a valid result

This answers question 2 of the note above and the five F13(2) questions of the decision record
(worklog `NONFINITE_PARAMETERS_AND_F13.md` §K6), on the owner's authorization (at the end of this
section). It adds one re-arm point and removes one flush. It does not change what is measured, the
measure's time constants or the predict; where the applied gain lands it changes in one place, a
same-rate re-prepare that keeps the result (the correction at the end of this section).

**Where the matcher's two halves stand after every transition.** The *analysis* (K-weighting filter
states and the two energy integrators) describes the audio heard over the last ~0.4 s; the *result*
(`displayedGainDb`, `prevPredictedGainDb`, the published `matchGainDb`) is what that analysis has
concluded so far; the applied gain (`matchGainSmooth`) is the engine's, not the matcher's. The
contract, by transition:

| transition | analysis | result | applied gain |
|---|---|---|---|
| live edit, any field | carried | carried; re-converges with the measure's time constants (the lag the Context accepts); a measurement input's edit leaves it **not current** until the measure has caught up (the amendment of 2026-09-25) | glides |
| switch bottom, the signal path changes (`processingDiffers`) | **re-armed** (`softReset`) | carried; not current until the measure catches up (2026-09-25) | glides (Case B, or Case A excluded by condition 3) |
| forced swap without an injection — preset load, undo, redo — that changes only continuous controls | carried | carried; not current until the measure catches up, as after the same live edit (2026-09-25) | glides; behaves like the same live edit (**Q2: unchanged**) |
| Level Match engage that changes only the gain (Case A) | carried | carried | lands on the published value (the amendment above) |
| Level Match engage together with a sound change (Case B) | carried (re-armed if the path changes) | carried — it describes the sound before the switch | starts at unity and glides; at an A/B switch, on the injected slot gain (**Q3: unchanged**) |
| Case-A engage shortly after a sound change | carried, still converging | carried, still converging; not current (2026-09-25) | lands on it, converged or not (**Q4: unchanged** — no convergence guard; re-examined against currency and kept, the Note of 2026-09-25, stale engage) |
| **A/B switch whose slots differ in anything the measurement reads** | **re-armed** (new) | overwritten by the destination slot's remembered gain, current only if its record was measured, at this rate, for these measurement inputs, otherwise not current (2026-09-25, A/B provenance) | lands on that gain |
| A/B switch whose slots differ only in what the measurement does not read (Output Gain, Output Balance, Bypass, Band Solo, the Level Match switch, an inert guarded field), or not at all | carried (re-armed only if the path changes) | overwritten by the slot's remembered gain, current by the same rule | lands on that gain |
| host reset (R9) | re-armed | carried, current or not as before it; one that lands an in-flight duck's measurement-input change leaves it not current (2026-09-25) | lands at the next silence→audio edge |
| **re-prepare at the same sample rate, measurement inputs unchanged, the result current** (2026-09-25) | **re-armed** | **carried** (new) | Level Match on: **starts on the kept result** (`prepare()`; the correction below), quiet audio included; Level Match off: rests at unity, so a later engage starts as Case A / Case B say |
| re-prepare at a new sample rate, or after a primed snapshot that changes a measurement input, or before the measure has caught up with one changed earlier (2026-09-25), or the first prepare | flushed | flushed (0 dB, then the predict floor) | starts at unity (the flushed 0 dB); lands at the next silence→audio edge, and below that edge's detector glides to the predict floor (unchanged) |
| NaN self-heal (ADR-0009) | flushed | flushed (ADR-0009's own owner question) | glides |

**Q1 — the A/B injection (new).** An A/B switch injects the destination slot's remembered gain into
the result at the switch's silent bottom (#23). When the two slots differ in anything the measurement
reads, the carried analysis still describes the *source* slot's audio and drags that correct value
toward the source for seconds — the one transition where the two halves disagree in the wrong
direction (worklog §K3–§K4, KI-030). The analysis is now re-armed there: at the bottom the engine
computes `measChangedAtBottom = duckMeasDirty || measurementInputsDiffer (p, pendingP)` — the same
answer the Case-A landing uses, now held once for both — and an injection consumed in that block
calls `softReset()` before it writes the slot's value. Slots that differ in nothing the measurement
reads keep a converged analysis, so the Note of 2026-09-22's dimMode table keeps its A/B row, and a
Level Match or Bypass toggle alone still re-measures nothing. The re-arm clears the analysis only: the
injected result is written after it and survives, so this is not a re-measure (:240-241, :344) and
an A/B that also turns Level Match on does not slam (:124-125). An injection consumed in a block with
no switch bottom (the defensive consumer; after a host reset the pending injection is adopted there,
on an analysis the reset has already re-armed) re-arms nothing.

**Q2, Q3, Q4 — unchanged, decided.** A forced swap without an injection keeps its analysis and tracks
the same live edit; the Drive 0 ↔ 10 undo figures (+6.73 / −7.89 dB) are that live edit's own lag and
the rise-only predict (worklog §K3), not a route defect, and every alternative measured traded one
direction for the other or made the forced route diverge from the live edit (§K5). A Case-B engage
keeps its unity start. A Case-A engage keeps landing on whatever is published (re-examined against the
currency the matcher reports since 2026-09-25, and kept: the Note of 2026-09-25, stale engage, below).

**Q5 — a same-rate re-prepare keeps a valid result (new).** `prepare()` used to flush the whole
matcher. The review gate's scope sentence below — *"a re-prepare still resets all of it"* (:182) —
describes what the reset/thread-model change of `c5f3d8f` left in place; the owner's words there
(:187) approve that change and say nothing about the re-prepare, and this note's own reason for the
flush (:39-40) is the sample rate. The flush is now kept only where that reason holds: a re-prepare
**keeps** the result, and re-arms the analysis as a host reset does, when all of these hold —
1. the engine was prepared before (there is a result to keep, and until then the rate reads its
   44.1 kHz default, which a first prepare at 44.1 kHz would otherwise match — Test 67 leg (5)(d),
   State test 131 leg (h));
2. the sample rate is bit-identical to the previous one — the K-weighting coefficients, the 0.4 s
   window and the glide constants are functions of the rate alone, and a block size is no input to
   the measurement (the glide coefficients re-key on each block's length);
3. the snapshot `primeParameters()` adopted before it changes nothing the measurement reads
   (`measurementInputsDiffer` — a restore that moved Drive between two prepares flushes; one that
   moved only Output Gain keeps);
4. the published value is finite;
5. the published value is **current** — the measure has caught up with the last change to anything
   it reads, however that change arrived — and `reset()` will not adopt such a change from an
   in-flight duck (the amendment of 2026-09-25 below: condition 3 sees only what the prime brings).
Otherwise it flushes as before. **A new sample rate keeps the flush, by decision.** The analysis
must be rebuilt there (its coefficients are functions of the rate); the result need not be — carried
across a rate change on Haas programmes it measured 0.02–0.13 dB from the destination's converged
value, against the flush's +2.7 to +3.3 dB for ~3 s (worklog §L4), because the measurement adds no
rate dependence of its own and what differs is the sound itself (Haas's fractional delay at 44.1 kHz,
harmonics above the lower Nyquist). The authorization limits P4 to the same rate, the evidence covers
one algorithm and stationary programmes, and a rate change is rare; extending the keep is recorded as
a candidate, not adopted.

**What this changes in the text above.**
- *Note of 2026-09-22*, "The measure is re-armed in exactly one place: the silent duck bottom" (:101):
  historical, and already too narrow before this amendment (the host reset re-arms, `prepare()` and
  the self-heal flush). The re-arm and flush points are the table above: the switch bottom when the
  path changes, an A/B injection when the slots' measurement inputs differ, every host reset, a
  same-rate re-prepare that keeps its result; the flushes are any other re-prepare and the self-heal.
  Its dimMode table and "Toggling Level Match / Bypass must NOT re-measure" (:124-125) stand.
- *Note of 2026-09-24*, third bullet ("An A/B switch whose slots differ only in continuous controls
  undoes its own injection"): resolved — a Drive 2 → 8 switch now stays within 0.10 dB of a fresh
  instance at the destination (from +1.59 dB), settling in 124 ms instead of ~2.4 s. Its "within 0.022–0.042 dB" was re-arming at
  *every* injection, which this is not.
- *Question 2*: answered — yes at an injection whose slots differ in a measurement input; no for
  forced swaps without an injection.
- *Amendment above*, condition 3 ("`processingDiffers` still alone decides the `softReset()`") and
  "`measurementInputsDiffer` is not a re-arm trigger" (:330-333): at a bottom that consumes an A/B
  injection it now is one. Case A is untouched — an injection block never lands (condition 4) — and
  the landing and the re-arm read the same `measChangedAtBottom`, so they cannot disagree about
  whether the switch changed what the measurement reads.
- *The wholesale flush* (:39-40, "which `prepare()` still performs, because a new sample rate
  invalidates the measurement too"): now performed at a new rate only, or when the inputs changed.
- *Review gate R9*, "a re-prepare still resets all of it" (:182): a re-prepare at a new rate still
  does; one at the same rate with the same measurement inputs keeps the published gain and re-arms
  the analysis. State test 120's leg 2 now pins both halves.

**Measured on the implementation** (processor level, 48 kHz / 256, pink noise; `pkX` / `dipX`
against the two counterfactual trajectories, as in worklog §E; worklog §L4 — the same figures the
scratch builds of §K5 gave):

| route | before | after |
|---|---|---|
| A/B, slots differ only in Drive 2 → 8 / 8 → 2 (Level Match on in both) | +1.59 / −1.84 dB, settle 2,448 / 2,548 ms | +0.10 / +0.04 dB, 124 / 125 ms |
| A/B that turns Level Match on while Drive changes (the slot's gain is injected) | +1.63 dB | +0.07 dB |
| re-prepare, same rate, same or doubled block size | +2.46 dB, settle 2,215 ms | +0.04 dB, 0 ms |
| re-prepare at a new rate (48 → 44.1 kHz) | +2.40 dB | unchanged (flushes by decision; see Q5) |
| A/B between identical slots, or slots differing only in Level Match | — | unchanged (not re-armed) |
| Undo / preset / redo Drive 0 ↔ 10, live edits, Case A and Case B engages, host reset | — | unchanged (44 of 49 harness rows byte-identical; the five above are the changes) |

**Architecture Review Gate — owner authorization of 2026-09-24.**

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | the PR #156 body and the implementing commit message: a change to an **Accepted ADR** (this one's Note of 2026-09-22 and the review-gate scope sentence) |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's authorization of 2026-09-24**: *"You are explicitly authorized to make the owner decisions for the unresolved F13(2) questions based on your own investigation, measured evidence, and the recommendations already recorded in the worklog."* Q1: *"Adopt **P1b**"* — *"Re-arm the analysis at an A/B injection only when the two slots differ in something the Level Match measurement actually reads."* Q2–Q4: *"Keep the existing behavior."* / *"Keep the current behavior."* Q5: *"Adopt **P4**"* — *"Preserve the existing published Level Match result across a same-rate `re-prepare` when that result remains valid."* and *"Do not automatically generalize this to different-rate re-prepare."* Review of the implementation: pending, PR #156 |
| 3 | if the change is a decision, an ADR is added/updated | this Amendment, in place, as the one above (ADR_POLICY's "a reversed decision adds a new ADR" read with this ADR's own precedent: the decision stands, two of its notes are narrowed) |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered** — no parameter ID, range, default, automation flag, serialization field or reported-latency value changes; no DSP node, stage order, thread or cross-thread path changes (`primeMeasChanged` and `keepMatchResult` are written and read on the prepare path, which JUCE never runs concurrently with `process()` or `reset()`) |

**Correction, 2026-09-25 — the applied gain at a kept re-prepare (Devin review of PR #156).** Q5 keeps
the *result*; the first implementation left the *applied gain* where `prepare()` writes it, at unity,
and let process()'s silence→audio snap land it — and that snap fires only on a block whose input
reaches its ~−60 dBFS detector (`inSq ≥ 1e-6·n`). Audio resumed below it (a quiet intro, a fade, a
tail) therefore played the smoother's 0.12 s glide from 0 dB to the kept value. Measured through the
processor, Drive 8, a kept −6.03 dB, resumed at −70 dBFS: −0.001 / −0.371 / −1.162 / −2.503 / −6.031 dB
at 0 / 10 / 30 / 60 / 120 ms; now −6.031 dB from the first sample (State test 132; engine level, Test 68,
the same at −70, −90 and −125 dBFS, for kept values from −10.95 to +7.71 dB). This table's row said
"lands at the next silence→audio edge" and contradicted what Q5 decides — the kept result is what plays
— so the row is corrected above, and `prepare()` now starts the applied gain on the kept result when
Level Match is on in the state its `reset()` settles (after it, because `reset()` may adopt an in-flight
duck's snapshot). Unchanged: with Level Match off it rests at unity, so a later engage starts as Case A /
Case B say; every flush (new rate, primed measurement change, first prepare, a non-finite result) starts
at unity, which is the flushed 0 dB; the re-arm rule, the measurement, the predict, the host reset and
the engage cases. The applied gain now lands at a silence→audio edge, an A/B injection, a Case-A bottom
(the amendment above, :327-329) and a kept re-prepare. A flush's own quiet glide from unity to the
predict floor has no kept result behind it and is recorded, not changed (worklog §M). Owner instruction
of 2026-09-25: *"the applied Level Match smoother must begin from the retained match value rather than
unity"* and *"The first quiet blocks after re-prepare must already use the retained Level Match gain."*
Gate: a text correction to this Accepted ADR, flagged in the PR #156 body and the implementing commit
(`043c7e3`); step 2, the owner's instruction above; review of the implementation: pending, PR #156; no
compatibility trigger (no parameter, schema, thread, order or latency change — the write is on the
prepare path).

Related code (this amendment): `src/dsp/AnamorphEngine.cpp:1263` (`measChangedAtBottom`, one
answer for the Case-A landing and the injection re-arm), `:680` (the re-arm, since the A/B provenance
amendment in `adoptRememberedMatch`, which the two consumers call at `:1375-1380` and `:1413-1418`), `:63` (`keepMatch`), `:77` and `:167` (the kept matcher skips
`loudness.prepare` and takes `softReset`), `:291` (`reset (everything)`), `:175-176` (the kept result
becomes the applied gain; the correction); `src/dsp/AnamorphEngine.h:125` (`primeParameters` records
`primeMeasChanged`).

## Amendment, 2026-09-25 — a same-rate re-prepare keeps only a result that is current (Devin review: live edits)

Devin's review of PR #156 found that Q5 above keeps a result measured for the previous sound: *"Live
edits retain stale match gain."* Condition 3 compares the snapshot `primeParameters()` adopts with the
engine's live `p`. A live edit, a duck bottom and a forced swap write `p` when they happen, so when the
host re-prepares, the prime compares the new state with itself and sees no change. The result the
re-prepare then keeps still describes the audio before the edit.

**Reproduced through the processor** (48 kHz / 256; Haas, Amount 0.5, Drive 8, Width 1.0, Output Gain
−3 dB, Level Match on; stationary noise, converged 3 s; worklog §N1). A live Width 1.0 → 2.0 and a
same-rate `prepareToPlay` one block later kept −5.471 dB. A fresh instance at Width 2.0 measures
−7.728 dB, so the kept value played 2.25 dB loud at the first sample (1.84 dB·s over 3 s). The same
edit made while suspended reaches `prepare()` through the prime and flushes. Every measurement-input
category behaves the same way:

| live edit, then a re-prepare 5 ms later | kept (before this amendment): error at 0 s, over 3 s | flushed (the restore route) |
|---|---|---|
| Width 1.0 → 2.0 | 2.25 dB, 1.84 dB·s | 3.40 dB, 1.81 dB·s |
| Width 1.0 → 1.3 | 0.62, 0.56 | 2.02, 1.78 |
| Drive 8 → 12 | 1.55, 1.37 | 1.55, 1.37 (the predict floor sets both) |
| Drive 8 → 2 | 3.02, 1.66 | 0.43, 0.40 |
| Mix 1.0 → 0.5 | 2.25, 1.63 | 0.74, 0.65 |
| Amount 0.5 → 1.0 | 0.78, 0.75 | 2.18, 1.97 |
| Output Gain −3 → −9 (not a measurement input) | kept, 0.00 | kept, 0.00 |

Keeping is sometimes closer than flushing (Width 1.0 → 1.3) and sometimes 2.6 dB further (Drive 8 → 2).
Either way the kept value is not a measurement of the sound that plays, and that is what Q5 keeps a
result for.

**The invariant — the owner's boundary, adopted.** *A same-rate re-prepare may retain a published Level
Match result only when that result is still valid for the measurement state that the re-prepared engine
will use.* Valid here means **current**: the published value is the measure's answer for the inputs it
now reads, within the Level Match settle tolerance of 0.1 dB. There are four cases:
1. **An immediate re-prepare after a measurement-input change is not current.** This holds however the
   change arrived: live, at a duck bottom, by a forced swap, or adopted by the re-prepare's own
   `reset()`. The re-prepare flushes exactly as the same change made through the prime does.
2. **A change the measure has caught up with is current again**, and a re-prepare keeps it.
3. **A change to something the measurement does not read invalidates nothing.** These are the fields
   `measurementInputsDiffer` does not compare: Output Gain, Output Balance, Bypass, Band Solo, the
   Level Match switch, and an inert guarded field.
4. **A non-finite result flushes** (condition 4, unchanged). A flush publishes no measurement of any
   state, so it counts as current, and a re-prepare right after it is the same either way (0 dB).

**When a result is current again: the measure's own evidence, not a clock.** The engine calls
`LoudnessMatch::inputsChanged()` whenever it adopts a change to anything the measurement reads. From
then on, each audible block splits what the published value glides toward into two parts:
- **The measurement of the audio heard *since* the change.** The integrators are linear, so that audio's
  energy is exactly each integrator minus its value at the change, decayed as the integrator decays it.
  The measure's own target formula reads it.
- **Everything older**: the value at the change, and the targets that the pre-change energy still
  coloured. This part fades with the same glide.

The result is current again when both of these hold:
- the post-change measurements make up at least half of the published value, so their glide-weighted
  mean is itself a measurement rather than its first few milliseconds;
- the published value is within 0.1 dB of that mean, so everything older moves it by no more than the
  tolerance.

Silent blocks do not glide, so they confirm nothing. A block adds share only when the *input* heard
since the change is itself a measurement: its energy is above the silence gate and at least half of what
the dry integrator holds. The split is exact for the integrators, not for the pipeline in front of them.
Pre-change audio still in flight, in the dry reference's alignment and filters or in a delay line
emptying into the wet, arrives after the change and counts as post-change audio. Against no input, or
against a sliver of it, those tails are the whole ratio: the silence floor against a tail, or one tail
against another, often at the −24 dB clamp. The half keeps them a small part of what is read. The gate
is absolute because the half is relative: a host reset's `softReset()` empties the pre-change snapshot,
and after that any input is at least half. Both sides of the comparison are glide-weighted over the same
recent targets, so a programme whose loudness moves cancels out of it. There is no timeout. On audible
programme the older part decays geometrically with the glide, so currency always returns: 2–4 s after a
large edit, from ~0.6 s after a small one. During silence nothing moves.

How each transition affects currency:
- **Marked not current:**
  - a live edit on the Normal path;
  - a change heard during a duck (in a fade-in no bottom follows to report it);
  - every duck bottom that changes a measurement input (`measChangedAtBottom`: forced swaps and
    discrete changes);
  - a host reset that lands an in-flight duck carrying such a change.
- **Made current:** an A/B injection, because the slot's value is that slot's own measurement,
  restored with its state (a slot left before it had caught up is the recorded exception below).
  *Superseded by the amendment of 2026-09-25, A/B provenance, below:* made current only when the slot's
  record was measured, at this rate, for the measurement inputs it is restored into; otherwise not
  current.
- **Cleared:** a flush (`reset`).
- **Unchanged by `softReset()`:** both the currency and the post-change share survive, because it
  re-arms only the integrators, which then hold post-change audio alone.

`prepare()` also refuses to keep while a duck that changes a measurement input is in flight, because
its own `reset()` then adopts the change the duck would have reported at its bottom. On the processor
path that is an ordinary duck's continuous part, which went live at its entry (`duckMeasDirty`). On the
unprimed engine API it is also the duck's pending snapshot, which the processor's prime folds into
condition 3 (recorded in worklog §M5, closed here). Nothing in this amendment
changes what is published, when or how fast it moves, the re-arm rule, or the applied gain.

**Strategies compared (worklog §N3).**

| strategy | verdict |
|---|---|
| S0 — status quo | keeps Case 1 (the finding) |
| S1 — a sticky "dirty" flag cleared only by a flush or an injection | never keeps after any edit: the rule the owner excluded |
| S2 — time since the change | a timeout; the design has no time-based validity contract |
| S3 — the engine's smoothers settled | they settle in 20–120 ms; the measure needs seconds |
| S4 — a "measured-for" snapshot or generation | still needs a rule for when the measure has caught up, and its revert shortcut (an edit and back) is wrong while the analysis still holds the excursion |
| S5a — the post-change energy dominates the integrators and one block agrees within 0.1 dB | **rejected on measurement**. A host reset right after the edit empties the integrators, so dominance held trivially, and the first block's short-window target agreed by chance: current at block 0, 2.26 dB off (4.15 dB on modulated programme). On modulated programme the moving target crossed the gliding value at ~1.3 s, up to 1.33 dB off |
| S5, share from any post-change audio over the gate, input or wet | **rejected on measurement**. A Width edit at Drive 24 on modulated programme, with the input silent from the edit for 3 s or more: the Haas delay line emptied into the wet with no dry behind it. While the gate stayed open on the decaying pre-change energy, that tail's ratio to the silence floor agreed with the gliding value, and the result went current 2.7 s into the silence, 2.3 dB off the fresh reference. Test 69 (10a) pins the case on the matcher directly |
| S5, share from post-change input over the gate, without the half | **rejected on measurement**. Drive 8 → 2 with the input silent for 1 s from the edit, on noise, one seed of eight: the dry reference's tail held the post-change dry energy just over the gate (7 ppm of the integrator) for the first 0.26 s of the silence, against the Haas tail in the wet. That −24 dB target took up to a quarter of the share, and 0.43 s after the audio resumed the post-change mean met the gliding value 2.0 dB off the fresh reference. Test 69 (10c) pins it: without the half, 6 of its 25 runs go current up to 2.3 dB off |
| **S5 — the glide split above, share only from post-change input over the gate and at least half of the dry integrator (adopted)** | no early currency in 24 scenarios × 8 seeds × 2 programmes: currency no earlier than 0.63 s after a change, and at that moment within 0.15 dB of a fresh reference on noise, 0.19 dB on modulated programme. On two sweeps of a Width edit at Drive 0–24 dB, with the input silent for 1–8 s from the edit or after 2–40 blocks of audio (8 seeds × 2 programmes each), within 0.15 dB at currency and 0.16 dB after |
| S6 — publish the post-change measurement at the re-prepare instead of flushing | a candidate that would remove the trade-off below; not adopted, because it changes what is published |

**Measured on the implementation** (worklog §N4).
- **Processor, same setup.** Before the measure catches up, a re-prepare flushes exactly as the restore
  route does; after, it keeps. Error at the first sample against the fresh destination:

  | live edit | flushes while | keeps from | kept error |
  |---|---|---|---|
  | Width 1.0 → 2.0 | ≤ 3.2 s | 4.3 s | 0.04 dB |
  | Width 1.0 → 1.3 | ≤ 2.1 s | 3.2 s | 0.03 dB |
  | Drive 8 → 12 | ≤ 2.1 s | 3.2 s | 0.09 dB |
  | Drive 8 → 2 | ≤ 3.2 s | 4.3 s | 0.06 dB |
  | Mix 1.0 → 0.5 | ≤ 3.2 s | 4.3 s | 0.04 dB |
  | Amount 0.5 → 1.0 | ≤ 2.1 s | 3.2 s | 0.05 dB |
  | Output Gain −3 → −9 | never | at once | 0.00 dB |

  The "flushes while" and "keeps from" times are points of the probe's grid (0.005, 0.02, 0.1, 0.3,
  0.5, 1.1, 2.1, 3.2, 4.3, 6.4 and 8.5 s). A modulated programme keeps Width 1.0 → 1.3 and Amount
  from 2.1 s (0.12 / 0.14 dB off) and Drive 8 → 12 from 4.3 s (0.03 dB). Its other rows match noise,
  0.04–0.06 dB off.
- **Engine, the false-currency scan.** 24 scenarios × 8 seeds × {noise, modulated programme}: path
  changes, continuous edits, silence gaps of 0.1–5 s after the edit, and a host reset right after the
  edit. Currency never came earlier than 0.63 s after a change (S5a: at block 0). Two sweeps of the
  post-change gate add 210 rows: a Width edit at Drive 0–24 dB, with the input silent for 1–8 s from
  the edit or after 2–40 blocks of audio. None went current more than 0.16 dB off.
- **Nothing changes without a re-prepare.** Output samples and the published and applied gain per block
  are bit-identical before and after on 22 of 23 processor routes:
  - Apply, Undo and Redo;
  - engages;
  - A/B with its injection;
  - a preset load and a forced Undo;
  - host resets inside a fade;
  - a live script with a drag, a ducked algorithm change and host resets;
  - an A/B script;
  - a re-prepare with nothing changed, and one 6 s after a live edit.

  The 23rd route is the positive control, a live edit with a re-prepare 5 ms later.
- **Regression coverage:** Test 69 (the engine, and in its leg (10) the matcher directly) and State test
  133 (the processor). Against `0fbce03` their flush claims fail and every keep, premise and control
  passes (worklog §N5). Leg (10) drives the API this amendment adds, so it has no `0fbce03` run; each
  rejected gate above fails it.

**The trade-off (recorded).** A re-prepare between ~0.3 s and ~3–4 s after a large measurement-input
change now flushes where the partly converged value was closer: Width 1.0 → 2.0 at 1.07 s kept 1.07 dB
off, against 3.45 dB for the flush. Condition 3 already accepts the same trade-off for the restore route,
and S6 would remove it. Automation of a measurement input keeps the result not current, because each
block's change restarts it, so a re-prepare during such automation flushes.

**Recorded, not changed: a slot's remembered value is restored as current.** *(Resolved by the
amendment of 2026-09-25, A/B provenance, below.)* An A/B switch injects the
destination slot's remembered gain (#23), and the matcher takes it as current. A slot left within
~2–4 s of a measurement-input edit remembered a value that was still converging, and a re-prepare right
after switching back keeps it. Measured through the processor (worklog §N6): slot B at Drive 12, left
0.3 s after its edit, kept 1.09 / 0.61 / 0.12 dB off at 0.1 / 0.6 / 2.1 s after the switch back. The
flush there plays 7.61 dB off at its first sample, 1.41 dB·s over 3 s against the keep's 0.13–0.97.
A never-visited slot, and both slots after a session restore, inject 0.0 dB (ER-STATE-20's
fresh-instance value) the same way. Requiring the measure to confirm a restored value first would close
both cases, but it has two costs:
- it flushes a converged slot's value for the first ~0.6 s after every switch;
- it fails Test 67 leg (5), whose probes displace the value by an engine-API injection before a
  re-prepare, and State test 132 leg (6).

Carrying each slot's currency with its remembered value would close both cases without those costs.
The value crosses from the message thread to the audio thread with the injection, though, so that needs
a second cross-thread field: a threading-model change, outside this finding. It is recorded as a
candidate, not adopted.

**What this changes in the text above.**
- **The transition table:** the result column now says where a transition leaves the result not current.
  The keep row requires a current result, and the flush row includes a re-prepare made before the
  measure has caught up.
- **Q5:** gains condition 5.
- **The F13(2) amendment's "Measured on the implementation" row** "re-prepare, same rate, same or doubled
  block size" stands. It was measured on a converged result, which is current.

**Architecture Review Gate — owner authorization of 2026-09-25.**

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | the PR #156 body and the implementing commit message: a change to an **Accepted ADR** (Q5's keep conditions) |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's authorization of 2026-09-25**: *"You are explicitly authorized to make the owner decision based on the measured evidence and your own recommendation."* The boundary: *"A same-rate re-prepare may retain a published Level Match result only when that result is still valid for the measurement state that the re-prepared engine will use."* Excluded: *"Do NOT implement a simplistic rule such as: 'Any live measurement-input edit permanently invalidates `keepMatch`.'"* Review of the implementation: pending, PR #156 |
| 3 | if the change is a decision, an ADR is added/updated | this Amendment, in place, as the two above |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered**: no parameter ID, range, default, automation flag, serialization field or reported-latency value changes, and no DSP node, stage order, thread or cross-thread path changes. The new state is written and read on the audio thread and on the prepare and reset paths that already write the matcher, which JUCE never runs concurrently with `process()` |

Related code (this amendment): `src/dsp/LoudnessMatch.h:94-103` (`inputsChanged`, `isResultCurrent`;
`setDisplayedGainDb` at `:73-79` makes the result current, since the A/B provenance amendment only when
its caller says the value is measured); `src/dsp/LoudnessMatch.cpp:213-264`
(the currency bookkeeping); `src/dsp/AnamorphEngine.cpp:61-65` (`keepMatch`: `adoptsMeasChange` and
`isResultCurrent`); `:239-244` (a host reset's adoption); `:791` and `:880-882` (a live edit, a change
heard during a duck); `:1264` (the duck bottom).

## Amendment, 2026-09-25 — an A/B slot's remembered value carries the validity of the result it was taken from (Devin review: unsettled A/B gain; "A/B provenance")

Devin's review of PR #156 found the case the amendment above recorded and did not change: *"Unsettled
A/B gain survives re-prepare."* `LoudnessMatch::setDisplayedGainDb` made every restored value current,
so a slot left before the measure had caught up came back current, and a same-rate re-prepare kept it
(condition 5 reads the currency, not the value).

**Reproduced through the processor** (48 kHz / 256; Haas, Amount 0.5, Drive 8, Output Gain −3 dB, Level
Match on; noise; worklog §O1). Slot B, a copy of converged A, is edited Drive 8 → 12 and left *t* later;
6 s on A; back to B; `prepareToPlay` at the same rate 4 blocks after the switch. The published value
right before the re-prepare is the same before and after this amendment at every *t*. What moves is
its label, and so the re-prepare's verdict:

| B left after its edit | recorded when left | before: kept, error at the first sample (3 s) | after |
|---|---|---|---|
| 0.1 s | not current | kept, 1.596 dB (1.407 dB·s) | flushed (1.418 dB·s) |
| 0.3 s | not current | kept, 1.526 dB (1.335 dB·s) | flushed (1.406 dB·s) |
| 0.6 s | not current | kept, 1.252 dB (1.102 dB·s) | flushed (1.408 dB·s) |
| 1.1 s | not current | kept, 0.801 dB (0.708 dB·s) | flushed (1.403 dB·s) |
| 2.1 s | not current | kept, 0.286 dB (0.263 dB·s) | flushed (1.408 dB·s) |
| 3.2 / 4.3 / 6.0 s | measured | kept, 0.086 / 0.025 / 0.014 dB | kept, the same, bit for bit |

**Root cause: a validity-state error, not convergence lag.** The trace of the 0.3 s case, block by block:
- when B is left, the published −6.0728 dB is **not current**: a fresh B measures −7.608;
- the processor stores the value alone (`abMatchGain[1]`);
- at the return's silent bottom the injection writes it back, and `setDisplayedGainDb` sets the result
  **current**;
- the re-prepare 4 blocks later passes condition 5 and keeps −6.0844 against a fresh −7.6109.

The value was never wrong for what it was: a measure 0.3 s into converging. The label was. It described
a measurement that had not happened. A never-visited slot shows the same thing without any edit: it
injects 0 dB (ER-STATE-20's fresh-instance value), is made current, and is kept 4.57 dB off.

**The invariant — adopted.** *A remembered A/B Level-Match gain carries the validity state of the
measurement result that produced it. Restoring the value and restoring its validity are separate.* The
value is restored as it always was: audio, the published value and the applied gain are unchanged on
every route. Its validity is restored from the record, and is decided where that record's evidence
lives.

**Why "current" is not enough to record, and what is.** Current, in the amendment above, says the
published value describes no *previous* state. A flush is current by that definition: it "clears the
question" because it describes no state at all. That is right in the flush's own context. The first
block's predict floor lands on the flushed 0 dB, and a re-prepare right after the flush is the same
either way. It is not portable: a record taken in the first seconds after a flush and restored later,
somewhere the floor does not fire, carries 0 dB or a half-converged value as current. Measured (State
test 134, w5z): A/B pressed before the first block after the first prepare, back to that slot 5.5 s
later, re-prepared — the 0 dB record was kept 4.63 dB off. The record therefore carries a
stricter bit, **measured** (`LoudnessMatch::isResultMeasured()`), which implies current. It says the
measure has *confirmed* the published value for the inputs it now reads, by the same criterion that
makes a changed result current again (the post-change measurements at least half of the value, and the
value within 0.1 dB of their mean). It is:
- cleared by a flush (`reset`) and by `inputsChanged()`;
- cleared by the predict floor when the floor lowers a measured value. After a restore the floor can do
  that in the bottom's own block, when the destination's Drive or Mix is higher than the source's, and
  the published value is then a prediction, like a flush's, not the slot's measurement;
- kept by `softReset()`;
- set by the confirmation, or by a restore whose record says so.

`isResultCurrent()` is unchanged, so condition 5 and every re-prepare verdict that involves no A/B
restore are unchanged.

**The record, and where it lives.** Each slot's record has four fields:
- the value published when the slot was left;
- whether it was measured;
- the adopted state (`EngineParameters`) it was measured for;
- the sample rate it was measured at.

The record belongs to the **engine** (`AnamorphEngine::abMemory`), not the processor. All four fields
are audio-thread state; reading the currency from the message thread is a data race, and the fields
must be read together. Capture and restore both run on the audio thread (or the prepare path), where
the fields are written.
- **Capture:** when the engine takes the switch (`takeRequests`, at the top of `setParameters`, or in
  `primeParameters` before it adopts the prime), it records the slot being left from its own `p`, `sr`
  and matcher.
- **Not measured during a duck in flight** that has made a measurement-input change live which only its
  bottom will report (`duckMeasDirty`, the rule `prepare()` already applies). Otherwise the record
  would pair the pre-change result with the post-change `p`: an ordinary duck opened by a
  non-measurement discrete change together with a Width edit, the switch inside its fade-out, was kept
  2.13 dB off (State test 134, window (g)).
- **Not captured** while a restore is still armed, because the engine never adopted the slot that
  switch was going to, and not in the word that forgets.
- **Restore:** at the forced bottom the destination's record is adopted, value always. It is measured
  only if it was measured, at the rate the engine runs at now, for the measurement inputs it is
  restored into (`measurementInputsDiffer`: the same tolerant comparison condition 3 uses, so the
  preset round-trip drift of the log-mapped crossovers and Mono Maker Freq, measured at up to 12 ulp,
  is not a difference). Otherwise it is restored **not current** (`inputsChanged()`), and the measure
  confirms it from there.
- **Re-arm (P1b, F13(2) Q1):** unchanged, and separate. The re-arm answers where the *analysis* came
  from (the source slot's audio, re-armed when the two slots' measurement inputs differ). The
  measured bit answers where the *value* came from. Neither reads the other.

**The handoff: one existing atomic, no new path.** The processor's `abSwitchToAdopted` calls
`engine.requestAbSwitch (from, to)` where it called `requestDuck()`, and no longer stores or injects
a gain. The request travels in the existing `std::atomic<int> duckRequest`, taken whole with one
relaxed `exchange` per block, as before. It now carries:
- bit 0: the forced duck;
- bit 1: an A/B switch, with the two slot indices in bits 4–7 and 8–11;
- bit 2: forget.

The writers never clobber one another:
- `requestDuck()` is a `fetch_or`;
- `requestAbSwitch()` is a CAS that keeps a pending switch's source, so A → B → A before any block
  leaves B's record untouched (the processor used to overwrite it with A's live value, 1.61 dB);
- `forgetAbMatchMemory()`, from `adoptRestoreTail` (ER-STATE-20), is a CAS that keeps a pending duck
  and drops a pending switch.

The message thread reads nothing of the engine's for a switch, and no audio-thread function waits,
locks or allocates. The new audio-path functions are in `AnamorphEngine.cpp`, where the realtime lint
reaches them from `setParameters` and `process` (a seeded lock is reported). They copy one 132-byte
trivially copyable snapshot per switch. Under ThreadSanitizer, 5.2 M message-thread requests against 4000 audio blocks with a prime and a
prepare every 500 gave no report; the same harness with the record written from the message thread gave
four. `injectMatchGainDb` stays as an engine API whose caller asserts
the value is measured (Tests 67 and 69 drive it); the processor no longer calls it.

**Every capture and restore path, traced** (worklog §O3):

| path | record taken | restored | measured when restored |
|---|---|---|---|
| `abSwitchTo` / `abToggle` (both reach `abSwitchToAdopted`) | the slot left, at the next block | at the forced bottom | if recorded measured, same rate, same measurement inputs |
| a second switch before the first is taken (A → B → A) | the source of the pending one, once | the final destination | as above; the slot in between keeps its record |
| a switch while a restore is armed | none | the new destination | as above |
| `abCopyToOther` onto the inactive slot | unchanged (its state moves, its record does not) | at the next switch to it | no, if the copy moved a measurement input |
| the shared Oversampling setting changed on the other slot | — | at the next switch | no (`oversample` is a measurement input) |
| an edit to the slot not yet adopted when it was left, or made during the return fade | — | — | no (the recorded state differs from the adopted one) |
| a switch the prime takes (`prepareToPlay` with a switch pending) | from the state before the prime | on the first block (the duck was dropped) | at the same rate as above; at a new rate no |
| a new sample rate | kept, stamped with its rate | — | no at the new rate; yes again back at the rate it was measured at |
| a host `reset()` with a switch in flight | as taken | on the next block | as above |
| `setStateInformation` (`adoptRestoreTail`) | every record forgotten: 0 dB, not measured; a pending switch dropped | — | no |
| NaN self-heal, first prepare, any flush | the live result becomes current, not measured | — | a slot left before the measure confirms is not measured |
| `injectMatchGainDb` (engine API) | — | at the next forced bottom, or the next block | yes, by the caller's word |

**Strategies compared** (worklog §O4; three adversarial reviews, measured on scratch prototypes):

| strategy | verdict |
|---|---|
| A — a boolean stored with the value on the message thread | wrong in every window below: the message thread cannot read the currency without a race, and a flag copied at the switch describes the wrong moment in (a), (b), (d), (e) and (f) |
| B1 — a per-change generation | misses (d)–(f): the slot's state changes without a matcher event |
| B2 — a bitwise fingerprint of the measurement inputs | false "not current" on 4–5.5 % of converged round trips (the 12-ulp crossover drift) unless it re-keys through the tolerance. The re-keyed design ("B2-exact") needs a second list mirroring `measurementInputsDiffer` and a 64-bit published word. Its injection can land after the bottom, un-re-armed, and drift 0.53 dB while labelled measured |
| C — derive at restore: every restored value waits for the measure's confirmation | flushes a converged slot's value for ~0.6 s after every switch; fails Test 67 (5)(b) and Test 69 (5a) / (6b) |
| **D — the engine keeps the record (adopted)** | the restore is armed with the duck and consumed at the bottom with P1b, so no late landing. First prototype refuted twice: it recorded `isResultCurrent()` (the flush and floor cases, up to 4.72 dB) and missed `duckMeasDirty` (window (g), 2.26 dB). Both are closed here by the measured bit and the in-flight-duck rule |

**Measured on the implementation** (worklog §O6):
- **Windows** (the same processor probe before and after; re-prepare 4 blocks after the return):

  | window | before | after |
  |---|---|---|
  | (a) an edit to B not yet adopted when B is left | kept 1.599 dB | flushed |
  | (b) an ordinary duck in flight when B is left (Haas → Velvet) | kept 1.715 dB | flushed |
  | (c0) A → B → A before any block, later back to B | B restored A's value, kept 1.609 dB | B's own value, kept 0.009 dB |
  | (c1) the same with one block between | kept 1.608 dB | kept 0.007 dB |
  | (d) Copy A onto a visited B, then to B | kept 1.900 dB | flushed |
  | (e) Oversampling changed on A (2× / 4× / 8×) | kept 0.565–0.636 dB | flushed |
  | (f) an edit to B during the return fade | kept 1.049 dB | flushed |
  | a never-visited slot | kept 4.574 dB | flushed |
  | control: B converged, nothing changed | kept 0.015 dB | kept 0.015 dB, bit for bit |

- **The processor contract** (State test 134: 58 heap processors on one seeded stream, each claim
  beside an event-matched control and a fresh processor at the destination; re-prepare 4 blocks after
  the return; the pre-fix column is the same test built against `20af101`):

  | case | before | after |
  |---|---|---|
  | B left 0.1 / 0.3 / 1.1 s after Drive 8 → 12 | kept 1.61 / 1.54 / 0.81 dB off | flushed; after the measure re-confirms on B, kept 0.017 dB off |
  | B left 6 s after the edit (measured) | kept, 0.008 dB | the same |
  | an ordinary duck carrying Width 1 → 2 in flight when B is left (window (g)); control: the duck alone | kept 2.13 dB off; control kept | flushed; control kept |
  | Width 1 → 2 one block before leaving; Output Gain or Output Balance instead | kept 2.13 dB off; kept | flushed; kept |
  | A left before the first block after the first prepare / 0.3 s after it; control: left at 4.5 s | kept 4.63 / 1.06 dB off; kept | flushed; kept |
  | the floor lowers a measured restore (B at Drive 24, returning from Drive 8), left 4 blocks later; control: B at Drive 4 | kept 1.27 dB off; kept | flushed; kept |
  | identical slots, B not measured (only visit 0.3 s); a never-visited slot | kept 1.48 / 4.63 dB off | flushed |
  | B measured at 48 kHz restored at 44.1 kHz; the 48 → 44.1 → 48 round trip | kept; kept | flushed; kept |
  | a switch the prime takes, at a new rate / at the same rate | kept; kept | flushed; kept |
  | a session restore, then the first switch | kept 1.59 dB off | flushed |
  | a switch, then a session restore, in one turn | the previous project's B value injected into the restored A (1.91 dB off) | the switch dropped; A keeps its own |
  | the switch and an Undo on B in one turn | kept 1.91 dB off | flushed |
  | off-grid crossover / Mono Maker frequencies moved by the round trip | kept | kept |
  | a kept measured restore resumed at −70 dBFS, negative (−5.50 dB) and positive (+7.79 dB) match | — | the applied gain from the first sample (max \|a − P\| 0.0000 over 0.12 s); the stale twins flush |
- **Nothing else moves.** Output, published and applied gain are bit-identical per block on all 23
  processor routes of the live-edit amendment's harness, A/B script included. DSP suite output is
  byte-identical, and the finite-parameter hashes are unchanged (186 of 186). Two audible changes remain,
  both where the old record's provenance was wrong:
  - (c0) / (c1): the value restored for B is B's own, where the processor used to hand it A's;
  - a switch pending when a session restore lands is dropped, where the old injection carried the
    previous project's slot value into the restored session.
- **Regression coverage.** Test 70 (the engine and the matcher, 62 checks) and State test 134 (the
  processor, 135 checks). The API Test 70 drives does not exist at `20af101`: the HEAD-equivalent
  composite fails 15 of its checks, `20af101`'s engine behind a shim 23. State test 134 against `20af101`
  fails 29: the flush claims and the two provenance corrections. Every mutant of the record, the request
  word, the matcher's bit and the re-arm is rejected; two variants survive by design (worklog §O7).
  State test 132 leg (6)'s A/B pair plays a 4 s pre-roll: its premise, a kept re-prepare after an early
  visit, had been a flush-current capture.
- **The trade-off (recorded).** A slot left before its measure confirms now flushes at a re-prepare
  right after the return, as the live edit does since the amendment above. At *t* = 1.1–2.1 s the kept
  value was closer (0.26–0.71 dB·s against the flush's 1.40). So is a slot visited within ~2.7 s of any
  flush, including A/B pressed right after playback starts. With switching faster than confirmation
  (~0.6 s at best), a restored not-measured value never confirms: nothing carries evidence across
  visits. All three are conservative and cost only at a re-prepare.

**Recorded, not changed.**
- **The predict floor over a measured restore (audio).** Returning to a slot whose Drive or Mix is
  higher, the floor replaces the slot's measured value with the prediction in the bottom's block; the
  measure then eases it back. Skipping the floor for a measured restore would keep the slot's level.
  That is an audible change, and a candidate.
- **Copy carrying the live record.** A Copy's destination could take the source's live record: same
  state, same measurement. It would remove the flush after a Copy and the lurch HEAD already plays
  there. It changes what is restored, so it is a candidate.
- **Evidence across visits, cross-rate retention, and the measure's own scope.** Currency is a
  function of the measurement inputs and the rate: a programme change, or a bus-layout change before a
  same-rate re-prepare, is the measure's ordinary tracking, as for any converged result. The P1b
  re-arm's first blocks can move a measured restore 0.3–0.8 dB (shared with HEAD).

**What this changes in the text above.**
- **The amendment above, "Made current: an A/B injection":** now *made current only when the slot's
  record was measured, at this rate, for these measurement inputs*. Its "Recorded, not changed: a
  slot's remembered value is restored as current" is resolved here. The candidate it names,
  "carrying each slot's currency with its remembered value", is adopted without the second
  cross-thread field it assumed: the record stays on the audio thread.
- **The F13(2) transition table's two A/B rows:** the result is the slot's remembered value, current
  only as above.
- **Q1:** the re-arm is now performed by the engine's restore (`adoptRememberedMatch`), at the same
  bottom, on the same `measChangedAtBottom`.

**Architecture Review Gate — owner authorization of 2026-09-25.**

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | the PR #156 body and the implementing commit message: a change to an **Accepted ADR** (the amendment above's A/B currency and its recorded case) and to the payload of an existing cross-thread atomic |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's authorization of 2026-09-25**: *"Do not ask for another owner decision unless the repository contains genuinely contradictory requirements that make a technically defensible decision impossible."* The boundary: *"A remembered A/B Level Match gain must carry the validity state of the measurement result that produced it."* and *"Do not simply add a boolean because it is easy if the actual lifecycle requires stronger provenance."* The threading constraints: *"No audio-thread mutexes."*, *"No new audio-thread shared mutable state that introduces a race."*, *"Do not make the Processor depend on engine-thread-only mutable state without an established handoff mechanism."* Review of the implementation: pending, PR #156 |
| 3 | if the change is a decision, an ADR is added/updated | this Amendment, in place |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered**: no parameter ID, range, default, automation flag, serialization field (the record was never serialized) or reported-latency value changes, and no DSP node or stage order changes. The thread model gains no thread, no direction and no ordering. The existing relaxed request word carries two slot indices and a forget bit (`THREAD_MODEL.md`, `THREADING_POLICY.md`), and the processor's `abMatchGain[]` becomes engine state written and read only where the matcher is. |

Related code (this amendment): `src/dsp/LoudnessMatch.h` (`setDisplayedGainDb (db, measured)`,
`isResultMeasured`, `inputsChanged`), `src/dsp/LoudnessMatch.cpp` (`reset`; the predict floor's
un-measure; the confirmation that sets `resultMeasured`), `src/dsp/AnamorphEngine.h`
(`requestAbSwitch`, `forgetAbMatchMemory`, `requestDuck`, `AbMatchMemory`, the request-word bits),
`src/dsp/AnamorphEngine.cpp` (`requestAbSwitch`, `forgetAbMatchMemory`, `takeRequests`,
`restoreAbSlot`, `adoptRememberedMatch`, and the two consumers in `process`), `src/PluginProcessor.cpp`
(`abSwitchToAdopted`, `adoptRestoreTail`). Tests: Test 70, State test 134; State test 132 leg (6)'s
pre-roll.

## Note, 2026-09-25 — a gain-only engage lands on the published value, current or not (Devin review: "Level Match engages on stale compensation"; "stale engage")

Devin's review of `1c22d51` (PR #156) found *"Level Match engages on stale compensation"* at
`src/dsp/AnamorphEngine.cpp:1270-1271`. The sequence it gives:
1. With Level Match off, a live edit to something the measurement reads leaves the result not current
   (`inputsChanged`; the Amendment of 2026-09-25, live edits).
2. Level Match is turned on before the measure has caught up.
3. At the bottom, `p` and `pendingP` both hold the edit, so `measChangedAtBottom` is false.
4. Case A lands the published value, which *"belongs to the previous sound"*.

The review proposed adding `&& loudness.isResultCurrent()` to the landing.

This note decides the finding under the owner's authorization. It changes no behaviour. The engine's two
comments at the bottom, the matcher's currency comment (`LoudnessMatch.h`) and this ADR's Case A text now say
what the landing asks (worklog `NONFINITE_PARAMETERS_AND_F13.md` §P).

**Reproduced.** Processor at 48 kHz / 256: Haas 50 %, Drive 8, Output Gain 0, Level Match off; a Width 1.0 → 2.0
gesture; the toggle 0.3 s later (State test 135 leg (1)).
- Before the edit the result is current, and the published value −5.517 dB moved 0.003 dB over the
  0.5 s before it: converged.
- The edit leaves it not current, and it is still not current at the toggle and at the bottom.
- At the bottom `measChangedAtBottom` and `procChanged` are false, so the landing fires. The bottom block
  plays −5.761 dB; a fresh processor at Width 2.0 publishes −7.792 dB.
- The result is current again 3.28 s after the edit (engine reproduction).

The code does what the finding says.

**What the landed value is.** The matcher runs whether or not Level Match is on, and a Case-A switch changes
nothing it reads. The value the engage lands is therefore the value Level Match on throughout publishes at
that block.
- **Engine.** Within 1.6e-5 dB in 85 of 85 landings. The whole published trajectory is bit-identical to a
  twin taking the same forced ducks, with two exceptions: Multiband on (≤ 4.2e-4 dB, the documented H4
  reference switch), and a forced bottom the twin does not share (≤ 0.005 dB, its module restarts).
- **What that value is.** Not yet current, it still partly describes the sound before the edit
  (`LoudnessMatch.h`; the Amendment of 2026-09-25, live edits). That is exactly what Level Match on
  throughout is publishing at that moment, and gliding its applied gain toward.
- **Why this is not a validity error.** A validity error is a result carried out of the context its
  analysis measured: a same-rate re-prepare after the audio stopped, or an A/B record restored into another
  slot or at another rate. Those are what currency governs (the two amendments of 2026-09-25). A Case-A
  landing carries the result nowhere. It moves only the applied gain onto the published trajectory, and
  leaves the result, its currency, its measured bit and the analysis untouched. Test 71 and State test 135
  read the result as FLUSHED right after the landing block: the landing validates nothing.
- **Processor.** 665 engages were measured, 505 of them landings on a non-current result. HEAD's
  mean |applied − fresh| over the 500 ms after the bottom is never more than 0.07 dB above the always-on twin's,
  and its peak never more than 0.12 dB above. In 73 rows it is more than 0.07 dB *below* the twin's (up to
  0.79 dB), where the twin's own applied gain still trails its published value. The engine set measures
  that trail at the bottom: up to 0.14 dB after a Width edit, 0.15–0.51 dB after Drive and Mix cuts, and
  2.10 dB 50 ms after a Drive 0 → 12 pre-duck.

The distance from a fresh instance is the measure's own convergence lag, the lag this ADR's Context
accepts. The landing only removes the twin's trail behind it.

**Enabling Level Match vs. having a current result to land.** Turning Level Match on asks one question:
where does the applied gain start? A current result answers a different one: may the result be carried
into another context?
- **Case A does not guarantee a current result.** The measure may still be converging on an edit made
  before the switch.
- **It does not need one.** Before the measure catches up, the only other start this engine has is Case B's
  unity, which is no measurement of anything. The glide from unity then heads for the same non-current
  published value anyway, and follows it within ~0.5 s (the smoother's τ ≈ 120 ms, restarted each block).
- **Case B is unchanged.** An engage that also changes what the measurement reads still starts from unity.
- **A/B provenance stays separate.** An A/B restore still takes priority over the landing, and restores its
  record's validity on its own terms. The landing never promotes a restored value: State test 135 leg (4),
  a not-measured slot record, is FLUSHED after the toggle's landing.

**The options, measured.**

The engine set is 75 non-current landings:
- Haas, Velvet, Chorus, Dimension D, Multiband and Mono Maker;
- Width, Drive, Mix, Haas Delay, Multiband band-width and Mono Maker frequency edits;
- forced engages and Undo of Apply.

The processor set is 665 engages, 505 of them non-current landings:
- Width edits at three input correlations and two Drives;
- algorithm changes, and cuts of Drive, Mix, Amount and Width;
- Mono Maker off, edits in silence, host resets, Undo sequences, and A/B with a not-measured record;
- on correlated, uncorrelated and anti-correlated programmes.

Its re-prepare and NaN self-heal rows leave the result current and fall outside the 505.

Per option: mean |applied − fresh| after the bottom over the window stated, and the signed peak.

- **A — land only when current (the review's proposal).** The engage glides from unity instead, so its
  error at the bottom is −G against the fresh value G.
  - **Where it is worse:**
    - Over 500 ms, the engine set's 75 rows: worse in 54, better in 21.
    - The processor set's 505 rows: HEAD better in 246, A in 225, 34 ties.
    - **The review's own case:** 2.02 → 5.90 dB mean over 120 ms, peak +2.04 → +7.61 dB.
    - **An edit the measure barely feels** (Haas Delay 15 → 30 ms, 0.05 s): 0.00 → 3.69 dB over 120 ms,
      peak +5.34 dB. The result is not current, yet it is within 0.02 dB of the fresh value: currency is
      not accuracy.
    - **Undo of Apply after an edit and its Undo:** 0.22 → 3.63 dB over 120 ms, peak +5.33 dB. KI-031's
      swell returns.
  - **Where it is better:**
    - On the engine set, only after edits that make the sound quieter, engaged within ~1–3 s. For example,
      over 500 ms at 0.05 s, Drive 8 → 4 goes 2.22 → 1.44 dB and Mix 1 → 0 goes 5.23 → 3.79 dB. Where the
      match is a cut, A then errs loud where HEAD errs quiet.
    - On the processor set, also in the positive class below. For example, anti-correlated Width 0 → 2 at
      0.02 s goes 7.22 → 5.62 dB, and uncorrelated goes 5.35 → 4.44 dB.
    - In 149 of the 505 rows, A is better on both mean and peak without being louder.
  - **It fails Test 66 (6 checks).** That test's lanes engage 1.9 s after a measurement-input change,
    0.134 dB from settled and not current. A turns that residual into a +4.85 dB glide.
- **B — land only when measured.** A measured result is always current, so B is identical to A on every
  non-current landing. It differs only on a current value the measure has not confirmed, such as a flush's,
  and there it refuses the landing: a +7.63 dB peak after a same-rate re-prepare.
- **C2 — land, but when the result is not current start a boost above the level heard at the level heard.**
  It removes the landing's own step in the positive class below. It is worse in 23 rows, all on the quiet
  side (down to −7.45 dB), and it fails 3 Test 66 checks: legs (2) and (11), and the landing-liveness count.
- **Keep (adopted).** The engage lands on the value Level Match on throughout publishes. The error is the
  measure's.

**Decision.** Keep the landing. **Q4 stands**: it was decided by the F13(2) amendment and is re-examined here
against the currency the matcher now reports.
- **The finding describes the code but is not a defect of it.** The landed value is still partly the
  previous sound's, because the measure has not caught up, but it is the value Level Match on throughout
  publishes. The landing carries it into no other context.
- **The proposed guard trades one error direction for the other without an overall gain.** It is worse in
  54 of the engine set's 75 rows and roughly evenly split on the processor set. After louder, trivial and
  forced engages it errs loud by the whole match.
- **It breaks accepted coverage.** It reopens KI-031 on Undo of Apply after an edit, and Test 66 already
  pins that landing.
- **No timeout, second tolerance or new flag is added.** Currency is the only validity state this ADR
  defines, and it is not the question the landing asks.

**Recorded, not changed.**
- **The positive stale step.** Sometimes the non-current published value is a boost above the level heard
  with Level Match off: an edit made the sound louder while the match was boosting, such as a narrowed image
  widened at Drive 0. The landing then steps up to that stale boost:
  - **anti-correlated programme, 21 ms after the edit:** +6.00 dB above the level heard before, and
    +10.12 dB over the fresh value −4.12 dB (Test 71 leg (4));
  - **uncorrelated programme:** +4.25 dB;
  - **correlation 0.3 or more:** at most 0.6 dB more than a glide.

  This is KI-031's signature, louder than both the level before and the level after. Level Match on
  throughout publishes the same surge: the predict reads only Drive and Mix, so a Width or narrowing change
  that raises the output is never pre-ducked (KNOWN_ISSUES KI-030). C2 is the measured candidate; it is not
  adopted because it fails Test 66.
- **The silence→audio edge snap** lands the published value too, current or not. It fires at every
  silence→audio edge with Level Match on: after a host reset, when audio resumes after an edit made in
  silence, and on an engage whose bottom meets an audio edge. This is the Decision's own snap, unchanged,
  and it lands the same trajectory.
- **A 5 ms boundary between Case B and Case A.** An edit and the toggle in one block give Case B, a glide
  from unity; one block apart they give Case A, a landing. By this Note's argument a Case-B bottom's
  published value is the on-throughout trajectory too. Its unity start is kept by Q3, the owner's decision
  on measurement: a Case-B landing measured better in one direction and worse in the other two (worklog §K5,
  P3). That value is no further from the trajectory.

**What this changes in the text above.**
- **The O4g amendment, Case A.** "The published value still describes the sound that plays after the
  bottom" is now spelled as what it asks: the matcher is already measuring that sound, converged or not.
  Its "What this changes" bullet names the landing's question accordingly: *does the switch change what the
  measurement reads*.
- **"The boundary with F13(2)."** Its "is not decided here" now points to Q4 and to this re-examination.
- **The F13(2) table's Q4 row, and "Q2, Q3, Q4 — unchanged, decided".** Both now point to this note.

**Regression coverage.**
- **Test 71** (the engine, 27 checks) and **State test 135** (the processor, 23 checks).
- **Premises, each asserted.**
  - The result is current before the edit, and converged there (the published value moved ≤ 0.02 dB over
    the 0.5 s before it).
  - The edit reached the engine.
  - The result is not current after the edit, at the engage and right before the bottom.
  - The bottom is at event + 2.
- **Claims.**
  - The bottom block plays the value it publishes. That value is ≥ 3 dB from unity and ≥ 0.5 dB from a
    fresh instance.
  - The published trajectory is the on-throughout lane's, and the applied gain joins that lane's.
  - The landing validates nothing.
  - The result recovers, and a new rate still flushes.
- **Controls.** A current result (O4g), an edit the measurement does not read, a flush's current value,
  Undo of Apply on a stale result (no swell), and a not-measured A/B record. Case B is read after its
  fade-in, where only the bottom's report can make it not current.
- **Mutants rejected** (worklog §P6): A, B, C2, a landing that marks the result current, no landing, a
  landing blind to `measChangedAtBottom`, and an ordinary duck's bottom that does not report its
  measurement change. No suite covered that last gap before this Note's Case B verdict.

**Architecture Review Gate — owner authorization of 2026-09-25.**

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | the PR #156 body and the commit message: a text change to an **Accepted ADR** (Case A's premise, the F13(2) boundary, the Q4 row); no production code changes beyond three comment edits (two in `AnamorphEngine.cpp`, one in `LoudnessMatch.h`; object code byte-identical) |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's authorization of 2026-09-25**: *"Treat this as a real production bug unless investigation disproves it on the current head."* *"Prove that: … the observed incorrect gain is attributable to that landing rather than normal Level Match convergence."* *"Do not wait for another owner decision. You are explicitly authorized to make the owner decision based on the measured evidence and repository contract."* Review: pending, PR #156 |
| 3 | if the change is a decision, an ADR is added/updated | this Note, in place: the decision it records (Q4) stands |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered**: no parameter, schema, thread, DSP-order or latency change; no behaviour change |

Related code (this note): `src/dsp/AnamorphEngine.cpp:1263-1271` (the bottom's answer and the landing
predicate), `:1959-1960` (the landing), `:1993-1994` (the edge snap); `src/dsp/LoudnessMatch.h:94-103`
(`inputsChanged`, `isResultCurrent`). Tests: Test 71, State test 135.

## Consequences
- No drift on silence; no ratchet; no Mix=100% slam; unbiased at unity.
- Deliberately **not** a continuously-adapting AGC.

## Related code
- `src/dsp/LoudnessMatch.cpp:16-46` (K-weighting), `:85-106` and `:142-181` (predict; the floor's
  un-measure), `:183-209` (measure/hold), `:108-117` (`softReset`: the analysis only), `:65-79` (`reset`:
  both halves), `:213-264` (the result's currency and its confirmation; the amendments of 2026-09-25)
- `src/dsp/AnamorphEngine.cpp:1834-1835` (A(dry) reference), `:1954` (the measurement), `:1993-1994`
  (silence-edge snap), `:598-683` (the A/B record: request, forget, capture, restore)
- `src/PluginProcessor.cpp:455` (`applyAutoGain`)

Evidence [Verified]:
- Source: src/dsp/LoudnessMatch.cpp:16-267
- Tests: testLevelMatchUnity, testLevelMatchNoRatchet, testLevelMatchMixCouplingNoSlam,
  testLevelMatchSilenceFreeze, testMultibandUnityMatch
- History [Partially Verified]: CHANGELOG.md [0.8.1]
- Related incident: `../../POSTMORTEMS.md` INC-002 (Level Match ratchet / Mix=100% slam)
