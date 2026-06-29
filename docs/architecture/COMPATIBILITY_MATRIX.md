# COMPATIBILITY_MATRIX.md

Status taxonomy: **Verified** (provable from build/CI/code) · **Partially Verified**
(README/CI claim, not fully provable here) · **Unverified** (could work, no evidence in repo) ·
**Not Supported** (deliberate, evidence-backed exclusion).

## Plugin formats

| Format | Status | Evidence |
|---|---|---|
| **VST3** | **Verified** | Built on Linux/Windows/macOS; primary target; pluginval gate. CMakeLists.txt:80; build.yml all jobs |
| **AU (Audio Unit)** | **Verified (build)** / **Unverified (host)** | Built on macOS as `.component` (universal); real Logic/GarageBand loading not tested in repo. CMakeLists.txt:81-83; build.yml:139-159 |
| **Standalone** | **Verified** | Built on all three OSes. CMakeLists.txt:84-86 |
| **AAX** | **Not Supported** | Out of scope: needs an Avid account + PACE/iLok signing. docs/policies/COMPATIBILITY_POLICY.md. (DSP core is wrapper-agnostic, so a future AAX wrapper is low-cost, but it is explicitly not built today.) |

## Platforms / architectures

| Platform | Status | Evidence |
|---|---|---|
| **Linux x86-64** | **Verified (authoritative gate)** | CI builds VST3+Standalone; headless pluginval **strictness 10** under xvfb is the release gate. build.yml:22-58 |
| **Windows x86-64** | **Verified (build)** / pluginval informational | MSVC build; pluginval `continue-on-error`. build.yml:60-105 |
| **macOS universal (arm64 + x86_64)** | **Verified (build)** / pluginval informational | `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, `lipo` verifies both slices; deployment target 10.13. build.yml:107-159 |

## I/O layouts

| Layout | Status | Evidence |
|---|---|---|
| stereo → stereo | **Verified** | src/PluginProcessor.cpp:33-43; test `testTransparentDefault` |
| mono → stereo | **Verified** | src/PluginProcessor.cpp:41-42,77-78 |
| **mono → mono** | **Not Supported** | Deliberately rejected: output is always stereo. src/PluginProcessor.cpp:31, :38-39 |

## DAW hosts

No DAW compatibility matrix exists in the repository; pluginval (strictness 10 on Linux) is the
**proxy** for host conformance, not a substitute for real-DAW testing.

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
| JUCE | tag **8.0.14** (FetchContent, `GIT_SHALLOW`) | **Verified** | CMakeLists.txt:33,44-50 |
| C++ standard | C++17 (`CMAKE_CXX_STANDARD 17`) | **Verified** | CMakeLists.txt:16-18 |
| pluginval | latest release (downloaded by script) | **Verified** | scripts/run-pluginval.sh:34 |

See `docs/policies/DEPENDENCY_POLICY.md` for the JUCE version-lock reasoning.
