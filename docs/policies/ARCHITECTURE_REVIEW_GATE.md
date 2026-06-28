# ARCHITECTURE_REVIEW_GATE.md

Repository Governance Policy. Some changes are too consequential to merge on a green build alone.

## Rule

The following changes **require human Architecture Review and must NOT be auto-merged even if CI,
the DSP self-tests, and pluginval all pass**:

- **DSP Graph change** — adding/removing/reordering a DSP node (`DSP_GRAPH_REFERENCE.md`).
- **Signal Flow change** — altering the order or placement of any stage (`SIGNAL_FLOW.md`).
- **Thread Model change** — new thread, new cross-thread path, new atomic ordering (`THREAD_MODEL.md`).
- **Parameter Registry change** — adding/removing/renaming any parameter ID, changing range/
  default/automatable/exclusion (`PARAMETER_REGISTRY.md`).
- **Serialization Registry change** — any field add/remove/semantic change (`SERIALIZATION_REGISTRY.md`).
- **Latency change** — sources, engagement condition, or reported value (`LATENCY_MODEL.md`).
- **Plugin Format change** — adding/removing a format (VST3/AU/AAX/Standalone).
- **Build System change** — CMake structure, JUCE version/pin, dependency set.

## Why these specifically

Each maps to a field-breaking risk (compatibility, real-time safety, or host PDC) that a passing
test cannot rule out — e.g. tests cannot prove a renamed ID won't break a user's saved session,
or that a new thread path is race-free under every host.

## Procedure

1. The author flags the change as gated (it touches one of the areas above).
2. A human reviewer with DSP/audio context reviews against the relevant Policy + ADR.
3. If the change is a decision, an **ADR** is added/updated (`ADR_POLICY.md`).
4. Compatibility-affecting changes additionally run the
   `procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`.

## Relationship to the AI Agent

Detecting any gated change is an **AI Agent Hard Stop** — the agent stops and requests human
review rather than proceeding (`AI_AGENT_POLICY.md`).
