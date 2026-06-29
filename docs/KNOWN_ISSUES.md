# KNOWN_ISSUES.md

**Only currently-existing, confirmed problems/limitations.** Resolved or historical issues belong
in `POSTMORTEMS.md`, not here. Each entry is evidence-backed (constraint C7). When an item is
fixed, remove it here and (if notable) add a `POSTMORTEMS.md` entry.

Verified against repository HEAD `41acaa7` (version 0.8.7; JUCE 8.0.14).

| ID | Issue | Severity | Status |
|---|---|---|---|
| KI-001 | Concurrent Multiband-Enable + other discrete change cold-starts the crossover bank | Low | Confirmed, masked (inaudible) |
| KI-002 | macOS artifacts not notarized (manual de-quarantine required) | Medium | Confirmed (distribution) |
| KI-003 | pluginval Linux editor tests crash (external host-side JUCE) | Low | Confirmed, mitigated/external |
| KI-004 | No automated DAW/host-compatibility testing | Medium | Confirmed (coverage gap) |
| KI-005 | No graphical installer (manual copy install) | Low | Confirmed (packaging) |
| KI-006 | Linux: tooltip rounded corners render an opaque black background instead of transparent | Low | Confirmed (Linux); UI/platform rendering only |

---

## KI-001 — Concurrent Multiband-Enable + discrete change cold-starts the bank during the duck
When `mbEnable` flips **simultaneously with another discrete change** (e.g. algorithm), the change
is deferred to the silent duck bottom, where `mbStructuralChange` (which still includes
`mbEnable`) resets the multiband bank and SoloMonitor — so the crossover bank cold-starts during
the fade-in instead of staying warm, partially defeating the 0.8.6 warm-bank design for that
specific case. The reset is **masked by the duck (inaudible)**, so there is no user-visible defect;
a stand-alone `mbEnable` toggle (the common case) is unaffected and stays warm.
- **Evidence [Verified]:** src/dsp/AnamorphEngine.cpp:488-489 (`mbStructuralChange` includes
  `pendingP.mbEnable != p.mbEnable`), :555 (reset on it). Raised in Devin review of PR #50
  (unresolved thread). See FUTURE_RISKS / ADR-0004 (warm-bank intent).
- **Possible resolution:** remove `mbEnable` from `mbStructuralChange` so a concurrent toggle fades
  out via `mbEnableBlend` while staying warm. This is a DSP state-transition change → requires an
  ADR + Architecture Review (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`); not done here.

## KI-002 — macOS artifacts not notarized
CI ad-hoc codesigns the macOS bundles but does **not** notarize them, so Gatekeeper quarantines
them after download and the user must run `xattr -dr com.apple.quarantine` before the DAW will load
them.
- **Evidence [Verified]:** .github/workflows/build.yml:148-151 (`codesign --sign -`, no notarization);
  packaging/macos/INSTALL.txt:4-10,30-33. See `docs/procedures/PACKAGING.md`.

## KI-003 — pluginval Linux editor tests crash (external)
The editor open/close tests can crash under pluginval on Linux due to a use-after-free in
**pluginval's own JUCE** X11 `XEmbedComponent` (`ConfigureNotify`→`callAsync` capturing a raw
pointer). It is **not a defect in this plugin** (the plugin already drops its OpenGL child window on
Linux, INC-006/ADR-0011) and is mitigated by a signal-only retry, but it cannot be fixed from this
repository.
- **Evidence [Verified]:** scripts/run-pluginval.sh:46-76; ADR-0011. See FUTURE_RISKS RISK-004.

## KI-004 — No automated DAW/host-compatibility testing
There is no in-repo test matrix across real DAWs; pluginval is the only conformance proxy. Host
behaviour (Ableton/Logic/Cubase/Reaper/Pro Tools/...) is therefore **Unverified**.
- **Evidence [Verified]:** docs/architecture/COMPATIBILITY_MATRIX.md (hosts Unverified); docs/procedures/TESTING.md ("What cannot be verified headlessly").
  Enforced as a manual line item in `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`.

## KI-005 — No graphical installer
Installation is a manual file copy to the platform plug-in folders (plus de-quarantine on macOS);
the repository contains no `.pkg`/`.msi`/installer build.
- **Evidence [Verified]:** no installer in the repository; packaging/macos/INSTALL.txt; `docs/procedures/PACKAGING.md` (TODO).

## KI-006 — Linux tooltip corners render black instead of transparent
On **Linux**, the rounded-capsule tooltip shows an **opaque black** fill in the corners (outside the
rounded shape) rather than the transparent background, so the rounding reads as a black box. This is
a **UI / platform rendering** observation only — it does **not** touch DSP, parameters, serialization,
or session state, and is fully isolated from the pluginval state-restoration work.
- **Platform matrix:** Linux — **Confirmed** (0.8.7 testing). Windows — **Unverified** (not yet
  checked). macOS — **Not observed**.
- **Root cause:** **not determined** (not assumed). The tooltip is painted as a 6 px rounded capsule
  in `AnamorphLookAndFeel::drawTooltip` (src/gui/LookAndFeel.cpp:811-819). A *related, already-known*
  precedent is documented for the popup menu — "rounded corners on an opaque … window leave bright
  corner/edge artefacts on some hosts," which is why the menu is kept square (src/gui/LookAndFeel.cpp:543-545).
  Whether the **same mechanism** applies to the tooltip window on Linux is **unconfirmed** and needs
  platform diagnosis (e.g. the `TooltipWindow` backing surface / alpha handling); do not treat the
  precedent as the proven cause.
- **Evidence [Partially Verified]:** Anamorph 0.8.7 platform feedback (Linux); rendering path
  src/gui/LookAndFeel.cpp:811-819. Cosmetic and low-impact: tooltips are **off by default**
  (src/InternalState.h:51, `int_tooltipsOn = false`).
- **Scope rule:** may be addressed independently as a LookAndFeel / `TooltipWindow` rendering change
  **only if** it stays unrelated to DSP / state; it must **not** block or influence the
  state/pluginval workflow.
