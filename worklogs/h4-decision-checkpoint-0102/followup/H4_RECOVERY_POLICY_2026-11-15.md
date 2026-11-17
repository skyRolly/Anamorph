# H4 exceptional-output and aligned-history decision checkpoint

ADR-0074 remains **Proposed**. This nonshipping analysis does not select a
production policy. The original policy experiments used reviewed integration
`70bbdc22`; later source-bound experiments use actual integrated main
`230da0e2`. Historical candidate and instruction data retain their original
identities and do not become measurements of a later candidate.

## Existing contracts and owner objectives

ADR-0005 requires phase-matched aligned dry at partial Mix, the clean Mix=0
null, and the established Level-Match reference. H4's existing full-wet gate
uses exact Wet and the delayed clean Measure reference. ADR-0009 requires
finite output and self-healing without a limiter; the real-time policy requires
bounded, allocation-free processing. The owner additionally asks a new dry
recovery design to preserve valid Wet, the opposite channel and unrelated
histories, avoid invalid aligned publication/consumption, and return to normal
phase-matched operation. Those objectives do not by themselves specify the
allowed affected-channel output or authorize changing existing contracts.

A reset does not recreate an unknown recursive trajectory. An injected-state
[actual history trace](history-step-70bb-results/README.md) showed 32/32
newly written finite affected samples differing from the clean twin; output
differences first appeared at offsets 0 and 6 for zero and actual x4 latency.
The checked valid opposite-channel, conditioned and raw histories survived.
This models an invalid state at a consumption boundary; it does not establish
that injection occurs naturally or that all other recovery paths are covered.

## Policy alternatives

| Alternative | Available information and consequences | Current disposition |
| --- | --- | --- |
| Channel-local reset plus sanitized reconstructed samples | Bounded and preserves checked unrelated state, but the missing recursive trajectory is not reconstructed. Finite samples can still be wrong; the candidate's pre-Enable guard also missed N2-09. | Incomplete; needs an accepted affected-channel/history guarantee and post-interpolation protection. |
| Substitute valid conditioned dry temporarily | A [one-sample injected fault-block experiment](conditioned-fallback-70bb-results/README.md) published finite conditioned input and preserved checked unrelated paths at zero and actual x4 latency. It is generally not crossover phase-matched to Wet; 32/32 affected history samples still differed from the clean twin, and invalid conditioned samples need their own handling. The original reset cleared both same-channel banks; a later [bank-local diagnostic](bank-local-main230-results/README.md) preserves an independently valid other fade bank in 24 selected injections but still clears both for unattributed output-only faults. | Scoped positive and negative controls pass; full recovery/phase policy, cost and owner approval remain open. |
| Substitute valid Wet temporarily | Wet can be preserved when only Dry fails, but using it as the affected output changes the requested Mix and does not reconstruct future aligned dry history; it is unavailable if Wet is invalid. | Requires a distinct exceptional-output policy and history strategy. |
| Zero only known invalid samples | Bounded and can preserve finite unrelated data, but a finite bad sample or already-published wrong history is not identified merely by `isfinite`. | Insufficient as a complete contract. |
| Reconstruct from saved state and input history | Exact replay needs an independently valid checkpoint and all relevant input/coefficient/bank transitions since it. The same arithmetic may reproduce overflow; work may spike. Fixed short-history replay already failed low-frequency/high-rate cases. | Needs a materially new bounded mechanism and architecture review; not established here. |
| Retain H4 or defer | Leaves the measured N2-01 re-entry transient and V2-01 verification limitation. | An admissible owner decision with explicit documented limitation. |

The [production N2-09 trace](shipping-trace-70bb-results/README.md) found finite
opposite-sign operands overflowing the historical interior Enable expression,
then actual aligned writes/reads and self-heal at Mix 0.4 with OS off and x4.
A diagnostic exceptional-only widening prevented those writes for that case,
but seven later dry-source interpolation failures remained; that second
boundary is only a counterfactual result so far. A [bounded unpatched search](shipping-secondary-search-70bb-results/README.md) found none in 216 cases; that is not a general reachability proof. Neither a pre-Enable finite
scan nor this one fallback establishes the complete H4 recovery contract.
N2-09 is separate from N2-08 and the N2-03 endpoint correction.

The historical matched consumption-aware candidate cost at 48 kHz/128 was
+1.68%, +15.41%, +27.53% and +37.01% whole-engine instructions for 1–4 bands;
a four-band observed fade was +47.70%. Candidate-minus-A2 was 16–386
instructions/block in selected cells, so continuously maintaining the dry bank
is the dominant measured cost. Reduced-output work previously saved under
0.6%; a new filter representation is not automatically floating-point or
state-trajectory equivalent under glides/fades. These virtual-host instruction
counts are not native CPU time, deadlines or instance capacity. No allocation
was observed with live guards in 24 selected bank-local injected recoveries;
comprehensive exceptional-path and representative-hardware evidence remain
unestablished.

The actual-main replay and the selected first-consumption and live-fade
experiments have now run. Recommend the smallest remaining discriminating
experiment for output-only fault attribution and post-Enable/history
publication, then specify the acceptable affected-channel trajectory and
measure a supported candidate on representative native hardware. The former
is unfinished engineering; whether an altered exceptional trajectory is
acceptable is an owner policy decision. Alternatively the owner may retain or
defer the documented H4 limitation. A material recovery policy or different
filter representation needs its applicable architecture review and explicit
owner approval before shipping.


The [actual-main replay](actual-main230-results/README.md) reproduces the
consumption-aware candidate's focused recovery and first-consumption results
on `230da0e2`; it does not resolve the affected-channel policy or validate
the separate fallback's full behavior.

The later [expanded bank-local run](bank-local-expanded-main230-results/README.md)
adds first-consumption, consumed-state, full-DSP and low-frequency controls on
the integrated source. Its receipt records all seven executions; their streams
and required artifact were independently rehashed, while the enclosing session
exit was unobserved. The live-fade preservation observation is separate. No
complete exceptional recovery policy or native performance claim follows.

## Live-fade arithmetic attribution — 2026-11-17

The [direct production-module control](https://github.com/skyRolly/Anamorph/tree/6b1bbe9b047b54d916b1f9d731e235430d869c72/worklogs/staged-integration-maintenance-0102/review-20261117/h4-fade-attribution/main230-results)
observed a two-bank fade with deliberately injected finite dry states: both
bank outputs and their checked post-step states were finite, yet the float
subtraction in the fade overflowed. A separate injected-NaN lane distinguished
one invalid bank from a finite other bank. The positive passed 23/23 and three
opposite claims failed 1/24 each. This is direct `MultibandWidth::processBlock`
evidence on main `230da0e2`, not a full-engine reachability or recovery test.
It supports separating bank-output attribution from fade-arithmetic validation:
resetting both banks solely because the combined result is non-finite is not
justified by this selected arithmetic failure. It does not prove that the
injected states have valid histories, that any fallback is phase-matched, or
that per-bank checks catch hidden invalid state or post-Enable overflow.

The unresolved engineering question is the first-consumption and history
trajectory under an explicit exceptional-output policy, including post-Enable
validation and actual engine-level attribution. A finite replacement alone is
insufficient. Continue targeted work or retain/defer the documented limitation;
the owner has not accepted a recovery policy or H4 performance tradeoff. No
bank-local candidate cost or native deadline result is inferred from historical
A2/consumption-aware instruction figures.
