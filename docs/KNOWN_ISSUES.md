# KNOWN_ISSUES.md

**Only currently-existing, confirmed problems/limitations.** Resolved or historical issues belong
in `POSTMORTEMS.md`, not here. Each entry is evidence-backed (constraint C7). When an item is
fixed, remove it here and (if notable) add a `POSTMORTEMS.md` entry.

Verified against repository HEAD `c605fbe` (version 0.8.7; JUCE 8.0.14).

| ID | Issue | Severity | Status |
|---|---|---|---|
| KI-001 | Concurrent Multiband-Enable + other discrete change cold-starts the crossover bank | Low | Confirmed, masked (inaudible) |
| KI-002 | macOS artifacts not notarized (manual de-quarantine required) | Medium | Confirmed (distribution) |
| KI-003 | pluginval Linux editor tests crash (external host-side JUCE) | Low | Confirmed, mitigated/external |
| KI-004 | No automated DAW/host-compatibility testing | Medium | Confirmed (coverage gap) |
| KI-005 | No graphical installer (manual copy install) | Low | Confirmed (packaging) |
| KI-006 | Linux: tooltip rounded corners render an opaque black background instead of transparent | Low | Fix applied (LookAndFeel); Linux visual re-test pending |
| KI-007 | Windows: pluginval "Editor Automation" abnormally terminates (was hidden by a run-pluginval.ps1 false green) | Medium | False green closed; GL-drop cleared the crash (CI-confirmed); advancedMode-automation fix pending CI |

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

## KI-007 — Windows CI runner cannot host the editor GUI tests
On the GitHub **`windows-latest`** runner, pluginval's editor **"Editor Automation"** test fails
(originally a hard crash at the first Editor-Automation line; in CPU mode a contained `exit 1`). This
was first **masked by a false green** — `scripts/run-pluginval.ps1` ran `exit $LASTEXITCODE`, and an
abnormal termination leaves `$LASTEXITCODE` `$null` so `exit $null` exited **0**. The false green is
closed (null/large/negative codes are treated as a crash and fail the gate), which surfaced the
failure.
- **Root cause — environmental, confirmed:** the GPU-less/headless `windows-latest` runner cannot
  host this editor's intensive GUI test. It fails there in **both** rendering modes — GL mode (the
  runner's GDI-generic **OpenGL 1.1** renderer lacks the GL2 shader/VBO entry points JUCE needs) and
  CPU mode (`#if JUCE_MAC` build). The **plugin editor is not at fault**: it validates cleanly under
  pluginval strictness 10 on **Linux** (xvfb, CPU) and **macOS** (GPU/GL, incl. the same editor
  automation). When GL is re-enabled on **Linux** the analogous crash reproduces (~1 in 3 runs) and a
  **core dump's crashing frame is `juce::XEmbedComponent::Pimpl::handleX11Event(...)::{lambda}` under
  `juce::MessageManager::runDispatchLoop`** — i.e. inside JUCE's own X11 embedding on the host side,
  never in plugin code (the KI-003 class). gdb/ASan hide it (timing-sensitive race), confirming a
  host-side teardown race, not a plugin use-after-free.
- **Resolution:** Windows CI runs pluginval with **`--skip-gui-tests`** (`scripts/run-pluginval.ps1`).
  All non-GUI tests (audio / state / parameters / buses / automation) still run and still **block** on
  every platform; the editor GUI tests remain fully exercised on **Linux + macOS**. This is the Windows
  analogue of the KI-003 handling and uses pluginval's designed flag for GUI-hostile environments.
  **`--skip-gui-tests` is not the same as the randomise-mode "never skip" rule** (CI_CD.md): the two
  validation *modes* always run on every platform; this skips one *test category* on the one runner
  that provably cannot host it.
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
- **Evidence [Verified]:** run 28678842525 Windows job log (original: `... / Editor Automation...` last
  line, empty exit code, yet a green step — the false green); run 28695538067 Windows job on `b70f0b2`
  (post-GL-drop: full suite completes incl. Plugin state restoration, then `exit 1` inside Editor
  Automation — hard crash gone, contained failure remains); scripts/run-pluginval.ps1 (the closed false
  green). Related: KI-003 (the Linux editor-harness crash), scripts/run-pluginval.sh:70-91 (crash-retry).
