# JUCE 9.0.1 Upgrade (v0.9.4 cycle)

Controlled dependency migration: JUCE **9.0.0 → 9.0.1**, the maintenance release of the line
Anamorph already ships. No feature work, no redesign; the diff is the pin change, the version
bump and documentation. A JUCE bump is a **Build System change** → Architecture-Review-Gate item
(precedent: ADR-0012 for 8.0.8 → 8.0.14, ADR-0022 for 8.0.14 → 9.0.0); recorded in **ADR-0026**
and flagged on the PR.

## 1. Dependency audit (Phase 1 — before any modification)

* **Old pin**: commit `f8f8864172464b9adf9eba6101e1f784838d1597` = tag `9.0.0`,
  `ANAMORPH_JUCE_VERSION="9.0.0"`, `GIT_SHALLOW TRUE`.
* **New upstream**: tag `9.0.1` = commit `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8`
  ("JUCE version 9.0.1", 2026-08-10, lightweight tag). Identity confirmed from two independent
  paths: `git ls-remote --tags` before the change, and the FetchContent'd tree at
  `build/_deps/juce-src` after it (`git rev-parse HEAD` → the same SHA, subject
  "JUCE version 9.0.1").
* **Delta size**: 56 commits, 199 files.
* **Toolchain minimums unchanged**: CMake ≥ 3.22, C++17, macOS deployment target — no module
  raised a requirement.
* **Licence**: unchanged. JUCE's `LICENSE.md` is **byte-identical** between the two tags (still
  dual AGPLv3 / commercial JUCE 9).

### 1.1 Breaking changes in 9.0.1 (`BREAKING_CHANGES.md`, 2 entries) vs Anamorph's surface

| Upstream change | Anamorph exposure | Action |
|---|---|---|
| zlib / libjpeg / libpng / libflac now built as **C**, so their symbols are no longer inside C++ namespaces (ODR risk only if the binary *also* links an external copy) | **None.** Anamorph links no external zlib/png/jpeg/flac and defines none of the `JUCE_INCLUDE_*_CODE` overrides. The switch itself was **already in force at 9.0.0** — both tags carry the same 208 `.c` files under `modules/`, and the 9.0.1 commit (`4257ee7`) only *documents* it and adds the new `JUCE_*_INCLUDE_PATH` knobs. The project already declares `LANGUAGES C CXX` (`CMakeLists.txt:14`), which is why 9.0.0 built at all. | none |
| WebBrowserComponent JS interop package moved to `native/typescript/webview-interop` | **None.** `JUCE_WEB_BROWSER=0` (`CMakeLists.txt:193`), and the package is TypeScript/JavaScript — never compiled into a binary. | none |

### 1.2 Non-breaking 9.0.1 changes reviewed for regression risk

The decisive structural fact: of the JUCE modules Anamorph links, **`juce_dsp`,
`juce_audio_basics`, `juce_data_structures` and `juce_audio_plugin_client` changed only their
module-header `version:` string — zero lines of code**. The DSP primitives (oversampling,
Linkwitz–Riley, `dsp::AudioBlock`), the ValueTree/APVTS backing and the VST3/AU/Standalone
wrappers therefore cannot have moved. `juce_audio_processors` changed only its header version
plus `juce_AudioUnitPluginFormatImpl.h`, which is the AU **hosting** path Anamorph does not use.

What did change, and why it is or is not reachable:

* **`juce_core/xml/juce_XmlDocument.cpp`** — the one changed file squarely on Anamorph's **state**
  path (`copyXmlToBinary`/`getXmlFromBinary`, `parseXML` in `PluginProcessor` and
  `PresetManager`). Comment- and PI-skipping were factored into one helper; the loop now cannot
  spin forever on a malformed header, `readChildElements` also skips `<? … ?>` instructions
  (previously only `<!-- … -->`), and the "unterminated comment" error text became "unexpected
  end of stream". Anamorph's serialized state contains neither comments nor processing
  instructions. Covered by the 894-check state suite (§3), including the corrupt/foreign-state
  and malformed-slot-XML cases.
* **`juce_events/native/juce_Messaging_linux.cpp`** — the message-queue fd callback now yields
  after ~100 ms so it cannot starve the other `LinuxEventLoop` callbacks (upstream's
  "unresponsive Linux GUIs" fix), and a saturated socket queue adds a `jassertfalse` (a no-op in
  the Release builds that ship). Scheduling only; no semantic change.
* **`juce_gui_basics` Linux** — `XIQueryDevice` returning `nullptr` is now handled instead of
  dereferenced; displays are enumerated via XRandR even when the WM publishes no `_NET_WORKAREA`;
  a zero `hTotal`/`vTotal` no longer divides by zero; the vblank timer is started with a **period
  in ms** instead of a frequency passed to `startTimerHz`, and the guard now compares like with
  like so the timer is not restarted on every display-change callback. Anamorph uses
  `juce::VBlankAttachment` (editor meters, `FrameClock`), so this is a reachable path — but the
  resulting period is the same for real refresh rates (60 Hz → 16 ms either way;
  `startTimerHz(hz)` is `startTimer(1000 / hz)`), and `FrameClock` is driven by the callback's
  timestamp rather than by an assumed rate.
* **`juce_gui_basics/detail/juce_MouseInputSourceImpl.h`** — gesture (magnify/rotate) hit-testing
  now divides by the desktop scale factor. Anamorph handles no gesture events (`mouseMagnify`
  unused), so this is unreachable.
* **Windows** — Direct2D no longer leaves an unpainted seam at opaque component edges under
  fractional display scaling; `UiaDisconnectAllProviders` is now gated inside the UIA wrapper so
  it is only called from standalone apps (the call site's own `isStandaloneApp()` guard moved
  there — net-neutral for a plug-in, which never called it either way).
* **macOS** — `CoreGraphicsMetalLayerRenderer` guards nil textures/drawables;
  `MessageManager` avoids posting to the system queue during shutdown and its
  `runDispatchLoopUntil` return value is fixed; CoreAudio avoids `AudioObjectHasProperty` and
  compiles against older SDKs. The CoreAudio work is the Standalone's device layer — the plug-in
  does not own the device.
* **`juce_audio_formats`** — WAV/AIFF readers hardened against malformed input. Anamorph reads
  no audio files (no `AudioFormatReader` use anywhere in `src/`), so the paths are unreachable;
  recorded as upstream hygiene, not as an Anamorph-visible fix.
* **`juce_core/maths/juce_MathsFunctions.h`** — `findNearestValue` added (purely additive; no
  name collision in `src/`). `juce_core.h` moves the `Span`/`Functional` includes earlier to
  satisfy it — internal ordering only.
* No project exposure at all: Android, iOS, in-app purchases, the Projucer, the demos, the CI
  and documentation commits.

### 1.3 Known-issue cross-checks

* **KI-013** (macOS release-outside reconcile inert): `getNativeRealtimeModifiers` in
  `juce_NSViewComponentPeer_mac.mm` is **byte-identical** 9.0.0 vs 9.0.1 — not fixed, no
  regression; the known issue simply persists.
* **KI-019** (Linux app-switch dismissal inert): `LinuxComponentPeer::isActiveApplication` is
  **byte-identical** between the two tags — still the write-once latch, so KI-019 stands
  unchanged.
* **KI-003/KI-007** (pluginval-side X11 XEmbed crash): lives in pluginval's own JUCE, not ours —
  unaffected by this bump. It surfaced once during validation and was absorbed by
  `run-pluginval.sh`'s signal-only retry (§3).
* No JUCE patches are carried in-repo, so there was nothing to re-apply.

## 2. Changes applied (Phase 2)

* `CMakeLists.txt`: `project(... VERSION 0.9.4)`; pin → `e18f7f5…` with
  `ANAMORPH_JUCE_VERSION="9.0.1"` and the banner comment updated. The pin block keeps its exact
  line count, so every `CMakeLists.txt:36-38` citation in the docs stays valid.
* **Project C++ sources: no changes required** (§1.1).
* **`scripts/setup-linux.sh`: no change required** — no module Anamorph uses altered its
  declared `linuxPackages` / `OSXFrameworks` / `windowsLibs`; the only metadata difference across
  all fourteen module headers is the `version:` field.
* Docs synced: CHANGELOG `[0.9.4]`, ADR-0026 (+ index row, Proposed), DEPENDENCY_POLICY (pin
  table, version-lock rule, compliance log), BUILD.md, TROUBLESHOOTING, README, REPOSITORY_MAP,
  COMPATIBILITY_MATRIX, FUTURE_RISKS RISK-001, HANDOVER, THIRD_PARTY_LICENSES + TRADEMARKS
  (pinned-version citations), `.github/dependabot.yml`, DOCUMENTATION_COVERAGE.
* **Drift found and reported, not silently carried**: `.github/workflows/codeql.yml` still said
  "JUCE 8.0.14 arrives via CMake FetchContent" — stale since the 9.0.0 migration. The sentence's
  point is version-independent, so the minimal correction was to drop the number rather than
  restate it (C6).

## 3. Validation

* **DSP bit-identity (twin dump)** — the ADR-0022 harness re-run against the new pair: the 8
  `AnamorphDSP` sources plus a deterministic scenario driver compiled against **both** JUCE
  checkouts with identical flags (Release, `juce_recommended_config_flags` + the shipped
  `AnamorphHardening` flags), 32 scenarios (Haas/Velvet/Chorus/Dim-D × OS Off/2x/4x/8x × M/S
  on/off; drive 8 dB, amount 0.7, width 1.6, mix 0.8, multiband + mono-maker + level-match on;
  120 noise + 120 silence blocks each at 48 kHz/512), FNV-1a over every output byte.
  **All 32 hashes, all 32 reported latencies and all 32 predicted latencies are identical.**
  Harness self-check: the 32 hashes are **mutually distinct**, so the matrix is discriminating
  rather than trivially equal (the failure mode the 8.0.14 → 9.0.0 run hit with `algoAmount` at
  its 0 default). Tool kept in the session scratchpad per the `xbench.cpp` / ADR-0022 precedent;
  the methodology above is the record.
* **Build**: full fresh Linux Release build green from a deleted `build/` — VST3, Standalone and
  both test executables. FetchContent-by-SHA proven end-to-end: `build/_deps/juce-src` resolves
  to `e18f7f5…` / "JUCE version 9.0.1", with `GIT_SHALLOW` retained. Windows/macOS builds are
  exercised by the CI matrix on push.
* **Suites at JUCE 9.0.1**: `AnamorphTests` **140 checks, 0 failures**; `AnamorphStateTests`
  **894 checks, 0 failures** — including the parameter-registry snapshot **frozen under 8.0.14**,
  which passes byte-for-byte: parameter IDs/names/order/flags/ranges and the serialization
  schema are unchanged under 9.0.1.
* **pluginval strictness 10, locally green in both modes ×3**: `deterministic` passed 3/3 first
  attempt; `randomise` passed 3/3, with the third pass taking one retry.
  `run-pluginval.sh` retries **only** on a signal-crash exit (≥ 128) and fails immediately on any
  real validation failure, so that retry is the documented host-side JUCE/X11 `XEmbedComponent`
  flake (KI-003/KI-007), not a plugin defect. The same gates run blocking on all three CI
  platforms on push.
* **Warnings**: every one of the **18 Anamorph-owned translation units** was recompiled with the
  shipped flags against both JUCE trees and the diagnostics diffed. **19 warning instances on
  each side, byte-identical** once the JUCE path prefix is normalised —
  `-Wsign-conversion` ×6, `-Wshadow` ×4, `-Wswitch-enum` ×3, `-Woverloaded-virtual` ×3,
  `-Wfloat-equal` ×2, `-Wmisleading-indentation` ×1, all the pre-existing project baseline.
  **No new warnings from the upgrade**, and the baseline itself is untouched (no unrelated
  cleanup).
* **Third-party attribution re-verified** (`RELEASE_POLICY.md` requires this after any JUCE
  bump): JUCE's `LICENSE.md` and all twelve licence files cited by `THIRD_PARTY_LICENSES.md` §2
  (VST3 SDK, HarfBuzz, SheenBidi, LunaSVG, PlutoVG, FreeType-in-PlutoVG, stb, libpng, libjpeg,
  zlib, FLAC, Ogg Vorbis, AudioUnitSDK) are **byte-identical** between the two tags. The only
  licence files 9.0.1 adds are the three inside the new `webview-interop` TypeScript package,
  which is never compiled into an Anamorph binary. The inventory is unchanged; only the pinned
  version and commit it cites moved.

## 4. Compatibility verification

| Contract | Evidence |
|---|---|
| VST3 loading | pluginval strictness 10 green locally (deterministic + randomise ×3) and on the CI gates for this PR |
| Parameter IDs / names / order / automation flags | registry-snapshot fixture frozen under 8.0.14 passes unchanged under 9.0.1 (state test 2) |
| Serialization schema + raw-exact round-trip | state tests 1/3 green under 9.0.1 (`copyXmlToBinary` framing unchanged upstream; the XmlDocument change is comment/PI skipping only) |
| Legacy sessions (v0.2 / pre-0.6.4 / pre-0.8.4) | state tests 4-6 green under 9.0.1 |
| Presets | state tests 8/10/11/12 green under 9.0.1 |
| A/B state | state tests 3/9 green under 9.0.1 |
| DSP output + reported latency | 32/32 twin-dump hashes + latencies identical (§3); `juce_dsp` has zero code change between the tags |
| Bypass / lifecycle / click-free transitions | DSP suite (bypass-null, transition, parked-path bit-exactness tests) green under 9.0.1 |

## 5. Remaining migration risks

1. **Level-5 manual audition (OPEN, human-gated)** — required by DEPENDENCY_POLICY rule 2 for
   any JUCE bump. The twin dump proves engine numerics and the modules behind the DSP and the
   wrappers have no code change at all, but 9.0.1 *does* touch editor-adjacent framework code:
   Linux message-loop scheduling and display/vblank handling, Windows Direct2D edge painting,
   macOS Metal-layer guards, and FreeType named-instance guards. Appearance and feel are a human
   judgement. Until attested, ADR-0026 stays **Proposed** — and ADR-0022's own still-open
   audition of the 9.0 line is now discharged against this build rather than against 9.0.0.
2. **Windows/macOS compile** — not locally provable here; the CI matrix on this PR is the
   verification (fail-closed).
3. **New macOS CoreAudio calls** — the Standalone's device layer only (the plug-in does not own
   the device); a macOS Standalone smoke test is prudent at the next audition, as it was for
   9.0.0.
4. **KI-013 / KI-019 unchanged** — 9.0.1 fixes neither (both re-verified byte-identical); no
   regression, the known issues simply persist.
