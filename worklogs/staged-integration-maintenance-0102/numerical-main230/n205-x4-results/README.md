# N2-05 and N2-04: actual x4 history boundary

Three serial receipt-controlled Linux/Ninja relinks used actual integrated main
`230da0e2`, pinned JUCE, baseline receipt `31bf22e2` and specification
`4ff61f0634e9feb007c778c41ebfdf43b5824525`. The test deliberately injects a
NaN into an active Wet LR4 integrator after warm-up at Mix=0 and compares an
uninjected twin. It asserts actual x4 execution and six-sample latency; this is
not evidence that the injected state arises naturally. Each receipt records its
expected executable exit 0 and declared execution markers; a separate check
rehashed all three stdout/stderr files. The enclosing command session became
unavailable after the environment resumed, so its final exit is not asserted.
Each retained runner log reaches `run 0 exit 0 expected 0`. Numerical outcomes
below are separately inspected observations, not declared numerical oracles.

| Variant | Fault block exact Dry | Next block exact Dry | Re-entry zeros | Wet invalid after fault |
| --- | ---: | ---: | ---: | --- |
| Main | 0/128 | 116/128 | 0/128 | no |
| N2-05 flagged diagnostic | 128/128 | 116/128 | 0/128 | no |
| Flagged diagnostic + #243 N2-04 engine change | 128/128 | 128/128 | 0/128 | no |

The selected flagged-only path preserves this fault block but the existing
self-heal still clears the conditioned delay, losing six delayed samples per
channel in the next block. Adding only the independently proposed N2-04 engine
change preserves those selected 12 samples in this comparison. The combined
patch is `38180928c8ba79c74fdee69efe8c7a5f84b3cb4def4fb6cb631a298c210d9e8b`,
expected tree `f50db51e88fad822e220d6ddad6f5e2617fe8d55`. It is a diagnostic,
not a merged source or proof of all N2-04/N2-05 interactions. The clean twin had
no re-entry zeros in every lane; the Mix clock advanced. These observations do
not establish other sample rates, reset/re-prepare, Match, other faults,
complete Wet or opposite-channel state preservation, or a final recovery
contract. The N2-05 additional detection boundary still needs architecture
review; #243 remains a separate unmerged PR.
