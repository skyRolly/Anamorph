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
- **Version 0.9.0** (pre-1.0). Active development on a feature-branch → PR → `main` workflow.
- Validation gate: **33 DSP self-tests** + the **9-test state-compatibility suite** + **pluginval strictness 10** (both modes ×3, blocking on all three CI platforms).
- A green build + pluginval pass is **"ready to audition,"** not final sign-off (audio/visual
  quality needs a DAW — see `docs/procedures/TESTING.md`).

## Supported platforms & formats
- **Formats:** VST3 (all platforms), **AU** (macOS, for Logic/GarageBand), Standalone.
  **AAX is not supported**.
- **Platforms:** Linux x86-64, Windows x86-64, macOS universal (arm64 + x86_64).
- **I/O:** stereo→stereo and mono→stereo (output is always stereo; **mono→mono is not supported**).
- Full matrix + status: `docs/architecture/COMPATIBILITY_MATRIX.md`.

## Requirements
- **CMake ≥ 3.22**, a **C++17** compiler, **Ninja** (recommended). **JUCE 9.0.0** is fetched
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
`RELEASE_MANIFEST.txt`. Both routes install to the standard system-wide locations. Step-by-step: **[`docs/user/INSTALLATION.md`](docs/user/INSTALLATION.md)**;
full manual: **[`docs/user/USER_MANUAL.md`](docs/user/USER_MANUAL.md)**.
Prebuilt binaries for all three OSes are also uploaded as **GitHub Actions artifacts** on every
push (macOS bundles are ad-hoc signed, not notarized — see `packaging/macos/INSTALL.txt`).

New to the plug-in? The manual's **[Quick start](docs/user/USER_MANUAL.md#2-quick-start)**
takes you from download to first widened sound. Stuck? **[SUPPORT.md](SUPPORT.md)** says what
to check and what a useful bug report contains.

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

The documentation falls into three classes, kept deliberately separate:
**user documentation** ([`docs/user/`](docs/user/) — installation guide + user manual — and
[`SUPPORT.md`](SUPPORT.md), all also attached to releases), **legal / licensing documents**
([`NOTICE`](NOTICE) · [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md); Anamorph's own
`LICENSE`/EULA are pending — see below), and **developer documentation** (everything else
under `docs/`, plus `CLAUDE.md`).

## Licensing
**Anamorph is a closed-source commercial product — it is not open-source software.** No
`LICENSE` file is present and this repository grants no rights to use, copy, modify or
redistribute the code or binaries beyond what written permission from RollyTech provides
(all rights reserved by default). Two owner-level licensing actions remain open, recorded in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) §"Open licensing decisions" and
[`docs/architecture/RELEASE_HARDENING_PLAN.md`](docs/architecture/RELEASE_HARDENING_PLAN.md):
Anamorph's own `LICENSE`/EULA text, and the **commercial JUCE licence tier** — Anamorph is
built on **JUCE**, whose modules are dual-licensed AGPLv3 or commercial, and a closed-source
distribution model cannot use the AGPLv3 arm, so the commercial tier must be in place before
commercial distribution.
Third-party attribution for the shipped binaries is in [`NOTICE`](NOTICE); the full verified
inventory (component, purpose, origin, licence, obligations) is in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) — both are published with every release.
**Commercial VST3 distribution requires reviewing Steinberg's licensing requirements
separately.**

Contributors and AI agents: read **[`CLAUDE.md`](CLAUDE.md)** and `docs/policies/AI_AGENT_POLICY.md`
before changing code — some changes (parameter IDs, serialization, threading, DSP order, latency)
are hard-stop, human-review-required.

## Simple vs. Advanced mode
**Simple** (default) is the Widen core around the vectorscope (algorithm, Drive, Amount, Width).
**Advanced** adds the Input, Output, and Multiband modules; Advanced-only modules default-bypass when
Advanced is off while their values are remembered. Settings (Oversampling, window size, persistence,
tooltips, animations) are host-hidden session state.
