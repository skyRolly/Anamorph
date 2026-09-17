# ADR-0054 — JUCE dependency upgrade 9.0.1 → 9.0.2

**Status:** **Proposed** — the headless half of `DEPENDENCY_POLICY.md` rule 2 is complete and
recorded below; **two owner actions remain** and are named in *Outstanding* at the end. This ADR
becomes `Accepted` when they are done, which is the sequence ADR-0022 and ADR-0026 each followed.

## Context
JUCE is pinned to an exact version, and any JUCE bump is a **Build System change requiring an
ADR + human Architecture Review** (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`,
`DEPENDENCY_POLICY.md` rule 1). ADR-0012 recorded 8.0.8 → 8.0.14, ADR-0022 recorded 8.0.14 → 9.0.0
and replaced the mutable tag-name pin with the tag's **immutable commit SHA**, and ADR-0026 recorded
9.0.0 → 9.0.1. JUCE **9.0.2** is the next maintenance release of the line already shipped; the owner
commissioned the move to it as part of a general dependency refresh (2026-09-17).

## Problem
Move to JUCE 9.0.2 with **no change** to DSP output, reported latency, parameter semantics,
serialization or the set of third-party components compiled into the product; keep the diff minimal;
keep the pin immutable.

## Options
- **A. Stay on 9.0.1.** Rejected for the commissioned upgrade, and for the reason ADR-0022 and
  ADR-0026 already gave: deferring a patch release only makes the next migration larger.
- **B. Bump to the mutable tag `9.0.2`.** Rejected — it reintroduces the re-pointed-tag
  supply-chain weakness ADR-0022 closed.
- **C. Bump pinned to the tag's commit SHA, with the ADR-0026 verification repeated, and pin the
  one module default 9.0.2 flips out from under the product.** Chosen.

## Decision
- `ANAMORPH_JUCE_TAG` → **`72782788ce18c2d4d760b28e0921d6ffc6431102`** (the commit of upstream tag
  `9.0.2`, read with `git ls-remote --tags` and corroborated by the fetched tree's own
  `project(JUCE VERSION 9.0.2 …)` and by `JUCE_BUILDNUMBER 2` in
  `modules/juce_core/system/juce_StandardHeader.h:44`); `ANAMORPH_JUCE_VERSION` → **`9.0.2`**.
  Both are printed by the configure banner (`CMakeLists.txt:67-72, 81-89`). The tag is a
  **lightweight** tag, so the ref and the commit are the same object and there is no peeling step.
  GitHub still serves shallow fetch-by-SHA, so `GIT_SHALLOW` is retained.
- **`JUCE_USE_MP3AUDIOFORMAT=0` is now pinned explicitly**, on all six targets that carry the
  `JUCE_*` contract (`CMakeLists.txt:505, 536, 579, 619, 660, 701`). This is the ONE build change
  the bump requires, and it is a change that **preserves** the shipped configuration rather than
  altering it — see *The one flag that had to be pinned*.
- **No C++ source change.** The single 9.0.2 breaking change has no project exposure, and the two
  entries 9.0.2 adds retroactively under its `Version 9.0.1` heading have none either — see
  *Breaking-change exposure*.
- **No build-dependency change.** All fifteen module declarations Anamorph builds moved **only**
  their `version:` field; no `linuxPackages` / `OSXFrameworks` / `windowsLibs` value changed, so
  `scripts/setup-linux.sh` is untouched and `libegl-dev` (ADR-0022) remains the Linux GL
  requirement. Toolchain contract unchanged: CMake ≥ 3.22, **C++23** (ADR-0027).

## Breaking-change exposure

`BREAKING_CHANGES.md` in the 9.0.2 tree carries one entry under `# Version 9.0.2` and **two new
entries added retroactively under `# Version 9.0.1`** (they were not in the 9.0.1 tag's own copy of
that file, so for this repository they arrive with this bump). All three were checked against `src/`
and `tests/` by name:

| Change | Exposure |
|---|---|
| `AudioDeviceSelectorComponent::getMidiInputSelectorListBox` removed | **None.** The symbol appears nowhere in `src/` or `tests/`. Anamorph builds no device-selector UI of its own; the Standalone wrapper's is JUCE's. |
| `OpenGLImageType::create()` now honours `Image::SingleChannel` | **None.** `OpenGLImageType` and `getFrameBufferFrom` appear nowhere in the tree. |
| `OpenGLContext::setImageCacheSize()` now counts bytes, not pixels (default raised 8 MB → 32 MB so behaviour is unchanged) | **None.** Anamorph's whole use of the class is `setContinuousRepainting`, `attachTo`, `detach`, `isAttached` and `triggerRepaint` (`src/PluginEditor.cpp:307, 321, 790, 1990-1991`); it never sets a cache size, so it takes the unchanged default. |

## The one flag that had to be pinned

`JUCE_USE_MP3AUDIOFORMAT` **defaulted to 0 through 9.0.1** (`juce_audio_formats.h:117-119`) and
**defaults to 1 from 9.0.2** (`:110-112`); the same edit deleted the patent/IP disclaimer that stood
beside it. Anamorph links `juce_audio_utils`, so `juce_audio_formats` is compiled, and taking the new
default would have:

1. compiled `juce_MP3AudioFormat.cpp`'s body into **every shipped binary** — dead code, because
   `grep -rn "AudioFormat" src/ tests/` matches nothing at all; and
2. falsified a standing sentence in a **shipped** document: `THIRD_PARTY_LICENSES.md` §"Present but
   not compiled" states *"Anamorph therefore ships no MP3 decoder."*

Pinning the flag to 0 keeps the product byte-for-byte in the configuration the bump found it in.
The flag is the no-op here; its omission would have been the change. `THIRD_PARTY_LICENSES.md` now
records the default flip and says the pin is what keeps its claim true.

## Verification (headless, this change)

- **DSP bit-identity proven, not assumed.** `tests/dsp_dump.cpp` — the committed harness
  `DEPENDENCY_POLICY.md` rule 2 names — built from one source tree against **both** JUCE checkouts
  with otherwise identical flags (`-DANAMORPH_BUILD_DSPDUMP=ON`, Release, GCC 13.3, the procedure in
  `docs/procedures/TESTING.md` §Proving a dependency bump is bit-identical). **32 scenarios** (4
  algorithms × 4 oversampling factors × M/S off/on), 48 kHz / 512 samples, 120 blocks of fixed-seed
  noise then 120 of digital silence, hashed FNV-1a over **every output byte plus the reported
  latency**. `diff` of the two outputs is **empty**, and the harness's own `--self-check` passed on
  both sides ("32 scenarios, all repeatable and all distinct"), so the instrument is discriminating
  rather than uniformly blind. **Run twice** — once before the `JUCE_USE_MP3AUDIOFORMAT` pin and once
  after — bit-identical both times.
- **Reported latency is inside that proof**, not beside it: the dump hashes the latency column, and
  `LATENCY_MODEL.md`'s 2× = 4 / 4× = 6 / 8× = 6 row is therefore proven unchanged rather than
  re-asserted.
- **The two modules the product's behaviour rests on did not move at all.** `juce_dsp` and
  `juce_audio_processors` differ between the tags **only** in their module header's `version:`
  field — every other file in both is byte-identical. That is the mechanism behind the bit-identical
  dump, and it is why the repository's citations into those modules survive the bump verbatim:
  `juce_AudioProcessor.cpp`, `juce_AudioProcessorParameter.h`, `juce_AudioProcessorValueTreeState.{h,cpp}`,
  `juce_Oversampling.cpp` and `juce_FilterDesign.cpp` were each compared byte-for-byte and are
  identical, as are `juce_MouseInputSource.cpp`, `juce_Desktop.cpp`, `juce_Slider.cpp` and
  `juce_StandaloneFilterWindow.h`.
- **Third-party licence re-verification (rule 3, and `RELEASE_POLICY.md`).** All twelve licence files
  `THIRD_PARTY_LICENSES.md` cites were compared across the tags. Ten are byte-identical. Two —
  `flac/Flac Licence.txt` and `oggvorbis/Ogg Vorbis Licence.txt` — shrank, and the diff is
  **deletion-only**: JUCE moved its own 21-line "I've incorporated this into JUCE" preamble into the
  new `JUCE_CHANGES.txt` / `JUCE_UPSTREAM.txt` companion files. `tail -n +22 <old>` is byte-identical
  to each new file, so **no licence term changed**.
- **JUCE's own dependency list moved.** Through 9.0.1 it was the inline list in `LICENSE.md`; 9.0.2
  replaces that list with a pointer to a new SPDX SBOM, `JUCE.spdx.json`. `LICENSE.md` also gained a
  paragraph directing licensing questions to the EULA and the FAQ. Since `THIRD_PARTY_LICENSES.md`
  names that list as the authority its inventory was verified against **and** tells the next reader to
  re-read it after a bump, both statements were re-pointed at the SBOM.
- **No effect annotations appeared.** `grep` for `clang::nonblocking` / `nonallocating` /
  `function_effects` over the whole 9.0.2 `modules/` tree matches nothing, so
  `CI_CD.md` §Realtime and `build.yml`'s scoped `-Wfunction-effects` rationale still hold.
- **The rest of the exposure surface, stated rather than implied.** Of the modules Anamorph builds,
  only these carry a real code change: `juce_opengl` (GL context/image/texture/framebuffer),
  `juce_graphics` (`juce_SimpleShapedText.cpp` — glyph-cluster counting in text shaping),
  `juce_gui_basics` (`juce_PopupMenu.cpp` — accessible-focus now resolves to the first child handler;
  `juce_Accessibility_windows.cpp`), `juce_gui_extra` (`juce_WebBrowserComponent.h`, unreachable at
  `JUCE_WEB_BROWSER=0`), `juce_audio_devices` and `juce_audio_utils` (Standalone-only device
  handling), `juce_audio_formats` (MP3 default — pinned off above; a WAV missing-pad-byte fix Anamorph
  never reaches), `juce_audio_basics` (UMP/CoreAudio time conversion), `juce_audio_plugin_client`
  (ARA only) and `juce_core` (`JUCE_BUILDNUMBER`, vendored zlib). **Three of those are editor-visible
  on a path Anamorph does use** — OpenGL rendering, text shaping and popup-menu accessibility — and
  no headless gate covers what they look like. That is precisely the gap rule 2's Level-5 audition
  exists to close, and it is left open below rather than argued away.

## Consequences
- The pin is immutable and one patch release newer; the product's engine output, reported latency,
  parameter semantics and serialization are unchanged, by measurement.
- One more `JUCE_*` flag is now part of the dependency contract (`DEPENDENCY_POLICY.md` rule 5). It
  is load-bearing: dropping it silently adds an MP3 decoder to the shipped binary and makes a shipped
  licence document wrong.
- `CHANGELOG.md` carries **no entry** for this bump — `CHANGELOG_POLICY.md` rule 3 admits user-visible
  changes only, and by construction this one has none to report.

## Outstanding (owner)
1. **Human Architecture Review.** `ARCHITECTURE_REVIEW_GATE.md` gates a Build System change and says
   a green build does not clear it. The evidence a reviewer needs is above and in
   `worklogs/JUCE902_UPGRADE_v0.9.8.md`.
2. **The Level-5 manual audition** (`DEPENDENCY_POLICY.md` rule 2) — a DAW audition against this
   build. It is a human sign-off and is not headlessly reproducible; the three editor-visible module
   changes listed above are what it is for.

## Related
- ADR-0012 (8.0.8 → 8.0.14), ADR-0022 (8.0.14 → 9.0.0, and the immutable-SHA pin), ADR-0026
  (9.0.0 → 9.0.1), ADR-0027 (C++23), ADR-0011 (X11/EGL).
- `docs/policies/DEPENDENCY_POLICY.md` (rules 1–5 and the compliance log),
  `docs/policies/ARCHITECTURE_REVIEW_GATE.md`, `docs/procedures/TESTING.md`
  §Proving a dependency bump is bit-identical, `worklogs/JUCE902_UPGRADE_v0.9.8.md`.
