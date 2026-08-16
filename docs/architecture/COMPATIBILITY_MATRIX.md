# COMPATIBILITY_MATRIX.md

Status taxonomy: **Verified** (provable from build/CI/code) · **Partially Verified**
(README/CI claim, not fully provable here) · **Unverified** (could work, no evidence in repo) ·
**Not Supported** (deliberate, evidence-backed exclusion).

## Plugin formats

| Format | Status | Evidence |
|---|---|---|
| **VST3** | **Verified** | Built on Linux/Windows/macOS; primary target; pluginval gate. CMakeLists.txt:206; build.yml all jobs |
| **AU (Audio Unit)** | **Verified (build + conformance)** / **Unverified (host)** | Built on macOS as `.component` (universal) and, since 0.9.4, put through the same blocking pluginval gate as the VST3 (both modes ×3, against the packaged bundle, after an install into `~/Library/Audio/Plug-Ins/Components/` + `AudioComponentRegistrar` restart). Still unverified against a *real* host: pluginval loads the AU through JUCE's `AudioUnitPluginFormat`, so Logic/GarageBand loading is not tested in repo, and `auval` is not run (`docs/procedures/CI_CD.md` §"Known coverage limits"). CMakeLists.txt:207-209; build.yml macos job (:1213-1626) |
| **Standalone** | **Verified** | Built on all three OSes. CMakeLists.txt:210-212 |
| **AAX** | **Not Supported** | Out of scope: needs an Avid account + PACE/iLok signing. docs/policies/COMPATIBILITY_POLICY.md. (DSP core is wrapper-agnostic, so a future AAX wrapper is low-cost, but it is explicitly not built today.) |

## Platforms / architectures

| Platform | Status | Evidence |
|---|---|---|
| **Linux x86-64** | **Verified (blocking gate)** | CI builds VST3+Standalone; headless pluginval at the configured strictness (deterministic ×3 + randomise ×3) under xvfb — **blocking**. `.github/workflows/build.yml` |
| **Windows x86-64** | **Verified (blocking gate)** | MSVC build; pluginval at the configured strictness, deterministic ×3 + randomise ×3 — **blocking** (`run-pluginval.ps1`, no `continue-on-error`). `.github/workflows/build.yml` |
| **macOS universal (arm64 + x86_64)** | **Verified (blocking gate)** | `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, `lipo` verifies both slices; pluginval at the configured strictness, both modes ×3 — **blocking**. `.github/workflows/build.yml` |

## I/O layouts

| Layout | Status | Evidence |
|---|---|---|
| stereo → stereo | **Verified** | src/PluginProcessor.cpp:7-8 (bus declaration), :76-86 (`isBusesLayoutSupported`); test `testTransparentDefault` |
| mono → stereo | **Verified** | src/PluginProcessor.cpp:84-85 (mono input accepted), :120-121 (mono duplicated to both channels) |
| **mono → mono** | **Not Supported** | Deliberately rejected: output is always stereo. src/PluginProcessor.cpp:81-82 |

## DAW hosts

No DAW compatibility matrix exists in the repository; pluginval (blocking on all three platforms) is
the **proxy** for host conformance, not a substitute for real-DAW testing. The strictness the three
rows above run at is `ANAMORPH_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml`.

| Host | Status | Note |
|---|---|---|
| REAPER | **Unverified** | Referenced for its parameter-listing behaviour (drove the 0.8.4 InternalState design) and as the suggested audition host; not a tested compatibility claim. docs/procedures/TESTING.md; src/InternalState.h:15-18 |
| Ableton Live, Logic Pro, GarageBand, Cubase, Pro Tools, Studio One, Bitwig, etc. | **Unverified** | No evidence in repo. AU build targets Logic/GarageBand but host-load is untested here. |

`TODO: populate a real-DAW host matrix from manual validation (requires a machine with audio +
display; see TESTING.md "What cannot be verified headlessly"). Do not mark any host Verified
without test evidence.`

## Toolchain / dependency pins

| Dependency | Pin | Status | Evidence |
|---|---|---|---|
| JUCE | **9.0.1** — immutable commit `e18f7f5…` (FetchContent, `GIT_SHALLOW`; ADR-0022, ADR-0026) | **Verified** | CMakeLists.txt:36-38,47-55 |
| C++ standard | C++23 (`CMAKE_CXX_STANDARD 23`; ADR-0027) | **Verified** | CMakeLists.txt:16-18 |
| pluginval | latest release (downloaded by script) | **Verified** | scripts/run-pluginval.sh:121 |

See `docs/policies/DEPENDENCY_POLICY.md` for the JUCE version-lock reasoning.
