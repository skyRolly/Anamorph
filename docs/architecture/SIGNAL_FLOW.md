# SIGNAL_FLOW.md

The absolute processing order inside `AnamorphEngine::process` and the invariants that
order guarantees. Reorder constraints are formalised in `DSP_GRAPH_REFERENCE.md`; the
order itself is a binding invariant (`docs/policies/DSP_POLICY.md`).

Evidence [Verified] for the entire chain:
- Source: src/dsp/AnamorphEngine.cpp:685-1385 (`process`)
- Source: src/dsp/AnamorphEngine.h:22-41 (chain header comment)
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
| 0 · Input level tap | src/dsp/AnamorphEngine.cpp:821 (`levels.input.process`) |
| 0b · True-bypass dry capture | src/dsp/AnamorphEngine.cpp:823-892 (`bypassDelayBuffer`) |
| 1 · Input conditioning | src/dsp/AnamorphEngine.cpp:895 (`applyInputConditioning`) |
| 1b · M/S Solo | src/dsp/AnamorphEngine.cpp:900-903 |
| — conditioned-input capture | src/dsp/AnamorphEngine.cpp:911-912 (`dryScratch`) |
| 2a · Oversampled nonlinear region | src/dsp/AnamorphEngine.cpp:914-929 |
| 2b · Linear algorithm at base rate | src/dsp/AnamorphEngine.cpp:931-933 |
| 2c · Global Width (MS-domain) | src/dsp/AnamorphEngine.cpp:935-949 (`applyWidth`) |
| 2d · Multiband Width | src/dsp/AnamorphEngine.cpp:951-1037 (`multiband.processBlock`) |
| 3 · Dry/Wet Mix | src/dsp/AnamorphEngine.cpp:1039-1147 |
| 4 · Mono Maker (post-Mix) | src/dsp/AnamorphEngine.cpp:1154 (`monoMaker.process`) |
| 5 · Output stage | src/dsp/AnamorphEngine.cpp:1156-1282 |
| 6 · Band Solo monitor | src/dsp/AnamorphEngine.cpp:1300 (`soloMonitor.process`) |
| 6b · NaN/Inf self-heal | src/dsp/AnamorphEngine.cpp:1302-1346 |
| 7 · Bypass crossfade | src/dsp/AnamorphEngine.cpp:1348-1373 (`bypassBlend`) |
| 8 · Metering tap | src/dsp/AnamorphEngine.cpp:1375-1384 (`scope.pushBlock`) |

## Invariants (must hold; each is testable)

| Invariant | Where enforced | Test |
|---|---|---|
| **Mono Maker runs POST-Mix**, on the mixed signal, in place. | src/dsp/AnamorphEngine.cpp:1154 (`monoMaker.process`) | testMonoMakerPostMix |
| **Band Solo is the very last audio stage and is monitoring-only** — it never changes any effect stage; `mask==0` → bit-exact true output. | src/dsp/AnamorphEngine.cpp:1300 (`soloMonitor.process`); src/dsp/SoloMonitor.h:12-16, 33 | testSoloMonitor, testSoloNoGhostInSilence |
| **Effect engine is solo-agnostic** — the Multiband always sums every band; solo is a downstream monitor. | src/dsp/MultibandWidth.h:43-45 | testSoloMonitor (energy-transparent) |
| **Dry path is delay-compensated** to the wet (oversampling) latency. | src/dsp/AnamorphEngine.cpp:1039-1147, getLatencySamples | testBypassNullAndLatency |
| **Dry path is phase-matched** through the same crossovers as the wet (A(dry)) so a partial Mix never combs the mono sum. The reconstruction is gated off in the settled-full-wet state (Mix exactly 1, Match off, no crossfade — Wave 2 / H4) and re-engages phase-matched on a Mix dip. | src/dsp/AnamorphEngine.cpp:951-1037, 1039-1147 | testMultibandMonoCompat, testDryAlignGateRecomb |
| **Mix = 0 is a bit-exact null** (smoothstep clean→aligned crossfade over first ~5% of Mix). | src/dsp/AnamorphEngine.cpp:1052, 1086-1106 (`kAlignMix`) | testBypassNullAndLatency / testTransparentDefault |
| **Oversampling wraps only Drive + Chorus/Dim-D**; linear stages stay outside; OS off ⇒ 0 latency. | src/dsp/AnamorphEngine.cpp:21-25, 914-929 (`osActiveFor`) | testBypassNullAndLatency |
| **Bypass is a click-free crossfade to the delay-aligned RAW input**, not a mute; chain + analysis always run. | src/dsp/AnamorphEngine.cpp:823-892, 1348-1373 (`bypassBlend`) | testBypassCrossfadeClickFree, testLevelMatchRunsInBypass |
| **Level Match measures the post-Mono-Maker output vs the delay-aligned reconstruction A(dry).** | src/dsp/AnamorphEngine.cpp:1166-1171 (`loudness.process`) | testLevelMatchUnity, testMultibandUnityMatch |

## Notes

- **M/S domain.** When `msMode` is on, Input conditioning decodes Mid/Side→L/R inside
  `applyInputConditioning`; Balance/polarity act in the M/S domain before decode.
  Source: src/dsp/AnamorphEngine.cpp:529-591 (`applyInputConditioning`); the M/S
  decode branch is src/dsp/AnamorphEngine.cpp:566-579.
- **Discrete switches** (algorithm/routing/band-count/oversampling-path) are applied at the
  silent bottom of a raised-cosine duck (fade-out ~6 ms, fade-in ~28 ms). Bypass, Multiband
  Enable, and Band Solo are **not** ducked — they use their own click-free crossfades.
  Source: src/dsp/AnamorphEngine.cpp:223-250 (`discreteDiffers`); the duck fade
  times at src/dsp/AnamorphEngine.cpp:72-73 (`switchIncOut`); the Multiband Enable
  crossfade at src/dsp/AnamorphEngine.cpp:966-1037 (`mbEnableBlend`); Band Solo at
  src/dsp/AnamorphEngine.cpp:1300 (`soloMonitor.process`); the Bypass crossfade at
  src/dsp/AnamorphEngine.cpp:1348-1373 (`bypassBlend`).
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

Any change to this order or these invariants requires an ADR and Architecture Review
(`docs/policies/ADR_POLICY.md`, `docs/policies/ARCHITECTURE_REVIEW_GATE.md`).
