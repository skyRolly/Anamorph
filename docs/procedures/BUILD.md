# BUILD.md

How to configure and build Anamorph. Headless, command-line only (CMake + JUCE; no IDE/Projucer).

## Toolchain

- **CMake ≥ 3.22**, a **C++23** compiler, **Ninja** (recommended generator). Verified on Clang 22 (which builds the Linux release artifact, ADR-0030), GCC 16.2,
  AppleClang 21 (Xcode 26.6 — the `macos-latest` image, `macos-26-arm64`) and MSVC (VS 2022 on
  `windows-latest`); on MSVC, CMake requests C++23 as `/std:c++latest` (ADR-0027). The macOS
  figure moved with the CI runner: the C++23 migration was verified on the then-current
  `macos-14` image, whose AppleClang identifies as 15.0.0.15000309 (Xcode 15.4) — the compiler
  version, not the Xcode version, is what `CMAKE_CXX_COMPILER_VERSION` reports.
- **JUCE 9.0.1** is fetched automatically (CMake `FetchContent`, pinned to the tag's immutable
  commit SHA `e18f7f5…`) — or pointed at a local checkout. See
  `docs/policies/DEPENDENCY_POLICY.md` for the version-lock reasoning, ADR-0022 for the move to
  the 9.0 line and the SHA pin, and ADR-0026 for the 9.0.1 bump.

Evidence [Verified]: CMakeLists.txt:1 (`cmake_minimum_required(VERSION 3.22)`), :16-18 (C++23),
:52-54 (JUCE 9.0.1 commit pin), :63-71 (FetchContent).

## Linux dependencies (Ubuntu)

```bash
scripts/setup-linux.sh     # safe to re-run; installs build + X11/audio/GTK/WebKit deps + xvfb
```

Installs: `build-essential cmake git ninja-build pkg-config`, `curl unzip`, ALSA/JACK/libcurl,
FreeType/Fontconfig, X11 (`libx11/xcomposite/xcursor/xext/xinerama/xrandr/xrender`),
`libglu1-mesa-dev mesa-common-dev libegl-dev`, `libwebkit2gtk-4.1-dev libgtk-3-dev`, and `xvfb`.
**`libegl-dev` is required since JUCE 9** (it creates Linux OpenGL contexts via EGL instead of
GLX; ADR-0022). If `libwebkit2gtk-4.1-dev` is unavailable, try `libwebkit2gtk-4.0-dev`.

Three of these serve **pluginval**, not the build: `xvfb` (the editor tests need a display) and
`curl` + `unzip` (`run-pluginval.sh` downloads and extracts the pluginval release). The `curl`
CLI is *not* implied by `libcurl4-openssl-dev`, which is only the development headers —
GitHub-hosted runners preinstall both tools, so a missing one surfaces on a fresh machine or a
minimal container rather than in CI.
Evidence [Verified]: scripts/setup-linux.sh:66-74 (the core package list), :83-88 (what the
`full` profile adds), :18-20 (the EGL note),
:13-17 (the curl/unzip note).

## Configure + build

```bash
# Recommended (Ninja, Release):
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# ...or the convenience wrapper (prints the produced .vst3 path):
scripts/build.sh            # scripts/build.sh [Release|Debug]
```

Evidence [Verified]: scripts/build.sh:14-15.

## Build options (CMakeLists.txt)

| Option | Default | Effect |
|---|---|---|
| `ANAMORPH_BUILD_TESTS` | ON | Build the `AnamorphTests` + `AnamorphStateTests` console apps (CMakeLists.txt:27, 483) |
| `ANAMORPH_BUILD_STANDALONE` | ON | Add the Standalone target (CMakeLists.txt:28, 388-390) |
| `ANAMORPH_JUCE_PATH` | "" | Use a local JUCE checkout instead of fetching (CMakeLists.txt:66, 77-79) |
| `ANAMORPH_JUCE_TAG` | `e18f7f5…` (= tag 9.0.1) | JUCE git rev to fetch when no local path; `ANAMORPH_JUCE_VERSION` carries the readable version (CMakeLists.txt:70-72) |
| `ANAMORPH_BUILD_NUMBER` | 0 | CI build/dev number shown in the About box (CMakeLists.txt:430) |

Offline build (no network) with a local JUCE:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_JUCE_PATH=/path/to/JUCE
```

## Formats produced

`VST3` everywhere; `+ AU` additionally on macOS; `+ Standalone` when `ANAMORPH_BUILD_STANDALONE`
is ON. Evidence [Verified]: CMakeLists.txt:384-390.

## Artifact paths

```
build/Anamorph_artefacts/Release/VST3/Anamorph.vst3
build/Anamorph_artefacts/Release/AU/Anamorph.component        # macOS only
build/Anamorph_artefacts/Release/Standalone/Anamorph[.app|.exe]
build/.../AnamorphTests                                       # the DSP self-test app
build/.../AnamorphStateTests                                  # the state-compatibility self-test app
```

Evidence [Verified]: scripts/build.sh:19-54; .github/workflows/build.yml (build/stage steps).

**Symbols (ADR-0021):** local Release builds carry full debug info (`-g` / `/Zi` via the
`AnamorphHardening` flags) and are **never stripped locally** — debugging a local build works
out of the box. Stripping (with debug-info retention as separate `Anamorph-<OS>-debug`
artifacts) happens only in CI packaging; see `docs/procedures/CI_CD.md` / `PACKAGING.md`.

## The x86-64 instruction-set baseline (ADR-0031, ADR-0032)

`AnamorphHardening` adds, for GCC/Clang x86-64 targets:

```
-march=haswell -ffp-contract=off
```

and, since ADR-0032, for MSVC: **`/arch:AVX2`** at the default (non-contracting) `/fp:precise` —
deliberately with no `/fp` flag; the VS2022+ non-contracting default is the decision, and it is
asserted per push by the `windows` job's toolset ≥ 14.30 gate and the blocking `windows-avx2-ab`
A/B rather than trusted.

so a local Release build on an x86-64 Linux or Intel Mac needs an **Intel Haswell (2013) / AMD
Excavator (2015)** CPU or newer — the same floor the shipped binaries carry. A configure on such a
target prints `Anamorph: x86-64 ISA baseline (ADR-0031) ...` so the log says which branch was taken.

Three things about this are easy to get wrong when editing `CMakeLists.txt`:

- **The two flags are one decision and must never be separated.** `-march=haswell` introduces an FMA
  instruction the frozen baseline did not have; `-ffp-contract=off` forbids its use, which is the
  only reason the change is bit-exact. Dropping the second flag moves 88.9 % of output samples and
  costs the GCC/Clang cross-check.
- **arm64 gets nothing**, deliberately (ADR-0031 option 4); MSVC's flag is ADR-0032's, not a
  variant of the GCC/Clang pair. On Apple the flags are
  passed as `-Xarch_x86_64` so a universal build reaches only the `x86_64` slice; an unqualified
  `-march=haswell` is handed to the arm64 driver invocation as well and fails the build.
- **They are compile options, not link options.** Under LTO the codegen happens at link time, but
  both toolchains stream the target CPU/feature set per function into the IR, so the compile-time
  `-march` is what governs the emitted code.

To reproduce the frozen pre-ADR-0031 baseline for a comparison — which the `DEPENDENCY_POLICY`
rule-2 twin dump needs, since a Class-A claim has to be checked both ways — configure with
**`-DANAMORPH_X86_ISA_BASELINE=OFF`**, which emits a `WARNING` naming what was given up.
`-DCMAKE_CXX_FLAGS="-march=x86-64"` does **not** work: the flags live on the `AnamorphHardening`
target, and CMake places target compile options *after* `CMAKE_CXX_FLAGS` on the command line, so
the target's `-march=haswell` is the later one and wins. Since ADR-0032 the option governs MSVC
too (`OFF` drops `/arch:AVX2`), which is exactly how the `windows-avx2-ab` blocking gate builds its
baseline side. A build with the option OFF is measurably slower and is not the shipped
configuration; nothing in CI turns it off outside that gate's baseline half.

Evidence [Verified]: CMakeLists.txt (the `AnamorphHardening` x86-64 baseline block);
`docs/policies/COMPATIBILITY_POLICY.md` ("Runtime compatibility: the x86-64 ISA floor").

## Network domains the build needs (restricted sandboxes)

- Ubuntu apt mirrors (`archive.ubuntu.com` / `ports.ubuntu.com`) — `setup-linux.sh`.
- `github.com` — JUCE source (pinned commit SHA via FetchContent).
- `github.com` — pluginval release (only for `scripts/run-pluginval.sh`).

Evidence [Verified]: scripts/setup-linux.sh:8-12.

## Compile definitions (part of the build contract)

`ANAMORPH_VERSION_STRING`, `ANAMORPH_BUILD_NUMBER`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`,
`JUCE_VST3_CAN_REPLACE_VST2=0`, `JUCE_DISPLAY_SPLASH_SCREEN=0`, `JUCE_REPORT_APP_USAGE=0`,
`JUCE_STRICT_REFCOUNTEDPOINTER=1`. `ANAMORPH_BUILD_NUMBER` is the one of these attached to the
single translation unit that reads it rather than to the targets — its value changes every CI run,
and a target-wide definition put that changing value on the command line of every TU.
Evidence [Verified]: CMakeLists.txt:452-462.
