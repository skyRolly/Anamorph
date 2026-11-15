# Unpatched production post-Enable search — reviewed integration 70bb

A new clean checkout of reviewed integration source `70bbdc22`, tree
`e968d53284a38704a1f3982428a134c23252f3c3`, used pinned JUCE 9.0.5 and a
fresh full target build. The committed specification/probe `a6eab0c7` ran
without a source patch. The receipt identifies the source, dependency, build,
objects, probe executable and streams; runner and program exited 0. This is
actual unpatched production processing on the reviewed integration identity,
not yet the final staged main commit.

Across the same 216 finite-input Enable-fade cases as the candidate search,
all 108 requested x4 cases observed wrapping, 124,200 interior fade ticks ran,
conditioned samples remained finite, and the clock matched. The observed
post-Enable aligned scratch was non-finite on **57,572 channel-samples** and
the delayed measurement reference on **55,857**, with no non-finite aligned
ring slot retained at block-end. The exact per-case outputs are retained. At
2 bands, 1000 Hz, +/-0.49*FLT_MAX alternating every 64 samples, both OS-off and
x4 cases reported **40** non-finite aligned samples, matching the candidate's
untraced count for that workload. Other cases differ markedly because shipping
does not have the candidate's pre-Enable dry-state/output recovery.

These observations establish non-finite aligned/reference samples in unpatched
production under extreme finite input. They do not by themselves distinguish
finite-endpoint interpolation overflow from already non-finite dry-filter
output. The separately prepared receipt-checked engine-only trace targets that
causal boundary. Block-end ring finiteness is not proof that invalid history was
never written/read before a reset. No ordinary-input or audible impact claim is
made; N2-08 and H4 re-entry remain distinct until a causal control says otherwise.
