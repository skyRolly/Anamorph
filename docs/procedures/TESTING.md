# TESTING.md

How to run and interpret the validation suite. Acceptance levels and the hard gate are defined in
`docs/policies/TESTING_POLICY.md`.

## Headless self-tests (DSP + state)

```bash
scripts/build.sh                 # build (produces AnamorphTests + AnamorphStateTests)
scripts/run-tests.sh             # runs BOTH console apps (fail-closed: a missing binary fails)
```

`run-tests.sh` finds `AnamorphTests` and `AnamorphStateTests` under `build/` and runs both; it
exits non-zero on any failed `check` or missing binary. Evidence [Verified]: scripts/run-tests.sh.

### What the tests cover

`tests/dsp_tests.cpp` has **40 DSP tests** using a `check(cond, "what")` harness, covering: MS
round-trip (bit-exact), transparent default, true-bypass null + latency match, Mono Maker
(post-Mix), Multiband mono-compat, Solo band selectivity + transparency, Level Match
(unity/no-ratchet/silence-freeze/mix-coupling/multiband-unity), crossover automation safety,
NaN recovery, four click-free crossfade tests (transitions, bypass, multiband enable,
solo+multiband-enable), the dry-align gate comb regression (`testDryAlignGateRecomb`,
Wave 2 / H4: a Mix dip after a gated full-wet stretch must re-engage the dry bank
phase-matched — the KI-#1 metric), the split-movement regression
(`testMultibandSplitDragNoPitchShift`, Test 29): the worst 100 ms pitch chunk of a 150 Hz tone
must stay < 18 cents (the accepted controlled-FM bound of the R(f) = 4·max(1, f/300) oct/s
slew-limited smoother, ADR-0015 final + slow-drag fix) through drags and the whole catch-up —
including an unbroken crawl-crossing scenario where the crossover passes the tone (~14 cents
measured; the pre-0.8.10 uncapped ~8 oct/s glide measures ~28 and the interim bare one-pole
tracker ~50, both fail) — the max spectral spur around a 1 kHz tone during a 60 Hz-cadence drag
must stay below −31 dBc (measures −41.3; the interim chained bank crossfades measure −28.5 dBc
and the rejected fref=150 cap variant −27, both fail), a discrete 4-octave target step must land
within ~200 ms via the bank crossfade, a RELEASED 6-octave flick must land by plain gliding well
under a second, and a NORMAL-SPEED drag (150 Hz → 12 kHz over 0.95 s at a 60 Hz cadence,
~600 px/s on the real display) must have its audible band edge AT the target 0.1–0.35 s after
release on both paths (the flat 4 oct/s cap of the slow-drag regression measures 0.47 of full
level on the solo path and 0.60 of the width-0 leak on the multiband path — both fail), all
click-free — on both the Multiband and Solo-monitor paths; and the forced-duck dry-fill gain regression (`testDryFillRespectsOutputGain`, Test 30):
with Output Gain at −24 dB an undo/redo-style Mix toggle must not spike beyond 2× the steady
output (the unscaled raw-level fill measures 15.8× and fails) while still filling the dip; and
the forced-swap-during-fade-out regression (`testForcedSwapDuringOrdinaryFadeOut`, Test 31): a
forced bulk swap landing while an ordinary discrete duck is still fading OUT must keep forced
semantics — stale delay-line audio must not replay after the silent bottom (the pre-fix engine,
which dropped the consumed forced request in that window, measures a 0.494-peak Haas-tail replay
against silent input and fails) — while the upgrade stays click-free and the duck still bottoms
at silence; and the high-sample-rate terminal-snap regression (`testHighRateCrossoverSnap`,
Test 32): a moved crossover must land **bitwise-exactly** on its target and let the solo
monitor's settled fast path go cold, at 44.1/48/96/192 kHz, through targets inside the measured
192 kHz float-stall zones (just above the binade edges ≥ 2048 Hz) including the worst one
(16.6 kHz) — the pre-fix glide, whose one-pole add stalls below `ulp(f)/2` while the gap is
still above the terminal-snap eps, rests 0.4688/0.9375/1.8750/3.75 Hz short at 192 kHz, never
goes cold, and fails, while the normal-rate passes double as the unchanged-behavior guard; and
the solo-monitor cold-through-drag regression (`testSoloColdThroughDrag`, Test 33, Wave 3): with
NOTHING soloed, dragging the splits at UI cadence must leave the monitor's settled fast path
engaged — the bank stays cold, the output buffer is **bit-untouched** on every block — and
re-engaging a solo must snap the cutoffs to the freshest drag targets under the engage crossfade
(the pre-Wave-3 gains+cutoffs gate wakes the bank on the first target move and glides instead of
snapping, failing both the stayed-cold and freshest-snap checks); and the parked-Haas
warm-history regression (`testHaasParkedWarmHistory`, Test 34, Wave 4): with Haas selected and
Amount settled at exactly 0 (under FTZ, as on the real audio thread) every block must pass
through **bit-untouched**, re-engaging on silent input must play back audio recorded WHILE
parked (the delay lines must keep recording through the parked fast path — this fails if a
future change stops the parked ring writes), and re-parking must return to bit-transparency
once the wet glide drains; and three feature-coverage tests added after a 2026-08-18
line/branch-coverage audit found these shipped stages had **zero** executions in either suite:
mono-sum input conditioning (`testMonoSumInputConditioning`, Test 35: a pure-side tone is
silenced, a mono tone passes at level with no side content, and mono-sum-off preserves the side
control), M/S input solo (`testMsSoloInputIsolation`, Test 36: Mid solo passes mono / rejects
side, Side solo passes side — and, the documented feedback-#15 property, Side solo on mono
content stays silent even at full Amount because the solo runs BEFORE the widener), and the
Level-Match injection consume paths (`testMatchInjectRestore`, Test 37, feedback #16/#23: both
the un-ducked defensive consume and the forced-duck silent-bottom consume adopt the injected
per-A/B-slot trim as a SEED — measured ≤ −4 dB displayed from a −6 dB injection — after which
MEASURE re-converges as the design intends, with no level slam); and the audio-path allocation
guard (`testProcessIsAllocationFree`, Test 38, ADR-0029): `tests/AllocationGuard.h` replaces
`operator new`/`delete` and interposes the malloc family, arms the counters **only** around
`process()` (allocation in `prepare()` is required by policy), and asserts zero across the same
algorithm × oversampling × M/S matrix — 3,840 armed calls. It is the tier that reaches **MSVC**,
where RealtimeSanitizer does not run, since `operator new` replacement is standard C++. The test
**self-checks its counters first** and discloses any half that is not live: the malloc half is
compiled out under ASan (an executable-defined `malloc` fights ASan's allocator) and the whole
guard is compiled out for the valgrind build (`-DANAMORPH_NO_ALLOC_GUARD` — memcheck reports
`Mismatched free() / delete []` when `operator new` hands back `std::malloc` memory), each with a
`::warning::` rather than a silent pass. Under RealtimeSanitizer the whole guard is compiled out
too, and there it is a correctness requirement rather than a convenience — its interposers would
shadow RTSan's own and blind that lane (ADR-0029 §7). That stand-down is detected by
`__has_feature(realtime_sanitizer)` and **cross-checked from outside the compiler**: the
`realtime` job also passes `-DANAMORPH_RTSAN_LANE=1`, and the header `#error`s if the lane is
declared while the guard is still live, so a renamed or removed feature name fails the build
instead of silently hollowing out the lane.

The newest DSP test is the **A7-9 parked-path liveness gate**
(`testA79ParkedPathsReachableAfterStall`, Test 41). It answers a question this suite could not ask
for two waves: *is a fast path ever actually reached?* `VelvetNoise`, `HaasProcessor` and
`ChorusEngine` each carry a cheap Amount-0 path, and each was gated on the wet glide reaching
**exactly** 0 — which under FTZ it never does, because with a 0 target the update is `a -= k*a` and
the DECREMENT underflows before `a` does, stalling the glide just under `FLT_MIN/k`. Every one of
those paths was therefore dead after a user turned Amount down, and nothing observed it, because on
real signal `x + 1e-35*(d - x)` is bit-exactly `x`.

The oracle is **a second instance of the same module**. `S` is driven the way a user drives it —
engaged, then turned down and left to stall. `P` sees the identical input with Amount at 0 from
`prepare()`, so it is genuinely parked. All three modules record the **input** in their delay lines
rather than their own output, so the two rings hold identical history and any difference between the
two outputs is the residual and nothing else. Three checks per case, each a different claim: real
signal must be **exactly** equal (the "A7-9 changed no audible bit" guard, true before and after);
digital silence must stay within the derived `FLT_MIN/k` stall ceiling; and digital silence must be
**exactly 0** — which is the gate, and which **fails on all four cases against the pre-A7-9
sources**. `ChorusEngine` is run at 48 kHz *and* 192 kHz because its smoothing coefficient is the
only rate-dependent one of the three, so 192 kHz is where the worst case lives and it is asserted
rather than extrapolated. Under `ANAMORPH_TESTS_NO_FTZ` the stall does not occur at all and the three
checks pass without discriminating — lost coverage, not a false pass, the same trade `isBad` makes.

The DSP test before it is the **Velvet gather/per-sample path-equivalence oracle**
(`testVelvetGatherEqualsPerSampleLoop`, Test 40, A7-2T). It exists because the guard described next
is a RELATIVE one: Test 39 compares the build under test against itself at different block lengths,
so its oracle cannot see a defect that is a pure function of the sample stream -- a gather whose
taps all read one sample too deep gives the same wrong answer at every block length. Test 40 supplies
the missing ABSOLUTE reference, and needs no product change to do it: the module already contains two
implementations of the same arithmetic, and the gather's eligibility gate ends with
`numSamples <= (int) accum.size()` while `accum` is sized from `prepare()`'s `maxBlockSize` alone --
so an instance prepared for a SMALLER block runs the per-sample loop over the same audio, with an
identical ring, tap set, weights and coefficients (all derived from the sample rate and seed, never
from the block size). The two must be **bit-identical**. Swept at **44.1 / 48 / 96 / 192 kHz** over
block sizes **32 / 128 / 512 / 4096** plus a **density-1.0** pass: 4096 exceeds `decorrSamps` at 44.1
and 48 kHz, so every tap splits into a ring run plus a same-block tail -- a regime Test 39 cannot
reach, its largest block being 512 -- and density 1.0 activates all 64 taps, where the default 0.5
activates only the shallow 32. It **proves itself live** on a seeded one-sample tap-delay error,
which fails 20 of its 20 equivalence checks at sample 3 of block 0. The same seed is caught by Test 39
too, but through that test's SCHEDULE rather than its oracle -- at block 215, its transport stop;
with the stop removed, at block 247, its moving density; with every path crossing removed, not at all.
**This test is the gate for A7-2**: the ring-gather rewrite is bit-identical when right and silently
wrong-by-a-constant-delay when not, and it must not land before this is green.

The DSP test before *that* is the **Velvet block-length invariance guard**
(`testVelvetBlockLengthInvariance`, Test 39, A7-1 / 0.9.5; renamed under A7-2B). It was written when
`VelvetNoise` carried its H5 linear history image ACROSS blocks -- slid forward rather than
re-gathered -- which made the image cross-block state, correct only while every path that did not
maintain it invalidated it. **A7-2B deleted the image and the slide**, so that state no longer
exists; the test is kept unchanged because its assertion is about the module's contract rather than
that mechanism, and it now guards the ring split's block-anchored arithmetic instead. The test
drives the module through one fixed schedule (engage, park,
re-engage, transport stop, moving density) at **44.1 / 48 / 96 / 192 kHz**, once in 512-sample
blocks, once in 32-sample blocks and once through a **cycle of mixed sizes** (32, 128, 64, 256, 32 --
summing to 512, so the events still land on a block boundary and every neighbouring pair differs),
and requires all three to be **bit-identical**: every piece of state here advances per SAMPLE, so the
output is a function of the sample stream alone, and any per-block bookkeeping that is wrong BY THE
BLOCK LENGTH -- a stale slide offset then, a mis-split ring run now -- perturbs the runs differently
and cannot survive the comparison. The mixed cycle is what
exercises the slide arithmetic's real subject: `linHistSlide` carries the JUST-PROCESSED block's
length, so a run whose blocks never change size could be correct with the offset confused for a
constant. The comparison is made on BITS rather than with `==` -- `-Wfloat-equal` is at zero in the
Clang baseline, and a float `==` is the wrong predicate for a bit-identity claim anyway, calling +0
and -0 equal (which this module's own signed-zero algebra cares about) and NaN unequal to itself. The same test passes unchanged against the pre-0.9.5 engine, so it asserts a
contract the module already had rather than one invented for the change. It **proves itself live**
three ways: the engaged stretch must really decorrelate; the transport stop must really flush the
wet (measured 15.4-25.2 % of the engaged figure with the stop, 90.6-128.9 % with the stop event
removed, so the 50 % bound sits between two measured populations); and both defect classes were
seeded and caught -- a wrong slide fails at sample 32, a missing invalidation at the stop block.
`worklogs/performance/PERF_AUDIT_v0.9.5_IMPLEMENTATION.md` §2.2.

The suite
additionally carries **one state-restoration robustness guard**,
`testAbActiveClampOnCorruptState` — it drives a corrupted `<AB active="…">` blob through the same
read+clamp the processor uses (`anamorph::clampAbSlotIndex`, `src/AbSlotIndex.h`) and asserts an
out-of-range A/B index can never index `abSlot[]`/`abUndo[]` out of bounds, while valid 0/1 are
preserved. Evidence [Verified]: tests/dsp_tests.cpp (`main` registers all tests).

### State-compatibility self-tests (v0.8.13 harness)

`tests/state_tests.cpp` (**15 tests**, own console target `AnamorphStateTests`) automates the
COMPATIBILITY policy family against the **real `AnamorphAudioProcessor`** (the target compiles
the plugin sources; since 2026-08-21 it also constructs and destroys the real editor, headlessly
and without ever showing it — no peer, no message loop, no interaction):
serialized-schema shape (every `SERIALIZATION_REGISTRY.md` field), a **parameter-registry
snapshot** (IDs/names/order/automation flags/step texts exact + range mappings probed at 5
normalised points, vs `tests/fixtures/parameter_registry.snapshot`), a raw-exact
save→load→save round-trip (byte-identical; APVTS + `raw` + InternalState + A/B slots + preset
meta; undo cleared), the three legacy migration paths via frozen fixtures
(`legacy_v0_2_bare_apvts.xml`, `legacy_pre_0_6_4_ab_slots.xml`,
`legacy_pre_0_8_4_view_params.xml`), corrupt/foreign-state robustness (garbage/truncated blob,
out-of-range `AB@active` clamp end-to-end, unknown future fields, corrupt slot XML), the user
preset save→reload round-trip incl. the exclusion rules (`mbSolo` reset, Bypass/`advancedMode`
untouched), A/B + view-param preservation across restore, **factory/user preset identity when a
user preset carries a factory preset's name** (0.9.2: saving under the shared name selects the USER
row, both rows stay individually selectable, an A/B round-trip keeps the identity, an undo after a
save keeps it too, a preset switch invalidates redo when the identity moves even if the two presets
sound identical **but re-picking the already-selected row does not**, and a
`.anamorph` loaded from OUTSIDE the preset folder or a user preset deleted from disk both tick
**nothing** rather than falling back to the same-named factory row),
**factory-id integrity** (ids present, unique, and every one resolving in the table — an
unresolvable id would apply the plain defaults, so exactly one factory preset may sit on the
all-defaults signature), and the **indicator identity across a session reload** (factory and user
identities restore, per A/B slot; an unresolvable factory id, a deleted user preset, a preset nested
in a SUB-folder of the preset folder, a preset whose file NAME `juce::File::isAbsolutePath` accepts
(a leading `~` on POSIX) and a pre-0.9.2 session with no identity each take their documented
fallback; and in EVERY one of those
eight paths — the seven that go through the reload helper plus the A/B slot check — the restored
parameters are asserted bit-identical, because the identity is metadata and must never influence the
sound), and the **wrapper audio path** (`testWrapperProcessBlockAudioPath`, 2026-08-18: the real
`processBlock` over a denormal-provoking noise→silence matrix with **no test-side FTZ arming**, so
it regresses `processBlock`'s own `ScopedNoDenormals` — and it is the only test in either suite
that drives the wrapper's audio path, which is what points the `sanitizers` job's ASan/UBSan and
valgrind runs of this suite at the wrapper's parameter snapshotting and buffer handling; a
liveness RMS check first proves the invariant is not vacuously green, and the
`ANAMORPH_TESTS_NO_FTZ` escape relaxes only the denormal half, exactly as in the DSP suite),
and the **editor lifetime** (`testEditorConstructDestroy`, 2026-08-21: `createEditor()` five times
over, each laid out and destroyed through `editorBeingDeleted`, asserting the premise first — a
non-null editor of the concrete type — then that layout ran and that **no peer was created**, so
"headless" is a property the test proves rather than one the environment happens to supply).
It needed no new target and no CMake change: this binary already compiled `PluginEditor.cpp` and
already linked `juce_audio_utils`/`juce_dsp`/`juce_opengl`, because `createEditor()` references
them — it had simply never been called. What that buys is the reason to do it here at all: this
suite already runs under **ASan+UBSan+vptr, LeakSanitizer, valgrind memcheck and LTO codegen**, so
the editor's constructor and destructor — 68 direct children, three LookAndFeels, an
`OpenGLContext` member, a `VBlankAttachment` and a `FrameClock` — now run under all of them.
Measured clean on the first exposure: 940×720, 68 children, five construct/destroy cycles, no
sanitizer report. **Linux-only by construction** (`#if JUCE_LINUX || JUCE_BSD`): headless editor
construction is unverified on Windows and macOS where this suite is also a blocking gate, and
KI-007 records that the GPU-less Windows runner cannot host editor GUI tests at all — every
instrument this test feeds is a Linux job, so the scoping costs no coverage. Widening it needs one
green run on the other two, not an argument.
Evidence [Verified]: tests/state_tests.cpp; CMakeLists.txt (`AnamorphStateTests`).

**Changing the parameter surface intentionally** (ADR + `PARAMETER_REGISTRY.md` update
required, per `PARAMETER_COMPATIBILITY_POLICY.md`): re-freeze the snapshot with
`AnamorphStateTests --write-snapshot` and let the snapshot diff be reviewed in the PR. An
**unintentional** change fails the suite on all three CI platforms — that is the point.
The registry comparison is numerically tolerant (1e-4 relative) only for the numeric fields —
the five range-mapping probes, the default, and the interval (libm ULP differences across
platforms); IDs, names, ordering, flags, counts and step texts compare exactly.

### Adding a test

Bug fixes ship a regression test that **fails on the old code and passes on the fix** (the
project's established practice; `docs/policies/TESTING_POLICY.md`). Use the existing
`check(cond, "description")` harness and add the call in `main` (DSP behaviour →
`tests/dsp_tests.cpp`; state/serialization/preset behaviour → `tests/state_tests.cpp`).

## Opt-in targets (not built by default, not shipped)

Three targets exist behind OFF-by-default options or outside CMake entirely. None of them enters a
release build; each answers a question the two self-test suites structurally cannot.

| Target | How to build | What it answers |
|---|---|---|
| `AnamorphBench` | `-DANAMORPH_BUILD_BENCH=ON`, Release | The `PERFORMANCE_BUDGET` §"required benchmark procedure" matrix — ns/sample and worst single block across sample rate, block size, algorithm, oversampling and multiband. |
| `AnamorphFuzzState` | `-DANAMORPH_BUILD_FUZZ=ON` with Clang + `-fsanitize=address,undefined` | `setStateInformation` against inputs nobody wrote by hand. |
| `AnamorphDspDump` | `-DANAMORPH_BUILD_DSPDUMP=ON`, Release | Whether a dependency bump changed engine output at all — §Proving a dependency bump is bit-identical. |
| `tests/realtime_effects.cpp` | no target — `clang++ -fsyntax-only -Werror=unknown-warning-option -Werror=function-effects`, then the same command again with `-DANAMORPH_EFFECTS_CANARY`, which must FAIL | Whether the JUCE-free leaf DSP is provably effect-clean at **compile** time, on branches no test executes — and, through the second compile, whether the diagnostic proving it is still active at all. |

```bash
# Benchmark. It REFUSES to run (exit 2) if it cannot identify the CPU and
# ANAMORPH_BENCH_CPU is unset -- PERFORMANCE_BUDGET constraint C2: a number
# without its machine and method is not a measurement.
cmake -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_BENCH=ON
cmake --build build-bench --target AnamorphBench
ANAMORPH_BENCH_SECONDS=10 ANAMORPH_BENCH_REPS=5 \
  ./build-bench/AnamorphBench_artefacts/Release/AnamorphBench

# State fuzzing. A REJECTED blob is a pass; the oracle is the sanitizer.
cmake -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=clang-22 -DCMAKE_CXX_COMPILER=clang++-22 \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DANAMORPH_BUILD_FUZZ=ON -DANAMORPH_BUILD_TESTS=OFF -DANAMORPH_BUILD_STANDALONE=OFF
cmake --build build-fuzz --target AnamorphFuzzState
# Note: libFuzzer SAVES new coverage-increasing inputs into the corpus
# directory it is given. They are .gitignore'd (only the `*.bin` seeds
# are tracked), so a local run cannot add them to a commit by accident.
# The note sits ABOVE the command, not between its continuation and its
# arguments: a `\` followed by a comment line splices the two into
# `ASAN_OPTIONS=... # ...`, which sets nothing for anything, and the fuzzer
# then runs on the next line with leak detection ON -- see below.
ASAN_OPTIONS=detect_leaks=0 \
  ./build-fuzz/AnamorphFuzzState tests/fuzz-corpus -max_total_time=90
```

`detect_leaks=0` on the fuzz run is required, not optional: the harness leaks JUCE's
`ScopedJuceInitialiser_GUI` on purpose, because letting `shutdownJuce_GUI()` run under libFuzzer's
`exit()` double-frees in `DeletedAtShutdown::deleteAll()` during `__run_exit_handlers`. Leak coverage
for the same code is the `sanitizers` job's, which runs with `detect_leaks=1`.

**CI now builds all four and gates on three of them.** The fuzz run and the compile-only effects
check are hard gates on their output. `AnamorphDspDump` joined them on 2026-08-21 and is the third:
`linux-lto-tests` runs it with `--self-check`, which asserts every scenario is repeatable and that
they are distinct from each other, and exits 3 if not — so the gate is on the instrument's ability to
discriminate, not on its hashes. Nothing in CI diffs those hashes and nothing stores them; that is
still a human's step at bump time (§Proving a dependency bump is bit-identical). Until that date no
job built this target at all, which made it the one committed harness with no protection against the
rot its neighbour's CI step exists to prevent. The benchmark is built and smoke-run but its
*numbers* are not gated — measured run-to-run spread on an idle machine is 7.2% (median ns/sample)
and 65.4% (worst block), so a threshold would be noise rather than signal; what the build catches is
a harness that has silently stopped compiling against the engine it measures. The fuzz run and the
compile-only effects check are hard gates.

## Proving a dependency bump is bit-identical

`DEPENDENCY_POLICY.md` rule 2 makes bit-identical engine output the gate a JUCE bump must pass.
`tests/dsp_dump.cpp` is the instrument, and it is **committed** — the two bumps that passed this rule
before it existed each used a scratchpad tool that was then discarded, so the gate was permanent and
the instrument was rebuilt from scratch every time.

The tool prints one deterministic line per scenario: an FNV-1a hash over **every output byte** plus
the engine's reported latency, across 32 scenarios (4 algorithms × 4 oversampling factors × M/S
off/on) at 48 kHz / 512 samples, 120 blocks of fixed-seed noise then 120 of digital silence — the
silence phase is what catches denormal and tail differences the noise phase hides.

```bash
# Build the SAME source against two JUCE checkouts, otherwise identical flags.
for JUCE in /path/to/JUCE-old /path/to/JUCE-new; do
  out="dump-$(basename "$JUCE")"
  cmake -B "build-$out" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DANAMORPH_BUILD_DSPDUMP=ON -DANAMORPH_BUILD_TESTS=OFF \
        -DANAMORPH_BUILD_STANDALONE=OFF -DANAMORPH_JUCE_PATH="$JUCE"
  cmake --build "build-$out" --target AnamorphDspDump
  "./build-$out/AnamorphDspDump_artefacts/Release/AnamorphDspDump" > "$out.txt"
done
diff dump-JUCE-old.txt dump-JUCE-new.txt && echo "bit-identical"
```

An empty diff is the proof. Any differing line names the exact scenario to investigate, and the
latency column moving is its own finding — a reported-latency change is an AI-agent hard stop.

**The tool checks itself before it reports, every run, not on request.** Two properties, because
they fail independently: every scenario must be **repeatable** (the same scenario run twice hashes
the same — otherwise every diff is noise) and all 32 must be **distinct from each other**
(otherwise a diff is empty for the wrong reason). It exits **3** rather than printing a table it has
not shown to be discriminating.

That second check is not hypothetical. The first run of the original scratchpad tool left
`algoAmount` at its `0` default, which is identity for the wet path, so the algorithms hashed the
same as one another and the tool reported 32 matching hashes while never reaching the code under
test. It was caught by a human noticing two rows that should differ did not. Setting `algoAmount`
back to `0` in the committed harness today reproduces it exactly — 16 colliding scenario pairs,
named, exit 3. **Fix the scenario set; never relax the check.**

Two build choices are deliberate. It does **not** link `juce_recommended_lto_flags`, unlike
`AnamorphBench` beside it: the bench must measure the shipped binary so it carries the shipped
flags, while this tool must isolate one variable and LTO is a second one — link-time inlining can
differ between two runs for reasons unrelated to the dependency under test. And nothing is stored:
no committed golden hashes, because that would be the golden-master DSP baseline this repository
deliberately rejects. The question is never "does this match a stored value" but "does build A match
build B", and only a diff between two runs answers it.

## pluginval (VST3 + AU conformance)

```bash
scripts/run-pluginval.sh 10 deterministic        # fixed seed (mode A)
scripts/run-pluginval.sh 10 randomise            # --randomise x3 (mode B)
scripts/run-pluginval.sh 10                      # deterministic (default mode)
scripts/run-pluginval.sh                         # default strictness 8 (the working bar)
scripts/run-pluginval.sh 10 deterministic au     # the AU, macOS only (see below)
```

Strictness targets (spec 11.3): `5` development, `8` standard gate, `10` pre-release gold standard.
The value CI enforces is `ANAMORPH_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml`, which is
the single place it is written down.

Each `mode` — `deterministic` and `randomise` (`--randomise`, randomised test **order**) — runs
**3 consecutive** passes, and **both must pass on all three platforms** (Windows uses
`run-pluginval.ps1`): the randomise mode exercises state restoration under an order a fixed run
cannot reach.

**The deterministic seed is nonzero, and it did not used to be.** Both scripts passed
`--random-seed 0`, which pluginval reads as *"generate a random seed"* (`Source/PluginTests.h`;
`Source/CommandLine.cpp` forwards the flag only when it differs from that default) — so the
"deterministic" mode drew a fresh seed on every run and a failure in it could not be reproduced from
the log. The seed is now pinned to a nonzero value, identically in both scripts. It is meaningful
*without* `--randomise`: it seeds the RNG the tests themselves draw from, while `--randomise` only
shuffles test order, so the two flags are independent.

**The third argument is the format**: `vst3` (default) or `au`. `au` is macOS-only and **errors**
(exit 2) on any other host rather than skipping — a gate that quietly does nothing is worse than no
gate. The AU is the only format Logic and GarageBand load, and it is validated in CI on macOS in both
modes ×3, exactly like the VST3. macOS resolves Audio Units through the **AudioComponent registry**,
which only knows about bundles under a `Components` directory, so a freshly built, never-installed
`.component` can report *zero plugin types* however correct it is. Install it first — CI copies it to
`~/Library/Audio/Plug-Ins/Components` and kills `AudioComponentRegistrar` to force a re-scan — and
point the script at it with `ANAMORPH_PLUGINVAL_BUNDLE`. That variable is fail-closed: set but
missing is an error, never a silent fall back to discovery.

Discovery of the bundle is fail-closed on **ambiguity** too: exactly one match under `build/` is
required. The previous `find … | head -n1` validated whichever bundle enumerated first, so a stale or
multi-config tree could pass the release gate on a bundle that was not the one just built — a
local-only hazard, which is exactly where it would go unnoticed.

The script downloads pluginval if absent (a failed `chmod +x` on it is now an error rather than
`|| true`, so a setup fault reports where it happens instead of resurfacing as an opaque "cannot
execute" from the validation loop), and runs under `xvfb-run` when available (Linux editor tests need
a display). Evidence [Verified]: scripts/run-pluginval.sh / scripts/run-pluginval.ps1.

### Signal-only retry (known X11 host flake)

`run-pluginval.sh` (and `run-pluginval.ps1` on Windows, without the X11-specific retry) treats a real
validation failure (exit < 128) as a failure immediately. On Linux it retries up to 3 times **only on
a signal-crash** (exit ≥ 128) to absorb a use-after-free in **pluginval's own JUCE** X11
`XEmbedComponent` (a `ConfigureNotify`→`callAsync` on rapid editor open/close), not a plugin defect —
the plugin already drops its OpenGL child window on Linux (ADR-0011).

**The retry is scoped to the platform its justification names.** Until 2026-08-18 the script applied
the same three attempts on **macOS**, which shares none of that X11 machinery: there, a crash had two
extra chances to pass and no documented flake to absorb, and this section already described the
behaviour as Linux-only. `CRASH_RETRY_ATTEMPTS` is now set from `uname -s` — 3 on Linux, **1**
everywhere else — and a single-attempt failure prints a distinct message so it cannot be misread as
an exhausted retry. Evidence [Verified]: scripts/run-pluginval.sh (`run_one_pass`, and the `case
"$(uname -s)"` above it).

## CI integration

All three build jobs run the self-tests + pluginval in **both** modes (deterministic ×3 + randomise
×3), and **all three are blocking** — Windows/macOS do not use `continue-on-error`, so a non-zero
pluginval exit fails the job on every platform. Linux/macOS use `run-pluginval.sh`; Windows uses
`run-pluginval.ps1`. macOS additionally runs the whole gate a second time for the **AU**, and runs
the self-tests a second time for the **x86_64 slice under Rosetta 2**.

A fourth build job, **`macos-intel`** (`macos-15-intel`), runs the same self-tests and the same
full gate — VST3 and AU, both modes ×3 — against a thin `x86_64` build on **native Intel
hardware**, and it is blocking too. It exists for the difference between *an x86_64 binary running
under Rosetta* and *an x86_64 binary running on an Intel CPU*: Rosetta translates and then executes
on arm64, so the DSP invariants that depend on the hardware denormal-flush bits are being checked
against the wrong register file. It packages and uploads nothing — the shipped macOS artifact is
still the universal bundle from the `macos` job. Its first step **fails** the job if `uname -m` is
not `x86_64` or `sysctl.proc_translated` is not `0`, so it can never report a green Intel result
from somewhere that is not Intel.

Six further jobs run beside the build jobs, none in a `needs:` chain in either direction, so a
finding in one never skips a binary that is otherwise fine. (`merge-check` is not among them: its
`if:` is the exact complement of every other job's, so it runs only on the same-repo pull-request
event — where it is the only job that runs at all.)

| Job | Run it locally as |
|---|---|
| `docs` | `python3 scripts/check-docs.py --self-test && python3 scripts/check-docs.py` |
| `source-lint` | `python3 scripts/check-portability.py --self-test` then the lint, `python3 scripts/check-realtime.py --self-test` then that lint, then `python3 scripts/check-citations.py --self-test` then `--check --base <rev>` |
| `sanitizers` | ASan+UBSan over both suites, then valgrind memcheck over both suites (the valgrind step sets `ANAMORPH_TESTS_NO_FTZ=1` — see below) |
| `realtime` | `cmake -B build-rtsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C(XX)_COMPILER=clang(++)-<major> -DCMAKE_C(XX)_FLAGS="-fsanitize=realtime -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=realtime`, build `AnamorphTests`, run it with **no `RTSAN_OPTIONS`** (ADR-0029 — `halt_on_error=false` would make it report and pass) |
| `linux-lto-tests` | `cmake -B build-lto -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_STANDALONE=OFF -DCMAKE_C_FLAGS=-flto -DCMAKE_CXX_FLAGS=-flto -DCMAKE_EXE_LINKER_FLAGS=-flto`, build both test targets, run both — the suites against the shipped optimization class (see `CI_CD.md`) |
| `fuzz` | the `AnamorphFuzzState` recipe under §"Opt-in targets" above, verbatim — the CI step adds only `-seed=20260818 -rss_limit_mb=4096 -print_final_stats=1` and an `-artifact_prefix` for the reproducer it uploads on a finding |

**`ANAMORPH_TESTS_NO_FTZ=1` is for valgrind and nothing else.** The DSP suite treats a denormal in
the engine output as a failure, which holds because the audio path runs under
`juce::ScopedNoDenormals` and the CPU flushes denormals to zero *in hardware*. valgrind emulates
floating point and does not honour the FTZ/DAZ bits, so under memcheck denormals survive and the
check fails on a build that is correct on every real CPU — while memcheck itself reports **zero
errors** on the same run. The variable relaxes that half of the check (NaN and Inf stay failures)
and only a literal `1` enables it. Every native job runs without it, so the invariant is gated on
every push on every platform; never set it for a normal run. A run that *is* relaxed says so — the
suite prints a `::warning::` naming the un-asserted half at start-up and repeats it beside the
verdict, so an inherited or stale setting cannot produce a `ALL TESTS PASSED` line that looks like a
full gate.

**`check-citations.py` needs a base revision, and which one you pick changes the answer.** CI compares
against the *previous push*; a local run against `origin/main` can differ, because a document whose
citation *count* differs from the base falls back to ordinal pairing, which only judges base
spellings still present verbatim — and a re-anchor changes the spelling. Run **both** before
concluding the gate is green. If you re-anchor a citation deliberately, declare the pair in
`DELIBERATE_REAIMS` in the **same change set**: the tool cannot tell a repair from a drift, so a fix
landed on its own turns the gate red on the commit that fixed it.

See `CI_CD.md`. Evidence [Verified]: `.github/workflows/build.yml`.

## Failure analysis

| Symptom | Likely cause | Where to look |
|---|---|---|
| A `check` assertion fails | DSP regression | the named test in `tests/dsp_tests.cpp`; compare against the invariant it guards (`docs/policies/DSP_POLICY.md`) |
| A state-test `check` fails | serialization / parameter-surface regression | the named test in `tests/state_tests.cpp`; if the change is INTENTIONAL it needs the compatibility-policy process (ADR + registry update + `--write-snapshot`) |
| pluginval exits < 128 | real validation failure | the pluginval log line; do **not** retry — it's a genuine defect |
| pluginval exits ≥ 128 (crash) | the known X11 host flake | retried automatically; if it still fails after 3 tries, treat as a failure (`scripts/run-pluginval.sh:171-197`, `run_one_pass`) |
| `AnamorphTests`/`AnamorphStateTests` `not found` | not built yet | run `scripts/build.sh` first (`scripts/run-tests.sh:51-73`) |

## Gaps in the automated coverage (known, deliberate)

Things the gates above do **not** do. All are recorded so nobody assumes coverage that
doesn't exist. One entry — automated AU validation — is now **closed** and kept struck through
rather than deleted, because a gap that was real and is now covered is worth being able to find.

- **GUI-lifetime defects have no headless test.** This is a **`TESTING_POLICY` rule-1 exception
  under ADR-0025**, and this entry is the register that ADR names. Its four required disclosures:

  1. *Why no reliable test exists.* The Level-2/3 surface is two console targets
     (`scripts/run-tests.sh`), and `tests/state_tests.cpp:6-11` records that it constructs and
     destroys the editor but never SHOWS it — "no peer, no message loop, no interaction". A defect
     that exists only while a
     **modal child is open and its owner is destroyed** — the 0.9.2 preset drop-down crash,
     **INC-010** — has no object to act on there. Level 4 does open and close the editor, but
     pluginval drives a host we do not control and never opens a menu first.
  2. *What replaced it.* The fix removes the lifetime rather than the symptom: a menu given
     `withParentComponent` is a child component, so `ModalComponentManager`'s
     `ComponentMovementWatcher` cancels it with result 0 on the owner's destruction **or hide**, and
     it has no independent lifetime left to get wrong. The remaining asynchronous window is closed by
     a `SafePointer`. Every other async/modal callback in the editor was audited for the same shape;
     the "Load Preset…" file chooser was the only other one, and it got the same guard. The
     mechanism was re-derived from the pinned JUCE source rather than assumed — see INC-010.
  3. *Where the gap is tracked.* Here, and cross-referenced from `POSTMORTEMS.md` INC-010.
  4. *Whether infrastructure could close it.* **Partly, and concretely.** The *structural* half —
     "is the menu a child of the editor" — becomes assertable the moment the harness instantiates an
     editor, which the sibling plug-in Anabasis already does in its own suite on all three CI
     runners. That is a harness change to prove on the Windows and macOS runners on its own merits,
     not to fold into a crash fix. The *behavioural* half — destroy the owner while the menu is
     modal and then click an item — needs a driven message loop and remains out of reach. Per
     ADR-0025 §5 this entry is revisited when that harness lands, not left standing.

- **Editor interaction defects have no headless test either.** A second
  **`TESTING_POLICY` rule-1 exception under ADR-0025**, covering **all six** v0.9.3 GUI fixes.
  Enumerated in full rather than leaving any to be inferred, because ADR-0025 §3 makes the four
  disclosures mandatory *per invocation* and every one of the six ships without a regression test:

  1. the Multiband add-split preview line stalling under a moving pointer;
  2. the unified pop-up dismissal shield — a dismissing click must close the pop-up and touch nothing
     underneath (Settings drop-downs, the Save Preset text menu, the preset menu);
  3. **pop-up lifetime** — a drop-down must not outlive the editor being hidden, destroyed or sent to
     the background, and cancelling one must neither pull the host window back to the front nor apply
     a half-typed inline edit;
  4. menu width measured from the item text (it clipped *Select All*);
  5. disabled menu items drawn dimmed;
  6. **Tooltips off meaning off** — gated at the source through the virtual `getTipFor`.

  The same four disclosures apply to all six:

  1. *Why no reliable test exists.* They need things the two console targets do not have — a real
     vblank tick plus pointer motion over a settled spectrum for the first, JUCE's modal machinery
     delivering a real mouse-down for the shield, and a rasteriser plus a font for the menu-width and
     disabled-item rendering. The editor is constructed but never shown
     (`tests/state_tests.cpp:6-11`), and neither suite has a pointer or a display.
  2. *What replaced it.* Every root cause was traced to specific lines — our own S2 repaint gate for
     the first, `juce_Component.cpp:2507-2544` and `juce_ModalComponentManager.cpp:81-89` in the
     pinned tree for the shield, and the mismatch between `getIdealPopupMenuItemSize` and
     `drawPopupMenuItem`'s own layout for the width. Where a GUI test would normally be the evidence,
     the shield's riskiest property is instead **proved from the source**: it cannot be raised in
     front of a menu, because `MenuWindow` sets `alwaysOnTop` (`juce_PopupMenu.cpp:365`) and
     `Component::toFront` on a non-always-on-top component inserts behind every always-on-top sibling
     (`juce_Component.cpp:914-922`). Conditions and reasoning in
     `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md`, plus a manual check per platform. That check was
     **performed and signed off by the maintainer on 2026-08-09 for the first two fixes** (the
     add-split preview line and the pop-up dismissal behaviour), discharging this disclosure for
     those. The later three — the shield's interception-only redesign, the two menu-rendering fixes
     and the Tooltips transition — carried a sign-off on the **problem reports and the required
     contract** rather than on a manual test of the implementation. The **visual** half of what was
     then still owed is now discharged: the maintainer **reviewed and approved it on 2026-08-11** —
     the equal-width Widen / Style-Focus row is confirmed **intentional**, the narrower Simple-mode
     Widen control is **accepted**, the current pop-up/menu width behaviour is **accepted**, and the
     remaining visual verification items are **approved** (recorded in
     `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md` §7 and §10). That sign-off covers the **visual/UI**
     items only: the behavioural per-platform checks in the same lists (a dismissing click reaching
     no control, pop-up lifetime across a hidden/closed/backgrounded window, the out-of-process host
     confirmation) and the **installer** checks in the fifth bullet below are **not** covered by it
     and remain owed. None of this touches the Level-5 *audio* audition or
     the compatibility checklist, which are separate and remain open (`HANDOVER.md` §Release Status).
  3. *Where the gap is tracked.* Here, alongside the INC-010 entry above, and referenced from
     **INC-011**'s Prevention field.
  4. *Whether infrastructure could close it.* **Yes, and it is the same infrastructure** the INC-010
     entry names: a harness that instantiates the editor and drives synthetic mouse events. All of
     these become assertable at that point — a hover move must dirty the frame, a click while a
     pop-up is open must reach the shield and no control, and a measured menu must fit its longest
     item. Revisited when that harness lands.

- **Hover occlusion under an open pop-up has no committed test.** A third
  **`TESTING_POLICY` rule-1 exception under ADR-0025**, for the 0.9.4 fix that stops a control
  covered by a drop-down reporting itself hovered (`cursorIsOverOpenPopup()`). The four disclosures,
  written fresh per ADR-0025 §3 rather than deferred to the entry above:

  1. *Why no reliable regression test exists.* It would have to live on the Level-2/3 surface, and
     that surface has no pointer and nothing on screen: `tests/state_tests.cpp:6-11` records that it
     "constructs and destroys the editor but never shows it", and the defect is
     a property of `Component::getMouseXYRelative()` — `getLocalPoint (nullptr,
     Desktop::getMousePositionFloat())` (`juce_Component.cpp:3233-3236`) — so reproducing it needs a
     **real OS cursor** over a **real menu window**, i.e. a display. Level 4 opens the editor but
     pluginval drives a host we do not control and never opens a menu, let alone positions a pointer
     inside one.
  2. *What verification was performed instead.* Not a structural argument — a **measurement**, on
     the running editor. A throw-away harness (not committed; see disclosure 4) linked the real
     `AnamorphAudioProcessor`, instantiated the editor into a window on an `xvfb` display, warped the
     real pointer with `Desktop::setMousePosition`, and read the eased `"hovA"` property the
     LookAndFeel actually paints from. With the pointer at the centre of an open combo list
     (menu `702,279 125×114`), the `Knob` underneath (`705,307 122×131`) read **0.990 before the fix
     and 0.000 after**, three runs each, same geometry and same probe point; the combo that owns the
     list read ~0.02 both ways, so the fix removes the false highlight without touching the true one.
     Dismissing the list with the pointer unmoved returned the knob to **0.990**, so nothing is left
     stuck dark. The preset-menu branch was **mutation-tested**: with the modal-child scan disabled
     and a preset library large enough to make the menu 690 px tall, the A/B control underneath read
     **0.990**; restored, **0.000** — so that branch is load-bearing, not defensive. The un-settle in
     `refreshPopupShield` was mutation-tested the same way (**1.000 → 0.022** with it, **0.990 →
     0.990** without).
  3. *Where the gap is tracked.* Here, and from `CHANGELOG.md` `[0.9.4]` and `HANDOVER.md`. The two
     defects the same measurement found were filed as **KI-024** (the Settings / About / Save-Preset
     overlays occlude identically — measured at hovA **0.990** behind an open Settings panel) and
     **KI-025** (the idle gate could seal on a still-lit control when the pointer left the editor
     inside one frame — measured **0.990**). **Both were fixed on 2026-08-19** and removed from
     `KNOWN_ISSUES.md` per its fixed-item rule. Those fixes ship under this same exception and for
     the same reason, and were verified the same way — the harness above, extended to every overlay,
     plus three mutation runs and a before/after idle-pass measurement.
  4. *Whether infrastructure could close it.* **Yes — and this fix narrows the standing claim above,
     which is worth recording rather than repeating.** The INC-010 and v0.9.3 entries both state that
     the *behavioural* half — a driven message loop with synthetic pointer input — "remains out of
     reach". Measured 2026-08-19, on Linux it is not: `xvfb` is already installed on the CI runner
     for pluginval, and the harness above drove the editor, opened menus and positioned the pointer
     with no repository change at all. What is still owed is making it a **committed** target — a
     CMake target, an `xvfb` wrapper in `run-tests.sh`, and the same thing proven on the Windows and
     macOS runners, where no equivalent virtual display is configured. That is a harness change to
     land on its own merits with its own CI evidence, exactly as the INC-010 entry says, and not to
     fold into a hover fix. Revisited when it lands; at that point this entry and the two above are
     closed together, because the same harness reaches all three.

- ~~**The AU is never validated automatically.**~~ **CLOSED.** The macOS job now runs the full
  pluginval gate against `Anamorph.component` as well as `Anamorph.vst3` — same strictness, both
  modes, ×3 each — so the build Logic Pro and GarageBand load passes the same format-conformance
  gate as the VST3. The registry problem this entry described is solved the way it predicted: an
  install step copies the bundle into `~/Library/Audio/Plug-Ins/Components/` and forces a
  refresh (`killall -9 AudioComponentRegistrar`) before validation, and
  `ANAMORPH_PLUGINVAL_BUNDLE` points the script at *that* copy rather than at the build tree.
  **Ordering:** the AU (and VST3) gates run **after** the packaging step, against
  `dist/Anamorph-macOS/` — the stripped, ad-hoc-signed tree the artifact is uploaded from — so the
  validated bytes are the shipped bytes. The stripped-but-unsigned state this entry warned about
  never arises, because `package` signs *after* it strips and the gate runs after both. The
  installed copy is removed again once the AU gates have reported, so reproducing these steps by
  hand does not leave a plug-in behind in your real `~/Library`. One thing this deliberately did
  **not** do, so the remaining scope is not overstated:
  - It uses **pluginval**, not Apple's `auval` (`auval -v aufx Anmr RTec`, matching the
    `PLUGIN_CODE` / `PLUGIN_MANUFACTURER_CODE` in `CMakeLists.txt:395-396`). pluginval hosts the AU
    through JUCE's `AudioUnitPluginFormat`, which is the same resolution path a JUCE-hosted DAW
    takes and the same test set the other two platforms are held to; `auval` is Apple's own
    conformance tool and tests things pluginval does not. Adding it is a further step, not a
    substitute for this one.
- **No frozen golden-audio reference exists.** `tests/fixtures/` holds a parameter-registry
  snapshot and three legacy session XMLs — metadata, not audio. The DSP suite pins *behavioural
  invariants* (exact nulls, click-freeness, spectral-spur and pitch bounds, cold-path bit-identity)
  rather than a stored waveform, which is deliberate: a golden audio file would freeze bit-exact
  output and collide with the Class-B numerical changes `DSP_POLICY.md` explicitly permits. The
  right tool for "did this change alter the sound" is the **twin dump** — build the engine before
  and after, run the same scenario matrix through both, compare hashes and reported latencies —
  which is what the JUCE 9 migration used across 32 scenarios
  (`worklogs/JUCE9_MIGRATION_v0.8.13.md`). That harness is session-local and not committed, so the
  method must currently be re-created per investigation.

- **No gate ever installs anything.** CI builds the packages and inspects them — the Inno Setup
  exe, the expanded `.pkg` (component identifiers, `customize="allow"`, non-relocatable
  components, payload completeness), the staged Linux tree — but never runs an installation,
  because installing needs elevation and would mutate the runner. Everything that only exists at
  *install* time is therefore manual. **INC-012** is what that gap costs: bundle relocation is a
  property of Installer.app's behaviour, invisible in the archive, and every manual check until
  then had been a first install onto a machine with no prior copy — the one case it cannot affect.
  The checks that remain owed to a human:
  - **macOS `.pkg`, per format (VST3 / AU / app), four cases each:** fresh machine · over an
    existing install · after moving the installed item elsewhere · after deleting it. Each must end
    with the item present at the destination in
    `docs/procedures/PACKAGING.md` §"macOS reinstall behaviour", and a moved copy must be left
    where the user put it.
  - **Linux `install.sh`/`uninstall.sh`:** both modes, plus the failure paths (no `sudo` on
    `PATH`; a `sudo` the user cannot authenticate). *Verified 2026-08-11 on Linux against a stubbed
    payload* — default/`1`/unrecognised answers all install per-user with no elevation, `2` installs
    system-wide via `sudo`, missing `sudo` and denied elevation each exit 1 having installed
    nothing, root skips the prompt, and install→uninstall round-trips in both modes. The
    **replacement transaction** was verified the same way and to the same date, by injecting each
    failure rather than reading the code: failed staging, failed commit, `INT`/`TERM`/`HUP` delivered
    inside the swap window, `SIGKILL` in the window followed by recovery on the next run (including
    a next run that itself fails), staging location on a normal layout and on a `~/.vst3` symlinked
    to a second filesystem, uninstall after an interrupted install, and the coexistence warning —
    each against a control run of the previous script that ends with nothing installed. *Re-verified
    2026-08-20 against a stubbed payload* for the options and the staging guards brought over from
    the sibling product: `--help`, `--user`/`--system` non-interactively, both flags together and an
    unrecognised option each exiting 1, a repeated flag accepted, `--user` under root refused by
    both scripts; a `TERM` inside the copy and inside each of the two rename windows leaving either
    the previous plug-in or the new one in place with no scratch left behind; a symlinked and a
    foreign-owned candidate together stopping the run with both paths named and nothing installed;
    a group-writable candidate refused, left untouched and fallen through; a parked copy restored by
    the next install, reported rather than skipped when its directory is no longer usable, kept by a
    plain uninstall and removed only by `--discard-parked`; and a system-wide install as root
    naming the per-user copy it coexists with. `INT` was delivered as `TERM` because a job
    backgrounded by a non-interactive shell inherits `SIGINT` ignored — a property of the harness,
    not of the script, whose three signal traps are identical. What those
    runs do **not** cover, and a real machine must: that a DAW actually finds
    `~/.vst3/Anamorph.vst3` after a per-user install.
  - **Windows installer:** unchanged in 0.9.3 beyond the two 0.9.2 casing corrections.
  **Could infrastructure close it:** yes, and cheaply for macOS — `installer -pkg … -target /`
  on the runner, then assert the three destinations, re-run after `mv`-ing one away. That is the
  obvious follow-up if this class recurs.

## What cannot be verified headlessly

Audio **sound quality** and GUI/vectorscope **visual appearance** cannot be judged in a headless
sandbox. Load the built `.vst3` in a DAW (e.g. Reaper) on a machine with audio + display. A green
build + pluginval pass is "ready to audition," not final sign-off
(`docs/policies/TESTING_POLICY.md` Level 5).
