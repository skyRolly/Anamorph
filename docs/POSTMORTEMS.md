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
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.2]. **[Verified]:** test `testCrossoverAutomationSafe`; ADR-0009; src/dsp/MultibandWidth.cpp:55-71.

- **Problem:** Automating a split toward Nyquist (4 bands crowded high) made the DSP blow up.
- **Symptom:** A "+600 dB" burst that stuck one channel and killed the other.
- **Root cause:** The ordered crossover separation could push a cutoff **above Nyquist**, where the Linkwitz-Riley coefficients go non-finite.
- **Defect mechanism:** The 1.1× ordering enforcement was applied without a Nyquist ceiling, so separation lifted the top cutoff past `0.45·sr` → LR coeffs → Inf.
- **Fix:** Clamp every crossover Nyquist-safe `[20, 0.45·sr]` **before** ordering, then re-clamp **top-down** so separation can never exceed the ceiling. Added an engine-wide NaN/Inf self-heal.
- **Why this fix:** Fixes the instability at the source (the coefficient domain), not by limiting amplitude — dynamics/headroom preserved (no clipper).
- **Prevention:** Test `testCrossoverAutomationSafe`; ADR-0009; the clamp is shared by Multiband/Solo/dry-align banks.

## INC-004 — Meter NaN-latch (bright bar vanished)
- **Date:** 2026-06-27 (`f259a80`) · **Affected version:** ≤0.8.1, fixed 0.8.2 · **Severity:** Medium
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.2]. **[Verified]:** test `testMeterRecoversFromNaN`; src/dsp/LevelMeters.h:73-77.

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
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.5]. **[Verified]:** commit c924ff8; ADR-0011; src/PluginEditor.cpp:246-256; scripts/run-pluginval.sh:46-76.

- **Problem:** Rapid editor open/close on Linux crashed (pluginval "Editor Automation" and real Linux DAWs).
- **Symptom:** A use-after-free segfault during editor teardown.
- **Root cause:** Attaching a `juce::OpenGLContext` on X11 adds an embedded child window whose `ConfigureNotify` makes the host's `XEmbedComponent` post an async lambda capturing a raw `this`, which can fire after the editor is destroyed.
- **Defect mechanism:** The UAF is in JUCE's **host-side** X11 embedding; the plugin's GL child window generated the `ConfigureNotify` traffic that triggered it.
- **Fix:** Skip the GL attach on Linux/BSD (`#if ! (JUCE_LINUX || JUCE_BSD)`) — render CPU-side (visually identical); release the VBlank callback before detaching in the destructor; add a signal-only pluginval retry.
- **Why this fix:** Removing the GL child window cuts the `ConfigureNotify` traffic at its source; the crash lives in pluginval's own JUCE and cannot be fixed from this repo, so the retry mitigates the residual flake.
- **Prevention:** ADR-0011; `run-pluginval.sh` signal-only retry; see KNOWN_ISSUES (residual external flake).

## INC-007 — Multiband Enable mute/dropout
- **Date:** 2026-06-28 (`10fbfa0`) · **Affected version:** ≤0.8.5, fixed 0.8.6 · **Severity:** Medium
- **Evidence [Partially Verified]:** CHANGELOG.md [0.8.6]. **[Verified]:** test `testMultibandEnableCrossfadeClickFree`; ADR-0004; src/dsp/AnamorphEngine.cpp:655-707.

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
- **Evidence [Verified]:** CHANGELOG.md [0.8.7]; commit 6a24b82; test `testSoloMultibandEnableClickFree`; src/dsp/AnamorphEngine.cpp:831-845; ADR-0004.

- **Problem:** With a Band Solo active, toggling Multiband Enable clicked on both edges (a regression introduced by INC-007's 0.8.6 change).
- **Symptom:** An audible click (amplitude + phase step) on both enable and disable edges, only when a band was soloed.
- **Root cause:** The SoloMonitor call was hard-gated `if (p.mbEnable) soloMonitor.process(...)`. When 0.8.6 made `mbEnable` a continuous (un-ducked) toggle, the gate started/stopped the monitor in one block while its `passGain`/`bandGain` crossfade was frozen at the soloed target — inserting/removing the whole band-pass in a single sample.
- **Defect mechanism (Verified):** `SoloMonitor` is click-free *only* because its crossfade advances every block; gating the call on the instantaneously-flipping `mbEnable` bypassed that crossfade. The `mbEnableBlend` output crossfade is at an earlier stage and is a no-op at default unit widths, so it could not mask the step.
- **Fix:** Run the monitor **every block**, masking the solo by `mbEnable`: `soloMonitor.process(L, R, p.mbEnable ? p.mbSolo : 0, n)`.
- **Why this fix:** Restores the monitor's documented every-block invariant; it morphs solo↔passthrough over its own ~12 ms ramp; at `mask 0` the settled monitor is a bit-exact passthrough, so steady-state and the `mbSolo` parameter are untouched.
- **Prevention:** Regression test `testSoloMultibandEnableClickFree` (worst single-sample step 0.31 on the old code → 0.015 on the fix); ADR-0004 records the "must run every block" invariant.

---

## Adding an incident

Create the next `INC-NNN` only when backed by a commit/PR/test/README. Use the template above;
cite evidence with a confidence level; cross-link the relevant ADR. Do not fabricate dates —
use the fix-commit date or mark `[Unverified]`.
