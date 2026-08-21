# REPOSITORY_MAP.md

Directory and file map with per-component responsibilities. Architecture rationale is in
`docs/architecture/ARCHITECTURE.md`.

## Top level

```
Anamorph/
├── CMakeLists.txt          Build: JUCE FetchContent (9.0.1, pinned by commit SHA), AnamorphDSP INTERFACE lib,
│                           AnamorphHardening flags (ADR-0021), plugin target
│                           (VST3 [+AU on macOS] [+Standalone]), tests app.
├── README.md               Project façade (features, status, quick start, docs nav).
├── CHANGELOG.md            Version history (Keep a Changelog; evidence-cited).
├── CLAUDE.md               AI/contributor entry point: mandatory policy pre-read + repo constraints.
├── NOTICE                  Third-party attribution that must accompany a binary distribution
│                           (published as a version-named asset on every GitHub release — the
│                           sole carrier of the mandatory IJG acknowledgement).
├── THIRD_PARTY_LICENSES.md Verified inventory of every third-party component: purpose, origin,
│                           licence, obligations, what is compiled in vs only vendored, and the
│                           open licensing decisions. Re-verify after any JUCE bump.
├── EULA.md                 Anamorph's own end-user terms — an UNAPPROVED DRAFT, not in force and
│                           not shipped by any installer; every open owner/legal decision marked.
├── PRIVACY.md              What Anamorph collects (nothing), sends (nothing) and writes to disk;
│                           every claim cited to source. Legal class, derived from the code.
├── TRADEMARKS.md           Product/company name status, third-party marks used descriptively, and
│                           the naming obligations the dependency licences impose.
├── SUPPORT.md              Internal-testing guide: what a tester may do with a build, what to
│                           check first, and what a test report must contain.
├── src/                    Source (wrapper + GUI + DSP core).
├── tests/                  Headless self-tests (DSP + state compatibility), fixtures, and the
│                        opt-in benchmark / fuzz / compile-only realtime targets.
├── worklogs/               Session-local investigation records for future agents (NOT
│                           architecture docs; e.g. performance/WAVE3_INVESTIGATION.md,
│                           release-hardening/RH_PR2_INVESTIGATION.md — finalized decisions
│                           graduate to ADRs; worklogs are the raw evidence trail).
├── scripts/                setup (incl. the pinned-Clang apt source) / build / test / pluginval,
│                           plus the seven CI lints
│                           (check-docs, check-portability, check-realtime,
│                           check-citations, check-clang-warnings,
│                           check-gcc-warnings, check-linux-abi — each with its
│                           own self-test).
├── packaging/              Per-platform install notes + installer assets (linux/, windows/, macos/).
├── .github/                CI + security tooling: workflows/ (build + validate on 3 OSes with
│                           retain-then-strip symbol pipeline; doc/source lints; Clang AND GCC
│                           warning gates; ASan/UBSan + valgrind; RTSan; state fuzzing; CodeQL;
│                           MSVC /analyze; Dependency Review), actions/ (the shared Linux setup)
│                           and dependabot.yml (github-actions ecosystem only; every `uses:` in
│                           this tree is pinned to a commit SHA).
└── docs/                   This documentation library.
```

## `src/` — wrapper + GUI

| File | Responsibility |
|---|---|
| `PluginProcessor.{h,cpp}` | VST3/Standalone wrapper: bus layouts, `processBlock`, state save/recall, PDC, custom Undo/Redo, A/B compare. |
| `PluginParameters.{h,cpp}` | APVTS layout (`createAnamorphLayout`), `pid::` IDs, atomic cache, `toEngine` → `EngineParameters`. |
| `InternalState.h` | Host-hidden session/view params (Oversampling, UI Scale, Persistence, Tooltips, Animations, Meters). |
| `PresetManager.{h,cpp}` | Factory + user `.anamorph` presets (sound params only). Factory presets carry an immutable internal id, user presets are identified by their file — the `Selection` that keeps a shared name from mis-ticking the menu, and that is carried in plug-in state so the indicator survives a reload (ADR-0024). |
| `PluginEditor.{h,cpp}` | Simple/Advanced UI, OpenGL context (macOS/Windows only), 24 Hz + VBlank timers. |

## `src/gui/` — GUI components

| File | Responsibility |
|---|---|
| `LookAndFeel.{h,cpp}` | Dark "digital" look; knob/slider drawing incl. reset-sweep easing. |
| `Vectorscope.{h,cpp}` | Diamond/Lissajous goniometer (reads `ScopeBuffer`). |
| `SpectrumImager.{h,cpp}` | Multiband spectral editor (FFT + drag-to-split bands); writes crossover/width/solo params. |
| `LevelMeter.{h,cpp}` | Per-channel L/R Peak + RMS meters. |
| `CorrelationMeter.{h,cpp}` | Phase-correlation + balance meters (GUI class `StereoMeter`). |
| `FrameClock.h` | Adaptive display-rate refresh driver shared by the four visualizers (VBlank-paced, ~125 Hz cap; 0.8.10). |

## `src/dsp/` — format-agnostic DSP core (`AnamorphDSP` INTERFACE lib)

| File | Responsibility |
|---|---|
| `EngineParameters.h` | POD snapshot driving the engine (the wrapper↔engine boundary). |
| `AnamorphEngine.{h,cpp}` | The serial DSP chain orchestrator; switch machine; crossfades; latency. |
| `MidSide.h` | MS matrix (1/√2) + `applyWidth`. |
| `HaasProcessor.{h,cpp}` | Precedence delay widening. |
| `VelvetNoise.{h,cpp}` | Velvet-noise decorrelation (mono→stereo). |
| `ChorusEngine.{h,cpp}` | Chorus + Dimension-D. |
| `MonoMaker.{h,cpp}` | LR4 low-freq mono (post-Mix). |
| `MultibandWidth.{h,cpp}` | 1–4 band per-band width + phase-matched A(dry). |
| `LR4Xover.h` | Flat-state Linkwitz–Riley crossover clone (Wave-2 H6; bit-identical to the `juce::dsp` original). |
| `SoloMonitor.{h,cpp}` | Post-everything Band-Solo audition band-pass. |
| `LoudnessMatch.{h,cpp}` | BS.1770 Level Match (Measure + absolute Predict). |
| `Correlation.h` | Phase-correlation estimator. |
| `LevelMeters.h` | L/R Peak+RMS metering with NaN self-heal. |
| `ScopeBuffer.h` | Lock-free SPSC scope ring. |
| `RealtimeAnnotations.h` | `ANAMORPH_NONBLOCKING` — the guarded spelling of Clang's `[[clang::nonblocking]]` **type** attribute (it goes after the parameter list; the prefix form is a hard error). Declares the `REALTIME_AUDIO_POLICY` contract on the audio entry point for the `realtime` job to enforce; `__has_cpp_attribute`-guarded, so it is inert on GCC/MSVC/AppleClang and changes no object code anywhere (ADR-0029). |

## `tests/`, `scripts/`, `packaging/`, `.github/`

| Path | Responsibility |
|---|---|
| `tests/dsp_tests.cpp` | 37 headless DSP acceptance tests + 1 A/B state-restoration clamp guard (`check(cond, "...")` harness; `main` runs all). |
| `tests/AllocationGuard.h` | The audio-path allocation guard (Test 38, ADR-0029): replaceable `operator new`/`delete` plus glibc malloc-family interposition, armed only around `process()`. The tier that reaches **MSVC**, where RTSan does not run. Covers the plain, nothrow **and C++17 over-aligned** `operator new`/`delete` forms (the over-aligned pair uses `posix_memalign`, not C11 `aligned_alloc`, which libc++ withholds below a macOS 10.15 deployment target — this header compiles on both macOS jobs). Self-checks its counters before reporting a zero; stands down entirely under **RTSan** (self-detected — its interposers would otherwise shadow RTSan's allocation interceptors and blind that lane) and for the valgrind build (`ANAMORPH_NO_ALLOC_GUARD`), and drops its malloc half under ASan — each disclosed with a `::warning::`. |
| `tests/realtime_canary.cpp` | Liveness proof for the `realtime` job: an annotated function commits a real, *escaping* heap allocation, so the job can assert the lane is able to fail. Compiled directly by the workflow step (no CMake target — a liveness probe should not be a Build System change). The escape sink matters: at `-O2` Clang deletes a non-escaping `malloc`/`free` pair before the sanitizer pass sees it. |
| `tests/realtime_effects.cpp` | Compile-time realtime proof for the **JUCE-free leaf layer** (`realtime` job, `-fsyntax-only -Werror=function-effects`, seconds, no CMake target): an `ANAMORPH_NONBLOCKING` driver calls `MidSide`, `LR4Xover`, `ScopeBuffer`, `CorrelationMeter` and `LevelMeters` exactly as the audio path does, so the compiler proves those bodies effect-clean *before* any test executes them. Measured 0 diagnostics over that layer against 52 over the engine TU (all transitive through JUCE) — which is why the flag is scoped here and nowhere else (ADR-0029 §3). |
| `tests/bench.cpp` | The `PERFORMANCE_BUDGET` §"required benchmark procedure" harness, behind `ANAMORPH_BUILD_BENCH` (OFF). Shipped-Release flags, the full SR × block × algorithm × oversampling × multiband matrix, median ns/sample + worst single block over ≥5 reps with the spread reported. **Enforces constraint C2**: exits 2 rather than print an unattributable number when it cannot identify the CPU and `ANAMORPH_BENCH_CPU` is unset. CI builds and smoke-runs it but does **not** gate on the numbers — measured run-to-run spread is 7.2% (median) and 65.4% (worst block). |
| `tests/fuzz_state.cpp` | libFuzzer target over `setStateInformation`, the one entry point that parses bytes the plug-in did not write (`fuzz` job, ASan + UBSan as the oracle; a rejected blob is a **pass**). Behind `ANAMORPH_BUILD_FUZZ` (OFF); `-fsanitize=fuzzer` is **target-scoped** because libFuzzer's `main` breaks CMake's compiler probe. Leaks JUCE's `ScopedJuceInitialiser_GUI` deliberately — letting `shutdownJuce_GUI()` run under libFuzzer's `exit()` double-freed in `DeletedAtShutdown::deleteAll()`, which the fuzzer found on the empty input. |
| `tests/dsp_dump.cpp` | The **DEPENDENCY_POLICY rule-2 instrument**, behind `ANAMORPH_BUILD_DSPDUMP` (OFF): one FNV-1a hash over every output byte plus the reported latency, for 32 scenarios (4 algorithms × 4 oversampling factors × M/S). Build against two JUCE checkouts and `diff` — an empty diff is the bit-identity proof. Committed because the two bumps that passed this rule before it each used a scratchpad tool that was then thrown away. **Checks itself every run** and exits 3 rather than print: all 32 must be repeatable AND distinct, which is the exact defect the first scratchpad run shipped (`algoAmount` at its identity default hashed the algorithms the same and proved nothing). Deliberately does **not** link the LTO flags — it isolates the dependency, and link-time inlining is a second variable. |
| `tests/fuzz-corpus/` | Three fuzzing seeds (`*.bin`, one per legacy fixture), generated from `tests/fixtures/*.xml` in JUCE's `copyXmlToBinary` framing so the fuzzer starts from inputs that already reach the parser rather than from noise. **libFuzzer also WRITES here** — this directory is its first positional argument, so it saves newly discovered inputs into it, named after the SHA-1 of their contents. In CI the checkout is discarded; locally it lands in the working tree, and 62 such files were once committed by a `git add -A` alongside documentation saying the corpus held three. The root `.gitignore` now tracks `tests/fuzz-corpus/*.bin` only, so a discovery cannot be committed by accident and a seed still can be, deliberately — the rule lives at the root rather than in this directory because a `.gitignore` inside it would itself be read as a corpus entry (measured: seed count 3 → 4). |
| `tests/state_tests.cpp` | 13 headless state-compatibility tests (schema shape, parameter-registry snapshot, raw-exact round-trip, 3 legacy migration fixtures, corrupt-state robustness, preset round-trip, A/B + view-param preservation, factory/user preset identity under a shared name, factory-id integrity, indicator identity across a session reload, wrapper audio path through the real `processBlock`) — own console target `AnamorphStateTests` compiling the plugin sources. |
| `tests/fixtures/` | Compatibility fixtures: `parameter_registry.snapshot` (re-frozen only via `AnamorphStateTests --write-snapshot` for INTENTIONAL parameter changes) + 3 frozen legacy session XMLs (v0.2 / pre-0.6.4 / pre-0.8.4). |
| `scripts/setup-linux.sh` | Ubuntu build dependencies (+ xvfb, + lld for the Clang/LTO link). |
| `scripts/setup-llvm-apt.sh` | Installs ONE Clang major (compiler + `lld` + `libclang-rt`) from **apt.llvm.org**, for the two Linux Clang jobs — Ubuntu's archives stop at `clang-20` for noble while the pin is upstream stable (ADR-0028). Takes the major as an argument so `ANAMORPH_CLANG_VERSION` stays the single authority; reads the suite codename from `/etc/os-release`; **fail-closed** (the compiler is the job, unlike the optional ccache beside it) and asserts the installed major. |
| `scripts/build.sh` | CMake + Ninja build; prints artifact paths (VST3, Standalone, both suites). |
| `scripts/run-tests.sh` | Runs `AnamorphTests` + `AnamorphStateTests`, fail-closed on absence **and** ambiguity (exactly one match each). `ANAMORPH_TEST_RUNNER` prefixes both invocations — the macOS job passes `arch -x86_64` to execute the universal binary's other slice. |
| `scripts/preflight.sh` | Local pre-push aggregate (2026-08-18): the **seven** checkers with their `--self-test`s, the citation gate against **all three** bases that can disagree (`origin/main`, the branch merge base, and `HEAD~1` — the push predecessor, which is what CI compares and which the other two stop approximating once a branch has more than one commit), then `run-tests.sh` when a built tree exists — skipped **with a note**, never silently. States its own limits: the two warning gates need a build log from the pinned compiler, and the ABI floor needs the linked artifacts — so those run as self-tests only, except the ABI floor, which also runs for real when a local Release build is present. |
| `scripts/run-pluginval.sh` | pluginval on Linux/macOS (strictness + mode + **format** args — `deterministic` \| `randomise` each ×3, `vst3` \| `au`; fixed **nonzero** seed; `ANAMORPH_PLUGINVAL_BUNDLE` overrides discovery for an installed AU; signal-only crash retry **scoped by `uname -s`** — 3 attempts on Linux for the X11 XEmbed flake, **1 on macOS**, which shares none of that machinery). |
| `scripts/run-pluginval.ps1` | pluginval on Windows (same strictness/mode/×3/seed structure; exit code is the sole signal; `--skip-gui-tests` for the GPU-less runner, KI-007). |
| `scripts/check-docs.py` | Structural Markdown lint over the whole document set (table integrity, relative links, blockquote lazy continuation, unclosed fences, CHANGELOG entry-boundary rule). `--self-test` first. |
| `scripts/check-realtime.py` | Static realtime lint (`source-lint`): scans the bodies of the functions `REALTIME_AUDIO_POLICY` binds — `processBlock` (wrapper included, so the scan root is `src`, not `src/dsp`), `process`, and every module's `reset`/`softReset` — for its forbidden list (allocation, container growth, locks, threading, IO). **Function-scoped on purpose** — `prepare()` is required to allocate, so a file-wide token scan would flag the eight legitimate `setSize` calls in `AnamorphEngine.cpp`; comments and string literals are blanked before matching. The third realtime tier (ADR-0029): the only one that reads code the suite never executes. `--self-test` proves it fires and stays quiet — including over the two lexical constructs that would otherwise blank a line and swallow a real violation (an unbalanced quote inside a raw string; an encoded character literal such as `L'a'`, whose opening quote has the same alphanumeric-both-sides shape as a digit separator), and over a definition whose opening brace sits far from its signature — the brace search is unbounded, because a bounded one dropped such a definition from the scanned set with no diagnostic. |
| `scripts/check-linux-abi.py` | Asserts the shipped Linux binaries stay within a declared **glibc/libstdc++ floor** — the maximum symbol version they import, which is the oldest system that can load them at all. Runs as the `linux` job's **last step**, on what the strip produced (the exact shipped bytes), so a breach fails the run without withholding the suites' results or the artifacts needed to diagnose an image move. Holds the floor itself; `COMPATIBILITY_MATRIX.md` defers rather than restating a number. Exists because a runner-image move raises that floor **silently and retroactively**, with no failure in CI and no line in any diff. `--self-test` covers the ordering trap (2.38 outranks 2.9 numerically, not lexically) and treats "no version references found" as an error rather than a pass — **per declared family as well as per binary**: a family the binary does not reference at all used to be skipped, so a shipped artifact that stopped importing `GLIBCXX_*` would have passed that half of the floor vacuously. |
| `scripts/check-portability.py` | Rejects explicit template arguments on `juce::jmin/jmax/snapToZero` (instantiates `dsp::SIMDRegister<T>` — compiles on Linux, fails on macOS); also checks the Linux installer/uninstaller scratch-name sets agree. Two different proofs: `--self-test` (in `source-lint`, beside the lint) proves the **checker** still fires and still stays quiet; `--compile-canary` (in `linux-clang`, where JUCE is checked out) proves the **hazard** still exists in the pinned JUCE. |
| `scripts/check-citations.py` | Keeps `file.cpp:NNN` evidence anchors in `docs/` pointing at the text they named at a base revision. Governs `src/`, `tests/`, the four `scripts/`, **`CMakeLists.txt` and `.github/workflows/build.yml`** — the last two added after 96 anchors into them drifted while the gate reported clean; `packaging/*/INSTALL.txt` and `NOTICE` remain outside it. `--check` reports drift, `--fix` re-anchors; deliberate re-aims are declared in `DELIBERATE_REAIMS`. `--self-test` proves the ownership test, the line map and the span rewriter are live — this is the only lint that **writes** to the documents, so a defect corrupts rather than merely misses. `--fix` also **reports the `DELIBERATE_REAIMS` declarations its own rewrite invalidates**, naming the replacement spelling: the self-test catches a dead declaration, but in CI minutes later and without knowing what killed it. A misaimed declaration is a hard failure in `--check` (the mode CI runs) but only a warning in `--fix`, which then repairs everything else and still exits non-zero: declared anchors drift for ordinary reasons — two are single lines in `build.yml` — and stopping the whole run over one blocked the repair of every unrelated citation too (measured: 0 re-anchored vs 31 across 16 documents). The document that **owns** the misaimed declaration is withheld from the rewrite, because re-deriving one anchor while its excused neighbour stays put is how a sentence ends up naming one line twice. |
| `scripts/check-clang-warnings.py` | The first-party warning gate for the `linux-clang` job: classifies each Clang diagnostic **structurally** by resolved path (`src/`, `tests/`, never through `_deps`). `--self-test` proves the classifier is live. |
| `scripts/check-gcc-warnings.py` | The **GCC-only** first-party warning gate (`linux-lto-tests`). Not redundant beside the Clang gate: Clang's `-Wshadow-all` structurally does not report a parameter or local shadowing a member outside a constructor, and Clang has no `-Wmisleading-indentation`. Five gated flags, held here and read by the workflow via `--print-flags` so build and gate cannot drift; `-Wnull-dereference` and `-Wmismatched-new-delete` are excluded with the measurements that rejected them. Same structural classifier, baseline format and exit-2-on-version-mismatch semantics as the Clang gate. `--self-test` proves the classifier and both comparison directions are live. |
| `scripts/gcc-warning-baseline.txt` | The accepted GCC-only first-party sites (3 today) — a **debt list, not a permission list**. Keyed on `(path, flag)` with a distinct-site count and no line numbers; records `# gcc-major:` and the gate refuses to run against another major. |
| `src/AbSlotIndex.h` | `anamorph::kNumAbSlots` + `clampAbSlotIndex` — single source of truth for A/B slot sizing/clamping. |
| `packaging/macos/INSTALL.txt` | macOS install + de-quarantine instructions (ad-hoc signed, not notarized). Installation content only — no testing or attribution section. |
| `packaging/macos/build-pkg.sh` | Builds the macOS `.pkg` installer (three component packages + productbuild, component selection with a full-install default, every component non-relocatable so a re-install always writes its declared destination) from the CI-staged payload. |
| `packaging/windows/Anamorph.iss` + `INSTALL.txt` | Inno Setup installer script (stable AppId; component page + dual-path destination page, VST3 → Common Files) + Windows install notes (shipped in the zip). |
| `packaging/linux/install.sh`, `uninstall.sh`, `INSTALL.txt` | Linux installer/uninstaller — prompts for per-user (`~/.vst3`, `~/.local/bin`, no root; the default) or system-wide (`/usr/lib/vst3`, `/usr/local/bin`, `sudo`), or is told with `--user` / `--system` when there is no terminal to ask on (`uninstall.sh` also takes `--discard-parked`) — + install notes; all three ship in the zip. |
| `docs/user/USER_MANUAL.md` | Full end-user manual (interface, signal flow, algorithms, presets, workflows, troubleshooting); attached to GitHub releases. |
| `docs/user/INSTALLATION.md` | End-user installation guide for all three platforms (installer + manual routes). |
| `.github/actions/setup-linux-build/` | Composite action carrying the Linux setup the seven Linux jobs share: `setup-linux.sh`, optionally the pinned Clang (`clang-version`) and extra apt packages (`extra-packages`, both fail-closed), then the ccache install behind its non-fatal fallback. **The ccache fallback is a policy, and this is its one copy** — it used to be written out seven times, six of them byte-identical. The per-job cache **lineage** deliberately stays in `build.yml`: that part genuinely differs per job. |
| `.github/workflows/build.yml` | 3-OS build + DSP **and state** self-tests + pluginval (both modes ×3, **blocking on all three platforms**, VST3 everywhere and **AU on macOS**; strictness held once in `env.ANAMORPH_PLUGINVAL_STRICTNESS`); plus `macos-intel` on `macos-15-intel`, which runs the same suites and the same VST3+AU gate against a thin `x86_64` build on **native Intel** hardware and ships nothing (the `macos` job's Rosetta step executes the *shipped* slice but on arm64 hardware, so the two are complements); plus seven non-packaging jobs with no `needs:` in either direction — `docs`, `source-lint`, `linux-clang` (Clang warning gate over `src/`+`tests/`, and the only job that builds the LTO'd plugin with a second compiler), `sanitizers` (ASan+UBSan with **LeakSanitizer now a gate**, then valgrind memcheck), `linux-lto-tests` (**digest-pinned `gcc:16.2.0` container**: both suites built and run with `-flto`, the only place a behavioural assertion executes LTO codegen, plus the GCC-only warning gate and the benchmark build+smoke), `realtime` (the DSP suite under **RealtimeSanitizer** behind a liveness canary, plus the leaf-layer `-Werror=function-effects` check — the first mechanical detectors for `REALTIME_AUDIO_POLICY`; ADR-0029), `fuzz` (`setStateInformation` under libFuzzer, bounded, corpus-seeded). Stages the per-platform packages — flat `Anamorph-<OS>` artifacts (loose files; release.yml archives the release zip from the same tree), Windows/macOS installers, `-debug` symbols. One run per ref (`concurrency`, tags exempt from cancellation); same-repo PR events skipped as duplicates of the branch push. Every job carries an explicit `timeout-minutes`, and every Ninja job compiles through **ccache** restored from `actions/cache` (Windows excepted — `/Zi` is required for the shipped PDB). Also callable (`workflow_call`) by release.yml. |
| `.github/workflows/release.yml` | RH-PR-8 release skeleton: annotated `vX.Y.Z` tag → fail-closed metadata validation → reused build.yml gates → **draft** GitHub Release (versioned artifacts + SHA-256 sums + manifest); `workflow_dispatch` = rehearsal. |
| `.github/workflows/codeql.yml` | CodeQL (`c-cpp` manual build + `actions`); alerts scoped to repo-own code. See `docs/procedures/CI_CD.md` §Security scanning. |
| `.github/workflows/msvc.yml` | MSVC `/analyze` → SARIF; JUCE treated as external; path-filtered triggers. |
| `.github/workflows/dependency-review.yml` | Dependency Review on PRs to `main` (GitHub Actions deps; comment on failure only). |
| `.github/dependabot.yml` | Weekly `github-actions` bumps in two semver-split groups (minor/patch, major), with the SHA-pinned MSVC-analysis action ignored. Everything else is maintained by another mechanism — `DEPENDENCY_POLICY.md` §Update mechanisms. |
| `.github/ISSUE_TEMPLATE/` | `bug_report.yml` — the "Test report — bug" form (version+build, OS, DAW, format, install route, repro — the fields triage actually needs; carries the closed-source/public-tracker notice) + `config.yml` (links to the install guide, FAQ, known issues and the internal testing guide). |

## `docs/` — documentation library

```
docs/
├── SOURCE_OF_TRUTH.md, HANDOVER.md, REPOSITORY_MAP.md, DOCUMENTATION_COVERAGE.md,
│   POSTMORTEMS.md, KNOWN_ISSUES.md, FUTURE_RISKS.md, COMMERCIAL_STATUS.md
│   (COMMERCIAL_STATUS.md = internal index of the product model, distribution model and the
│    open owner/legal decisions; it indexes, never overrides, the records it cites)
├── user/           (end-user class: USER_MANUAL, INSTALLATION)
├── architecture/   (system reference: ARCHITECTURE, SIGNAL_FLOW, DSP_GRAPH_REFERENCE,
│                    THREAD_MODEL, API_REFERENCE, PARAMETER_*/SERIALIZATION_*/STATE_*,
│                    LATENCY_MODEL, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT,
│                    DSP_ALGORITHMS, COMPATIBILITY_MATRIX, RELEASE_HARDENING_PLAN,
│                    design-decisions/ADRs)
├── procedures/     (DEVELOPMENT, BUILD, CI_CD, PACKAGING, TESTING, RELEASE_PROCESS,
│                    RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING)
└── policies/       (REALTIME_AUDIO, THREADING, DSP, COMPATIBILITY family,
                     ARCHITECTURE_REVIEW_GATE, ADR, DOCUMENTATION_LIFECYCLE, AI_AGENT,
                     CHANGELOG, TESTING, RELEASE, DEPENDENCY, CODE_STYLE)
```

Evidence [Verified]: file tree from the repository; CMakeLists.txt:93-233 (hardening interface) + :244 (`AnamorphDSP`) / :270 (`juce_add_plugin`); src/ listing.
