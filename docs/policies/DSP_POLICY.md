# DSP_POLICY.md

**Priority: 3.** System Policy — system-level DSP invariants extracted from the code, the
architecture docs, and the ADRs. These must hold across releases.

## Invariants (binding)

1. **The signal chain is strictly serial, in this fixed order** (see `SIGNAL_FLOW.md`):
   Input conditioning → Effect engine (Drive → algorithm → global Width → Multiband) →
   Dry/Wet Mix → Mono Maker → Output stage → Band Solo monitor → metering.
   Evidence [Verified]: src/dsp/AnamorphEngine.cpp:583-896.

2. **Mono Maker runs post-Mix, in place.** It collapses the lows of the *mixed* signal so the
   low end is mono at any Mix amount. (ADR-0006) Evidence: AnamorphEngine.cpp:761-766; test
   `testMonoMakerPostMix`.

3. **Band Solo is post-everything and monitoring-only.** It never changes any effect stage;
   `mask==0` → bit-exact true output; it is **called every block**, and its click-free
   crossfade advances on every block in which any gain is unsettled. Once fully settled at
   passthrough, the per-sample work is skipped and the filter bank goes cold; re-entry resets
   the filters and snaps the cutoffs to their targets under the ~12 ms crossfade (settled fast
   path, 0.8.9 / H1; since 0.8.10 cutoff changes are a bounded-time one-pole glide, with a
   single bank crossfade for multi-octave jumps). (ADR-0004/0006) Evidence: AnamorphEngine.cpp:878-894 (call site + invariant
   comment); SoloMonitor.h:22-38 (crossfade + settled fast path); SoloMonitor.cpp (gate +
   cold re-entry); tests `testSoloMonitor`, `testSoloNoGhostInSilence`,
   `testMultibandSplitDragNoPitchShift`.

4. **The effect engine is solo-agnostic** — the Multiband always sums every active band.
   Evidence: MultibandWidth.h:29-32.

5. **Oversampling wraps only the nonlinear/modulation stages** (Drive, Chorus, Dimension-D);
   linear stages stay outside; OS off ⇒ 0 latency. (ADR-0003) Evidence:
   AnamorphEngine.cpp:14-23.

6. **Mono compatibility by construction.** Width/decorrelation modify only the Side; `L+R = 2·Mid`
   always. Band-split stages are Linkwitz-Riley applied identically to L and R (allpass-flat Mid).
   Evidence: MidSide.h:38-40; VelvetNoise.h:13-18; MultibandWidth.h:24-27.

7. **The dry path is delay-compensated and phase-matched** to the wet (A(dry)); `Mix=0` is a
   bit-exact null. (ADR-0005) Evidence: AnamorphEngine.cpp:726-759; test `testMultibandMonoCompat`.

8. **Identity at zero.** Every widening algorithm is identity at `amount = 0`; global `Width = 1`,
   `Drive = 0 dB`, and `Mix` neutrality are identities (transparent on load). Evidence:
   HaasProcessor/VelvetNoise/ChorusEngine headers; test `testTransparentDefault`.

9. **Crossovers are Nyquist-clamped + ordered top-down; no output clipper; NaN/Inf self-heals.**
   (ADR-0009) Evidence: MultibandWidth.cpp:55-71; AnamorphEngine.cpp:847-870; tests
   `testCrossoverAutomationSafe`, `testNoBadSamples`.

10. **Level Match measures the post-Mono-Maker output against A(dry)**, and is an absolute
    Measure+Predict (never a continuous AGC). (ADR-0007) Evidence: AnamorphEngine.cpp:768-785.

## Enforcement

- Any change to stage order, stage placement (esp. invariants 1–6), oversampling scope, or the
  crossover safety clamps is a **DSP signal-flow change** → Architecture Review Gate + ADR + an
  **AI Agent Hard Stop**.
- A change must keep the relevant self-tests green (`docs/policies/TESTING_POLICY.md`).
- Changing this policy requires an ADR.
