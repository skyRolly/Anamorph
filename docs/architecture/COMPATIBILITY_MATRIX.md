# COMPATIBILITY_MATRIX.md

Status taxonomy: **Verified** (provable from build/CI/code) · **Partially Verified**
(README/CI claim, not fully provable here) · **Unverified** (could work, no evidence in repo) ·
**Not Supported** (deliberate, evidence-backed exclusion).

## Plugin formats

| Format | Status | Evidence |
|---|---|---|
| **VST3** | **Verified** | Built on Linux/Windows/macOS; primary target; pluginval gate. CMakeLists.txt:218; build.yml all jobs |
| **AU (Audio Unit)** | **Verified (build + conformance)** / **Unverified (host)** | Built on macOS as `.component` (universal) and, since 0.9.4, put through the same blocking pluginval gate as the VST3 (both modes ×3, against the packaged bundle, after an install into `~/Library/Audio/Plug-Ins/Components/` + `AudioComponentRegistrar` restart) — and, since the `macos-intel` job, through that same gate a second time against a thin `x86_64` build on **native Intel** hardware. Still unverified against a *real* host: pluginval loads the AU through JUCE's `AudioUnitPluginFormat`, so Logic/GarageBand loading is not tested in repo, and `auval` is not run (`docs/procedures/CI_CD.md` §"Known coverage limits"). CMakeLists.txt:219-221; .github/workflows/build.yml:1544-2004 (the `macos` job) |
| **Standalone** | **Verified** | Built on all three OSes. CMakeLists.txt:222-224 |
| **AAX** | **Not Supported** | Out of scope: needs an Avid account + PACE/iLok signing. docs/policies/COMPATIBILITY_POLICY.md. (DSP core is wrapper-agnostic, so a future AAX wrapper is low-cost, but it is explicitly not built today.) |

## Platforms / architectures

| Platform | Status | Evidence |
|---|---|---|
| **Linux x86-64** | **Verified (blocking gate)** | CI builds VST3+Standalone; headless pluginval at the configured strictness (deterministic ×3 + randomise ×3) under xvfb — **blocking**. `.github/workflows/build.yml` |
| **Windows x86-64** | **Verified (blocking gate)** | MSVC build; pluginval at the configured strictness, deterministic ×3 + randomise ×3 — **blocking** (`run-pluginval.ps1`, no `continue-on-error`). `.github/workflows/build.yml` |
| **macOS universal (arm64 + x86_64)** | **Verified (blocking gate)** | `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, `lipo` verifies both slices; pluginval at the configured strictness, both modes ×3 — **blocking**. `.github/workflows/build.yml` |
| **macOS x86_64 on native Intel silicon** | **Verified (blocking gate)** | Distinct from the row above, which is built and validated on an **Apple Silicon** runner and executes its x86_64 slice only under **Rosetta 2** (translated onto arm64 hardware). The `macos-intel` job builds thin `x86_64` on `macos-15-intel` and runs both self-test suites plus the full pluginval gate (VST3 and AU, both modes ×3) on a real Intel CPU, after asserting `uname -m == x86_64` and `sysctl.proc_translated == 0`. It ships nothing — the shipped bundle is still the universal one. .github/workflows/build.yml:2062-2307 (the `macos-intel` job), .github/workflows/build.yml:2007-2063 (its rationale block) |

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
| JUCE | **9.0.1** — immutable commit `e18f7f5…` (FetchContent, `GIT_SHALLOW`; ADR-0022, ADR-0026) | **Verified** | CMakeLists.txt:48-50, 59-67 |
| C++ standard | C++23 (`CMAKE_CXX_STANDARD 23`; ADR-0027) | **Verified** | CMakeLists.txt:16-18 |
| pluginval | latest release (downloaded by script) | **Verified** | scripts/run-pluginval.sh:121 |

See `docs/policies/DEPENDENCY_POLICY.md` for the JUCE version-lock reasoning.
