# HANDOVER.md

Operational status snapshot for technical handover. Update on every release
(`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`). Facts are Verified from the repository;
fields with no repository evidence are marked `TODO` rather than invented (constraint C7).

Snapshot taken at HEAD `c605fbe` (state-restoration + CI-gate hardening; JUCE 8.0.14).

## Operational status

| Field | Value |
|---|---|
| **Current Version** | 0.8.9 (`CMakeLists.txt:14`; version bumped for the functional/UX-fix PR #56; **released** with PR #58, which folds the Wave-2 performance work — H3/H4/H5/H6/H11/H15/ALG-4 — and a lifecycle fix into the final `[0.8.9]` CHANGELOG entry). |
| **Branch Strategy** | Feature branch `claude/beautiful-sagan-JAUFI` → PRs into `main`. CI builds every branch; `main` carries shipped versions. (No git tags — see RISK-003.) |
| **Build Status** | Green at v0.8.9 (Wave-1 performance PR #55 + functional/UX PR #56 + investigation-only PR #57 + Wave-2 performance/lifecycle PR #58; local build + 90-check DSP self-tests green; pluginval runs on the CI Linux gate — the local sandbox cannot fetch the pluginval release under the egress policy, the same constraint recorded for the JUCE bump). Earlier: the **JUCE dependency was bumped 8.0.8 → 8.0.14** (`41acaa7`, ADR-0012) and **verified green by CI** — build + DSP self-tests + `pluginval PASSED at strictness 10` on the Linux gate. All 0.8.8/0.8.9 performance work is behaviour-preserving (bit-exact or bounded Class-B outputs, per item; DSP self-tests still green). Build = CMake + JUCE 8.0.14, VST3 [+AU macOS] [+Standalone]. |
| **Test Status** | 27 DSP self-tests **+ an A/B state-restoration clamp guard** (90 individual assertions), and pluginval strictness 10 on the Linux gate in **both modes** (deterministic + `--randomise` ×3, blocking). The JUCE 8.0.14 bump was green on CI (`41acaa7`); the randomise gate + clamp guard were added afterwards; the newest are `testRapidForcedSwapDryFill` (Test 27, rapid forced-swap dry-fill) and `testMultibandFlatRecombination` (Test 28, close-crossover flatness), after `testForcedSwapNoDropout` (Test 26). `docs/procedures/TESTING.md`. |
| **Release Status** | Pre-1.0 (0.8.x line). **v0.8.9 finalized** 2026-07-12 (PR #58). Distribution = CI artifacts (Linux/Windows/macOS); macOS ad-hoc signed, **not notarized** (KI-002). No formal release tags. |
| **Known Blockers** | None blocking a build/ship. Open items are KI-001…KI-009 (`KNOWN_ISSUES.md`) — none release-blocking (all Low/Medium, confirmed/mitigated/reported-external). KI-009 (REAPER Save Preset focus) is host-specific, pending manual investigation. |
| **Pending Tasks** | Post-v0.8.9 PR in flight: undo/redo audible-dropout fix (forced-duck dry-fill) + adaptive display-rate GUI refresh (`FrameClock`) + KI-009; a **fresh post-v0.8.9 profiling baseline** was produced (scratchpad report). The re-ranked backlog: **H7 + a LevelMeters gate** (top broad ROI, both Category C → one Architecture Review), then **Multiband LR4 SIMD** (highest ROI for the shipped mb4 default, non-Category-C), then **OSD-2** (SIMD oversampler, OS-only). H16 is largely superseded by `FrameClock`. Formal CPU/memory budget numbers remain a TODO (PERFORMANCE_BUDGET); real-DAW host matrix (KI-004/KI-009); adopt git release tags (RISK-003); optional KI-001 warm-bank refinement (needs ADR). |
| **Roadmap** | `TODO: no roadmap is recorded in the repository. Requires project-owner input. (AAX remains Not Supported by decision — COMPATIBILITY_POLICY.)` |
| **Ownership** | `TODO: no owner/team metadata in the repository. Requires project-owner input.` Company of record: RollyTech (`CMakeLists.txt:89`); contact per repository owner. |

## Critical dependencies (with version-lock reasons)

| Dependency | Pin | Version-lock reason |
|---|---|---|
| **JUCE** | `8.0.14` (FetchContent, `CMakeLists.txt:33`) | Framework for all DSP, parameters/state, GUI, and plugin wrappers. An unpinned bump can silently change DSP/latency/state-ABI and the X11 editor path (INC-006 lives in JUCE). Pin = reproducible, audited behaviour. A bump is a Build System change (ADR + Review). The 8.0.8 → 8.0.14 bump is recorded in ADR-0012. See `docs/policies/DEPENDENCY_POLICY.md`, RISK-001. |
| **C++ standard** | C++17 (`CMakeLists.txt:16-18`) | The codebase targets C++17; raising it is a build-contract change. |
| **pluginval** | latest release (downloaded) | The conformance gate (strictness 10). Not vendored; fetched by `scripts/run-pluginval.sh`. |
| Linux system libs | distro (`scripts/setup-linux.sh`) | ALSA/JACK/X11/FreeType/GTK/WebKit/mesa/xvfb for headless build + validation. |

## Documentation ownership (proposed RACI — confirm with project owner)

No team structure exists in the repository, so the following is a **proposed** mapping (governance
guidance, not asserted fact); the project owner should confirm/assign:

| Documentation area | Proposed owner |
|---|---|
| `docs/architecture/` (incl. ADRs, DSP, signal flow) | DSP / Audio engineer |
| `docs/architecture/PARAMETER_*`, `SERIALIZATION_*`, `STATE_*` | Whoever owns the parameter/state surface (compatibility-critical) |
| `docs/procedures/` (BUILD, CI_CD, PACKAGING, RELEASE) | Build / Release engineer |
| `docs/policies/` | Tech lead / maintainer (these are binding) |
| `POSTMORTEMS`, `KNOWN_ISSUES`, `FUTURE_RISKS`, `HANDOVER` | Maintainer (updated per release/incident) |

## First steps for a new maintainer

1. Read `docs/SOURCE_OF_TRUTH.md`, then `docs/policies/AI_AGENT_POLICY.md` (Hard Stop conditions).
2. Build + test locally per `docs/procedures/BUILD.md` / `TESTING.md`.
3. Before touching DSP/parameters/threading/state/latency, read the governing policy + ADR.
4. Keep docs in sync per `docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`; refresh this file each release.
