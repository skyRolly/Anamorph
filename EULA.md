# Anamorph — End-User Licence Agreement (DRAFT)

**Anamorph** · © 2026 RollyTech · Applies to: the Anamorph plug-in and Standalone application
(VST3, Audio Unit, Standalone) and their accompanying documentation.

---

## 0. Status of this document — read first

> **This is an unapproved draft, not a licence in force.** It was prepared inside the
> repository so that the terms a closed-source commercial product needs are written down and
> reviewable. It has **not** been reviewed or approved by the owner or by legal counsel, it is
> **not** presented by any installer, and no build of Anamorph currently ships it.
>
> Every clause below marked **`[OWNER/LEGAL DECISION]`** is a genuine open decision, not
> placeholder prose to be tidied away. Until those are resolved and the document is approved,
> the repository's position is unchanged and is the one recorded in
> [`docs/COMMERCIAL_STATUS.md`](docs/COMMERCIAL_STATUS.md): **Anamorph is closed-source
> commercial software, no licence is granted, and all rights are reserved.**
>
> Tracking: `KI-015` ([`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md)) ·
> `RISK-006` ([`docs/FUTURE_RISKS.md`](docs/FUTURE_RISKS.md)) ·
> `RH-R11` / `RH-F1` ([`docs/architecture/RELEASE_HARDENING_PLAN.md`](docs/architecture/RELEASE_HARDENING_PLAN.md)).
>
> Nothing in this repository is legal advice.

---

## 1. Definitions

| Term | Meaning |
|---|---|
| **Software** | The Anamorph audio plug-in and Standalone application in binary form — VST3, Audio Unit and Standalone builds — together with the installers, presets and documentation distributed with them. |
| **Licensor** | RollyTech. *`[OWNER/LEGAL DECISION]` — the exact legal entity, its form and its registered address are not recorded anywhere in this repository.* |
| **You** | The individual or organisation that installs or uses the Software. |
| **Pre-release build** | Any build distributed before a general commercial release, including internal-testing and beta builds. Anamorph is in this phase today. |
| **Third-Party Components** | The software listed in [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) and attributed in [`NOTICE`](NOTICE). |

## 2. Licence grant

### 2.1 Pre-release evaluation licence (the current phase)

Where the Licensor supplies You with a Pre-release build for testing, the Licensor grants You a
personal, non-exclusive, non-transferable, revocable licence to install and use that build **for
the sole purpose of evaluating it and reporting the results to the Licensor**, for as long as the
Licensor permits. This grant conveys no other right of any kind.

Pre-release builds are, by definition, incomplete and unvalidated. Anamorph's own release rules
state that a build passing every automated gate is *"ready to audition," not final*
([`docs/policies/RELEASE_POLICY.md`](docs/policies/RELEASE_POLICY.md) precondition 7) — do not use
a Pre-release build for production work you cannot afford to lose.

Testing conduct and reporting are covered by [`SUPPORT.md`](SUPPORT.md).

### 2.2 Commercial end-user licence

*`[OWNER/LEGAL DECISION]` — not drafted. The commercial licensing model (per-seat, per-machine,
per-user, subscription or perpetual), the number of permitted activations, whether a licence is
transferable, and how it is delivered and validated are all undecided. No commercial licence is
offered by this repository.*

## 3. Restrictions

Except to the extent that mandatory law expressly permits otherwise (see §3.1), You may not:

1. **Redistribute.** Copy, publish, upload, sell, resell, rent, lease, lend, sublicense,
   time-share, bundle, or otherwise make the Software available to any third party.
2. **Reverse engineer.** Reverse engineer, decompile or disassemble the Software, or otherwise
   attempt to derive its source code, algorithms or internal structure from the binaries.
3. **Claim source rights.** Anamorph is closed source. Supplying, using or testing the Software
   grants **no right whatsoever** to its source code — not to read it, receive it, request it,
   or have it escrowed. Nothing in this repository, including the technical documentation under
   `docs/`, constitutes a source-code licence.
4. **Modify or create derivative works**, including patching, hooking or repackaging the shipped
   binaries.
5. **Remove or obscure notices** — copyright lines, the About screen's version/build information,
   the third-party attribution in [`NOTICE`](NOTICE) and
   [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md), or this document.
6. **Circumvent** any licensing, activation or integrity mechanism the Software may contain.
   *(Anamorph currently contains none: no licensing, activation or entitlement code exists
   anywhere in `src/`. The protection posture is proposed but not yet decided —
   [`docs/architecture/RELEASE_HARDENING_PLAN.md`](docs/architecture/RELEASE_HARDENING_PLAN.md)
   §5 (design only) and §8, which lists ADR-0018 as an ADR still to be written.)*

### 3.1 Rights that survive these restrictions

Some jurisdictions grant users non-excludable rights — for example a statutory right to decompile
for interoperability purposes. This document does not purport to remove any right that cannot
lawfully be removed.

*`[OWNER/LEGAL DECISION]` — how the reverse-engineering restriction (§3, restriction 2) is worded
for the EU, the UK and other markets with mandatory interoperability provisions needs counsel's
review before this EULA ships.*

## 4. Ownership

The Software is **licensed, not sold**. All right, title and interest in the Software, including
all intellectual property rights in it, remain with the Licensor and its suppliers. Rights not
expressly granted are reserved.

Product and third-party name usage is addressed separately in [`TRADEMARKS.md`](TRADEMARKS.md).

## 5. Third-Party Components

The Software incorporates Third-Party Components. Those components are governed by **their own
licences**, which are reproduced or cited in [`NOTICE`](NOTICE) and inventoried in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md). Where a Third-Party Component's licence
grants You rights broader than this document, that licence governs **for that component**.

Nothing in §3 is intended to restrict a right You hold under a Third-Party Component's own licence.

## 6. Feedback

*`[OWNER/LEGAL DECISION]` — undecided. Whether bug reports, suggestions and test results submitted
by testers may be used by the Licensor without restriction or compensation, and on what terms, has
not been settled. Testers should assume nothing beyond what [`SUPPORT.md`](SUPPORT.md) states about
the reporting channel.*

## 7. Data

Anamorph collects no personal data, transmits nothing, and makes no network connections of its own.
The verified detail — what is written to disk, what leaves the machine, and what does not — is in
[`PRIVACY.md`](PRIVACY.md).

## 8. No warranty

*`[OWNER/LEGAL DECISION]` — the binding wording, and its interaction with non-excludable consumer
guarantees in each market of sale, requires counsel. The following states the project's factual
position and the disclaimer it intends to give.*

The Software is provided **"AS IS" and "AS AVAILABLE", without warranty of any kind**, express or
implied, including any implied warranty of merchantability, fitness for a particular purpose,
non-infringement, or uninterrupted or error-free operation. The Licensor does not warrant that the
Software will meet Your requirements or work with any particular host application, operating system
or hardware.

Factually, on the current build:

- The host and format coverage that has actually been verified is recorded in
  [`docs/architecture/COMPATIBILITY_MATRIX.md`](docs/architecture/COMPATIBILITY_MATRIX.md); a real
  digital-audio-workstation host matrix has **not** been completed.
- Known defects and limitations are listed openly in
  [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md).
- macOS builds are ad-hoc signed and **not notarized**; Windows installers are **not**
  Authenticode-signed (`KI-002`, `RH-PR-3`/`RH-PR-5`).

Some jurisdictions do not allow the exclusion of implied warranties; in those jurisdictions the
exclusion applies only to the extent permitted.

## 9. Limitation of liability

*`[OWNER/LEGAL DECISION]` — the liability cap, the carve-outs required by law (death or personal
injury, fraud, gross negligence), and the wording for each market of sale require counsel. No
figure is asserted here.*

The Licensor's intent is to exclude liability for indirect, incidental, special, consequential and
punitive damages, and for lost profits, lost revenue, lost data, lost recordings or lost studio
time, arising out of or relating to the Software — and to cap direct liability at the amount You
paid for the Software. **The cap and the carve-outs are undecided.**

## 10. Term and termination

This licence takes effect when You install or use the Software and continues until terminated.
It terminates automatically if You breach any restriction in §3. For Pre-release builds it also
terminates when the Licensor ends the testing programme or withdraws Your build, whichever is
earlier.

On termination You must stop using the Software and remove all copies from Your systems
(the uninstall steps for each platform are in
[`docs/user/INSTALLATION.md`](docs/user/INSTALLATION.md)).

## 11. Governing law and jurisdiction

*`[OWNER/LEGAL DECISION]` — undecided. No governing law, venue or dispute-resolution mechanism has
been chosen, and none is asserted here.*

## 12. Contact

*`[OWNER/LEGAL DECISION]` — no legal or licensing contact address is recorded in this repository.
The only contact detail present anywhere in the product is the About screen's link to
`https://www.rolly.tech` (`src/PluginEditor.h:456`).*

---

## Open decisions blocking approval of this EULA

| # | Decision | Section | Tracked as |
|---|---|---|---|
| 1 | Legal entity, form and address of the Licensor | §1 | `RH-F1` |
| 2 | Commercial licensing model, activation and transferability | §2.2 | `RH-F1` |
| 3 | Reverse-engineering wording vs. mandatory interoperability rights | §3.1 | `RH-F1` |
| 4 | Ownership of tester feedback | §6 | — |
| 5 | Warranty disclaimer wording per market | §8 | `RH-F1` |
| 6 | Liability cap and non-excludable carve-outs | §9 | `RH-F1` |
| 7 | Governing law, venue, dispute resolution | §11 | `RH-F1` |
| 8 | Legal/licensing contact address | §12 | `RH-F1` |
| 9 | Commercial **JUCE 9** licence — the closed-source model rules out the AGPLv3 arm of JUCE's dual licence, so the commercial tier must be obtained before commercial distribution | prerequisite | `RH-R11` / `RH-F1`, `KI-015` |
| 10 | Steinberg **VST 3** trademark and plug-in distribution review | prerequisite | `RH-R10` / `RH-F2` |

Decisions 9 and 10 are prerequisites for distributing the Software commercially at all — they are
not clauses of this document. See
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) §"Open licensing decisions".
