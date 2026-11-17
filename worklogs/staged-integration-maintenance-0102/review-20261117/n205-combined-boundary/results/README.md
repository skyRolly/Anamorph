# N2-05 combined-engine injected boundary: zero and wrapped latency

Three serial receipt-controlled engine-only relinks used main `230da0e2`,
receipt `31bf22e2`, pinned JUCE and spec/probe `0d386977`. Each diagnostic
contains the exact combined b182 production engine, including N2-04, N2-06 and
N2-07; the naive and flagged variants add only their selected N2-05 comparison.
The three complete receipts, all output streams and runner logs are retained.
The environment restarted before the enclosing shell status was observed;
companion verification rehashed the 12 recorded streams and checked all exits
and declared markers, while each runner log reaches its final expected exit.
This is source-equivalent combined-engine evidence, not an exact b182 Git-tree
build, an actual merged main, or natural reachability of the injected state.

The probe deliberately injects NaN into an active Wet LR4 state at Mix=0 and
checks zero latency and actual x4 wrapping with six samples of latency. Counts
are stereo samples in one 64-sample block:

| Variant | Fault block exact Dry | Next block exact Dry | Wet invalid after fault | Re-entry zeros |
| --- | ---: | ---: | --- | ---: |
| Combined engine | 0/128 | 128/128 | no | 0/128 |
| Naive exact Dry | 128/128 | 128/128 | yes | 128/128 |
| Reset-indication diagnostic | 128/128 | 128/128 | no | 0/128 |

The same selected counts occurred at both latencies; each clean twin had zero
re-entry zeros and the Mix clock advanced. A direct selected recovery-contract
assertion failed as intended for baseline and naive variants and passed for the
flagged diagnostic in both lanes. N2-04's independent conditioned-ring scan is
present in all three; the historical flagged-only 116/128 next-block result
without N2-04 remains a separate source identity.

The reset indication detects a non-finite `wet-dry` difference, which can also
arise from finite-operand overflow; it does not identify the originating stage
or detect all hidden invalid state. The experiment does not establish Match,
repeated recovery, reset/re-prepare, preservation of all valid Wet/opposite-
channel histories, full exceptional allocation or cost. It does not select a
production policy or accept Proposed ADR-0075.
