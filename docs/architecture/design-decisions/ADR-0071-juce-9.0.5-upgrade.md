# ADR-0071 — JUCE 9.0.4 → 9.0.5

**Integration — 2026-11-04:** owner-authorized [PR #213](https://github.com/skyRolly/Anamorph/pull/213) merged as `18a51df773d8701004fe86e85df820fdf6949f2d`; its tree equals approved head `520537e849b3f42a4300d55acc652a354d360025`. ADR-0071 remains **Accepted**. `main` pins JUCE **9.0.5** at `d60b7a78030661574a615b586d9325ba7a7fa7dd`. Level-5 listening and native/manual acceptance are **PASS — owner-attested**; execution date, exact payload and environment were not supplied. No new release is authorized.

The dated implementation and review snapshots below retain their original status at the time.


**Current human evidence:** Level-5 listening and native/manual acceptance **PASS — owner-attested
for JUCE 9.0.5**, recorded 2026-11-04 (Asia/Shanghai); exact tested payload/environment fields
are not supplied. The architecture status remains Accepted; the PR has merged under the later owner authorization above. No release is authorized.

**Status:** Accepted — 2026-11-04 (Asia/Shanghai), on the owner's explicit reply
“approve ADR / dont merge PR” to the approval request for ADR-0071 and Anabasis ADR-0051.
This clears the Architecture Review Gate for the scoped implementation below. It does not
approve PR merge or release publication. Human acceptance was separately owner-attested later;
see the reconciliation evidence below. Proposed earlier
the same day; the original investigation and its unavailable-build observations remain history.
Amends ADR-0061's current dependency pin; its dated 9.0.4 evidence remains intact.

## Context

Verified remote main: `82c98e45984f5c7bc0f953340622f90523957780`; initially clean working tree,
no open PRs before this assignment. Product version is **0.10.0** and stays unchanged.

| Identity | Version | Immutable commit |
|---|---|---|
| Baseline, Accepted ADR-0061 | 9.0.4 | `3fb5cbc0340758596dac39f7c8de47d562b4896e` |
| Target of this decision | 9.0.5 | `d60b7a78030661574a615b586d9325ba7a7fa7dd` |

Both official lightweight tags were checked with `git ls-remote`; clean detached source
trees and JUCE's project/module versions corroborate them. The official release was published
2026-11-02. Detailed source, acquisition, licensing and validation evidence is in
[`worklogs/JUCE905_UPGRADE_REVIEW.md`](../../../worklogs/JUCE905_UPGRADE_REVIEW.md) and its
[evidence ledger](../../../worklogs/juce905-review/evidence.json).

## Problem

Adopt the selected framework while preserving DSP output, APVTS publication, state readers,
parameter/format identities, latency/PDC, supported platforms and the compiled codec set.
Changing only CACHE defaults would silently retain 9.0.4 in reused builds. JUCE 9.0.5 also
contains explicit breaking API/parser changes; its patch number is not compatibility evidence.

The gate names **“Build System change — CMake structure, JUCE version/pin, dependency set”**
and states **“Detecting any gated change is an AI Agent Hard Stop — the agent stops and
requests human review rather than proceeding.”** Required approval is a human reviewer with
DSP/audio context approving this proposal's implementation scope under
[`ARCHITECTURE_REVIEW_GATE.md`](../../policies/ARCHITECTURE_REVIEW_GATE.md), Procedure 2–3.
Earlier owner approvals cover their dated decisions, not this new proposal.

## Options

1. Retain 9.0.4: preserves the present audited baseline; fallback if approval or validation fails,
   but does not fulfill the selected upgrade.
2. Use mutable `9.0.5` or a branch: rejected; violates immutable pinning under ADR-0022.
3. Change defaults alone or force cached values: rejected; respectively retains stale builds
   or overwrites intentional custom revisions.
4. Keep FetchContent and the local checkout escape hatch, select the exact target SHA and
   extend stale-default rejection: **chosen under owner approval**. Remove `GIT_SHALLOW` only if a supported
   acquisition configuration demonstrably fails and that correction is reviewed.

## Decision and implementation scope

- Change `ANAMORPH_JUCE_VERSION`/`ANAMORPH_JUCE_TAG` to the target pair under the owner's approval.
- Extend `cmake/CheckJuceCache.cmake` to reject either 9.0.4 field, retaining the historical
  9.0.2/9.0.3 refusals, actionable two-field reset, custom revisions and local comparisons.
  Update `tests/juce_cache_tests.py`, including repeated configuration, partial/mixed resets,
  actual FetchContent provenance and a local 9.0.4 override.
- Preserve CMake ≥3.22, C++23, compiler pins, ISA/FP/LTO flags, target graph and every existing
  `JUCE_*` definition, including the MP3/Opus/WebP exclusions on all consumers.
- Preserve all production DSP, parameter IDs/ranges/defaults, state schemas and migrations,
  product/format identities, latency and real-time contracts. Any necessary JUCE-induced
  source correction must be separately identifiable and reviewed; no such correction is
  demonstrated necessary in production. The observed test-only correction is recorded below.
- Update current dependency declarations/provenance, cache tests, live JUCE citations,
  dependency/build/CI/testing records, `NOTICE` and `THIRD_PARTY_LICENSES.md` together when the
  pin actually changes. Historical ADRs, release attestations and measurements retain 9.0.4.
  Apply the changelog evidence policy to the implemented dependency change; no release entry
  or version bump is authorized here.

## Upstream compatibility and DSP risks

- `juce_dsp`, `juce_audio_basics`, `juce_events` and `juce_opengl` change module metadata only.
  Filter/oversampling maths and MessageManagerLock are unchanged; this is source evidence,
  not a numerical/host pass.
- **ADR-0057 precondition 2 holds in the target source.** APVTS implementation is byte-identical,
  SHA-256 `94760f6858acf7fcf52915504cc24702cdea9c031d9a9593919de108884a1ad6`.
  `ParameterAdapter::parameterValueChanged` line 155 assigns the `std::atomic<float>` declared
  at line 208 without an explicit weaker order: `seq_cst`, stronger than release. Anamorph's
  `ParamPointers::toEngine` retains its default `seq_cst` loads. No x86 result substitutes for
  this source precondition on weakly ordered hardware.
- XML changes are reachable, but Anamorph's Accepted ADR-0055/0056 boundaries already reject
  DOCTYPE and cap document depth at 8, below JUCE's new 256 limit. Keep those boundaries and
  their negative controls. Verify real legacy/user/session fixtures and Unicode metadata.
- VST3 changes Windows DPI handling. Native child-window association, mouse retargeting,
  parented PopupMenu transparency, tooltips and platform rendering change. Audit the popup
  shield, callback lifetime, editor scale/restore regressions and native GL behavior.
  AU and Standalone wrapper source files are unchanged; shared GUI dependencies still move.
- HarfBuzz 14.2.1 → 15.0.0 affects shipped text rendering. The reviewed licence texts and
  existing codec payload trees are unchanged; verify compiled objects and native text layout
  after implementation. Commercial JUCE/distribution decisions are not changed.
- Projucer deployment defaults and Android/Java/Gradle changes do not alter this CMake desktop
  project. Xcode 27 cannot support the existing older deployment floor; do not raise that floor
  or change toolchains to obtain a green build. Escalate an actual native conflict separately.

## Test requirements, observed evidence and acceptance criteria

Observed: existing seven cache tests and source-policy checker controls run; source acquisition
with immutable SHA plus `GIT_SHALLOW` resolves exactly at CMake 3.22.6 and 3.31.6. This isolated acquisition
probe is not a configured product. Both 9.0.4 and 9.0.5 product configurations fail building
juceaide because `ft2build.h` is missing; no DSP/state/pluginval result is claimed.

Before implementation acceptance, require fresh/reused/local dependency tests with actual git
HEAD and compile-source provenance; frozen-source `AnamorphDspDump --self-check` and identical
old/new dump output within each architecture/configuration; full DSP/state/GUI regressions;
cross-version state/preset/render/latency equality; the existing Linux/Windows/native Intel/
Apple Silicon CI matrix with both pluginval modes ×3 at the workflow strictness, AU on macOS,
sanitizer/TSan/realtime/memcheck/fuzz/ABI/warning/source/docs/analysis/dependency gates and native
packaging/symbol checks. Do not widen tolerances, regenerate fixtures or remove negative controls.

**At proposal time — Pending Human Acceptance:** new Level-5 listening, DAW session reload/automation/PDC,
editor/menu/scaling/accessibility/native OpenGL and relevant installation/packaging checks.
The exact worksheet is in the worklog. Existing 9.0.4 attestations do not accept 9.0.5.

## Consequences and rollback

The owner approved implementation and automated validation. This branch changes the dependency
pin and its cache/provenance checks, while manual acceptance, PR merge and release approval remain
separate. This decision amends ADR-0061's dependency pin; integration on main awaits PR review.
If any contract differs, stop that implementation at its applicable gate and retain the evidence.

Before integration, rollback is closing/withdrawing the proposal or reverting the implementation
on its own branch. After a future integration, revert that product's upgrade commit(s), restore
the 9.0.4 pin and corresponding cache/tests/docs, clear both cached fields and rebuild from the
verified old SHA. Re-run compatibility checks; do not change product/release tags or assets.

## Related code; evidence and confidence

`CMakeLists.txt`, `cmake/CheckJuceCache.cmake`, `tests/juce_cache_tests.py`, `tests/dsp_dump.cpp`,
`src/PluginParameters.cpp`, `src/XmlBoundary.h`, `src/PluginProcessor.cpp`, `src/PluginEditor.cpp`,
`.github/workflows/build.yml`; Accepted ADR-0011/0022/0031/0032/0034/0035/0055/0056/0057/0061/
0067/0068/0070. **Verified:** remote/tag identities, source differences, APVTS order, source
licence comparisons and recorded tool exits. **Unverified:** target DSP/state/host equivalence,
full native target matrix and human acceptance. Anamorph owns this proposal independently of
the sibling Anabasis ADR-0051; no shared source or cross-repository implementation is introduced.


## Approved implementation continuation — 2026-11-04

The pin/cache change is implemented on the unmerged branch. `cmake/VerifyJuceSource.cmake`
prints the actual fetched Git HEAD and fails if an immutable pin resolves to a different checkout;
custom named revisions and the documented local-path comparison remain supported. The existing
cache harness tests both a genuine mismatched git checkout and a matching custom SHA, in addition
to old-cache/refusal/reset cases. Current runtime evidence and explicit pending gates are retained
in the implementation continuation of the worklog and `worklogs/juce905-review/implementation.json`.
No PR merge, product version change, release operation or manual acceptance is authorized by this
implementation record. Existing proposal-stage missing-header measurements remain dated history.


## Observed test compatibility correction

The first implemented Linux run [41710637307](https://github.com/skyRolly/Anamorph/actions/runs/41710637307)
failed only the XML differential step: its old oracle required an unguarded deep parse to crash.
JUCE 9.0.5 now rejects excessive nesting before that failure. This is an obsolete test expectation;
no product boundary, serialization schema, fixture or tolerance is changed. The separately
identifiable correction retains the old deep inputs and the legacy-pin branch, requires parser
rejection at 9.0.5, and adds contained signal/timeout/setup-error canaries plus an independent
product-cap control. Resource/setup failures now fail closed. All 18 named, seven legitimate and
1 500 generated cases remain mandatory. Negative-control and runtime observations are retained
in the worklog and implementation evidence. This falls within this ADR's approved JUCE-induced
compatibility-correction scope; it does not relax ADR-0056's 256 KB/depth-8/no-DOCTYPE boundary.


## Observed implementation validation — 2026-11-04

Historical snapshot before the new owner attestation: its pending status is superseded by the
current human evidence above and the later reconciliation below; measured outcomes retain their scope.

Both pins use equivalent current production source (comment-only `src/` diff), GCC 14 Release
settings and fixtures in isolated Linux builds. All 32 committed DSP dump scenarios and five
transitions have identical hashes and latency/trace metadata. Both full suites pass 945 DSP and
13,959 state checks. A 9.0.4 session re-saves byte-for-byte; all 36 normalized `getValue()` parameter bit patterns per slot,
metadata, six distinct 44.1/48/96 kHz render hashes and reported latencies agree. The committed
parser self-tests, dump controls, source/cache negative controls and corrected XML controls stay live.
Local strictness-10 pluginval passes both modes ×3. Required source-head CI platform/sanitizer/static
jobs pass, including the reporting-only cross-slice retry (attempt 2); final evidence-head checks are tracked in the PR.
The worklog and timestamped target CI ledger distinguish passes, initial failure, cancellations and
skips. Manual DAW/listening/native acceptance remains **Pending Human Acceptance**; this ADR
approval does not accept those tests or authorize merge/release.


## Owner review follow-up — 2026-11-04

The source verifier must preserve explicit FetchContent archive overrides. Normal Git fetches
and Git-backed source overrides retain immutable-HEAD enforcement; an archive without its own
Git metadata is permitted as trusted, explicitly **unverified** development input, like the
existing local-checkout escape hatch. It does not prove the default pin or licensing inventory.
An enclosing repository's HEAD is never substituted. The check remains after JUCE configuration,
before product targets: this is provenance validation, not an execution trust boundary. No
FetchContent acquisition model, compiler/codec/format setting or product contract changes.

Live handover/CI/build/compliance records distinguish prior 9.0.4 attestations from current
9.0.5 automated evidence and pending Level-5/manual acceptance. Session-probe bit listings use
normalized `getValue()`; byte-identical serialization and render hashes are separate checks.
No independent unnormalized parameter measurement is claimed. The review follow-up requires
fresh tests/CI. The 16-case cache/source suite passes on CMake 3.22.6 and 3.31.6, and a
full real-archive configure passes with the expected unverified-identity warning. Source-head
CI remains separately required. Evidence is in the worklog and `review-followup.json`; no merge is authorized.


## Human reconciliation and source-metadata correction — 2026-11-04

The new owner attestation explicitly passes Level-5 listening and native/manual acceptance for
JUCE 9.0.5. It is not inherited from 9.0.4 and does not grant merge/release permission. The owner
is the attestor; 2026-11-04 (Asia/Shanghai) is the recording date, not an inferred execution date.
Actual test execution date/time, tested source commit and payload SHA-256, machine/OS/CPU/architecture, DAW/version, plugin formats, device, sample rates/buffer sizes, programme/session identifiers and per-scenario observations were not supplied. The evidence fields and known review context are in
`worklogs/juce905-review/acceptance-reconciliation.json`; named host rows are not fabricated.

The confirmed damaged-metadata bug is corrected within this ADR's existing source-verification
scope: an archive requires absence of a `.git` directory entry. `EXISTS` plus `IS_SYMLINK` alone
still misses unreadable files/directories on both supported CMake versions. Literal directory-entry
detection rejects those entries and dangling links while retaining real worktrees, valid metadata
symlinks, immutable mismatch rejection, archive warnings, no-Git archive support and parent isolation.
The 28-case suite passes on 3.22.6/3.31.6; original and partial-fix regressions are retained in
`worklogs/juce905-review/metadata-review.json`.

The post-configuration ordering is retained. This satisfies the already-selected contract and
explicitly does not gate JUCE configuration-time execution. Moving verification ahead of source
population is invalid for fresh sources; split-population/hybrid alternatives add build orchestration
and CMake-version/provider policy ownership and would require a separately reviewed architecture
change. The worklog records four alternatives, before-product-target boundary evidence and CMake
source documentation. No target/compiler/codec/product/DSP/state/GUI/latency change is introduced.
Fresh corrected-head CI remains mandatory; no prior success is substituted. Existing human
acceptance remains recorded without a repeat request; exact payload binding is still not supplied.
