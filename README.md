# Anamorph — Stereo Tools Audio Plugin

**Anamorph** (by **RollyTech**) is a stereo-field toolkit: it turns mono into stereo, controls
stereo width (globally and per band), and provides the full set of stereo tools (M/S, mono-maker,
channel utilities, monitoring) around a high-end diamond vectorscope. Built with **CMake + JUCE**
only — it configures and builds entirely from the command line on a headless Linux machine, no IDE.

## Headline features
- **Turn mono into stereo** and control stereo **Width** (global + up to 4 phase-coherent bands).
- Widening engine: **Haas / Velvet-Noise / Chorus / Dimension-D**, with **Drive** (oversampled).
- Full stereo toolkit: **M/S** mode, **Mono Maker**, channel kill/swap/balance/polarity, **Band Solo**.
- **Level Match** (BS.1770) for fair A/B; **A/B compare** + per-slot Undo/Redo; **presets**.
- Diamond **vectorscope**, correlation + L/R Peak/RMS meters; click-free transitions throughout.

## Project status
- **Version 0.9.4** (pre-1.0), in the **internal-testing phase** — builds go to testers for
  evaluation, not to customers (see [Licensing](#licensing); internal record:
  [`docs/COMMERCIAL_STATUS.md`](docs/COMMERCIAL_STATUS.md)).
- **0.9.4 is a maintenance release: JUCE 9.0.0 → 9.0.1, plus the move to the C++23 language
  standard, and one interaction fix — a control covered by an open drop-down no longer lights up
  as though you were pointing at it.** Apart from that fix, the only change to Anamorph's own
  source is one added `#include` (ADR-0027), and the
  sound, reported latency, parameters and saved state are unchanged — proven by 32-scenario engine
  twin dumps that are bit-identical both across the two JUCE versions and across C++17 vs C++23,
  both self-test suites, and pluginval at strictness 10 in both modes. Building from source now
  needs a C++23 compiler; installed builds are unaffected. What the JUCE bump brings is upstream
  maintenance in the framework the editor sits on: on Linux a busy message queue can no longer
  starve the window system (upstream's unresponsive-GUI fix), screens are detected on more window
  managers and a missing input-device list no longer crashes; on Windows a thin unpainted seam at
  panel edges under fractional display scaling is gone; on macOS, renderer and shutdown guards.
  ADR-0026, ADR-0027.
- **0.9.3 is a round of interaction fixes.** The Multiband **add-split preview line** keeps
  following the pointer instead of occasionally hanging at one spot on a quiet track; **a click
  that closes a menu now only closes the menu** — it can no longer also close a panel, discard a
  half-typed preset name or nudge a control underneath; an open menu no longer **outlives the plug-in
  window** — hiding it, closing it or switching to another application closes the menu too, so
  nothing is left floating and the first click on return goes to the control you aimed at;
  right-click menus size themselves to their longest item and grey out what you cannot pick; and
  turning **Tooltips** off now takes effect at once. One visible change beyond the fixes: the **Widen**
  row's two drop-downs are now equal width with the join between them on the panel's centre line.
  Installers: the **Linux** installer now asks where to install and **defaults to your own account**
  (`~/.vst3`, no root needed), and the **macOS** `.pkg` re-installs properly after the app or a
  plug-in has been moved or deleted — it used to update the moved copy and report success while the
  standard location stayed empty. Nothing in `src/` changed for either: saved state, parameters and
  the DSP are unchanged.
- **0.9.2 fixes the preset drop-down** (it no longer stays on screen — or crashes on click —
  after the plug-in window closes) and lets a user preset share a factory preset's name without
  losing its place in the menu; the Settings control *Window Size* is now labelled **UI Scale**.
  Saved state, parameters and the DSP are unchanged.
- **0.9.1 changed the manufacturer code** (`Anmf` → `RTec`, the vendor identifier now shared with
  the second RollyTech plug-in). It is host-facing identity, so **sessions saved with a pre-0.9.1
  build report Anamorph as missing** — re-insert the plug-in and re-load the preset. Audio,
  parameters and presets are unchanged. ADR-0023 · [`KI-016`](docs/KNOWN_ISSUES.md).
- Active development on a feature-branch → PR → `main` workflow.
- Validation gate: **33 DSP self-tests** + the **12-test state-compatibility suite** + **pluginval strictness 10** (both modes ×3, blocking on all three CI platforms).
- A green build + pluginval pass is **"ready to audition,"** not final sign-off (audio/visual
  quality needs a DAW — see `docs/procedures/TESTING.md`).

## Supported platforms & formats
- **Formats:** VST3 (all platforms), **AU** (macOS, for Logic/GarageBand), Standalone.
  **AAX is not supported**.
- **Platforms:** Linux x86-64, Windows x86-64, macOS universal (arm64 + x86_64).
- **I/O:** stereo→stereo and mono→stereo (output is always stereo; **mono→mono is not supported**).
- Full matrix + status: `docs/architecture/COMPATIBILITY_MATRIX.md`.

## Requirements
- **CMake ≥ 3.22**, a **C++23** compiler, **Ninja** (recommended). **JUCE 9.0.1** is fetched
  automatically (pinned to an immutable commit via CMake `FetchContent`) or pointed at a local
  checkout.
- Linux build deps install via `scripts/setup-linux.sh`. See `docs/procedures/BUILD.md`.

## Quick start (headless Linux)

```bash
# 1. Install build dependencies (Ubuntu; safe to re-run)
scripts/setup-linux.sh

# 2. Configure + build (fetches the pinned JUCE commit via CMake FetchContent)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release        # ...or: scripts/build.sh

# 3. Run the headless self-tests (DSP + state compatibility)
scripts/run-tests.sh

# 4. Validate the VST3 with pluginval — the release gate is BOTH modes, x3 each
scripts/run-pluginval.sh 10 deterministic
scripts/run-pluginval.sh 10 randomise
```

The produced plugin is typically at `build/Anamorph_artefacts/Release/VST3/Anamorph.vst3`.

## Installing (users)
Releases are published on the **GitHub Releases** page (draft → published after the manual
audition): per platform an **installer** (Windows Inno Setup installer and macOS `.pkg`,
both with component selection; on Linux the zip itself carries `install.sh`) *and* a plain
zip (flat contents — extracting shows the files directly), plus `SHA256SUMS.txt` and
`RELEASE_MANIFEST.txt`. Both routes install to the standard locations — system-wide on
Windows and macOS; on Linux `install.sh` asks, and defaults to a per-user install into
`~/.vst3` that needs no root. Step-by-step: **[`docs/user/INSTALLATION.md`](docs/user/INSTALLATION.md)**;
full manual: **[`docs/user/USER_MANUAL.md`](docs/user/USER_MANUAL.md)**.
Prebuilt binaries for all three OSes are also uploaded as **GitHub Actions artifacts** on every
push (macOS bundles are ad-hoc signed, not notarized — see `packaging/macos/INSTALL.txt`).

New to the plug-in? The manual's **[Quick start](docs/user/USER_MANUAL.md#2-quick-start)**
takes you from download to first widened sound. Testing a pre-release build?
**[SUPPORT.md](SUPPORT.md)** is the internal testing guide — what a tester may do with a build,
what to check first, and what a test report must contain.

To build without network (JUCE already on disk): `cmake -B build -DANAMORPH_JUCE_PATH=/path/to/JUCE ...`

## Architecture (one paragraph)
A **format-agnostic DSP core** (`src/dsp/`, the `AnamorphDSP` library) driven by a POD parameter
snapshot, behind a thin **plugin wrapper + GUI** (`src/`, `src/gui/`). The signal chain is strictly
serial: **Input conditioning → Effect engine (Drive → algorithm → Width → Multiband) → Dry/Wet Mix →
Mono Maker (post-Mix) → Output → Band Solo monitor (post-everything) → metering**. Oversampling wraps
only the nonlinear stages. Full detail: `docs/architecture/`.

## Documentation
The full technical documentation lives in **[`docs/`](docs/)**:
- **Start here:** [`docs/SOURCE_OF_TRUTH.md`](docs/SOURCE_OF_TRUTH.md) · [`docs/HANDOVER.md`](docs/HANDOVER.md) · [`docs/REPOSITORY_MAP.md`](docs/REPOSITORY_MAP.md)
- **For users:** [`docs/user/`](docs/user/) (installation guide, user manual)
- **Architecture & decisions:** [`docs/architecture/`](docs/architecture/) (signal flow, DSP algorithms, parameters, state, threading, latency, ADRs)
- **How-to:** [`docs/procedures/`](docs/procedures/) (build, CI/CD, testing, packaging, release)
- **Rules (binding):** [`docs/policies/`](docs/policies/) (real-time audio, threading, DSP, compatibility, AI-agent)
- **History & status:** [`CHANGELOG.md`](CHANGELOG.md) · [`docs/POSTMORTEMS.md`](docs/POSTMORTEMS.md) · [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) · [`docs/FUTURE_RISKS.md`](docs/FUTURE_RISKS.md)

The documentation falls into **four classes**, kept deliberately separate:

| Class | Documents | Audience |
|---|---|---|
| **User** | [`docs/user/INSTALLATION.md`](docs/user/INSTALLATION.md) · [`docs/user/USER_MANUAL.md`](docs/user/USER_MANUAL.md) (incl. its [Quick start](docs/user/USER_MANUAL.md#2-quick-start) and [FAQ](docs/user/USER_MANUAL.md#9-faq--troubleshooting)) | anyone running the plug-in |
| **Internal / testing** | [`SUPPORT.md`](SUPPORT.md) · the [bug-report form](.github/ISSUE_TEMPLATE/bug_report.yml) | internal and beta testers |
| **Legal / licensing** | [`EULA.md`](EULA.md) (draft) · [`PRIVACY.md`](PRIVACY.md) · [`TRADEMARKS.md`](TRADEMARKS.md) · [`NOTICE`](NOTICE) · [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) | everyone; Anamorph's own `LICENSE` is still pending — see below |
| **Developer** | everything under [`docs/`](docs/) not listed above (architecture, procedures, policies, status), plus [`CLAUDE.md`](CLAUDE.md) and [`docs/COMMERCIAL_STATUS.md`](docs/COMMERCIAL_STATUS.md) | maintainers and AI agents |

The user manual, `SUPPORT.md`, `NOTICE` and `THIRD_PARTY_LICENSES.md` are attached to every
release as version-named assets; per-platform install steps ship inside each package as
`INSTALL.txt`. Developer documentation is **not** shipped, with two documented exceptions:
[`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) and [`CHANGELOG.md`](CHANGELOG.md) are
developer-authored but deliberately **tester-surfaced** (`SUPPORT.md` routes testers to both).

## Licensing
**Anamorph is a closed-source commercial product — it is not open-source software.** The
source being readable here is not a licence: no `LICENSE` file is present, and this repository
grants no right to use, copy, modify or redistribute the code or binaries beyond what written
permission from RollyTech provides (all rights reserved by default). Nothing under `docs/` is a
source-code licence.

Three strands, kept separate:

- **Anamorph's own terms** — still to be settled. [`EULA.md`](EULA.md) is an **unapproved
  draft** with every open decision marked; it is not in force and no build ships it. Testers
  hold an evaluation-only permission ([`SUPPORT.md`](SUPPORT.md) §1).
- **Third-party dependency licences** — attribution for the shipped binaries is in
  [`NOTICE`](NOTICE); the full verified inventory (component, purpose, origin, licence,
  obligations) is in [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md). Both are published
  with every release. Anamorph is built on **JUCE**, whose modules are dual-licensed AGPLv3 or
  commercial; a closed-source distribution model cannot use the AGPLv3 arm, so the **commercial
  JUCE tier** must be in place before commercial distribution.
  **Commercial VST3 distribution requires reviewing Steinberg's licensing requirements
  separately.**
- **Product data and names** — [`PRIVACY.md`](PRIVACY.md) (Anamorph collects nothing and makes
  no network connections) and [`TRADEMARKS.md`](TRADEMARKS.md).

The open owner/legal decisions are indexed in
[`docs/COMMERCIAL_STATUS.md`](docs/COMMERCIAL_STATUS.md) §4 and recorded authoritatively in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) §"Open licensing decisions" and
[`docs/architecture/RELEASE_HARDENING_PLAN.md`](docs/architecture/RELEASE_HARDENING_PLAN.md).

Contributors and AI agents: read **[`CLAUDE.md`](CLAUDE.md)** and `docs/policies/AI_AGENT_POLICY.md`
before changing code — some changes (parameter IDs, serialization, threading, DSP order, latency)
are hard-stop, human-review-required.

## Simple vs. Advanced mode
**Simple** (default) is the Widen core around the vectorscope (algorithm, Drive, Amount, Width).
**Advanced** adds the Input, Output, and Multiband modules; Advanced-only modules default-bypass when
Advanced is off while their values are remembered. Settings (Oversampling, UI Scale, persistence,
tooltips, animations) are host-hidden session state.
