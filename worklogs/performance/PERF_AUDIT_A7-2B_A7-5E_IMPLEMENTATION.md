# A7-2B implemented · A7-5E answered · A7-9 re-verified

**Date:** 2026-08-22 · **Base:** `main` at `2c40a86` (PR #129 merged; Test 40 in the tree)

Method unchanged: callgrind Ir with **startup subtraction** (3.0 s run minus 1.0 s run, over 2.0 s,
so process start, dynamic linking and `prepare()` cancel). **Wall-clock is not a datum in this
round and none is quoted as one** — see §9.

---

# Part I — A7-5E: are the two shipped slices already different?

## 1. The question, and what could actually be executed here

The A7-5 investigation proposed diffing `AnamorphDspDump` built for `arm64` against the same built
for `x86_64` on a macOS runner. **This container is x86-64 Linux with no aarch64 emulator**, so that
run was not possible. What *was* possible answers the same question in two measured halves:

1. **Does the arm64 slice contract?** — decided by cross-compiling the shipped DSP sources to
   aarch64 and reading the generated code.
2. **Does contraction change the output?** — decided by the committed twin dump, on x86-64, with
   the ISA held fixed and contraction as the only variable.

## 2. Half one: the arm64 codegen of the shipped DSP

Clang 18 (`--target=aarch64-linux-gnu`), the project's own flags, every DSP translation unit:

| module | FMA instructions, aarch64 | FMA instructions, x86-64 baseline |
|---|---:|---:|
| `AnamorphEngine` | 72 | 0 |
| `LoudnessMatch` | 77 | 0 |
| `MultibandWidth` | 62 | 0 |
| `ChorusEngine` | 26 | 0 |
| `MonoMaker` | 22 | 0 |
| `VelvetNoise` | 22 | 0 |
| `SoloMonitor` | 18 | 0 |
| `HaasProcessor` | 9 | 0 |
| **total** | **308** | **0** |

Every module contracts on aarch64 and none does on the frozen x86-64 baseline. The mechanism is not
subtle: AArch64's base ISA includes `FMLA`/`FMADD` unconditionally, so `a*b+c` compiles to one
instruction with no flag at all — confirmed on a two-line reduction, which emits a single `fmadd`.
The x86-64 baseline has no FMA instruction, so the same permissive contraction default is inert.

*Caveat, stated because it bounds the claim.* This is **codegen inspection, not execution**, and the
cross-compile used the host's C++ headers with two shims (`gnu/stubs-32.h`, and `bits/wordsize.h`
forced to 64 because the host header selects that only under `__x86_64__`). The shims affect which
headers parse, not which FP instructions the arithmetic compiles to. It remains an inference about
the shipped arm64 slice rather than a measurement of it, and §5 keeps the confirming CI step open.

## 3. Half two: contraction alone, ISA held fixed

Four `AnamorphDspDump` builds, 32 scenarios each:

| comparison | FMA in binary | scenarios differing |
|---|---:|---:|
| baseline vs `-march=haswell -ffp-contract=off` | 0 vs 0 | **0 / 32 — IDENTICAL** |
| `-march=haswell` contract off vs contract fast | 0 vs 707 | **32 / 32** |
| baseline vs `-march=haswell` (contract fast) | 0 vs 707 | 32 / 32 |

**This corrects the A7-5 investigation's attribution.** That round reported the AVX2 output change
and named contraction as the mechanism, but had not separated contraction from vectorization. They
are now separated, and the split is total: **AVX2 vectorization alone changes nothing** — 0/32 on
the twin dump and **0 mismatches across 180 configurations** on the scenario twin — while
contraction alone changes **every** scenario. (An earlier attempt using `-mfma` alone was vacuous
and is recorded so nobody repeats it: GCC emitted **0** FMA instructions at that flag, so the
comparison tested nothing and reported IDENTICAL for the wrong reason.)

## 4. The A7-5E answer

* **Do the shipped slices already produce different DSP results?** **Yes — confirmed by executing
  the committed harness on both architectures (§5): 32 of 32 scenarios differ.**
* **Is the numerics contract already architecture-dependent?** **Yes**, and nothing in the
  repository states it or checks it. ADR-0021's freeze protects the x86-64 side only: it withholds
  the flags that would *give* x86-64 an FMA, while arm64 has one unconditionally and no flag is
  passed to stop it being used.
* **Does this affect the AVX2 decision?** Substantially — see §6.

## 5. CONFIRMED BY EXECUTION (2026-08-22, later the same round)

The inference above has been replaced by a run. `qemu-user-static` and `g++-aarch64-linux-gnu` were
installed, and **the committed harness itself** — `tests/dsp_dump.cpp`, unmodified, with the eight
DSP translation units and the four JUCE modules it needs — was cross-built for `aarch64-linux-gnu`
and, from the same sources and the same flags, for `x86_64`. Both were then **executed** (the
aarch64 one under `qemu-aarch64-static`) and their 32 scenario hashes diffed. Both pass
`--self-check`, so both instruments are live: 32 scenarios, all repeatable and all distinct.

| comparison | result |
|---|---|
| **aarch64 (default flags) vs x86-64 (default flags)** — the shipped configuration | **32 / 32 scenarios differ** |
| aarch64 default vs aarch64 `-ffp-contract=off` | **32 / 32 differ** |
| x86-64 default vs x86-64 `-ffp-contract=off` | **identical** |
| aarch64 `-ffp-contract=off` vs x86-64 `-ffp-contract=off` | **24 / 32 still differ** |

FMA instructions in the DSP objects alone (excluding static libc/libm): **529 on aarch64 at default
flags, 0 with `-ffp-contract=off`, 0 on x86-64 either way.**

**The answer to A7-5E is confirmed: the two architecture slices already produce different numerical
results.** Two independent mechanisms produce that, and separating them is the part that matters:

1. **FP contraction**, which is ISA-intrinsic — aarch64 has `FMLA` in its base ISA and contracts by
   default; x86-64 without an FMA-bearing `-march` has nothing to contract into. Removable with a
   flag.
2. **Something else, which a flag does not remove** — 24 of 32 scenarios still differ with
   contraction disabled on both sides.

## 5a. What the residual 24 are, and it is a clean split

| oversampling factor | scenarios agreeing across architectures with contraction off |
|---|---|
| ×1 | **8 of 8 — all agree** |
| ×2, ×4, ×8 | **0 of 24 — all differ** |

Every scenario that differs runs oversampling; every scenario at ×1 agrees. **The residual
cross-architecture difference is confined to the oversampling path**, whose polyphase filter
coefficients are derived at runtime through transcendental libm calls, and libm is a different
implementation on each architecture. Anamorph's own DSP, at ×1, is architecture-portable once
contraction is off — all eight scenarios agree bit for bit.

**This corrects a claim made earlier in this same document.** §8 originally suggested that setting
`-ffp-contract=off` "would make the two shipped macOS slices bit-identical to each other for the
first time". That is **false**: it would do so only with oversampling off. Full cross-architecture
identity additionally requires the oversampling coefficients to stop coming from libm — precomputing
them, or deriving them in a way that does not vary by platform. That is a much larger question than
a compiler flag and is **not** proposed here.

## 5c. CONFIRMED ON macOS — the platform-exact run (2026-08-22)

The Linux result has now been reproduced on the hardware and toolchain that
actually ship it. `macos-crossslice` (a job of its own — see below) runs the committed
`AnamorphDspDump` on an Apple Silicon runner: built for `arm64` and for `x86_64` from the same
sources and flags, the arm64 slice executed natively and the x86_64 slice under Rosetta 2, both
passing `--self-check`, and the 32 scenario hashes diffed.

| comparison, **Apple Clang / Apple libm / real arm64** | result |
|---|---|
| **arm64 vs x86_64, shipped flags** | **32 of 32 scenarios differ** |
| **arm64 vs x86_64, `-ffp-contract=off` on both** | **24 differ, 8 agree** |
| the 8 that agree | `chorus-os1-lr/ms`, `dimd-os1-lr/ms`, `haas-os1-lr/ms`, `velvet-os1-lr/ms` — **every scenario at oversampling ×1, and only those** |

**This is the Linux pattern exactly** — same counts, same split, same scenario names. The two
mechanisms and their proportions are properties of the architectures and of the libm boundary, not
of GCC, of glibc or of qemu:

* FP contraction accounts for the difference in the 8 oversampling-×1 scenarios, and a flag removes
  it;
* the oversampling path accounts for the other 24, and a flag does not — its polyphase coefficients
  are derived at runtime through transcendental libm calls, and **Apple's libm does not agree with
  itself across its own two architectures** any more than glibc's does.

**A7-5E is closed.** The shipped universal binary's two slices produce different DSP output today,
confirmed by execution on the shipping toolchain. The Linux round's inference was correct in
mechanism, proportion and detail.

*(One reporting artifact, recorded because the first run's numbers were quoted from it: the step's
"scenarios agree" count included a blank header line, so that run printed 1 and 9 where the true
figures are 0 and 8. The counting is fixed; the 8 named scenarios were never affected.)*

## 5b. What this confirmation does and does not transfer

**Both halves transferred, and §5c is the check that says so.** The contraction result is ISA-level:
any AArch64 compiler contracts `a*b+c` into `FMLA` by default because the instruction is in the base
ISA, and the frozen x86-64 baseline has no such instruction. The residual-24 result was measured
first against glibc's libm and was explicitly flagged as not transferable without a check; the macOS
run has since made that check and returned the same 24, the same 8, and the same scenario names.

**What remains untested.** Only what neither environment covers: Windows/MSVC, and any future
platform. The mechanism argument predicts the same shape wherever an FMA-bearing ISA meets a
platform libm, but that is prediction, not measurement.

---

# Part II — A7-5 AVX2 readiness, revised by A7-5E

## 6. The decision has changed shape: two thirds of the win is Class A

Because vectorization and contraction are separable, so is the benefit. Whole-engine Ir/sample,
48 kHz / 128, scenario `working`:

| build | Ir/sample | vs baseline | class |
|---|---:|---:|---|
| baseline (frozen flags, shipped) | 1704.9 | — | — |
| `-march=haswell -ffp-contract=off` | 1412.2 | **−17.2 %** | **Class A** — 32/32 dump identical, 180/180 configurations identical |
| `-march=haswell` (contract fast) | 1264.3 | −25.8 % | Class B — 32/32 differ |

**66 % of the AVX2 win is available without changing a single output bit.** The earlier framing —
"~20 % engine-wide, but a Class-B change to 88.9 % of samples" — was a single bundled option; it is
actually two options with very different costs.

## 6a. The two options, side by side

| | **Option A — Class A** | **Option B — Class B** |
|---|---|---|
| flags | `-march=haswell -ffp-contract=off` | `-march=haswell` (contraction left at its default) |
| **performance gain** | **−17.2 %** engine-wide (1704.9 → 1412.2 Ir/sample, 48 kHz/128, `working`) | **−25.8 %** (→ 1264.3 Ir/sample) |
| **numerical impact** | **none.** 32/32 twin-dump scenarios identical; **0 mismatches across 180 configurations** | **88.9 % of samples change**; max delta 2.384e-07 (−123.8 dB), RMS 4.949e-08 (−137.5 dB) |
| class | **A** (bit-exact) | **B** (in the accepted range: H3 `tanh` is 3.5e-7) |
| **compatibility** | Haswell (2013) / Excavator (2015) becomes a hard ISA floor; failure mode on older CPUs is `SIGILL` inside the host | identical ISA floor |
| GCC/Clang cross-check | **kept** — both toolchains stay bit-identical, because neither can contract | **lost** — the two diverge once contraction is live (32/32 in every pairing) |
| **ADR changes required** | amend ADR-0021 to carry two numerics-affecting flags, one of which *pins* the numerics rather than moving them; add an ISA floor to `COMPATIBILITY_POLICY` | the same, plus an accepted error bound in `DSP_POLICY` and a replacement for the two-toolchain cross-check |
| cross-architecture effect | removes the **contraction** half of the arm64/x86-64 difference (§5); does **not** remove the oversampling half | leaves x86-64 contracting too — which makes the slices *more* alike in mechanism but not bit-identical |

**Neither option is recommended for implementation in this round.** The material above is what an ADR
needs; the decision is §8.

## 7. What each option still costs

Common to both, and unchanged by A7-5E: an **ISA floor**. `-march=haswell` makes Haswell (2013) /
Excavator (2015) a hard requirement, and the failure mode on older hardware is `SIGILL` inside the
host, which the plug-in cannot diagnose. `COMPATIBILITY_POLICY` states no ISA floor today.

Specific to the Class-A option (`-ffp-contract=off`): it is the first numerics-affecting flag the
project would carry, so ADR-0021's freeze must be amended even though the output does not move — and
it would, for the first time, make the x86-64 contraction behaviour *explicit* rather than incidental.

Specific to the Class-B option: 88.9 % of samples change at a max delta of 2.384e-07 (−123.8 dB),
and GCC/Clang stop agreeing bit-for-bit, which costs the two-toolchain cross-check the `linux-lto-tests`
GCC job and the Clang jobs currently provide for free.

## 8. Decisions still required before an ADR can be written

1. **Class A or Class B?** A7-5E turns this into a real choice rather than a single package.
   −17.2 % with no bit change, or −25.8 % with a Class-B change of the H3 `tanh` order.
2. **What is the minimum supported x86-64 ISA?** A user-visible contract with a `SIGILL` failure
   mode; it belongs in `COMPATIBILITY_POLICY` *before* any flag lands, not after.
3. **Does `-ffp-contract=off` become the project's explicit numerics position on every platform?**
   Worth considering on its own merits — it removes the contraction half of the cross-architecture
   difference — but §5a is the caveat: it does **not** by itself make the two slices bit-identical,
   because the oversampling path differs for a reason a flag cannot reach.
4. **Is losing the GCC/Clang bit-agreement acceptable** if option B is taken, and what replaces it?
5. **Confirm A7-5E by execution** (§5) before the ADR is drafted.
6. **Does ADR-0021 get amended, superseded, or scoped?** The new ADR must say which, and
   `DSP_POLICY` and `CMakeLists.txt:82-84` must be resynced with it.

**Recommendation: still do not implement.** But the recommendation on *what to propose* has changed:
the ADR should present the Class-A variant as the primary option, with the Class-B one as a separate,
later question.

## 9. Why no wall-clock appears above

The A7-5 investigation quoted a wall-clock table alongside its Ir figures. It does not reproduce on
this container today: the same binary and workload measured **292 ms** in that round and **196 ms**
now, and an interleaved three-way run gives ranges that overlap almost completely (baseline
197–223 ms against contract-off 187–231 ms for a change Ir puts at −17 %). `PERFORMANCE_BUDGET.md`
already says wall-clock on a shared runner is not a datum; this round treats that as binding and
quotes Ir only. **The earlier wall-clock table should be read as indicative of its own session, not
as a reproducible measurement.**

---

# Part III — A7-2B: implemented

## 10. What changed

`linHist` — the linear history image H5 built and A7-1 slid — is **deleted**, along with the
`linHistSlide` offset, its clear-on-entry, its re-arm, its `reset()` clear and its `prepare()`
allocation. Each tap now reads `midHist` in place as **1–3 unit-stride runs**: a ring portion of
`min(k, numSamples)` samples from `(writePos - k) & histMask`, emitted as one run when it does not
cross the ring origin and two when it does, then this block's own `midBlk[i - k]` for `i >= k`.

The module now carries **no cross-block scratch state at all** — the property A7-1 had to defend
with an invalidation rule is gone rather than defended.

Two spellings the investigation identified as traps are excluded by construction and said so in the
source: the ring run is `min(k, numSamples)` (taking `k` overruns `accum` on small blocks — ASan
catches it), and the tail is indexed `midBlk[i - k]` rather than based on `midBlk.data() - k` (which
is UB that ASan + UBSan + `local-bounds` + `pointer-overflow` together do **not** catch).

## 11. Class A, on both committed instruments

| evidence | result |
|---|---|
| `AnamorphDspDump`, 32 scenarios, pre- vs post-change | **identical** |
| dump `--self-check` (repeatable AND distinct) | passed |
| 180 configurations (9 scenarios × 5 block sizes × 4 rates), FNV-1a over every output sample | **0 mismatches** |
| `AnamorphTests` incl. Test 39 and Test 40 | **202 checks, 0 failures** |
| `AnamorphStateTests` | 920 checks, 0 failures |
| ASan + UBSan + `local-bounds` + `pointer-overflow`, `detect_leaks=1` | 200 checks, **0 diagnostics** |

Test 40 is the gate this change was made to wait for: it compares the gather against the per-sample
loop directly, which is the axis Test 39 cannot see.

## 12. Measured effect

Callgrind Ir/block, startup-subtracted, scenario `working` (density 0.5):

| configuration | v0.9.5 | A7-2B | delta |
|---|---:|---:|---:|
| 48 kHz / 32 | 65,141.4 | 57,161.8 | **−12.2 %** |
| 48 kHz / 128 | 220,223.6 | 212,995.2 | −3.3 % |
| 48 kHz / 512 | 842,022.4 | 835,344.9 | −0.8 % |
| 96 kHz / 64 | 125,388.6 | 109,390.6 | −12.8 % |
| 192 kHz / 32 | 91,065.7 | 57,178.7 | **−37.2 %** |
| 192 kHz / 128 | 246,145.8 | 212,823.9 | −13.5 % |
| 192 kHz / 512 | 865,718.7 | 832,481.7 | −3.8 % |

The fixed per-block term, fitted from the measured points: **12,957 → 5,546 Ir at 48 kHz** and
**39,373 → 6,104 Ir at 192 kHz**. The shape is the result, not the percentages: A7-1's term was
proportional to `decorrSamps` and therefore grew with the sample rate; A7-2B's does not. At a
32-sample block the rate penalty from 48 to 192 kHz falls from **39.8 % to 0.04 %**.

## 13. The measured trade, and the fast path that shrank it

The win narrows as the rate falls, because the image A7-1 slid is smallest at 44.1 kHz while the new
per-tap preamble is rate-independent and paid once per active tap. Measured across the density axis
at 44.1 kHz / 32:

| density (active taps) | first form | **shipped form (no-wrap fast path)** |
|---|---:|---:|
| 0.0 (0 taps) | −0.9 % | −0.9 % |
| 0.5 (32 taps, the default) | +0.9 % | **−0.2 %** |
| 1.0 (64 taps) | +3.0 % | **+1.0 %** |

The ring portion usually does not cross the ring origin, so the wrap bookkeeping is pure overhead in
the common case; the shipped form branches once and emits a single run when `r0 + fromRing <= ringN`.
It is better at every point measured — 48 kHz/32 −11.3 % → −12.2 %, 192 kHz/32 −36.5 % → −37.2 % —
and it is what holds the worst case at **+1.0 %, at 44.1 kHz with a 32-sample buffer and Density at
maximum**. That corner is the one place A7-2B is worse than what it replaces, and it is stated here
rather than left to be discovered.

## 14. Residual risk

* **The +1.0 % corner** above. It is the lowest supported rate × smallest block × maximum density;
  every other measured point is neutral or better, and the programme's target — the worst block, at
  high rates and small buffers — improves by up to 37 %.
* **Deep taps and the wrap remain lightly exercised by committed evidence.** Test 40 sweeps density
  1.0 and block 4096, which reaches both; the 180-configuration sweep still runs at density 0.5.
* **Single-platform.** All of the above is x86-64 Linux. Given Part I, cross-architecture bit
  identity is an open question for the whole product, not for this change.

---

# Part IV — A7-9 readiness (no DSP change; approval still required)

## 15. The analysis re-verified against the post-A7-2B engine

The mechanism is unchanged and was re-run, not carried over. The glide is `a += k*(target - a)`;
with a zero target under `ScopedNoDenormals` the **decrement** underflows before the state does, so
`a` freezes at `≈ FLT_MIN / k` and the next decrement is exactly `0.0`.

| module | k | stall, post-A7-2B | `FLT_MIN / k` | rate behaviour |
|---|---|---:|---:|---|
| `VelvetNoise` | `0.0015f`, a constant | 7.83374862e-36 | 7.83662901e-36 | flat |
| `HaasProcessor` | `0.001f`, a constant | 1.17487956e-35 | 1.17549435e-35 | flat |
| `ChorusEngine` | `1/(0.01·sr)` | 5.63638313e-36 @48k | 5.64237288e-36 | **scales ×4.00 over the range** |

All three parked fast paths still gate on a **value** test that a stalled glide defeats, while
Velvet's density gate — three lines above its amount gate — tests the **fixpoint** and is immune.

## 16. A7-2B changed A7-9's economics, and this is the headline of Part IV

The cost of the missed park is the cost of the path the module is stuck on. A7-2B made that path
cheap and rate-independent, so **most of A7-9's Velvet cost has already been absorbed**:

| module / config | penalty before A7-2B | **penalty now** | absolute Ir/block now |
|---|---:|---:|---:|
| `VelvetNoise` 48 kHz / 32 | +752 % | **+251 %** | +4,019 |
| `VelvetNoise` 192 kHz / 32 | **+2,372 %** | **+252 %** | +4,032 |
| `VelvetNoise` 48 kHz / 128 | +316 % | +188 % | +11,467 |
| `HaasProcessor` 48 kHz / 128 | +203 % | +203 % | +5,635 |
| `ChorusEngine` 48 kHz / 128 | +370 % | +370 % | +14,220 |

At 192 kHz / 32 the Velvet figure fell from **37,951 to 4,032 Ir/block — an 89 % reduction**. Two
consequences for the decision:

* **A7-9 is no longer the largest item on the A7 roadmap.** It was, on the pre-A7-2B numbers.
* **The priority inside A7-9 has moved to `ChorusEngine`**, now the largest single contributor, and
  it is also the one whose stall threshold scales with the sample rate.

## 17. Numerical bounds, re-measured

Two instances, identically built and driven; afterwards one has its glide forced to exact zero —
what a fix produces. Every later difference is exactly the residual a fix would remove.

| module | noise input | digital silence, 48 kHz | digital silence, 192 kHz |
|---|---|---:|---:|
| `VelvetNoise` | **0 / 102,400 differ** | 4.470e-37 | 4.627e-37 |
| `HaasProcessor` | **0 / 102,400 differ** | 1.643e-36 | 1.644e-36 |
| `ChorusEngine` | **0 / 102,400 differ** | 1.081e-36 | **4.476e-36** |

**Bound over the supported range: 4.476e-36 (≈ −707 dBFS), set by `ChorusEngine` at 192 kHz.**

> **CORRECTED 2026-08-22 during implementation.** That figure is the maximum *this harness* observed,
> not a stimulus-independent bound. Driving ±0.7 noise, Test 41 measures **1.563e-35** (Chorus,
> 192 kHz), 8.043e-36 (Haas, 48 kHz) and 7.145e-36 (Velvet, 48 kHz) against the pre-A7-9 sources —
> 3.5× the figure above. What bounds the residual is `FLT_MIN/k` times the module's wet gain.
> `PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md` §4c. The direction of the change is unaffected: after
> A7-9 the silence output is an **exact zero**, which is stronger than any bound.

Unchanged by A7-2B. Inaudible by construction on real signal; the residual appears only where the
dry term is `+0` and cannot absorb it.

## 18. The decision material

**What would change.** Three gates change from a value test to a **fixpoint** test, mirroring the
density gate that already sits three lines above Velvet's:
`VelvetNoise.cpp:154` (amount), `HaasProcessor.cpp:73`, `ChorusEngine.cpp:97`. No state is mutated; only
which path runs. The alternative — snapping the glide state to zero — is worse: it mutates DSP state
rather than choosing a path, and every consumer of `currentAmount` would need re-auditing.

**Why it is safe.** The change is bounded at 4.476e-36, which is ~26 orders of magnitude tighter
than the tightest accepted Class-B precedent (H4 dry-align, 2.4e-10) and ~32 tighter than the
loosest (H11, 8.2e-4). Zero samples differ on real signal at any rate measured.

**Why it is not Class A, and no Class-A variant exists.** The residual *is* what distinguishes the
stalled state from the parked one, so removing it is what "fix" means. Parking only when the residual
provably cannot change the output is not expressible as a block-level gate — `x + a*(d-x)` is
bit-identical to `x` for a normal `x` and is not when `x` is `+0`.

**Contract changes involved.** None to parameters, serialization, threading, signal order or
reported latency. It is a **Class-B numerics change** under `DSP_POLICY`, which is the whole of the
approval question.

**What approval is required.** Explicit maintainer approval for a Class-B change, recorded. No
`ARCHITECTURE_REVIEW_GATE` category is triggered and no Accepted ADR is contradicted.

## 18a. Decision-ready summary (A7-9)

| question | answer, as measured |
|---|---|
| **Expected benefit after A7-2B** | `ChorusEngine` **+14,220 Ir/block** at 48 kHz/128; `HaasProcessor` **+5,635**; `VelvetNoise` **+4,019** at 48 kHz/32 and **+4,032** at 192 kHz/32. Velvet's share fell **89 %** because A7-2B made the path it is stuck on cheap and rate-independent. |
| **Remaining affected modules** | All three, unchanged: `VelvetNoise`, `HaasProcessor`, `ChorusEngine`. Priority is now **Chorus first** — largest share, and the only one whose stall threshold scales with sample rate (`wSmooth = 1/(0.01·sr)`). |
| **Numerical difference bounds** | **4.476e-36** worst case across 44.1–192 kHz (Chorus at 192 kHz), ≈ −707 dBFS. **0 of 102,400 samples differ on real signal** in all three modules at every rate measured; the residual appears only on digital silence, where the dry term is `+0` and cannot absorb it. |
| **Class A or Class B?** | **Class B, and no Class-A variant exists.** The residual *is* what distinguishes the stalled state from the parked one, so removing it is what "fix" means. Parking only when the residual provably cannot change the output is not expressible as a block-level gate: `x + a*(d-x)` is bit-identical to `x` for a normal `x` and is not when `x` is `+0`. |
| **Unresolved risks** | (a) The benefit is now modest relative to its Class-B cost — this is the main open judgement. (b) A fixpoint gate must be proved *live* by a test that shows the park is reached after a ramp-down; the current suite cannot distinguish a reachable fast path from an unreachable one, which is how this survived. (c) Chorus's rate-dependent coefficient is worth a maintainer's eye independently of the gate change. |
| **What approval is required** | Explicit, recorded maintainer approval for a Class-B numerics change under `DSP_POLICY`. No `ARCHITECTURE_REVIEW_GATE` category is triggered; no Accepted ADR is contradicted; no parameter, serialization, threading, signal-order or latency change. |

**Recommendation: approve, but at lower priority than before.** It was the roadmap's largest item on
the pre-A7-2B numbers and is not any more. Nothing about its safety changed; its value did.

## 19. Remaining questions a maintainer decision needs to answer

1. **Is 4.476e-36 acceptable**, given zero audible effect and the precedent range?
2. **Is it still worth doing now that A7-2B absorbed 89 % of the Velvet cost?** The remaining prize
   is ~4,000 Ir/block for Velvet and ~14,000 for Chorus — real, but no longer the roadmap's largest.
3. **Should `ChorusEngine` be done first**, or all three together? Doing one is a smaller Class-B
   surface; doing all three is one decision instead of three.
4. **Should the rate-dependence of Chorus's coefficient be addressed at the same time?** Its stall
   threshold scales with sample rate because `wSmooth = 1/(0.01·sr)`; a fixpoint gate is immune to
   that, but the coefficient's rate dependence is worth a maintainer's eye on its own.

## 20. The comment corrections should be separate, and here is why

Three source comments stated a premise that is measurably false — that the one-pole "flushes to true
zero". A7-9C has since replaced each with a correction, at `VelvetNoise.cpp:228-240`,
`HaasProcessor.cpp:51-63` and `ChorusEngine.cpp:72-85`; the line numbers here are those corrected
blocks, since the text they replaced no longer exists to cite.

**Recommendation: correct the comments independently of the Class-B decision, and first.**

* The correction is **Class A** — it changes no code, no behaviour and no bits — while the
  optimization is Class B. Bundling them makes a zero-risk documentation repair wait on a numerics
  approval it does not need.
* `DOCUMENTATION_LIFECYCLE_POLICY` does not permit a known-false justification to stand in the
  source once the drift is reported, and it has now been reported twice.
* If the optimization is **declined**, the comments must be corrected anyway — so the correction is
  unconditional and the optimization is not.
* If the optimization is **approved**, it will rewrite those comments regardless, and having them
  already true makes that diff about the gate rather than about the false premise.

**DONE.** A7-9C landed in the follow-up round: all three comments now state the precondition
without the false justification, and each records how the parked state is and is not reached —
including the closed form of the stall and, for Chorus, that its threshold scales with the sample
rate. Verified behaviour-neutral: the 32-scenario twin dump is **identical** across the comment
change, both suites stay at 202/0 and 920/0, and `check-realtime` and `check-portability` are clean.
The A7-9 gates themselves are untouched, which is the decision this does not pre-empt.

---

# Part V — roadmap

| item | status |
|---|---|
| **A7-0** — bench on a named machine, fill the `PERFORMANCE_BUDGET.md` rows | **blocked, unchanged.** This container is a masked-CPU shared machine; §9 re-demonstrates why. RISK-002 open, rows unpopulated. |
| **A7-1** — Velvet history slide | DONE (v0.9.5), and **superseded by A7-2B**. |
| **A7-2T** — path-equivalence oracle (Test 40) | DONE (PR #129). Spent as designed: A7-2B landed against it. |
| **A7-2 / A7-2B** — residual per-block term | **DONE this round.** Class A on both instruments; −12.2 % at 48 kHz/32, −37.2 % at 192 kHz/32; rate dependence removed; one +1.0 % corner recorded. |
| **A7-5E** — cross-slice experiment | **CONFIRMED BY EXECUTION** (§5): the committed harness, cross-built and run on both architectures, differs in **32/32** scenarios. Two mechanisms — contraction (flag-removable) and oversampling coefficients from libm (not). The macOS CI diff remains worth running as platform-exact confirmation, no longer as the deciding experiment. |
| **A7-5 / W5-D** — AVX2 | **readiness advanced, not implemented.** Now two options, one of them Class A at −17.2 % (§6). Six decisions listed (§8). |
| **A7-9** — the glide stall | **readiness complete; implementation pending explicit Class-B approval.** Cost re-measured and 89 % of the Velvet share absorbed by A7-2B (§16). |
| **A7-9C** — correct the three false comments | **DONE** (§20). Class A, verified behaviour-neutral against the twin dump. |
| A7-4 · A7-8 | maintainer decisions, unchanged. |
| A7-3 · A7-6 · A7-7 | not scheduled, unchanged. |
