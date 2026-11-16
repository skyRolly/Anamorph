# Independent maintenance PR pairwise merge rehearsal

Read-only `git merge-tree --write-tree` comparisons used the actual main
`230da0e2` common base and current PR heads #243 `6a65b1d1`, #244 `544ff751`,
#245 `44ef025a`. No PR branch, base or main was changed and no merge was
performed. Raw merge-tree outputs are retained. Changed-file overlaps are
11 for #243/#244, 12 for #243/#245 and 17 for #244/#245.

- #243/#244: seven textual conflicts, all in documentation/changelog.
- #243/#245: seven textual conflicts, all in documentation/changelog.
- #244/#245: twelve textual conflicts, including `tests/dsp_tests.cpp` test
  registration and `scripts/check-citations.py`, plus documentation/changelog.

The engine file auto-merges textually in these pairs, which is not proof of
combined behavior. If the owner later authorizes these independent PRs to be
merged, update and review each remaining base explicitly, preserve each test
registration, combine the documentation and citation changes, and rerun the
applicable exact-head/merge-source matrix and isolated regression controls.
A pairwise virtual merge is neither a resolved integration tree nor combined
validation. No new maintenance PR merge is authorized by the prior four-PR
integration approval.
