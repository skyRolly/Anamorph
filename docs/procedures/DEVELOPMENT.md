# DEVELOPMENT.md

Local development and debugging. Prerequisites and build commands are in `BUILD.md`; this doc
covers the dev loop.

## First-time setup

```bash
scripts/setup-linux.sh                                   # Linux deps (Ubuntu)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug         # Debug for development
cmake --build build --config Debug
```

For a fast inner loop, prefer building a single target:
```bash
cmake --build build --target AnamorphTests   # DSP self-tests (fastest feedback)
cmake --build build --target Anamorph_VST3    # the plugin only
cmake --build build --target Anamorph_Standalone
```

## The dev loop

1. Edit DSP in `src/dsp/` (format-agnostic core) or wrapper/GUI in `src/`.
2. Build + run the self-tests (no DAW needed):
   ```bash
   scripts/build.sh Debug && scripts/run-tests.sh
   ```
3. For anything user-visible, validate the VST3 with pluginval (`TESTING.md`).
4. Audio/visual quality must be auditioned in a DAW — it cannot be judged headlessly
   (`docs/policies/TESTING_POLICY.md` Level 5).

## Where things live

See `docs/REPOSITORY_MAP.md` for the full map. Key entry points:
- DSP chain orchestration: `src/dsp/AnamorphEngine.cpp` (`process`).
- Parameter surface: `src/PluginParameters.{h,cpp}` (`createAnamorphLayout`, `pid::`).
- State save/recall: `src/PluginProcessor.cpp` (`get/setStateInformation`).
- Editor / GUI: `src/PluginEditor.cpp`, `src/gui/`.
- Tests: `tests/dsp_tests.cpp` (DSP), `tests/state_tests.cpp` (state/parameter compatibility) + `tests/fixtures/`.

## Standalone app for quick manual checks

The Standalone target (`build/Anamorph_artefacts/<cfg>/Standalone/Anamorph`) opens the editor and
runs the engine against the system audio device — useful for a quick GUI/behaviour check without a
DAW. Note: this still cannot substitute for in-DAW automation/host-state testing.

## Before you change DSP, parameters, threading, state, or latency

Read `docs/policies/AI_AGENT_POLICY.md` first. These areas are governed by binding policies and
the Architecture Review Gate; some changes are **Hard Stop** conditions requiring human review
(parameter ID changes, serialization schema changes, threading changes, DSP signal-order changes,
reported-latency changes, ADR conflicts).

## Debugging notes

- The audio thread is real-time (`docs/policies/REALTIME_AUDIO_POLICY.md`): do not add logging,
  allocation, or locks to `processBlock` / `AnamorphEngine::process`, even temporarily in a
  committed change.
- A non-finite-sample self-heal exists in the engine (`AnamorphEngine.cpp:847-870`); if you see
  the DSP reset itself, a NaN/Inf was produced upstream — fix the source, not the guard.
- On Linux the editor renders CPU-side (no OpenGL attach) by design (ADR-0011); GPU compositing
  paths only run on macOS/Windows.
