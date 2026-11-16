# N2-07 candidate: isolated Drive-path controls, v3

Actual integrated main `230da0e2b0b81422f7e3964eea2833c48266a963`, pinned
JUCE `d60b7a78030661574a615b586d9325ba7a7fa7dd`, baseline receipt
`31bf22e2`, probe and specifications `5c9358a5`, and the candidate patch and
expected tree in the receipts were used for seven **serial** receipt-controlled
Linux/Ninja relinks. All seven outer runners exited 0, with the specified
negative executable exits. The patch changes only the Drive engine translation
unit; these are diagnostic executions, not clean production PR builds.

| Engine variant | Checks | Failures | Meaning |
| --- | ---: | ---: | --- |
| Unmodified main | 13 | 5 | Settled positive/negative, wrapped, held and discrimination premise |
| Candidate | 13 | 0 | Scoped positive result |
| Restore settled expression | 13 | 5 | Includes changed settled reference and its premise |
| Restore unwrapped smoothing expression | 13 | 1 | Actual terminal ramp output |
| Restore wrapped held-envelope expression | 13 | 1 | Wrapped held contribution |
| Remove non-finite-operand protection | 13 | 1 | Existing self-heal no longer observed |
| Change interior expression | 13 | 1 | Selected recorded interior output bits |

In the positive run the unwrapped ramp actually reached its endpoint within a
block and 577 terminal inputs distinguished the old arithmetic; it matched its
settled twin with zero selected mismatches. The x4 path had six samples of
reported latency. A deliberately injected OS fade and ongoing Drive gain ramp
exercised the held-envelope path (`held_path=1`); that injection is not proof
that this state combination is naturally reached through host automation. The
selected interior output hash agrees with main under this particular compiler
and workload; it is not a cross-platform proof or a whole-domain oracle.

The v2 experiment and its failed discrimination premise remain separately
preserved in `../n207-v2-failed-premise`. These results do not establish
complete production regression, hosted portability, native performance,
listening impact or owner approval. N2-06 and the H4 recovery decision remain
independent. All receipts, stdout, stderr and runner logs are retained here.
