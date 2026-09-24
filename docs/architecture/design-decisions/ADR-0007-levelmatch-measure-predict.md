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

- **Case A — the switch changes nothing the Level-Match measurement reads.** The published value
  still describes the sound that plays after the bottom, so the fade-in starts from it: right after
  that block's `loudness.process` the smoother is landed, current and target, on the target the
  level-match stage has just computed (`src/dsp/AnamorphEngine.cpp:1810`). This is an **alignment
  of an existing result, not a new measurement**: nothing in `LoudnessMatch` is reset, re-armed,
  written or read differently. Case A holds only when all of these do:
  1. nothing the measurement reads differs between the state heard before the switch and the state
     adopted at the bottom — `! measurementInputsDiffer (p, pendingP)` (`:527`);
  2. no such change was made live during the switch's own fade-out — `! duckMeasDirty`: an ORDINARY
     duck applies its continuous controls at once (`copyContinuous`), so by the bottom `p` already
     carries them and the comparison above cannot see them (`:638`, `:747`);
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
measured in worklog §J, and is not decided here.

**What this changes in the text above.**
- *Decision* ("A silence→audio edge snaps the applied gain …"): still true, and no longer the only
  point where the applied gain lands — it now lands at a silence→audio edge, at an A/B injection, and
  at a Case-A bottom. Everywhere else it glides.
- *Note of 2026-09-22* ("re-armed in exactly one place"; "must NOT re-measure"): unchanged. A Case-A
  landing re-arms nothing, and `measurementInputsDiffer` is not a re-arm trigger. The two functions ask
  different questions — *did the signal path change* (re-arm) and *does the published value still
  describe the sound that will play* (land).
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

Related code (this amendment): `src/dsp/AnamorphEngine.cpp:527` (`measurementInputsDiffer`),
`:638` and `:747` (`duckMeasDirty`), `:1128` (the decision at the bottom), `:1810` (the landing);
`scripts/check-state-coverage.py` (`MEASUREMENT_INPUTS`).

## Consequences
- No drift on silence; no ratchet; no Mix=100% slam; unbiased at unity.
- Deliberately **not** a continuously-adapting AGC.

## Related code
- `src/dsp/LoudnessMatch.cpp:15-43` (K-weighting), `:74-95` (predict), `:131-156` (measure/hold)
- `src/dsp/AnamorphEngine.cpp:1272-1305` (A(dry) ref + silence-edge snap)
- `src/PluginProcessor.cpp:402-424` (`applyAutoGain`)

Evidence [Verified]:
- Source: src/dsp/LoudnessMatch.cpp:15-156
- Tests: testLevelMatchUnity, testLevelMatchNoRatchet, testLevelMatchMixCouplingNoSlam,
  testLevelMatchSilenceFreeze, testMultibandUnityMatch
- History [Partially Verified]: CHANGELOG.md [0.8.1]
- Related incident: `../../POSTMORTEMS.md` INC-002 (Level Match ratchet / Mix=100% slam)
