# KNOWN_ISSUES.md

**Only currently-existing, confirmed problems/limitations.** Resolved or historical issues belong
in `POSTMORTEMS.md`, not here. Each entry is evidence-backed (constraint C7). When an item is
fixed, remove it here and (if notable) add a `POSTMORTEMS.md` entry.

Version-synced to **v0.9.5** (the A7 performance programme). That round **added one issue**:
**KI-026**, the x86-64 ISA floor. It is the first entry here recording a limitation that was
*chosen* rather than discovered — ADR-0031 compiles the Linux binaries and the macOS `x86_64` slice
`-march=haswell` for a measured −17.2 % of the engine's instruction count with the output
bit-identical, and the price is that pre-2013 Intel / pre-2015 AMD hardware raises `SIGILL` inside
the host. The three A7-9 gate repairs in the same version removed no entry and added none: the
defect they fixed (three Amount-0 fast paths that were unreachable after a ramp-down) was never
filed here, having been found by measurement in the same programme that fixed it. Net for 0.9.5:
**one issue added, none removed.**

Prior sync: **v0.9.4** (the JUCE 9.0.0 → 9.0.1 dependency upgrade, ADR-0026, plus the
C++ standard 17 → 23 migration, ADR-0027 — **no issue added or removed**: the only `src/` change
is one added `#include`, engine output bit-identical across both the two JUCE versions and
C++17 vs C++23, and
the two known issues whose mechanism lives in JUCE (KI-013, KI-019) were re-verified byte-identical
there, so neither is fixed upstream nor regressed). The same version also moves the macOS CI job
off the **deprecated** `macos-14` image to `macos-latest`, and the four
`-Wimplicit-int-float-conversion` diagnostics AppleClang 21 then raised on pre-existing code are
**fixed** (an explicit `(float)` cast at each site; the three translation units compile to
byte-identical machine code, so nothing observable changed) — likewise **no issue added or
removed**, and no new limitation to record here. Both are described in
`docs/procedures/CI_CD.md`. The same version's **CI/validation round** then removed one issue:
**KI-014** "The macOS AU is shipped but never validated automatically" — the macOS job now runs
the full pluginval release gate against the AU as well as the VST3 (same strictness, both modes
×3, against the packaged bundle), so the coverage gap the entry recorded no longer exists. No
issue was added by that round: the residual — the gate is **pluginval**, not `auval` — is
recorded as scope in `docs/procedures/CI_CD.md` §"Known coverage limits" and as the narrowed
RH-F3, not as a limitation of the product. The same version's **engineering-roadmap round** then
added one: **KI-023**, the Linux glibc floor. It records a limitation that has been true since the
CI image moved to Ubuntu 24.04 and was simply never measured — the round that measured it also
gated it, so it can no longer rise unnoticed. The same version's **hover-occlusion round**
(2026-08-19) added **two more**, both found by the measurement that verified the fix rather than by
reading: **KI-024**, the Settings / About / Save Preset overlays occlude exactly as a drop-down does
and were not covered by that fix, and **KI-025**, the micro-animation idle gate could seal on a
control that was still lit. **Both were fixed in the same version (2026-08-19) and are removed from
this file per the fixed-item rule above** — the overlay case by a general per-widget occlusion test
that derives what counts as an overlay instead of listing the three, and the idle gate by giving its
"nothing can move" condition the second half it was missing. Net for 0.9.4 so far: **two issues
added and the same two removed**, plus KI-023.
**Drift corrected in the same round (`DOCUMENTATION_LIFECYCLE_POLICY` C6):** the summary table
below stopped at KI-022, so **KI-023 had a full entry but no table row** since it was filed on
2026-08-18. The row is added here alongside the two new ones; no other content changed.
Prior sync: **v0.9.3** (six GUI interaction fixes plus an equal-width Widen row — the Multiband add-split preview line, the
unified pop-up dismissal shield, pop-up lifetime across a hidden, destroyed or backgrounded window,
two menu-rendering fixes and the Tooltips on/off transition —
**five issues added**: KI-018, the dismissing click still counts toward JUCE's double-click run;
KI-019, the app-switch pop-up dismissal is inert on Linux/X11; KI-020, the dismissal guarantee
is per-instance where pop-up modality is process-global; and, from the 0.9.3 **installer** work,
KI-021, a Linux per-user install leaves an existing system-wide one in place, and KI-022, the macOS
package no longer follows a bundle the user moved out of its standard location).
Prior sync: **v0.9.2**
(preset drop-down lifetime/crash fix, factory-preset identity, the
`Window Size` → `UI Scale` label and the installer component titles — **one issue added**: KI-017,
macOS suppresses key auto-repeat for letters and digits in any focused text field, which is an OS
text-input behaviour rather than a plug-in defect; **no issue removed** — the crash fixed this
cycle was never filed here, it was reported directly by the maintainer). Prior sync:
**v0.9.1** (manufacturer-code change, ADR-0023 — **one issue added**: KI-016,
sessions saved before 0.9.1 report the plug-in as missing because the AU manufacturer field and
the VST3 class UID changed; no issue removed, no status moved, and the DSP is bit-identical to
0.9.0). Prior sync: **v0.9.0** (release-prep, 2026-07-24, PR #87 + the installer/packaging rework
PR #89 — no plugin code changed since
v0.8.12 (the JUCE 9 bump is proven bit-identical), so no issue's status moved except one issue
**removed**: KI-005 "No graphical installer" — v0.9.0 ships a Linux install script (inside
the zip), a Windows Inno Setup installer and a macOS .pkg, all installing system-wide with
component selection on Windows/macOS (unsigned until RH-PR-3/5; `docs/procedures/PACKAGING.md`
§Installers). Previously verified against repository HEAD `64e87c4` (post-v0.8.12
content re-audit), synced to the **v0.8.12 release** (changelog-dated 2026-07-22, PR #79
performance Wave 6 + PR #80 GUI interaction fixes — one issue **added**: KI-013, the
release-outside stuck-press reconcile is inert on macOS
(JUCE's realtime query returns cached button state there); no issue removed. The **v0.8.11 release**
of 2026-07-20 (PRs #60/#61 — the ADR-0015 crossover-follower fixes; PRs #62/#76 — performance
Waves 3–5; PR #63 — RH-PR-2 build hardening) added and
removed none). Prior sync: the **v0.8.10 release** (finalized 2026-07-14, PR #59 — undo/redo forced-duck dry-fill + rapid-swap
robustness, multiband flat recombination, adaptive `FrameClock` GUI refresh, plus the pre-merge
correctness round: split-drag pitch-shift fix, Band Solo alt-click exclusive solo, Option-reset
undo fix — the last of which surfaced **KI-010** (typed value-box entry still bypasses undo, same
mechanism, reported not fixed) — and the second correctness round: the split-drag transition
rework — final design after five measured rounds: a slew-limited cutoff smoother under a
frequency-proportional R(f) = 4·max(1, f/300) oct/s cap (ADR-0015 final + slow-drag fix: normal
drags track 1:1, a small controlled FM above the cap — the 1.25 oct/s "inaudibility" follower +
release consolidation was rejected for interaction latency, and the interim flat 4 oct/s cap
for the slow-drag regression) plus a discrete-jump bank crossfade, recorded as the **KI-012**
limitation
(artifact-free fast IIR tracking is physically impossible) — the
forced-duck dry-fill output-gain latch (Test 30), and **KI-011** (Apple-Silicon-native tooltip
white corners — fix applied, hardware re-test pending); **KI-009 carried
forward** — the REAPER Save Preset focus report, host-specific, pending manual investigation, not
fixed). Prior: the v0.8.9 release (finalized 2026-07-12, PR #58 — Wave-2 performance work; no
new/removed issues), including the KI-008 addition from the PR #57 investigation (previously synced
for the functional/UX PR #56;
JUCE 8.0.14; before that 0.8.8 for PR #54).

| ID | Issue | Severity | Status |
|---|---|---|---|
| KI-001 | Concurrent Multiband-Enable + other discrete change cold-starts the crossover bank | Low | Confirmed, masked (inaudible) |
| KI-002 | macOS artifacts not notarized (Gatekeeper prompt on the `.pkg`; manual de-quarantine on the zip route) | Medium | Confirmed (distribution) |
| KI-003 | pluginval Linux editor tests crash (external host-side JUCE) | Low | Confirmed, mitigated/external |
| KI-004 | No automated DAW/host-compatibility testing | Medium | Confirmed (coverage gap) |
| KI-006 | Linux: tooltip rounded corners render an opaque black background instead of transparent | Low | Fix applied (LookAndFeel); Linux visual re-test pending |
| KI-007 | Windows: pluginval "Editor Automation" abnormally terminates (was hidden by a run-pluginval.ps1 false green) | Medium | False green closed; GL-drop cleared the crash (CI-confirmed); advancedMode-automation fix in place — no recurrence observed (green release gates recorded in HANDOVER Build Status, v0.8.9–v0.8.12) |
| KI-008 | Advanced-toggle one-frame tear in async-resize hosts (JUCE VST3 wrapper window-grant gap) | Low | Confirmed, external (JUCE wrapper + host); not fixable plugin-side without a JUCE change |
| KI-009 | REAPER: Save Preset text editor loses keyboard focus (Space hits transport; a click does not re-focus until the dialog is reopened) | Low | Reported, host-specific (REAPER); pending manual investigation |
| KI-010 | Typing a value into a knob/slider text box creates no Undo step (gesture-less edit path) | Low | Confirmed (code path); reported during the 0.8.10 Option-reset fix, not yet fixed |
| KI-011 | macOS Apple-Silicon-native: tooltip corners rendered an opaque white frame (TooltipWindow opacity contract) | Low | Fix applied (editor marks the TooltipWindow non-opaque on macOS); Apple Silicon visual re-test pending |
| KI-012 | Fast Multiband split drags carry a small controlled FM (~14 cents at a 150 Hz crossing, ~7 cents above 300 Hz, under the R(f) = 4·max(1, f/300) oct/s cap; normal drags track 1:1; a violent flick catches up in ~0.5 s of continuous motion; fast artifact-free tracking needs linear-phase splits = latency change) | Low | Documented limitation (ADR-0015 final + slow-drag fix); revisit only via Architecture Review |
| KI-013 | macOS: release outside the window can still leave a control stuck pressed (the v0.8.12 reconcile is inert there — JUCE's realtime query returns cached mouse-button state on macOS) | Low | Confirmed, external (JUCE platform implementation); recovery on cursor re-entry intact |
| KI-015 | Anamorph declares **no licence of its own** — the repository root has no `LICENSE` file and the installers present no EULA, so the terms the binaries are offered under are undeclared | High | Confirmed; owner/legal decision (RH-R11 / RH-F1), not an engineering task |
| KI-016 | **Sessions saved with a pre-0.9.1 build report Anamorph as missing** — 0.9.1 changed the manufacturer code `Anmf` → `RTec`, which is the AU component's manufacturer field and feeds the VST3 class UID, so the host cannot match the old identity | Medium | Confirmed, **deliberate** and one-time (ADR-0023); recovery is to re-insert the plug-in |
| KI-017 | macOS: holding a letter or digit in a plug-in text field does not auto-repeat (symbols do) — macOS press-and-hold owns the alphanumeric keys once a JUCE text field has focus | Low | Confirmed external (macOS text input); JUCE path verified, OS attribution pending one `defaults` check; user-side workaround documented |
| KI-018 | The click that dismisses a pop-up is not delivered to any control (by design), but it still counts toward JUCE's multi-click run, so a fast follow-up click can arrive as a **double**-click | Low | Confirmed; traced to `MouseInputSourceImpl::registerMouseDown`, which is component-agnostic. No in-bounds fix exists — see the entry |
| KI-019 | **Linux/X11**: the 0.9.3 dismissal of an open pop-up when the user switches application does not fire — JUCE's foreground flag there is a write-once latch, never cleared, so "switched away" can never be observed | Low | Confirmed (code path, pinned JUCE); inert in the SAFE direction — no spurious dismissal. The hidden-editor and editor-destroyed halves work normally on Linux |
| KI-020 | With **two or more Anamorph editors open at once**, the "a dismissing click activates nothing" guarantee holds only within the instance that owns the open pop-up: modality is process-global, so the re-delivered click can reach a *different* instance's control | Low | Confirmed (code path); pre-dates 0.9.3 and is not introduced by the shield. No in-bounds fix — see the entry |
| KI-021 | **Linux**: a per-user install (the 0.9.3 default) does not remove an existing **system-wide** one, so both are on the DAW's scan path and its scan order decides which version loads — an update can look as if it did not apply | Low | Confirmed, **deliberate**: removing the system copy needs the elevation the per-user mode exists to avoid. The installer now **warns** when it finds one; removal is one `sudo ./uninstall.sh` |
| KI-022 | **macOS**: components are non-relocatable since 0.9.3 (INC-012), so a bundle the user deliberately moved — to `~/Applications`, or a plug-in kept under `~/Library/Audio/Plug-Ins/…` — is left where it is and a fresh copy is installed at the standard location, leaving two copies with the same bundle identifier | Low | Confirmed, **deliberate trade** for INC-012: following a moved copy is exactly the behaviour that let an install silently write nowhere useful. Documented in the installer's `INSTALL.txt`; removal is manual |
| KI-023 | **Linux**: the shipped binaries record the CI image's glibc/libstdc++ floor (measured GLIBC_2.38 / GLIBCXX_3.4.31), so they do not load at all on older distributions — Ubuntu 22.04 LTS included | Medium | Confirmed, **measured**; the floor was never chosen and is now asserted on every push so it cannot rise unnoticed. Lowering it is a release-topology decision, not taken |
| KI-026 | **Pre-2013 Intel / pre-2015 AMD CPUs**: every shipped x86-64 binary is compiled for AVX2 — Linux and the macOS `x86_64` slice at `-march=haswell` (ADR-0031, 0.9.5), Windows at `/arch:AVX2` (ADR-0032) — so on an older CPU the plug-in raises an illegal-instruction fault **inside the host** (`SIGILL`; `STATUS_ILLEGAL_INSTRUCTION` on Windows). The DAW reports a crash, not an incompatible plug-in | Medium | Confirmed, **deliberate** (ADR-0031/0032; output bit-identical, verified per push on Windows by the blocking A/B gate). Only Apple Silicon is unaffected. No in-product diagnosis is possible; the requirement is documented in the user guides |

---

## KI-001 — Concurrent Multiband-Enable + discrete change cold-starts the bank during the duck
When `mbEnable` flips **simultaneously with another discrete change** (e.g. algorithm), the change
is deferred to the silent duck bottom, where `mbStructuralChange` (which still includes
`mbEnable`) resets the multiband bank and SoloMonitor — so the crossover bank cold-starts during
the fade-in instead of staying warm, partially defeating the 0.8.6 warm-bank design for that
specific case. The reset is **masked by the duck (inaudible)**, so there is no user-visible defect;
a stand-alone `mbEnable` toggle (the common case) is unaffected and stays warm.
- **Evidence [Verified]:** src/dsp/AnamorphEngine.cpp:689 (`mbStructuralChange` includes
  `pendingP.mbEnable != p.mbEnable`), :743 (reset on it). Raised in Devin review of PR #50
  (unresolved thread). See FUTURE_RISKS / ADR-0004 (warm-bank intent).
- **Possible resolution:** remove `mbEnable` from `mbStructuralChange` so a concurrent toggle fades
  out via `mbEnableBlend` while staying warm. This is a DSP state-transition change → requires an
  ADR + Architecture Review (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`); not done here.

## KI-002 — macOS artifacts not notarized
CI ad-hoc codesigns the macOS bundles but does **not** notarize them. Two user-facing
consequences, both still open:
- **Zip route:** Gatekeeper quarantines the extracted bundles, so the user must run
  `xattr -dr com.apple.quarantine` before the DAW will load them.
- **`.pkg` route (v0.9.0):** the installed payloads are **not** quarantined (no Terminal step),
  but opening the unsigned package itself is refused once — the user has to approve it via
  *System Settings → Privacy & Security → Open Anyway*.

Notarization (RH-PR-3) closes both.
- **Evidence [Verified]:** .github/workflows/build.yml:1960-1962 (`codesign --force --deep --sign -`,
  no notarization); packaging/macos/INSTALL.txt:4-10 (ad-hoc, not notarized), :34-41 (the
  Gatekeeper approval for the .pkg), :61-65 (the zip-route `xattr` step).
  See `docs/procedures/PACKAGING.md`.

## KI-003 — pluginval Linux editor tests crash (external)
The editor open/close tests can crash under pluginval on Linux due to a use-after-free in
**pluginval's own JUCE** X11 `XEmbedComponent` (`ConfigureNotify`→`callAsync` capturing a raw
pointer). It is **not a defect in this plugin** (the plugin already drops its OpenGL child window on
Linux, INC-006/ADR-0011) and is mitigated by a signal-only retry, but it cannot be fixed from this
repository.
- **Evidence [Verified]:** scripts/run-pluginval.sh:147-197 (`run_one_pass`, signal-only retry); ADR-0011. See FUTURE_RISKS RISK-004.

## KI-004 — No automated DAW/host-compatibility testing
There is no in-repo test matrix across real DAWs; pluginval is the only conformance proxy. Host
behaviour (Ableton/Logic/Cubase/Reaper/Pro Tools/...) is therefore **Unverified**.
- **Evidence [Verified]:** docs/architecture/COMPATIBILITY_MATRIX.md (hosts Unverified); docs/procedures/TESTING.md ("What cannot be verified headlessly").
  Enforced as a manual line item in `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`.

*(KI-005 "No graphical installer" — RESOLVED in v0.9.0: Linux install script shipped in
the zip, Windows Inno Setup installer (component selection + dual-path destination page),
macOS .pkg (component selection); see `docs/procedures/PACKAGING.md` §Installers. The
installers are not yet signed/notarized — that remains KI-002 / RH-PR-3/5.
ID retired, never reused.)*

## KI-006 — Linux tooltip corners render black instead of transparent
On **Linux**, the rounded-capsule tooltip showed an **opaque black** fill in the corners (outside the
rounded shape) rather than the transparent background, so the rounding read as a black box. This is a
**UI / platform rendering** issue only — it does **not** touch DSP, parameters, serialization, or
session state, and is fully isolated from the pluginval state-restoration work.
- **Platform matrix:** Linux — Confirmed (0.8.7 testing). Windows — Unverified. macOS — Not observed.
- **Mechanism:** on platforms **without per-pixel window alpha** (Linux/X11 with no compositor),
  `juce::TooltipWindow` cannot be semi-transparent, so the area **outside** the rounded capsule
  renders the window's opaque (black) fill. This is the same class of artefact already documented for
  the popup menu, which is kept square for exactly this reason (src/gui/LookAndFeel.cpp:557-560).
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

## KI-009 — REAPER: Save Preset text editor keyboard focus is lost after the field loses focus
In **REAPER** the Save Preset name field does not hold keyboard focus reliably:
- With the field focused, pressing **Space types nothing into the field and instead triggers the
  DAW transport** (Play/Pause).
- After the field **loses focus once** (e.g. clicking elsewhere in the plugin window, or the host
  reclaiming focus), **clicking the text again does not reactivate editing** and does not restore
  the text-selection highlight — the caret does not return.
- Editing only becomes possible again after **closing and reopening** the Save Preset dialog (which
  re-runs the deferred focus-grab retry, see below).
- **Other tested DAWs behave correctly** — the field takes focus on open, Space types a space, and
  a later click re-focuses the field normally.

This is a **host-specific interaction**, not a general editor defect: the same code path works in
the other hosts exercised so far. It is filed for **manual investigation** on REAPER specifically;
no fix is attempted here (a fix would need the reproducing host in front of a developer to confirm
the JUCE focus/peer path REAPER takes).

- **Affected host(s) [Reported]:** REAPER (version / OS not yet pinned in the report). Not observed
  in the other DAWs tested. REAPER's overall compatibility is already **Unverified** in the matrix
  (`docs/architecture/COMPATIBILITY_MATRIX.md`; KI-004 — no automated host testing), so this is a
  concrete, reproducible instance of that coverage gap rather than a regression from a known-good
  state.
- **Observed behaviour / reproduction (REAPER):** (1) load Anamorph (VST3) in REAPER; (2) open the
  preset menu → **Save Preset**; (3) with the name field showing, press **Space** → the transport
  toggles instead of a space being typed; (4) click outside the field, then click back on the text
  → the field does **not** regain the caret/selection and typing does not go to it; (5) close and
  reopen the Save Preset dialog → editing works again (until focus is next lost).
- **Current evidence [Partially Verified]:** The plugin already carries a **one-time** focus
  workaround for the *open* path: `focusSaveNameField()` grabs keyboard focus and, if the grab does
  not stick (the preset-menu's desktop window still owns OS focus at the callback instant, and JUCE
  aborts an internal focus move while `! peer->isFocused()`), it retries on later message-loop
  passes up to four times (src/PluginEditor.cpp:2095-2103; declared src/PluginEditor.h:248). This shipped in
  the v0.8.9 CHANGELOG "Fixed" entry ("The Save Preset name field reliably receives typing — Space
  included") and was **validated headless end-to-end**, i.e. against the JUCE wrapper, not against
  REAPER. The retry loop runs **only on dialog open** (`showSavePreset(true)` → `focusSaveNameField(4)`);
  there is **no focus re-acquisition after a later focus loss** — no `focusLost` handler,
  `mouseDown`-grab, or `setMouseClickGrabsKeyboardFocus` override on `saveNameEditor` (repo-wide:
  the only focus calls are src/PluginEditor.cpp:2064 (the on-open call) / src/PluginEditor.cpp:2095-2103 (`focusSaveNameField` itself) and the unrelated SpectrumImager freq editor).
  A click on the field then relies on JUCE's default click-to-focus, which is subject to the same
  `peer->isFocused()` abort if REAPER holds OS focus on the plugin's parent window — consistent with
  "clicking the text does not reactivate editing until the dialog is reopened". This is a strong
  hypothesis, **not yet confirmed on REAPER hardware**; the exact focus/peer sequence REAPER follows
  is the open question the manual investigation must answer.
- **Scope:** host-dependent (REAPER); severity **Low** (Save Preset only; the workaround on reopen
  restores editing; no audio, parameter, automation, preset-data or serialization impact). No other
  workflow is affected. The general "type in a text field" path works in the other tested hosts.
- **Status:** **Reported, host-specific, pending manual investigation.** Not fixed in this change by
  decision — a correct fix needs REAPER in front of a developer to (a) confirm which JUCE focus/peer
  branch aborts and (b) verify any re-focus handler (e.g. a `focusLost`/`mouseDown` re-grab mirroring
  the open-path retry) actually sticks in REAPER without regressing the hosts that already work. Track
  under the KI-004 host-matrix gap; revisit when a REAPER audition slot is available.

## KI-010 — Typed value-box entry creates no Undo step (gesture-less edit path)

Found while fixing the 0.8.10 Option/Alt-click reset undo bug (CHANGELOG [0.8.10]). Typing a
value into a knob/slider's text box commits via `juce::Slider::setValue`, which reaches the
parameter through the `SliderAttachment` **without** a host change gesture — the same mechanism
that made resets un-undoable. The processor's undo coalescer deliberately folds gesture-less
changes into the committed baseline (that is how host automation is excluded from undo,
ADR-0008), so a typed edit produces **no Undo entry and does not invalidate Redo**. The reset
path is fixed (wrapped in `beginChangeGesture`/`endChangeGesture`, `Knob::doReset`); the typed
path is **left unchanged in 0.8.10 by scope decision** — it was not part of the reported issue,
and a correct fix has UX questions of its own (a gesture per keystroke vs per commit; interaction
with the focus-driven `knobSweepTime` easing).

- **Repro:** adjust any knob by typing into its value box and pressing Enter → press Undo: the
  previous action is reverted instead of the typed edit; Redo (if it was available) survives.
- **Also affected (same class):** the Multiband display's **mouse-wheel** nudges of a split
  frequency or band width (`SpectrumImager::mouseWheelMove` → gesture-less `setParam`). A wheel
  scroll on a regular knob IS undoable (JUCE's `Slider::mouseWheelMove` wraps the change in a drag
  notification, which the attachment turns into a gesture); every click/drag/reset edit inside the
  imager is undoable too (verified: they run through `beginGesture`/`endGesture` or ride a
  concurrent gesture's coalesced snapshot). Only the imager's wheel path and the text-box path
  are gesture-less.
- **Scope:** editor-only; automation/preset/serialization unaffected (the value itself lands
  correctly and marks the preset dirty). Severity **Low**.
- **Evidence [Verified, code path]:** src/PluginEditor.h (`Knob::doReset` gesture wrap + comment);
  src/PluginProcessor.cpp `pollUndoCoalesce` (the non-gesture fold branch); JUCE
  `SliderParameterAttachment::sliderValueChanged` → `setValueAsPartOfGesture` (no begin/end for
  programmatic value changes).
- **Fix direction (when scheduled):** wrap the text-commit path in a gesture the same way the
  reset now is — e.g. detect a text-box-driven `onValueChange` (focus held, mouse up — the same
  predicate the reset-sweep easing already uses) and issue a complete gesture around it.

## KI-011 — macOS Apple-Silicon-native: tooltip corners rendered an opaque white frame

Reported on **macOS running the ARM-native build** (Intel and the same machine under Rosetta
render correctly): the area outside the tooltip's rounded capsule showed an **opaque white
rectangle** instead of transparent rounded corners.

- **Mechanism [Verified, code path]:** `juce::TooltipWindow` declares itself **opaque**
  (`setOpaque (true)` in its constructor — JUCE 8.0.14 juce_TooltipWindow.cpp:42; unchanged in
  JUCE 9.0.0, now at windows/juce_TooltipWindow.cpp:43) while
  `AnamorphLookAndFeel::drawTooltip` deliberately leaves the pixels outside the rounded capsule
  unpainted on platforms with per-pixel window alpha. That is an opacity-contract violation: the
  corner pixels are **undefined**, and what appears there depends on the compositing pipeline.
  Intel/Rosetta happened to show the stale (transparent) backing; Apple-Silicon-native AppKit
  initialises the opaque layer-backed `NSWindow` with its background colour (white — JUCE pins
  tooltips to the light Aqua appearance) before the component paints → the white frame. This is
  the same undefined-pixels class as KI-006's black corners on uncomposited Linux/X11.
- **Fix [code Verified; Apple Silicon visual re-test pending]:** the editor marks its
  `TooltipWindow` **non-opaque on macOS** (src/PluginEditor.cpp, constructor). The JUCE peer then
  creates a transparent `NSWindow` (`setOpaque:NO` + `clearColor` background) and **clears the
  backing to alpha 0 on every paint** (`NSViewComponentPeer::drawRectWithContext` →
  `CGContextClearRect` for non-opaque components), so the corners are genuinely transparent by
  contract on every pipeline — Intel, Rosetta, and ARM native. macOS-gated (`JUCE_MAC`): Windows
  and Linux keep their existing (working) behaviour; uncomposited Linux keeps the KI-006 corner
  pre-fill, which covers the same class there.
- **Scope:** cosmetic, tooltips only (off by default); no audio/parameter/state impact. Severity
  **Low**. A side effect on macOS is that the native drop shadow now follows the capsule's alpha
  outline instead of the rectangular window bounds — the correct shape.
- **Status:** **Fix applied; pending an Apple-Silicon-native visual re-test by the maintainer**
  (the headless gate cannot judge GUI appearance — TESTING_POLICY Level 5). Remove this entry and
  move it to POSTMORTEMS once confirmed on hardware.

## KI-012 — Fast Multiband split drags carry a small controlled FM (design limitation)

A **deliberate, measured limitation** shipped with the 0.8.10 split-movement fix, documented so
it is not mistaken for a defect. A swept zero-latency IIR crossover is inherently a phase
modulator: its allpass phase at any fixed frequency rotates by up to 2π per crossover crossing,
which is a genuine frequency shift of `0.312·R` Hz at sweep rate `R` oct/s. No transition scheme
removes this — five implementations were measured against a pure-sine protocol (uncapped ~8 oct/s
glide: +31 cents at a 150 Hz crossing; chained 12 ms bank crossfades: −25…−28 dBc modulation
sidebands; τ=15 ms one-pole tracking: ~50 cent FM at a fast crossing; a 1.25 oct/s
"inaudibility" cap + 0.25 s release consolidation: measurably clean but rejected in interactive
testing for **interaction latency**; a flat ~4 oct/s cap: fixed the flick case but pinned every
normal drag whole octaves behind on the ~90 px/octave display — the v0.8.10 slow-drag
regression). The shipped design (ADR-0015 final + slow-drag fix) is a slew-limited smoother
under a **frequency-proportional cap `R(f) = 4·max(1, f/300 Hz)` oct/s**, keeping the restated
product trade: *a small amount of controlled FM is preferable to obvious interaction latency.*

- **Consequence:** drags within the cap track 1:1 (± a 20 ms ease — the crossover feels
  attached to the mouse; the cap is 13.3 oct/s at 1 kHz, 160 at 12 kHz, so normal gestures
  never outrun it above ~300 Hz). Movement faster than the cap carries a bounded shift of
  **0.42 % of the crossing (~7 cents) above 300 Hz and ≤ 1.25 Hz below** — worst measured
  100 ms chunk **~14 cents at a 150 Hz crossing** (spurs at the −41 dBc analysis floor,
  < 0.1 dB envelope ripple) — and even a violent full-panel flick catches up in ~0.5 s of
  *continuous* motion after release (no timers, no delayed jump). Discrete jumps (> 1.5 oct
  target steps: automation snaps, preset-style changes) land within ~12 ms via the state-copied
  bank crossfade as before.
- **The only artifact-free fast alternative is linear-phase crossovers** (a moving linear-phase
  split at unit width is a pure delay — zero phase modulation by construction), which adds
  reported latency: a **Hard Stop** item (`ARCHITECTURE_REVIEW_GATE.md`) requiring an ADR, PDC/
  dry-alignment rework, and a project-owner decision. Not attempted here by policy.
- **Evidence [Verified]:** src/dsp/MultibandWidth.h (design rationale + measurements);
  tests/dsp_tests.cpp `testMultibandSplitDragNoPitchShift` (Test 29, grades the whole movement);
  ADR-0015 "v0.8.10 final decision"; CHANGELOG [0.8.10] + [0.8.11] (the slow-drag fix entry moved
  there in the maintainer-instructed consolidation). Severity **Low** (small bounded
  artifact, accepted product trade).

## KI-013 — macOS: release outside the window can still leave a control stuck "pressed"

- **Problem:** v0.8.12 reconciles stuck pressed/drag state against the real OS mouse-button state
  when a mouse-up lands outside the plugin window (CHANGELOG `[0.8.12]`). On macOS the mechanism is
  **inert**: JUCE 8.0.14's `ModifierKeys::getCurrentModifiersRealtime()` refreshes only *keyboard*
  modifiers there and returns the *cached* mouse-button flags (it never queries
  `[NSEvent pressedMouseButtons]`), so the "cached-down && realtime-up" gate can never fire. macOS
  behaviour is therefore unchanged from pre-0.8.12: a lost outside release can leave a control
  visually pressed until the cursor re-enters and the next real event resynchronizes.
- **Mitigating factor:** AppKit delivers the mouse-up to the window that captured the mouse-down,
  so lost releases are rare on macOS in the first place; recovery on cursor re-entry is intact.
- **Evidence [Verified]:** JUCE 8.0.14 (FetchContent) `juce_NSViewComponentPeer_mac.mm` (realtime query
  returns cached mouse flags; **re-verified unchanged in JUCE 9.0.0** during the ADR-0022 bump and
  again in **9.0.1** during ADR-0026, where the file is byte-identical — still
  keyboard-modifiers-only); `worklogs/MOUSE_RELEASE_STATE_FIX_v0.8.12.md` §2 (platform caveat);
  CHANGELOG `[0.8.12]` ("Effective on Windows and Linux"). Fixable only via a JUCE-side change or a
  platform-specific `pressedMouseButtons` query (would need its own review). Severity **Low**,
  external (JUCE platform implementation).

*(KI-014 "The macOS AU is shipped but never validated automatically" — RESOLVED in v0.9.4: the
macOS CI job now runs the full pluginval release gate against the AU as well as the VST3, at the
same strictness, in both modes, three consecutive passes each, and against the **packaged**
bundle — the stripped, ad-hoc-signed tree the artifact ships from. The registry problem the entry
described is handled the way it predicted: the `.component` is installed into
`~/Library/Audio/Plug-Ins/Components/` and `AudioComponentRegistrar` restarted before validation,
because macOS resolves Audio Units through the AudioComponent registry and a `.component` outside
one can report zero plugin types however correct it is. The gate uses **pluginval**, not `auval`;
that residual is scope, not a coverage gap, and is recorded in `docs/procedures/CI_CD.md`
§"Known coverage limits" and as the narrowed RH-F3. See `docs/procedures/TESTING.md`
§"Gaps in the automated coverage". ID retired, never reused.)*

## KI-015 — Anamorph declares no licence of its own
The repository root has **no `LICENSE` file**, and neither installer presents an end-user licence
agreement, so the terms under which Anamorph's own source and binaries are offered are
undeclared. `EULA.md` exists only as an **unapproved draft** — explicitly not in force, not
shipped, with every open owner/legal decision marked — so it does not close this issue. The owner has stated the product model (2026-07-26): **closed-source commercial**.
JUCE 9 modules are dual-licensed **AGPLv3 or commercial**, and a closed-source distribution
cannot use the AGPLv3 arm — so the commercial JUCE tier must be obtained before commercial
distribution, alongside Anamorph's own LICENSE/EULA text.
- **Scope:** owner/legal decision. No code change can close it, and this repository deliberately
  makes no determination. Third-party *attribution* — a separate obligation — **is** discharged:
  `NOTICE` and `THIRD_PARTY_LICENSES.md` accompany every download as version-named release-page
  assets, which since 2026-07-26 are the sole carrier of the IJG acknowledgement.
- **Evidence [Verified]:** no `LICENSE`/`COPYING` at the repository root; `THIRD_PARTY_LICENSES.md`
  §"Open licensing decisions"; JUCE `LICENSE.md` in the pinned tree (dual licence);
  `docs/architecture/RELEASE_HARDENING_PLAN.md` RH-R11 / RH-F1.
- **Related:** the Steinberg VST 3 review (RH-F2) is a separate owner item — the SDK code in the
  pinned JUCE tree is MIT, but VST trademark use and plug-in distribution terms are governed
  separately.
- **Index:** all open owner/legal decisions, including this one, are listed in
  `docs/COMMERCIAL_STATUS.md` §4.

## KI-016 — Sessions saved before 0.9.1 report Anamorph as missing
v0.9.1 changed the **manufacturer code** from `Anmf` to `RTec` (**ADR-0023**) — the vendor
identifier every RollyTech plug-in shares. That code is the **AU component's manufacturer field**,
and it feeds the **VST3 class UID** (JUCE derives the UID from the manufacturer code, the plug-in
code and the plug-in name), so a host that recorded the pre-0.9.1 identity in a session cannot
match the 0.9.1 build and reports Anamorph as **missing** rather than loading it.

This is a *deliberate, one-time* change, taken before the first release tag because a manufacturer
code only becomes more expensive to change. It is **not** a defect and will not recur: the code is
frozen from 0.9.1 onward (`docs/policies/COMPATIBILITY_POLICY.md` — the ADR-0023 exception is the
only one granted).

- **What a tester sees:** the plug-in slot in an old session shows as missing/unavailable; on
  macOS, Logic and GarageBand no longer list the old AU until the new one is scanned.
- **Workaround:** re-insert Anamorph on the track and re-load the preset (or re-dial the
  settings). If the plug-in does not appear at all, force a plug-in rescan and clear any
  failed-scan/blocklist entry.
- **What is NOT affected:** saved parameter state, preset files (`.anamorph`), the parameter IDs,
  the serialization schema, the install locations, and the audio — the 0.9.1 DSP is bit-identical
  to 0.9.0. Only the *identity the host files the plug-in under* changed. Automation lanes are
  lost only because they belong to the instance the host can no longer find, not because
  automation itself changed.
- **macOS AU validation** is now `auval -v aufx Anmr RTec`.
- **Why no automated gate caught it:** nothing in the repository's validation observes plug-in
  identity — the DSP and state suites do not compile it into an assertion, and pluginval validates
  whatever UID the built VST3 carries. That is precisely why the change is recorded here and in
  ADR-0023 rather than left to surface itself.
- **Evidence [Verified (code) / Verified — manual (new identity) / Unverified (old-session
  effect)]:** CMakeLists.txt:395 (`PLUGIN_MANUFACTURER_CODE RTec`); ADR-0023 (`Accepted`
  2026-07-30); CHANGELOG `[0.9.1]`. The **new** identity registering correctly was confirmed by the
  maintainer's Level-5 check on 2026-07-30 (host registration + `auval -v aufx Anmr RTec`) — a
  human sign-off, not headlessly reproducible. The **old-session** effect described above is
  derived from the mechanism (the host matches on the identity, so it cannot resolve a pre-0.9.1
  reference) and was **not** separately observed; it is stated as a consequence, not as a
  measurement.
- **Closure:** this entry stays for as long as pre-0.9.1 builds are in testers' hands; it is
  removed once no tester is carrying one, and is then recorded in `POSTMORTEMS.md` only if it
  actually cost someone work.

## KI-017 — macOS: holding a letter or digit in a text field does not auto-repeat
Reported for the **Save Preset** name field: holding a letter or a digit types the character once
and then stops, while punctuation/symbol keys repeat normally. It is **not specific to that
field** — the same applies to every text entry in the plug-in (the knob/slider value boxes), and
it is a **macOS text-input behaviour, not an Anamorph or JUCE defect**.

Mechanism, traced end to end in the pinned JUCE source. A focused `juce::TextEditor` is a
`juce::TextInputTarget`, so `ComponentPeer::findCurrentTextInputTarget()` returns it
(`juce_ComponentPeer.cpp:291-301`). Both `keyDown:` and `performKeyEquivalent:` then funnel into
`NSViewComponentPeer::sendEventToInputContextOrComponent`, whose **first** act is
`[inputContext handleEvent: ev]` (`juce_NSViewComponentPeer_mac.mm:1655-1662`); JUCE's own
`redirectKeyDown` / `TextEditor::keyPressed` runs only if the input context declines the event
(`:1667-1668`). Printable characters therefore arrive through AppKit's `insertText:` (`:2396-2435`),
which is exactly where macOS implements **press-and-hold** — JUCE supports it deliberately (its
comments at `:2409-2412` and `:2580` describe the accent popup). "Special" keys take the other
branch — `doCommandBySelector:` (`:2437-2467`) → `redirectKeyDown` → `TextEditor::keyPressed` — and
are therefore re-delivered on every OS repeat. Neither JUCE nor Anamorph contains any repeat logic
to compensate: for printable characters, auto-repeat is owned entirely by the OS.

Two AppKit behaviours sit exactly on that path and both produce "letters and digits dead, other
keys fine":
1. **A non-Roman input source** (Pinyin, Zhuyin, ABC-Extended) consumes `a`–`z` as composition
   keys and `0`–`9` as candidate selectors while passing punctuation straight through. This
   matches the reported set — letters **and** digits — exactly.
2. **`ApplePressAndHoldEnabled`** (default on) hands the accent-capable keys to the press-and-hold
   panel instead of the key-repeat generator. On its own it does not account for digits.

- **Affected platform:** macOS only. Windows (`juce_HWNDComponentPeer_windows.cpp`) and Linux
  (`juce_XWindowSystem_linux.cpp`) deliver every repeat as an ordinary key event and are
  structurally unaffected.
- **Which of the two it is, and the user-side workaround** — in this order, seconds each:
  1. Switch the macOS input source to plain **ABC / U.S.** (not Pinyin, Zhuyin or ABC-Extended),
     reopen Save Preset, hold a letter and a digit. Repeat returns ⇒ cause 1, external.
  2. `defaults write -g ApplePressAndHoldEnabled -bool false`, then log out and back in (or
     relaunch the host). Repeat returns ⇒ cause 2, external. This is a **system** preference,
     deliberately left to the user.
  3. Pin the failing set precisely. "letters + digits fail, punctuation repeats" and "every
     printable character fails, Backspace/arrows repeat" point at different mechanisms; the second
     is exactly the `insertText:` vs `doCommandBySelector:` split above.
- **Confirming it is the OS, not the plug-in** (either check settles it): type into one of the
  plug-in's knob value boxes — the same no-repeat applies, because it is the same JUCE text-input
  path; or rename a track in the DAW itself — its own fields behave identically while the setting
  is on. The sibling plug-in **Anabasis** lands on the identical path (its save field is a plain
  `juce::TextEditor` with the same setup), so it must show the same behaviour on the same machine;
  if it does **not**, this attribution is wrong — see Closure.
- **Why no fix is attempted here.** Three routes were examined and all three are worse than the
  symptom. (1) Having the plug-in write `ApplePressAndHoldEnabled` itself: `NSUserDefaults` is
  **process-wide**, so a plug-in doing that silently changes the host's own text fields and every
  other plug-in in that process — state Anamorph does not own. (2) Bypassing the input context for
  printable keys: that is where dead keys, accents and CJK/IME composition live, so it would trade
  a repeat annoyance for broken non-Latin input — and it is a JUCE source patch, i.e. a Build
  System change under `ARCHITECTURE_REVIEW_GATE.md` + `DEPENDENCY_POLICY.md` (the pin is an
  immutable SHA, ADR-0022). (3) Synthesising the repeat from a `Timer` inside a `TextEditor`
  subclass: it would double-type for every user who has press-and-hold disabled, and it cannot
  read the user's System Settings repeat delay/rate.
- **Relationship to KI-009:** different mechanism, same control. KI-009 is a *focus* problem
  (REAPER-specific, the field stops receiving keys at all); this is a *repeat* problem that occurs
  with focus working correctly, in every host, on macOS.
- **Evidence [Verified (code path) / Unverified (the macOS-side attribution)]:**
  src/PluginEditor.cpp:374-384 (the field), src/PluginEditor.cpp:2053-2103 (show + focus);
  `juce_NSViewComponentPeer_mac.mm:1655-1668, 2396-2435`; `juce_ComponentPeer.cpp:291-301`. The
  JUCE trace is verified line by line against the pinned commit; the attribution to the macOS
  text-input layer is inferred from the symptom signature (letters **and** digits suppressed,
  symbols unaffected) and has **not** been observed on hardware. Steps 1–2 above are what move it
  to Verified and decide which of the two causes it is. Everything inside the plug-in was
  eliminated by inspection: the `focusSaveNameField` retry is bounded at 4 × 50 ms and only runs
  from `showSavePreset(true)`; `setSelectAllWhenFocused` fires once per focus gain, not per
  keystroke; the 24 Hz timer and the VBlank attachment only repaint; the UI-scale
  `AffineTransform` is not consulted by key routing; and both `getCurrentModifiersRealtime()`
  call sites are gated behind a held mouse button.
- **Closure:** if the DAW's own text fields repeat while the plug-in's do not — or if Anabasis
  repeats on the same machine — this attribution is wrong and the entry is re-opened as a
  host-side first-responder investigation (the same class as KI-009): `performKeyEquivalent:`
  reaches the JUCE view through the view hierarchy even when it is not first responder, whereas
  plain `keyDown:` requires first-responder status, so a host that reclaims first responder after
  the first key-down would drop exactly the printable-character repeats while special keys keep
  arriving.

## KI-018 — a fast click right after dismissing a pop-up can register as a double-click
Since v0.9.3 the click that dismisses a pop-up is consumed by the editor's `PopupShield` and reaches
no control — that part works. What it cannot do is un-count that click. If the user clicks again
**within the double-click timeout and close to the same spot**, the control that finally receives it
sees `getNumberOfMultipleClicks() == 2` and JUCE calls its `mouseDoubleClick`. On a knob that means a
reset-to-default or the numeric entry box, from what the user experienced as a first click.

**Mechanism, from the pinned JUCE 9.0.1.** The multi-click run lives on the *input source*, not on a
component. `MouseInputSourceImpl::registerMouseDown` records only position, time, buttons, touch flag
and peer id (`juce_MouseInputSourceImpl.h:581-599`), and `canBePartOfMultipleClickWith` (`:565`)
compares exactly those — the **target component is not part of the comparison**. Registration happens
in the event dispatch (`:238`) before any component is consulted, so a click the shield swallows is
already in the run by the time we could react to it.

**Why it is not fixed.** There is no in-bounds lever:

- `MouseInputSource` exposes **no** reset/clear API for the run — only getters
  (`juce_MouseInputSource.h:175`), and `mouseDowns[]` is private to the impl.
- `MouseEvent::setDoubleClickTimeout` (`juce_MouseEvent.h:374`) is **static and process-global**.
  Writing it from a plug-in would change double-click behaviour for the host and every other plug-in
  in that process — the same objection that ruled out writing `ApplePressAndHoldEnabled` in
  **KI-017**, and out of bounds for the same reason.
- Keeping the shield up for the whole double-click timeout would swallow the legitimate second
  click, contradicting the interaction contract the shield exists to enforce.
- Guarding inside each control's `mouseDoubleClick` re-creates the per-control approach the shield
  replaced: every knob, label, the imager and the A/B control would need it, and a control added
  later would silently miss it.
- Patching JUCE is a Build System change under `ARCHITECTURE_REVIEW_GATE.md` + `DEPENDENCY_POLICY.md`
  (the pin is an immutable SHA, ADR-0022), for a Low-severity cosmetic race.

- **What a tester sees:** with a drop-down or right-click menu open, click outside it to dismiss and
  then click a knob again quickly — the knob may jump to its default or open its value box instead of
  registering as a plain click.
- **Workaround:** pause briefly (past the system double-click time) or move the pointer more than a
  few pixels before the next click; either breaks the run.
- **What is NOT affected:** the dismissal guarantee itself. The first click still reaches no control,
  no parameter moves from it, and nothing is written to state. Severity **Low**.
- **What would close it:** a JUCE API to reset the multi-click run on an input source, or a
  per-source (rather than process-global) double-click timeout. Both are upstream changes.

## KI-019 — Linux/X11: an open pop-up is not dismissed when you switch application
Since v0.9.3 the editor cancels an open drop-down or right-click menu when the plug-in window is
hidden, when the editor is destroyed, or when the user switches to another application. **The third
of those does not happen on Linux.** The first two work normally there.

**Mechanism, from the pinned JUCE 9.0.1.** The app-switch branch asks
`juce::Process::isForegroundProcess()`. On Linux that is
`LinuxComponentPeer::isActiveApplication` (`juce_Windowing_linux.cpp:687`), a static initialised to
`false` (`:678`) and assigned **only** `true`, from `grabFocus()` on a successful X11 focus grab
(`:326-327`). Nothing ever sets it back. So it is a write-once latch rather than a live foreground
state: before the latch the call is always `false`, after it always `true`, and either way the value
never *changes*, which is what "switched away" requires.

**Why that is the safe direction.** The editor probes the call at the moment a pop-up opens and only
treats a later `false` as an app switch if it read `true` then. On Linux those two readings are always
equal, so the branch is inert — it can never cancel a menu spuriously. The failure mode is a missing
dismissal, not an unwanted one.

- **What a tester sees:** on Linux, Alt-Tab away with a drop-down or right-click menu open **and the
  pointer resting on a menu item**, and the menu stays on screen. With the pointer anywhere else JUCE
  dismisses it itself, so the visible case is narrow.
- **Workaround:** move the pointer off the menu before switching, or click the menu away first.
- **What is NOT affected:** hiding the plug-in window and closing it both still cancel the menu on
  Linux, because those are decided by `isShowing()`, which is accurate on every platform.
- **What would close it:** an upstream Linux `isActiveApplication` that follows X11 focus-out as well
  as focus-in, or JUCE exposing `detail::WindowingHelpers::isEmbeddedInForegroundProcess`. Both are
  JUCE changes; re-deriving X11 focus ownership inside the editor would put platform-specific window
  code in a component that has none.

## KI-020 — the pop-up dismissal guarantee is per-instance, not per-process
Since v0.9.3 the click that dismisses a pop-up reaches no control — **in the editor that owns the
pop-up**. With two or more Anamorph editors visible at once it does not hold across them.

**Mechanism.** JUCE modality is process-global. A menu open in instance A blocks *every* component in
the process, B's included, so a click on B takes the same `internalMouseDown` path
(`juce_Component.cpp:2507-2544`): A's menu is dismissed and the same mouse-down is then re-delivered
to B's control. B's `PopupShield` is not raised, because it is raised only from hooks owned by B's own
look-and-feels and by B's own `presetMenusOpen` — B has no way to know A opened a menu. So B's knob,
A/B control or Multiband split acts on a click the user meant only as a dismissal.

**Not introduced by the shield.** This is the pre-0.9.3 behaviour, unchanged: before the shield the
click reached a control in *every* instance, including the owning one. 0.9.3 removed the same-instance
half of the problem and left the cross-instance half exactly as it was.

**Why it is not fixed.** Closing it means one editor observing another's modal state. The available
routes are all out of bounds or worse than the defect: a process-wide registry shared between editor
instances is exactly the shared-static ownership INC-010 rejected (static-destruction order at DLL
unload); raising every instance's shield from any instance's menu would make an unrelated editor
unclickable whenever a neighbour opens a drop-down; and `PopupMenu::dismissAllActiveMenus()` is the
process-global call INC-010 ruled out for the same reason.

- **What a tester sees:** two Anamorph instances side by side; open a drop-down in one and click a
  control in the other — the drop-down closes *and* that control responds.
- **Workaround:** dismiss the menu with a click inside its own editor first.
- **What is NOT affected:** everything within one editor. The single-instance case — which is what the
  0.9.3 CHANGELOG entry describes — is unchanged and complete. Severity **Low**: it needs two editors
  open simultaneously and a click aimed at the second one while the first has a menu up.
- **What would close it:** a JUCE signal identifying which component tree a modal dismissal belonged
  to, which would let a non-owning editor recognise the re-delivered click without shared state.

## KI-021 — a Linux per-user install does not displace an existing system-wide one
0.9.3 made the per-user location (`~/.vst3` + `~/.local/bin`) the installer's **default**, where
every earlier build installed system-wide. A user who ran `sudo ./install.sh` under 0.9.2 and then
takes the new default ends up with **both**:

| | VST3 | Standalone |
|---|---|---|
| new, per-user | `~/.vst3/Anamorph.vst3` | `~/.local/bin/Anamorph` |
| older, system-wide | `/usr/lib/vst3/Anamorph.vst3` | `/usr/local/bin/Anamorph` |

Both VST3 paths are default scan paths in REAPER, Bitwig and Ardour, so the host lists Anamorph
twice, and **which one it loads depends on its scan order** — the update can appear not to have
applied at all.

- **Why it is not fixed in code:** removing `/usr/lib/vst3/Anamorph.vst3` requires root, and the
  whole point of the per-user mode is that it never asks for it. Silently escalating from the
  no-root path would be worse than the duplication.
- **What the installer does instead:** after a per-user install it **detects** the system-wide copy
  (a plain `test -e`, no elevation) and prints what is still installed there plus the one command
  that removes it, `sudo ./uninstall.sh`.
- **What a tester sees:** two Anamorph entries after a rescan, or an "old" version loading after an
  apparently successful update.
- **Workaround:** `sudo ./uninstall.sh` (removes only the system-wide copy), then rescan.
- **Evidence [Verified]:** `packaging/linux/install.sh` (per-user summary block);
  `docs/procedures/PACKAGING.md` §Installers; `packaging/linux/INSTALL.txt` and
  `docs/user/INSTALLATION.md` troubleshooting entries.

## KI-022 — macOS: a user-moved copy is left behind, so two copies can coexist
Since 0.9.3 every macOS component is built **non-relocatable** (`BundleIsRelocatable=false`), which
is the fix for **INC-012** — relocation is what let the installer write over a copy the user had
moved (the Trash included) and report success while the standard location stayed empty.

The trade is the case relocation was designed for: a user who *deliberately* keeps `Anamorph.app`
in `~/Applications`, or a plug-in under `~/Library/Audio/Plug-Ins/…`, now gets a second copy at the
standard location on the next install. Both carry the same bundle identifier, and a host scanning
user *and* system plug-in folders lists Anamorph twice.

- **Why it is not fixed in code:** following a moved copy is precisely the behaviour INC-012 removed.
  A destination that depends on where a previous install ended up cannot be made reliable; a fixed
  destination can. The duplication is visible and removable, the INC-012 failure was neither.
- **What a tester sees:** after an upgrade, a copy at the standard location *and* the one they had
  moved; possibly two entries in a host that scans both folders.
- **Workaround:** delete whichever copy is unwanted. Only shows up on an **upgrade** of a
  hand-moved install, which no automated gate exercises.
- **Evidence [Verified]:** `packaging/macos/build-pkg.sh` (`build_component`);
  `docs/procedures/PACKAGING.md` §"macOS reinstall behaviour (idempotency)" §Not chased;
  `packaging/macos/INSTALL.txt`; INC-012 in `docs/POSTMORTEMS.md`.

## KI-023 — the Linux build does not load on distributions older than its glibc floor

The shipped Linux VST3 and Standalone are linked on the CI runner image, and a Linux binary records
the oldest glibc/libstdc++ version that provides each imported symbol. The **maximum** of those is
the oldest distribution the artifact can load on at all: below it the dynamic loader refuses with
`version 'GLIBC_x.y' not found`, before any of this project's code runs — the plug-in does not
appear in the host, rather than appearing and misbehaving.

Measured 2026-08-18 on the artifact this repository ships: **GLIBC_2.38** and **GLIBCXX_3.4.31**,
i.e. Ubuntu 23.10+ / Debian 13+ and GCC 13+. **Ubuntu 22.04 LTS ships glibc 2.35, so the plug-in
does not load there.**

- **Why it is not fixed here:** the floor was never chosen. It is whatever `ubuntu-latest` happened
  to be when the binaries were linked, and it rose retroactively when that image moved to 24.04.
  Lowering it means building against an older toolchain or a sysroot — a release-topology decision
  (which image, which container, whether the release build stops sharing the CI image), not a CI
  tweak. That decision has not been taken.
- **What is fixed:** it is no longer invisible. `scripts/check-linux-abi.py` asserts the floor on
  every push, on the **stripped** bytes and as the `linux` job's last step, so the run that raises it
  is the run that fails instead of a user's DAW — while still producing the suites' results and the
  artifacts, because the trigger is a runner-image move rather than anything in this tree. Raising it is now deliberate: change the declared constant in
  the same change and name the systems it drops.
- **Evidence [Verified]:** scripts/check-linux-abi.py (the declared floor and the gate);
  `docs/architecture/COMPATIBILITY_MATRIX.md` §"Linux runtime ABI floor".

## KI-026 — the x86-64 builds do not run on pre-Haswell Intel / pre-Excavator AMD CPUs

Every shipped x86-64 binary is compiled for AVX2: from 0.9.5 the **Linux** binaries and the
**`x86_64` slice of the macOS universal build** at `-march=haswell` (ADR-0031), and the **Windows**
build at `/arch:AVX2` (ADR-0032). Either way the compiler may emit AVX2, FMA, BMI1/BMI2, F16C,
LZCNT and MOVBE anywhere in the image. On an Intel CPU older than **Haswell (2013)** or an AMD CPU older than
**Excavator (2015)**, the first such instruction raises `SIGILL` — an illegal-instruction fault
**inside the host process**. The DAW reports a crash, or simply disappears; it does not report an
incompatible plug-in.

This is the mirror image of KI-023 in one respect and its opposite in another: like the ABI floor it
is a hard requirement below which nothing this project wrote gets to run, and unlike the ABI floor it
was **chosen**, with a measured benefit (−17.2 % of the engine's instruction count) and no change to
a single output bit.

- **Why the plug-in cannot say so itself:** the fault can be raised by code the dynamic loader runs
  before any Anamorph entry point does — static initialisers are compiled under the same flags. A
  `__builtin_cpu_supports` check would have to live in a separately compiled baseline translation
  unit gating the entire plug-in, which is a different build design, not a message.
- **Who is affected in practice:** on **macOS**, only a Mac old enough to be running macOS
  10.13–10.15 (the deployment target is 10.13, and High Sierra reaches Macs back to 2009); every Mac
  that can run a newer macOS already exceeds the floor. On **Linux**, the declared glibc/libstdc++
  floor (KI-023) already implies a distribution far newer than 2013, so the binding constraint is the
  hardware rather than the distribution. On **Windows**, any x64 machine with a pre-2013 Intel /
  pre-2015 AMD CPU — Windows 10/11 install on such hardware, so the floor binds on hardware there
  too, surfacing as `STATUS_ILLEGAL_INSTRUCTION` (0xC000001D) at plug-in load. **Only Apple Silicon
  carries no such requirement.**
- **Recovery for someone affected:** none in-product. The requirement is stated in
  `docs/user/INSTALLATION.md` and `docs/user/USER_MANUAL.md` §2 ("What you need") so it is visible
  before install.
- **Evidence [Verified]:** CMakeLists.txt (the `AnamorphHardening` x86-64 baseline block, both
  branches); `docs/policies/COMPATIBILITY_POLICY.md` §"Runtime compatibility: the x86-64 ISA floor";
  `docs/architecture/design-decisions/ADR-0031-x86-64-isa-baseline.md`;
  `docs/architecture/design-decisions/ADR-0032-msvc-avx2-baseline.md` (the Windows half, with the
  per-push blocking A/B gate).


