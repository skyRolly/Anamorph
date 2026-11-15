# 0.10.2 final review and focused H4 checkpoint

This checkpoint preserves the assignment and the preceding committed record
before new investigation. The identities below are historical review
references supplied at assignment start, not assumptions about current refs.
Current remote refs, merge state, review threads and exact-head CI must be
fetched independently before making readiness claims.

## Previous record and reviewed identities

The prior continuity and decision evidence is committed on
`codex/0102-independent-review-record` at
`f826947bccda75f99f35ffb2675dd95b24bdf154`. The previously reviewed main was
`51e6250d34b44d838a51f9a5086105d39d6b0f29`; #241 and #242 are reported
already merged and their actual merge identities and post-merge validation
must be established. Reviewed heads were #242
`73c68f7af90fe0d437ea2087d3cb48f9153b7b89`, #237
`738b7b23211bb4c26258b3f8912efdad03ea4556`, #238
`f08d4ebfdc3710f3d3cae961a8d2c43a74c85fec`, #239
`30560d330a707f6087f14baacace78ffeac9b4df`, #240
`b49c1345d07396524618d6be517c21edfa4beb4f`. The historically validated
combined source is `3669cf716b27de56dd125f29d0b2672598aebdc2`, preceded by
`f2686cb5b0b3687a3e343340f2494f59961aa83a` and
`7764df5c2d02eee7a11c475fcca93097f9559dee`. Its historical local DSP/State
results were 5614/0 and 16555/0; its Rosetta results were 5610/0 and 16400/0.
Do not present these as validation of a subsequently changed source.

## Pending independent review findings

- R-237-01, P2 provenance: separate correctness of maintained infrastructure
  from completeness of historical H4 replays. Preserve the mapping and the
  original full finding rather than silently declaring historical results
  revalidated. Retired unsafe relinkers remain disabled.
- R-237-02, P2 verification: test the actual source verifier against a
  diagnostic deletion whose original tracked path remains present. Determine
  whether to support path-set changes with complete verification or explicitly
  reject unsupported additions/deletions/renames. Verify modes, types and
  valid existing diagnostics, including negative controls.
- R-237-03, P2 verification: investigate generated include directories,
  include precedence and shadowing, generated configuration and other compiler
  inputs in fresh builds and receipt-checked relinks. Bind applicable inputs or
  fail closed and require fresh builds. An actual Anamorph receipt-controlled
  negative case is required. Preserve existing N2 mutation isolation.
- Review result-artifact completeness, including required CSV/oracle/Callgrind
  side files, and select a maintainable CI policy for lightweight guard tests.
  A successful executable exit is not proof of a complete experiment.
- R-239-01 is previously resolved at the assertion level; preserve the 392
  endpoint checks and 88/44/44/1 isolated mutation sensitivity. Preserve the
  independent N2-02 implementation, 720 engine/20 processor controls, original
  and forced-adoption mutations, and 56-check Match/reset evidence.
- R-242-01/02/03 were resolved in the fixture harness. Confirm their actual
  merged main state and post-merge validation without redesign absent a new
  defect. The historical `d8627e55` failure and earlier #240 Rosetta failure
  remain unattributed unless original-attempt evidence becomes available.

## H4 decision boundaries

N2-01 and V2-01 remain P2. ADR-0074 is Proposed; no shipping H4 change,
recovery-contract change, or architecture acceptance is authorized. Preserve
Test 25 assertions/thresholds and the nonshipping failing-on-main oracle.
The A2 output-only hidden-state witness, the separate N2-08 extreme-finite wet
LR4 overflow, and the test-injected dry-state Mix-consumption experiment have
separate causal scopes. Do not conflate them. Previous injected re-entry results
were 128 affected samples at zero latency and 122 with actual x4 wrap and six
samples latency; they model an invalid dry state, not proof of its natural cause.

Develop a bounded candidate decision checkpoint. Verify active per-bank and
per-channel dry-state counts 0/4/10/16 for 1/2/3/4 bands; establish bank/fade,
consumption, aligned-history, and recovery boundaries; compare existing H4,
unguarded warming, output-only A2, full state scan and consumption-aware
alternatives. Keep candidate implementations in nonshipping evidence. Measure
candidate-specific costs only under matched, recorded source and workload
premises. Historical A2 whole-engine instruction increases of about
15.3/27.4/36.9% at 48 kHz/128 for 2/3/4 bands do not measure a new candidate,
CPU percentage or native real-time capacity. Preserve rejected short-history,
reduced-output and numerical-representation limitations. Do not claim a full
recovery contract or representative native hardware evidence without proof.

N2-04–07 remain P3; N2-08 is a separate P3 review assessment. No unrelated
production correction is authorized. Preserve the JUCE bit-exactness contract
when assessing later N2-08 alternatives.

## Integration and authorization

Proposed remaining order is #237 → #238 → #239 → #240, subject to refreshed
branch graph and evidence. Keep #237's branch until dependent PRs have been
explicitly retargeted. Prepare a new isolated combined source on actual current
main, preserving merged #241/#242, both production fixes, all test and lint
registrations, Proposed ADR-0074, evidence and documentation. Compare against
`3669cf71`; validate exact new PR heads and integration identities. Historical
H4 research is not automatically a blocker for independently verified N2
corrections or merging a Proposed ADR document.

Preserve product version 0.10.1, JUCE 9.0.5 pin, compatibility contracts,
accepted ADRs and original evidence. Do not merge remaining PRs, enable
auto-merge, delete prerequisite branches, change production H4, accept ADR-0074,
make unrelated P3 production fixes, bump version, tag or release. Do not claim
native DAW, listening, representative performance or Level-5 acceptance
without evidence. Commit new findings, results, limitations and owner choices
under worklogs and update applicable live records and PR descriptions.
