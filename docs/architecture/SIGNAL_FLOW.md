# SIGNAL_FLOW.md

The absolute processing order inside `AnamorphEngine::process` and the invariants that
order guarantees. Reorder constraints are formalised in `DSP_GRAPH_REFERENCE.md`; the
order itself is a binding invariant (`docs/policies/DSP_POLICY.md`).

Evidence [Verified] for the entire chain:
- Source: src/dsp/AnamorphEngine.cpp:806-1561 (`process`)
- Source: src/dsp/AnamorphEngine.h:23-42 (chain header comment)
- Tests: tests/dsp_tests.cpp :: testMonoMakerPostMix, testSoloMonitor, testMultibandMonoCompat,
  testLevelMatchAndSolo, testNoClicksAcrossTransitions

## Block-level order

```
Raw stereo input (mono upmixed to stereo by the wrapper)
  │
  0. Input level tap                          levels.input.process
  │
  0b. True-bypass dry capture (RAW input,     bypassDelayBuffer
      delay-aligned to wet latency)
  │
  1. Input conditioning                        applyInputConditioning
       channel kill / Swap / Balance / polarity / (M/S decode if msMode) / Mono
  │
  1b. M/S Solo (isolate Mid or Side)
  │
  -- capture the conditioned input ONCE (dryScratch): the dry source for the
     dry/wet Mix, and the same buffer the silence-edge scan reads
  │
  2. Effect engine
       2a. Oversampled nonlinear region:        Drive (tanh) -> Chorus/Dim-D
           (only Drive>0 or mod algorithm engages OS; else base rate)
       2b. Linear algorithm at base rate:       Haas OR Velvet
       2c. Global Width (MS-domain)             applyWidth
       2d. Multiband Width (1-4 bands)          multiband.processBlock
           (click-free mbEnableBlend output crossfade; bank kept warm)
  │
  3. Dry/Wet Mix (delay-compensated,
     phase-matched A(dry))
  │
  4. Mono Maker (post-Mix, in place)           monoMaker.process
  │
  5. Output stage
       Level Match measure (post-Mono-Maker) -> Output Gain / Auto Gain / Output Balance
       + click-free switch duck (raised cosine)
  │
  6. Band Solo monitor (POST-EVERYTHING)       soloMonitor.process
       band-passes the produced output to the soloed band(s); monitoring only
  │
  6b. NaN/Inf self-heal (per-sample)
  │
  7. Bypass crossfade (processed <-> raw)      bypassBlend
  │
  8. Metering tap (scope + correlation + out)
```

Every line number above lives **here instead of in the diagram**, once, path-qualified — so the
citation gate can see it and `--fix` can maintain it. The diagram carries the order and the symbol,
which is what it is for and what does not rot.

| Stage | Source |
|---|---|
| 0 · Input level tap | src/dsp/AnamorphEngine.cpp:957 (`levels.input.process`) |
| 0b · True-bypass dry capture | src/dsp/AnamorphEngine.cpp:959-1030 (`bypassDelayBuffer`) |
| 1 · Input conditioning | src/dsp/AnamorphEngine.cpp:1033 (`applyInputConditioning`) |
| 1b · M/S Solo | src/dsp/AnamorphEngine.cpp:1038-1041 |
| — conditioned-input capture | src/dsp/AnamorphEngine.cpp:1049-1050 (`dryScratch`) |
| 2a · Oversampled nonlinear region | src/dsp/AnamorphEngine.cpp:1052-1099 |
| 2b · Linear algorithm at base rate | src/dsp/AnamorphEngine.cpp:1101-1103 |
| 2c · Global Width (MS-domain) | src/dsp/AnamorphEngine.cpp:1105-1119 (`applyWidth`) |
| 2d · Multiband Width | src/dsp/AnamorphEngine.cpp:1121-1207 (`multiband.processBlock`) |
| 3 · Dry/Wet Mix | src/dsp/AnamorphEngine.cpp:1209-1317 |
| 4 · Mono Maker (post-Mix) | src/dsp/AnamorphEngine.cpp:1324 (`monoMaker.process`) |
| 5 · Output stage | src/dsp/AnamorphEngine.cpp:1326-1452 |
| 6 · Band Solo monitor | src/dsp/AnamorphEngine.cpp:1470 (`soloMonitor.process`) |
| 6b · NaN/Inf self-heal | src/dsp/AnamorphEngine.cpp:1472-1522 |
| 7 · Bypass crossfade | src/dsp/AnamorphEngine.cpp:1524-1549 (`bypassBlend`) |
| 8 · Metering tap | src/dsp/AnamorphEngine.cpp:1551-1560 (`scope.pushBlock`) |

## Invariants (must hold; each is testable)

| Invariant | Where enforced | Test |
|---|---|---|
| **Mono Maker runs POST-Mix**, on the mixed signal, in place. | src/dsp/AnamorphEngine.cpp:1324 (`monoMaker.process`) | testMonoMakerPostMix |
| **Band Solo is the very last audio stage and is monitoring-only** — it never changes any effect stage; `mask==0` → bit-exact true output. | src/dsp/AnamorphEngine.cpp:1470 (`soloMonitor.process`); src/dsp/SoloMonitor.h:12-16, 33 | testSoloMonitor, testSoloNoGhostInSilence |
| **Effect engine is solo-agnostic** — the Multiband always sums every band; solo is a downstream monitor. | src/dsp/MultibandWidth.h:43-45 | testSoloMonitor (energy-transparent) |
| **Dry path is delay-compensated** to the wet (oversampling) latency. | src/dsp/AnamorphEngine.cpp:1209-1317, getLatencySamples | testBypassNullAndLatency |
| **Dry path is phase-matched** through the same crossovers as the wet (A(dry)) so a partial Mix never combs the mono sum. The reconstruction is gated off in the settled-full-wet state (Mix exactly 1, Match off, no crossfade — Wave 2 / H4) and re-engages phase-matched on a Mix dip. | src/dsp/AnamorphEngine.cpp:1121-1207, 1209-1317 | testMultibandMonoCompat, testDryAlignGateRecomb |
| **Mix = 0 is a bit-exact null** (smoothstep clean→aligned crossfade over first ~5% of Mix). | src/dsp/AnamorphEngine.cpp:1222, 1256-1276 (`kAlignMix`) | testBypassNullAndLatency / testTransparentDefault |
| **Oversampling wraps only Drive + Chorus/Dim-D**; linear stages stay outside; OS off ⇒ 0 latency. | src/dsp/AnamorphEngine.cpp (`osActiveFor`, the wrap and its `else` arm) | testBypassNullAndLatency |
| **Reported latency follows the SELECTED FACTOR, not the wrap's engagement** (ADR-0034). Where the wrap is skipped for want of nonlinear work, a 2-channel integer ring stands in for its group delay **in the wrap's own place in the chain**, so the five `-lat` ring reads below measure from an unchanged point and no parameter can move the host's PDC. | src/dsp/AnamorphEngine.cpp (`osCompDelayBuffer`, `osLatencyFor`) | testOversamplingLatencyIsFactorOnly |
| **Bypass is a click-free crossfade to the delay-aligned RAW input**, not a mute; chain + analysis always run. | src/dsp/AnamorphEngine.cpp:959-1030, 1524-1549 (`bypassBlend`) | testBypassCrossfadeClickFree, testLevelMatchRunsInBypass |
| **Level Match measures the post-Mono-Maker output vs the delay-aligned reconstruction A(dry).** | src/dsp/AnamorphEngine.cpp:1336-1341 (`loudness.process`) | testLevelMatchUnity, testMultibandUnityMatch |

## Notes

- **M/S domain.** When `msMode` is on, Input conditioning decodes Mid/Side→L/R inside
  `applyInputConditioning`; Balance/polarity act in the M/S domain before decode.
  Source: src/dsp/AnamorphEngine.cpp:650-712 (`applyInputConditioning`); the M/S
  decode branch is src/dsp/AnamorphEngine.cpp:687-700.
- **Discrete switches** (algorithm/routing/band-count/oversampling-path) are applied at the
  silent bottom of a raised-cosine duck (fade-out ~6 ms, fade-in ~28 ms). Bypass, Multiband
  Enable, and Band Solo are **not** ducked — they use their own click-free crossfades.
  Source: src/dsp/AnamorphEngine.cpp:276-316 (`discreteDiffers`); the duck fade
  times at src/dsp/AnamorphEngine.cpp:82-83 (`switchIncOut`); the Multiband Enable
  crossfade at src/dsp/AnamorphEngine.cpp:1136-1207 (`mbEnableBlend`); Band Solo at
  src/dsp/AnamorphEngine.cpp:1470 (`soloMonitor.process`); the Bypass crossfade at
  src/dsp/AnamorphEngine.cpp:1524-1549 (`bypassBlend`).
- **Forced bulk swaps** (undo / redo / A/B / preset — `requestDuck()`) run the same duck, but
  its output is **dry-filled**: stage 5 crossfades toward the delay-aligned raw input (the
  true-bypass ring) instead of dipping to silence, so the swap is heard as a short dip to the
  dry signal, never a dropout. The fill is presented at the **output-stage gain heard when the
  duck began** (`dryDuckGainL/R`, latched at fade-out entry like `dryDuckLat`): the ring holds
  the unity-level input, and at an extreme Output Gain (e.g. −24 dB) an unscaled fill burst in
  up to 24 dB louder than the surrounding audio (fixed 0.8.10; unity gain/balance is
  bit-identical to the original arithmetic — `testDryFillRespectsOutputGain`, Test 30). The
  swap still lands at processed weight 0 (smoother snap + wholesale node reset unchanged). A
  latency-crossing forced swap keeps the original duck-to-silence (the ring read offset would
  jump at full dry weight). Ordinary discrete ducks are unchanged (bit-exact). True bypass
  still presents the ring at unity — bypass semantics untouched. Source:
  src/dsp/AnamorphEngine.cpp (`dryDuck`/`dryDuckGain`; gate at the ring fill, blend in the
  stage-5 output loop); guarded by `testForcedSwapNoDropout` (Test 26) and Test 30.
- **Who raises a forced duck, and when** (2026-09-03, ER-GUI-06). A forced duck belongs to a swap
  that is actually happening, so `requestDuck()` for a preset load is raised by the LOAD PATH —
  the processor's `PresetManager::onAboutToLoad` hook — and not by the UI that asked for the load.
  That hook is the only point that is both **after every check that can refuse** (a missing factory
  id, an unparsable file, and since ER-STATE-24 a foreign root) and **before the first parameter
  moves**, which is what the mask depends on. Until this was moved the editor raised it at the call
  site, so a load the manager then REFUSED still dry-filled the next ~32 ms to mask a swap that had
  not occurred — measured as an engaged widener's side energy at **0.4549 of the un-clicked
  control's** (State test 35), the same "duck whose swap already happened" fault
  `AnamorphEngine::primeParameters` documents for the activation route. Keeping the request in one
  place also keeps the three call sites — the preset menu, `Load Preset…`, and the prev/next
  buttons via `step()` — from drifting apart. **A successful load ducks exactly as before.**

Any change to this order or these invariants requires an ADR and Architecture Review
(`docs/policies/ADR_POLICY.md`, `docs/policies/ARCHITECTURE_REVIEW_GATE.md`).
