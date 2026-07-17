# TESTING_POLICY.md

Repository Governance Policy. Test acceptance levels and the release gate.

## Acceptance levels

| Level | Name | What | Where |
|---|---|---|---|
| **1** | Static analysis | Compiler warnings (recommended warning flags), CodeQL | `juce::juce_recommended_warning_flags` (CMakeLists.txt:143,165); GitHub code scanning |
| **2** | Unit / behaviour | Deterministic DSP assertions + state-restoration robustness | `tests/dsp_tests.cpp` (30 DSP tests + 1 A/B state-restoration clamp guard) |
| **3** | DSP validation | MS round-trip exact; no NaN/Inf/denormals across the algorithm × OS × feature matrix; latency==actual; bypass null; click-free transitions | `tests/dsp_tests.cpp` |
| **4** | pluginval | VST3 conformance; editor open/close under `xvfb` | `scripts/run-pluginval.sh` |
| **5** | Manual validation | Audio sound quality + GUI/OpenGL visual appearance (cannot be judged headlessly) | Load `.vst3` in a DAW |

## Hard release gate

- **Level 2/3 self-tests must pass** (the headless gate, `scripts/run-tests.sh`): the 30 DSP
  self-tests **and** the A/B state-restoration clamp guard.
- **pluginval must pass at strictness 10 in BOTH modes on ALL THREE platforms** (Linux, Windows,
  macOS), each mode run as **3 consecutive passes**: **deterministic** (`run-pluginval.sh 10
  deterministic`, fixed `--random-seed 0`) **and** **randomise** (`run-pluginval.sh 10 randomise` —
  `--randomise`). The randomise mode exercises state restoration under randomised test order +
  fuzzing that a fixed-seed run can miss. **All are blocking** — there is no `continue-on-error`; a
  non-zero pluginval exit fails the job on every platform (Windows uses `run-pluginval.ps1`).
- Level 5 is **required for final sign-off** but cannot gate CI; a green build + pluginval pass is
  "ready to audition," not "shipped."

Evidence [Verified]: scripts/run-tests.sh; scripts/run-pluginval.sh / scripts/run-pluginval.ps1
(mode handling + 3-pass loop + signal-only retry); .github/workflows/build.yml (uniform blocking gate).

## Rules

1. **Every bug fix ships a regression test** that fails on the old code and passes on the fix
   (the project's established practice — e.g. the 0.8.7 Solo+Multiband click test).
2. **DSP-policy invariants must have a guarding test** where feasible (see the invariant→test map
   in `DSP_POLICY.md`).
3. The pluginval **signal-only retry** is permitted (it works around a host-side JUCE/X11 crash,
   not a plugin defect) but never retries a real validation failure (`run-pluginval.sh:46-76`).
