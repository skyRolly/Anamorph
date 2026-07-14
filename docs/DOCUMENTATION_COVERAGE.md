# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

Last updated: for the **v0.8.10 release** (finalized 2026-07-14, PR #59). The `[Unreleased]`
CHANGELOG entries (undo/redo forced-duck dry-fill + rapid-swap robustness, multiband flat
recombination, adaptive `FrameClock` GUI refresh) are folded into the `[0.8.10]` section; the
version is bumped to 0.8.10 across CMakeLists / README / HANDOVER / KNOWN_ISSUES / FUTURE_RISKS;
KI-009 (REAPER Save Preset) is carried forward as an open, host-specific issue (not fixed).
Includes the pre-merge review round: (a) Multiband
flat recombination — the crossover reconstruction now phase-compensates each lower band by the
splits above it (allpass telescoping), removing the −17.75 dB dip at close crossovers; documented
in DSP_ALGORITHMS (MultibandWidth) + CHANGELOG, guarded by `testMultibandFlatRecombination`
(Test 28). (b) Rapid forced-swap dry-fill robustness — every forced swap re-evaluates dry-fill,
never reusing a prior swap's stale offset; CHANGELOG + `testRapidForcedSwapDryFill` (Test 27).
(c) FrameClock review — the Advanced-only SpectrumImager now stops its display-rate clock while
hidden (Simple mode), mirroring the meters (no unnecessary vblank ticks). DSP test count
25→**27**, checks 77→**90** (README, TESTING_POLICY, TESTING, HANDOVER; `testRapidForcedSwapDryFill`
gained fade-in and fade-out latency-crossing retarget cases during the pre-merge verification pass).
No parameter/automation/
preset/serialization/latency change; the multiband fix changes only the multiband audio output
(the intended fix — twin dump confirms latency unchanged, non-multiband scenarios identical).
Prior: for the **post-v0.8.9 PR** (three items + a fresh profiling baseline). (1) Undo/Redo
audible-dropout fix — the forced switch duck is now dry-filled from the true-bypass ring;
documented in SIGNAL_FLOW (forced-swap note) + CHANGELOG `[Unreleased]`, guarded by the new
`testForcedSwapNoDropout` (Test 26, count 24→**25** DSP tests, 73→**77** checks). (2) Adaptive
display-rate GUI refresh — new `gui::FrameClock` (VBlank, capped ~120 Hz) replaces the four fixed
60 Hz visualizer timers, with dt-corrected ballistics; new module coverage row + THREAD_MODEL timer
table/top-row + PERFORMANCE_BUDGET GUI row + CHANGELOG `[Unreleased]`. (3) **KI-009** added — the
REAPER Save Preset focus report (host-specific, pending manual investigation), version-sync header
updated. A post-v0.8.9 DSP+GUI profiling baseline was produced (callgrind Ir + wall-clock +
EdBench A/B); per established convention the report stays in the session scratchpad and is **not**
committed (no volatile clock-dependent numbers enter the permanent budget). Prior: the **v0.8.9
release** (finalized 2026-07-12, PR #58) — the `[Unreleased]` CHANGELOG entries from Wave-2 Step-1
and Step-2 (H3/H4/H5/H6/H11/H15/ALG-4, the tooltip revert, and the `viewGenWatcher` destructor
lifecycle fix) folded into `[0.8.9]`; every `CHANGELOG [Unreleased]` evidence citation across the
docs set (PERFORMANCE_BUDGET) updated to `CHANGELOG [0.8.9]`. One new module row (`LR4Xover`, the
flat-state LR4 crossover); H3/H4/H5/H6/H15 documented across DSP_ALGORITHMS, DSP_GRAPH_REFERENCE,
SIGNAL_FLOW, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT, THREAD_MODEL/THREADING_POLICY (two new
generation counters, same staleness-hint pattern), TESTING (new `testDryAlignGateRecomb`, test
count 23→24).
Prior: Wave-2 Step-1 (PR #58) — no module-coverage change; the H11/ALG-4 DSP work documented in
DSP_ALGORITHMS + PERFORMANCE_BUDGET + CHANGELOG, and `AI_AGENT_POLICY.md` gained constraint C8
(UI text requires explicit instruction). Retro-covers PR #57 (KNOWN_ISSUES KI-008 added; no
coverage change — this header was missed in that PR). Prior: the initial 0.8.9 version bump
(PR #56) — no coverage change; the 0.8.8 idle-performance PR (#54) — threading paths
(`soundParamGen`) and the ScopeBuffer per-block publication model documented; prior full audit at
HEAD `c605fbe` (JUCE 8.0.14).

## Code-module coverage

| Module | Documented in | Coverage | Confidence |
|---|---|---|---|
| `AnamorphEngine` (chain/switch machine) | SIGNAL_FLOW, DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, ADR-0004/0005/0006 | Full | Verified |
| `EngineParameters` (POD boundary) | ARCHITECTURE, API_REFERENCE, ADR-0001 | Full | Verified |
| `PluginParameters` / APVTS | PARAMETER_REGISTRY, PARAMETER_REFERENCE, ADR-0002 | Full | Verified |
| `InternalState` | PARAMETER_REGISTRY, STATE_SERIALIZATION, ADR-0010 | Full | Verified |
| `PresetManager` | API_REFERENCE, STATE_SERIALIZATION | Partial (interface + role; preset file format not exhaustively documented) | Verified |
| State save/recall | STATE_SERIALIZATION, SERIALIZATION_REGISTRY | Full | Verified |
| `MidSide` | DSP_ALGORITHMS | Full | Verified |
| `HaasProcessor` | DSP_ALGORITHMS | Full | Verified |
| `VelvetNoise` | DSP_ALGORITHMS | Full | Verified |
| `ChorusEngine` | DSP_ALGORITHMS | Full | Verified |
| `MonoMaker` | DSP_ALGORITHMS, SIGNAL_FLOW, ADR-0006 | Full | Verified |
| `MultibandWidth` | DSP_ALGORITHMS, ADR-0005/0009 | Full | Verified |
| `LR4Xover` (flat-state LR4 crossover, Wave 2 / H6) | DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT + its own bit-exactness contract comment | Full | Verified |
| `SoloMonitor` | DSP_ALGORITHMS, ADR-0004/0006 | Full | Verified |
| `LoudnessMatch` | DSP_ALGORITHMS, ADR-0007 | Full | Verified |
| `Correlation` / `LevelMeters` / `ScopeBuffer` | DSP_ALGORITHMS, THREAD_MODEL | Full | Verified |
| Threading / OpenGL gate | THREAD_MODEL, ADR-0011 | Full | Verified |
| Latency / PDC | LATENCY_MODEL, ADR-0003 | Full | Verified |
| Real-time safety | REALTIME_SAFETY_AUDIT, REALTIME_AUDIO_POLICY | Full | Verified |
| `gui/FrameClock` (adaptive display-rate refresh, post-0.8.9) | THREAD_MODEL, PERFORMANCE_BUDGET, CHANGELOG + its own header contract | Full | Verified |
| `PluginEditor` / `gui/*` | THREAD_MODEL, REPOSITORY_MAP | Partial (threading + lifecycle documented; per-widget layout/LookAndFeel not exhaustively) | Verified |
| Build / CI / packaging | BUILD, CI_CD, PACKAGING | Full | Verified |
| Tests | TESTING, TESTING_POLICY | Full | Verified |
| Performance (numbers) | PERFORMANCE_BUDGET | Structural only | Unverified (no benchmark data — TODOs) |
| Host (DAW) compatibility | COMPATIBILITY_MATRIX | Listed | Unverified (no in-repo DAW tests) |
| AAX, mono→mono | COMPATIBILITY_MATRIX, COMPATIBILITY_POLICY | Documented as excluded | Not Supported |

## Documentation-set self-coverage (deliverables present)

| Tier | Files | Status |
|---|---|---|
| docs root | SOURCE_OF_TRUTH, HANDOVER, REPOSITORY_MAP, DOCUMENTATION_COVERAGE, POSTMORTEMS, KNOWN_ISSUES, FUTURE_RISKS | Present |
| architecture | 14 docs + ADR_INDEX + 12 ADRs | Present |
| procedures | 8 docs | Present |
| policies | 15 docs | Present |
| root | README, CHANGELOG, CLAUDE | Present |

## Known coverage gaps / TODOs

- **Performance numbers** — `PERFORMANCE_BUDGET.md` carries explicit TODOs; populate from profiling.
- **DAW host matrix** — `COMPATIBILITY_MATRIX.md` hosts are Unverified; populate from manual testing.
- **GUI per-widget reference** — editor layout/LookAndFeel is documented at the threading/lifecycle
  level only; a per-widget reference is not present (low priority — GUI changes don't gate releases).
- **Pre-0.6 version history** — CHANGELOG entries for early versions are Partially Verified (README
  + commits); no git tags exist for exact per-version attribution.

## Update protocol

On any change, set this file's "Last updated" to the new HEAD and adjust the affected rows. A new
module → add a row; a new doc → add to self-coverage; new perf/host data → upgrade the confidence.
