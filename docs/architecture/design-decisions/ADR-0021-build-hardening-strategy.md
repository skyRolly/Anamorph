# ADR-0021 — Build Hardening Strategy (RH-PR-2)

**Status:** Accepted (Build System gate — human sign-off is the merge of the RH-PR-2 review,
per `ARCHITECTURE_REVIEW_GATE.md`; same path as ADR-0012).

ADR-0016..0020 are reserved by `RELEASE_HARDENING_PLAN.md` §8 for licensing/signing/installer
decisions not yet made; this decision takes the next free number.

## Context

The release-hardening program (RELEASE_HARDENING_PLAN, RH-PR-2 = §6.1) requires the shipped
binaries to stop handing over a reverse-engineering map while *preserving* internal debugging
and field-crash symbolication. Measured baseline (worklogs/release-hardening/
RH_PR2_INVESTIGATION.md): the Release VST3 `.so` shipped a full static symbol table
(15,094 entries, ~995 project-named symbols — every DSP/processor method name readable), while
**no debug info existed on any platform** (no `-g`, no dSYM, no PDB), so a field crash was
already unsymbolizable — stripping without a retention pipeline would have made that permanent
(RH-R4 vs RH-R8 tension). macOS ad-hoc signing failures were swallowed (`|| true`, RH-R9), and
two of three CI artifact uploads accepted empty results (`if-no-files-found: warn`).

## Problem

Reduce the public binaries' symbol surface and pin platform hardening flags, with **zero
behaviour change** (DSP numerics frozen — `DSP_POLICY`; no `-ffast-math`, no optimization-level
change), while making crash symbolication *possible for the first time* and keeping local
developer builds fully debuggable.

## Options considered

1. **Strip in CMake (post-build).** Rejected: strips local developer builds too, violating the
   "preserve debugging capability internally" goal. Strip belongs to CI packaging, after debug
   info is captured.
2. **Full strip (`-s`, remove `.dynsym`).** Rejected: the VST3 host resolves
   `GetPluginFactory`/`ModuleEntry`/`ModuleExit` from `.dynsym`; a full strip produces a plugin
   no host can load. `--strip-unneeded` keeps exactly the dynamic surface (measured: 16 exports
   before and after).
3. **Add explicit visibility flags.** Rejected as redundant: JUCE's plugin helpers already set
   `CXX_VISIBILITY_PRESET hidden` + `VISIBILITY_INLINES_HIDDEN` on the shared-code and every
   format target (JUCEUtils.cmake; measured: only 16 dynamic exports). Restating it would mask
   the real source. Recorded as a verified property instead.
4. **Whole-binary obfuscation / packers / anti-debug.** Already rejected as security theater by
   RELEASE_HARDENING_PLAN §5 (notarization/AV/host-stability costs, days of delay for attackers).
5. **Blanket directory-scope `add_compile_options`.** Rejected: silently hits FetchContent
   internals (`juceaide`) and any future target; an INTERFACE target linked into named targets
   is explicit and reviewable.
6. **The adopted flag set** (below).

## Decision

A dedicated `AnamorphHardening` INTERFACE target in `CMakeLists.txt` carries all hardening
flags and is linked `PUBLIC` into the shared-code target (usage requirements propagate to every
format target's compiles and final links) and into `AnamorphTests` (the self-tests always run
under the shipped flag configuration):

- **GCC/Clang:** `-fstack-protector-strong` (explicit, was distro-default-dependent);
  `-ffunction-sections -fdata-sections`; Release adds `-g` (debug sections only — codegen
  unchanged). Linux link: `-Wl,--gc-sections -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack`
  (full RELRO; baseline was partial). macOS link: `-Wl,-dead_strip`.
- **MSVC:** `/guard:cf` (compile+link — the one hardening flag that is not an MSVC default);
  `/DYNAMICBASE /NXCOMPAT` pinned explicitly; Release adds `/Zi` + `/DEBUG /OPT:REF /OPT:ICF`
  so linker PDBs exist at all (`/OPT:REF,ICF` restores the exact non-`/DEBUG` link semantics —
  the shipped image is unchanged, only the PDB is new).
- **CI retain-then-strip pipeline** (`build.yml`): Linux `objcopy --only-keep-debug` →
  `strip --strip-unneeded` → `--add-gnu-debuglink`, ordered **before** pluginval so the gate
  validates the stripped bytes; macOS `dsymutil` → `strip -x` → codesign **last** (stripping
  after signing would invalidate the seal); Windows PDBs actively removed from the public
  bundle copy (the VST3 linker drops the PDB inside the bundle directory) and retained
  separately. Debug info ships as separate `Anamorph-<OS>-debug` artifacts; public artifacts
  never contain symbols.
- **Artifact/signing hygiene:** every `|| true` removed from staging/codesign/lipo steps;
  `if-no-files-found: error` on all uploads. (Failure *visibility* half of RH-R9; Developer ID
  signing + notarization remain RH-PR-3.)
- **Untouched by decision:** `-O3`, LTO (verified already active via
  `juce::juce_recommended_lto_flags` on compile and final link), no `-ffast-math`; symbol
  visibility (JUCE-owned, verified); RTTI stays on (typeinfo strings are accepted residue —
  `-fno-rtti` would be an ABI/behaviour risk for zero-day gain).

## Security tradeoffs

- **Gain:** no static symbol map in shipped binaries (`nm: no symbols`; −19.8% Linux `.so`
  size); full RELRO closes GOT-overwrite gadgets; CFG on Windows raises indirect-call hijack
  cost; explicit stack canaries survive toolchain changes; crack distribution gets no free
  function-name roadmap (RTTI class names remain — accepted, cost of `dynamic_cast`).
- **Cost:** `/guard:cf` adds a small indirect-call runtime check (no allocation/locks — audio
  thread contract intact; numerics untouched); `-g` under LTO makes CI links slower and the
  retained debug artifact large (~87 MB/platform) — confined to a separate CI artifact;
  symbol-served crash reports now *depend* on the debug artifacts being archived per release
  (pairs with RELEASE_HARDENING_PLAN §7 crash-reporting row).
- **Not claimed:** none of this stops a determined reverse engineer (client-side code never
  does — RELEASE_HARDENING_PLAN §5 threat-model honesty); it raises effort and removes the
  free map.

## Consequences

- pluginval (Linux) now gates the exact stripped bytes users receive.
- A signing or staging failure fails the CI job instead of shipping a broken/empty artifact.
- Local builds are *more* debuggable than before (Release now carries debug info locally;
  nothing is stripped outside CI packaging).
- RH-PR-3/5/8 inherit the pipeline slots (dSYM/PDB retention is where release-archival and
  notarization steps attach).

## Related code

`CMakeLists.txt` (AnamorphHardening block), `.github/workflows/build.yml` (strip/retain +
artifact steps), `worklogs/release-hardening/RH_PR2_INVESTIGATION.md` (baseline + validation
evidence).

## Evidence + confidence

**Verified (measured):** baseline/post metrics, byte-identical twin engine dump
(sha256 `6efa116a…3472` both builds), 130/130 self-tests under hardened flags, stripped-binary
`dlopen` + entry-point resolution, full-RELRO flags in the ELF. **To confirm on CI:** Windows
PDB emission paths and macOS dsymutil/strip/codesign sequence (derived from CMake/JUCE
semantics; validated by the first CI run of this PR).
