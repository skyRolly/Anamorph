# ADR-0034 — Reported latency follows the Oversampling **factor**, not the wrap's engagement

**Status:** Accepted (Latency change — maintainer instruction 2026-09-03)

**Amends [ADR-0003](ADR-0003-oversampling-strategy.md).** ADR-0003's engagement rule and its filter
choice stand unchanged; only its latency clause — *"When nothing nonlinear is active, reported
latency is 0"* — is reversed here. Nothing is superseded: the wrap still engages on exactly the same
predicate, for exactly the same reason.

## Context

`osActiveFor(e) = e.oversample != Off && (e.driveDb > 0.01 || isModAlgorithm(e.algorithm))` answers
four questions at once in the engine: does the resampling wrap RUN, what is latched into
`osEngaged`, does a change need a duck — and, until this ADR, **how much latency does the plug-in
report**. The first three are load-bearing. The fourth was an accident of the first.

## Problem

With a factor selected, moving **Drive** across 0.01 dB — or changing **Algorithm** into or out of
Chorus / Dimension-D — flipped the reported PDC between 0 and the factor's latency. Hosts answer a
latency change by restarting or re-priming the processing graph, so an ordinary knob move produced
an audible dropout. Reported by the maintainer, 2026-09-03:

> *"when Drive is at zero, the latency reported to the host is zero; then, when it is turned up
> slightly, it will report a latency value. During this adjustment, the plugin's own latency
> changes, causing an interruption / glitch in the sound."*

Measured on the pre-change build with `AnamorphTests --os-latency-probe` (48 kHz):

| factor | Drive 0.005 dB, Haas | Drive 6 dB, Haas |
|---|---|---|
| 2× | 0 | **4** |
| 4× | 0 | **6** |
| 8× | 0 | **6** |

The same step appears on an Algorithm change at Drive 0 (Haas → Chorus).

## Requirement

A latency change — and the glitch that follows it — is acceptable **only** at the moment the
Oversampling selection itself changes. After that, no parameter may move the reported number. The
CPU saving that skips the wrap for a linear chain must be **kept**.

## Options

- **A. Always run the wrap when a factor is selected.** Makes the number constant, and deletes the
  optimisation that motivates the predicate. Measured cost of the resampling round trip alone at
  48 kHz / 128 on the reference machine: 211.12 → 380.00 ns/sample at 2× → 8× against 152–160
  ns/sample with the wrap skipped. It also changes the sound of every linear chain, since the signal
  would then travel through the half-band IIR pair. **Rejected — it is the optimisation the
  requirement explicitly protects.**
- **B. Report the factor's latency always, and let the chain be shorter than it claims.** The host
  would compensate for a delay that is not there and every other track would sit N samples early.
  **Rejected — a reported latency the chain does not have is worse than one that moves.**
- **C. Report the factor's latency always, and stand in for the wrap's delay with a pure integer
  delay line while the wrap is skipped.** Chosen.

## Decision

1. `AnamorphEngine::predictLatency(e)` reads **`e.oversample` and nothing else**;
   `getLatencySamples()` reads **`p.oversample` and nothing else**. Both go through one helper,
   `osLatencyFor`, so they cannot drift apart. `oversample` is a discrete control, so the reported
   number can still only move at a `reset()` or a silent duck bottom — never mid-block.
2. A new 2-channel integer ring, **`osCompDelayBuffer`**, supplies the wrap's group delay in exactly
   the state where the wrap is skipped but a factor is selected. It sits **in the wrap's own place**
   in the chain — inside the `else` arm of `if (auto* os = currentOversampler())`, after
   `processNonlinearRegion` and before the linear-algorithm dispatch — so every downstream stage's
   alignment is identical whichever path ran, and the five existing `-lat` ring reads (the clean
   dry, the phase-matched `A(dry)`, the Level-Match reference, the true-bypass crossfade and the
   dry-filled duck) keep measuring from the same point.
3. `osActiveFor`, `osEngaged`, `currentOversampler()` and the `osActiveFor` term in
   `discreteDiffers` are **unchanged**. The wrap still runs only when it has work; the path swap is
   still ducked. What the duck no longer masks is a latency change, because there no longer is one.
4. The ring is flushed wherever the three oversamplers are: `prepare()`, `reset()`, the
   `osPathChanged` branch at the silent duck bottom, the forced-duck wholesale reset, and the
   NaN/Inf self-heal. It is allocated in `prepare()` only.
5. ~~**The duck around that crossing dry-fills instead of muting.**~~ **SUPERSEDED by
   [ADR-0035](ADR-0035-oversampling-path-crossfade.md)**, which removed the duck around this
   crossing altogether — so there is no longer a duck to dry-fill. The measurement below stands as
   the record of why the plain duck-to-silence was wrong; what it did not reach, and ADR-0035 did,
   is that the duck could not mask the handover at all, because its gain lands downstream of the
   wideners' delay lines. Kept verbatim: Holding the reported number still
   is only half of what the report asked for: the crossing is still a discrete PATH change, so it
   still opens the click-free duck, and an ordinary duck fades to **silence**. Measured with points
   1–4 in place and this one absent: an ordinary Drive move 0.4 → 0 dB with a factor selected drove
   the output to **−52.6 / −53.3 / −53.3 dB** at 2×/4×/8× and spent **6.7 ms more than 20 dB down**,
   inside a ~34 ms envelope — and not at all with Oversampling Off, which is the tell. So
   `discreteDiffers` is split: its `osActiveFor` term is separated from the rest as
   `discreteDiffersOther`, and the ordinary discrete branch dry-fills **only** when the OS path is
   the sole difference *and* the swap keeps the reported latency. Every other discrete duck is
   unchanged. **This is legal only because of points 1–2**: before them the crossing changed the
   reported latency, so a fill read at one fixed offset would have stepped by the latency delta.
   Post-fix the same move keeps **0.9556 / 0.9616 / 0.9616** of its settled level against a
   **0.9337** Oversampling-Off control.

## Consequences

- **The reported number now moves only with the Oversampling selection**, which is a Settings
  control (ADR-0010) and therefore never automated. No APVTS parameter can move it.
- **Selecting 2×/4×/8× now shows latency in the host even on a fully linear chain.** This is the
  price of the requirement and is visible to users: the user manual said the opposite in three
  places and is corrected in the same change. The Settings tooltip was corrected too and then
  **reverted on maintainer instruction** — tooltips were not in scope for this work — so it reads as
  it did before, `Off (1x) = no latency`, which remains true.
- **The CPU saving is intact and measured.** 48 kHz / 128 on the reference machine, the `working`
  chain with Drive 0 and a linear algorithm: OS Off 153.27, 2× 152.31, 4× 159.52, 8× 155.91
  ns/sample — inside the 6–11 % run-to-run spread of each other, and nowhere near the engaged rows
  (211.12 / 265.46 / 380.00). The stand-in ring's own cost is below this instrument's floor.
- **The output in the skipped state is bit-for-bit what it was, delayed.** The ring copies floats;
  it performs no arithmetic, so it cannot diverge across architectures and needs no twin-dump
  scenario.
- **A second click is removed with it, measured rather than assumed.** In true bypass the output IS
  the raw-input ring read at `-lat`, and the Bypass crossfade is applied *after* the switch duck's
  gain — so while fully bypassed the duck attenuates nothing, and a `lat` that moved on the Drive
  threshold jumped the read position by 4–6 samples at full level. Measured on the pre-change engine
  with a 220 Hz / 0.5 sine (largest possible smooth step 0.01440): worst sample-to-sample step
  **0.07162** at 2× and **0.09988** at 4× and 8×, swept over 16 start phases. After the change the
  worst step is exactly the smooth bound. Test 52 leg D.
- **TWO behaviour changes beyond latency, in opposite directions, both declared.** Both dry-fill
  gates (`predictLatency(target) == dryDuckLat`) reduce to *"the swap keeps the oversampling
  factor"*, so the latency-crossing set does not shrink — it **moves**:
  - *Gained.* A forced swap (A/B, preset, undo) that crosses the **Drive** threshold with a factor
    selected used to be latency-crossing — dry-fill disabled, duck to silence — and is now
    latency-neutral, so it keeps its dry fill. Measured, Off-baseline swap through that crossing:
    **−54.9 dB before, −3.5 dB after**. (The ordinary, non-forced duck this bullet also used to
    claim is gone entirely under ADR-0035.)
  - *Lost.* A forced swap that changes only the **oversampling factor** while Drive is 0 and the
    algorithm is linear used to be latency-neutral (0 → 0) and dry-filled; it is now
    latency-crossing (0 ↔ 6) and ducks to silence. Measured, Off → 4× at Drive 0 through a forced
    duck: worst level after the swap **−2.1 dB** before, **−54.2 dB** after. This is **permitted by
    the requirement** — it happens at the moment Oversampling is switched, which is the one moment
    an interruption is allowed — and it makes the two routes to an Oversampling change consistent,
    since the Settings menu itself has always been an ordinary duck-to-silence. It is recorded here
    rather than left to be discovered, and it is a candidate for a later, separately-gated
    improvement: latching the fill's read offset from the TARGET rather than the current state would
    restore seamlessness, at the cost of a 4–6 sample crossfade misalignment.
  The fill is otherwise still always read at the correct offset, because the offset it is gated
  against is the thing that did not change.
- **KI-027's remaining cost is retired.** The audio-thread re-report it describes still happens, but
  `setLatencySamples` now always finds the value unchanged and returns without notifying, so the
  lock / heap append / pipe write cannot be reached from a parameter move. **D-1's deferral
  mechanism is kept in full** — the Oversampling Setting is still writable from a host's own thread
  during `setStateInformation` (RISK-007), which is now the only value-changing requester.
- **Three state tests were re-instrumented, not weakened.** State tests 22, 24 and 27 drove latency
  through Drive because it was the only automatable latency-bearing control; they now drive the
  Oversampling Setting. State test 27's ER-STATE-14 leg would otherwise **hang**, not fail: its
  barrier is JUCE's change-only listener notification, and a Drive move no longer produces one.

## Related code

- `src/dsp/AnamorphEngine.cpp` — `osLatencyFor` / `getLatencySamples` / `predictLatency`; the
  stand-in ring in `process()`; the flush sites in `prepare()`, `reset()`, the duck bottom and the
  self-heal.
- `src/dsp/AnamorphEngine.h` — `osCompDelayBuffer` / `osCompDelayWrite`.
- `src/PluginProcessor.cpp` — `deliverLatency()` unchanged; the two APVTS listeners kept as the
  defensive re-derivation path, their rationale corrected.

Evidence [Verified]:
- Source: `src/dsp/AnamorphEngine.cpp`, `src/dsp/AnamorphEngine.h`
- Test: `tests/dsp_tests.cpp :: testOversamplingLatencyIsFactorOnly` (Test 52) — **12 of its 26 checks
  fail against the pre-change engine and 0 after** (leg E's three are what point 5 above fixes); its Leg C rejects option A by 1.31–1.57 absolute
  on a ±1 noise stream, measured by building that counterfactual
- Probe: `AnamorphTests --os-latency-probe` (the matrix above)
- Measurement: `AnamorphBench` §"Oversampling SELECTED but skipped, Drive 0"
