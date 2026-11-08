# ADR-0073 — Preserve Linux ABI and TSan coverage across runner rollouts

**Status:** **Proposed** (2026-11-08 UTC). The owner authorized investigating and
correcting PR #231's two CI environments. The shipping-label pin is flagged under
`ARCHITECTURE_REVIEW_GATE.md` rule 3 for independent human review before merge;
this record does not assert architecture acceptance or authorize a release.

## Context

ADR-0030 keeps Clang/lld as the shipping Linux toolchain. Clang remains pinned to
22; the production ABI checker still caps GLIBC/GLIBCXX/CXXABI at
2.38/3.4.31/1.3.14. The x86-64 Haswell/Excavator contract is unchanged.
`ubuntu-latest` resolved to Ubuntu 24.04 on the last passing main build, then
Ubuntu 26.04 on PR #231. The TSan job already retained Ubuntu 24.04 container
userspace, which does not isolate the host's address-space layout.

## Problem

Run [42473360414](https://github.com/skyRolly/Anamorph/actions/runs/42473360414)
at `5ab9d4668ccc8600115ceeb687560b640cdc1e9b` exposed two environment failures:

- Both real shipping formats import `sqrtf`, `atan2f` and `log10f` at GLIBC_2.43
  on Ubuntu 26.04, exceeding the existing ceiling. Their prior Ubuntu 24.04
  imports of those symbols were GLIBC_2.2.5. The targeted retry still exceeded
  the ceiling in both shipping formats.
- TSan cannot reliably initialize its shadow-memory layout on the measured
  32-bit-entropy host under container seccomp mode 2. The first attempt failed
  before the canary could report its race. A targeted retry reported the seeded
  race, then failed initialization in the first real project probe.

The same Clang/TSan package passed on the preceding Ubuntu 24.04 run with
28-bit entropy and seccomp mode 2. Neither failure establishes a project race.

## Options

1. Retry unchanged: tested; neither failed job completed its required validation.
2. Select the previously measured, supported Ubuntu 24.04 host family for the
   two broken jobs: smallest correction, preserving all existing build/test bodies.
3. Introduce a shipping container or dedicated sysroot: can isolate userspace,
   but adds build/packaging machinery; a container alone did not fix TSan's host layout.
4. Change compiler/runtime, symbol linkage, ABI ceilings or host security settings:
   unnecessary for the measured compatible environment and broader than this repair.

## Decision

Propose option 2. `linux` and `tsan` select `ubuntu-24.04`. TSan keeps its
`ubuntu:24.04` container; its recorded requested runner is updated to match.
Other jobs keep their existing environments. No compiler, dependency, ISA,
product source, canary, suppression, sanitizer option, ABI check or packaging
body changes.

## Consequences

The label fixes the OS family, not the image revision, kernel or package patches.
Future image changes still require the actual seeded-race diagnostic, all five
probes x5, the complete instrumented state suite and suppression liveness to pass.
The unchanged ABI checker must inspect both real rebuilt shipping artifacts.
Revisit this selection when support changes or either detector rejects the image;
do not weaken the detector or change kernel/security settings to accommodate it.

## Related code

- `.github/workflows/build.yml`: `linux`, `tsan` and requested-runner metadata.
- `scripts/check-linux-abi.py`, `CMakeLists.txt`: unchanged ABI and ISA contracts.
- `tests/tsan_canary.cpp`, `tests/tsan-suppressions.txt`: unchanged liveness inputs.

## Evidence + confidence

**Verified environment comparison:** passing main run 42367437446, jobs
128388364557/128388364523, versus PR run 42473360414, jobs
128709107788/128709107564 and targeted retry jobs 128772509275/128772509596.
Actual shipping artifacts and retained TSan metadata were inspected, not synthetic
substitutes. The [investigation record](../../../worklogs/MULTIBAND_FREQUENCY_INPUT.md#ci-environment-repair-pr-231)
contains image/package versions and measurements. Corrected exact-HEAD hosted
results are recorded on [PR #231](https://github.com/skyRolly/Anamorph/pull/231);
prior passing evidence alone does not certify the corrected head.
