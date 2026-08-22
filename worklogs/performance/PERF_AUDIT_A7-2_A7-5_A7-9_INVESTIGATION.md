# A7-2 · A7-5 / W5-D · A7-9 — engineering investigation

**Date:** 2026-08-22 · **Base:** `main` at `100b98c` (v0.9.5, PR #127 merged) · **Product code
changed: none.**

This round answers three questions that do not need the physical benchmark machine A7-0 is blocked
on. Nothing here is implemented. Everything measured was measured on this container, with the
method `docs/architecture/PERFORMANCE_BUDGET.md` fixes: callgrind Ir with **startup subtraction**
(every steady-state figure is a 3.0 s run minus a 1.0 s run, over 2.0 s, so process start, dynamic
linking and `prepare()` cancel exactly). Wall-clock appears once, labelled, and is **not** a datum.

Three prototypes and six probes were built in the session scratchpad and are **not** part of the
repository:

| harness | what it is for |
|---|---|
| `vnB` | variant B built **faithfully** — `linHist`, `linHistSlide`, the slide and the image build all deleted |
| `vnB-t1`, `vnB-t2` | variant B carrying one seeded implementation trap each |
| `vnseed` | the shipped engine carrying a seeded uniform tap-delay error |
| `oracle2` | the two oracles side by side — Test 39's, and the proposed path-equivalence one |
| `tapprobe`, `runstats` | what the tap set actually is, and which gather shapes a schedule executes |
| `ftzprobe`, `ftzcost`, `ftzdelta` | A7-9: the stall, what it costs, and what a fix would change |

---

# Part I — A7-2: the residual per-block term

## 1. The design verifies against the current architecture

Four properties were re-derived from the source rather than carried over from the previous round's
write-up.

**Ring sizing is always adequate, and B needs *less* history than the code it replaces.**
`prepare()` builds the ring as `nextPow2(ceil(0.045*sr) + 5)` (`src/dsp/VelvetNoise.cpp:15-16`)
while the window is `decorrSamps = max(8, round(0.045*sr))` (`:26`). For every rate at or above
~178 Hz, `round(W) <= ceil(W)`, so the ring is at least `decorrSamps + 5`. Independently, tap
positions are clamped to `[1, decorrSamps-1]` (`:31`). B's deepest read is therefore
`writePos - (decorrSamps - 1)`; the ring walk it replaces reads `writePos - decorrSamps`, one
sample deeper (`:180-181`). **B tightens the requirement rather than loosening it, at every rate.**

**`linHist[0]` is written every block and read by nothing.** The tap source pointer is
`linHist.data() + (decorrSamps - pos[t])` (`:190`) and the minimum index it can reach is
`decorrSamps - pos[t] >= 1`, because `pos[t] <= decorrSamps - 1`. The image's oldest entry is dead.
B deletes it along with the rest of the image.

**The ring may be read in bulk before the per-sample loop, at any block length.** At sample `i` the
per-sample loop reads ring position `writePos₀ + i - k`. That is one of *this* block's own writes
only if `(i - k) + size <= i - 1`, i.e. `size <= k - 1` — impossible, since `k <= decorrSamps - 1 <
size`. So **no ring read can ever observe a write made by the same block**, which is exactly the
licence B needs to hoist all of them. This holds even when `numSamples` exceeds the ring size (at
44.1 kHz the ring is 2048 and a 4096-sample buffer laps it twice).

**Bit-exactness holds for a stronger reason than "0 mismatches".** The split is along `i`, not
along `t`. Every `accum[i]` still receives exactly one add per tap, in ascending-`t` order, from the
same zero-filled `+0`. Nothing in the accumulation order, the starting zero, or any value changes —
B reads the same floats from where they already are.

## 2. The prototype was rebuilt faithfully, and re-measured

The previous round's prototype still carried `linHist`; only its tap loop had been replaced. It has
been rebuilt as the plan actually describes — image, slide flag and allocation all removed — and
re-measured against the shipped v0.9.5 engine.

| evidence | result |
|---|---|
| 180 configurations (4 rates × 5 block sizes × 9 scenarios), FNV-1a over every output sample of both channels | **0 mismatches** |
| absolute path-equivalence oracle (§4), 4 rates × 4 block schedules | **16 / 16 hashes equal to the shipped engine** |
| both under ASan + UBSan + `local-bounds` + `pointer-overflow` | **0 diagnostics** |

**Cost, callgrind Ir with startup subtraction, scenario `working`:**

| configuration | shipped Ir/block | variant B Ir/block | delta | shipped Ir/sample | B Ir/sample |
|---|---:|---:|---:|---:|---:|
| 48 kHz / 32 | 65,141.4 | 57,758.9 | **−11.3 %** | 2,035.7 | 1,805.0 |
| 48 kHz / 128 | 220,223.6 | 212,995.2 | −3.3 % | 1,720.5 | 1,664.0 |
| 48 kHz / 512 | 842,022.4 | 835,344.9 | −0.8 % | 1,644.6 | 1,631.5 |
| 192 kHz / 32 | 91,065.7 | 57,784.2 | **−36.5 %** | 2,845.8 | 1,805.8 |
| 192 kHz / 128 | 246,145.8 | 212,823.9 | −13.5 % | 1,923.0 | 1,662.7 |

Fitting `Ir/block = fixed + perSample × N` through the measured points isolates the term A7-2 is
about:

| | fixed per-block term, 48 kHz | fixed per-block term, 192 kHz |
|---|---:|---:|
| shipped (A7-1) | 12,957 Ir | 39,373 Ir |
| variant B | **5,546 Ir** | **6,104 Ir** |

The headline is in the last row, not the percentages. The shipped fixed term **scales with the
sample rate**, because it is proportional to `decorrSamps`; B's does not. At block 32 the shipped
engine costs 2,035.7 Ir/sample at 48 kHz and 2,845.8 at 192 kHz — a **39.8 %** rate penalty. Variant
B costs 1,805.0 and 1,805.8 — a **0.04 %** rate penalty. A7-2B does not shrink the residual term so
much as **remove its sample-rate dependence**, which is the property that made small buffers at high
rates the worst block in the first place.

## 3. Test 39's oracle is not sufficient — and the first version of this section overstated it

Test 39 (`tests/dsp_tests.cpp:3054`, `testVelvetLinearImageInvariance`) compares **the build under
test against itself** at different block lengths. There is no reference implementation anywhere in
it. Its **oracle** therefore discriminates exactly one class of defect: corruption whose extent is
measured from the block start. A defect that is a pure function of the sample stream — the same
wrong answer at every block length — is invisible to that comparison by construction.

The most likely A7-2B defect is exactly that shape: a **uniform one-sample error in the tap delay**,
both the ring run and the `midBlk` run derived from `writePos - k - 1` instead of `writePos - k`. It
produces a valid-but-wrong FIR.

**CORRECTION.** The first version of this section reported that seeding that error leaves Test 39
green at all four sample rates. That was measured on a session-local harness which reproduced Test
39's *comparison* but not its *schedule* — the harness held amount and density constant with the
transport always playing, so it never crossed between paths — and the result was reported as Test
39's. Re-measured against the committed test, with the seed applied at
`src/dsp/VelvetNoise.cpp:190` and the suite rebuilt each time:

| build under test | Test 39's two bit-identity checks | first difference |
|---|---|---|
| shipped v0.9.5 | pass ×4 rates | — |
| **seeded, Test 39 as committed** | **FAIL ×4 rates** | block 215 — the transport stop |
| **seeded, transport-stop events removed** | **FAIL ×4 rates** | block 247 — the moving density |
| **seeded, every path crossing removed** | **pass ×4 rates** | — |
| **seeded, Test 40 (§4)** | **FAIL, 20 of 20 checks** | sample 3 of block 0, every size and rate |
| variant B (faithful) | pass ×4 rates | — |

So the committed Test 39 **does** catch this seed — through its **schedule**, not its oracle. Its
detection depends on the run crossing from the gather to the per-sample loop, which its schedule
happens to do twice; with both crossings removed it passes on a build that is provably wrong. That
makes it a guard whose competence over this defect class is incidental rather than designed, and one
that a future schedule edit could silently remove. The oracle-level gap is real and is what §4
closes; the claim that the committed test was blind was not, and is withdrawn.

Note also which paths the schedule can and cannot cross: the park at block 20 does **not** cross,
because of A7-9 — with a 0 target the amount one-pole stalls just above zero under FTZ, so the
gather keeps its eligibility and the Wave-5 parked path is never reached. The two crossings are the
transport stop and the moving density, and nothing else.

## 4. The oracle that closes it — reachable today, with no product change

`prepare()` sizes `accum` from `maxBlockSize`, and the gather gate requires
`numSamples <= (int) accum.size()` (`src/dsp/VelvetNoise.cpp:148`). **An instance prepared for a
smaller block therefore runs the per-sample loop on the very same audio** — with an identical ring,
tap set, envelopes and coefficients, because `prepare()` derives every one of those from `sr` and
`seed` alone; `linHist`, `accum` and `midBlk` are the *only* block-sized state.

So the absolute oracle is two instances of the shipped class, no test hook, no friend declaration,
no product change:

* instance A: `prepare(sr, N)`, fed `N`-sample blocks → **gather path**
* instance B: `prepare(sr, N-1)`, fed the same `N`-sample blocks → **per-sample loop**
* assert bit-identical output.

Verified live in both directions: **44 cells** (4 rates × 11 block sizes from 1 to 4096) clean on the
shipped engine, and failing at all four rates on the seeded build. Cost is one extra engine instance
and a second pass over the same stimulus.

## 5. What Test 39's schedule does and does not execute under B

Instrumenting variant B with per-shape counters and replaying Test 39's own schedules answers the
coverage question by census rather than by inspection. `wrapped` = the ring portion crossed the ring
origin and emitted 2–3 runs; `split` = the tap also read a `midBlk` tail.

| schedule (density 0.5) | 44.1 kHz | 48 kHz | 96 kHz | 192 kHz |
|---|---|---|---|---|
| 512 — wrapped / split | 12.8 % / 50.0 % | 6.8 % / 46.9 % | 4.8 % / 25.0 % | 2.8 % / 12.5 % |
| 32 — wrapped / split | 1.5 % / 3.1 % | 0.8 % / 3.1 % | 0.4 % / 3.1 % | 0.2 % / 3.1 % |
| mixed {32,128,64,256,32} — wrapped / split | 5.0 % / 10.0 % | 2.5 % / 10.0 % | 1.2 % / 5.6 % | 0.6 % / 3.8 % |

**Both new branch shapes are genuinely executed** by the schedule Test 39 already runs — the wrap
fires between 0.2 % and 12.8 % of tap-gathers and the `midBlk` tail between 3.1 % and 50 %. Test 39's
weakness is its *oracle*, not its reach: it executes the wrap and still cannot tell a wrong answer
from a right one.

Three regimes are outside its reach entirely, and were enumerated rather than assumed:

* **`k == numSamples` is unreachable at every one of its four rates.** Enumerating the actual tap
  set: `pos` spans [3, 982] at 44.1 kHz, [4, 1069] at 48 kHz, [7, 2139] at 96 kHz and [15, 4278] at
  192 kHz, and none of `{32, 64, 128, 256, 512}` is a member at any rate. The boundary between "the
  ring run covers the whole block" and "a `midBlk` tail begins" is never landed on exactly.
* **Density never exceeds 0.9 in the repository, and at the default 0.5 exactly `32 of 64` taps are
  active — the *shallow* half.** The deepest active tap is ~½·`decorrSamps`; taps beyond it are
  never gathered by any committed evidence. Deep taps are where the wrap is likeliest.
* **`numSamples > decorrSamps` is qualitatively different and unreachable.** Test 39's largest block
  is 512 against `decorrSamps` of 1985–8640. In that regime **every** tap splits and, at 44.1/48/96
  kHz, **none** wraps — 100 % split / 0 % wrapped at block 4096. It is an ordinary host
  configuration (an offline bounce at a 2048 or 4096 buffer), not a corner case.

## 6. Two implementation traps — one caught by the existing gate, one silent

Both are hazards of the *obvious* way to write the split. Neither is present in the plan text or in
the prototype; both were seeded deliberately to find out what would notice.

| trap | what it is | what the sanitizer gate does |
|---|---|---|
| **1** | ring run length taken as `k` rather than `min(k, numSamples)` | **ASan heap-buffer-overflow**, named to the exact line, on the normal path at any block shorter than the deepest tap |
| **2** | `const float* src = midBlk.data() - k;` as the tail's base pointer | **nothing.** Clean run, correct output, under `-fsanitize=address,undefined` *plus* `local-bounds` *plus* `pointer-overflow` |

Trap 1 is severe but self-announcing — the repository's `sanitizers` job already runs the DSP suite,
and Test 39 already runs at block 32, so it would be caught before review. **Trap 2 is the dangerous
one**: forming a pointer before the start of an object is undefined behaviour that happens to work
on every mainstream target, produces bit-correct output, and is invisible to every tool the project
runs. It must be excluded **by construction** — index the tail as `midBlk[i - k]` with `i >= k` — and
the plan must say so, because no gate will say it later.

## 7. A7-2 — recommendation, risk, required tests, gating

**Recommendation: implement variant B, and do not implement variant A.** The design verifies against
the current architecture on every axis checked; the bit-exactness is provable rather than merely
measured; the prototype is bit-identical to the shipped engine over 180 configurations *and* against
an absolute oracle; and it removes the sample-rate dependence of the residual per-block term rather
than amortising it, which is what variant A was rejected for.

**Risk assessment.**

| risk | severity | mitigation |
|---|---|---|
| uniform tap-delay error surviving the suite | **high** — the most likely defect, and today's tests are provably blind to it | the §4 path-equivalence oracle, committed as a test, is provably live against it |
| trap 2 (out-of-range base pointer) shipping silently | **medium** | forbidden by construction in the plan; no tool catches it |
| trap 1 (unguarded run length) | low | ASan already catches it |
| `numSamples > decorrSamps` regime untested | medium | add one long-block configuration to the sweep and to Test 39 |
| deep taps / density > 0.5 never exercised | medium | add one density-1.0 configuration |
| single-platform evidence | medium | see A7-5 §11 — cross-architecture bit-identity is an open question for the *whole* product, not for B |

**Required tests before implementation.**

1. **New, committed: the path-equivalence oracle of §4** — gather vs per-sample loop, bit-identical,
   at 44.1 / 48 / 96 / 192 kHz. This is the one item that closes the flagship gap; it must be proved
   live (it fails on a seeded uniform delay) at the time it is written, per `TESTING_POLICY` rule 4.
2. **Extend Test 39** with one block length greater than `decorrSamps` (e.g. 44.1 kHz at 4096) and
   one density-1.0 pass, so the all-taps-split regime and the deep half of the tap set are executed.
3. Regenerate the `AnamorphDspDump` twin diff and the 180-configuration sweep.
4. Both suites under ASan + UBSan with `local-bounds` **and** `pointer-overflow`.
5. Full `scripts/preflight.sh`.

**Gating: A7-0 is no longer the only gate.** A7-2B should now be gated on **both**:

* **A7-0** — unchanged. The benefit is a real-machine claim and this container cannot make it.
* **A7-2T** (new) — the §4 path-equivalence oracle committed, live, and green **before** the gather
  is rewritten. Landing B first and the oracle afterwards would mean the change with the highest
  chance of a silent, uniform arithmetic error goes in while the only test that can see one does not
  yet exist. The order is the gate.

Nothing in A7-2B triggers an `ARCHITECTURE_REVIEW_GATE` category and it conflicts with no Accepted
ADR: no parameter ID, no serialization schema, no threading model, no DSP signal order, no reported
latency, and the output is bit-identical.

---

# Part II — A7-5 / W5-D: AVX2 architecture decision preparation

## 8. Current SIMD and build assumptions — measured, not assumed

* **There is no explicit SIMD anywhere in `src/`.** No intrinsics, no `juce::dsp::SIMDRegister`, no
  `#pragma omp simd`. Every vector instruction in the shipped binary is the auto-vectoriser's.
* **There is no architecture flag anywhere in the build.** `-march`, `-mtune`, `-mavx`, `-msse`,
  `-mfma`, `-ffast-math`, `-ffp-contract`, `/arch:` and `/fp:` appear in **zero** of
  `CMakeLists.txt`, `.github/workflows/*.yml` and `scripts/*.sh`. The only mention is the record of
  their deliberate absence: *"`-O3` / LTO / `-ffast-math` — numerics-affecting flags are frozen"*
  (`CMakeLists.txt:82-84`, citing ADR-0021 and `DSP_POLICY`).
* The x86-64 baseline therefore has **SSE2 and no FMA**.
* macOS ships a **universal `arm64;x86_64`** binary with `CMAKE_OSX_DEPLOYMENT_TARGET=10.13`
  (`.github/workflows/build.yml:1700-1701`); the native-Intel job builds a thin `x86_64`
  (`:2358-2359`).

**Consequence: enabling AVX2 is not a performance flag with a numeric side effect. It is a change to
a frozen ADR-0021 property, and therefore an ADR-level decision — a `CLAUDE.md` hard stop ("conflict
with an Accepted ADR"), not an optimization round.**

## 9. What enabling AVX2 actually does — the decisive experiment

`AnamorphDspDump` (32 scenarios: 4 algorithms × 4 oversampling factors × M/S off/on) was built four
ways on this container and cross-compared. This is an A-vs-B comparison of builds, exactly the use
the harness documents; no stored golden is involved.

| comparison | scenarios differing |
|---|---|
| GCC 13 baseline **vs** Clang 18 baseline (frozen flags) | **0 / 32 — identical** |
| GCC baseline vs GCC `-march=haswell` | **32 / 32** |
| Clang baseline vs Clang `-march=haswell` | **32 / 32** |
| GCC `-march=haswell` vs Clang `-march=haswell` | **32 / 32** |

The mechanism is the point. Both toolchains default to permitting FP contraction, but **the frozen
x86-64 baseline has no FMA instruction to contract into**, so the permissive default is inert and
the two compilers agree bit for bit. Enabling AVX2 *activates* that default — and because GCC and
Clang make different contraction choices, it simultaneously **breaks agreement between the two
toolchains the project builds with**. The freeze is currently doing more work than its own comment
claims: it is what makes the Linux GCC-LTO job and the Clang jobs mutually verifying.

**Magnitude of the audio change** (192,000 samples, scenario `working`, 48 kHz / 128):

| | |
|---|---|
| differing samples | 170,631 of 192,000 — **88.9 %** |
| max abs delta | **2.384e-07** (−123.8 dB below peak) |
| RMS of the difference | 4.949e-08 (−137.5 dB below peak) |

That is a **Class B** change of the same order as the already-accepted H3 `tanh` rational
(3.5e-7) — i.e. within the programme's precedent, but not remotely bit-exact.

**Benefit.** Whole-engine cost at `-march=haswell`, scenario `working`, 48 kHz / 128:
**1,704.9 → 1,264.3 Ir/sample (−25.8 %)**. Ir is a poor proxy across an ISA change — a wider
instruction retires more work — so this was cross-checked against wall-clock. *Indicative only; this
container is a masked-CPU shared machine and wall-clock is not a datum here:*

| scenario | baseline | `-march=haswell` | |
|---|---:|---:|---|
| defaults (transparent) | 82 ms | 71 ms | −13.4 % |
| working | 292 ms | 227 ms | −22.3 % |
| oversampling ×8 | 680 ms | 574 ms | −15.6 % |
| Dimension-D | 311 ms | 240 ms | −22.8 % |
| Chorus | 299 ms | 232 ms | −22.4 % |
| Haas | 282 ms | 222 ms | −21.3 % |

Ir and wall-clock agree in direction and closely in magnitude (−25.8 % vs −22.3 % on `working`), so
the benefit is real and roughly a fifth of the engine — **larger than every optimization A7-1
through A7-9 combined**, and won without touching a line of DSP.

## 10. Cross-platform implications

* **Baseline choice is a compatibility contract, not a tuning knob.** AVX2 requires Haswell (2013) /
  Excavator (2015). Setting it as a hard floor turns every older x86-64 machine from "slower" into
  "will not run", and the failure mode is `SIGILL` inside the host — not a diagnostic the plugin can
  produce. `COMPATIBILITY_POLICY` owns that contract; today it says nothing about an ISA floor,
  because there has never been one to state.
* **Windows/MSVC does not spell it the same way.** `/arch:AVX2` is the nearest equivalent and its
  contraction behaviour is governed by `/fp:`, a third axis the project has also frozen. A
  cross-platform "enable AVX2" is three different decisions with three different numeric outcomes,
  not one flag.
* **It costs the project its two-toolchain cross-check** (§9), which the `linux-lto-tests` GCC job
  and the Clang jobs currently provide for free.

## 11. Apple Silicon — and the question this opens for the whole product

AArch64's base ISA includes `FMLA`/`FMADD` **unconditionally**; there is no flag to enable and no
baseline without it. Under the same permissive contraction defaults, **the compiler may already be
contracting on the arm64 slice of the shipped universal binary**, with no flag anywhere in the
build.

If so, the two slices of the universal binary are already not bit-identical to each other — and
**nothing in the repository has ever checked.** `TESTING_POLICY:24-29` runs the suites three times on
macOS precisely because arm64, Rosetta-translated x86_64 and native Intel are not interchangeable,
but the reason it gives is the hardware denormal-flush bits (MXCSR vs FPCR), not contraction; and
the suites assert behaviour, not bit-identity between architectures.

**This reframes A7-5.** The question is not only "should we accept a Class-B change on x86_64". It
is "is cross-architecture bit-identity a property this product has, or one it only assumes" — and
that is answerable cheaply, before any ADR is written:

> **Proposed pre-ADR experiment.** On the existing macOS job, build `AnamorphDspDump` twice — once
> `-DCMAKE_OSX_ARCHITECTURES=arm64`, once `x86_64` — run both on the same runner (the x86_64 one
> under Rosetta, as the job already does for the suites) and diff the 32 hashes. It is an A-vs-B
> comparison of two builds, which is the only thing the harness is allowed to answer, and it needs
> no stored baseline. One job step, no new tooling.

The result changes the decision materially. If the slices already differ, "the product is bit-exact
across architectures" is a belief rather than a fact, and enabling AVX2 on x86_64 costs a property
the product did not have. If they are identical, the freeze is protecting something real and the bar
for breaking it is correspondingly higher.

## 12. Would runtime dispatch be preferable?

Runtime dispatch (`__builtin_cpu_supports`, or JUCE's own SIMD abstraction, selecting an AVX2 kernel
at load time) removes the compatibility cliff of §10 but **not** the numeric one — and it makes the
numerics *worse-defined*, because the same binary then produces different bits on different CPUs.
Every downstream property the project has built on bit-identity — the twin-dump bump gate, the
GCC/Clang cross-check, `AnamorphDspDump`'s whole reason to exist — becomes conditional on the
machine that ran it. It also multiplies the test matrix by the number of dispatched paths, on a
project whose gate already runs pluginval in two modes × three passes on five platform
configurations.

**Assessment: runtime dispatch is the *wrong first step*, but not because it is hard.** It is a way
to keep the compatibility floor while accepting the numeric change; the numeric change is the part
that needs the decision. A hand-written, explicitly-FMA-free AVX2 kernel for the LR4 crossover — the
original W5-D idea — is the only variant that could be Class A, and it is strictly more work than
the flag while capturing a fraction of the −22 %.

## 13. Decisions required before any A7-5 implementation

1. **Is a Class-B change of 2.4e-07 acceptable for a ~20 % engine-wide gain?** The programme's
   standing bar is Class A; the accepted Class-B precedents are H3 (3.5e-7), the allpass (1.19e-7),
   H11 (8.2e-4) and H4 (2.4e-10). This sits inside that range — but every one of those was a
   *targeted* change with a stated bound on one kernel, and this one changes 88.9 % of output samples
   across the whole engine.
2. **What is the minimum x86-64 ISA the product supports?** This must be written into
   `COMPATIBILITY_POLICY` before, not after — it is a user-visible contract with a `SIGILL` failure
   mode.
3. **Is losing the GCC/Clang bit-agreement cross-check acceptable**, and if not, what replaces it?
4. **Run the §11 experiment first.** The universal binary's two slices should be diffed before the
   ADR is drafted; the answer changes what the ADR is deciding.
5. **Does ADR-0021's numerics freeze get amended, superseded, or scoped?** A new ADR must say which,
   explicitly, and `DSP_POLICY` and `CMakeLists.txt:82-84` must be resynced with it.

**Recommendation: do not implement. Do not draft the ADR yet.** Run the §11 experiment, then draft
it — with decisions 1–3 stated as the decision, not as background.

---

# Part III — A7-9: the Amount glide never reaches zero

## 14. Root cause, and the closed form

The glide is `a += k * (target - a)`. With `target == 0` this is `a -= k*a`. Under the audio
thread's `ScopedNoDenormals` (`src/PluginProcessor.cpp:119`), **the decrement underflows before the
state does**: once `k*a` falls below `FLT_MIN` it flushes to exactly zero, and `a` stops moving. The
fixed point is absorbing — the next decrement is exactly `0.0`, forever.

The stall value is not a magic constant. It is **`≈ FLT_MIN / k`**, approached from below, with the
last few ULPs depending on the trajectory:

| module | `k` | measured stall (48 kHz) | `FLT_MIN / k` | next decrement |
|---|---:|---:|---:|---:|
| `VelvetNoise` | 0.0015 — a constant | 7.83374862e-36 | 7.83662901e-36 | **0.0** |
| `HaasProcessor` | 0.001 — a constant | 1.17487956e-35 | 1.17549435e-35 | **0.0** |
| `ChorusEngine` | **`1/(0.01·sr)`** — 1/480 only at 48 kHz | 5.63638313e-36 | 5.64237288e-36 | — |

(`docs/DOCUMENTATION_COVERAGE.md:254-262` records `7.82561114e-36` for VelvetNoise from a different
ramp; both sit in the same sub-0.2 % band below `FLT_MIN/k`. The closed form is the durable
statement, not any single landing point.)

The recurrence itself contains no `sr`, so the stall depends on the sample rate exactly as far as
`k` does — **and that differs by module.** `VelvetNoise` and `HaasProcessor` glide at compile-time
constants (`aSmooth = 0.0015f` and `0.001f`), so their stall is genuinely rate-independent.
`ChorusEngine` is the exception: `wSmooth = 1.0f / std::max (1.0, 0.01 * workingRate)`
(`src/dsp/ChorusEngine.cpp:70`), so `k = 1/480` holds **only at 48 kHz** and the stall
`FLT_MIN / k = FLT_MIN · 0.01 · sr` scales linearly with the rate. Measured across the range, with
the ramp allowed to converge at each rate (convergence is per-sample, so a fixed block count is not
a fixed ramp):

| module | 44.1 kHz | 48 kHz | 96 kHz | 192 kHz | |
|---|---:|---:|---:|---:|---|
| `VelvetNoise` | 7.83374862e-36 | 7.83374862e-36 | 7.83374862e-36 | 7.83374862e-36 | flat |
| `HaasProcessor` | 1.17487956e-35 | 1.17487956e-35 | 1.17487956e-35 | 1.17487956e-35 | flat |
| `ChorusEngine` | 5.17668692e-36 | 5.63638313e-36 | 1.12767137e-35 | 2.25593495e-35 | **×4.00 over the range** |

All three are start-independent to within the band above.

## 15. It is not one module. The same false premise is load-bearing in three

This is the material finding of Part III, and it is new. Three shipped fast paths justify themselves
with the *same sentence*, and it is wrong in all three:

| site | the premise, verbatim | the gate |
|---|---|---|
| `src/dsp/VelvetNoise.cpp:233-235` | *"the amount glide settled at exactly 0 … its one-pole flushes to true zero under the block's `ScopedNoDenormals`"* | value test |
| `src/dsp/HaasProcessor.cpp:51-53` | *"the wet glide settled at EXACTLY 0 (the audio thread runs under `ScopedNoDenormals`, so the asymptotic amount tail flushes to true zero)"* | value test (`:60`) |
| `src/dsp/ChorusEngine.cpp:73-74` | *"the wet glide settled at exactly 0 (it flushes to true 0 under the block's `ScopedNoDenormals`)"* | value test (`:83`) |

The one gate in the same file that is **not** affected is Velvet's density gate, and the difference
is instructive: it tests the **fixpoint** (`dNext == currentDensity`), so a stalled glide satisfies
it. The three amount/wet gates test the **value** (`> 0.0f`), so a stalled glide defeats them.

**Consequence: all three Wave-4/Wave-5 parked fast paths are unreachable after a user turns the
control down.** They are reached only from a fresh `prepare()` with the control at its 0 default —
which is the state each was measured in. `prepare()` assigns `currentAmount = targetAmount`, so a
host-initiated re-prepare clears the stall; nothing else does.

For `VelvetNoise` the consequence is worse than "the cheap path is skipped": a stalled
`currentAmount > 0.0f` **satisfies the H5 gather gate** (`:148`), so the engine sits on the *most*
expensive path rather than the cheapest.

## 16. What the missed park costs

Callgrind Ir with startup subtraction, per module in isolation. "parked" is the control at 0 from a
fresh `prepare()`; "stalled" is engaged, then ramped to 0 — what a user leaves behind.

| module | configuration | parked Ir/block | stalled Ir/block | penalty |
|---|---|---:|---:|---:|
| `VelvetNoise` | 48 kHz / 32 | 1,600 | 13,629 | **+752 %** |
| `VelvetNoise` | 48 kHz / 128 | 6,113 | 25,421 | +316 % |
| `VelvetNoise` | 192 kHz / 32 | 1,600 | 39,549 | **+2,372 %** |
| `HaasProcessor` | 48 kHz / 128 | 2,778 | 8,413 | +203 % |
| `ChorusEngine` | 48 kHz / 128 | 3,841 | 18,061 | +370 % |

In whole-engine terms, Velvet's missed park alone is ~12,000 Ir/block at 48 kHz / 32 against a
65,141 Ir/block engine — **~18 %** — and ~38,000 Ir/block at 192 kHz / 32. **This is larger than
everything A7-1 recovered and larger than everything A7-2B would recover.** The Wave-4 and Wave-5
performance claims are, today, true only for a session in which the control was never touched.

## 17. Audible impact: none, and this is measured

Two instances, identically constructed and identically driven through the same ramp; afterwards one
has its glide state forced to exact zero — which is precisely what any fix would produce. Every
later difference is therefore exactly the residual a fix would remove.

| module | stimulus | differing samples (48 kHz) | worst abs delta (48 kHz) |
|---|---|---:|---:|
| `VelvetNoise` | noise | **0** / 102,400 | 0 |
| `HaasProcessor` | noise | **0** / 102,400 | 0 |
| `ChorusEngine` | noise | **0** / 102,400 | 0 |
| `VelvetNoise` | digital silence | 1,876 / 102,400 | 4.470e-37 |
| `HaasProcessor` | digital silence | 51,195 / 102,400 | 1.643e-36 |
| `ChorusEngine` | digital silence | 51,759 / 102,400 | 1.081e-36 |

The residual inherits §14's rate behaviour, so **the bound is not a single number**. Re-measured at
each rate with a converged ramp:

| module, digital silence | 44.1 kHz | 48 kHz | 96 kHz | 192 kHz |
|---|---:|---:|---:|---:|
| `VelvetNoise` | 4.443e-37 | 4.470e-37 | 3.736e-37 | 4.627e-37 |
| `HaasProcessor` | 1.605e-36 | 1.643e-36 | 1.639e-36 | 1.644e-36 |
| `ChorusEngine` | 1.003e-36 | 1.081e-36 | 2.224e-36 | **4.476e-36** |

Velvet and Haas are flat; Chorus scales with `sr` and **overtakes Haas above 48 kHz**. The bound over
the supported range is therefore set by Chorus at the highest rate, not by Haas at 48 kHz.

On real signal the residual is **exactly invisible** — the stalled amount multiplies into a product
that itself flushes to zero. The differences appear only on digital silence, where the dry term is
`+0` and cannot absorb the residual, and only while the delay lines still hold pre-silence audio.
Worst case across all three modules and all four rates: **4.476e-36 — `ChorusEngine` at 192 kHz,
about −707 dBFS.** (At 48 kHz alone it is 1.643e-36, set by `HaasProcessor`; quoting that as the
bound would understate the high-rate case by 2.7×.)

## 18. Classification: Class B, and no Class-A fix exists

Every candidate repair changes bits, and always by exactly the residual in §17:

* **snap the glide to zero** — changes the sample at which the amount reaches zero;
* **change the gate from a value test to a fixpoint test** (matching the density gate) — parks
  without touching the state, but the parked path skips the tap accumulation on the strength of
  "the multiplier is exactly `+0`", which a stalled amount makes false;
* **park only when the residual provably cannot change the output** — not expressible as a
  block-level gate, because it depends on the per-sample dry value: `x + a*(d-x)` is bit-identical to
  `x` for a normal `x`, and is not when `x` is `+0`.

The residual **is** the thing that distinguishes the two states, so removing it is what "fix" means.
**Class B, bounded at 4.476e-36** across the supported sample-rate range. For scale, that is ~26
orders of magnitude tighter than the
tightest accepted Class-B precedent (H4 dry-align, 2.4e-10) and ~32 tighter than the loosest (H11
chorus LFO, 8.2e-4).

## 19. A7-9 — recommendation

**Recommend implementation, subject to the explicit maintainer approval a Class-B change requires.
Do not implement without it.** The reasoning:

* the benefit is the largest single item on the A7 roadmap (§16), and it is not a new optimization —
  it *restores three already-shipped optimizations to the state in which they were measured*;
* the cost is bounded at 4.476e-36 across the rate range, inaudible by construction, and orders of
  magnitude inside precedent;
* it is also a documentation-integrity defect: three source comments state a premise that is false,
  and one of them (`VelvetNoise`) is already recorded as such in
  `docs/DOCUMENTATION_COVERAGE.md:254-262` while the other two are not recorded anywhere.

**If approval is withheld, the comments must still be corrected** — `DOCUMENTATION_LIFECYCLE_POLICY`
does not let a known-false justification stand in the source, and this document is the drift report
that policy requires before an edit. That correction is Class A and costs nothing.

**Preferred repair, if approved:** change the three gates from a **value** test to a **fixpoint**
test, mirroring the density gate that already sits three lines above Velvet's — one shape, applied
three times, in the file that already demonstrates it. Snapping the state is the alternative and is
worse: it mutates DSP state rather than choosing a path, and every downstream consumer of
`currentAmount` would need re-auditing.

**Required before it lands:** a test that proves the park is *reached* after a ramp-down (a liveness
assertion, per `TESTING_POLICY` rule 4 — the current suite cannot tell a reachable fast path from an
unreachable one, which is how this survived); the `AnamorphDspDump` twin diff with the ≤1.6e-36
bound stated; and the three source comments rewritten to say what is actually true.

---

# Part IV — roadmap and maintainer decision summary

## 20. Roadmap after this round

| item | status after this round |
|---|---|
| **A7-0** — bench on a named machine, fill the `PERFORMANCE_BUDGET.md` rows | **open, unchanged.** Not attempted. RISK-002 unchanged. Rows unpopulated. |
| **A7-1** — VelvetNoise history slide | DONE (v0.9.5, PR #127). |
| **A7-2** — residual per-block term | **investigated twice; not implemented.** Variant A rejected on measurement; **variant B recommended**, prototype re-verified faithfully. A7-2T is now in the tree, so the remaining gate is **A7-0**. |
| **A7-2T** — commit the path-equivalence oracle | **DONE.** `testVelvetGatherEqualsPerSampleLoop` (Test 40): 24 checks at 4 rates × blocks 32/128/512/4096 plus a density-1.0 pass; suite 178 → 202. Proven live on a seeded one-sample tap-delay error (20 of 20 fail, at sample 3 of block 0). No product change. |
| **A7-5 / W5-D** — multiband LR4 SIMD / AVX2 | **investigated; not implemented; ADR not drafted.** Blocked on the §11 cross-slice experiment and on decisions 1–5. |
| **A7-5E** (new) — diff the universal binary's two slices | **open.** One CI step, no new tooling, answers a question the product has never asked. |
| **A7-9** — the amount glide stalls above zero | **investigated; widened from one module to three; recommended, pending explicit approval.** Class B, bound 4.476e-36 across the rate range. |
| A7-4 · A7-8 | maintainer decisions, unchanged. |
| A7-3 · A7-6 · A7-7 | not scheduled, unchanged. |

## 21. Maintainer decision summary

| # | decision | recommendation | why it cannot be decided by a green build |
|---|---|---|---|
| **1** | Approve **A7-2B** (delete `linHist`, gather from the ring) | **Yes**, after A7-2T | Bit-identical, so nothing red would ever indicate a fault; the flagship defect class is one the current suite provably cannot see |
| **2** | ~~Approve **A7-2T** as a prerequisite~~ — **done**, Test 40 is in the tree | — | Landed first, as the ordering required |
| **3** | Approve the **A7-5E** cross-slice experiment | **Yes** — cheap, and it should precede any ADR | It asks whether a property the product assumes is real |
| **4** | Approve **AVX2 / A7-5** in principle | **Not yet** | ~20 % engine-wide, but a Class-B change to 88.9 % of samples, an ISA compatibility floor, and the loss of the GCC/Clang cross-check — an ADR-0021 amendment, a `CLAUDE.md` hard stop |
| **5** | Approve **A7-9** as a Class-B change bounded at 4.476e-36 | **Yes, with explicit approval recorded** | Largest single item on the roadmap; changes output bits, so it cannot ride a green build |
| **6** | If 5 is declined, approve the **comment correction** alone | **Yes** | Three source comments assert a false premise; Class A, zero cost |

**Nothing in this round changed product code, DSP code, `PERFORMANCE_BUDGET.md` rows, or RISK-002.**
