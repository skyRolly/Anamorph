# SOURCE_OF_TRUTH.md

Defines documentation authority and conflict resolution for the Anamorph repository.
When two sources disagree, the **higher** source in the list below wins, and the lower
source must be corrected to match.

## Authority order (highest → lowest)

1. **Source Code** (`src/**`) — the running behaviour. The ultimate ground truth.
2. **Verified Test Cases** (`tests/dsp_tests.cpp`, `tests/state_tests.cpp`) — executable
   assertions that pin behaviour the code must satisfy. A claim proven by code **and** a
   test is the strongest evidence available (`Verified`).
3. **ADR** (`docs/architecture/design-decisions/`) — the final, dated record of a
   design decision. An ADR records *why* a constraint exists and supersedes any
   descriptive document about the same topic. Measurement records in `worklogs/` (including
   `worklogs/JUCE904_UPGRADE_0.9.9.md`, `worklogs/STATE_HOST_HARDENING_0.9.10.md` and
   `worklogs/NATIVE_ICON_INTEGRATION_ROUND1.md`) support an ADR;
   they do not replace its decision or confer owner approval.
4. **Policies** (`docs/policies/`) — invariant constraints (what may not change). A
   Policy outranks descriptive Architecture: where Architecture *describes* and Policy
   *forbids*, the Policy is binding.
5. **Architecture** (`docs/architecture/`) — descriptive system reference.
6. **Procedures** (`docs/procedures/`) — how to build, test, release, troubleshoot.
7. **README.md** — project façade / entry point. Lowest authority for technical detail.

> Note: ADR is the final *decision* record; Policy is the *enforcement* of decisions.
> An ADR may change a Policy, but only by an explicit new/updated ADR (see
> `docs/policies/ADR_POLICY.md`). Policy weight is higher than general descriptive
> Architecture because a Policy is a hard rule, not a description.

## Root-level contributor instructions

The 2026-10-31 Linux correction is governed by Accepted
[ADR-0069](architecture/design-decisions/ADR-0069-linux-plugin-only-installation.md):
VST3-only installation, portable Standalone and preservation of legacy desktop resources.
The owner authorized implementation/validation/PR preparation, with human review
and manual merge required. A later read-only snapshot verifies PR #193 merged at
`4c13688` and the rebuilt unpublished 0.10.0 draft from Release run 41173229942.
Final owner confirmation recorded 2026-10-31 (Asia/Shanghai) explicitly approves all
relevant ADRs and attests real-machine testing completed/approved. This is the authority
for ADR-0069 acceptance and NR-001/002 closure, not an inference from CI or merge.
Detailed host matrix NR-003 is Deferred; unsupplied individual cases remain unverified.
Current identities, remaining decisions and stable forward work-item IDs are in
[`worklogs/NEXT_RELEASE_DEVELOPMENT_ROADMAP.md`](../worklogs/NEXT_RELEASE_DEVELOPMENT_ROADMAP.md),
a developer worklog, not a new authority class or architecture approval.
Historical ADR-0065 and prior release approval/evidence records remain dated history.
The NR-006 [ScopeBuffer review packet](../worklogs/NR006_SCOPE_PUBLICATION_REVIEW.md)
is supporting developer evidence, not an approved replacement for the threading policies.
The [NR-017 diagnostic inventory](../worklogs/NR017_STATIC_DIAGNOSTIC_INVENTORY.md) and
its CSV preserve analyzer observations/dispositions, not GitHub alert state or new approval.

The dated release audit and execution evidence are in
[`worklogs/RELEASE_READINESS_0.10.0.md`](../worklogs/RELEASE_READINESS_0.10.0.md).
It records the original owner's manual approval and automated outcomes separately, plus the
P1 implementation/validation and original-candidate evidence archive. ADR-0067/0068 are
Accepted on the owner's explicit 2026-10-30 approval and effective on main via PR #187
(`b085c87`). Final-main and normal Release gates passed; targeted manual completion is
owner-attested separately. The new unpublished draft and archived original identities are recorded there. The worklog does not replace binding policies.

`AGENTS.md` is the primary root-level contributor/agent guide and belongs to the developer documentation class. It points readers to binding policies and does not override the authority order above. `CLAUDE.md` is retained as a Claude-compatible import of `AGENTS.md`; it is a compatibility pointer, not a separate source of rules.

The [memcheck budget follow-up](../worklogs/CI_MEMCHECK_BUDGET_FOLLOWUP.md)
is supporting developer evidence for CI orchestration and actual job outcomes.
Its parallel-process correction does not change the product thread model,
accept source-level races or confer merge/release approval.

## Scope: the other three documentation classes

The numbered order above governs the **developer documentation** chain. The README's other
three classes sit alongside it, not inside it:

- **User documentation** (`docs/user/`) is *derived*: it must describe what the code and
  developer chain establish, and on any conflict it is the user document that gets corrected
  (per `DOCUMENTATION_LIFECYCLE_POLICY.md`). It never serves as evidence.
- **Internal / testing documentation** (`SUPPORT.md`, `.github/ISSUE_TEMPLATE/`) is derived in
  the same way, with one addition: its statements about what a tester may and may not do
  restate the legal class and must not diverge from it. Where it conflicts with `EULA.md`,
  `SUPPORT.md` is corrected.
- **Legal documents** are two different things and rank differently:
  - `NOTICE` and `THIRD_PARTY_LICENSES.md` are **authoritative for third-party attribution and
    licence facts** — each claim cites the licence file it was read from — and change only
    through the re-verification procedure they describe (`THIRD_PARTY_LICENSES.md` §"How this
    inventory was produced"). Where a developer doc disagrees with them on an attribution fact,
    the developer doc is corrected.
  - `EULA.md`, `PRIVACY.md` and `TRADEMARKS.md` concern **Anamorph's own terms**. `PRIVACY.md`
    is derived from the source and is corrected when the source disagrees. `EULA.md` is an
    unapproved draft and is authoritative for nothing; it may only be changed by, or on the
    instruction of, the owner. `TRADEMARKS.md` is factual except for the items it explicitly
    marks as open.
  - `docs/COMMERCIAL_STATUS.md` is a developer **index** of all of the above; on any conflict
    the record it cites wins.

No document in these three classes may be cited as evidence for a technical claim.

## Conflict-resolution rule

If documentation and source code disagree:

1. **Report the drift** (do not silently overwrite — see `AI_AGENT_POLICY.md` /
   constraint C6).
2. The source code wins **unless** the disagreement is itself a code defect contradicting
   an `Accepted` ADR or Policy — in which case the code change requires Architecture Review
   (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`) and may be the actual bug.
3. Apply the **smallest** correction that re-syncs the document, with an evidence citation.

## Confidence levels (used throughout `docs/`)

| Level | Meaning |
|---|---|
| **Verified** | Provable from current source code, or code + a test case. |
| **Partially Verified** | Supported by README / commit / PR / code comment, but not fully provable from current code alone. |
| **Unverified** | No sufficient factual evidence; could be true but unproven (e.g. real-DAW host behaviour, performance numbers). |
| **Not Supported** | A deliberate, evidence-backed exclusion (e.g. AAX format, mono→mono I/O). Distinct from Unverified. |

## Evidence citation format

```
Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.cpp:1157-1873
- Test:   tests/dsp_tests.cpp :: testNoClicksAcrossTransitions
- Commit: 6a24b82
```

At least one source is mandatory for any historical, design-decision, incident, risk, or
known-issue claim.

The namespace and Windows installer worklogs (`worklogs/2026-10-23-product-namespace.md`
and `worklogs/2026-10-23-windows-installer-icon.md`) support Accepted ADR-0063 and ADR-0064.
ADR-0062 remains Accepted. Final owner approval, cleared architecture blockers and verified
main ancestry are recorded in `architecture/design-decisions/ADR_INDEX.md` under the
2026-10-24 reconciliation. The earlier implementation permission and dated evidence remain
historical; unperformed runtime/manual validation is not converted into a pass.

Historically Accepted ADR-0065 records the separately authorized Linux desktop resource contract;
the owner's direct confirmation on 2026-10-25 completes human architecture/ADR
approval, including the delivered helper snapshot amendment. Worklogs remain evidence.
Implementation is effective through PR #175 merge `14bde706e2d121c9970cf00c757cb12d3178b9a5`.
It is now Superseded by Accepted ADR-0069, effective through PR #193 at `4c13688`.
Future Composer work and native visual certification remain separate.
The existing ADR directory/index and procedures remain the navigation/authority path.

Installed-receipt decision: [ADR-0066](architecture/design-decisions/ADR-0066-installed-receipt-validation-boundary.md) is Accepted by the owner's direct
confirmation on 2026-10-25. PR #174 merged as `04e6ed48acc3532a112ab8a6ea1fb969f4abc83b`;
its installed-receipt implementation is effective on main. ADR-0065 and its
helper amendment were implemented through PR #175 merge `14bde706e2d121c9970cf00c757cb12d3178b9a5`;
that historical Linux contract is superseded by ADR-0069/PR #193.
Native fixtures are in `scripts/fixtures/macos-receipts`;
`scripts/test-macos-receipt-gates.py` checks the production scheduling contract.
