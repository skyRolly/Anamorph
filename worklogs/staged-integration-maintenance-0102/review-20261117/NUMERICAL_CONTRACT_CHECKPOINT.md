# N2-08, N2-09 and recovery contract checkpoint — 2026-11-17

This is an interim source review, not a statement that the three maintenance
PRs have merged. At this checkpoint actual main is `230da0e2b0b81422f7e3964eea2833c48266a963`;
the corrected combined rehearsal is `b182cc0a6d68b105a0226cfcffd9fe6cfe767003`
(tree `d91c4b01c8fda95d11f09cdc4b6627d16a7a84b5`) and its tests are running.
The earlier 0ea rehearsal missed the newest #244 test because of a stale local
ref; the failed marker and superseded attempts are retained in
[`stale-head-attempt/`](stale-head-attempt/README.md).
Historical results keep their original source and environment identities.
The owner's current integration and research authorization is recorded in
[`../OWNER_DECISIONS_2026-11-17.md`](../OWNER_DECISIONS_2026-11-17.md).

## N2-08: wet LR4 finite-input overflow

**Confirmed P3 review assessment** for the recorded extreme finite sequence,
not attributed to H4. A copied active Wet LR4 state and finite next input
produce a non-finite first numerator before multiplication by `h`; the existing
final-output guard then scrubs and resets. Widening that one copied-state step
produced representable low/high output and next state for the first sample
only. [Original trace and controls](https://github.com/skyRolly/Anamorph/blob/f511198dcf24b480579e61c47c4f5a949f1f1aa7/worklogs/h4-review-followup-0102/README.md)
remain scoped to that observation.

`LR4Xover.h` explicitly states expression-for-expression JUCE equivalence for
every input and cutoff sequence, including float rounding. Widening only an
exceptional numerator may preserve ordinary-input bits but changes the stated
contract on the exceptional sequence. It is therefore not automatically
permitted by ordinary-bit comparisons. The owner has not accepted that change.

| Alternative | Established consequence | Decision status |
| --- | --- | --- |
| Retain current LR4 and existing final self-heal | Preserves the accepted JUCE-equivalence contract; extreme finite input can still lose the affected output block. | Viable explicit risk/deferral decision; N2-08 remains open. |
| Exceptional-only widening within LR4 | May avoid the demonstrated intermediate overflow, but diverges from JUCE on affected sequences. One copied step does not establish both sections, state/tails, repeated faults, glides, fades or full-engine behavior. | Needs a separately proposed numerical-contract decision, complete trajectory/twin controls and cost evidence. |
| Other bounded arithmetic in LR4 | Has the same contract question if it differs from JUCE; mathematical output equivalence does not imply bit/state-trajectory equivalence. | No supported implementation selected. |
| Engine-level validation/recovery with unchanged LR4 | Can retain filter implementation identity but changes what state/output the engine consumes or resets and may discard valid information. | Detection/recovery placement needs its applicable Architecture Review Gate and a specified policy. |

A decision to retain/defer is available now with the above known behavior. A
choice of a specific corrective algorithm is not yet evidence-complete. The
smallest useful follow-up is a source-bound full-trajectory twin for a specific
exception-only candidate, including both sections, repeated input, tails,
cutoff glides/bank fades, full-engine comparison and exceptional/ordinary cost.
No production filter change or owner approval is implied here. Whether to defer
this P3 issue for 0.10.2 is a release risk decision, not automatic closure.

## N2-09: interior Multiband Enable arithmetic

**Confirmed P3 review assessment** on historical combined production source
`70bbdc22`: finite opposite-sign dry Enable operands at a strict interior
coefficient overflowed the historical float subtraction. The source-bound
trace observed invalid aligned-history writes and reads and the existing output
self-heal with OS off and actual x4/six-sample latency. The trace was
instrumented; the matching uninstrumented search also found the selected
40-sample observation. [Raw trace](https://github.com/skyRolly/Anamorph/blob/f511198dcf24b480579e61c47c4f5a949f1f1aa7/worklogs/h4-decision-checkpoint-0102/followup/shipping-trace-70bb-results/README.md).
N2-03 did not introduce this interior arithmetic and its exact 0/1 selection
remains required.

ADR-0004 and ADR-0005 record preservation of the original interior expression
in the N2-03 implementation. ADR-0004's separate R-01 Bypass correction is a
relevant precedent: retain a finite original float result and recompute only
an overflowing interpolation between finite operands in double, at the same
stage. It does not automatically authorize changing every other documented
interior expression or recovery outcome. For a finite blend strictly between
0 and 1, a convex combination of two finite float operands is representable
within their range when evaluated with sufficient intermediate range; actual
compiler behavior, the four call sites and the invalid-operand path still need
verification. Endpoint selection, finite historical results, coefficients
outside the supported smoother range, tick ownership, Match/reference, delay
and final recovery must remain covered.

A diagnostic dry-only exceptional widening prevented invalid Enable history
writes in the historical selected trace but exposed a later dry-source
interpolation overflow in that altered trajectory. The separate 216-case
unpatched search found no independent occurrence of that secondary mechanism;
this is not proof of impossibility. An engine trace specification and an
engine-only observation patch for the combined rehearsal are committed under
`n209-combined-trace/`; they are not yet a result or a production correction.

The next engineering step is to run that source-bound trace, then establish
actual Wet and Dry call-site controls, exact endpoint and ordinary float-bit
identity, invalid operand handling, aligned-history and Match behavior, and
sensitive mutations for a complete candidate. If a candidate fits the existing
accepted same-stage contract and its implementation-note clarification, it may
be proposed in a separate PR; an actual change to an accepted numerical or
architecture contract requires owner review and a Proposed decision. Do not
infer this question is settled by the Bypass precedent alone. No new N2-09 PR
or release acceptance is represented by this checkpoint.

## N2-05 and H4 boundaries

A separate documentation-only [draft PR #246](https://github.com/skyRolly/Anamorph/pull/246)
contains Proposed ADR-0075 at `b1f407d0afdc6dfef4fe13eeb73ae79b25802a81`.
It preserves Accepted ADR-0009 and does not select or authorize an early Wet
recovery detector. The selected naive Dry-only and reset-indication experiments
remain narrower than a complete recovery policy. ADR-0074 remains Proposed.

Source review of the actual Multiband fade shows `runDry` computes each live
bank separately, then `da + w*(db-da)` combines their results. A non-finite
combined output alone cannot identify whether a bank result was invalid or a
finite pair overflowed in the combination. The [direct-module attribution experiment](h4-fade-attribution/main230-results/README.md)
compares copied bank states and actual production `MultibandWidth::processBlock`
under deliberately injected states. It passed 23/23 checks and distinguished
finite bank outputs whose fade arithmetic overflows from one injected invalid
bank; three false-claim controls each failed 1/24. The initial compile failure
and its namespace fix are preserved. These direct-module injections do not
establish engine-level natural reachability, a suitable fallback, or a complete
aligned-history policy.

## Changelog evidence and validation boundary

The current Unreleased entries were reviewed against CHANGELOG_POLICY rule 2.
The combined rehearsal cites PRs #238, #239, #244 and #245; the N2-04 entry
already cites its implementing commit; the other current entries cite their
implementing commits. Each has a verification marker. Historical entries and
the recorded historical exceptions are untouched. `check-docs.py` validates
changelog structure, ordering and release links; its checks do not establish
that each cited PR or SHA actually supports a bullet. That remains a human
semantic review. A syntax-only new gate could detect some omissions, but its
bullet/continuation and historical-exception model would need independent
self-tests and could not validate evidence relevance. It is not added as an
unreviewed integration change; a focused future lint can be proposed if the
repository wants that separate maintenance scope.

The first combined fresh-replay attempt correctly rejected an ignored Python
cache created by an earlier help command. A retry was stopped when another
help command wrote the same kind of cache during its build, violating the
no-concurrent-writers premise. Neither attempt is a provenance-qualified
result. A separate clean checkout was started for the actual fresh replay; no
historical results or safeguards were modified.
