# Anamorph — Trademark and name-usage notes

**Anamorph** · © 2026 RollyTech · Closed-source commercial audio software.

This is a factual record of the names used by and around Anamorph, and of the name-usage
obligations the project's own dependency licences impose. **No trademark claim, registration
status or legal determination is asserted here.** Nothing in this document is legal advice.

---

## 1. Anamorph and RollyTech

| Name | Where it appears | Status |
|---|---|---|
| **Anamorph** | product name — `CMakeLists.txt:14, 427`, the plug-in's About screen, every artifact and installer | *`[OWNER/LEGAL DECISION]`* — whether the name is registered, pending or unregistered in any jurisdiction is **not recorded anywhere in this repository**. Use no ™ or ® symbol until that is settled. |
| **RollyTech** | company name — `CMakeLists.txt:422` (`COMPANY_NAME`), the `NOTICE` copyright line, the preset directory path, the bundle identifier `com.rollytech.anamorph`, and since 0.9.1 the four-character manufacturer code **`RTec`** (`CMakeLists.txt:424`, ADR-0023) that every RollyTech plug-in shares | *`[OWNER/LEGAL DECISION]`* — same: no registration status is recorded. |
| **`www.rolly.tech`** | About-screen link — `src/PluginEditor.h:475` | Domain, not a mark. |

Until the owner records a registration status, documentation must describe these as *names*, and
must not print a ™ or ® symbol next to them — an unfounded registration symbol is itself a
misrepresentation risk.

### Use of the Anamorph name by others — not decided

The repository's recorded position is that no terms are declared and all rights are reserved by
default ([`docs/COMMERCIAL_STATUS.md`](docs/COMMERCIAL_STATUS.md) §1), so no right to use the
product or company name is granted with a copy of the software. Nothing in this repository
restricts referring to Anamorph by name in a review, a tutorial, a compatibility list or a bug
report, and no brand-usage policy exists (§4 item 4). Using the name or any Anamorph branding
**as your own**, on your own product, or in a way that suggests endorsement or origin, does not
follow from possessing a copy.

*`[OWNER/LEGAL DECISION]` — no position on third-party use of the Anamorph or RollyTech names is
recorded, and none is determined here.*

*`[OWNER/LEGAL DECISION]` — a formal brand-usage/press-kit policy (logo files, permitted variants,
minimum clear space, colour) does not exist. None is needed for the internal-testing phase.*

## 2. Third-party marks appearing in Anamorph and its documentation

All of the following belong to their respective owners. They appear here, in the user manual and
in the installation guide **descriptively** — to say which format Anamorph builds, which operating
system a step applies to, or which host a setting lives in.

| Mark | Owner | Why it appears | Where the mark is used in this repo |
|---|---|---|---|
| **VST**, **VST 3** | Steinberg Media Technologies GmbH | Anamorph builds a VST3 plug-in | `THIRD_PARTY_LICENSES.md` §3 |
| **Audio Units** / **AU**, **macOS**, **Logic Pro**, **GarageBand**, **Apple Silicon**, **Rosetta**, **Gatekeeper** | Apple Inc. | the macOS AU build, install steps and the notarization notes | `docs/user/INSTALLATION.md`, `docs/architecture/COMPATIBILITY_MATRIX.md` |
| **Windows**, **SmartScreen** | Microsoft Corporation | the Windows build and its unsigned-installer warning | `docs/user/INSTALLATION.md`, `packaging/windows/INSTALL.txt` |
| **JUCE** | Raw Material Software Limited | the framework Anamorph is built with | `THIRD_PARTY_LICENSES.md` §1 |
| **AAX**, **Pro Tools** | Avid Technology, Inc. | AAX is named to state it is **Not Supported**; Pro Tools is named in the list of hosts with no in-repo test evidence | `docs/policies/COMPATIBILITY_POLICY.md`; `docs/architecture/COMPATIBILITY_MATRIX.md:14,40` |
| **ASIO** | Steinberg Media Technologies GmbH | named only to state it is not enabled | `THIRD_PARTY_LICENSES.md` §4 |
| **REAPER**, **Ableton Live**, **Cubase**, **Nuendo**, **FL Studio**, **Bitwig Studio**, **Ardour**, **Qtractor** | Cockos Inc.; Ableton AG; Steinberg Media Technologies GmbH; Image-Line; Bitwig GmbH; the Ardour project; the Qtractor project | host-specific rescan and troubleshooting instructions | `docs/user/USER_MANUAL.md:96-102`, `docs/user/INSTALLATION.md`, `packaging/linux/INSTALL.txt:36` (Qtractor) |
| **Linux** | Linus Torvalds | the Linux build target | throughout |
| **OpenGL** | Khronos Group | the GPU compositing path on macOS/Windows | `THIRD_PARTY_LICENSES.md` §2 |
| **Inno Setup**, **pluginval** | jrsoftware; Tracktion Corporation | build/CI tooling, never redistributed | `THIRD_PARTY_LICENSES.md` §6 |

Owner names are the commonly stated proprietors of each mark. Only Steinberg Media Technologies
GmbH (`NOTICE:161`), the Khronos Group (`NOTICE:322`) and Raw Material Software Limited
(`NOTICE:23`) are independently recorded elsewhere in this repository; the rest are stated for
identification only. *`[OWNER/LEGAL DECISION]` — verify each proprietor before any commercial use
of these names.*

## 3. Name-usage obligations that the dependency licences actually impose

These are **binding conditions of licences Anamorph relies on**, not courtesy. They are already
reproduced in [`NOTICE`](NOTICE) and inventoried in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md); they are restated here because each is a
*naming* restriction rather than an attribution one.

1. **Independent JPEG Group (libjpeg)** — the IJG licence forbids using an IJG author's or
   company name in advertising or publicity relating to the software without prior written
   permission. Anamorph does not. The separate mandatory acknowledgement — *"this software is
   based in part on the work of the Independent JPEG Group"* — is carried in `NOTICE`,
   published as a release asset next to every download.
2. **Xiph.Org Foundation (FLAC, Ogg Vorbis)** — the BSD 3-clause third clause forbids using the
   Foundation's name, or its contributors' names, to endorse or promote Anamorph without prior
   written permission. Anamorph does not.
3. **zlib** — condition 1 forbids misrepresenting the origin of the software; Anamorph does not
   claim authorship of any third-party component.

## 4. Open review items

| # | Item | Why it is here | Tracked as |
|---|---|---|---|
| 1 | **Steinberg VST 3** — the SDK *code* bundled with JUCE 9.0.1 is MIT, but the **VST name and logo**, and the terms for developing and distributing VST 3 plug-ins, are governed separately (the SDK ships `VST3_Usage_Guidelines.pdf`; its README refers to a *Steinberg VST 3 Plug-In SDK Licensing Agreement*). **Commercial VST3 distribution requires reviewing Steinberg's requirements separately.** No determination is made in this repository. | blocks commercial sale | `RH-R10` / `RH-F2` |
| 2 | **"Dim-D" / "Dimension-D"** — the fourth widening algorithm is presented to the user as **`Dim-D`** (`src/PluginParameters.cpp:212`), and the user manual describes it as *"Roland Dimension-D-style widening"* (`docs/user/USER_MANUAL.md:287`), referencing a hardware product associated with Roland Corporation. Describing an emulation by reference to the hardware it emulates is common practice in this industry, but whether the wording is acceptable for a **commercial** release is a naming question this repository cannot answer. Flagged, not decided. | review before commercial release | *this document* |
| 3 | **Anamorph / RollyTech registration status** — unknown; see §1. | needed before any ™/® use | `RH-F1` |
| 4 | **Brand-usage policy for third parties** — does not exist; not needed for internal testing. | post-release | — |

Items 1 and 3 are the same owner/legal work stream as the missing `LICENSE`/EULA and the
commercial JUCE licence — see [`docs/COMMERCIAL_STATUS.md`](docs/COMMERCIAL_STATUS.md) for the
consolidated list.

---

All other product names, company names and marks mentioned in Anamorph's documentation are the
property of their respective owners and are used for identification purposes only.
