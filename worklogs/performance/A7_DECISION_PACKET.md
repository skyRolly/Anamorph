# A7 — maintainer decision packet

**Date:** 2026-08-22 · **State:** **ALL FOUR DECISIONS TAKEN AND IMPLEMENTED.** A7-2B and A7-2T
merged (PR #129, #130); **AVX2 Option B, the cross-architecture position, A7-9 and the A7-2B corner
were approved by the maintainer on 2026-08-22 and are implemented** —
`PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md`, ADR-0031. A7-0 remains blocked.

> **This document is now the record of the decisions, not a request for them.** It is kept because
> the options that were *not* taken (Option C; cross-architecture positions 2 and 3) are the material
> a future revisit needs. One figure below was corrected during implementation: the A7-9 bound —
> see Decision 2 and `PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md` §4c.

Everything below is measured, and every figure names the instrument that produced it. **A7-5E is
closed** — the cross-slice question was confirmed on real Apple Silicon, not
inferred. Ir figures are callgrind with startup subtraction (3.0 s run
minus 1.0 s run, over 2.0 s). Wall-clock is quoted nowhere: on this container the same binary and
workload measured 292 ms in one round and 196 ms in the next, which is why `PERFORMANCE_BUDGET.md`
does not accept it as a datum.

---

# Decision 1 — AVX2: three options

The A7-5E result restructured this. Vectorization and FP contraction were assumed to come as one
package; they are separable, and separating them creates a middle option that did not previously
exist. All three rows below are the *whole* decision, not variations on one.

| | **Option A — no AVX2** | **Option B — Class A** | **Option C — Class B** |
|---|---|---|---|
| **flags** | none (status quo) | `-march=haswell -ffp-contract=off` | `-march=haswell` |
| **performance benefit** | — | **−17.2 %** engine-wide (1704.9 → 1412.2 Ir/sample, 48 kHz/128, `working`) | **−25.8 %** (→ 1264.3 Ir/sample) |
| **numerical impact** | none | **none.** 32/32 twin-dump scenarios identical; **0 mismatches across 180 configurations** | **88.9 % of samples change**; max delta 2.384e-07 (−123.8 dB), RMS 4.949e-08 (−137.5 dB) |
| **class** | — | **A** (bit-exact) | **B** — inside precedent (H3 `tanh` 3.5e-7, allpass 1.19e-7, H11 8.2e-4) but broader than any of them: those bounded one kernel, this moves the whole engine |
| **compatibility impact** | none | **Haswell (2013) / Excavator (2015) becomes a hard floor.** Older x86-64 CPUs get `SIGILL` inside the host — not a diagnosable failure. `COMPATIBILITY_POLICY` states no ISA floor today | identical floor, identical failure mode |
| **effect on GCC/Clang verification** | intact | **intact.** Neither toolchain can contract, so they stay bit-identical to each other — the property that makes `linux-lto-tests` (GCC) and the Clang jobs mutually verifying | **lost.** 32/32 scenarios differ in every pairing once contraction is live, including GCC-haswell vs Clang-haswell |
| **required ADR changes** | none | amend **ADR-0021** (numerics-affecting flags are frozen) to carry two such flags — noting that one of them *pins* the numerics rather than moving them; add an ISA floor to `COMPATIBILITY_POLICY`; resync `DSP_POLICY` and `CMakeLists.txt:82-84` | all of Option B's, **plus** an accepted error bound in `DSP_POLICY` and a replacement for the two-toolchain cross-check |
| **cross-architecture effect** | none | removes the **contraction** component of the arm64/x86-64 difference; does **not** remove the oversampling component | leaves x86-64 contracting too — more alike in mechanism, still not bit-identical |

**DECIDED 2026-08-22: Option B.** Implemented as ADR-0031, scoped to the GCC/Clang x86-64 builds
(Linux x86-64 and the macOS `x86_64` slice via `-Xarch_x86_64`); arm64 and MSVC carry nothing. The
ISA floor and the cross-architecture statement went into `COMPATIBILITY_POLICY` *before* the flag
landed, as the approval required. Option C is not taken and remains a separate decision.

*The recommendation this replaced, kept for the reasoning:*
**Option B, and not yet.** It is two thirds of the benefit at zero numerical cost,
and it keeps the cross-toolchain check that currently catches compiler-level regressions for free.
But it still imposes the ISA floor, and that is a user-visible compatibility contract which must be
written into `COMPATIBILITY_POLICY` *before* a flag lands, not after. **Option C should be a separate
decision taken later, if ever** — it buys 8.6 percentage points more for a whole-engine Class-B
change and the loss of the cross-check, which is a poor trade against Option B rather than against
Option A.

**What would have to be true to prefer Option C:** that the extra 8.6 points matter on real hardware
(unknown — Ir is a poor proxy across an ISA change and this container cannot measure wall-clock), and
that a replacement for the GCC/Clang cross-check exists. Neither is established.

---

# Decision 2 — A7-9: the glide stall

**Approval required:** explicit, recorded maintainer approval for a **Class-B** numerics change under
`DSP_POLICY`. No `ARCHITECTURE_REVIEW_GATE` category is triggered; no Accepted ADR is contradicted;
no parameter, serialization, threading, signal-order or latency change.

| | |
|---|---|
| **What changes** | Three gates move from a **value** test to a **fixpoint** test, mirroring the density gate three lines above Velvet's: `VelvetNoise.cpp:154` (amount), `HaasProcessor.cpp:73`, `ChorusEngine.cpp:97`. No DSP state is mutated; only which path runs. |
| **Benefit, after A7-2B** | `ChorusEngine` **+14,220 Ir/block** (48 kHz/128); `HaasProcessor` **+5,635**; `VelvetNoise` **+4,019** (48 kHz/32) and **+4,032** (192 kHz/32). |
| **Bound** | ~~**4.476e-36** worst case across 44.1–192 kHz, ≈ −707 dBFS, set by `ChorusEngine` at 192 kHz.~~ **CORRECTED during implementation.** 4.476e-36 was the maximum *this investigation's harness* observed, not a stimulus-independent bound; driving ±0.7 noise, Test 41 measures **1.563e-35** (Chorus, 192 kHz), 8.043e-36 (Haas, 48 kHz) and 7.145e-36 (Velvet, 48 kHz) against the pre-fix sources — 3.5× the figure above. What bounds it is `FLT_MIN/k` times the module's wet gain. `PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md` §4c. Unchanged: **0 of 102,400 samples differ on real signal** in all three modules at every rate measured; the residual appears only on digital silence, where the dry term is `+0` and cannot absorb it — and after the fix that output is an **exact zero**, which is stronger than any bound. |
| **Class** | **B, and no Class-A variant exists.** The residual *is* what distinguishes the stalled state from the parked one, so removing it is what "fix" means. Parking only when the residual provably cannot change the output is not expressible as a block-level gate: `x + a*(d-x)` is bit-identical to `x` for a normal `x` and is not when `x` is `+0`. |

**DECIDED 2026-08-22: approved and implemented.** Risk (b) below — that a fixpoint gate must be
proved *live* — is discharged by **Test 41**, which is verified to fail on all four cases against the
pre-A7-9 sources. Risk (c), Chorus's rate-dependent coefficient, is unchanged and still open as an
independent question.

*The recommendation this replaced, kept for the reasoning:*
**Approve, at lower priority than the pre-A7-2B numbers implied.** A7-2B absorbed
**89 %** of Velvet's share (37,951 → 4,032 Ir/block at 192 kHz/32) by making the path a stalled module
is stuck on cheap and rate-independent. Nothing about the change's safety moved; its value did.
**Do `ChorusEngine` first** — largest remaining share, and the only one whose stall threshold scales
with sample rate (`wSmooth = 1/(0.01·sr)`).

**Unresolved risks.** (a) The benefit is now modest relative to a Class-B cost. (b) A fixpoint gate
must be proved *live* by a test that shows the park is reached after a ramp-down — the current suite
cannot distinguish a reachable fast path from an unreachable one, which is how this survived from
Wave 4 to now. (c) Chorus's rate-dependent coefficient deserves a maintainer's eye independently of
the gate change.

**Already done and not waiting on this:** A7-9C corrected the three source comments that claimed the
one-pole "flushes to true zero". That was Class A, required whether or not the optimization is
approved, and is merged.

---

# Decision 3 — cross-architecture bit identity: is it a goal?

A7-5E established that the shipped slices already differ. That raises a question the project has
never had to answer, and answering it is a prerequisite for Decision 1 being coherent.

**The finding, in two parts, now confirmed on the shipping toolchain.** The experiment has been run
on an Apple Silicon runner with Apple Clang and Apple libm — the committed `AnamorphDspDump` built
for both slices, the arm64 one native and the x86_64 one under Rosetta 2, 32 hashes diffed:

| | shipped flags | `-ffp-contract=off` on both |
|---|---|---|
| **macOS, Apple Clang / Apple libm** | **32 of 32 differ** | **24 differ, 8 agree** |
| Linux, GCC 13 / glibc / qemu | 32 of 32 differ | 24 differ, 8 agree |

The 8 that agree are every scenario at oversampling ×1 and only those, by name, on both platforms.
FP contraction is one cause and a flag removes it. The other is not flag-removable: the oversampling
path's polyphase coefficients are derived at runtime through transcendental libm calls, and **Apple's
libm does not agree with itself across Apple's own two architectures**.

**Three positions are available:**

1. **Not a goal.** Record in `COMPATIBILITY_POLICY` that numerics are per-architecture and that the
   twin-dump gate compares builds *within* an architecture only. Costs nothing; makes an existing
   reality explicit. **Recommended.**
2. **A goal for the DSP but not the oversampler.** `-ffp-contract=off` everywhere (Option B's flag,
   decoupled from AVX2) would achieve it for the ×1 path. Cheap, and it is the half the project
   actually authors.
3. **A goal outright.** Requires the oversampling coefficients to stop coming from libm —
   precomputed tables, or a derivation pinned to a portable implementation. Materially larger than a
   flag, and it is JUCE's code, not Anamorph's. **Not recommended without a concrete need.**

**DECIDED 2026-08-22: position 1.** `COMPATIBILITY_POLICY` now states that bit identity holds within
an architecture and build configuration and not across architectures, that the twin-dump gate
compares within one architecture only, and that the difference is **not to be removed** — neither by
disabling contraction on arm64 (position 2, explicitly declined: it is a Class-B change to shipped
arm64 numerics) nor by replacing JUCE's or libm's coefficient generation (position 3).

*The recommendation this replaced, kept for the reasoning:*
**Position 1 now, position 2 if and when Option B lands.** Position 3 should not be
opened without a user-visible reason to — and note what §5c makes concrete: it is not merely large,
it requires replacing JUCE's oversampling coefficient derivation, because the divergence is in
Apple's own libm rather than in anything this project writes.

---

# Decision 4 — A7-2B's +1.0 % corner

A7-2B is faster everywhere except one corner: **44.1 kHz, 32-sample buffer, Density at maximum**,
where it measures **+1.0 %**. The cause is structural rather than incidental — the history image
A7-1 slid is smallest at the lowest rate, while A7-2B's per-tap preamble is rate-independent and paid
once per active tap, so 64 preambles outweigh one small `std::copy`. A no-wrap fast path already
holds it at 1.0 %; without it the same point measures +3.0 %.

| density at 44.1 kHz / 32 | delta |
|---|---:|
| 0.0 (0 taps) | −0.9 % |
| 0.5 (the default) | −0.2 % |
| 1.0 (64 taps) | **+1.0 %** |

**DECIDED 2026-08-22: accepted. A7-2B is not reverted.**

*The recommendation this replaced, kept for the reasoning:*
**Accept it.** It is the lowest supported rate × smallest block × maximum density,
it is 1 %, and the default-density point at the same rate and block is *negative*. Against it stand
−12.2 % at 48 kHz/32 and −37.2 % at 192 kHz/32, and the removal of the residual term's sample-rate
dependence entirely (the 48→192 kHz penalty at a 32-sample block fell from 39.8 % to 0.04 %). The
alternative — keeping a rate-dependent hybrid that chooses between slide and ring-read — reintroduces
the cross-block state A7-2B deleted, for 1 % in one corner.

**If the maintainer disagrees**, the revert is clean: A7-2B is one self-contained change to
`VelvetNoise`, and Test 40 guards the path either way.

---

# Roadmap

| item | status | needs |
|---|---|---|
| **A7-0** — bench on a named machine, fill the `PERFORMANCE_BUDGET.md` rows | **BLOCKED.** This container is a masked-CPU shared machine; the wall-clock instability noted at the top is the standing evidence. RISK-002 open, rows unpopulated. | a named benchmark machine |
| **A7-1** — Velvet history slide | **DONE** (v0.9.5), **superseded by A7-2B** | — |
| **A7-2T** — path-equivalence oracle (Test 40) | **DONE** (PR #129). Spent as designed: A7-2B landed against it, and it is now the standing guard that the gather equals the per-sample loop. | — |
| **A7-2B** — residual per-block term | **DONE** (PR #130). Class A on both committed instruments; −12.2 % at 48 kHz/32, −37.2 % at 192 kHz/32; rate dependence removed. **Corner accepted 2026-08-22.** | — |
| **A7-5E** — cross-slice experiment | **CLOSED.** Confirmed by execution on the shipping toolchain (Apple Silicon, Apple Clang, Apple libm): **32/32 differ at shipped flags; 24 differ with contraction off, the 8 oversampling-×1 scenarios agreeing.** Identical to the Linux result in counts, split and scenario names. `macos-crossslice` in CI, reporting-only. | — |
| **A7-5 / W5-D** — AVX2 | **DONE — Option B** (ADR-0031, 2026-08-22): `-march=haswell -ffp-contract=off` on the GCC/Clang x86-64 builds only. −17.2 % engine-wide, 32/32 twin-dump identical, 0 FMA emitted, GCC/Clang cross-check intact. Option C not taken. | — |
| **A7-9** — the glide stall | **DONE** (2026-08-22, explicit Class-B approval recorded). Three gates moved from a value test to a fixpoint test; no DSP state mutated. **Test 41** is the liveness gate and is proven to fail on all four cases against the pre-A7-9 sources. Bound corrected — Decision 2. | — |
| **A7-9C** — correct the three false comments | **DONE** (PR #130). Class A, verified behaviour-neutral against the twin dump. | — |
| A7-4 · A7-8 | maintainer decisions, unchanged | — |
| A7-3 · A7-6 · A7-7 | not scheduled, unchanged | — |

## The order the work was actually done in (2026-08-22)

The recommended order below was followed, with one reordering forced by the approval itself: the
compatibility documentation had to land **before** the flag, so Decision 3 and the ISA floor were
written first and Decision 1's flag last.

1. **Decision 3, position 1** — `COMPATIBILITY_POLICY` cross-architecture statement. **DONE.**
2. **The ISA floor** — `COMPATIBILITY_POLICY` + ADR-0031, before any flag. **DONE.**
3. **Decision 1, Option B** — the flag pair, scoped to GCC/Clang x86-64. **DONE.**
4. **Decision 2** — A7-9, with Test 41 as the liveness precondition. **DONE.**
5. **Decision 4** — A7-2B's corner accepted. **DONE.**
6. **A7-0** — still blocked, and still the only item that needs hardware this container does not
   have. Everything above was decided without it; the `PERFORMANCE_BUDGET.md` rows still cannot be
   filled honestly on a shared runner.
