# ADR-0011 — Linux/X11 CPU rendering — no OpenGL attach

**Status:** Accepted

## Context
The editor uses a `juce::OpenGLContext` to GPU-composite the vectorscope and UI. On Linux/X11,
attaching a GL context adds an embedded child X11 window.

## Problem
That child window's `ConfigureNotify` events make the host's `XEmbedComponent` post async lambdas
capturing a raw `this`. When a host tears the editor window down between the event and the async
(pluginval's "Editor Automation" stress test, and real Linux DAWs), the lambda use-after-frees
inside JUCE's X11 embedding — a segfault that lives in the **host-side** JUCE, not the plugin.

**Confirmed [Verified]:** the crash was reproduced locally (GL re-enabled on Linux, pluginval
strictness 10, ~1 in 3 runs) and a core dump's crashing frame is
`juce::XEmbedComponent::Pimpl::handleX11Event(...)::{lambda}` invoked from
`juce::MessageManager::runDispatchLoop` — i.e. inside JUCE's own X11 embedding on the message loop,
never in plugin code. gdb/ASan hide it (timing-sensitive race), so it is a genuine host-side
teardown race, not a plugin-side use-after-free.

## Options
- **A. Keep attaching OpenGL on Linux.** Rejected — reproducible UAF crash under rapid open/close.
- **B. Skip the OpenGL attach on Linux/BSD; render CPU-side (visually identical).** Chosen (0.8.5).

## Decision
Guard the attach with `#if ! (JUCE_LINUX || JUCE_BSD)`. On Linux/BSD the editor (including the
vectorscope) renders via the normal CPU `paint()` path — visually identical. **macOS and Windows
keep GPU compositing** (real GPUs; macOS runs GL + the same editor automation green, confirming the
plugin's GL code is sound). The editor destructor releases the per-frame VBlank callback **before**
detaching GL/stopping timers (the VBlank lambda captures `this`). `triggerRepaint` is guarded by
`isAttached()` so it is a no-op on Linux. pluginval's retry wrapper additionally retries only on
signal-crashes (never on real validation failures), because the crash is in pluginval's own JUCE.

**Windows CI caveat (see KI-007):** GitHub's `windows-latest` runner is GPU-less/headless and cannot
host the editor GUI tests at all — they fail there in *both* GL mode (the GDI-generic OpenGL 1.1
renderer lacks the GL2 entry points JUCE needs) and CPU mode. That is an environmental limit of the
runner, not a plugin defect (the editor validates cleanly on Linux CPU + macOS GL). The Windows CI
therefore runs pluginval with `--skip-gui-tests`; GL stays enabled for real Windows machines.

## Consequences
- The Linux release gate (headless pluginval strictness 10) is stable.
- Real Windows/macOS machines GPU-composite the editor as designed; Linux renders CPU-side.
- A `soundSignature()` / editor teardown order that is safe whether or not GL was attached.
- No DSP / parameter / architecture changes; purely an editor/platform decision.

## Related code
- `src/PluginEditor.cpp` (attach gate `#if ! (JUCE_LINUX || JUCE_BSD)` + rationale; destructor order;
  `triggerRepaint` `isAttached()` guard)
- `scripts/run-pluginval.sh:46-76` (Linux/macOS signal-only retry); `scripts/run-pluginval.ps1`
  (Windows crash retry + `--skip-gui-tests`)

Evidence [Verified]:
- Source: src/PluginEditor.cpp (attach gate + destructor); local core dump (XEmbedComponent frame).
- History [Partially Verified]: CHANGELOG.md [0.8.5]; commit c924ff8.
- Related incident: `../../POSTMORTEMS.md` INC-006 (Linux editor-automation segfault); KNOWN_ISSUES KI-003 (Linux), KI-007 (Windows CI editor GUI tests).
