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
| KI-007 | Windows: pluginval "Editor Automation" abnormally terminates (was hidden by a run-pluginval.ps1 false green) | Medium | False green closed; GL-drop fix applied (CI confirmation pending) |

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

## KI-007 — Windows pluginval "Editor Automation" abnormally terminates
On **Windows**, pluginval consistently ends at `Starting tests in: pluginval / Editor Automation...`
without completing that test — the process dies with no clean exit code. This was **masked by a
false green**: `scripts/run-pluginval.ps1` ran `exit $LASTEXITCODE`, but an abnormal termination
leaves `$LASTEXITCODE` `$null` and `exit $null` exits **0**, so the crashed run passed the gate. The
false green is now closed (the script treats a null/large/negative code as a crash, retries, and
still fails after the retries — never `exit 0` on a non-pass), which **surfaces** this crash instead
of hiding it.
- **Platform matrix:** Windows — Confirmed (blocking gate on this branch). Linux/macOS — Not
  observed (both complete Editor Automation and pass genuinely: Linux ~40 s, macOS ~185 s per
  mode; the crashed Windows step ran in ~6–7 s).
- **Scope:** pre-existing and **platform-specific** — the base commit's Windows pluginval (then an
  "informational" step) also finished in ~7 s, and the parameter-layer changes on this branch are
  platform-agnostic (Linux/macOS unaffected). It is therefore **not** attributable to the discrete-
  parameter or undo work; it points at the Windows editor open path.
- **Root-cause hypothesis (strong, not stack-confirmed):** the crash is in the editor's **OpenGL**
  attach. GitHub's `windows-latest` runner is GPU-less and exposes only the **GDI-generic OpenGL 1.1**
  renderer, which lacks the GL2 shader/VBO entry points JUCE's GL `LowLevelGraphicsContext` calls, so
  the first Editor-Automation paint faults on the GL render thread. This is the same *class* of
  GL-under-editor-automation crash that made Linux drop its GL attach (KI-003 / ADR-0011); macOS CI
  has a real GL and is unaffected. A multi-lens investigation converged on GL as the cause.
- **Fix applied (CI confirmation pending):** the OpenGL attach is now guarded `#if JUCE_MAC` — GL is
  attached on macOS only; Windows (like Linux) renders via the CPU `paint()` path, which is **visually
  identical** (ADR-0011). This removes the Windows GL render thread and the fault with it. It adds no
  thread / cross-thread path / atomic ordering (it *removes* a render thread) and reuses the accepted
  0.8.5 Linux configuration, so it is an editor/platform decision, not a gated Thread-Model change.
  **If the Windows job is still red after this**, the crash is NOT the GL attach — revert the guard and
  capture a Windows crash stack (local `pluginval --validate` under a debugger, or a CI crash-dump
  upload) before trying again. Do **not** stack further blind guesses on top.
- **Deferred:** a GPU-capability probe (throwaway WGL context → confirm GL2 → attach) would restore GL
  on capable Windows machines, but needs a real Windows test bed; not attempted from this Linux sandbox.
- **Evidence [Verified]:** run 28678842525 Windows job log (`... / Editor Automation...` is the last
  line before both the deterministic and randomise passes end; `pluginval: FAILED ... (exit )` with
  an empty code yet a green step); scripts/run-pluginval.ps1 (the closed false green). Related: KI-003
  (the Linux editor-harness crash), scripts/run-pluginval.sh:70-91 (the crash-retry this PS1 now mirrors).
