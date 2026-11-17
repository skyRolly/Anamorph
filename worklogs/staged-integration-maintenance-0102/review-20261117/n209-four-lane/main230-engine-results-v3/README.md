# N2-09 four call sites: independent convex oracle

The receipt-controlled Linux/Ninja engine-only relink used main `230da0e2`,
receipt `31bf22e2`, pinned JUCE, the b182 combined production engine plus the
nonshipping call-site diagnostic, and probe/spec commit
`b7f5199d500956cbf40efcf87ffe97a8e9fa6065`. The complete identity binds the
selected source, patch, compiler inputs, objects, executable and outputs. The
outer runner exited 0; the positive and four omitted-call-site controls exited
0, 1, 1, 1 and 1 as specified. This is not an exact b182 Git-tree build.

The positive passed **46/46** checks across OS off and actual x4/six-sample
wrapping. It observed 40 deliberately injected finite-operand overflows over
all four actual Enable call sites, 20 selected aligned-history writes and 20
latency-offset reference comparisons. Each repaired value was compared both to
the diagnostic affine calculation and to an independently ordered long-double
convex expression within two float ULPs. The probe also observed four injected
originally non-finite operand cases reaching the existing final recovery, 260
actual observations of each terminal coefficient, and unchanged selected finite
interior results. Omitting each call-site fallback produced two failures.

This is selected injection and direct call-site/history evidence, not natural
reachability of all injected operand pairs or an exhaustive rounding proof.
The separate naturally driven combined-engine dry trace still shows the
secondary dry-source overflow after its dry-only fallback. Match, broader
recovery, portable compiler behavior, cost and full-engine regression remain
unestablished. The candidate modifies only a nonshipping diagnostic translation
unit; Accepted ADR-0004/0005 and shipping source remain unchanged.
