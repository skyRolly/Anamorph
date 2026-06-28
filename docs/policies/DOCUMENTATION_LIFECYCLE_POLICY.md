# DOCUMENTATION_LIFECYCLE_POLICY.md

Repository Governance Policy. Defines which documents must be updated when code changes — the
trigger map that keeps docs and code in sync (prevents documentation rot).

## Core rule

Documentation is updated **incrementally** alongside the code change in the same unit of work.
Apply the **smallest** change that re-syncs the doc; preserve hand-written content; never
regenerate a file wholesale unless explicitly requested (constraint C5). Before editing an
existing doc, run a **drift check** and report any code/doc disagreement (constraint C6).

## Trigger map (change → docs to update)

| Code change | Update these |
|---|---|
| **DSP algorithm / module maths** | `DSP_ALGORITHMS.md`, `SIGNAL_FLOW.md` (if order/placement), `DSP_GRAPH_REFERENCE.md`, an **ADR**, `CHANGELOG.md` |
| **Signal-flow / stage order** | `SIGNAL_FLOW.md`, `DSP_GRAPH_REFERENCE.md`, `DSP_POLICY.md` (if an invariant), **ADR**, `CHANGELOG.md` |
| **Add/remove/rename a parameter** | `PARAMETER_REGISTRY.md`, `PARAMETER_REFERENCE.md`, `PARAMETER_COMPATIBILITY_POLICY.md` (if contract), **ADR**, `CHANGELOG.md` |
| **State serialization schema** | `STATE_SERIALIZATION.md`, `SERIALIZATION_REGISTRY.md`, `SESSION_COMPATIBILITY_POLICY.md`, **ADR**, `CHANGELOG.md` |
| **Threading / cross-thread path** | `THREAD_MODEL.md`, `THREADING_POLICY.md`, **ADR** |
| **Latency behaviour** | `LATENCY_MODEL.md`, **ADR**, `CHANGELOG.md` |
| **Oversampling strategy** | `LATENCY_MODEL.md`, `DSP_ALGORITHMS.md`, ADR-0003 (update/supersede), `CHANGELOG.md` |
| **Build / CMake / JUCE pin** | `BUILD.md`, `CI_CD.md`, `DEPENDENCY_POLICY.md`, **ADR** (if architecture) |
| **CI workflow** | `CI_CD.md`, `TESTING.md` |
| **Packaging / signing** | `PACKAGING.md`, `RELEASE_PROCESS.md` |
| **New/changed test** | `TESTING.md`, `DOCUMENTATION_COVERAGE.md` |
| **Plugin format** | `COMPATIBILITY_MATRIX.md`, `COMPATIBILITY_POLICY.md`, **ADR**, `CHANGELOG.md` |
| **Ship a version** | `CHANGELOG.md`, `HANDOVER.md`, `README.md` (status/version) |
| **Fix a notable incident** | `POSTMORTEMS.md` (new INC), `KNOWN_ISSUES.md` (remove if it was listed) |
| **New unresolved limitation** | `KNOWN_ISSUES.md` and/or `FUTURE_RISKS.md` (new RISK) |

## Audit obligation

On every documentation-affecting change, update `DOCUMENTATION_COVERAGE.md` (the persistent
coverage audit). A future agent must keep it current.

## Enforcement

Documentation drift discovered during work must be **reported, not silently corrected** (C6).
This policy is invoked by `AI_AGENT_POLICY.md` after any code change.
