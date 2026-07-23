# REPOSITORY_MAP.md

Directory and file map with per-component responsibilities. Architecture rationale is in
`docs/architecture/ARCHITECTURE.md`.

## Top level

```
Anamorph/
├── CMakeLists.txt          Build: JUCE FetchContent (9.0.0, pinned by commit SHA), AnamorphDSP INTERFACE lib,
│                           AnamorphHardening flags (ADR-0021), plugin target
│                           (VST3 [+AU on macOS] [+Standalone]), tests app.
├── README.md               Project façade (features, status, quick start, docs nav).
├── CHANGELOG.md            Version history (Keep a Changelog; evidence-cited).
├── CLAUDE.md               AI/contributor entry point: mandatory policy pre-read + repo constraints.
├── src/                    Source (wrapper + GUI + DSP core).
├── tests/                  Headless self-tests (DSP + state compatibility) and fixtures.
├── worklogs/               Session-local investigation records for future agents (NOT
│                           architecture docs; e.g. performance/WAVE3_INVESTIGATION.md,
│                           release-hardening/RH_PR2_INVESTIGATION.md — finalized decisions
│                           graduate to ADRs; worklogs are the raw evidence trail).
├── scripts/                setup / build / test / pluginval.
├── packaging/              macOS install notes.
├── .github/                CI + security tooling: workflows/ (build + validate on 3 OSes with
│                           retain-then-strip symbol pipeline; CodeQL; MSVC /analyze;
│                           Dependency Review) and dependabot.yml (github-actions ecosystem only).
└── docs/                   This documentation library.
```

## `src/` — wrapper + GUI

| File | Responsibility |
|---|---|
| `PluginProcessor.{h,cpp}` | VST3/Standalone wrapper: bus layouts, `processBlock`, state save/recall, PDC, custom Undo/Redo, A/B compare. |
| `PluginParameters.{h,cpp}` | APVTS layout (`createAnamorphLayout`), `pid::` IDs, atomic cache, `toEngine` → `EngineParameters`. |
| `InternalState.h` | Host-hidden session/view params (Oversampling, Window Size, Persistence, Tooltips, Animations, Meters). |
| `PresetManager.{h,cpp}` | Factory + user `.anamorph` presets (sound params only). |
| `PluginEditor.{h,cpp}` | Simple/Advanced UI, OpenGL context (macOS/Windows only), 24 Hz + VBlank timers. |

## `src/gui/` — GUI components

| File | Responsibility |
|---|---|
| `LookAndFeel.{h,cpp}` | Dark "digital" look; knob/slider drawing incl. reset-sweep easing. |
| `Vectorscope.{h,cpp}` | Diamond/Lissajous goniometer (reads `ScopeBuffer`). |
| `SpectrumImager.{h,cpp}` | Multiband spectral editor (FFT + drag-to-split bands); writes crossover/width/solo params. |
| `LevelMeter.{h,cpp}` | Per-channel L/R Peak + RMS meters. |
| `CorrelationMeter.{h,cpp}` | Phase-correlation + balance meters (GUI class `StereoMeter`). |
| `FrameClock.h` | Adaptive display-rate refresh driver shared by the four visualizers (VBlank-paced, ~125 Hz cap; 0.8.10). |

## `src/dsp/` — format-agnostic DSP core (`AnamorphDSP` INTERFACE lib)

| File | Responsibility |
|---|---|
| `EngineParameters.h` | POD snapshot driving the engine (the wrapper↔engine boundary). |
| `AnamorphEngine.{h,cpp}` | The serial DSP chain orchestrator; switch machine; crossfades; latency. |
| `MidSide.h` | MS matrix (1/√2) + `applyWidth`. |
| `HaasProcessor.{h,cpp}` | Precedence delay widening. |
| `VelvetNoise.{h,cpp}` | Velvet-noise decorrelation (mono→stereo). |
| `ChorusEngine.{h,cpp}` | Chorus + Dimension-D. |
| `MonoMaker.{h,cpp}` | LR4 low-freq mono (post-Mix). |
| `MultibandWidth.{h,cpp}` | 1–4 band per-band width + phase-matched A(dry). |
| `LR4Xover.h` | Flat-state Linkwitz–Riley crossover clone (Wave-2 H6; bit-identical to the `juce::dsp` original). |
| `SoloMonitor.{h,cpp}` | Post-everything Band-Solo audition band-pass. |
| `LoudnessMatch.{h,cpp}` | BS.1770 Level Match (Measure + absolute Predict). |
| `Correlation.h` | Phase-correlation estimator. |
| `LevelMeters.h` | L/R Peak+RMS metering with NaN self-heal. |
| `ScopeBuffer.h` | Lock-free SPSC scope ring. |

## `tests/`, `scripts/`, `packaging/`, `.github/`

| Path | Responsibility |
|---|---|
| `tests/dsp_tests.cpp` | 33 headless DSP acceptance tests + 1 A/B state-restoration clamp guard (`check(cond, "...")` harness; `main` runs all). |
| `tests/state_tests.cpp` | 9 headless state-compatibility tests (schema shape, parameter-registry snapshot, raw-exact round-trip, 3 legacy migration fixtures, corrupt-state robustness, preset round-trip, A/B + view-param preservation) — own console target `AnamorphStateTests` compiling the plugin sources. |
| `tests/fixtures/` | Compatibility fixtures: `parameter_registry.snapshot` (re-frozen only via `AnamorphStateTests --write-snapshot` for INTENTIONAL parameter changes) + 3 frozen legacy session XMLs (v0.2 / pre-0.6.4 / pre-0.8.4). |
| `scripts/setup-linux.sh` | Ubuntu build dependencies (+ xvfb). |
| `scripts/build.sh` | CMake + Ninja build; prints artifact paths. |
| `scripts/run-tests.sh` | Runs `AnamorphTests` + `AnamorphStateTests` (fail-closed). |
| `scripts/run-pluginval.sh` | pluginval on Linux/macOS (strictness + mode args — `deterministic` \| `randomise`, each ×3; signal-only retry for the X11 host flake). |
| `scripts/run-pluginval.ps1` | pluginval on Windows (same strictness/mode/×3 structure; exit code is the sole signal). |
| `src/AbSlotIndex.h` | `anamorph::kNumAbSlots` + `clampAbSlotIndex` — single source of truth for A/B slot sizing/clamping. |
| `packaging/macos/INSTALL.txt` | macOS install + de-quarantine instructions (ad-hoc signed, not notarized). |
| `.github/workflows/build.yml` | 3-OS build + DSP tests + pluginval; strictness-10, both modes ×3, **blocking on all three platforms**; also callable (`workflow_call`) by release.yml. |
| `.github/workflows/release.yml` | RH-PR-8 release skeleton: annotated `vX.Y.Z` tag → fail-closed metadata validation → reused build.yml gates → **draft** GitHub Release (versioned artifacts + SHA-256 sums + manifest); `workflow_dispatch` = rehearsal. |
| `.github/workflows/codeql.yml` | CodeQL (`c-cpp` manual build + `actions`); alerts scoped to repo-own code. See `docs/procedures/CI_CD.md` §Security scanning. |
| `.github/workflows/msvc.yml` | MSVC `/analyze` → SARIF; JUCE treated as external; path-filtered triggers. |
| `.github/workflows/dependency-review.yml` | Dependency Review on PRs to `main` (GitHub Actions deps; comment on failure only). |
| `.github/dependabot.yml` | Weekly grouped `github-actions` bumps; JUCE stays manually pinned (`DEPENDENCY_POLICY.md`). |

## `docs/` — documentation library

```
docs/
├── SOURCE_OF_TRUTH.md, HANDOVER.md, REPOSITORY_MAP.md, DOCUMENTATION_COVERAGE.md,
│   POSTMORTEMS.md, KNOWN_ISSUES.md, FUTURE_RISKS.md
├── architecture/   (system reference: ARCHITECTURE, SIGNAL_FLOW, DSP_GRAPH_REFERENCE,
│                    THREAD_MODEL, API_REFERENCE, PARAMETER_*/SERIALIZATION_*/STATE_*,
│                    LATENCY_MODEL, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT,
│                    DSP_ALGORITHMS, COMPATIBILITY_MATRIX, RELEASE_HARDENING_PLAN,
│                    design-decisions/ADRs)
├── procedures/     (DEVELOPMENT, BUILD, CI_CD, PACKAGING, TESTING, RELEASE_PROCESS,
│                    RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING)
└── policies/       (REALTIME_AUDIO, THREADING, DSP, COMPATIBILITY family,
                     ARCHITECTURE_REVIEW_GATE, ADR, DOCUMENTATION_LIFECYCLE, AI_AGENT,
                     CHANGELOG, TESTING, RELEASE, DEPENDENCY, CODE_STYLE)
```

Evidence [Verified]: file tree from the repository; CMakeLists.txt:77-113 (hardening interface) + :124 (`AnamorphDSP`) / :150 (`juce_add_plugin`); src/ listing.
