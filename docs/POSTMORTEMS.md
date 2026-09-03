# POSTMORTEMS.md

Incident review library. Every incident is reconstructed from **repository evidence only**
(commits, the CHANGELOG, tests, current code) — no invented events. Dates are the **fix commit**
dates (Verified from git); affected versions are from the CHANGELOG / commit history (Partially
Verified, as the repository has **no git tags**).

Template per incident: Problem · Symptom · Root cause · Defect-formation mechanism (evidence
required) · Fix · Why this fix · Prevention.

---

## INC-001 — Band Solo "ghost" / engage tick
- **Date:** 2026-06-23 (fix commit `6d2023b`) · **Affected version:** ≤0.8.0, fixed 0.8.1 · **Severity:** Medium
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.1]; commit 6d2023b. **[Verified]:** test `testSoloNoGhostInSilence`; ADR-0004; src/dsp/SoloMonitor.cpp.

- **Problem:** Toggling Band Solo emitted a transient even when the DAW was stopped/paused/fed a zero buffer.
- **Symptom:** An audible tick on solo engage; a "ghost signal" when toggling solo during silence.
- **Root cause:** Band Solo was applied via an output **duck** (a hard switch), which could push a transient through even on silence.
- **Defect mechanism:** The duck-to-silence switch swapped solo state instantaneously; with no continuous crossfade, the band-pass insert/remove stepped the output.
- **Fix:** Made Band Solo a **post-everything warm monitor** with smoothed per-band gains that always run (no duck) — `SoloMonitor`.
- **Why this fix:** A warm, always-running crossfade morphs solo↔passthrough with zero-slope seams, so a toggle can't emit a transient even from silence; `mask==0` settles bit-exact.
- **Prevention:** Regression test `testSoloNoGhostInSilence`; the design is recorded in ADR-0004.

## INC-002 — Level Match ratchet / Mix=100% slam
- **Date:** 2026-06-23 (`6d2023b`) · **Affected version:** ≤0.8.0, fixed 0.8.1 · **Severity:** Medium
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.1]. **[Verified]:** tests `testLevelMatchNoRatchet`, `testLevelMatchMixCouplingNoSlam`; ADR-0007; src/dsp/LoudnessMatch.cpp.

- **Problem:** The loudness-match gain drifted/ratcheted toward the −24 dB floor, and raising Mix with Drive cranked slammed loud.
- **Symptom:** Cranking Drive up/down/up while paused ratcheted the gain down; Mix→100% produced a loudness slam; bias away from unity.
- **Root cause:** The match was an accumulating estimate that path-depended on parameter motion and ignored the Drive×Mix coupling.
- **Defect mechanism:** An incremental/accumulator predict had no absolute anchor, so repeated parameter sweeps compounded; Mix wasn't fed to the predictor.
- **Fix:** Reworked into **Measure (BS.1770, holds on silence) + absolute Predict** (a pure function of Drive and Mix, floor-only), with a silence→audio edge snap.
- **Why this fix:** An absolute, non-accumulating estimate can't ratchet or path-depend; feeding Mix pre-ducks the slam; the edge snap compensates the first audible block.
- **Prevention:** Tests `testLevelMatchNoRatchet`, `testLevelMatchMixCouplingNoSlam`, `testLevelMatchSilenceFreeze`, `testLevelMatchUnity`; ADR-0007.

## INC-003 — Multiband crossover automation explosion ("+600 dB")
- **Date:** 2026-06-27 (`f259a80`) · **Affected version:** ≤0.8.1, fixed 0.8.2 · **Severity:** Critical
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.2]. **[Verified]:** test `testCrossoverAutomationSafe`; ADR-0009; src/dsp/MultibandWidth.cpp:96-102 (Nyquist clamp).

- **Problem:** Automating a split toward Nyquist (4 bands crowded high) made the DSP blow up.
- **Symptom:** A "+600 dB" burst that stuck one channel and killed the other.
- **Root cause:** The ordered crossover separation could push a cutoff **above Nyquist**, where the Linkwitz-Riley coefficients go non-finite.
- **Defect mechanism:** The 1.1× ordering enforcement was applied without a Nyquist ceiling, so separation lifted the top cutoff past `0.45·sr` → LR coeffs → Inf.
- **Fix:** Clamp every crossover Nyquist-safe `[20, 0.45·sr]` **before** ordering, then re-clamp **top-down** so separation can never exceed the ceiling. Added an engine-wide NaN/Inf self-heal.
- **Why this fix:** Fixes the instability at the source (the coefficient domain), not by limiting amplitude — dynamics/headroom preserved (no clipper).
- **Prevention:** Test `testCrossoverAutomationSafe`; ADR-0009; the clamp is shared by Multiband/Solo/dry-align banks.

## INC-004 — Meter NaN-latch (bright bar vanished)
- **Date:** 2026-06-27 (`f259a80`) · **Affected version:** ≤0.8.1, fixed 0.8.2 · **Severity:** Medium
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.2]. **[Verified]:** test `testMeterRecoversFromNaN`; src/dsp/LevelMeters.h:110-111 (finite clamp), :179 (`sanitize`).

- **Problem:** A single non-finite sample permanently latched a meter envelope at NaN.
- **Symptom:** The bright (RMS) meter bar vanished and never returned.
- **Root cause:** The envelope follower propagated a NaN indefinitely (NaN compares poison the running value).
- **Defect mechanism:** No per-sample finite guard before the envelope; once `env = NaN`, every subsequent `max/lerp` stayed NaN.
- **Fix:** Per-sample finite clamp + `sanitize()` that flushes any non-finite envelope back to its floor.
- **Why this fix:** Guarantees the meter always recovers when finite audio returns, independent of the upstream cause.
- **Prevention:** Test `testMeterRecoversFromNaN`; documented in DSP_ALGORITHMS / ADR-0009.

## INC-005 — Bypass click + stale-audio burst
- **Date:** 2026-06-27 (`3686d12`) · **Affected version:** ≤0.8.2, fixed 0.8.3 · **Severity:** Medium
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.3]. **[Verified]:** tests `testBypassCrossfadeClickFree`, `testBypassToggleRobust`, `testLevelMatchRunsInBypass`; ADR-0004.

- **Problem:** Toggling Bypass clicked, stopped Level Match, and could replay a stale fragment.
- **Symptom:** A click on bypass toggle; Level Match froze while bypassed; a filter/oversampler burst as the duck lifted.
- **Root cause:** Bypass was a discrete ducked switch that gated the whole chain (including analysis) and left stale state.
- **Defect mechanism:** Muting the chain stopped the analysis path and left delay-line/oversampler contents that replayed on re-engage.
- **Fix:** Bypass became a **click-free output crossfade** to the delay-aligned RAW input; the chain + analysis always run; stateful nodes cleared at the duck bottom for the structural cases.
- **Why this fix:** Crossfading the output (not muting) keeps analysis live and is bit-exact at the endpoints; no stale state can re-enter.
- **Prevention:** Tests above; ADR-0004; confirmed no output clipper.

## INC-006 — Linux editor-automation segfault (OpenGL/X11 UAF)
- **Date:** 2026-06-28 (`c924ff8`) · **Affected version:** ≤0.8.4, fixed 0.8.5 · **Severity:** High (crash)
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.5]. **[Verified]:** commit c924ff8; ADR-0011; src/PluginEditor.cpp:277 (GL-attach platform gate); scripts/run-pluginval.sh:147-198.

- **Problem:** Rapid editor open/close on Linux crashed (pluginval "Editor Automation" and real Linux DAWs).
- **Symptom:** A use-after-free segfault during editor teardown.
- **Root cause:** Attaching a `juce::OpenGLContext` on X11 adds an embedded child window whose `ConfigureNotify` makes the host's `XEmbedComponent` post an async lambda capturing a raw `this`, which can fire after the editor is destroyed.
- **Defect mechanism:** The UAF is in JUCE's **host-side** X11 embedding; the plugin's GL child window generated the `ConfigureNotify` traffic that triggered it.
- **Fix:** Skip the GL attach on Linux/BSD (`#if ! (JUCE_LINUX || JUCE_BSD)`) — render CPU-side (visually identical); release the VBlank callback before detaching in the destructor; add a signal-only pluginval retry.
- **Why this fix:** Removing the GL child window cuts the `ConfigureNotify` traffic at its source; the crash lives in pluginval's own JUCE and cannot be fixed from this repo, so the retry mitigates the residual flake.
- **Prevention:** ADR-0011; `run-pluginval.sh` signal-only retry; see KNOWN_ISSUES (residual external flake).

## INC-007 — Multiband Enable mute/dropout
- **Date:** 2026-06-28 (`10fbfa0`) · **Affected version:** ≤0.8.5, fixed 0.8.6 · **Severity:** Medium
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.6]. **[Verified]:** test `testMultibandEnableCrossfadeClickFree`; ADR-0004; src/dsp/AnamorphEngine.cpp:124-125, :513, :920-921 (`mbEnableBlend` crossfade).

- **Problem:** Toggling Multiband Enable briefly muted/dropped the output.
- **Symptom:** A momentary dropout on enable/disable.
- **Root cause:** Multiband Enable was in the discrete-change set, routing through the duck-to-silence switch machine.
- **Defect mechanism:** The duck muted the output to swap the structural state; for a simple on/off this was an unnecessary mute.
- **Fix:** Made it a ~12 ms **output crossfade** (`mbEnableBlend`) between the multiband result and the pre-multiband signal, keeping the crossover bank **warm** across the toggle (reset only while the blend is ~0).
- **Why this fix:** A warm-bank crossfade is bit-exact at the endpoints and never mutes; a band-**count** change still ducks (a true structural rewire).
- **Prevention:** Test `testMultibandEnableCrossfadeClickFree`; ADR-0004. (See KNOWN_ISSUES KI-001 for a residual concurrent-change edge case.)

## INC-008 — Alt/Option-click reset animation regression
- **Date:** 2026-06-28 (`10fbfa0`) · **Affected version:** ≤0.8.5, fixed 0.8.6 · **Severity:** Low (GUI)
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.6]; commit 10fbfa0. **[Verified]:** src/PluginEditor.cpp / src/gui/LookAndFeel.cpp `resetSweep` handling.

- **Problem:** Alt/Option-click knob reset snapped instead of animating (double-click still animated).
- **Symptom:** No eased travel on Alt/Option-click reset.
- **Root cause:** A reset is itself a mouse-down event, so the button stayed physically held; the position-easing snapped to target whenever a button was down.
- **Defect mechanism:** The "held button → snap" rule didn't distinguish a reset gesture from a drag; double-click only escaped by releasing within one vblank (a race).
- **Fix:** A short `resetSweep` flag (set only while animations are on) opts the eased travel out of the button-held snap, for both LookAndFeel draw paths and the easing step.
- **Why this fix:** Distinguishes a reset from a drag without changing drag behaviour; animations-off still snaps exactly as before.
- **Prevention:** Documented behaviour; (no DSP test — GUI animation, validated manually).

## INC-009 — Band Solo + Multiband Enable click
- **Date:** 2026-06-28 (`6a24b82`) · **Affected version:** 0.8.6, fixed 0.8.7 · **Severity:** Medium
- **Evidence [Verified]:** CHANGELOG.md [0.8.7]; commit 6a24b82; test `testSoloMultibandEnableClickFree`; src/dsp/AnamorphEngine.cpp:1423 (`soloMonitor.process`, every block); ADR-0004.

- **Problem:** With a Band Solo active, toggling Multiband Enable clicked on both edges (a regression introduced by INC-007's 0.8.6 change).
- **Symptom:** An audible click (amplitude + phase step) on both enable and disable edges, only when a band was soloed.
- **Root cause:** The SoloMonitor call was hard-gated `if (p.mbEnable) soloMonitor.process(...)`. When 0.8.6 made `mbEnable` a continuous (un-ducked) toggle, the gate started/stopped the monitor in one block while its `passGain`/`bandGain` crossfade was frozen at the soloed target — inserting/removing the whole band-pass in a single sample.
- **Defect mechanism (Verified):** `SoloMonitor` is click-free *only* because its crossfade advances every block; gating the call on the instantaneously-flipping `mbEnable` bypassed that crossfade. The `mbEnableBlend` output crossfade is at an earlier stage and is a no-op at default unit widths, so it could not mask the step.
- **Fix:** Run the monitor **every block**, masking the solo by `mbEnable`: `soloMonitor.process(L, R, p.mbEnable ? p.mbSolo : 0, n)`.
- **Why this fix:** Restores the monitor's documented every-block invariant; it morphs solo↔passthrough over its own ~12 ms ramp; at `mask 0` the settled monitor is a bit-exact passthrough, so steady-state and the `mbSolo` parameter are untouched.
- **Prevention:** Regression test `testSoloMultibandEnableClickFree` (worst single-sample step 0.31 on the old code → 0.015 on the fix); ADR-0004 records the "must run every block" invariant.

## INC-010 — Preset drop-down outlived the editor (dangling LookAndFeel + callback)
- **Date:** 2026-08-07 (PR #100) · **Affected version:** ≤0.9.1, fixed 0.9.2 · **Severity:** High (host crash)
- **Evidence [Verified (code) / Reported (crash)]:** CHANGELOG.md [0.9.2]; PR #100; src/PluginEditor.cpp `showPresetMenu`; JUCE 9.0.0 `juce_PopupMenu.cpp` MenuWindow ctor (`setLookAndFeel` then `pc->addChildComponent`).

- **Problem:** With the preset drop-down open, closing the plug-in window — or switching to another plug-in — left the menu on screen; hovering it lost the custom item styling, and clicking any item took down the plug-in and/or the host.
- **Symptom:** A menu floating with no window behind it, unstyled on hover, then a crash on click. Reported by the maintainer; never filed as a Known Issue.
- **Root cause:** with no parent component, `juce::PopupMenu::MenuWindow` is an independent, always-on-top **desktop** window owned by the process-global `ModalComponentManager`; the editor holds no reference to it and is never consulted when it dies. Its `showMenuAsync` callback captured a raw `this`.
- **Defect mechanism (Verified from the pinned JUCE 9.0.0 source):** three distinct facts, only one of which is the crash.
  1. **The leftover menu.** `MenuWindow::windowIsStillValid()` exists to dismiss exactly this case — it fires when `componentAttachedTo != options.getTargetComponent()`. But both are `WeakReference<Component>` to `presetName`, an editor member: on the editor's death they null in the *same instant*, the comparison is false, and no dismissal happens.
  2. **The lost styling — NOT a use-after-free.** The MenuWindow **copies** the menu's look-and-feel into its own `Component::lookAndFeel` slot (`juce_PopupMenu.cpp:366`), and that slot is a `WeakReference` (`juce_Component.h`), so it nulled rather than dangled; `Component::getLookAndFeel()` then falls back to the default `LookAndFeel_V4`. (The `PopupMenu` itself is a stack local in `showPresetMenu` and is long gone by then — it cannot be the reference that outlives the editor.) Cosmetic, and it only trips the `~LookAndFeel` debug assertion about live weak references.
  3. **The crash.** Clicking an item runs `triggerCurrentlyHighlightedItem` → `dismissMenu` → `exitModalState (resultID)` with a non-zero result, and the user lambda dereferences the freed editor and the freed processor reference it holds. `withDeletionCheck` was never used, so JUCE's `resultID = 0` escape hatch was inert.
- **Fix:** `Options().withParentComponent (this)` — JUCE then does `pc->addChildComponent (this)` instead of `addToDesktop`, so the MenuWindow is a **child**: clipped to the editor, stacked with it (it goes behind our window instead of floating over the next plug-in), and cancelled with result 0 by `ModalComponentManager`'s `ComponentMovementWatcher` when the editor is destroyed **or hidden**. `Component::getLookAndFeel()` walks up to the editor's own `lnf`, so `setLookAndFeel` is dropped entirely. The callback captures a `juce::Component::SafePointer` — **not redundant**: that cancel is asynchronous, and between `~Component` and the async dismissal the menu is parentless with its 20 Hz `MouseSourceState` timer still running, still able to emit a non-zero result. Two parented-menu side effects were neutralised in the same change: `withMaximumNumColumns (1)` (a parented menu is budgeted against the editor, and JUCE adds COLUMNS before it scrolls — past ~14 user presets the list would silently go two-column) and a no-op `drawResizableFrame` (JUCE paints a frame over the border ring only when a menu is parented). The "Load Preset…" `FileChooser` callback, one step further along the same path, got the same guard.
- **Why this fix:** it removes the lifetime, not the symptom. The alternatives were both worse: `PopupMenu::dismissAllActiveMenus()` in the destructor also closes **another instance's** menu, and a shared `static` LookAndFeel trades the dangle for static-destruction order at DLL unload. The sibling plug-in Anabasis reached the same conclusion independently.
- **Prevention:** this fix ships **without** an automated regression test, under the `TESTING_POLICY` rule-1 exception recorded in **ADR-0025** — the qualifying condition being that no automated surface reaches the defect, not that a test would be hard: `tests/state_tests.cpp:6-11` constructs the editor but never shows it, so there is no object on screen to destroy while a menu is modal, and pluginval (Level 4) drives a host we do not control and never opens a menu. **What replaced it:** the defect class is removed by construction — a parented menu has no independent lifetime to get wrong — with the residual asynchronous window closed by a `SafePointer`, and every other async/modal callback in the editor audited for the same shape (the file chooser was the only other one, and got the same guard). **Tracked at:** `docs/procedures/TESTING.md` §"Gaps in the automated coverage". **Could infrastructure close it:** partly — the structural half ("is the menu a child of the editor") becomes assertable once the harness instantiates an editor, which the sibling Anabasis already does on all three runners; the behavioural half needs a driven message loop and does not.

## INC-011 — Dismissing a text-field context menu discarded the typed preset name
- **Date:** 2026-08-09 (PR #101) · **Affected version:** ≤0.9.2 (pre-existing, not introduced by the 0.9.2 work), fixed 0.9.3 · **Severity:** Medium (loss of typed user input)
- **Evidence [Verified]:** CHANGELOG.md [0.9.3]; PR #101; src/PluginEditor.h `Backdrop::mouseDown` / `PopupShield`; JUCE 9.0.0 `juce_Component.cpp:2507-2544`, `juce_TextEditor.cpp:1567-1593`, `juce_TextEditor.h:817`; `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md` §4.

- **Problem:** In the Save Preset dialog, right-clicking the name field opens the system text menu (Cut/Copy/Paste/Select All). Dismissing that menu with a click **outside the dialog panel** also closed the dialog.
- **Symptom:** One click, two effects: the context menu closed *and* the Save Preset dialog closed, discarding the name the user had just typed. Nothing was saved and there was no undo — the text simply had to be retyped.
- **Root cause:** JUCE **deliberately re-delivers** the mouse-down that dismissed a modal pop-up. `Component::internalMouseDown` sees the target is blocked, calls `internalModalInputAttempt()` — which dismisses the menu synchronously — and then, observing that the modal loop has exited, hands the *same* event to the component underneath (`juce_Component.cpp:2507-2544`; the comment there states the intent outright). Underneath was `savePresetBackdrop`, whose `onDismiss` is `showSavePreset (false)`.
- **Defect mechanism (Verified from the pinned JUCE 9.0.0 source):** nothing here was individually wrong; two locally-correct decisions composed badly across a framework boundary.
  1. `Backdrop::mouseDown` dismisses its panel on any click outside `panel` — correct in isolation, and the behaviour every modal panel in the editor wants.
  2. `saveNameEditor` is a plain `juce::TextEditor`, and `popupMenuEnabled` defaults to **true** (`juce_TextEditor.h:817`); nothing in `src/` disables it, so a right-click opens a modal `PopupMenu` (`juce_TextEditor.cpp:1567-1593`). The gate on that branch is `wasFocused || ! selectAllTextWhenFocused`, and the dialog focuses the field on open, so it is always taken.
  Neither party knows about the other: the backdrop cannot tell an aimed click from a re-delivered one, and JUCE's contract says the re-delivery is intended. **The plug-in's implicit assumption — "a mouse-down I receive is one the user aimed at me" — is the actual defect**, and it was invisible because it is never written down anywhere.
- **Why verification did not catch it:** three independent reasons, all structural rather than accidental.
  - **No automated surface reaches it.** `tests/state_tests.cpp:6-11` constructs the editor but never shows it; neither suite has a pointer or a display, and the defect needs a real modal pop-up plus a real click. pluginval (Level 4) drives a host we do not control and never opens a context menu.
  - **The earlier fix in the same cycle could not have surfaced it.** The Settings drop-down click-through was fixed first with a predicate reading `ComboBox::isPopupActive()`. That design is *incapable* of expressing this case — `TextEditor::menuActive` is private and a `TextEditor` is not a `ComboBox` — so however thoroughly that fix was exercised, it would never have reached the sibling defect. A fix scoped to the mechanism that happened to **expose** a defect, rather than to the defect's **cause**, cannot find its siblings. That is the transferable lesson from this incident.
  - **The manual report path found the drop-down case and stopped.** The Settings symptom was reported and fixed; nobody thought to try a right-click in a different dialog, because nothing connected the two.
- **Fix:** an editor-level **`PopupShield`** — a transparent, full-editor child that is always visible and normally inert, and starts **intercepting** while any pop-up is on screen. It is then the component the re-delivered click lands on, and it does nothing with it. Two feeders keep it in sync: `AnamorphLookAndFeel::preparePopupMenuWindow` catches every menu built through our look-and-feel (both `ComboBox` and `TextEditor` set the menu's look-and-feel to ours), and `showPresetMenu` counts its own menu, which cannot reach that hook because `MenuWindow` binds `getLookAndFeel()` *before* parenting and calls through that bound reference. Interception is toggled rather than visibility to avoid `setVisible`'s repaint side effects (a full-editor `repaint()` on every menu open, a `repaintParent()` and cached-image release on every close, `juce_Component.cpp:555-563`), **not** to protect hover — hover is safe either way. Every fake mouse move here is asynchronous (`sendFakeMouseMove` → `triggerAsyncUpdate`, `juce_MouseInputSourceImpl.h:449-451`), so it is dispatched after the menu is already modal, and `Component::internalMouseEnter`/`internalMouseExit` both early-return for any component blocked by a modal one (`juce_Component.cpp:2414-2420`, `:2452-2458`); independently of that, this editor derives every hover visual geometrically from `getMouseXYRelative()` rather than from enter/exit (`src/PluginEditor.cpp:1740-1744`, `src/PluginEditor.cpp:1453-1456`), the v0.6.1 stuck-hover fix.
- **Why this fix:** the contract is "the dismissing click must not act on anything underneath", and *underneath* is any control the cursor happens to be over — several of which act on the press itself (`ABControl::mouseDown` toggles A/B, `SpectrumImager::mouseDown` can add a band). A predicate per control is N places to get right and N places for a future control to be forgotten; one shield is one place and covers controls added later for free. It also removes the class rather than the instance, which is what the per-control approach failed to do. Its riskiest property is proved from the source rather than left to a GUI test: the shield cannot be raised in front of a menu, because `MenuWindow` sets `alwaysOnTop` (`juce_PopupMenu.cpp:365`) and `Component::toFront` inserts a non-always-on-top component behind every always-on-top sibling (`juce_Component.cpp:914-922`).
- **Prevention:** ships **without** an automated regression test under the `TESTING_POLICY` rule-1 exception recorded in **ADR-0025**, on the same qualifying condition as INC-010 — no automated surface reaches it, not that a test would be hard. **What replaced it:** the assumption is now explicit and enforced in one place instead of implied in many, with the z-order property proved from the pinned source. **Tracked at:** `docs/procedures/TESTING.md` §"Gaps in the automated coverage", alongside INC-010; the manual checks are listed at the end of `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md`. **Residual:** **KI-018** — the shield stops the click reaching a control but cannot un-count it, so JUCE's multi-click run still includes it and a very fast follow-up click can arrive as a double-click; every available lever is out of bounds (see the KI). **Could infrastructure close it:** yes, and it is the same harness INC-010 names — one that instantiates the editor and drives synthetic mouse events would make "a click while a pop-up is open reaches the shield and no control" directly assertable.

## INC-012 — macOS installer updated a moved copy instead of installing to `/Applications`
- **Date:** 2026-08-11 (PR #102) · **Affected version:** 0.9.0–0.9.2 (every build of the `.pkg` since it was introduced), fixed 0.9.3 · **Severity:** Medium (an install reports success while the item is missing)
- **Evidence [Verified (packaging configuration) / Reported (behaviour)]:** maintainer report; `packaging/macos/build-pkg.sh` (pre-fix: `pkgbuild --root/--identifier/--version/--install-location` with **no** `--component-plist`); `pkgbuild(1)` component-plist semantics (`BundleIsRelocatable`, `BundleIsVersionChecked`); CHANGELOG.md [0.9.3].

- **Problem:** Re-running `Anamorph-<version>-macOS.pkg` did not necessarily install anything where it said it would. It wrote over an earlier copy wherever that copy had ended up.
- **Symptom:** Install, then move `/Applications/Anamorph.app` somewhere else (or drag it to the Trash) and run the installer again. The installer reports **success**, `/Applications/Anamorph.app` is still absent, and the moved copy has been quietly updated instead. The same shape applies to `Anamorph.vst3` and `Anamorph.component` — a plug-in moved out of `/Library/Audio/Plug-Ins/...` is what gets refreshed, so the DAW still sees nothing at the standard path.
- **Root cause:** `pkgbuild` synthesises a component property list for every bundle it finds in the payload, and its default for `BundleIsRelocatable` is **true**. At install time Installer.app then looks the bundle identifier up in the receipt/Spotlight database and, if a copy exists **anywhere on the volume**, redirects the payload onto *that* path — `--install-location` is used only when the lookup finds nothing. The build script never passed `--component-plist`, so all three components shipped with the relocating default. The Trash is not an exception: a "deleted" app is still a file on the volume and still indexed, which is why deleting the app and reinstalling also appeared to do nothing.
- **Defect mechanism:** an unstated default doing something reasonable for a different product. Relocation exists so an app the user *chose* to keep in `~/Applications` is updated in place rather than duplicated; for a plug-in suite with fixed, host-scanned destinations it is exactly wrong — the destination is what the host scans, not wherever the user last dragged the bundle. Nothing in the script expressed which of the two it wanted, so it silently got the other one. `BundleIsVersionChecked` (also defaulting to true) is the same defect from the other end: reinstalling the same version over an intact copy is skipped rather than rewritten, so the pkg could not be used to repair a damaged install either.
- **Why verification did not catch it:** the build-time self-check verified the **package** (three component identifiers present, `customize="allow"`, all choices pre-selected) and never the **install**, which is the only place relocation is observable — it is a property of Installer.app's behaviour at install time, not of the archive's structure. CI has no install step (installing needs root and would mutate the runner), and every manual check was a *first* install onto a machine with no prior copy, the one case relocation cannot affect.
- **Fix:** `build_component()` now runs `pkgbuild --analyze` and patches what pkgbuild itself produced — `BundleIsRelocatable=false`, `BundleIsVersionChecked=false`, `BundleOverwriteAction=upgrade` on every entry (nested bundles included) — then passes it back with `--component-plist`. Each component therefore writes its payload to its declared `--install-location` unconditionally, from the payload alone, consulting no previous installation state. Patching the analysed plist rather than hand-writing one keeps `RootRelativeBundlePath` exact by construction. A per-component `postinstall` verifies its item exists at the destination and fails the install otherwise, so success can no longer be reported over a missing item.
- **Why this fix:** it removes the decision rather than second-guessing it. The alternatives all re-introduce state: a `preinstall` that deletes a found copy needs the same unreliable lookup, and `pkgutil --forget` in a script only clears the receipt — Spotlight would still find the moved bundle. Turning relocation off means there is no lookup to be wrong about. The three destinations are fixed by what hosts scan, so nothing of value is lost by making them non-negotiable.
- **Prevention:** build-time assertions that fail the macOS job, added in the same change: every component's `PackageInfo` must list **no** relocatable (`<relocate>`) and **no** version-checked (`<bundle-version>`) bundle and must declare its `postinstall`; and `pkgutil --expand-full` must show each component's payload carrying the whole bundle down to `Contents/MacOS/Anamorph` (payload completeness, previously never checked). **The review of this fix caught the fix reproducing the defect's own shape:** the version assertion was first keyed on `<version-check>`, an element pkgbuild never writes, so it passed unconditionally — a check that cannot fire is the same silent success as an install that cannot install. The name is now *proved* on every build against a throwaway component built with the defaults left on, which must match both patterns; if it does not, the build stops and prints that component's `PackageInfo` instead of relying on an assertion that has gone quietly dead. **What this does not cover:** the install itself, for the same reason it was never covered — the four-case matrix (fresh / over an existing copy / after moving it away / after deleting it, per format) is a **manual** check on a Mac, listed in `docs/procedures/TESTING.md`. **Could infrastructure close it:** partly — a macOS runner could `installer -pkg … -target /` into a scratch volume and assert the destinations, including after moving the payload away; that is a real CI step, not an impossibility, and is the obvious follow-up if this class recurs. **Documented at:** `docs/procedures/PACKAGING.md` §"macOS reinstall behaviour (idempotency)".

---

## Adding an incident

Create the next `INC-NNN` only when backed by a commit/PR/test/README. Use the template above;
cite evidence with a confidence level; cross-link the relevant ADR. Do not fabricate dates —
use the fix-commit date or mark `[Unverified]`.
