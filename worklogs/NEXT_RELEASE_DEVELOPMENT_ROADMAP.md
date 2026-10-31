# Next release investigation and development roadmap

**Investigated:** 2026-10-31 UTC. **Original investigation status:** reviewed/merged as PR #194. **Current status:** execution active.
**Repository:** `skyRolly/Anamorph`. **Baseline:** `main` at
`4c136881f245470577d66a7603f15b177ddcf3dc` (PR #193).
**Original investigation scope:** documentation, read-only inspection and existing checkers;
no product or release changes. Later execution is separately authorized as recorded below.

This is the current **forward execution register**. It replaces the forward priorities in
the older product-readiness/global-review roadmaps and the historical residual list in
[RELEASE_READINESS_0.10.0.md](RELEASE_READINESS_0.10.0.md), not their measurements or approvals.
That readiness worklog remains the release execution/history record; the
[engineering-review programme](engineering-review/ENGINEERING_REVIEW_PROGRAMME.md) remains the
historical finding register. Update the NR IDs here as subsequent assignments execute.
Worklogs do not override [source authority](../docs/SOURCE_OF_TRUTH.md), policies or Accepted ADRs.

## PR #196 independent review — 2026-11-01 (Asia/Shanghai)

**Disposition: preserve the implementation.** `abSwitchToAdopted` polls the existing
coalescer after same-slot refusal and before the outgoing slot's `syncCommitted`
reset. Both switch APIs reach it after existing restore admission. The adopted helper
does not drain again or introduce an audio call, lock, atomic ordering or callback
route. It respects open gestures/transactions and records the existing owned-parameter
endpoints; Undo/Redo, preset flush and restore-cleared histories keep their contracts.
No new architecture decision or product correction was justified by this review.

Fresh deterministic negative control in the retained Debian 13 environment, Clang
22.1.8 Release: State 149 with **exact `origin/main` PluginProcessor.cpp** and current
tests fails **10/126**; restoring the reviewed processor passes **126/126**, before
and after the control. No sleeps, expectation changes or source changes were kept.
The test covers explicit/toggle switches, repeat cycles, independent histories, exact
step counts, same-slot no-op, empty gestures and post-gesture host automation.
The full state suite retains preset/restore/pending-state and deferred-command
boundaries. Source inputs remain identical to reviewed source head `4a431ca`.

Refreshed historical CI: Build 41216103432, merge-check 41216269049, CodeQL
41217038192 and MSVC 41217041554 are now **completed/success**, including the formerly
pending ARM macOS and sanitizers. This refresh does not certify a later head: the
additive parent-sync/review commit must receive its own Build/merge-check, and the
existing analyzer workflows will be dispatched on its exact ref without changing
their filters. Preserve the historical rejected GCC 14 ABI preflight; a new Clang
result cannot convert it into a pass. No owner real-machine approval is assigned to
this new code, and no merge or next roadmap implementation is authorized.

## PR #195 review disposition — 2026-11-01 (Asia/Shanghai)

Owner handoff findings accepted with two precision corrections: the malformed ADR-0065
row contains **four**, not five, cells under three headings; RELEASE_PROCESS's obsolete
paragraph refers to the Linux correction context rather than naming #193 explicitly.
Both defects are real. The index now preserves historical acceptance/helper amendment,
PR #175 implementation and supersession by Accepted ADR-0069/PR #193 in the intended
columns. A same-file audit also found and escaped ADR-0050's pre-existing `||` table
separator. SOURCE_OF_TRUTH now labels its older desktop effectivity as historical.

Release instructions now distinguish completed #193 merge at `4c13688`, Release 7
run 41173229942 and draft 429760301 replacement from current verification and future
authorized operations. Fresh PR/tag/run/release APIs confirm that history, all 11
assets and unpublished status; no rebuild or release modification was performed.
Historical approval/manual records remain intact. NR-003 remains Deferred.

At review start all four PRs had no submitted reviews, inline threads or conversation
comments through the GitHub APIs; the owner's findings are tracked here regardless.
Existing heads `f64f9d1` → `4a431ca` → `017a2e4` → `ccf1743` were preserved locally.
Repairs are additive and descendants will merge their updated parents without
rewriting shared history. This branch's review correction commit identifies its head;
final stack heads/CI evidence will be recorded in #198's handoff and PR discussions.
All four PRs must remain open and unmerged. No auto-merge, release operation or next
implementation batch is authorized; explicit owner merge authorization is required.

## Current execution — owner decisions recorded 2026-10-31 (Asia/Shanghai)

Execution base: `6ae102ca72d83eb07c0a84d76465b26c457cdc16` (merged PR #194);
product/test/packaging/build/workflow inputs equal the investigated `4c13688` candidate.
The owner explicitly confirms all relevant ADRs manually approved and real-machine
testing manually completed and approved. This is the authority for NR-001/002 closure.
ADR-0069 becomes Accepted; ADR-0065 remains Superseded. Other existing ADR lifecycle
states are preserved; no unwritten future architecture is pre-approved.

**NR-001 Completed; NR-002 Completed; applicable NR-016 Completed** in the owner-decision
review branch. Integration is pending human review/merge. Real-machine results are
recorded as owner-attested; actual test dates, hosts/versions, hardware, format/device,
individual outcomes and selected tested-artifact hashes remain NOT RECORDED.
**NR-003 Deferred** by explicit owner instruction; no unexecuted matrix case is passed.
These decisions supersede the initial missing-approval/manual-evidence observations below.

NR-022 is now confirmed, implemented and locally validated on its separate review branch
(commit `0e5e644`); native CI and human PR review remain pending. Next: NR-006
architecture packet and NR-017 static-diagnostic inventory; neither authorizes speculative runtime work.
NR-004/005/007 retain their recurrence triggers. NR-013 still needs a pinned reproducing
REAPER environment; the general real-machine attestation does not prove that specific
reported focus failure fixed. NR-018/019 owner/legal/provisioning blockers remain.
No PR merge, tag change, release modification or publication is authorized.

## 1. Recommendation

**Keep 0.10.0 as the next release candidate. It is already tagged and rebuilt, but not published.**
The Linux decision/manual gates are now closed by the owner confirmation above; do not repeat
the completed P1 fixes, automatically rebuild the draft, or bump a version merely to start work.
No confirmed P0 defect was established. There are **22 forward items: P0 0 / P1 3 / P2 16 / P3 3**.
These count decision, validation and enhancement work as well as investigations; they are not
22 confirmed product defects. The P1 items concern the Linux approval record, targeted Linux
acceptance, and conditional public/commercial distribution decisions. No open P1 runtime defect
is established by this investigation.

The short path is **NR-001 → NR-002 → NR-021**, with NR-018 required before the owner selects a
public/commercial distribution route. The other phases are incremental development lanes, not
prerequisites that force every deferred item into 0.10.0.

## 2. Verified baseline

### 2.1 Repository, versions and release identities

Evidence [Verified]: fresh `git fetch origin --prune --tags`, GitHub repository/PR APIs,
authenticated Releases REST listing, annotated tag object, downloaded manifest and assets.
API/read-only evidence was collected on 2026-10-31; these are snapshots, not permanent claims
about a mutable draft.

| Surface | Observed state |
|---|---|
| Default branch / head | `main` / `4c136881f245470577d66a7603f15b177ddcf3dc` |
| Source version | `project(Anamorph VERSION 0.10.0 ...)` in `CMakeLists.txt`; empty `[Unreleased]`, dated `[0.10.0] — 2026-10-30` in `CHANGELOG.md` |
| Dependencies / language | JUCE 9.0.4, immutable `3fb5cbc0340758596dac39f7c8de47d562b4896e`; C++23; pluginval 1.0.4 content-locked through `scripts/pluginval-release.json` |
| Current annotated tag | `0.10.0`, object `1190e455c7c3a1b97bd4262360fdbeb8cea3c5b0`, peels to the baseline; tagger date 2026-10-31 05:35:42 UTC |
| Current 0.10.0 release | ID **429760301**, title `Anamorph 0.10.0`, `draft=true`, `prerelease=false`, `published_at=null`, target commit = baseline; [draft URL](https://github.com/skyRolly/Anamorph/releases/tag/untagged-5c0f02d1d702f1d4bf30) |
| Current release build | [Release run 41173229942](https://github.com/skyRolly/Anamorph/actions/runs/41173229942), About build **7**; manifest timestamp 2026-10-31 06:17:41 UTC |
| Older version | Annotated `0.9.9` at `c4f390f9cccaa9551d680cb1305fa4a669b24587`; release ID 419665361 is also Draft, `published_at=null` |
| Other tag | `archive/recovery-0.10.0-draft-assets-20261031` is a recovery reference, not a later product version |
| Publication | The authenticated listing contains two releases, both drafts. No published GitHub Release or version superseding 0.10.0 was observed. A dated changelog/tag alone does not mean publication. |

The current tag annotation explicitly records replacement of draft 429645975 and tag object
`13e59971da17aa68adb67400798dc02c94d7dac7` after manually merged PR #193. The readiness worklog's
P1 draft 428664381 / source `b085c87` / Release 5 is **historical**, not the current candidate.
The archived original and P1 JSON records remain provenance, not current release metadata.
This assignment observed those later operations; it did not perform or authorize them.

### 2.2 Recent integration and review state

Evidence [Verified]: GitHub metadata confirms all rows below merged into `main`; ancestry was
checked locally. No open PRs or issues were returned by repository-scoped searches. Issue-only
search also returned no historical issue records. Worklogs, rather than issue tickets, carry most
of this project's backlog. Fifteen relevant recent PRs were checked for review threads:
#193, #192, #191, #190, #189, #188, #187, #186, #185, #184, #183, #182, #181, #178, #177;
each returned an empty thread list. Conversation retrieval for #193/#191/#189 was also empty.
This establishes no unresolved GitHub thread in that inspected set, not universal design approval.

| PR / main merge | Result and present disposition |
|---|---|
| [#193](https://github.com/skyRolly/Anamorph/pull/193) / `4c13688` | Linux legacy desktop cleanup removed; preservation is current behavior |
| [#191](https://github.com/skyRolly/Anamorph/pull/191) / `cf51bc3` | Linux VST3-only installer, executable-mode/package/native gates; its legacy-cleanup feature was superseded by #193 |
| [#192](https://github.com/skyRolly/Anamorph/pull/192) / `16caf34` | Three CodeQL action pins updated to v4.39.1 |
| [#189](https://github.com/skyRolly/Anamorph/pull/189) / `2e578c3` | Bounded native Intel AU investigation/controls; no product fix; RISK-017 retained |
| [#190](https://github.com/skyRolly/Anamorph/pull/190), [#188](https://github.com/skyRolly/Anamorph/pull/188) | Reconciled the earlier P1 draft's completion; later Linux rebuild now supersedes that candidate |
| [#187](https://github.com/skyRolly/Anamorph/pull/187) / `b085c87` | Non-finite snapshot/display/restore-window and nested-restore commit fixes, Accepted ADR-0067/0068 |
| [#185](https://github.com/skyRolly/Anamorph/pull/185) / `a5973c6` | Release caller permission startup failure repaired; later normal Release runs succeed |
| [#183](https://github.com/skyRolly/Anamorph/pull/183), [#182](https://github.com/skyRolly/Anamorph/pull/182), [#181](https://github.com/skyRolly/Anamorph/pull/181) | Native macOS icon correction; strict Windows dump parsing; pluginval acquisition/cache integrity implemented |

### 2.3 CI, failures and what a green result means

Evidence [Verified]: workflow source, latest job/step APIs and selected failure/current logs.
Latest baseline [Build & Validate 41167465458](https://github.com/skyRolly/Anamorph/actions/runs/41167465458)
has 13 successful executing jobs and three expected skips (PR merge-check and optional macOS
selection/co-installation consumers). Baseline [CodeQL 41167465482](https://github.com/skyRolly/Anamorph/actions/runs/41167465482)
and [MSVC analysis 41167465497](https://github.com/skyRolly/Anamorph/actions/runs/41167465497)
also succeeded. Release 41173229942 has 15 successful jobs, including metadata validation and
draft creation, and the same three expected skips.

The reusable Build workflow covers Linux Clang shipping builds, Windows/MSVC, universal macOS,
native Intel macOS, GCC LTO, ASan/UBSan/Valgrind, RTSan, TSan, seeded state fuzzing, docs/source
checks and Windows AVX2 differential output. The macOS cross-slice numerical report is explicitly
non-blocking; it must not be cited as a cross-architecture identity guarantee. Ordinary VST3
pluginval on all platforms and AU on ARM/native Intel remain blocking at configured strictness
**10**, deterministic and randomized, three passes per group. The current Release Linux log
records **945 DSP / 10,827 state checks, zero failures**, also exercising State 146–148.

The seven workflow files are `build.yml`, `release.yml`, `codeql.yml`, `msvc.yml`,
`dependency-review.yml`, plus the dispatch-only `rosetta-scope.yml` and
`au-teardown-diagnostic.yml`. The latter two are diagnostic consumers, not substitutes for the
normal build. A sample of the latest 300 runs (2026-10-26 09:26:23 through 2026-10-31 05:35:46 UTC)
contains 252 success, 33 cancelled, 14 failure and one startup_failure. It includes experimental
branches/controls; **it is not a product reliability rate**. Cancellation is not a defect count.

| Failure evidence | Current interpretation |
|---|---|
| [41005362987](https://github.com/skyRolly/Anamorph/actions/runs/41005362987), native Intel AU randomise, job 123898933769 | Post-SUCCESS `bad_function_call` / exit 9; recurring diagnostic captures on macOS 15/26. Root cause unresolved, owner permits occasional CI failure; NR-004. Every ordinary macOS crash still fails immediately. |
| [40953216526](https://github.com/skyRolly/Anamorph/actions/runs/40953216526), job 123726496183 | ARM display oracle expected `-0.0 dB` where the specified NaN default formats `0.0 dB`; corrected in PR #187. Current gate passes; do not reopen the fixed parameter defect. |
| [40880140543](https://github.com/skyRolly/Anamorph/actions/runs/40880140543), job 123469156120 | 16pt macOS icon comparison failed on an intermediate #183 head; current native generator/staged icon gates pass. Completed work. |
| [40599339270](https://github.com/skyRolly/Anamorph/actions/runs/40599339270), job 122523254764 | Rosetta Test 69 floating equality failed; current test requires literal +0 bits with negative controls. Historical comparison cause remains unknown; separate from Test 61 and AU teardown. NR-007. |
| [40221042187](https://github.com/skyRolly/Anamorph/actions/runs/40221042187), outside the 300-run sample | Rosetta Test 61 newest-frame assertion failed; captured printout does not establish cause. Current runs pass; NR-007 retains the original record. |
| [40349275028](https://github.com/skyRolly/Anamorph/actions/runs/40349275028), 40338130238, 40323699873 | Bounded Rosetta debugger attach/cleanup failures; not evidence of a DSP fix or a justified test waiver. NR-007. |
| Original Release startup_failure / PR #185 | Static nested-workflow permission mismatch repaired, with routing liveness controls; subsequent releases run normally. Closed, not a new blocker. |

Code-scanning alert-state retrieval still returns **HTTP 403, “Resource not accessible by
integration”**. Workflow success does not prove zero diagnostics. Historical SARIF counts in the
P1 worklog are not today's open-alert count; NR-017 must obtain/triage current evidence.

### 2.4 Installation, packages and actual downloaded bytes

All **11** assets of draft 429760301 were downloaded read-only. Sizes and SHA-256 values match
the GitHub asset API digests. The nine payload/document assets also match both SHA256SUMS and
the manifest; the two index files have independent API digests. No installer was run on this
workspace. Full hashes are preserved here so evidence survives a future mutable-draft replacement.

| Asset | SHA-256 |
|---|---|
| `Anamorph-0.10.0-Linux.zip` | `033188e31a1d53c78141cbfdf15f19e4b3c99cb439cb97104df088219c3ad9a3` |
| `Anamorph-0.10.0-Windows.zip` | `beef3082c9bc83b320d67ed456292134292a44ccf809b4c6b2a4f08210075c22` |
| `Anamorph-0.10.0-Windows-Installer.exe` | `cfb47d01ff00bb6c26fff4ffb377a69fd6065313a5115dc186e8b7ac52b6b46e` |
| `Anamorph-0.10.0-macOS.zip` | `80f90e98af5b80f0ba0d527159724540a0a3c434c4cd8c0bc11287eec18c7dd0` |
| `Anamorph-0.10.0-macOS.pkg` | `75a458f212375e45311459351f4e1f923c6093fe4650df4f17f3b902f70a47ca` |
| `Anamorph-0.10.0-UserManual.md` | `074ff9c913aa1bfe12fa21e0e805ee044a6ff24d76781b060df34ac5e3de5610` |
| `Anamorph-0.10.0-NOTICE.txt` | `ffc0e6d6e3dbce4355325805f50e25d8a5b6322e0a52143304a988de403be7f2` |
| `Anamorph-0.10.0-THIRD_PARTY_LICENSES.md` | `47f623c519505dc9c7ba02ba27c8f360c19b67f7633a3e56e2714f7a69ddf081` |
| `Anamorph-0.10.0-SUPPORT.md` | `00ebf8f6ebc80888a999cee28b500f4f11d132b06b19a12fd3e3fa86cdd9dd92` |
| `RELEASE_MANIFEST.txt` | `b61646ee91443d55e0e18e33f0db948c6f9982c0a5a3b8723f4d321ed7570beb` |
| `SHA256SUMS.txt` | `5531e19b4ab20a3781aa0fcc40fddd8334c92fb702ba77d116287180263d86d1` |

Evidence [Verified]:

- **Linux:** ZIP contains exactly the intended top-level VST3, portable `Anamorph`, two entry
  scripts and INSTALL.txt. Both scripts and both ELF payloads have 0755 archive modes; script
  and INSTALL bytes match baseline source. Existing package checker self-test/staged check pass
  on an extracted scratch copy with archive modes restored. Existing ABI checker/self-test pass:
  CXXABI_1.3.9 / GLIBC_2.38 / GLIBCXX_3.4.31 (within its declared floors). Native artifact
  [13031478194](https://github.com/skyRolly/Anamorph/actions/runs/41173229942/artifacts/13031478194)
  SHA-256 `84c82843a5a1f1523f7d8a0cfeb55be8900a446c94fed17e65904e475f416497`
  was independently downloaded/inspected: exact baseline source, **17 tests / 0 failures / 0
  errors**, real UIDs and sudo in private mount/PID/network roots, no user namespace or security
  setting changes, `customer_archive_eligible=true`. All six staged-file hashes match the ZIP.
  The current scripts install/remove VST3 only and leave **all** legacy launchers, icons,
  receipts, journals and old installed/staged Standalone copies untouched. Do not resurrect
  #191's cleanup test or infer that manual stale-launcher removal has occurred.
- **Windows:** actual release EXE/ZIP digests verified; baseline Release job's install/reinstall/
  uninstall and icon gates passed. Downloaded evidence artifact
  [13032576591](https://github.com/skyRolly/Anamorph/actions/runs/41173229942/artifacts/13032576591),
  SHA-256 `d70d68fca05e087cec39cbaddc4ec0cd6343e75fcb12b1b3f647b81e08e0cf01`,
  records no failure across VST3-only, Standalone-only and combined configurations, native-copy
  equality, stable uninstall icon, and unmanaged-file preservation. This is isolated native
  installer evidence, not a SmartScreen/GUI/DAW certification.
- **macOS:** downloaded ZIP plists report 0.10.0 and `tech.rolly.anamorph` for app/AU/VST3.
  Universal-bundle/native-Intel conformance and icon/dSYM gates pass. Downloaded installed-receipt
  artifact [13032568029](https://github.com/skyRolly/Anamorph/actions/runs/41173229942/artifacts/13032568029),
  SHA-256 `5fb91eab4c80d752404dc4c4513f4a5a79f56106cab0c2aa3e27b8a17fb38685`,
  records baseline source, macOS 26.6.2/arm64, **the exact downloaded PKG digest**, three canonical
  component receipts, no failure and payload hashes. It explicitly says default-full only,
  sibling count 0, `both_product_coinstallation_tested=false`. Selection/upgrade/moved-copy and
  real two-product coverage remain separate (NR-015). macOS removal follows documented manual
  bundle removal; no missing generic uninstaller is asserted.

The first CLI CI-artifact redirect returned HTTP 403; the GitHub connector successfully retrieved
all three evidence archives and their digests were checked. This did not block the investigation.
Release assets downloaded successfully through the normal authenticated route.

### 2.5 Supported contracts and current source inspection

Evidence [Verified]: `CMakeLists.txt`, `PluginProcessor.cpp`, `PluginParameters.cpp/.h`,
`PresetManager.cpp`, `XmlBoundary.h`, `dsp/ScopeBuffer.h`, DSP/state tests, and the linked policies.

- VST3 + Standalone on Linux/Windows/macOS; AU additionally on macOS. Stereo→stereo and
  mono→stereo are negotiated; mono→mono, AAX, Linux ARM and Windows ARM remain **Not Supported**,
  not defects to fix. x86-64 Haswell/Excavator AVX2 floor and Linux ABI floor are deliberate.
  `prepareToPlay` primes current parameters before preparation; `processBlock` duplicates mono,
  uses `ScopedNoDenormals` and passes fresh snapshots between bulk-completion reads.
- `src/dsp/` is unchanged from tag 0.9.9. Tests cover transparent/default and Mix-0 nulls,
  serial post-Mix Mono Maker/post-output Solo, oversampling factor latency and skipped-wrap delay,
  wrap crossfades, discrete transitions, rapid bulk swaps, oversized host blocks, NaN self-heal,
  Level Match/AB/re-prepare and host reset. No new bypass, latency, channel-layout or signal-order
  regression was established. This is not an exhaustive proof for every DAW/audio stimulus.
- `ParamPointers::toEngine` substitutes immutable declared defaults for non-finite raw values,
  retaining sequentially consistent loads; State 146/147 cover ingress/readback/audio/lifecycle
  and raw-restore windows. `latencyDeliveryIsSafe` and restore application scopes commit metadata
  before callbacks; the pending-restore drain rechecks callback publication (State 148).
  RISK-009's remaining lock reachability is not the fixed F9 metadata bug.
- Preset writes use `writeTextVerified`: hidden temporary, explicit stream status/size checks,
  then replacement before selecting the saved preset. Byte/document boundaries and deferred
  command completions are already covered. No current lost-save/false-success defect was
  established. The outstanding Save Preset **focus report** is NR-013, not evidence of data loss.
- No current allocation/lock/IO violation was established on the inspected audio path. Static
  reachability checks, the process allocation guard, RTSan canary/engine lane and state TSan
  probes exist. They do not prove every wrapper, GUI-reader or supported-host interleaving safe.
  ScopeBuffer's plain sample arrays and unacknowledged next overwrite retain the specifically
  documented cross-thread issue (NR-006).
- GUI open/close/gesture/Undo coverage is substantial; it is not absent as the 0.8.13 roadmap
  once said. REAPER refocus, platform visuals and accessibility need real interaction evidence;
  no accessibility failure is inferred merely from an absence of custom handler declarations.

## 3. Reconciliation: completed, retained and obsolete work

| Historical item | Verified current disposition / evidence |
|---|---|
| KI-029 / non-finite processor semantics | **Completed**, PR #187 / ADR-0067, State 146/147; numeric NaN writer fix also retained in State 142/INC-014. Never confuse this reused ID with the earlier fixed preset-baseline KI-029. |
| F9 nested restore metadata tail | **Completed**, PR #187 / ADR-0068 / State 148; RISK-009 lock residual is separately NR-005 |
| F10 nested band-move mouse release | **Completed**, PR #168 / State 143 / ADR-0050; no second replay should be added |
| F1/F5/F6 parser/fuzz/probe vacuity; F2/F3/F4 reset/tail/preset IO; F8/F11 transitions; F12 dispatch; F13 match cases | Completed portions are traced by the global-review R4–R7, NONFINITE_PARAMETERS_AND_F13 and STATE_HOST_HARDENING worklogs and current regression functions. Old “investigate next” labels do not reopen them. Remaining explicit coverage/scope decisions map to NR-005/006/008/009/011/012. |
| KI-010/013/027/028; RISK-003/007/011/014/016 | Closed as documented in KNOWN_ISSUES/FUTURE_RISKS and current tests/ADRs: gesture/physical release, latency dispatch, tags, state ownership, undo, parser bounds and fuzz seeds. Do not copy their historical open labels into new assignments. |
| No installer, no user docs/support, no dSYM, floating pluginval, no dump harness | **Obsolete backlog**: current packaging, docs/user, SUPPORT/bug template, native dSYM gates, PR #181 and `tests/dsp_dump.cpp` exist |
| Linux desktop publication and migration cleanup | **Superseded intentionally**, PRs #191/#193; retain portable Standalone and all legacy desktop resources. Neither automatic cleanup nor Linux desktop integration is proposed here. |
| Golden stored waveform | **Retain rejection**, RELEASE_HARDENING_PLAN RH-F4 and TESTING §Gaps: conflicts with permitted Class-B changes. Extend differential tests, not a frozen cross-platform audio hash (NR-011). |
| Fast split-drag FM, masked multiband cold start, pre-0.9.1 identity break | **Retain as-is** (KI-012/001/016, ADR-0015/0023): chosen contracts, no new contradictory evidence |
| Extreme finite-input Level Match memory | **Retain pending product decision**, R7 §C3 / ADR-0007: a very loud finite burst before a saturating Drive produces a real dry/wet energy difference remembered by the matcher. No input clipper/outlier rejection is approved; do not reinterpret this as the fixed non-finite-parameter bug. |
| RISK-012/013 undo scope | Preserve the shipped-format closure and accepted raw-store scroll-grouping residual. They do not establish that every unrelated A/B pending-step candidate is closed; NR-022 verifies that separate path. |
| Per-user/system coexistence, moved macOS copies, AVX2/Linux floor | **Retain/document**, KI-021/022/023/026. Do not remove other copies, lower floors or broaden support incidentally. |
| Tooltip fixes awaiting visual retest, async-resize tear, macOS repeat keys, popup double-click/app-switch/cross-instance limitations | Carry as low-priority manual triage/accepted external constraints under NR-014. They are not confirmed new regressions; do not invent JUCE patches or replace accepted Linux CPU rendering. |
| Performance waves/idle gates, premature wrapper extraction | Retain measured decisions; reopen optimization only after NR-010 identifies a real target. File size alone does not justify architecture churn. |

**Documentation drift reported:** README/readiness/source-of-truth formerly described the older
P1 draft or Linux work still in review; ADR-0069/index still say Proposed despite #193's owner merge
and subsequent rebuilt tag. `COMMERCIAL_STATUS.md` still says 0.9.10/in places 0.9.9 and “first tag
not yet cut”; REPOSITORY_MAP has an older ADR-0065 pending sentence. RELEASE_HARDENING_PLAN still
describes committing a now-committed dump harness and reserves old future ADR numbers. These are
documentary conflicts, not permission to accept ADRs or change legal terms. This PR adds current
navigation/baseline notices; NR-001 and NR-016 handle the remaining explicit decision/small
corrections. Historical dated statements and unprovided host details must remain identifiable.

## 4. Work-item register

Priorities follow demonstrated user impact and release risk, not old commercial “P0” labels.
**P0:** confirmed severe/release-blocking defect; **P1:** important near-term gate/reliability;
**P2:** meaningful bounded defect, investigation or engineering gap; **P3:** optional polish/content.
Confidence uses source-of-truth terminology: Verified, Partially Verified, Unverified. A verified
mechanism is not proof of a field failure. Every item requires a human PR review; extra architecture,
owner/legal or manual requirements are stated below. Statuses are execution states, not defect verdicts.

| ID | Priority | Status | Kind / disposition | Phase / target |
|---|---|---|---|---|
| NR-001 | P1 | Completed | Approval-record gap — confirm decision, then reconcile | 0 / current 0.10.0 |
| NR-002 | P1 | Completed | Current Linux acceptance evidence — validate | 0 / current 0.10.0 |
| NR-003 | P2 | Deferred | Named host/lifecycle coverage — establish evidence | 1 / next validation cycle |
| NR-004 | P2 | Deferred | AU teardown cause — retain occasional-failure disposition | 1 / recurrence-triggered |
| NR-005 | P2 | Deferred | Restore/listener lock edge — investigate only on trigger | 1 / conditional maintenance |
| NR-006 | P2 | Pending | Scope overwrite/thread contract — decision before correction | 1 / architecture phase |
| NR-007 | P2 | Deferred | Rosetta assertion/debugger causes — bounded recurrence work | 1 / recurrence-triggered |
| NR-008 | P2 | Deferred | Additional state rejection — preserve acceptance pending ruling | 2 / approved hardening only |
| NR-009 | P2 | Deferred | Multiband snapshot coherence — retain bounded model | 2 / approved architecture only |
| NR-010 | P2 | Pending | CPU/peak/memory budget — measure | 2 / engineering baseline |
| NR-011 | P2 | Pending | Differential transition coverage — improve | 2 / next engineering increment |
| NR-012 | P3 | Pending | Oversampling alias reduction — measure before adding assertions | 2 / optional DSP study |
| NR-013 | P2 | Pending | REAPER preset-focus report — reproduce before fixing | 1 / bounded UX maintenance |
| NR-014 | P3 | Pending | Visual/keyboard/accessibility audit — validate/polish | 3 / product evaluation |
| NR-015 | P2 | Deferred | macOS selections/upgrades/co-installation — expand if needed | 2 / distribution coverage |
| NR-016 | P2 | Completed | Remaining documentation drift — correct narrowly | 0 then 2 / docs-only |
| NR-017 | P2 | Pending | Warning/scanner evidence inventory — classify, no blanket cleanup | 2 / engineering debt |
| NR-018 | P1 | Blocked | Public/commercial distribution decisions — owner/legal | 0 decision / commercial lane |
| NR-019 | P2 | Blocked | Signing/notarization — design after owner provisioning | 3 / commercial readiness |
| NR-020 | P3 | Deferred | Preset/onboarding/product enhancements — select scope | 3 / possible future minor |
| NR-021 | P2 | Pending | Candidate-specific release validation — close selected scope | 4 / each actual release |
| NR-022 | P2 | Ready for Review | Confirmed Undo-step loss — correction and regression implemented | 1 / investigation before maintenance |

### NR-001 — Close the Linux architecture/approval record

- **Completed on owner authority:** explicit 2026-10-31 (Asia/Shanghai) confirmation that all
  relevant ADRs were manually approved. ADR-0069/index now record Accepted; ADR-0065 remains
  Superseded with original acceptance/history intact. PR #193 is already effective at `4c13688`.
- **Evidence/confidence:** Verified approval provenance (the direct continuation instruction),
  ADR text/index and merged ancestry. Approval is not inferred from CI or merge.
- **Acceptance/validation:** current decision/navigation agree; applicable obsolete pre-merge/
  rebuild claims corrected under NR-016. Docs/citation/self-test and whitespace checks required
  on this review branch. Human approval is supplied; integration still awaits manual review/merge.
- **Disposition/target:** completed documentation reconciliation for current 0.10.0; no product,
  architecture design, version or release operation. NR-002's attestation is recorded separately.

### NR-002 — Certify the changed Linux install workflow in a real host

- **Completed by owner attestation:** “Real-machine testing has been manually completed and
  approved by the owner,” in the continuation request for NR-001/002 after PR #194.
- **Evidence/confidence:** Verified receipt of that approval; empirical per-case details remain
  Unverified/NOT RECORDED. This is not testing performed by the agent. Actual test date,
  host/OS/CPU/device/format, observations and selected tested-artifact hashes were not supplied;
  no particular artifact is falsely attributed to the human test.
- **Acceptance/validation:** release-readiness, Level-5 and compatibility records preserve that
  scope separately from the existing native/asset evidence. No redundant test request; NR-003 is
  explicitly Deferred and no unexecuted matrix row becomes PASS.
- **Disposition/target:** completed current Linux manual gate for 0.10.0, dependent on NR-001's
  now-supplied approval. Automated requirements and human review of this record remain intact;
  no publication or tag/release operation is included.

### NR-003 — Establish a named host and lifecycle evidence matrix

**Deferred by the owner on 2026-10-31 (Asia/Shanghai).** Detailed cases remain unexecuted/
unverified; broad real-machine approval of NR-002 does not mark them passed.

- **Evidence/status/confidence:** Verified coverage gap: COMPATIBILITY_MATRIX and KI-004/RISK-005;
  owner attestations exist but named environments/results are NOT RECORDED. Processor I/O,
  state/latency/reset and pluginval behavior are covered separately.
- **Impact/risk:** host-specific recall, PDC, UI keyboard and closed-editor lifecycle failures can
  evade a conformance host. No specific untested DAW is asserted broken.
- **Approach/order:** after the Linux pilot NR-002, select supported host/OS versions with the
  owner. Start with REAPER VST3 and Logic/GarageBand AU access, then other claimed hosts; include
  Standalone audio-device/reopen behavior. Capture mono→stereo, bypass/PDC/OS changes, offline
  render, save/reload/AB, automation, reset, editor close/reopen and teardown.
- **Acceptance/tests:** per-host scenario outcomes and artifact/source retained; unsupported
  layouts remain rejected; unknown hosts stay Unverified. Failures get separate reproduced IDs.
  **Human DAW/device testing and owner support-scope selection required.** No hard dependency on
  optional features; use existing state/DSP tests as controls. Phase 1; feed NR-004/005/013/021.

### NR-004 — Retain the unresolved native Intel AU teardown risk

- **Evidence/status/confidence:** Verified intermittent abort, not verified ownership: RISK-017,
  CI_1957_NATIVE_INTEL_AU_TEARDOWN and #189. Apple CAEventReceiver first-throw site and successful
  listener controls do not exonerate the product. Latest baseline/release native gates passed.
- **Impact/risk:** a validation process can abort after SUCCESS; DAW prevalence unknown. A broad
  retry/exception adapter could hide a lifetime defect and weaken release assurance.
- **Approach/dependencies:** **defer active work under the owner's recorded occasional-failure
  disposition**. Reopen on the same ordinary-CI throw, a supported DAW teardown failure, or a
  controlled experiment identifying callback ownership. Preserve source/tool hashes, seed, native
  OS and first-throw stacks before a bounded experiment; use existing typed-throw/pending controls.
- **Acceptance/tests:** for investigation, an attributable trace or an honest bounded inconclusive
  report; for any later fix, a failing-before/passing-after lifetime regression plus all native
  format/mode gates and repeated teardown. **Human review; manual DAW test if host exposure is
  implicated; architecture review if ownership/threading changes.** No speculative fix in 0.10.0.

### NR-005 — Keep the remaining restore/listener lock edge separate from fixed F9

- **Evidence/status/confidence:** Verified lock witness, Unverified supported-host deadlock:
  RISK-009, STATE_HOST_HARDENING focused risks, `StateCommandGate`, `adoptPendingHostState` and
  State 148. ADR-0068 defers latency callouts under enclosing commands; it does not remove every
  outer replacement or JUCE listener lock.
- **Impact/risk:** a demonstrated opposing host lock could hang restore/edit/save. Currently no
  supported-host deadlock is established; an adoption guard previously broke callback-save coherence.
- **Approach/dependencies:** preserve until NR-003 or an actual report supplies a supported-host
  trace, or the owner explicitly broadens the contract. Then map both lock orders/callbacks and
  propose snapshot/cancellation semantics against ADR-0036/0068; do not revive the rejected guard.
- **Acceptance/tests:** a controlled opposing-lock reproducer or evidence-backed retain decision;
  any approved correction preserves callback save fixed points and Settings/AB/Undo/preset
  coherence across repeated restore cycles, State 27/148 and TSan. **Human architecture review
  before threading/ordering changes; host test for demonstrated exposure.** Conditional maintenance.

### NR-006 — Decide the ScopeBuffer overwrite contract

- **Evidence/status/confidence:** Verified source-level issue: R7_PRODUCTION_PATH_COVERAGE §E and
  `dsp/ScopeBuffer.h::pushBlock/readLatest`. Release/acquire publishes prior writes; it does not
  order a reader against the producer's next lap over plain float arrays. Current THREAD_MODEL
  descriptions overstate that boundary. This is independent of single-threaded Rosetta Test 61.
- **Impact/risk:** display-only torn scope/FFT windows under a lapped reader; formally a C++ data
  race. No audio corruption/crash or field frequency is established. Calling aligned access
  “benign” is not a language-level race-freedom proof. TSan silence with memcpy is not closure.
- **Approach/order:** after NR-001, prepare a small architecture decision: honest current-risk
  documentation versus a bounded race-free publication design. Evaluate atomic sample access or
  reader-safe fixed buffers without audio locks/allocations or unbounded retries. Do not implement
  until owner review; a waiver must not describe the code as race-free.
- **Acceptance/tests:** explicit selected contract; if corrected, a deterministic lapped-reader
  schedule/negative control, buffer-wrap/newest-frame tests, suitable race instrumentation and
  audio allocation/performance checks. **Human threading review and visual audition required.**
  Phase 1 architecture work; implementation version chosen after evidence/approval.

### NR-007 — Preserve distinct Rosetta assertion and debugger unknowns

- **Evidence/status/confidence:** Verified historical failures; causes Unverified: §2.3,
  KNOWN_ISSUES combined-main/debugger sections, TESTING exact-zero section, #177/#180 and retained
  `scope_ring_diagnostic.cpp`. Test 69 now checks strict +0 bits; Test 61 has mismatch diagnostics.
- **Impact/risk:** intermittent validation uncertainty; no proven product fault. Translated LLDB
  attach can leave a backend outside the known client group; another uncontrolled campaign is unsafe.
- **Approach/dependencies:** defer absent recurrence. On recurrence capture raw float bits,
  FP mode, test/compiler/architecture identities and compare direct x86/native ARM/translation
  controls. Keep Test 61, Test 69 and AU teardown as three independent causal questions. Use
  dispatch-only tools solely on authorized disposable hosts with bounded service cleanup.
- **Acceptance/tests:** reproducible operands/cause or bounded inconclusive record; unchanged strict
  assertions, no epsilon/retry/architecture skip. Diagnostic edits require existing liveness and
  process-cleanup controls; a native session is needed to claim native resolution. **Human review
  and disposable-Mac access required.** Phase 1 recurrence work, not a current release waiver.

### NR-008 — Preserve bounded host-state tolerance until a specific owner ruling

- **Evidence/status/confidence:** Verified RISK-015, `XmlBoundary::DocumentRule`, State 116 E/144,
  Accepted ADR-0055/0056. Host outer/AB surfaces are bounded, but intentionally accept some
  first-document/trailing/NUL/invalid-UTF8/parseable-truncated forms that preset files reject.
- **Impact/risk:** bounded corrupt-session defaulting/partial recall; no current crash, hang,
  unbounded parser read or file access established. Tightening can reject historical host chunks.
- **Approach/dependencies:** retain as-is unless safety evidence changes or the owner chooses
  precise original-byte UTF-8, NUL, declared-length/framing and trailing-document rules. Inventory
  old writer/host padding before proposing both outer and nested AB policy; do not flip two enums
  and call all byte forms fixed.
- **Acceptance/tests:** explicit preserve/defer decision closes planning; approved implementation
  must exercise valid→invalid→valid→invalid on both surfaces, current/legacy/field captures and
  seeded fuzz/parser bounds without losing accepted valid recall. **Human serialization review
  required.** Phase 2 conditional hardening, excluded from current 0.10.0 by default.

### NR-009 — Retain the bounded multiband reader model

- **Evidence/status/confidence:** Verified RISK-010, `ParamPointers::toEngine`, multiband/Solo
  clamps and ADR-0041/0042/0044/0057. Individual seq_cst loads are not a coherent multi-field
  snapshot. Forced bulk swaps have completion gating; ordinary GUI/automation/session restores
  retain the narrower model.
- **Impact/risk:** briefly legal but unintended layouts are possible. Current count/split/solo
  repairs and smoothing bound harm; no new audible regression established.
- **Approach/dependencies:** defer unless a writer/store-order/repair changes or an audible
  restore artifact is measured. Any immutable-layout or generation scheme needs a complete reader
  and host-automation contract; a write-side commit alone cannot fix a tearing reader.
- **Acceptance/tests:** retained constraints reverified, or approved design with deterministic
  writer/reader interleavings, bulk completion controls, legal split/solo behavior and RTSan/
  allocation/performance evidence. **Human DSP/parameter/threading review and audition required.**
  Depends on trigger/owner decision; Phase 2 architecture, no incidental load-order weakening.

### NR-010 — Produce a reproducible CPU, peak-block and memory budget

- **Evidence/status/confidence:** Verified measurement gap: PERFORMANCE_BUDGET CPU/memory/operating
  point TODOs, `tests/bench.cpp`, performance/platform worklogs. Existing optimization instruction
  counts and CI bench smoke are not a portable instance-count or dropout budget.
- **Impact/risk:** users lack defensible low-buffer/instance capacity; no current regression or
  over-budget number is demonstrated. Shared-runner wall time must not become a flaky gate.
- **Approach/order:** quiet known machine(s), shipped Release flags, record CPU/OS/compiler/config,
  matrix 44.1/48/96/192 kHz, 32/64/128/256 blocks, algorithms, engaged and selected-but-skipped OS,
  static and moving splits. Follow the existing >=5-repeat median/spread/worst-block procedure;
  measure prepare-time memory. Stage representative worst cells before expanding the matrix.
- **Acceptance/tests:** raw results/methodology, modest-machine 48k/128 capacity and drag peak
  answer; numeric targets explicitly selected, not estimated. Repeatability controls and existing
  suites/twin dump before any optimization. **Known-hardware measurement and human budget review
  required.** Phase 2; future optimization depends on this, not vice versa.

### NR-011 — Extend differential DSP coverage beyond steady state

- **Evidence/status/confidence:** Verified gap: `tests/dsp_dump.cpp`, TESTING §Gaps, ADR-0032.
  Current 32-scenario dump primarily measures engaged steady state; existing behavior tests do
  exercise transitions, but that is a different assurance from cross-build numerical comparison.
- **Impact/risk:** dependency/toolchain changes can affect blind transition/reset/bypass/solo/
  crossover/idle paths without changing the current dump. No actual new sonic regression claimed.
- **Approach/dependencies:** one test-infrastructure PR; add a small deterministic transient set
  with reported latency and explicit behavior invariants. Reuse strict parser/self-checks and
  same-architecture/toolchain baselines. Include seeded changed-sample/latency negatives; preserve
  permitted Class-B comparisons and Windows same-machine-class limits.
- **Acceptance/tests:** each new path is demonstrably engaged, a mutant fails, both unchanged-build
  repeatability and before/after comparison work; current suites and dump consumers remain live.
  **Human DSP review; manual audition for any discovered/approved audible change.** Phase 2;
  no frozen golden waveform and no cross-architecture bit-identity target.

### NR-012 — Measure the oversampling quality claim

- **Evidence/status/confidence:** Verified coverage distinction in R7 §H: latency/wrap-running
  tests do not measure alias reduction. An aliasing defect is **Unverified**.
- **Impact/risk:** quality evidence is useful, but a guessed dB threshold could encode filter/
  stimulus accidents and reject correct processing.
- **Approach/order:** optional, after NR-011 or separately as a measurement-only study. Compare
  driven high-tone aliases Off/2x/4x/8x at controlled rates/levels with harmonics distinguished
  from aliases, settling and leakage controls, and a bypass/linear reference.
- **Acceptance/tests:** repeatable spectra and a justified decision to add a discriminating
  behavioral test or retain current coverage; no DSP/filter change assumed. If a defect emerges,
  create a separately prioritized fix. **Human DSP interpretation; architecture review only for
  a subsequent DSP/OS policy change.** Phase 2 optional; not a release condition.

### NR-013 — Reproduce REAPER Save Preset refocus

- **Evidence/status/confidence:** KI-009 report is **Partially Verified**; current
  `PluginEditor::showSavePreset/focusSaveNameField` retries focus on opening, not after every
  later loss. No REAPER version/OS pinned; current preset storage is separately protected.
- **Impact/risk:** Space can reach transport and typing can require reopening the dialog; no
  evidence of preset-data loss. Aggressive refocus can steal host shortcuts in other contexts.
- **Approach/dependencies:** use NR-003 host capture to record initial open, Space, loss/click
  refocus, reopen, Escape/Return, popup and editor close. Observe JUCE peer/keyboard-focus state
  before choosing a local correction or closing an obsolete/nonreproducing report.
- **Acceptance/tests:** exact reproducible host conditions and before/after focus behavior, or
  explicit unconfirmed/closed disposition. A fix needs automated component/wrapper regression
  where feasible and manual REAPER plus an unaffected-host control. If no reliable surface
  exists, disclose all four ADR-0025 exception requirements. **Manual host test/human review
  required; dependency changes need architecture approval.** Phase 1 bounded maintenance.

### NR-014 — Audit visual, keyboard and accessibility behavior

- **Evidence/status/confidence:** KI-006/011 fixes exist but specific visual retests remain
  recorded as pending; KI-008/017/018/019/020 describe external/limited behavior. Custom-drawn
  SpectrumImager gestures and Save Preset keyboard handlers are source facts; actual assistive
  technology usability is **Unverified**, not a confirmed defect.
- **Impact/risk:** visual/focus/navigation friction; changes can steal gestures or alter owned UI
  wording. Accepted Linux X11 CPU rendering and the display-rate/idle gates must be preserved.
- **Approach/order:** Phase 3 after NR-003/013; audition tooltips at scale, Advanced/resize,
  multi-instance menus, keyboard traversal/value entry, screen-reader control names/values and
  high-DPI/contrast on supported platforms. Separate tested fixed visuals from external residuals.
- **Acceptance/tests:** scenario/accessibility observations, concrete defects with user impact,
  or close/retain dispositions; no blanket UI rewrite. Any selected fix gets regression coverage
  or the narrow ADR-0025 disclosure, visual comparison and manual assistive-device test.
  **Human UX validation and explicit authorization for any new/reworded UI copy required.** P3;
  promote a newly demonstrated access failure by its actual impact.

### NR-015 — Expand macOS native installation scope only where useful

- **Evidence/status/confidence:** Verified default-full receipt/payload gate (§2.4), Accepted
  ADR-0066. Current optional selection/co-installation jobs skipped; zero sibling receipts is
  not two-product testing. KI-022 intentionally preserves moved bundles.
- **Impact/risk:** selected-component and genuine upgrade combinations have narrower evidence;
  no current install failure demonstrated. Native installer experiments write system locations.
- **Approach/dependencies:** defer until supported distribution needs the coverage or a relevant
  packaging change occurs. Use existing optional consumers on separately authorized disposable
  Macs with proven input provenance; test each format selected, fresh/reinstall/deleted/moved
  paths, and actual two-product coexistence with before/after receipts.
- **Acceptance/tests:** selection-aware expected receipts/payloads, foreign/moved items preserved,
  meaningful sibling count and negative controls, no receipt forgetting or live-workstation
  fallback. **Human/disposable-host authorization and manual Installer/host checks required.**
  Phase 2; does not invalidate the passing default-full candidate or mandate a new installer.

### NR-016 — Resolve remaining documentary contradictions without rewriting history

**Applicable updates Completed in the owner-decision branch (integration pending):** final ADR
status/supersession, live Linux references, actual tag/draft/version status, committed twin-dump
harness and reserved-ADR navigation. Historical dated worklogs and earlier attestations remain.
The original investigation/acceptance criteria below explain the scope of that correction.

- **Evidence/status/confidence:** Verified drift enumerated in §3; COMMERCIAL_STATUS,
  REPOSITORY_MAP, RELEASE_HARDENING_PLAN and dated readiness/engineering histories.
- **Impact/risk:** future agents can repeat fixed work, choose obsolete ADR numbers or mistake
  historical attestations for current certification. Documentation-only risk, not an audio bug.
- **Approach/order:** after NR-001 identifies decision authority, one small docs PR corrects live
  version/tag/dump/ADR-navigation claims and adds historical qualifiers. Check against this
  snapshot plus fresh APIs. Do not replace legal wording, reset old review dates, “complete”
  unperformed tests, mass-move resolved issue history, or regenerate the 5,000-line programme.
- **Acceptance/tests:** every changed claim cites actual code/API/decision evidence; forward
  links lead here and release execution remains in readiness; no duplicate backlog source.
  Docs/citation liveness/checks and local link/diff review required. **Human documentation review;
  owner approval for substantive legal or ADR-status changes.** Phase 0 narrow decision sync,
  Phase 2 broader drift; independent of product implementation.

### NR-017 — Inventory current static diagnostics before debt work

- **Evidence/status/confidence:** Verified current analysis jobs pass; alert-state API 403; prior
  P1 SARIF/stack-size analysis is historical. `scripts/*warning-baseline.txt`, `codeql.yml`,
  `msvc.yml` and TESTING raw-SARIF sections preserve the intended evidence route.
- **Impact/risk:** raw stack/initialization warnings need context; counting SARIF rows as bugs or
  widening baselines hides the real question. No new vulnerability is claimed here.
- **Approach/dependencies:** fetch the current CodeQL/PREfast SARIF and exact source/run; distinguish
  open-alert state (owner/API access needed) from analyzer output. Classify first-party actionable
  diagnostics versus generated/third-party/reporting artifacts; prioritize only validated impact.
- **Acceptance/tests:** traceable rule/location/disposition inventory, explicit access limits and
  no unreviewed suppression/baseline changes. For selected correctness fixes, old-code failing
  regression and appropriate dynamic checks; for scanner-only changes, same-job liveness.
  **Human review; owner assistance only to obtain unavailable alert state.** Phase 2, may proceed
  without NR-018; no full security scan or blanket warning cleanup implied.

### NR-018 — Decide the public/commercial distribution boundary

- **Evidence/status/confidence:** Verified record gap: KI-015/RISK-006, COMMERCIAL_STATUS §4,
  unapproved EULA, root without own LICENSE, public repository, RELEASE_POLICY attribution rules.
  Purchase/account/legal facts not recorded are **Unverified**, not assumed absent outside the repo.
- **Impact/risk:** undefined terms/JUCE entitlement and distribution scope materially affect any
  public/commercial offer. These were not blockers to an authorized private draft and are not a
  newly invented P0 code defect. Entitlement enforcement is a product choice, not proof of safety.
- **Approach/dependencies:** owner/legal records intended audience/channel, JUCE tier/rights,
  own source/binary terms, EULA, Steinberg marks/distribution review, privacy/controller and
  repository-visibility decision. Preserve existing third-party notices; no agent-authored legal
  grant, automatic visibility change or inference from public source availability.
- **Acceptance/validation:** explicit approved decisions/links, unresolved items clearly block
  the selected commercial route; tester/attribution route addressed. **Owner/legal approval
  required, no code test can supply it.** P1 conditional public/commercial prerequisite; NR-019/
  commercial NR-020 depend on selected scope; internal validation can continue independently.

### NR-019 — Design signing/notarization after provisioning

- **Evidence/status/confidence:** Verified unsigned Windows/ad-hoc macOS distribution in
  packaging/workflows, KI-002 and RH-PR-3/5. No managed signing credentials/account approval
  established; install skeletons already exist and do not need rebuilding as a feature.
- **Impact/risk:** Gatekeeper/SmartScreen friction; signing keys and secret exposure require a
  separate reviewed design. This is not a defect in DSP or a mandate to weaken platform checks.
- **Approach/order:** after NR-018 selects distribution and owner supplies accounts/service,
  approve a signing/custody ADR using available index numbering, protected release-only secrets,
  sign payloads then installers, notarize/staple and verify exact final bytes/checksums. Keep
  untrusted PR builds credential-free and release publishing manually controlled.
- **Acceptance/tests:** clean supported machines validate signatures/trust, tampered/unsigned
  negatives fail, package/receipt/icon gates still apply, notices and manifest describe final
  signed bytes. **Human build/security review, account provisioning and native trust tests
  required.** Phase 3 commercial lane; no certificate or workflow change in this assignment.

### NR-020 — Select product enhancements after evidence and owner direction

- **Evidence/status/confidence:** Proposed enhancements, not defects: older PRODUCT_READINESS
  preset/onboarding suggestions, current factory table/manual, RELEASE_HARDENING_PLAN licensing/
  update/crash-report proposals. No current approved requirement for a new algorithm, preset
  bank, entitlement system, network update check or telemetry.
- **Impact/risk:** content/onboarding can improve discovery; new DSP/parameters/networking or
  persistent licence fields enlarge compatibility, privacy and real-time obligations.
- **Approach/dependencies:** defer scope selection until NR-003/010/014 feedback. Audition existing
  presets first, then propose content or onboarding separately. Commercial entitlement, update
  checks or crash collection require NR-018 decisions and their own ADR/privacy/design review;
  preserve current no-network/no-collection behavior until explicitly changed.
- **Acceptance/tests:** owner selects one measurable user outcome, wording, compatibility impact
  and PR boundary. Preset changes preserve stable identity/recall and are auditioned; future
  runtime features carry their own regression/RT/serialization/network validation plan.
  **Human product approval/audition and applicable architecture/legal review required.** Phase 3;
  a possible later minor, never grounds to delay the current draft indefinitely.

### NR-021 — Validate the selected candidate and prepare a release decision

- **Evidence/status/confidence:** Verified baseline gates/assets in §2; RELEASE_POLICY,
  TESTING_POLICY, RELEASE_COMPATIBILITY_CHECKLIST and LEVEL5_AUDITION define the actual gate.
  Prior original/P1 owner attestations remain valid historical evidence, not invented per-host rows.
- **Impact/risk:** shipping a different candidate than the approved/tested one defeats otherwise
  correct validation. Documentation-only planning is not permission to mutate a release.
- **Approach/dependencies:** for current 0.10.0, close NR-001/002 and explicitly record the chosen
  distribution decision NR-018 if public/commercial. Recheck live tag/source/run/assets for drift;
  reuse exact-source evidence where unchanged. For later product changes, run fresh mandatory
  gates, compatibility checklist and targeted Level-5/manual scenarios for the changed surfaces.
- **Acceptance/tests:** identified source/artifacts, all required tests/checklist/architecture and
  human gates resolved, notices present, residuals disclosed, and concrete reviewable go/no-go
  packet. No required failure relabeled as a pass. **Human final review and separate explicit
  authorization for tag/release operations/publication required.** Phase 4 for each candidate;
  passing this item prepares a decision, it does not execute one.

### NR-022 — Preserve completed Undo across a direct A/B switch

- **Ready for Review / Verified:** reproduced against current unchanged processor code
  (`f64f9d1`, runtime equal to `4c13688`). A completed gesture followed by A→B→A before a
  coalescer poll retains the sound but loses its Undo step. State test 149 fails **10/126**;
  the matching polled controls pass. The earlier global-review candidate is confirmed.
- **Impact/risk:** bounded loss of per-slot edit history, not audio/state-file corruption.
  Priority remains P2. `abSwitchToAdopted` now polls the completed outgoing edit through the
  existing adopted helper before replacing/resetting it, after the same-slot no-op check.
  Existing admission/lock/drain, ownership, serialization, parameters and latency stay intact;
  this enforces Accepted ADR-0008 rather than introducing a new architecture decision.
- **Implementation/evidence:** commit `0e5e6449e23fd61c6967d5aa4ce42a94002e78b3`;
  `src/PluginProcessor.cpp::abSwitchToAdopted`; `tests/ab_undo_tests.h`, registered in the
  ordinary suite and `--ab-pending-undo-probe`. INC-021 and ADR-0008 record the causal chain.
- **Acceptance observed locally:** **126/126** focused checks; **945 DSP / 10,953 state**
  checks under `MALLOC_PERTURB_=1`; locked Linux VST3 pluginval strictness 10, deterministic
  and randomise ×3, all first attempts. Controls cover repeated cycles, relative/absolute
  switches, exact step counts, separate B history, later host writes, same-slot and empty edits.
- **Remaining validation/review:** required native CI/sanitizer/RTSan/TSan/LTO gates and human
  PR review. Local container uses GCC 14.2.0 Release, not the pinned shipping Clang; its ABI
  floor result must not be waived. Source citations and four historical target declarations
  are re-anchored with unchanged expected tokens/checker behavior. No version bump/release
  change; this is Unreleased maintenance work, not part of the existing 0.10.0 draft.


## 5. Version and release strategy

1. **Current candidate: 0.10.0.** Existing tag/draft already includes PR #187 and Linux #191/#193;
   do not propose the same fixes as 0.10.1 work. No source fix is justified solely to satisfy an
   old roadmap entry. Finish NR-001/002/021, retaining the owner's draft-only boundary unless a
   subsequent assignment explicitly changes it. No tag replacement is recommended by this plan.
2. **Later maintenance:** if an evidence-backed, compatible defect correction is selected after
   this baseline, propose a patch release. **0.10.1 is a conditional candidate**, not a reserved
   decision. If 0.10.0 remains unpublished, the owner can instead choose to revise that candidate
   under a separately approved identity/recovery plan or use a new number; never force-move a tag
   based on this document. Already-observed supersession is not standing authorization.
3. **Later capability work:** a meaningful approved feature/support expansion may justify a minor
   (potentially 0.11.0); choose the version only after scope, compatibility/ADR impact and validation
   are agreed. Performance measurement, docs, optional probes or unresolved suspicions alone do
   not justify a minor bump. Pre-1.0 is not permission to break persistent identities.
4. **1.0:** requires an explicit support/lifecycle commitment and commercial decision closure,
   signed-distribution plan where selected, repeatable native/host validation and human acceptance.
   It is not the next recommended version on current evidence. Protection/entitlement features
   must not be treated as automatic prerequisites without a selected product policy.

## 6. Executable phases and PR boundaries

### Phase 0 — Current-candidate decisions and acceptance

**Objective:** close evidence/approval gaps without product changes. **Items:** NR-001, NR-002,
the decision-related subset of NR-016; NR-018 only for a selected public/commercial route.
Prerequisite: re-fetch and compare this baseline; retain all old attestations with their source.

Sequence: review NR-001 contract → record exact-asset NR-002 results → reconcile small live
decision references → send the resulting candidate to Phase 4. One docs PR for approval/evidence
is a reasonable boundary; owner/legal decisions can be a separate review. Docs/citation liveness,
link/whitespace checks and native manual evidence are the gates. Completion requires explicit
decision state and no unknown result relabeled PASS. If manual tests expose a defect, stop the
release path, create a separately prioritized work item and revise scope; do not silently fix it
inside the evidence PR. Unknown public rights keep the candidate private/draft.

### Phase 1 — Stability and host behavior, evidence before implementation

**Objective:** choose corrections only for reproduced user consequences. **Items/order:** NR-022
headless A/B investigation, NR-003 host pilot → NR-013 focus investigation; NR-006 architecture
packet as a separate review.
NR-004/005/007 reopen only on their explicit triggers. This phase can follow Phase 0 without
holding 0.10.0 for all investigations.

One PR per bounded investigation, then a separate PR per approved correction; do not combine
ScopeBuffer publication with restore locking or a JUCE bump. Tests must fail on the old behavior,
cover cycles/negative controls, and pass relevant native/RT/TSan/host gates. Completion is an
evidence-backed fix, close, retain or explicit defer decision for each selected item, not a
requirement to produce a patch. A threading/latency/serialization decision pauses dependent
implementation for human review; unrelated read-only work can continue.

### Phase 2 — Compatibility, performance and engineering evidence

**Objective:** improve confidence without speculative runtime churn. **Items/order:** NR-016
remaining drift, NR-017 diagnostic inventory, NR-010 measurements, NR-011 transient comparison;
NR-012 optional spectra and NR-015 native installation scope when useful. NR-008/009 remain
deferred until their contract/evidence prerequisites are met.

PR boundaries: one docs correction; one diagnostic inventory; one reproducible benchmark packet;
one transient-test increment; any later optimization or native test expansion separately. Preserve
warning baselines, liveness, supported floors and numerical scope. Completion is reproducible
results and selected targets, meaningful tests/negative controls and no weakened existing gate.
No measurable bottleneck means no optimization PR. Any new support/toolchain/DSP policy needs its
own approved ADR and revalidation rather than inheriting a different platform's results.

### Phase 3 — Product improvements and conditional commercial infrastructure

**Objective:** select user value and the intended distribution contract. **Items:** NR-014, then
owner-selected NR-020; NR-019 only after NR-018/provisioning. Dependencies: host/UX/performance
evidence for content choices; explicit new wording/parameter/persistence/privacy decisions as needed.

Separate native signing from entitlement/runtime UI and preset content PRs; serialize shared
processor/editor/registry changes. Acceptance is the selected item's observable behavior,
compatible recall, appropriate tests and human visual/listening/trust evidence. Do not turn old
reserved ADR numbers or historical architecture sketches into an approved implementation.
Unchosen features remain Deferred; a later minor's scope/version is decided here.

### Phase 4 — Candidate-specific validation and human release review

**Objective/item:** NR-021. Current 0.10.0 can enter directly from Phase 0; a later implementation
candidate enters after its selected earlier phases, not after every backlog item is complete.

Sequence: freeze proposed scope/source → required automated/native checks → compatibility and
targeted manual evidence → artifact/notes/notice reconciliation → reviewed go/no-go packet.
Keep any later version/release-preparation PR separate from substantive fixes. Completion is a
concrete approved candidate with all required gates and disclosed residuals. Failure, unexplained
source/hash skew or missing mandatory human approval returns the candidate to the appropriate
phase. Publication/tag/replacement requires a **new authorized execution assignment**.

## 7. Original immediate assignment — Batch A (now completed by owner decisions)

**Exact items:** **NR-001 and NR-002**, plus only the dependent decision-record corrections
within NR-016. Do not begin runtime implementation or release operations.

1. Read this baseline, fetch current `main`, inspect current tag/draft/manifest and compare runtime
   and Linux script hashes. If they changed, update the evidence before using this plan; do not
   rerun unrelated campaigns merely because main gained documentation commits.
2. Present the merged Linux contract to the owner: VST3-only installation; portable Standalone;
   legacy desktop/receipt/journal/Standalone preservation. Record explicit final ADR-0069
   disposition, preserving historical ADR-0065 and approval scopes. No automatic acceptance.
3. Use §2.4's Linux ZIP or newly reverified exact replacement for targeted host/manual checks:
   real plugin discovery/load, intended version, user install without icon permission repair,
   reinstall/removal, portable app and legacy/coexisting-copy observations. Use authorized native
   systems; document host details or a clearly scoped owner attestation if details are unavailable.
4. Update the existing release-readiness/ADR/index/compatibility records and NR statuses with exact
   evidence. Create a docs-only review PR; retain unknowns rather than invent a pass. If an actual
   defect appears, file a new work-item ID with reproduction/impact and propose its PR separately.

**Deliverables:** a reconciled approval record, targeted Linux acceptance report tied to artifact
hash/source/host, updated progress/decision rows, and a reviewable remaining-gates list.
**Completion gates:** documented human ADR disposition, targeted PASS or explicit blocking defect,
no product/test/build/version changes, docs/citation checker liveness/checks and diff review.
**Proceed condition:** only then prepare NR-021's go/no-go packet. This batch authorizes no
rebuild, tag edit, release replacement, merge or publication; those need their own later scope.

## 8. Validation and approval rules for later execution

Binding references: [TESTING_POLICY](../docs/policies/TESTING_POLICY.md),
[ARCHITECTURE_REVIEW_GATE](../docs/policies/ARCHITECTURE_REVIEW_GATE.md),
[REALTIME_AUDIO_POLICY](../docs/policies/REALTIME_AUDIO_POLICY.md),
[DSP_POLICY](../docs/policies/DSP_POLICY.md),
[THREADING_POLICY](../docs/policies/THREADING_POLICY.md),
[compatibility family](../docs/policies/COMPATIBILITY_POLICY.md),
[release checklist](../docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md) and
[RELEASE_POLICY](../docs/policies/RELEASE_POLICY.md).

- Fixes carry discriminating old-fail/new-pass regressions; state tests cycle and repeat, not
  just transition once. A checker proves liveness in the job before its own use. The narrow
  ADR-0025 GUI/OS exception requires reason, substitute verification, tracked gap and a concrete
  future test surface; inconvenience is not an exception.
- Audio changes preserve allocation/lock/IO freedom, serial chain, Mix-0/bypass nulls, factor-only
  latency, supported channels, finite recovery, click-free transitions and bulk-swap completion.
  Select relevant DSP/state/native tests and the full mandatory release gate for a candidate.
  Performance optimizations require a measured benefit and declared numerical class.
- Parameter identity/range/default/automation, state schema/acceptance, threading/order, DSP graph,
  latency, format, build/dependency/toolchain choices follow the human Architecture Review Gate.
  A green test suite cannot grant approval. No blanket permission is supplied by this roadmap.
- Preserve approved original/P1 human evidence. Only new changed surfaces require new targeted
  certification; do not demand redundant unrelated auditions. Never invent a named DAW, date,
  device or artifact hash for an older broad attestation.
- A current draft, a hosted artifact and a published release are different states. Keep notices
  alongside the chosen binary distribution, preserve traceability, and do not claim publication
  or final commercial readiness from draft creation or conformance tests.

## 9. Investigation limits and open decisions

This assignment inspects current source and relevant historical findings; it does not run a DAW,
audio device, visual/accessibility audition, fresh native build or exhaustive security scan. It
reuses exact-source CI results and independently verifies release/native-report bytes as described.
No candidate was installed on a live workstation. No defect ownership is inferred from exception
location, TSan silence, runner success or a diagnostic timeout.

NR-001/002 are now closed by explicit owner confirmation. The named host scope/matrix (NR-003)
is Deferred. Remaining decisions include the ScopeBuffer publication contract (NR-006),
additional parser restrictions/coherent multiband model only if selected (NR-008/009), and public/
commercial/signing/product choices (NR-018/019/020). A current supported-host failure would change
priorities immediately; none was established by this assignment. The only ongoing API access limit
is code-scanning alert state (HTTP 403); it does not block a documentation commit or PR.

## 10. Progress tracking and evidence updates

Allowed statuses: **Pending, Investigating, Blocked, In Progress, Ready for Review, Completed,
Deferred**. Update §4's row and its item details together. A completed investigation can conclude
“retain” or “defer”; it does not imply a fix. Never reuse an NR ID or silently close another KI/RISK.

For each execution update append a row below with date, NR ID, actor/reviewer, exact source,
PR/run/artifact, result, remaining gate and any disposition/priority change. Add the new source
snapshot rather than overwriting this baseline. A new implementation starts only in a separately
authorized assignment; the investigation PR stops at human review.

| Date (UTC) | Item(s) | Progress / evidence | Remaining gate |
|---|---|---|---|
| 2026-10-31 | Investigation | Baseline `4c13688`, API/review/CI/source reconciliation, 11 release assets and three native evidence archives inspected; forward register prepared. Documentation-only PR #194 merged as `6ae102c`. | Initial investigation complete; later execution recorded below |

### Roadmap quality review

- Completed PR #187 fixes and obsolete installer/docs/pluginval/dump gaps removed from forward work.
- No P0 assigned without a confirmed severe defect; conditional commercial and manual gates labeled.
- AU, Rosetta, ScopeBuffer and restore-lock evidence kept causally distinct.
- ADR-0069 acceptance not inferred from merge; Accepted compatibility/RT/DSP decisions preserved.
- Every NR item has evidence/confidence, disposition/status, impact/risk, dependencies/order,
  approach, observable acceptance, tests, human requirements and target phase.
- Deferred triggers and direct Phase 0→4 release path prevent scope inflation and endless prerelease work.
- Remaining documentary contradictions are reported; historical attestations and unknowns preserved.

Documentation validation (2026-10-31 UTC): `check-docs.py --self-test` 700 cases and ordinary
check 177 files clean; `check-citations.py --self-test` 276 cases and `--check --base origin/main`
610 unchanged anchors; `git diff --check` clean. Local-link review found no missing file targets;
register review found 22 unique IDs/cards and the stated priority counts. The existing docs
checker emits Python invalid-escape SyntaxWarnings at script lines 18/2106; both invocations
exit 0 and no checker was modified. No product implementation is part of this plan.

### Execution update — owner decisions / NR-016

| Recorded (Asia/Shanghai) | Items | Authority / result | Remaining gate |
|---|---|---|---|
| 2026-10-31 | NR-001, NR-002 | Direct owner continuation instruction after merged #194: all relevant ADRs manually approved; real-machine testing completed/approved. Existing ADR-0069 Accepted; no fabricated test details. | Review/merge of the documentation record only; no repeat owner approval requested |
| 2026-10-31 | NR-003 | Owner explicitly defers detailed host/test matrix. | Later scoped assignment; unexecuted cases remain unverified |
| 2026-10-31 | NR-016 | Corrected live decision/version/tag/dump/navigation contradictions; preserved dated history and all engineering gates. | Documentation checker/self-test/citation/diff evidence in the review PR |

### NR-022 execution — 2026-10-31 (Asia/Shanghai)

| Evidence | Result |
|---|---|
| Prior-code full suites | 945 DSP / 10,827 state checks, zero failures |
| New State 149 on prior processor | 126 checks / 10 failures; unpolled A→B→A fails, polled controls pass |
| Corrected State 149 | 126 checks / 0 failures |
| Full corrected Release suites | 945 DSP / 10,953 state, zero failures; `MALLOC_PERTURB_=1` |
| Locked Linux VST3 conformance | strictness 10, deterministic ×3 and randomise ×3; each first attempt |
| Source checker pairs | realtime 97 self-tests; dispatch 52; portability 120; state coverage 110; ordinary checks clean |
| Local shipping-ABI check | GCC 14 VST3 requires CXXABI_1.3.15 above the 1.3.14 floor; rejected as expected for this documented local toolchain. No floor/toolchain/gate changed; pinned Clang CI remains mandatory. |
| Citation liveness | 276 self-tests; 28 automatic source anchors plus three manual shifted citations; four corresponding historical declarations moved, expected tokens unchanged |
| Source / review scope | commit `0e5e644`; stacked on owner-decision PR #195; native CI and human review pending; no merge/version/tag/release operation |

Automated evidence was produced in the existing managed Debian/GCC 14.2.0 container
with mounted source/dependency tree, not a newly claimed real-DAW audition. No source,
warning/ABI threshold, pluginval retry rule or engineering gate was weakened.
