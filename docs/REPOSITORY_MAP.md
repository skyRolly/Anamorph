# REPOSITORY_MAP.md

Directory and file map with per-component responsibilities. Architecture rationale is in
`docs/architecture/ARCHITECTURE.md`.

## Top level

```
Anamorph/
├── CMakeLists.txt          Build: JUCE FetchContent (pinned 8.0.8), AnamorphDSP INTERFACE lib,
│                           plugin target (VST3 [+AU on macOS] [+Standalone]), tests app.
├── README.md               Project façade (features, status, quick start, docs nav).
├── CHANGELOG.md            Version history (Keep a Changelog; evidence-cited).
├── CLAUDE.md               AI/contributor entry point: mandatory policy pre-read + repo constraints.
├── src/                    Source (wrapper + GUI + DSP core).
├── tests/                  Headless DSP self-tests.
├── scripts/                setup / build / test / pluginval.
├── packaging/              macOS install notes.
├── .github/workflows/      CI (build + validate on 3 OSes).
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
| `CorrelationMeter.{h,cpp}` | Phase-correlation + balance meters. |

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
| `SoloMonitor.{h,cpp}` | Post-everything Band-Solo audition band-pass. |
| `LoudnessMatch.{h,cpp}` | BS.1770 Level Match (Measure + absolute Predict). |
| `Correlation.h` | Phase-correlation estimator. |
| `LevelMeters.h` | L/R Peak+RMS metering with NaN self-heal. |
| `ScopeBuffer.h` | Lock-free SPSC scope ring. |

## `tests/`, `scripts/`, `packaging/`, `.github/`

| Path | Responsibility |
|---|---|
| `tests/dsp_tests.cpp` | 23 headless DSP acceptance tests (`check(cond, "...")` harness; `main` runs all). |
| `scripts/setup-linux.sh` | Ubuntu build dependencies (+ xvfb). |
| `scripts/build.sh` | CMake + Ninja build; prints artifact paths. |
| `scripts/run-tests.sh` | Runs `AnamorphTests`. |
| `scripts/run-pluginval.sh` | pluginval (strictness arg; signal-only retry for the X11 host flake). |
| `packaging/macos/INSTALL.txt` | macOS install + de-quarantine instructions (ad-hoc signed, not notarized). |
| `.github/workflows/build.yml` | 3-OS build + DSP tests + pluginval; Linux strictness-10 is the blocking gate. |

## `docs/` — documentation library

```
docs/
├── SOURCE_OF_TRUTH.md, HANDOVER.md, REPOSITORY_MAP.md, DOCUMENTATION_COVERAGE.md,
│   POSTMORTEMS.md, KNOWN_ISSUES.md, FUTURE_RISKS.md
├── architecture/   (system reference: ARCHITECTURE, SIGNAL_FLOW, DSP_GRAPH_REFERENCE,
│                    THREAD_MODEL, API_REFERENCE, PARAMETER_*/SERIALIZATION_*/STATE_*,
│                    LATENCY_MODEL, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT,
│                    DSP_ALGORITHMS, COMPATIBILITY_MATRIX, design-decisions/ADRs)
├── procedures/     (DEVELOPMENT, BUILD, CI_CD, PACKAGING, TESTING, RELEASE_PROCESS,
│                    RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING)
└── policies/       (REALTIME_AUDIO, THREADING, DSP, COMPATIBILITY family,
                     ARCHITECTURE_REVIEW_GATE, ADR, DOCUMENTATION_LIFECYCLE, AI_AGENT,
                     CHANGELOG, TESTING, RELEASE, DEPENDENCY, CODE_STYLE)
```

Evidence [Verified]: file tree from the repository; CMakeLists.txt:62-115; src/ listing.
