# BUILD.md

How to configure and build Anamorph. Headless, command-line only (CMake + JUCE; no IDE/Projucer).

## Toolchain

- **CMake ≥ 3.22**, a **C++23** compiler, **Ninja** (recommended generator). Verified on GCC 13,
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
:36-38 (JUCE 9.0.1 commit pin), :47-55 (FetchContent).

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
Evidence [Verified]: scripts/setup-linux.sh:44-54 (package list), :18-20 (the EGL note),
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
| `ANAMORPH_BUILD_TESTS` | ON | Build the `AnamorphTests` + `AnamorphStateTests` console apps (CMakeLists.txt:27,305) |
| `ANAMORPH_BUILD_STANDALONE` | ON | Add the Standalone target (CMakeLists.txt:28,210-212) |
| `ANAMORPH_JUCE_PATH` | "" | Use a local JUCE checkout instead of fetching (CMakeLists.txt:32,43-45) |
| `ANAMORPH_JUCE_TAG` | `e18f7f5…` (= tag 9.0.1) | JUCE git rev to fetch when no local path; `ANAMORPH_JUCE_VERSION` carries the readable version (CMakeLists.txt:36-38) |
| `ANAMORPH_BUILD_NUMBER` | 0 | CI build/dev number shown in the About box (CMakeLists.txt:252) |

Offline build (no network) with a local JUCE:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_JUCE_PATH=/path/to/JUCE
```

## Formats produced

`VST3` everywhere; `+ AU` additionally on macOS; `+ Standalone` when `ANAMORPH_BUILD_STANDALONE`
is ON. Evidence [Verified]: CMakeLists.txt:206-212.

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

## Network domains the build needs (restricted sandboxes)

- Ubuntu apt mirrors (`archive.ubuntu.com` / `ports.ubuntu.com`) — `setup-linux.sh`.
- `github.com` — JUCE source (pinned commit SHA via FetchContent).
- `github.com` — pluginval release (only for `scripts/run-pluginval.sh`).

Evidence [Verified]: scripts/setup-linux.sh:8-12.

## Compile definitions (part of the build contract)

`ANAMORPH_VERSION_STRING`, `ANAMORPH_BUILD_NUMBER`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`,
`JUCE_VST3_CAN_REPLACE_VST2=0`, `JUCE_DISPLAY_SPLASH_SCREEN=0`, `JUCE_REPORT_APP_USAGE=0`,
`JUCE_STRICT_REFCOUNTEDPOINTER=1`. Evidence [Verified]: CMakeLists.txt:277-284.
