# Release readiness — 0.10.0 (draft only)

## Later baseline and forward roadmap — 2026-10-31 UTC

The read-only [next-release investigation](NEXT_RELEASE_DEVELOPMENT_ROADMAP.md) verifies
PR #193 merged at `4c136881f245470577d66a7603f15b177ddcf3dc` and a subsequent normal
[Release run 41173229942](https://github.com/skyRolly/Anamorph/actions/runs/41173229942)
(build 7). Current draft ID **429760301** targets that source and remains unpublished.
All 11 release assets and three native installation evidence archives were downloaded
and checked; the current Linux report has 17 native tests, zero failures/errors and
staged hashes matching the release ZIP. The roadmap preserves current identities/hashes
and is the forward execution register; the sections below retain dated release history.

The following “in review”, “rebuild required” and P1-draft identity statements describe
their earlier task snapshots, not the live baseline above. ADR-0069 remains Proposed
in source despite the owner merge; explicit lifecycle reconciliation is NR-001.
Current-candidate targeted Linux human acceptance is not established by the earlier
original/P1 attestations (NR-002). No approval, implementation or release operation
is inferred or performed by recording this later snapshot.

**0.10.0 Linux correction in review (2026-10-31):** VST3-only installation with
portable Standalone and preservation of all legacy desktop resources is the current candidate under
proposed ADR-0069. Implementation/validation/PR preparation are authorized; human
review and manual merge are required. No tag or release edit/publication is authorized
in this task. After merge, rebuild 0.10.0 from the exact merged source, repeat applicable
release/native gates and targeted Linux manual checks, then replace the existing
0.10.0 release only under separate authorization. Prior completed P1 evidence below
is historical and does not certify the corrected Linux candidate.

## Legacy cleanup removal — current candidate validation (2026-10-31)

The owner's follow-up removes legacy desktop cleanup from both Linux entry scripts
and both installation modes. Launchers, icons, ownership receipts, recovery journals
and installed/staged Standalone binaries are left untouched. VST3 replacement,
destination checks and parked-copy recovery retain their existing behavior.
Source: commit `f728b50`; review: [PR #193](https://github.com/skyRolly/Anamorph/pull/193);
contract: [proposed ADR-0069](../docs/architecture/design-decisions/ADR-0069-linux-plugin-only-installation.md).

Local validation for this preservation candidate: 14 Bubblewrap installer cases,
six native-plan/coverage cases, package self-test/source contract, shell syntax,
documentation structure for 14 changed Markdown files and `git diff --check` passed.
The installer cases now require legacy preservation; the obsolete partial-cleanup
retry case is retired, and deletion mutants exercise the preservation assertion.
The optional earlier-release archive check was not run for this candidate. Real
UID/sudo hosted-VM validation was not run locally and remains a required CI gate.
PR #191's migration, native/platform and earlier manual results below do not certify
this follow-up. Release rebuild/replacement still requires separate authorization.

## Historical PR #191 migration validation (2026-10-31; superseded by PR #193)

The following record describes the earlier cleanup implementation and its original
validation only. Its cleanup behavior, case count and migration limits are historical;
the preservation contract and validation scope above govern the current candidate.

The owner explicitly requested correction in 0.10.0 and selected VST3-only script
installation with portable Standalone retained. Dedicated branch:
`fix/0.10.0-linux-plugin-installer`. PR preparation stops before human review/merge;
no existing tag or release is edited, deleted, recreated or published by this task.
Review: [PR #191](https://github.com/skyRolly/Anamorph/pull/191). Its checks and
`Anamorph-Linux-install-validation` artifact retain fresh native/platform results;
the PR description records final run identities, outcomes and remaining limits.

Root cause: ADR-0065's desktop helper applied a whole-ancestry group/other-write
refusal before both binary installation and removal. A normal personal-group 0775
icon directory therefore blocked an otherwise independent audio-plugin operation.
The earlier binary workflow (`fc7428e^`) did not depend on desktop icon ancestry.
The corrected scripts retain staged VST3 replacement and recovery, make installed
module chmod mandatory before publication, and remove desktop publication. Migration
uses protected old receipts/owner/content hashes without chmod/chown of shared trees.

Local completed validation:

- `scripts/preflight.sh`: source/documentation/identity/icon/realtime/portability and
  other mandatory local lint/self-tests, plus existing native Linux DSP/state suites:
  945 DSP checks and 10,827 state checks, zero failures. Runtime C++/CMake inputs are
  unchanged; the full fresh build/native/platform result remains a separate CI gate.
- `tests/linux_install_tests.py --legacy-archive <verified prior ZIP>`: all 15 cases,
  including fresh user/system namespaces, permission-independent installation,
  optional portable payload, repeated install/uninstall, legacy receipt cleanup,
  unrelated/modified/link/missing-receipt preservation, partial cleanup retry,
  error/SIGKILL/TERM/HUP/INT swap recovery and executable-mode failure before commit.
  A portable input extracted at the legacy bin path is also explicitly preserved.
  A final permission regression also verifies that inaccessible icon ancestry keeps
  its ownership receipt and warns, rather than being mistaken for an absent resource.
- Six native-plan/lifecycle/upload-condition tests, eight icon/preflight aggregate
  regressions, and package source/staged-contract positive/negative self-tests.
- The actual existing 0.10.0 Linux ZIP (asset 690966946) was read-only downloaded and
  SHA-256 checked against `d2aea303302bc4b1e975494588a1e25e50b5471d3916b42c7852ffa005bb9963`.
  Its actual old scripts/binaries/resources were installed in isolated user and
  mapped-root system namespaces. Old user removal refused a 0775 icon parent;
  corrected upgrade/reinstall/repeated removal cleaned verified resources and the
  exact old Standalone, preserved unrelated icons, and kept the parent mode intact.
- Workflow edits moved eight governed citations; the same source text was re-anchored
  by the citation gate. No referenced policy/compatibility decision was changed.

The local environment has no sudo; namespace-mapped root is not real-host privilege
evidence. Real ordinary/root UID, per-operation sudo, system killed-swap recovery and
foreign-owner preservation must pass in the mandatory staged hosted-VM CI gate.
Fresh cross-platform CI and manual Linux DAW/desktop cache observations are not
converted into local passes. Final CI evidence is linked from PR #191; release
replacement still requires separate approval after human review/manual merge.

Conservative migration limits: edited/unowned resources, malformed receipts,
incomplete/unknown legacy journals and binaries outside the verified old executable
digest remain with warnings. They do not block VST3 operations. Shared icon/cache
directories and permissions remain unchanged; a desktop may refresh stale cache
display later. Same-root operations must be serialized; no power-loss guarantee.

After human review and manual merge, build 0.10.0 from the exact final merged
source, repeat mandatory release/native checks and targeted Linux manual validation,
then request separate authorization to replace the existing 0.10.0 release/tag.
Existing P1 execution authorization/history below does not authorize that follow-up.

**Historical P1 replacement status: DRAFT COMPLETE; unpublished (2026-10-30, Asia/Shanghai).**
PR #187 merged at `b085c87d139f124e33a525e75ec13b8218c96f06`. Final-main Build 1956, CodeQL and MSVC analysis
passed; the recreated annotated tag's normal Release build 5 / run 41001157732 passed all
mandatory gates. New Draft Release [428664381](https://github.com/skyRolly/Anamorph/releases/tag/untagged-e9b61aa49fbd4b5cb114) targets that exact source.
All 11 assets were downloaded and independently verified against API digests, checksums,
manifest, metadata and five fresh same-run build artifacts. The owner-approved P1 changes and
targeted human attestation are included; original unrelated manual approval remains preserved.
The old unpublished draft/tag were deleted under the explicit one-time authorization, then
recreated without force-moving a tag. **No public publication occurred or is authorized.**
The dated execution record and `release-0.10.0-p1/draft-replacement.json` hold final identities.

**Historical original-candidate completion (2026-10-30):** required validation and manual
approvals were complete for the original source; all 11 uploaded assets were downloaded and
verified. Delivery recovery and the merged future workflow repair remain recorded below.

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

## P1 replacement decision and execution record — 2026-10-30, Asia/Shanghai

### Historical proposal-round authority and mandatory stop

Fetched main/tags; authoritative main is `0ac384088104dfd4dd128ab6751c828c6cd51be8`
(PR #186). PR #185's reusable Release caller grants both contents/actions read; the source
still uses the normal annotated-tag pipeline, `gh release create --draft --verify-tag`,
and the common `scripts/changelog-section.awk` extractor. No later production fix for
either P1 was found between the historical release commit and fetched main.

The owner explicitly authorizes investigation, preparation and one-time supersession of the
unpublished **0.10.0** candidate by **deleting the old draft and tag, then recreating both**.
This is not permission to force-move a tag, reuse old payloads, change version or publish.
The ordinary no-rewrite convention is reconciled by this task's explicit, narrowly scoped
exception; no separate exception-approval mechanism was found in RELEASE_POLICY/RELEASE_PROCESS
or ADR-0059. Independently mandatory architecture and release gates remain binding.

`ARCHITECTURE_REVIEW_GATE.md` says: “Detecting any gated change is an AI Agent Hard Stop —
the agent stops and requests human review rather than proceeding.” ADR_POLICY also requires
an ADR for parameter-meaning decisions. The earlier state/host decision record explicitly
required approval before implementing new invalid-input semantics. The owner's present request
requires an **approved** solution and does not approve a particular policy or nested-callback
contract. Therefore this branch prepares evidence, opt-in diagnostics, documentation corrections
and **Proposed** ADR-0067/0068; it changes no production parameter, DSP or restore code.
Neither ADR is Accepted, neither P1 is closed, and no merge/tag/deletion/release has occurred.

### Refreshed original candidate and recovery evidence

| Identity | Verified original value | Replacement |
| --- | --- | --- |
| Annotated tag object | `769d52fb09116b787184d119ff1db6075187e36d` | Not created |
| Peeled source commit | `cd70a0b8a20d04c246a826c7b1bf616de2721c65` | Final approved main not yet determined |
| Draft Release ID | `428196589` | Not created |
| Name | `Anamorph 0.10.0` | Must remain the same |
| Draft/publication | `draft=true`, `published_at=null` | Must remain unpublished |
| Source build/run | 1940 / 40915335616 | Fresh normal tag workflow required |
| Original failed Release/recovery | 40915398330 / 40926100598 | No ad-hoc recovery substituted for a normal replacement build |

The draft URL remains the historical `untagged-e925c4f20dd9c3da3855` URL recorded above.
Its API `target_commitish` is `main`, a mutable name; original source identity is established
by the annotated tag and downloaded manifest, not by treating that string as a commit SHA.
Current main Build run **40929397409** completed with all 13 running jobs successful, including
native/translated macOS, Windows, Linux, sanitizer, realtime and TSan gates; three optional/PR-only
jobs were skipped. That is evidence for its exact pre-fix source, not for future replacement code.

Internal archival evidence is in `worklogs/release-0.10.0-original/`: original title/body/status,
release/asset IDs, sizes, server digests, provenance, raw annotated tag/tagger record, manifest
and checksums. All **11 original assets were freshly downloaded** into the local mode-0700
directory `/workspace/0.10.0-original-evidence` (asset files mode 0600). Every downloaded
size and SHA-256 matches the API record; all **nine payload checksum entries** match SHA256SUMS
and appear in the original manifest. Manifest SHA-256:
`5a3dcf9a36b80ab15f964af772b4aa770f227df7c5f2f4cd00f586148aa1da94`;
SHA256SUMS SHA-256: `23484e113614643b1523b9f775ab0d661ff3ad0813aac7e69a6261be078b2366`.
These bytes remain original-source evidence and must never be relabeled as replacement assets.
The local restricted copies need durable restricted retention before any future destructive action
if the execution workspace will not persist. No obsolete second active Release is created.

Fresh metadata and historical execution records establish a draft-only candidate; no public
publication or external reliance is known from this investigation. API publication fields and
download counts alone cannot prove that no private copy has ever been distributed. Reconcile
any newly known distribution/dependency or identity change before deletion; stop if established.
The connected GitHub tools lack release/asset-management operations, so authenticated `gh api`
was used for those reads/downloads. No credentials were extracted or replaced.

### Original-code reproductions and proposal-round decision scope

Local tools: retained managed native image, GCC 14.2.0 Release, exact clean JUCE 9.0.4 pin
`3fb5cbc0340758596dac39f7c8de47d562b4896e`. The retained helper requested four CPUs but this
executor exposes three. An untracked task helper uses three CPUs/parallel build jobs; no compiler,
ISA, dependency or tracked environment setup is changed. An initial bare-shell CMake invocation
could not start; the documented native environment was then used. State processes ran serially.

| Check on current production code | Actual outcome |
| --- | --- |
| Full DSP suite | 945 checks / 0 failures |
| Full state suite before and after adding the opt-in audio diagnostic | 5942 checks / 0 failures on both runs |
| `--nonfinite-ingress-probe` | Existing host/text/restore table reproduced, including NaN notification with usable raw 0.71 |
| `--nonfinite-save-probe` | 108 checks / 0 failures; existing formatter repair preserved |
| `--restore-lock-probe` | Exit 0; outer-held replacement-lock edge witnessed; nested final Width 0.6 with preset `older` |
| Unchanged `--restore-reentry-probe` | **30 checks / 4 failures**, final/new-poll preset-name assertions; callback saves pass |
| New `--nonfinite-audio-probe` | **288 checks / 24 failures**, sustained output mutes through real host/text parameter and processor paths |

The expanded ingress probe also records custom discrete values and host formatting. Algorithm
`+Inf` retains `getValue=inf`, APVTS raw **3**, but displays **Haas**; NaN Multiband Bands displays
**0** outside its 1–4 range. These are current GCC14 observations, not sanitizer diagnoses.
ADR-0067 therefore includes finite checks before text-format integer conversions as well as DSP
snapshot reads; a DSP-only patch would leave the formatting inconsistency unresolved.

The new audio diagnostic exercises Width, Haas Delay, Chorus Rate and Output Gain;
NaN/+Inf/−Inf; host writes and parameter text; fresh instances, live writes, reset and re-prepare.
It renders finite-default and held-value control processors on identical seeded input. Its
assertions require finite, audible output and both live controls; they **do not select a new
invalid-value meaning**. Zero-energy Width/Haas Delay/Output Gain NaN legs fail despite finite
output samples. All finite-control and finite-output assertions pass. Infinity inputs currently
clamp to finite endpoints on these controls. Chorus differences remain evidence, not a claim
that this diagnostic measures every transient/coefficient failure. It is opt-in until the approved
implementation turns it into a default regression; existing default assertions are unchanged.

Example live host-NaN Output Gain energies on identical input: invalid **0**, default
**273.665**, held −18 dB **4.3373**. Thus “default” removes the mute but raises the target
18 dB relative to holding the previous value. This is exposed for human review, not hidden
behind `isfinite` or self-approved as a harmless repair.

**Proposed ADR-0067:** finite DSP snapshots using each parameter's actual declared default for
a non-finite *raw atomic*, preserving host/text readback and serialized-input resolution.
Finite endpoint results already produced by the existing setters/parsers remain endpoints.
No mutable cache or new cross-thread path. The proposal specifies the usable-raw restore
window, initial/reset lifetime, Density/Level-Match consequences and the Output Gain tradeoff.
Its implementation/whole-ingress acceptance matrix remains pending approval, not measured closure.

**Proposed ADR-0068:** complete both inline and adopted restores before delivering synchronous
latency notifications; preserve fixed-point callback saves. Defer reports under an outer state
command to the existing safe timer door. The proposal specifies ownership, request ordering,
all Settings/A/B/Undo/preset fields and inline generation-zero behavior. The early-return guard
is rejected. The measured outer-held edge and JUCE/host opposing-lock residual remain distinct
from the metadata defect. No supported-host deadlock is asserted or marked resolved.

Reproduction commands (use the documented native environment and Xvfb on Linux; state processes
must run serially). The last two commands intentionally return 1 until approved fixes exist:

```sh
xvfb-run -a build/AnamorphStateTests_artefacts/Release/AnamorphStateTests --nonfinite-ingress-probe
xvfb-run -a build/AnamorphStateTests_artefacts/Release/AnamorphStateTests --nonfinite-save-probe
xvfb-run -a build/AnamorphStateTests_artefacts/Release/AnamorphStateTests --restore-lock-probe
xvfb-run -a build/AnamorphStateTests_artefacts/Release/AnamorphStateTests --nonfinite-audio-probe
xvfb-run -a build/AnamorphStateTests_artefacts/Release/AnamorphStateTests --restore-reentry-probe
```

### Customer notes and independent preparation

The 0.10.0 changelog's entire internal introductory paragraph is removed, along with approval/
validation bookkeeping in its bullets. Legitimate product changes, date, citation forms, category
order, historical entries and link definitions remain. Material still-unfixed audio/restore
limitations are expressed in user terms; they are removed/replaced with precise Fixed entries
only when implementation and tests establish closure. No unimplemented P1 fix is advertised.
Unreleased is structurally valid and empty; INC-018 automation history and P2/P3 priorities remain
in this internal record, not as forthcoming product features.

`scripts/check-release-notes.py` extracts only the current CMake version with the existing awk
program. The same-run liveness test and actual content check run in Build's `source-lint` job,
which the normal Release workflow already requires. It rejects the previous draft-readiness
paragraph, worklog/risk identifiers and approval/gate bookkeeping while allowing evidence citations
and real signing/CPU limitations. The old notes fail the guard (negative control). No extractor,
tag grammar, metadata/date check, permission grant, build gate or `--draft` safeguard is removed.
The remote original notes are retained unchanged until a fully validated replacement is possible.

The original API target was mutable `main`. Replacement draft creation now passes explicit
`--target "${GITHUB_SHA}"`, matching the source recorded in its manifest. The production creation
shell is executed only against a recording CLI in the routing regression. It checks the source
target, draft/verified-tag safeguards, title, notes path and 11 asset arguments; four deliberate
variants (missing target, mutable `main`, missing draft, missing verified-tag) must fail. No actual
Release command is invoked by the test. This is a narrow provenance correction, not permission
to create a replacement before its P1 approval/validation gates.

Preparation checks: documentation self-test **700 cases**, current documentation structure
**174 files clean**, citation self-test **270 cases**, and **610 current evidence anchors**
verified against main. The six new Build lines shifted nine existing citation anchors; the
checker reported them before its minimal re-anchoring. Routing/permission/draft-creation
liveness and the real extracted-note content guard pass. The original notes fail the content
guard as intended. Existing Python invalid-escape SyntaxWarnings and GCC14/JUCE warning sites
remain visible; no warning baseline or validator threshold is changed. These are preparation
results, not a claim of final P1/native/pluginval/manual release validation.

### Deferred work and human-validation scope

- **RISK-015:** preserve bounded malformed-input acceptance and legacy fixtures. Reopen only on
  a concrete supported-host/safety failure or an approved byte/framing/UTF-8 contract backed by
  historical host/writer evidence. No new parser restriction or limit change.
- **KI-015:** retain commercial/JUCE licensing, product terms, signing, Authenticode and
  notarization as the public-distribution track. Reviewed current NOTICE/THIRD_PARTY_LICENSES
  requirements: every draft/release asset set retains both attribution files. No dependency
  change or new component is proposed; do not claim unsigned installers are signed/notarized.
- **RISK-008/RISK-010 and P3:** retain host-runloop, independent multiband reads, extra golden
  audio/performance and optional native selection/co-installation work. Reopen on a P1-induced
  regression, a new writer/order change, or a supported-host reproducer. No broad audit/refactor.
- **Original human approval:** preserve the owner's 2026-10-29 approval for the original source,
  including its unsupplied-environment limitations. Do not repeat icon/install checks without a
  related change. New invalid-input audio/Level-Match behavior and nested restore/automation/
  callback-save semantics require targeted approval on the new binaries. CI cannot supply it.

### State-aware replacement plan — execution prerequisites not yet satisfied

After human decisions, implement the approved contracts and default regressions, synchronize
their architecture/compatibility documents, run all required suites/pluginval/sanitizers/native
gates and targeted human validations, then merge the reviewed changes. Record the exact final
main commit and recheck version **0.10.0**, dated customer notes, clean checkout and all required
source gates. Do not delete either original object during this preparation.

Immediately before destructive execution, re-read the Release by ID and all releases for the
tag, the remote tag ref, annotated object/peeled commit, and current Actions runs. Require exactly
the original unpublished identity above, no known previous public publication/external reliance,
no conflicting creation run, durable original evidence and all approved final-source gates.

| Observed state | Permitted resume action |
| --- | --- |
| Original draft and original tag both present | Only after all prerequisites, guarded `gh release delete 0.10.0 --repo skyRolly/Anamorph --cleanup-tag --yes`; verify both objects absent |
| Original draft absent; original tag still present | Verify absence/current ref/object again; remove only that exact original tag using the supported ref deletion; do not delete any new Release |
| Both original objects absent | Confirm approved main unchanged; remove only stale local original tag; create annotated `0.10.0` on the approved commit and push normally |
| Replacement tag present; workflow pending/failed | Verify its exact new object/commit; retain it, inspect the normal Release run and use supported safe recovery; never delete it as an assumed original |
| Replacement draft partial | Inspect its new ID, tag, source/run, assets and failure; reconcile only that candidate without mixing sources or weakening gates |
| Replacement fully validated | Record identities and downloads; leave draft unpublished |
| Published Release, unknown ID/tag identity, or known external dependence | Stop; no deletion, force-move or automatic retry |

Create the replacement annotated tag only on a clean final approved main checkout, with the bare
`0.10.0` convention. Verify remote object type/peeled commit. Let its normal tag push run Release's
full reusable Build and fresh assembler; do not upload original assets. The replacement should
explicitly target the final commit (inspect the API's target commitish as well as its tag/manifest).
Exactly one new active draft, all mandatory jobs, 11 fresh assets, nine payload checksum entries,
all server sizes/digests, source/build/run manifest, archive/installer/bundle identity and version,
manual/NOTICE/licenses/SUPPORT, and byte-equivalent extracted notes must be verified by download.
Require `draft=true`, `published_at=null`; **never publish**.

**Proposal-round gate (superseded by the approval/implementation record below):** human
Architecture Review of ADR-0067/0068, followed by implementation, fresh mandatory
validation and targeted manual approval. Replacement tag/commit/Release ID/workflow
and asset verification are **not available because replacement has not begun**. Original green
checks and owner approval are preserved as historical evidence, not substituted for this gate.

## Approved P1 implementation and PR #187 review corrections — 2026-10-30

The owner's direct **“Approved”** accepts the complete proposed ADR-0067/0068 contracts,
including Output Gain's 0 dB fallback, Density's 0.5 fallback and the nested latency-callback
support boundary. Both ADRs are Accepted. This is Architecture Review evidence, not an
attestation that changed binaries were auditioned. Original 2026-10-29 manual approval remains
historical and unrelated completed checks are preserved.

### Production implementation

- Every `ParamPointers` binding records its actual declared denormalized default once.
  `toEngine` retains `seq_cst` loads and substitutes that default only for non-finite raw values,
  before all integer/enum/Boolean conversions. No parameter writes, history cache, allocation,
  lookup, mutex or new synchronization is added to the audio path. Finite values take exactly
  the previous mapping; registry IDs/ranges/defaults/flags/version and fixtures are unchanged.
- Existing host/text parsing and numeric readback remain. Float/discrete display formatting
  guards non-finite input before conversion using existing default/endpoint labels.
- The proposal's restore-window handshake premise was incorrect: host session installs never
  entered ADR-0057 bulk apply. No new protocol is introduced. The existing private-copy repair
  now also repairs an unusable `value` when the established resolver selected usable `raw`,
  before JUCE's first notification. Exact final raw reassertion is unchanged. Re-saving that
  malformed spelling produces its resolved finite value; valid finite bytes and all accepted
  malformed-document boundaries remain unchanged. Numeric-NaN writer repair is untouched.
- Both message-thread restore paths bracket application through complete Settings/A-B/Undo/
  preset/committed-state publication. Latency delivery requires depth zero, no enclosing state
  command and no pending restore. Otherwise the existing release-store/acquire-exchange request
  remains for the safe timer door, including recursively pumped timers. No old restore writes
  follow the callback. Off-thread requests never read the new message-thread depth.
- F9 metadata correctness is fixed under the approved serialized callback contract; callback
  fixed-point saves remain. The rejected early-return guard is not reintroduced. The distinct
  outer-held lock witness and JUCE listener-lock residual stay open as RISK-009; no supported-host
  deadlock is claimed or concealed. KI-029 moves to INC-019; F9 is recorded in INC-020.

### Regression and review scope

State 146 retains the original production-path audio failure and adds exact finite-default twin
assertions (320 checks). State 147 covers every registered parameter through host/text/direct raw
faults, both modes, live/reset/re-prepare audio/readback, plus repeated session/preset/A-B invalid
restores and first-notification usable-raw windows (2552 checks). State 148 keeps all original
F9 diagnostic assertions and expands complete callback-state and lock-admission coverage
(1778 checks). The original negative controls were 24/288 audio failures and 4/30 metadata
failures; these are retained as historical measurements.

Only related processor Density/crossover premises in State 128/138 change to the approved
default policy. Direct-engine DSP 64/74 and unrelated finite/match-provenance assertions remain.
State 43 still proves newer engine priming during adoption and additionally requires deferred
in-flight reporting and the final committed host report, matching ADR-0068.

The review's preflight omission is corrected: `check-release-notes.py --self-test` precedes its
ordinary invocation immediately after structural documentation checking. The preflight checker
inventory and CI_CD/TESTING inventory are updated. Two real-shell regression fixtures prove that
structurally valid forbidden current notes fail before the next source gate and valid customer
notes reach it; the preflight fixture suite now has eight cases. An actual full-preflight negative
control with `KI-029 remains open` in the 0.10.0 lead also failed at the content gate after the
structural gate passed; the temporary defect was restored byte-for-byte.

HANDOVER's operational Build/Test/Release rows explicitly scope completed validations and the
complete draft to the original candidate. Its current replacement status is recorded separately.
Customer notes describe implemented observable behavior and keep internal approval/worklog/risk
status here. The normal Release workflow remains the only replacement packaging path.

### Remaining gates and exact replacement state

Local full-suite/preflight/pluginval results and exact new-source CI/artifact identities are to
be recorded below as executed. Mandatory platform, sanitizer, warning, registry, resource and
package gates remain binding. Targeted human validation is specified in LEVEL5_AUDITION and
RELEASE_COMPATIBILITY_CHECKLIST: changed invalid host/text sound and finite recovery (Output Gain,
Density), session/preset/A-B recall, preset/Settings coherence and restore/re-prepare latency/PDC.
No new manual result is inferred from approval or automation.

Original release ID `428196589`, annotated tag object `769d52fb09116b787184d119ff1db6075187e36d`
and peeled commit `cd70a0b8a20d04c246a826c7b1bf616de2721c65` remain the guarded original identities.
No merge, deletion, replacement tag, replacement workflow or new release has occurred. The
state-aware plan above remains binding; defer P2/P3 on the previously recorded reopen conditions.

The first full implementation suite produced 10592 checks / 1 failure: State 43 still expected
a host latency report inside an unfinished adoption, which ADR-0068 deliberately defers. Its
engine-priming assertion is preserved against the actual engine latency, and two new assertions
require deferred in-flight reporting and correct final committed reporting. No production
workaround or unrelated assertion removal was used. The final run is recorded separately.

Citation maintenance retained all existing re-aim declarations, moved their current targets
with source shifts and added six narrow inspected edited-span transitions. Two old anchors
claimed `toEngine` while pointing into layout/bind; they now identify the actual function.
One historical changelog citation is qualified by its already-stated `10fbfa0` commit to retain
its original line identity. Every historical description/order and other byte is preserved.

### Fresh local implementation validation (GCC 14.2.0 Release / JUCE 9.0.4)

The complete VST3, Standalone, DSP and state targets build successfully. Source checker pairs
(structure, release-note content, portability, realtime, dispatch, state coverage), warning
checker self-tests and workflow routing/permission/actual-creation controls pass. Citation
liveness now has 276 cases, 610 anchors pass against main, and documentation has 700 liveness
cases / 174 clean files. The real-shell preflight suite passes all eight cases.

**Local aggregate ABI failure:** after source gates pass, the real built-output ABI gate rejects
both GCC 14 binaries for `CXXABI_1.3.15` above the declared `CXXABI_1.3.14` floor. This local
non-shipping toolchain result is not a green preflight or a shipping ABI result. No floor,
checker, compiler pin or mandatory job is relaxed. Full suites run separately, and the
mandatory Clang shipping build must verify actual candidate ABI before release preparation
can proceed. Existing GCC/JUCE warning debt and Python invalid-escape warnings remain visible.

### Completed local behavior and real-time evidence

Initial implementation commit: `0718bfdaaf33a1d68c7e7c2b7c5f6355897f3392` (PR #187). These
measurements describe that implementation before the fixed-point follow-up below; they are
not substituted for final-source results.

| Check | Actual result | Scope / qualification |
| --- | --- | --- |
| Full DSP suite | 945 checks / 0 failures | Existing allocation-guard liveness: new, aligned new and malloc-family all live |
| Full state suite | 10594 checks / 0 failures | Includes State 146/147/148 and the approved State 43 timing assertions |
| Linux VST3 pluginval strictness 10, deterministic | 3/3 passes, all first attempts | Locked v1.0.4 and unchanged production runner |
| Linux VST3 pluginval strictness 10, randomise | 3/3 passes | Pass 1 first attempt exited 139, then passed on permitted crash-only retry; passes 2/3 first attempts |
| Original restore-lock probe | Outer-held edge still witnessed; final Width 0.6 / preset newer | No supported-host deadlock claimed; latency delivery occurs after safe admission |
| Realtime source closure | 97 liveness cases; 51 files, 0 violations | Includes the processor snapshot reader and unchanged atomic-order gate |
| Snapshot/engine coverage | 110 liveness cases; all 36 fields and 12 modules | Both reset scopes retained |
| Preflight entry regressions | 8/8 | Real aggregate shell, note checker and awk; isolated fixtures |

The randomise crash log contains `Segmentation fault` / exit 139 and no validation assertion
failure; the unchanged runner classifies it as the Linux crash-only category allowed by policy.
No backtrace was emitted, so this particular crash's frame is not independently attributed to
XEmbed. It is not counted as an assertion retry or an all-first-attempt result.

DSP allocation checks observed 2916 and 664 armed event/process calls with new=0/malloc=0,
and Test 75's 7791 handshake cases reported zero allocations. The actual GCC 14 processor
`ParamPointers::toEngine` disassembly contains no call instruction (388 assembly lines); its
bounded finite checks and atomic reads add no allocator, wait or library callback to that
snapshot boundary. This is bounded x86-64 codegen evidence, not a substitute for the mandatory
RTSan, TSan and native platform jobs. No test-only allocation interposer is added to production.

### Candidate CI and artifact identity

The implementation push is [Build & Validate 1950](https://github.com/skyRolly/Anamorph/actions/runs/40953216526),
with PR merge checks [1951](https://github.com/skyRolly/Anamorph/actions/runs/40953224042),
[CodeQL](https://github.com/skyRolly/Anamorph/actions/runs/40953224090) and
[MSVC analysis](https://github.com/skyRolly/Anamorph/actions/runs/40953224142). At this record's
write time, source/docs, RTSan, TSan, seeded fuzzing, GCC LTO/warnings and Windows AVX2 identity
checks have completed successfully; native packaging/ABI, remaining platform and analysis jobs
are still running. No aggregate/native pass is inferred from partial results.

This follow-up changes documentation only: API/reference/audit now distinguish finite DSP
interpretation from unchanged stored identity, inventories include local note checking, and
LEVEL5's old 0.9.9 heading is explicitly historical. Final-head checks and artifact identities
are reported on [PR #187](https://github.com/skyRolly/Anamorph/pull/187). They must all be verified
on that exact candidate before merge; prior-code CI is not a substitute. A final tag must be on
validated approved main, and replacement must rebuild all payloads through normal Release.

The original draft was re-read after the implementation push: ID `428196589`, title
`Anamorph 0.10.0`, 11 assets, `draft=true`, `published_at=null`, tag object and peeled source
still exactly match the archived original. No release/tag mutation or publication occurred.

The remaining human gate is the targeted changed-behavior scope in LEVEL5_AUDITION and
RELEASE_COMPATIBILITY_CHECKLIST. The owner's architecture approval and original-candidate
manual approval remain recorded separately. Do not merge or start the guarded original
draft/tag deletion until targeted validation is actually attested and mandatory head CI passes.

### Callback-published restore fixed-point follow-up

Review of the actual message-thread getter found one missing schedule: the latency callback
starts/joins a newer background restore and simply returns, with no nested pump or save.
The first implementation flushes notification after its drain loop, so the callback can leave
new sound/pending metadata before the outer getter serializes it. Extended State 148 repeats
this on getter/timer doors and checks already-captured bytes rather than re-saving to repair
them. Against `0718bfd` production code it measures **2011 checks / 54 failures**. This is a
real negative control for the follow-up, separate from the original 4/30 F9 failures.

`adoptPendingHostState` now rechecks after the notification and applies a callback-published
restore in a fresh scope before returning. Each previous restore and its metadata are already
fully committed; no write from that older payload resumes. Each non-blocking pass retains
transaction/try-lock refusal, consumes nothing on refusal and adds no blocking operation.
This implements the already-approved ADR-0036/0068 fixed-point contract, not a new callback
support boundary, thread path or atomic ordering. The targeted regression is **2011 / 0**, and
the full final suites are **DSP 945 / 0, state 10827 / 0**.

ADR-0068, TESTING, F9/INC-020 and the current handover describe this final case. Earlier
1778/10594 results remain historical measurements of the first implementation. The final
plugin builds/pluginval runs and exact new-head native/sanitizer/analysis evidence remain
required; older passing jobs do not validate this follow-up. Original release/tag remain intact.

Final follow-up local execution completed: VST3 and Standalone rebuild succeeded; full DSP
**945 / 0** and state **10827 / 0**; locked pluginval strictness 10 deterministic **3/3** and
randomise **3/3**, every final-source pass on its first attempt (no crash retry). The unchanged
restore-lock probe still witnesses the outer-held edge and finishes at Width 0.6 / preset newer.
The earlier randomise retry above belongs only to the initial implementation run.

All source checker pairs and warning-gate liveness checks were repeated for the follow-up.
Documentation 700-case liveness / 174 clean files and citation 276-case liveness pass; 609
base-owned anchors retain their text against main and the implementation parent. Existing
re-aim declarations were preserved and four current targets followed the seven-line source
shift; no new exemptions were needed for the fixed-point follow-up. API/audit text explicitly
separates finite rendering from invalid stored identity. No warning baseline, parser boundary,
registry field, version, release-creation safeguard or native validation gate is weakened.

The initial source's official Clang Linux job `123726496197` passed its actual shipped-ABI
floor and first-party warning gates. Thus the earlier GCC 14 local aggregate rejection is not
a claimed shipping artifact regression. The new exact-head Clang/native/sanitizer/analysis
checks remain required independently and are exposed on PR #187; the initial run cannot
validate the fixed-point follow-up. Targeted human validation remains pending on the actual
new candidate binaries. No merge or draft/tag replacement is performed before that gate.

### Native ARM display-oracle correction

Initial Build 1950 / source `0718bfd` completed with one failed job: Apple Silicon macOS.
The actual state log reports **10439 checks / 4 failures**, all State 147 Output Gain host/text
NaN display comparisons: actual `0.0 dB`, finite-twin expectation `-0.0 dB`. Linux, Windows,
native Intel, GCC LTO/warnings, source/docs, RTSan, TSan, fuzz and the other completed gates
were successful; CodeQL and MSVC analysis also succeeded. No complete candidate pass is
claimed for that initial source.

The NaN formatter deliberately uses the declared plain +0 dB default. ARM's existing finite
range/snap round-trip can produce a value whose display is -0.0 dB. The new test incorrectly
required those different formatting paths to have the same sign. Its NaN Output Gain oracle
now explicitly requires the existing `0.0 dB` text, while every finite display and other
parameter comparison remains exact. No production formatter, finite arithmetic or readback
is changed; no blanket signed-zero normalization or assertion removal is introduced. Native
ARM CI must verify this correction on the final source; a local GCC pass does not do so.

After the display-oracle correction, the complete local suites were rebuilt/re-run: DSP **945 / 0**,
state **10827 / 0**. No production code changed after the final six first-attempt pluginval
passes. The final exact-head native matrix must confirm the ARM-specific oracle.

## Final candidate validation and human completion — 2026-10-30, Asia/Shanghai

Runtime source `9f9a76ea9ea848f406362553ed29b51770ac375a` is validated by
[Build 1952](https://github.com/skyRolly/Anamorph/actions/runs/40988058477): all 13 running
jobs succeeded; the three optional/event-specific jobs were skipped. The PR merge check
(40988061775), CodeQL (40988061774), MSVC analysis (40988061830) and dependency review
(40988061799) also succeeded. Valgrind reports zero errors from zero contexts for both
suites, with zero suppressions. Native ARM and Rosetta validate the display-oracle correction.

Five candidate platform/installer archives were downloaded, matched to API sizes/SHA-256
digests and source/run identities, and inspected. Windows app/VST3 versions and installer
ProductVersion are 0.10.0; the generic Inno loader's fixed numeric version remains zero as
before. macOS plists and all three component PackageInfo versions are 0.10.0, with both
arm64/x86_64 slices and the expected namespace. The downloaded Linux payload independently
passes the unchanged ABI checker: CXXABI_1.3.9, GLIBC_2.38, GLIBCXX_3.4.31. This does not
turn the earlier local GCC 14 aggregate failure into a pass.

The security-alert API returned integration HTTP 403, so the completed SARIF artifacts were
downloaded and compared with the preimplementation candidate. Raw counts remain 65 C++
CodeQL, 0 Actions and 192 PREfast. Only existing C6262 stack-byte estimates change with the
larger immutable parameter bindings; no diagnostic threshold or suppression was changed.
Candidate provenance/digests and validation are committed in `release-0.10.0-p1/validation.json`.

**Targeted human gate cleared.** The owner directly replies “Human validation has been
completed” to the request identifying candidate `9f9a76e` and its verified downloads, invalid
input audio/automation recovery and session/preset/A-B/Settings/PDC scope. Record this as the
owner's completion attestation, not tests performed by the agent. Actual test date, DAW/OS/CPU,
format/device/session, individual outcomes and selected artifact hash remain NOT RECORDED.
The prior architecture approval and original candidate's unrelated manual approval are retained.

The next source changes record this evidence only; runtime code, tests, CMake version,
workflow behavior, metadata gates and all fixtures remain unchanged. Final main must pass
the mandatory CI gates before original draft/tag deletion. The normal recreated-tag Release
workflow must build fresh payloads, and every new release asset/manifest/note must be verified.
No public publication is authorized. At this attestation record, original ID 428196589 and
tag object 769d52fb09116b787184d119ff1db6075187e36d still exist unchanged and unpublished.

## Executed P1 draft replacement — 2026-10-30, Asia/Shanghai

The owner's explicit draft-only one-time supersession authorization was executed after the
approved P1 implementation and targeted human completion. PR #187 merged into main at
`b085c87d139f124e33a525e75ec13b8218c96f06`. Runtime code, tests, CMake version, scripts and workflows match the human-validated
`9f9a76e` candidate exactly; the attestation commit changes documentation/evidence only.
Final-main [Build 1956](https://github.com/skyRolly/Anamorph/actions/runs/40997892863) and companion CodeQL/MSVC analyses passed before
either original object was deleted. No automated gate was waived or weakened.

| Identity | Archived original candidate | Verified replacement |
|---|---|---|
| Version / title | 0.10.0 / Anamorph 0.10.0 | 0.10.0 / Anamorph 0.10.0 |
| Annotated tag object | `769d52fb09116b787184d119ff1db6075187e36d` | `7229ec12472649a0d9be0cd563a94e713bc206be` |
| Peeled source | `cd70a0b8a20d04c246a826c7b1bf616de2721c65` | `b085c87d139f124e33a525e75ec13b8218c96f06` |
| GitHub Release ID | 428196589, deleted | [428664381](https://github.com/skyRolly/Anamorph/releases/tag/untagged-e9b61aa49fbd4b5cb114) |
| Source build/run | 1940 / 40915335616 | Normal Release 5 / [41001157732](https://github.com/skyRolly/Anamorph/actions/runs/41001157732) |
| Publication state | Draft, published_at null before deletion | **Draft, published_at null** |
| Assets | 11 authenticated restricted recovery copies retained | 11 fresh attached assets, all downloaded/verified |

Immediately before deletion, the original Release ID/body/assets, annotated object/peeled
source, unpublished status, all 11 backup sizes/hashes and absence of a conflicting Release run
were re-read and verified. No evidence of prior public publication or external reliance was known.
Supported `gh release delete 0.10.0 --cleanup-tag --yes` removed the original draft/tag;
both absences were checked. Only the matching stale local tag was removed. Main/checkout/version
and notes were rechecked, a new annotated tag was created on the validated main commit and
pushed normally. The old ref was never force-moved. Retry checkpoints distinguish original,
absent and replacement identities; no deletion was repeated against the replacement.

The tag push triggered the normal Release workflow, including its full reusable Build matrix
and unchanged caller permissions. The new draft was created with exact target `b085c87d139f124e33a525e75ec13b8218c96f06`,
title `Anamorph 0.10.0` and customer-facing notes matching the tagged changelog extraction.
Exactly one active release names 0.10.0. All mandatory native, sanitizer/threading/fuzz,
pluginval, source, packaging and release metadata gates passed. Final-main SARIF findings
match the fully validated candidate after path normalization: 65 C++ CodeQL, 0 Actions,
192 PREfast, including unchanged frame estimates. Existing findings are not claimed absent.

Every release asset's downloaded size and SHA-256 matches the GitHub API. All nine payload
entries agree with SHA256SUMS and RELEASE_MANIFEST, which identifies this exact source,
normal Release run and About-box build number. All files in the three public ZIPs match
the corresponding fresh same-run CI artifacts; both installers match their same-run files
byte for byte. All five binary payload hashes differ from the archived original candidate.
No stale original binaries or debug artifacts are attached. Unix executable modes are present.
Actual Windows binary versions/installer ProductVersion and macOS plist/component versions are
0.10.0; universal slices, bundle IDs, package receipts and install locations match the contract.
Inno's generic loader retains its existing zero fixed numeric version; its application
ProductVersion is correctly 0.10.0. Downloaded Linux binaries pass the unchanged ABI gate.
The manual, NOTICE, THIRD_PARTY_LICENSES and SUPPORT match the tagged source byte for byte.
Full asset IDs, sizes/hashes, normal-run artifact identities and inspection results are in
`release-0.10.0-p1/draft-replacement.json`.

The 2026-10-30 targeted manual completion and original 2026-10-29 unrelated approval remain
owner attestations; unprovided DAW/OS/format, actual test date and individual observations remain
NOT RECORDED. No new listening result is inferred from CI or from the metadata-only follow-up.
This follow-up records the now-observed release state after tag creation. It does not move the
immutable replacement tag or change any release payload or its provenance.

**Remaining scope:** no P1/draft-validation blocker remains. The distinct RISK-009 outer-held
lock/JUCE listener-lock residual is not a demonstrated supported-host deadlock. RISK-015 parser
acceptance, KI-015 commercial/JUCE/distribution/signing/notarization decisions and deferred P3
items retain their recorded reopen conditions. macOS bundles remain ad-hoc signed and not
notarized; Windows installers remain unsigned. Public publication needs a separate owner
decision and applicable distribution prerequisites. This task leaves the release **unpublished**.

## Original-request completion reconciliation — 2026-10-30, Asia/Shanghai

After the owner restored the full original request and reconfirmed human verification,
the release, tag, source, merged PRs and CI were re-read from GitHub. The original release
428196589 returns 404. Exactly one active 0.10.0 release exists: replacement 428664381,
`draft=true`, `published_at=null`, title Anamorph 0.10.0, exact target `b085c87`.
Annotated tag object `7229ec12472649a0d9be0cd563a94e713bc206be` still peels to that source.
Normal tag-push Release 5 / run 41001157732 and final-main Build 1956 remain completed
successfully; all mandatory Release jobs pass, with only the three documented optional/
event-specific jobs skipped. PR #187 and the execution-evidence PR #188 are merged.

All 11 current release assets were downloaded again into a fresh audit directory.
Their IDs, sizes, update times and SHA-256 values match the API and committed
`release-0.10.0-p1/draft-replacement.json`; all nine checksum/manifest payload entries match.
The five same-run CI artifact identities/digests were refreshed and their retained ZIPs
rehashed. Every file in each customer platform ZIP and both installer payloads matches
those verified same-run files. CRCs, executable modes, macOS versions/identifiers and
universal slices, VST3 metadata and tagged manual/attribution/support bytes were checked.
Previously recorded Windows/installer format inspections apply to the exact same hashes.
All five binary payload hashes remain different from the archived original candidate.
The remote description equals the tagged changelog extraction byte for byte.

The production finite-default snapshot and restore completion/recheck paths were inspected
directly. Runtime, tests, scripts, workflows and version are unchanged between the
human-validated `9f9a76e`, released `b085c87` and the subsequent evidence-only main change.
The parameter registry fixture is unchanged from the original release. The owner's renewed
manual-completion statement adds no inferred DAW, device, date or per-scenario result.

The remaining original-request work was documentation reconciliation: the state-hardening
worklog and repository map still called Accepted ADR-0067/0068 unapproved proposals;
KNOWN_ISSUES/FUTURE_RISKS and INC-019/020 still described completed P1 validation as pending. Their current
summaries now reflect execution, preserving historical measurements and the distinct
RISK-009 residual. RELEASE_PROCESS's original recovery is explicitly historical.
P2/P3 dispositions and their concrete reopen conditions above are unchanged.

The later native Intel AU teardown investigation remains separately tracked in draft
PR #189; no cause or crash fix is claimed here and none of its experimental source is
added to this release. No new release/tag operation is needed or performed against the
verified replacement. The original request's P1, approval, replacement and asset-validation
requirements are complete. Publication remains a separate, unauthorized owner action.
