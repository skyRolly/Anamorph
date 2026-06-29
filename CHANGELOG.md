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
- Upgraded the pinned **JUCE** dependency **8.0.8 → 8.0.14** (`CMakeLists.txt` `ANAMORPH_JUCE_TAG`;
  see ADR-0012). Build/dependency change only — no DSP, signal-chain, parameter, or serialization
  changes; CI re-validates the build + 23 DSP self-tests + pluginval (strictness 10), green on the
  Linux gate. The post-upgrade manual audition (Level 5) against the 8.0.8 baseline found no
  perceptual regressions (ADR-0012). Evidence: `CMakeLists.txt:33`; commit `41acaa7`. [Verified]
- Refactored the root `README.md` (slimmed; version history moved into this file) and `CLAUDE.md`
  (policy entry-point); corrected documentation citations and aligned/clarified the signal-chain
  section comments in `EngineParameters.h` / `AnamorphEngine.cpp` (comment-only, no behaviour
  change). Evidence: commits `e83370d`, `2fe5e05`, `1914c52`, `655b6e4`. [Verified]

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
