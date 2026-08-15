# C++ standard migration 17 → 23 (v0.9.4 cycle)

Controlled language-standard migration: `CMAKE_CXX_STANDARD` **17 → 23**, applied on top of the
finished v0.9.4 tree (JUCE 9.0.1) **without advancing the version number**. No feature work, no
redesign. A C++-standard change is part of the build contract, so it is a **Build System change**
→ Architecture-Review-Gate item; recorded in **ADR-0027** and flagged on the PR.

The migration was **adopted, not fallen back from**: the fallback to C++20 written into the task
was not triggered. The single incompatibility found across the three shipped toolchains was one
missing standard include, which is an ordinary compatibility adjustment.

## 1. Toolchain audit (Phase 1 — before any modification)

| Platform (CI job) | Compiler | Standard library | CMake maps `CXX_STANDARD 23` to |
|---|---|---|---|
| Linux (`ubuntu-latest`) | GCC 13.3 | libstdc++ 13 | `-std=c++23` (`GNU-CXX.cmake`, GCC ≥ 11) |
| macOS (`macos-14`) | AppleClang 15.4 | macOS SDK libc++ | `-std=c++2b` (`AppleClang-CXX.cmake`, ≥ 13.0) |
| Windows (`windows-latest`) | MSVC (Visual Studio generator, auto-detected) | MSVC STL | `/std:c++latest` (`MSVC-CXX.cmake`, ≥ 19.29.30129) |

* **CMake floor unchanged**: `cxx_std_23` exists since CMake 3.20, below the project's
  `cmake_minimum_required(VERSION 3.22)` — `CMakeLists.txt:1` needed no edit.
* **JUCE imposes no ceiling**: every module declares `target_compile_features(... cxx_std_17)`
  as an INTERFACE **minimum** (`JUCEModuleSupport.cmake:340,595`), so a higher project standard
  wins and JUCE's own helper targets (`juceaide`, `juce_linux_subprocess_helper`,
  `juce_lv2_helper`) compile at the project standard too.
* **`CMAKE_CXX_EXTENSIONS OFF` retained** — the migration moves the standard level only, never to
  a `gnu++` dialect.
* **Windows caveat, recorded rather than glossed over.** MSVC has **no stable `/std:c++23`**; it
  offers `/std:c++23preview` (VS 17.13+, documented as "may change and may not be ABI compatible
  across releases") and `/std:c++latest`, and CMake — on master as well as the version in use —
  maps `CXX_STANDARD 23` to **`/std:c++latest`**, which Microsoft documents as enabling
  "some in-progress and experimental features" that are "subject to breaking changes or removal
  without notice". See ADR-0027 §Consequences: this is a real property of the Windows
  configuration, not a defect, and it does not block adoption — but it is the reason the Windows
  language mode is not frozen the way the JUCE pin and the numerics-affecting flags are.
  Provenance of the flag: read from CMake's `MSVC-CXX.cmake` mapping, not scraped from the CI
  log (the Visual Studio generator does not echo compile command lines at default verbosity).
  That the mapping fired at all is empirical: `CMAKE_CXX_STANDARD_REQUIRED ON` makes configure
  fail closed when the compiler has no option for the requested standard, and the Windows
  configure + build succeeded.

## 2. The one incompatibility, and its fix

```
src/dsp/HaasProcessor.cpp:28:5: error: no matching function for call to 'fill'
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    ^~~~~~~~~
.../usr/include/c++/v1/__bit_reference:425:1: note: candidate template ignored:
    could not match '__bit_iterator' against '__wrap_iter'
```

**Mechanism.** `HaasProcessor.cpp` included `<cmath>` and got `std::fill` transitively. libc++
removes the transitive `<algorithm>` include once `_LIBCPP_STD_VER >= 20`, so under `-std=c++2b`
the only `fill` still visible is the `std::vector<bool>` `__bit_iterator` overload declared in
`<bit_reference>` — hence "no matching function", not "undeclared identifier".

**Fix.** `#include <algorithm>` in `src/dsp/HaasProcessor.cpp`, matching the include set
`ChorusEngine.cpp` and `VelvetNoise.cpp` already carry for the same call. One line, no behaviour
change.

**Why only this file.** GCC/libstdc++ and the MSVC STL still provide the transitive include, so
the defect is invisible on two of the three platforms — it was found by an actual macOS build,
not by inspection. A `-fsyntax-only` sweep of **all 27 project translation-unit compilations**
(the `AnamorphTests` + `AnamorphStateTests` command sets, which between them cover every `src/`
and `tests/` source) under Clang 18 + libc++ + `-std=c++23` reports **zero** errors after the
fix, so no second file depends on a dropped transitive include.

A wider include-hygiene pass was deliberately **not** done: files that compile correctly under
C++23 on all three toolchains are out of scope for this change.

## 3. Changes applied

| File | Change |
|---|---|
| `CMakeLists.txt:16` | `set(CMAKE_CXX_STANDARD 17)` → `23`. Single-line, in place — `:17-18` (`STANDARD_REQUIRED`/`EXTENSIONS`) and every downstream `CMakeLists.txt:NNN` doc citation keep their line numbers. |
| `src/dsp/HaasProcessor.cpp:3` | `#include <algorithm>` added (§2). |

No other source, script, workflow or CMake line changed. The project version stays **0.9.4** and
the release date stays **2026-08-15**, as commissioned.

## 4. Validation

### 4.1 Builds

| Configuration | Result |
|---|---|
| GCC 13.3 / libstdc++ / `-std=c++23`, Release, shipped flags | **green**, 0 errors |
| Clang 18 / **libc++** / `-std=c++23`, Release, shipped flags (macOS proxy) | **green**, 0 errors |
| CI `linux` — `ubuntu-latest`, GCC 13, VST3 + Standalone + tests | **green** |
| CI `windows` — `windows-latest`, MSVC `/std:c++latest`, VST3 + Standalone + tests | **green** |
| CI `macos` — `macos-14`, AppleClang 15.4, universal arm64 + x86_64, VST3 + AU + Standalone + tests | **green** (failed before the §2 fix — that failure is the evidence the fix was needed) |

libc++ was linked deliberately: AppleClang uses libc++, and libc++ is where the C++20/23
transitive-include removal bites. The Linux GCC build alone would have declared the migration
clean and shipped the macOS break.

### 4.2 Self-tests (Level 2/3 hard release gate)

* `AnamorphTests` — **140 checks, 0 failures**
* `AnamorphStateTests` — **894 checks, 0 failures**, including the parameter-registry snapshot
  **frozen under JUCE 8.0.14**, which passes unchanged → the parameter surface and the
  serialization schema are identical under C++23.

Both suites green in all three of: GCC/libstdc++ C++23, Clang/libc++ C++23, and the CI matrix.
The counts equal the C++17 baseline exactly (140 / 894).

### 4.3 Engine bit-identity (C++17 vs C++23)

The ADR-0022/0026 twin-dump harness, re-pointed from "two JUCE trees" to **two language
standards**: the 8 `AnamorphDSP` sources plus the deterministic scenario driver compiled twice
against the **same** JUCE 9.0.1 checkout with the same shipped hardening flags, differing only in
`-std=c++17` vs `-std=c++23`. 32 scenarios (Haas/Velvet/Chorus/Dim-D × OS Off/2x/4x/8x × M/S
on/off; 120 noise + 120 silence blocks each at 48 kHz / 512), FNV-1a over every output byte.

**Result: all 32 hashes and all 32 predicted/reported latency pairs identical.** The 32 hashes
are mutually distinct, so the matrix discriminates rather than collapsing to a constant.

This is the evidence that matters for `DSP_POLICY` and `LATENCY_MODEL`: raising the language
standard changed neither the audio output nor the reported latency by a single byte.

### 4.4 Compiler diagnostics (Level 1 gate)

All 27 project translation-unit compilations re-run with the shipped flags at `-std=c++17` and at
`-std=c++23`, real `-O3` codegen (not syntax-only, so back-end diagnostics are covered):
**29 warning instances each, sets identical**. C++23 introduces no new diagnostic — the baseline
is the pre-existing `-Wsign-conversion` / `-Wshadow` / `-Wswitch-enum` / `-Wfloat-equal` /
`-Wmisleading-indentation` / `-Woverloaded-virtual` set.

### 4.5 pluginval (Level 4 gate)

Strictness **10**, both modes, 3 consecutive passes each — deterministic and `--randomise` —
green locally on the C++23 build, and green on all three blocking CI platform gates.

## 5. Remaining risks

1. **MSVC ships `/std:c++latest`, not a frozen C++23** (§1). Consequence: a Visual Studio update
   on `windows-latest` can change the Windows language/library mode with no repository change.
   Mitigations available if it ever bites: pin the Windows toolchain, or set `/std:c++20`
   for MSVC only. Neither is warranted today — the mode builds and passes the strictness-10 gate
   — and both are recorded in ADR-0027 §Consequences so the option is not lost. This closes when
   Microsoft ships a stable `/std:c++23`.
2. **Transitive-include reliance is a class, not an instance.** §2 fixed the one occurrence that
   any current toolchain rejects; further libc++ include-graph pruning in a future Xcode could
   expose another. The macOS CI job is the detector, and it is blocking.
3. **No Level-5 audition is required by this change.** The twin dump proves the engine is
   byte-identical and the editor sources are untouched; unlike a JUCE bump, no framework code
   changed underneath the GUI. (`DEPENDENCY_POLICY` rule 2 is a *JUCE-bump* rule.)

## 6. Compatibility summary

| Contract | Before | After |
|---|---|---|
| Language standard | C++17 | **C++23** (`CMAKE_CXX_EXTENSIONS OFF` retained) |
| CMake minimum | ≥ 3.22 | ≥ 3.22 (unchanged) |
| JUCE pin | 9.0.1 @ `e18f7f5…` | unchanged |
| macOS deployment target | 10.13 | unchanged |
| Linux system packages | `scripts/setup-linux.sh` | unchanged |
| DSP output / reported latency | — | **bit-identical** (§4.3) |
| Parameter surface / state schema | — | unchanged (registry snapshot passes, §4.2) |
| Project version / release date | 0.9.4 / 2026-08-15 | unchanged, as commissioned |
