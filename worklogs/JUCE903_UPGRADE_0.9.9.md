# JUCE 9.0.2 → 9.0.3 upgrade — measurement record (0.9.9)

Companion evidence for **ADR-0060** (`Proposed`). Everything below was measured on this tree on
2026-09-29, with the command or search that produced it, so a reader can repeat it. The bump ships inside
0.9.9 on the owner's instruction of 2026-09-29, which also moved the 0.9.9 release date to 2026-10-01.

## §1. What the pin moves to, and how the SHA was established

| | |
|---|---|
| From | `9.0.2` = `72782788ce18c2d4d760b28e0921d6ffc6431102` |
| To | `9.0.3` = `be29c81492b6151c8ea8d14c840e1311963b3a83` |
| Source | `git ls-remote --tags https://github.com/juce-framework/JUCE.git '9.0.3*'` — one line, no `^{}` peel: the tag is lightweight, so the ref is the commit |
| Corroboration 1 | the checkout's `git rev-parse HEAD` = `be29c81…`, clean working tree |
| Corroboration 2 | the tree's own `CMakeLists.txt:37`: `project(JUCE VERSION 9.0.3 LANGUAGES C CXX)` |
| Corroboration 3 | `modules/juce_core/system/juce_StandardHeader.h:44` — `JUCE_BUILDNUMBER` 2 → 3 |

## §2. Exposure — the two breaking changes, checked by name

`BREAKING_CHANGES.md` at 9.0.3 has one entry under `# Version 9.0.3` and one entry that the 9.0.2 tag's
own copy did not have, added under `# Version 9.0.2`.

| Entry | Search | Result |
|---|---|---|
| `SystemStats::isOperatingSystem64Bit()` reports the OS | `grep -rn isOperatingSystem64Bit src tests` | nothing |
| `ThreadPool::addJob` takes one callable | `grep -rn ThreadPool src tests` | nothing |

## §3. The two things that were NOT inert — Opus and WebP

`juce_audio_formats.h:110-112` (`JUCE_USE_OPUS`, default 1) and `juce_graphics.h:127-129`
(`JUCE_USE_WEBP`, default 1) are new at 9.0.3. The MP3 block moved from `juce_audio_formats.h:110-112` to
`:134-136`. JUCE's `JUCEModuleSupport.cmake` is byte-identical between the tags and globs every top-level
module source (`:224`), so all thirteen new top-level translation units are compiled in every target that
links the two modules: `juce_audio_formats_{ogg,opus,opusfile}.c` and ten `juce_graphics_libwebp_*.c`.

Pinned to 0 on all six targets (`CMakeLists.txt:515-516` in the `Anamorph` block), measured on the objects
of the local 9.0.3 build with `nm --defined-only`:

| Object | Defined symbols |
|---|---|
| `juce_audio_formats_opus.c.o`, `juce_audio_formats_opusfile.c.o` | 1 each (a hidden placeholder) |
| each of the ten `juce_graphics_libwebp_*.c.o` | 0 |
| `juce_audio_formats_ogg.c.o` | 71 (libogg, all hidden — the same set 9.0.2 compiled inside `juce_audio_formats.cpp`) |
| the same ten libwebp objects under `JUCE/tools/extras` (`juceaide`, built with JUCE's defaults) | 15–554 each — the build helper, not shipped |

Shipped binaries (`nm -C`, case-insensitive): the Linux `Anamorph.so` (VST3) and the Standalone contain
**0** symbols matching `webp`, `opus`, ` ogg_`, `vorbis`, `FLAC` or `mp3`. Under LTO and `--gc-sections`
none of the codecs survives into the product at all, as was already the case at 9.0.2; the licence notices
for FLAC and Ogg Vorbis describe what is *compiled*, which is the conservative reading.

## §4. Rule-2 evidence — the twin dump

`tests/dsp_dump.cpp`, `-DANAMORPH_BUILD_DSPDUMP=ON`, Release, GCC 13.3, one frozen `git worktree` of
`756a5c2` (so neither build could see an edit made while the other ran), built twice with
`-DANAMORPH_JUCE_PATH` at the two clean checkouts.

| Run | Result |
|---|---|
| 9.0.2 vs 9.0.3, new flags at JUCE's defaults | `diff` empty — 32/32 scenarios, hash and latency |
| 9.0.2 vs 9.0.3, `JUCE_USE_OPUS=0` / `JUCE_USE_WEBP=0` (as shipped) | `diff` empty |
| 9.0.3 defaults vs 9.0.3 pinned | `diff` empty |
| `--self-check`, every run | "32 scenarios, all repeatable and all distinct" |

First-party compiler warnings of the two dump builds, normalised to file and message: identical sets
(`-Wswitch-enum` 1, `-Wmisleading-indentation` 1, `-Wfloat-equal` 5, `-Wsign-conversion` 2).

## §5. Suites and pluginval on the 9.0.3 build

The working tree with the pin moved, configured with `-DANAMORPH_JUCE_PATH=<9.0.3 checkout>`, Release,
Ninja, built at `-j 2` (a `-j 4` build was OOM-killed on this 15 GB host while other jobs ran; not a
source problem).

| Gate | Result |
|---|---|
| `AnamorphTests` | 944 checks, 0 failures, exit 0 |
| `AnamorphStateTests` (isolated `HOME` / `XDG_CONFIG_HOME`) | 5600 checks, 0 failures, exit 0 |
| `scripts/run-pluginval.sh 10 deterministic vst3` (pluginval 1.0.4, `xvfb`) | ALL 3 passes succeeded, seed `0x1`, attempt 1 |
| `scripts/run-pluginval.sh 10 randomise vst3` | ALL 3 passes succeeded, attempt 1 |

One diagnostic the local GCC build shows that CI does not gate: `-Wstringop-overflow` ×4 at the LTO link
of the VST3, in libstdc++'s `vector::insert` as inlined into `juce_OpenGLHelpers.cpp:282`
(`initEGLContext`) via `juce_OpenGLContext.cpp:937`. The 9.0.3 diff of `juce_OpenGLHelpers.cpp` does not
touch that region (lines 270-290 identical); third-party, not first-party.

## §6. The module-level picture

| Module | 9.0.2 → 9.0.3 |
|---|---|
| `juce_audio_processors`, `juce_dsp`, `juce_audio_basics`, `juce_data_structures`, `juce_audio_utils` | `version:` line only |
| `juce_audio_processors_headless` | LV2 / VST3 **hosting** only — compiled out |
| `juce_audio_plugin_client` | VST3: one ARA destructor line (compiled out); `juce_VST3ModuleInfo.h`: one destructor; AU: `SetBusCount` loop (unreachable — no `canAddBus`/`canRemoveBus` override); Standalone byte-identical |
| `juce_audio_devices` (Standalone) | ALSA sample-rate/channel handling incl. a PipeWire channel-mix workaround; `AudioDeviceManager` requests 44.1 kHz when neither the setup nor the device names a rate; Windows MIDI Services preview 7 behind `JUCE_USE_WINDOWS_MIDI_SERVICES` (default 0) |
| `juce_audio_formats` | Opus added; Ogg split into `codecs/ogg` + `codecs/vorbis`, libogg 1.3.4 → 1.3.6; FLAC byte-identical except the moved `juce_flac_config.h` |
| `juce_graphics` | libwebp added; image-format registration and an SVG bounding box; fonts, shaping and renderers byte-identical |
| `juce_opengl` | frame buffer / cached image honour a texture larger than requested; bounded `glGetError` drain |
| `juce_gui_basics` | Windows placement/focus; X11 `WM_DELETE_WINDOW` under modal components, XInput touch modifiers; macOS peer |
| `juce_events` | Linux message queue: one pipe write per drain (`socketSignalled`) instead of one per message |
| `juce_core` | `ThreadPool` API, `Thread::sleep (Milliseconds)`, `SystemStats`, `ListenerList` reference overloads, `WaitableEvent` assertion, macOS network cancel; `juce_File.cpp` changed in its unit tests only; `xml/` byte-identical |

ADR-0057 precondition 2: `juce_AudioProcessorValueTreeState.cpp` byte-identical (sha256 `94760f68…`);
`unnormalisedValue = newValue;` at `:155` into a `std::atomic<float>` declared at `:208`.

## §7. Rule-3 evidence — licences and attribution

- `JUCE.spdx.json`, package by package: every JUCE module moves 9.0.2 → 9.0.3; `libogg` 1.3.4 → 1.3.6;
  new `Opus` 1.6.1, `opusfile` 0.12-59-g6dfd29e, `libopusenc` 0.3, `libwebp` 1.6.0 (all BSD-3-Clause, all
  pinned out). No other package changes.
- Licence files: the Ogg Vorbis text moved from `codecs/oggvorbis/libvorbis-1.3.7/COPYING` to
  `codecs/vorbis/COPYING`, `cmp`-identical, and `codecs/ogg/COPYING` is the same text again. Every other
  cited licence file is byte-identical; `diff -rq` of the VST 3 SDK directory is empty.
- `NOTICE`'s Ogg Vorbis block already carries that text. Only its pin line moved.
- Pin homes updated: `THIRD_PARTY_LICENSES.md`, `NOTICE`, `README.md`, `docs/procedures/BUILD.md`,
  `docs/procedures/TROUBLESHOOTING.md`, `.github/dependabot.yml`, `TRADEMARKS.md`,
  `docs/COMMERCIAL_STATUS.md`, plus the glossed pin citations `scripts/check-citations.py` enforces
  (`HANDOVER.md`, `COMPATIBILITY_MATRIX.md`, `DEPENDENCY_POLICY.md`).

## §8. Citations into JUCE

Every `juce_*.{h,cpp,mm,c}:NNN` citation in the tracked tree was collected, attributed to its file (bare
`:NNN` continuations to the most recently named file), and mapped between the trees with
`difflib.SequenceMatcher` over the two copies of each changed file. 55 cited files are byte-identical.
Twelve changed; in six of them cited lines moved. Each mapped line was then compared by content. The map,
and which accepted ADRs keep 9.0.2 numbers, is ADR-0060 §Citation map. One pre-existing error surfaced:
`src/PluginProcessor.h` cited `CallPrepareToPlay::no` at `:3470`, a blank line at 9.0.2; it now cites
`:3468`.

## §9. What is left, and whose it is

The owner's: the Architecture Review of ADR-0060, the Level-5 audition of the 9.0.3 build
(`LEVEL5_AUDITION.md` §Scope for 0.9.9, groups A–G), and re-attestation of compatibility-checklist
items 5 and 7 on it. Nothing else in this record is waiting.
