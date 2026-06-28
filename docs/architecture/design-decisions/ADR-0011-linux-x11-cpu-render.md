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

## Options
- **A. Keep attaching OpenGL on Linux.** Rejected — reproducible UAF crash under rapid open/close.
- **B. Skip the OpenGL attach on Linux/BSD; render CPU-side (visually identical).** Chosen (0.8.5).

## Decision
Guard the attach with `#if ! (JUCE_LINUX || JUCE_BSD)`. On Linux/BSD the editor (including the
vectorscope) renders via the normal CPU `paint()` path — visually identical. macOS/Windows keep
GPU compositing. The editor destructor also releases the per-frame VBlank callback **before**
detaching GL/stopping timers (the VBlank lambda captures `this`). `triggerRepaint` is guarded by
`isAttached()` so it is a no-op on Linux. pluginval's retry wrapper additionally retries only on
signal-crashes (never on real validation failures), because the crash is in pluginval's own JUCE.

## Consequences
- The Linux release gate (headless pluginval strictness 10) is stable.
- No DSP / parameter / architecture changes; purely an editor/platform decision.

## Related code
- `src/PluginEditor.cpp:246-256` (gate + rationale), `:627-632` (destructor order), `:1151-1152`
- `scripts/run-pluginval.sh:46-76` (signal-only retry)

Evidence [Verified]:
- Source: src/PluginEditor.cpp:246-256,627-632; scripts/run-pluginval.sh:46-76
- History [Partially Verified]: README:39-47 (0.8.5); commit c924ff8
- Related incident: `../../POSTMORTEMS.md` INC-006 (Linux editor-automation segfault)
