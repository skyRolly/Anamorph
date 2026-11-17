# N2-09 — proposed Enable interior numerical decision

**Status: Proposed for human review; no accepted amendment or shipping
implementation.** This is a decision package and scoped diagnostic evidence,
not an ADR registered on main. It must not be read as approval to change the
current production kernel. Main at this checkpoint is `230da0e2`; the
combined maintenance rehearsal is `b182cc0a` and has not been merged.

## Existing requirement and conflict

Accepted ADR-0004's N2-03 implementation note says the shared Enable kernel
“keeps the original float expression strictly inside the ramp.” Accepted
ADR-0005 likewise states the interior float expression remains unchanged.
Those statements have no exception for an infinite intermediate from finite
operands. A fallback that changes such a result therefore contradicts their
literal recorded scope, even if all **finite original results** stay bit-exact.
`AGENTS.md` lists a “conflict with an Accepted ADR” as a hard-stop change;
`docs/policies/AI_AGENT_POLICY.md` says the agent must stop and request human
review when a change contradicts an Accepted ADR. The owner has authorized an
N2-09 fix only if the existing accepted numerical/architecture contracts permit
it, and has not accepted a new contract in this assignment. Consequently no
shipping fallback is implemented here. This is a substantive numerical
exception, not merely a source citation update.

ADR-0004's R-01 Bypass correction is relevant precedent, not automatic
approval: in that distinct stage the existing float result is retained when
finite and an overflowing finite-operand interior is recalculated in double.
ADR-0009's finite-output/self-heal objective and ADR-0005's aligned reference
also matter. Avoiding an intermediate infinity can change whether the existing
final reset occurs, so unchanged ordinary bits alone do not settle recovery or
measurement consequences.

## Established evidence

The [source-bound combined-engine trace](n209-combined-trace/main230-engine-results/README.md)
reproduced the naturally driven dry-Enable example with the N2-04, N2-06 and
N2-07 engine corrections present: 40 finite-operand overflows and aligned-ring
writes/reads at OS off and actual x4. It is an instrumented relink on verified
main-230 objects, not the exact b182 full build. Its optional **dry-only**
widening prevented the selected invalid Enable writes but exposed seven later
dry-source interpolation overflows in the altered trajectory. That secondary
mechanism has not been shown independently reachable on unpatched production;
the 216-case negative search is not a proof of impossibility.

A separate [four-call-site diagnostic](n209-four-lane/main230-engine-results-v3/README.md)
injected selected finite extreme operands into each actual Wet/Dry L/R Enable
call during OS-off and actual x4 execution. Its positive passed 46/46 selected
checks, including a separately ordered long-double convex oracle (two-float-ULP
bound), actual terminal endpoint observations, selected aligned history and
latency-offset reference checks, and non-finite operand recovery controls.
Omitting each call-site fallback produced two failures. Those injections are
not natural reachability of every lane; the diagnostic is an engine-only
comparison, not an amended shared kernel, a complete Match oracle, a complete
recovery policy or representative performance evidence.

## Candidate amendment text — not accepted

Subject to owner approval and the verification below, amend the N2-03 interior
statement in ADR-0004 and ADR-0005 as follows:

> At exact Enable coefficients 0 and 1 the existing source/destination
> selection remains bit-exact. For a coefficient strictly inside [0,1],
> evaluate the historical float expression first and return its result
> unchanged whenever that result is finite. If and only if that result is
> non-finite while both operands are finite, recompute the same convex blend
> with sufficient intermediate precision and convert to float once. Keep the
> historical handling of originally non-finite operands and coefficients
> outside the supported interior. Preserve the existing single smoother tick,
> four call sites, signal order, latency, H4 gate and measurement-reference
> ownership. This exception does not promise recovery from unrelated invalid
> recursive state or a later dry-source interpolation overflow.

The proposal needs an actual compiler-portable implementation and tests; the
quoted wording is not itself evidence of complete behavior. Whether the
revised contract permits an altered self-heal trajectory for finite inputs
must be explicitly reviewed together with ADR-0009 and Level-Match.

## Owner options and consequences

1. **Approve the narrowly specified exception, conditional on full verification.**
   It may preserve representable finite blends and avoid the demonstrated
   invalid aligned write, at the cost of a changed exceptional result and
   potentially changed subsequent recovery. The selected secondary dry-source
   overflow remains separately open. Ordinary finite result bits and endpoint
   semantics are to remain unchanged.
2. **Retain the historical expression and defer N2-09.** This preserves the
   current numerical contract and existing final reset, while the confirmed
   extreme-input aligned-history/processed-output limitation remains open.
3. **Request a different bounded mechanism or larger recovery design.** It
   requires a precise placement, history/Match behavior, real-time cost and
   the applicable architecture review; a guard that merely replaces output
   with silence is not equivalent to reconstructing valid aligned history.

Recommendation: the first option is worth further review because it directly
addresses the demonstrated finite-operand overflow and preserves observed
ordinary results, but accept it only with explicit owner numerical review and
completion of the engineering checks below. The owner may instead retain or
defer the P3 limitation with a recorded release risk decision. This is not a
request to accept ADR-0074 or any H4 policy.

## Remaining engineering before a production PR is review-ready

- Reproduce the trace on actual post-integration main and compile the selected
  shared-kernel change in a fresh full build, rather than treating the
  engine-only diagnostic as production proof.
- Independently test all four call sites, signed-zero endpoints, arbitrary
  finite interior results, out-of-range coefficients and non-finite operands;
  keep the existing N2-03 and Match-on/off regression assertions and sensitive
  mutations. Use the independent numerical oracle and actual history reads.
- Establish Match/reference and final recovery behavior, zero and actual
  wrapped latency, repeated transitions and reset/re-prepare. Characterize the
  separate dry-source interpolation consequence without silently combining it.
- Verify allocation-free bounded real-time execution, ordinary and exceptional
  cost, portability, full DSP/State and the applicable hosted matrix on the
  final proposed source. Update the relevant ADRs, TESTING, coverage inventory,
  Signal Flow descriptions and changelog under the documentation policy.
- Obtain the explicit human decision required for the conflict with Accepted
  ADRs before implementing a contradictory shipping contract. A new maintenance
  PR would require its own merge authorization.
