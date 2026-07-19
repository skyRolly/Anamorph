# CI_CD.md

Continuous integration / delivery. Source of truth: `.github/workflows/build.yml`
(build + validate) and the security-scanning workflows listed in
[Security scanning](#security-scanning).

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
5. **Symbol handling (RH-PR-2, ADR-0021)** — Linux extracts split debug info (`objcopy
   --only-keep-debug`), strips the shipped binaries (`strip --strip-unneeded`; `.gnu_debuglink`
   embedded) and asserts `GetPluginFactory` is still exported — ordered **before** pluginval so
   the gate validates the stripped bytes. macOS runs `dsymutil` → `strip -x` → ad-hoc codesign
   (in that order — stripping after signing would invalidate the seal) inside the packaging
   step. Windows retains the Release linker PDBs (now generated via `/Zi` + `/DEBUG`) and
   removes them from the public bundle copy.
6. **pluginval** — at strictness 10 in **two explicit, distinct, blocking steps on every platform**:
   **deterministic** (`--random-seed 0`) **and** **randomise** (`--randomise`), each repeated **3
   consecutive passes**. Linux/macOS use `scripts/run-pluginval.sh <strictness> <mode>`; Windows uses
   `scripts/run-pluginval.ps1 -Strictness <n> -Mode <mode>` (same structure). A non-zero pluginval
   exit fails the job — no swallowed exit codes. **Both mode steps ALWAYS run**: the randomise step is
   guarded with `if: ${{ !cancelled() }}`, so a deterministic failure **never skips** randomise — both
   modes report independently every run (a deterministic failure must not hide the randomise result).
   The job still fails if either mode fails. **Windows** additionally runs with `--skip-gui-tests`:
   the GPU-less/headless `windows-latest` runner cannot host the editor GUI tests (environmental, not a
   plugin defect — the editor validates on Linux + macOS; see KI-007). This skips one *test category*
   on one runner, distinct from the mode-level "never skip" rule above; all non-GUI tests still block.
7. **Stage + upload artifacts** (`actions/upload-artifact@v5`) — public artifacts plus a
   separate `Anamorph-<OS>-debug` artifact per platform (crash-symbolication material; never
   mixed into the public one). All staging is strict: no `|| true`, `if-no-files-found: error`.
   **Customer uploads are fail-closed**: each requires the DSP self-tests AND its own
   strip/staging/packaging step to have succeeded (`steps.<id>.outcome` gating), and the staging
   steps self-validate (no symbol table, no debug files in the public copy). A pluginval-only
   failure still uploads beta artifacts; developer `-debug` artifacts survive packaging failures.

Evidence [Verified]: `.github/workflows/build.yml`; `scripts/run-pluginval.sh`; `scripts/run-pluginval.ps1`.

## Validation is uniform and blocking on every platform

Each of Linux, Windows and macOS runs the SAME gate — pluginval strictness 10, deterministic ×3 **and**
randomise ×3 — and **all are blocking**. Linux runs headless under `xvfb`. The `--randomise` mode
randomises test order + fuzzed values to surface value-/order-dependent defects a fixed-seed run can
miss. Evidence [Verified]: `.github/workflows/build.yml`.

## Artifacts

| Artifact | Contents | `if-no-files-found` |
|---|---|---|
| `Anamorph-Linux` | stripped `Anamorph.vst3` + `Anamorph` (Standalone) | error |
| `Anamorph-Linux-debug` | `Anamorph.vst3.so.debug`, `Anamorph.standalone.debug` (split debug info) | error |
| `Anamorph-Windows` | `Anamorph.vst3` + `Anamorph.exe` (Standalone; PDBs removed) | error |
| `Anamorph-Windows-debug` | `Anamorph.vst3.pdb`, `Anamorph.standalone.pdb` | error |
| `Anamorph-macOS` | universal stripped `Anamorph.vst3` + `.component` (AU) + `.app` + `INSTALL.txt` | error |
| `Anamorph-macOS-debug` | `Anamorph.vst3.dSYM`, `Anamorph.component.dSYM`, `Anamorph.app.dSYM` | error |

The macOS job captures dSYMs, strips, then ad-hoc codesigns the bundles, verifies both arch
slices with `lipo -archs`, and asserts the stripped VST3 still exports `GetPluginFactory` — all
strict (a failure fails the job; the `\|\| true` swallowing was removed in RH-PR-2/ADR-0021).
Evidence [Verified]: `.github/workflows/build.yml`.

## Security scanning

Separate from the build/validate pipeline, four security workflows/configs run against `main`:

| File | What it does | Triggers |
|---|---|---|
| `.github/workflows/codeql.yml` | CodeQL: `c-cpp` (manual build — VST3 + tests targets, Standalone off) + `actions`. Alerts filtered to repo-own code (`paths-ignore: build` excludes the FetchContent'd JUCE tree). Default query suite. | push/PR to `main` (docs-only changes skipped), weekly, dispatch |
| `.github/workflows/msvc.yml` | MSVC `/analyze` (NativeRecommendedRules) → SARIF upload. Build step required (juceaide-generated files); JUCE under `build/_deps` treated as external. | push/PR to `main` path-filtered to `src/`, `tests/`, `CMakeLists.txt`; weekly; dispatch |
| `.github/workflows/dependency-review.yml` | Dependency Review on PRs (GitHub Actions deps only — the graph does not index CMake FetchContent). Comments only on failure. | PR to `main` |
| `.github/dependabot.yml` | Weekly grouped `github-actions` version bumps (single PR). JUCE is **not** Dependabot-managed — pinned + review-gated per `DEPENDENCY_POLICY.md`. | weekly |

Both analysis workflows configure with `-DANAMORPH_BUILD_STANDALONE=OFF`: the Standalone format
recompiles the same translation units as VST3, so analyzing it doubles cost for zero extra
coverage. Evidence [Verified]: the four files above.

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
