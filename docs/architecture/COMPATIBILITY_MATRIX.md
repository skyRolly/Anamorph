# COMPATIBILITY_MATRIX.md

Status taxonomy: **Verified** (provable from build/CI/code) · **Partially Verified**
(README/CI claim, not fully provable here) · **Unverified** (could work, no evidence in repo) ·
**Not Supported** (deliberate, evidence-backed exclusion).

## Plugin formats

| Format | Status | Evidence |
|---|---|---|
| **VST3** | **Verified** | Built on Linux/Windows/macOS; primary target; pluginval gate. CMakeLists.txt:142; build.yml all jobs |
| **AU (Audio Unit)** | **Verified (build)** / **Unverified (host)** | Built on macOS as `.component` (universal); real Logic/GarageBand loading not tested in repo. CMakeLists.txt:143-145; build.yml macos job (:355-542) |
| **Standalone** | **Verified** | Built on all three OSes. CMakeLists.txt:146-148 |
| **AAX** | **Not Supported** | Out of scope: needs an Avid account + PACE/iLok signing. docs/policies/COMPATIBILITY_POLICY.md. (DSP core is wrapper-agnostic, so a future AAX wrapper is low-cost, but it is explicitly not built today.) |

## Platforms / architectures

| Platform | Status | Evidence |
|---|---|---|
| **Linux x86-64** | **Verified (blocking gate)** | CI builds VST3+Standalone; headless pluginval **strictness 10** (deterministic ×3 + randomise ×3) under xvfb — **blocking**. `.github/workflows/build.yml` |
| **Windows x86-64** | **Verified (blocking gate)** | MSVC build; pluginval **strictness 10**, deterministic ×3 + randomise ×3 — **blocking** (`run-pluginval.ps1`, no `continue-on-error`). `.github/workflows/build.yml` |
| **macOS universal (arm64 + x86_64)** | **Verified (blocking gate)** | `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, `lipo` verifies both slices; pluginval **strictness 10**, both modes ×3 — **blocking**. `.github/workflows/build.yml` |

## I/O layouts

| Layout | Status | Evidence |
|---|---|---|
| stereo → stereo | **Verified** | src/PluginProcessor.cpp:33-43; test `testTransparentDefault` |
| mono → stereo | **Verified** | src/PluginProcessor.cpp:41-42,77-78 |
| **mono → mono** | **Not Supported** | Deliberately rejected: output is always stereo. src/PluginProcessor.cpp:31, :38-39 |

## DAW hosts

No DAW compatibility matrix exists in the repository; pluginval (strictness 10, blocking on all three
platforms) is the **proxy** for host conformance, not a substitute for real-DAW testing.

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
| JUCE | **9.0.0** — immutable commit `f8f8864…` (FetchContent, `GIT_SHALLOW`; ADR-0022) | **Verified** | CMakeLists.txt:36-38,47-55 |
| C++ standard | C++17 (`CMAKE_CXX_STANDARD 17`) | **Verified** | CMakeLists.txt:16-18 |
| pluginval | latest release (downloaded by script) | **Verified** | scripts/run-pluginval.sh:34 |

See `docs/policies/DEPENDENCY_POLICY.md` for the JUCE version-lock reasoning.
