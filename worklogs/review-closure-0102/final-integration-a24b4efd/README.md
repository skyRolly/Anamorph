# Corrected final integration — 2026-11-15

Validation branch head `a24b4efdfad4d267a63560079b0451c2c7487596`, tree
`687c1edbe015ddd1c69dcb33a461ce98296ef230`, merges the State-summary runner
correction from #237 `e64e11fbf54b910aa6a32acafbeb4c3f64815f20` into the
previous integration `0226e033`. The other current PR heads remain #238
`f11dba49b9ff68c48f96d1b4435c9464a1d7bb24`, #239
`b08d514f9f147fd8c1b19e24ec0cba2f9ae57ef6`, and #240
`c84ee8028eb12ee147e1c3b51d543696a26ef263`; their actual base branch is now
`e64e11fb`. Main is `1ade6e1ad128b067930ab216bc355605388eb1de` and contains
merged #241 and #242. The head and base refs were independently resolved.

The only delta from `0226e033` is the verification parser, its synthetic test,
and corresponding testing/coverage documentation. Shipping source and tests
remain byte-identical to `3669cf716b27de56dd125f29d0b2672598aebdc2`, the #242
fixture matches current main, and the archived original audit remains
byte-identical to `6b55dfadcd89c23b7739c1fbb694adef707bed92`. The
[prior checkpoint](../final-integration-0226e033/README.md) describes the
conflict resolutions and retains the real State-summary parsing failure. The
new merge was textual-conflict-free and ADR-0074 remains Proposed.

Local checks on `a24b4efd`: the toolchain-container provenance suite passed
22 tests; state coverage and its 110-case self-test, portability, real-time,
documentation and citation checks passed. Their logs are retained here. The
unchanged fixture's local ordinary-user handshake, fault and three-case suite
passed on `0226e033`; those exact files are unchanged at this head. This is
not a native Rosetta result.

Full exact-source replay and hosted validation were started separately:
`integration-a24b4efd-controls-spec.json` requests full DSP and State plus
independent engine mutations; `integration-a24b4efd-clamp-spec.json` requests a
fresh header mutation. Hosted Build & Validate run `43708860006`, CodeQL
`43708864706` and Microsoft analysis `43708864659` refer to `a24b4efd`.
Record their actual outcomes as they become available; earlier green checks
are not final-head results. The prior integration's Build & Validate run was
cancelled by the newer source; its separate CodeQL and Microsoft checks passed.

The CodeQL run on prior #237 head `10ddde5e` listed current SARIF artifacts,
but a new read-only download attempt returned HTTP 403 from the artifact store.
The status of the CodeQL job was accessible; the raw SARIF contents were not.
The temporary signed download URL is intentionally not retained. This access
limit is separate from the earlier inaccessible failure-specific Rosetta and
cleanup artifacts.
