# REPOSITORY_MAP.md

Directory and file map with per-component responsibilities. Architecture rationale is in
`docs/architecture/ARCHITECTURE.md`.

## Top level

```
Anamorph/
├── CMakeLists.txt          Build: JUCE FetchContent (9.0.1, pinned by commit SHA), AnamorphDSP INTERFACE lib,
│                           AnamorphHardening flags (ADR-0021), plugin target
│                           (VST3 [+AU on macOS] [+Standalone]), tests app.
├── README.md               Project façade (features, status, quick start, docs nav).
├── CHANGELOG.md            Version history (Keep a Changelog; evidence-cited).
├── CLAUDE.md               AI/contributor entry point: mandatory policy pre-read + repo constraints.
├── NOTICE                  Third-party attribution that must accompany a binary distribution
│                           (published as a version-named asset on every GitHub release — the
│                           sole carrier of the mandatory IJG acknowledgement).
├── THIRD_PARTY_LICENSES.md Verified inventory of every third-party component: purpose, origin,
│                           licence, obligations, what is compiled in vs only vendored, and the
│                           open licensing decisions. Re-verify after any JUCE bump.
├── EULA.md                 Anamorph's own end-user terms — an UNAPPROVED DRAFT, not in force and
│                           not shipped by any installer; every open owner/legal decision marked.
├── PRIVACY.md              What Anamorph collects (nothing), sends (nothing) and writes to disk;
│                           every claim cited to source. Legal class, derived from the code.
├── TRADEMARKS.md           Product/company name status, third-party marks used descriptively, and
│                           the naming obligations the dependency licences impose.
├── SUPPORT.md              Internal-testing guide: what a tester may do with a build, what to
│                           check first, and what a test report must contain.
├── src/                    Source (wrapper + GUI + DSP core).
├── tests/                  Headless self-tests (DSP + state compatibility) and fixtures.
├── worklogs/               Session-local investigation records for future agents (NOT
│                           architecture docs; e.g. performance/WAVE3_INVESTIGATION.md,
│                           release-hardening/RH_PR2_INVESTIGATION.md — finalized decisions
│                           graduate to ADRs; worklogs are the raw evidence trail).
├── scripts/                setup / build / test / pluginval, plus the four CI lints
│                           (check-docs, check-portability, check-citations,
│                           check-clang-warnings — each with its own self-test).
├── packaging/              Per-platform install notes + installer assets (linux/, windows/, macos/).
├── .github/                CI + security tooling: workflows/ (build + validate on 3 OSes with
│                           retain-then-strip symbol pipeline; doc/source lints; Linux+Clang
│                           warning gate; ASan/UBSan + valgrind; CodeQL; MSVC /analyze;
│                           Dependency Review) and dependabot.yml (github-actions ecosystem only).
└── docs/                   This documentation library.
```

## `src/` — wrapper + GUI

| File | Responsibility |
|---|---|
| `PluginProcessor.{h,cpp}` | VST3/Standalone wrapper: bus layouts, `processBlock`, state save/recall, PDC, custom Undo/Redo, A/B compare. |
| `PluginParameters.{h,cpp}` | APVTS layout (`createAnamorphLayout`), `pid::` IDs, atomic cache, `toEngine` → `EngineParameters`. |
| `InternalState.h` | Host-hidden session/view params (Oversampling, UI Scale, Persistence, Tooltips, Animations, Meters). |
| `PresetManager.{h,cpp}` | Factory + user `.anamorph` presets (sound params only). Factory presets carry an immutable internal id, user presets are identified by their file — the `Selection` that keeps a shared name from mis-ticking the menu, and that is carried in plug-in state so the indicator survives a reload (ADR-0024). |
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
| `tests/state_tests.cpp` | 12 headless state-compatibility tests (schema shape, parameter-registry snapshot, raw-exact round-trip, 3 legacy migration fixtures, corrupt-state robustness, preset round-trip, A/B + view-param preservation, factory/user preset identity under a shared name, factory-id integrity, indicator identity across a session reload) — own console target `AnamorphStateTests` compiling the plugin sources. |
| `tests/fixtures/` | Compatibility fixtures: `parameter_registry.snapshot` (re-frozen only via `AnamorphStateTests --write-snapshot` for INTENTIONAL parameter changes) + 3 frozen legacy session XMLs (v0.2 / pre-0.6.4 / pre-0.8.4). |
| `scripts/setup-linux.sh` | Ubuntu build dependencies (+ xvfb, + lld for the Clang/LTO link). |
| `scripts/build.sh` | CMake + Ninja build; prints artifact paths (VST3, Standalone, both suites). |
| `scripts/run-tests.sh` | Runs `AnamorphTests` + `AnamorphStateTests`, fail-closed on absence **and** ambiguity (exactly one match each). `ANAMORPH_TEST_RUNNER` prefixes both invocations — the macOS job passes `arch -x86_64` to execute the universal binary's other slice. |
| `scripts/run-pluginval.sh` | pluginval on Linux/macOS (strictness + mode + **format** args — `deterministic` \| `randomise` each ×3, `vst3` \| `au`; fixed **nonzero** seed; `ANAMORPH_PLUGINVAL_BUNDLE` overrides discovery for an installed AU; signal-only retry for the X11 host flake). |
| `scripts/run-pluginval.ps1` | pluginval on Windows (same strictness/mode/×3/seed structure; exit code is the sole signal; `--skip-gui-tests` for the GPU-less runner, KI-007). |
| `scripts/check-docs.py` | Structural Markdown lint over the whole document set (table integrity, relative links, blockquote lazy continuation, unclosed fences, CHANGELOG entry-boundary rule). `--self-test` first. |
| `scripts/check-portability.py` | Rejects explicit template arguments on `juce::jmin/jmax/snapToZero` (instantiates `dsp::SIMDRegister<T>` — compiles on Linux, fails on macOS); also checks the Linux installer/uninstaller scratch-name sets agree. `--compile-canary` proves the hazard still exists in the pinned JUCE. |
| `scripts/check-citations.py` | Keeps `file.cpp:NNN` evidence anchors in `docs/` pointing at the text they named at a base revision. `--check` reports drift, `--fix` re-anchors; deliberate re-aims are declared in `DELIBERATE_REAIMS`. |
| `scripts/check-clang-warnings.py` | The first-party warning gate for the `linux-clang` job: classifies each Clang diagnostic **structurally** by resolved path (`src/`, `tests/`, never through `_deps`). `--self-test` proves the classifier is live. |
| `src/AbSlotIndex.h` | `anamorph::kNumAbSlots` + `clampAbSlotIndex` — single source of truth for A/B slot sizing/clamping. |
| `packaging/macos/INSTALL.txt` | macOS install + de-quarantine instructions (ad-hoc signed, not notarized). Installation content only — no testing or attribution section. |
| `packaging/macos/build-pkg.sh` | Builds the macOS `.pkg` installer (three component packages + productbuild, component selection with a full-install default, every component non-relocatable so a re-install always writes its declared destination) from the CI-staged payload. |
| `packaging/windows/Anamorph.iss` + `INSTALL.txt` | Inno Setup installer script (stable AppId; component page + dual-path destination page, VST3 → Common Files) + Windows install notes (shipped in the zip). |
| `packaging/linux/install.sh`, `uninstall.sh`, `INSTALL.txt` | Linux installer/uninstaller — prompts for per-user (`~/.vst3`, `~/.local/bin`, no root; the default) or system-wide (`/usr/lib/vst3`, `/usr/local/bin`, `sudo`) — + install notes; all three ship in the zip. |
| `docs/user/USER_MANUAL.md` | Full end-user manual (interface, signal flow, algorithms, presets, workflows, troubleshooting); attached to GitHub releases. |
| `docs/user/INSTALLATION.md` | End-user installation guide for all three platforms (installer + manual routes). |
| `.github/workflows/build.yml` | 3-OS build + DSP **and state** self-tests + pluginval (both modes ×3, **blocking on all three platforms**, VST3 everywhere and **AU on macOS**; strictness held once in `env.ANAMORPH_PLUGINVAL_STRICTNESS`); plus four non-packaging jobs with no `needs:` in either direction — `docs`, `source-lint`, `linux-clang` (Clang warning gate over `src/`+`tests/`, and the only job that builds the LTO'd plugin with a second compiler), `sanitizers` (ASan+UBSan then valgrind memcheck). Stages the per-platform packages — flat `Anamorph-<OS>` artifacts (loose files; release.yml archives the release zip from the same tree), Windows/macOS installers, `-debug` symbols. One run per ref (`concurrency`, tags exempt from cancellation); same-repo PR events skipped as duplicates of the branch push. Also callable (`workflow_call`) by release.yml. |
| `.github/workflows/release.yml` | RH-PR-8 release skeleton: annotated `vX.Y.Z` tag → fail-closed metadata validation → reused build.yml gates → **draft** GitHub Release (versioned artifacts + SHA-256 sums + manifest); `workflow_dispatch` = rehearsal. |
| `.github/workflows/codeql.yml` | CodeQL (`c-cpp` manual build + `actions`); alerts scoped to repo-own code. See `docs/procedures/CI_CD.md` §Security scanning. |
| `.github/workflows/msvc.yml` | MSVC `/analyze` → SARIF; JUCE treated as external; path-filtered triggers. |
| `.github/workflows/dependency-review.yml` | Dependency Review on PRs to `main` (GitHub Actions deps; comment on failure only). |
| `.github/dependabot.yml` | Weekly grouped `github-actions` bumps; JUCE stays manually pinned (`DEPENDENCY_POLICY.md`). |
| `.github/ISSUE_TEMPLATE/` | `bug_report.yml` — the "Test report — bug" form (version+build, OS, DAW, format, install route, repro — the fields triage actually needs; carries the closed-source/public-tracker notice) + `config.yml` (links to the install guide, FAQ, known issues and the internal testing guide). |

## `docs/` — documentation library

```
docs/
├── SOURCE_OF_TRUTH.md, HANDOVER.md, REPOSITORY_MAP.md, DOCUMENTATION_COVERAGE.md,
│   POSTMORTEMS.md, KNOWN_ISSUES.md, FUTURE_RISKS.md, COMMERCIAL_STATUS.md
│   (COMMERCIAL_STATUS.md = internal index of the product model, distribution model and the
│    open owner/legal decisions; it indexes, never overrides, the records it cites)
├── user/           (end-user class: USER_MANUAL, INSTALLATION)
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

Evidence [Verified]: file tree from the repository; CMakeLists.txt:77-177 (hardening interface) + :188 (`AnamorphDSP`) / :214 (`juce_add_plugin`); src/ listing.
