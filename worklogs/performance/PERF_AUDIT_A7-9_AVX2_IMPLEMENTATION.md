# A7 — implementation of the approved decisions (A7-9, AVX2 Option B, cross-arch policy, A7-2B corner)

**Date:** 2026-08-22 · **Branch:** `claude/anamorph-ci-workflow-8iu7yk` · **Predecessor:**
`worklogs/performance/A7_DECISION_PACKET.md` (the four decisions), `PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md`
(the measurements), `PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md` (the A7-9 investigation).

The investigation phase is closed. This worklog records what was implemented against the maintainer's
approvals, what each item was verified with, and **one correction to a figure the investigation
recorded** (§4c). Nothing outside the four approved items was changed.

> **STATUS — PARTLY SUPERSEDED (2026-08-30).** This worklog is the record of the **2026-08-22**
> round and is preserved as written. Every statement in it about **MSVC / Windows carrying no ISA
> flag** was true of that round and is **no longer current**: **ADR-0032** (Accepted, 2026-08-30)
> adopted `/arch:AVX2` for the MSVC x64 target, the Windows twin-dump instrument was built in the
> R-round, and `windows-avx2-ab` is now a **blocking** CI gate. The superseded passages are marked
> in place below (§2, §7) rather than rewritten. For current state read **ADR-0032**,
> `docs/policies/COMPATIBILITY_POLICY.md`, and `PERF_AUDIT_PLATFORM_COVERAGE.md`
> §"Current state after ADR-0032 — the eight facts". Nothing else in
> this document — the A7-9 fixpoint work, the §4c figure correction, the A7-2B corner acceptance —
> is affected.

| approved item | status |
|---|---|
| **AVX2 Option B (Class A)** — `-march=haswell -ffp-contract=off` | **DONE**, ADR-0031 |
| **Cross-architecture numerical policy** — bit identity is per-architecture | **DONE**, `COMPATIBILITY_POLICY` |
| **A7-9** — fixpoint gates in Velvet / Haas / Chorus | **DONE**, Test 41 |
| **A7-2B corner** — record acceptance of +1.0 % | **DONE**, §5 |

---

## 1. Order of work, and why compatibility documentation went first

The approval said the compatibility documentation must be updated **before** the flag is enabled, so
that is the order the commits are in: `COMPATIBILITY_POLICY` gains the ISA floor and the
cross-architecture statement, ADR-0031 records the decision and amends ADR-0021, and only then does
`CMakeLists.txt` carry `-march=haswell`. The floor is a user-visible contract with a `SIGILL` failure
mode; a flag that lands first and is documented afterwards is a period in which the product's
supported hardware is whatever the build files happen to say.

## 2. AVX2 Option B — what landed

`AnamorphHardening` (the ADR-0021 INTERFACE target) now carries, **for GCC/Clang x86-64 only**
*(historical — as of this round; the target has carried an MSVC x64 branch as well since ADR-0032)*:

```
-march=haswell -ffp-contract=off
```

- **Linux x86-64** and a thin `x86_64` macOS build: directly.
- **macOS universal**: `-Xarch_x86_64` on each flag. A universal build runs the driver once per
  architecture from one compile command; an unqualified `-march=haswell` is handed to the arm64
  invocation as well and fails the build.
- **arm64**: nothing. AArch64 has `FMLA` in its base ISA and the shipped arm64 slice contracts today;
  `-ffp-contract=off` there would be a Class-B change to shipped arm64 numerics for no user-visible
  gain (ADR-0031 option 4).
- **MSVC**: nothing. `/arch:AVX2` is not the equivalent pair — MSVC controls contraction separately —
  and no twin-dump instrument runs on Windows, so the Class-A property could not be *demonstrated*
  there, only assumed (ADR-0031 option 5). Recorded as an open item in §7.
  > **SUPERSEDED by ADR-0032 (2026-08-30).** The reasoning above is preserved as the ADR-0031-round
  > record; its conclusion no longer holds. The blocker was the missing instrument, not the flag: the
  > R-round built the Windows twin dump, it measured **0 of 32 scenarios moved** by `/arch:AVX2` on
  > toolset 14.51.36231 across two runs, and the Class-A property is now *demonstrated* on Windows
  > rather than assumed. MSVC x64 carries `/arch:AVX2` today, at the default non-contracting
  > `/fp:precise` — the "MSVC controls contraction separately" observation is what makes that safe,
  > not what blocks it. `/fp:contract` remains **off**; it moves 32 of 32 scenarios.

Both are **compile** options. Under LTO the codegen happens at link time, but both toolchains stream
the target CPU/feature set per function into the IR, so the compile-time `-march` governs what is
emitted.

### 2a. Verification, on this tree

| check | result |
|---|---|
| committed twin dump, baseline (`8fe20be`) vs the same sources + the flag pair | **0 of 32 scenarios differ** — Class A confirmed |
| FMA instructions in the built `AnamorphDspDump` | **0** (`objdump -d \| grep -cE 'vfmadd\|vfmsub'`) |
| AVX2 actually emitted | **780** `%ymm` operands — the flag is doing work, not sitting inert |
| DSP self-tests under the flag pair | 214 checks, 0 failures |

The 0-FMA count is the load-bearing one. It is what makes `-march=haswell` a pure ISA change here:
the instruction exists in the target now, and the optimiser is forbidden from using it to fuse a
multiply and an add the source wrote separately.

### 2b. Two CI consequences, both handled

**Rosetta 2 does not translate AVX2 by default.** The `macos` job runs the `x86_64` slice under
Rosetta for its Intel coverage, and `macos-crossslice` runs the `x86_64` dump the same way. Both now
set `ROSETTA_ADVERTISE_AVX=1` and **probe** with a three-line AVX2 program built by the same
compiler before running anything; a probe that faults degrades to a `::warning::`, the same shape as
the existing "Rosetta unavailable" path. This costs no gate — the blocking Intel coverage is the
`macos-intel` job on native `macos-15-intel` hardware, and every Mac that runs macOS 15 is Haswell+
by Apple's own hardware requirements.

**A7-5E's "shipped flags" arm changed meaning slightly, and its finding did not.** Part 1 of the
cross-slice comparison now builds the x86_64 slice at the ADR-0031 pair. Because the frozen baseline
had no FMA instruction at all, contraction was already impossible on x86-64 and the flag pins what
was true by accident; part 2 therefore still changes only the arm64 side, and the recorded result
(32/32 differ at shipped flags; 24 differ with contraction off, the 8 oversampling-×1 scenarios
agreeing) stands.

## 3. Cross-architecture numerics — the policy statement

`COMPATIBILITY_POLICY` now says, as a first-class contract:

> Numerical bit identity is guaranteed within the same architecture and build configuration, and not
> across different CPU architectures. The twin-dump gate compares builds within one architecture only.

with the A7-5E table, the two mechanisms (FP contraction, which a flag reaches; libm-derived
oversampling coefficients, which it does not), and an explicit instruction **not** to remove the
difference — neither by disabling contraction on arm64 nor by replacing JUCE's or libm's coefficient
generation.

## 4. A7-9 — the fixpoint gates

### 4a. The change

Three gates move from a **value** test to a **fixpoint** test. No DSP state is mutated and no state
evolution is altered; only which path runs.

| module | gate | before | after |
|---|---|---|---|
| `VelvetNoise` | parked path | `! (currentAmount > 0) && ! (targetAmount > 0)` | `amountParked` |
| `VelvetNoise` | H5 gather path | `(currentAmount > 0 \|\| targetAmount > 0)` | `! amountParked` |
| `HaasProcessor` | parked path | `! (\|amount\| > 0) && ! (\|currentAmount\| > 0)` | target 0 **and** `aNext == currentAmount` |
| `ChorusEngine` | idle path | `! (\|currentWet\| > 0) && ! (\|wetTarget\| > 0)` | target 0 **and** `wNext == currentWet` |

`amountParked` is computed **once** in `VelvetNoise` and used in both directions, so the gather gate
and the parked gate remain exact complements: no state can be eligible for neither or for both. Each
gate keeps the pre-A7-9 condition as a second disjunct, for the one input the fixpoint test does not
subsume — a NaN target with the current value already 0, where the old gate parked (identity, NaN
never enters the state) and an equality test would not. **The gates can therefore only ever admit
more than they did before; nothing that used to park stops parking.**

The fixpoint test is decisive for a whole block rather than one sample because the targets only move
between blocks, so the map is idempotent and the fixpoint absorbing. This is the same test
`VelvetNoise` has always used for its **density** glide, three lines above the gate that was wrong.

### 4b. Test 41 — the gate, proven to fire

`testA79ParkedPathsReachableAfterStall` drives two instances of each module through identical input.
`S` is driven the way a user drives it — engaged, then turned down and left to stall. `P` sees the
same input with Amount at 0 from `prepare()`, so it is genuinely parked. All three modules record the
**input** in their delay lines, not their own output, so the two rings hold identical history and any
difference between the two outputs is the residual and nothing else.

Three claims, three checks, per module:

| claim | pre-A7-9 | post-A7-9 |
|---|---|---|
| real signal → **exactly** equal | passes | passes |
| silence → within the `FLT_MIN/k` stall ceiling | passes | passes |
| silence → **exactly 0** (the fast path is REACHED) | **FAILS** | passes |

Verified to fire: built against the pre-A7-9 sources with the identical test file, the third check
fails on all four cases and the other two pass, which is the shape the change predicts.

| case | residual removed (pre-A7-9) | `FLT_MIN/k` ceiling | real-signal diff |
|---|---:|---:|---:|
| `VelvetNoise` 48 kHz | 7.145162e-36 | 7.837e-36 | 0 |
| `HaasProcessor` 48 kHz | 8.042941e-36 | 1.175e-35 | 0 |
| `ChorusEngine` 48 kHz | 3.911637e-36 | 5.642e-36 | 0 |
| `ChorusEngine` 192 kHz | **1.563218e-35** | 2.257e-35 | 0 |

`ChorusEngine` is tested at both ends of the supported range because its smoothing coefficient is the
only rate-dependent one of the three (`wSmooth = 1/(0.01·workingRate)`), so the stall value — and
with it the residual — scales with the sample rate. 192 kHz is where the worst case lives, so it is
asserted rather than extrapolated.

### 4c. CORRECTION: 4.476e-36 is not a stimulus-independent bound

The approval carried the investigation's figure forward as a requirement — *"preserve the measured
numerical bound: worst case ≤ 4.476e-36"*. **That figure does not survive a louder stimulus, and the
requirement as literally worded cannot be met by any implementation, because the quantity it bounds
belongs to the code being REMOVED rather than to the fix.**

What 4.476e-36 was: the largest residual the investigation's own harness observed, at
`ChorusEngine` / 192 kHz (`PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md` §17). Driving ±0.7 noise —
the amplitude the rest of this suite uses — Test 41 measures **7.145e-36 (Velvet, 48 kHz)**,
**8.043e-36 (Haas, 48 kHz)** and **1.563e-35 (Chorus, 192 kHz)** against the pre-fix sources. The
worst case is therefore **3.5× the recorded figure**, and it was understated because the residual is
`a_stall × (wet term)` and the wet term scales with the signal that was in the delay lines when the
silence began.

What actually bounds it, and is stimulus-independent: `a_stall ≤ FLT_MIN/k`, since the glide stalls
where the decrement `k·a` first flushes. That is **7.837e-36** (Velvet, k = 0.0015), **1.175e-35**
(Haas, k = 0.001) and **2.257e-35** (Chorus at 192 kHz, k = 1/1920) — times the module's wet gain,
which for `VelvetNoise` measured 1.30× its input peak because the normalised tap sum can exceed unity.
Test 41 asserts against `2 × FLT_MIN/k` and prints the measured value, so a residual orders of
magnitude larger would be caught while the headroom for the wet gain stays explicit.

**None of this changes the decision, and the direction of the change is not in question.** The
residual is what the fix REMOVES; post-A7-9 the silence output is **exactly zero**, which is stronger
than any bound. Even at the corrected worst case, 1.563e-35 is ≈ −696 dBFS, ~25 orders of magnitude
below the smallest accepted Class-B precedent (H4 dry-align, 2.4e-10) and ~28 below a 24-bit LSB. It
is flagged here because a bound that is quoted in a policy-adjacent document should be the one that
is true, not the one that was convenient.

> **Arithmetic corrected 2026-08-30.** The second figure read **~34** until this date. It is a ratio,
> and the ratio is `2^-23 / 1.563e-35 = 7.63e27`, i.e. **~28** orders — 34.8 is
> `log10(1 / 1.563e-35)`, the absolute exponent, which is not a comparison against anything. The
> first figure is unaffected and was always right: `2.4e-10 / 1.563e-35 = 1.54e25`, ~25 orders. The
> wrong number never propagated: the only other document that makes the 24-bit comparison at all is
> `CHANGELOG.md` `[0.9.5]`, which already says twenty-eight. This is a fix to the source record, not
> to anything that quoted it.

### 4d. What A7-9 did NOT do

- No DSP state is snapped, frozen or mutated. The stalled `currentAmount`/`currentWet` is left
  exactly where it is, so a later re-engage resumes from the same value the pre-A7-9 code would have
  resumed from — the re-engage path is bit-identical.
- The envelope/gate followers, the delay-line writes, the write indices, the iterated phase
  accumulation and the depth/delay glides all keep running in every parked path, unchanged (the W3-9
  reasoning that rejected freezing them).
- Nothing outside the three gate expressions and their comments was touched in `src/`.

### 4e. Effect on the committed twin dump

**0 of 32 scenarios differ.** That is not evidence the change is Class A — it is evidence the dump
does not reach the state: `tests/dsp_dump.cpp` holds `algoAmount` at 0.7 for every scenario and never
ramps it to 0, so no glide ever stalls. This is the same blind spot that let the defect survive from
Wave 4, and Test 41 is what closes it.

## 5. A7-2B's +1.0 % corner — accepted

Recorded per Decision 4: the corner at **44.1 kHz / 32-sample buffer / Density at maximum**, where
A7-2B measures **+1.0 %**, is **accepted**. A7-2B is not reverted.

| density at 44.1 kHz / 32 | delta |
|---|---:|
| 0.0 (0 taps) | −0.9 % |
| 0.5 (the default) | −0.2 % |
| 1.0 (64 taps) | **+1.0 %** |

Against it: −12.2 % at 48 kHz/32, −37.2 % at 192 kHz/32, and the removal of the residual term's
sample-rate dependence entirely (the 48→192 kHz penalty at a 32-sample block fell from 39.8 % to
0.04 %). The alternative — a rate-dependent hybrid choosing between slide and ring-read —
reintroduces the cross-block state A7-2B deleted, for 1 % in one corner.

## 6. Validation

All of the following ran locally on this tree, on Linux x86-64 with the ADR-0031 flags live.

| gate | result |
|---|---|
| DSP self-tests | **214 checks, 0 failures** (202 before this round; Test 41 adds 12) |
| state-compatibility suite | **920 checks, 0 failures** |
| twin dump — AVX2 half (baseline `8fe20be` vs the same sources + the flag pair) | **0 of 32 scenarios differ** — Class A confirmed |
| twin dump — A7-9 half (flags vs flags + A7-9) | 0 of 32 differ, and §4e says why that proves nothing |
| Test 41 against the pre-A7-9 sources | **fires on all four cases** (the exact-zero check); the other two pass, which is the shape the change predicts |
| FMA census, shipped VST3 `.so` | **0** `vfmadd`/`vfmsub`/`vfnmadd`/`vfnmsub` |
| AVX2 census, shipped VST3 `.so` | **73,726** `%ymm` operands — the flag is doing work |
| Linux ABI floor (`check-linux-abi.py`, on the linked `.so`) | within the declared floor — `GLIBC_2.38`, `GLIBCXX_3.4.31`, `CXXABI_1.3.9` |
| **pluginval, strictness 10, VST3** | **deterministic ×3 and randomise ×3, all green**, under xvfb against the built bundle |
| `check-docs` | 110 files clean |
| `check-portability` | 52 files scanned, 0 violations |
| `check-realtime` | 44 files scanned, 0 violations |
| `check-citations` (all three bases) | 388 anchors intact |
| RTSan, ASan/UBSan, valgrind, Windows, macOS ×3 | CI |

### 6a. Two gate-adjacent things this round had to touch

**The Clang warning baseline grew by three sites, deliberately.** Each fixpoint gate is an exact
`==` between two floats, which is `-Wfloat-equal`, and which is exactly what the gate asks — an
epsilon there would be the defect rather than the fix. `VelvetNoise` has carried the same idiom for
its density glide since long before the warning gate existed (the two pre-existing entries on that
file). Baseline is now **17 sites / 9 entries**; `CI_CD.md` §"The Clang warning baseline" carries the
reasoning next to the table, so the next reader does not have to reconstruct it from a count. The
GCC gate is unaffected — `-Wfloat-equal` is not one of the five GCC-only flags it covers.

**`preflight.sh` found a stale local build tree, not a defect.** Its DSP half was reporting 178
checks from a binary configured with `ANAMORPH_BUILD_TESTS=OFF`, so `cmake --build` was failing with
`unknown target` behind a pipe that swallowed the exit code. Worth recording because it is the
`scripts/build.sh` failure mode the repository already documents — a gate that "passed" by testing a
stale binary.

## 7. Open items this round deliberately did not take

1. **MSVC ISA baseline.** `/arch:AVX2` remains unset. It needs its own measurement, and the
   instrument that would produce it — a twin dump on Windows — does not exist. ADR-0031 option 5.
   > **CLOSED — SUPERSEDED by ADR-0032 (2026-08-30).** Both halves of this item are now false. The
   > instrument exists (`windows-avx2-ab`, a blocking gate); it produced the measurement (0/32
   > scenarios moved, two runs, toolset 14.51.36231); and `/arch:AVX2` is **set** on the MSVC x64
   > target. Kept here as the record of what was open at the close of the ADR-0031 round.

2. **Option C (`-march=haswell` with contraction live).** 8.6 percentage points more, for a
   whole-engine Class-B change and the loss of the GCC/Clang cross-check. A separate decision, if ever.
3. **A7-0.** Still blocked on a named benchmark machine; `PERFORMANCE_BUDGET.md` rows unfilled.
4. **The 4.476e-36 figure** appears in three documents (§4c). It is corrected where it is quoted as a
   bound; the investigation's own §17 table is left as the historical measurement it is, with a
   forward reference.
