# KNOWN_ISSUES.md

**Only currently-existing, confirmed problems/limitations.** Resolved or historical issues belong
in `POSTMORTEMS.md`, not here. Each entry is evidence-backed (constraint C7). When an item is
fixed, remove it here and (if notable) add a `POSTMORTEMS.md` entry.

Verified against repository HEAD `c605fbe` (0.8.7 content audit); version-synced to 0.8.9 for the functional/UX PR #56 (JUCE 8.0.14; previously synced to 0.8.8 for PR #54).

| ID | Issue | Severity | Status |
|---|---|---|---|
| KI-001 | Concurrent Multiband-Enable + other discrete change cold-starts the crossover bank | Low | Confirmed, masked (inaudible) |
| KI-002 | macOS artifacts not notarized (manual de-quarantine required) | Medium | Confirmed (distribution) |
| KI-003 | pluginval Linux editor tests crash (external host-side JUCE) | Low | Confirmed, mitigated/external |
| KI-004 | No automated DAW/host-compatibility testing | Medium | Confirmed (coverage gap) |
| KI-005 | No graphical installer (manual copy install) | Low | Confirmed (packaging) |
| KI-006 | Linux: tooltip rounded corners render an opaque black background instead of transparent | Low | Fix applied (LookAndFeel); Linux visual re-test pending |
| KI-007 | Windows: pluginval "Editor Automation" abnormally terminates (was hidden by a run-pluginval.ps1 false green) | Medium | False green closed; GL-drop cleared the crash (CI-confirmed); advancedMode-automation fix pending CI |
| KI-008 | Advanced-toggle one-frame tear in async-resize hosts (JUCE VST3 wrapper window-grant gap) | Low | Confirmed, external (JUCE wrapper + host); not fixable plugin-side without a JUCE change |

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
- **Evidence [Verified]:** .github/workflows/build.yml:159-162 (`codesign --sign -`, no notarization);
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
On **Linux**, the rounded-capsule tooltip showed an **opaque black** fill in the corners (outside the
rounded shape) rather than the transparent background, so the rounding read as a black box. This is a
**UI / platform rendering** issue only — it does **not** touch DSP, parameters, serialization, or
session state, and is fully isolated from the pluginval state-restoration work.
- **Platform matrix:** Linux — Confirmed (0.8.7 testing). Windows — Unverified. macOS — Not observed.
- **Mechanism:** on platforms **without per-pixel window alpha** (Linux/X11 with no compositor),
  `juce::TooltipWindow` cannot be semi-transparent, so the area **outside** the rounded capsule
  renders the window's opaque (black) fill. This is the same class of artefact already documented for
  the popup menu, which is kept square for exactly this reason (src/gui/LookAndFeel.cpp:543-545).
- **Fix [code Verified; Linux visual re-test pending]:** `AnamorphLookAndFeel::drawTooltip`
  (src/gui/LookAndFeel.cpp) now pre-fills the full tooltip bounds with the capsule colour when
  `juce::Desktop::canUseSemiTransparentWindows()` is `false`, so the corners match the capsule rather
  than rendering black. Where transparent windows ARE available (macOS / Windows / compositing Linux)
  the corners stay genuinely transparent — **no macOS/Windows visual change**. The headless gate
  cannot judge GUI appearance (TESTING_POLICY Level 5), so a **Linux visual re-test by the maintainer**
  is needed to fully close this; until then it stays listed here rather than moved to POSTMORTEMS.
- **Evidence:** src/gui/LookAndFeel.cpp `drawTooltip` (the alpha-gated corner fill); 0.8.7 Linux
  feedback. Cosmetic, low-impact: tooltips are **off by default** (src/InternalState.h:51).

## KI-007 — Windows CI: pluginval script did not wait for pluginval (garbled output + false pass/fail)
The Windows pluginval step produced **interleaved/garbled console output** and reported both false
GREENS (originally) and, once a crash-retry loop was added, false REDS — while the plugin actually
validated fine.
- **Root cause — CONFIRMED (a script bug, not a plugin defect):** `pluginval.exe` is a **GUI-subsystem**
  app, so PowerShell's call operator (`& $pv`) does **not wait** for it — it returns immediately with a
  `$null $LASTEXITCODE`. The original `exit $LASTEXITCODE` therefore did `exit 0` (false green). After
  the crash-retry loop was added, that `$null` was misread as a *crash*, so the loop retried, and **each
  retry launched another pluginval that kept validating in the background** → three concurrent validators
  writing to one console (the "garbled" interleaving) and a false failure. The CI log shows it directly:
  three `Started validating: …` lines appear *after* the script already "gave up". **Fix:** launch
  pluginval via `System.Diagnostics.Process` with `UseShellExecute=$false` (inherits the console so
  output still streams) and `WaitForExit()`, then read the **real** `.ExitCode`. Exactly one pluginval
  runs at a time — no interleaving — and the exit code is now trustworthy (`scripts/run-pluginval.ps1`).
- **`--skip-gui-tests` on Windows (retained, conservative):** the GPU-less/headless `windows-latest`
  runner almost certainly cannot render the JUCE OpenGL editor — it exposes only the GDI-generic
  **OpenGL 1.1** renderer, which lacks the GL2 shaders JUCE needs (a well-documented JUCE-on-headless-CI
  limit). Because the wait bug above masked **every** real editor result on Windows, this was never
  actually observed, so skipping the editor GUI tests there is a **precaution**, not a proven necessity.
  All non-GUI tests (audio/state/parameters/buses/automation) still run and still **block**; the editor
  is fully validated on **Linux (xvfb) + macOS**. Distinct from the mode-level "never skip" rule
  (CI_CD.md): the two validation *modes* always run; this skips one *test category* on one runner.
- **OpenGL is unchanged for users:** the attach guard is restored to `#if ! (JUCE_LINUX || JUCE_BSD)`,
  so **Windows and macOS keep GPU/GL rendering** (real machines have a GPU); only Linux/X11 stays CPU
  (the host-side XEmbed UAF above, ADR-0011). No plugin *editor* code changed.
- **Related hardening — `advancedMode` is non-automatable:** host-automating that UI-layout toggle
  drives editor resizes (`applyUiScale`), whose `ConfigureNotify` storm hits the **same** host-side
  XEmbed UAF on Linux/X11 even with GL off — reproduced here (GL-off Linux + `advancedMode` automatable
  crashed under `--randomise`; core dump = `XEmbedComponent`). `isAutomatable()` is now false, removing
  that trigger and stabilising the Linux gate; a layout toggle has no place in an automation lane
  anyway. See `PARAMETER_REGISTRY.md` and KI-003.
- **Coverage note:** the editor is not exercised by pluginval on Windows CI (a coverage gap like
  KI-004), but its code is platform-agnostic and validated on two platforms; a GPU-equipped Windows
  runner would let the GUI tests run there too.
- **Evidence [Verified]:** run 28702902413 Windows job log (`c496c8b`) shows the confirmed root cause —
  after the script "gives up" (`pluginval: still crashing ... after 3 attempts`), **three** `Started
  validating: …` lines appear and their output interleaves (the garbling), i.e. `& $pv` never waited
  and the retries spawned concurrent validators; the detached runs actually print `SUCCESS`. The
  original false green (run 28678842525: empty exit code yet a green step) is the same non-waiting bug
  (`exit $null` → 0). Fix: `scripts/run-pluginval.ps1` (`System.Diagnostics.Process` + `WaitForExit`).
  Related: KI-003 (Linux GL editor host-side crash), scripts/run-pluginval.sh (crash-retry).

## KI-008 — Advanced-toggle one-frame tear in async-resize hosts (JUCE VST3 wrapper window-grant gap)
Toggling Advanced mode can show **one to a few frames** of the new layout **clipped to the old
window size** (entering Advanced: the Multiband/Input/Output tiers cut off; leaving: the Simple
layout with a black band below it) before the plugin window snaps to its new size — the visible
"controls jump/shake for a frame". This is **not** an editor defect: the editor's own mode switch
is a single atomic relayout (0.8.9 verified: zero component-bounds churn across 30/30 sampled
frames after the toggle; visibility/layout consistent at every host-observable instant).
- **Mechanism — CONFIRMED (PR #57 investigation):** JUCE's VST3 `ContentWrapperComponent` never
  resizes itself when the editor resizes; it calls `plugFrame->resizeView()` and waits for the
  host's `onSize()` to resize the OS child window (juce_audio_plugin_client_VST3.cpp: the
  `childBoundsChanged` → `resizeHostWindow` path). A host that grants the resize **asynchronously**
  (on a later message-loop pass — FL/Live/Bitwig-class behaviour) leaves a gap in which the editor
  content is the NEW mode while the OS window is still the OLD size; anything painted in that gap
  is the torn frame. JUCE itself acknowledges this host class: the wrapper hard-codes an allowlist
  (Wavelab/Ableton Live/Bitwig, + Reaper on macOS) that self-resizes immediately to close the gap.
- **Evidence [Verified]:** PR #57 harness (instrumented JUCE hosting glue emulating a deferred
  grant against the real Anamorph.vst3): with a synchronous grant the wrapper/editor size mismatch
  window is ~0.5 ms inside one message pass and never paints (zero mismatch paints); with a
  50–120 ms deferred grant the wrapper **paints the torn window** ("wrapper=940x900
  editorWants=940x720") and screen capture shows the clipped Advanced layout in the Simple-size
  window until the grant lands. Linux Standalone (X11/CPU): does **not** reproduce (window and
  content change together). Externally corroborated by JUCE forum reports of the same artifact
  class (VST3 resize glitches in FL Studio/Bitwig/Live; "UI shifted with swaths left uncovered"
  on FL/macOS).
- **Scope:** host-dependent (async-granting DAWs); severity Low (cosmetic, one-to-few frames,
  only on the mode toggle / window-size change). The OpenGL attachment on Windows/macOS may add a
  same-length stale-stretch during the same gap (not representatively testable headless).
- **Possible resolution (not done here):** upstream JUCE issue and/or extending the wrapper's
  self-resize allowlist to the affected host(s) via a patched JUCE — a dependency change requiring
  an ADR + Architecture Review (`DEPENDENCY_POLICY.md`); needs the affected host/OS identified
  first, and manual DAW validation. Any editor-side workaround (resizing the wrapper parent from
  plugin code) would gamble on per-host behaviour JUCE itself allowlists, and is deliberately not
  attempted.
