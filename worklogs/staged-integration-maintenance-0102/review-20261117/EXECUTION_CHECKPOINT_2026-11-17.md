# Numerical maintenance execution checkpoint — 2026-11-17

This is a progress record, not a statement that the maintenance PRs have
merged or that pending checks passed. The owner's authorization and boundaries
are recorded in [OWNER_DECISIONS_2026-11-17.md](../OWNER_DECISIONS_2026-11-17.md).
Actual main at this checkpoint is `230da0e2b0b81422f7e3964eea2833c48266a963`.
Repository merge commits are enabled and automatic branch deletion is disabled.

## Reviewed and corrected PR heads

| PR | Corrected head | Review corrections and present boundary |
| --- | --- | --- |
| #243 | `381c89c26c08a40c2bbe2aa01c861630593b5390` | Retains N2-04. Adds the missing #238 and #239 Unreleased evidence references and associated coverage audit. Its exact-head hosted workflows have passed. |
| #244 | `3775c392989db9808f67725d8d15a63dfcc08ffc` | Adds #244 changelog evidence and an actual wrapped terminal Mix endpoint observation. The preceding head failed hosted Linux builds because the new local variable shadowed another local; this head renames it and retains the observation. Its hosted validation is pending at this checkpoint. |
| #245 | `a6b66c252cd4e819e9ceb0289284580313af1a16` | Adds #245 evidence and right-channel-only Drive endpoint observations. Independent right-only settled, smoothing and injected held-envelope mutations failed 5, 1 and 1 checks respectively, while the corrected 20-check selector passed. Its exact-head hosted workflows have passed. |

All three were non-draft, targeted `main`, were reported mergeable and had
auto-merge disabled when queried. Their actual heads and base must be queried
again immediately before each merge. The direct changed-file intersections
against main are 11 for #243/#244, 12 for #243/#245 and 17 for #244/#245.
Source, registrations, citations, documentation and changelog require semantic
as well as textual review. The intended order remains #243, #244, #245.

The Unreleased audit found the same missing implementing-source reference in
N2-02, N2-03, N2-06 and N2-07. The corrections cite their actual PRs; N2-04
already cited an implementing commit. Each current entry was checked for an
evidence source and verification marker. Existing document checks do not prove
that an evidence reference is semantically relevant, so the latter remains a
human review obligation. Historical entries were not rewritten.

## Disposable integration, validation and preservation

Combined commit `d5e38a534cb2252476b0569ae6fda9ae66074dbb`, tree
`65938e2ca5f0068f2ed404870a88a5d4da5673f4`, contains actual main and all three
corrected heads. A subsequent source-relative current-issues correction gives
`6466b740a7263aa02ede5b5fb9414282fd86042e` (tree
`d90937f16a787a2a4f61c50dd61c16eedc372f2b`); only `KNOWN_ISSUES.md` and its
documentation-coverage audit differ. It removes the stale pending-PR wording,
identifies N2-05 as open, and makes no assertion that this branch is main.
Before and after that edit, documentation and citation checks passed. The
current disposable source is not merged main. The
previous `b182cc0a` source had the same production files, and locally passed
N2-04 125/125, N2-06 16/16, N2-07 20/20, N2-03 392/392, N2-02 720/720,
full DSP 5775/0 and State 16555/0. Its hosted Build & Validate failed the
shadow warning in the new Mix test; its successful CodeQL and Microsoft
analysis apply only to that historical head. The corrected `d5e38a53` local
[fresh replay](combined-d5e-results/README.md) has since passed those same
focused selectors and full DSP/State suites with an observed runner exit of 0;
its hosted workflows and the 6466 source's fresh and hosted validation remain
separate. Earlier rejected and abandoned
attempts are retained in [stale-head-attempt](stale-head-attempt/README.md) and
[combined-b182-results](combined-b182-results/README.md).

The 17 isolated mutation specifications and final test-support bytes are
recorded in [combined-mutations-final-tests](combined-mutations-final-tests/README.md).
Their serial execution is in progress and results are not yet claimed here.
The runner is serial because its receipt-controlled relinks share mutable
diagnostic build inputs. The archive
under `worklogs/dsp-numerical-audit-0102/original` was compared against actual
main and remains byte-identical. The research branch and the evidence
prerequisite branch remain present; no automatic or manual deletion is planned.
The [initial attempt](combined-mutation-initial/README.md) identified a
downstream Mix dependency in the N2-03 output oracle, retained its expected
failure, and corrected the control specification without changing test code.

## Independent research and owner decisions

- [N2-05 combined-engine controls](n205-combined-boundary/results/README.md)
  distinguish output selection from Wet-state recovery. Under deliberately
  injected Wet invalid state at zero and actual x4 latency, naive Mix=0 Dry
  selection preserved the fault block but left Wet invalid and produced
  128/128 re-entry zeros; the selected reset-indication diagnostic preserved
  that block and healed the tested state. It does not establish a complete
  detector, Match policy or preservation of every unrelated state. Draft
  PR #246 proposes ADR-0075; no production change or acceptance is implied.
- [H4 fade attribution](h4-fade-attribution/main230-results/README.md)
  demonstrates on the actual processing module that deliberately injected
  finite individual bank outputs can overflow only in their fade arithmetic;
  a separate injected invalid bank leaves the other monitored bank finite.
  Per-bank observation can distinguish these selected cases, but this is not
  a complete engine-level history or recovery contract. ADR-0074 remains
  Proposed. The nonshipping H4 research ref is `b6c1ca299cca7122a626cd312d21e2811c919a3c`.
- [N2-09 numerical decision](N2_09_PROPOSED_NUMERICAL_DECISION.md) records
  naturally driven finite-operand Enable overflow and four independently
  injected actual call-site controls. Accepted ADR-0004/0005 explicitly
  preserve the historical interior expression. The proposed exceptional-only
  widening therefore requires a human numerical-contract decision; no
  shipping implementation has been authorized under the existing contracts.
- [N2-08 checkpoint](NUMERICAL_CONTRACT_CHECKPOINT.md) keeps Wet LR4 overflow
  separate. A widened copied first step is not a full trajectory; changing
  LR4 arithmetic conflicts with the stated JUCE bit-equivalence contract.
  Retention/deferral is a possible risk decision, while selecting a correction
  needs the specified trajectory, state, numerical and architecture evidence.

The owner's decisions on new numerical or recovery contracts, ADR acceptance,
new PR merges, manual acceptance and release remain separate from the existing
authorization to merge #243–#245 after their required checks pass.
