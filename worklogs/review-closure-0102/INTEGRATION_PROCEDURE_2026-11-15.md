# Staged integration procedure — 2026-11-15

The intended order is #237 → #238 → #239 → #240. #241 and #242 are already
merged on main `1ade6e1ad128b067930ab216bc355605388eb1de`. #238, #239 and
#240 are sibling PRs based on the actual #237 branch head `e64e11fb`; they
are not serial production branches. The H4 owner decision is independent of
merging its documentation, and ADR-0074 remains Proposed.

## Branch deletion is a real prerequisite

GitHub's repository setting `delete_branch_on_merge` was **true** when checked
on 2026-11-15. Merging #237 with that setting unchanged can delete
`codex/0102-n2-evidence` while all three dependent PRs still target it. Do not
rely on automatic retargeting or branch deletion behavior. Before an owner
merges #237, either an authorized repository administrator must temporarily
disable automatic head-branch deletion and verify the setting, or the owner
must explicitly retarget all three dependent PRs before merging #237 and
accept/review their temporarily expanded diffs. No repository setting or PR
base was changed during this assignment. Keep the prerequisite branch until
all three retargets have been confirmed. Do not delete it under this assignment.

## Owner-executed sequence after explicit merge authorization

1. Re-fetch actual main and PR branch refs, reviews, branch-deletion setting,
   status checks and mergeability. Confirm that the exact final heads and the
   intended integration have passed applicable exact-source validation. Any
   new source, conflict resolution or CI failure requires re-review.
2. Prevent automatic deletion as above. With owner approval, merge #237 to main
   using a merge method that preserves its head as an ancestor; a merge commit
   makes the dependency boundary explicit. Verify the actual main result and
   its hosted checks.
3. Explicitly retarget #238 to main, bring the current main into the branch if
   needed, resolve conflicts by retaining merged fixture/provenance coverage,
   and re-run applicable exact-head and merge-source CI. After owner review,
   merge #238; confirm the resulting main identity and checks.
4. Explicitly retarget #239 to updated main, integrate main and resolve
   changelog, testing/coverage, ADR-0004, engine and test-registration overlaps.
   Preserve the independently reviewed endpoint kernel, Match-on oracle and
   #238 controls. Review and validate the exact new head and merge result before
   an owner-authorized merge.
5. Explicitly retarget #240 to updated main; retain all correction records,
   existing thresholds and the merged fixture while resolving documentation
   overlaps. Confirm ADR-0074 is still Proposed, H4 shipping code is unchanged,
   and nonshipping candidates remain evidence only. Review and validate exact
   head and merge result before an owner-authorized documentation merge.
6. Confirm all dependent bases directly, retain the prerequisite branch as
   directed, and only then consider any separately authorized repository-setting
   restoration or branch cleanup. Re-run final integration checks against actual
   main; a pre-merge validation commit is not post-merge evidence.

If a squash or rebase merge is chosen instead, the old prerequisite head is not
necessarily an ancestor of main. Reconstruct or merge each dependent branch
against the actual main, verify that prerequisite files do not appear as new
review scope, repeat conflict and semantic checks, and obtain review on the new
identities. Do not assume an API's cached base SHA is the branch tip.

The prepared validation identity is `a24b4efdfad4d267a63560079b0451c2c7487596`.
Its [integration record](final-integration-a24b4efd/README.md) describes
retained files and conflicts. Hosted checks are tied to that exact source;
future retarget or merge commits require their own applicable validation.
