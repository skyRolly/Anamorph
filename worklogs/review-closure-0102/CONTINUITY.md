# 0.10.2 review closure — starting checkpoint

Created before new investigation on 2026-11-14. The complete previous execution record is `worklogs/independent-review-followup-0102/REVIEW.md` on `codex/0102-independent-review-record`. The identities below are supplied historical review references, not claims about current refs or final validation.

| PR | Reviewed head | Scope |
|---|---|---|
| #237 | `32e60cb6` | Evidence preservation and partial provenance hardening |
| #238 | `61e82e51` | N2-02 production correction |
| #239 | `383353c0` | N2-03 correction and Match-on regression |
| #240 | `039b122d` | H4 decision package and evidence |
| #242 | `915c2fa3` | POSIX cleanup fixture handshake |

Reviewed main `51e6250d34b44d838a51f9a5086105d39d6b0f29`; prior combined `f2686cb5b0b3687a3e343340f2494f59961aa83a`; historical combined `7764df5c2d02eee7a11c475fcca93097f9559dee`. #241 was already merged. Supplied update: #239 macOS rerun `43572823816` completed successfully; verify remotely. Re-fetch heads, actual base refs, mergeability, review threads, CI and logs. Prototype `codex/0102-provenance-prototype` at `af7d4059372e855b236b17b86ff167c204155358`, implementation `efe12d176da0b5fad8c748ed8bb95daee69c0e78`, is unintegrated and its synthetic controls are not provenance closure.

## Independent review items

- **R-242-01:** test lifecycle requires coverage audit as well as testing documentation. Check other PRs for the same drift.
- **R-242-02:** the 250 ms delayed-parent control is scheduler-dependent; replace with bounded, explicitly synchronized positive and negative controls that establish causality and clean up owned processes.
- **R-242-03:** investigate whether missing/malformed identity after leader exit leaves an owned child alive, unreaped, or reported incorrectly. Preserve strict fixture ownership, bounded cleanup and process-local subreaper behavior. Prior hosted failure's inaccessible artifact does not establish its cause.
- **R-237-01:** source/build provenance remains incomplete across replay and relink tools. `git status` can miss a modified tracked file marked assume-unchanged; examine index flags, raw tracked bytes, sparse trees, dependencies, diagnostic patches, source/configuration/objects/probe inputs. Evaluate and integrate or replace the prototype; preserve original historical evidence byte-for-byte. Run real production/diagnostic replays and isolated mutations as well as synthetic controls.
- **R-239-01:** Match-on output regression was corrected with 392 assertions; historical controls reported 88 original, 44 wet-only, 44 dry-only and one clamp failure. Revalidate with trustworthy replay; keep this coverage distinct from the entire Level-Match algorithm.
- **N2-02:** preserve the focused implementation unless new evidence establishes a defect. Review Match/reset coverage and run isolated original-defect and premature forced-adoption controls on final/integrated source.

## H4 boundaries

ADR-0074 is **Proposed**. No production H4 correction, recovery/performance tradeoff or architecture acceptance is authorized. N2-01's ordinary dry-state transient and V2-01's aggregate-RMS coverage limitation are distinct from A2's confirmed hidden invalid dry state, the separately unisolated extreme-input zero block, and the unvalidated state-scan candidate. The prior zero block also occurred after dry reset, without a Mix dip and on unpatched production; do not ascribe it to A2 or dry re-entry without causal evidence. Preserve the earlier distinct A/C dry-consumption experiment. Trace production stages, state ownership and guards; assess candidate recovery, cost and lower-cost alternatives in nonshipping worktrees. Do not infer representative native timing or DAW capacity from shared-host instruction or timing figures.

## Remaining scope and authorization

N2-01 P2, V2-01 P2 verification and N2-04–07 P3 remain open unless independently established evidence changes their disposition. Preserve current product version, JUCE 9.0.5 pin, parameter IDs, mappings, serialization, identity, accepted ADRs, Test 25 thresholds, and original evidence. Keep PRs open; do not merge, enable auto-merge, delete prerequisite branches, tag, release, bump version or perform release preparation. A proposed ADR document is not architecture acceptance. Separate correctness, verification, integration, owner architecture and release decisions. Record exact-head local and hosted results, inaccessible artifacts, unsupported environments and all unresolved work; do not claim native DAW, listening, representative hardware or Level-5 acceptance without evidence.
