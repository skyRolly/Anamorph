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

**Same-repo pull requests run `merge-check` and nothing else.** The build and lint jobs carry
`if: github.event_name != 'pull_request' || github.event.pull_request.head.repo.full_name != github.repository`.
`push: ["**"]` already builds the SHA, so the `pull_request` event was a duplicate 3-OS matrix per
commit for as long as a PR stayed open. **Fork** PRs are not covered by the push trigger (the push
happens in the fork), so they still run the full matrix — the only case that trigger uniquely serves.

The push build and the PR build are **not the same tree**, and that is what `merge-check` exists
for. `push` builds the branch **tip**; `pull_request` builds `refs/pull/N/merge`, the tip merged
with the base as it stands. A PR green on its own tip can be broken once the base moves under it,
and while every job was skipped that tree was never compiled until the merge landed. `merge-check`
carries the exact complement of the guard — same-repo PRs only — checks out the merge commit,
builds it and runs both self-test suites. It deliberately stops there: packaging, pluginval and the
other two platforms validate properties of the tip that the push build already gated, so re-running
them would restore the duplicate matrix the guard removed. It produces no artifacts.

Two further consequences: the guarded jobs report as **skipped** in a same-repo PR's checks list
(they can still be required checks if the branch protection accepts a skipped conclusion, and
`merge-check` is the one that actually runs there, so it is the one to require if only one can be);
and inside a reusable workflow `github.event_name` is the **caller's** event — `release.yml` fires
only on a tag `push` and `workflow_dispatch`, so the guard is true there and every guarded job runs
while `merge-check` is skipped, which is correct: a tag has no merge result. **Adding a
`pull_request:` trigger to `release.yml` would break that**; no job here gates on another job's
output, so the failure mode is a missing check rather than a green run that built nothing.
Evidence [Verified]: `.github/workflows/build.yml` (`concurrency:` block; each job's `if:`).

## pluginval strictness lives in one place

`ANAMORPH_PLUGINVAL_STRICTNESS` in `build.yml`'s `env:` block is the **single authority** for the
number. It replaced six literals spread across the three build jobs — six chances for a raise to
land in five of them — and the same rule now holds across the documents: `TESTING_POLICY.md` states
what the gate *requires*, this file states how it is *wired*, `RELEASE_POLICY.md`,
`DEPENDENCY_POLICY.md`, `COMPATIBILITY_MATRIX.md` and `RELEASE_COMPATIBILITY_CHECKLIST.md` state
that the gate must pass, and **none of them prints the value**. Read it from the workflow. The one
place a literal number still appears is `DEPENDENCY_POLICY.md`'s compliance log, where it records
the strictness a past bump was actually verified at — a fact about a run, not a requirement, and
correct to leave frozen.
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

Every push builds the full set of formats on all three desktop OSes — plus a **second macOS job
that ships nothing and exists only to execute on Intel silicon** — alongside nine non-packaging
jobs that guard classes the build matrix cannot see:

| Job | Runner | Builds | pluginval |
|---|---|---|---|
| **merge-check** | `ubuntu-latest` + **pinned `clang`** | VST3 + Standalone + tests, from `refs/pull/N/merge` — **same-repo PRs only**, no packaging, no artifacts | — |
| **docs** | `ubuntu-latest` | — (`scripts/check-docs.py --self-test` then the lint) | — |
| **source-lint** | `ubuntu-latest` | — (each lint preceded by its own `--self-test`: `check-portability.py`, then `check-citations.py --check`) | — |
| **linux** | `ubuntu-latest` + **pinned `clang`/`lld`** | **Clang: the shipped VST3 + Standalone (+ tests)**; also the portability canary, the first-party Clang warning gate, and a `-fsyntax-only` compile of the two opt-in instruments | VST3, **both modes ×3** (deterministic + randomise) — **blocking** |
| **sanitizers** | `ubuntu-latest` | Clang ASan+UBSan build, plus an unsanitized build for valgrind | — |
| **windows** | `windows-latest` (MSVC, multi-config) | VST3 + Standalone (+ tests) | VST3, **both modes ×3** — **blocking** |
| **macos** | `macos-latest` (Apple Silicon) | universal VST3 + AU + Standalone (+ tests) | **VST3 and AU**, both modes ×3 each — **blocking** |
| **macos-intel** | `macos-15-intel` (**native Intel**) | thin x86_64 VST3 + AU (+ tests); Standalone off; **no packaging, no artifacts** | **VST3 and AU**, both modes ×3 each — **blocking** |
| **macos-crossslice** | `macos-latest` (Apple Silicon) | the twin-dump instrument for both universal slices (arm64 native + x86_64 under Rosetta); **no packaging, no artifacts; job-level `continue-on-error` — reporting-only by design** (a cross-architecture comparison is Class B, not a gate; ADR-0031 §Consequences) | — |
| **windows-avx2-ab** | `windows-latest` (MSVC) | two twin-dump builds — baseline vs `/arch:AVX2` — diffed hash-for-hash; **no packaging, no artifacts**; **BLOCKING** (the ADR-0032 per-push Class-A assertion: any of the 32 scenarios moving fails the push, with NUMERICAL vs INFRASTRUCTURE failures distinctly labelled) | — |
| **linux-lto-tests** | `ubuntu-latest` + **floating `gcc:16`** (major pinned, patch not) | GCC `-flto`: both test targets only (Standalone off); **no packaging, no artifacts** | — (the suites against LTO codegen, the GCC warning gate, and the two instruments' liveness builds) |
| **realtime** | `ubuntu-latest` | Clang `-fsanitize=realtime`: the DSP suite only (Standalone off); **no packaging, no artifacts** | — (the audio path under RealtimeSanitizer, plus the leaf-layer `-Wfunction-effects` check) |
| **fuzz** | `ubuntu-latest` | Clang libFuzzer + ASan/UBSan: `AnamorphFuzzState` only (tests and Standalone off); **no packaging, no artifacts** | — (`setStateInformation` under libFuzzer) |

None of these jobs is in a `needs:` chain, in either direction. A prose defect, a
portability lint hit or a sanitizer finding fails the run without skipping a binary that is
otherwise fine, and a red build does not skip them.

**That is a statement about *this* workflow, and the release path is different.**
`release.yml` calls `build.yml` as a single `build:` job and its `draft-release` job is
`needs: [validate, build]`. A called workflow's aggregate result is what that edge observes, so on
a release tag **every** job here — including `sanitizers`, `linux-lto-tests`, `realtime`, `fuzz`
and `windows-avx2-ab` — is
release-blocking: a failure in any of them skips the draft release, even though the per-push
artifacts were still uploaded. The one exception is `macos-crossslice`, whose **job-level
`continue-on-error`** makes it reporting-only on every trigger, release tags included — the
deliberate ADR-0031 Class-B scoping, not a gap. That follows `RELEASE_POLICY.md` §Artifacts ("the existing
`build.yml` gates are reused unchanged") and is the intended behaviour; the absence of a `needs:`
edge above must not be read as release non-blocking.

### What the non-packaging jobs are for

- **docs** — structural Markdown lint over the whole document set (table integrity, relative links,
  blockquote lazy continuation, unclosed fences, and the CHANGELOG entry-boundary rule the release
  notes extraction depends on). `--self-test` runs **first** and is the load-bearing half: a checker
  that has stopped matching anything is indistinguishable from a clean tree.
- **source-lint** — (a) the **JUCE SIMD overload hazard**: an explicit template argument on
  `juce::jmin/jmax/snapToZero` instantiates `dsp::SIMDRegister<T>`, which completes on Linux (where
  `size_t` IS `uint64_t`) and **fails to compile on macOS** (where it is not). The divergence is in
  the *typedef*, so no Linux compiler can see it — a lint is the only Linux-runnable guard, and
  the Clang gate would not catch it. The tree is clean of it; the job is a regression guard.
  (b) the **evidence-anchor gate**: `docs/` carries 184 `file.cpp:NNN` citations, and an edit above
  one silently re-aims it. See [Evidence anchors](#evidence-anchors).
  (c) the **static realtime lint** (`check-realtime.py`, ADR-0029): the bodies of audio-path
  functions are scanned for the `REALTIME_AUDIO_POLICY` forbidden list. Its scan root is **`src`**,
  not `src/dsp`, because the Policy's first named function,
  `AnamorphAudioProcessor::processBlock`, is not under `src/dsp`; module `reset`/`softReset` bodies
  are in scope for the same reason. It is the third realtime tier and the only one that reads code
  the DSP suite never executes — RTSan and the allocation guard are runtime tools, and the suite
  covers 93.4 % of lines / 79.9 % of branches in `src/dsp`.
  Function-scoped deliberately: `prepare()` is *required* to allocate, so a file-wide
  token scan would flag the eight legitimate `setSize` calls in `AnamorphEngine.cpp` and be switched
  off.
  **The scanned set is the audio thread's REACHABLE set, not a list of names** (2026-08-18). The
  Policy-named functions are the seeds; from each, every callee **defined in the same file** is
  scanned too, transitively. Before that, a helper was invisible purely because of what it was
  called — `AnamorphEngine::updateDerived()` (run at the bottom of a switch duck) and
  `VelvetNoise::updateWeights()` (run per block while the density glide moves) are audio-thread code
  that no version of a hand-maintained name list would have kept up with. 35 bodies became 61.
  `prepare`/`prepareToPlay`/`releaseResources` are never followed, which is how allocation stays
  legal where the Policy says it is legal. The forbidden set also gained the forms this codebase
  actually writes — `.assign`, `.insert`, `make_unique`, `make_shared` — which it had been missing:
  `.assign` is the allocation idiom of every DSP module and `make_unique` is how the engine
  allocates its oversamplers, so the likeliest regression was the one the lint could not see.
  Each of the three runs its own `--self-test` **first**, in this job and ahead of the lint it
  verifies — the step immediately before, for the two that can be; for `check-citations.py` its own
  step ahead of the one that resolves the base revision and then compares, which is the job-and-order
  form `TESTING_POLICY.md` rule 4 requires. The same load-bearing move as `docs`. The
  portability self-test is not the same check as `--compile-canary` in `linux`: that one asks
  whether the pinned JUCE still *has* the hazard, this one whether the checker still *finds* it, and
  a green canary over a dead scanner reports a clean tree.
- **linux (the Clang warning gate)** — `juce_recommended_warning_flags` picks its set by **compiler ID**, and Clang's is
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
  positives rather than silence; memcheck answers the same question with no rebuild. The UBSan list
  is `undefined` plus `vptr` (C++ only — Clang 21 dropped it from the group, ADR-0028) plus five
  groups outside `undefined`, added 2026-08-18 after a census run showed both suites execute ZERO
  diagnostics under them: `float-divide-by-zero`, `implicit-conversion`, `unsigned-shift-base`,
  `local-bounds` (trap-based — a hit can die by SIGILL with no diagnostic text), `nullability`. The
  **`implicit-conversion` carries one scoped exemption** (`scripts/ubsan-ignorelist.txt`,
  2026-08-21): the editor-lifetime test made the constructor shape text, which reaches vendored
  HarfBuzz inside JUCE, where `hb-face.cc` assigns `-1` to an `unsigned` as its documented
  "not computed yet" sentinel. UBSan is right that it narrows; it is not a defect and it is not
  this project's code. The ignorelist is a clang `[implicit-conversion]` section over `*juce-src/*`
  — **one sub-check, one tree**: every other sanitizer still instruments the vendored code in full,
  and no first-party path can match. Verified in both directions on clang-22, since an ignorelist
  that silenced everything would look identical to one that works: with a narrowing conversion
  seeded into a `juce-src` path AND into `src/PluginProcessor.cpp`, the vendored one goes quiet and
  the first-party one still fails the run. The alternatives were dropping `implicit-conversion`
  outright (weakens every TU, first-party included) or not constructing the editor under sanitizers
  (removes the coverage that test exists for). The
  full `integer` group is **deliberately absent**: its `unsigned-integer-overflow` half flags legal,
  intentional wraparound (JUCE string hash, `Random` LCG, tick arithmetic, libstdc++'s mersenne
  twister — all census-measured) and would fail the job under `halt_on_error=1` on correct
  third-party code. `ASAN_OPTIONS` adds `check_initialization_order=1:strict_init_order=1:`
  `strict_string_checks=1` (measured clean on both suites). valgrind runs
  **both** suites because the read that would matter runs through the real wrapper `processBlock`,
  which only `AnamorphStateTests` drives (`testWrapperProcessBlockAudioPath` — before 2026-08-18 no
  test in that suite called `processBlock` and this sentence was aspirational; the workflow comment
  says so too). `--error-exitcode=1` makes a finding fail the job (not
  valgrind's default). **`detect_leaks=1` — LeakSanitizer is a gate here now.** The old
  justification for `0` (JUCE singleton teardown reports) was retested 2026-08-18 and no longer
  held: both suites already ran leak-clean, so the flag was suppressing a detector that had nothing
  to suppress. It was flipped in the same cycle. The consequence is the point of the change — a leak
  introduced in an audio-plugin process is a leak in a host that stays open for hours — so a report
  here is to be **investigated**, never answered by setting it back to `0`. (The `fuzz` job is the
  one place that still runs with `detect_leaks=0`, for a specific and documented reason: see below.)
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
- **realtime** — the DSP suite built with **`-fsanitize=realtime`** and run (added 2026-08-18,
  ADR-0029). It is the first mechanical detector for `REALTIME_AUDIO_POLICY`, the repository's
  Priority-1 policy, which was previously enforced only by review and a hand-written audit: ASan,
  UBSan and valgrind all treat a `malloc` added to `AnamorphEngine::process` as a perfectly correct
  allocation, and RTSan is the only tool here that asks *where* it happened. It needs **its own job**
  by driver restriction — clang rejects `-fsanitize=realtime` alongside `address`, `undefined`, this
  pipeline's `address,undefined,vptr` set, or `thread`. The job sets **no `RTSAN_OPTIONS`** and that
  is load-bearing: RTSan halts on the first violation by default, and `halt_on_error=false` makes the
  process print its reports and still exit 0. A **liveness canary** (`tests/realtime_canary.cpp`)
  compiles and runs first and the step fails unless it aborts *with* a sanitizer report — the same
  prove-it-can-fail discipline as the seven lints' `--self-test`s.
  The job also runs **`-Werror=function-effects` over the JUCE-free leaf layer**
  (`tests/realtime_effects.cpp`, `-fsyntax-only`, seconds): a driver function annotated
  `ANAMORPH_NONBLOCKING` calls `MidSide`, `LR4Xover`, `ScopeBuffer`, `CorrelationMeter` and
  `LevelMeters` exactly as the audio path does, so the compiler proves those bodies effect-clean
  *before* any test runs them. That scope is the whole point and is measured, not assumed: over the
  leaf layer the flag emits **0** diagnostics and still fires precisely (the seeded
  `ANAMORPH_EFFECTS_CANARY` call to an allocating helper fails the step by name — *not* a call to
  `applyWidth`, which this page named until 2026-08-19: its definition is visible in that TU, so
  Clang infers its effects and the driver's own call to it is clean), while over `AnamorphEngine.cpp` it emits **52**
  from JUCE calls whose definitions the TU cannot see — JUCE 9.0.1 carries no annotations of its own.
  So the flag is enabled exactly where it is signal and stays off where it is noise; ADR-0029 §3
  records both measurements and the boundary between them.
  That TU is compiled **twice**, and the second compile is this gate's liveness proof: a clean
  compile is its whole output, and an unrecognised `-Werror=<name>` is only a *warning* to Clang, so
  a renamed or dropped `function-effects` would leave the step exiting 0 while checking nothing
  (measured on Clang 22.1.8 — with the option misspelled, a TU carrying a real violation compiles
  with status 0). The second compile adds `-DANAMORPH_EFFECTS_CANARY`, which seeds an allocating
  non-annotated helper and a call to it into the same file, and the step fails unless that compile
  fails *with* a `-Wfunction-effects` diagnostic. `-Werror=unknown-warning-option` on both compiles
  makes the renamed case fail on the *first* compile and by name; the canary would catch that case
  too — a name Clang no longer knows is a diagnostic it no longer emits — and additionally covers
  the one the flag never can, an option still accepted but no longer implemented.
  RTSan is the strongest of the three realtime tiers but the least portable — Clang, Linux/macOS
  only. The **allocation guard** compiled into the DSP suite (Test 38) covers the shipped toolchains
  it cannot reach, MSVC included, because `operator new` replacement is standard C++; it runs in
  the jobs that build that suite rather than in one of its own. Two builds compile it out and the
  test says so in both: the valgrind build by flag (`-DANAMORPH_NO_ALLOC_GUARD`), and **this job**
  by self-detection — the guard's interposers would otherwise shadow RTSan's allocation
  interceptors and blind the lane (measured, ADR-0029 §7).
  That self-detection is a single spelling — `__has_feature(realtime_sanitizer)` — and since
  2026-08-19 it is **cross-checked rather than trusted**: the job's `CMAKE_CXX_FLAGS` carry
  `-DANAMORPH_RTSAN_LANE=1` beside `-fsanitize=realtime`, and `tests/AllocationGuard.h` `#error`s if
  the lane is declared but the guard did not stand down. The two statements sit on one line and
  cannot drift apart; the check is deliberately *not* keyed on the feature macro, because a test
  that consults the signal it is verifying proves nothing. Without it, a renamed or removed feature
  name would silently compile the guard back in and the lane would report a clean run with its
  **allocation** detection switched off — lock and blocking-call interception survives, but
  allocation is the class this suite exists to police, and on a healthy tree the run exits 0 either
  way.
- **linux-lto-tests** — both suites built and run with `-flto` on GCC Release (added 2026-08-18),
  the **GCC-only first-party warning gate** (§The GCC warning baseline), and the liveness builds of
  the two committed instruments: `AnamorphBench` (built and smoke-run, no timing asserted) and, since
  2026-08-21, `AnamorphDspDump` (built and run with `--self-check`, which asserts its 32 scenarios are
  repeatable and mutually distinct and exits 3 if not). Both are compiled here by the container's
  **g++ 16**, not by the Clang that builds the shipped artifact — the `linux` job covers that half
  with a `-fsyntax-only` compile of both translation units under the pinned Clang, so a Clang-only
  break in either still fails a gate.
  The shipped plugin is the only target linking `juce::juce_recommended_lto_flags`, and the test
  targets deliberately do not (so the sanitizers job builds them cleanly and quickly) — which meant
  no behavioural assertion had ever executed link-time-optimized codegen while the binary users load
  is exactly that. This job runs the identical `-flto` spelling through the CMake cache variables so
  no CMake structure changes (a CMake-structure change is a gated Build System change); pluginval
  still validates the shipped bytes for conformance, this job validates the numeric assertions under
  the shipped optimization class. Not in any `needs:` chain, same reasoning as `sanitizers`.
  It also carries the **GCC warning gate**, and carries it *here* rather than in a job of its own
  because its two targets are already the whole first-party surface: `AnamorphStateTests` compiles
  `${ANAMORPH_PLUGIN_SOURCES}` and both targets compile `src/dsp/*.cpp` through the `AnamorphDSP`
  INTERFACE library, so the gate costs one `tee` and one Python invocation rather than a runner.
- **fuzz** — `setStateInformation` under **libFuzzer**, with ASan + UBSan as the oracle (added
  2026-08-18). This is the one entry point where the plug-in parses bytes it did not write: a session
  saved by an older version, a preset from another machine, a host that truncated a chunk.
  `AnamorphStateTests` already drives the *shaped* cases — three legacy fixtures, a garbage blob, an
  out-of-range A/B index, unknown fields, a corrupt slot — and every one of them exists because a
  human thought of it first; this is the part that does not have to. A **rejected** blob is a pass,
  because refusing malformed state is what the path is for; the failure condition is a sanitizer
  report, not an assertion. It is **bounded** so it stays a gate rather than a background service: a
  fixed `-max_total_time=90` from the three committed corpus seeds in `tests/fuzz-corpus/`
  (which libFuzzer also writes its own discoveries back into — harmless on a CI checkout, and
  `.gitignore`d so a local run cannot commit them)
  (real fixtures in JUCE's `copyXmlToBinary` framing, so the fuzzer starts from inputs that already
  reach the parser), under a **fixed `-seed`** — the same discipline `run-pluginval.sh` applies to
  its deterministic mode, and for a sharper reason: this job is release-blocking through
  `release.yml`'s aggregate dependency, and a release must not be able to fail on a lottery. The
  honest limit is stated rather than papered over: `-max_total_time` is wall-clock, so a slower
  runner executes fewer inputs and the *tail* of the exploration is machine-dependent (measured: 792
  vs 807 executions across two identical local runs). `-runs=N` would be exactly reproducible but
  would make the duration machine-dependent instead, and a release gate that can overrun its timeout
  on a slow runner is the worse failure. So a finding is reproduced from the **uploaded artifact**,
  never by re-running the fuzzer — the harness feeds its input to the real entry point unmodified,
  so those bytes reproduce exactly, on any machine. A crashing input is uploaded as `Anamorph-fuzz-findings` — libFuzzer writes the
  exact reproducing bytes and the harness feeds its input to the real entry point unmodified, so the
  artifact is a host chunk that reproduces by being handed back. `detect_leaks=0` here is deliberate
  and is **not** a gap: the harness leaks exactly one object on purpose (JUCE's
  `ScopedJuceInitialiser_GUI`), because letting `shutdownJuce_GUI()` run under libFuzzer's `exit()`
  produced a double-free in `DeletedAtShutdown::deleteAll()` during `__run_exit_handlers` — measured,
  on the empty input, within 60 s. Leak coverage for the same code lives in `sanitizers`, which now
  runs with `detect_leaks=1`.

`MALLOC_PERTURB_=1` is set on the `linux` self-test steps: glibc then fills **fresh**
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

The three **shipping** jobs use the **floating** `*-latest` label; `macos-intel` is the one pinned
label in the workflow and is dealt with separately below. macOS moved off the pinned `macos-14`
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
`::warning::` as the record.)

**`macos-intel` covers the other half — native Intel hardware.** Rosetta 2 translates the x86_64
instruction stream and runs the result on **arm64** hardware, so the Rosetta step answers "does the
*shipped* slice execute correctly?" and cannot answer "does an Intel CPU run this correctly?". The
`macos-intel` job answers the second: it configures **thin x86_64** (`-DCMAKE_OSX_ARCHITECTURES=x86_64`,
same `10.13` deployment target, `ANAMORPH_BUILD_STANDALONE=OFF`), so every object comes out of the
Intel code generator with no arm64 slice for the loader to pick instead, then runs both self-test
suites and the full pluginval gate — VST3 **and** AU, both modes ×3 each, at the same
`ANAMORPH_PLUGINVAL_STRICTNESS`. Four defect classes live only there: Intel code generation at
`-O3 + LTO`; the **denormal invariant**, which holds because `ScopedNoDenormals` flushes *in
hardware* — MXCSR's FTZ/DAZ on x86_64, FPCR's FZ on arm64 — and is therefore checked here against
the register the shipped Intel slice really sets; the Intel macOS AudioUnit/VST3 runtime; and a
**second AppleClang** on the macOS side (the two images do not carry the same Xcode, and the four
`-Wimplicit-int-float-conversion` sites below are what a single macOS toolchain cost last time).

Its first step **fails, not warns**, if `uname -m` is not `x86_64` or `sysctl.proc_translated` is
not `0` — a job whose purpose is "execute on Intel" is worse than absent if it quietly executes
somewhere else, because the green tick would be read as "Intel is fine". That is the opposite call
from the Rosetta step above, and deliberately: there the coverage is a bonus on a job with other
work to do, here the coverage *is* the job. A second assertion checks `lipo -archs` on both built
bundles is exactly `x86_64` — asserted, never echoed, for the same reason the packaging step
asserts rather than prints. The job **packages, signs and uploads nothing**: the artifact users
receive is still the universal bundle built and gated by `macos`, and a second macOS artifact would
only raise the question of which is authoritative.

`macos-15-intel` is the one **pinned** runner label in the workflow, by necessity rather than
preference: per `actions/runner-images`, `macos-latest` / `macos-26` / `macos-15` are the **arm64**
images and the Intel ones are named separately (`macos-15-intel`, `macos-26-intel`, both GA;
`macos-14*` deprecated), so "latest" has no Intel reading to follow. **15 rather than 26** is also
deliberate — `macos-26-intel` would carry the same Xcode generation as `macos-latest` and differ
from `macos` in ISA alone, whereas `macos-15-intel` differs in toolchain as well, which is the
second-AppleClang coverage this project has already paid for the lack of. The cost of that choice,
stated rather than left to be discovered: a failure seen only on this job has **two** candidate
causes, ISA and toolchain, and the disambiguation is a one-word edit — re-run it on
`macos-26-intel`, and if it still fails the cause is the ISA. The runtime assertion is what makes
the pin safe rather than brittle.

Back to `macos-latest`, and to the 2026-08-15 move described above: that image carries the macOS
toolchain, so the move took the macOS compiler with it — **AppleClang
15.0.0.15000309 (Xcode 15.4) → 21.0.0.21000101 (Xcode 26.6)**, image `macos-26-arm64`
`20260728.0273.1`. `CMAKE_OSX_DEPLOYMENT_TARGET=10.13` is still accepted and both slices still
build. One measured consequence: AppleClang 21 raised
**`-Wimplicit-int-float-conversion` at four pre-existing sites** — `src/PluginEditor.cpp:246, 247`,
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

### The compiler cache

Every job that uses the Ninja generator — `merge-check`, `linux`, `sanitizers`,
`linux-lto-tests`, `realtime`, `macos`, `macos-intel` — compiles through **ccache**, restored from and saved to the GitHub Actions
cache (`actions/cache`, SHA-pinned like every other action — see §Action refs are pinned to commit
SHAs).

**Why, measured rather than assumed.** `.ninja_log` splits a cold Linux Release build into
**1409 CPU-seconds of compilation (75%) and 468 of LTO link (25%)**, and the compilation is
overwhelmingly JUCE: ~9k lines of first-party source against a framework that each of the three
JUCE-linking targets compiles separately. JUCE is pinned to an immutable commit
(`CMakeLists.txt:83-89`, ADR-0022/ADR-0026), so that 75% is byte-identical from run to run.
Measured on 4 cores — the runner's core count — the same build with a warm cache is **7m41s → 3m40s
(−52%)**, at **137 direct hits / 6 misses**, and the residual is the LTO link, which no compiler
cache touches. The then-separate `linux-clang` configuration, measured the same way against the **then-pinned Clang
18**, was **5m48s → 2m36s (−55%)** at **129 hits / 5 misses** (a figure from that measurement, not a
claim about the current pin — the compiler has since moved to 22). Both were measured with the build number
*deliberately changed between the two runs*, so they describe the CI case rather than a favourable
one.

**Measured in CI, not only locally.** Baseline run `31952680908` (before this landed) against warm run
`31959972853`, both the full matrix, all jobs green:

| | baseline | warm | build step: baseline → warm |
|---|---|---|---|
| **run wall clock** | **29m51s** | **17m13s** (−42%) | |
| `macos` (critical path) | 29m44s | 17m09s | 16m40s → **3m09s** |
| `macos-intel` | 21m26s | 13m12s | 10m21s → 3m04s |
| `sanitizers` | 15m07s | 6m38s | 12m14s → 3m31s (two builds) |
| `linux` | 10m57s | 6m21s | 8m04s → 3m34s |
| `linux-clang` (job since folded into `linux`) | 9m38s | 4m13s | 7m58s → 2m29s |
| `windows` (uncached) | 12m49s | 10m02s | runner variance only |

Restoring a cache costs 2–4s and saving one 2–4s, against ~104 MB per lineage — negligible against
the minutes recovered. The **shape of the pipeline has inverted**: `macos` is still the critical
path, but its cost is now the four pluginval passes (12m54s of its 17m09s), not compilation
(3m09s). Further wall-clock reduction has to come out of validation, which is not a trade this
project should make lightly.

**Why it cannot serve a wrong object.** ccache's own hash is the correctness boundary, not the cache
key: it hashes the preprocessed source, the complete command line (every `-D`, `-I`, `-f` and
`-arch`) and — via `CCACHE_COMPILERCHECK=content` — the bytes of the compiler binary. A GitHub cache
key can therefore only ever cost a *hit*; it cannot manufacture a wrong one. The keys are
deliberately coarse for that reason and do **not** hash `CMakeLists.txt`: a key that changed on
every CMake edit would discard the cache for no correctness gain ccache is not already providing.
Multi-arch is included — the full `-arch` **list** is hashed, so a universal object cannot be served
to a thin build or the reverse, and the `macos` job's existing `lipo` assertion on both slices is
the backstop either way.

**Warnings survive a hit.** ccache replays the compiler's stderr verbatim — caret lines and
`[-Wflag]` included. That is load-bearing rather than incidental: `linux` gates on the
diagnostic text of its own build (see "The Clang warning baseline"), and a cache that swallowed
warnings would turn that gate green by deleting its input. Verified against the **then-pinned Clang
18** before enabling, by running that job's real build and its real gate twice: cold and warm produced
**54 warning lines each, `diff`-identical**, and the same verdict — *no new first-party warnings,
14 accepted sites in 7 baseline entries* — with 129 of the 134 compilations served from cache on the
warm run. Those three figures are from that measurement, under that compiler; the property they
establish (replayed stderr is byte-identical to compiled stderr) is a ccache property and is not
version-specific, and the *verdict* half still holds unchanged at Clang 22 — and, measured during
ADR-0033's evaluation, at Clang 23 as well: the first-party set came back `diff`-identical under both.
(The accepted set was 14 sites in 7 entries then; it is **17 sites in 9 entries** since
0.9.5: A7-9's three fixpoint gates
each compare two floats exactly, which is the point of them — `docs/architecture/PERFORMANCE_BUDGET.md`,
the A7-9 entry.)

**One repository property makes it work, and it had to be created.** `ANAMORPH_BUILD_NUMBER` is
`${{ github.run_number }}` and therefore changes every run. It was a *target-wide* compile
definition, and the two targets carrying it are **84.4% of compile time**
(`AnamorphStateTests` 57.7% + `Anamorph` 26.7%) — so every push was guaranteed to miss on 84.4% of
the build because an About-box string had incremented. It is now attached to the single translation
unit that reads it (`CMakeLists.txt`, `set_source_files_properties` beside the version block;
`src/PluginEditor.cpp` already carried the `#ifndef … "0"` fallback). Nothing else ever read it.
A second property is inherited rather than created: each job's build directory name is fixed
(`build`, `build-san`, `build-vg`, `build-lto`, `build-bench`, `build-dump`, `build-rtsan`,
`build-fuzz`), which matters because FetchContent puts JUCE
*inside* the build directory, so its path is in the `-I` flags of every compile — the same tree
built at two different directory names shares nothing.

**That same property is why `linux-lto-tests` fetched JUCE three times.** One `cmake -B` per build
directory means one FetchContent clone per build directory, and that job configures three
(`build-lto`, then `build-bench` and `build-dump` for the two committed instruments). Since
2026-08-21 the two secondary configures pass `-DANAMORPH_JUCE_PATH` at `build-lto/_deps/juce-src` —
the tree this job downloaded minutes earlier, at the SHA `CMakeLists.txt` pins. Nothing is cached
and nothing crosses runs; a missing directory fails `add_subdirectory` rather than silently
re-fetching. One cost, stated: `ANAMORPH_JUCE_PATH` takes the `add_subdirectory` branch, so those
two builds' JUCE `-I` paths move from their own `_deps` to `build-lto`'s — one cold ccache pass for
`build-bench` and `build-dump`, then stable. They are throwaway liveness builds, so that is a fair
trade for two fewer clones. `sanitizers` still fetches twice (`build-san`, `build-vg`) and the same treatment is
available there; it was left alone because that lineage is shared between a sanitized and an
unsanitized build and the change was not worth perturbing it without CI evidence.

**A cross-run `actions/cache` for the JUCE checkout was measured and declined.** The clone is
already `GIT_SHALLOW` at a pinned commit: fetching exactly that commit measured **5.0 s** for a
120 MB tree, while the cache that would replace it is 44 MB compressed and took **2.6 s** to
decompress and extract before any download. A cache hit therefore saves on the order of a second
and a half per configure, against jobs that run 6 to 21 minutes — and buys a key to maintain plus a
path by which the release build could link the wrong bytes. The waste worth removing was the
duplicate fetch inside one job, which needs no cache.

**Cache lineages.** `linux` and `merge-check` share one (`ccache-ubuntu-clang<major>-release-`): same
compiler, same configuration, same build directory name, and they never run in the same event, so a
PR's `merge-check` restores what the last push to the base branch wrote. That is the whole reason
`merge-check` is worth caching — it is the *only* build on the same-repo PR path and therefore that
path's entire critical path. `linux`, `merge-check` **and `sanitizers`** all key on the pinned Clang major, so
raising the pin starts clean lineages instead of restoring entries no build can hit again — ccache
hashes the compiler binary's own contents (`CCACHE_COMPILERCHECK=content`), so objects from the
previous major are dead weight rather than wrong answers, but a restored cache full of them is still
a restore that buys nothing. `linux-lto-tests` and `realtime` each have their own (`ccache-ubuntu-gcc<major>-lto-`,
`ccache-ubuntu-realtime-clang<major>-`) for the same
kind of reason: under `-flto` GCC emits GIMPLE bytecode objects, so it shares no entries with
`linux`'s native ones and a shared lineage would only have the two evict each other. `macos` and
`macos-intel` each have their own.

**Not on Windows.** ccache's MSVC support requires `/Z7`-style embedded debug info, and this project
compiles Release with `/Zi` precisely so the linker emits the PDB that ships as the
crash-symbolication artifact. Windows is also not the critical path (12m49s against macOS's 29m44s),
so the trade is not worth making.

Each cached job zeroes its statistics at configure time and prints them after the build, so the hit
rate is visible per run rather than inferred. **A cold cache is not a failure** — it is what the
first run on a new lineage looks like.

**And neither is an absent one.** *Required* tools fail loudly; the cache warns and steps aside.
Each install step resolves `ANAMORPH_COMPILER_LAUNCHER` into `$GITHUB_ENV` — `ccache` when it is
genuinely installed, empty when it is not — and the configure step passes that through to
`CMAKE_<LANG>_COMPILER_LAUNCHER`, where empty is how CMake spells "no launcher". A package-manager
failure therefore costs the cache and nothing else: the build falls back to an ordinary compiler
invocation, slower but never different, with a `::warning::` naming what was lost. Ninja keeps the
opposite semantics because it is a genuine requirement — though `|| true` is still correct there,
since it is **preinstalled** on the runner images and a Homebrew hiccup was never load-bearing for
it. That distinction was briefly lost: for one commit a bare `ccache --version` after
`brew install ninja ccache || true` turned any transient brew failure into a hard failure of a
release-gating job, which is the trade this paragraph exists to prevent being made again.

`.ccache` lives inside the workspace and is git-ignored. On a hosted runner the location is safe by
ordering — `actions/checkout` runs before the cache restore in every job, so there is nothing to
clean away — and it cannot reach an artifact, because every packaging step names `build/…` and
`dist/…` explicitly rather than globbing the root.

**Reviewed and confirmed by the maintainer (2026-08-16)** — the cache strategy, the preserved
validation coverage and the Intel job's validation scope are settled, not open review items.

### Job timeouts

Every job carries an explicit `timeout-minutes` (10 for the two lint jobs, 30–60 for the build
jobs). Without one a wedged job runs to **GitHub's 6-hour default** while holding its slot in the
`concurrency` group, and because `release.yml` reuses this workflow whole, it would hold a release
for that long too. The ceiling is real rather than theoretical: `scripts/run-pluginval.sh` passes
`--timeout-ms 600000`, so `macos-intel`'s twelve pluginval passes have a two-hour worst case on
their own. Each value is roughly double the measured runtime, which leaves room for a cold cache and
a slow runner while still failing inside the hour.

## Pipeline (per job)

1. **Checkout** (`actions/checkout@v7`), then — on every Ninja job — **restore the compiler cache**
   (`actions/cache@v6`; see "The compiler cache" above).
2. **Configure** — `cmake -B build [-G Ninja] -DCMAKE_BUILD_TYPE=Release
   -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
   -DANAMORPH_BUILD_NUMBER=${{ github.run_number }}` (the run number becomes the About-box build
   number). The launcher flags are absent on Windows alone. Windows uses the default VS generator; macOS adds
   `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`, and
   `macos-intel` the same deployment target with `-DCMAKE_OSX_ARCHITECTURES=x86_64` plus
   `-DANAMORPH_BUILD_STANDALONE=OFF` (that target recompiles the identical translation units and
   nothing in that job validates it — the same reason `codeql.yml` and `msvc.yml` turn it off).
3. **Build** — `cmake --build build --config Release`.
4. **DSP + state self-tests** — `scripts/run-tests.sh` runs `AnamorphTests` **and**
   `AnamorphStateTests` fail-closed (Linux/macOS); on Windows the step locates and runs both
   `AnamorphTests.exe` and `AnamorphStateTests.exe`, propagating the first failing exit code.
   Discovery is fail-closed on **ambiguity** as well as absence, on all three platforms: exactly one
   match is required, and both binaries are located before either runs. The previous
   `find … | head -n1` / `Select-Object -First 1` took whichever path enumerated first, so a
   multi-config tree (which is what Windows always has) or a stale second build tree could gate on a
   *Debug* binary while the uploaded artifacts came from Release — a green report about the wrong
   build. macOS then repeats the step for the **x86_64 slice under Rosetta** (see above), and
   `macos-intel` runs the same script a third time — unprefixed, because there the binaries are
   native.
5. **Symbol handling (RH-PR-2, ADR-0021)** — Linux extracts split debug info (`objcopy
   --only-keep-debug`), strips the shipped binaries (`strip --strip-unneeded`; `.gnu_debuglink`
   embedded) and asserts `GetPluginFactory` is still exported — ordered **before** pluginval so
   the gate validates the stripped bytes. The debuglink stores the **bare basename**, and always did:
   `objcopy` records only the basename of the `--add-gnu-debuglink` argument, so
   `--add-gnu-debuglink="dist/Anamorph-Linux-debug/…"` and the current form — run from inside the
   debug directory — produce a byte-identical `.gnu_debuglink` section. A debugger resolves that
   name relative to the *stripped binary's own* directory, then `.debug/`, then the global debug
   dir, so a user must place the downloaded `.debug` file next to its binary either way.
   macOS runs `dsymutil` → `strip -x` → ad-hoc codesign
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
   on **macOS** it is `package`, on Windows it is `build`, and on **`macos-intel`** it is `thin` for
   the VST3 steps and `au_install` for the AU steps — in each case the step that produces the bytes
   being validated. (`macos-intel` names two because its AU is validated from an installed copy
   rather than from the build tree; `thin` carries no `if:` of its own, so naming it subsumes that
   job's build the way `strip` does on Linux.) Without it a compile error let the gates run against a tree with no
   plug-in and the job's last error was a cascade rather than the cause.
   **A failing DSP/state self-test does NOT skip pluginval, and that is the decision, not a side
   effect.** The producer step is `strip`/`build`/`package`, none of which depends on `tests`, so a
   red self-test leaves the binary intact and the format-conformance gates still report. One run
   therefore yields the whole picture — a behavioural regression and a conformance regression are
   different defects and are worth knowing about together — at the cost of two extra pluginval runs
   after a genuinely broken build. The customer uploads are unaffected: they gate on `tests`, so a
   failed self-test still blocks the artifact. **Reviewed and confirmed by the maintainer
   (2026-08-16); treat it as settled rather than as an open review item.**
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
macOS runs it **twice over**, once per format (VST3 and AU), and then **again on native Intel**
(`macos-intel`, both formats, both modes): a fourth job running half a gate would have made the
heading above false, and `randomise` draws a fresh seed per run, so on Intel it pushes values
through Intel-generated code on an Intel FPU — an (architecture × value) space neither the Intel
deterministic run nor the arm64 randomise run reaches. Linux runs headless under `xvfb`. The
`--randomise` mode randomises test order to surface order-dependent defects a fixed-seed run can
miss; the fixed seed (nonzero — see step 6) seeds the RNG the tests themselves draw from, so the two
flags are independent rather than two spellings of the same thing.
Evidence [Verified]: `.github/workflows/build.yml`.

### The Clang warning baseline

The Clang gate asserts **no new** first-party warnings, not **zero**. The tree carries 17 distinct
first-party Clang warning sites:

| Count | Flag | Path |
|---|---|---|
| 1 | `-Wfloat-equal` | `src/dsp/ChorusEngine.cpp` |
| 1 | `-Wfloat-equal` | `src/dsp/HaasProcessor.cpp` |
| 3 | `-Wfloat-equal` | `src/dsp/VelvetNoise.cpp` |
| 4 | `-Wmissing-prototypes` | `tests/state_tests.cpp` |
| 1 | `-Wshadow` | `src/PluginProcessor.cpp` |
| 1 | `-Wshadow-field` | `src/PluginEditor.h` |
| 2 | `-Wsign-conversion` | `src/dsp/ScopeBuffer.h` |
| 3 | `-Wswitch-enum` | `src/dsp/AnamorphEngine.cpp` |
| 1 | `-Wunused-but-set-variable` | `tests/dsp_tests.cpp` |

Fourteen of them are older than the job. **Three are not, and they were added deliberately in 0.9.5**
— the A7-9 fixpoint gates in the three DSP modules (`ChorusEngine`, `HaasProcessor`, and the third
`VelvetNoise` site). Each is an exact `==` between two floats, which is precisely what the gate they
implement asks: *can this glide still move?* An epsilon there would be the defect, not the fix — and
`VelvetNoise` has carried the same idiom for its density glide since long before the gate existed,
which is what the two pre-existing entries on that file are. `docs/architecture/PERFORMANCE_BUDGET.md`
(the A7-9 entry) carries the reasoning; that is what "read the diff, and the diff is the review"
means in practice for this file.

Clearing the rest means renaming a member across the editor, adding cases to engine switches and
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
`ANAMORPH_CLANG_VERSION` in `build.yml` is the single authority for the major; each consuming job installs
`clang-<n>`/`lld-<n>` and configures with `clang-<n>`/`clang++-<n>`; `sanitizers` uses the same major,
which also lets it name `libclang-rt-<n>-dev` directly instead of scraping `clang --version` for it.
The baseline records the major in a `# clang-major:` line and `--clang-major` **refuses to run** when
the two disagree — exit 2, the code meaning *the check* could not run, not the 1 meaning the tree
regressed. An unrecorded version is refused for the same reason a wrong one is: it cannot be
confirmed to describe this compiler. Bump the pin and re-baseline in the **same** change.

**The pin is 22 — upstream stable — and it comes from apt.llvm.org.** The rule is *upstream stable*,
not *newest* and not *whatever Ubuntu packages*. Ubuntu's own archives stop at `clang-20` for noble,
which is what `ubuntu-latest` resolves to, so the toolchain is installed by
`scripts/setup-llvm-apt.sh` from **apt.llvm.org** — upstream's own Debian/Ubuntu channel. Ubuntu's
packaging boundary is a fact about Ubuntu's release process, not about this project, and it is not
allowed to hold the warning gate, the sanitizer host and — since ADR-0030 — the shipped Linux compiler
majors behind upstream. ADR-0028 carries the rule and the options it rejected (including the
intermediate 20 step and its mistaken reading of 21/22 availability).

**"Upstream stable" is now ASSERTED, and 23 is what forced that.** apt.llvm.org publishes rolling
BRANCH builds, so a `-N` suite can serve a commit from before N's release under a version string that
already reads like the release. `setup-llvm-apt.sh` therefore carries an upstream **release identity**
per major — the full version plus the `llvmorg-<version>` tag's commit — refuses a major it has no
identity for (exit 2), and reads the build commit from **four** sources — `clang --version` and the
installed version of each of the three packages. Every source carrying a commit must agree, at least
one must carry one, and it must be that tag's; anything else is exit 1. A compiler whose `--version`
omits the commit is therefore still verifiable from package metadata, and one for which **no** source
carries it fails **closed** rather than passing on a version string. `--self-test` proves that decision
function in `source-lint`. noble-22 passes because it is built from `ca7933e47d3a`, which **is**
`llvmorg-22.1.8^{}`.
**23 does not**: LLVM 23.1.0 released 2026-08-25 (tag commit `ea7d852a`), and on 2026-08-30 every
apt.llvm.org `-23` suite — noble, resolute, bookworm, trixie — still carried
the same commit `55feb0a3b6b7` (noble's package is
`1:23.1.0~++20260818083557+55feb0a3b6b7`; the rest differ only in build timestamp), built 2026-08-18 —
**49 commits before** that
tag, while no Ubuntu series publishes `clang-23` at all. **ADR-0033** carries the measurement, the
deferral and the single condition that lifts it.

### The GCC warning baseline

A second warning gate looks redundant next to the first, because
`juce_recommended_warning_flags` picks its set by **compiler ID** and Clang's set is strictly the
larger one. It is not redundant, because *larger* is not *a superset*. GCC reports two classes Clang
structurally does not:

| Count | Flag | Path |
|---|---|---|
| 1 | `-Wshadow` | `src/PluginParameters.cpp` |
| 1 | `-Wshadow` | `src/PluginProcessor.cpp` |
| 1 | `-Wmisleading-indentation` | `src/dsp/AnamorphEngine.cpp` |

Clang's `-Wshadow-all` does **not** report a parameter or local shadowing a member outside a
constructor, and Clang has no equivalent of `-Wmisleading-indentation` at all. (`src/PluginProcessor.cpp`
appears in *both* baselines under `-Wshadow`, for **different** sites — the counts are per `(path, flag)`,
not per line, so that one shared filename is a coincidence rather than duplicated coverage.) All three
sites are benign today and the gate does not ask for them to be fixed; it exists so the **next** one
fails the push that introduces it, on the cheapest runner in the matrix.

**The gated set is deliberately narrow, and the two exclusions are the interesting part.**
`-Wshadow`, `-Wmisleading-indentation`, `-Wduplicated-cond`, `-Wduplicated-branches` and
`-Wlogical-op` are in. The last three produce **zero** first-party hits today and are gated precisely
because of that: they cost nothing now and guard classes only GCC can see. Two are out:

- **`-Wnull-dereference`** produced four first-party hits, all of the "potential null pointer
  dereference" kind GCC emits after inlining when it cannot prove a branch unreachable. A four-entry
  baseline of unprovable warnings is the shape that trains people to regenerate a baseline without
  reading it — and the baseline diff *is* the review. It is also the one flag in the candidate set
  whose answer would depend on link-time inlining decisions in an `-flto` job.
- **`-Wmismatched-new-delete`** is a false positive **by construction** here.
  `tests/AllocationGuard.h` replaces the global `operator new`/`delete`; GCC attributes an
  allocation to the replaced `operator new[]` and does not follow it through to the `std::malloc`
  that actually produced the memory, so it reports `free` on "new[] memory" however the deallocators
  forward among themselves — verified **both** ways, by funnelling every deallocation through
  `::operator delete` and by calling `std::free` directly.

The flags are **not restated in the workflow**: the configure step asks the script for them
(`check-gcc-warnings.py --print-flags`), because a flag present in the build but unknown to the gate
is a warning nobody counts, and a flag known to the gate but absent from the build is a baseline
entry that can never be reproduced. None of the five affects codegen, so the LTO objects that job
exists to test are byte-identical with and without them, and all five are **front-end** diagnostics,
so `-flto` neither hides nor invents any of them.

**The comparison, the file format and the failure semantics are the Clang gate's**, restated rather
than imported so an edit aimed at one cannot break the other: structural path classification (resolve
the path, reject anything under `_deps`, require `src/` or `tests/`), `(path, flag)` keys with a
distinct-site count and no line numbers, a falling count as a `::notice::` and never a failure, an
unparseable baseline as exit 2, and `scripts/gcc-warning-baseline.txt` as a debt list rather than a
permission list.

**The compiler is pinned, and so is the image.** `-Wmisleading-indentation`'s heuristic and
`-Wshadow`'s treatment of members have both moved between GCC majors, so the counts mean nothing
against another one; the baseline records `# gcc-major:` and the gate exits 2 rather than 1 when the
two disagree. The *supply* differs from Clang's, though, and that decides the
mechanism. Clang comes from apt.llvm.org because upstream packages it there; the GNU project ships
only source, leaving packaging to distributions. **No package source ships a released GCC 16 for any
runner-available Ubuntu**: noble stops at `g++-14`; `ubuntu-toolchain-r/test` carries only a trunk
snapshot (`16-20260315`) and Ubuntu 26.04 the same class (`16-20260322`), both predating the 16.1
release; stable 16.2.0 exists in `apt` only for an unreleased Ubuntu series. Those snapshots are the
"newest, not stable" that ADR-0028 rejected for Clang 23, so the mechanism is the **official upstream
toolchain image** on `linux-lto-tests`.

**The tag floats on the major, and that is deliberate.** `gcc:16` resolves to the newest 16.x the
image publishes, so patch releases arrive without a commit here. This is the one weak pin in the
file, and it is weak because GCC is now a *compatibility checker* rather than the shipping toolchain
— a checker wants the newest 16.x automatically; a shipping toolchain must not. Nothing downstream
needs the patch: `gcc-warning-baseline.txt` records the **major**, and the job asserts the major
(`g++ -dumpversion`) rather than a full version, so 16.2 → 16.3 is silent and a 17 would be loud.

What the move cost the baseline was **measured, not assumed**, with clang-20 kept as the control: the
same three targets built from one tree under 20 and 22 emit a `diff`-identical **52-instance** warning
census, and `--write-baseline` at 22 reproduces all **7 entries / 14 sites** unchanged — so the only
line that moved in `scripts/clang-warning-baseline.txt` is `# clang-major:`. The same census also
matches Clang 18's, so this tree's accepted set has now been stable across three majors. Both suites
pass under the clang-22 build (140 and 894 checks) and again under its sanitizer build; the LTO
`Anamorph_VST3` link and `check_linker_flag`'s lld probe are green at 22
(`clang++-22 -fuse-ld=lld` resolves to `/usr/lib/llvm-22/bin/ld.lld`, LLD 22.1.8); and
`--compile-canary` still rejects the explicit `SIMDRegister` form.

**`-fsanitize=vptr` is now named explicitly, and that is coverage preserved rather than added.** Clang
21 removed `vptr` from the `-fsanitize=undefined` group, so the bare `address,undefined` the
`sanitizers` job used to carry would have silently stopped checking bad downcasts and bad vtables the
moment the pin passed 20. Reproduced rather than taken from a release note: one bad-downcast program,
`-fsanitize=undefined`, clang-20 reports *"downcast of address … which does not point to an object of
type 'B'"*, clang-22 reports **nothing**, and `-fsanitize=undefined,vptr` restores it on 22. It needs
RTTI, which this project never disables — so it is named on the **C++ compile flags only**. On a C
translation unit it is dead (a C TU emits the same `__ubsan` reference count with and without it), and
this job compiles 19 of them from the C sources JUCE vendors; clang-22 accepts it there silently, but
the driver already hard-errors on `vptr` + `-fno-rtti` even in C mode, and this job fails closed at
configure time rather than degrading.

The costs, stated so they are not discovered later. A **third-party apt source** is now in the two
Clang jobs. Its trust surface is narrowed three ways rather than merely acknowledged: the signing key is
fetched over HTTPS **and pinned by identity** — the keyring must hold *exactly one* primary key and it
must be `6084F3CF814B57C1CF12EFD515CF4D18AF4F7421` (*Sylvestre Ledru — Debian LLVM packages*), because
`signed-by=` trusts every key in the file — so a rotated, substituted or appended key fails the job with
a specific message rather than being trusted silently; `signed-by=` scopes that key to this one suite; and the
install is **fail-closed** — if apt.llvm.org is unreachable those two jobs fail saying so, while the
three *shipping* build jobs never touch it. The install is 15 packages / 155 MB / **17.8 s** measured on
a 4-core box, against 14 / 113 MB / 10.9 s for clang-20 from the stock archive and a no-op for the
preinstalled clang-18.

**Reproducibility is weaker than the stock archive, and that is the real trade.** apt.llvm.org
publishes *branch builds*, and the version string alone does not say which commit — noble-22 is
`1:22.1.8~++20260714014902+ca7933e47d3a-…` and the leading `~` makes it sort *below* a hypothetical
`1:22.1.8-1`. Suites are rebuilt while their branch is open and freeze once it closes, and the pool
keeps **only the current `.deb` per architecture** — which is also why an exact *package-version* pin
is not merely unenforceable here but impossible: it would stop resolving the next time the suite is
rebuilt. What IS pinnable is the compiler's own identity, and since ADR-0033 that is what the install
asserts: `ca7933e47d3a` is `llvmorg-22.1.8^{}`, so this suite serves the **release commit**, not merely
a build of its branch. **22.x is closed** (22.1.8 is upstream's newest 22 tag), and the mirror shows
it: noble-22's index has not moved since 2026-07-30. So the pin rests on a frozen suite carrying a
released compiler, and a rebuild from any other commit fails the install rather than passing silently.
One gain, too: apt.llvm.org publishes noble-22 for `amd64 arm64 s390x`, so a future
`ubuntu-24.04-arm` Clang job could install it — `clang-20` from the stock archive (amd64/i386 only)
could not.

**If you re-check apt.llvm.org and it seems to disagree, trust upstream's release page over
apt.llvm.org's own bookkeeping — and trust the PACKAGE over both.** On 2026-08-30 the site's homepage
still called 23 the development branch and `llvm.sh` still read `CURRENT_LLVM_STABLE=22`, five days
after 23.1.0 shipped, with no `llvm-toolchain-noble-24` suite published. Both lag, so llvm.org's
*Latest LLVM Release* banner and the per-release documentation at `releases.llvm.org/<version>/` decide
what upstream stable IS — the same resolution ADR-0028 reached when those two apt.llvm.org proxies
disagreed with each other. But "a release exists" and "this mirror serves it" are different questions,
and ADR-0033 is the round that learned to ask the second one: the `.deb`'s embedded build commit is the
only source that answers it, and it is now asserted at install time.

**The one upstream default worth naming, because it is a silent one.** Clang ≥ 20 turns on distinct
TBAA tags for incompatible pointers by default, which upstream says "may silently change code behavior
for code containing strict-aliasing violations" (`-fno-pointer-tbaa` disables it). *When this was
written* the job was not a shipping compiler — the Linux artefact was GCC's — but it was a *detector*,
so a codegen change here was worth having looked at rather than assumed away. **Since ADR-0030 the
clause no longer holds and the concern is larger, not smaller: this compiler ships the Linux
artefact.** The measurement below stands as taken: both suites pass under the clang-22 Release build
(140 + 894) and again under its ASan+UBSan+vptr build with `halt_on_error=1`, and the diagnostic set
did not move. The ABI changes in 20, 21 and 22 (Itanium construction-vtable mangling; larger records
returned in memory; the MSVC-ABI destructor change) cannot reach this pipeline: every job builds its
whole tree, JUCE included, from source with one compiler, so there is no mixed-major link anywhere, and
the MSVC-ABI item belongs to a compiler this project does not use on Windows.

**The baseline held still because the tree trips none of the new checks, not because the compiler
stood still.** Clang 21 and 22 add 51 and 26 new warning flags respectively and escalate three
diagnostics to error-by-default (chained comparisons and comparison fold-expressions in 21,
`-Wincompatible-pointer-types` in 22). The build log was checked for the ones most likely to reach
JUCE-style code and none appears: `-Wunnecessary-virtual-specifier`, `-Wcharacter-conversion`,
`-Wexperimental-lifetime-safety`, and the new default-on `-Wgcc-install-dir-libstdcxx` — that last one
is worth naming because it fires on images carrying several GCC toolchains, which this one does
(12/13/14). One loss is real but forward-looking: Clang 22 **removes
`-Wperf-constraint-implies-noexcept` from `-Wall`**, which cannot fire while nothing here is annotated
`[[clang::nonblocking]]`, and must be enabled explicitly if that ever changes.

**The standard library does not move with the pin.** Both clang-20 and clang-22 select the same
libstdc++ on this image (`Selected GCC installation: …/13`), so a Clang major bump changes no C++23
*library* surface here; that follows the runner's GCC, not `ANAMORPH_CLANG_VERSION`.

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
pointed at in the base — the whole "an edit above shifted it" class. On its own it cannot tell you an
anchor was aimed at the wrong code to begin with, so anchors outside the set below are *adopted*,
not audited, and a clean run means none of them **moved**.

**Since 2026-08-21 that hole is closed for the anchors that say what they point at.** A citation
written in this repository's own convention carries the symbol beside the line number —
`` src/PluginProcessor.cpp:183-193 (`updateLatency`) `` — and the checker now reads that gloss and
asserts the token is in the cited lines. It needs no base revision, because it is not a question
about drift: it asks whether an anchor lands on what its own document says it lands on, in the tree
as it is now. Exactly two gloss shapes are claimed — one backticked identifier, or one double-quoted
source string, alone in the parentheses — so prose like `(24 Hz timer)` asserts nothing rather than
having an assertion invented for it.

It is **opt-in per document** (`GLOSS_CHECKED_DOCS`), and the list names documents rather than
anchors so it holds no line numbers and cannot itself go stale. Eight architecture documents are in
it, carrying 43 glossed citations. Measured when it was written (seven documents then): 20 glossed
citations across them, **5 firing, all 5 genuine
defects, 0 false positives** — anchors that were fully qualified, parsed, and green at 342/342 while
pointing at unrelated code (`ScopedNoDenormals` cited 10 lines early, `isBusesLayoutSupported` 53,
`applyAutoGain` 53, `updateLatency` 10, and a Vectorscope sentence cited at 18-20 that lives on 21).
Repo-wide the same extraction fires 10 times, 9 genuine and 1 false, which is why it is opt-in.
`--fix` deliberately does **not** repair one: it re-anchors by the line map, which would carry a
wrong aim to a new line and change nothing about the aim.

`docs/architecture/SIGNAL_FLOW.md` **joined the list on 2026-08-22**, and how it got there is the
argument for the whole mechanism. It carried two qualified anchors and **33 bare ones** that no run
had ever seen — in the document that records DSP signal order, a `CLAUDE.md` hard-stop class. Sixteen
of them sat inside an ASCII diagram, where a 26-character path would have destroyed the column
alignment the diagram exists for; so the line numbers moved **out of the diagram** into a stage table
beside it and came back path-qualified. The diagram kept the order and the symbol — what it is for,
and what does not rot. The gate now sees **40 anchors there where it saw 2**, 23 of them glossed and
content-checked.

**`--fix` now reports the declarations it invalidates** (2026-08-18). A `DELIBERATE_REAIMS` entry is
a claim about a *spelling*, and a re-anchor can quietly falsify it: the anchor an entry names drifts
for an unrelated reason — an edit to the **cited** file — `--fix` re-anchors it correctly, and the
entry is left naming a string the document no longer contains, excusing nothing. Section 9 of the
self-test already fails on that, and that gate holds; what it could not do is tell the person who
caused it. It runs in CI, minutes later, in a different job, and knows only that an entry is dead —
while `--fix`, which killed it, is holding the replacement spelling. It now prints that spelling as
a `::warning::` at the moment of the rewrite. Observed twice in one change set (edits to
`run-pluginval.sh` and `CMakeLists.txt` moved anchors six entries named), and verified live
end-to-end: shifting `run-pluginval.sh` by one line produced
`update it to scripts/run-pluginval.sh:122`. A warning rather than an error, because `--fix`'s job
is to repair drift and refusing to do it because a declaration will need an edit would leave **both**
problems in place.

**When you re-anchor deliberately** — moving an anchor onto the code it should always have named —
the tool cannot distinguish that from drift, and the gate goes red on the commit that *fixed* it.
A line whose CONTENT changes on its own schedule is a different case and has its own table:
`VERSIONED_LINES` covers `CMakeLists.txt:14`, the `project(... VERSION ...)` line, whose text
changes at every release while the anchor never moves. `DELIBERATE_REAIMS` cannot express
that — it excuses a changed SPELLING, and `is_declared_reaim` returns false when the
spelling is unchanged. **That refusal alone did not stop an entry outliving its transition,
and until 2026-08-30 this paragraph said it did.** The table was keyed on ONE spelling matched
against either side of the change, so once the base caught up, every later movement of that anchor
arrived as `declared → something new` and was excused by a declaration written for a transition that
had already merged — silently, when the declared span was wide enough that the aim-check still found
its token. The key is now the **transition**, `(document, base anchor, current anchor)`, so a
declaration authorises one movement and stops matching the moment either end differs. For a `VERSIONED_LINES`
entry the base comparison is replaced by a permanent token check
(`verify_versioned_lines()`, a hard failure in every mode), it is keyed by one exact
`(path, line)` pair, and it applies only while the anchor has not moved. Both check paths -- the
paired one and the count-mismatch fallback reached when a document changes how many times it cites a
file -- go through one shared `anchor_still_right()`, because when they each carried their own copy
only one of them had the substitution, and a version bump landing beside an added citation was
reported as drift. Added for the
0.9.5 release, which was the first version bump after `CMakeLists.txt` came under the gate
and which the gate blocked; `worklogs/performance/PERF_AUDIT_v0.9.5_IMPLEMENTATION.md` §4a.

Declare the pair in `DELIBERATE_REAIMS` in the **same change set** as the re-anchor, never in a
follow-up. The list is expected to return to empty: an entry stops matching once the
base carries the corrected spelling, and the next run reports it as removable — act on that only
when the base you ran against is the branch's **merge base**, which is the base that still needed it.
`--fix` re-anchors mechanically; anchors it reports as `UNMAPPABLE` (the cited lines were themselves
edited) need a human. Prose *examples* of a citation must use a path outside `TRACKED`, or they get
re-anchored too.

**Rewriting a tracked file is the other case that needs a declaration, and it is the one that bites
after a merge.** When a change set rewrites a cited source *and* re-anchors the citations into it,
every one of those anchors is `UNMAPPABLE` against the merge base — the text they named is gone, so
no line number satisfies the same-text test and `--fix` cannot repair it. Against the branch's
previous push the same anchors look clean, because that base already carries the rewrite, so the
gate is green on the branch and red on the first default-branch build after the merge. Declare them
in the same change set. The v0.9.4 round's six entries are that case, and the block in
`scripts/check-citations.py` records which region each one names.

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
| `Anamorph-macOS-debug` | `Anamorph.vst3.dSYM`, `Anamorph.component.dSYM`, `Anamorph.app.dSYM` — **produced on every run since 2026-08-18**. It used to be absent from every run: Release+LTO left the DWARF in ld64's temporary object, gone before `dsymutil`, so the validated-dSYM condition never held. `-Wl,-object_path_lto` retains that object; zero usable dSYMs is now an **error**, and each dSYM is still individually validated (DWARF payload, UUID match across slices, ≥1 compile unit) before it is kept | error |

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
- **Native Intel now runs, but not on the shipped bytes.** This limit used to read "no native Intel
  macOS runner"; `macos-intel` closed it, and what is left is narrower and worth stating exactly.
  Two things are covered and they are not the same thing: the `macos` job executes the **shipped**
  x86_64 slice under Rosetta 2 (translated, on arm64 hardware), and `macos-intel` executes a
  **separately compiled** thin x86_64 build on a real Intel CPU. Neither is "the shipped slice on an
  Intel CPU" — that would need the universal artifact carried to an Intel runner, which nothing here
  does. The residual gap is therefore a *toolchain* one, not an ISA one: the two builds come from
  different macOS images and different AppleClang majors, so an Intel defect that only the
  cross-compiler emits would be seen by neither. Both jobs are blocking, so the ISA half is gated;
  closing the last of it means uploading and revalidating the universal bundle on the Intel runner,
  which is a further step and not a substitute for either of these.
- **The Windows staging self-check is a delete-confirmation, not a property check.** It re-lists the
  debug extensions the step just deleted, so it can only fire if `Remove-Item` silently failed.
  Linux inspects ELF section headers and asserts the export; macOS asserts both slices. Windows
  asserts nothing about the shipped `.vst3` being loadable. The honest way to close it is to read
  the PE export table — the staging step already parses PE headers for the CodeView record. Do **not**
  substitute a raw byte-string search for `GetPluginFactory`: matching the name anywhere in the file
  is not evidence that it is exported.
- **`--skip-gui-tests` on Windows** skips one test category on one runner (KI-007, environmental).
- **The Clang warning gate is not `-Werror`** — see §Build matrix for why it cannot be yet, and
  [The Clang warning baseline](#the-clang-warning-baseline) for the 17 sites it currently accepts.
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
| `.github/workflows/codeql.yml` | CodeQL: `c-cpp` (manual build — VST3 + tests targets, Standalone off) + `actions`. Alerts filtered to repo-own code (`paths-ignore: build` excludes the FetchContent'd JUCE tree). Default query suite. Uploads to Code Scanning **and** keeps the raw SARIF as an artifact. | push/PR to `main` (docs-only changes skipped), weekly, dispatch |
| `.github/workflows/msvc.yml` | MSVC `/analyze` (NativeRecommendedRules) → SARIF upload. Build step required (juceaide-generated files); JUCE under `build/_deps` treated as external. Uploads to Code Scanning **and** keeps the raw SARIF as an artifact. | push/PR to `main` path-filtered to `src/`, `tests/`, `CMakeLists.txt`; weekly; dispatch |
| `.github/workflows/dependency-review.yml` | Dependency Review on PRs (GitHub Actions deps only — the graph does not index CMake FetchContent). Comments only on failure. | PR to `main` |
| `.github/dependabot.yml` | Weekly `github-actions` version bumps in **two groups split by semver impact** — minor/patch in one PR (most of the volume: the `github/codeql-action` trio releases every week or two, and since every ref became a SHA pin the `actions/*` point releases land here too), majors in another, so one major cannot block every safe bump behind it. Both groups keep `patterns: "*"`, which is what holds a multi-ref family (`codeql-action/{init,analyze,upload-sarif}` — three dependency names) together. `microsoft/msvc-code-analysis-action` is **ignored**: its SHA pin carries no tag, and an untagged pin is followed to the latest *commit*, not the latest release. `cooldown` is unset — Dependabot already withholds a new version for 3 days by default. Nothing else in this repository is a Dependabot ecosystem; `DEPENDENCY_POLICY.md` §Update mechanisms says what maintains each of the rest. | weekly |

Both analysis workflows configure with `-DANAMORPH_BUILD_STANDALONE=OFF`: the Standalone format
recompiles the same translation units as VST3, so analyzing it doubles cost for zero extra
coverage. Evidence [Verified]: the four files above.

### Raw scanner SARIF artifacts

Both scanners also publish their SARIF as Actions artifacts — `codeql-sarif-<language>-<sha>` and
`prefast-sarif-<sha>`. The Code Scanning alert and check-run annotation APIs are not reachable from
every audit context; Actions artifacts are. The artifact is strictly richer than the dashboard for
CodeQL: `paths-ignore: build` filters the fetched JUCE tree out of the ALERTS, but those results
remain in the raw SARIF. Note the name carries `github.sha`, which on a `pull_request` event is the
merge commit, not the head commit. Neither analysis runs a second time — each new step reads the
SARIF the scanner had already written.

**The raw scanner SARIF is kept on the same `!cancelled()` principle** the artifact gating in
§Pipeline uses — a report Code Scanning REJECTS is exactly when the raw SARIF is most worth having,
and a success gate would discard it precisely then. The two scanners need different conditions
because they are shaped differently: `msvc.yml` produces and uploads in SEPARATE steps, so its
artifact gates exactly on `steps.run-analysis.outcome == 'success'` and keeps
`if-no-files-found: error` unconditionally. `codeql.yml`'s `analyze` does BOTH, so its own outcome
cannot separate "no SARIF" from "SARIF written, upload refused": it gates on `outcome != 'skipped'`
and downgrades `if-no-files-found` to `warn` when the analysis itself failed — deliberately
avoiding the trap §Pipeline step 7 describes against the `-debug` uploads, where a second failure
on a missing file buries the real error. `warn` never publishes an empty artifact; upload-artifact
skips the upload when nothing matches.

Evidence [Verified]: `.github/workflows/codeql.yml`, `.github/workflows/msvc.yml`.

### The Linux ABI floor

The `linux` job asserts, on the **stripped** binaries and as its **last step**, that the shipped VST3
and Standalone stay within a declared glibc/libstdc++ floor. This is a compatibility claim the
pipeline previously did not make: a Linux binary records the oldest version providing each imported
symbol, and the maximum of those is the oldest system that can load it — below it the dynamic
loader refuses before any of this project's code runs.

Nothing chose that number. It is whatever `ubuntu-latest` shipped when the binaries were linked, and
a runner-image move raises it **silently and retroactively** — the artifact stops loading on systems
it loaded on last week, with no failure in CI and no line in any diff. Measured when the gate landed:
`GLIBC_2.38` and `GLIBCXX_3.4.31`, i.e. Ubuntu 23.10+ and GCC 13+, so the artifact does **not** load
on Ubuntu 22.04 LTS. That was not a decision; it was an image move nobody saw.

The floor gained a third family, `CXXABI`, while GCC 16 was being evaluated for the Linux build
(2026-08-21). `GLIBC` and `GLIBCXX` did not move under it — the GCC 16.2 artifact still asked for
`GLIBC_2.38` and `GLIBCXX_3.4.31` — but its exception path pulled
`__cxa_call_terminate@CXXABI_1.3.15`, which libstdc++ first shipped in **GCC 14**. Reading `GLIBCXX`
alone, the gate would have reported a GCC 13 floor and passed an artifact needing a GCC 14 runtime.
A family the gate does not name cannot raise the floor loudly, so it raises it silently — and that is
the one failure mode this gate exists to prevent.

**The declared value is GCC 13's, `CXXABI_1.3.14`**, so the three families describe one runtime
between them rather than two. The shipped artifact is Clang's and needs only `CXXABI_1.3.9`,
comfortably under; the supported floor therefore stays **Ubuntu 23.10 / Debian 13**, unchanged from
before. A future move back to a compiler that emits `1.3.15` now *fails* this gate instead of passing
it, which is the point of declaring the family at all.

The floor lives in `scripts/check-linux-abi.py` and `COMPATIBILITY_MATRIX.md` defers to it rather
than restating a number. The gate does not attempt to *lower* the floor — that means an older
toolchain or a sysroot, a release-topology decision rather than a CI tweak — it makes the run that
raises it the run that fails, so raising it becomes deliberate and reviewable.

### Action refs are pinned to commit SHAs

Every `uses:` in every workflow names a **commit SHA**, with the version it corresponds to in a
trailing comment (2026-08-18). Before that, the `actions/*` refs were bare majors — `@v7`, `@v6` —
which are **mutable tags**: at the time of the change `actions/checkout@v7` and `v7.0.1` resolved to
the same commit, and nothing but the tag owner's restraint kept them that way.

The argument for pinning is not generic supply-chain hygiene, it is an **internal inconsistency**.
This repository already pins JUCE to an immutable commit SHA, and `DEPENDENCY_POLICY.md` gives the
reason in as many words: so the dependency cannot silently change under a re-pointed tag. JUCE is
source that gets compiled and never sees a credential. An action is code that executes **on the
runner with the job's token**. Pinning the weaker of the two and not the stronger was the gap.

**The cost is real and is accepted deliberately.** A bare major is rewritten only when the major
moves, so Dependabot's volume here used to be the `github/codeql-action` trio and essentially
nothing else. A SHA pin is rewritten on **every** release, so `checkout`, `cache`, `upload-artifact`
and `download-artifact` now produce updates too. The two update-type groups already in
`.github/dependabot.yml` are what absorbs that: more dependencies move, but they move together
within their semver class, so it is still at most two PRs a week. When reviewing one, **read the
version comment, not just the SHA** — the comment is the only human-legible half, and a bump that
changes the SHA without changing the comment is the shape to stop on.

`microsoft/msvc-code-analysis-action` was already SHA-pinned and stays excluded from Dependabot, for
the separate reason recorded in that file: its pin is *ahead* of the last release, so following it
would swap a documented deliberate pin for an untagged upstream HEAD.

### One composite action for the Linux setup

Seven Linux jobs opened with the same three moves — `chmod +x scripts/*.sh`, `setup-linux.sh`, then
a ccache install behind a fallback that must not fail the job. Four of them added one line for the
pinned Clang, two added a package, and the ccache block itself was **byte-for-byte identical in six
of the seven, comment included**.

That is a correctness hazard rather than untidiness. The ccache fallback is a *policy* — "an
optimization, never a requirement" — and a policy written out seven times is a policy that can hold
in six places. The round that introduced `.github/actions/setup-linux-build` is itself the worked
example: it had to add a compiler pin to exactly one of the seven copies.

The action takes two inputs (`clang-version`, `extra-packages`), both fail-closed, because a job
asks for those because it cannot work without them. What it deliberately does **not** absorb is the
per-job ccache **lineage** — the `actions/cache` key — which stays in `build.yml` because that is
the part genuinely different in every job and whose reasoning is *about* that job: which compiler
produced the objects, which build directory, and which other job it may share entries with. Folding
those into an input would turn seven readable explanations into one parameter nobody can read. The
two macOS jobs also keep their own block: `brew` and `apt` differ enough that a shared action would
be a conditional pretending to be a step. A `./`-prefixed local action is this repository rather
than a dependency, so Dependabot ignores it by design.

## Reproducing CI locally

The lint jobs need no toolchain and no build, so run them first — they are the ones that cost
seconds and catch the most:

```bash
python3 scripts/check-docs.py --self-test && python3 scripts/check-docs.py
python3 scripts/check-portability.py --self-test && python3 scripts/check-portability.py
python3 scripts/check-realtime.py --self-test && python3 scripts/check-realtime.py
python3 scripts/check-citations.py --self-test
python3 scripts/check-citations.py --check --base origin/main   # --fix re-anchors
python3 scripts/check-clang-warnings.py --self-test              # gate needs a clang build log
python3 scripts/check-gcc-warnings.py --self-test                # gate needs a gcc build log
python3 scripts/check-linux-abi.py --self-test                   # gate needs linked artifacts
```

`check-citations.py` compares against **a** base, and which one matters: CI uses the previous push,
so a local run against `origin/main` can reach a different verdict — and on a branch with more than
one commit it routinely does, because an anchor that drifted from an *earlier commit on the branch*
has already been re-anchored relative to `main` (a differing citation *count* for
a document makes the tool fall back to ordinal pairing, which only judges base spellings still
present verbatim). Check **both** before concluding the gate is green.

**`scripts/preflight.sh`** (added 2026-08-18) runs the whole lint block above in one command — all
**seven** checkers with their self-tests, the citation gate against **all three** bases that can
disagree — `origin/main`, the branch merge base, and `HEAD~1`, the **push predecessor** CI actually
compares (added 2026-08-18 after it cost a red run: three anchors drifted from an earlier commit on
the same branch, both `origin/main` bases already carried the re-aimed spelling, and preflight went
green while `source-lint` did not) — the ABI floor for real when a local Release build is present
(the one of the three build-dependent gates whose input an ordinary local build produces),
then `scripts/run-tests.sh` when a built tree exists at `./build` (skipped WITH A NOTE when
none does — never silently). Measured ~5 s on a built tree. It says out loud the one thing it
cannot cover: the full Clang warning gate needs a clang build log, so only that lint's self-test
runs locally.

**Read its exit status, not a filtered view of its output.** The script is `set -euo pipefail`, so
the FIRST failing checker ends the run and everything after it never executes — a non-zero exit is
not "one finding in an otherwise green preflight", it is an *unknown* result for every later stage.
Piping the run through `grep` substitutes grep's status for the script's and can swallow a finding
whose wording the pattern did not anticipate. Recorded because it cost a red run (round 21,
2026-09-02): `check-docs` — preflight's **second** command — was failing on a `DOCUMENTATION_COVERAGE`
line that began with a `|` character, the filtered view showed nothing, the round reported a green
preflight, and the `docs` job went red on the push.

Then the build and the release gate:

```bash
scripts/setup-linux.sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_NUMBER=0
cmake --build build --config Release
scripts/run-tests.sh
scripts/run-pluginval.sh 10 deterministic     # 10 = ANAMORPH_PLUGINVAL_STRICTNESS in build.yml
scripts/run-pluginval.sh 10 randomise         # --randomise x3 (the state-restoration gate)
```

The compiler cache is **not** part of that reproduction and `setup-linux.sh` does not install it:
it is a CI-time optimization, and a local tree already gets incremental rebuilds from Ninja. To
reproduce the CI build exactly, add what the workflow adds — `apt-get install -y ccache`, then
`-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache` on the `cmake -B` line.
Note that a cache built at one build-directory name will not be hit at another (JUCE lives inside
the build directory, so its path is in every `-I`), which is why CI's directory names are fixed.

`scripts/run-pluginval.sh` takes an optional third argument, the format: `vst3` (default) or `au`.
`au` is macOS-only and **errors** on any other host rather than skipping silently; on macOS install
the `.component` first (or point `ANAMORPH_PLUGINVAL_BUNDLE` at an installed one), because the
AudioComponent registry only finds bundles under a Components directory.

`macos-intel` is reproducible only **on an Intel Mac** — on Apple Silicon the same commands compile
and then run under Rosetta, which is the coverage the job exists to go beyond, so a local "pass"
there proves the wrong thing. Check first, then use the job's own configure line:

```bash
uname -m                                   # must be x86_64
sysctl -n sysctl.proc_translated           # must be 0 (or absent) -- 1 means Rosetta
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 \
      -DANAMORPH_BUILD_STANDALONE=OFF -DANAMORPH_BUILD_NUMBER=0
cmake --build build --config Release
lipo -archs build/Anamorph_artefacts/Release/VST3/Anamorph.vst3/Contents/MacOS/Anamorph  # x86_64
scripts/run-tests.sh                       # unprefixed: the binaries are native here
```

The `sanitizers`, `realtime` and `fuzz` jobs use their own build trees so they never collide with the one
above — `build-clang`, `build-san`, `build-vg`. All are covered by `.gitignore`'s `build*/`.

```bash
CLANG=22   # ANAMORPH_CLANG_VERSION in .github/workflows/build.yml is the authority
scripts/setup-llvm-apt.sh "$CLANG"   # Ubuntu has no clang-22 for noble; this is how CI gets it
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
