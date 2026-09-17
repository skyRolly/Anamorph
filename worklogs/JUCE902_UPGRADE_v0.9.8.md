# JUCE 9.0.1 → 9.0.2 upgrade — measurement record (v0.9.8)

Companion evidence for **ADR-0054**. Everything below is a measurement taken on this tree, with the
command that produced it. Where a question was answered "no exposure", the search that answered it is
named, because "I looked and found nothing" is only evidence if the reader can repeat the looking.

## §1. What the pin moves to, and how the SHA was established

| | |
|---|---|
| From | `9.0.1` = `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` |
| To | `9.0.2` = `72782788ce18c2d4d760b28e0921d6ffc6431102` |
| Source | `git ls-remote --tags https://github.com/juce-framework/JUCE.git` |
| Corroboration 1 | the fetched tree's own `CMakeLists.txt`: `project(JUCE VERSION 9.0.2 LANGUAGES C CXX)` |
| Corroboration 2 | `modules/juce_core/system/juce_StandardHeader.h:44` — `JUCE_BUILDNUMBER` 1 → 2 |
| Corroboration 3 | the configure banner: `-- Anamorph: fetching JUCE 9.0.2 (72782788…) from github.com/juce-framework/JUCE` |

JUCE's release tags are **lightweight**, so `refs/tags/9.0.2` *is* the commit — there is no `^{}`
peeling step, and the pin is the ref's own object id. (LLVM's are annotated, which is why the Clang
work in the same round does peel; the two are not the same kind of tag and the difference matters.)

## §2. Exposure — the three breaking changes, checked by name

`BREAKING_CHANGES.md` in the 9.0.2 tree has **one** entry under `# Version 9.0.2`. It also carries
**two entries under `# Version 9.0.1` that the 9.0.1 tag's own copy of that file did not have**
(2 `## Change` sections there, 4 here), so for this repository all three arrive with this bump and
all three were checked.

```
grep -rn "getMidiInputSelectorListBox\|OpenGLImageType\|setImageCacheSize\|\
AudioDeviceSelectorComponent\|getFrameBufferFrom" src/ tests/     ->  no matches
```

| Change | Verdict |
|---|---|
| `AudioDeviceSelectorComponent::getMidiInputSelectorListBox` removed (a screen-reader accessibility fix: the `ListBox` base class is gone) | **Unreachable.** Anamorph builds no device-selector UI; the Standalone wrapper's is JUCE's own and Anamorph never asks it for the list box. |
| `OpenGLImageType::create()` now honours `Image::SingleChannel` | **Unreachable.** Neither the type nor `getFrameBufferFrom` appears in the tree. |
| `OpenGLContext::setImageCacheSize()` now counts **bytes** (default raised 8 MB → 32 MB so the effective cache is unchanged) | **Unreachable.** Anamorph's entire use of `OpenGLContext` is `setContinuousRepainting(false)`, `attachTo(*this)`, `detach()`, `isAttached()` and `triggerRepaint()` — `src/PluginEditor.cpp:307, 321, 790, 1990-1991`; `src/PluginEditor.h:6, 590`. It never sets a cache size, so it takes the unchanged default. |

## §3. The one thing that was NOT inert — `JUCE_USE_MP3AUDIOFORMAT`

Not a documented breaking change; found by diffing the module headers.

```
9.0.1  juce_audio_formats.h:117-119   #ifndef JUCE_USE_MP3AUDIOFORMAT / #define … 0 / #endif
9.0.2  juce_audio_formats.h:110-112   #ifndef JUCE_USE_MP3AUDIOFORMAT / #define … 1 / #endif
```

The same edit deleted the 11-line **IMPORTANT DISCLAIMER** that stood above it (*"NOT guaranteed to
be free from infringements of 3rd-party intellectual property"*).

Anamorph links `juce_audio_utils`, so `juce_audio_formats` **is** compiled. Taking the new default
would have compiled `juce_MP3AudioFormat.cpp`'s body into every shipped binary — dead code, since
`grep -rn "AudioFormatManager\|AudioFormatReader\|registerBasicFormats\|AudioFormat" src/ tests/`
matches **nothing at all** — and it would have falsified a sentence in a **shipped** document,
`THIRD_PARTY_LICENSES.md` §"Present but not compiled": *"Anamorph therefore ships no MP3 decoder."*

`JUCE_USE_MP3AUDIOFORMAT=0` is therefore pinned on all six targets carrying the `JUCE_*` contract
(`CMakeLists.txt:505, 536, 579, 619, 660, 701`). **The flag is the no-op; its omission would have
been the change.** That is the whole of the build diff this bump required.

## §4. Rule-2 evidence — the twin dump

Built from ONE source tree against TWO JUCE checkouts with otherwise identical flags, per
`docs/procedures/TESTING.md` §Proving a dependency bump is bit-identical: Release, Ninja,
GCC 13.3.0, `-DANAMORPH_BUILD_DSPDUMP=ON -DANAMORPH_BUILD_TESTS=OFF -DANAMORPH_BUILD_STANDALONE=OFF`,
`-DANAMORPH_JUCE_PATH=<checkout>`.

```
run 9.0.1 exit=0 lines=36
run 9.0.2 exit=0 lines=36
diff dump-9.0.1.txt dump-9.0.2.txt   ->  empty
```

- **32 scenarios** (4 algorithms × 4 oversampling factors × M/S off/on), 48 kHz / 512 samples,
  120 blocks of fixed-seed noise then 120 of digital silence; FNV-1a over **every output byte**, with
  the engine's **reported latency** in its own column.
- The harness's `--self-check` printed *"32 scenarios, all repeatable and all distinct"* on **both**
  sides, so the instrument discriminates — this is not the 8.0.14 → 9.0.0 failure mode where a
  scenario set left `algoAmount` at its identity default and hashed everything the same.
- **Run twice**: once before the `JUCE_USE_MP3AUDIOFORMAT` pin and once after. Bit-identical both
  times.
- Because the latency column is inside the hash, `LATENCY_MODEL.md`'s **2× = 4, 4× = 6, 8× = 6** row
  is *proven* unchanged rather than re-asserted.

## §5. Why the dump came out identical — the module-level mechanism

`diff -rq` over `modules/`, then per-module comparison of the
`BEGIN_JUCE_MODULE_DECLARATION` blocks:

- **All fifteen modules Anamorph builds moved only their `version:` field.** No
  `linuxPackages`, `OSXFrameworks` or `windowsLibs` value changed, so `scripts/setup-linux.sh` is
  untouched and `libegl-dev` (ADR-0022) remains the Linux GL requirement.
- **`juce_dsp` and `juce_audio_processors` contain no other change at all** — the module header is
  the only differing file in each. The DSP primitives and the whole `AudioProcessor` / APVTS /
  parameter surface are literally the same code at both pins.

Files this repository cites by path and line, each compared byte-for-byte — **all identical**:

```
juce_audio_processors_headless/processors/juce_AudioProcessor.cpp        (TESTING.md:434)
juce_audio_processors_headless/processors/juce_AudioProcessorParameter.h (check-dispatch.py's premise)
juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.{h,cpp} (ADR-0036, RISK-009)
juce_gui_basics/mouse/juce_MouseInputSource.cpp                          (KNOWN_ISSUES:667)
juce_gui_basics/desktop/juce_Desktop.cpp                                 (KNOWN_ISSUES:705)
juce_gui_basics/widgets/juce_Slider.cpp                                  (ADR-0053's wheel arithmetic)
juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h        (THREADING_POLICY:260)
juce_dsp/processors/juce_Oversampling.cpp, juce_dsp/filter_design/juce_FilterDesign.cpp
```

## §6. What DID change in modules Anamorph builds

Stated rather than implied, because three of these are on paths the product uses and **no headless
gate covers what they look like**:

| Module | Change | Reaches Anamorph? |
|---|---|---|
| `juce_opengl` | context, image, texture, framebuffer, graphics context, `juce_gl.h` | **Yes** — the editor attaches an `OpenGLContext`. Rendering path. |
| `juce_graphics` | `detail/juce_SimpleShapedText.cpp` — glyph **cluster** counting (`numGlyphsInCluster` replaces a `- 1`), a `clusterOfLastConsumedGlyph` carry, two new `jassert`s | **Yes** — all editor text. |
| `juce_gui_basics` | `menus/juce_PopupMenu.cpp` — accessible focus now resolves to the handler's first child; `native/accessibility/juce_Accessibility_windows.cpp` | **Yes** (menus), Windows-only for the second. |
| `juce_audio_formats` | MP3 default (§3, pinned off); a WAV missing-final-pad-byte fix | Compiled, never called. |
| `juce_audio_devices`, `juce_audio_utils` | CoreAudio/WASAPI device handling, `AudioDeviceSelectorComponent` | Standalone host only. |
| `juce_audio_basics` | UMP sysex7 timestamps, CoreAudio time conversions | Not reached (no MIDI). |
| `juce_audio_plugin_client` | ARA only | Not built. |
| `juce_gui_extra` | `juce_WebBrowserComponent.h` | Unreachable at `JUCE_WEB_BROWSER=0`. |
| `juce_core` | `JUCE_BUILDNUMBER`, vendored zlib | Inert. |

The OpenGL, text-shaping and menu-accessibility rows are exactly what `DEPENDENCY_POLICY.md`
rule 2's **Level-5 manual audition** exists for. It is outstanding, and ADR-0054 says so instead of
arguing the gap away.

## §7. Rule-3 evidence — licences and attribution

Every licence file `THIRD_PARTY_LICENSES.md` cites, compared across the two checkouts:

| Result | Files |
|---|---|
| byte-identical | `libvorbis-1.3.7/COPYING`, `AudioUnitSDK/LICENSE.txt`, `VST3_SDK/LICENSE.txt`, `zlib/README`, `lunasvg/LICENSE`, `lunasvg/plutovg/LICENSE`, `harfbuzz/COPYING`, `jpglib/README`, `pnglib/LICENSE`, `sheenbidi/LICENSE` |
| **deletion-only** | `flac/Flac Licence.txt` (2448 → 1553 bytes), `oggvorbis/Ogg Vorbis Licence.txt` (2477 → 1470 bytes) |

Both deletions are JUCE's own 21-line *"I've incorporated this into the JUCE codebase"* preamble,
which 9.0.2 moves into the new `JUCE_CHANGES.txt` / `JUCE_UPSTREAM.txt` companion files. Proven
mechanically rather than by reading: `tail -n +22 <old>` is **byte-identical** to each new file, so
**no licence term changed**.

Two upstream documentation moves that the bump makes this repository's problem:

1. **The dependency list left `LICENSE.md`.** Through 9.0.1 `LICENSE.md` carried the inline list of
   ~20 vendored components; 9.0.2 replaces it with a pointer to a new SPDX SBOM, `JUCE.spdx.json`.
   `THIRD_PARTY_LICENSES.md` names that list twice — once as the authority its inventory was verified
   against, once in its *"to re-verify after a JUCE bump, repeat exactly that"* instruction — and both
   are now re-pointed, with the 9.0.1 spelling kept in parentheses so an older checkout still reads.
2. **`LICENSE.md` gained a paragraph** directing licensing questions to the EULA and the FAQ and
   stating the JUCE team provides no compliance confirmations. No change to the dual AGPLv3 /
   commercial grant itself; recorded here because `COMMERCIAL_STATUS.md`'s open owner action is about
   that grant.

`NOTICE` and `THIRD_PARTY_LICENSES.md` both carry the new version and commit — ER-DOC-02 is the
reason that is checked explicitly rather than assumed.

## §8. Everything else the round touched

- **Clang stays at 22.** ADR-0033's lift condition was re-measured against the live apt.llvm.org
  indexes and is still unmet — see the `DEPENDENCY_POLICY.md` compliance log for the numbers.
- **GCC stays at 16.** Newest upstream release is 16.2.0; the floating `gcc:16` tag already tracks it.
- **`github/codeql-action` v4.37.9 → v4.38.0**, SHA `cdf488f…` → `4bd7200e1f146b1c937cae12d258b50f41a53cf8`,
  on all three refs (`codeql.yml` init + analyze, `msvc.yml` upload-sarif). Every other action pin was
  measured against its upstream tag list and is **already at the newest release**: `actions/checkout`
  v7.0.1, `actions/cache` v6.1.0, `actions/upload-artifact` v7.0.1, `actions/download-artifact`
  v8.0.1, `actions/dependency-review-action` v5.0.0. `microsoft/msvc-code-analysis-action` remains
  deliberately pinned ahead of its last release and deliberately ignored by Dependabot.
- **pluginval** stays unpinned — `RELEASE_HARDENING_PLAN.md` RH-F6 owns that gap, and closing it is a
  change to the policy table, not to this round.
