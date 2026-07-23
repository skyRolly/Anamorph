# ADR-0022 — JUCE dependency upgrade 8.0.14 → 9.0.0 + immutable-commit pinning

**Status:** Proposed (implementation verified headlessly; pending the maintainer's
Architecture-Review sign-off and the Level-5 manual audition — see Consequences)

## Context
JUCE is pinned to an exact version (ADR-0003, ADR-0012, `docs/policies/DEPENDENCY_POLICY.md`);
any JUCE bump is a **Build System change requiring an ADR + verification**
(`docs/policies/ARCHITECTURE_REVIEW_GATE.md`). JUCE **9.0.0** was released upstream; the
commissioned v0.8.13 task is a controlled migration 8.0.14 → 9.0.0. Independently, the
post-v0.8.12 audit's supply-chain item flagged that pinning by **tag name** is mutable — an
upstream re-pointed tag would silently change our dependency.

## Problem
Move to JUCE 9.0.0 with **no change** to DSP output, reported latency, parameter semantics, or
serialization; keep the diff minimal; and make the pin immutable/reproducible.

## Options
- **A. Stay on 8.0.14.** Rejected for the commissioned migration — forgoes upstream maintenance
  across a major line; the longer the gap grows, the harder every future migration becomes.
- **B. Bump to the mutable tag `9.0.0`.** Rejected — repeats the supply-chain weakness the audit
  flagged (RISK-001 re-pointed-tag variant).
- **C. Bump pinned to the tag's commit SHA, with full verification.** Chosen.

## Decision
- `ANAMORPH_JUCE_TAG` → **`f8f8864172464b9adf9eba6101e1f784838d1597`** (the commit of upstream
  tag `9.0.0`, verified via `git ls-remote` + checkout log "JUCE version 9.0.0"); the new
  `ANAMORPH_JUCE_VERSION="9.0.0"` cache variable carries the human-readable version and both are
  printed by the configure banner (`CMakeLists.txt:36-38,47-55`). GitHub serves shallow
  fetch-by-SHA (verified), so `GIT_SHALLOW` is retained.
- `scripts/setup-linux.sh` adds **`libegl-dev`**: JUCE 9 creates Linux GL contexts via **EGL
  instead of GLX** (module metadata `linuxPackages: egl gl`), so EGL is a Linux build dependency
  even though Anamorph never attaches a GL context on Linux (ADR-0011 unchanged).
- **No C++ source change.** The complete 8.0.14 → 9.0.0 breaking-change surface
  (BREAKING_CHANGES.md: Windows multi-touch default-off, `Drawable::createFromSVG(XmlElement&)`
  removal, `DrawableShape` signatures, `Drawable` no longer a `Component`, Linux EGL) has **zero
  project exposure** — audit table in `worklogs/JUCE9_MIGRATION_v0.8.13.md` §1.1. Toolchain
  contract unchanged: CMake ≥ 3.22, **C++17**, macOS deployment 10.13 (upstream minimum 10.11).

## Verification (headless, this change)
- **DSP bit-identity proven, not assumed**: a 32-scenario twin engine dump (4 algorithms × 4
  oversampling factors × M/S on/off; 120 noise + 120 silence blocks each; FNV-1a over every
  output byte) built against 8.0.14 and 9.0.0 with identical flags produced **identical hashes
  and identical reported latencies for all 32 scenarios** (`worklogs/JUCE9_MIGRATION_v0.8.13.md`
  §3 — includes the juce_dsp Oversampling path, the one upstream component most able to change
  wet-path numerics).
- Suites at JUCE 9: **AnamorphTests 140 checks** and **AnamorphStateTests 774 checks** green;
  the parameter-registry snapshot fixture (frozen under 8.0.14) passes **unchanged** →
  parameter surface + serialization schema identical.
- pluginval strictness 10 both modes ×3 on the three CI platforms (blocking, unchanged gate).

## Consequences
- The version-lock rationale (ADR-0003/0012) is unchanged; the pin is now **immutable** — a
  re-pointed upstream tag can no longer alter the dependency (closes the RISK-001 sub-risk).
- An existing `build/` caches the old pin (`ANAMORPH_JUCE_TAG` is a CACHE var): after pulling a
  pin change, delete `build/` or `-U` the two variables (TROUBLESHOOTING row added). The
  configure banner prints `version (rev)` so a mismatch is visible.
- Licence: still dual AGPLv3/commercial, now under the **JUCE 9 EULA** wording — an owner-level
  business note, no build/code impact.
- **Open items (human-gated):** (1) Architecture-Review sign-off for this Build System change;
  (2) the **Level-5 manual audition** required by DEPENDENCY_POLICY rule 2 for any JUCE bump —
  the 9.0.0 build must be auditioned in a DAW against the 8.0.14 baseline (as done for
  ADR-0012). The twin dump covers engine numerics; it cannot cover editor/visual appearance
  (JUCE 9 reworked SVG/font/renderer internals) or host-integration feel. Until both are done
  this ADR stays **Proposed** and the migration is "ready to audition", not signed off.

## Related code
- `CMakeLists.txt:36-38` (pin), `:47-55` (FetchContent); `scripts/setup-linux.sh` (EGL).

Evidence:
- Source [Verified]: CMakeLists.txt:36-38 (SHA pin; `ANAMORPH_JUCE_VERSION`).
- Upstream [Verified]: `git ls-remote` tag `9.0.0` → `f8f8864…`; JUCE `BREAKING_CHANGES.md`
  ("Version 9.0.0" section) and `CHANGE_LIST.md` reviewed at that commit.
- Twin dump [Verified]: `worklogs/JUCE9_MIGRATION_v0.8.13.md` §3 (32/32 hashes + latencies
  identical; scratchpad tool, methodology recorded in the worklog).
- KI-013 re-check [Verified]: JUCE 9 `getNativeRealtimeModifiers` still refreshes keyboard
  modifiers only — the macOS caveat stands (KNOWN_ISSUES.md KI-013).
- Policy basis: `DEPENDENCY_POLICY.md` (Upgrade rules), `ARCHITECTURE_REVIEW_GATE.md`; history:
  ADR-0012 (8.0.8 → 8.0.14).
