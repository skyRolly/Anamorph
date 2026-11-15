# H4 consumption-aware candidate: follow-up controls — 2026-11-15

The isolated, nonshipping consumption-aware candidate and its first 40-check
positive run are committed in PR #240 at `c84ee8028eb12ee147e1c3b51d543696a26ef263`.
The engineering decision remains Proposed; this record is separate follow-up
work, not shipping source.

A freshly rebuilt negative candidate changes only the detector's selection so
that it scans the active bank even when the recorded block also processed the
other bank. The exact patch, resulting tree and original probe are in
`h4-consumption-active-only-spec.json`; its new [receipt and raw output](h4-consumption-active-only-results/identity.json)
record base `003c9a1a2558da6bd89e2624421a6e205eb8acdf`, diagnostic tree
`2ebae9d2c9be067a0e788f37e0d5cc8a6c24864b`, patch SHA-256
`76af9a06cb63039ebd162c3253c2514a84dbe3fd19f9992f2f3e947ee0c44784`, and
pinned JUCE. The runner exited zero because the selected negative executable
exited one as expected. It reported **40 checks, two failures**, specifically
that one live fade bank and the source retiring during the block were missed.
The unmutated candidate passed those same checks. This demonstrates sensitivity
to the named detector boundary; it does not establish natural occurrence of the
injected invalid states or the complete recovery contract.
