# CI_CD.md

Continuous integration / delivery. Source of truth: `.github/workflows/build.yml`
(build + validate), `.github/workflows/release.yml` (tag-triggered release pipeline
skeleton, RH-PR-8) and the security-scanning workflows listed in
[Security scanning](#security-scanning).

## Triggers

`build.yml`: `push` to any branch (`"**"`), `pull_request`, `workflow_dispatch`, and
`workflow_call` (so `release.yml` can reuse the whole matrix — tag pushes do NOT trigger
`build.yml` directly, the `branches` filter excludes tag events). Permissions:
`contents: read`. Evidence [Verified]: build.yml (`on:` block).

**Concurrency.** One in-flight run per ref: `concurrency.group` is
`${{ github.workflow }}-${{ github.ref }}`, and `cancel-in-progress` is guarded on the ref
(`!startsWith(github.ref, 'refs/tags/')`) rather than set to a bare `true`. Three pushes in ten
minutes previously left three full matrices racing; now the older ones are cancelled — **except on
a tag**, where a release build must never be cancelled by an unrelated push, so tag refs queue.

**Same-repo pull requests are skipped.** Every job carries
`if: github.event_name != 'pull_request' || github.event.pull_request.head.repo.full_name != github.repository`.
`push: ["**"]` already builds the SHA, so the `pull_request` event was a duplicate 3-OS matrix per
commit for as long as a PR stayed open. **Fork** PRs are not covered by the push trigger (the push
happens in the fork), so they still run — the only case that trigger uniquely serves. Two
consequences: these jobs report as **skipped** in a same-repo PR's checks list (they can still be
required checks if the branch protection accepts a skipped conclusion), and inside a reusable
workflow `github.event_name` is the **caller's** event — `release.yml` fires only on a tag `push`
and `workflow_dispatch`, so the guard is true there and every job runs. **Adding a `pull_request:`
trigger to `release.yml` would break that**; no job here gates on another job's output, so the
failure mode is a missing check rather than a green run that built nothing.
Evidence [Verified]: `.github/workflows/build.yml` (`concurrency:` block; each job's `if:`).

## pluginval strictness lives in one place

`ANAMORPH_PLUGINVAL_STRICTNESS` in `build.yml`'s `env:` block is the **single authority** for the
number, and it is **10**. It replaced six literal `10`s spread across the three build jobs — six
chances for a raise to land in five of them. This document and `TESTING_POLICY.md` describe what the
gate *requires* and how it is *wired*; neither restates the value.
Evidence [Verified]: `.github/workflows/build.yml` (`env:` block).

`release.yml`: `push` of an annotated `v[0-9]+.[0-9]+.[0-9]+` tag, plus `workflow_dispatch`
as a no-release **rehearsal** (validate + full build only). Jobs: fail-closed metadata
validation (tag ⇄ `CMakeLists.txt` version ⇄ `CHANGELOG.md` section, annotated-tag check, and —
since the section is published verbatim as the release **notes body**, heading included — a check
that the heading carries an ISO release date, which rejects a bare undated heading as well as
`Unreleased`; the release *title* is set separately) →
`build.yml` via `workflow_call` (single build, identical gates and artifacts) → **draft**
GitHub Release (the validated `Anamorph-<OS>` staging trees archived as
`Anamorph-<version>-<OS>.zip` with the executable bits the artifact transport drops
restored and verified fail-closed, + the two installers (Windows Inno Setup exe, macOS
pkg; the Linux installer is `install.sh` inside the Linux zip), already version-named at
build time and moved unmodified after a fail-closed name/version check,
+ `Anamorph-<version>-UserManual.md` + `Anamorph-<version>-NOTICE.txt`
+ `Anamorph-<version>-THIRD_PARTY_LICENSES.md` + `Anamorph-<version>-SUPPORT.md`
+ `SHA256SUMS.txt` over all assets + `RELEASE_MANIFEST.txt` + the CHANGELOG section as notes; `contents: write` scoped to that one
job; publishing the draft stays a manual maintainer action per RELEASE_POLICY). No
third-party actions beyond `actions/checkout` / `actions/download-artifact` + the `gh` CLI
with the ephemeral `GITHUB_TOKEN`; no signing secrets exist in the repository.
Evidence [Verified]: release.yml.

## Build matrix

Every push builds the full set of formats on all three desktop OSes, alongside four
non-packaging jobs that guard classes the build matrix cannot see:

| Job | Runner | Builds | pluginval |
|---|---|---|---|
| **docs** | `ubuntu-latest` | — (`scripts/check-docs.py --self-test` then the lint) | — |
| **source-lint** | `ubuntu-latest` | — (each lint preceded by its own `--self-test`: `check-portability.py`, then `check-citations.py --check`) | — |
| **linux** | `ubuntu-latest` | VST3 + Standalone (+ tests) | VST3, **both modes ×3** (deterministic + randomise) — **blocking** |
| **linux-clang** | `ubuntu-latest` | Clang: both test targets + `Anamorph_VST3` (Standalone off) | — (no packaging; the warning gate + both suites) |
| **sanitizers** | `ubuntu-latest` | Clang ASan+UBSan build, plus an unsanitized build for valgrind | — |
| **windows** | `windows-latest` (MSVC, multi-config) | VST3 + Standalone (+ tests) | VST3, **both modes ×3** — **blocking** |
| **macos** | `macos-latest` (Apple Silicon) | universal VST3 + AU + Standalone (+ tests) | **VST3 and AU**, both modes ×3 each — **blocking** |

None of the four non-packaging jobs is in a `needs:` chain, in either direction. A prose defect, a
portability lint hit or a sanitizer finding fails the run without skipping a binary that is
otherwise fine, and a red build does not skip them.

### What the non-packaging jobs are for

- **docs** — structural Markdown lint over the whole document set (table integrity, relative links,
  blockquote lazy continuation, unclosed fences, and the CHANGELOG entry-boundary rule the release
  notes extraction depends on). `--self-test` runs **first** and is the load-bearing half: a checker
  that has stopped matching anything is indistinguishable from a clean tree.
- **source-lint** — (a) the **JUCE SIMD overload hazard**: an explicit template argument on
  `juce::jmin/jmax/snapToZero` instantiates `dsp::SIMDRegister<T>`, which completes on Linux (where
  `size_t` IS `uint64_t`) and **fails to compile on macOS** (where it is not). The divergence is in
  the *typedef*, so no Linux compiler can see it — a lint is the only Linux-runnable guard, and
  `linux-clang` would not catch it. The tree is clean of it; the job is a regression guard.
  (b) the **evidence-anchor gate**: `docs/` carries 184 `file.cpp:NNN` citations, and an edit above
  one silently re-aims it. See [Evidence anchors](#evidence-anchors).
  Each of the two runs its own `--self-test` **first**, in this job, immediately before the lint it
  verifies — the same load-bearing move as `docs`, and required by `TESTING_POLICY.md` rule 4. The
  portability self-test is not the same check as `--compile-canary` in `linux-clang`: that one asks
  whether the pinned JUCE still *has* the hazard, this one whether the checker still *finds* it, and
  a green canary over a dead scanner reports a clean tree.
- **linux-clang** — `juce_recommended_warning_flags` picks its set by **compiler ID**, and Clang's is
  strictly larger than GCC's (`-Wshorten-64-to-32`, `-Wconditional-uninitialized`,
  `-Wsign-conversion`, `-Wcast-align`, `-Wshift-sign-overflow`,
  `-Wzero-as-null-pointer-constant`, `-Wimplicit-int-float-conversion`). Every one of those reached
  this project only from the macOS runner, minutes into a universal build — which is exactly how the
  four `-Wimplicit-int-float-conversion` sites below sat in the tree for months. The gate is
  **first-party warnings only** (`src/`, `tests/`), classified **structurally** by resolved path
  rather than by an anchored `grep` on one spelling the build system is free to change — this build
  emits **two** spellings of one header today (`src/dsp/ScopeBuffer.h` and
  `src/gui/../dsp/ScopeBuffer.h`), which the resolver folds into one key and a text match would not.
  `check-clang-warnings.py --self-test` proves the classifier **and the baseline comparison** are
  live before their silence is trusted.
  `-Werror` is not used and cannot be: JUCE's module sources compile into our targets, so a blanket
  `-Werror` would gate on a dependency's warnings and be switched off at the first JUCE bump.
  It is a **no-new-warnings** gate — see [The Clang warning baseline](#the-clang-warning-baseline).
  The job builds `Anamorph_VST3` as well as the suites because the plugin is the **only** target that
  links `juce_recommended_lto_flags` — a defect the optimiser acts on only at `-flto` is invisible to
  every non-LTO artefact in this pipeline, and LTO is what users install. Linux+Clang uses **lld**
  (`AnamorphHardening`, probed with `check_linker_flag`): GNU ld scans a static archive once, while
  Clang's LTO codegen runs after that scan and then needs members it passed over. GCC — the shipped
  Linux build — never reaches that branch, so the released binary's link is unchanged.
- **sanitizers** — ASan + UBSan over both suites, then **valgrind memcheck** over both suites from a
  separate *unsanitized* Release build. The point is to catch **on Linux** defects that only
  *manifest* elsewhere: Linux hands back zero-filled pages and macOS does not, so an uninitialised
  read of DSP state is benign here and arbitrary there. MemorySanitizer is deliberately not used —
  it needs every dependency including JUCE instrumented, and an uninstrumented one produces false
  positives rather than silence; memcheck answers the same question with no rebuild. valgrind runs
  **both** suites because the read that would matter runs through the real wrapper `processBlock`,
  which only `AnamorphStateTests` drives. `--error-exitcode=1` makes a finding fail the job (not
  valgrind's default). `detect_leaks=0` — JUCE's singletons are torn down at exit in ways
  LeakSanitizer reports and this is not a leak gate.
  The valgrind step sets **`ANAMORPH_TESTS_NO_FTZ=1`**, which relaxes exactly one assertion and only
  under this tool. `juce::ScopedNoDenormals` sets the CPU's FTZ/DAZ bits so a denormal result is
  flushed to zero *in hardware*; valgrind emulates floating point and does not honour those bits, so
  denormals survive into the output and "engine output free of NaN/Inf/denormals" fails on a build
  that is correct on every real CPU. Measured, not assumed: on that same run memcheck reports **zero
  errors** — the tool finds no defect and the test fails anyway. NaN and Inf stay failures; only the
  denormal half is relaxed, only in this step, and every native job asserts the full check, so the
  `DSP_POLICY` invariant is still gated on every push on every platform. Do not set the variable
  anywhere else. (Pointing valgrind at the state suite alone was the alternative and was rejected:
  that suite passes under memcheck untouched, but it would leave the DSP suite with no
  uninitialised-read detector at all.)

`MALLOC_PERTURB_=1` is set on the `linux` and `linux-clang` self-test steps: glibc then fills **fresh**
heap with `0xFE` and **freed** heap with `0x01`, so an uninitialised read of audio state comes back as
≈ `-1.69e38` — enormous *and* wrong-signed, which every level, null, transparency and click-free
assertion in both suites fails on. It is the cheap version of the question valgrind answers slowly, on
every push. Not set on macOS, where libmalloc ignores it and the variable would read as coverage that
does not exist.

**The value is not the fill byte**, and this setting said `255` for one round on the assumption that
it was. glibc applies the variable asymmetrically —
`alloc_perturb(p,n) → memset(p, perturb_byte ^ 0xff, n)` but
`free_perturb(p,n) → memset(p, perturb_byte, n)` — so the fresh-allocation fill is the **complement**
of the value. `255` therefore wrote `0x00` into fresh buffers: precisely the benign zero-fill the step
exists to defeat, and *strictly worse than leaving the variable unset*, because unset a recycled chunk
still reads back real garbage while `255` zeroes that too. Measured, not inferred.

The original rationale — "`0xFF` fills a float buffer with NaN" — could not have held at **any** value.
A float whose four bytes are all `B` has exponent `((B & 0x7f) << 1) | (B >> 7)`, which is `0xFF` only
for `B = 0xFF`, and a fresh fill of `0xFF` needs `perturb_byte = 0`, which is glibc's
"perturbation off" sentinel. All 255 selectable values were swept: 254 give a loud finite float, `255`
gives zero, and none gives NaN, Inf or a denormal. NaN coverage is what the `sanitizers` job provides.

All three runners use the **floating** `*-latest` label. macOS moved off the pinned `macos-14`
image on 2026-08-15: `actions/runner-images` marks macOS 14 **deprecated** (deprecation opened
2026-07-06, October brownouts, **fully unsupported 2026-11-02**, after which a job carrying the
label is terminated with an error), and `macos-latest` currently resolves to **macOS 26 Arm64**.
The x86_64 half of the universal binary is cross-compiled on the arm64 runner and the packaging
step **asserts** both slices are present, so an image change that broke the fat build fails the job
rather than silently shipping a thin one. That step previously only `echo`ed the output of `lipo
-archs`, which verifies nothing — `lipo -archs` exits 0 for **any** valid Mach-O, thin ones
included — so an arm64-only build would have shipped labelled universal with a green tick and the
evidence sitting unread in the log. It now loops over the three bundles and fails if either `arm64`
or `x86_64` is absent.

The x86_64 slice is also **executed** now, under Rosetta 2, by a second self-test step
(`ANAMORPH_TEST_RUNNER="arch -x86_64" scripts/run-tests.sh`). Until that step existed the slice was
compiled on every push and run by nothing on any platform: half the macOS user base had
compilation-only coverage, on a product whose `CMAKE_OSX_DEPLOYMENT_TARGET=10.13` exists precisely
to claim Intel support. If Rosetta is absent the step emits a `::warning::` naming the coverage that
was lost rather than passing quietly — Rosetta's presence is the *image's* property, not the
product's, so a green product should not go red for it, but a gate that silently does nothing is
worse than no gate. It is a **blocking condition on the macOS customer uploads**, exactly like the
native arm64 run — a slice that fails its behavioural gate is a defect in the product, so shipping
the package anyway would validate the Intel half and then ignore the verdict. (The Rosetta-absent
path exits 0, so it does not block: the uploads proceed on compilation-only Intel coverage with the
`::warning::` as the record.) **Native** Intel hardware is still not covered; see
[Known coverage limits](#known-coverage-limits).

The image carries the macOS toolchain, so this moved the macOS compiler with it: **AppleClang
15.0.0.15000309 (Xcode 15.4) → 21.0.0.21000101 (Xcode 26.6)**, image `macos-26-arm64`
`20260728.0273.1`. `CMAKE_OSX_DEPLOYMENT_TARGET=10.13` is still accepted and both slices still
build. One measured consequence: AppleClang 21 raised
**`-Wimplicit-int-float-conversion` at four pre-existing sites** — `src/PluginEditor.cpp:245,246`,
`src/gui/LookAndFeel.cpp:262` and `src/dsp/VelvetNoise.cpp:30`, each an `int` widened inside a
float expression (108 → 126 warning instances on that first job). No warning disappeared and no
other category appeared. **All four were then fixed** in the follow-up change: each `int` operand
now carries the explicit `(float)` cast that spells out the conversion the compiler was already
performing implicitly, which is why the three translation units compile to **byte-identical**
machine code before and after (verified object-for-object at the shipped flags with `-g0
-fno-lto`, so only real codegen is compared). Confirmed on the runner: the macOS job's warning set
is now **15 sites / 108 instances, `diff`-identical to the `macos-14` / AppleClang 15 set** — the
image change added four diagnostics and the fix removed exactly those four. Bit-exact macOS
output across the two compilers is **not** claimed: it is not provable headlessly from this
repository, and compiler-level numerical differences are the Class-B changes `DSP_POLICY.md`
permits (see RH-F4). What is proven is the behavioural gate — both suites and both pluginval
modes green on the new image.

Validation is **uniform and blocking on every platform**: there is no `continue-on-error` — a non-zero
pluginval exit fails the job everywhere (the old Windows/macOS `continue-on-error` masked real `exit 1`
failures as green and has been removed). Evidence [Verified]: `.github/workflows/build.yml`.

## Pipeline (per job)

1. **Checkout** (`actions/checkout@v7`).
2. **Configure** — `cmake -B build [-G Ninja] -DCMAKE_BUILD_TYPE=Release
   -DANAMORPH_BUILD_NUMBER=${{ github.run_number }}` (the run number becomes the About-box build
   number). Windows uses the default VS generator; macOS adds
   `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`.
3. **Build** — `cmake --build build --config Release`.
4. **DSP + state self-tests** — `scripts/run-tests.sh` runs `AnamorphTests` **and**
   `AnamorphStateTests` fail-closed (Linux/macOS); on Windows the step locates and runs both
   `AnamorphTests.exe` and `AnamorphStateTests.exe`, propagating the first failing exit code.
   Discovery is fail-closed on **ambiguity** as well as absence, on all three platforms: exactly one
   match is required, and both binaries are located before either runs. The previous
   `find … | head -n1` / `Select-Object -First 1` took whichever path enumerated first, so a
   multi-config tree (which is what Windows always has) or a stale second build tree could gate on a
   *Debug* binary while the uploaded artifacts came from Release — a green report about the wrong
   build. macOS then repeats the step for the **x86_64 slice under Rosetta** (see above).
5. **Symbol handling (RH-PR-2, ADR-0021)** — Linux extracts split debug info (`objcopy
   --only-keep-debug`), strips the shipped binaries (`strip --strip-unneeded`; `.gnu_debuglink`
   embedded) and asserts `GetPluginFactory` is still exported — ordered **before** pluginval so
   the gate validates the stripped bytes. The debuglink now stores the **bare basename**, written by
   running `objcopy` from inside the debug directory: `objcopy` stores the filename exactly as given
   and a debugger resolves it relative to the *stripped binary's own* directory, so the previous
   `--add-gnu-debuglink="dist/Anamorph-Linux-debug/…"` baked a CI-workspace-relative path into the
   shipped `.so` — a path that exists on no user's machine, making the downloaded `-debug` artifact
   unfindable and the debuglink decoration. macOS runs `dsymutil` → `strip -x` → ad-hoc codesign
   (in that order — stripping after signing would invalidate the seal) inside the packaging
   step. Windows retains the Release linker PDBs (now generated via `/Zi` + `/DEBUG`) and
   removes them from the public bundle copy.
6. **pluginval** — at `ANAMORPH_PLUGINVAL_STRICTNESS` in **two explicit, distinct, blocking steps on
   every platform**: **deterministic** (`--random-seed 1`) **and** **randomise** (`--randomise`),
   each repeated **3 consecutive passes**. Linux/macOS use
   `scripts/run-pluginval.sh <strictness> <mode> [format]`; Windows uses
   `scripts/run-pluginval.ps1 -Strictness <n> -Mode <mode>` (same structure). A non-zero pluginval
   exit fails the job — no swallowed exit codes.
   **The seed is nonzero, and that is a fix, not a detail.** Both scripts previously passed
   `--random-seed 0`, and pluginval treats **0 as "generate a random seed"** (`Source/PluginTests.h`;
   `Source/CommandLine.cpp` only forwards the flag when it differs from that default), so
   `--random-seed 0` is exactly equivalent to passing nothing — the "deterministic" half of the
   release gate was not deterministic on any platform, and a failure in it was not reproducible from
   the log. Verified against pluginval 1.0.4: seed 0 printed a different `Random seed:` every run,
   seed 1 printed `0x1` every time. The two scripts pin the same nonzero value so the three platforms
   validate against the same seed.
   **Both mode steps ALWAYS run** for a *validation* failure: they carry the same explicit condition
   (`!cancelled() && steps.<producer>.outcome == 'success'`), so a deterministic failure **never
   skips** randomise — both modes report independently every run. The producer is named rather than
   using a bare `!cancelled()`, which is true after *any* upstream failure: on Linux it is `strip`,
   on **macOS** it is `package`, and on Windows it is `build` — in each case the step that produces
   the bytes being validated. Without it a compile error let the gates run against a tree with no
   plug-in and the job's last error was a cascade rather than the cause.
   **On macOS the gates run after packaging**, against `dist/Anamorph-macOS/`, so they see the
   stripped and ad-hoc-signed bundles the artifact is uploaded from. They used to run before it, and
   every pass then described a bundle that `strip -x` and `codesign --force --deep` rewrote
   immediately afterwards. The trade is stated rather than discovered: a *packaging* failure now
   skips validation, which is the same trade Linux already makes and for the same reason — a
   half-packaged bundle is in a state nobody ships. The uploads are unaffected; they gate on
   `tests` + `package` and never on pluginval, so a validation-only failure still yields a beta
   artifact.
   **macOS also validates the AU**, at the same strictness, in the same two modes, ×3 each. It is the
   one format that exists on exactly one platform and the only one Logic and GarageBand load, and
   until this landed the gate ran against the VST3 alone on all three OSes — so the AU shipped to
   Logic users having passed no automated validation at all. The `.component` is **installed** into
   `~/Library/Audio/Plug-Ins/Components` first (and `AudioComponentRegistrar` killed to force a
   re-scan) because macOS resolves Audio Units through the AudioComponent registry, which only knows
   about bundles under a Components directory: a `.component` outside one can report zero plugin
   types however correct it is. The AU steps gate on that **install** (which itself gates on
   `package`), the VST3 steps on `package` directly, so neither format can hide the other's result.
   A final step removes the installed copy again, so reproducing the sequence by hand does not leave
   a plug-in behind in a real `~/Library`.
   **Windows** additionally runs with `--skip-gui-tests`:
   the GPU-less/headless `windows-latest` runner cannot host the editor GUI tests (environmental, not a
   plugin defect — the editor validates on Linux + macOS; see KI-007). This skips one *test category*
   on one runner, distinct from the mode-level "never skip" rule above; all non-GUI tests still block.
7. **Stage + upload artifacts** (`actions/upload-artifact@v7`) — per platform: the public
   `Anamorph-<OS>` loose-file artifact (also the source `release.yml` archives the release
   zip from) and a separate `Anamorph-<OS>-debug` artifact (crash-symbolication
   material; never mixed into the public one). All staging is strict: no `|| true`,
   `if-no-files-found: error`.
   **Customer uploads are fail-closed**: each requires the DSP self-tests AND its own
   strip/staging/packaging step to have succeeded, and the staging steps self-validate (no symbol
   table, no debug files in the public copy). A pluginval-only failure still uploads beta artifacts;
   developer `-debug` artifacts survive packaging failures.
   On macOS "the DSP self-tests" is **both** self-test steps: the native arm64 run and the x86_64
   run under Rosetta each gate the `Anamorph-macOS` artifact and the `.pkg`, because the product
   ships as one universal binary and the arm64 run alone is half its behavioural gate. The
   `-debug` (dSYM) upload is unaffected, per the developer-artifact rule above.
   Two gating details are **step outputs rather than step outcomes**, deliberately. The Windows
   staging step emits `public_ok=true` at the moment the public copy is assembled and purged —
   before the developer-side PDB work that can abort — and the customer zip and the installer gate on
   *that*, not on the whole step: a purely developer-side symbol problem previously withheld the
   Windows beta artifact and its installer, while macOS treated the same class of failure as
   best-effort. The two platforms had opposite policies for one situation; this is the macOS policy,
   applied to Windows. Symmetrically, the Linux strip step and the Windows staging step each write
   `debug_artifacts=true` **last**, and the `-debug` uploads gate on that instead of on "the step was
   not skipped": both create their debug directory at the top of the step, so an abort part-way
   through would otherwise fire the upload against a directory that exists and may be empty, failing
   a *second* time on `if-no-files-found` and burying the real error under a cascade.
8. **Installers (v0.9.0)** — the Linux zip itself carries `install.sh`/`uninstall.sh`
   (per-user install by default, system-wide on request, since 0.9.3; `release.yml`
   restores and then fail-closed-verifies their executable bits when it archives the
   release zip). After the Windows/macOS staging steps, a separate packaging step builds the
   user-installable installer from the same validated staging dir (Windows Inno Setup
   installer — component selection + dual-path destination page — via the preinstalled
   `ISCC.exe`; macOS `.pkg` with component selection via `packaging/macos/build-pkg.sh`,
   self-checked for components + customize attributes). Uploaded as
   `Anamorph-<OS>-installer` artifacts under the same fail-closed gate as the customer
   zips (tests + own step outcome). See `PACKAGING.md` §Installers.

Evidence [Verified]: `.github/workflows/build.yml`; `scripts/run-pluginval.sh`; `scripts/run-pluginval.ps1`.

## Validation is uniform and blocking on every platform

Each of Linux, Windows and macOS runs the SAME gate — pluginval at
`ANAMORPH_PLUGINVAL_STRICTNESS`, deterministic ×3 **and** randomise ×3 — and **all are blocking**.
macOS runs it **twice over**, once per format (VST3 and AU). Linux runs headless under `xvfb`. The
`--randomise` mode randomises test order to surface order-dependent defects a fixed-seed run can
miss; the fixed seed (nonzero — see step 6) seeds the RNG the tests themselves draw from, so the two
flags are independent rather than two spellings of the same thing.
Evidence [Verified]: `.github/workflows/build.yml`.

### The Clang warning baseline

`linux-clang` asserts **no new** first-party warnings, not **zero**. The tree already carries 14
distinct first-party Clang warning sites, every one of them older than the job:

| Count | Flag | Path |
|---|---|---|
| 2 | `-Wfloat-equal` | `src/dsp/VelvetNoise.cpp` |
| 4 | `-Wmissing-prototypes` | `tests/state_tests.cpp` |
| 1 | `-Wshadow` | `src/PluginProcessor.cpp` |
| 1 | `-Wshadow-field` | `src/PluginEditor.h` |
| 2 | `-Wsign-conversion` | `src/dsp/ScopeBuffer.h` |
| 3 | `-Wswitch-enum` | `src/dsp/AnamorphEngine.cpp` |
| 1 | `-Wunused-but-set-variable` | `tests/dsp_tests.cpp` |

Clearing them means renaming a member across the editor, adding cases to engine switches and
changing float comparisons in DSP code — source work that belongs in its own review under
`DSP_POLICY.md`, not in a CI change. The alternatives were both worse: a job that lands **red**
teaches everyone to ignore it, and a job that **cannot fail** is not a gate. So the accepted set is
pinned in `scripts/clang-warning-baseline.txt` and anything above it fails.

**The baseline is a debt list, not a permission list.** It is keyed on `(path, flag)` with a
distinct-site count — deliberately **not** on line numbers, which drift on every unrelated edit above
a warning; a baseline that failed on changes introducing nothing would get regenerated blindly, which
accepts whatever else appeared alongside. The count is what stops a file that already has one
`-Wsign-conversion` from absorbing a second for free. A count that **falls** is a `::notice::` asking
for the file to shrink, never a failure — the commit that fixes a warning must not be the commit that
goes red. Regenerate with `--write-baseline` and **read the diff**; that diff is the entire review.

**The baseline describes one compiler, and the compiler is pinned.** Which diagnostics Clang emits is
a property of the major version — `-Wshadow-field`, `-Wsign-conversion` and
`-Wunused-but-set-variable` have all moved between majors — so counts taken from one say nothing
about another. Left unpinned, the reference point was whatever `ubuntu-latest` resolved `clang` to
that week, and a runner-image bump would have failed the gate on a push that changed nothing in the
tree: the same defect the "never key on line numbers" rule exists to avoid, one level up.
`ANAMORPH_CLANG_VERSION` in `build.yml` is the single authority for the major; `linux-clang` installs
`clang-<n>`/`lld-<n>` and configures with `clang-<n>`/`clang++-<n>`; `sanitizers` uses the same major,
which also lets it name `libclang-rt-<n>-dev` directly instead of scraping `clang --version` for it.
The baseline records the major in a `# clang-major:` line and `--clang-major` **refuses to run** when
the two disagree — exit 2, the code meaning *the check* could not run, not the 1 meaning the tree
regressed. An unrecorded version is refused for the same reason a wrong one is: it cannot be
confirmed to describe this compiler. Bump the pin and re-baseline in the **same** change.

**Only judge a baseline against a FULL build.** A count also falls when the log simply lacks the
translation unit that carries the warning, which is what an incremental rebuild produces — ninja
recompiles only what changed. CI always builds from a fresh checkout, so its log is complete; a local
`cmake --build` after a one-file edit is not, and shrinking the baseline from one of those deletes
entries for warnings that are still in the tree. The notice says so; `rm -rf build-clang` first if
you intend to act on it.

### Evidence anchors

`source-lint` runs `scripts/check-citations.py --check --base <rev>` over every `file.cpp:NNN`
citation in `docs/` and the root Markdown whose path is listed in the script's `TRACKED` tuple —
every root-spelled source path the documents currently cite, 184 anchors at the time it landed.

**`--self-test` runs first, and this is the checker that most needs one.** The other three lints
report; this one also **rewrites** governed documents under `--fix`, so a defect does not merely miss
drift — it replaces a correct anchor with a wrong one and prints success. It has done that four
times (a `rev:`-qualified anchor reaching the ownership test, a compound citation left internally
contradictory, one span applied twice, a provenance sentence wrapping the sibling product's range),
and each is now a case in the self-test, in the direction it failed. The cases drive the real
ownership test, the real citation pattern, the real diff line-map and the real span rewriter over
synthetic input, so the run needs no base revision and cannot be satisfied by a clean tree.

**What it compares against.** On `push` (the normal path) the base is `github.event.before`, the
branch's previous tip: **one push of drift at a time**, which is sufficient only because every push
is checked — hence no `needs:` on the job. On the first push of a branch (`before` is all-zeros) and
after a force-push (`before` may no longer exist) it falls back to `HEAD~1` with a `::notice::`,
under-checking rather than failing on a question it cannot answer. On a **fork** pull request it uses
`base.sha` through `git merge-base`.

**What it can and cannot do.** It detects that a citation no longer points at the same *text* it
pointed at in the base — the whole "an edit above shifted it" class. It cannot tell you an anchor was
aimed at the wrong code to begin with; this repository's existing anchors are therefore *adopted*,
not audited. A clean run means none of them **moved**.

**When you re-anchor deliberately** — moving an anchor onto the code it should always have named —
the tool cannot distinguish that from drift, and the gate goes red on the commit that *fixed* it.
Declare the pair in `DELIBERATE_REAIMS` in the **same change set** as the re-anchor, never in a
follow-up. The list starts empty and is expected to return to empty: an entry stops matching once the
base carries the corrected spelling, and the next run reports it as removable. `--fix` re-anchors
mechanically; anchors it reports as `UNMAPPABLE` (the cited lines were themselves edited) need a
human. Prose *examples* of a citation must use a path outside `TRACKED`, or they get re-anchored too.

## Artifacts

| Artifact | Contents | `if-no-files-found` |
|---|---|---|
| `Anamorph-Linux` | loose staged files (extract the artifact zip once → payload directly): stripped `Anamorph.vst3` + `Anamorph` (Standalone) + `install.sh`/`uninstall.sh` + `INSTALL.txt` | error |
| `Anamorph-Linux-debug` | `Anamorph.vst3.so.debug`, `Anamorph.standalone.debug` (split debug info) | error |
| `Anamorph-Windows` | loose staged files: `Anamorph.vst3` + `Anamorph.exe` (Standalone; PDBs removed) + `INSTALL.txt` | error |
| `Anamorph-Windows-installer` | `Anamorph-<version>-Windows-Installer.exe` (Inno Setup) | error |
| `Anamorph-Windows-debug` | `Anamorph.vst3.pdb`, `Anamorph.standalone.pdb` | error |
| `Anamorph-macOS` | loose staged files: universal stripped `Anamorph.vst3` + `.component` (AU) + `.app` + `INSTALL.txt` | error |
| `Anamorph-macOS-installer` | `Anamorph-<version>-macOS.pkg` (VST3 + AU + app components) | error |
| `Anamorph-macOS-debug` | `Anamorph.vst3.dSYM`, `Anamorph.component.dSYM`, `Anamorph.app.dSYM` — **best-effort**: the upload step is skipped (with a CI warning) when Release+LTO yields no usable dSYM, so this artifact can be absent | error (when it runs) |

The `Anamorph-<OS>` artifacts hold **loose files** so a downloaded artifact extracts
straight to the payload (no nested archive); the artifact transport drops Unix executable
bits on that route (`INSTALL.txt` documents the fallbacks). `release.yml` archives the
**same** trees into the published release zips, restoring those bits first and failing
closed if any expected executable is missing one — so release downloads always extract
runnable. Attribution/support files are **not** inside the packages; they ship as
release-page assets (`PACKAGING.md` §Third-party attribution).

The macOS job captures dSYMs, strips, then ad-hoc codesigns the bundles, **asserts** both arch
slices are present (not merely printing `lipo -archs` — see §Build matrix), and asserts the stripped
VST3 still exports `GetPluginFactory` — all strict (a failure fails the job; the `\|\| true`
swallowing was removed in RH-PR-2/ADR-0021).
Evidence [Verified]: `.github/workflows/build.yml`.

## Known coverage limits

Stated here rather than left to be rediscovered. None is a defect; each is a decision.

- **Every platform now validates the bytes it ships**, but by three different routes, so it is worth
  being precise about each. **Linux** strips *before* pluginval, so the gate sees the stripped `.so`.
  **macOS** validates *after* the packaging step, against `dist/Anamorph-macOS/` — the stripped,
  ad-hoc-signed tree the artifact is uploaded from — for the VST3 and the AU alike; this was not
  true until the gate was moved, and until then every macOS pass described a bundle that `strip -x`
  and `codesign --force --deep` rewrote immediately afterwards. **Windows** validates the build-tree
  bundle, and that *is* the shipped image: the staging step copies the bundle and deletes debug
  *files* from the copy, but nothing rewrites the `.vst3` module itself, so the validated and shipped
  bytes are the same. The residual asymmetry is only in what each staging self-check can assert —
  see the Windows bullet below.
- **No native Intel macOS runner.** The `macos` job builds universal on Apple Silicon and now
  executes the x86_64 slice under **Rosetta 2**, which translates x86_64 to arm64 and runs on arm64
  hardware — FPCR rather than MXCSR, NEON rather than SSE. That closes the "run by nothing" gap but
  not the "run by an Intel CPU" one. A dedicated `macos-*-intel` job is the only thing that would,
  at the cost of a fourth macOS runner per push; the cheap 95% is taken and the expensive 5% is not.
- **The Windows staging self-check is a delete-confirmation, not a property check.** It re-lists the
  debug extensions the step just deleted, so it can only fire if `Remove-Item` silently failed.
  Linux inspects ELF section headers and asserts the export; macOS asserts both slices. Windows
  asserts nothing about the shipped `.vst3` being loadable. The honest way to close it is to read
  the PE export table — the staging step already parses PE headers for the CodeView record. Do **not**
  substitute a raw byte-string search for `GetPluginFactory`: matching the name anywhere in the file
  is not evidence that it is exported.
- **`--skip-gui-tests` on Windows** skips one test category on one runner (KI-007, environmental).
- **The `linux-clang` job is not `-Werror`** — see §Build matrix for why it cannot be yet, and
  [The Clang warning baseline](#the-clang-warning-baseline) for the 14 sites it currently accepts.
- **The AU is validated by pluginval rather than `auval`.** The gate hosts the `.component` through
  JUCE's `AudioUnitPluginFormat` — the same resolution path a JUCE-hosted DAW takes, and the same
  test set the other two platforms are held to. Apple's `auval` (`auval -v aufx Anmr RTec`) is its
  own conformance tool and tests things pluginval does not; adding it is a further step, not a
  substitute for this one. (The bundle it validates *is* the shipped one — see the first bullet.)
- **`ANAMORPH_TESTS_NO_FTZ=1` on the valgrind step** relaxes the denormal half of one DSP assertion,
  because valgrind emulates floating point and does not honour the CPU's FTZ/DAZ bits. NaN and Inf
  stay gated there, and every native job asserts the full check. See §Build matrix.
- **No gate ever installs anything** — CI inspects the packages but never runs an installation
  (`TESTING.md` §Gaps in the automated coverage).

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

The lint jobs need no toolchain and no build, so run them first — they are the ones that cost
seconds and catch the most:

```bash
python3 scripts/check-docs.py --self-test && python3 scripts/check-docs.py
python3 scripts/check-portability.py --self-test && python3 scripts/check-portability.py
python3 scripts/check-citations.py --self-test
python3 scripts/check-citations.py --check --base origin/main   # --fix re-anchors
python3 scripts/check-clang-warnings.py --self-test
```

`check-citations.py` compares against **a** base, and which one matters: CI uses the previous push,
so a local run against `origin/main` can reach a different verdict (a differing citation *count* for
a document makes the tool fall back to ordinal pairing, which only judges base spellings still
present verbatim). Check **both** before concluding the gate is green.

Then the build and the release gate:

```bash
scripts/setup-linux.sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_NUMBER=0
cmake --build build --config Release
scripts/run-tests.sh
scripts/run-pluginval.sh 10 deterministic     # 10 = ANAMORPH_PLUGINVAL_STRICTNESS in build.yml
scripts/run-pluginval.sh 10 randomise         # --randomise x3 (the state-restoration gate)
```

`scripts/run-pluginval.sh` takes an optional third argument, the format: `vst3` (default) or `au`.
`au` is macOS-only and **errors** on any other host rather than skipping silently; on macOS install
the `.component` first (or point `ANAMORPH_PLUGINVAL_BUNDLE` at an installed one), because the
AudioComponent registry only finds bundles under a Components directory.

The `linux-clang` and `sanitizers` jobs use their own build trees so they never collide with the one
above — `build-clang`, `build-san`, `build-vg`. All are covered by `.gitignore`'s `build*/`.

```bash
CLANG=18   # ANAMORPH_CLANG_VERSION in .github/workflows/build.yml is the authority
cmake -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="clang-$CLANG" -DCMAKE_CXX_COMPILER="clang++-$CLANG" \
      -DANAMORPH_BUILD_STANDALONE=OFF
cmake --build build-clang --target AnamorphTests AnamorphStateTests Anamorph_VST3 2>&1 | tee clang-build.log
python3 scripts/check-clang-warnings.py --log clang-build.log --root "$PWD" \
        --build-dir "$PWD/build-clang" --clang-major "$CLANG"
python3 scripts/check-portability.py --compile-canary build-clang/_deps/juce-src/modules \
        --cxx "clang++-$CLANG"
```

Use the **pinned** major, not your distribution's default `clang`: the baseline records which
compiler it describes and the checker refuses to compare against a different one, so an unpinned
local run reports `exit 2` rather than a misleading pass or fail.

See `TESTING.md` for the validation gate and `PACKAGING.md` for the macOS signing/quarantine steps.
