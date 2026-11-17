# N2-09 four call sites: invalid-operand recovery control

This serial receipt-controlled relink repeats the selected combined-engine
experiment in `../main230-engine-results-v1/` using probe/spec commit
`57ba0fe2511a9d3e61d8fcc0802f45e01092e434`. The source, JUCE, build,
diagnostic patch, executable and result identities are in `identity.json`.
The runner exited 0 and the five executable exits were 0, 1, 1, 1 and 1 as
specified. It is an instrumented combined-engine relink on verified main-230
objects, not the exact b182 Git-tree build or merged main.

The positive passed **46/46**: in addition to v1's four call-site, selected
history/reference, finite-interior and endpoint checks, all four deliberately
injected non-finite operand lanes retained the original invalid classification,
reached the existing final recovery as observed by the Multiband running-state
reset, and returned finite output. This is the selected OS-off invalid-input
boundary; it does not establish all Wet/Match/state recovery behavior. Each
omitted call-site fallback failed two assertions on the selected OS-off/x4
injections. Injections are not natural reachability evidence.

The earlier naturally driven N2-09 trace and secondary dry-source overflow
counterfactual remain separately recorded. The provisional double expression
is repeated by the test's bit comparison, so this version is not an independent
mathematical rounding oracle; that limitation is not hidden by the passing
call-site checks. The diagnostic is nonshipping and does not amend Accepted
ADR-0004/0005 or approve a numerical change.
