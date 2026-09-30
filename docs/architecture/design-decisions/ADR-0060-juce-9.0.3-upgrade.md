# ADR-0060 — JUCE dependency upgrade 9.0.2 → 9.0.3

**Status:** **Proposed** — implemented on `claude/anamorph-comprehensive-review-90tpty` with the
headless verification recorded below, and **awaiting the two human acts no gate can produce**: the
owner's Architecture Review (`ARCHITECTURE_REVIEW_GATE.md` — a JUCE pin is a gated Build System
change, and detecting one is an AI-agent hard stop, so this change must not be merged on a green build
alone) and the `DEPENDENCY_POLICY.md` rule-2 Level-5 audition of the 9.0.3 build. The bump ships in
**0.9.9** on the owner's instruction of 2026-09-29, so until both acts are recorded 0.9.9 is not
taggable (`RELEASE_POLICY.md` preconditions 5 and 7). This is the sequence ADR-0022, ADR-0026 and
ADR-0054 each followed: headless evidence first, then the review and the audition.

## Context
JUCE is pinned to an exact commit, and any JUCE bump is a **Build System change requiring an ADR +
human Architecture Review** (`DEPENDENCY_POLICY.md` rule 1). ADR-0012, ADR-0022, ADR-0026 and ADR-0054
recorded 8.0.8 → 8.0.14 → 9.0.0 → 9.0.1 → 9.0.2. JUCE **9.0.3** is the next maintenance release of the
line already shipped. The owner commissioned the move on 2026-09-29 and asked for it to ship inside
0.9.9 rather than a later version; the same instruction moved the 0.9.9 release date to 2026-10-01.

## Problem
Move to JUCE 9.0.3 with **no change** to DSP output, reported latency, parameter semantics,
serialization or the set of third-party components compiled into the product; keep the diff minimal;
keep the pin immutable.

## Options
- **A. Stay on 9.0.2.** Rejected for the commissioned upgrade, for the reason ADR-0022, ADR-0026 and
  ADR-0054 gave: deferring a patch release only makes the next migration larger.
- **B. Bump to the mutable tag `9.0.3`.** Rejected — it reintroduces the re-pointed-tag supply-chain
  weakness ADR-0022 closed.
- **C. Bump pinned to the tag's commit SHA and take 9.0.3's new module defaults.** Rejected: 9.0.3
  adds two codecs that are **on by default** (below), so this option compiles Opus and libwebp into
  every target and adds two third-party libraries that `THIRD_PARTY_LICENSES.md` and `NOTICE` do not
  carry — a change to the shipped component set that the product has no use for.
- **D. Bump pinned to the tag's commit SHA, repeat the ADR-0054 verification, and pin the two new
  defaults off.** Chosen.

## Decision
- `ANAMORPH_JUCE_TAG` → **`be29c81492b6151c8ea8d14c840e1311963b3a83`** (the commit of upstream tag
  `9.0.3`); `ANAMORPH_JUCE_VERSION` → **`9.0.3`** (`CMakeLists.txt:67-72`). The fetched tree's own
  headers carry `version: 9.0.3` in every module declaration. `GIT_SHALLOW` is retained.
- **`JUCE_USE_OPUS=0` and `JUCE_USE_WEBP=0` are pinned explicitly** on all six targets that carry the
  `JUCE_*` contract (`CMakeLists.txt:515-516`, and the two lines after `JUCE_USE_MP3AUDIOFORMAT=0` in
  `AnamorphTests`, `AnamorphStateTests`, `AnamorphBench`, `AnamorphDspDump` and `AnamorphFuzzState`) —
  see *The two flags that had to be pinned*. These are the only build changes the bump requires, and
  both **preserve** the shipped configuration rather than alter it.
- **No C++ source change.** Neither 9.0.3 breaking change reaches Anamorph — see *Breaking-change
  exposure*. The source edits in this change are comments that cite JUCE line numbers (§Citation map).
- **No build-dependency change.** The declaration blocks of all fifteen modules Anamorph builds moved
  **only** their `version:` field, so `scripts/setup-linux.sh` is untouched. Two new link needs appear
  in `juce_audio_devices` without a declaration change and need no package: on Windows a
  `#pragma comment (lib, "cfgmgr32.lib")` (a Windows SDK import library), and on Linux `dlopen`/`dlsym`
  (a probe for PipeWire's ALSA plug-in) and ALSA's `snd_pcm_type`, already satisfied by `juce_core`'s
  declared `dl` and `juce_audio_devices`' declared `alsa` package. Toolchain contract
  unchanged: CMake ≥ 3.22, **C++23** (ADR-0027).

## Breaking-change exposure

`BREAKING_CHANGES.md` in the 9.0.3 tree carries one entry under `# Version 9.0.3` and **one entry added
retroactively under `# Version 9.0.2`** (not in the 9.0.2 tag's own copy of the file, so for this
repository it arrives with this bump). Both were checked against `src/` and `tests/` by name:

| Change | Exposure |
|---|---|
| `SystemStats::isOperatingSystem64Bit()` now reports the operating system, not the process | **None.** The symbol appears nowhere in `src/` or `tests/`. |
| `ThreadPool::addJob` takes a single callable returning `void` or `JobStatus` | **None.** Anamorph uses no `ThreadPool`; its only schedulers are the editor's 24 Hz and the processor's 20 Hz message-thread timers (`THREADING_POLICY.md`). |

## The two flags that had to be pinned

9.0.3 adds **Opus** reading and writing to `juce_audio_formats` (vendored opus 1.6.1, opusfile
0.12-59-g6dfd29e and libopusenc 0.3; `JUCE_USE_OPUS`, `juce_audio_formats.h:110-112`, default **1**)
and **WebP** images to `juce_graphics` (vendored libwebp 1.6.0; `JUCE_USE_WEBP`, `juce_graphics.h:127-129`,
default **1**). Anamorph links `juce_audio_utils`, so both modules are compiled. Taking the defaults
would have:

1. compiled both libraries into **every shipped binary** as dead code — Anamorph reads and writes no
   audio file (`grep -rn "AudioFormat" src/ tests/` matches nothing) and decodes no image of its own
   (no `ImageCache`, `ImageFileFormat` or `Drawable` loading anywhere in `src/`); and
2. added two third-party components, each with its own BSD licence file and libwebp with a separate
   patent grant, that the shipped attribution documents do not carry.

Pinned to 0, JUCE's module build still compiles the thirteen new top-level translation units — its
module CMake (`JUCEModuleSupport.cmake`, byte-identical between the tags) globs every top-level source —
but they emit no code: `juce_audio_formats_opus.c` and `juce_audio_formats_opusfile.c` each define one
hidden placeholder symbol, and the ten `juce_graphics_libwebp_*.c` objects define **none**. The shipped
Linux VST3 and Standalone contain no `webp`, `opus`, `ogg_`, `vorbis`, `FLAC` or `mp3` symbol at all
(`nm`, this build). The one binary that does carry libwebp is JUCE's own build helper `juceaide`, which
runs on the build machine with JUCE's defaults and is not shipped. This is ADR-0054's MP3 precedent from
the other direction — there, a default flipped under the product; here, new defaults arrived with new
code — and `DEPENDENCY_POLICY.md` rule 5 governs all three flags from here.

**The Ogg Vorbis component changes version but not membership.** `JUCE_USE_OGGVORBIS` still defaults
to 1, and 9.0.3 splits the old `codecs/oggvorbis/` tree into `codecs/ogg/` and `codecs/vorbis/`, moves
libogg into its own C translation unit (`juce_audio_formats_ogg.c`) and upgrades it **1.3.4 → 1.3.6**.
The compiled object exports the same 71 hidden `ogg_*`/`oggpack_*` symbols as before. libvorbis stays
1.3.7 with only its `#include` lines rewritten. The licence text is byte-identical; its path moved, so
`THIRD_PARTY_LICENSES.md`'s Ogg Vorbis row now cites `codecs/vorbis/COPYING` and `codecs/ogg/COPYING`.
(9.0.3 still ships the orphaned `codecs/oggvorbis/JUCE_UPSTREAM.txt`, which describes the 1.3.4 layout
that no longer exists — an upstream leftover, not something this repository cites.)

## Verification (headless, this change)

- **DSP bit-identity proven, not assumed.** `tests/dsp_dump.cpp` built from one frozen source tree
  (`756a5c2`) against **both** JUCE checkouts, Release, GCC 13.3, the procedure in
  `docs/procedures/TESTING.md` §Proving a dependency bump is bit-identical. 32 scenarios, FNV-1a over
  every output byte plus the reported latency. `diff` of the two outputs is **empty**, and
  `--self-check` passed on both sides ("32 scenarios, all repeatable and all distinct"). **Run twice**
  — with `JUCE_USE_OPUS` / `JUCE_USE_WEBP` at JUCE's defaults and pinned to 0 — and all four 9.0.3/9.0.2
  tables are identical.
- **Reported latency is inside that proof**, so `LATENCY_MODEL.md`'s 2× = 4 / 4× = 6 / 8× = 6 row is
  proven unchanged at 9.0.3 rather than re-asserted.
- **Suites on the 9.0.3 build** (this tree, the shipped flag set): DSP self-tests **944 checks, 0
  failures**; state suite **5600 checks, 0 failures** (isolated `HOME`). That includes the parameter
  registry snapshot, the three legacy fixtures and the committed 0.9.5 field capture.
- **pluginval** 1.0.4 at strictness 10 under `xvfb` against the 9.0.3 VST3 built from this tree:
  *ALL 3 deterministic pass(es) succeeded* (seed `0x1`) and *ALL 3 randomise pass(es) succeeded*, each
  on its first attempt. The macOS AU and Windows gates run in CI.
- **Compiler diagnostics.** The twin-dump builds against 9.0.2 and 9.0.3 emit the **same** first-party
  warning set, line for line. The full local GCC build adds one third-party `-Wstringop-overflow`
  diagnostic under LTO, inside libstdc++'s `vector::insert` as inlined into `juce_OpenGLHelpers.cpp:282`
  / `juce_OpenGLContext.cpp:937`; that region is byte-identical between the tags, and CI gates
  first-party warnings only.
- **Which modules moved.** `juce_audio_processors`, `juce_dsp`, `juce_audio_basics`,
  `juce_data_structures` and `juce_audio_utils` differ between the tags **only** in their module
  header's `version:` field; `juce_dsp.cpp.o` defines the same 1524 symbols with the same sizes. That
  is necessary but not sufficient — `juce_core`'s headers did change, and a byte-identical module can
  still inline differently (`ValueTree::isEquivalentTo` does) — which is why the twin dump, not the
  source diff, carries the proof.
- **ADR-0057 precondition 2 re-verified** (rule 2). `ParameterAdapter::parameterValueChanged` still
  stores `unnormalisedValue = newValue;` (`juce_AudioProcessorValueTreeState.cpp:155`) into a
  `std::atomic<float>` (`:208`) — a `seq_cst` store — and the whole file is byte-identical.
- **Plug-in wrappers.** Standalone is byte-identical. The VST3 wrapper loses one line — `virtual
  ~JuceARAFactory() = default;` under `JucePlugin_Enable_ARA` (compiled out at 0) — and
  `juce_VST3ModuleInfo.h` loses the analogous destructor of `JucePluginCompatibility`. The AU wrapper
  rewrites `SetBusCount`'s loop, which Anamorph cannot reach: it overrides neither `canAddBus` nor
  `canRemoveBus`, so the property is refused as not writable before the loop. No state, parameter,
  latency or threading entry point changed.
- **Third-party licences (rule 3, `RELEASE_POLICY.md`).** Every licence file `THIRD_PARTY_LICENSES.md`
  cites is byte-identical except the Ogg Vorbis file, which moved with identical text (above). The VST 3
  SDK directory is byte-identical. `JUCE.spdx.json` differs only by the version bump, libogg 1.3.6, and
  the four new packages (Opus, opusfile, libopusenc, libwebp), all of which are pinned out. `NOTICE`'s
  Ogg Vorbis text already matches the new file.
- **No effect annotations appeared.** No `clang::nonblocking` / `nonallocating` in either tree, so
  `CI_CD.md` §Realtime and `build.yml`'s scoped `-Wfunction-effects` rationale still hold.
- **The rest of the exposure surface, stated rather than implied.** Of the modules Anamorph builds, the
  ones carrying a real code change are: `juce_audio_devices` (Standalone only — ALSA sample-rate and
  channel handling, a device that reports no current rate now requested at 44.1 kHz instead of 0,
  Windows MIDI Services behind `JUCE_USE_WINDOWS_MIDI_SERVICES`, default 0); `juce_opengl`
  (`OpenGLFrameBuffer` / cached-image paths now handle a texture allocated larger than requested, and a
  bounded `glGetError` drain); `juce_gui_basics` (Windows window placement and focus; X11
  `WM_DELETE_WINDOW` under modal components and XInput touch modifiers; macOS peer); `juce_events`
  (the Linux message queue signals its pipe once per drain instead of once per message —
  `KNOWN_ISSUES.md` KI-027's cost description is updated accordingly); `juce_core` (files, posix
  streams, system stats); `juce_graphics` (image-format registration, an SVG bounding box — neither
  called by Anamorph, and text shaping, fonts and the renderers are byte-identical); and
  `juce_audio_processors_headless` (LV2/VST3 **hosting** only, compiled out). **Three are editor- or
  device-visible on paths Anamorph does use** — the OpenGL framebuffer on macOS/Windows, the Windows and
  X11 window peers, and the Standalone device layer — and no headless gate covers them. That is the gap
  the rule-2 Level-5 audition exists to close, and it is left open below rather than argued away.

## Citation map

9.0.3 moved lines in six JUCE files the repository cites. Live documents and code comments are re-aimed
at 9.0.3 in this change; **accepted ADRs keep the line numbers of the tree they were written against**
(ADR-0054's precedent: an ADR records a decision at its date), and this table maps them:

| File | 9.0.2 → 9.0.3 | Cited from (unchanged ADRs) |
|---|---|---|
| `juce_audio_plugin_client_VST3.cpp` | −1 past line 2498 (2822 → 2821, 3469 → 3468, 3475-3479 → 3474-3478, 3482-3493 → 3481-3492, 3537 → 3536, 3563 → 3562, 3591 → 3590) | ADR-0036 §11 (`:3537`), ADR-0056 (`:2822`) |
| `juce_NSViewComponentPeer_mac.mm` | +46 from ≈ line 2200 (2396-2435 → 2442-2481, 2437-2467 → 2483-2513, 2580 → 2626, 2986-2999 → 3032-3045); 1655-1668 and 302-307 unchanged | ADR-0053 (`:2986-2999`) |
| `juce_XWindowSystem_linux.cpp` | 4176 → 4177, 4176-4189 → 4177-4195 (touch modifiers inserted inside the span), 4207 → 4213; 2299-2306 and 676-678 unchanged | ADR-0053 (`:4176-4189`) |
| `juce_Windowing_windows.cpp` | 2606-2613 → 2612-2619 | ADR-0053 (`:2606-2613`) |
| `juce_SharedCode_posix.h` | 527-538 → 532-543 (`FileOutputStream::writeInternal`) | — |
| `juce_Messaging_linux.cpp` | `postMessage` 79-96 → 79-92, **content changed** (see above) | — |
| `juce_audio_formats.h` | `JUCE_USE_MP3AUDIOFORMAT` 110-112 → 134-136; 110-112 is now `JUCE_USE_OPUS` | ADR-0054 (`:110-112` is correct for 9.0.2) |

`src/PluginProcessor.h` cited `CallPrepareToPlay::no` at `:3470`, which was already one line off at
9.0.2 (3470 was blank; the call was 3469). It now cites 9.0.3's `:3468`. Every other repository citation
into JUCE names a file that is byte-identical between the tags, or a span the diff left in place.

## Consequences
- The pin is immutable and one patch release newer; engine output, reported latency, parameter
  semantics and serialization are unchanged, by measurement.
- Two more `JUCE_*` flags are part of the dependency contract (rule 5). Both are load-bearing: dropping
  either compiles an unused third-party library into the product and makes the shipped attribution
  documents incomplete.
- The shipped Ogg Vorbis component moves from libogg 1.3.4 to 1.3.6 under the same licence.
- `CHANGELOG.md` `[0.9.9]` carries a **Changed** entry, as the 9.0.0 and 9.0.1 bumps did: this bump
  reaches the editor and the Standalone app through upstream fixes (Windows window placement and focus,
  the OpenGL framebuffer, Standalone device handling), and the owner asked for it to be part of 0.9.9.
  ADR-0054 took no entry because 9.0.2 had nothing user-visible to report.
- 0.9.9's Level-5 audition (recorded 2026-09-28) and the owner's attestations of compatibility-checklist
  items 5 and 7 were made on the 9.0.2 build. They **do not carry to the 9.0.3 build** — the same
  per-build rule that reopened the audition for 0.9.6 and 0.9.7 — so `RELEASE_POLICY.md` preconditions 2
  and 7 are open again for 0.9.9 (`LEVEL5_AUDITION.md`, `RELEASE_COMPATIBILITY_CHECKLIST.md`).

## The two human acts still owed

1. **Human Architecture Review.** `ARCHITECTURE_REVIEW_GATE.md` gates a Build System change and says a
   green build does not clear it. The review is of the evidence above and in
   `worklogs/JUCE903_UPGRADE_0.9.9.md`, including the `JUCE_USE_OPUS=0` / `JUCE_USE_WEBP=0` pins.
2. **Level-5 manual audition of the 9.0.3 build** (rule 2; `RELEASE_POLICY.md` precondition 7), with
   the re-attestation of checklist items 5 (host matrix) and 7 (automation playback) on the same build.
   Beyond the standing script, it is what covers the paths listed above that no headless gate reaches:
   the editor under OpenGL on macOS and Windows, window placement and focus of the editor and the
   Standalone on Windows, closing the editor and the Standalone window on Linux (X11), and the
   Standalone opening an audio device on Linux (ALSA) and Windows.

When both are recorded, this ADR moves to **Accepted** with the date and what was reported, and nothing
more — as ADR-0054 did.

## Related
- ADR-0012 (8.0.8 → 8.0.14), ADR-0022 (8.0.14 → 9.0.0, and the immutable-SHA pin), ADR-0026
  (9.0.0 → 9.0.1), ADR-0054 (9.0.1 → 9.0.2, and the MP3 pin this follows), ADR-0057 (precondition 2),
  ADR-0027 (C++23), ADR-0011 (X11/EGL).
- `docs/policies/DEPENDENCY_POLICY.md` (rules 1–5 and the compliance log),
  `docs/policies/ARCHITECTURE_REVIEW_GATE.md`, `docs/procedures/TESTING.md` §Proving a dependency bump
  is bit-identical, `THIRD_PARTY_LICENSES.md`, `worklogs/JUCE903_UPGRADE_0.9.9.md`.
