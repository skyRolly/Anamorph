# TROUBLESHOOTING.md

Diagnosing build, validation, and runtime problems. For the validation workflow see `TESTING.md`.

## Build / configure

| Symptom | Cause | Fix |
|---|---|---|
| FetchContent fails to clone JUCE | No network to `github.com` | Use a local checkout: `-DANAMORPH_JUCE_PATH=/path/to/JUCE` (BUILD.md). |
| Missing X11/ALSA/GTK headers on Linux | Build deps not installed | Run `scripts/setup-linux.sh`. |
| `libwebkit2gtk-4.1-dev` not found | Newer/older Ubuntu | Try `libwebkit2gtk-4.0-dev` (scripts/setup-linux.sh:52 installs 4.1; :58 documents the fallback). |
| `EGL/egl.h` not found on Linux | `libegl-dev` missing — JUCE 9 builds its Linux GL context on EGL | `scripts/setup-linux.sh` installs it (scripts/setup-linux.sh:18-20,51); on a hand-rolled dep list add `libegl-dev`. |
| `curl: command not found` / `unzip: command not found` running `run-pluginval.sh` | The pluginval download/extract tools are missing — `libcurl4-openssl-dev` is headers, not the CLI | `scripts/setup-linux.sh` installs both (scripts/setup-linux.sh:46); CI runners preinstall them, so this only appears on a fresh machine or a minimal container. |
| `AnamorphTests not found` when testing | Not built, or tests disabled | `scripts/build.sh`; ensure `ANAMORPH_BUILD_TESTS=ON`. |
| Wrong/old JUCE behaviour | Stale fetched JUCE | Confirm the pinned commit `e18f7f5…` = JUCE 9.0.1 (CMakeLists.txt:48-50); a JUCE bump is a Build System change (ARCHITECTURE_REVIEW_GATE, ADR-0022 / ADR-0026). |
| Configure says `fetching JUCE 9.0.1 (<old rev>)` | `ANAMORPH_JUCE_TAG` is a CACHE variable — an existing `build/` keeps the OLD pin after a pull | Delete `build/` (or `cmake -B build -UANAMORPH_JUCE_TAG -UANAMORPH_JUCE_VERSION`) so the new pin takes effect; the configure banner prints version + rev precisely so a mismatch is visible. |
| Linker errors mixing JUCE modules | DSP compiled as a STATIC lib | The DSP core is an **INTERFACE** lib by design (CMakeLists.txt:200-211) — keep it INTERFACE; do not pre-compile JUCE modules into a static lib. |

## Validation (pluginval)

| Symptom | Cause | Fix |
|---|---|---|
| pluginval crashes on editor open/close (Linux) | Known host-side JUCE X11 `XEmbedComponent` use-after-free (not the plugin) | Handled by the signal-only retry in `scripts/run-pluginval.sh:147-197`; the plugin already drops its OpenGL child window on Linux (ADR-0011). |
| pluginval exits < 128 | Real validation failure | Read the log line; this is a genuine defect — do **not** retry. |
| Editor tests fail "no display" | Headless without xvfb | The script uses `xvfb-run -a` when available (scripts/run-pluginval.sh:129-131); install `xvfb`. |

## Runtime / DAW

| Symptom | Cause | Fix |
|---|---|---|
| macOS plugin won't load after a **zip** install | Gatekeeper quarantine (ad-hoc signed, not notarized) | `sudo xattr -dr com.apple.quarantine <bundle>` (PACKAGING.md / `packaging/macos/INSTALL.txt`), or use the `.pkg`, whose payloads are not quarantined (KI-002). |
| Logic Pro doesn't see the plugin | Logic loads **AU only** | Install the `.component`; verify with `auval -v aufx Anmr RTec`. |
| Plugin not offered on a mono track | Expected | mono→stereo is the headline layout; **mono→mono is Not Supported** (output is always stereo, PluginProcessor.cpp:81-82). |
| Vectorscope looks different on Linux vs macOS/Windows | By design | Linux/BSD render CPU-side (no OpenGL attach); macOS/Windows GPU-composite (ADR-0011). Visually identical. |
| DSP suddenly resets / brief glitch under extreme automation | NaN/Inf self-heal fired | A non-finite sample was produced upstream; the engine self-heals (AnamorphEngine.cpp:1256-1300). Crossovers are Nyquist-clamped (ADR-0009) — if it recurs, capture the parameter automation that triggered it. |
| A control click/pops on toggle | Should not happen | All discrete switches duck; Bypass/Multiband-Enable/Solo crossfade (ADR-0004). If reproducible, add a regression test (TESTING.md) and check the relevant click-free test. |
| Meters stuck / bar vanished | Was a NaN-latch (fixed 0.8.2) | Meters self-heal non-finite envelopes (LevelMeters.h:92-100, with `sanitize` at :179). If it recurs, the source is upstream non-finite audio. |

## "What cannot be verified headlessly"

Audio quality and GUI appearance need a DAW with audio + display. A green build + pluginval pass
is "ready to audition," not final (`docs/policies/TESTING_POLICY.md` Level 5).
