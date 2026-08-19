# COMPATIBILITY_MATRIX.md

Status taxonomy: **Verified** (provable from build/CI/code) · **Partially Verified**
(README/CI claim, not fully provable here) · **Unverified** (could work, no evidence in repo) ·
**Not Supported** (deliberate, evidence-backed exclusion).

## Plugin formats

| Format | Status | Evidence |
|---|---|---|
| **VST3** | **Verified** | Built on Linux/Windows/macOS; primary target; pluginval gate. CMakeLists.txt:262; build.yml all jobs |
| **AU (Audio Unit)** | **Verified (build + conformance)** / **Unverified (host)** | Built on macOS as `.component` (universal) and, since 0.9.4, put through the same blocking pluginval gate as the VST3 (both modes ×3, against the packaged bundle, after an install into `~/Library/Audio/Plug-Ins/Components/` + `AudioComponentRegistrar` restart) — and, since the `macos-intel` job, through that same gate a second time against a thin `x86_64` build on **native Intel** hardware. Still unverified against a *real* host: pluginval loads the AU through JUCE's `AudioUnitPluginFormat`, so Logic/GarageBand loading is not tested in repo, and `auval` is not run (`docs/procedures/CI_CD.md` §"Known coverage limits"). CMakeLists.txt:263-265; .github/workflows/build.yml:1629 (the `macos` job header) |
| **Standalone** | **Verified** | Built on all three OSes. CMakeLists.txt:266-268 |
| **AAX** | **Not Supported** | Out of scope: needs an Avid account + PACE/iLok signing. docs/policies/COMPATIBILITY_POLICY.md. (DSP core is wrapper-agnostic, so a future AAX wrapper is low-cost, but it is explicitly not built today.) |

## Platforms / architectures

| Platform | Status | Evidence |
|---|---|---|
| **Linux x86-64** | **Verified (blocking gate)**, above a **declared ABI floor** | CI builds VST3+Standalone; headless pluginval at the configured strictness (deterministic ×3 + randomise ×3) under xvfb — **blocking**. The shipped binaries' glibc/libstdc++ floor is asserted on every push against the exact stripped bytes (`scripts/check-linux-abi.py`, which holds the number; this table deliberately quotes none). Distributions **below** that floor are not supported: the dynamic loader refuses the library before any of this project's code runs. `.github/workflows/build.yml` |
| **Windows x86-64** | **Verified (blocking gate)** | MSVC build; pluginval at the configured strictness, deterministic ×3 + randomise ×3 — **blocking** (`run-pluginval.ps1`, no `continue-on-error`). `.github/workflows/build.yml` |
| **macOS universal (arm64 + x86_64)** | **Verified (blocking gate)** | `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, `lipo` verifies both slices; pluginval at the configured strictness, both modes ×3 — **blocking**. `.github/workflows/build.yml` |
| **macOS x86_64 on native Intel silicon** | **Verified (blocking gate)** | Distinct from the row above, which is built and validated on an **Apple Silicon** runner and executes its x86_64 slice only under **Rosetta 2** (translated onto arm64 hardware). The `macos-intel` job builds thin `x86_64` on `macos-15-intel` and runs both self-test suites plus the full pluginval gate (VST3 and AU, both modes ×3) on a real Intel CPU, after asserting `uname -m == x86_64` and `sysctl.proc_translated == 0`. It ships nothing — the shipped bundle is still the universal one. .github/workflows/build.yml:2183 (the `macos-intel` job header), .github/workflows/build.yml:2126-2182 (its rationale block) |

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
| JUCE | **9.0.1** — immutable commit `e18f7f5…` (FetchContent, `GIT_SHALLOW`; ADR-0022, ADR-0026) | **Verified** | CMakeLists.txt:52-54, 63-71 |
| C++ standard | C++23 (`CMAKE_CXX_STANDARD 23`; ADR-0027) | **Verified** | CMakeLists.txt:16-18 |
| pluginval | latest release (downloaded by script) | **Verified** | scripts/run-pluginval.sh:121 |

See `docs/policies/DEPENDENCY_POLICY.md` for the JUCE version-lock reasoning.

## Windows runtime requirement

The shipped Windows binaries need the **Visual C++ redistributable for the MSVC ABI series they were
built against**, and that series — not the exact toolset — is what a user has to have installed.
`windows-latest` floats and MSVC is auto-detected, so until 2026-08-18 nothing in the pipeline said
which toolchain produced a release: an image move changed the shipped compiler with no line in any
diff, and a released `.vst3` could not be traced back to the compiler that made it.

The `windows` job now reads the toolset out of the CMake cache, records it in the job summary, and
**asserts the ABI series is 14.x**. The narrowness is the point: every 14.x toolset since VS2015 is
binary compatible and needs the same redistributable, so gating on the full version would fail on
ordinary compiler updates that change nothing a user can observe. A move *off* 14.x is the event
that changes what users must install, and that is what fails.

Reporting must not decide whether a release ships, so a cache it cannot read is a `::warning::` with
the lines it looked at, never a failure — unlike the Linux floor below, which is about the artifact
itself rather than about a record of it.

## Linux runtime ABI floor

The Linux artifact's minimum system is not a choice this project made — it is whatever the runner
image's glibc and libstdc++ happened to be when the binaries were linked. Every imported symbol
carries the version that introduced it, and the **maximum** of those versions is the oldest system
that can load the library at all: below it the dynamic loader fails with
`version 'GLIBC_x.y' not found` before any of this project's code runs, so it is not a degraded
experience, it is a plug-in that does not appear.

`scripts/check-linux-abi.py` asserts that maximum against a declared floor on every push, in the
`linux` job, **after** the strip step (so it reads the exact bytes users receive) and **before**
pluginval (so a breach fails ahead of the twenty-minute gate). The number lives in that script and
**this table deliberately does not restate it**, for the same reason nothing here restates the
pluginval strictness: two copies of a number is one copy that goes stale.

What the gate is for is the direction of travel. A runner-image move raises the floor silently and
retroactively — the artifact simply stops loading on systems it loaded on last week, with no
failure anywhere in CI and no line in any diff. This makes the run that raises it the run that
fails. Raising the floor is then a deliberate act: change the constant in the same change, and say
in the PR which systems it drops.

Lowering the floor is a different and larger question — it means building against an older
toolchain or a sysroot, which is a release-topology decision rather than a CI tweak. The gate does
not attempt it and does not pretend to; it makes the cost visible so the decision can be taken
knowingly.
