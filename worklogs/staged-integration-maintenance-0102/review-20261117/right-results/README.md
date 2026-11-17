# R-245-02: right-channel Drive endpoint controls

Five serial Linux/Ninja receipt-controlled relinks used actual main
`230da0e2`, baseline receipt `31bf22e2`, pinned JUCE, and probe/spec commit
`5c19439a8ab00f8041f0fe2eb3c0263fa27f960f`. The probe header matches
`tests/n2_drive_endpoint_tests.h` on PR #245 head `a6b66c25` byte-for-byte.
Engine-only candidate and right-only mutation trees and patches are recorded
in the receipts. Every outer runner exited 0; baseline and mutation
executables exited 1 as expected.

| Variant | Checks / failures | Right-channel sensitivity |
| --- | ---: | --- |
| Main | 20 / 9 | Old settled, wrapped and held paths fail |
| Candidate | 20 / 0 | Both channels pass |
| Right settled expression restored | 20 / 5 | Two right settled assertions fail; affected right twin also breaks its ramp premise/output and right wrapped output |
| Right smoothing expression restored | 20 / 1 | Right terminal-ramp assertion fails |
| Right held-envelope expression restored | 20 / 1 | Right held-envelope assertion fails |

The positive run observed 577 discriminating terminal samples per channel,
zero terminal mismatches, and actual x4 wrapping. The held-envelope case
intentionally injects an OS fade; it establishes that processing boundary, not
natural host reachability. The independent saturation reference remains the
settled endpoint oracle; the wrapped and held checks use their stated bounded
output conditions. These engine-only relinks are not a complete PR-head build.
The original 13-check whole-call-site, non-finite and interior controls remain
in the prior record; they have not been silently relabeled as these executions.
