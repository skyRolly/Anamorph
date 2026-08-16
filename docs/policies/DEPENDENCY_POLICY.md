# DEPENDENCY_POLICY.md

Repository Governance Policy. Third-party dependency locking and upgrade safety.

## Current dependencies

| Dependency | Pin | Mechanism | Evidence |
|---|---|---|---|
| **JUCE** | **9.0.1**, pinned by **immutable commit SHA** `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` | CMake `FetchContent` (`GIT_SHALLOW`), overridable via `-DANAMORPH_JUCE_PATH` | CMakeLists.txt:36-38,47-55 |
| **pluginval** | latest release (download) | `scripts/run-pluginval.sh` | scripts/run-pluginval.sh:119-126 |
| **C++ standard** | C++23 | `CMAKE_CXX_STANDARD 23`, extensions off (ADR-0027) | CMakeLists.txt:16-18 |
| Linux system libs | distro packages | `scripts/setup-linux.sh` (ALSA, JACK, X11, FreeType, GTK/WebKit, mesa, **EGL — required by JUCE 9's Linux GL context path**, xvfb) | setup-linux.sh |

## Version-lock reasoning

- **JUCE is pinned to an exact IMMUTABLE commit** (`e18f7f5…` = tag 9.0.1; `ANAMORPH_JUCE_VERSION`
  carries the human-readable version), not a branch, `latest`, or a mutable tag *name* — since the
  v0.8.13 cycle the SHA pin also protects against an upstream re-pointed tag (ADR-0022). JUCE is
  the framework for the entire DSP (oversampling, Linkwitz-Riley filters, `dsp::AudioBlock`),
  parameter system (APVTS), GUI, and plugin-format wrappers — an unpinned bump can silently change
  DSP behaviour, latency, the editor/X11 embedding path (the 0.8.5 incident lives in JUCE's X11
  host code), and the parameter/state ABI. The pin makes builds reproducible and keeps the audited
  behaviour stable. Evidence [Verified]: CMakeLists.txt:36-38,47-55; the X11 dependency is
  documented in ADR-0011.

## Upgrade rules

1. A JUCE version bump is a **Build System change** → `ARCHITECTURE_REVIEW_GATE.md` + an ADR.
2. After any bump: full DSP self-tests + pluginval at the configured strictness
   (`ANAMORPH_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml`) in **both modes**
   (deterministic and `--randomise` ×3) on all three OSes, **and** a manual audition (Level 5) — a
   JUCE change can move DSP/latency/editor behaviour invisibly to the headless gate.
3. Re-verify the `RELEASE_COMPATIBILITY_CHECKLIST.md` (latency reporting, session reload) after a bump.
4. Prefer the offline path (`-DANAMORPH_JUCE_PATH`) for reproducibility in restricted CI.
5. `JUCE_*` compile flags in `CMakeLists.txt:257-262` (no webview, no curl, no splash, strict
   ref-counted pointer) are part of the dependency contract; changing them is a build change.

## Compliance log

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
