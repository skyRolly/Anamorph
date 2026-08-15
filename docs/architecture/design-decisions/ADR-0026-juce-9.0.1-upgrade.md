# ADR-0026 — JUCE dependency upgrade 9.0.0 → 9.0.1

**Status:** **Accepted** (Architecture Review + the DEPENDENCY_POLICY rule-2 Level-5 manual
audition signed off 2026-08-15)

## Context
JUCE is pinned to an exact version, and any JUCE bump is a **Build System change requiring an
ADR + verification** (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`, `DEPENDENCY_POLICY.md`
rule 1). ADR-0012 recorded 8.0.8 → 8.0.14, ADR-0022 recorded 8.0.14 → 9.0.0 and replaced the
mutable tag-name pin with the tag's **immutable commit SHA**. JUCE **9.0.1** was released
upstream on 2026-08-10; the commissioned v0.9.4 task is the controlled migration to it.

## Problem
Move to JUCE 9.0.1 with **no change** to DSP output, reported latency, parameter semantics or
serialization; keep the diff minimal; keep the pin immutable.

## Options
- **A. Stay on 9.0.0.** Rejected for the commissioned upgrade — 9.0.1 is the maintenance release
  of the line we already ship, and deferring patch releases only makes each later migration
  larger (the argument ADR-0022 already made).
- **B. Bump to the mutable tag `9.0.1`.** Rejected — reintroduces the re-pointed-tag supply-chain
  weakness ADR-0022 closed.
- **C. Bump pinned to the tag's commit SHA, with the ADR-0022 verification repeated.** Chosen.

## Decision
- `ANAMORPH_JUCE_TAG` → **`e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8`** (the commit of upstream
  tag `9.0.1`, verified via `git ls-remote` **and** by the fetched tree's own checkout log
  "JUCE version 9.0.1"); `ANAMORPH_JUCE_VERSION` → **`9.0.1`**. Both are printed by the configure
  banner (`CMakeLists.txt:36-38,47-55`). GitHub still serves shallow fetch-by-SHA, so
  `GIT_SHALLOW` is retained.
- **No C++ source change, and no build-dependency change.** Neither 9.0.1 breaking change has
  project exposure, and no JUCE module Anamorph uses changed its declared `linuxPackages` /
  `OSXFrameworks` / `windowsLibs` metadata — only the `version:` field moved 9.0.0 → 9.0.1
  (audit table in `worklogs/JUCE901_UPGRADE_v0.9.4.md` §1). `scripts/setup-linux.sh` is
  therefore unchanged; `libegl-dev` (added by ADR-0022) remains the Linux GL requirement.
  Toolchain contract unchanged: CMake ≥ 3.22, **C++17**.

## Verification (headless, this change)
- **DSP bit-identity proven, not assumed**: the ADR-0022 twin-dump harness re-run — the 8
  `AnamorphDSP` sources plus a deterministic scenario driver compiled against **both** JUCE
  checkouts with identical flags, 32 scenarios (Haas/Velvet/Chorus/Dim-D × OS Off/2x/4x/8x ×
  M/S on/off; 120 noise + 120 silence blocks each at 48 kHz/512), FNV-1a over every output byte —
  produced **identical hashes and identical reported and predicted latencies for all 32
  scenarios**. The 32 hashes are mutually distinct, so the matrix discriminates.
- **The strongest structural evidence is upstream's own diff**: `juce_dsp`,
  `juce_audio_basics`, `juce_data_structures` and `juce_audio_plugin_client` changed **only their
  module-header `version:` string** between the two tags — zero code — so the DSP primitives, the
  ValueTree/APVTS backing and the VST3/AU/Standalone wrappers cannot have moved.
- Suites at JUCE 9.0.1: `AnamorphTests` **140 checks, 0 failures**; `AnamorphStateTests`
  **894 checks, 0 failures**, including the parameter-registry snapshot **frozen under 8.0.14**,
  which passes unchanged → parameter surface + serialization schema identical.
- **pluginval strictness 10, locally green**: deterministic ×3 and `--randomise` ×3 (one
  randomise pass took the script's signal-only retry — the documented host-side JUCE/X11 XEmbed
  flake, KI-003/KI-007; `run-pluginval.sh` retries only on a signal exit, never on a validation
  failure). The same gates run blocking on the three CI platforms.
- **No new compiler warnings**: all 18 Anamorph-owned translation units recompiled with the
  shipped flags against both trees produce a **byte-identical** diagnostic set once the JUCE
  path prefix is normalised (19 instances: the pre-existing `-Wsign-conversion`/`-Wshadow`/
  `-Wswitch-enum`/`-Woverloaded-virtual`/`-Wfloat-equal`/`-Wmisleading-indentation` baseline).
- **Third-party attribution re-verified** (`RELEASE_POLICY.md` requires this after any JUCE
  bump): JUCE's `LICENSE.md` and all twelve licence files behind
  `THIRD_PARTY_LICENSES.md` §2 are byte-identical between the two tags; the only licence files
  9.0.1 adds belong to the new WebBrowserComponent **TypeScript** package, which is not compiled
  into any Anamorph binary. The inventory is unchanged; only the pinned version/commit it cites
  moved.

## Consequences
- The version-lock rationale (ADR-0003/0012/0022) is unchanged and the pin stays immutable.
- The stale-cache trap ADR-0022 recorded still applies: `ANAMORPH_JUCE_TAG` is a CACHE variable,
  so an existing `build/` keeps the old pin after a pull — delete `build/` or `-U` the two
  variables (TROUBLESHOOTING row, already present).
- Licence terms are unchanged (dual AGPLv3 / commercial JUCE 9; `LICENSE.md` byte-identical).
- **The two human gates are closed.** Architecture Review signed off this Build System change,
  and the **Level-5 manual audition** required by DEPENDENCY_POLICY rule 2 was performed against
  the 9.0.1 build (2026-08-15) — the part no headless gate reaches, since the twin dump covers
  engine numerics but not editor **appearance or feel**, and 9.0.1 does change editor-adjacent
  framework code (Linux message-loop scheduling, Linux display enumeration and vblank period,
  Windows Direct2D edge painting, macOS Metal-layer guards). The same audition discharges the
  one ADR-0022 left open for the 9.0 line, which is now signed off against 9.0.1.

## Related code
- `CMakeLists.txt:33-38` (pin + banner comment), `:47-55` (FetchContent).

Evidence:
- Source [Verified]: CMakeLists.txt:36-38 (`ANAMORPH_JUCE_VERSION "9.0.1"`, SHA pin).
- Upstream [Verified]: `git ls-remote --tags` → `9.0.1` = `e18f7f5…`; the FetchContent'd tree at
  `build/_deps/juce-src` resolves to that SHA with subject "JUCE version 9.0.1"; JUCE
  `BREAKING_CHANGES.md` ("Version 9.0.1", 2 entries) and `CHANGE_LIST.md` reviewed at that commit.
- Twin dump [Verified]: `worklogs/JUCE901_UPGRADE_v0.9.4.md` §3 (32/32 hashes + latencies
  identical; scratchpad tool, methodology recorded in the worklog per the ADR-0022 precedent).
- Known-issue re-checks [Verified]: `getNativeRealtimeModifiers` (KI-013) and
  `LinuxComponentPeer::isActiveApplication` (KI-019) are byte-identical between 9.0.0 and 9.0.1 —
  neither known issue is fixed upstream, neither regresses.
- Policy basis: `DEPENDENCY_POLICY.md` (Upgrade rules), `ARCHITECTURE_REVIEW_GATE.md`; history:
  ADR-0012 (8.0.8 → 8.0.14), ADR-0022 (8.0.14 → 9.0.0 + SHA pin).
