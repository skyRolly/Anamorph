# Release readiness — 0.10.0 (draft only)

## Scope and authority

Recorded 2026-10-29, Asia/Shanghai. The owner requests promotion of Unreleased to v0.10.0,
a release-blocker audit, pre-release preparation and release execution as **DRAFT** once all
required checks pass. The repository's tag is the bare **0.10.0** (ADR-0059).

Audit base: fetched `origin/main` at `ad4ee7070ce2cc9b41d8de359e4e663351b9f630`, including
PR #183's corrected macOS icon rendering. Previous annotated tag: `0.9.9`, peeling to
`c4f390f9cccaa9551d680cb1305fa4a669b24587`. There are no open GitHub issues or PRs at the
initial audit snapshot. The existing 0.9.9 GitHub release is a draft.

Read the contributor instructions, applicable compatibility/DSP/realtime/release/documentation
policies and current published Keep a Changelog 1.1.0. This preparation changes the version and
release records, not runtime behavior, dependency pins, toolchains, workflow gates or architecture.
The original development name 0.9.10 remains in historical evidence filenames.

## Human approvals — complete

The owner's direct instruction states: “Human Level-5 listening tests and other required manual
validations have already been completed and approved; record these approvals without requesting
redundant testing.” Record this as **PASS / APPROVED for 0.10.0**, independently of the historical
0.9.9 attestation. Required host matrix, automation, prior-version session/preset recall and native
installation/appearance validations are covered by that instruction. Actual human test dates,
exact artifact hashes, hosts/versions, OS/CPU, formats, devices and per-scenario results were not
supplied and are **NOT RECORDED**. No redundant testing is requested and no detail is inferred
from CI. See `docs/procedures/LEVEL5_AUDITION.md` and `RELEASE_COMPATIBILITY_CHECKLIST.md`.

Existing architecture approvals are retained: ADR-0062 (including the owner's 2026-10-29 macOS
icon fallback approval), ADR-0063/0064 (2026-10-24) and ADR-0065/0066 (2026-10-25) are Accepted.
No new architecture exception is needed for a version-only CMake edit. The user authorizes a
draft, not public publication; commercial/licensing decisions are unchanged.

## Repository audit

| Surface | Evidence and disposition |
|---|---|
| Parameters and plugin identity | `src/PluginParameters.cpp/.h` and `tests/fixtures/parameter_registry.snapshot` unchanged from 0.9.9. RTec/Anmr, product name, VST3 identity and Windows AppId preserved. Application bundle/package receipt namespace changes are explicitly covered by Accepted ADR-0063; its development-receipt break is disclosed. |
| DSP, threading and latency | `src/dsp/` unchanged from 0.9.9. No engine stage, toolchain/ISA choice, audio-thread operation or reported-latency rule changes in this release preparation. Existing bulk-restore protocol preserved. |
| State and presets | PR #168's `SerializedXml.h` formats numeric NaNs on a private serialization copy; finite output, fields, readers and acceptance remain unchanged. State tests 142/144 cover save and malformed-input boundaries; existing legacy and real field captures remain. |
| GUI gesture release | PR #168's corrected single nested mouse-release replay follows Accepted ADR-0050. State test 143 verifies release/Undo/Solo boundaries; no speculative restore-reentry mitigation is shipped. |
| Native resources and installers | PRs #170–#175 and #183, Accepted ADR-0062–0066. Unchanged approved B0 artwork, complete native macOS iconset, Windows component-safe uninstall icon, owned Linux desktop resources and guarded real macOS receipts remain behind their platform gates. PR #180 changes display names only. |
| Validation supply chain | PR #181 pins pluginval v1.0.4 release archives, executable hashes and cached inventories. PR #182 rejects ambiguous Windows DSP dump records without weakening numerical comparison. |
| Release assets | Existing release workflow reuses the full build gate, archives the validated payloads, preserves installers, adds the manual/NOTICE/THIRD_PARTY_LICENSES/SUPPORT, SHA256SUMS and traceability manifest, then creates a draft. No public publish step is authorized. |
| Documentation drift | Corrected the obsolete 0.9.10 status, approval scope, moving merged-branch changelog evidence and HANDOVER's floating-pluginval claim. Historical observations remain dated; owner attestation is recorded separately. |

No new release-blocking source regression was established by this audit. The following existing
residuals retain their documented preserve/defer dispositions; they are not represented as fixed
or as newly approved architecture changes. Mandatory automated checks remain a blocking gate.

## Next development round — prioritized residuals

1. **P1 — KI-029: non-finite host/text parameter semantics.** Saving numeric NaN is repaired,
   but audible NaN mutes/transients and module-specific ingress behavior remain. Obtain the owner
   contract for NaN/each infinity, initial/reset lifetime, host readback and the usable-raw restore
   window before an ADR-backed semantic/threading change. Evidence: the decision package in
   `worklogs/STATE_HOST_HARDENING_0.9.10.md`, DSP tests 59/64/74 and State tests 123/128/138/142.
2. **P1 — F9 / RISK-009: nested restore tail and lock edge.** The synthetic timer-pump schedule
   can leave older preset metadata over a newer sound. No supported-host deadlock is established;
   the rejected adoption guard breaks callback-save coherence and must not be shipped. Obtain a
   supported-host trace or explicit support-contract decision, then design cancellation/snapshot
   semantics preserving ADR-0036's fixed-point saves. Evidence: `--restore-lock-probe`,
   `--restore-reentry-probe` and the same worklog's focused residuals. The opt-in failing diagnostic
   is evidence of this existing residual, not part of the default release regression suite.
3. **P2 — RISK-015: additional malformed host-state restrictions.** Preserve current bounded
   acceptance until the owner specifies UTF-8/NUL/framing/trailing-document/truncation rules for
   both outer and A/B payloads. Historical writer/host tolerance must be assessed before narrowing
   acceptance under an approved ADR. Evidence: State tests 116 E/144 and Accepted ADR-0056.
4. **P2 — KI-015 / commercial distribution.** JUCE/commercial licensing, approved terms,
   Authenticode/notarization and external-distribution decisions remain on the existing roadmap.
   They are not preconditions for creating a draft under RELEASE_PROCESS, and this task does not
   authorize public distribution. Required attribution assets must accompany every release page.
5. **P3 — existing platform/performance scope.** Retain documented Linux host-runloop coverage
   (RISK-008), independent multiband loads (RISK-010), golden-audio/performance-budget work and
   optional native selection/co-installation coverage. No current supported-host regression was
   established; do not turn an unmeasured scope into a pass or silently widen support claims.

These priorities will be carried into a fresh **Unreleased** section after the annotated release
tag exists. During preparation the top dated entry carries its future tag link, as required by
CHANGELOG_POLICY rule 8; no development change is falsely listed as shipped.

## Automated validation and execution

### Local pre-release gate — PASS (2026-10-29)

- Loaded the managed environment's documented compiler/sysroot setup. Actual compiler: Debian
  Clang 22.1.8 (`1~deb13u4`); this is not a claim of the official CI compiler's provenance.
  The configured JUCE source has clean git status and HEAD exactly
  `3fb5cbc0340758596dac39f7c8de47d562b4896e` (9.0.4).
- Fresh `cmake -S . -B build`, then Release targets `Anamorph_VST3`, `Anamorph_Standalone`,
  `AnamorphTests` and `AnamorphStateTests`: exit 0. The build uses the current 0.10.0 version.
- `xvfb-run -a bash scripts/preflight.sh`: exit 0. DSP: **945 checks / 0 failures**.
  State: **5942 checks / 0 failures**. Source/structure/identity/icon/desktop/integrity checker
  suites and the local Linux ABI check pass. Full authoritative warning gates and native-platform
  tests remain CI work; the advisory syntax sweep skips the external JUCE override layout.
- Locked official pluginval v1.0.4 acquisition/integrity verification: exit 0.
  `scripts/run-pluginval.sh 10 deterministic vst3` and
  `scripts/run-pluginval.sh 10 randomise vst3`: each **3/3 passes**, every pass on its first
  attempt. The script runs under Xvfb on Linux. No crash retry was needed.
- Existing local Python invalid-escape SyntaxWarnings in `check-docs.py` and the missing-PowerShell
  integration skip are reported rather than counted as native Windows evidence. The latter is
  exercised by the existing Windows CI job; no checker or compiler baseline is weakened.

The compatibility checklist is **8/8 PASS** from fresh local measurements plus owner attestation.
Remaining execution gates: preparation/merge CI, the unchanged full platform/packaging matrix,
annotated tag on main, tag-triggered Release workflow and draft asset/hash/manifest verification.
These are required before completion; no remote tag or draft is claimed before execution.
