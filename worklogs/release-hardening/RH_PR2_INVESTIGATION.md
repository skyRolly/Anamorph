# RH-PR-2 Build Hardening — Investigation Worklog

Work package: RELEASE_HARDENING_PLAN §6.1 / §10 row "RH-PR-2 Build hardening".
Baseline commit: `bc5f852` (main after PR #61). Local platform: Linux x86_64, GCC, Unix Makefiles,
Release. macOS/Windows facts are derived from the CMake/JUCE sources and CI definitions and are
marked for CI confirmation where they cannot be executed here.

## 1. Current build state (measured baseline)

### Symbol visibility — ALREADY HIDDEN (plan drift, reported)

RELEASE_HARDENING_PLAN §1 "Symbol hygiene" says *"no explicit `CXX_VISIBILITY_PRESET`"*. True of
our `CMakeLists.txt`, but **JUCE's plugin helpers already set hidden visibility** on both the
shared-code target and every format target:

- `_deps/juce-src/extras/Build/CMake/JUCEUtils.cmake:1485-1487` (wrapper targets) and
  `:1634-1636` (shared-code target): `VISIBILITY_INLINES_HIDDEN TRUE`, `C_VISIBILITY_PRESET
  hidden`, `CXX_VISIBILITY_PRESET hidden`.
- Confirmed in the compile line (`build/CMakeFiles/Anamorph.dir/flags.make`):
  `-fvisibility=hidden -fvisibility-inlines-hidden`.
- Confirmed in the binary: the shipped VST3 `.so` exports only **16 dynamic symbols** — the three
  VST3 entry points (`GetPluginFactory`, `ModuleEntry`, `ModuleExit`) plus libstdc++
  vague-linkage/RTTI residue. The export surface is already minimal.

**Consequence:** RH-PR-2 does not need to add visibility flags to the plugin targets; the §6.1
row is satisfied-by-JUCE and only needed verification. The plan row is corrected in this PR.

### The actual gap: full symtab ships (RH-R4)

Measured on the Release VST3 `.so` (6,643,160 bytes) and Standalone (7,043,824 bytes):

| Metric | VST3 .so | Standalone |
|---|---|---|
| `.symtab` entries | 15,094 | 16,075 |
| Project-named symbols (`nm -C \| grep -i anamorph`) | 995 | 998 |
| Dynamic exports | 16 | 12 |
| Debug sections | 0 | 0 |

Every internal function name (`AnamorphAudioProcessor::reassertParameters`,
`createAnamorphLayout`, DSP class names…) is present in the static symbol table — a free
reverse-engineering map. **No debug info exists at all** (Release builds without `-g`), so today
there is also *nothing to retain* for crash symbolication: stripping without first generating
debug info would make field crashes permanently undiagnosable (RH-R8).

### Hardening flags present/absent (measured)

| Flag | State |
|---|---|
| LTO | **Active and verified**: `-flto` on compile and final link (via `juce::juce_recommended_lto_flags`, `PUBLIC` on the shared-code target → propagates to format targets). Resolves §6.1's "effect to verify". |
| Optimization | `-O3 -DNDEBUG` (via `juce_recommended_config_flags`); untouched by this PR per §6.1 ("no flag change that could alter DSP numerics"). |
| PIC/PIE | `-fPIC` on all plugin objects (required for the shared plugin). |
| RELRO | **Partial only** — `GNU_RELRO` segment present, no `BIND_NOW` dynamic flag. |
| Non-exec stack | Present (`GNU_STACK RW`, not `RWE`). |
| Stack protector | Indeterminate from the binary (one `__stack_chk` reference — distro-default coverage only); not explicit in the build. |
| Section GC | Absent — no `-ffunction-sections/-fdata-sections/--gc-sections`. |

### Assert/log/string hygiene (audited)

- `strings -n 6` over the Release `.so`: **0** source-path strings (`/home/`, `.cpp`…),
  **0** assertion-message strings. The only `assert` hits are the mangled symtab name of
  `reassertParameters` (gone after strip). `NDEBUG` is doing its job; no logging strings leak.
- RTTI typeinfo names (class names) necessarily remain even after strip (required for
  `dynamic_cast`/exception handling); acceptable residue, not actionable without ABI risk.

### macOS (from CI definition + JUCE sources; execution on CI only)

- Same JUCE hidden-visibility properties apply (same JUCEUtils.cmake paths).
- **No dSYM generation**: Ninja + `CMAKE_BUILD_TYPE=Release` compiles without `-g`; `dsymutil`
  is never run (grep of build.yml: no `dsymutil`, no `strip`).
- **Signing**: ad-hoc only, `codesign --force --deep --sign -` with **`|| true`** on all three
  bundles (build.yml:167-169) — a signing failure ships a broken artifact silently (RH-R9).
  `--deep` is deprecated for distribution signing (RH-PR-3 scope).
- **No notarization** (RH-R2; needs Apple Developer account — human-gated, RH-PR-3).

### Windows (from CMake semantics; PDB behaviour to confirm on CI)

- CMake's default MSVC Release configuration compiles **without `/Zi`** and links **without
  `/DEBUG`** → **no PDB is produced at all** for Release. Nothing to separate or retain (RH-R8).
- `/DYNAMICBASE`, `/NXCOMPAT`, `/GS` are MSVC/linker defaults (assumed present, §6.1 marked
  Unverified); **`/guard:cf` (CFG) is NOT a default** and is absent.
- Release artifact staging copies the `.vst3` bundle directory recursively — if PDBs are enabled
  later without care, the linker drops `Anamorph.vst3.pdb` next to the binary **inside the
  bundle**, which would leak full symbols into the public artifact. The staging step must
  actively separate PDBs.

### CI / artifact handling

- Single `build.yml` (push/PR/dispatch, `contents: read`); no release pipeline, no tags.
- Artifacts: `Anamorph-Linux` (`if-no-files-found: warn`), `Anamorph-Windows` (`warn`),
  `Anamorph-macOS` (`error`). The two `warn`s mean a staging failure can upload an **empty
  artifact successfully** — silent artifact loss.
- Linux staging: `cp ... || true` on the Standalone binary — same silent-loss pattern.
- macOS packaging: `codesign || true` ×3, `lipo -archs || true` ×3 — the universal-slice
  *verification* itself cannot fail the job.

## 2. Decisions (what RH-PR-2 ships)

All behaviour-neutral (no optimization/numerics flag touched; `-ffast-math` stays off):

1. **Debug-info generation + retain-then-strip pipeline** (closes RH-R4 without opening RH-R8):
   - GCC/Clang Release: add `-g` (documented as codegen-neutral; only adds debug sections).
   - Linux CI: `objcopy --only-keep-debug` → `strip --strip-unneeded` → `--add-gnu-debuglink`;
     debug files uploaded as a separate `Anamorph-Linux-debug` artifact.
   - macOS CI: `dsymutil` → `strip -x` → *then* ad-hoc codesign (strip after signing would
     invalidate the signature); dSYMs uploaded as `Anamorph-macOS-debug`.
   - Windows: enable `/Zi` + `/DEBUG /OPT:REF /OPT:ICF` for Release (PDBs finally exist);
     PDBs pulled **out** of any staged bundle and uploaded as `Anamorph-Windows-debug`.
     `/OPT:REF,ICF` restores the non-`/DEBUG` linker defaults, so the shipped image is the
     same as today's — `/DEBUG` alone would have disabled them.
2. **Section GC**: `-ffunction-sections -fdata-sections` + `-Wl,--gc-sections` (Linux),
   `-Wl,-dead_strip` (macOS). Unreferenced code/data dropped at link; no semantic effect.
3. **Linker hardening (Linux)**: `-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack` (full RELRO;
   noexecstack was already the effective state, now pinned).
4. **Stack protector**: `-fstack-protector-strong` made explicit (was distro-default-dependent).
5. **Windows hardening made explicit**: `/guard:cf` (compile+link — the one non-default),
   `/DYNAMICBASE /NXCOMPAT` pinned explicitly.
6. **CI artifact hygiene**: `if-no-files-found: error` everywhere; all `|| true` removed from
   staging/signing/lipo steps (RH-R9's visibility half fixed here; proper Developer ID signing
   remains RH-PR-3).
7. Flags delivered via a dedicated `AnamorphHardening` INTERFACE target linked `PUBLIC` into the
   shared-code target (usage requirements propagate to every format target's compile and final
   link) and into `AnamorphTests` (same flags = tests validate the shipped configuration).

## 3. Rejected approaches

| Rejected | Why |
|---|---|
| Adding `CXX_VISIBILITY_PRESET` to our CMakeLists | Redundant — JUCE already sets it on all plugin targets (measured: 16 dyn exports). Restating it would mask the real source of the setting and drift if JUCE changes. Verification recorded instead. |
| `strip` inside CMake (post-build command) | Would strip *local developer* builds too, destroying debuggability (§ goal "preserve debugging capability internally"). Strip belongs to CI packaging, where retention artifacts are captured first. |
| `-s`/full `strip` (remove dynsym) on the `.so` | `--strip-unneeded` keeps the dynamic symbols the VST3 host resolves (`GetPluginFactory` — measured present post-strip); a full strip of `.dynsym` would produce a plugin no host can load. |
| Touching `-O3`/LTO/`-ffast-math` | §6.1 explicitly freezes numerics-affecting flags; LTO verified already active; nothing to change. |
| Whole-binary obfuscation/packers | Already rejected as security theater in RELEASE_HARDENING_PLAN §5. |
| Blanket `add_compile_options` at directory scope | Would also hit `juceaide`/FetchContent internals and future targets invisibly; the INTERFACE-target route is explicit, per-target, and reviewable. |
| Fixing `codesign --deep` → nested signing now | Distribution signing rework is RH-PR-3 (needs Developer ID + notarization design, ADR-0019). Only the failure-swallowing is fixed here. |

## 4. Experiments & validation runs (evidence)

- **Baseline** (bc5f852, unmodified flags): full self-test suite `130 checks, 0 failures`
  (the pre-Wave-3 tree's count; the suite is 136 checks after the v0.8.11 rebase — §6).
- **Twin engine dump** (behaviour-neutrality proof): a local console harness (kept out of the
  shipped tree; source preserved below) drives `anamorph::AnamorphEngine` at 48 kHz/512 through
  ~10.7 s of deterministic input (two detuned sines + fixed-seed LCG noise) with a feature-heavy
  trajectory — Velvet engine, drive 6 dB, global width 1.4, multiband enabled with two mid-run
  crossover moves (exercises the slew follower), Mono Maker, mix/output-gain changes — and dumps
  every output float. Baseline SHA-256:
  `6efa116a923440125522368dbe815abdba414c32d95d4e75f8142c3b716e3472` (rms 0.569, peak 1.51 —
  live signal, not a degenerate dump).
- **Post-hardening**: same suite + byte-identical dump hash required (recorded in §5).
- Post-strip load-surface check: `GetPluginFactory/ModuleEntry/ModuleExit` must remain in
  `.dynsym` after `strip --strip-unneeded` (recorded in §5).

### Twin-dump harness source (for reproduction in later RH PRs)

Compile as a JUCE console app linking `AnamorphDSP + juce_dsp + juce_audio_basics` with
`juce_recommended_config_flags + lto_flags` (a temporary `juce_add_console_app` target;
`-DANAMORPH_DUMP_SRC=<path>`):

```cpp
// (verbatim harness used for this PR)
#include <cstdio>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/AnamorphEngine.h"

int main (int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "engine_dump.bin";
    constexpr double sr = 48000.0; constexpr int block = 512, nBlocks = 1000;
    anamorph::AnamorphEngine eng; eng.prepare (sr, block);
    anamorph::EngineParameters p;
    p.algoAmount = 0.6f; p.driveDb = 6.0f; p.width = 1.4f;
    p.mbEnable = true; p.mbWidthLow = 0.8f; p.mbWidthHigh = 1.6f;
    p.monoMakerEnable = true; p.mix = 0.8f;
    eng.setParameters (p);
    juce::AudioBuffer<float> buf (2, block);
    std::FILE* f = std::fopen (path, "wb"); if (! f) return 2;
    uint32_t lcg = 0x12345678u; double ph1 = 0.0, ph2 = 0.0;
    for (int b = 0; b < nBlocks; ++b)
    {
        if (b == 200) { p.mbFreqLow = 90.0f;    p.mbFreqMid = 1500.0f; eng.setParameters (p); }
        if (b == 500) { p.mbFreqHigh = 9000.0f; p.width = 0.7f;        eng.setParameters (p); }
        if (b == 800) { p.mix = 1.0f;           p.outputGainDb = -3.0f; eng.setParameters (p); }
        for (int i = 0; i < block; ++i)
        {
            lcg = lcg * 1664525u + 1013904223u;
            const float noise = (float) ((int32_t) lcg) / 2.1e10f;
            ph1 += 2.0 * juce::MathConstants<double>::pi * 220.0  / sr;
            ph2 += 2.0 * juce::MathConstants<double>::pi * 3001.0 / sr;
            buf.setSample (0, i, 0.35f * (float) std::sin (ph1) + noise);
            buf.setSample (1, i, 0.35f * (float) std::sin (ph2) - noise);
        }
        eng.process (buf);
        for (int c = 0; c < 2; ++c)
            std::fwrite (buf.getReadPointer (c), sizeof (float), (size_t) block, f);
    }
    std::fclose (f); return 0;
}
```

## 5. Post-change validation results

All measured on Linux x86_64 / GCC / Release with the full hardening set applied:

- **DSP self-tests:** `130 checks, 0 failures` (identical to baseline; 136/136 on the rebased
  v0.8.11 tree — §6).
- **Twin engine dump:** byte-identical to baseline —
  `sha256 6efa116a923440125522368dbe815abdba414c32d95d4e75f8142c3b716e3472` for BOTH
  `dump_baseline.bin` and `dump_hardened.bin`; `cmp` clean. The flag set is behaviour-neutral
  on real engine output, not just by argument.
- **Hardened binary (pre-strip):** debug sections present (8), **full RELRO** (`BIND_NOW` now
  set; baseline had partial RELRO only), non-exec stack unchanged.
- **CI strip pipeline simulated locally** (`objcopy --only-keep-debug` → `strip
  --strip-unneeded` → `--add-gnu-debuglink`), on the VST3 `.so`:

| Metric | Baseline shipped | Hardened shipped (stripped) |
|---|---|---|
| File size | 6,643,160 B | **5,325,368 B** (−19.8%; strip + `--gc-sections`) |
| `.symtab` | 15,094 entries, 995 project-named | **none** (`nm: no symbols`) |
| Dynamic exports | 16 | 16 (unchanged; `GetPluginFactory`/`ModuleEntry`/`ModuleExit` intact) |
| Crash symbolication | impossible (no debug info existed) | full — 87.5 MB `.debug` file, `AnamorphEngine::process` resolvable, `.gnu_debuglink` embedded |

- **Load test:** the stripped `.so` `dlopen`s cleanly and all three VST3 entry points resolve
  (ctypes probe). `--gc-sections` removed no needed init/exported code.
- **Residual strings:** 57 project-named strings remain post-strip — all RTTI typeinfo names
  (`_ZTS…`/typeinfo residue incl. lambda target types held by `std::function`), required for
  `dynamic_cast`/EH; documented acceptable (removing them means `-fno-rtti`, an ABI/behaviour
  risk far outside this PR).
- **Note for CI cost accounting:** `-g` under LTO makes the intermediate link output large
  (~93 MB pre-strip `.so`) and the retained `.debug` artifact is ~87 MB per platform per run.
  That is the price of RH-R8 symbolication; it lives in a separate artifact, never in the
  public one.
- pluginval cannot run in this sandbox (egress policy — same constraint recorded for the JUCE
  bump, ADR-0012); the strictness-10 both-modes gate runs on CI, which now validates the
  **stripped** Linux binary (the strip step was ordered before pluginval on purpose).

## 6. Review follow-up — artifact-safety fixes (v0.8.11 sync)

**Release context:** RH-PR-2 ships as part of **v0.8.11**. The branch was rebased onto main
after the v0.8.11 version bump (PR #64) and performance Wave 3 (PR #62) merged; the CHANGELOG
entry moved from `[Unreleased]` into `[0.8.11]` **### Security** per the release structure.
Post-rebase suite under the hardened flags: **136 checks, 0 failures** (Wave 3 added Test 33 +
checks). The flag set itself is unchanged from §2, so the §5 byte-exact twin-dump proof of
flag-neutrality stands (it was measured against the flags, and no numerics-affecting flag was
touched then or now; Wave 3's own behaviour evidence is in
`worklogs/performance/WAVE3_INVESTIGATION.md`).

### Review findings (both confirmed real)

1. **Linux: a strip failure could still upload an unstripped customer artifact.** Root cause:
   the staging and upload steps ran under `if: always()` — designed to preserve beta artifacts
   when *pluginval* fails, but it equally fired when the *strip* step failed, staging and
   uploading the un-stripped binaries the build produced (exactly the RH-R4 leak this PR
   exists to close).
2. **Windows: a mid-step packaging failure could upload the public bundle with its PDB
   inside.** Root cause: operation order inside the staging step — the PDB *retention* loop
   (which `Write-Error`-aborts if a PDB is missing) ran BEFORE the public-copy PDB *removal*;
   with `$ErrorActionPreference='Stop'`, an abort in retention skipped the removal entirely,
   and the `if: always()` upload then published the bundle with `Anamorph.pdb` still inside
   (the VST3 linker drops it inside the bundle directory that gets copied recursively).

### Fix (CI-only; no build-flag, binary, or runtime change)

- **Upload gating:** customer staging/uploads are now conditioned on their producing step
  having SUCCEEDED — Linux `stage` requires `steps.strip.outcome == 'success'`, each customer
  upload requires its staging/packaging step's `outcome == 'success'` (macOS given the same
  pattern for uniformity). `!cancelled() && outcome == 'success'` is used instead of plain
  step-ordering defaults so a *pluginval* failure still yields beta artifacts (the original
  reason for `always()`), while a *packaging* failure cannot upload. Developer `-debug`
  artifacts remain preserved on failure (`outcome != 'skipped'`) — they never contain
  customer binaries.
- **Windows step reordering (leak-safe by construction):** (1) locate everything first — an
  abort copies nothing; (2) copy the public artifact; (3) **immediately purge every debug
  file** (`.pdb/.ilk/.exp/.debug`) from the public copy — before any abortable step; (4)
  retain PDBs into the debug dir (abort here is now leak-safe); (5) validate the public copy
  contains no debug files.
- **Self-validation added:** the Linux strip step asserts `.symtab` is gone from both shipped
  binaries; the Linux staging step re-asserts no `.symtab` and no `.debug`/PDB files inside
  the customer dist; the Windows staging step asserts no debug-extension files remain.

### Validation (verbatim step scripts extracted from build.yml and executed locally)

- **Linux success path:** strip + stage exit 0; customer dist `nm: no symbols`, zero debug
  files; debug dir carries both `.debug` files.
- **Linux failure sim 1 (strip fails — missing binary):** strip step exits 1 → under the new
  conditions `steps.strip.outcome == 'failure'` skips staging AND the customer upload; no
  customer dist is even created.
- **Linux failure sim 2 (defense-in-depth — an unstripped binary reaches staging):** staging
  validation catches it (`::error::customer artifact contains an unstripped binary`), exits 1,
  customer upload gated off.
- **Linux failure sim 3 (stray `.debug` file in the public copy):** validation detects, fails.
- **Windows (bash mirror of the pwsh phases — pwsh unavailable in this sandbox; same commands,
  same order, same abort semantics):** success path separates PDBs (public: none; debug:
  `Anamorph.vst3.pdb` + `Anamorph.standalone.pdb`); failure sim (Standalone PDB missing —
  abort in phase 4) leaves the public dist with **zero** PDBs because the purge ran in
  phase 3, and the upload is gated off anyway — two independent layers.
- Workflow YAML parse-validated. First real Windows/macOS execution remains this PR's CI run
  (asserted strictly — wrong assumptions fail loudly, and now cannot upload).

### Conflict-resolution notes (rebase onto v0.8.11 main)

- `CHANGELOG.md`: kept main's `[0.8.11]` structure (Wave 3 `### Changed` + the two maintenance
  fixes under `### Fixed`); the RH-PR-2 entry added as `### Security` inside `[0.8.11]`
  (Keep-a-Changelog section order), updated to describe the upload-gating/self-validation
  behaviour. `[Unreleased]` no longer exists.
- `docs/DOCUMENTATION_COVERAGE.md`: both sides prepended a "Last updated" entry — resolved
  with this PR's entry first, the v0.8.11 version-prep entry demoted to the "Prior:" chain
  (nothing dropped).
- `docs/REPOSITORY_MAP.md`: both sides added a `worklogs/` top-level entry (Wave 3's and this
  PR's) — merged into one.
- `docs/architecture/RELEASE_HARDENING_PLAN.md` QA-gate row: synced 31/130 → **32/136** — the
  one-line drift the version-bump PR explicitly recorded as pending "once the PRs land".
- `CMakeLists.txt` auto-merged: v0.8.11 version from main preserved, `AnamorphHardening`
  block intact. `build.yml` had no upstream changes.

## 7. Final review fixes

1. **Windows PDB retention robustness.** The retention searched the whole build tree for the
   literal name `Anamorph.pdb` and took the FIRST path whose string contained `VST3`/
   `Standalone` — fragile against PDB naming and layout changes, and silently wrong on multiple
   matches. Now: every locate demands **exactly one** match (bundle, the `.vst3` image inside
   it, the Standalone exe — zero OR multiple matches are hard errors), and each image's linker
   PDB is taken from **that image's own output directory** (where the linker writes it) with an
   exactly-one requirement — no filename guessing, no tree-wide substring matching, clear
   errors naming the image when the expectation fails. The leak-safe phase order from §6 is
   unchanged (locate → copy → purge → retain → validate).
2. **Documentation count drift (Wave-3 rebase).** ADR-0021's evidence line and the plan's §10
   row said "130 self-tests"; the rebased tree's suite is **136 checks**. Both updated; the
   worklog's §4/§5 baseline numbers are annotated as the pre-Wave-3 tree's historical counts
   rather than rewritten (they were true of those runs).
3. **Customer uploads now require the DSP self-tests to pass.** The upload gates from §6
   checked only strip/staging success, so a tests failure (the behavioural gate) could still
   ship a customer artifact. Every customer upload now additionally requires
   `steps.tests.outcome == 'success'` (all three platforms — the `DSP self-tests` steps got
   `id: tests`). pluginval-only failures still yield beta artifacts (unchanged, deliberate);
   developer `-debug` artifacts still survive failures (`outcome != 'skipped'`).

Validation: suite re-run green (**136 checks, 0 failures**); workflow YAML parse-validated;
Linux strip/stage step scripts re-extracted verbatim and re-run (success path: `nm: no
symbols`, zero debug files; unstripped-binary and stray-debug-file sims still fail closed);
Windows phase logic re-simulated in a bash mirror including the new anchored PDB discovery —
success separates `Anamorph.vst3.pdb`/`Anamorph.standalone.pdb`, a missing sibling PDB and an
ambiguous double-PDB both abort with the public dist already purged (zero PDBs). Remaining
CI-only checks: first real execution of the Windows pwsh staging and the macOS
`dsymutil → strip -x → codesign` sequence on this PR's run — wrong assumptions fail loudly and
cannot upload.

## 8. CI follow-up — Windows PDB discovery + macOS dSYM validation (first real CI run)

§7 item 1 shipped untested on real Windows CI (§7's own validation note flagged this as the one
remaining CI-only check). The first actual run proved the "own output directory" assumption wrong.

### Windows — root cause

CI failure:

```
expected exactly one linker PDB next to:
build/Anamorph_artefacts/Release/VST3/Anamorph.vst3/Contents/x86_64-win/Anamorph.vst3
found 0
```

`AnamorphHardening` (`CMakeLists.txt`) adds `/DEBUG /OPT:REF /OPT:ICF` for MSVC Release but never
sets `PDB_OUTPUT_DIRECTORY`/`PDB_NAME` on any target. JUCE's CMake support retargets
`RUNTIME_OUTPUT_DIRECTORY`/`LIBRARY_OUTPUT_DIRECTORY` for the VST3 format target so the shipped
binary lands directly at its final bundle path (`.../Contents/x86_64-win/Anamorph.vst3`) — but
that retarget does not carry the PDB with it. With `PDB_OUTPUT_DIRECTORY` unset, the Visual Studio
generator falls back to MSBuild's own default (`$(OutDir)$(TargetName).pdb`), where `$(TargetName)`
is the *CMake target's* internal name (e.g. `Anamorph_VST3`), not the `OUTPUT_NAME`/`SUFFIX`-renamed
`Anamorph.vst3` used for the shipped image. §6's investigation-phase note ("the linker drops
`Anamorph.vst3.pdb` next to the binary inside the bundle") was an unverified CMake-semantics
inference, explicitly marked "PDB behaviour to confirm on CI" — this run is that confirmation, and
it was wrong on both counts named in this task: neither the same directory nor the same base name.

### Windows — fix

Same-directory / fixed-name guessing is replaced with reading the authoritative source: the
CodeView (RSDS) debug-directory record MSVC embeds in the linked PE image itself, which carries the
PDB's exact link-time name and path (`Get-EmbeddedPdbPath` in `build.yml`, a ~55-line PE/COFF
debug-directory parser — DOS/PE header → optional-header data directory #6 → section-table RVA
mapping → `IMAGE_DEBUG_TYPE_CODEVIEW`/`RSDS` record → NUL-terminated path string). The recorded path
is tried directly first (valid within the same job/workspace that produced it); if that doesn't
resolve, a scoped search under `build/` for that *exact* recorded filename is used, still requiring
precisely one match. This is anchored to linker-recorded ground truth rather than a guessed pattern,
so it does not reintroduce the tree-wide substring matching §7 removed. A missing CodeView record
(no `/DEBUG`) or an unresolvable/ambiguous PDB both fail the job clearly (`Write-Error`, consistent
with the project's existing fail-closed policy for developer debug artifacts — `if-no-files-found:
error`, upload gated on the staging step's success). Leak-safe phase order from §6/§7
(locate → copy public → purge → retain debug → validate) is unchanged; only the PDB-locate logic
inside the retain phase changed.

### macOS — reviewer-identified risk

Reviewer flagged that `dsymutil` under LTO can produce an empty, incomplete, or warning-only
invalid dSYM, or fail on missing object-file references — and, critically, that `dsymutil`
frequently still exits `0` when this happens, only warning on stderr. The pre-fix script
(`dsymutil "$BIN" -o "$DBG/....dSYM"; strip -x "$BIN"`) relied solely on `set -e`'s exit-code check,
which this failure mode does not trip.

### macOS — fix

Before stripping, each bundle's `dsymutil` invocation is now validated three ways: (1) its stdout
and stderr are captured and any `warning` line is treated as fatal (the same heuristic used by
common Crashlytics/Sentry-style symbol-upload pipelines for exactly this dsymutil behaviour); (2)
the produced dSYM's DWARF payload (`Contents/Resources/DWARF/Anamorph`) must exist and be non-empty;
(3) `dwarfdump --uuid` on that DWARF file must report the identical UUID set as `dwarfdump --uuid`
on the still-unstripped binary, so a dSYM that is present but covers the wrong/incomplete slices is
still caught. Any failure aborts the job before `strip -x` runs, so a broken debug artifact is never
uploaded silently. LTO, codesign-last ordering, and every other RH-PR-2 packaging decision (§6, §7)
are unchanged.

### Validation (this follow-up; both platforms are CI-only, no Windows/macOS runner available here)

- `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build.yml'))"` — parses clean.
- macOS packaging step body extracted verbatim and checked with `bash -n` — clean.
- Windows staging step body extracted verbatim; brace/paren balance verified with a comment-and-
  string-aware PowerShell tokenizer (naive counting is skewed by parentheses inside comments/error-
  message text) — balanced. No `pwsh` is available in this sandbox to execute it; first live
  execution is this PR's Windows CI run, same caveat §7 already carried forward for the code this
  replaces.
- Full local DSP self-test suite re-run: **136 checks, 0 failures** (no DSP/runtime/parameter/
  serialization code touched by this follow-up — confirmed by `git diff --stat` showing only
  `.github/workflows/build.yml` plus this worklog entry).
- Scope check: `git diff --stat` against the pre-follow-up RH-PR-2 tip shows exactly
  `.github/workflows/build.yml` (workflow logic) and this file (documentation) — no CMake, ADR, or
  `src/`/`tests/` changes.

## 9. CI follow-up 2 — macOS dSYM capture must be best-effort under Release+LTO

§8's macOS hardening introduced a regression: it treated **any** dsymutil warning as a degenerate
dSYM and failed the packaging job. The first real macOS run failed with:

```
warning: (x86_64) /tmp/lto.o unable to open object file: No such file or directory
warning: (arm64) /tmp/lto.o unable to open object file: No debug symbols in executable
```

### Root cause — the assumption "any dsymutil warning = invalid dSYM" is wrong for this build

On macOS the linker does not embed DWARF in the linked binary; it records references (OSO/N_OSO
entries) to the **object files** that hold it, and dsymutil later reads those objects to assemble
the dSYM. `AnamorphHardening` adds `-g` so the objects do carry DWARF — but under LTO the "object"
the final link consumes is the linker's **temporary LTO-merged module** (`/tmp/lto.o`), deleted as
soon as the link finishes. By packaging time there is nothing for dsymutil to read: it warns
exactly as observed, exits **0**, and emits a dSYM without usable DWARF. This is an inherent
property of the Release+LTO configuration, not a broken artifact — the shipped binary never
contained that debug info in the first place, so the customer artifact is bit-for-bit unaffected.
§8's warnings-are-fatal gate therefore failed **every** macOS packaging run while guarding nothing:
warning text is not a usability signal.

### Correction (CI-only, `build.yml` macOS job)

Developer dSYM capture is now **best-effort**, judged on the *output*, never on warning text:

- Per bundle: run dsymutil (warnings logged, informational); keep the dSYM only if the DWARF
  payload exists and is non-empty **and** `dwarfdump --uuid` on it matches the binary's slice UUID
  set exactly **and** it contains ≥ 1 DWARF compile unit (a UUID-only stub with zero CUs is
  degenerate — the expected LTO outcome). An unusable dSYM is discarded with a `::warning::`
  annotation and packaging **continues**.
- `strip -x` runs unconditionally after each capture attempt; codesign-last ordering, lipo slice
  checks, and the `GetPluginFactory` export check are unchanged. A new final self-check asserts
  the customer staging dir contains no `*.dSYM`/`*.debug`/`*.pdb` (parity with Linux/Windows
  staging self-validation) — customer-artifact protection is strengthened, not weakened.
- The `-debug` upload is gated on a new `debug_artifacts` step output (written **before** the
  abortable codesign/verify steps, preserving the §6 property that captured debug artifacts
  survive later failures): zero usable dSYMs → upload skipped with a clear warning instead of
  `if-no-files-found: error` failing the job on an empty directory.
- Windows PDB retention (§8) and Linux strip/staging (§6/§7) are untouched.

**Accepted consequence:** with the current pinned flag set, macOS CI ships no crash-symbolication
artifact (Linux `.debug` and Windows PDB retention are unaffected — their debug info doesn't ride
on deleted temporaries). Restoring full macOS symbolication requires persisting the LTO object via
`-Wl,-object_path_lto,<path>` — a linker-flag change deliberately **not** made here (out of this
follow-up's scope; no compiler/CMake changes) and recorded as an RH-PR-3 candidate.

### Validation (this follow-up)

- Workflow YAML parses; macOS step body extracted verbatim and `bash -n` clean.
- **Local end-to-end simulation** of the extracted step body with mocked
  `dsymutil`/`dwarfdump`/`strip`/`codesign`/`lipo`/`nm` and a faked build tree, four scenarios:
  1. **Warning-only degenerate dSYM (the CI failure case):** dsymutil prints the exact observed
     warnings, exits 0, emits a dSYM with no DWARF payload → step **succeeds**, customer staging
     dir fully populated (3 bundles + INSTALL.txt), degenerate dSYM discarded (debug dir empty),
     `debug_artifacts=false`, warnings annotated.
  2. **Valid dSYM:** DWARF payload + matching UUIDs + CUs present → retained,
     `debug_artifacts=true`, step succeeds (validation still enforced).
  3. **UUID mismatch:** dSYM discarded with warning, step still succeeds, `debug_artifacts=false`.
  4. **Debug material planted in the customer staging dir:** final self-check **fails the step**
     (fail-closed proof for the new customer-side assertion).
- Full DSP suite re-run: **136 checks, 0 failures** (no product code touched; diff = `build.yml` +
  this file).
- Real-runner caveat unchanged from §7/§8: first live execution of the corrected macOS path is
  this push's CI run.
