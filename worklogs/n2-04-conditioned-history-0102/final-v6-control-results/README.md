# N2-04 final v6 isolated controls

These three receipt-controlled relinks ran **serially** against the fresh
exact-PR-head `6a65b1d18a992c738b3f004f7690ea87d510bd40` build and receipt
at commit `facafb27ff02cec560c01417189f3b620c147901`. The committed control
specifications and probe support are at `de67f6bd6dc0de40166e17c9a6b12615c0ad0e7a`;
the test header's SHA-256 matches the exact PR head. The source, pinned JUCE,
generated inputs, link objects, probe and result identities and commands are
recorded in each `identity.json`. No concurrent source, dependency, build or
Git-index writer was intentionally run during these controls.

| Control | Outer runner | Executable | Assertion result |
| --- | ---: | ---: | --- |
| Positive | 0 | 0 | 125/0 |
| Original full clear only | 0 | 1 expected | 28 failures / 125 |
| Unsafe no-sanitization retention only | 0 | 1 expected | 6 failures / 125 |

The diagnostic trees were respectively
`7c9e8eae7af575757a0d945dc2a3c94e5161e8bb` and
`81fb9c0a05a53a55194d53821fba7aa13426106d`; the committed patch bytes only
change the engine translation unit. These are expected negative-control
failures, not product-test failures. Their logs and receipts preserve the
original bytes; compressed patches and logs do not replace their recorded
SHA-256 identities. Hosted exact-head validation is reported separately.
