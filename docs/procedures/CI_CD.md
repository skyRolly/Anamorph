# CI_CD.md

Continuous integration / delivery. Source of truth: `.github/workflows/build.yml`.

## Triggers

`push` to any branch (`"**"`), `pull_request`, and `workflow_dispatch`. Permissions:
`contents: read`. Evidence [Verified]: build.yml:3-10.

## Build matrix

Every push builds the full set of formats on all three desktop OSes:

| Job | Runner | Builds | pluginval |
|---|---|---|---|
| **linux** | `ubuntu-latest` | VST3 + Standalone (+ tests) | strictness 10, **both modes** (deterministic + randomise×3) — **authoritative gate** (blocking) |
| **windows** | `windows-latest` (MSVC, multi-config) | VST3 + Standalone (+ tests) | strictness 10, both modes — informational (`continue-on-error`) |
| **macos** | `macos-14` (Apple Silicon) | universal VST3 + AU + Standalone (+ tests) | strictness 10, both modes — informational (`continue-on-error`) |

Evidence [Verified]: build.yml:12-19,22-23,63-64,116-117.

## Pipeline (per job)

1. **Checkout** (`actions/checkout@v4`).
2. **Configure** — `cmake -B build [-G Ninja] -DCMAKE_BUILD_TYPE=Release
   -DANAMORPH_BUILD_NUMBER=${{ github.run_number }}` (the run number becomes the About-box build
   number). Windows uses the default VS generator; macOS adds
   `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`.
3. **Build** — `cmake --build build --config Release`.
4. **DSP self-tests** — `scripts/run-tests.sh` (Linux/macOS); on Windows the runner locates and
   runs `AnamorphTests.exe`.
5. **pluginval** — at strictness 10 in **both modes** on Linux (blocking): `run-pluginval.sh 10
   deterministic` (fixed seed) **and** `run-pluginval.sh 10 randomise` (`--randomise`, 3 consecutive
   passes). Windows/macOS download pluginval and run both modes informationally (`continue-on-error`).
6. **Stage + upload artifacts** (`actions/upload-artifact@v4`).

Evidence [Verified]: build.yml:24-61 (linux), :63-114 (windows), :116-178 (macos).

## Why Linux is the authoritative gate

Linux runs headless pluginval at strictness 10 under `xvfb`, in **both** the deterministic and the
randomise×3 modes, and is **blocking** for both; Windows/macOS pluginval is informational so a flaky
GUI test on those runners never blocks the tester artifacts. Evidence [Verified]: build.yml:17-19,41-45,82-83,140-141.

## Artifacts

| Artifact | Contents | `if-no-files-found` |
|---|---|---|
| `Anamorph-Linux` | `Anamorph.vst3` + `Anamorph` (Standalone) | warn |
| `Anamorph-Windows` | `Anamorph.vst3` + `Anamorph.exe` (Standalone) | warn |
| `Anamorph-macOS` | universal `Anamorph.vst3` + `.component` (AU) + `.app` + `INSTALL.txt` | error |

The macOS job ad-hoc codesigns the bundles and verifies both arch slices with `lipo -archs`.
Evidence [Verified]: build.yml:47-61,97-114,150-178.

## Reproducing CI locally

```bash
scripts/setup-linux.sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_NUMBER=0
cmake --build build --config Release
scripts/run-tests.sh
scripts/run-pluginval.sh 10 deterministic
scripts/run-pluginval.sh 10 randomise        # --randomise x3 (the state-restoration gate)
```

See `TESTING.md` for the validation gate and `PACKAGING.md` for the macOS signing/quarantine steps.
