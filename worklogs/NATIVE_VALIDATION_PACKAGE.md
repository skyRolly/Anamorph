# NR-006 / NR-010 / NR-014 native evidence package

## Owner-accepted native performance evidence — 2026-11-07 (Asia/Shanghai)

The owner supplied `anamorph-native-evidence.zip` (SHA-256
`2468efadf36730029b135996908c696e829eccd9cbb57a95249920aca5ae7c9e`) and explicitly
**approved ADR-0070 Acceptance and rollback item 5**, accepting the measured native
performance cost for the supplied machine and workloads with the stated limitations.
The later owner decision is separate from the archive: its `performance_acceptance`,
`performance_cost_owner_acceptance` and `representative_acceptance` fields were null
when collected and remain unchanged. The owner also states Level-5 and all other
applicable manual validations are completed, and completed roadmap work has its
applicable human acceptance. These are owner attestations, not individual agent-observed
host results. Exact human-tested artifact, DAW/versions, session, individual results and
actual manual test dates are **NOT RECORDED**; do not derive them from the benchmark.

### Provenance and integrity

The supplied ZIP is held with the owner handoff; it is not embedded in this repository.
Independent verification found 1,229 ZIP entries, including `SHA256.json`; all 1,228
manifest entries exist and match, with no unlisted payloads or duplicate/unsafe paths.
The 400 planned requests, 400 one-line raw outputs, completion journal, 80 five-repeat
metric groups and 38 comparisons reconcile. An independent recomputation of each group
median, minimum, maximum, spread and worst observed call, plus each relative comparison,
agrees with `analysis.json`/`summary.json`. The two summary files agree. All paired
original/corrected no-reader records have matching input/audio hashes and latency for
all five repeats. Raw result stdout/stderr and per-case controls were checked; zero raw
stderr files were nonempty. This is evidence review, not an independent rerun on the
Mac. Reproduce the independent check with
`python3 worklogs/native-validation/verify-adr0070-archive.py <archive.zip>` in a
checkout containing the measured commit; it reads the ZIP without extracting or
modifying it. Disposable negative controls detected a changed raw file, a changed
timing with refreshed manifest, and a duplicated comparison with refreshed manifest.
The uploaded archive must be retained separately.

`source.json` and `run-status.json` identify reviewed PR #225 head
`a0a48290479cac99f74c6a1391990263c82dbf29`; all 65 listed source-file SHA-256 values
match that Git commit. The historical ScopeBuffer fixture SHA-256
`1be1baed6bbb439ad5b517ea2efe4cacd80fbb849959224c2561c38a84588e60` and Git blob
`ed24ceab5ced2784d9fe96effdc46f8aeb03f472` match both the fixture and source commit
`00de8c7796f37d08a338256e01830bb312ba105c`. The original overlay compiles that old
header with the current benchmark/engine layout in a nonshipping build; it does not
replace production source. Only two original objects and their output paths differ;
recorded compiler flags and the other linked objects are shared. Original concurrent
reads are deliberately excluded because the old plain storage has a data race.

The archive reports a physical Intel MacBookPro16,1, i9-9980HK (8 cores/16 logical),
64 GiB, macOS 15.8.1 build 24H32 x86_64, on AC power without heavy background work.
This is representative of the owner's accepted **measured Intel Mac scope**, not proof
for other machines or a population-wide sample. It records Apple Clang 17.0.0.17000604,
CMake 4.4.4, Ninja 1.13.2, Python 3.11.0, JUCE 9.0.5 at
`d60b7a78030661574a615b586d9325ba7a7fa7dd`, Release, `-O3`, `-flto`,
`-march=haswell`, `-ffp-contract=off`, C++23 and NDEBUG. Both variants explicitly use
`-nostdinc++ -isystem` with the Command Line Tools `MacOSX.sdk/usr/include/c++/v1`.
The owner reports this header selection was needed because the default Command Line
Tools libc++ 13 headers lacked `std::bit_cast`; a separate failed-build log is not in
the archive. The CMake compiler description records implicit `c++` linkage. The archive
does not record an SDK version/header digest, resolved SDK symlink target or `otool -L` runtime
linkage, so those cannot be independently identified here; CI's actual shipping
compiler/deployment and package checks remain authoritative. Benchmark binary hashes
are reported as original `c9fda8e6f7b0cac7c46cf7a9564d80b38e0bef798a7a3f300746e03130291bc2`
and corrected `3a5f3df1a5f466ef8016376d5a08031efa996c8c845c0a3b90d37e4b64af0c85`;
the binaries themselves are not in the ZIP, so their hashes and runtime linkage were
not independently rechecked. The merge source `1a4927636f7229ddb64748590f2bf295c6f865ec`
differs from the measured head only in PR #226's CodeQL/MSVC workflow action pins;
source equivalence is not a claim of binary identity.

### Measurement results and interpretation

Each reported median below is the median of **five per-run average nanoseconds per
frame**, not the median of individual calls. Spread is `(max run average − min run
average) / median`. Original and corrected runs alternate order. The clock is included,
not subtracted; the observed calibration range is significant for very short pushes.

| Measurement | Original median | Corrected median | Change / scope |
|---|---:|---:|---|
| Scope push, 48 kHz / 128 frames | 0.4755 ns/frame | 1.3487 ns/frame | +183.7%; about +0.112 µs per block in this microbenchmark; run spreads 14.2% / 2.3% |
| Scope push, 48 kHz / 512 frames | 0.1909 ns/frame | 1.1452 ns/frame | +499.9%; about +0.489 µs per block; run spreads 39.4% / 8.4% |
| Scope read, 8,192 frames | 0.2007 ns/frame | 2.2519 ns/frame | +1,022.3%; about 1.64 µs / 18.45 µs per read; run spreads 6.2% / 6.2% |
| Working engine, 48 kHz / 128, unpaced | 153.011 ns/frame | 152.183 ns/frame | −0.54%; spreads 9.0% / 10.3% |
| Working engine, 48 kHz / 128, paced, no reader | 269.051 ns/frame | 283.755 ns/frame | +5.47%; spreads 19.6% / 10.7% |

Across 27 comparable engine cells the median changes span −4.47% to +7.55%; this is
not a universal CPU bound. Isolated read ratios are large; their absolute measurements
must not be read as a whole-editor multiplier. The corrected-only model reads 8,000- and
8,192-frame windows at approximately 60 Hz. Their isolated median reads total about
36.4 µs per refresh, or 2.18 ms of measured read time per second at that cadence,
excluding consumption, FFT, painting, scheduling and host overhead. Actual GUI/FFT
and DAW performance were not measured. Unpaced silence represents a stopped/idle
engine case; active and 750 ms stalled readers are **models**, measured only for the
corrected implementation. Reader acceptance and producer progress were nonzero.

At 48 kHz / 128 frames the nominal interval is 2.667 ms. Unpaced working-engine worst
observed process calls were 0.116 ms original / 0.456 ms corrected; the corrected
0.456 ms maximum occurred in one repeat, while its other repeat maxima were below
0.102 ms. Paced no-reader maxima were 0.654 ms original / 0.905 ms corrected; in
18,750 calls per variant, two original and four corrected calls exceeded 0.5 ms.
Corrected stalled-reader maximum was 1.786 ms, one call above 0.5 ms among 18,750.
These outliers warrant attention but the data do not identify cause or establish a
formal worst-case time: scheduler/clock and process-only measurements exclude the
wrapper, host and device. At 48 kHz / 16,401 frames the nominal interval is
341.688 ms; paced no-reader observed maxima were 4.781 / 4.803 ms. These intervals
are context, not proof that callbacks always meet deadlines.

ScopeBuffer size is 131,080 / 262,152 bytes and Engine object size is
139,648 / 270,720 bytes, each **+131,072 bytes (+128 KiB)** per object. In the
192 kHz / 16,401-frame / 8× memory case, median fresh-process preparation RSS deltas
were 4,329,472 / 4,341,760 bytes, a 12,288-byte observed difference. RSS samples,
allocator granularity and page commitment are not object size and do not negate the
128 KiB increase; multi-instance process RSS was not measured.

**Disposition:** owner-approved ADR-0070 item 5 for this measured scope. No additional
measurement is required to repeat that accepted decision. Other hardware, actual host/
GPU/FFT behavior, page-residency scaling and formal WCET remain unmeasured; no numerical
budget, real-time deadline guarantee or universal performance equivalence is inferred.

## Historical collection disposition

**Owner-deferred execution — 2026-11-02 (Asia/Shanghai).** Keep this package and
all acceptance/provenance requirements for a later assignment. Do not request
native hardware, access, measurements or manual observations in the current
engineering-review round. The instructions below remain the preserved procedure;
preparation and automated smoke are not native acceptance.

PR #202's Accepted [ADR-0070](../docs/architecture/design-decisions/ADR-0070-validated-atomic-scope-windows.md)
implementation, PR #204's rotary titles and PR #205's validation package are
integrated on main `109d1830a946d16e0a1d2f3f8eb2fd4ad71de0f6`.
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

### Pin the integrated test source

Use an unused working directory on the identified native machine. These Git
commands also work in the Windows native-tools prompt; do not reset an existing
working checkout or replace existing evidence:

```sh
git clone --no-checkout https://github.com/skyRolly/Anamorph.git Anamorph-native-109d183
cd Anamorph-native-109d183
git checkout --detach 109d1830a946d16e0a1d2f3f8eb2fd4ad71de0f6
git rev-parse HEAD
git status --porcelain
```

Confirm the exact commit above and empty status before collecting. Use the
platform command below with new sibling build/evidence directories. Defaults
retain five repetitions; do not add smoke or skip-plugin flags to an acceptance
run. The generated `source.json`, `plugin-build-identity.json` and manual worksheet
bind the evidence to this test build. Loading the old draft is a different test.

No native workstation/access is currently configured for the agent. The pending
handoff needs only OS/version, CPU/model/architecture, and whether authorized
execution access or owner-returned files will be supplied. Once available, run
collection, the existing host/accessibility checklists and the analysis/pack
commands below; return the ZIP and printed SHA256. Preserve incomplete attempts
alongside retries. Preparing these steps is not a native test result.

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
Linux tool CI additionally checks exception cleanup with an ASan fault and
invalid-owner control: the reader must join before engine storage is destroyed.
These instrumented variants and intentional failures are diagnostic evidence,
never performance measurements; existing sanitizer gates remain unchanged.
Oversized push cost is normalized by requested input frames, including discarded
prefix frames. Read cost is normalized by copied frames.
Primitive liveness is independent of the engine control: pushes start from zero
storage, verify publication progress and check the retained new-data tail; each
read poisons both outputs and consumes/checks every copied sample. Preparation,
checks and hashing remain outside the timed call. Raw probe schema2 includes
publication counts and rejects the former empty consumption hash. Earlier
schema1 smoke data is historical and cannot satisfy the new primitive controls.

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
manual checks. Recomputed metrics/classification must match `summary.json` from
collection. A mismatch or missing collection summary fails before writing or
replacing `analysis.json`; preserve the raw data, original summary and any prior
analysis for investigation, and recollect into a new directory when needed.
Pack also retains failed/interrupted collections, prunes root and nested
`original-overlay` build trees (including lifetime-control source links), rejects
symlinks in retained evidence, writes a SHA256 file manifest and prints
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
