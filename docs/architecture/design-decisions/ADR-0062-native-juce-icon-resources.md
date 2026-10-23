# ADR-0062 — Conventional JUCE product icon resources

**Status:** Accepted — the repository owner explicitly approved Architecture Review on
2026-10-23: **“Approve architecture review for the proposed Round 1 change.”** The reviewed
proposal was the two-line ICON_BIG/ICON_SMALL addition and the two unchanged B0 PNGs whose
digests are recorded in `resources/icons/PROVENANCE.json`. Approval permits implementation,
validation and an unmerged PR; it does not approve a merge, release or subsequent integration.

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
