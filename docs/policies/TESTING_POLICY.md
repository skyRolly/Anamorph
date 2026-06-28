# TESTING_POLICY.md

Repository Governance Policy. Test acceptance levels and the release gate.

## Acceptance levels

| Level | Name | What | Where |
|---|---|---|---|
| **1** | Static analysis | Compiler warnings (recommended warning flags), CodeQL | `juce::juce_recommended_warning_flags` (CMakeLists.txt:143,165); GitHub code scanning |
| **2** | Unit / behaviour | Deterministic DSP assertions | `tests/dsp_tests.cpp` (24 tests) |
| **3** | DSP validation | MS round-trip exact; no NaN/Inf/denormals across the algorithm × OS × feature matrix; latency==actual; bypass null; click-free transitions | `tests/dsp_tests.cpp` |
| **4** | pluginval | VST3 conformance; editor open/close under `xvfb` | `scripts/run-pluginval.sh` |
| **5** | Manual validation | Audio sound quality + GUI/OpenGL visual appearance (cannot be judged headlessly) | Load `.vst3` in a DAW |

## Hard release gate

- **Level 2/3 DSP self-tests must pass** (the headless gate, `scripts/run-tests.sh`).
- **pluginval must pass at strictness 10** on the **Linux** authoritative gate (`run-pluginval.sh 10`).
  Windows/macOS pluginval is informational (`continue-on-error`) so a flaky GUI test on those
  runners never blocks tester artifacts.
- Level 5 is **required for final sign-off** but cannot gate CI; a green build + pluginval pass is
  "ready to audition," not "shipped."

Evidence [Verified]: scripts/run-tests.sh; scripts/run-pluginval.sh:10-13,57-76; .github/workflows/build.yml:38-42,79-86,131-137.

## Rules

1. **Every bug fix ships a regression test** that fails on the old code and passes on the fix
   (the project's established practice — e.g. the 0.8.7 Solo+Multiband click test).
2. **DSP-policy invariants must have a guarding test** where feasible (see the invariant→test map
   in `DSP_POLICY.md`).
3. The pluginval **signal-only retry** is permitted (it works around a host-side JUCE/X11 crash,
   not a plugin defect) but never retries a real validation failure (`run-pluginval.sh:46-76`).
