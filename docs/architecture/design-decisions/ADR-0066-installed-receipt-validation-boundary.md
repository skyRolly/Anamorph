# ADR-0066 — Installed receipt ownership, liveness and PKG artifact boundary

**Status:** Accepted. The owner directly confirmed on 2026-10-25 that human
architecture review and approval are complete for this existing decision.
This is the confirmation and record-update date; the original approval date
was not separately specified. Merge/publication, separately required code
review and unexecuted tests are not approved or certified by this confirmation.

**Implementation effectivity (2026-10-25):** effective on main through the owner's
PR #174 merge `04e6ed48acc3532a112ab8a6ea1fb969f4abc83b`. The original human
acceptance provenance is unchanged; this records actual effectivity, not a new
GitHub review submission or approval of any later branch/release.

## Context

Anamorph's namespace decision is already Accepted. The current receipt smoke
checks two required paths, and workflow `!cancelled()` can run installation after a
failed unlabelled liveness step. TESTING_POLICY omits this new installed-state gate.
Raw retained pkgutil evidence uses `install-location`, volume-relative to `/`.

## Problem

Require complete receipt ownership independently of content equality and prevent
a failed checker self-test from authorizing installation or PKG artifact upload.

## Options

- Substitute `location`: contradicted by retained native plists. Rejected.
- Compare only plist/executable or full payload hashes: leaves receipt omissions. Rejected.
- Rely on a failed job with `!cancelled()`: does not suppress later steps. Rejected.
- **Exact owned leaves plus explicit same-job liveness dependency:** proposed.
- Tie the separate ZIP to receipt success: broadens its existing artifact policy. Rejected.

## Decision

Retain native plists/file lists and the measured `install-location` contract. Require
every installed/staged regular-file and symlink path in the named receipt; normalize
safely, accept measured directories and reject foreign/traversal/malformed entries.
Keep independent full tree and symlink-target equality, identity/AU/separation and
disposable-host controls. Default-full native smoke still expects exactly three
receipts. Selected combinations and co-installation require separately identified
artifacts and selection-aware fresh-VM evidence.

Name `receipt_liveness`; installation and PKG upload require its successful outcome.
Same-job self-tests and production-expression fault replay cover failed/skipped
liveness and failed installation with retained diagnostics. No deliberately invalid
installer is published. The separate loose bundle ZIP retains its original gate;
CI artifacts do not constitute a release. The accompanying TESTING_POLICY
addition is approved by this Accepted decision and is effective through the
actual PR #174 merge recorded above. Acceptance alone did not authorize that merge.

## Consequences

Stricter receipt validation and scheduling tests, with raw native fixtures and
head-qualified evidence. No new product dependency, CMake/JUCE pin, namespace,
independent AU/VST3/AppId identity, version, DSP/GUI/state/parameter/latency or
signing/release-policy change. GUI/DAW/historical-upgrade results need their own
evidence. No legacy namespace fallback or receipt migration.

## Related code

`scripts/test-macos-receipts.py`, `scripts/test-macos-receipt-gates.py`,
`.github/workflows/build.yml`, `docs/policies/TESTING_POLICY.md`.

## Evidence and confidence

Old production validators accept omitted resources/signatures/symlinks while trees
match. New regression and ownership-bypass mutants fail for that intended reason.
Production-condition replay exposes old failed-liveness scheduling and verifies the
new explicit dependency. Retained native fixture provenance identifies the exact
original head/run/artifact; new native results must identify their own head. See
`worklogs/2026-10-24-installed-macos-receipts.md`.
