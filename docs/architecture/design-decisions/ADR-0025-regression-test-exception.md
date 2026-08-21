# ADR-0025 — Documented exception to the per-fix regression-test rule, for defects with no stable automated test surface

**Status:** Accepted (maintainer instruction, 2026-08-07)

**Amends:** `docs/policies/TESTING_POLICY.md` rule 1. Per `ADR_POLICY.md` rule 5 and
`SOURCE_OF_TRUTH.md` ("An ADR may change a Policy, but only by an explicit new/updated ADR"), this
ADR is the instrument that makes that change; the Policy text carries the rule, this ADR carries the
reasoning and the bounds.

## Context
`TESTING_POLICY.md` rule 1 reads: *"Every bug fix ships a regression test that fails on the old code
and passes on the fix."* It is stated unconditionally and the Rules section carries no exception
mechanism.

The repository's automated surface is four levels deep (`TESTING_POLICY.md` §Acceptance levels) and
each has a hard boundary. `tests/dsp_tests.cpp` drives the engine directly. `tests/state_tests.cpp`
compiles the plug-in sources and exercises the real `AnamorphAudioProcessor`, but **links the editor
without ever instantiating it**. pluginval validates the built VST3 through a host it does not
control. Level 5 is a human in a DAW.

Some real defects fall between those surfaces. The v0.9.2 preset drop-down crash (**INC-010**) is
the concrete case that forced this decision: a use-after-free that exists only while a modal child
component is open *and* its owner is destroyed. It is not expressible in either self-test target,
and pluginval's editor open/close does not open a menu first.

## Problem
Three ways of handling that, and until now the repository has silently used the third:

- Claim compliance that does not exist. Dishonest, and it corrupts the one signal — a green
  gate — that the release process depends on.
- Block the fix until a test exists. That makes shipping a **crash fix** contingent on building new
  test infrastructure, which is a worse outcome for users than shipping the fix with a disclosed gap.
- Ship the fix and record the gap in a Procedure. This is what v0.9.2 did (`TESTING.md` §Gaps in the
  automated coverage, alongside the pre-existing AU-conformance and golden-audio entries, which
  `KNOWN_ISSUES.md` KI-014 and `RELEASE_HARDENING_PLAN.md` RH-F3 already cite as the canonical
  register). It is honest and it is where readers already look — but a Procedure cannot carry an
  exception to a Policy, so the Policy as written did not describe what the project actually does.

The last of those is the engineering reality. The governance text should say so, **without**
loosening the default: the failure mode to avoid is an exception that becomes the path of least
resistance for any fix whose test is merely awkward to write.

## Options
- **A. Leave rule 1 unconditional and keep recording deviations in a Procedure.** Rejected — the
  Policy then misdescribes the project, and each deviation is re-litigated by the next reader or
  auditor with no rule to point at.
- **B. Weaken rule 1** to "ships a regression test where practical". Rejected — "practical" is
  self-judged and unbounded; it would erode the default that has produced the existing suite.
- **C. A one-off waiver for INC-010.** Rejected on maintainer instruction, and rightly: a waiver
  fixes one entry and leaves the next identical case with no rule.
- **D. Keep rule 1 as the default and add a narrowly-scoped, disclosure-bound exception.** Chosen.

## Decision

### 1. The default is unchanged
Every bug fix ships a regression test that fails on the old code and passes on the fix. This remains
the rule, and it remains the expected outcome. Nothing below relaxes the **release gate**: Levels 2,
3 and pluginval stay blocking exactly as `TESTING_POLICY.md` §Hard release gate states.

### 2. The exception, and the only thing that qualifies for it
A fix may ship without a regression test **only** when no *reliable* automated test can be written
for the defect **because the repository has no stable automated surface that reaches it** — not
because the test would be difficult, slow, or inconvenient.

Qualifying classes, non-exhaustive but indicative of the bar:

- **GUI / component lifetime** — behaviour that only exists while a UI object is alive, being torn
  down, or interacting with another that is (INC-010).
- **Host-owned UI behaviour** — what a DAW does with focus, key routing, window ownership or modal
  state. The plug-in observes it; it cannot drive it. (KI-009 is the same class.)
- **OS-level asynchronous behaviour** — window-server, input-method or scheduler behaviour the
  process does not command (KI-017 is the same class, though it needed no fix).

**Explicitly not qualifying:** a defect reachable from `dsp_tests`/`state_tests` whose test is
laborious; a defect that a *reasonable extension* of an existing surface would reach; anything where
"no test" really means "no time". If a surface exists, rule 1 applies.

### 3. Four disclosures, all mandatory
Invoking the exception is only complete when all four are recorded. A fix missing any of them has
not met rule 1 as amended:

1. **Why no reliable regression test exists** — which surface it would have to live on, and the
   specific reason that surface cannot reach the defect. A citation, not an assertion.
2. **What verification was performed instead** — the manual reproduction, the structural argument,
   or the source-level proof that replaces the test. "Prevented by construction" is acceptable
   *only* when the construction is named and the mechanism cited.
3. **Where the coverage gap is tracked** — the register entry, so the gap is discoverable from the
   testing documentation rather than only from the commit that created it.
4. **Whether future test infrastructure could close it** — named concretely if so, with what it
   would cost and why it was not done in this change; or stated as structurally impossible if not.
   This is what keeps an exception from becoming permanent by default.

### 4. Where it is recorded
`docs/procedures/TESTING.md` §"Gaps in the automated coverage (known, deliberate)" is **the
register**. It already served that purpose for the AU-conformance and golden-audio gaps; this ADR
makes it the named home for rule-1 exceptions too. The incident's own record (`POSTMORTEMS.md`, or
`KNOWN_ISSUES.md` where the defect stays open) carries disclosures 1, 2 and 4 in its Prevention
field and points at the register.

### 5. It is re-examined, not granted in perpetuity
An exception is scoped to the **absence of a surface**, so it lapses when the surface appears. When
test infrastructure lands that would reach a previously-excepted defect, the register entry is
revisited and the test written; the entry is removed only when the gap is actually closed.

## Consequences
- The Policy now describes what the project does. An auditor reading `TESTING_POLICY.md` finds the
  rule, the bound, and the register in one place instead of discovering a Procedure-level deviation
  and having to judge it.
- The bar is deliberately awkward: four disclosures with citations is more work than writing a test
  for anything that *can* be tested, which is the intended incentive.
- Applied to **INC-010** (the only current invocation), and consistent with how KI-014 (`auval` not
  run) and RH-F3 already use the same register for coverage gaps that were never defects.
- **Not retroactive as an excuse.** It does not bless past fixes that shipped without tests for
  other reasons; it defines the conditions from 0.9.2 onward.
- The exception cannot hide a regression: the *release* gate is untouched, so a fix that breaks
  something with a test still fails CI.

## Related code
- `docs/policies/TESTING_POLICY.md` §Rules rule 1 (the amended rule)
- `docs/procedures/TESTING.md` §"Gaps in the automated coverage (known, deliberate)" (the register)
- `docs/POSTMORTEMS.md` INC-010 (the first invocation)
- `tests/state_tests.cpp:6-11` — the harness comment recording that the editor is constructed and
  destroyed but never SHOWN, which is the surface boundary this ADR is about (it read "linked but
  never instantiated" when this ADR was accepted; the boundary the ADR turns on — no pointer, no
  display, nothing on screen — is unchanged)

Evidence [Verified]:
- Source: `tests/state_tests.cpp:6-11` (the editor is constructed and destroyed, never shown);
  `scripts/run-tests.sh`
  (the two console targets are the whole Level-2/3 surface); `scripts/run-pluginval.sh` (Level 4
  drives a host the plug-in does not control).
- Policy: `docs/policies/TESTING_POLICY.md` rule 1 as it stood before this ADR;
  `docs/policies/ADR_POLICY.md` rule 5; `docs/SOURCE_OF_TRUTH.md` (ADR → Policy amendment path).
- Precedent for the register: `docs/procedures/TESTING.md` §Gaps, cited as canonical by
  `docs/KNOWN_ISSUES.md` KI-014 and `docs/architecture/RELEASE_HARDENING_PLAN.md` RH-F3.
- History: PR #100; INC-010.
