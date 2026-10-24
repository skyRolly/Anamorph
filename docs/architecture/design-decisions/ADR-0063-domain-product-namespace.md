# ADR-0063 — Domain product namespace for pre-release packages

**Status:** Accepted — explicit final owner approval of ADR-0063 and the completed
namespace implementation. **Architecture Review Gate: CLEARED; owner decision: APPROVED.**
Approval provenance and verified merge ancestry are recorded in [ADR_INDEX.md](ADR_INDEX.md#owner-approval-reconciliation--record-updated-2026-10-24).
The original final-approval date is not recorded; this status update is dated 2026-10-24
(Asia/Shanghai). The earlier 2026-10-23 permission, **“Approve B, C, and E for
implementation and testing,”** is retained as implementation-stage history.

## Context

The owner controls `rolly.tech` and directs all future products to `tech.rolly.<product>`.
The historical starting tree `098884e7fe12049010f2eaf8379e1514efdad0ab` used
`com.rollytech.anamorph`. ADR-0023 recorded that bundle/package IDs were unchanged by the manufacturer decision.
This accepted decision amends only those namespace statements; its RTec decision and historical evidence
remain. ADR-0062 still describes the earlier icon-only change, which changed no identity.
The product is pre-release; the owner explicitly accepts the development package-identity break.

## Problem

Current application/package identifiers must follow the owner's domain without accidentally
changing independent host plugin IDs or Windows uninstall registration. The old ADR-0023 description of name-derived VST3 UIDs is also qualified against this pinned
configuration: VST2 replacement is disabled and the current UID path uses manufacturer/plugin
codes. Namespace and independent host identities must be reviewed separately.

## Options

- Keep the previous namespace: contradicts the domain decision.
- **Use `tech.rolly.anamorph` and unique package suffixes: accepted.**
- Add legacy aliases/receipt support: explicitly rejected for unpublished development builds.
- Change manufacturer/plugin codes, product name or Windows AppId: unnecessary independent
  identity breaks and outside the authorized namespace change.
- Add format suffixes to every CFBundleIdentifier: rejected; JUCE uses the base for these formats.

## Decision

- CMake BUNDLE_ID is `tech.rolly.anamorph`. With pinned JUCE 9.0.4, ordinary Standalone, VST3
  and AU use this same CFBundleIdentifier. Native generated plists are the verification gate.
- pkgbuild receipt IDs and productbuild references are `tech.rolly.anamorph.vst3`,
  `tech.rolly.anamorph.au`, `tech.rolly.anamorph.app`. The uninstalled packaging canary uses
  `.probe`. Receipt IDs are distinct from CFBundleIdentifier and from each other.
- Preserve `RollyTech`, `Anamorph`, manufacturer `RTec`, plugin code `Anmr`, and Windows
  AppId `{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}`. Preserve the version, formats, source/DSP,
  UI/DSP behavior, parameter IDs, state, serialization and latency.
- No old-identifier aliases, fallback paths, receipt handling or package migration machinery.
- Generated unused AAX/ARA/AU-framework defaults inherit the base too; this enables no new format.
- The partial namespace amendment to ADR-0023 is effective. ADR-0023 remains Accepted
  for its other decisions and historical evidence; ADR-0062 retains its original icon-only scope.

## Independent identities and consequences

Pinned `JUCEUtils.cmake` emits `JucePlugin_CFBundleIdentifier` from BUNDLE_ID but emits AU
manufacturer/subtype from the independent codes. `juce_PlistOptions.cpp` writes those separate
fields. `VST3/juce_VST3ModuleInfo.h` uses `VST3Interface::jucePluginId(manufacturer, plugin,
interfaceType)` because this product sets `JUCE_VST3_CAN_REPLACE_VST2=0`; the product name is
not an input on this path. The old general statement about name-derived UIDs is not the behavior
of this pinned configuration. BUNDLE_ID is not an input either.

The measured pre-change Linux component CID is `ABCDEF019182FAEB52546563416E6D72`, controller
`ABCDEF011234ABCD52546563416E6D72`. AU remains `aufx / Anmr / RTec`. Native CI checks both.
Windows VERSIONINFO product/vendor and VST3 metadata are checked without executing installer
payloads. Windows AppId is a separate unchanged GUID, not a reverse-DNS string.

macOS registration/cache or host session lookup can still react to CFBundleIdentifier. Actual
DAW recall/rescan is a human test; stable independent IDs alone are not a host compatibility
certification. New receipts intentionally do not replace old development receipt identities.
No old receipt is forgotten or deleted by these scripts. There is no state-format break or
state compatibility exemption. Linux ELF has no CFBundleIdentifier; the base appears in JUCE
build metadata, and no Linux desktop identifier is installed by the existing packaging.

## Merged implementation and original review order

PR #170 merged as `b4a95b3a6d86acf222de523c6fb04de23ad8e602`, followed by namespace
PR #171 as `b4ea7a2ac25b89b943d350a6bb0d42ccc6273727`, then installer PR #172 as
`1e17f5845a90919949435c60d6ddf2bf2bd3fde1` (inspected main). The original stack kept
the three review units separate and was reconciled by ordinary merges. Final owner
approval closes this decision's architecture/acceptance blockers. It does not certify
unperformed DAW, receipt-installation or shell/cache tests, or authorize future architecture.

## Related code

`CMakeLists.txt`, `packaging/macos/build-pkg.sh`, `packaging/windows/Anamorph.iss`,
`scripts/check-product-identity.py`, `.github/workflows/build.yml`.

## Evidence and confidence

Source/pinned-framework relationships and starting identity inventory are **Verified**.
See [the worklog](../../../worklogs/2026-10-23-product-namespace.md) for the independently
collected inventory, exact old/new outputs, commands and limitations. Generated/native claims
are scoped to the actual checks and CI head that produced them; synthetic self-tests do not
prove native builds. Native DAW/installation review remains **Unverified** until performed.

## Linux follow-up scope — 2026-10-24

The earlier no-installed-launcher statement describes the namespace-only migration.
Proposed ADR-0065 now records the separately owner-authorized Linux implementation:
`tech.rolly.anamorph` desktop/hicolor names, unchanged host identities and no ELF
embedded-icon claim. ADR-0063's namespace decision remains Accepted; ADR-0065's
formal acceptance/merge review is pending and is not inferred from this record.

2026-10-25 current scope reconciliation: the owner directly confirmed ADR-0065
Accepted, including its delivered helper snapshot amendment. This does not change
ADR-0063's already Accepted status or certify native/runtime behavior.
