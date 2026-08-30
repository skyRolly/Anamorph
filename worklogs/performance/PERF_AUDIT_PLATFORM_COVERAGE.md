# Platform coverage audit — the A7 programme and every prior optimization round

**Date:** 2026-08-22 · **Branch:** `claude/anamorph-ci-workflow-8iu7yk` (PR #131) · **Task:** determine,
with evidence, which platforms and toolchains actually receive each performance optimization; classify
every gap as intentional or accidental; evaluate each uncovered platform. **No build flag, platform
policy, or DSP change is made by this round** — this document is the deliverable, and its
recommendations are inputs to maintainer decisions, not decisions.

**Method.** Thirteen independent evidence/verification passes over `CMakeLists.txt`, all five workflow
files, all of `src/`, the JUCE checkout, the worklogs, and — for the MSVC questions — Microsoft's
documentation fetched live (`learn.microsoft.com`, `devblogs.microsoft.com`). Seven load-bearing claims
were then adversarially re-verified against the tree: five confirmed, **two refuted** — and both
refutations are themselves findings (§8). Empirical anchor: the all-green CI run on `b63049c`
(the first run with the ADR-0031 flags live), including the `macos-crossslice` log showing the AVX2
probe *executing* under Rosetta 2 and the A7-5E result unchanged at the new flags (32/32 differ at
shipped flags; 24/32 with contraction off, the 8 oversampling-×1 scenarios agreeing).

---

## 1. The headline, first

**Exactly one optimization this repository *applies* is platform-conditional: ADR-0031's
`-march=haswell -ffp-contract=off`.** (One compiler-*emergent* performance property — implicit FMA
contraction — is additionally platform-conditional, and the matrix's bottom row tracks it.) Everything else — A7-2B, A7-9, and every named item from Waves
1–6 — is a single portable C++ implementation with **zero** platform, compiler, or ISA conditionals,
compiled identically by GCC, Clang, AppleClang and MSVC. An exhaustive grep of `src/` for
preprocessor/ISA conditionals (`#if*`, `JUCE_WINDOWS/MAC/LINUX/ARM`, `__SSE*`, `__AVX*`, `__FMA*`,
NEON, `__aarch64*`, `_M_X64`, `_M_ARM64`, `_mm_*`, `vld1/vfma`, non-`once` pragmas) returns **seven
hits in two files, none of them in any DSP optimization**: the Clang-attribute feature-test block in
`RealtimeAnnotations.h` (two feature-test lines plus the fallback-define guard; diagnostics-only), and
four in `PluginEditor.cpp` — two version-macro fallbacks, one `JUCE_MAC` tooltip cosmetic, and, at
:307, the ADR-0011 OpenGL-attach gate (`#if !(JUCE_LINUX || JUCE_BSD)`), the one deliberate GUI
backend asymmetry, governed by an Accepted ADR.

So the platform-coverage question decomposes cleanly into three separable questions, and conflating
them is how coverage *looks* uneven:

1. **Is the optimization present in the shipped code for that platform?** (Uniform for everything
   except ADR-0031.)
2. **Was its benefit ever measured on that platform?** (Almost never — §3.)
3. **Is its behaviour validated on that platform?** (Broadly yes — §3.)
4. **Is the *recorded mechanism* accurate on that platform?** (Mostly — the exceptions are F-1/F-2,
   and they are comment defects, not code defects.)

## 2. Coverage matrix — optimization × platform

Legend: ✅ present · ⛔ deliberately absent (recorded decision, citation given) · ▢ not a supported
target (no builds exist) · n/a not applicable.

| Optimization | Linux x86-64 | Windows x86-64 (MSVC) | macOS x86_64 | macOS arm64 | Linux arm64 / Win arm64 |
|---|---|---|---|---|---|
| **A7-2B** ring gather (+ A7-2T Test 40, A7-1 superseded) | ✅ | ✅ | ✅ | ✅ | ▢ |
| **A7-9** fixpoint gates (Velvet/Haas/Chorus) | ✅ | ✅ | ✅ | ✅ | ▢ |
| **A7-5 / ADR-0031** `-march=haswell -ffp-contract=off` | ✅ GCC+Clang | ⛔ ADR-0031 option 5 | ✅ via `-Xarch_x86_64` | ⛔ ADR-0031 option 4 | ▢ |
| Wave 1–2: H1, H3 tanh kernel, H4, H5/ALG-4, H6, H8, H9, H10, H11, H12, H15 | ✅ | ✅ | ✅ | ✅ | ▢ |
| Wave 3: shared LR4 coefficients, allpass telescope | ✅ | ✅ | ✅ | ✅ | ▢ |
| Wave 4–5: parked-path family (H12 is Wave 1; Haas is Wave 4; Velvet is Wave 5), NaN detector, segmented scope/bypass ring fills, meter-publish caching, Level-Match memoisation, `sameParameters`, W5-C hoist | ✅ | ✅ | ✅ | ✅ | ▢ |
| GUI: S1/S2/S3 gates, H2/H13/N2 static-layer caches, H15, H17, `ignoreNegativeFreqs`, Wave-4 LevelMeter cache + hidden-Vectorscope gate + `Path` reuse, Wave-5 editor memoisations, Wave 6 trim, FrameClock, 0.8.8 `pushBlock` | ✅ (on the ADR-0011 CPU backend) | ✅ (GL backend) | ✅ (GL) | ✅ (GL) | ▢ |
| Implicit: FMA contraction of DSP expression chains | ⛔ pinned off (that is what makes ADR-0031 Class A) | ⛔ off by MSVC default (VS2022 `/fp:precise`, doc-verified) | ⛔ pinned off (`-Xarch_x86_64`) | ✅ **free, by base-ISA FMLA** — no flag involved | ▢ |

The row enumerations are representative supersets of the headline items, not a re-derived registry;
the **exhaustiveness proof is §1's zero-conditional grep** — whatever a row omits by name is still
covered by “no source file selects code by ISA, vendor, OS, or word size.”

Two rows deserve the emphasis:

- **The bottom row is the one place arm64 is *ahead*.** By every indicator on record the shipped
  arm64 slice contracts `a*b+c` into `FMLA` at AppleClang's default: a Clang-18 cross-compile census
  counted **308 FMA instructions across the 8 DSP TUs on aarch64 against 0 on x86-64** (both before
  and after ADR-0031 — the x86 flag pins what the missing instruction previously enforced by
  accident), and the AppleClang cross-slice runs confirm slice-level numerical divergence. The
  worklog that produced the census brands it an inference about the shipped slice rather than a
  measurement of it; per-expression fusion in the shipped binary has not been disassembled (§10).
  Either way, arm64 has the fused-codegen benefit x86-64 deliberately forgoes for bit-identity.
- **The GUI row's platform difference is the backend, not the optimization.** Same paint code
  everywhere; Linux/BSD render it on the CPU by ADR-0011 (Accepted; host-side `XEmbedComponent`
  use-after-free). The Wave-6 transparency-layer trim therefore shrinks a GL FBO on macOS/Windows and
  a software image on Linux — same source, platform-asymmetric effect, both beneficial.

## 3. Coverage matrix — measured vs validated (the part that IS uneven)

| Platform / toolchain | Optimizations present | Benefit measured here? | Behaviour validated here? |
|---|---|---|---|
| Linux x86-64, **GCC 13.3** (this container) | all | **YES — every headline figure**: A7-1 −14.3/−32.3 %, A7-2B −12.2/−37.2 %, A7-9's Ir/block recoveries, AVX2 −17.2 %; callgrind Ir with startup subtraction; "Intel Xeon @ 2.80 GHz, 4 cores, gcc 13.3.0, shared container" | locally: suites, twin dump, Test 41 fired pre-fix |
| Linux x86-64, **Clang 22** (the *shipped* artifact) | all | **NO** — no headline Ir figure was ever produced from a Clang-22-built binary | fully: suites (`MALLOC_PERTURB_`), ASan/UBSan+vptr, RTSan, fuzz, pluginval ×3 ×2 modes on the stripped bytes, ABI floor |
| Linux x86-64, **GCC 16** (compat gate) | all | no | suites under LTO, dump `--self-check`, bench smoke |
| Linux x86-64, distro GCC (valgrind step, CodeQL — unpinned) | all | no | valgrind memcheck over both suites (`ANAMORPH_TESTS_NO_FTZ=1`); CodeQL is compile-only |
| **Windows x86-64, MSVC** | all except ADR-0031 | **NO — nothing was ever measured on Windows** | suites (both exes, fail-closed discovery), pluginval ×3 ×2 modes; **no bit-exactness instrument**: no CI job builds `AnamorphDspDump` on Windows |
| **macOS x86_64**, AppleClang (universal slice; `macos-intel` thin deliberately carries a second, older AppleClang generation — both unpinned, from each image's Xcode) | all incl. ADR-0031 | no | suites under Rosetta 2 (AVX2-probed, warning-degradable) **and** on native Intel silicon (blocking, asserted `proc_translated == 0`); pluginval VST3+AU ×3 ×2 on native Intel; dump runs in `macos-crossslice` |
| **macOS arm64**, AppleClang (unpinned, image Xcode) | all; FMLA contraction free | no (no callgrind on macOS; A7-0's named-machine blocker applies) | suites native, pluginval VST3+AU ×3 ×2, dump `--self-check` + cross-slice comparison |
| clang-cl | — | — | **never exercised** — no CI job, no local configuration uses it |

The unevenness the task suspected is real, and it lives in the **measurement** column, not the
**presence** column: every benefit figure in the repository comes from one shared Linux x86-64
container with GCC-13-built binaries, while the shipped Linux binary is Clang 22 and the other
platforms have no instruction-count instrument at all. That is the already-recorded **A7-0 /
RISK-002** blocker (`PERFORMANCE_BUDGET.md` rows deliberately empty; wall-clock repudiated — 292 ms
vs 196 ms for the same binary and workload in consecutive rounds), not a new gap. The AVX2 change is
the least exposed to it: its Class-A property means the *behavioural* claim transfers to every
GCC/Clang x86-64 build bit-for-bit, and GCC-haswell vs Clang-haswell were measured bit-identical.

## 4. Intentional or accidental? Every gap classified

| Gap | Classification | Record |
|---|---|---|
| ADR-0031 flags absent on **MSVC** | **Intentional** | ADR-0031 option 5 ("on evidence grounds rather than on merit"); `CMakeLists.txt` comment at the MSVC exclusion; `COMPATIBILITY_MATRIX` Windows row ("no ISA floor… outside ADR-0031's scope") |
| ADR-0031 flags absent on **arm64** | **Intentional** | ADR-0031 option 4; `COMPATIBILITY_POLICY` adds a standing prohibition: the cross-slice difference "is accepted and is not to be removed" |
| `ANAMORPH_X86_ISA_BASELINE` inert on MSVC (OFF changes nothing, warns nothing there) | **Structural** (no recorded decision — the option exists for GCC/Clang A/B re-verification and the ISA block simply lives in the other branch); worth knowing before anyone builds a Windows A/B on it — §5 |
| **clang-cl** would get no ISA flags (takes the `if(MSVC)` branch) | **Structural** (no recorded decision; falls out of `if(MSVC)`); clang-cl is unused everywhere. Boundary condition worth recording: GNU-driver clang targeting the MSVC ABI (`clang++ --target=x86_64-pc-windows-msvc`) sets `MSVC=false` and **would** take the GCC/Clang branch and receive `-march=haswell` — an untested combination no toolchain currently exercises (F-6) |
| No benefit figures off the one Linux container | **Known and recorded** (A7-0 / RISK-002), not new |
| Headline Ir figures measured only on **GCC-13-built** binaries while the shipped Linux binary is **Clang 22** | **Accidental — and the one measurement gap closable without a named machine**: callgrind runs on this container regardless of which compiler built the binary; the named-machine blocker applies to wall-clock and to other OSes, not to this. R-6 |
| No bit-exactness instrument on Windows | **Accidental/unrecorded** — a pre-existing absence that ADR-0031 option 5 *relies on* as evidence but nowhere *decides*; now the load-bearing blocker for any MSVC extension, and cheap to remove (§5, R-1) |
| `juceaide` and the VST3 manifest helper compile outside `AnamorphHardening` | Accidental-but-harmless: configure-time host tooling, never shipped, compiles no `src/` code (V1's refutation, §8 F-5) |
| **Linux arm64 / Windows arm64**: no builds | Out of scope **by omission, not by recorded decision** — no `COMPATIBILITY_MATRIX` row says "Not Supported" the way AAX and mono→mono are recorded as deliberate exclusions (§7, R-3) |
| Linux CI jobs run AVX2 binaries with no capability probe (unlike both Rosetta steps) | Accepted residual: GitHub-hosted x86-64 runners are Haswell+; empirically green. A self-hosted pre-Haswell runner would SIGILL — noted, no action (F-7) |

## 5. Special focus: Windows MSVC x86-64 and `/arch:AVX2`

**What MSVC builds at today:** `/arch` default (SSE2 baseline — the same frozen floor x86-64 GCC/Clang
had before ADR-0031) and default `/fp:precise`; the only non-default flags are ADR-0021's hardening
set (`/guard:cf`, Release `/Zi`, `/DYNAMICBASE /NXCOMPAT /DEBUG /OPT:REF /OPT:ICF`) plus JUCE's
`/Ox` and LTCG (`/MP` too, but that parallelizes the build and touches no codegen). No `/arch:` or `/fp:` flag exists anywhere in the build files (repo-wide grep: the
only `/arch:` mention in a build file is the CMakeLists comment saying it is *not* the equivalent pair).

**The task said: do not assume `/arch:AVX2` is correct. Verified, against live Microsoft sources:**

1. **What `/arch:AVX2` does** *(doc-verified: the ISA set)*: enables AVX2 + **FMA** + some BMI — a
   Haswell-congruent floor, so the user-visible ISA-floor consequence would match the GCC/Clang side
   exactly, including the failure mode: `STATUS_ILLEGAL_INSTRUCTION` (0xC000001D) raised inside the
   host process. The reachability from loader-run static initialisers at `LoadLibrary` time is this
   repository's own KI-026/ADR-0031 analysis applied to the PE loader — inference, consistent with
   the documented mechanism, not doc content.
2. **Contraction is controlled separately, and the default changed** *(doc-verified, two sources)*:
   since Visual Studio 2022 (17.0), "floating-point contractions aren't generated by default under
   `/fp:precise`… Previous compiler versions could generate contractions by default"; the opt-in is
   the new `/fp:contract` switch. On any VS2022+ toolset (14.3x/14.4x, 14.5x if the image moves to
   VS2026), **`/arch:AVX2` alone is therefore *expected* to preserve scalar bit-exactness** — the
   analogue of `-march=haswell` with contraction off **by current default rather than by pinned
   flag**: a materially weaker guarantee than the GCC/Clang pair, whose own CMake comment exists
   precisely because defaults are not pins. That is why R-1 requires a recorded contraction stance.
   *(Toolset↔version mapping beyond the doc-verified ABI-series table is consistent training memory,
   labelled as such. The windows job does not pin a toolset: it records the full version per run —
   which is what places a run relative to the 17.0 boundary, after the fact — and gates only the
   14.x ABI series, a gate that would alone still admit pre-17.0 14.1x/14.2x toolsets. That
   `windows-latest` currently resolves only to 14.3x+ is a property of the floating runner image,
   not asserted anywhere; an R-1 instrument step should assert toolset ≥ 14.30 itself.)*
3. **What could still legally change** *(the honest residual)*: the auto-vectorizer gaining 256-bit
   lanes. Per-lane elementwise promotion is bit-preserving; loop *reductions* need reassociation,
   which the `/fp:precise` doc text forbids unless bitwise-identical — but whether MSVC's vectorizer
   holds that line for every loop shape in this engine is **exactly the question only a twin dump can
   answer**, which is ADR-0031 option 5's stated reason for requiring measurement. *(Unverified
   beyond doc text; do not assume.)*
4. **A Windows validation instrument is feasible and cheap** *(verified)*: `tests/dsp_dump.cpp` is
   fully platform-neutral — no ifdefs, no threads, no clock, no environment reads; fixed-seed LCG
   stimulus, FNV-1a over raw output bytes, `printf` only; the target deliberately excludes LTO. A
   Windows CI step can configure twice (baseline vs `-DCMAKE_CXX_FLAGS="/arch:AVX2"` — necessary
   because `ANAMORPH_X86_ISA_BASELINE` is inert on MSVC) and diff the 32 hashes as a **same-machine
   A/B**, which is precisely the scope `COMPATIBILITY_POLICY` gives bit-identity claims. No
   cross-platform identity is required or implied.
5. **A Windows-specific caveat the x86-64 GCC/Clang side does not have** *(doc-verified)*: the x64
   UCRT **dispatches its transcendental implementations at process start by CPU capability** (FMA3
   vs not), independent of compile flags — "slight differences in the result… may be observable…
   between computers that do or don't support FMA3." The engine calls CRT transcendentals at runtime
   (the per-block H11 `sin`/`cos` seed; the oversampling coefficient derivation at `prepare()`), so
   **Windows engine output is already machine-class-dependent today**, before any flag change. Two
   consequences: (a) a same-machine A/B is the *only* honest instrument shape on Windows; (b)
   `COMPATIBILITY_POLICY`'s "within the same architecture and build configuration" phrasing has a
   Windows-shaped nuance worth a sentence next time that document is open (R-5).
6. **Is the benefit likely meaningful?** Plausibly comparable in direction to the −17.2 % measured on
   GCC/Clang — the hot loops are the same shapes — but **no instrument can currently quantify it**:
   callgrind does not exist on Windows, wall-clock is repudiated project-wide, and A7-0's named-machine
   blocker applies with extra force. The honest statement is: benefit unquantified, mechanism shared.

**Recommendation R-1 — defer the flag; build the instrument first.** The extension is *plausibly*
Class A on VS2022 toolsets, but ADR-0031 option 5's logic still holds: it could only be assumed, not
demonstrated. The cheap, decision-free first move is a **reporting-only Windows twin-dump A/B step**
(the `macos-crossslice` pattern: build the dump at defaults and at `/arch:AVX2`, diff, print, exit 0).
That converts "could not be demonstrated" into data for the price of one CI step and no product
change. Adopting the flag afterwards would need: its own ADR (superseding option 5), the ISA floor
extended to Windows in `COMPATIBILITY_POLICY` / `COMPATIBILITY_MATRIX` / `KNOWN_ISSUES` (KI-026) /
both user guides **before** the flag lands (the ADR-0031 ordering rule), a recorded contraction
stance with a toolset-≥ 14.30 assertion in the job (the default is a default, not a pin), and — the
**recurring** maintenance cost, stated so a maintainer accepts it with eyes open — the twin-dump A/B
promoted from one-time demonstration to a **permanent blocking step of the windows job**: the
toolset floats, and both properties the Class-A claim rests on (the contraction-off default, the
vectorizer's reduction discipline) are toolset-version behaviours that can drift with the runner
image. Until the maintainer wants that whole chain, **Windows stays correctly excluded** — the
exclusion of the *flag* is documented in four places and is not an accident (the absence of the
*instrument* is the accidental half — §4).

## 6. Special focus: ARM64

**Does arm64 already receive equivalent compiler optimizations? Yes — and more directly than x86-64
does.** Three mechanisms, none needing action:

1. **FMA contraction is live on arm64 today**, by base-ISA `FMLA` at AppleClang's default
   contraction setting — the codegen benefit ADR-0031 deliberately declined on x86-64 to stay Class A.
   Census: 308 FMA instructions in the DSP TUs (aarch64) vs 0 (x86-64, both flag states).
2. **Vectorization needs no `-march`**: NEON (128-bit) is unconditional base ISA. There is no 256-bit
   NEON and no SVE on Apple consumer silicon, so **AVX2's width win has no arm64 analogue** — the
   asymmetry is architectural, not a coverage hole. The repo contains no first-party NEON/SVE code;
   JUCE's own NEON SIMD ops compile in automatically.
3. **The one candidate "equalizing" action is affirmatively forbidden**: applying `-ffp-contract=off`
   to arm64 was ADR-0031 option 4, rejected — a Class-B change to shipped arm64 numerics for no
   user-visible gain, which would close only the contraction half of the cross-slice difference
   (24/32 scenarios differ via libm oversampling coefficients even with contraction off everywhere).
   `COMPATIBILITY_POLICY` records the difference as "accepted and… not to be removed."

**A7-9 on arm64 — the mechanism differs, the fix holds** (this is verifier V4's refutation, F-1):
under contraction the glide decrement is fused, so the x86 stall-at-`FLT_MIN/k` never forms; measured
through a faithful FMA analogue (on x86, not disassembled from the shipped slice — §10), the
contracted+flush glide walks to an **exact 0** instead. At that terminal state the old value test and
the new fixpoint test first hold together — thousands of samples after the point where the x86 build
parks at its stall — and the fixpoint gate parks correctly regardless; its second disjunct covers the
exact-zero state. The fix is correct on every platform; what is x86-specific is the *comments'*
account of the mechanism (R-4).

**Recommendation R-2 — no action on macOS arm64**, and record (this document) that the absence of
flags there is a decision with a paper trail, not a gap. Benefit measurement on arm64 stays blocked
on the same A7-0 instrument problem (no callgrind; Instruments needs a human and a named machine).

## 7. Linux arm64, Windows arm64, and "other supported targets"

There are none. Verified across `COMPATIBILITY_MATRIX` (platform rows), every workflow `runs-on`,
`release.yml`'s artifact list, and the packaging texts ("built by CI for x86_64 Linux"; "The CI build
is x64-only"): no arm64 Linux/Windows build exists, ships, or is promised. The only Linux-aarch64
execution on record is the A7-5E qemu-user *experiment* — an instrument run inside a worklog, not a
target. The near-miss: ADR-0028 notes apt.llvm.org publishes arm64 packages "so a future
ubuntu-24.04-arm Clang job could install it" — an explicit hypothetical about a non-shipping detector
job.

**Recommendation R-3:** if the maintainer wants these omissions to become decisions, add **Not
Supported** rows to `COMPATIBILITY_MATRIX` (the taxonomy already exists for AAX and mono→mono:
"a deliberate exclusion… not 'unverified'"). Documentation-only, deferred to the maintainer; until
then the honest status is *unsupported by omission*. Should a Linux arm64 target ever be added, the
ADR-0031 guards already behave correctly: `CMAKE_SYSTEM_PROCESSOR` fails the x86-64 regex and no
x86 flags are applied — the build would succeed at AArch64 base ISA with contraction live, i.e. the
macOS arm64 posture.

## 8. Findings (things this audit established that the tree does not yet say)

- **F-1 — the A7-9 stall has three platform-dependent terminal states, and the shipped comments
  describe only one.** Measured: x86-64 shipped (no contraction, FTZ) stalls at ~`FLT_MIN/k`
  (1.17e-35 for Haas — what the comments say); FTZ-off (valgrind; any no-FTZ platform) stalls at a
  **subnormal ~7e-43** — the decrement rounds to zero long before `a` does; contracted+flush
  (arm64-class builds — verified through an FMA analogue on x86, *inferred* for the shipped slice)
  walks to an **exact 0**. The fixpoint gate
  parks correctly in all four measured configurations — the *fix* is architecture-neutral; the
  *comments* in the three DSP files are accurate only for the ADR-0031 x86-64 baseline.
- **F-2 — Test 41's no-FTZ comment states a measured falsehood.** It claims that under
  `ANAMORPH_TESTS_NO_FTZ` "the glide walks down through the denormals to a true zero… so all three
  checks pass without discriminating." Measured: it stalls at the ~7e-43 subnormal, not zero — and
  the exact-zero check would therefore *still fire* against pre-A7-9 code without FTZ. The comment is
  wrong in a conservative direction (the test is stronger than it claims), but wrong.
- **F-3 — Windows bit-identity has a pre-existing machine-class dependence** via the UCRT's runtime
  FMA3 dispatch of transcendentals (§5.5) — true before ADR-0031, unaffected by it, and a nuance the
  compatibility documentation does not yet carry.
- **F-4 — a theoretical gate/loop contraction-mismatch exists on contracting platforms, in FOUR
  places, and one of them is not silence-confined.** A gate's block-start expression (`aNext`,
  `wNext`, `dNext`) and the loop's per-sample update are separate expressions; identical contraction
  of both is compiler practice, not standard-guaranteed. For the three **amount** gates (target
  pinned at 0) a mismatch either parks at the gate's fixpoint or fails to park and reproduces the
  pre-A7-9 stalled residual — both confined to the documented silence-residual class. The fourth
  instance, `VelvetNoise`'s `densityAtFixpoint` gate on the H5 gather path, is the one whose
  hypothetical mismatch lands on **engaged real signal**: the gather requires amount engaged, the
  density target is an arbitrary normal-range value (no FTZ argument applies), and `updateWeights()`
  runs per-sample only in the fallback loop — so a gather admitted while the loop's density tick
  could still move would run with stale weights. **Test 40 (gather == per-sample loop) is the
  standing guard for exactly that divergence, on every platform that runs the suites.**
  Same-TU/same-flags compilation makes the whole family practice-impossible; recorded because
  "practice-impossible" and "guarded" are different strengths, and this one is both.
- **F-5 — two host tools escape `AnamorphHardening`** (`juceaide`, the VST3 manifest helper): nested
  sub-builds, never shipped, no `src/` code. No action.
- **F-6 — the GNU-driver-clang-targeting-MSVC boundary** (§4): would take the GCC/Clang branch and
  receive `-march=haswell` against the MSVC ABI — no current toolchain does this; latent, recorded.
- **F-7 — Linux CI executes AVX2 binaries unprobed** (unlike both Rosetta steps; Windows has no AVX2
  binaries to probe). Safe on GitHub-hosted runners; a pre-Haswell self-hosted runner would SIGILL in
  every job that executes engine binaries — valgrind included, whose VEX rejects instructions it
  cannot translate the same way. The one executed binary built *outside* `AnamorphHardening`, the
  realtime job's bare-`clang++` RTSan canary, would be unaffected. Recorded, no action.

## 9. Recommendations, consolidated

| # | Recommendation | Action class | When |
|---|---|---|---|
| R-1 | **MSVC `/arch:AVX2`: defer the flag; add a reporting-only Windows twin-dump A/B step first** (the `macos-crossslice` pattern). Adoption afterwards requires its own ADR + the Windows ISA floor documented before the flag (the ADR-0031 ordering rule) | CI step (reporting-only), then a maintainer decision | instrument: next CI round · flag: maintainer |
| R-2 | **arm64: no action** — contraction already live, width win architecturally absent, the equalizing flag affirmatively rejected (option 4) | none | — |
| R-3 | Record **Linux/Windows arm64 as "Not Supported"** in `COMPATIBILITY_MATRIX` if the omission should become a decision | docs-only | maintainer |
| R-3b | **clang-cl: exclude.** No build uses it; if one ever appears it is a *third* flag branch, not either existing one — it takes `if(MSVC)` yet accepts GCC-style flags via `/clang:` — and gets its own ADR then (F-6 records the GNU-driver boundary) | none now | if ever adopted |
| R-4 | Correct the **A7-9 comment inaccuracies** (F-1, F-2): one architecture-note sentence in each of the three DSP files; rewrite Test 41's no-FTZ sentence to the measured subnormal-stall mechanism | Class-A comment fix (A7-9C precedent) | next code round |
| R-5 | Add the **UCRT FMA3 dispatch caveat** (F-3) to `COMPATIBILITY_POLICY`'s Windows discussion next time that file is open | docs-only | opportunistic |
| R-6 | Benefit figures remain single-toolchain (GCC-13-built binaries on the one container). A7-0 / RISK-002 still blocks wall-clock and other OSes — but **the Clang-22 Ir gap is closable here**: one callgrind round over Clang-22-built binaries on this container, same methodology, would either confirm the −17.2 % class figure on the shipped toolchain or record why it differs. Recommend as the next measurement round's first item | measurement round | next A7 pass |

## 10. Evidence & confidence

**Verified (this round):** the complete `src/` conditional inventory (7 hits, listed); the
`AnamorphHardening` linkage of all six binary targets and the exact guard chain incl. config-scoping
(the `-march` pair carries no `$<CONFIG:…>`); the per-job CI map (compiler, flags-reach, execution
substrate) — thirteen independent compiles across twelve compiling jobs in five workflow files, the
sanitizers job compiling twice (pinned-Clang ASan/UBSan, then unpinned distro-GCC for valgrind) —
incl. the CodeQL compile; `release.yml` reusing `build.yml` via `workflow_call` (one build topology); the
MSVC `/fp` and `/arch` semantics against two live Microsoft sources; `dsp_dump.cpp` platform
neutrality; the F-1/F-2 terminal states by direct measurement of the glide map under four
FTZ×contraction configurations; the b63049c CI logs (crossslice probe executed; A7-5E counts
unchanged). **Plausible (labelled):** exact toolset↔`_MSC_VER` mapping beyond the doc-verified ABI
table; MSVC vectorizer reduction behaviour (doc text only — the open question R-1's instrument
exists to close); per-expression FMLA fusion of the three glide updates in the *shipped* arm64 slice
— every census is a cross-compile inference, and the exact-zero terminal state was measured through
an x86 FMA analogue rather than disassembled from the shipped binary. **Not measured anywhere:** any benefit figure off the Linux container; any
Windows numerical experiment.

---

# The R-round: what was executed against R-1…R-6 (2026-08-30)

The maintainer approved executing the audit's recommendations as evidence-gathering and
documentation work, with the standing decisions preserved. What landed, per item:

## R-1 — the Windows MSVC A/B instrument exists (reporting-only)

A new `windows-avx2-ab` CI job builds `AnamorphDspDump` **three** times on `windows-latest` —
**A** at MSVC defaults (`/arch` SSE2 baseline, default `/fp:precise`), **B** at `/arch:AVX2`, **C**
at `/arch:AVX2 /fp:contract` — self-checks each, and diffs the 32 scenario hashes pairwise. A==B is
the Class-A question (vectorizer + instruction selection at the wider ISA; contraction documented
off by default on VS2022 toolsets); B vs C isolates contraction alone. The job records the exact
MSVC toolset from the CMake cache and states which side of the 14.30 boundary the run sits on
rather than assuming; JUCE is fetched once and reused (the `macos-crossslice` pattern). It is
**reporting-only by construction**: `continue-on-error`, every path exits 0, it uploads nothing,
and the shipped `windows` job's flags are untouched. Promotion to a blocking gate is an explicit
future-adoption requirement, not this job's doing. **The A/B/C result is read from the CI run and
recorded in §R-results below** — this container is Linux and cannot execute MSVC; nothing here is
fabricated.

## R-2 — arm64: confirmed no action

No code or flag change. The current documentation was re-read for overstatement and none needed
correction beyond what this worklog's own §6 already carries (per-expression fusion labelled
inference; the census a cross-compile).

## R-3 — the scope is now recorded, not implied

`COMPATIBILITY_MATRIX` gains two platform rows — **Linux arm64: Not Supported** and **Windows
arm64: Not Supported** — using the deliberate-exclusion taxonomy AAX and mono→mono already use, and
a new section, *"Toolchains the ISA baseline is (and is not) validated for"*, recording that
**clang-cl is not a supported or validated ADR-0031 toolchain**, that the `if(MSVC)` branch must
not be read as evidence about it (structural, not decided), and that a future clang-cl toolchain is
a *third* case requiring its own investigation and ADR. The `CMakeLists.txt` scope comment was
brought into agreement (its "no twin-dump instrument runs on Windows" clause was about to go stale
— the instrument now exists — and it gained the clang-cl paragraph). No third compiler branch was
created; no arm64 jobs were created.

## R-4 — the F-1/F-2 comment corrections are in

All three A7-9 blocks (`VelvetNoise.cpp`, `HaasProcessor.cpp`, `ChorusEngine.cpp`) now carry the
same architecture note: the stall value each block quotes is **one configuration's** — the x86-64
baseline's (contraction pinned off, FTZ on); with FTZ off the glide fixpoints at a **~7e-43
subnormal** (the decrement underflows first); under FMA contraction (arm64-class builds, measured
through an x86 FMA analogue) it walks to an **exact 0**; the fixpoint test parks correctly at all
three terminal states, the second disjunct covering the exact-zero one. Test 41's false paragraph
is rewritten to the measured mechanism and now *names its own prior error*: the earlier claim that
without FTZ the glide "walks to a true zero" and the checks "pass without discriminating" is
recorded as false — the glide stalls at the subnormal (measured 6.99e-43 for k = 0.001), the gate
parks there, and the exact-zero check would still fire against pre-fix sources, so **no
discrimination is lost without FTZ; only the stall value moves**. No gate logic, coefficient, or
numerical behaviour changed; the suite re-ran green (214 + 920).

## R-5 — the UCRT caveat is in the policy

`COMPATIBILITY_POLICY` §"Numerical compatibility" now scopes the Windows half explicitly: the x64
UCRT selects FMA3/non-FMA3 transcendental implementations **at process start by CPU capability**
(documented Microsoft behaviour, independent of compile flags, predating ADR-0031), the engine
calls CRT transcendentals at runtime, so a Windows bit-identity claim is scoped to **the same
machine class** — an existing platform property, explicitly distinguished from the proposed
`/arch:AVX2` flag, with the same-machine A/B noted as the control. No runtime dispatch was added
and nothing about the numerical model changed.

## Review-status verification — the probe WAS wrong, and is fixed

The review finding at the `macos` Rosetta step was re-verified rather than assumed addressed, and
it was **half-unaddressed**: the probe program used `_mm256_set1_pd`/`_mm256_add_pd`, which are
**AVX1** — it proved 256-bit *float* execution, not AVX2. Both probes (the `macos` Rosetta suites
step and `macos-crossslice`) now execute genuine AVX2 (`_mm256_set1_epi32` / `_mm256_add_epi32` /
`_mm256_extract_epi32` — `vpbroadcastd`/`vpaddd ymm`/`vextracti128`, verified in the compiled
probe's disassembly). The `CMakeLists.txt:193` note was informational; no change.

## R-6 — the Clang measurement gap is closed, and the methodology validated itself

Same cell, same instrument, same container. Harness: a session-scratch single-cell driver
replicating `tests/bench.cpp`'s "working (reference)" cell **exactly** (same `EngineParameters`,
same fixed-seed LCG + 220 Hz tone stimulus, 48 kHz / 128) minus the `chrono` reads; callgrind Ir
with startup subtraction (3.0 s run minus 1.0 s run, over 2.0 s = 96,000 samples); machine
"Intel(R) Xeon(R) Processor @ 2.10GHz", 4 cores, shared container. Compilers: the container's
GCC 13.3.0 (the toolchain every historical figure came from) and **clang-22.1.8 from apt.llvm.org —
the exact pinned major that builds the shipped Linux artifact** (`ANAMORPH_CLANG_VERSION: 22`),
installed via `scripts/setup-llvm-apt.sh 22`. Both compilers, both flag states
(`ANAMORPH_X86_ISA_BASELINE` ON = shipped, OFF = frozen pre-0.9.5 baseline), current tree:

| toolchain / flags | 3.0 s Ir | 1.0 s Ir | Δ Ir | **Ir/sample** | AVX2 delta |
|---|---:|---:|---:|---:|---:|
| GCC 13.3, baseline OFF | 249,667,008 | 85,904,208 | 163,762,800 | **1705.9** | — |
| GCC 13.3, shipped flags | 204,474,351 | 70,830,317 | 133,644,034 | **1392.1** | **−18.4 %** |
| Clang 22.1.8, baseline OFF | 184,159,803 | 63,837,415 | 120,322,388 | **1253.4** | — |
| Clang 22.1.8, shipped flags | 148,761,850 | 52,013,035 | 96,748,815 | **1007.8** | **−19.6 %** |

Four results, in decreasing order of importance:

1. **The methodology validated itself before saying anything new**: GCC-13 at the frozen baseline
   measures **1705.9 Ir/sample against the historical 1704.9** — 0.06 % apart across a different
   harness instance, a later tree (A7-9 included) and months of container drift. The instrument is
   as machine-stable as `PERFORMANCE_BUDGET.md` claims.
2. **The AVX2 win holds on the shipped toolchain**: −19.6 % on Clang 22 against −18.4 % on GCC 13
   (−17.2 % historical, pre-A7-9 tree). The ADR-0031 benefit is not a GCC artifact.
3. **The shipped Clang-22 binary is substantially leaner than the toolchain all historical figures
   were measured on**: 1007.8 vs 1392.1 Ir/sample at shipped flags — **−27.6 %**. Every historical
   Ir figure in this repository therefore *overstates* the shipped binary's instruction count by
   roughly a third in absolute terms; the **deltas** (which is what every A7 decision was based on)
   transfer, the absolutes do not. Historical GCC-13 figures are left untouched, as required.
4. **Workload-level identity corroborated**: all four builds print an identical output checksum
   over the 3.0 s render — consistent with Class A (flags) and the GCC/Clang bit-agreement, on
   real engaged signal.

### F-8 — found while validating: GCC 13 emits FMA under `-ffp-contract=off`, legitimately

The GCC-13 shipped-flags scratch binary carried **4 `vfmadd132ps`** despite the pin — all inside
JUCE's `AudioDataConverters` (dead code on the DSP path). Minimal repro: a vectorized
**unsigned**-int32→float conversion (`s * (float) u32[i]`) — GCC 13 lowers u32→f32 through a
split-halves idiom whose combine step (`hi·2¹⁶ + lo`, the multiply exact) it emits as FMA, with
**bit-identical semantics fused or unfused**. This is instruction *selection*, not floating-point
*contraction*; `-ffp-contract=off` governs only the latter, and Clang 22 lowers the same conversion
FMA-free. The shipped binaries read 0 FMA because `--gc-sections` removes the unreferenced
converters, and nothing in `src/` converts u32→float. **Consequence for verification practice**:
the ADR-0031 FMA census is the load-bearing check and stays; a future nonzero census must be
*diagnosed* (exact-idiom FMA is possible and harmless) rather than read as a broken pin — and the
census's true guarantee is "no *contraction* FMA", demonstrated by the twin dump beside it.

## R-results — the first `windows-avx2-ab` run (read from CI, not predicted)

Run 33290956204 on `windows-latest`, job green in 6m 59s, alongside an otherwise all-green matrix
(the fixed AVX2 probes included).

| fact | value |
|---|---|
| MSVC toolset | **14.51.36231** (14.5x series; ≥ 14.30, so the job itself printed: "VS2022+ non-contracting `/fp:precise` default applies: **YES**") |
| flags | A = MSVC defaults (SSE2 baseline, `/fp:precise`) · B = `/arch:AVX2` · C = `/arch:AVX2 /fp:contract` |
| **A vs B — the Class-A question** | **IDENTICAL: 0 of 32 scenarios differ.** `/arch:AVX2` alone, at the default non-contracting `/fp:precise`, preserved every scenario hash |
| **B vs C — contraction alone** | **32 of 32 scenarios differ** — every scenario, every oversampling factor, both channel modes |
| A vs C | 32 of 32 differ (the whole package with contraction on) |

**Reading.** The MSVC result has exactly the shape the GCC/Clang measurements had: at a fixed ISA,
the ISA extension alone is bit-preserving (Linux: 0/32 for the flag pair; Windows: 0/32 for
`/arch:AVX2` at the non-contracting default) and **contraction alone is the bit-moving half**
(Linux `-march=haswell` contract-fast vs off: 32/32; Windows `/fp:contract`: 32/32). The open
question the audit could not answer from documentation — whether MSVC's vectorizer would
reassociate reductions under `/fp:precise` — is answered empirically for this instrument's
coverage: **it did not**. The Class-A property is now *demonstrated* on MSVC, on this toolset,
rather than assumed; ADR-0031 option 5's stated blocker is discharged as evidence, not as a
decision.

**One instrument defect on the first run, found and fixed the same day**: the dump prints its
self-check verdict line to stdout ahead of the table, and the job's parser counted it as a 33rd
"scenario" — it was the single "agreeing" row in the B-vs-C count (the log read "32 of 33 differ").
The parser now requires the 16-hex hash field; the true figures are the table above. Same defect
class as A7-5E's blank-line counting artifact, recorded for the same reason.

**Status: evidence READY FOR ADR; adoption remains DEFERRED.** Remaining blockers before any
future MSVC `/arch:AVX2` adoption, unchanged from R-1 and now concrete:

1. A maintainer decision and its own ADR, superseding ADR-0031 option 5.
2. The Windows ISA floor written into `COMPATIBILITY_POLICY` / `COMPATIBILITY_MATRIX` /
   `KNOWN_ISSUES` / both user guides **before** the flag lands (the ADR-0031 ordering rule).
3. A toolset ≥ 14.30 assertion in the `windows` job (today it gates only the 14.x series, which
   would still admit pre-17.0 toolsets; this run's 14.51 was observed, not guaranteed).
4. `windows-avx2-ab` promoted from reporting-only to a **permanent blocking gate**, because both
   properties the Class-A claim rests on are toolset-version behaviours and the toolset floats.
5. Honest framing of the benefit: **no Windows instruction-count instrument exists** — the
   adoption case rests on the shared mechanism (same hot loops, 256-bit lanes; −18…−20 % measured
   on GCC/Clang for the analogous change) plus this bit-identity demonstration, not on a Windows
   measurement.
