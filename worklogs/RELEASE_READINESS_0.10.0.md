# Release readiness — 0.10.0 (draft only)

**Current status: P1 replacement BLOCKED on human Architecture Review (2026-10-30,
Asia/Shanghai).** Proposed ADR-0067/0068 and current-code reproductions are prepared. Neither
P1 is closed and no replacement source, tag or release exists. The original draft/tag remain
intact. See **P1 replacement decision and execution record** below.

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

### Current authority and mandatory stop

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

### Current-code reproductions and decision scope

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

**Open gate:** human Architecture Review of ADR-0067/0068, followed by implementation, fresh
mandatory validation and targeted manual approval. Replacement tag/commit/Release ID/workflow
and asset verification are **not available because replacement has not begun**. Original green
checks and owner approval are preserved as historical evidence, not substituted for this gate.
