# NR-006 / NR-010 / NR-014 native evidence package

## Scope and current disposition

PR #202's Accepted [ADR-0070](../docs/architecture/design-decisions/ADR-0070-validated-atomic-scope-windows.md)
implementation and PR #204's rotary titles are integrated on main
`34cc369669a01e9f5f242dfaf2c226686572e4f1`.
This package prepares remaining measurements and native observations; it does
not record their completion. Available execution is a shared Linux container.
Its smoke results establish tool operation only. Representative hardware,
native host observations and owner acceptance remain Pending.

Use the existing [performance procedure](../docs/architecture/PERFORMANCE_BUDGET.md#how-to-produce-those-numbers--required-benchmark-procedure)
and [NR-006 evidence](NR006_SCOPE_PUBLICATION_REVIEW.md). The new mode supplements
the original benchmark and retained experiment data. It changes no shipping
source, CMake target, dependency, parameter, signal order or atomic protocol.

## Build and collect

Use a quiet, physically identified native machine, mains power and recorded
power/thermal conditions. Close unrelated heavy applications. Record any
virtualization, translation, power management or background load. Do not call
shared-runner results representative. Requirements: Git, Python 3.10+, CMake,
Ninja, normal platform/JUCE build dependencies and a clean checkout containing
this package. Normal collection requires Linux Clang 22, macOS AppleClang or
Windows MSVC 19.30+, Release, LTO, existing ISA settings and no sanitizer/coverage/
fast-math overrides. Exact compiler version and expanded commands are retained.

From the repository root, use **new build/evidence directories**. Evidence must
be outside the checkout. Linux example (install prerequisites per README):

```sh
python3 scripts/run-native-validation.py collect \
  --build-dir ../anamorph-native-build --output-dir ../anamorph-native-evidence \
  --cxx clang++-22 --machine-note "physical workstation; power mode; cooling; background load"
```

macOS, in a native Terminal (record Rosetta separately):

```sh
python3 scripts/run-native-validation.py collect \
  --build-dir ../anamorph-native-build --output-dir ../anamorph-native-evidence \
  --cxx clang++ --machine-note "Mac model; mains power; power mode; background load"
```

Windows, in **x64 Native Tools Command Prompt for VS 2022**, with Python, CMake
and Ninja on PATH (one line):

```bat
python scripts\run-native-validation.py collect --build-dir ..\anamorph-native-build --output-dir ..\anamorph-native-evidence --cxx cl --machine-note "physical workstation; power mode; cooling; background load"
```

`--juce-path /path/to/existing/JUCE` reuses an existing checkout, subject to the
repository's usual dependency verification. C and C++ compilers are paired
automatically for clang/gcc/cl; `--cc` selects the C compiler explicitly if needed.
JUCE's actual commit/clean state and both compiler descriptions are retained.
`--jobs N` controls build concurrency.
The collector builds existing Bench, DSP/state test and plugin targets. It does
not install plugins (`COPY_PLUGIN_AFTER_BUILD FALSE`), run installers, modify
release objects or automatically run state tests against a live user profile.
DSP allocation/liveness test output is retained; its platform-specific guard
availability must be read from that output. State tests remain the existing CI/
isolated-profile gate, not a new automatic action on an owner's settings.

For tool diagnosis only, add `--smoke --skip-plugin-build` with a fresh output
directory. This reduces repetitions/duration and labels all results synthetic.
Smoke never satisfies performance acceptance. Normal mode defaults to five
repetitions and ten seconds of audio per engine cell. Allow build time plus
several minutes of paced measurements. Collection is synchronous; interrupting
it retains a nonpassing partial journal and raw logs. Do not reuse or overwrite
an interrupted directory; return it alongside any retry.

## What is measured

The collector records machine/OS/architecture/CPU, compiler, source commit and
source hashes, build commands/options, probe hashes, plugin bundle/file hashes,
clock-pair overhead, raw per-call times, input/output hashes and latency.
Five repetitions alternate original/corrected order. Scope pushes cover
32/64/128/256/512/1024/16401 frames; reads cover 1200/8000/8192 frames.
Engine cells cover defaults, working, silence, bypass, each mode, multiband,
oversampling/filter-selection distinctions, 44.1/48/96/192 kHz and normal/large
blocks. Fresh-process memory observations record RSS before construction,
after construction and after prepare at 192 kHz/16401 frames/8x, plus struct sizes.

The historical header is pinned to git blob
`ed24ceab5ced2784d9fe96effdc46f8aeb03f472`. Only original Engine/Bench objects are
rebuilt against that layout; both translation units agree. Actual Ninja commands
retain the same compiler, numerical flags and remaining objects. No source file
is rewritten. Paired output hashes and reported latency must agree. Original
scope access is **serial only**: concurrent original reads would execute the
known data race and produce invalid comparative evidence.

Paced full-engine controls compare original/current with no reader (hidden
reader model). Current-only active/stalled reader cells copy both native window
sizes at 60 Hz; the stalled reader pauses 750 ms while producer progress is
checked. These model reader load, not a JUCE editor, FFT, rendering or audio-device
callback. Real GUI-visible/hidden/idle overhead must be observed below. Input
generation, hash consumption, pacing and sleeps are outside the measured
`process()` interval. Raw call times include clock overhead, which is reported
and not subtracted. These are observed peaks, not WCET/deadline guarantees.
Oversized push cost is normalized by requested input frames, including discarded
prefix frames. Read cost is normalized by copied frames.

`summary.json` reports repetition median, range/spread, worst observed call,
RSS deltas and original/current deltas, with no invented pass threshold.
RSS includes allocator/OS effects and is not exact allocated bytes. Bare-engine
idle/silence costs do not establish actual host suspension or GUI idle cost.
The native mode mirrors the shipping callback's `ScopedNoDenormals` and records
denormal handling/default nearest rounding; missing/incorrect FP observations
are rejected. The existing default benchmark keeps its original environment.
All acceptance fields remain null until reviewed evidence supports an explicit
owner decision. A tool-complete run is not an accepted cost budget.

## Native GUI and host procedure

Copy/edit the generated `manual-acceptance.json`; keep its raw observations and
acceptance decisions separate. Record operator/date, exact OS/CPU, host/version/
architecture, format, actual loaded module path/hash, source/build, audio device/
driver, rate/buffer, display scaling/refresh and tool versions. Confirm the host
loaded this build, not the unchanged 0.10.0 draft. Match
`plugin-build-identity.json`; store screenshots/video/logs under `manual/`.
Keep a separate worksheet for each host/format/rate/buffer/display configuration.
Save the disposable test session or its parameter-state/preset evidence, and keep
parameters unchanged across comparisons; host defaults are not assumed to equal
the benchmark's working cell. Do not overwrite earlier observations.
Use a disposable session and ordinary monitor-safe gain. Add the generated
14-second stereo `playback-fixture.wav` (active 0–4 s, silent 4–7 s, active 7–11 s,
silent 11–14 s); its exact bytes/hash/segments are recorded. Host resampling and
buffer settings must be recorded. Start at 48 kHz/128 frames; repeat transitions
at a supported small and large host buffer. Record unsupported settings.

| Worksheet ID | Execute and record |
|---|---|
| `scope-playback` | Play active segments with editor visible; inspect stable, live stereo vectorscope and freezes/glitches. Compare the known input orientation/phase changes with observed movement. |
| `spectrum-fft` | Inspect changing spectrum/FFT while playing active and silent segments; record tears, stale display, spikes, hangs or unexpectedly missing updates. |
| `silence-transitions` | Play through both active→silent→active boundaries, letting at least 8192 zero frames reach the engine. Verify stale active trace clears/repaints and activity returns. Repeat with long silence. |
| `rejected-snapshot-retry` | Stress supported large blocks, resize and hide/restore around final silence. Record whether rejection was actually observed with a debugger/test observation and whether a subsequent valid paint recovers. If rejection cannot be established, record **Not observed/Pending**, not a pass inferred from a stable picture. Deterministic State150 rejection controls are separate automated evidence. |
| `start-stop-seek-resume` | Start/stop, seek within active/silent portions and resume. Record audio/display continuity. A stopped host may deliver no zero frames: retained history alone is not a failure; prepare/reset deliberately preserve scope history/count. |
| `editor-close-reopen` | Close/reopen during active playback and silence, at least ten cycles; inspect audio continuity, fresh display, crashes and memory growth. |
| `hidden-restored` | Hide/minimize/restore editor during active playback and long silence. Verify update recovery; record whether host continues audio while hidden. |
| `resize-refresh` | Resize repeatedly in Simple/Advanced layouts and use available display refresh/scaling settings. Record staleness, clipping and recovery; unavailable displays remain untested. |
| `multiple-instances` | Run two instances with distinguishable inputs; open/close/hide independently. Verify their traces/parameters remain independent and host remains responsive. |
| `host-lifecycle` | Save/reload a disposable project, stop/resume device, and remove/reinsert plugin while playback is stopped. Record crashes, stale handlers, unexpected parameter/state changes or stuck processing. |

For active, input-silent and transport-stopped conditions, compare editor visible
and hidden, then reopen/restore. After warm-up record **five** equal-duration
observations (at least ten seconds each) with host CPU and observed longest-block/
real-time-peak metric where available, underruns and process RSS. Include raw
host meter captures/logs, durations and units in `host_performance_repetitions`.
Loop fixture 0–4 seconds for continuous active input, or 4–7 seconds for
continuous zero input while transport runs; transport-stopped is a separate cell.
Define whether each metric covers the plugin, track, audio thread or whole host;
host-wide peaks must not be attributed entirely to this plugin. If the host
does not expose longest-block timing, record it unavailable. Do not infer a peak
from average CPU. GUI memory and whole-host results supplement, not replace,
the engine measurements. No known-racy legacy host run is required.

## NR-014 native accessibility procedure

Use macOS Accessibility Inspector/VoiceOver or Windows Inspect/Accessibility
Insights/Narrator where available. On Linux record the actual supported native
tool/backend; do not assume JUCE/desktop integration. Record tool/OS/host versions,
observed accessibility tree, spoken labels and successful/failed actions.

| Worksheet ID | Execute and record |
|---|---|
| `ax-names` | Inspect all 12 rotaries in their relevant mode/Simple/Advanced layouts: Drive, Amount, Width, Delay, Density, Rate, Depth, Mix, Output, Balance (output), Freq and Balance (input). Preserve the two existing Balance captions; distinguish their layout/location. Verify exposed names match visible captions. |
| `ax-values-actions` | Inspect value/range, change a control using the assistive tool and verify the attached parameter and displayed value. Observe host automation feedback. Record unsupported actions. |
| `ax-focus-navigation` | Attempt native keyboard/assistive focus traversal and delivered arrow adjustment. Record actual host routing, reachable/unreachable controls and focus indication. Automated delivered-key coverage does not prove Tab traversal; this patch did not change the existing rotary keyboard-focus policy. |
| `ax-modifiers-attachments` | Exercise fine adjustment and existing reset modifiers supported by this OS; observe value/host parameter agreement and unrelated controls. Record exact keys/buttons and host interception. |
| `ax-destruction-reopen` | Switch Simple→Advanced→Simple, close/reopen and remove the plugin; verify accessibility elements refresh, values remain attached and no stale actions/crash occur. |

For each check use **Pass**, **Fail**, **Pending**, **Unavailable** or **Not observed**,
with concrete evidence and reproduction steps. Missing capabilities are not
passing acceptance. Record broader traversal/usability issues separately from
the bounded metadata correction. Do not apply speculative host focus changes.

## Return and evaluate evidence

After actual observations, save the worksheet and supporting files, then run:

```sh
python3 scripts/run-native-validation.py analyze --evidence-dir ../anamorph-native-evidence
python3 scripts/run-native-validation.py pack --evidence-dir ../anamorph-native-evidence --archive ../anamorph-native-evidence.zip
```

Use `python` on Windows. Analysis rejects incomplete/modified matrices, missing
repetitions/provenance, inert work, original concurrent reads, output/latency
disagreement and unsupported normal builds. It does not accept performance or
manual checks. Pack also retains failed/interrupted collections, excludes
temporary original objects/executables, writes a SHA256 file manifest and prints
the archive SHA256. Return the zip plus that digest and exact host details for
review; retain the build and original evidence locally until reviewed. Inspect
paths/machine/host details before sharing. Do not edit raw measurements to hide
outliers or failures; explain noise and rerun into a new directory.

Review provenance/completeness first, then paired medians, spread, worst calls,
struct/RSS changes and actual host observations. Investigate repeatable product
regressions separately from clock noise, process scheduling and tool failures.
The owner must separately confirm representative context and accept costs and
native GUI/accessibility outcomes. Pending observations and NR-010 numerical
budgets remain open; this package grants no release, install, merge or architecture
authorization. The unchanged 0.10.0 draft excludes these post-tag corrections.
