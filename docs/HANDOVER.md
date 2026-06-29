# HANDOVER.md

Operational status snapshot for technical handover. Update on every release
(`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`). Facts are Verified from the repository;
fields with no repository evidence are marked `TODO` rather than invented (constraint C7).

Snapshot taken at HEAD `1914c52`.

## Operational status

| Field | Value |
|---|---|
| **Current Version** | 0.8.7 (`CMakeLists.txt:14`; latest version commit `6a24b82`). |
| **Branch Strategy** | Feature branch `claude/beautiful-sagan-JAUFI` → PRs into `main`. CI builds every branch; `main` carries shipped versions. (No git tags — see RISK-003.) |
| **Build Status** | Green at v0.8.7 (`6a24b82`) on JUCE 8.0.14. The **JUCE dependency was bumped 8.0.14 → 8.0.14** — re-verified by CI on push (the local sandbox cannot fetch JUCE under the egress policy). No DSP/source logic changed. Build = CMake + JUCE 8.0.14, VST3 [+AU macOS] [+Standalone]. |
| **Test Status** | 23 DSP self-tests + pluginval strictness 10 (Linux gate). Last verified green at v0.8.7; comment/doc-only changes since cannot affect them. `docs/procedures/TESTING.md`. |
| **Release Status** | Pre-1.0 (0.8.x line). Distribution = CI artifacts (Linux/Windows/macOS); macOS ad-hoc signed, **not notarized** (KI-002). No formal release tags. |
| **Known Blockers** | None blocking a build/ship. Open items are KI-001…KI-005 (`KNOWN_ISSUES.md`) — none release-blocking. |
| **Pending Tasks** | Performance profiling (PERFORMANCE_BUDGET TODOs); real-DAW host matrix (KI-004); adopt git release tags (RISK-003); optional KI-001 warm-bank refinement (needs ADR). |
| **Roadmap** | `TODO: no roadmap is recorded in the repository. Requires project-owner input. (AAX remains Not Supported by decision — COMPATIBILITY_POLICY.)` |
| **Ownership** | `TODO: no owner/team metadata in the repository. Requires project-owner input.` Company of record: RollyTech (`CMakeLists.txt:89`); contact per repository owner. |

## Critical dependencies (with version-lock reasons)

| Dependency | Pin | Version-lock reason |
|---|---|---|
| **JUCE** | `8.0.14` (FetchContent, `CMakeLists.txt:33`) | Framework for all DSP, parameters/state, GUI, and plugin wrappers. An unpinned bump can silently change DSP/latency/state-ABI and the X11 editor path (INC-006 lives in JUCE). Pin = reproducible, audited behaviour. A bump is a Build System change (ADR + Review). See `docs/policies/DEPENDENCY_POLICY.md`, RISK-001. |
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
