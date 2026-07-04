# ADR-0011 — CPU rendering off macOS — OpenGL attach on macOS only

**Status:** Accepted

## Context
The editor uses a `juce::OpenGLContext` to GPU-composite the vectorscope and UI. Attaching that
context has a **platform-specific failure mode** under pluginval's "Editor Automation" stress test
(rapid editor open / automate / close), on two of the three target platforms.

## Problem
- **Linux/X11:** attaching GL adds an embedded child X11 window; its `ConfigureNotify` events make
  the host's `XEmbedComponent` post async lambdas capturing a raw `this`. When the host tears the
  editor window down between the event and the async (pluginval, and real Linux DAWs), the lambda
  use-after-frees inside JUCE's X11 embedding — a segfault in the **host-side** JUCE, not the plugin.
- **Windows:** GitHub's `windows-latest` CI runner (and any GPU-less machine) exposes only the
  **GDI-generic OpenGL 1.1** renderer, which lacks the GL2 shader/VBO entry points JUCE's GL
  `LowLevelGraphicsContext` calls. The first Editor-Automation paint faults on the GL render thread —
  the process dies with no clean exit code (KI-007). macOS CI has a real GL and is unaffected.

## Options
- **A. Keep attaching OpenGL everywhere.** Rejected — reproducible crash under rapid open/close on
  Linux (UAF) and Windows CI (GL 1.1 fault).
- **B. Attach GL on macOS only; render CPU-side elsewhere (visually identical).** Chosen
  (Linux/BSD in 0.8.5; extended to Windows here).

## Decision
Guard the attach with `#if JUCE_MAC`. Off macOS the editor (including the vectorscope) renders via
the normal CPU `paint()` path — **visually identical** (the GL context only GPU-composites; it adds
no visual features). The editor destructor releases the per-frame VBlank callback **before**
detaching GL/stopping timers (the VBlank lambda captures `this`); `detach()` is a safe no-op when GL
was never attached, and `triggerRepaint` is guarded by `isAttached()`, so no teardown change is
needed off macOS. pluginval's retry wrapper retries only on signal-crashes (never on real validation
failures).

A future GPU-capability probe (create a throwaway WGL context, confirm GL2 before attaching) could
re-enable GL on capable Windows machines, but it needs a real Windows test bed; dropping the attach
is the proven Linux remedy applied to the same symptom.

## Consequences
- The Linux release gate (headless pluginval strictness 10) is stable.
- **Windows:** the GL render thread is removed, which is expected to clear the KI-007 Editor-
  Automation crash (empirically **CI-confirmed pending** at the time of this change — the fix is
  applied on the strong hypothesis that both surviving crash lenses point to GL; if the Windows job
  is still red after this, the crash is elsewhere and the attach guard is not the fix).
- Trade-off: real Windows machines with a GPU no longer GPU-composite the editor (CPU paint is
  visually identical; the cost is a small, imperceptible compositing overhead for a ~24–60 Hz scope).
- Removes a render thread on Windows; adds no thread, cross-thread path, or atomic ordering. Same
  class as the accepted 0.8.5 Linux change — an editor/platform decision, not a DSP/parameter/
  threading-architecture change.

## Related code
- `src/PluginEditor.cpp` (attach gate `#if JUCE_MAC` + rationale, destructor order, `triggerRepaint`
  `isAttached()` guard)
- `scripts/run-pluginval.sh:46-76` / `scripts/run-pluginval.ps1` (signal/crash retry)

Evidence [Verified]:
- Source: src/PluginEditor.cpp (attach gate + destructor); scripts/run-pluginval.sh:46-76
- History [Partially Verified]: CHANGELOG.md [0.8.5] (Linux); commit c924ff8. Windows extension: this change.
- Related incident: `../../POSTMORTEMS.md` INC-006 (Linux editor-automation segfault); KNOWN_ISSUES KI-007 (Windows).
