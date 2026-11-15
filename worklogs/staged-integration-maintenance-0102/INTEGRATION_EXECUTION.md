# Staged integration execution

## Starting state

Re-fetched actual main `1ade6e1ad128b067930ab216bc355605388eb1de` and heads
#237 `e64e11fbf54b910aa6a32acafbeb4c3f64815f20`, #238
`f11dba49b9ff68c48f96d1b4435c9464a1d7bb24`, #239
`b08d514f9f147fd8c1b19e24ec0cba2f9ae57ef6`, and #240
`22e56b32e02420502cf9b091bf8b04baad2c340e`. The latter three actually target
`codex/0102-n2-evidence`; #237 targets main. All four are open, non-draft,
mergeable, no auto-merge, and have no unresolved review threads or submitted
GitHub reviews. Owner review and merge authorization are recorded separately.

At the start of this assignment `delete_branch_on_merge` was **false**, in
contrast to true in the previous dated review. The setting was already safe;
no change is needed and the original value to preserve is false. GitHub reports
merge commits allowed. The branch-protection detail endpoint returned HTTP 403;
this access limitation does not establish absence of protection.

#237's exact-head Build & Validate, CodeQL and Microsoft analysis passed;
the PR merge-check `43708836275` passed against this unchanged head and main.
Main is an ancestor of #237; local merge-tree equals #237 tree
`0383c56bead454fc41703e5215192b7d0278767e`. Original audit preservation,
preexisting main's merged fixture, and exact-head validation are recorded in
`worklogs/review-closure-0102/FINAL_REVIEW_2026-11-15.md`. The planned merge
is within the owner's approved scope; no production H4 change is included.

## #237 merge and #238 preparation

#237 was merged by the authorized merge method. Main is
`8dfc08dbfaaeec8be05e73a48dff3e8d50190d2e`, parents `1ade6e1a` and
`e64e11fb`, with tree `0383c56bead454fc41703e5215192b7d0278767e` matching the
pre-merge expected tree. The prerequisite branch still exists at `e64e11fb`;
GitHub's automatic-deletion setting remains false. Post-merge Build & Validate
`43737668160`, CodeQL `43737668156`, and manually dispatched Microsoft analysis
`43737863355` were started on exact main. The Microsoft workflow's push path
filter did not schedule it for this evidence-only change. Do not treat started
runs as completed.

#238 was explicitly retargeted from the prerequisite branch to main and merged
with main locally: new head `c3f31dabe84de4294815da14e53ae015b68a3a9c`, parents
`f11dba49b9ff68c48f96d1b4435c9464a1d7bb24` and `8dfc08db`, tree
`f5fd8ca5a432c04986bbda203d79f613086611cf`. Its reviewed production and test
files did not change; the only merge delta adds #237's State-summary parser,
its synthetic tests and the additive testing/coverage descriptions. The merge
was textually clean. Local docs, citation and state-coverage checks passed.
The host's first 22-case provenance run had two errors because host `cmake`
was missing; the same suite in `codex-rolly-native:trixie-gcc14-v1` passed
22/22. That was an environment failure, not a product or guard failure.

Exact-head push Build & Validate `43737810405`, manually dispatched CodeQL
`43737863163`, Microsoft `43737862700`, and PR merge-source checks were started
for `c3f31dab`. #238 remains open; do not merge it until required exact-source
and #237 post-merge validation is complete and successful.

The independently preserved H4 branch now includes a candidate patch and
fresh replay at **provisional** integration `70bbdc22`; its 48 recovery checks,
5614 DSP checks and eight low-frequency assertions passed. A shared Git metadata
writer overlapped that build, so that run is explicitly qualified. A separate
quiet one-sample engine boundary experiment at the same source passed 28/28;
its false-clean-history and injected-invalid-history controls failed 2/30 and
8/28 respectively as expected. Its source/receipts and limitations are on
`codex/0102-h4-recovery-followup` at `b4032243` and later. Neither experiment
is a replay on actual merged main or an approved H4 recovery policy.

## #238 merged; #239 retargeted

The exact #237 main `8dfc08db` Build & Validate `43737668160`, CodeQL
`43737668156`, and Microsoft analysis `43737863355` all completed successfully.
#238 head `c3f31dab` passed its full branch Build & Validate `43737810405`,
CodeQL `43737863163`, Microsoft analysis `43737862700`, and PR merge-source
checks. Its merge tree was its head tree `f5fd8ca5a432c04986bbda203d79f613086611cf`.
The authorized merge commit is `b4bf18f7980b796b89dea1dad8ed57fed267d626`,
parents `8dfc08db` and `c3f31dab`, same tree. The prerequisite branch remains
present and automatic deletion remains false. Post-merge main Build & Validate
`43766535759`, CodeQL `43766535738`, and Microsoft `43766535834` started on
`b4bf18f7`; do not treat them as complete until their results are recorded.

#239 was explicitly retargeted to main and merged with it in head
`fe2affc1fcfcf0c41f2376b28ab3fcbd4015d94b`, parents `b08d514f` and
`b4bf18f7`, tree `10cc9c24e8b0d4ea90debd833c06373bd29f6373`. The engine files
merged without textual conflict. Ten additive conflicts in changelog, README,
coverage, repository map, source of truth, DSP architecture/graph, ADR-0004,
TESTING and DSP registrations retained both corrections and their tests. The
resolved source/test and those conflicted documents match the prior reviewed
pre-H4 integration `84ffeaef`, except that coverage and TESTING additionally
retain #237's four-line State-summary parser explanation and corresponding audit.
The original audit and fixture changes remain intact. Local documentation,
citation, 22-case provenance and state-coverage self-test/lint passed; the
state-coverage self-test reported 112 cases at this source. Exact-head branch
matrix and analyzers and PR merge-source checks were started. #239 remains open
until those and #238 post-merge validation pass.

A new source-bound H4 candidate search at provisional `70bbdc22` found actual
finite-input post-Enable aligned overflow during Disable fades (including actual
x4 wrapping); the observed delayed measurement reference was also non-finite.
This is candidate evidence, not yet an unpatched-production classification.
The separately preserved first trace attempt showed invalid ring writes/reads
and later Mix/self-heal, but failed its own overly strict control premise and
has no completed receipt. Follow-up traces and unpatched controls are required.
The candidate remains nonshipping and ADR-0074 remains Proposed.

A separate provisional N2-04 clone based on `70bbdc22` tests a bounded
exception-only conditioned-ring scrub/preserve design; it is not yet a PR or
validated on actual integrated main. Its selected-factor 2x/4x/8x Haas
latency-stand-in regression passed 59/0; restored original ring clearing failed
12/59 and unconditional retention failed 6/59. The latter controls include
injected ring poison and actual non-finite input carried over the latency
boundary. The first natural-input test incorrectly expected same-block healing,
failed 6/56, and was corrected after observing the selected-factor latency.
The host-independent results are provisional until their source/patch/build and
logs are committed and the final actual-main correction is reviewed and tested.
