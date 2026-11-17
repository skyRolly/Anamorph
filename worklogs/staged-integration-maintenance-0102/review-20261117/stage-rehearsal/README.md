# Staged merge rehearsals — not merged PRs

The owner-authorized sequence is #243, #244, #245. These are disposable local
rehearsals against actual main `230da0e2`; neither is a GitHub PR update or a
merge into main. Re-query every ref, review, setting and required exact-source
check before using them. The two commits are preserved in
[`rehearsals.bundle`](rehearsals.bundle), SHA-256
`c1bc6164a914411ca77ddeea17aa5b2c56a1344a4b0513d53d4042b8ced6d552`.
The bundle requires the already published #243, #244 and #245 heads listed
below; `git bundle verify` passed with those prerequisite objects present.

## N2-04 plus N2-06

Rehearsal `2b536bad854bf5b6e5a172ea4ecdc75e24171fc0` has #244 head
`3775c392989db9808f67725d8d15a63dfcc08ffc` and #243 head
`381c89c26c08a40c2bbe2aa01c861630593b5390` as parents; its tree is
`ba80464f465f605a8e0bde32b189764dbd46ffba`. Seven files conflicted:
CHANGELOG, DOCUMENTATION_COVERAGE, KNOWN_ISSUES, REALTIME_SAFETY_AUDIT,
SIGNAL_FLOW, ADR-0009 and TESTING. The previous reviewed two-PR resolution
`22759c26` supplied the additive conflict choices; the later #244 wrapped-test
documentation changes were then reapplied. The actual combined engine and test
registrations match that earlier two-PR source, and the #244 test header is
byte-identical to its final corrected head.

The first citation check rejected the previously resolved metering reference:
after both changes `scope.pushBlock` is at line 2403, outside the old range.
That was re-derived from source, as were the adjacent self-heal and Bypass
ranges. A second check identified eight further moved citations; the existing
citation tool re-anchored them, and the final checks passed: 211 documentation
files and 603 citation anchors. All failed and passing check logs are retained
here. Re-derive these references again if the actual merged source changes;
do not use a stale line number merely to satisfy a checker.

## All three corrections

Merging the two-PR rehearsal into #245 head
`a6b66c252cd4e819e9ceb0289284580313af1a16` produced 13 conflicted paths,
including `tests/dsp_tests.cpp` and `scripts/check-citations.py`. The final
resolved tree was set to the separately reviewed combined source
`6466b740a7263aa02ede5b5fb9414282fd86042e` after verifying that all three PR
heads are ancestors of that source, the N2-04/N2-06/N2-07 test headers retain
their final bytes, and its changes against the two-PR tree contain the intended
N2-07 implementation, test, documentation and source-relative issue inventory.
Rehearsal `e1ae73f8536634ed6ed10169c29beea31fb0abbb` has tree
`d90937f16a787a2a4f61c50dd61c16eedc372f2b`, exactly the 6466 combined tree.
It is not an actual main commit or evidence that its hosted workflows passed.

Once #243 is actually merged and required checks pass, an integration branch
may use the first resolution, then merge actual current main and verify its
tree and ancestry. Once #244 is actually merged and checked, the same approach
can use the second resolution, then merge actual current main and verify its
tree. If the PR heads, main tree, policies or conflict surface have changed,
stop and re-review rather than mechanically restoring a rehearsal tree. Each
resulting PR head and actual merge source still require their own validation;
the final main commit and post-merge CI remain distinct identities.
