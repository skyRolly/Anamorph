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

### Compiler and toolchain versions

Amended by **ADR-0028**. The **Build System change** item above covers compiler versions, and what
decides whether a particular change is gated is **who chooses the version** — not which platform it is
on, and not whether that compiler's output ships:

1. **A version this repository pins is gated.** The Linux `ANAMORPH_CLANG_VERSION` and
   `CMAKE_CXX_STANDARD` are both this case, and both carry an ADR (0028, 0027). The Clang pin was
   gated even when no Clang job uploaded an artifact — the pin is a repository *decision*, and a
   decision is what an ADR records; since ADR-0030 it also builds the shipped Linux artifact, so it is
   now gated on both counts.
2. **A version the runner image supplies is not gated, because it cannot be.** GitHub re-points
   `macos-latest` and `windows-latest` with no commit here, so AppleClang, MSVC, CMake and glibc can
   move with no pull request to review; a rule demanding review for those is one this repository is
   unable to obey. What is required instead is **detection and record** — the Clang warning baseline is
   the detector for diagnostic changes, and the consequences are written up in
   `procedures/CI_CD.md` §Build matrix when they land.
3. **Changing which of those two a toolchain is** — pinning a floating label, or unpinning a pinned
   one — **is gated when that toolchain builds shipped artifacts**, because that is the repository
   taking or handing over control of the shipped bytes.

**The AppleClang 15 → 21 precedent is reconciled by rule 2, not exempted from rule 1.** The
2026-08-15 move of `macos-14` → `macos-latest` changed no version this repository had pinned —
AppleClang has never been pinned here — and the compiler that followed was GitHub's choice, not a
value in this tree. It was handled as a CI change with a full write-up in `CI_CD.md` §Build matrix,
which is what rule 2 asks for. Under rule 3 the *label* move would be gated today, because it handed a
shipping toolchain to the image; the rule is stated here so that answer is available next time instead
of being re-argued. Numeric symmetry between platforms is explicitly **not** required: Apple's
compiler versions are not upstream LLVM's, and the two are chosen by different parties.

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
