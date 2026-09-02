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
   descriptive document about the same topic.
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
- Source: src/dsp/AnamorphEngine.cpp:494-955
- Test:   tests/dsp_tests.cpp :: testNoClicksAcrossTransitions
- Commit: 6a24b82
```

At least one source is mandatory for any historical, design-decision, incident, risk, or
known-issue claim.
