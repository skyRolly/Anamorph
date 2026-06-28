# CI_CD.md

Continuous integration / delivery. Source of truth: `.github/workflows/build.yml`.

## Triggers

`push` to any branch (`"**"`), `pull_request`, and `workflow_dispatch`. Permissions:
`contents: read`. Evidence [Verified]: build.yml:3-10.

## Build matrix

Every push builds the full set of formats on all three desktop OSes:

| Job | Runner | Builds | pluginval |
|---|---|---|---|
| **linux** | `ubuntu-latest` | VST3 + Standalone (+ tests) | strictness 10 — **authoritative gate** (blocking) |
| **windows** | `windows-latest` (MSVC, multi-config) | VST3 + Standalone (+ tests) | strictness 10 — informational (`continue-on-error`) |
| **macos** | `macos-14` (Apple Silicon) | universal VST3 + AU + Standalone (+ tests) | strictness 10 — informational (`continue-on-error`) |

Evidence [Verified]: build.yml:12-19,21-23,60-61,107-108,131-132.

## Pipeline (per job)

1. **Checkout** (`actions/checkout@v4`).
2. **Configure** — `cmake -B build [-G Ninja] -DCMAKE_BUILD_TYPE=Release
   -DANAMORPH_BUILD_NUMBER=${{ github.run_number }}` (the run number becomes the About-box build
   number). Windows uses the default VS generator; macOS adds
   `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`.
3. **Build** — `cmake --build build --config Release`.
4. **DSP self-tests** — `scripts/run-tests.sh` (Linux/macOS); on Windows the runner locates and
   runs `AnamorphTests.exe`.
5. **pluginval** — `scripts/run-pluginval.sh 10` on Linux (blocking); Windows/macOS download
   pluginval and run strictness 10 informational.
6. **Stage + upload artifacts** (`actions/upload-artifact@v4`).

Evidence [Verified]: build.yml:24-58 (linux), :62-105 (windows), :109-167 (macos).

## Why Linux is the authoritative gate

Linux runs headless pluginval at strictness 10 under `xvfb` and is **blocking**; Windows/macOS
pluginval is informational so a flaky GUI test on those runners never blocks the tester
artifacts. Evidence [Verified]: build.yml:17-19,41-42,79-81,131-132.

## Artifacts

| Artifact | Contents | `if-no-files-found` |
|---|---|---|
| `Anamorph-Linux` | `Anamorph.vst3` + `Anamorph` (Standalone) | warn |
| `Anamorph-Windows` | `Anamorph.vst3` + `Anamorph.exe` (Standalone) | warn |
| `Anamorph-macOS` | universal `Anamorph.vst3` + `.component` (AU) + `.app` + `INSTALL.txt` | error |

The macOS job ad-hoc codesigns the bundles and verifies both arch slices with `lipo -archs`.
Evidence [Verified]: build.yml:44-58,88-105,139-167.

## Reproducing CI locally

```bash
scripts/setup-linux.sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_NUMBER=0
cmake --build build --config Release
scripts/run-tests.sh
scripts/run-pluginval.sh 10
```

See `TESTING.md` for the validation gate and `PACKAGING.md` for the macOS signing/quarantine steps.
