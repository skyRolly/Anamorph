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

- **Baseline** (bc5f852, unmodified flags): full self-test suite `130 checks, 0 failures`.
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

- **DSP self-tests:** `130 checks, 0 failures` (identical to baseline).
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
