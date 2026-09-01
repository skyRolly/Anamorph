# REALTIME_SAFETY_AUDIT.md

Per-module audit against `docs/policies/REALTIME_AUDIO_POLICY.md`. Scope: does the audio path
(`process`/`processBlock`/`reset`) allocate, lock, or do IO? Allocation in `prepare()` is
permitted and noted separately.

Audit basis: full read of `src/dsp/**` and `src/PluginProcessor.cpp` (two independent passes).

## Audit table

| Module | Audio-path status | Allocation (prepare only) | Evidence |
|---|---|---|---|
| `AnamorphAudioProcessor::processBlock` | **Verified** — `ScopedNoDenormals`; param snapshot is atomic loads; no alloc/lock/IO | n/a (engine.prepare) | src/PluginProcessor.cpp:136-204 |
| `AnamorphEngine::process` | **Verified** — all scratch pre-sized; no alloc/lock/IO | prepare(): all buffers + oversamplers | src/dsp/AnamorphEngine.cpp:28-125 vs :660-1339 |
| `MidSide` | **Verified** — pure arithmetic, `noexcept` | none | MidSide.h:21-42 |
| `HaasProcessor` | **Verified** — `process`/`reset` use pre-sized vectors (`std::fill`, no resize) | prepare(): `bufL/bufR.assign` | HaasProcessor.cpp:15-22,46-63 |
| `VelvetNoise` | **Verified** — no alloc/lock/IO; note O(64) per-sample loop + transport-stop `std::fill` (no alloc) | prepare(): `midHist.assign`, RNG construct | VelvetNoise.cpp:10-17,81-139 |
| `ChorusEngine` | **Verified** — `std::sin` per sample, pre-sized buffers | prepare(): `bufL/bufR.assign` (sized to 8× rate) | ChorusEngine.cpp:11-19,55-112 |
| `MonoMaker` | **Verified** — per-sample `setCutoffFrequency` (in-place coeff recompute, no alloc) | prepare(): scalar only (`LR4Xover` state is flat — no heap since Wave 2 / H6) | MonoMaker.cpp:7-18,25-47 |
| `MultibandWidth` | **Verified** — capped cutoff moves (0.8.10: per-sample coeff recompute while tracking under the R(f) = 4·max(1, f/300) oct/s slew cap; one ~12 ms dual-bank crossfade with 2× filter ticks on a discrete target step), no alloc/lock/IO | prepare(): scalar only (24× `LR4Xover.prepare`, flat state — no heap since Wave 2 / H6) | MultibandWidth.cpp (prepare/reset/glide + fade trigger/processBlock) |
| `SoloMonitor` | **Verified** — capped cutoff moves (0.8.10, as MultibandWidth) + `SmoothedValue`, no alloc | prepare(): 6× flat-state filter + smoother reset | SoloMonitor.cpp (prepare/reset/glide + fade trigger/process) |
| `LoudnessMatch` | **Verified** — fixed nested biquad structs; `pow/log10/tanh`; no alloc | prepare(): coeff compute only | LoudnessMatch.cpp:47-156 |
| `CorrelationMeter` | **Verified** — scalar one-poles only | none | Correlation.h:36-95 |
| `LevelMeters` | **Verified** — scalar envelopes; NaN self-heal per sample | none | LevelMeters.h:60-167 |
| `ScopeBuffer` | **Verified** — fixed `std::array`, lock-free SPSC | none | ScopeBuffer.h:28-57 |

## Cross-cutting findings (Verified)

- **No `new` / `malloc` / `std::vector::resize` / mutex / file IO on any audio path** across
  all 12 DSP modules + the processor. All heap allocation is confined to `prepare()` (via
  `std::vector::assign` or `juce::dsp::*::prepare`).
- **Non-finite guard:** an engine-wide per-sample NaN/Inf check replaces only non-finite
  samples with 0 and resets stateful nodes; it is not a level limiter and never alters valid
  audio. Evidence: src/dsp/AnamorphEngine.cpp:1310-1354.
- **`reset()` paths run `std::fill`/filter resets** but never allocate, and are invoked at safe
  points (prepare, host reset, the silent duck bottom, NaN self-heal).

## Mechanical enforcement (since 2026-08-18, ADR-0029)

This audit is no longer the only thing standing behind its own claims. `AnamorphEngine::process`
carries `ANAMORPH_NONBLOCKING`, and the `realtime` CI job builds the DSP suite with
`-fsanitize=realtime` and runs it on every push: any allocation, lock or blocking call reached from
the engine's audio entry point aborts the job at the offending frame. Demonstrated both ways before
it landed — the suite runs violation-free under RTSan, and a seeded allocation in `process` fails
the run at **exit 43** naming the offending frame. The RTSan build reports **156** of the suite's
**162** checks: Test 38's own assertions stand down there, because the allocation guard's interposers
would otherwise shadow RTSan's allocation interceptors and blind the lane (measured; see
`tests/AllocationGuard.h` and ADR-0029 §7). RTSan covers that violation class itself in that build,
so nothing is lost.

**What that does and does not cover.** It is a runtime tool on Clang/Linux, so it sees exactly what
the DSP suite executes and nothing the shipped MSVC/AppleClang binaries do differently. It does not
retire the per-module reading below; it makes a regression in the *executed* paths fail loudly
instead of surviving to a DAW.

**Two further tiers now cover what RTSan cannot** (both landed 2026-08-18, ADR-0029 §7):

- the **allocation guard** compiled into the DSP suite (`tests/AllocationGuard.h`, Test 38) counts
  `operator new` and malloc-family allocations while `process()` runs and asserts zero over 3,840
  armed calls. `operator new` replacement is standard C++, so this tier reaches **MSVC**, which RTSan
  never runs on. Both violation classes were seeded into the real `process()` and caught. It stands
  down in the RTSan and valgrind builds by design (see above), which is why the two tiers are
  complementary rather than redundant: each covers where the other cannot run.
- the **static lint** (`scripts/check-realtime.py`) scans audio-path bodies for the forbidden list
  with no build at all, on every platform — the only tier that reads the branches the suite does not
  execute (measured `src/dsp` coverage: 93.4 % of lines, 79.9 % of branches).

## Items needing a non-static check

`TODO: a sanitizer/RT-audit run (e.g. running the built plugin under a real-time-violation
detector, or auditing JUCE's Oversampling::processSamplesUp/Down for internal allocation) would
upgrade the "no allocation inside JUCE's oversampler call" assumption from inferred to measured.
The plugin's own code is allocation-free on the audio path; JUCE internals are trusted by
construction (initProcessing is called in prepare).`

Source for the OS init: src/dsp/AnamorphEngine.cpp:51-53 (`initProcessing` at prepare).

**Partially measured since 2026-08-18** (this entry does not close the TODO above): a dynamic
allocation-interposition probe over the real engine + the pinned JUCE 9.0.1 — 32 configurations
(4 algorithms × 4 oversampling factors × 2 M/S variants), 7,680 armed `process()` calls with
mid-stream algorithm swaps, bypass crossfades and crossover drags — counted **zero** `operator
new`/`malloc`-family calls on the audio path, *including through
`juce::dsp::Oversampling::processSamplesUp/Down` at ×2/×4/×8*, while the same probe counted the
expected `prepare()` allocations (102 `new` + 663 `malloc`-family) and caught both classes of
seeded violation. **That probe is now a committed gate**: it became
`tests/AllocationGuard.h` + Test 38, which runs the same counting over the same matrix (3,840 armed
calls per run) rather than once in a session.

**The SWITCH is armed as well as the steady state, since 2026-08-19, and until then it was not.**
Each of the 32 configurations is now applied *inside* the armed region, so the block that adopts a
discrete change — `src/dsp/AnamorphEngine.cpp:725-800`: algorithm tails cleared, the three
oversamplers and the chorus reset on an oversampling-path change, the crossover cleared on a
topology change — runs with the counters watching. Before that the configuration was applied and
then `reset()` *outside* the armed region, and `reset()` flushes an in-flight duck straight to its
target (`src/dsp/AnamorphEngine.cpp:150-157`), so every armed block sat in the steady-state
no-change gate and the gate proved the audio path allocation-free only while nothing was changing.
Measured both ways with one allocation seeded into that adopt block: invisible then (3,840 armed
calls, worst `new` 0, green), a failure now (worst `new` 2, worst `malloc` 2). The test also counts
the landings it observes — reported latency is re-latched only in that block — and fails if the
count is zero, so restoring the flush fails the run rather than quietly narrowing it. What the gate
does **not** carry over from the probe is bypass crossfades and crossover drags: `bypass` and
`mbBands` are fixed across its matrix. Those are covered by the click-free-transition tests in the
same suite, and on Linux/Clang by RTSan running that suite. It asserts in every job that builds the DSP suite
except the two where its interposers would fight another tool -- RTSan (it would shadow the
sanitizer's allocation interceptors) and valgrind (memcheck reports the new/malloc pairing as a
mismatched free) -- and in both of those it says so rather than reporting a zero nothing counted.

**The TODO is now largely answered by a committed mechanism** (ADR-0029): the `realtime` job runs the
DSP suite under RealtimeSanitizer on every push, and that suite exercises the oversampled path at
×2/×4/×8 across the whole algorithm matrix — so "no allocation inside JUCE's oversampler call" is
re-measured continuously rather than inferred, and any regression aborts the job at the JUCE frame
that allocated. What keeps the TODO open rather than closed is scope, not doubt: RTSan runs on
Clang/Linux only and sees only the paths the suite executes, so the same assertion for the shipped
MSVC and AppleClang binaries still rests on construction — though the allocation guard's
`operator new` half narrows even that, since it is standard C++ and compiles into the suite on
every platform. The other committed follow-up is `testWrapperProcessBlockAudioPath`
(tests/state_tests.cpp — the wrapper path under the sanitizers/valgrind jobs).
