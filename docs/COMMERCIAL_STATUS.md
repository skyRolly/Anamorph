# COMMERCIAL_STATUS.md

**Internal record — developer documentation, not a user-facing or legal document.**

The single place that states what Anamorph *is* commercially, what phase it is in, and which
owner/legal decisions are still open. It **indexes** the authoritative records rather than
restating them; where this file and the record it cites disagree, the cited record wins
(`SOURCE_OF_TRUTH.md`).

Last reviewed: **2026-07-26** (substance unchanged since). The release then in preparation was
v0.9.0; none of v0.9.0 through v0.9.5 was tagged, and the release in preparation is now
**v0.9.6** (the engineering-review fixes on top of the 0.9.5 A7 performance programme). Nothing in this document — the product model, the
distribution model, or the open owner/legal decisions — is affected by that renumbering, so the
review date stands and moves only when the substance does.

---

## 1. Product model

| | |
|---|---|
| **Model** | **Closed-source commercial.** Owner statement, 2026-07-26. |
| **Source availability to users** | None. No user, customer or tester receives, or has any right to, Anamorph's source code (`EULA.md` §3, restriction 3). |
| **Own licence terms** | **None declared.** There is no `LICENSE` file at the repository root and no EULA is presented by any installer. Absent a grant, all rights are reserved by default. Tracked as **KI-015** / **RISK-006** / **RH-R11**. |
| **Draft terms that exist** | [`EULA.md`](../EULA.md) — an unapproved draft with every open decision marked; not in force, not shipped. |
| **Price / sale channel** | Not decided, not implemented. No purchase, activation, licence-key or entitlement mechanism exists anywhere in the code; the protection posture is an unwritten decision (`RELEASE_HARDENING_PLAN.md` §8, ADR-0018 required, not yet written). |

## 2. Current phase — internal testing

The current pre-1.0 build (**v0.9.6**; see `docs/HANDOVER.md`) is being prepared for
**internal / beta testing**, not for sale.

- Builds reach testers as per-push CI artifacts today; the **GitHub Release** route is implemented
  but no tag has been cut yet (`RISK-003`) — `docs/procedures/PACKAGING.md`. Publishing a release
  is a manual maintainer action after the Level-5 audition (`docs/policies/RELEASE_POLICY.md`
  precondition 7).
- **No licence is in force** (`EULA.md` §0, and §1 above). Testers hold builds by the owner's
  direct permission, for evaluation and reporting only; `EULA.md` §2.1 is the **draft** wording
  intended to cover that phase, not its source. The tester-facing statement is
  [`SUPPORT.md`](../SUPPORT.md) §1.
- Feedback runs through the project's testing channel, defined in `SUPPORT.md`.
- **No commercial distribution may occur** until the items in §4 are closed.

## 3. Distribution model

| Channel | What ships | Status |
|---|---|---|
| **GitHub Releases** (draft → manually published) | flat permission-preserving zips per platform, Windows Inno Setup installer, macOS `.pkg`, user manual, `NOTICE`, `THIRD_PARTY_LICENSES.md`, `SUPPORT.md`, `SHA256SUMS.txt`, `RELEASE_MANIFEST.txt` | implemented; first tag not yet cut (`RISK-003`) |
| **Per-push CI artifacts** | loose-file payloads for testers; the same trees are archived into the release zips | implemented |
| **Commercial storefront** | — | does not exist; not designed |

Packages themselves are deliberately **lean** — payload + `INSTALL.txt` only, and since
2026-07-26 `INSTALL.txt` carries installation instructions plus a copyright line and nothing
else. Legal and support documents accompany a download as release-page assets, which are
therefore the sole carrier of the mandatory IJG acknowledgement
(`RELEASE_POLICY.md` §"Third-party attribution"). The product model is stated **once for a general audience** — `README.md`
§Licensing — and is otherwise kept only where it is operative: §1 above, the legal class
(`NOTICE`, `EULA.md`, `PRIVACY.md`, `TRADEMARKS.md`, `THIRD_PARTY_LICENSES.md`), the
internal/testing class (`SUPPORT.md` §1 and the bug-report form) and the developer documents where
it drives the JUCE-tier consequence (`KI-015`, `RISK-006`, `RH-R11`). User-facing documents are
deliberately **not** repeat-labelled (owner instruction, 2026-07-26).

**Which documents are published as release assets — a deliberate split.** `EULA.md` is **not**
published, because it is an unapproved draft: shipping it would present it as terms in force,
which `EULA.md` §0 explicitly denies. `TRADEMARKS.md` and this file are internal-facing and are
not published either. `PRIVACY.md` is currently repo-only as well; the repository is public
(§4 item 5), so testers reach `EULA.md`, `PRIVACY.md` and this file by the absolute GitHub URLs
`SUPPORT.md` links, while `TRADEMARKS.md` is reachable only from `README.md` and `EULA.md` §4.
None of them travels with an offline download. *Adding `PRIVACY.md` to the release assets is a
two-line `release.yml` change (a staging `cp` plus an asset argument to `gh release create`) and a
reasonable next step — it is a packaging-behaviour change and therefore an owner call, not one
this documentation pass makes.*

**Signing status:** macOS bundles are ad-hoc signed and **not notarized**; Windows installers are
**not** Authenticode-signed (`KI-002`, `RH-PR-3`/`RH-PR-5`). Both are user-visible on first launch
and are documented in `docs/user/INSTALLATION.md`.

## 4. Open owner / legal decisions

None of these is an engineering task; no code change can close any of them.

| # | Decision | Blocks | Authoritative record |
|---|---|---|---|
| 1 | **Commercial JUCE 9 licence.** JUCE modules are dual-licensed AGPLv3 *or* commercial. A closed-source distribution cannot satisfy the AGPLv3 arm, so the commercial tier must be in place before commercial distribution. Which tier, and its purchase, are unrecorded. | commercial release | `THIRD_PARTY_LICENSES.md` §"Open licensing decisions" #1; `KI-015`; `RISK-006`; `RH-R11`/`RH-F1` |
| 2 | **Anamorph's own `LICENSE`.** The repository declares no terms for its own source or binaries. | commercial release | `THIRD_PARTY_LICENSES.md` §"Open licensing decisions" #2; `KI-015`; `RISK-006`; `RH-R11`/`RH-F1` |
| 3 | **EULA** for the distributed binaries. A draft exists ([`EULA.md`](../EULA.md)) with 10 marked open decisions; no installer presents it. | commercial sale | `THIRD_PARTY_LICENSES.md` §"Open licensing decisions" #3; `EULA.md` §"Open decisions" |
| 4 | **Steinberg VST 3 review.** The SDK code bundled with JUCE 9.0.1 is MIT, but the VST name/logo and the plug-in development/distribution terms are governed separately. | commercial VST3 distribution | `THIRD_PARTY_LICENSES.md` §3; `RH-R10`/`RH-F2`; `TRADEMARKS.md` §4 |
| 5 | **Repository visibility.** The GitHub repository `skyRolly/Anamorph` is **public**, with forking enabled and no `LICENSE` file, while the product model is closed-source commercial. Whether the source stays publicly readable is an owner decision with a direct bearing on decisions 1–3. Stated as a fact; no determination is made here. | should be settled alongside 1–3 | *this document* |
| 6 | **Trademark status** of "Anamorph" and "RollyTech"; the `Dim-D` / "Roland Dimension-D-style" naming reference. | any ™/® use; commercial release | `TRADEMARKS.md` §1, §4 |
| 7 | **Privacy/controller identity** and any statutory disclosure required in a market of sale. Anamorph collects nothing, so the factual position is simple; the formal disclosure is not written. | commercial sale in regulated markets | `PRIVACY.md` §7 |
| 8 | **Ownership of tester feedback.** | tester programme | `EULA.md` §6 |

## 5. What engineering has already discharged

So that these are not re-opened as if outstanding:

- **Third-party attribution.** `NOTICE` + `THIRD_PARTY_LICENSES.md` inventory every component
  compiled into the binaries, verified against the pinned JUCE tree, and discharge the mandatory
  binary-distribution notices (IJG libjpeg, FLAC, Ogg Vorbis, HarfBuzz, SheenBidi). Published with
  every release, and are the sole carrier of the IJG line since `INSTALL.txt` became
  installation-only. (`RH-R10` engineering half — closed.)
- **Build/supply-chain hardening.** `ADR-0021`; JUCE pinned to an immutable commit (`ADR-0022`).
- **Release pipeline.** Tag-triggered, fail-closed, draft-only (`RH-PR-8`).
- **Installers** for Windows and macOS, plus Linux install scripts (`RH-PR-5b`/`RH-PR-6`) — built
  in CI from the same validated staging directories as the zips.
- **User and tester documentation.** `docs/user/` (manual + installation guide), `SUPPORT.md`.
- **Legal/product documents.** `EULA.md` (draft), `PRIVACY.md`, `TRADEMARKS.md`, this record.

## 6. Preconditions for a commercial release

Both lists must be empty:

**Owner/legal** — §4 items 1, 2, 3, 4 (and 6, 7 for the markets concerned).

**Engineering / process** — **one** `RELEASE_POLICY.md` precondition is open for v0.9.6, tracked in
`docs/HANDOVER.md` §Release Status: `RELEASE_COMPATIBILITY_CHECKLIST.md` has never been completed
for this release. The **Level-5 manual audition is CLOSED**: the 2026-08-15 audition covered the
then-shipping v0.9.4 / JUCE 9.0.1 build and did not carry over (the machine code changed on every
x86-64 platform under ADR-0031/0032, and the engine gained the 0.9.6 fixes — ER-DOC-01), so it was
re-run, and **the maintainer performed it against the final v0.9.6 build and it PASSED** (recorded
2026-09-01; `docs/procedures/LEVEL5_AUDITION.md` §Recorded auditions, where the fields the report
did not supply are marked NOT RECORDED rather than inferred). Every ADR remains `Accepted`. Signing and notarization
(`RH-PR-3`/`RH-PR-5`) are not release-blocking but are user-visible.

---

**Maintenance rule.** When any §4 decision is made, update the authoritative record first
(`KNOWN_ISSUES.md` / `FUTURE_RISKS.md` / `RELEASE_HARDENING_PLAN.md` / `THIRD_PARTY_LICENSES.md`),
then this index, then `DOCUMENTATION_COVERAGE.md`
(`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).
