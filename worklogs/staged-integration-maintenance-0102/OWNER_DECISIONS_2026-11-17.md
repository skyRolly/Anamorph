# Owner decisions and continuity — 2026-11-17 UTC

This is the starting record for the numerical-maintenance integration and
recovery investigation. The identities below are reviewed historical references;
actual remote refs and validation must be fetched again before acting.

## Authorization and boundaries

The owner authorizes focused review corrections and staged merge-commit
integration of #243, #244 and #245 once their review and validation conditions
are satisfied, with branch updates and focused conflict resolution. The owner
also authorizes a complete current-Unreleased changelog evidence audit and
correction. The N2-04, N2-06 and N2-07 correction directions are accepted
subject to those checks; N2-07 virtual-host instruction counts are not native
performance acceptance. New maintenance PRs need separate merge authorization.

The owner authorizes independent N2-05, N2-08, N2-09 and H4 investigation and
Proposed architecture/numerical decision packages. A focused N2-09 production
correction may be prepared if it implements accepted contracts without an
unapproved architecture or numerical-contract change. ADR-0074 remains
Proposed; no production H4 implementation, N2-05 detection-boundary change,
N2-08 change to JUCE numerical equivalence, or new exceptional-output policy is
accepted. No version bump, tag, release or publication is authorized.

Recovery investigation must preserve valid signals, opposite-channel and
unrelated state and histories, prevent consumption of known invalid data,
restore normal processing under an explicit bounded allocation-free contract,
and may investigate bounded affected-path differences when history is proven
unrecoverable. These objectives are not acceptance of a specific algorithm.

## Reviewed references

- Continuity record `59363ac3`; reviewed main
  `230da0e2b0b81422f7e3964eea2833c48266a963`; prior intended integration
  `70bbdc22acdfd6286d13b1a15cb445367525c5e5`.
- #243 `6a65b1d18a992c738b3f004f7690ea87d510bd40`: N2-04.
- #244 `544ff75165b9b89f7b60606a231f3aaba35cc681`: N2-06.
- #245 `44ef025aab46e09a8be7069184ce79071b7d5a09`: N2-07.
- Independent nonshipping H4 research `codex/0102-h4-recovery-followup` at
  `f511198dcf24b480579e61c47c4f5a949f1f1aa7`.
- #237–#242 had already been merged. At the reviewed checkpoint all three
  maintenance PRs targeted main, were non-draft and mergeable, had auto-merge
  disabled, and had applicable exact-head hosted checks passing. Automatic
  head-branch deletion was false. These are not claims about current refs.

## Open review and decision work

R-244-01 and R-245-01 identify missing implementing Evidence Sources in the
Unreleased changelog; the same issue affects the merged N2-02 and N2-03 entries.
R-245-02 identifies insufficient independent sensitivity to right-channel-only
Drive endpoint call-site regressions. Preserve the focused production fixes;
review and fix these issues, including isolated right-channel controls.

The previously rehearsed maintenance PR pairs had documentation/citation and
one test-registration conflict; no resolved combined maintenance source had
been validated. Recompute with final heads, validate an isolated combined source
and isolated mutation controls, then integrate #243 → #244 → #245 if the actual
conditions support that order. Verify actual main after every merge.

N2-05's naive Dry selection hides injected invalid Wet state. A nonshipping
reset-indication candidate preserved the selected fault block; under actual x4
its next block retained 116/128 exact Dry samples, or 128/128 with the
independent N2-04 engine correction. The injected state is not natural
reachability evidence. The additional detection boundary requires architecture
review and broader recovery controls.

H4's bank-local candidate preserved the other live dry bank in selected
injections but did not reproduce the clean affected-channel history after
recovery. Output-only attribution, post-Enable publication and the acceptable
affected-path trajectory remain open. N2-09 is the independently reachable
finite-operand interior Enable overflow; N2-08 is the separate Wet LR4
numerator overflow under the existing JUCE bit-exact contract. Preserve their
separate identities and evidence.

The earlier Windows Configure, Rosetta and POSIX cleanup hosted failures remain
unattributed unless original attempt-specific evidence establishes their cause.
Successful retries do not explain those failures. Earlier owner-attested manual
acceptance retains its original scope; no new human, native-hardware, DAW,
listening, Level-5 or release acceptance is inferred.
