# ADR-0061 — JUCE dependency upgrade 9.0.3 → 9.0.4

**Status:** **Accepted** — **Architecture Review APPROVED by the repository owner,
2026-10-21 UTC**, in the explicit final owner release attestation. All pending ADR approvals
are granted. Final **0.9.9 / JUCE 9.0.4**: Level-5 **PASS (A–G)**, compatibility **8/8 PASS**,
required human gates **COMPLETE**; planned release **2026-10-22**. The full attestation and
NOT RECORDED details are in `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md`
§Final owner release attestation. Supersedes ADR-0060; its historical evidence is retained.

## Context

PR #160 merged JUCE 9.0.3 into main at `e0532bc`. On 2026-10-21 the owner requested a safe
upgrade to 9.0.4, fixes for any resulting issues, synchronized documentation, and a 2026-10-22
release date, all inside **0.9.9**. No release tag has been created.

## Problem

Upgrade the framework without changing DSP output, latency, parameter semantics, state fields,
or the compiled third-party component set. Verify the changed XML/String and message-manager
paths, and prevent an existing 9.0.3 CMake cache from silently retaining the previous framework.

## Options

- Keep 9.0.3: does not satisfy the commissioned upgrade.
- Fetch mutable tag `9.0.4`: loses the immutable-pin guarantee established by ADR-0022.
- Fetch the tag's immutable commit and retain the audited configuration: **chosen**.

## Decision

- Pin **9.0.4** to **`3fb5cbc0340758596dac39f7c8de47d562b4896e`**
  (`CMakeLists.txt:70-72`). The lightweight upstream tag, JUCE's own project version and module
  declarations agree. Keep `GIT_SHALLOW`, CMake ≥ 3.22 and C++23.
- Keep `JUCE_USE_MP3AUDIOFORMAT=0`, `JUCE_USE_OPUS=0` and `JUCE_USE_WEBP=0` on all six targets.
  No codec or system-library dependency is added. All fifteen consumed module declarations
  differ only in `version:`. The new Linux `NEEDS_OPENGL_ES` option defaults off; its optional
  `egl`/`glesv2` probe does not add a dependency to the desktop OpenGL configuration.
- Extend `cmake/CheckJuceCache.cmake` to refuse either cached field from **9.0.2 or 9.0.3**
  before FetchContent. Print the exact two-field reset command. Preserve other overrides;
  intentional older-framework comparisons use `ANAMORPH_JUCE_PATH`.
- Change no first-party C++ behavior. Source edits only re-aim JUCE citations and clarify the
  XI2 registration comment. The new master-device touch masks still exclude click-ignoring peers.
- Keep the current release at **0.9.9** and change its planned date to **2026-10-22**.

## Exposure and verification

`BREAKING_CHANGES.md` is byte-identical between the tags. The lack of a listed breaking change
is not the compatibility proof; the relevant implementation changes are audited separately.

| Area | Evidence / disposition |
|---|---|
| DSP, parameters, APVTS | `juce_dsp`, `juce_audio_processors`, `juce_data_structures` and `juce_audio_utils` change only their module version. APVTS implementation SHA-256 stays `94760f6858acf7fcf52915504cc24702cdea9c031d9a9593919de108884a1ad6`; ADR-0057's `seq_cst` store precondition holds. |
| Engine output and latency | One frozen first-party tree, `e0532bc`, built against both clean JUCE checkouts: **32/32 scenarios bit-identical**, reported latency included; both `--self-check` runs pass. |
| XML and strings | `XmlDocument` is unchanged; `XmlElement` changes formatted mixed-content output. Anamorph's state uses attribute-only ValueTrees. String construction now bounds allocation/copy consistently for malformed UTF-8; embedded NUL still terminates decoding, so the existing state/preset boundaries remain necessary. A real 9.0.3 session re-saves byte-exactly and renders identically under 9.0.4. Focused String/MemoryBlock probes pass ASan + UBSan; full suite and XML differential results are in the worklog. |
| Threading / wrappers | VST3, AU and Standalone wrappers are byte-identical to 9.0.3. MessageManagerLock internals change; message-thread identity reads now use an atomic instead of a mutex. Anamorph adds no thread, callback path, or atomic ordering. State/concurrency tests and pluginval both modes ×3 pass on Linux. |
| GUI / devices | Windows DPI/placement, X11/Xwayland input, popup menus, OpenGL function checks and Standalone audio-device setup change. The owner attests the complete Level-5 scope PASS; specific display, host and device observations are NOT RECORDED. The MIDI disconnection API is not used by Anamorph. |
| Licences / contents | All **66** licence/readme files checked are byte-identical. SPDX changes only JUCE version/namespace metadata; third-party packages and licence facts do not move. `NOTICE` and the inventory follow the new pin. |
| Cached builds | Seven regression tests cover new/reused caches, both partial resets, retained custom overrides, local comparisons and legacy 9.0.2 fields. The real build wrapper refuses an existing 9.0.3 cache before fetching. |

Commands, compiler details and the remaining validation results are in
`worklogs/JUCE904_UPGRADE_0.9.9.md`.

## Citation map

Live citations move to 9.0.4; accepted ADRs and dated worklogs keep their historical numbers.
Representative mappings (file names below refer to the JUCE tree):

| File | 9.0.3 → 9.0.4 |
|---|---|
| `juce_PopupMenu.cpp` | `333–336 → 348–351`, `365–500 → 380–515`; later bodies move by 18, e.g. `806–816 → 824–834`, `1422–1425 → 1440–1443`, `2291–2292 → 2309–2310` |
| `juce_String.cpp` | first-NUL loop `132 → 99` (rewritten as `createFromCharPointerWhile`); codepage fallback `2030–2034 → 2017–2021` |
| `juce_XmlElement.cpp` | text-node helper `966–971 → 970–975`, `914–917 → 918–921` |
| `juce_Windowing_linux.cpp` | focus flag `678 → 681`, assignment `687 → 690` |
| `juce_Windowing_windows.cpp` | touch opt-in `2612–2619 → 2645–2652` |
| `juce_XWindowSystem_linux.cpp` | XI2 availability `2299–2306 → 2320–2327`; touch dispatch `4177–4195 → 4218–4236`, clear `4213 → 4254`; mask setup rewritten at `645–659` |

## Consequences

The normal configure/build path now rejects a stale 9.0.3 pin instead of producing a 0.9.9 binary
against it. One reset of both cache entries selects the new default; no override is rewritten.
Saved-session formats, parameter IDs and latency rules remain the same.

**Required human gates COMPLETE (owner attestation, 2026-10-21 UTC):** Architecture Review
**APPROVED**, final 9.0.4 Level-5 **PASS (A–G)**, compatibility **8/8 PASS** including items 5/7.
This is the owner's explicit approval, not an inference from CI or a merge. Actual human test
date, exact artifact, DAW, OS, device, session and individual measurements are **NOT RECORDED**.

## Related code

`CMakeLists.txt`, `cmake/CheckJuceCache.cmake`, `tests/juce_cache_tests.py`,
`tests/dsp_dump.cpp`, `tests/state_tests.cpp`, `tests/xml_boundary_differential.cpp`.

## Evidence and confidence

**Verified:** upstream tags/source comparison, immutable pin in commit `5fcf153`, module metadata,
licence files, APVTS store, cache regression tests and the twin dump. Full build, DSP/state suites, the cross-version session, the XML differential, focused sanitizer
probes and pluginval both modes ×3 are recorded in the worklog. **Owner-attested PASS:** final 0.9.9 / JUCE 9.0.4 human acceptance, recorded 2026-10-21 UTC.
Specific host/device/display observations are **NOT RECORDED**; no result is inferred from PR #160's merge.
