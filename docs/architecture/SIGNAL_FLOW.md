# SIGNAL_FLOW.md

The absolute processing order inside `AnamorphEngine::process` and the invariants that
order guarantees. Reorder constraints are formalised in `DSP_GRAPH_REFERENCE.md`; the
order itself is a binding invariant (`docs/policies/DSP_POLICY.md`).

Evidence [Verified] for the entire chain:
- Source: src/dsp/AnamorphEngine.cpp:493-949 (`process`)
- Source: src/dsp/AnamorphEngine.h:21-40 (chain header comment)
- Tests: tests/dsp_tests.cpp :: testMonoMakerPostMix, testSoloMonitor, testMultibandMonoCompat,
  testLevelMatchAndSolo, testNoClicksAcrossTransitions

## Block-level order

```
Raw stereo input (mono upmixed to stereo by the wrapper)
  │
  0. Input level tap                          levels.input.process            (:583)
  │
  0b. True-bypass dry capture (RAW input,     bypassDelayBuffer               (:592-606)
      delay-aligned to wet latency)
  │
  1. Input conditioning                        applyInputConditioning          (:609)
       channel kill / Swap / Balance / polarity / (M/S decode if msMode) / Mono
  │
  1b. M/S Solo (isolate Mid or Side)           (:614-617)
  │
  -- capture conditioned input as Level-Match reference (inputScratch)         (:621-622)
  -- capture dry for dry/wet Mix (dryScratch)                                  (:627-628)
  │
  2. Effect engine
       2a. Oversampled nonlinear region:        Drive (tanh) -> Chorus/Dim-D    (:631-645)
           (only Drive>0 or mod algorithm engages OS; else base rate)
       2b. Linear algorithm at base rate:       Haas OR Velvet                  (:648-649)
       2c. Global Width (MS-domain)             applyWidth                      (:652-653)
       2d. Multiband Width (1-4 bands)          multiband.processBlock          (:667-707)
           (click-free mbEnableBlend output crossfade; bank kept warm)
  │
  3. Dry/Wet Mix (delay-compensated,           (:728-759)
     phase-matched A(dry))
  │
  4. Mono Maker (post-Mix, in place)           monoMaker.process               (:765-766)
  │
  5. Output stage                              (:771-829)
       Level Match measure (post-Mono-Maker) -> Output Gain / Auto Gain / Output Balance
       + click-free switch duck (raised cosine)
  │
  6. Band Solo monitor (POST-EVERYTHING)       soloMonitor.process             (:894)
       band-passes the produced output to the soloed band(s); monitoring only
  │
  6b. NaN/Inf self-heal (per-sample)           (:854-870)
  │
  7. Bypass crossfade (processed <-> raw)      bypassBlend                     (:878-888)
  │
  8. Metering tap (scope + correlation + out)  (:891-898)
```

## Invariants (must hold; each is testable)

| Invariant | Where enforced | Test |
|---|---|---|
| **Mono Maker runs POST-Mix**, on the mixed signal, in place. | :761-766 | testMonoMakerPostMix |
| **Band Solo is the very last audio stage and is monitoring-only** — it never changes any effect stage; `mask==0` → bit-exact true output. | :878-894; SoloMonitor.h:11-28 | testSoloMonitor, testSoloNoGhostInSilence |
| **Effect engine is solo-agnostic** — the Multiband always sums every band; solo is a downstream monitor. | MultibandWidth.h:29-32 | testSoloMonitor (energy-transparent) |
| **Dry path is delay-compensated** to the wet (oversampling) latency. | :728-737, getLatencySamples | testBypassNullAndLatency |
| **Dry path is phase-matched** through the same crossovers as the wet (A(dry)) so a partial Mix never combs the mono sum. The reconstruction is gated off in the settled-full-wet state (Mix exactly 1, Match off, no crossfade — Wave 2 / H4) and re-engages phase-matched on a Mix dip. | :655-690, :739-759 | testMultibandMonoCompat, testDryAlignGateRecomb |
| **Mix = 0 is a bit-exact null** (smoothstep clean→aligned crossfade over first ~5% of Mix). | :726, :748-758 | testBypassNullAndLatency / testTransparentDefault |
| **Oversampling wraps only Drive + Chorus/Dim-D**; linear stages stay outside; OS off ⇒ 0 latency. | :19-23, :631-645 | testBypassNullAndLatency |
| **Bypass is a click-free crossfade to the delay-aligned RAW input**, not a mute; chain + analysis always run. | :585-606, :872-888 | testBypassCrossfadeClickFree, testLevelMatchRunsInBypass |
| **Level Match measures the post-Mono-Maker output vs the delay-aligned reconstruction A(dry).** | :795-812 | testLevelMatchUnity, testMultibandUnityMatch |

## Notes

- **M/S domain.** When `msMode` is on, Input conditioning decodes Mid/Side→L/R inside
  `applyInputConditioning`; Balance/polarity act in the M/S domain before decode.
  Source: src/dsp/AnamorphEngine.cpp:415-436.
- **Discrete switches** (algorithm/routing/band-count/oversampling-path) are applied at the
  silent bottom of a raised-cosine duck (fade-out ~6 ms, fade-in ~28 ms). Bypass, Multiband
  Enable, and Band Solo are **not** ducked — they use their own click-free crossfades.
  Source: src/dsp/AnamorphEngine.cpp:158-185 (`discreteDiffers`), :70-71, :686-731 (Multiband
  Enable crossfade), :878-894 (Band Solo), :920-940 (Bypass crossfade).
- **Forced bulk swaps** (undo / redo / A/B / preset — `requestDuck()`) run the same duck, but
  its output is **dry-filled**: stage 5 crossfades toward the delay-aligned raw input (the
  true-bypass ring) instead of dipping to silence, so the swap is heard as a short dip to the
  dry signal, never a dropout. The swap still lands at processed weight 0 (smoother snap +
  wholesale node reset unchanged). A latency-crossing forced swap keeps the original
  duck-to-silence (the ring read offset would jump at full dry weight). Ordinary discrete
  ducks are unchanged (bit-exact). Source: src/dsp/AnamorphEngine.cpp (`dryDuck`; gate at the
  ring fill, blend in the stage-5 output loop); guarded by `testForcedSwapNoDropout` (Test 26).

Any change to this order or these invariants requires an ADR and Architecture Review
(`docs/policies/ADR_POLICY.md`, `docs/policies/ARCHITECTURE_REVIEW_GATE.md`).
