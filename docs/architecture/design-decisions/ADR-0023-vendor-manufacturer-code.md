# ADR-0023 — Vendor manufacturer code `Anmf` → `RTec` (product-line identity)

**Status:** **Accepted** — the *decision* is signed off. Architecture Review cleared by the
maintainer on **2026-07-30**, together with the Level-5 identity check (see *Verification
performed*).

**The `COMPATIBILITY_POLICY` exception is NOT yet fully satisfied.** It needs all four conditions,
and only three are met:

| Condition | State |
|---|---|
| 1 — an ADR records the decision | ✅ this document |
| 2 — migration plan, or the identity carve-out | ✅ carve-out 2a/2b/2c (below) |
| 3 — the **Release Compatibility Checklist** passes | ❌ **OPEN** — never completed for this release (`HANDOVER.md`, Release Status). It is a **release-time** gate, not a merge-time one, so it does not block landing this change; it does block cutting `v0.9.1`. |
| 4 — Architecture Review Gate cleared | ✅ 2026-07-30 |

Condition 3 is deliberately left open rather than waved through: this ADR states below that it
"must not claim a green gate it did not observe", and the release checklist is exactly such a gate.

## Context

`PLUGIN_MANUFACTURER_CODE` is a 4-character OSType identifying the **vendor**, not the product.
Anamorph was created as RollyTech's first plug-in and took `Anmf` — an abbreviation of *Anamorph*,
the product. That was serviceable while there was one product.

RollyTech is now building a second plug-in, **Anabasis** (a mastering loudness maximizer), as part
of a product line. A vendor code that spells the *first product* would have to be inherited
verbatim by every later product, so every RollyTech plug-in would forever identify its
manufacturer as "Anmf" — a code that means nothing to anyone who has not used Anamorph, and that
reads as a different vendor's abbreviation to anyone who has.

## Problem

Either:

- keep `Anmf` and have the whole product line carry the first product's abbreviation as its
  vendor identity, permanently; or
- change it to a code that spells the company — accepting that the manufacturer code is
  **host-facing identity**, so changing it breaks the identity every already-distributed build
  established.

The change cannot be deferred and taken "later, more carefully": it gets strictly more expensive
with every build that reaches a user, and after a public release it is effectively impossible.

## What the code actually controls

- **AU:** the manufacturer field of the component description. `Anamorph.component` is registered
  under `aufx / Anmr / <manufacturer>`; the manufacturer is part of the component's identity, and
  `auval` addresses it by that triple.
- **VST3:** the plug-in's class UID is derived by JUCE from the manufacturer code, the plug-in
  code and the plug-in name. Changing any of the three changes the UID.

Consequently a host that recorded the old identity in a session does not load a *changed* plug-in
— it fails to find the plug-in at all and reports it as missing. This is more visible, and less
dangerous, than a silent behavioural change: nothing is mis-restored, and re-inserting the plug-in
recovers a working session (its saved *parameter* state is keyed by the plug-in's own
serialization, which is untouched).

## Options

- **A. Keep `Anmf` forever.** Zero breakage. The product line permanently identifies its vendor by
  the first product's abbreviation, and every future RollyTech plug-in inherits that.
- **B. Change to a company-spelling code now, before the first tag.** Chosen. Breaks the identity
  of builds already given to testers; costs one documented, recoverable disruption at the cheapest
  moment it will ever be available.
- **C. Change it at v1.0.** Same breakage, strictly later, against a larger installed base, and
  after the point where `COMPATIBILITY_POLICY` treats the identity as settled. Strictly worse
  than B.
- **Which code:** `RTec` (chosen), `RolT`, `Roll`, `RlyT` were considered. All satisfy the AU
  requirement of at least one uppercase character (Apple reserves all-lowercase OSTypes). `Roll`
  was rejected as a common English word with a correspondingly higher chance of colliding with
  another vendor's registered code. `RTec` was chosen by the owner: it reads as *RollyTech* with
  the company's two components both present (R + Tec).

## Decision

`PLUGIN_MANUFACTURER_CODE` becomes **`RTec`** in v0.9.1, and is the vendor code for **every**
RollyTech plug-in from now on. Anabasis adopts the same value at its P1 skeleton, before it has
ever built, so it never carries an identity it has to change.

`PLUGIN_CODE` (`Anmr`), `BUNDLE_ID` (`com.rollytech.anamorph`), `COMPANY_NAME` (`RollyTech`),
`PRODUCT_NAME` and the parameter/serialization surface are **unchanged** — this ADR changes the
vendor field only.

**The code is frozen from the first annotated tag.** After that, a second change would be a
`COMPATIBILITY_POLICY` breach with no remaining "before the first release" justification, and there
is no scenario in which this ADR is a precedent for one.

### This ADR also amends `COMPATIBILITY_POLICY.md`

`COMPATIBILITY_POLICY` §"The only exception" required, as condition 2, that "a **migration plan**
preserves old sessions (a read path / default for the old form)". That condition is **unsatisfiable
by construction** for an identity change: the host matches on the identity, so once it changes the
host never reaches the plug-in's state-restoration code and there is no read path to write. Left
as-is, the policy would have declared this change both permitted (it is an exception) and
impossible (condition 2 can never be met) — a rulebook that contradicts itself is worse than one
that forbids the change outright.

This ADR therefore adds an explicit **identity carve-out** to condition 2, satisfied by all of:
**2a** no annotated release tag exists at all; **2b** a documented recovery procedure in
`KNOWN_ISSUES.md`; **2c** the ADR records the changed field as frozen afterwards.

2a is the load-bearing clause, and it is a **condition on the state of the world, not a token this
exception consumes**: it is true while the product has never been released — for every identity
field at once — and becomes permanently false when the first annotated tag is cut, again for every
field at once. So before the first tag the carve-out remains available for `PLUGIN_CODE` or
`PRODUCT_NAME` too (a product rename, say); after it, for none of them. That is what makes the
carve-out non-repeatable across a product's life without making it artificially scarce beforehand.

Per `ADR_POLICY.md` rule 5, a Policy change is enacted by an ADR, which is what this section does.

## Consequences

- **Sessions saved with any pre-0.9.1 build report Anamorph as missing.** Recovery: re-insert the
  plug-in and re-load the preset, or re-dial the settings. Recorded for testers as **KI-016**.
- **Logic/GarageBand users must let the AU be re-scanned**; the old component identity disappears
  and the new one appears. `auval` is now `auval -v aufx Anmr RTec`.
- **Automation lanes and presets are unaffected in themselves** — parameter IDs and the state
  schema are untouched — but a lane belongs to a plug-in *instance*, so it is lost with the
  instance the host can no longer find.
- **The installers and file locations are unchanged**; `com.rollytech.anamorph.*` package
  identifiers are unchanged, so an upgrade install still replaces rather than duplicates.
- **The DSP, the parameter surface and the serialized state are bit-identical to 0.9.0.** The
  registry-snapshot and state-compatibility suites pass unchanged, which is exactly the point:
  this change is invisible to every gate the repository automates, and is therefore recorded here
  rather than relied upon to surface itself.

## Verification performed

- **Level-5 identity check — PERFORMED (2026-07-30, maintainer).** The 0.9.1 build was loaded in a
  host, the new identity registers, and `auval -v aufx Anmr RTec` was run on macOS. This is the
  only check that actually exercises the change, and it is a **human sign-off**: it is not
  reproducible from CI and no automated gate in this repository can substitute for it (nothing in
  the suite observes plug-in identity — that is the whole reason this ADR exists).
  `[Verified — manual, not headlessly reproducible]`
- **Architecture Review — signed off (2026-07-30, maintainer).** Clears the
  `ARCHITECTURE_REVIEW_GATE` item and condition 4 of the `COMPATIBILITY_POLICY` exception.
- `AnamorphTests` + `AnamorphStateTests`: **expected unchanged** — neither compiles the plug-in
  identity into an assertion. Confirmation comes from the CI run on the change set, not from this
  ADR. `[Unverified in-repo]`
- pluginval: **expected unchanged**; it validates the built VST3 wherever the UID lands.
  `[Unverified in-repo]`

The two `Unverified in-repo` rows are deliberately *not* upgraded by the sign-off: the maintainer
confirmed the identity behaviour, which is what needed a human. The suites and pluginval are
machine-checkable and are reported by CI on this change — an ADR must not claim a green gate it
did not observe (constraint C2/C7).

## Related code

- `CMakeLists.txt:364` (`PLUGIN_MANUFACTURER_CODE RTec`), `:356` (`PLUGIN_CODE`, unchanged),
  `:14` (version 0.9.1)

Evidence [Verified (code) / Unverified (host behaviour)]:
- Source: CMakeLists.txt:362-367
- Changelog: `[0.9.1]`
- Known issue: `docs/KNOWN_ISSUES.md` KI-016
- Policy: `docs/policies/COMPATIBILITY_POLICY.md` (the exception this ADR satisfies),
  `docs/policies/ARCHITECTURE_REVIEW_GATE.md`
- History [Unverified]: the reason `Anmf` was chosen originally is not recorded anywhere in this
  repository; that it abbreviates the product name is evident from the string itself, not from a
  documented decision.
