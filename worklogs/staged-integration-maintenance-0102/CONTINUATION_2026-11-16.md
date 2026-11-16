# Continuation checkpoint — 2026-11-16 UTC

This records the established state before further investigation or integration.
The owner-authorized sequence remains #237 → #238 → #239 → #240; no new
maintenance PR may be merged under that authorization. ADR-0074 remains
Proposed and all H4 candidates remain nonshipping.

- #237 merged as `8dfc08dbfaaeec8be05e73a48dff3e8d50190d2e`; #238 merged as
  `b4bf18f7980b796b89dea1dad8ed57fed267d626`. Their exact-main Build &
  Validate, CodeQL and Microsoft analysis passed.
- #239 merged as `377173bb5e3c023e28f07d5ed5a3834fccb3af8c`, with reviewed
  staged head `fe2affc1fcfcf0c41f2376b28ab3fcbd4015d94b` and the expected
  tree. Its full head and merge-source checks passed. Post-merge Build
  `43771546559`, CodeQL `43771546557` and Microsoft `43771546579` were still
  running at the previous checkpoint.
- #240 is retargeted to main at head
  `ae606b4230c3a23d1efac174c97408dc348df01f`, tree
  `a322ae4cab7c4656a3fd2beb465f3e62e31d8f9c`, identical to rehearsal
  `ca5c6fc2285628d6cf03989d27ee50095d9740f1`. Compared with `70bbdc22`,
  its production source, tests, scripts and original audit are identical;
  the differences are ten live documentation/decision-record files. Local
  docs, citations, state-coverage self-test/lint, 22 provenance controls and
  original-audit digest checks passed. Exact-head Build `43771716406`,
  CodeQL `43771745488`, Microsoft `43771748663` and PR merge-source checks
  had started. Do not merge until they and #239 post-merge checks pass.
- Automatic head-branch deletion was false at assignment start and remained
  false after explicit retargeting. The evidence prerequisite remains at
  `e64e11fb`; it and the independent research branches must not be deleted.

The independent H4 branch `codex/0102-h4-recovery-followup` includes the
source-bound N2-09 production trace and policy checkpoint. Its nonshipping
conditioned-dry fault-block fallback, at reviewed source `70bbdc22`, passed
30 injected first-consumption checks; switch-off failed 2/30, a false clean
history claim failed 2/32, and injected invalid history failed 8/30 as
expected. Affected aligned history still differed from the clean twin in
32/32 samples; affected output differed in 32/32 zero-latency and 26/32 actual
x4/six-sample-latency samples. Its reset clears both dry banks of the affected
channel, so preservation of a valid same-channel fading bank is unproved.
The candidate is not a complete recovery contract or native-performance
result. See the committed follow-up at `fe62d2bd` and its parent results.

The N2-04 preparation branch `codex/0102-n2-04-prep` includes the focused
exception-only conditioned-ring scan and a 125-check v6 selector. Local v6
runs passed 125/0 and full DSP 5739/0; restoring the old clear failed 28/125
and unsafe unconditional retention failed 6/125. Raw v6 development evidence
is on `codex/0102-n2-04-research` at `96d33bec`. Earlier preparation head
`623d3c33` has a separately source-bound fresh Linux/Ninja replay: v5 selector
109/0, full DSP 5723/0 and full State 16555/0, receipt committed at
`dd1556b3`. These are different test/source identities and neither is a
final-main PR result.

A receipt-controlled positive v5 relink passed. Two attempted v5 mutation
relinks were incorrectly launched concurrently against the same JUCE Git
index. One failed closed because `git write-tree` could not create
`.git/index.lock`; the other exited as expected but overlapped that writer and
therefore does not satisfy the documented no-concurrent-writer premise. The
source checkout, dependency and build were not intentionally altered. Preserve
both attempt logs and rerun both controls sequentially with fresh output
directories; do not claim their overlapping results as accepted provenance.
No failed artifact or historical evidence should be overwritten.

The next engineering steps are to inspect actual hosted outcomes and branch
refs, complete the sequential relink controls, validate any remaining N2-04
changes against actual final main, and finish the authorized staged integration
only if its exact-source checks pass. Release, native/manual acceptance,
production H4 implementation, new recovery policy and architecture acceptance
remain separate decisions.

## Resumed execution

#239 actual-main Build `43771546559`, CodeQL `43771546557` and Microsoft
`43771546579` all completed successfully. #240 exact head `ae606b42` passed
Build `43771716406` (16 successful jobs, three conditional skips), CodeQL
`43771745488`, Microsoft `43771748663` and its PR merge-source/dependency
checks. With live base/head, review threads, mergeability and branch deletion
rechecked, #240 was merged by the authorized merge method as
`230da0e2b0b81422f7e3964eea2833c48266a963`, parents `377173bb` and
`ae606b42`, tree `a322ae4cab7c4656a3fd2beb465f3e62e31d8f9c`. All four
reviewed PRs are now merged. Compared with `70bbdc22`, only the ten documented
live documentation and decision-record files differ; `src`, `tests`, `scripts`,
`.github` and the archived original audit are identical. ADR-0074 remains
Proposed and the production H4 behavior is unchanged. The evidence and H4
research branches remain; the original automatic-deletion setting was false and
is still false. Actual final-main Build `43796315012`, CodeQL `43796314706`
and Microsoft `43796314750` are running; no final-main success is claimed yet.

The previously overlapping N2-04 v5 relinks were rerun sequentially. Their
outer runners passed with the expected executable results: positive 109/0,
original-clear mutation 24/109 failures and unsafe retention 6/109 failures.
The complete receipts and failed parallel attempts are retained on the research
branch at `73b75072` and `4ef45146`, respectively.

A separate draft [PR #243](https://github.com/skyRolly/Anamorph/pull/243)
contains the focused N2-04 correction at head
`6a65b1d18a992c738b3f004f7690ea87d510bd40`, based on actual main `230da0e2`.
It preserves the 125-check v6 test, including repeated recovery, and requires
independent owner review; its merge is not authorized. Exact-head local docs,
603 citation anchors, state-coverage lint and 112 self-tests, and 22 provenance
guards passed. Earlier v5 source-bound and v6 local development results retain
their identities. A new exact-head v6 full DSP/State replay and hosted checks
are running; no results are implied by their launch.

The nonshipping H4 candidate was freshly replayed on actual main `230da0e2`
using the specification at `24d2d7f0` and diagnostic tree
`17ca85e77e49e4701865287e99d7c402f8981a99`; the independently checked JUCE
clone did not share its Git index with the concurrent N2-04 replay. The scoped
recovery controls passed 48/0 and history controls passed 28/0; deliberate
false-clean and invalid-history mutations failed as expected. Full DSP passed
5614/0 and the independent low-frequency matrix oracle passed 8/8. A first
oracle invocation used the wrong CSV and failed with `KeyError`; that attempt
is retained separately. The original injected-state experiment still produced
128/128 and 122/128 affected output differences at zero and actual x4 wrapped
latency, respectively, and 128/128 newly written history differences in both
cases. These are not clean-reference recovery. Complete receipts, commands,
negative controls and the oracle distinction are recorded on the research
branch at `7bf87e7f313c964dfcb223b9ad3975ac36bffddf`. The acceptable
affected-channel trajectory and aligned-history policy, live fade-bank
coverage, exceptional allocation and representative native performance remain
unestablished; ADR-0074 is still Proposed.

## Final-main Windows configure failure — attempt 1

The actual-main Build & Validate run `43796315012`, source `230da0e2`, attempt
1 reported the `windows` job failure at its `Configure` step (job
`133065821289`, 2026-11-16 03:16:53–03:17:10 UTC). Checkout and the preceding
PowerShell pluginval-acquisition self-tests passed; the Windows build, tests,
pluginval and packaging steps in that job were skipped. Other jobs remain in
progress. The completed #240 head `ae606b42` had an identical tree and passed
its Windows job, but that does not identify the cause of this attempt.

The GitHub job-log endpoint redirected to artifact storage that returned
Forbidden. The available metadata establishes the failing step, not whether
it was product, dependency, runner or network related. Do not attribute the
failure to a particular cause or treat a successful retry as its original
log. When the run completes, rerun the failed job on the same exact source,
retain both attempts and investigate any accessible evidence. Final-main
validation is not yet complete.

## Exact-head N2-04 replay and mutations

The fresh Linux/Ninja replay of PR #243 head `6a65b1d1` has completed:
125 focused checks, 5739 DSP checks and 16555 State checks passed. Its
receipt and raw results are committed on `codex/0102-n2-04-research` at
`facafb27ff02cec560c01417189f3b620c147901`. Three subsequent receipt-controlled
relinks ran serially: positive 125/0, original-clear mutation 28/125 failures,
and unsafe-retention mutation 6/125 failures, each with its expected process
exit. Their receipts and raw results are committed at `1091fbd8`. The earlier
overlapping v5 attempts remain documented separately. Exact-head hosted Build
`43796853198`, CodeQL `43796889447` and Microsoft `43796892090` passed, as did
the PR merge-source and dependency checks. The full Build run had 16 successful
jobs and three conditional skips. PR #243 remains open and was marked ready
for independent owner review; auto-merge is disabled and no new PR merge is
authorized.

The fresh integrated-main numerical replay at `31bf22e2` reproduced the
selected N2-05, N2-06 and N2-07 witnesses. The N2-06 ordinary finite-input
comparison is deliberately cancellation-sensitive, not a claim of incidence
or audibility. A nonshipping finite-operand endpoint candidate and its
independent smoother, interior and self-heal controls are specified at
`e2ea2fd8`; their specification is not a passing result or a production PR.

The subsequently executed [N2-06 diagnostic controls](numerical-main230/n206-candidate-results/README.md)
at `825d22e5` pass 11/11 with the finite-operand endpoint candidate; the
unmodified-main control fails the two intended endpoint assertions. This is
scoped evidence and does not itself establish all Match, Multiband Enable,
oversampling, non-finite or regression interactions. A separate production
branch is under development; no N2-06 PR or merge is implied by this record.

The [live-fade H4 fallback control](https://github.com/skyRolly/Anamorph/tree/4f9195a99a148c2031761a741b842267874892c7/worklogs/h4-decision-checkpoint-0102/followup/fade-collateral-main230-results)
passed 277 selected observations and showed that all 24 deliberate recovery
events reset valid state in the other live dry bank of the affected channel.
Checked Wet, opposite channel, and raw/conditioned histories were preserved;
no allocations were observed in those selected calls with live guards. Both
specified negative controls failed 24 assertions as expected. Injection does
not establish natural reachability. This is nonshipping evidence, not a new
accepted recovery policy or deadline guarantee; ADR-0074 remains Proposed.

After the final-main Build run finished with its original Windows configure
failure, GitHub reran the failed Windows job on the same source `230da0e2` as
attempt 2 of `43796315012`. That retry is in progress; its result and the
original inaccessible failure log must remain separate.

## Current staged and maintenance checkpoint

The original final-main Windows Configure failure remained unattributed because
its attempt-specific job-log redirect returned Forbidden. A failed-job retry
on the same exact source `230da0e2` passed. Build & Validate `43796315012` now
reports success, as do CodeQL `43796314706` and Microsoft C++ analysis
`43796314750`; the original failed attempt and the retry are recorded
separately in [main230-hosted-validation](main230-hosted-validation/README.md).
These are automated results, not new manual or representative-hardware
acceptance. The original deletion setting was false and remains false; the
evidence branch is `e64e11fb`, and the H4 research ref is `f511198d`.

[PR #243](https://github.com/skyRolly/Anamorph/pull/243) head `6a65b1d1`
(N2-04) and [PR #244](https://github.com/skyRolly/Anamorph/pull/244) head
`544ff751` (N2-06) are open, non-draft, mergeable, based on main `230da0e2`,
and have auto-merge disabled. Both have fresh exact-head DSP/State, focused
mutation controls and hosted Build, CodeQL and Microsoft analysis passing.
The N2-06 exact-head receipt and distinct predecessor failures are in
[n206-pr544ff-final-results](numerical-main230/n206-pr544ff-final-results/README.md).
Separate owner review and merge authorization are required for each new PR.

The independent H4 branch contains a nonshipping bank-local candidate against
actual main. Its selected live-fade test passed 277 observations and preserved
the checked other live bank in 24 deliberately injected recoveries; the
expanded first-consumption controls passed 30 and the low-frequency oracle 8.
The affected aligned history still differed from its clean twin in 32/32
selected writes, and affected output differed in 32/32 zero-latency and 26/32
actual x4/six-sample-latency samples. Finite output is not clean-reference
recovery. Output-only invalidity with finite scans still has ambiguous bank
attribution; post-Enable publication, acceptable exceptional trajectory,
complete recovery and representative native performance remain unestablished.
See the research branch at `f511198d` and its `bank-local-main230-results` and
`bank-local-expanded-main230-results`. N2-09's distinct production witness is
not an authorized production fix in these PRs. ADR-0074 remains Proposed.

[Draft PR #245](https://github.com/skyRolly/Anamorph/pull/245) head
`f78b53208623fbd7702194e41bd931d708a837fa` proposes the separate N2-07
finite Drive endpoint correction. Its exact committed test header passed 13/13
receipt-controlled positive assertions; main failed 5/13, and isolated
settled, smoothing, held-envelope, missing invalid-operand protection and
altered-interior mutations all failed their intended controls. The held fade
is deliberately injected. [Receipts and the rejected abbreviated-SHA attempt](numerical-main230/n207-pr-header-controls/README.md)
are committed at `62306735`. This does not replace the clean full-PR replay,
matched cost comparison or hosted validation, which are in progress under
their own identities. The earlier v2 failed discrimination premise remains
preserved. Do not merge this new maintenance PR under the integration approval.

N2-05 remains an independently confirmed dry-loss problem: selecting dry at
Mix=0 alone can hide invalid wet state from the existing self-heal. N2-08's
exceptional finite wet LR4 overflow remains separate; widening the filter can
conflict with its all-input JUCE bit-equivalence contract and needs the
applicable owner/architecture decision. No P3 finding is an automatic release
blocker solely by priority. H4 policy, ADR acceptance, any new PR merges,
release scope and applicable native/manual acceptance remain distinct.

## Drive PR head and measured cost update

The initial #245 head `f78b5320` failed the hosted source-lint citation
self-test. Local reproduction identified four stale dry-bank citation
declarations; current head `44ef025aab46e09a8be7069184ce79071b7d5a09`
corrects them without changing production or test bytes. The 267-case local
self-test and 603 anchors passed, and the earlier failure and inaccessible
hosted job log are retained in [n207-f78-hosted-failure](numerical-main230/n207-f78-hosted-failure/README.md).
The current exact-head hosted source-lint passed; its other hosted checks are
still running at this checkpoint. The separately launched fresh full build was
for `f78b5320`, so its eventual result must retain that source identity.

A [matched instruction comparison](numerical-main230/n207-cost-results/README.md)
used main and the source-bound engine-only candidate at 48 kHz/128 samples:
one/four bands increased 1.578%/0.886% unwrapped and 1.950%/1.571% with
actual x4 wrapping. These are virtualized whole-engine instruction counts,
not native timing, deadline or instance-capacity evidence; they do not measure
the newer H4 candidate. PR #245 remains draft, open and unmerged pending final
exact-head validation and owner review.

## Final maintenance checkpoint

#245 current head `44ef025aab46e09a8be7069184ce79071b7d5a09` passed
[exact-head Build, CodeQL and Microsoft analysis](numerical-main230/n207-pr44-hosted-validation/README.md),
with its merge-source check and dependency review. Its merge ref
`7f5be00f2935d0936eaecc01fcf01a2d05513f5a` has the same tree as the PR head.
The predecessor fresh Linux replay passed 13 Drive, 392 Multiband, 720 duck,
5,627 DSP and 16,555 State checks; it remains scoped to `f78b5320`, not called
a current-head local replay. The original wrapper exit was unavailable, but
the container exit and all executable exits were observed 0 and outputs were
rehashed. #245 is now non-draft, open, mergeable and ready for independent
owner review; auto-merge is disabled and its merge is not authorized. Its
earlier failed citation self-test and inaccessible original job log remain
separate. All three new maintenance PRs need separate owner decisions.

The N2-05 nonshipping reset-indication diagnostic preserved the selected
finite-input witness and the [injected Wet-state boundary](numerical-main230/n205-flagged-results/README.md);
a naive output-only shortcut left Wet invalid until Mix re-entry. In a further
[actual x4 test](numerical-main230/n205-x4-results/README.md), the diagnostic
preserved the fault block but only 116/128 exact Dry samples on the next
block; adding just #243's N2-04 engine correction preserved 128/128 in that
selected next block. The injected state is not natural-reachability evidence.
The additional N2-05 detection boundary needs architecture review and broader
recovery tests before a production proposal. No H4 implementation was shipped.

A [pairwise virtual merge rehearsal](numerical-main230/maintenance-merge-rehearsal/README.md)
records documentation/citation conflicts and the #244/#245 test-registration
conflict. No new maintenance branches were merged or retargeted; eventual
owner-authorized integration must resolve and revalidate each exact source.

The three open PR descriptions now link the pairwise conflict rehearsal; #243
also links the selected x4 N2-04/N2-05 interaction without claiming a combined
production fix. The [manual acceptance scope](MANUAL_ACCEPTANCE_SCOPE.md)
preserves the earlier owner attestation and identifies targeted review of the
new DSP behavior. No further PR merge, architecture acceptance, release or
new human/hardware acceptance has been authorized or inferred.
