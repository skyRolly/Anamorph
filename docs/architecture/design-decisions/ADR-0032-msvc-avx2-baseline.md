# ADR-0032 — Extend the x86-64 ISA baseline to Windows/MSVC: `/arch:AVX2`

**Status:** Accepted (Build System change — explicit maintainer approval 2026-08-30, applying the
prepared adoption packet; supersedes the deferral recorded as ADR-0031 option 5, which required
exactly the evidence this ADR now cites).

## Context

ADR-0031 gave the GCC/Clang x86-64 builds `-march=haswell -ffp-contract=off` — measured Class A —
and deferred MSVC on evidence grounds: `/arch:AVX2` is not the equivalent pair (MSVC controls
contraction separately, through `/fp`) and no bit-exactness instrument ran on Windows, so the
Class-A property could be assumed but not demonstrated. The platform-coverage audit's R-round built
that instrument: the `windows-avx2-ab` job builds the committed `AnamorphDspDump` three ways on
`windows-latest` and diffs the 32 scenario hashes as a **same-machine A/B** — the scope
`COMPATIBILITY_POLICY` gives every bit-identity claim, and doubly necessary on Windows, where the
x64 UCRT dispatches its transcendental implementations by CPU class at process start.

## Problem

Take the AVX2 vectorization win on Windows without moving an output bit, without resting a Class-A
claim on an unguarded toolset default, and without an undocumented ISA floor reaching users.

## Evidence

Measured on MSVC toolset **14.51.36231** (`windows-latest`), across **two runs** — the first run's
phantom 33rd-row parser artifact was found, fixed, and the corrected parser replicated the verdict:

- **A (MSVC defaults: SSE2 baseline, `/fp:precise`) vs B (`/arch:AVX2`): IDENTICAL — 0 of 32
  scenarios differ.** The vectorizer performed no bit-moving reassociation for this instrument's
  coverage.
- **B vs C (`/arch:AVX2 /fp:contract`): 32 of 32 scenarios differ** — contraction is the
  bit-moving mechanism, the same split A7-5E measured on GCC/Clang (0/32 for the pinned flag pair;
  32/32 with contraction live).
- Microsoft documents `/fp:precise` as non-contracting by default since VS2022 (17.0 / toolset
  14.30), with `/fp:contract` the explicit opt-in (verified against learn.microsoft.com and the
  VS2022 announcement; the platform-coverage audit §5 carries the citations).

## Options considered

1. **Keep the deferral.** Rejected by this ADR: the deferral's own stated blocker — "could not be
   demonstrated" — is discharged by the measurement above.
2. **`/arch:AVX2` at default `/fp:precise` — ADOPTED.** Class A demonstrated; the Windows analogue
   of ADR-0031 option 2, with the default-not-pin weakness closed by the toolset assertion and the
   promoted blocking gate below.
3. **`/arch:AVX2 /fp:contract`.** Rejected: Class B on every scenario (32/32), and it would break
   the cross-platform symmetry of the contraction stance for no demonstrated gain. `/fp:contract`
   is **not** enabled anywhere; enabling it is a new ADR.
4. **Runtime CPU dispatch.** Rejected for ADR-0031 option 6's reasons — this is a deliberate
   Windows x86-64 **ISA-floor decision**, not a dispatch mechanism.

## Decision

- `AnamorphHardening`'s `if(MSVC)` branch adds **`/arch:AVX2`**, guarded by the **same
  `ANAMORPH_X86_ISA_BASELINE` option** that governs the GCC/Clang pair — the option stops being
  inert on MSVC, giving Windows the same one-switch baseline-comparison path the twin dump needs.
- **No `/fp` flag is added**: the non-contracting VS2022+ default *is* the decision, made sound by
  two standing guards rather than by trust in a default —
- the `windows` job's toolset step **asserts minor ≥ 30 within the 14.x series** (the boundary at
  which `/fp:precise` stopped contracting by default) and now **fails on an unreadable toolset
  version** instead of warning: an unplaceable toolset cannot anchor this ADR's claim.
- **`windows-avx2-ab` is promoted from reporting-only to a BLOCKING gate**: baseline
  (`ANAMORPH_X86_ISA_BASELINE=OFF`) vs the shipped configuration must agree on **all 32
  scenarios**, on every push. The `/fp:contract` build (C) stays **informational only** — 32/32
  differing is an observation about contraction, not a contract — and a C-side failure never fails
  the gate. The gate distinguishes **numerical failure** (comparison ran, hashes differ — the
  Class-A contract broken) from **infrastructure failure** (the gate could not run) in its error
  labels; both block, deliberately: no other job verifies this property, so a gate that cannot run
  must not pass (`fail-closed`, the standing rule of this workflow).
- The Windows ISA floor (Haswell 2013 / Excavator 2015; failure mode `STATUS_ILLEGAL_INSTRUCTION`
  0xC000001D inside the host at plug-in load) is documented in `COMPATIBILITY_POLICY`,
  `COMPATIBILITY_MATRIX`, `KNOWN_ISSUES` (KI-026 widened) and both user guides **in the same change
  as the flag** — the ADR-0031 ordering rule.

## Consequences

- Windows users need Haswell (2013) / Excavator (2015) or newer; below it the host crashes at
  plug-in load with no diagnosable message — the same floor, failure mode, and documentation
  treatment as ADR-0031's, now uniform across every shipped x86-64 binary. Only the arm64 slice
  carries no floor.
- **The benefit on Windows is mechanism-shared, not measured.** No Windows instruction-count
  instrument exists, and no Windows-specific performance figure may be quoted anywhere. The
  measured figures for the analogous change are Linux: −18.4 % (GCC 13.3) / −19.6 % (Clang 22.1.8)
  engine Ir/sample.
- The UCRT's runtime FMA3 transcendental dispatch (machine-class-dependent output, documented in
  `COMPATIBILITY_POLICY`) is unchanged by this ADR and unrelated to `/arch`.
- clang-cl remains outside the validated set (`COMPATIBILITY_MATRIX`, toolchain section) — it would
  now structurally *inherit* `/arch:AVX2` through the `if(MSVC)` branch, which is recorded there as
  one more reason a clang-cl toolchain, if ever introduced, needs its own investigation and ADR.
- ARM64 is untouched; Linux/Windows arm64 remain Not Supported; ADR-0031's GCC/Clang pair is
  unchanged.

## Related code

`CMakeLists.txt` (the `AnamorphHardening` MSVC branch), `.github/workflows/build.yml` (the
`windows` job's toolset assertion; the `windows-avx2-ab` blocking gate),
`docs/policies/COMPATIBILITY_POLICY.md`, `docs/architecture/COMPATIBILITY_MATRIX.md`,
`docs/KNOWN_ISSUES.md` (KI-026), `docs/user/USER_MANUAL.md`, `docs/user/INSTALLATION.md`,
`worklogs/performance/PERF_AUDIT_PLATFORM_COVERAGE.md` (§5, §R-results),
`worklogs/performance/MSVC_AVX2_ADOPTION_PACKET.md` (the prepared-changes record this ADR applies).

## Evidence + confidence

**Verified (measured):** the A/B/C table above — two CI runs on toolset 14.51.36231, the second
with the corrected 16-hex-hash parser. **Verified (docs):** the `/fp` semantics change at VS2022.
**Not measured, stated as such:** any Windows performance figure. **To confirm on the first
post-adoption CI run:** the toolset assertion green at ≥ 14.30; the blocking gate green at 0/32
with the flag live — where the gate's B build now takes the flag from `AnamorphHardening` itself
(the shipped mechanism) and self-checks that the flag is actually present in B's build files and
absent from A's, so "0/32" can never again be trivially true because the flag silently failed to
apply.
