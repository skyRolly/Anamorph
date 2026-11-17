# ADR-0075 — Mix-zero output selection and recovery ownership

**Status:** Proposed. No new detection boundary, recovery algorithm or
production change is accepted by this document. ADR-0009 remains Accepted and
ADR-0074 remains Proposed.

## Context

ADR-0005 requires the clean, bit-exact Mix=0 null; ADR-0009 requires finite
output and self-healing without an output limiter. On reviewed main
`230da0e2b0b81422f7e3964eea2833c48266a963`, the engine advances Wet and Dry,
combines them, measures the result, then the existing final-output guard
sanitizes non-finite output and resets its stateful nodes. It also clears
aligned-dry and oversampling-compensation delay; raw bypass reads are separately
validated. The separately reviewed N2-04 correction preserves finite
conditioned-ring samples and sanitizes invalid ones within that existing guard.
Its integration is a separate fact, not assumed by this proposal.

The owner seeks normal valid-input behavior, preservation of independently
valid Wet, Dry, opposite-channel state and histories, prevention of known-invalid
state or sample consumption, bounded affected-path recovery when information is
irrecoverable, and bounded allocation-free real-time execution. These objectives
are not an approved exceptional-output algorithm or a replacement for the
Accepted ADRs.

## Problem

At exact Mix=0, the historical expression can propagate an invalid Wet value
through `0 * (wet - dry)`, losing independently valid Dry (N2-05). Selecting Dry
alone can hide the invalid Wet output from the final guard, leaving invalid Wet
state for later Mix re-entry. A finite final sample does not prove every internal
state finite. Conversely a non-finite `wet - dry` can result from finite operand
subtraction overflow, so it does not by itself identify the failing stage.

Existing whole-node and history reset behavior and Level-Match ownership must
be assessed separately from exact output selection. ADR-0009 records an older
question about full `loudness.reset()` discarding Match gain; the processor
parameter-NaN semantics in its historical discussion were separately clarified
by Accepted ADR-0067. This proposal does not reopen that resolved parameter rule
or decide how much measurement state may be preserved.

## Options

1. **Retain or defer:** keep the existing final-output-only guard and document
   the Mix=0 limitation. No new detector or reset semantics; valid Dry may be
   lost in the demonstrated fault block.
2. **Select Dry plus an explicit indication:** select independently finite Dry
   at the endpoint, carry a demonstrated invalid-arithmetic indication to a
   defined recovery owner, and retain the existing final reset initially. The
   diagnostic detects a non-finite difference, including finite subtraction
   overflow; it neither attributes an earlier stage nor detects hidden invalid
   state that still yields finite arithmetic. Its broad reset can discard other
   valid state and its full cost/coverage is unmeasured.
3. **Earlier Wet boundary detection:** inspect a specified Wet result before it
   can be masked, then trigger a defined local or existing recovery path. This
   changes detection scope and work; a finite Wet output still does not prove
   all recursive state valid. The chosen boundary, invalid inputs, measurement
   ordering and possible same-block histories need independent verification.
4. **State-aware or path/channel-local recovery:** track or validate precisely
   the state and samples that can later be consumed and reset only established
   invalid state. It could retain more valid work, but correctness, ownership,
   fault attribution, state cost and bounded execution are not established.
5. **Skip Wet at Mix=0:** rejected as a direct inference from the endpoint
   witness: it changes Wet state continuity and potentially Measure behavior.
   A different proposal would require its own architecture review.

## Decision

**Pending.** No option is approved for production. Recommend a bounded
comparison of option 2 and a precisely specified earlier-boundary alternative
against the current integrated baseline, with option 1 retained as an explicit
risk decision. The review must specify detection ownership, the sample and
history validity boundary, recovery trigger and timing, reset scope, Match
behavior and the affected-channel trajectory. A signal-flow/detection-boundary
change needs the Architecture Review Gate; acceptance of this Proposed record
must not be inferred from its publication.

## Consequences and required verification

An eventual candidate must preserve ordinary valid-input output and clocks,
parameter and latency contracts, and independently valid paths to the extent
established by its policy. It must prevent known-invalid sample publication or
later consumption; reset cannot recreate a lost recursive trajectory. It must
state how zero and nonzero latency, actual wrapping, repeated recovery, Mix
re-entry/reversal, Enable, Match, reset and re-prepare are handled. Test actual
engine output, state and histories against independent references; include the
naive Dry-only mutation, delayed detection, lost valid current/delayed Dry,
invalid Wet surviving re-entry, and collateral reset controls. Instrumented
fault injection must be distinguished from natural reachability. Measure normal
and exceptional work and exceptional allocations before asserting real-time
suitability. H4's dry-state policy and N2-09's Enable arithmetic remain distinct.

## Related code

`src/dsp/AnamorphEngine.cpp`: Multiband Enable, aligned and conditioned delay,
Mix, Level-Match and final-output self-heal. See ADR-0005, ADR-0007, ADR-0009
and Proposed ADR-0074. This record changes none of those executable paths.

## Evidence and confidence

The [source-bound injected-state comparison](https://github.com/skyRolly/Anamorph/tree/5940c4859274ce89f1444c2b001eba4413171d92/worklogs/staged-integration-maintenance-0102/numerical-main230/n205-boundary-results)
found naive Dry selection preserved 128/128 selected fault-block samples but
left injected invalid Wet state, then lost 128/128 samples at re-entry. This
establishes that selected boundary, not natural reachability of the injection.
The [reset-indication diagnostic](https://github.com/skyRolly/Anamorph/tree/5940c4859274ce89f1444c2b001eba4413171d92/worklogs/staged-integration-maintenance-0102/numerical-main230/n205-flagged-results)
preserved that selected boundary; the [finite-input witness](https://github.com/skyRolly/Anamorph/tree/5940c4859274ce89f1444c2b001eba4413171d92/worklogs/staged-integration-maintenance-0102/numerical-main230/n205-flagged-natural-results)
returned 128/128 exact Dry versus 0/128 on main, but did not independently
observe the flag or attribute the upstream failure. An [actual x4 interaction](https://github.com/skyRolly/Anamorph/tree/441fe002/worklogs/staged-integration-maintenance-0102/numerical-main230/n205-x4-results)
retained 116/128 next-block samples with the indication alone and 128/128 when
the independent N2-04 correction was included. Its injection is deliberate,
its enclosing session exit was not observed, and its selected numerical outputs
were separately inspected. These scoped results do not establish a complete
recovery contract, final-main behavior, native performance or owner acceptance.

## Combined-engine boundary follow-up (2026-11-17)

A [source-bound selected-boundary comparison](https://github.com/skyRolly/Anamorph/tree/e48de7dc5e67b4ed10583693a978accdbbd721dc/worklogs/staged-integration-maintenance-0102/review-20261117/n205-combined-boundary/results)
used main `230da0e2` verified build objects and the combined b182 production
engine, with N2-04, N2-06 and N2-07 present. At both zero and actual x4/six-
sample latency, deliberately injected invalid Wet state caused the combined
engine to lose the 128-sample stereo fault block; naive Dry selection preserved
it but left Wet invalid and lost 128 samples at re-entry. A reset-indication
diagnostic preserved 128/128 selected fault and next-block Dry samples, healed
the monitored Wet state and produced no selected re-entry zeros. Assertions
against the selected recovery contract failed for the first two variants and
passed for the diagnostic. All recorded executable exits and output hashes
were independently rechecked; the enclosing session exit was unobserved.

The indication detects a non-finite difference, not its source, and can miss
hidden state whose current output is finite. This injection does not establish
natural reachability, complete Match behavior, all valid state/history
preservation, repeated recovery, reset/re-prepare, allocation or cost. The
result advances the comparison; it does not select option 2 or establish an
accepted recovery contract. Actual post-integration main still needs its own
source identity and validation.
