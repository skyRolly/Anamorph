# COMPATIBILITY_MATRIX.md

Status taxonomy: **Verified** (provable from build/CI/code) · **Partially Verified**
(README/CI claim, not fully provable here) · **Unverified** (could work, no evidence in repo) ·
**Not Supported** (deliberate, evidence-backed exclusion).

## Plugin formats

| Format | Status | Evidence |
|---|---|---|
| **VST3** | **Verified** | Built on Linux/Windows/macOS; primary target; pluginval gate. CMakeLists.txt:344; build.yml all jobs |
| **AU (Audio Unit)** | **Verified (build + conformance)** / **Unverified (host)** | Built on macOS as `.component` (universal) and, since 0.9.4, put through the same blocking pluginval gate as the VST3 (both modes ×3, against the packaged bundle, after an install into `~/Library/Audio/Plug-Ins/Components/` + `AudioComponentRegistrar` restart) — and, since the `macos-intel` job, through that same gate a second time against a thin `x86_64` build on **native Intel** hardware. Still unverified against a *real* host: pluginval loads the AU through JUCE's `AudioUnitPluginFormat`, so Logic/GarageBand loading is not tested in repo, and `auval` is not run (`docs/procedures/CI_CD.md` §"Known coverage limits"). CMakeLists.txt:345-347; .github/workflows/build.yml:1640 (the `macos` job header) |
| **Standalone** | **Verified** | Built on all three OSes. CMakeLists.txt:348-350 |
| **AAX** | **Not Supported** | Out of scope: needs an Avid account + PACE/iLok signing. docs/policies/COMPATIBILITY_POLICY.md. (DSP core is wrapper-agnostic, so a future AAX wrapper is low-cost, but it is explicitly not built today.) |

## Platforms / architectures

| Platform | Status | Evidence |
|---|---|---|
| **Linux x86-64** | **Verified (blocking gate)**, above a **declared ABI floor** *and* a **declared ISA floor** (Haswell 2013 / Excavator 2015 — ADR-0031) | CI builds VST3+Standalone; headless pluginval at the configured strictness (deterministic ×3 + randomise ×3) under xvfb — **blocking**. The shipped binaries' glibc/libstdc++ floor is asserted on every push against the exact stripped bytes (`scripts/check-linux-abi.py`, which holds the number; this table deliberately quotes none). Distributions **below** that floor are not supported: the dynamic loader refuses the library before any of this project's code runs. `.github/workflows/build.yml` |
| **Windows x86-64** | **Verified (blocking gate)**; **no ISA floor** — the MSVC build carries no `/arch:` flag and is outside ADR-0031's scope | MSVC build; pluginval at the configured strictness, deterministic ×3 + randomise ×3 — **blocking** (`run-pluginval.ps1`, no `continue-on-error`). `.github/workflows/build.yml` |
| **macOS universal (arm64 + x86_64)** | **Verified (blocking gate)**; the `x86_64` slice is above a **declared ISA floor** (Haswell 2013 / Excavator 2015 — ADR-0031), the `arm64` slice above none | `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, `lipo` verifies both slices; pluginval at the configured strictness, both modes ×3 — **blocking**. `.github/workflows/build.yml` |
| **macOS x86_64 on native Intel silicon** | **Verified (blocking gate)** | Distinct from the row above, which is built and validated on an **Apple Silicon** runner and executes its x86_64 slice only under **Rosetta 2** (translated onto arm64 hardware). The `macos-intel` job builds thin `x86_64` on `macos-15-intel` and runs both self-test suites plus the full pluginval gate (VST3 and AU, both modes ×3) on a real Intel CPU, after asserting `uname -m == x86_64` and `sysctl.proc_translated == 0`. It ships nothing — the shipped bundle is still the universal one. .github/workflows/build.yml:2392 (the `macos-intel` job header), .github/workflows/build.yml:2205-2261 (its rationale block) |

### CPU instruction-set floor (x86-64)

The **GCC/Clang** x86-64 builds are compiled `-march=haswell -ffp-contract=off` (ADR-0031), so
**Intel Haswell (2013) / AMD Excavator (2015) is a hard requirement** for the Linux binaries and for
the `x86_64` slice of the macOS universal build. Below it the plug-in raises `SIGILL` inside the host
process — a crash, not a diagnosable rejection. The **Windows** build and the **arm64** slice carry
no such requirement. `docs/policies/COMPATIBILITY_POLICY.md` ("Runtime compatibility: the x86-64 ISA
floor") is the authority; it also records that changing the floor — raising it, lowering it, or
extending it to a platform that has none — needs an ADR and a row here.

Note the interaction with the two macOS jobs: the `macos` job executes the `x86_64` slice under
**Rosetta 2**, which does not translate AVX2 by default, so that step probes for AVX2 first and
degrades to a `::warning::` rather than reporting a product failure. The blocking Intel coverage is
`macos-intel` on native `macos-15-intel` hardware, which is Haswell+ by Apple's own requirements for
macOS 15.

### Numerical identity across architectures

**Not a goal, and measured.** The `arm64` and `x86_64` slices of the shipped macOS universal binary
do not produce identical bits: 32 of 32 twin-dump scenarios differ at shipped flags, 24 with FP
contraction disabled on both. Two causes — AArch64's base-ISA `FMLA`, and JUCE's oversampling
coefficients coming from a libm that does not agree with itself across Apple's own two architectures.
Every bit-identity claim in this repository is scoped **within one architecture and build
configuration**. `docs/policies/COMPATIBILITY_POLICY.md` ("Numerical compatibility"); evidence
`worklogs/performance/PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md` §5c.

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
| JUCE | **9.0.1** — immutable commit `e18f7f5…` (FetchContent, `GIT_SHALLOW`; ADR-0022, ADR-0026) | **Verified** | CMakeLists.txt:65-67, 76-84 |
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

It runs as the job's **last step**, and for the same reason the Linux floor below does: the event it
detects is an image moving the compiler underneath the build, so failing *before* the build would
have destroyed the very evidence needed to judge it — no suites, no pluginval, no artifacts. Placed
last, a mismatch fails the run with all of that in hand, and remains just as release-blocking, since
the release depends on the workflow's aggregate result. It reads the CMake cache, so it is gated on
the configure step rather than on the build: the toolset is still recorded when a build fails, which
is when knowing the compiler is most useful.

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

`scripts/check-linux-abi.py` asserts that maximum against a declared floor on every push, as the
**last step of the `linux` job** — it reads what the strip step produced, which is the exact bytes
users receive, and it is judged only once the suites, pluginval, staging and every upload have run.
The floor is raised by the ENVIRONMENT rather than by this repository's code, so a breach is the one
case where withholding the evidence helps least: the run goes red *and* still hands over the test
results and the artifacts needed to diagnose it. It is no less mandatory for being last — the release
depends on the workflow's aggregate result, which a failed job decides wherever it failed. The number
lives in that script and
**this table deliberately does not restate it**, for the same reason nothing here restates the
pluginval strictness: two copies of a number is one copy that goes stale.

What the gate is for is the direction of travel. A runner-image move raises the floor silently and
retroactively — the artifact simply stops loading on systems it loaded on last week, with no
failure anywhere in CI and no line in any diff. This makes the run that raises it the run that
fails. Raising the floor is then a deliberate act: change the constant in the same change, and say
in the PR which systems it drops.

**Every declared family must be present, not merely within its floor.** Both inspected artifacts are
C++ binaries linked against the system libstdc++ and glibc, so each must import every declared
family — `GLIBC_*`, `GLIBCXX_*` and, since the GCC 16 migration, `CXXABI_*`. Comparing only the families a binary happens to reference made an *absent* one read as
a satisfied one: an artifact that stopped importing `GLIBCXX_*` altogether passed the libstdc++ half
of the floor vacuously. Since 2026-08-19 that is an error, because the two things which produce it
are exactly the two the gate exists to notice — the wrong file being inspected, and a link-topology
change such as `-static-libstdc++`. The second is a real way to lower the floor, and lowering it is
the deliberate, reviewable decision described below rather than something a gate should absorb while
reporting clean.

**Why `CXXABI_*` is declared as well.** It and `GLIBCXX_*` both come out of libstdc++ but move on
their own schedules, so a compiler change can move one without the other. Measured while GCC 16 was
being evaluated for the Linux build: that artifact asked for the same `GLIBCXX_*` as the GCC 13
build, while its exception path pulled a `CXXABI_*` symbol libstdc++ first shipped a major later. A
family the gate does not name cannot raise the floor loudly, so it raises it silently — the one
failure mode this gate exists to prevent, reappearing in an undeclared family. The declared value is
the one matching the `GLIBCXX_*` floor, so the three families describe **one** runtime between them
rather than two. The Linux artifact is built by Clang (ADR-0030) and sits under all three. The script
holds the numbers; this table still quotes none.

Lowering the floor is a different and larger question — it means building against an older
toolchain or a sysroot, which is a release-topology decision rather than a CI tweak. The gate does
not attempt it and does not pretend to; it makes the cost visible so the decision can be taken
knowingly.
