# ADR-0062 — Conventional JUCE product icon resources

**Status:** Accepted — the repository owner explicitly approved Architecture Review on
2026-10-23: **“Approve architecture review for the proposed Round 1 change.”** The reviewed
proposal was the two-line ICON_BIG/ICON_SMALL addition and the two unchanged B0 PNGs whose
digests are recorded in `resources/icons/PROVENANCE.json`. That original approval permitted
implementation, validation and an unmerged PR. The owner's later final approval confirms the
completed #170 scope, now merged as `b4a95b3a6d86acf222de523c6fb04de23ad8e602`;
ADR-0062 remains Accepted. See [ADR_INDEX.md](ADR_INDEX.md#owner-approval-reconciliation--record-updated-2026-10-24)
for the final-approval provenance (original date not recorded; record updated 2026-10-24).

**Current scope pointer:** Accepted ADR-0063 separately governs the namespace, and Accepted
ADR-0064 completes the Windows installer follow-up originally deferred here. This Round 1
record continues to describe the icon-only change; its no-identity-change claim is historical
to #170. Linux/Composer work and native visual certification remain separate.

## Context

Anamorph main `098884e` has no configured product icon. Candidate B0 was accepted in
`skyRolly/icon-vector-reconstruction` PR #10, merge `30349f3`. Current approved source main
`35cf982` includes a later Pillow dependency/provenance update; the two imported PNGs and the
canonical SVG remain byte-identical to that accepted merge.

## Problem

Supply branded build-time resources through the pinned JUCE 9.0.4 conventional icon path,
without changing plug-in identity, runtime image loading, audio/state behavior or packaging policy.
This is a Build System change under `ARCHITECTURE_REVIEW_GATE.md`; implementation was paused
until the explicit owner review above, rather than inferring approval from the task or a build.

## Options

- Keep generic resources: does not implement the approved product icon.
- **Vendor the two verified PNGs and use JUCE ICON_BIG/ICON_SMALL: chosen.** No network or
  adjacent source checkout is needed by an ordinary build.
- Use custom generators, patched JUCE internals or Apple Icon Composer: unnecessary for this
  first round and outside its approved scope. Consider a supported fallback only if measured
  native output proves inadequate.

## Decision

- `resources/icons/anamorph-1024.png` is ICON_BIG; `anamorph-32.png` is ICON_SMALL. The paths
  are anchored to CMAKE_CURRENT_SOURCE_DIR inside `juce_add_plugin(Anamorph)`.
- Preserve B0 exactly, including the optical source for 32 actual pixels and below. No new art,
  saturation/glow/geometry policy, JUCE pin, compiler, C++ standard, format or architecture change.
- Keep JUCE's normal generated ICNS/ICO/RC and bundle handling, before existing signing.
  Record actual frames rather than requiring equality with the independent icon encoders:
  Windows requests 16/32/48/256; macOS writes the supplied 32/1024 images. Missing display
  sizes are rendered by the OS and require native visual inspection.
- Validate source hashes/CMake bindings, actual native bundles/PE resources and Linux outputs
  in the existing workflow. Each new checker invocation is preceded by its live self-test.
- Defer Windows installer branding/uninstall icon, Linux desktop/XDG installation and genuine
  Icon Composer authoring. Host-controlled DAW lists remain outside a portable icon guarantee.

## Consequences

The source repository contains reproducible approved inputs. macOS app/VST3/AU bundles and
Windows executable/VST3 resources can carry the icon; Linux's conventional JUCE path adds no
portable ELF icon. Resampling and sparse ICNS contents require evidence, not an assumption that
source quality proves native display quality. Finder/Dock, Explorer/taskbar/DPI and real desktop
sessions remain manual tests. Architecture approval is not native certification or shipping approval.

## Related code

`CMakeLists.txt`, `resources/icons/PROVENANCE.json`, `scripts/check-icons.py`,
`.github/workflows/build.yml`. The complete provenance, validation commands, limitations,
deferred work and rollback are in [the integration record](../../../worklogs/NATIVE_ICON_INTEGRATION_ROUND1.md).

## Evidence and confidence

**Verified:** current main configuration, copied-input hashes, unchanged B0 bytes at source
`35cf982`, and pinned JUCE `3fb5cbc0340758596dac39f7c8de47d562b4896e`:
`extras/Build/CMake/JUCEUtils.cmake` (`_juce_generate_icon`, `_juce_add_resources_rc`,
`_juce_create_windows_package`), `juce_build_tools/utils/juce_Icons.cpp` and
`juce_PlistOptions.cpp`. Native build/check results are scoped to the exact CI run that
produces their reports. **Unverified:** real Finder/Dock, Explorer/taskbar/DPI and DAW icon use.

## macOS fallback amendment — 2026-10-29

**Status: Accepted for this narrow implementation and validation.** On 2026-10-29
(Asia/Shanghai), the owner requested execution of `PROMPT.md` from
`anamorph-macos-icon-handoff.zip`. That prompt explicitly authorizes the measured macOS
packaging/CMake fallback, its regressions and this limited ADR amendment, while forbidding
merge/release publication and changes to production signing credentials. Prompt SHA-256:
`661c4a0318295dae334cf809d7bde53ecdcdde40b9a2ac9fe30bc80931b76fcb`.
This is new approval for the fallback; the original Round 1 approval above is unchanged.

**Context/problem:** live baseline `06105461d79cca68389dd8470c316b84bb56252a` retains JUCE's
two-entry ICNS. The installed 0.9.10 app (`tech.rolly.anamorph`) has the handoff's exact bad
container SHA-256 `88b9839f2ed351e9e958963476338b1fe474f87a10d5a0aafbc72a3f89bd582d`.
Native `NSWorkspace.icon(forFile:)` application rendering on macOS 15.8.1 x86_64 reproduces
colored noise and opaque backgrounds at affected sizes, including in disposable app bundles
with separate identities. Standalone PNG decoding had concealed this defect. This establishes
format behavior without relying on an installed application's cached state; Launchpad itself
remains a separate observation.

**Options/decision:** take the supported native fallback anticipated in Options above.
Compile all ten standard `.iconset` filenames with `/usr/bin/iconutil`, resampling with
`/usr/bin/sips`. Actual pixels <=32 use the dedicated optical input; >=64 use the large PNG.
Both approved input files remain byte-identical. The measured native small entries are
`ic04`/`ic05` ARGB; higher/retina entries use suitable PNG slots. PNG in `icp4`/`icp5`/`icp6`
is refused. A prebuilt external ICNS, JUCE patch/upgrade, asset catalog or Icon Composer
authoring adds unnecessary scope for this measured repair.

Run the generator after `juce_add_plugin` has completed all of JUCE's configure-time icon
and plist work, using its `JUCE_ICON_FILE` path. Regenerate on every configure and add an
explicit custom-command dependency on both inputs and the generator to all macOS formats.
Resource copying and the existing signing order follow generation. Windows retains its JUCE
ICO path and Linux its existing resources. The 10.13 deployment target, JUCE pin, compiler
flags, formats, identities and all runtime/audio/state contracts remain unchanged.

**Consequences/evidence:** `check-icons.py` now decodes bounded ARGB/RGB RLE, validates paired
masks, metadata, duplicate/bounds/encoding rules and full coverage, and retains bundle/plist
equality checks. `render-macos-icon.swift` draws application icons into AppKit bitmap contexts
at 16–512 points at 1x/2x. `test-macos-icons.py` accepts actual iconutil output and rejects the
old structural layout even though its PNG decodes, while recording the native negative control.
Measured corrected fixtures have <=1.05% premultiplied RGB mean absolute error, zero alpha
error and full silhouette overlap; the checker budgets 4% RGB, 3% alpha and >=95% overlap
for native resampling/color management. Transparency and opacity floors also reject opaque
noise. Neither these measurements nor this approval certify Launchpad, untested macOS
versions, Windows/Linux execution or release readiness.

**Related code/confidence:** `cmake/MacIcon.cmake`, `cmake/GenerateMacIcon.cmake`,
`scripts/check-icons.py`, `scripts/render-macos-icon.swift`, `scripts/test-macos-icons.py`,
`.github/workflows/build.yml`. Source and actual native fixture rendering are Verified;
fresh built/staged outcomes are recorded in the dated integration worklog below its original
Round 1 record. Independent format corroboration: [Chromium's icon conventions](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/mac/icons.md),
[Apple's iconset instructions](https://developer.apple.com/library/archive/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Optimizing/Optimizing.html).
