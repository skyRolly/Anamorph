# BUILD.md

How to configure and build Anamorph. Headless, command-line only (CMake + JUCE; no IDE/Projucer).

## Toolchain

- **CMake ≥ 3.22**, a **C++17** compiler, **Ninja** (recommended generator).
- **JUCE 8.0.14** is fetched automatically (CMake `FetchContent`, pinned tag) — or pointed at a
  local checkout. See `docs/policies/DEPENDENCY_POLICY.md` for the version-lock reasoning.

Evidence [Verified]: CMakeLists.txt:1 (`cmake_minimum_required(VERSION 3.22)`), :16-18 (C++17),
:33 (JUCE tag 8.0.14), :44-50 (FetchContent).

## Linux dependencies (Ubuntu)

```bash
scripts/setup-linux.sh     # safe to re-run; installs build + X11/audio/GTK/WebKit deps + xvfb
```

Installs: `build-essential cmake git ninja-build pkg-config`, ALSA/JACK/curl, FreeType/Fontconfig,
X11 (`libx11/xcomposite/xcursor/xext/xinerama/xrandr/xrender`), `libglu1-mesa-dev mesa-common-dev`,
`libwebkit2gtk-4.1-dev libgtk-3-dev`, and `xvfb`. If `libwebkit2gtk-4.1-dev` is unavailable, try
`libwebkit2gtk-4.0-dev`. Evidence [Verified]: scripts/setup-linux.sh:21-33.

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
| `ANAMORPH_BUILD_TESTS` | ON | Build the `AnamorphTests` console app (CMakeLists.txt:27,207) |
| `ANAMORPH_BUILD_STANDALONE` | ON | Add the Standalone target (CMakeLists.txt:28,141-143) |
| `ANAMORPH_JUCE_PATH` | "" | Use a local JUCE checkout instead of fetching (CMakeLists.txt:32,38-40) |
| `ANAMORPH_JUCE_TAG` | 8.0.14 | JUCE tag to fetch when no local path (CMakeLists.txt:33) |
| `ANAMORPH_BUILD_NUMBER` | 0 | CI build/dev number shown in the About box (CMakeLists.txt:178) |

Offline build (no network) with a local JUCE:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_JUCE_PATH=/path/to/JUCE
```

## Formats produced

`VST3` everywhere; `+ AU` additionally on macOS; `+ Standalone` when `ANAMORPH_BUILD_STANDALONE`
is ON. Evidence [Verified]: CMakeLists.txt:137-143.

## Artifact paths

```
build/Anamorph_artefacts/Release/VST3/Anamorph.vst3
build/Anamorph_artefacts/Release/AU/Anamorph.component        # macOS only
build/Anamorph_artefacts/Release/Standalone/Anamorph[.app|.exe]
build/.../AnamorphTests                                       # the DSP self-test app
```

Evidence [Verified]: scripts/build.sh:19-30; .github/workflows/build.yml (build/stage steps).

**Symbols (ADR-0021):** local Release builds carry full debug info (`-g` / `/Zi` via the
`AnamorphHardening` flags) and are **never stripped locally** — debugging a local build works
out of the box. Stripping (with debug-info retention as separate `Anamorph-<OS>-debug`
artifacts) happens only in CI packaging; see `docs/procedures/CI_CD.md` / `PACKAGING.md`.

## Network domains the build needs (restricted sandboxes)

- Ubuntu apt mirrors (`archive.ubuntu.com` / `ports.ubuntu.com`) — `setup-linux.sh`.
- `github.com` — JUCE source (pinned tag via FetchContent).
- `github.com` — pluginval release (only for `scripts/run-pluginval.sh`).

Evidence [Verified]: scripts/setup-linux.sh:8-13.

## Compile definitions (part of the build contract)

`ANAMORPH_VERSION_STRING`, `ANAMORPH_BUILD_NUMBER`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`,
`JUCE_VST3_CAN_REPLACE_VST2=0`, `JUCE_DISPLAY_SPLASH_SCREEN=0`, `JUCE_REPORT_APP_USAGE=0`,
`JUCE_STRICT_REFCOUNTEDPOINTER=1`. Evidence [Verified]: CMakeLists.txt:180-189.
