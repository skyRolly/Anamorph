# TROUBLESHOOTING.md

Diagnosing build, validation, and runtime problems. For the validation workflow see `TESTING.md`.

## Build / configure

| Symptom | Cause | Fix |
|---|---|---|
| FetchContent fails to clone JUCE | No network to `github.com` | Use a local checkout: `-DANAMORPH_JUCE_PATH=/path/to/JUCE` (BUILD.md). |
| Missing X11/ALSA/GTK headers on Linux | Build deps not installed | Run `scripts/setup-linux.sh`. |
| `libwebkit2gtk-4.1-dev` not found | Newer/older Ubuntu | Try `libwebkit2gtk-4.0-dev` (setup-linux.sh:33). |
| `AnamorphTests not found` when testing | Not built, or tests disabled | `scripts/build.sh`; ensure `ANAMORPH_BUILD_TESTS=ON`. |
| Wrong/old JUCE behaviour | Stale fetched JUCE | Confirm the pinned tag `8.0.14` (CMakeLists.txt:33); a JUCE bump is a Build System change (ARCHITECTURE_REVIEW_GATE). |
| Linker errors mixing JUCE modules | DSP compiled as a STATIC lib | The DSP core is an **INTERFACE** lib by design (CMakeLists.txt:54-73) — keep it INTERFACE; do not pre-compile JUCE modules into a static lib. |

## Validation (pluginval)

| Symptom | Cause | Fix |
|---|---|---|
| pluginval crashes on editor open/close (Linux) | Known host-side JUCE X11 `XEmbedComponent` use-after-free (not the plugin) | Handled by the signal-only retry in `run-pluginval.sh:46-76`; the plugin already drops its OpenGL child window on Linux (ADR-0011). |
| pluginval exits < 128 | Real validation failure | Read the log line; this is a genuine defect — do **not** retry. |
| Editor tests fail "no display" | Headless without xvfb | The script uses `xvfb-run -a` when available (run-pluginval.sh:42-44); install `xvfb`. |

## Runtime / DAW

| Symptom | Cause | Fix |
|---|---|---|
| macOS plugin won't load after download | Gatekeeper quarantine (ad-hoc signed, not notarized) | `sudo xattr -dr com.apple.quarantine <bundle>` (PACKAGING.md / INSTALL.txt). |
| Logic Pro doesn't see the plugin | Logic loads **AU only** | Install the `.component`; verify with `auval -v aufx Anmr Anmf`. |
| Plugin not offered on a mono track | Expected | mono→stereo is the headline layout; **mono→mono is Not Supported** (output is always stereo, PluginProcessor.cpp:33-43). |
| Vectorscope looks different on Linux vs macOS/Windows | By design | Linux/BSD render CPU-side (no OpenGL attach); macOS/Windows GPU-composite (ADR-0011). Visually identical. |
| DSP suddenly resets / brief glitch under extreme automation | NaN/Inf self-heal fired | A non-finite sample was produced upstream; the engine self-heals (AnamorphEngine.cpp:847-870). Crossovers are Nyquist-clamped (ADR-0009) — if it recurs, capture the parameter automation that triggered it. |
| A control click/pops on toggle | Should not happen | All discrete switches duck; Bypass/Multiband-Enable/Solo crossfade (ADR-0004). If reproducible, add a regression test (TESTING.md) and check the relevant click-free test. |
| Meters stuck / bar vanished | Was a NaN-latch (fixed 0.8.2) | Meters self-heal non-finite envelopes (LevelMeters.h:73-77). If it recurs, the source is upstream non-finite audio. |

## "What cannot be verified headlessly"

Audio quality and GUI appearance need a DAW with audio + display. A green build + pluginval pass
is "ready to audition," not final (README:503-508; `docs/policies/TESTING_POLICY.md` Level 5).
