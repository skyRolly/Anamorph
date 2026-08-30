# ADR-0031 — x86-64 ISA baseline: `-march=haswell -ffp-contract=off` (A7-5, Option B)

**Status:** Accepted (Build System change — maintainer approval 2026-08-22, selecting Option B of
`worklogs/performance/A7_DECISION_PACKET.md` Decision 1 and position 1 of its Decision 3).
**Option 5's MSVC deferral is superseded by [ADR-0032](ADR-0032-msvc-avx2-baseline.md)**
(2026-08-30): the instrument option 5 said was missing was built, measured 0/32 scenarios moved by
`/arch:AVX2` alone on toolset 14.51.36231, and the Windows build now carries the flag under its own
ADR. Everything else in this ADR stands, including option 4's arm64 exclusion.

**Amends ADR-0021.** ADR-0021 recorded, under "Untouched by decision", that numerics-affecting
compiler flags are frozen. That clause is amended here — narrowly, and for the x86-64 GCC/Clang
builds only. Everything else in ADR-0021 stands: the retain-then-strip pipeline, RELRO/CFG/stack
protector, `-O3`, LTO and the absence of `-ffast-math` are untouched.

## Context

`RELEASE_HARDENING_PLAN` froze the compiler's numerics because a bit-identical build was the
instrument every dependency bump had been validated with (`DEPENDENCY_POLICY` rule 2, the
`AnamorphDspDump` twin dump). The frozen x86-64 baseline is the System V default: SSE2, no AVX,
and — decisively — **no FMA instruction at all**, which is why the permissive `-ffp-contract=fast`
default has been inert on this platform for the project's whole life.

The A7 performance programme measured what that costs. The engine is a serial chain of small
per-sample loops; at the frozen ISA the auto-vectorizer has 128-bit registers and no fused
multiply-add, and the loops that dominate the profile (`VelvetNoise`'s tap gather, `LoudnessMatch`'s
K-weighting, the oversampling polyphase kernels) are exactly the shapes AVX2 was designed for.

The A7-5E experiment then produced the finding that restructured the decision:
**vectorization and FP contraction are separable.** They had been treated as one package —
"AVX2 is a Class-B change" — because `-march=haswell` enables both at once. `-ffp-contract=off`
splits them.

## Problem

Take the vectorization win without moving a single output bit, without losing the two-toolchain
bit-agreement that currently cross-checks GCC against Clang for free, and without letting an
ISA floor reach users undocumented.

## Options considered

1. **Status quo — no `-march`.** Zero risk, zero benefit. Rejected: the measured cost is 17.2 % of
   engine-wide instruction count, and the reason for the freeze (a bit-identical validation
   instrument) is preserved by option 2 rather than defended by option 1.
2. **`-march=haswell -ffp-contract=off` — Class A. ADOPTED.** −17.2 % engine-wide with **zero**
   output change: contraction is what would have moved the bits, and turning it off pins the
   arithmetic to exactly the sequence the frozen baseline produced. The flag does not *change* the
   numerics; it *states* what the missing FMA instruction was previously enforcing by accident.
3. **`-march=haswell` alone — Class B.** −25.8 %, but 88.9 % of samples change (max delta
   2.384e-07, −123.8 dB; RMS 4.949e-08, −137.5 dB) — inside the accepted Class-B precedent range
   (H3 `tanh` 3.5e-7, allpass 1.19e-7, H11 chorus LFO 8.2e-4) but broader than any of them: those
   bounded one kernel, this moves the whole engine. It also **destroys the GCC/Clang cross-check**
   — once contraction is live the two toolchains differ in all 32 twin-dump scenarios, in every
   pairing, including GCC-haswell vs Clang-haswell. Rejected here, and deliberately left as a
   separate decision rather than folded into this one: it buys 8.6 percentage points more for a
   whole-engine numerics change and the loss of a standing verification instrument.
4. **`-ffp-contract=off` on every architecture, not only x86-64.** Rejected. On AArch64 `FMLA` is
   in the base ISA and the shipped arm64 slice contracts today; disabling it there is a Class-B
   change to shipped arm64 numerics, taken for no user-visible reason. It would close one half of
   the arm64/x86-64 difference and not the other (see `COMPATIBILITY_POLICY`, "Numerical
   compatibility"), so it does not even buy cross-slice identity.
5. **Extend the ISA baseline to the MSVC build (`/arch:AVX2`).** Rejected **for now, on evidence
   grounds rather than on merit**: `/arch:AVX2` is not the equivalent of the adopted pair, because
   MSVC controls contraction separately, and no twin-dump instrument runs on Windows — so the
   Class-A property could not be *demonstrated* there, only assumed. Extending it needs its own
   measurement and its own ADR.
6. **A runtime CPU dispatch layer** (baseline build + AVX2 path selected by `__builtin_cpu_supports`).
   Rejected as disproportionate: it needs a separately compiled baseline translation unit, doubles
   the object code for the hot kernels, and would have to be validated twice over — for a plug-in
   whose supported-hardware floor is already 2013.

## Decision

`AnamorphHardening` (ADR-0021's INTERFACE target) carries, **for GCC/Clang x86-64 targets only**:

```
-march=haswell -ffp-contract=off
```

- **Linux x86-64** and any thin `x86_64` macOS build: the flags directly.
- **macOS universal:** `-Xarch_x86_64` on each flag, so they reach the `x86_64` slice and not the
  `arm64` one. A universal build runs the driver once per architecture; an unqualified `-march`
  would be handed to the arm64 invocation and fail the build.
- **arm64:** nothing added. Its contraction behaviour is unchanged, deliberately (option 4).
- **MSVC:** nothing added (option 5).

Both flags are **compile** options, not link options. Under LTO the codegen happens at link time,
but both toolchains stream the target CPU/feature set per function into the IR (LLVM's
`target-cpu`/`target-features` attributes; GCC's per-function target options), so the compile-time
`-march` is what governs the emitted code.

`-ffp-contract=off` is **not optional dressing on `-march=haswell`** — it is the half that makes
this Class A, and the two must never be separated. The order in which a reviewer should read them:
`-march=haswell` introduces an FMA instruction that did not exist on this target before, and
`-ffp-contract=off` forbids the optimiser from using it to fuse a multiply and an add that the
source wrote separately.

## Compatibility consequences

**An ISA floor now exists where none did: Haswell (Intel, 2013) / Excavator (AMD, 2015).** Below it
the binary raises `SIGILL` inside the host process, which the host reports as a crash rather than as
an incompatible plug-in, and which the plug-in cannot diagnose for itself — the fault can be raised
by loader-run static initialisers compiled under the same flags. It is materially user-visible on
**macOS**, where `CMAKE_OSX_DEPLOYMENT_TARGET` is 10.13 and High Sierra runs on Macs back to 2009.
This is written up as a first-class contract in `COMPATIBILITY_POLICY.md`
("Runtime compatibility: the x86-64 ISA floor"), which is where a change to the floor must go
through in future, and mirrored in `COMPATIBILITY_MATRIX.md`.

**Rosetta 2 does not translate AVX2 by default.** The `macos` job's Intel coverage runs the
`x86_64` slice under Rosetta, and the `macos-crossslice` job runs the `x86_64` dump the same way;
both now probe for AVX2 executability first and degrade to a `::warning::` when it is absent, the
same shape as the existing "Rosetta unavailable" path. This costs no gate: real Intel coverage is
the `macos-intel` job on `macos-15-intel` hardware, which is native and blocking.

**No session, parameter, serialization, latency or threading contract is touched**, and no
Architecture Review Gate category other than Build System is triggered.

## Consequences for verification

- The **twin dump stays the instrument**, and its Class-A claim was checked directly: baseline vs
  `-march=haswell -ffp-contract=off` is **0 of 32 scenarios different**, and **0 mismatches across
  180 configurations** (4 algorithms × oversampling × M/S × rate).
- The **GCC/Clang cross-check survives**, because neither toolchain can contract: GCC-haswell and
  Clang-haswell remain bit-identical to each other and to both baselines. This is the property
  option 3 would have destroyed.
- `COMPATIBILITY_POLICY` now states the scope of every bit-identity claim in this repository:
  **within one architecture and build configuration, not across architectures.** The twin-dump gate
  compares builds within an architecture only. That was already true; it was not written down until
  A7-5E measured how far from true the cross-architecture case is (32/32 scenarios differ at
  shipped flags).

## Related code

`CMakeLists.txt` (the `AnamorphHardening` x86-64 baseline block),
`.github/workflows/build.yml` (the Rosetta AVX2 probes in `macos` and `macos-crossslice`),
`docs/policies/COMPATIBILITY_POLICY.md` (ISA floor + cross-architecture numerics),
`worklogs/performance/PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md` (§5, §5c, §6, §6a — the
measurements), `worklogs/performance/A7_DECISION_PACKET.md` (Decisions 1 and 3).

## Evidence + confidence

**Verified (measured, callgrind with startup subtraction — 3.0 s run minus 1.0 s run over 2.0 s,
the `PERFORMANCE_BUDGET.md` unit; wall-clock is not quoted because this container is a shared
machine on which the same binary and workload measured 292 ms and 196 ms in consecutive rounds):**
engine-wide 1704.9 → 1412.2 Ir/sample at 48 kHz / 128, scenario `working` (−17.2 %); 32/32
twin-dump scenarios byte-identical to the baseline; 0/180 configuration mismatches; 0 FMA
instructions emitted under the adopted pair (objdump census), against 707 with contraction left at
its default; GCC-haswell vs Clang-haswell bit-identical.

**To confirm on CI:** the `-Xarch_x86_64` form on AppleClang for a universal link, and the Rosetta
AVX2 probe's verdict on the current `macos-latest` image — both derived from documented driver and
translation semantics and validated by the first CI run carrying this ADR.
