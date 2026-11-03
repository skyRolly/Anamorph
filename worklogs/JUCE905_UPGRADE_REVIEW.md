# JUCE 9.0.5 investigation and architecture review packet

Recorded 2026-11-04 (Asia/Shanghai). **Proposal only; Pending Human Architecture Review.**
Current CMake dependency remains 9.0.4. [Proposed ADR-0071](../docs/architecture/design-decisions/ADR-0071-juce-9.0.5-upgrade.md)
records the requested decision; [evidence.json](juce905-review/evidence.json) retains measured
source/provenance, independent baseline CI and local checks. No product code/build definition,
version, release tag, release asset, merge or shared module is changed.

## 1. Independent baseline and authorization boundary

| Item | Verified state |
|---|---|
| Remote main, fetched before edits | `82c98e45984f5c7bc0f953340622f90523957780` |
| Initial checkout | Clean `work` branch at `c35a9d6b68c53142d42c957d9ef6e5ab1b7caac9`; remote main had advanced |
| Upgrade branch | `codex/juce-905-upgrade`, created directly from fetched main |
| Previously open PRs | GitHub search returned no open PRs; latest main includes merged #212 |
| Product version | 0.10.0, unchanged |
| Current dependency | JUCE 9.0.4 / `3fb5cbc0340758596dac39f7c8de47d562b4896e` |
| Proposed dependency | JUCE 9.0.5 / `d60b7a78030661574a615b586d9325ba7a7fa7dd` |
| Governance | JUCE pin is a Build System gate; human DSP/audio review required before implementation |

Read the entry point, source-of-truth, agent/dependency/architecture/ADR/DSP/realtime/threading/
testing/compatibility/documentation/release policies and relevant Accepted ADRs. ADR-0061 and
`JUCE904_UPGRADE_0.9.9.md` provide the previous-upgrade record; earlier JUCE ADRs/worklogs are
historical evidence. ADR-0057's publication requirement and ADR-0067/0068/0070 remain binding.
The owner selected the target but explicitly retained the mandatory gate. No Proposed record is
marked Accepted and no earlier manual attestation is extended to this dependency.

## 2. Existing build, cache and platform contracts

`CMakeLists.txt` sets CMake ≥3.22/C++23 and exposes `ANAMORPH_JUCE_VERSION`, `ANAMORPH_JUCE_TAG`
and `ANAMORPH_JUCE_PATH`. A supplied local path uses `add_subdirectory`; otherwise FetchContent
uses the immutable commit and `GIT_SHALLOW TRUE`. `cmake/CheckJuceCache.cmake` refuses either
field from 9.0.2/9.0.3, preserves custom revisions and bypasses the guard for a local comparison.
It does **not** reject 9.0.4, correctly for current main; after a defaults-only bump, old caches
would retain it. Proposed correction: add the old version/SHA to the existing lists and change
the diagnostic to 9.0.5, with regression cases for fresh/reused/partial/mixed/reset/local paths.
Do not force arbitrary custom pins or substitute a tag name.

Supported builds remain Linux x86-64 and Windows x64 VST3/Standalone, macOS universal arm64+
x86_64 VST3/AU/Standalone, and the native Intel validation job (Standalone disabled there).
Retain the Haswell/Excavator ISA floor, architecture-specific numerical scope, macOS deployment
settings, Clang 22 shipping toolchain and GCC 16 compatibility lane. The normal tests, optional
`ANAMORPH_BUILD_DSPDUMP`, benchmark and fuzz targets keep their existing settings. All six
consumer definitions explicitly exclude MP3/Opus/WebP; no codec, SDK or dependency is added.

## 3. Upstream identities, release and module audit

Official [release](https://github.com/juce-framework/JUCE/releases/tag/9.0.5),
[CHANGE_LIST](https://github.com/juce-framework/JUCE/blob/d60b7a78030661574a615b586d9325ba7a7fa7dd/CHANGE_LIST.md),
[BREAKING_CHANGES](https://github.com/juce-framework/JUCE/blob/d60b7a78030661574a615b586d9325ba7a7fa7dd/BREAKING_CHANGES.md),
and [75-commit comparison](https://github.com/juce-framework/JUCE/compare/3fb5cbc0340758596dac39f7c8de47d562b4896e...d60b7a78030661574a615b586d9325ba7a7fa7dd)
were reviewed against clean detached checkouts. `git ls-remote` returns each stated SHA with
no annotated-tag peel; project versions and module declarations corroborate it.

The graph comes from this repository's explicit `juce::` links and recursive module declarations,
plus the generated plugin's `juce_audio_plugin_client` (15 modules). This is source-derived;
successful generated-command provenance remains pending because configuration cannot finish.

| Linked area | 9.0.4 → 9.0.5 finding | Disposition |
|---|---|---|
| dsp, audio_basics | Module version only; oversampling, filters, audio buffers unchanged | Direct source check; numerical gate still required |
| events, opengl | Module version only; MessageManagerLock and GL context implementation unchanged | Direct source check; platform peers/rendering still change |
| audio_processors/headless | APVTS unchanged; editor peer API, plugin-list callback lifetime and VST3 header include change | Direct editor dependency; no new parameter storage order |
| audio_plugin_client | Windows VST3 host-DPI tracking changes (`7a17fea6c`); AU and Standalone wrapper files unchanged; LV2 helper fix outside shipped formats | Direct Windows risk; no identity/process/latency change in the VST3 patch |
| data_structures/core | XML, ValueTree/var, JSON, ZIP, String comparison and Windows File/thread fixes | Classify by reachable path below |
| graphics | HarfBuzz 15, PNG/GIF validation, image/rectangle arithmetic fixes | Text/rendering direct; external-media exposure unestablished |
| gui_basics/gui_extra | Native child/associated windows, modal APIs, pointer retargeting, menus/tooltips, platform peers and file dialogs | Direct GUI/lifetime/native acceptance |
| audio_devices/audio_utils | WinMM/CoreMIDI changes, iOS device fix, device-error dialog API adaptation | Potential Standalone/device impact; no DSP algorithm change |

### API and internal assumptions

No first-party call/override of removed `NativeMessageBox::show/showAsync`, the old one-argument
`MessageBoxOptions::withButton`, `createAlertWindow`, `createNewPeer` or virtual `addToDesktop`
was found in `src/`. That search is source evidence, not a successful build.

APVTS `juce_AudioProcessorValueTreeState.cpp` is byte-identical, SHA-256
`94760f6858acf7fcf52915504cc24702cdea9c031d9a9593919de108884a1ad6`. Target lines 155/208
show default `seq_cst` atomic assignment/type; this satisfies ADR-0057's release-or-stronger
store requirement. `ParamPointers::toEngine` keeps its `seq_cst` loads and finite-default read.
Latency notification/restore contracts under ADR-0068 are unchanged in first-party source.

The popup constructor's parented branch still binds default look-and-feel before adding the
child. `preparePopupMenuWindow` moves 515 → 511; parented menus now retain transparency even
when desktop peers cannot. Child insertion ordering, modal entry and completion-owned deletion
remain; Component mouse callbacks now retarget a deleted recipient to a surviving ancestor.
The GL message try-lock is unchanged at `juce_OpenGLContext.cpp:405`; component painting remains
inside its scope. Buttons/attachments and APVTS are unchanged. Existing lifetime/restore/scale
regressions (States155–157 and SpectrumImager fixtures) must run against the target. Live citations
in changed upstream files must be refreshed during implementation, while historical ADRs retain
their original coordinates. No GUI/GL runtime acceptance is inferred from this audit.

### Security/reachability classification

| Change | Classification and evidence |
|---|---|
| XML depth/entity/allocation limits and external-entity opt-in (`3342d4091`, `f8a5c5d80`, `94410931f`) | **Directly applicable parser**, but `src/XmlBoundary.h`, `PresetManager.cpp` and `PluginProcessor.cpp` already bound session and separately parsed A/B/preset XML to depth 8 and reject DOCTYPE before JUCE. Retain the stronger product boundary and controls. New 256-depth/40-entity/expansion limits do not justify removing it. Legitimate fixture equivalence remains pending. |
| ValueTree stream and var array depth/allocation limits (`fde0a524a`) | **Irrelevant to identified persistence path**: product uses XML/`ValueTree::fromXml`, not binary `readFromStream/readFromData/readFromGZIPData`. Module presence does not establish a reachable input. |
| JSON nesting (`486906b64`) | **Irrelevant to identified runtime input**: no first-party JSON parser call in `src/`; Python tooling JSON is a different implementation. |
| ZIP entry/link/allocation confinement (`31b3b6806`, `2303b551c`) | **Irrelevant to identified product input**: no first-party archive extraction. Python pluginval acquisition is separate and unchanged. |
| WAV/AIFF channels/metadata (`8c0c0d9b2`, `8493b2c3c`) | **Potentially applicable transitively**: audio_formats links, but Anamorph does not read arbitrary audio files. Additional caller/input evidence is required for an exposure finding. |
| PNG dimensions, GIF LZW, convolution/rectangle arithmetic (`ab9047fb2`, `b90de9702`, `909ffb0fa`, `bf695ff98`) | **Potentially applicable** to framework images/painting/build-helper icons; approved fixed PNG inputs exist. Arbitrary external media ingestion is not established. Validate icons/rendering and compiled source provenance. |
| OSC and JavaScript (`7f5e58eae`, `3993ffc82`, `732213bc0`, `8ec5db950`) | **Irrelevant**: neither module is in the product's source-derived graph; no first-party API use. |
| Native GUI/pointer/dialog and Windows path/thread changes | **Directly/potentially applicable** through editors, file chooser/preset paths and Standalone. Native multi-monitor, teardown, cancel and path behavior require additional platform evidence. |

These are applicability assessments of upstream fixes, not claims of previously exploitable
product defects. No exploit reproduction or vulnerable-input generator is added.

## 4. Dependency, requirements and attribution

CMake remains ≥3.22, with no new desktop package declaration. `JUCEUtils.cmake` changes Windows
resource generation to configure time and preserves `Spatial;Fx` category order. This product's
configured categories are unaffected, but native VERSIONINFO/icon and symbol/package checks must
run. Android Gradle/Java/NDK changes and Projucer defaults do not configure these desktop builds.
Xcode 27's documented minimum target constraint is a potential native-toolchain issue, not
permission to raise the existing deployment floor. Fresh acquisition also succeeds on minimum-line CMake 3.22.6;
full product configuration remains blocked on the development prerequisites below.

Actual `JUCE.spdx.json` changes HarfBuzz **14.2.1 → 15.0.0**, Android Gradle wrapper
**8.14.5 → 9.5.1**, and JUCE-owned version metadata. No new package or changed licence expression
was found. The recorded scan compares **47 named licence/copyright/notice/readme files**, including
root `LICENSE.md` and the selected files beneath `modules/`, with no added/removed/changed file in that selection. This is
the exact scan scope, not the older worklog's differently scoped 66-file count.

The actual HarfBuzz `COPYING` still states “Old MIT”; its full attribution/disclaimer grant was
read. LunaSVG/PlutoVG licences, PlutoVG's transitive FreeType `FTL.TXT` and the embedded stb grants,
Steinberg/AudioUnit SDK, libpng/IJG/zlib/FLAC/Ogg/Vorbis and OpenGL notices retain their texts.
MP3/Opus/WebP implementation trees and exclusions remain unchanged. HarfBuzz's changed source
still needs target-object/native rendering confirmation; a patch version alone is not the audit.

The current `NOTICE`/inventory truthfully describe 9.0.4 and remain unchanged in this proposal.
On approved implementation, update their pin and target verification record and re-check actual
objects, including all six consumer compile definitions. Preserve the commercial JUCE arm,
existing owner licensing actions and distribution/notice decisions. No legal approval is inferred.

## 5. Executed checks and limits

Local environment: Linux x86-64, Debian GCC 14.2.0, CMake 3.31.6 and Ninja 1.11.1 installed into
a workspace-local tooling directory. No repository toolchain setting changed. The repository
setup script fails because `sudo` is absent; system apt update also fails permission checks.

| Check | Outcome |
|---|---|
| Official tags + detached checkouts + source/module/SPDX comparison | **Passed**, exact SHA verified |
| Existing current-dependency cache tests | **Passed**, 7 tests; these validate 9.0.4 behavior, not future 9.0.5 migration |
| Fresh isolated FetchContent/GIT_SHALLOW acquisition, CMake 3.22.6 and 3.31.6 | **Passed** in both independent fresh trees, actual clean checkout HEAD equals target SHA; 3.31.6 emits a probe-only CMP0169 deprecation warning because it populates without adding JUCE |
| Local 9.0.4 and 9.0.5 product configuration via existing escape hatch | **Failed — environment prerequisite**, both juceaide builds stop at missing `ft2build.h`; ALSA/fontconfig/EGL/GL development packages also missing |
| Portability, realtime, product identity, citations; checker self-tests | **Passed** on unchanged baseline source; documentation check passes after fetching existing remote tags |
| DSP/state suites, `AnamorphDspDump`, cross-version sessions/renders | **Unavailable locally**, build prerequisite prevents execution; source equality is not a numerical result |
| Target pluginval, sanitizers, TSan/realtime runtime, memcheck, fuzz, ABI/warnings, packaging/symbols | **Not run** for 9.0.5; full build unavailable and dependency modification gated |
| Native Windows/macOS Intel/Apple Silicon target execution | **Unavailable locally**, no native execution environment; later CI required |
| Level-5/DAW/native GUI acceptance | **Pending Human Acceptance** |

The initial docs failure was missing local release-tag refs, not a source or JUCE regression.
Fetching existing tags cleared it. GCC-14 ABI limitations and prior AU/pluginval/GUI incidents in
older upgrade/engineering worklogs remain historical; no tolerance, diagnostic or gate is relaxed.

### Remote baseline CI (9.0.4 only)

At the captured snapshot, [Build & Validate 41691099069](https://github.com/skyRolly/Anamorph/actions/runs/41691099069)
for baseline main was **in progress**. Linux, GCC-LTO, realtime, TSan, fuzz, Windows AVX2 A/B,
source/docs and cross-slice jobs had passed; Windows, macOS, native Intel, macOS native smoke and
sanitizers were pending. Optional package-selection/co-installation and merge-check were skipped.
Prior-main [41660239454](https://github.com/skyRolly/Anamorph/actions/runs/41660239454) and its MSVC/
CodeQL runs passed. None is target-9.0.5 evidence. The raw compact job/status snapshot is in the ledger;
later results must be recorded with their own exact source/run, not silently applied backwards.

## 6. Required execution after architecture approval

1. Implement only ADR-0071's pin/cache/test/provenance scope. Enable all applicable optional
   consumers for compile-definition inspection. Test fresh acquisition at supported CMake,
   reused 9.0.4 caches, each partial reset, full reset, custom pins and both local checkouts.
2. Freeze identical first-party source, compiler, flags, fixtures and configuration for each
   comparison. Use the committed `AnamorphDspDump` via `ANAMORPH_BUILD_DSPDUMP=ON`; run both
   `--self-check` invocations, compare complete outputs and retain identity/latency evidence.
   Also execute its existing transition scenarios and self-validating output parser. Compare
   within an architecture/build; retain the accepted cross-architecture numerical distinction.
3. Run full DSP/state suites, preserving rate/buffer/OS/reset/transport/bypass/Mix/non-finite,
   stereo and accepted mono-input behavior, metering, callback/restore and latency controls.
   Reuse the previous upgrade's real-processor state comparison procedure; capture 9.0.4
   sessions/presets, compare 9.0.5 re-save, all parameter bits/metadata, renders and latency.
4. Run existing required CI across Linux, Windows, Apple Silicon and native Intel: VST3,
   macOS AU, enabled Standalone, both pluginval modes ×3 at `ANAMORPH_PLUGINVAL_STRICTNESS`
   (currently 10); all existing sanitizer/memcheck/TSan/realtime/fuzz, source/docs/citation,
   warning/ABI/static-analysis/dependency and package/symbol gates. Inspect actual consumed
   checkout HEAD and compile source paths. Classify failed/skipped/pending/cancelled separately.
5. Investigate any changed number/state/host behavior before acceptance. Do not widen tolerances,
   rewrite fixtures, suppress diagnostics, change formats/platforms or remove negative controls.

## 7. Exact native/manual acceptance worksheet

Every item below is **Pending Human Acceptance for JUCE 9.0.5**. Record tester/date, exact
source SHA and artifact SHA-256, OS/CPU/architecture, DAW/version, format, device, rate/block,
old/new configuration and individual observations. Use current Level-5 and release-compatibility
procedures; older approval dates do not complete this worksheet.

- [ ] On Linux x86-64 and Windows x64 VST3, macOS native Intel and Apple Silicon VST3/AU:
      load the 9.0.4 session, play and reload it under 9.0.5; compare automation, factory/user
      presets, Unicode/XML-special names, A/B detour/return and Undo/Redo cycles.
- [ ] Listen to the existing Level-5 A–G programme across every widening algorithm, Drive,
      Multiband/Solo, Mono Maker, Level Match and Mix 0/intermediate/100%; compare bypass and
      transitions/reset/transport at matched gain. Check stereo and supported mono input.
- [ ] Verify host PDC and rendered impulse alignment for each selected OS factor, with and
      without nonlinear work; preset/A-B/restore/re-prepare must retain the established latency.
      Exercise 44.1/48/96/192 kHz and representative 64/256/1024 sample blocks where the host supports them.
- [ ] Repeatedly open/close/recreate editors during playback and session restore; exercise
      States155–157 lifetime scenarios, scale changes, spectral gestures, popup dismissal,
      keyboard/escape, preset browsing and pending modal/file callbacks at teardown.
- [ ] Move the Windows VST3 between different-DPI monitors in a per-monitor-aware host such as
      Reaper; test each product UI scale. Check popup shielding/transparency, tooltip placement,
      focus, accessibility labels/keyboard and no duplicated menu callbacks.
- [ ] On native macOS and Windows verify OpenGL creation, context loss/teardown, repainting and
      display-scale changes. On Linux verify CPU rendering/X11 embedding and popup focus under
      the normal window manager. A headless editor test does not complete this item.
- [ ] Check enabled Standalone launch, device chooser/error/cancel, device reopen and shutdown;
      check platform packages/icons/install/uninstall and VST3-only Linux installation with
      portable Standalone under their existing disposable-machine procedures.

## 8. Review scope, remaining blockers and rollback

The draft PR is an investigation/ADR packet. It requests approval to implement ADR-0071's
precise scope; it is not an implemented upgrade and cannot be merged as one. Sibling proposal:
[Anabasis upgrade branch](https://github.com/skyRolly/Anabasis/tree/codex/juce-905-upgrade).
Each product owns its branch/ADR/PR and no shared source is introduced.

Blockers: human architecture decision; Linux development prerequisites/native toolchains;
all unexecuted numerical/state/native target gates; final Level-5 acceptance. After approval,
current pin homes to update include CMake/cache/tests, dependency/build/CI/testing and architecture
records, README/build instructions, `.github/dependabot.yml` commentary, `NOTICE`, inventory,
`TRADEMARKS.md` and commercial provenance. Search the whole tree for `9.0.4`/`3fb5cbc` and distinguish
live contracts from dated history; do not blanket-replace release records. Changelog/release
readiness must describe only actual implementation and its evidence. This packet changes no
user-visible behavior, so no changelog/release claim is added.

Rollback: before implementation, withdraw the proposal; after implementation, revert only this
product's upgrade commit(s), restore 9.0.4 pin/guards/tests/provenance, reset both cached pin fields,
verify `3fb5cbc0340758596dac39f7c8de47d562b4896e`, rebuild and re-run compatibility checks.
No merge, version bump, tag movement, release publication or asset replacement is authorized.


## Proposal documentation validation

Final documentation and citation checks run after adding the ADR/review packet, with their
checker self-tests. The exact exits and summaries are retained in `juce905-review/evidence.json`.
Existing citation re-aim bookkeeping notes are informational; no source anchor or diagnostic
is suppressed. These checks validate the proposal documents, not a target plugin build.
Python 3.12 also emits two pre-existing invalid-escape SyntaxWarnings in `check-docs.py`;
the unchanged checker exits successfully after the existing remote release tags are fetched.
