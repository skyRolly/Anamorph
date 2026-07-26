# Third-party components in Anamorph

Complete inventory of third-party software that Anamorph compiles, links or redistributes,
with the licence of each and the exact file the licence was read from.

**This is a factual record, not legal advice.** No licensing or legal determination is made
here; open decisions are listed under [Open licensing decisions](#open-licensing-decisions)
and tracked in [`docs/architecture/RELEASE_HARDENING_PLAN.md`](docs/architecture/RELEASE_HARDENING_PLAN.md).
The short attribution notices that must accompany a binary distribution are reproduced in
[`NOTICE`](NOTICE).

## How this inventory was produced

Anamorph has exactly one declared dependency: **JUCE**, fetched by CMake `FetchContent` and
pinned to an immutable commit (`CMakeLists.txt:36-38`; ADR-0022). Every third-party component
below therefore arrives *inside the JUCE source tree* — nothing else is vendored, and no
package manager is used.

The inventory was verified against the pinned tree, not from memory:

| Question | How it was answered |
|---|---|
| Which components exist? | `LICENSE.md` at the root of the fetched JUCE checkout — JUCE's own authoritative dependency list |
| What licence does each carry? | the component's real licence file inside the JUCE tree (paths cited per row) |
| Which are actually compiled into Anamorph? | the generated `build/build.ninja` translation-unit list, plus `nm` on the produced object files (e.g. `jcopy_block_row` for libjpeg, `SBAlgorithmCreate` for SheenBidi, `FLAC__*`, `juce::OggVorbisNamespace::*`, `PVG_FT_*` for PlutoVG) |
| Which are present but *not* built? | the compile-time gate that excludes them (`#if` guard, platform guard, or a `FORMATS` value Anamorph does not build), confirmed by the absence of their symbols |

To re-verify after a JUCE bump, repeat exactly that: read the new `LICENSE.md`, then re-run the
symbol probes against a fresh Release build. See
[`docs/policies/DEPENDENCY_POLICY.md`](docs/policies/DEPENDENCY_POLICY.md).

Pinned version at the time of writing: **JUCE 9.0.0**, commit
`f8f8864172464b9adf9eba6101e1f784838d1597`. Paths below are relative to that checkout
(`build/_deps/juce-src/` in a local build) unless stated otherwise.

---

## 1. Framework

### JUCE

| | |
|---|---|
| **Purpose** | The entire application framework: DSP primitives (oversampling, Linkwitz–Riley filters, `dsp::AudioBlock`), the parameter system (APVTS), GUI, and the VST3/AU/Standalone format wrappers |
| **Origin** | Raw Material Software Limited — <https://juce.com> |
| **Licence** | **Dual: AGPLv3 *or* the commercial JUCE 9 licence** |
| **Licence file** | `LICENSE.md` (JUCE checkout root) |
| **Shipped** | Yes — statically compiled into every Anamorph binary |

JUCE's own words: *"The JUCE Framework modules are dual-licensed under the AGPLv3 and the
commercial JUCE licence."* Every JUCE source file repeats the choice in its header comment
(see e.g. `modules/juce_audio_formats/codecs/juce_MP3AudioFormat.cpp:1-33`).

**The owner has stated the product model (2026-07-26): closed-source commercial.** That
model cannot satisfy the AGPLv3 arm, so the commercial JUCE 9 licence must be in place
before commercial distribution; obtaining it (and which tier) remains an open owner action —
see [Open licensing decisions](#open-licensing-decisions). The repository currently declares
no licence of its own.

---

## 2. Compiled into the shipped binaries

Everything in this table produces object code in the Anamorph VST3 / AU / Standalone builds.
All of it arrives via JUCE modules; none of it is separately vendored by this repository.

| Component | Purpose in Anamorph | Licence | Licence file (in the JUCE tree) |
|---|---|---|---|
| **Steinberg VST 3 SDK** | The VST3 plug-in interface Anamorph implements | MIT (Steinberg Media Technologies GmbH, 2025) — but see §3 | `modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt` (identical texts in `base/`, `pluginterfaces/`, `public.sdk/`) |
| **HarfBuzz** | Text shaping for every label and readout in the GUI | "Old MIT" (HarfBuzz's own term) | `modules/juce_graphics/fonts/harfbuzz/COPYING` |
| **SheenBidi** | Unicode bidirectional text ordering for the GUI | Apache License 2.0 — © 2016-2025 Muhammad Tayyab Akram | `modules/juce_graphics/unicode/sheenbidi/LICENSE` |
| **LunaSVG** | SVG rasterisation in JUCE's `Drawable` path | MIT — © 2020-2025 Samuel Ugochukwu | `modules/juce_graphics/drawables/lunasvg/LICENSE` |
| **PlutoVG** | 2-D vector rasteriser used by LunaSVG | MIT — © 2020-2025 Samuel Ugochukwu | `modules/juce_graphics/drawables/lunasvg/plutovg/LICENSE` |
| **FreeType** (vendored *inside* PlutoVG) | Scanline rasteriser, path stroker and fixed-point maths — `plutovg-ft-raster.c`, `plutovg-ft-stroker.c`, `plutovg-ft-math.c` | FreeType Project Licence (FTL) — © 1996-2002, 2006 David Turner, Robert Wilhelm, Werner Lemberg | `.../lunasvg/plutovg/source/FTL.TXT` |
| **stb_truetype / stb_image / stb_image_write** (vendored inside PlutoVG) | TrueType glyph extraction and image encode/decode inside PlutoVG | MIT **or** public domain (Unlicense), at the recipient's choice — © 2017 Sean Barrett | licence text at the end of `.../plutovg/source/plutovg-stb-truetype.h` (same in the other two headers) |
| **libpng** | PNG image decoding | PNG Reference Library License v2 | `modules/juce_graphics/image_formats/pnglib/LICENSE` |
| **libjpeg (IJG)** | JPEG image decoding | Independent JPEG Group licence — **carries a mandatory acknowledgement** (see below) | `modules/juce_graphics/image_formats/jpglib/README` §LEGAL ISSUES |
| **zlib** | Deflate/inflate used by JUCE's zip and PNG paths | zlib licence — © 1995-2026 Jean-loup Gailly and Mark Adler | `modules/juce_core/zip/zlib/README` (§"Copyright notice") |
| **FLAC** | Audio-file reading via `juce_audio_formats` | BSD 3-clause — © Josh Coalson / Xiph.Org Foundation | `modules/juce_audio_formats/codecs/flac/Flac Licence.txt` |
| **Ogg Vorbis** | Audio-file reading via `juce_audio_formats` | BSD 3-clause — © 2002-2020 Xiph.org Foundation | `modules/juce_audio_formats/codecs/oggvorbis/libvorbis-1.3.7/COPYING` |
| **GLEW / Mesa / Khronos OpenGL declarations** | The OpenGL entry points in `juce_gl.h` (used by the macOS/Windows GPU compositing path; Anamorph renders CPU-side on Linux per ADR-0011) | BSD (GLEW), MIT (Mesa), MIT (Khronos) | `modules/juce_opengl/opengl/juce_gl.h`, between the `BEGIN_GLEW_LICENSE` / `END_GLEW_LICENSE` markers |
| **AudioUnitSDK** | The AU wrapper — **macOS builds only** | Apache License 2.0 | `modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt` |

FLAC and Ogg Vorbis reach the binary because `JUCE_USE_FLAC` and `JUCE_USE_OGGVORBIS` both
default to `1` (`modules/juce_audio_formats/juce_audio_formats.h:74-85`) and Anamorph does not
override them. Anamorph itself never reads audio files; the codecs come along with the module.

### Notices that are *mandatory*, not courtesy

Three of the above impose an attribution obligation on binary distribution. All three are
discharged by [`NOTICE`](NOTICE), which — together with this file — is published as a
**version-named asset on every GitHub release**, next to the zips and installers (the
packages themselves are deliberately lean; each `INSTALL.txt` inside them carries the IJG
acknowledgement verbatim plus a pointer to these release-page files). Anyone redistributing
the binaries away from the release page must carry `NOTICE` and this file along:

- **libjpeg (IJG)** — condition (2): *"If only executable code is distributed, then the
  accompanying documentation must state that 'this software is based in part on the work of the
  Independent JPEG Group'."* The IJG licence also forbids using an IJG author's or company name
  in advertising.
- **FLAC** and **Ogg Vorbis** (BSD 3-clause) — *"Redistributions in binary form must reproduce
  the above copyright notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution."* The third clause also
  forbids using the Xiph.Org Foundation name to endorse the product.
- **HarfBuzz** requires the copyright notice and its two disclaimer paragraphs to appear in all
  copies; **SheenBidi**'s Apache-2.0 terms require the licence and attribution notices to be
  carried along.

- **FreeType** (FTL) does not mandate a specific form, but §"Legal Terms" asks distributors to
  credit it and supplies the wording — *"Portions of this software are copyright © &lt;year&gt; The
  FreeType Project (www.freetype.org). All rights reserved."* — which `NOTICE` uses verbatim.

zlib and libpng ask for acknowledgement but explicitly do *not* require it; it is given anyway.
**stb** is dual MIT/public-domain, so attribution is optional; it is listed for completeness.

> **Two of these are not in JUCE's own `LICENSE.md` dependency list.** FreeType and stb reach the
> build *transitively*, vendored inside PlutoVG rather than by JUCE directly, so reading JUCE's
> list alone would have missed both. This is why the verification step above walks the actual
> compiled translation units (`juce_graphics_lunasvg.c` `#include`s `plutovg-ft-raster.c`,
> `plutovg-ft-stroker.c`, `plutovg-ft-math.c`, `plutovg-font.c` and `plutovg-surface.c`; the
> compiled object carries 30 `PVG_FT_*` symbols) instead of trusting an upstream manifest. Repeat
> that walk after any JUCE bump.

---

## 3. Steinberg VST 3 — separate review required

The VST 3 SDK **source code** bundled with JUCE 9.0.0 is under the **MIT licence**
(`.../VST3_SDK/LICENSE.txt`, "Copyright (c) 2025, Steinberg Media Technologies GmbH"). That is a
change from older SDK releases, which were dual-licensed GPLv3 / proprietary agreement — earlier
Anamorph documentation described that older arrangement and has been corrected.

The MIT grant covers the code. It does **not** cover the "VST" name and logo, or the terms on
which VST 3 plug-ins may be developed and distributed. Evidence in the pinned tree:

- `.../VST3_SDK/VST3_Usage_Guidelines.pdf` ships alongside the SDK.
- `.../VST3_SDK/README.md` states that the full VST 3 SDK obtained from Steinberg contains
  *"the **Steinberg VST 3 Plug-In SDK Licensing Agreement** that you have to sign if you want to
  develop or host **VST 3** plug-ins."*

> **Commercial VST3 distribution requires reviewing Steinberg's licensing requirements
> separately.** This repository makes no determination about which agreements apply, whether one
> must be signed, or how the VST trademark may be used. That review is an owner action and is
> tracked as an open item in `docs/architecture/RELEASE_HARDENING_PLAN.md`.

---

## 4. Present in the JUCE tree but NOT built into Anamorph

Listed for completeness so a future audit does not have to re-derive the exclusions. Each was
confirmed excluded by the stated gate *and* by the absence of its symbols from the build.

| Component | Licence (per JUCE's `LICENSE.md` / its own file) | Why it is not in Anamorph |
|---|---|---|
| **JUCE MP3 decoder** | JUCE's own terms, with an explicit patent/IP disclaimer | `JUCE_USE_MP3AUDIOFORMAT` defaults to **0** (`juce_audio_formats.h:99-101`) and Anamorph does not enable it, so `juce_MP3AudioFormat.cpp`'s body is `#if`-ed out. JUCE's disclaimer warns the code is *"NOT guaranteed to be free from infringements of 3rd-party intellectual property"* — Anamorph therefore ships no MP3 decoder. |
| **LV2 SDK** (lv2, lilv, serd, sord, sratom) | ISC | `juce_audio_processors_headless_lv2_libs.cpp` is compiled but its content is behind `#if JUCE_INTERNAL_HAS_LV2`; the object contains no `lv2_`/`lilv_`/`serd_`/`sord_`/`sratom_` symbols. Anamorph neither builds an LV2 plug-in nor hosts plug-ins. |
| **AAX SDK** | Proprietary Avid AAX licence / GPLv3 | AAX is **Not Supported** (`docs/policies/COMPATIBILITY_POLICY.md`); it is not in Anamorph's CMake `FORMATS`. |
| **Steinberg ASIO SDK** | Proprietary Steinberg ASIO licence / GPLv3 | Only the licence file and three headers are present (`modules/juce_audio_devices/native/asio/`); `JUCE_ASIO` is not enabled, and the SDK proper is not vendored. |
| **Oboe** | Apache License 2.0 | Android audio backend; Anamorph targets Linux/Windows/macOS only. |
| **CHOC (incl. QuickJS)** | ISC (CHOC), MIT (QuickJS) | Lives in `juce_javascript`, a module Anamorph does not link. |
| **Box2D** | zlib | Lives in `juce_box2d`, a module Anamorph does not link. |
| **pslextensions** | Public domain | Presonus VST3 extension headers; not referenced by Anamorph. |
| **ARA** | — | No ARA SDK is present in the pinned tree; the ARA translation unit compiles empty. |
| **reaper-sdk, Projucer icons, Android Gradle wrapper** | zlib / MIT / Apache 2.0 | JUCE examples, bundled apps and build tooling — not part of a plug-in build. |

---

## 5. Dynamically linked system libraries

These are **not redistributed** by Anamorph — they are provided by the operating system or the
user's distribution and resolved at load time. Recorded because a downstream packager may need
to know. Verified with `ldd` on a Release Linux VST3 build:

`libX11`, `libxcb`, `libXau`, `libXdmcp`, `libGL`, `libGLX`, `libGLdispatch`, `libEGL`,
`libfreetype`, `libfontconfig`, `libpng16`, `libz`, `libexpat`, `libbrotlicommon`,
`libbrotlidec`, `libbz2`, `libbsd`, `libmd`, `libstdc++`, `libgcc_s`, `libm`, `libc`.

On Windows and macOS the equivalents are OS frameworks (Core Audio / Audio Units, Direct2D,
etc.) shipped with the operating system.

---

## 6. Build- and CI-only tools

Used to produce or validate releases; **never redistributed inside a release artifact**, so no
notice obligation attaches to the shipped product.

| Tool | Where | Note |
|---|---|---|
| **pluginval** (Tracktion) | `scripts/run-pluginval.sh` / `.ps1` — downloaded at validation time | The validation gate; not packaged. Its version is not pinned — tracked as a supply-chain follow-up in `docs/policies/DEPENDENCY_POLICY.md`. |
| **Inno Setup 6** | Preinstalled on the `windows-latest` GitHub runner; compiles `packaging/windows/Anamorph.iss` | Produces the installer; the compiler itself is not shipped. |
| **`pkgbuild` / `productbuild`** | macOS runner, `packaging/macos/build-pkg.sh` | OS-native tools. |
| **GitHub Actions** (`actions/checkout`, `actions/upload-artifact`, `actions/download-artifact`, `github/codeql-action`, `actions/dependency-review-action`, `microsoft/msvc-code-analysis-action`) | `.github/workflows/` | CI only. |

---

## Open licensing decisions

These require an **owner/business decision** and are deliberately left open here. None of them
is resolved by this document.

1. **Obtaining the commercial JUCE 9 licence tier.** JUCE 9 modules are AGPLv3 *or*
   commercial. The owner has stated the product intent (2026-07-26): **Anamorph is a
   closed-source commercial plugin.** The AGPLv3 arm requires offering the source under
   AGPLv3-compatible terms, which a closed-source distribution model does not do — so as a
   factual consequence of that stated intent, **a commercial JUCE licence must be in place
   before commercial distribution**. Which tier, and its acquisition, remain owner/legal
   actions; nothing in this repository records that purchase.
2. **Anamorph's own licence.** The repository root has **no `LICENSE` file**, so the terms under
   which Anamorph's own source and binaries are offered are undeclared. `docs/user/USER_MANUAL.md`
   carries a bare copyright line ("© 2026 RollyTech") and nothing more.
3. **An end-user licence agreement (EULA)** for the distributed binaries, if the product is to be
   sold. The installers do not currently present one.
4. **Steinberg VST 3 requirements** for commercial distribution and trademark use — see §3.

Blockers 1–4 are recorded in `docs/architecture/RELEASE_HARDENING_PLAN.md` (RH-R11 / RH-F1;
RH-R10 / RH-F2 for the Steinberg item). They are **not** engineering tasks and cannot be closed
by a code change.
