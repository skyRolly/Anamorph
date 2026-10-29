# Release readiness — 0.10.0 (draft only)

**Final status: DRAFT COMPLETE (2026-10-30, Asia/Shanghai).** Required validation and manual
approvals are complete; all 11 uploaded assets were downloaded and verified. The original tag
remains unchanged. Delivery recovery and the merged future workflow repair are recorded below.

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

Preparation PR: [#184](https://github.com/skyRolly/Anamorph/pull/184). The release heading is
dated **2026-10-30**, the current Asia/Shanghai date after the local gate completed; the owner's
approval and local measurements remain correctly recorded on 2026-10-29.

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
At that point the remaining execution gates were preparation/merge CI, the full platform/packaging
matrix, annotated tag and draft verification. Completed execution and recovery are recorded below.

### CodeQL audit — completed (2026-10-30, Asia/Shanghai)

The alert-inventory API returned HTTP 403 for this integration. The workflow deliberately
exports raw SARIF for this audit context (`.github/workflows/codeql.yml`), so the supported
connector's artifact download was used and its ZIP bytes were verified against GitHub's
artifact digests. The CLI's separate Actions blob redirect was denied; the connector copy
was retrieved successfully. Alert open/dismissed states were not read.

- Candidate CodeQL run: [40907315871](https://github.com/skyRolly/Anamorph/actions/runs/40907315871),
  head `f7db27aeac847fb375f4c5093835047e0c25e765`, PR merge tree `cf2dca0e37b817b46e7fbb1f98276f097613f163`.
- Actions SARIF: **0 results**; artifact `12911378945`, ZIP SHA-256
  `154a3cb8f7edc703d9f4b18afe7783381a9d211b8033cb2006e36d9f297e06a7`.
- C++ SARIF: **65 results, all under `build/_deps/juce-src/`; zero first-party results**.
  Rules: `cpp/comparison-with-wider-type` (1), `cpp/unsafe-use-of-this` (1),
  `cpp/alloca-in-loop` (7), `cpp/integer-multiplication-cast-to-long` (56).
  Artifact `12912859856`, ZIP SHA-256
  `d049f5af1d8926dfea7cd028154db919ccb8695e49fbfc8b9c7bf773d65a59a6`.
- Baseline: [40901467382](https://github.com/skyRolly/Anamorph/actions/runs/40901467382) at
  audited main `ad4ee7070ce2cc9b41d8de359e4e663351b9f630`; artifact `12911531192`, ZIP SHA-256
  `d1299a0feda74b25e4acb4b75f842a0c5d1d5312df4577476c2e99002b1fafdb`.
  It also has 65 C++ results. Comparing rule, primary path/region and level yields **zero new
  and zero removed diagnostic locations**.

These are the existing vendored-framework diagnostics whose accepted, pin-locked disposition
the CodeQL workflow documents. No JUCE source, query scope, suppression or gate is changed.
The measured report comparison establishes no new candidate finding; it is not a claim that
the framework has no defects or that all scanner findings have been fixed.

### Automatic draft startup failure and recovery record

Preparation PR #184 merged as `cd70a0b8a20d04c246a826c7b1bf616de2721c65`, with a tree identical
to the fully validated head `f7db27aeac847fb375f4c5093835047e0c25e765`. Preparation build
40907304209 passed all 13 running jobs; PR merge-check, CodeQL, MSVC and dependency review passed.
The annotated `0.10.0` tag object is `769d52fb09116b787184d119ff1db6075187e36d`, pointing at
`cd70a0b8a20d04c246a826c7b1bf616de2721c65`.

Release run [40915398330](https://github.com/skyRolly/Anamorph/actions/runs/40915398330) failed
with `startup_failure`, before any job ran. GitHub's workflow annotation reports both optional
macOS review jobs requesting `actions: read` while the Release caller allows `actions: none`.
Static compilation checks those requests even though the jobs would be skipped. The initial
audit missed this contract. This development head adds the least required read grant only to
the reusable Build caller and adds live regression controls; runtime and tag source stay unchanged.

The existing tag remains at its original commit. The normal main-push Build & Validate run
[40915335616](https://github.com/skyRolly/Anamorph/actions/runs/40915335616), build **1940**, passed
all 13 running jobs on that exact tagged commit. Main CodeQL run 40915335668 and MSVC run
40915335638 also passed. The original tagged metadata validator was executed against the real
annotated ref and returned `version=0.10.0`, `is-release=true`; tag object and commit were
checked independently. The original tagged asset assembler was extracted verbatim, rendering
only the version expression and setting the real source build/run provenance.

Local release creation made private draft **428196589**, but the local `uploads.github.com`
request failed with HTTP 401 while ordinary API authentication worked. No login or credential
replacement was attempted. A single-purpose repository-scoped CI uploader recovered the existing
draft using the exact validated source artifacts, checking source run/head/build number, every
required job, original annotated tag and pre-existing unpublished draft before uploading.
It contains no release-creation or public-publishing operation. The operational workflow is retained
on `recovery/0.10.0-draft-assets`, commit `8c3112580a282afd4e0ac00cd6733870700709f5`.

[Recovery run 40926100598](https://github.com/skyRolly/Anamorph/actions/runs/40926100598) **PASS**.
It ran the original tagged metadata validator and asset assembler, used source build 1940 in the
manifest, uploaded all 11 assets with its repository-scoped token, and verified every server asset
digest. Its provenance artifact retains source-run/jobs metadata, draft metadata, checksums and
manifest. The unrelated default C++ matrix for this operational-only branch (40926100428) was
cancelled as redundant: the uploader consumes the already fully validated tagged commit and
builds no product code. The source validation run, fix validation and native rehearsal were retained.

PR [#185](https://github.com/skyRolly/Anamorph/pull/185), commit `3176baf`, merged as
`a5973c62d7249ce4296fe12f3f784886956c0b41`. Its full Build run 40917696003, PR merge-check
40917861965 and CodeQL 40917861590 pass. The corrected caller's full native
[Release rehearsal 40917857916](https://github.com/skyRolly/Anamorph/actions/runs/40917857916)
**PASS** (validate plus all 13 running Build jobs); draft-release is intentionally skipped on dispatch.
No runtime code, dependency pin, validation threshold or publication guard was changed.

### Final draft and uploaded asset verification — PASS

- Draft: [Anamorph 0.10.0](https://github.com/skyRolly/Anamorph/releases/tag/untagged-e925c4f20dd9c3da3855),
  ID **428196589**, `draft=true`, `published_at=null`. Notes match the original tagged changelog
  section, headed `0.10.0 — 2026-10-30`; Unreleased is excluded.
- **11 assets:** three platform ZIPs, Windows EXE installer, macOS PKG, user manual, NOTICE,
  THIRD_PARTY_LICENSES, SUPPORT, SHA256SUMS and RELEASE_MANIFEST. Debug symbols remain CI artifacts.
- All five downloaded source artifact ZIPs matched GitHub's recorded transport digests and came
  from run 40915335616 / commit `cd70a0b`. Installers were moved unchanged; payloads were not rebuilt.
- All final release assets were downloaded again. **Nine payload SHA-256 records pass**, every
  asset also matches GitHub's server digest/size, and the manifest names the unchanged tag/commit,
  real build **1940** and source validation run. Archive CRCs and flat layouts pass.
- Linux executable/script/VST3 and macOS bundle executable modes pass. Windows PE and PKG headers
  pass. All three macOS bundle identifiers are `tech.rolly.anamorph`, and short/build versions are
  exactly **0.10.0**. Linux installer/helper bytes match the tag, including the trusted helper pins.
- User manual, NOTICE, THIRD_PARTY_LICENSES and SUPPORT match their tagged source bytes exactly.
  The owner manual approval remains the 2026-10-29 attestation; no human test is repeated or invented.
- Prioritized residuals are recorded under CHANGELOG **Unreleased**. Runtime KI/RISK dispositions,
  Accepted ADRs, signing/notarization and commercial/legal decisions retain their existing status.

Source artifact provenance:

| Artifact | ID | Transport ZIP SHA-256 |
|---|---|---|
| Anamorph-Windows | 12915958452 | `e90e5b16e0b7ec9f5e067f6e986ed93cd4af9757c2a5d1faafb657e48ccbac1d` |
| Anamorph-Windows-installer | 12915819017 | `52182c32e8b1668a5d459704d6b06766c53b9eb0c8c324793168acfd37e99d93` |
| Anamorph-macOS | 12917133302 | `b0595c9dfa9649862bd138ac5c79bb31c4b9f3ca7401705c76b3e99d763b4c1b` |
| Anamorph-macOS-installer | 12916669168 | `870c2940d85347934953bbfd8624461582d069f757cf04977fcb67d8d96cb6b5` |
| Anamorph-Linux | 12917872735 | `08ad6d766ab9f1c83a423b29621313f07aea4ceec81ae06d7c66ea8c6b810494` |
