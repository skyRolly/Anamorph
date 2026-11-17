# H4 live-fade output attribution: direct module experiment

On actual main `230da0e2b0b81422f7e3964eea2833c48266a963` (tree
`a322ae4cab7c4656a3fd2beb465f3e62e31d8f9c`), the accepted Linux/Ninja
receipt-controlled relinker used fresh-build receipt `31bf22e2`, pinned JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd`, and spec/probe commit
`72cce83aeee9b0084f901d6732e95f356dc10846`. The unmodified production
`MultibandWidth.cpp` object was retained; its SHA-256
`1274a734a1f99aeff3ba507b00c62b49cc69bd17e9acd9f7ff011c3cac0249ba`
also matches combined rehearsal `0ea2299b`. The complete receipt binds the
compiler inputs, build objects and executable (`100190cb60720652a5068ca9d586dfe4be8d844a21d3779f5fdde31d6f6494bb`).
The outer runner exited 0; positive and three intentionally false assertions
exited 0, 1, 1 and 1 as specified.

The probe calls the actual two-band `MultibandWidth::processBlock` during an
observed live bank fade. It deliberately injects values into the two dry LR4
integrators, advances independent copies of both filters using the same input,
and checks that actual states match those copied trajectories. In the first
lane, both individual reconstructed outputs and both checked post-step states
were finite, but `db-da` overflowed and the actual fade output was `+Inf`;
the double-precision convex comparison was finite and representable. A
same-direction finite control produced the expected finite float expression.
In another lane a deliberately injected NaN made the source-bank output/state
invalid while the destination-bank output/state stayed finite and followed its
independent copy. The unaffected-channel outputs checked in these lanes were
finite. The positive ran **23/23 checks**. Three false-claim controls each
failed **1/24**: that one bank output must be invalid, that finite bank outputs
guarantee a finite fade, and that the finite destination bank is invalid.

This establishes that the combined output alone cannot attribute every fault
to a source bank. Observing each bank output before the fade can distinguish
the tested operand-failure and combined-arithmetic cases; finite state/output
is not proof of a complete valid history. The injected finite and NaN states
are boundary controls, not natural-reachability evidence. This is a direct
module experiment, not a full-engine or candidate recovery experiment: it
does not observe the Enable blend, aligned history, Mix, owner policy,
recovery duration, allocation behavior or native performance. It does not
show that resetting or preserving either injected state is always correct.
The prior source-bound 24 live-fade recovery observations remain separate.
ADR-0074 remains Proposed.
