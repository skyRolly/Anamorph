# N2-09 trace with the combined production engine

The receipt-controlled Linux/Ninja relink used main `230da0e2` and fresh-build
receipt `31bf22e2`, pinned JUCE, spec/probe `32b65b83` and the combined
`b182cc0a` production `AnamorphEngine.cpp` plus an explicitly identified
engine-only trace and disabled-by-default dry-Enable widening switch. All other
`src/` bytes match between main 230 and b182. This is an instrumented,
source-bound engine execution using main-230 objects; its Git tree and binary
are not the complete b182 build or actual merged main. The outer runner and
probe exited 0, and the spec checked execution markers and nonzero overflow,
actual OS selection/latency, and the selected trace invariants.

At two bands, 1000 Hz, Drive 6 dB, Mix .4, a Multiband Disable fade and the
previous finite alternating +/-0.49*FLT_MAX input, the original dry Enable
expression again produced **40** finite-operand interior overflows, **40**
invalid aligned-history writes and reads, **40** invalid Mix outputs, **40**
scrubs and **four** self-heals, both OS-off and actual x4/six-sample latency.
The first dry operands and coefficient match the historical `70bbdc22`
trace; no non-finite pre-Enable dry operand was recorded. The test-only
dry-only exceptional widening prevented those selected invalid ring writes,
while the altered trajectory reached **70** corrected Enable overflows and
**seven** later dry-source interpolation overflows and one self-heal in each
case. The Wet values in those later witnesses differ from the historical trace
because the combined engine includes the separately corrected Drive endpoint;
these results are not silently reported as a historical rerun of identical
bytes. Finite original interior float results observed by this trace remained
unchanged.

This reproduces the selected N2-09 mechanism with the three proposed production
corrections present. The dry-only widening is a counterfactual, not a complete
four-call-site correction or an accepted numerical policy. The secondary
boundary is demonstrated in that altered trajectory; the earlier 216-case
unpatched search did not prove independent reachability. No audible impact,
complete Match oracle, native performance, exact b182 full-suite result,
actual-main integration or release acceptance follows.
