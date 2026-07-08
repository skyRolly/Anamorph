# Changelog

All notable user-visible changes to Anamorph. Format follows [Keep a Changelog]; versions are
`MAJOR.MINOR.PATCH` (pre-1.0). The repository has **no git tags**, so each entry cites its **commit
SHA + date** as the Evidence Source (per `docs/policies/CHANGELOG_POLICY.md`). Entries for the
0.6.x line and earlier are reconstructed from commit history (the detailed per-version notes predate this changelog) and are marked accordingly.
Display-name renames are recorded as **Changed**, never as parameter removals (the IDs are immutable).

## [Unreleased]
### Added
- Documentation library under `docs/`: architecture reference + 12 ADRs, binding policies,
  procedures, and tracking docs (HANDOVER, POSTMORTEMS, KNOWN_ISSUES, FUTURE_RISKS, REPOSITORY_MAP,
  DOCUMENTATION_COVERAGE), plus this `CHANGELOG.md`. No plugin/behaviour change.
  Evidence: commits `c9b7fdf`, `a9e915e`, `97060b2`. [Verified]
### Changed
- **Engine CPU micro-optimisations**: the drive waveshaper's peak-preserving makeup (1/tanh) and
  its gain/blend reads are hoisted out of the per-sample loop once both smoothers have settled
  (any glide keeps the exact per-sample path), the two always-on delay rings wrap their write
  index by branch instead of an integer division per sample, and the Level-Match silence-energy
  scan runs only while Match is on or its engage duck is in flight (which keeps the silence-edge
  snap decision exactly as before). Output is bit-identical (full-engine dump across 20 scenarios
  incl. drive automation, all oversampling modes, impulse/delay-alignment, bypass, Match toggling
  with an engage-vs-audio-edge alignment sweep: byte-equal; reported latency unchanged). Measured:
  drive engaged −19/−38/−73 µs per 512-sample block at OS ×2/×4/×8, ~−6 µs/block across the board
  from the ring wrap, ~−1.6 µs/block with Match off. Evidence: PR #53. [Verified]
- **Scope ring publish batched**: the audio thread now publishes the vectorscope/analyser ring's
  write index once per block (one release-store) instead of once per sample, on the same atomic
  with the same release/acquire contract -- readers see whole blocks atomically and can never
  observe partially committed frames. Audio output, ring contents and read counters are
  byte-identical (deterministic dump), and a two-thread stress (10⁹-frame scale) shows no
  publication tears in either the old or new design. Measured: ~−2 µs per 512-sample block
  median across the matrix. Evidence: PR #53. [Verified]
- **Velvet decorrelator CPU (2)**: the sparse-FIR tap accumulation is now skipped when its
  contribution is exactly zero -- Amount exactly 0 (the default state) or the presence gate
  exactly closed (silence from start, or after the transport-stop flush) -- outside any stop
  fade, which keeps running the full path. No thresholds: only provably-exact zeros are skipped,
  history writes and every envelope/glide keep running, and output is bit-identical (validated
  sample-exact across 12 scenarios / ~5.6 M samples incl. a signed-zero adversarial case).
  Engine cost with Velvet at Amount 0 drops a further ~15-19 µs per 512-sample block at 48 kHz.
  Evidence: PR #53. [Verified]
- **Velvet decorrelator CPU**: the per-sample tap re-weighting (64-tap rebuild + square-root
  normalisation) now runs only while the Density glide is actually moving; once the glide reaches
  its float fixpoint the rebuild is skipped on an exact bit-compare (never a threshold -- the
  pre-0.4.1 drift gate was the #18 zipper and stays dead). Output is bit-identical in every
  scenario, moving or settled (validated sample-exact across 9 scenarios / ~3.9 M samples incl.
  fast Density drags, transport stop and the default preset). Engine cost with Velvet selected
  drops ~36-38 µs per 512-sample block at 48 kHz (default idle state −40 %); zipper-free Density
  behaviour is unchanged. Evidence: PR #53. [Verified]
- **Meters idle CPU/GPU** (Balance / Correlation pointers, Levels panel): each meter now repaints
  only when what it draws actually changed, and the default-hidden Levels panel stops its 60 Hz
  timer entirely while hidden (it restarts on Show Meters). The correlation/balance pointers'
  return-to-centre relax completes in full, then lands exactly on target (final step under 0.2 px
  and a quarter of a colour quantum -- invisible); the Levels panel compares every published value
  bitwise, so no decay, hold, clip colour or number update can ever be skipped. Ballistics, attack/
  release and all animations are unchanged while values move (measured: full-rate repaints while
  anything moves incl. the whole silence decay; 0 repaints once settled; hidden-meter timer
  wakeups 60/s → 0). Evidence: PR #53. [Verified]
- **Spectrum (Multiband) idle CPU/GPU**: the analyser now runs its 8192-point FFT only when the
  analysis window actually changed, and repaints only while something on screen still moves
  (spectrum decay, clip-red fade, animations, drags). Digital silence stops the FFT as soon as
  the window has drained (~170 ms) while the displayed decays complete in full; a frozen
  transport or a hidden imager (Simple mode / hidden editor) costs nothing. Re-showing resumes
  live analysis on the first frame. Analysis maths, FFT size/window, decay rates and rendering
  are unchanged (measured: 60/60 FFTs+paints per second while active, before and after; silence:
  FFT stops after ~11 ticks, paints end once decays land; hidden: 60 FFTs/s → 0).
  Evidence: PR #53. [Verified]
- **Vectorscope idle CPU/GPU**: the 60 Hz timer now repaints only while the displayed picture can
  actually change; after the trail fully scrolls out on digital silence (or when the host stops
  processing), the view paints one final frame and goes idle. Rendering while audio flows is
  unchanged (measured: full frame rate active; 0 repaints/s once quiescent; idle-editor CPU
  −40 % on the Linux Standalone under Xvfb). Trail look, decay timing, and persistence behaviour
  are pixel-identical. Evidence: PR #53. [Verified]
- Upgraded the pinned **JUCE** dependency **8.0.8 → 8.0.14** (`CMakeLists.txt` `ANAMORPH_JUCE_TAG`;
  see ADR-0012). Build/dependency change only — no DSP, signal-chain, parameter, or serialization
  changes; CI re-validates the build + 23 DSP self-tests + pluginval (strictness 10), green on the
  Linux gate. The post-upgrade manual audition (Level 5) against the 8.0.8 baseline found no
  perceptual regressions (ADR-0012). Evidence: `CMakeLists.txt:33`; commit `41acaa7`. [Verified]
- Refactored the root `README.md` (slimmed; version history moved into this file) and `CLAUDE.md`
  (policy entry-point); corrected documentation citations and aligned/clarified the signal-chain
  section comments in `EngineParameters.h` / `AnamorphEngine.cpp` (comment-only, no behaviour
  change). Evidence: commits `e83370d`, `2fe5e05`, `1914c52`, `655b6e4`. [Verified]
- CI pluginval gate **unified and hardened across all three platforms**: each of Linux, Windows and
  macOS now runs pluginval at strictness 10 in **two explicit, blocking steps** — deterministic
  (`--random-seed 0`) **and** `--randomise` — **each repeated 3 consecutive times**. The previous
  Windows/macOS `continue-on-error` (which swallowed `exit 1` and reported a false green) is removed;
  a non-zero pluginval exit now fails the job on every platform. Linux/macOS use
  `scripts/run-pluginval.sh`, Windows uses the new `scripts/run-pluginval.ps1` (same structure).
  `actions/checkout` and `actions/upload-artifact` bumped `v4 → v5` (clears the Node 20 deprecation
  warning). Evidence: `.github/workflows/build.yml`, `scripts/run-pluginval.sh`,
  `scripts/run-pluginval.ps1`.
- **Parameter display-name renames** (parameter **IDs unchanged**, so automation/state survive):
  "Algorithm" → **"Widen Algorithm"** and "Dimension Mode" → **"Dim-D Style"**, matching the GUI.
  `Multiband Bands` and `Multiband Solo` are now **exposed and automatable** in the host automation
  list (the previous `withAutomatable(false)` was removed). Conversely, **`Advanced Mode` is now
  non-automatable** (`isAutomatable()` = false): it is a UI-layout toggle, not a sound parameter.
  Host-automating it drives editor resizes (`applyUiScale`), and on **Linux/X11** the resize
  `ConfigureNotify` storm hits a use-after-free in the **host's** JUCE `XEmbedComponent` during rapid
  open/close (reproduced locally; the core dump lands in `XEmbedComponent` — KI-003/KI-007). A layout
  toggle has no place in an automation lane anyway. IDs, ranges and defaults are unchanged (a recorded
  automation-flag change, `PARAMETER_COMPATIBILITY_POLICY` rule 5). Evidence: `src/PluginParameters.cpp`;
  `docs/architecture/PARAMETER_REGISTRY.md`.
- **CI: the randomise pluginval gate is never skipped.** The randomise step (all three platforms) is
  guarded with `if: ${{ !cancelled() }}`, so a deterministic-mode failure no longer skips the randomise
  run — both modes report independently every CI run. The job still fails if either mode fails.
  Evidence: `.github/workflows/build.yml`; `docs/procedures/CI_CD.md`.
### Fixed
- **A/B compare slots are independent from plugin open again.** The two A/B slots were snapshotted
  **lazily** on the *first* A/B switch (`abEnsureInit`), so editing A *before* ever visiting B made B
  born as a copy of A's **already-edited** state — switching to B showed A's parameters, not the open
  (Default) state. Whether B ever looked "clean" depended on when the host happened to call
  `getStateInformation` (which also runs `abEnsureInit`) — a host-timing accident. Both slots are now
  initialized to the open state in the constructor, so an edit to A never leaks into B. The A/B
  switch/apply/serialization logic is unchanged (ADR-0008); only *when* the initial snapshot is taken
  changed. Evidence: `src/PluginProcessor.cpp` (constructor `abEnsureInit()`).
- **A corrupt user preset no longer leaves the undo bracket half-open.** In `PresetManager::load`,
  `onAboutToLoad` (which flushes undo coalescing) fired *before* the preset XML was parsed, so a file
  that failed to parse returned early and never fired the matching `onLoaded`, silently flushing a
  settled edit without recording its undo step. The XML is now parsed **before** the bracket is opened
  (matching `loadFile`), so a parse failure is a clean no-op. Evidence: `src/PresetManager.cpp` (`load`).
- **Windows pluginval: the script now WAITS for pluginval — fixes garbled output and false pass/fail
  (KI-007).** `pluginval.exe` is a **GUI-subsystem** app, so PowerShell's call operator (`& $pv`)
  returned immediately with a `$null` exit code *without waiting*. The original `exit $LASTEXITCODE`
  false-greened (null → `exit 0`); after the crash-retry loop was added, that null was misread as a
  crash and **each retry launched another pluginval that kept validating in the background** — three
  concurrent validators writing one console (the "garbled" interleaving) and a false failure, while the
  plugin actually validated fine. `scripts/run-pluginval.ps1` now launches pluginval via
  `System.Diagnostics.Process` (`UseShellExecute=$false`) + `WaitForExit()` and reads the **real**
  `.ExitCode`; exactly one runs at a time (no interleaving). OpenGL GPU rendering stays **ON** for
  Windows/macOS (`#if ! (JUCE_LINUX || JUCE_BSD)`); Windows CI keeps `--skip-gui-tests` conservatively
  (the GPU-less runner's GDI-generic OpenGL 1.1 very likely can't render the JUCE GL editor — never
  observed because the wait bug masked all Windows editor results; the editor is validated on Linux +
  macOS). Evidence: `scripts/run-pluginval.ps1`; KI-007.
- **Host state restore no longer notifies the host of parameter changes (Devin review).** During
  `setStateInformation`, `reassertParameters` called `setValueNotifyingHost` for each restored
  parameter, notifying the host mid-restore (some DAWs treat that as an automation write). It now takes
  a `notifyHost` flag: the host-restore path updates `getValue()` (`setValue`) and writes the DSP raw
  atomic directly — **no host notification** — while undo/redo/A-B (editor-initiated) keep the full
  notifying path. Evidence: `src/PluginProcessor.cpp` (`reassertParameters`).
- **Preset switching is undoable again (regression from the gesture-gated undo).** A preset load
  arrives as gesture-less `setValueNotifyingHost` calls, so the new gesture-gated coalescer folded it
  into the baseline **without** an undo step — after switching presets you could not Undo back to the
  previous preset. Each load is now explicitly bracketed (`PresetManager::onAboutToLoad` / `onLoaded`):
  a settled edit is flushed first, then the switch is recorded as exactly **one** undo step in the
  **active A/B slot's** history. A/B slots keep their independent histories (by design, ADR-0008);
  only preset switches *within* a slot are chained, and the switch itself is now an undo/redo step.
  Evidence: `src/PluginProcessor.cpp` (`commitPresetSwitchUndoStep`, constructor hooks),
  `src/PresetManager.cpp` (`load` / `loadFile`).
- **Windows pluginval no longer reports a false green when it crashes.** `scripts/run-pluginval.ps1`
  ran `exit $LASTEXITCODE`, but an abnormal pluginval termination (e.g. a crash in the editor tests)
  leaves `$LASTEXITCODE` `$null`, and `exit $null` exits **0** — so a crashed run *passed* the gate
  (observed: the Windows step ran in ~6–7 s vs Linux ~40 s / macOS ~185 s, ending at
  `pluginval: FAILED … (exit )` with an empty code yet still green). The script now treats a
  null/negative/large exit code as a crash (never success) and, like `run-pluginval.sh`, retries a
  crash and still fails after the retries — only a clean `exit 0` passes. This surfaces a pre-existing
  Windows "Editor Automation" crash (now tracked as **KI-007**). Evidence: `scripts/run-pluginval.ps1`.
- **Undo/Redo: one step per gesture, and host automation is never recorded.** Undo coalescing was
  time/signature-settle based, so a slow drag that dwelt mid-gesture (esp. Multiband Split / Band
  Width) recorded multiple intermediate steps, and any host-automation move could create undo steps.
  It is now **gesture-gated**: the processor listens to parameter begin/end gestures and commits
  exactly **one** undo step after the last gesture closes; automation (which never opens a gesture)
  folds into the baseline without an undo entry. A/B switch/copy **and undo/redo** reset the gesture
  state (a state jump is never a user gesture, so nothing re-commits after it). Evidence:
  `src/PluginProcessor.cpp` (`parameterGestureChanged` / `pollUndoCoalesce` / `undo` / `redo`).
- **Combo-box pop-ups drop BELOW the box again** instead of covering it with the selected item under
  the cursor. Added `AnamorphLookAndFeel::getOptionsForComboBoxPopupMenu` targeting the box's screen
  bounds (omitting the JUCE default `withItemThatMustBeVisible`/`withInitiallySelectedItem`). Evidence:
  `src/gui/LookAndFeel.cpp` (`getOptionsForComboBoxPopupMenu`).
- **Discrete parameters now round-trip their exact value under pluginval `--randomise`.** Stock
  `AudioParameterBool`/`Choice`/`Int` snap `getValue()` to the nearest legal step, which for few-step
  params can be `>0.1` from the raw value pluginval sets (seed-dependent "not restored" failures) — and
  they cannot be subclassed to fix it (JUCE declares their `getValue()`/`setValue()` **private**). The
  discrete params are reimplemented as minimal from-scratch `juce::RangedAudioParameter` subclasses
  (`RawChoice`/`RawBool`/`RawInt`) whose `getValue()` keeps the exact raw normalised value (restored via
  the `raw` attribute + `reassertParameters`); the DSP still reads the snapped value via
  `getRawParameterValue()` and host text via `getAllValueStrings()`. Because these are no longer the
  stock concrete types, `getBypassParameter()` now holds an `AudioProcessorParameter*` (no
  `dynamic_cast`) and the ComboBox item list is read from `getAllValueStrings()` — no behaviour change.
  See ADR-0013. Evidence: `src/PluginParameters.cpp` (`RawChoice`/`RawBool`/`RawInt`).
- **State restoration now round-trips every parameter exactly.** Two issues, both surfaced by the
  `--randomise` *Plugin state restoration* gate: (1) a wholesale `apvts.replaceState` did not
  reliably propagate to every parameter's cached value (an occasional param kept its pre-restore
  value); (2) APVTS serialises the **denormalised/snapped** value, so a **discrete** param
  (Bool/Choice/Int) given a raw normalised value mid-step (e.g. `Input Channel` at `0.177521` on a
  3-choice) round-tripped to the nearest legal value — `>0.1` away — and pluginval flagged it "not
  restored". Fix: `getStateInformation` additively records each parameter's **exact raw
  `getValue()`** as a `raw` attribute on its `PARAM` node, and `setStateInformation` →
  `reassertParameters` restores from `raw` (falling back to the denormalised `value` for legacy
  sessions). Additive + backward-compatible (old sessions ignore `raw`; the APVTS `value` is
  unchanged — no field removed/renamed). Evidence: `src/PluginProcessor.cpp`
  (`getStateInformation` / `reassertParameters`); CI runs `28356632727`, `28388176607` (the
  `--randomise` failures: discrete params "not restored"). See `SERIALIZATION_REGISTRY.md`.
- **The exact-value restore is extended to user actions** — undo / redo / A-B apply now re-assert
  every parameter from the snapshot (`reassertParameters` after `replaceState` in
  `applyStatePreservingView`), and A/B-slot snapshots carry the `raw` attribute
  (`copyStateWithRawValues`, used by `currentStateSet`), so discrete params no longer snap-drift on
  slot switching or undo. Evidence: `src/PluginProcessor.cpp`.
- **Windows CI no longer skips the randomise pluginval pass.** `run-pluginval.ps1` now makes the
  pluginval **exit code the sole** pass/fail signal (`$ErrorActionPreference = Continue` +
  `$PSNativeCommandUseErrorActionPreference = $false`), so pluginval's stderr progress can no longer
  throw a terminating error that fails the *deterministic* step and makes GitHub **skip** the
  randomise step. Evidence: `scripts/run-pluginval.ps1`.
- **Defensive A/B bounds.** `abSwitchTo` clamps its slot index (`juce::jlimit(0, kNumAbSlots-1, …)`),
  and `abUndo` / `abSlot` / `abMatchGain` are sized from `anamorph::kNumAbSlots` (single source of
  truth) instead of a hardcoded `2`. Evidence: `src/PluginProcessor.{h,cpp}`; `src/AbSlotIndex.h`.
- **Linux:** tooltips no longer render opaque **black corners** outside the rounded capsule on X11
  without a compositor — `drawTooltip` now fills the corner area with the capsule colour when
  per-pixel window alpha is unavailable; macOS/Windows transparent corners are unchanged (KI-006).
  Evidence: `src/gui/LookAndFeel.cpp` (`drawTooltip`). [Partially Verified] (Linux visual re-test pending)
- Session restore now **clamps a corrupted / out-of-range A/B "active" index** so it can never index
  the A/B slot arrays out of bounds; valid sessions are unaffected. Evidence:
  `src/PluginProcessor.cpp` (`setStateInformation`), `src/AbSlotIndex.h`; regression test
  `testAbActiveClampOnCorruptState`. [Verified]

## [0.8.7] — 2026-06-28
### Fixed
- Audible click when toggling Multiband Enable while a Band Solo was active: the post-everything
  Band-Solo monitor now runs every block (mask driven from `mbEnable`) instead of being hard-gated,
  so it morphs solo↔passthrough over its own ramp. Evidence: commit `6a24b82`; test
  `testSoloMultibandEnableClickFree`. [Verified]

## [0.8.6] — 2026-06-28
### Fixed
- Alt/Option-click knob reset now animates like double-click (a `resetSweep` flag opts the eased
  travel out of the button-held snap). Evidence: commit `10fbfa0`. [Partially Verified]
### Changed
- Multiband Enable now transitions via a ~12 ms click-free output crossfade (warm crossover bank),
  not a duck-to-silence — no mute/dropout. Evidence: commit `10fbfa0`; test
  `testMultibandEnableCrossfadeClickFree`. [Verified]
- Renamed the automation parameter display name **"Haas Side" → "Haas Focus"** (ID `haasSide`
  unchanged). Evidence: commit `10fbfa0`; `src/PluginParameters.cpp:135-136`. [Verified]

## [0.8.5] — 2026-06-28
### Fixed
- Linux editor crash under rapid open/close (OpenGL/X11 `XEmbedComponent` use-after-free): the
  editor now renders CPU-side on Linux/BSD (visually identical). Evidence: commit `c924ff8`. [Partially Verified] / code [Verified] (`src/PluginEditor.cpp:246-256`).

## [0.8.4] — 2026-06-27
### Changed
- Oversampling, Window Size, Scope Persistence, Tooltips, UI Animations and Show Meters are hidden
  from the host parameter list (moved out of the APVTS into a host-hidden `InternalState`); pre-0.8.4
  sessions are migrated. Evidence: commit `6bd158b`. [Partially Verified] / code [Verified].

## [0.8.3] — 2026-06-27
### Changed
- Bypass is a true click-free crossfade and the chain + Level-Match analysis always run (Bypass only
  changes the audio path). Confirmed there is no 0 dBFS output clipper. Evidence: commit `3686d12`;
  tests `testBypassCrossfadeClickFree`, `testLevelMatchRunsInBypass`. [Verified]

## [0.8.2] — 2026-06-27
### Fixed
- Multiband crossover automation no longer explodes near Nyquist (Nyquist-safe clamp + top-down
  ordering); meters recover from a NaN burst; Level Match reads ~0 at unity with Multiband on; clean
  Bypass transitions; meter holds reset on a transport seek. Evidence: commit `f259a80`; tests
  `testCrossoverAutomationSafe`, `testMeterRecoversFromNaN`, `testMultibandUnityMatch`. [Verified]
### Changed
- Advanced state travels with A/B; Settings/Multiband-Bands/Solo de-cluttered from automation;
  M/S-clarified automation names. Evidence: commit `f259a80`. [Partially Verified]

## [0.8.1] — 2026-06-23
### Fixed
- Band Solo is click-free and ghost-free (warm monitor, no duck); Level Match no longer ratchets
  toward −24 dB or slams at Mix=100% (Measure + absolute Predict). Evidence: commit `6d2023b`; tests
  `testSoloNoGhostInSilence`, `testLevelMatchNoRatchet`, `testLevelMatchMixCouplingNoSlam`. [Verified]
### Changed
- Folded the two outlier transitions into the one anti-click layer; band-pass preview is
  press-and-hold only. Evidence: commit `6d2023b`. [Partially Verified]

## [0.8.0] — 2026-06-22
### Changed
- Signal flow rebuilt as a strict serial chain: **Processing → Mix → Mono Maker (post-Mix) → Output
  → Band Solo monitor (post-everything)**, eliminating the solo/low-cut bug class. Evidence: commit
  `018dcdd`; tests `testMonoMakerPostMix`, `testSoloMonitor`. [Verified]

## [0.7.5] – [0.7.0] — 2026-06-21…22
### Changed
- 0.7.5 (`6846c60`): Mono Maker lows follow band 0's solo. 0.7.4 (`818b22f`): keep Mono Maker lows
  present while a band is soloed. 0.7.3 (`37526da`): Multiband Solo obeys Mix; Windows window-size
  DPI fix. 0.7.2 (`7d0ccdf`): phase-align the dry path with the Multiband crossovers for the Mix
  (mono-compatible at any Mix). 0.7.1 (`911701d`): per-band width smoothing (fast-drag clicks) +
  3-OS full-format CI. 0.7.0 (`dac5beb`): ground-up Multiband spectral editor (1–4 bands, drag-to-
  split, per-band Solo); pluginval gate at strictness 10. Evidence: the cited commits.
  [Partially Verified]

## [0.6.x] and earlier — 2026-06 (reconstructed)
`[Unverified Historical Reconstruction]` — the 0.2 → 0.6.19 line (variable-band Multiband DSP+GUI,
4-bit per-band Solo, the asymmetric click-free switch duck, Undo/Redo per A/B slot, M/S
encode→decode, transparent-on-load, level meters, oversampling) is described in commit history (the pre-refactor README's "What's new" sections, now superseded by this changelog) and exists as version commits in history (e.g. 0.6.10
`98e2886` … 0.6.19 `9da01ad`), but the repository has **no tags** to attribute exact per-version
feature sets to a released artifact. See `README.md` history for the narrative.

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
