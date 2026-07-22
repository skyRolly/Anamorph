# DEPENDENCY_POLICY.md

Repository Governance Policy. Third-party dependency locking and upgrade safety.

## Current dependencies

| Dependency | Pin | Mechanism | Evidence |
|---|---|---|---|
| **JUCE** | tag **8.0.14** | CMake `FetchContent` (`GIT_SHALLOW`), overridable via `-DANAMORPH_JUCE_PATH` | CMakeLists.txt:33,38-51 |
| **pluginval** | latest release (download) | `scripts/run-pluginval.sh` | run-pluginval.sh:34 |
| **C++ standard** | C++17 | `CMAKE_CXX_STANDARD 17`, extensions off | CMakeLists.txt:16-18 |
| Linux system libs | distro packages | `scripts/setup-linux.sh` (ALSA, JACK, X11, FreeType, GTK/WebKit, mesa, xvfb) | setup-linux.sh:21-29 |

## Version-lock reasoning

- **JUCE is pinned to an exact tag (8.0.14)**, not a branch or `latest`. JUCE is the framework for
  the entire DSP (oversampling, Linkwitz-Riley filters, `dsp::AudioBlock`), parameter system
  (APVTS), GUI, and plugin-format wrappers — an unpinned bump can silently change DSP behaviour,
  latency, the editor/X11 embedding path (the 0.8.5 incident lives in JUCE's X11 host code), and
  the parameter/state ABI. The pin makes builds reproducible and keeps the audited behaviour
  stable. Evidence [Verified]: CMakeLists.txt:33,44-50; the X11 dependency is documented in
  ADR-0011.

## Upgrade rules

1. A JUCE version bump is a **Build System change** → `ARCHITECTURE_REVIEW_GATE.md` + an ADR.
2. After any bump: full DSP self-tests + pluginval strictness 10 in **both modes** (deterministic
   and `--randomise` ×3) on all three OSes, **and** a manual audition (Level 5) — a JUCE change can
   move DSP/latency/editor behaviour invisibly to the headless gate.
3. Re-verify the `RELEASE_COMPATIBILITY_CHECKLIST.md` (latency reporting, session reload) after a bump.
4. Prefer the offline path (`-DANAMORPH_JUCE_PATH`) for reproducibility in restricted CI.
5. `JUCE_*` compile flags in `CMakeLists.txt:183-188` (no webview, no curl, no splash, strict
   ref-counted pointer) are part of the dependency contract; changing them is a build change.

## Compliance log

- **JUCE 8.0.8 → 8.0.14** — recorded in **ADR-0012** (the first dependency bump enforced under rule 1
  above; the bootstrap use of this rule). Verified green by CI (build + the then-current 23 DSP self-tests + pluginval
  strictness 10 on the Linux gate); commit `41acaa7`. The manual audition (rule 2, Level 5) **was
  performed** post-CI by the maintainer — a DAW audition of 8.0.14 against the 8.0.8 baseline with no
  perceptual regressions (2026-06-29) — and is recorded in **ADR-0012** (*Manual Audition (Level 5)*).
  It is a human sign-off, not headlessly reproducible. The forward-looking risk for *future* bumps
  stays tracked by RISK-001.
