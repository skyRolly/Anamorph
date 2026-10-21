# JUCE 9.0.3 → 9.0.4 upgrade — measurement record (0.9.9)

Companion evidence for **ADR-0061** (`Proposed`). Requested 2026-10-21: upgrade safely to
9.0.4, fix resulting issues, synchronize documentation, retain 0.9.9 and date it **2026-10-22**.
PR #160 is already on main (`e0532bc`). No tag or release is created by this work.

## 1. Pin and source audit

| | Evidence |
|---|---|
| From | `9.0.3` = `be29c81492b6151c8ea8d14c840e1311963b3a83` |
| To | `9.0.4` = `3fb5cbc0340758596dac39f7c8de47d562b4896e` |
| Tag | `git ls-remote --tags https://github.com/juce-framework/JUCE.git '9.0.4*'` returns that SHA with no annotated-tag peel |
| Corroboration | clean detached checkout; JUCE's project and all module versions say 9.0.4 |
| Breaking changes | `BREAKING_CHANGES.md` byte-identical; `CHANGE_LIST.md` adds the 9.0.4 section |
| Module requirements | fifteen consumed module declaration blocks differ only in `version:` |
| Licences | all 66 licence/readme files compared byte-identical; SPDX differs only in JUCE version/namespace metadata |

The upstream diff changes 165 files. Relevant production paths:

- `juce_core`: bounded malformed-UTF-8 String construction, safe empty/moved-from MemoryBlocks,
  null OutputStream input handling, and XmlElement formatted whitespace. `XmlDocument` is unchanged.
- `juce_events`: MessageManagerLock acquisition, recursion and cancellation; atomic message-thread ID.
- `juce_gui_basics`: Windows display scaling/window placement, X11/Xwayland master-pointer grabs,
  drag/drop, system tray and popup behavior. OpenGL function checks also change.
- `juce_audio_devices`: MIDI scheduling/disconnection fixes and more robust Standalone device setup.
  Anamorph does not use the new MIDI disconnection API.
- `juce_dsp`, `juce_audio_processors`, `juce_data_structures`, `juce_audio_utils`: module version only.
  The VST3, AU and Standalone wrappers are byte-identical; LV2 changes do not reach built formats.
- The new Linux `NEEDS_OPENGL_ES` is off by default. The GLES pkg-config probe is optional;
  desktop OpenGL still uses the same libraries. No setup-script package change is needed.
- Opus config gains platform guards, but Opus, WebP and MP3 remain disabled on all six targets.
  No third-party package, version or licence changes in the SPDX inventory.

ADR-0057 precondition 2 is re-verified: `juce_AudioProcessorValueTreeState.cpp` SHA-256 is
`94760f6858acf7fcf52915504cc24702cdea9c031d9a9593919de108884a1ad6` at both tags; its parameter
store remains `seq_cst`.

## 2. Frozen-tree engine comparison

One detached first-party worktree at **`e0532bc`**, compiled twice with **GCC 14.2.0**, CMake 3.31.6,
Ninja 1.12.1, Release and the shipped hardening/codec flags. Both configurations use
`-DANAMORPH_BUILD_DSPDUMP=ON -DANAMORPH_JUCE_PATH=<clean checkout>`; only the JUCE checkout differs.
Build the `AnamorphDspDump` target, run `--self-check`, then compare ordinary output with `diff -u`.

| Result | Measured |
|---|---|
| 9.0.3 `--self-check` | 32 scenarios, all repeatable and all distinct |
| 9.0.4 `--self-check` | 32 scenarios, all repeatable and all distinct |
| Output comparison | empty diff: **32/32 hashes and reported latencies identical** |

The first-party C++ changes in this upgrade are comments only, not a different engine tree.
The final citation audit also refreshed unchanged File, macOS-modifier and AudioProcessor evidence
and the realtime-annotation commentary against the 9.0.4 checkout.

## 3. Existing-cache regression

The previous guard recognized only 9.0.2. An existing 9.0.3 cache would therefore ignore the
9.0.4 defaults, the same failure PR #160 fixed for the preceding bump. The guard now recognizes
either field from 9.0.2 or 9.0.3, refuses configuration before FetchContent and prints:

```bash
cmake -S /path/to/Anamorph -B /path/to/Anamorph/build -UANAMORPH_JUCE_TAG -UANAMORPH_JUCE_VERSION
```

`python3 tests/juce_cache_tests.py`: **7 tests pass**, including retained custom overrides,
local older-JUCE comparisons, partial resets and legacy 9.0.2 fields. The actual `scripts/build.sh`
was run against the environment's existing 9.0.3 cache: refused before fetching. The reset command
then fetched 9.0.4 at the expected commit. The normal wrapper builds that tree.

## 4. Cross-version session probe

A probe using the real processor and the existing state target's compiled objects writes a
9.0.3 session before rebuilding. All **36** parameters have varied normalized values; InternalState
settings are nondefault; A/B slots differ; preset metadata contains Unicode and XML-special characters.

- Size: **12,157 bytes**, document depth **3**; A/B payloads **2,632 / 2,631 bytes**, depth **2**.
- SHA-256: `7c4dea4165336bdf22ae5e2149d29cac0f26233a14b78762151ab0fad950818e`.
- 9.0.3 re-read/re-save: byte-identical.
- 9.0.3 read/render: FNV-1a `5a1224f55fa07cde`, 65,536 stereo frames, reported latency **6**.
- 9.0.4 re-read/re-save: **byte-identical**, same `5a1224f55fa07cde` render hash and latency **6**.

The probe calls `setStateInformation`/`getStateInformation`, not a replacement serializer.
It renders after re-saving the restored blob so measurement state cannot alter that byte comparison.

## 5. Build and runtime gates

| Gate | Result |
|---|---|
| Normal Release wrapper, VST3 + Standalone + both suites | pass; fetched commit verified, final wrapper exit 0 |
| DSP suite | **944 checks, 0 failures** |
| State suite | **5600 checks, 0 failures** |
| Focused core ASan + UBSan probe | 12 malformed UTF-8 constructor cases and moved/empty MemoryBlock checks pass; no sanitizer findings |
| XML-boundary differential probe | 18 named + 7 legitimate shapes; 1500 generated cases, no false negatives; crash oracle live at 3000 levels |
| pluginval 1.0.4, strictness 10 (read from build.yml), deterministic ×3 / randomise ×3 | all six pass on first attempt, VST3 under xvfb |
| Symbol/component probes | no webp/opus/ogg/vorbis/FLAC/mp3 symbols in VST3 or Standalone; GCC 14 first-party warning sets identical |
| Documentation / citations / source lints | docs self-test 700 cases + 159 files; portability/realtime/dispatch/state-coverage gates and checker self-tests pass; citations pass against main |
| Cross-platform / shipping-toolchain CI | the PR workflow verifies Clang 22 Linux, macOS and Windows; local measurements above are GCC 14 on Linux |

The local environment uses GCC 14, whose `__cxa_call_terminate` import requires `CXXABI_1.3.15`,
above the repository's declared `1.3.14` floor. This was already measured before this upgrade
(PR #160; `CI_CD.md` explains the compiler difference). The shipping Linux job uses Clang.
The ABI policy is not raised to accommodate this local compiler. `scripts/preflight.sh` exits 1
at that ABI check; it is not reported as fully green. All earlier lint/self-test stages pass,
and the citation and runtime gates after its early exit were run separately.

## 6. Documentation and human checks

Current pin homes and the planned release date move together. Historical measurements and accepted
ADRs keep their dates/versions. ADR-0060 is superseded; ADR-0061 records the new decision and citation map.
Live JUCE references move to the actual 9.0.4 code, including the rewritten first-NUL loop and XI2 masks.

Architecture Review, Level-5 audition groups A–G, host-matrix attestation and automation-playback
attestation remain **open** for the 9.0.4 build. Historical 9.0.2/9.0.3 records do not certify it.
