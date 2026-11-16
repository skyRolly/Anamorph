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

The nonshipping H4 candidate has an actual-main replay specification at
`24d2d7f0`, applying the reviewed candidate patch to `230da0e2` with expected
diagnostic tree `17ca85e77e49e4701865287e99d7c402f8981a99`. Its fresh build
uses a separate independently checked JUCE clone, so its dependency index is
not shared with the concurrent N2-04 replay. The candidate recovery, history,
full DSP and low-frequency oracle runs are in progress, not yet results.
