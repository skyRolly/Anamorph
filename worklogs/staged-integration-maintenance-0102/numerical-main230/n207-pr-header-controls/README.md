# N2-07 PR test header — isolated controls

The header bytes match PR #245 head `f78b53208623fbd7702194e41bd931d708a837fa`
(SHA-256 `e53af484a9d7f2c98dd3ebb9d7c7e9d30a2a2e93f4cf94ad3d5104d9b894ba2d`).
Seven serial Linux/Ninja receipt-controlled relinks used main `230da0e2`,
pinned JUCE, baseline receipt `31bf22e2` and committed specification
`f44995fd42e92a92936e7bfde71e7f8bfb9f44b2`. The diagnostic engine's executable
changes match the PR; its diagnostic comments and a source citation differ.
This is not a clean complete-PR build. Every outer runner exited 0 and every
negative executable exited 1 as specified.

| Variant | Checks | Failures |
| --- | ---: | ---: |
| Main | 13 | 5 |
| Candidate | 13 | 0 |
| Settled expression restored | 13 | 5 |
| Unwrapped smoothing expression restored | 13 | 1 |
| Wrapped held-envelope expression restored | 13 | 1 |
| Non-finite protection removed | 13 | 1 |
| Interior expression changed | 13 | 1 |

The selected positive ramp reached its terminal point inside a block and had
577 input samples that distinguish the old expression. The x4 wrap reported
six samples of latency; the held-envelope OS fade is deliberately injected,
not evidence of natural host reachability. The interior oracle compares the
historical expression with the candidate's independently exercised saturation
twin for its selected inputs; it is not a whole-domain proof. Output, source,
object and command identities are in the receipts.

The first invocation supplied an abbreviated specification commit and was
rejected before compilation, as required by the verifier; its log is retained
as `abbreviated-spec-rejected.log`. A full commit was used for the successful
serial runs in new output directories. The earlier v2 probe's failed premise
and v3 diagnostic controls remain separate records. Exact-head full-build and
hosted results must be reported under their own identities.
