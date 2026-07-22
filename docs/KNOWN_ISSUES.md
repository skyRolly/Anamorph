# KNOWN_ISSUES.md

**Only currently-existing, confirmed problems/limitations.** Resolved or historical issues belong
in `POSTMORTEMS.md`, not here. Each entry is evidence-backed (constraint C7). When an item is
fixed, remove it here and (if notable) add a `POSTMORTEMS.md` entry.

Verified against repository HEAD `64e87c4` (post-v0.8.12 content re-audit); version-synced to the
**v0.8.12 release** (changelog-dated 2026-07-22, PR #79 performance Wave 6 + PR #80 GUI interaction
fixes — one issue **added**: KI-013, the release-outside stuck-press reconcile is inert on macOS
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
| KI-002 | macOS artifacts not notarized (manual de-quarantine required) | Medium | Confirmed (distribution) |
| KI-003 | pluginval Linux editor tests crash (external host-side JUCE) | Low | Confirmed, mitigated/external |
| KI-004 | No automated DAW/host-compatibility testing | Medium | Confirmed (coverage gap) |
| KI-005 | No graphical installer (manual copy install) | Low | Confirmed (packaging) |
| KI-006 | Linux: tooltip rounded corners render an opaque black background instead of transparent | Low | Fix applied (LookAndFeel); Linux visual re-test pending |
| KI-007 | Windows: pluginval "Editor Automation" abnormally terminates (was hidden by a run-pluginval.ps1 false green) | Medium | False green closed; GL-drop cleared the crash (CI-confirmed); advancedMode-automation fix in place — no recurrence observed (green release gates recorded in HANDOVER Build Status, v0.8.9–v0.8.12) |
| KI-008 | Advanced-toggle one-frame tear in async-resize hosts (JUCE VST3 wrapper window-grant gap) | Low | Confirmed, external (JUCE wrapper + host); not fixable plugin-side without a JUCE change |
| KI-009 | REAPER: Save Preset text editor loses keyboard focus (Space hits transport; a click does not re-focus until the dialog is reopened) | Low | Reported, host-specific (REAPER); pending manual investigation |
| KI-010 | Typing a value into a knob/slider text box creates no Undo step (gesture-less edit path) | Low | Confirmed (code path); reported during the 0.8.10 Option-reset fix, not yet fixed |
| KI-011 | macOS Apple-Silicon-native: tooltip corners rendered an opaque white frame (TooltipWindow opacity contract) | Low | Fix applied (editor marks the TooltipWindow non-opaque on macOS); Apple Silicon visual re-test pending |
| KI-012 | Fast Multiband split drags carry a small controlled FM (~14 cents at a 150 Hz crossing, ~7 cents above 300 Hz, under the R(f) = 4·max(1, f/300) oct/s cap; normal drags track 1:1; a violent flick catches up in ~0.5 s of continuous motion; fast artifact-free tracking needs linear-phase splits = latency change) | Low | Documented limitation (ADR-0015 final + slow-drag fix); revisit only via Architecture Review |
| KI-013 | macOS: release outside the window can still leave a control stuck pressed (the v0.8.12 reconcile is inert there — JUCE's realtime query returns cached mouse-button state on macOS) | Low | Confirmed, external (JUCE platform implementation); recovery on cursor re-entry intact |

---

## KI-001 — Concurrent Multiband-Enable + discrete change cold-starts the bank during the duck
When `mbEnable` flips **simultaneously with another discrete change** (e.g. algorithm), the change
is deferred to the silent duck bottom, where `mbStructuralChange` (which still includes
`mbEnable`) resets the multiband bank and SoloMonitor — so the crossover bank cold-starts during
the fade-in instead of staying warm, partially defeating the 0.8.6 warm-bank design for that
specific case. The reset is **masked by the duck (inaudible)**, so there is no user-visible defect;
a stand-alone `mbEnable` toggle (the common case) is unaffected and stays warm.
- **Evidence [Verified]:** src/dsp/AnamorphEngine.cpp:676 (`mbStructuralChange` includes
  `pendingP.mbEnable != p.mbEnable`), :743 (reset on it). Raised in Devin review of PR #50
  (unresolved thread). See FUTURE_RISKS / ADR-0004 (warm-bank intent).
- **Possible resolution:** remove `mbEnable` from `mbStructuralChange` so a concurrent toggle fades
  out via `mbEnableBlend` while staying warm. This is a DSP state-transition change → requires an
  ADR + Architecture Review (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`); not done here.

## KI-002 — macOS artifacts not notarized
CI ad-hoc codesigns the macOS bundles but does **not** notarize them, so Gatekeeper quarantines
them after download and the user must run `xattr -dr com.apple.quarantine` before the DAW will load
them.
- **Evidence [Verified]:** .github/workflows/build.yml:495-498 (`codesign --sign -`, no notarization);
  packaging/macos/INSTALL.txt:4-10,30-33. See `docs/procedures/PACKAGING.md`.

## KI-003 — pluginval Linux editor tests crash (external)
The editor open/close tests can crash under pluginval on Linux due to a use-after-free in
**pluginval's own JUCE** X11 `XEmbedComponent` (`ConfigureNotify`→`callAsync` capturing a raw
pointer). It is **not a defect in this plugin** (the plugin already drops its OpenGL child window on
Linux, INC-006/ADR-0011) and is mitigated by a signal-only retry, but it cannot be fixed from this
repository.
- **Evidence [Verified]:** scripts/run-pluginval.sh:63-96 (`run_one_pass`, signal-only retry); ADR-0011. See FUTURE_RISKS RISK-004.

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
  passes up to four times (src/PluginEditor.cpp:1387-1406; declared PluginEditor.h). This shipped in
  the v0.8.9 CHANGELOG "Fixed" entry ("The Save Preset name field reliably receives typing — Space
  included") and was **validated headless end-to-end**, i.e. against the JUCE wrapper, not against
  REAPER. The retry loop runs **only on dialog open** (`showSavePreset(true)` → `focusSaveNameField(4)`);
  there is **no focus re-acquisition after a later focus loss** — no `focusLost` handler,
  `mouseDown`-grab, or `setMouseClickGrabsKeyboardFocus` override on `saveNameEditor` (repo-wide:
  the only focus calls are PluginEditor.cpp:1481/:1496-1503 (`focusSaveNameField`) and the unrelated SpectrumImager freq editor).
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
  (`setOpaque (true)` in its constructor, JUCE 8.0.14 juce_TooltipWindow.cpp:42) while
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
  returns cached mouse flags); `worklogs/MOUSE_RELEASE_STATE_FIX_v0.8.12.md` §2 (platform caveat);
  CHANGELOG `[0.8.12]` ("Effective on Windows and Linux"). Fixable only via a JUCE-side change or a
  platform-specific `pressedMouseButtons` query (would need its own review). Severity **Low**,
  external (JUCE platform implementation).
