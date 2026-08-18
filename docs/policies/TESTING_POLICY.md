# TESTING_POLICY.md

Repository Governance Policy. Test acceptance levels and the release gate.

## Acceptance levels

| Level | Name | What | Where |
|---|---|---|---|
| **1** | Static analysis | Compiler warnings (recommended warning flags), **gated for first-party sources under Clang — no NEW warnings above `scripts/clang-warning-baseline.txt`**; CodeQL; MSVC `/analyze`; the source-portability and documentation lints | `juce::juce_recommended_warning_flags` (CMakeLists.txt:297,323,363); `scripts/check-clang-warnings.py` in the `linux-clang` job; `scripts/check-portability.py`, `scripts/check-docs.py`, `scripts/check-citations.py`, `scripts/check-realtime.py` (audio-path bodies vs the `REALTIME_AUDIO_POLICY` forbidden list); GitHub code scanning |
| **1b** | Dynamic analysis | ASan + UBSan over both suites, then valgrind memcheck over both suites from an unsanitized build; **RealtimeSanitizer over the DSP suite** in its own lane (ADR-0029 — the driver forbids combining it with the others), behind a liveness canary, enforcing `REALTIME_AUDIO_POLICY` on the annotated audio entry point; the **allocation guard** compiled into the DSP suite (Test 38 — `operator new` + malloc-family counters armed only around `process()`, self-checked for liveness, and the tier that reaches MSVC where RTSan cannot); `MALLOC_PERTURB_=1` on the per-push Linux self-tests (glibc fills fresh heap with `0xFE`, freed heap with `0x01` — the value is complemented for allocations, so it is **not** the fill byte) | `sanitizers` and `realtime` jobs in `.github/workflows/build.yml` |
| **2** | Unit / behaviour | Deterministic DSP assertions + state/parameter compatibility (schema shape, registry snapshot, raw-exact round-trip, legacy migrations, corrupt-state robustness, preset round-trip) + the wrapper audio path (real `processBlock`, own-FTZ denormal guard) | `tests/dsp_tests.cpp` (37 DSP tests + 1 A/B clamp guard) + `tests/state_tests.cpp` (13 state-compatibility tests, `AnamorphStateTests`); both suites additionally execute LTO-built in the `linux-lto-tests` job, so the assertions also run against shipped-class codegen |
| **3** | DSP validation | MS round-trip exact; no NaN/Inf/denormals across the algorithm × OS × feature matrix; latency==actual; bypass null; click-free transitions | `tests/dsp_tests.cpp` |
| **4** | pluginval | **VST3 conformance on all three platforms, AU conformance on macOS** — and on macOS both formats are gated twice, once on Apple Silicon and once on **native Intel**; editor open/close under `xvfb` | `scripts/run-pluginval.sh <strictness> <mode> [vst3\|au]` |
| **5** | Manual validation | Audio sound quality + GUI/OpenGL visual appearance (cannot be judged headlessly) | Load `.vst3` in a DAW |

## Hard release gate

- **Level 2/3 self-tests must pass** (the headless gate, `scripts/run-tests.sh`): the 37 DSP
  self-tests, the A/B state-restoration clamp guard, **and** the 13-test state-compatibility
  suite (`AnamorphStateTests` — both binaries are required; a missing one fails the gate, and an
  *ambiguous* one does too: exactly one match is required per binary, so a multi-config or stale
  build tree cannot let the gate report on a different configuration than the one just built).
  On macOS the suites run **three times**, and the three are not interchangeable: natively on
  **arm64**; for the shipped universal binary's **x86_64 slice under Rosetta 2** (translated, still
  executing on arm64 hardware); and on a **native Intel runner**, where a thin x86_64 build is
  executed by an actual Intel CPU. The product ships a universal binary and the packaging runner is
  Apple Silicon, so without the third the Intel half of the user base would be gated only by a
  translation layer — and the no-denormal invariant in particular depends on the hardware flush bits
  (MXCSR on x86_64, FPCR on arm64), not on the source.
- **pluginval must pass in BOTH modes on ALL THREE platforms** (Linux, Windows, macOS) and, on
  macOS, **for BOTH formats** (VST3 **and** AU — the AU is the only format Logic and GarageBand
  load, and it exists on exactly one platform) **on both macOS execution environments** (the
  Apple Silicon job and the native Intel job — same strictness, same two modes, same three passes
  in each; an architecture is not gated by a job that only compiles for it). Each mode runs as
  **3 consecutive passes**:
  **deterministic** (`run-pluginval.sh <n> deterministic`) **and** **randomise**
  (`run-pluginval.sh <n> randomise` — `--randomise`). The randomise mode exercises state
  restoration under randomised test order that a fixed-seed run can miss; the deterministic mode
  pins the seed the tests themselves draw from, and that seed is **nonzero** — pluginval treats
  `--random-seed 0` as "generate a random one", so the previous `0` made this mode neither
  deterministic nor reproducible. **All are blocking** — there is no `continue-on-error`; a
  non-zero pluginval exit fails the job on every platform (Windows uses `run-pluginval.ps1`).
  **This policy states no strictness number.** `ANAMORPH_PLUGINVAL_STRICTNESS` in
  `.github/workflows/build.yml` is the single authority for the value, and
  `docs/procedures/CI_CD.md` describes how the pipeline is wired; a number restated here is the
  copy that goes stale on the next raise. Lowering it is a deliberate act to be justified in the PR.
- Level 5 is **required for final sign-off** but cannot gate CI; a green build + pluginval pass is
  "ready to audition," not "shipped."

Evidence [Verified]: scripts/run-tests.sh; scripts/run-pluginval.sh / scripts/run-pluginval.ps1
(mode handling + seed + 3-pass loop + signal-only retry); .github/workflows/build.yml (uniform
blocking gate; `env.ANAMORPH_PLUGINVAL_STRICTNESS`; the macOS AU install + AU gates).

## Rules

1. **Every bug fix ships a regression test** that fails on the old code and passes on the fix
   (the project's established practice — e.g. the 0.8.7 Solo+Multiband click test).

   **Exception (ADR-0025), narrow and disclosure-bound.** A fix may ship without one **only** when
   no reliable automated test can be written because **the repository has no stable automated
   surface that reaches the defect** — GUI/component lifetime, host-owned UI behaviour, or
   OS-level asynchronous behaviour. Difficulty, slowness or inconvenience do **not** qualify: if a
   surface exists, or a reasonable extension of one would reach it, this rule applies unchanged.
   Invoking the exception requires **all four** of: (a) why no reliable test exists, citing the
   surface and the specific reason it cannot reach the defect; (b) what manual or structural
   verification replaced it; (c) where the coverage gap is tracked — the register is
   `docs/procedures/TESTING.md` §"Gaps in the automated coverage"; (d) whether future test
   infrastructure could close the gap, named concretely if so. An exception lapses when the surface
   appears, and its register entry is then revisited rather than left standing.
   This changes nothing about the **release gate** above: Levels 2, 3 and pluginval remain blocking.
2. **DSP-policy invariants must have a guarding test** where feasible (see the invariant→test map
   in `DSP_POLICY.md`).
3. The pluginval **signal-only retry** is permitted (it works around a host-side JUCE/X11 crash,
   not a plugin defect) but never retries a real validation failure
   (`run_one_pass`, `scripts/run-pluginval.sh:154-176`).
4. **A checker must prove it is live before its silence is trusted.** Every lint in the pipeline
   ships a `--self-test` that runs in the same job, immediately before the check itself —
   `check-docs.py`, `check-clang-warnings.py`, `check-portability.py`, `check-citations.py` and
   `check-realtime.py`, all five. A checker that has stopped matching anything is indistinguishable from a clean tree, and a
   gate that cannot fail is indistinguishable from a gate that passes. Adding a lint without one is
   not adding a gate.

   **What a self-test must do.** Run the checker's own functions over synthetic input, in **both**
   directions: every "must fire" case is a defect the lint exists to catch, every "must stay silent"
   case is valid input an over-eager revision flagged. Depend on nothing the job does not already
   have — no build tree, no base revision, no network — because a self-verification that needs the
   thing it is verifying cannot run beside it. The measure of one is whether a broken checker fails
   it: each of the four is written against the defects its checker has actually shipped, and is
   maintained by adding the next one.

   **A premise check is not a self-test, and the two are not interchangeable.**
   `check-portability.py --compile-canary` compiles two translation units against the pinned JUCE to
   assert the hazard the lint guards still exists — a question about the *dependency*, answerable
   only where JUCE is checked out, so it runs in `linux-clang` rather than beside the lint. Both are
   required and neither substitutes: a green canary with a dead scanner reports a clean tree, and a
   green self-test with a moved hazard guards something that is no longer there.
