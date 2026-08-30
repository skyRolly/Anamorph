# MSVC `/arch:AVX2` adoption packet — prepared changes, awaiting the maintainer's decision

**Date:** 2026-08-30 · **Status: PREPARED, NOT APPLIED.** Nothing in this document is enabled.
The shipped Windows build still compiles at the MSVC defaults (SSE2 baseline, `/fp:precise`), the
`windows-avx2-ab` job is still reporting-only, and ADR-0031 option 5 still governs. This packet
exists so that, if the maintainer approves extending the x86-64 ISA baseline to Windows, landing
it is an act of review rather than of reconstruction — and so that rejecting it costs one sentence.

**Why it stops here.** `ADR_POLICY.md` makes an ADR mandatory for a Build architecture change, and
`ARCHITECTURE_REVIEW_GATE.md` puts Build System changes behind human review. Registering ADR-0032
and flipping flags without that approval would bypass both. The decision boundary is therefore the
top of this file; everything below it is the prepared material.

---

## 0. The decision, stated so it can be taken in one line

> **Approve or decline:** extend ADR-0031's x86-64 ISA baseline to the Windows/MSVC build as
> `/arch:AVX2` at the default (non-contracting) `/fp:precise`, imposing the same Haswell/Excavator
> ISA floor on Windows users that Linux and Intel-macOS users already have, on the evidence below.

**The evidence for** (all measured, none assumed):

| fact | source |
|---|---|
| `/arch:AVX2` alone: **0 of 32 twin-dump scenarios differ** vs the MSVC baseline | `windows-avx2-ab`, toolset 14.51.36231, **two runs** (first with the phantom-row parser, replicated by the corrected parser) |
| `/fp:contract` at AVX2: **32 of 32 differ** — contraction is the bit-moving half, exactly as on GCC/Clang | same runs |
| VS2022+ `/fp:precise` does not contract by default; `/fp:contract` is the opt-in | learn.microsoft.com `/fp` + the VS2022 cppblog post (doc-verified in the platform-coverage audit) |
| The analogous change measured **−18.4 % (GCC 13.3) / −19.6 % (Clang 22.1.8)** engine Ir/sample on Linux | R-6 measurement round |
| The engine's hot loops are the same portable C++ on MSVC (zero platform conditionals in `src/`) | platform-coverage audit §1 |

**The costs, stated with equal weight:**

- A **user-visible ISA floor lands on Windows**: Intel Haswell (2013) / AMD Excavator (2015),
  failure mode `STATUS_ILLEGAL_INSTRUCTION` (0xC000001D) inside the host, undiagnosable from the
  plug-in — the same contract Linux/Intel-macOS users accepted in 0.9.5, now for the platform with
  the broadest old-hardware install base.
- The Class-A property is a **toolset-version behaviour** on a floating runner: the non-contracting
  default begins at 14.30, and `windows-latest` is unpinned. Adoption therefore *requires* the
  toolset assertion (§4) and the blocking gate (§5) — the flag without them is an unguarded claim.
- **No Windows benefit number exists or is promised.** No instruction-count instrument runs on
  Windows; every honest description says "mechanism-shared with the measured GCC/Clang result",
  never a Windows-specific figure. This is written into the draft ADR and the draft user docs.
- The UCRT already dispatches transcendentals by CPU class at runtime (policy-documented since the
  R-round); `/arch:AVX2` neither worsens nor fixes that, and the ADR says so to prevent the two
  being conflated later.

**If declined:** delete this packet, add one line to ADR-0031 option 5 recording that the evidence
was gathered and the extension declined on <date> (so the next person doesn't re-run the
experiment), and optionally demote `windows-avx2-ab` to a JUCE-bump-only job or delete it.

---

## 1. Draft ADR-0032 (verbatim, ready for `docs/architecture/design-decisions/ADR-0032-msvc-avx2-baseline.md`)

> Registering this file and its `ADR_INDEX.md` row is itself the approval act — do not commit it
> unapproved. Status starts **Proposed** and flips to Accepted with the review sign-off.

```markdown
# ADR-0032 — Extend the x86-64 ISA baseline to Windows/MSVC: /arch:AVX2

**Status:** Proposed (Build System change — Architecture Review Gate; supersedes the deferral
recorded as ADR-0031 option 5, which required exactly the evidence this ADR now cites).

## Context

ADR-0031 gave the GCC/Clang x86-64 builds -march=haswell -ffp-contract=off — measured Class A —
and deferred MSVC on evidence grounds: /arch:AVX2 is not the equivalent pair (MSVC controls
contraction separately) and no bit-exactness instrument ran on Windows, so the Class-A property
could be assumed but not demonstrated. The platform-coverage audit's R-round built that
instrument: the `windows-avx2-ab` job builds the committed AnamorphDspDump three ways on
windows-latest and diffs the 32 scenario hashes as a same-machine A/B — the scope
COMPATIBILITY_POLICY gives every bit-identity claim, doubly necessary on Windows where the UCRT
dispatches transcendentals by CPU class at process start.

## Problem

Take the AVX2 vectorization win on Windows without moving an output bit, without an unguarded
toolset-dependent claim, and without an undocumented ISA floor reaching users.

## Evidence

Measured on MSVC toolset 14.51.36231 (windows-latest), two runs (the first run's phantom
33rd-row parser artifact found, fixed, and the corrected parser replicating the verdict):

- A (defaults: SSE2, /fp:precise) vs B (/arch:AVX2): IDENTICAL — 0 of 32 scenarios differ.
  The vectorizer performed no bit-moving reassociation for this instrument's coverage.
- B vs C (/arch:AVX2 /fp:contract): 32 of 32 scenarios differ — contraction is the bit-moving
  mechanism, the same split A7-5E measured on GCC/Clang (0/32 for the flag pair; 32/32 with
  contraction live).
- Microsoft documents /fp:precise as non-contracting by default since VS2022 (17.0 / toolset
  14.30), with /fp:contract the explicit opt-in.

## Options considered

1. Keep the deferral. Rejected by this ADR: the deferral's own stated blocker is discharged.
2. **/arch:AVX2 at default /fp:precise — ADOPTED.** Class A demonstrated above; the Windows
   analogue of ADR-0031 option 2, with the default-not-pin weakness closed by the toolset
   assertion and the promoted gate below.
3. /arch:AVX2 /fp:contract — rejected: Class B on every scenario, and it would break the
   cross-platform symmetry of the contraction stance for no demonstrated gain.
4. Runtime dispatch — rejected for the same reasons as ADR-0031 option 6.

## Decision

- CMakeLists' AnamorphHardening if(MSVC) branch adds `/arch:AVX2`, guarded by the SAME
  ANAMORPH_X86_ISA_BASELINE option that governs the GCC/Clang pair (the option stops being inert
  on MSVC, giving Windows the same one-switch baseline-comparison path).
- No /fp flag is added: the non-contracting default IS the decision, and the two guards below are
  what make relying on a default sound. Adding /fp:contract anywhere is a new ADR.
- The windows job's toolset step additionally asserts minor >= 30 within the 14.x series — the
  boundary at which /fp:precise stopped contracting. A pre-14.30 toolset now fails the job
  instead of silently changing the numerics contract.
- windows-avx2-ab is promoted from reporting-only to a BLOCKING gate asserting A == B (0/32) on
  every push, because both properties this ADR rests on are toolset behaviours and the toolset
  floats. B vs C stays reporting-only: 32/32 is an observation, not a contract.
- The Windows ISA floor (Haswell/Excavator, STATUS_ILLEGAL_INSTRUCTION failure mode) is
  documented in COMPATIBILITY_POLICY / COMPATIBILITY_MATRIX / KNOWN_ISSUES (KI-026 extended) and
  both user guides BEFORE the flag lands, per the ADR-0031 ordering rule.

## Consequences

- Windows users need Haswell (2013) / Excavator (2015) or newer; below it the host crashes at
  plug-in load with no diagnosable message. This is the same floor, failure mode, and
  documentation treatment as ADR-0031's.
- The benefit on Windows is MECHANISM-SHARED, not measured: no Windows instruction-count
  instrument exists, and no Windows-specific figure may be quoted. The measured figures are
  Linux: −18.4 % (GCC 13.3) / −19.6 % (Clang 22.1.8) engine Ir/sample for the analogous change.
- The UCRT's runtime FMA3 transcendental dispatch (machine-class-dependent output, documented in
  COMPATIBILITY_POLICY) is unchanged by this ADR and unrelated to /arch.
- clang-cl remains outside the validated set (COMPATIBILITY_MATRIX toolchain section); ARM64 and
  the Linux/Windows arm64 Not Supported rows are untouched.

## Related code

CMakeLists.txt (AnamorphHardening MSVC branch), .github/workflows/build.yml (windows toolset
assertion; windows-avx2-ab gate), docs/policies/COMPATIBILITY_POLICY.md,
docs/architecture/COMPATIBILITY_MATRIX.md, docs/KNOWN_ISSUES.md (KI-026), docs/user/*,
worklogs/performance/PERF_AUDIT_PLATFORM_COVERAGE.md (§5, §R-results),
worklogs/performance/MSVC_AVX2_ADOPTION_PACKET.md (this decision's preparation).

## Evidence + confidence

Verified (measured): the A/B/C table above, two CI runs. Verified (docs): the /fp semantics
change at VS2022. Not measured, stated as such: any Windows performance figure. To confirm on
first post-adoption CI: the blocking gate goes green with the flag live (the gate then compares
the flagged build against the SSE2 baseline build, both freshly compiled in-job).
```

## 2. `docs/policies/COMPATIBILITY_POLICY.md` — exact edit

Section "Runtime compatibility: the x86-64 ISA floor", the Windows table row. **Current text**
says: no ISA floor beyond the MSVC default, unchanged by ADR-0031, extending needs its own
measurement. **Replace with:**

> | **Windows** | **The same floor, since ADR-0032.** The MSVC build compiles `/arch:AVX2` at the
> default (non-contracting) `/fp:precise`, so Haswell/Excavator is the floor on Windows too; the
> failure mode is `STATUS_ILLEGAL_INSTRUCTION` (0xC000001D) raised inside the host at plug-in
> load. The Class-A property (0/32 twin-dump scenarios moved by the flag) is toolset-dependent and
> is therefore **asserted on every push**: the `windows` job requires toolset ≥ 14.30 and the
> `windows-avx2-ab` gate blocks on A == B. No Windows performance figure exists; the benefit is
> mechanism-shared with the measured GCC/Clang result. |

Also: the first paragraph's "on the x86-64 binaries produced by GCC and Clang" widens to name all
three toolchains, and the "Who is actually exposed" row gains a Windows sentence (Windows 10+ x64
machines older than 2013/2015). **Ordering rule: this lands in the same commit as, or before, the
flag — never after.**

## 3. `docs/architecture/COMPATIBILITY_MATRIX.md` — exact edits

- Platform row "Windows x86-64": replace "**no ISA floor** — the MSVC build carries no `/arch:`
  flag and is outside ADR-0031's scope" with "above the **declared ISA floor** (Haswell 2013 /
  Excavator 2015 — ADR-0032), asserted per push by the toolset gate and the blocking A/B".
- Section "CPU instruction-set floor (x86-64)": rewrite the GCC/Clang-only scoping to cover all
  three toolchains; drop "The **Windows** build … carr[ies] no such requirement" (arm64 keeps its
  no-floor sentence).
- Section "Toolchains the ISA baseline is (and is not) validated for": MSVC moves from
  "excluded by recorded decision (option 5)" to "validated by ADR-0032's blocking A/B on the
  asserted ≥ 14.30 toolset"; the clang-cl paragraph is UNCHANGED (still a third case).

## 4. `.github/workflows/build.yml` — the toolset assertion, exact change

In the `windows` job's "Record + assert the MSVC toolset" step, after the existing series gate
(`if ($series -ne '14') { … exit 1 }`), add:

```powershell
$minor = [int]($version.Split('.')[1])
if ($minor -lt 30) {
  Write-Host "::error::MSVC toolset $version predates 14.30 (VS2022), where /fp:precise stopped emitting floating-point contractions by default -- the property ADR-0032's /arch:AVX2 Class-A claim rests on. A pre-14.30 image silently changes the numerics contract; do not proceed on it."
  exit 1
}
```

with a comment block noting the boundary source (the `/fp` doc + VS2022 blog, as cited in the
audit) and that the step stops being warning-only for THIS half (an unreadable version must also
become an error once the flag is live, since an unplaceable toolset can no longer be a footnote —
change the `exit 0` in the unreadable-version branch to `exit 1` in the same edit).

## 5. `.github/workflows/build.yml` — promoting `windows-avx2-ab` to a blocking gate, exact change

- Remove `continue-on-error: true` from the step.
- Job comment: replace the "REPORTING ONLY" paragraph with the gate rationale (the property is
  claimed by ADR-0032, so failing on it is enforcing a contract, not inventing one).
- Script changes: every `exit 0` on a build/self-check/run/parse failure becomes `exit 1`; the
  A-vs-B comparison exits 1 when any scenario differs (message naming the differing scenarios and
  ADR-0032); B-vs-C stays informational (no exit-code effect); the toolset-unreadable branch
  exits 1. The C build can optionally be dropped from the blocking path to halve gate cost —
  recommended: keep A and B blocking, run C only on workflow_dispatch.
- Note: after adoption the shipped `windows` build carries `/arch:AVX2` via AnamorphHardening,
  while this gate's A build passes `-DANAMORPH_X86_ISA_BASELINE=OFF` (which §6 makes meaningful on
  MSVC) — the A/B remains baseline-vs-flag by construction, not baseline-vs-shipped-by-accident.

## 6. `CMakeLists.txt` — exact change

In the `if(MSVC)` branch of `AnamorphHardening` (currently `/guard:cf` + Release `/Zi`):

```cmake
    if(ANAMORPH_X86_ISA_BASELINE)
        # ADR-0032: the Windows half of the x86-64 ISA baseline. /arch:AVX2 at
        # the DEFAULT /fp:precise, which VS2022+ documents as non-contracting --
        # measured 0/32 twin-dump scenarios moved (windows-avx2-ab, toolset
        # 14.51). No /fp flag is added: the default IS the decision, guarded by
        # the windows job's >=14.30 toolset assertion and the blocking A/B gate.
        target_compile_options(AnamorphHardening INTERFACE /arch:AVX2)
    endif()
```

plus: the option's comment block (lines ~41-53) drops "under MSVC the option changes nothing";
the scope comment's MSVC bullet (~167) is rewritten from "nothing is added" to the ADR-0032
state; `docs/procedures/BUILD.md`'s ISA section gains the Windows paragraph and loses the
"inert on MSVC" clause.

## 7. `docs/KNOWN_ISSUES.md` — exact edit

KI-026 (title, table row, and body) widens from "the Linux binaries and the macOS x86_64 slice"
to include Windows; "Windows and Apple Silicon are unaffected" becomes "Apple Silicon is
unaffected"; the body's "Who is affected" gains: on Windows, any x64 machine with a pre-2013
Intel / pre-2015 AMD CPU, and the failure surfaces as a host crash at plug-in load
(`STATUS_ILLEGAL_INSTRUCTION`). Evidence row gains ADR-0032.

## 8. `docs/user/USER_MANUAL.md` + `docs/user/INSTALLATION.md` — exact edits

Both currently carve Windows out ("Apple Silicon Macs and the Windows build are unaffected").
Replace with: the processor requirement covers **Windows, Linux, and the Intel half of the macOS
build**; only Apple Silicon Macs carry no requirement. No performance number is quoted for
Windows in either document.

## 9. `CHANGELOG.md` — draft entry (user-visible: requirement + speed)

One **Changed** entry under the release that ships it, mirroring the 0.9.5 AVX2 entry's structure:
Windows now requires Haswell/Excavator or newer; the sound is unchanged (bit-identical, verified
per push by a blocking A/B); the speed benefit is described as the same mechanism measured on the
other platforms, with no Windows-specific number. Evidence: the adopting PR + ADR-0032.

## 10. Lifecycle syncs

`ADR_INDEX.md` row (Proposed → Accepted at sign-off) · `DOCUMENTATION_COVERAGE.md` section ·
`HANDOVER.md` current-version row sentence · platform-coverage worklog + report page: flip the
"open maintainer decisions" register entry to adopted · re-run `check-citations --fix` in the same
commit (the CMakeLists and build.yml edits shift cited lines — the split-push lesson is now
twice-learned).

## The checklist the maintainer signs

- [ ] Approve ADR-0032 (or decline — see §0's decline path)
- [ ] Land §§2/3/7/8 (documentation) in the same commit as or before §§4/5/6 (flags + gates)
- [ ] Confirm the first post-adoption CI run: toolset gate green at ≥ 14.30, blocking A/B green at 0/32
- [ ] CHANGELOG + lifecycle syncs (§§9/10)
