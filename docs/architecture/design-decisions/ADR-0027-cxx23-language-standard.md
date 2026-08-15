# ADR-0027 — C++ language standard 17 → 23

**Status:** **Proposed** (Build System change — `ARCHITECTURE_REVIEW_GATE.md`; flagged on the PR
for human Architecture Review)

## Context
The C++ standard is part of the build contract: `DEPENDENCY_POLICY.md` lists it in the pinned
dependency table alongside JUCE and pluginval, and `ARCHITECTURE_REVIEW_GATE.md` classifies a
CMake/build-configuration change as a gated **Build System change**. The project has compiled at
`CMAKE_CXX_STANDARD 17` since its first commit; ADR-0012, ADR-0022 and ADR-0026 each recorded a
JUCE bump while explicitly leaving the C++17 contract untouched. The commissioned v0.9.4
follow-up task is the controlled migration to **C++23**, applied to the finished v0.9.4 tree
**without advancing the version number**.

## Problem
Raise the language standard with **no change** to DSP output, reported latency, parameter
semantics or serialization; keep the diff minimal; keep all three shipped platforms building and
passing the same blocking gates. Determine — from actual builds and tests, not from
inspection — whether C++23 is practically viable, and fall back to C++20 only if it presents
substantial compatibility or long-term maintenance problems that cannot reasonably be resolved.

## Options
- **A. Stay on C++17.** Rejected: the task commissions the migration, and nothing in the codebase
  or in JUCE 9 requires C++17 specifically (JUCE declares `cxx_std_17` as an INTERFACE
  **minimum**).
- **B. C++20.** The written fallback. Its one concrete advantage is Windows: `/std:c++20` is a
  released, ABI-stable MSVC mode, whereas C++23 reaches MSVC as `/std:c++latest`
  (§Consequences). Not taken — the fallback condition was not met, and the standard is not
  chosen for the weakest toolchain's flag naming when the strongest evidence available (three
  green platform builds, bit-identical engine output) says the higher standard works.
- **C. Per-platform standards** — C++23 on GCC/Clang, C++20 on MSVC. Rejected: it would compile
  the shipped binaries under two different language rules, permanently, which is a larger
  maintenance liability than the one it avoids and is contrary to the repository's
  "validate the shipped configuration" doctrine (ADR-0021).
- **D. C++23 uniformly, with the one incompatibility fixed.** Chosen.

## Decision
- `CMAKE_CXX_STANDARD` **17 → 23** (`CMakeLists.txt:16`). `CMAKE_CXX_STANDARD_REQUIRED ON` and
  `CMAKE_CXX_EXTENSIONS OFF` are retained unchanged (`:17-18`) — the migration moves the standard
  level only, never to a `gnu++` dialect. The edit is a single line in place, so every
  `CMakeLists.txt:NNN` citation elsewhere in the docs keeps its anchor.
- **One source change:** `#include <algorithm>` in `src/dsp/HaasProcessor.cpp`. libc++ drops the
  transitive `<algorithm>` include once `_LIBCPP_STD_VER >= 20`, so `HaasProcessor::reset()`'s
  `std::fill` lost its declaration under `-std=c++2b` and only the `<vector>` `__bit_iterator`
  overload stayed visible. The added include matches what `ChorusEngine.cpp` and
  `VelvetNoise.cpp` already carry for the same call. No behaviour change.
- **Nothing else.** No CMake floor change (`cxx_std_23` predates CMake 3.22), no toolchain
  version bump, no JUCE change, no test change, no version bump: the project stays **0.9.4**,
  released **2026-08-15**.

## Verification (headless, this change)
- **Three-platform builds, all green**: CI `linux` (GCC 13 / libstdc++), `windows`
  (MSVC `/std:c++latest`) and `macos` (AppleClang 15.4, universal arm64 + x86_64, VST3 + AU +
  Standalone). The macOS job **failed before** the `<algorithm>` fix and is green after it — the
  fix is evidenced by a real build, not asserted.
- **libc++ built locally on purpose**: a full Clang 18 + **libc++** + `-std=c++23` build, because
  AppleClang uses libc++ and a GCC-only check would have declared the migration clean while
  shipping the macOS break.
- **DSP bit-identity proven, not assumed**: the ADR-0022/0026 twin-dump harness re-pointed from
  two JUCE trees to **two language standards** — the 8 `AnamorphDSP` sources plus the
  deterministic driver compiled against the **same** JUCE 9.0.1 checkout with the same shipped
  flags, differing only in `-std=c++17` vs `-std=c++23`; 32 scenarios (Haas/Velvet/Chorus/Dim-D ×
  OS Off/2x/4x/8x × M/S on/off; 120 noise + 120 silence blocks each at 48 kHz/512), FNV-1a over
  every output byte — produced **identical hashes and identical predicted and reported latencies
  for all 32 scenarios**. The 32 hashes are mutually distinct, so the matrix discriminates.
- Suites under C++23: `AnamorphTests` **140 checks, 0 failures**; `AnamorphStateTests`
  **894 checks, 0 failures**, including the parameter-registry snapshot **frozen under 8.0.14**,
  which passes unchanged → parameter surface + serialization schema identical. Both green under
  GCC/libstdc++ **and** Clang/libc++, and on the CI matrix. Counts equal the C++17 baseline.
- **No new compiler warnings**: all 27 project translation-unit compilations re-run with the
  shipped flags at `-std=c++17` and `-std=c++23`, real `-O3` codegen — **29 instances each,
  sets identical** (the pre-existing `-Wsign-conversion`/`-Wshadow`/`-Wswitch-enum`/
  `-Wfloat-equal`/`-Wmisleading-indentation`/`-Woverloaded-virtual` baseline).
- **pluginval strictness 10**: deterministic ×3 and `--randomise` ×3, green locally on the C++23
  build and on all three blocking CI platform gates.

## Consequences
- **The fallback to C++20 was evaluated and not taken.** The only incompatibility across the
  three shipped toolchains was a single missing standard include — an ordinary compatibility
  adjustment, explicitly not a downgrade trigger.
- **MSVC gets `/std:c++latest`, not a frozen C++23, and this is a real long-term caveat.**
  Microsoft ships no stable `/std:c++23`: only `/std:c++23preview` (VS 17.13+, documented as
  "may change and may not be ABI compatible across releases") and `/std:c++latest`, which
  enables "some in-progress and experimental features" that are "subject to breaking changes or
  removal without notice". CMake maps `CXX_STANDARD 23` to `/std:c++latest` for MSVC ≥
  19.29.30129 (`Modules/Compiler/MSVC-CXX.cmake`, unchanged on master). Practical effect: a
  Visual Studio update on `windows-latest` can move the Windows language/library mode with no
  repository change — a wider drift surface than C++17/20 gave, on a floating runner the project
  already accepts. It is **not** blocking: the mode builds and passes the strictness-10 gate in
  both modes. If it ever bites, the two escape hatches are pinning the Windows toolchain or
  setting `/std:c++20` for MSVC only; this ADR records them so the option is not lost. The caveat
  closes when Microsoft ships a stable `/std:c++23`.
- **Building from source now needs a C++23 compiler** — the documented floor in `README.md` and
  `BUILD.md` moves from C++17. Users of the released binaries are unaffected.
- **Transitive-include reliance is now a live class of defect.** A future libc++ include-graph
  pruning could expose another file the way this one exposed `HaasProcessor.cpp`; the blocking
  macOS CI job is the detector.
- **No Level-5 audition is required.** `DEPENDENCY_POLICY` rule 2 is a JUCE-bump rule; here no
  framework code moved underneath the editor, the editor sources are untouched, and the engine is
  proven byte-identical.
- Unchanged: the JUCE pin and its version-lock rationale, the numerics-affecting-flag freeze
  (ADR-0021), the CMake minimum, the macOS deployment target, and `scripts/setup-linux.sh`.

## Related code
- `CMakeLists.txt:16-18` (standard, required, extensions off).
- `src/dsp/HaasProcessor.cpp:1-3` (include set), `:26-32` (`reset()`, the `std::fill` calls).

Evidence:
- Source [Verified]: CMakeLists.txt:16 (`set(CMAKE_CXX_STANDARD 23)`);
  src/dsp/HaasProcessor.cpp:3 (`#include <algorithm>`).
- Toolchain mapping [Verified]: CMake `Modules/Compiler/GNU-CXX.cmake` (`-std=c++23`),
  `AppleClang-CXX.cmake` (`-std=c++2b`, AppleClang ≥ 13.0), `MSVC-CXX.cmake`
  (`-std:c++latest`, MSVC ≥ 19.29.30129); Microsoft `/std` reference for the
  `/std:c++latest` and `/std:c++23preview` stability statements. JUCE's `cxx_std_17`
  INTERFACE minimum: `extras/Build/CMake/JUCEModuleSupport.cmake:340,595`.
- Build/test/twin-dump/warning evidence [Verified]: `worklogs/CXX23_MIGRATION_v0.9.4.md`
  §§2-4 (the macOS failure and its fix, 32/32 identical hashes + latencies, 140 + 894 checks,
  29/29 identical warning instances, pluginval both modes ×3).
- Policy basis: `ARCHITECTURE_REVIEW_GATE.md` (Build System change), `DEPENDENCY_POLICY.md`
  (pinned-dependency table), `TESTING_POLICY.md` (Levels 1-4 gate), `CODE_STYLE.md` (language
  line); history: ADR-0012, ADR-0022, ADR-0026 each left C++17 explicitly untouched.
