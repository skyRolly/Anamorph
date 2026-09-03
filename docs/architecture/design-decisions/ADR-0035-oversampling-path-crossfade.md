# ADR-0035 — The oversampling path swap is a crossfade, not a ducked switch

**Status:** Accepted (Signal Flow change — maintainer instruction 2026-09-03)

**Depends on [ADR-0034](ADR-0034-latency-follows-the-oversampling-factor.md)**, which is what makes
this possible, and **supersedes its Decision point 5** (the dry-filled ordinary duck). **Amends
[ADR-0004](ADR-0004-clickfree-transition-strategy.md)** by moving one transition from its duck class
to its crossfade class; every other transition keeps the mechanism ADR-0004 assigned it.

## Context

`osActiveFor` decides whether the oversampling wrap RUNS — the largest single CPU saving in the
engine, and untouched here and by ADR-0034. When it flips, the whole nonlinear region moves between
the resampled path and the base-rate path. That flip was **latched** and routed through the
click-free switch duck (#3), on the reasoning that a duck masks a discrete change.

## Problem

**The duck could not mask it.** The duck's gain is applied at the OUTPUT stage — downstream of
`HaasProcessor` (a 1–35 ms delay line, 12 ms by default) and `VelvetNoise` (a sparse FIR spanning
~21 ms). The handover's discontinuity therefore entered those delay lines **at full level**, and
re-emerged one widener-delay later, with the ~28 ms fade-in already finished and nothing left to
hide it.

Reported by the maintainer against the ADR-0034 tree:

> *"There is still an audible interruption / momentary silence when Oversampling is enabled (2×, 4×
> or 8×), the Widen algorithm is Haas or Velvet Noise, and Drive is changed from 0 to non-zero, or
> from non-zero to 0."*

Measured on a 220 Hz tone at 48 kHz, as a multiple of the settled sample-to-sample step, with the
arrival time given relative to the parameter move:

| | Haas | Velvet |
|---|---|---|
| Drive 0 → 6 dB | **2.62×** at +896 smp | **5.77×** at +1330 smp |
| Drive 6 → 0 dB | **1.16×** at +896 smp | **2.42×** at +1330 smp |
| Oversampling **Off** control | 2.00× / 0.96× | 1.89× / 1.00× |

+896 samples is **320 (the duck bottom, quantised to the block grid) + 576 (Haas's 12 ms delay)**,
exactly; +1330 is the same sum with Velvet's tap spread. The offset does **not** move with the
oversampling factor, which is what identifies the widener's delay line rather than the latency as
the carrier.

Two independent sources fed it, one in each direction:

1. **Leaving the wrap.** ADR-0034's stand-in ring was **cleared** at the duck bottom, so its first
   `lat` samples read back as **zeros** — a hole this change introduced.
2. **Entering the wrap.** The wrap's polyphase IIR starts from zero state and ramps in over its own
   group delay. Pre-existing. Removing the `reset()` does not help and was measured not to: a wrap
   that has not run is already at zero state.

## Options

- **A. Reset Haas and Velvet at the duck bottom too.** Nothing stale would re-emerge — because their
  delay lines would be empty. That replaces a few-sample discontinuity with a 12–35 ms hole in the
  widened signal. **Rejected.**
- **B. Lengthen the duck to cover the widener's delay.** A ~50 ms duck on an ordinary knob move, and
  still wrong for a 35 ms Haas setting. **Rejected.**
- **C. Make the handover continuous, and stop ducking it.** Chosen.

## Decision

1. **`osBlend`**, a 12 ms `SmoothedValue` — the same sample-safe ramp Multiband Enable uses — mixes
   the two paths: 0 = base-rate, 1 = wrapped. `osActiveFor(p)` is now its **target**, read live, and
   nothing is latched from it.
2. **The two paths are sample-aligned, which is why they may be mixed at all.** ADR-0034 gave both
   the same latency — the wrap's group delay on one side, `osCompDelayBuffer` on the other. Before
   it they differed by 4–6 samples and a crossfade would have combed. **This ADR is not available
   without ADR-0034.**
3. **The stand-in ring is written on every block**, whichever path is audible, and read back only
   when the base-rate path is. A cleared or cold ring is precisely source (1); a warm one hands back
   real history. Write-only costs two vector copies — the trade the true-bypass ring already makes.
   The ring now carries the **raw** input (delay, then region) so its history means the same thing
   whether or not the region ran.
4. **The wrap starts running while the blend is still ~0** and is reset at that instant — the
   `mbRunning` pattern, for the same reason: its settle from zero state happens under a ~0 gain.
   It goes cold once the blend has fully left it, which is what preserves the CPU saving.
5. **Both paths run from the same drive envelope.** `processNonlinearRegion` advances `driveSmooth`
   and `driveBlendSmooth` per sample, so the wrapped call advances them `factor` times as far over
   one block. With only ever one path running that was invisible; running both makes it a real
   divergence. The wrapped call is therefore given an `envStride` of the factor during a blend, so
   each path advances the envelope exactly `n` times, and both start the block from the same state.
   Measured before this: 2.0× / 4.0× / 7.3× at 2×/4×/8× on an instantaneous step — **scaling with
   the factor**, which is the signature. `envStride == 1`, every other caller, is the original loop
   unchanged.
6. **The mod algorithms stay on the wrapped path during a blend.** There is one `ChorusEngine`
   instance; running it from both paths in a block would advance its LFO and delay state twice.
7. **`osActiveFor` leaves `discreteDiffers`.** A Drive crossing no longer opens a duck of any kind.
   An oversampling **FACTOR** change still does — that one moves the reported latency, so the two
   paths are not aligned and cannot be crossfaded.
8. **A duck bottom that changed the OS path SETTLES the blend, it does not carry it across.** Both
   ducked routes into this stage — the ordinary factor change and the forced bulk swap — land
   `osBlend` (and `osRunning`) on the state they have just adopted, next to the oversampler, chorus
   and stand-in-ring resets that already happen there. A blend left in flight across that bottom
   spans two states it was never valid for: point 2 above only licenses mixing the paths **while
   they are sample-aligned**, and a factor change is precisely the transition that breaks the
   alignment. In the `-> Off` direction it is worse than misaligned — see point 9.
9. **The blend may never weight a path that does not exist.** `currentOversampler()` returns null
   for exactly one state, Oversampling **Off**, and the pointer — not the blend — is the authority
   on whether a wrapped buffer exists. It is read once at the top of the stage and the blend is
   forced to agree with it, so `wrapAudible` implies a live oversampler and a stale weight can only
   ever degrade the output to the correctly-processed **base-rate** path, never to unprocessed
   audio. `p.oversample` is discrete, so this can only fire at a silent duck bottom — the same
   instant point 8 lands the blend deliberately.
10. **A forced swap settles the blend at its silent bottom**, where every other control is already
   snapped. Without it the fade-in mixes in from the base-rate path with the drive smoothers snapped
   to a large new value — ~12 ms of the nonlinear stage running **undersampled**, the one thing the
   wrap exists to avoid. Measured: worst step 2.70× without the settle, 2.03× with it, which is
   exactly the pre-ADR-0034 figure for that swap.

## Consequences

- **The Drive crossing is now indistinguishable from having no oversampling.** All 24 combinations
  of {2×, 4×, 8×} × {Haas, Velvet} × {0 → 6 dB, 6 → 0 dB} × {instantaneous step, 300 ms knob sweep}
  match their Oversampling-Off control to two decimal places on the discontinuity and to ~0.1 % on
  the level. Test 53.
- **No duck at all on that transition**, so ADR-0034's dry-filled ordinary duck (its Decision point
  5) is retired along with the duck it filled. There is nothing left to fill.
- **The CPU saving is intact and re-measured.** 48 kHz / 128, `working` chain at Drive 0 with a
  linear algorithm: OS Off 159.19, 2× 170.06, 4× 166.60, 8× 169.44 ns/sample — inside the 5–9 %
  run-to-run spread — against the engaged 210.44 / 266.13 / 375.85. The wrap runs during a
  crossfade, which is ~12 ms per crossing.
- **ADR-0034's latency behaviour is unchanged and re-verified**: the reported number is still a
  function of the Oversampling selection alone, constant across the whole
  {factor} × {algorithm} × {drive} grid (Test 52 legs A and A2), and the chain still carries it
  (leg B), with the skipped state still bit-identical to the OS-off output delayed (leg C).
- **One A/B case improves markedly.** A forced swap (A/B, preset, undo) that crosses the Drive
  threshold with a factor selected fell to **−54.9 dB** before ADR-0034; it is **−3.5 dB** now, the
  same as the Oversampling-Off baseline for that swap.
- **`osEngaged` is gone**, replaced by `osRunning` (does the wrap run this block) plus the blend.
  Nothing else read it.
- **Points 8 and 9 answer a defect this ADR's first implementation shipped with, in the one
  direction it had not been measured: 2×/4×/8× → Off.** The factor change ducks, so the new state
  is adopted at the silent bottom; `osActiveFor` is false for Off, so the blend's target became 0
  while its current value was still 1; and `currentOversampler()` is null for Off, so the wrapped
  path was never computed. The mix therefore ran toward an `osPathScratch` holding nothing but the
  raw input, and for the 12 ms of the ramp the Drive stage and the modulation algorithms were
  absent from the output — at full level into Haas's and Velvet's delay lines, under a fade-in that
  had barely started. Measured with a 1 kHz mono probe at Drive 18 dB, Haas, as the third-harmonic
  ratio H3/H1 (a ratio of two bins, so the duck's fade cancels out of it), relative to the duck's
  silent bottom:

  | ms from bottom | −4 | 0 | +4 | +8 | +12 | +16 | +20 |
  |---|---|---|---|---|---|---|---|
  | 2×/4×/8× → Off, before | 0.288 | **0.103** | 0.195 | 0.261 | 0.288 | 0.289 | 0.288 |
  | 2×/4×/8× → Off, after | 0.288 | 0.289 | 0.289 | 0.289 | 0.289 | 0.289 | 0.288 |
  | 2× → 4× control | 0.288 | 0.288 | 0.288 | 0.288 | 0.288 | 0.288 | 0.288 |

  The **control is the same duck** — a factor→factor switch, with the same reported-latency step and
  the same resets, differing only in that the wrap runs on both sides of it — and it is flat, which
  is what attributes the collapse to the handover rather than to the duck. The output level tells
  the same story: 0.014 at the bottom before, 0.022 after, against the control's 0.022.
- **The other three directions are unchanged, measured rather than assumed.** Off → 2× and Off → 8×
  are byte-identical before and after on both traces and on the worst step (×1.04), and the two
  factor→factor controls likewise; the forced-swap table above reproduces to four decimal places on
  every one of its seven rows. What point 8 does change in the Off → factor direction is invisible
  on this probe and worth having anyway: the fade-in no longer plays ~12 ms of the nonlinear stage
  running at the base rate, which is the same argument point 10 makes for the forced swap.

## Related code

- `src/dsp/AnamorphEngine.cpp` — the OS stage's two paths and their mix; `discreteDiffers`;
  `currentOversampler`; `processNonlinearRegion`'s `runMod` / `envStride`; the forced-swap settle.
- `src/dsp/AnamorphEngine.h` — `osBlend`, `osRunning`, `osPathScratch`.

Evidence [Verified]:
- Test: `tests/dsp_tests.cpp :: testDriveCrossingIsSeamlessWithOversampling` (Test 53) — **26 of its
  56 checks fail against the pre-change engine and 0 after**
- Test: `tests/dsp_tests.cpp :: testOversamplingOffHandoffKeepsProcessing` (Test 54) — points 8/9;
  **12 of its 32 checks fail against the engine before those points and 0 after**. Its thresholds are
  calibrated against a factor→factor control run rather than against constants, and it covers both a
  linear widener (Haas) and a mod algorithm (Chorus), each with Drive, since the drive stage and the
  mod algorithms share the one wrapped buffer and stand or fall with it
- Probe: `AnamorphTests --os-off-probe` (the H3/H1 table above, all seven switch directions)
- Probe: `AnamorphTests --forced-swap-probe` (the A/B table above, measured on three engine versions)
- Measurement: `AnamorphBench` §"Oversampling SELECTED but skipped, Drive 0"
