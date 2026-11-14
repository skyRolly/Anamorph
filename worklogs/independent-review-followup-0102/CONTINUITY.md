# 0.10.2 independent review follow-up — continuity

Created 2026-11-14 before new investigation. This records the assignment's supplied review context; the identities and conclusions below are historical inputs, not assertions about current heads or independently revalidated results.

## Historical review references

| PR | Reviewed head | Scope |
|---|---|---|
| #237 | `6b55dfad` | Second DSP audit evidence preservation |
| #238 | `61e82e51` | N2-02 ordinary-duck control adoption |
| #239 | `003c9a1a` | N2-03 exact Multiband Enable endpoints |
| #240 | `c64e38b4` | H4 investigation, V2-01, proposed ADR-0074 |
| #241 | `77ff8b18` | CodeQL Action dependency update |

Previously validated combined production source: `7764df5c2d02eee7a11c475fcca93097f9559dee`. Historical suggested merge order: #241 → #237 → #238 → #239 → #240. Historical shared-file counts: #238/#239 12, #238/#240 8, #239/#240 8. Recheck all identities, branch relationships, comments, checks, and conflicts.

## Findings requiring independent reassessment

- **R-237-01**: replay and probe-build provenance: HEAD alone does not establish checkout contents, build origin, object freshness, JUCE identity, or exactly scoped diagnostic modifications. Audit all related helpers; preserve original evidence byte-for-byte; distinguish verified properties from premises.
- **R-239-01**: standalone wet kernel assertions and Match-off engine-output checks may leave Match-on actual wet contribution unverified. Seek an independent production-path oracle; retain endpoint, signed-zero, interior, smoother, Mix, Level-Match, aligned-dry, reference and mutation coverage. No unsupported audible-impact claim.
- **#238 / N2-02**: previous review found no merge-blocking defect; independently reassess ordinary and forced transitions, freeze/deferral, smoother ownership, measurement, reset/reprepare, processor automation and actual output, including negative controls.

## H4 decision context and boundaries

ADR-0074 remains **Proposed**. No production H4 correction or performance/recovery tradeoff is authorized. The supplied evidence reports a low-frequency Mix re-entry transient from skipped recursive dry-filter state (peak ≈ 0.1361084, initial RMS ≈ 0.1014817, initial level difference ≈ −2.5659 dB), an A2 guarded warm-bank candidate passing the focused numerical oracle, a candidate-only exceptional-state recovery issue in unguarded warming, and whole-engine instruction increases ≈ 15.3%, 27.4%, 36.9% at 2, 3, 4 bands (48 kHz/128). Instruction counts, wall time, thread CPU, real-time deadline behavior and instance capacity are distinct. Reassess the oracle, recovery completeness, candidate costs and decision alternatives without modifying shipping H4 behavior or accepting the ADR. Test 25's valid assertions and thresholds remain intact; mandatory N2-01 regression follows an authorized production correction, while the failing-on-main oracle remains scoped nonshipping evidence.

## Remaining findings and constraints

N2-01 P2 H4 dry-state continuity; V2-01 P2 verification H4 regression completeness; N2-04 P3 conditioned-dry-history loss; N2-05 P3 valid dry loss after invalid wet arithmetic at Mix=0; N2-06 P3 Mix=1 endpoint cancellation; N2-07 P3 Drive endpoint cancellation. Do not silently close findings.

Preserve product version, JUCE pin, parameter IDs/mappings, persistence, plugin identity, accepted ADRs, original historical evidence and CI gate semantics. Keep PRs open, unmerged and auto-merge disabled; do not delete prerequisite branches, tag, release or bump versions. Candidate H4 implementations stay separate from shipping source. Validate exact final identities; explicitly record unavailable environments/artifacts and distinguish established results from assumptions and recommendations. No native DAW, listening, hardware or Level-5 acceptance claim without evidence and authorization.
