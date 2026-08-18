# DEPENDENCY_POLICY.md

Repository Governance Policy. Third-party dependency locking and upgrade safety.

## Current dependencies

| Dependency | Pin | Mechanism | Evidence |
|---|---|---|---|
| **JUCE** | **9.0.1**, pinned by **immutable commit SHA** `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` | CMake `FetchContent` (`GIT_SHALLOW`), overridable via `-DANAMORPH_JUCE_PATH` | CMakeLists.txt:48-50, 59-67 |
| **pluginval** | latest release (download) | `scripts/run-pluginval.sh` | scripts/run-pluginval.sh:119-126 |
| **C++ standard** | C++23 | `CMAKE_CXX_STANDARD 23`, extensions off (ADR-0027) | CMakeLists.txt:16-18 |
| **Clang** (the Linux warning-gate + sanitizer jobs; **ships nothing**) | **major pinned — 22**, upstream stable (ADR-0028) | `ANAMORPH_CLANG_VERSION`, the single authority, consumed by both jobs' installs, their ccache lineages and `--clang-major`. Installed from **apt.llvm.org** by `scripts/setup-llvm-apt.sh` (Ubuntu's archives stop at 20 for noble), fail-closed. `scripts/clang-warning-baseline.txt` records the same major and the gate **refuses to run** on a mismatch | .github/workflows/build.yml:107-109 |
| **GCC** (the LTO + GCC-warning-gate job; **ships nothing from that job**, though the same major builds the shipped Linux artifact) | **major pinned — 13**, which is simply what `ubuntu-24.04` ships | `ANAMORPH_GCC_VERSION`, the single authority, consumed by that job's `apt` install, its ccache lineage and `--gcc-major`. Nothing to add to `apt` sources: the pin is *below* the image rather than ahead of it, which is why the **image** is pinned too (`runs-on: ubuntu-24.04`) — naming the compiler without naming the image only moves the unpinned variable one level up. `scripts/gcc-warning-baseline.txt` records the same major and the gate **refuses to run** on a mismatch | .github/workflows/build.yml:110 |
| Linux system libs | distro packages | `scripts/setup-linux.sh` (ALSA, JACK, X11, FreeType, GTK/WebKit, mesa, **EGL — required by JUCE 9's Linux GL context path**, xvfb) | setup-linux.sh |
| **GitHub Actions** | **every ref pinned to a commit SHA**, with its version in a trailing comment | **Dependabot**, weekly, two semver-split groups (it reads and rewrites that comment) | .github/dependabot.yml |
| Runner images | floating `*-latest`, plus two deliberate pins (`macos-15-intel`, for native Intel; `ubuntu-24.04` on `linux-lto-tests`, so the GCC warning baseline's reference compiler cannot move underneath it) | GitHub's own image rollout | build.yml `runs-on:` |

## Version-lock reasoning

- **JUCE is pinned to an exact IMMUTABLE commit** (`e18f7f5…` = tag 9.0.1; `ANAMORPH_JUCE_VERSION`
  carries the human-readable version), not a branch, `latest`, or a mutable tag *name* — since the
  v0.8.13 cycle the SHA pin also protects against an upstream re-pointed tag (ADR-0022). JUCE is
  the framework for the entire DSP (oversampling, Linkwitz-Riley filters, `dsp::AudioBlock`),
  parameter system (APVTS), GUI, and plugin-format wrappers — an unpinned bump can silently change
  DSP behaviour, latency, the editor/X11 embedding path (the 0.8.5 incident lives in JUCE's X11
  host code), and the parameter/state ABI. The pin makes builds reproducible and keeps the audited
  behaviour stable. Evidence [Verified]: CMakeLists.txt:48-50, 59-67; the X11 dependency is
  documented in ADR-0011.

## Update mechanisms

One row per externally maintained thing, and **why** that mechanism rather than a bot. Automation is
not the default here and neither is hand-maintenance; the question asked of each is whether an
automated PR would carry information a human could act on.

| Category | Mechanism | Why |
|---|---|---|
| GitHub Actions refs | **Dependabot**, weekly, two groups split by semver impact | The only Dependabot ecosystem this repository has. Every ref is a **commit SHA** with its version in a trailing comment (2026-08-18): a bare `@vN` is a mutable tag, and an action runs on the runner *with the job's credentials* — a privilege JUCE, which this project already pins by immutable SHA for the weaker reason, never has. The cost is accepted deliberately: a SHA pin is rewritten on every release rather than only on a major, so more dependencies move, which is what the update-type grouping absorbs. Grouping also keeps a multi-ref family (`codeql-action/{init,analyze,upload-sarif}`) moving together; splitting minor/patch from major is what stops one major blocking every safe bump. `microsoft/msvc-code-analysis-action` is excluded — see `.github/dependabot.yml` for the reason and `.github/workflows/msvc.yml` for the pin. |
| **JUCE** | manual, ADR + Architecture Review (rules 1–5 below) | An unpinned bump can silently move DSP behaviour, latency, the editor/X11 path and the state ABI. The evidence a bump needs is a twin dump and a Level-5 audition, which no bot can produce. CMake is not a Dependabot ecosystem in any case, and Renovate has **no CMake manager at all** (its only C/C++ manager is Conan), so "automate it" is not an available option, only a hand-written regex would be. |
| **Clang major** | manual, ADR-gated, and deliberately so — **re-tested after the move to apt.llvm.org, not inherited** | Still not expressible to Dependabot: it is a workflow `env:` value, and Dependabot parses no `env:` key in any ecosystem, nor any apt/deb one. Renovate *does* have a `deb` datasource and could point at an apt.llvm.org suite — so the honest reason is not "no tool can see it" but that **the PR could not be green**: `check-clang-warnings.py` exits 2 whenever the pin and the baseline's `# clang-major:` disagree, and reaching green means building with the new compiler and regenerating the site counts, which *is* the review. Every firing would also burn the full pluginval gate. The guard is the mechanism: it makes a stale pin a loud, specific failure instead of a silent comparison against another compiler's diagnostics. The human trigger is one page — `apt.llvm.org/llvm.sh`'s `CURRENT_LLVM_STABLE` — checked when upstream cuts a stable major, and a release candidate does not qualify. |
| Runner images (`*-latest`) | GitHub's rollout, watched by hand | Nothing to update: the labels already float, and that is intended for every job except `macos-15-intel`. Dependabot does not touch `runs-on:`. Renovate extracts runner labels but then **skips** any that are not a numeric version, so `ubuntu-latest` is read and discarded; pinning the labels to get bot coverage would trade a real property (tracking GitHub's supported image) for a PR. |
| apt / brew packages | unpinned, installed per job | No ecosystem covers either (`apt`, `deb` and `homebrew` are absent from Dependabot's 33). They are build-environment tools, not linked dependencies: `ninja`/`cmake` do not enter the artefact, and `ccache` is explicitly optional — the jobs fall back to no launcher with a `::warning::` rather than failing. The Clang packages are the exception and are pinned by major above. |
| Hand-run validation tools (`actionlint`; the SchemaStore `dependabot-2.0.json` schema) | unpinned, fetched when used | Neither runs in CI — they are local checks the contributor workflow leans on, so a version drift shows up as a different answer in front of a human, not as a silent gate change. Both are named here because this change made them load-bearing: `actionlint` 1.7.7 has a known false positive on this repository (`macos-15-intel` is absent from its built-in label list), and the SchemaStore schema is the **only** machine-readable Dependabot schema in existence — GitHub publishes none — so it is the authority a `dependabot.yml` edit is checked against. Pin either one only if it starts gating in CI. |
| **pluginval** | `releases/latest`, unpinned — a **known** gap | Tracked as `RELEASE_HARDENING_PLAN.md` **RH-F6**: the release gate's own tool can change under the project, and there is no checksum. Recorded here rather than fixed here; pinning it is a change to this policy's table, which is what RH-F6 says. No bot could cover it as it stands — there is no manifest to read, and a version would have to be pinned in a file before any tool could track it. |

**Renovate was evaluated and not adopted.** It is the only tool that could reach the two categories
Dependabot cannot (the Clang env value via `custom.regex`, runner labels via `github-runners`), and on
inspection it delivers neither: the Clang PR could not be green, and the runner-label manager discards
`*-latest`. It has no CMake manager, so JUCE stays hand-written either way. Against that, adopting it
means a second bot's app install and config to maintain for one repository. The conclusion is about
capability, not preference — re-open it if Renovate gains a CMake/FetchContent manager or if this
repository ever grows a real package manifest.

## Upgrade rules

1. A JUCE version bump is a **Build System change** → `ARCHITECTURE_REVIEW_GATE.md` + an ADR.
2. After any bump: full DSP self-tests + pluginval at the configured strictness
   (`ANAMORPH_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml`) in **both modes**
   (deterministic and `--randomise` ×3) on all three OSes, **and** a manual audition (Level 5) — a
   JUCE change can move DSP/latency/editor behaviour invisibly to the headless gate.
3. Re-verify the `RELEASE_COMPATIBILITY_CHECKLIST.md` (latency reporting, session reload) after a bump.
4. Prefer the offline path (`-DANAMORPH_JUCE_PATH`) for reproducibility in restricted CI.
5. `JUCE_*` compile flags in `CMakeLists.txt:291-296` (no webview, no curl, no splash, strict
   ref-counted pointer) are part of the dependency contract; changing them is a build change.

## Compliance log

- **Clang major 18 → 22** (via 20 in the same unmerged change set) — recorded in **ADR-0028**
  (`Accepted` 2026-08-17; the versions-considered evaluation, the install mechanism and the revisit
  trigger live there, as does the correction of the first draft's claim that 21/22 had "no noble
  publication"). Scope first, because it decides which rules apply: the pin is used by `linux-clang`
  and `sanitizers` only, **neither of which uploads an artifact** — shipped Linux bytes are GCC's,
  Windows' MSVC's, macOS's AppleClang's — so no shipped binary, reported latency or serialized state
  is touched, and rules 2–3 (twin dump, Level-5 audition, compatibility re-verification) have nothing
  to act on. Rule 1 **does** apply, and the ambiguity that made that a live question is now closed:
  ADR-0028 amends `ARCHITECTURE_REVIEW_GATE.md` with the *who chooses the version* rule, under which a
  repository-pinned compiler is gated and a runner-supplied one cannot be. **Why 22:** it is
  upstream **stable** (22.1.8, 2026-07-10; 23.1.0 is still rc3), which apt.llvm.org itself asserts via
  `CURRENT_LLVM_STABLE=22`. Ubuntu's archives stop at 20 for noble, so the toolchain comes from
  apt.llvm.org through `scripts/setup-llvm-apt.sh` — a packaging boundary in Ubuntu is not allowed to
  decide how current this project's detectors are. **One behavioural change, and it preserves coverage
  rather than adding it:** Clang 21 dropped `vptr` from `-fsanitize=undefined`, so the `sanitizers` job
  now names `-fsanitize=address,undefined,vptr`; without it the move past 20 would have silently
  stopped checking bad downcasts and bad vtables. Verification (all measured, one tree, JUCE at the
  pinned commit, clang-20 as the control): a **`diff`-identical 52-instance warning census** 20 vs 22
  — and identical to 18 as well — so the only line that changed in
  `scripts/clang-warning-baseline.txt` is `# clang-major:`; 140-check DSP + 894-check state suites green
  under the clang-22 build **and** under its ASan+UBSan+vptr build with `libclang-rt-22-dev`; the LTO
  `Anamorph_VST3` link and `check_linker_flag`'s lld probe both green at 22;
  `--compile-canary` still rejects the explicit `SIMDRegister` form; the gate demonstrably **refuses**
  the mismatched pair (exit 2); and the `vptr` loss reproduced directly by running one bad-downcast
  program under both majors. Costs, so they are not discovered later: a third-party apt source with an
  HTTPS-fetched signing key scoped by `signed-by=` to one suite, a **fail-closed** install (the two
  Clang jobs fail if apt.llvm.org is unreachable; the three shipping jobs do not use it), 15 packages /
  155 MB / 17.8 s per Clang job, one fresh ccache lineage each, and reproducibility that is now
  major-granular against a *snapshot*-versioned package rather than a release-versioned one. Synced:
  `ARCHITECTURE_REVIEW_GATE.md` (the new rule), `CI_CD.md` (§Cache lineages, §The Clang warning
  baseline, §Reproducing CI locally), `REPOSITORY_MAP.md`, this table and this log. No `src/`,
  `tests/`, `CMakeLists.txt` or packaging change; no `CHANGELOG.md` entry (rule 3 — not user-visible).
- **C++ standard 17 → 23** — recorded in **ADR-0027** (v0.9.4 cycle, applied on top of the
  JUCE 9.0.1 tree with **no version bump**). Rule 1 (Build System change → gate + ADR) applied to
  the `CMAKE_CXX_STANDARD` line as a pinned dependency of this table. One C++ source change was
  required and is the whole of it: `#include <algorithm>` in `src/dsp/HaasProcessor.cpp`, because
  libc++ stops including `<algorithm>` transitively at `_LIBCPP_STD_VER >= 20` — found by the
  **macOS CI build failing**, not by inspection. Verification: builds green on all three
  platforms (GCC 13, AppleClang 15.4, MSVC `/std:c++latest`) plus a local Clang 18 + **libc++**
  build as the macOS proxy; DSP suite (140 checks) + state suite (894 checks, incl. the
  8.0.14-frozen parameter-registry snapshot passing unchanged) green under both standard
  libraries; engine output proven **bit-identical** C++17 vs C++23 by the 32-scenario twin dump
  incl. reported latencies (`worklogs/CXX23_MIGRATION_v0.9.4.md`); 27 project translation-unit
  **compilations** — both self-test targets' command sets — produce a **byte-identical**
  29-instance warning set at both standards (the JUCE entry below cites 18/19 because it measured
  the `AnamorphStateTests` set alone; that narrower set still yields 18/19 at C++23, so the two
  are the same measurement at different scopes); pluginval strictness 10
  green in both modes ×3 locally and on the three CI gates. `CMAKE_CXX_EXTENSIONS OFF` and the
  CMake ≥ 3.22 floor are unchanged; no JUCE, packaging or system-library change. Rule 2's Level-5
  audition does **not** apply — it is a JUCE-bump rule, and here no framework code moved under
  the editor. Open caveat carried in ADR-0027 §Consequences: MSVC has no stable `/std:c++23`, so
  CMake requests `/std:c++latest` on Windows.
- **JUCE 9.0.0 → 9.0.1** — recorded in **ADR-0026** (v0.9.4 cycle). Zero C++ source changes and
  **no build-dependency change**: neither 9.0.1 breaking change has project exposure (the vendored
  zlib/jpeg/png/flac C-language switch was already in force at 9.0.0 and Anamorph links no
  external copy; the relocated WebBrowserComponent package is unreachable with
  `JUCE_WEB_BROWSER=0`), and no module Anamorph uses altered its declared
  `linuxPackages`/`OSXFrameworks`/`windowsLibs` — only the `version:` field moved. Rule-2
  verification: DSP suite (140 checks) + state suite (894 checks, incl. the 8.0.14-frozen
  parameter-registry snapshot passing unchanged) green under 9.0.1; engine output proven
  **bit-identical** 9.0.0 vs 9.0.1 by the 32-scenario twin dump incl. reported latencies
  (`worklogs/JUCE901_UPGRADE_v0.9.4.md`); pluginval strictness 10 green locally in both modes ×3
  plus the CI gates; the 18 project translation units produce a byte-identical warning set
  against both trees. Rule-3 re-verification: latency reporting (twin-dump latencies) and session
  reload (state suite) unchanged. Rule 5: the `JUCE_*` compile flags are untouched. The
  `THIRD_PARTY_LICENSES.md` re-verification required by `RELEASE_POLICY.md` was performed —
  JUCE's `LICENSE.md` and all twelve cited licence files are byte-identical between the tags.
  The **manual audition (rule 2, Level 5) was performed** by the maintainer against this build
  (2026-08-15) and **ADR-0026 is `Accepted`**; the same audition discharges the one ADR-0022 left
  open for the 9.0 line, which is now `Accepted` too.
- **JUCE 8.0.14 → 9.0.0 + immutable-commit pinning** — recorded in **ADR-0022** (v0.8.13 cycle).
  Zero C++ source changes (no project exposure to the 9.0.0 breaking surface); Linux gains
  `libegl-dev` (JUCE 9 GL-context path uses EGL). Rule-2 verification: DSP suite (140 checks) +
  state suite (774 checks, incl. the 8.0.14-frozen parameter-registry snapshot passing
  unchanged) green under 9.0.0, **and** engine output proven **bit-identical** 8.0.14 vs 9.0.0
  by a 32-scenario twin dump incl. reported latencies (`worklogs/JUCE9_MIGRATION_v0.8.13.md`);
  pluginval both modes ×3 runs on the CI gates. The **manual audition (rule 2, Level 5) was
  performed** (2026-08-15) and **ADR-0022 is `Accepted`**; it was carried out against the
  **9.0.1** build that succeeded this pin — engine output is bit-identical across
  8.0.14 → 9.0.0 → 9.0.1, so the later build's audition covers this one's editor surface too.
- **JUCE 8.0.8 → 8.0.14** — recorded in **ADR-0012** (the first dependency bump enforced under rule 1
  above; the bootstrap use of this rule). Verified green by CI (build + the then-current 23 DSP self-tests + pluginval
  strictness 10 on the Linux gate); commit `41acaa7`. The manual audition (rule 2, Level 5) **was
  performed** post-CI by the maintainer — a DAW audition of 8.0.14 against the 8.0.8 baseline with no
  perceptual regressions (2026-06-29) — and is recorded in **ADR-0012** (*Manual Audition (Level 5)*).
  It is a human sign-off, not headlessly reproducible. The forward-looking risk for *future* bumps
  stays tracked by RISK-001.
