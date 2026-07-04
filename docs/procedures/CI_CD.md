# CI_CD.md

Continuous integration / delivery. Source of truth: `.github/workflows/build.yml`.

## Triggers

`push` to any branch (`"**"`), `pull_request`, and `workflow_dispatch`. Permissions:
`contents: read`. Evidence [Verified]: build.yml:3-10.

## Build matrix

Every push builds the full set of formats on all three desktop OSes:

| Job | Runner | Builds | pluginval |
|---|---|---|---|
| **linux** | `ubuntu-latest` | VST3 + Standalone (+ tests) | strictness 10, **both modes ×3** (deterministic + randomise) — **blocking** |
| **windows** | `windows-latest` (MSVC, multi-config) | VST3 + Standalone (+ tests) | strictness 10, **both modes ×3** — **blocking** |
| **macos** | `macos-14` (Apple Silicon) | universal VST3 + AU + Standalone (+ tests) | strictness 10, **both modes ×3** — **blocking** |

Validation is **uniform and blocking on every platform**: there is no `continue-on-error` — a non-zero
pluginval exit fails the job everywhere (the old Windows/macOS `continue-on-error` masked real `exit 1`
failures as green and has been removed). Evidence [Verified]: `.github/workflows/build.yml`.

## Pipeline (per job)

1. **Checkout** (`actions/checkout@v5`).
2. **Configure** — `cmake -B build [-G Ninja] -DCMAKE_BUILD_TYPE=Release
   -DANAMORPH_BUILD_NUMBER=${{ github.run_number }}` (the run number becomes the About-box build
   number). Windows uses the default VS generator; macOS adds
   `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`.
3. **Build** — `cmake --build build --config Release`.
4. **DSP self-tests** — `scripts/run-tests.sh` (Linux/macOS); on Windows the runner locates and
   runs `AnamorphTests.exe`.
5. **pluginval** — at strictness 10 in **two explicit, distinct, blocking steps on every platform**:
   **deterministic** (`--random-seed 0`) **and** **randomise** (`--randomise`), each repeated **3
   consecutive passes**. Linux/macOS use `scripts/run-pluginval.sh <strictness> <mode>`; Windows uses
   `scripts/run-pluginval.ps1 -Strictness <n> -Mode <mode>` (same structure). A non-zero pluginval
   exit fails the job — no swallowed exit codes. **Both mode steps ALWAYS run**: the randomise step is
   guarded with `if: ${{ !cancelled() }}`, so a deterministic failure **never skips** randomise — both
   modes report independently every run (a deterministic failure must not hide the randomise result).
   The job still fails if either mode fails.
6. **Stage + upload artifacts** (`actions/upload-artifact@v5`).

Evidence [Verified]: `.github/workflows/build.yml`; `scripts/run-pluginval.sh`; `scripts/run-pluginval.ps1`.

## Validation is uniform and blocking on every platform

Each of Linux, Windows and macOS runs the SAME gate — pluginval strictness 10, deterministic ×3 **and**
randomise ×3 — and **all are blocking**. Linux runs headless under `xvfb`. The `--randomise` mode
randomises test order + fuzzed values to surface value-/order-dependent defects a fixed-seed run can
miss. Evidence [Verified]: `.github/workflows/build.yml`.

## Artifacts

| Artifact | Contents | `if-no-files-found` |
|---|---|---|
| `Anamorph-Linux` | `Anamorph.vst3` + `Anamorph` (Standalone) | warn |
| `Anamorph-Windows` | `Anamorph.vst3` + `Anamorph.exe` (Standalone) | warn |
| `Anamorph-macOS` | universal `Anamorph.vst3` + `.component` (AU) + `.app` + `INSTALL.txt` | error |

The macOS job ad-hoc codesigns the bundles and verifies both arch slices with `lipo -archs`.
Evidence [Verified]: `.github/workflows/build.yml`.

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
