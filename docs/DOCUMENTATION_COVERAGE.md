# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

Last updated: for the **0.9.4 change set** (2026-08-15, matching the CHANGELOG heading) — the
**CI review follow-up** (first below), then the **CI/validation round** it corrects, then the four
AppleClang 21 `-Wimplicit-int-float-conversion` fixes, the macOS CI runner move
`macos-14` → `macos-latest` that surfaced them, then the C++17 → C++23
language-standard migration, both applied on top of the JUCE 9.0.0 → 9.0.1
dependency upgrade in the same version; the JUCE entry follows them. Under it, the **0.9.3 change set** (2026-08-11) is retained in full — six editor-only GUI interaction fixes on
top of 0.9.2 (add-split preview line, unified pop-up dismissal, pop-up lifetime across a hidden,
destroyed or backgrounded window, menu width, disabled menu items, Tooltips off) plus a
**packaging round** (Linux per-user install default; the macOS re-install defect INC-012), landed
across seven rounds; the entries below run newest-first. Below them, the 0.9.2
entry (2026-08-07) is retained in full.

**CI + validation round — review follow-up (0.9.4, no version bump). Five corrections, and the
first is the one worth reading twice.**

Landed on top of the migration entry below, all five from a review of it. None changes what the
pipeline is *for*; four change whether it does what it claims.

1. **`MALLOC_PERTURB_=255` provided no coverage at all — it was worse than not setting it.** glibc
   applies the variable asymmetrically (`alloc_perturb → memset(p, perturb_byte ^ 0xff, n)`,
   `free_perturb → memset(p, perturb_byte, n)`), so the value is the FREED fill and its complement
   is the FRESH one. 255 therefore wrote `0x00` into fresh allocations: exactly the benign
   zero-filled heap the step exists to defeat, and *deterministically*, where leaving the variable
   unset at least returns real recycled garbage. Corrected to `1` (fresh `0xFE`, freed `0x01`).
   Measured three ways: a `malloc` probe (255 → `00 00 00 00`, 1 → `fe fe fe fe`); a sweep of all
   255 selectable values; and a read-before-write reproduction whose peak reads `0` under both
   *unset* and `255` and `1.69e38` under `1`. The original "0xFF fills a float buffer with NaN"
   rationale could not have held at any value — a float of four identical bytes `B` has exponent
   `((B & 0x7f) << 1) | (B >> 7)`, which is `0xFF` only for `B = 0xFF`, and a fresh fill of `0xFF`
   needs `perturb_byte = 0`, glibc's "off" sentinel. NaN coverage is the `sanitizers` job's.
2. **The PE header guard was two bytes short of the read it protected.** It admitted
   `peOffset + 24 <= length` and the code then read the optional-header Magic at `peOffset + 24..25`.
   Corrected to 26 (PE signature 4 + COFF header 20 + Magic 2). Reproduced against synthetic
   truncated images: at 24 and 25 bytes of slack the old bound let `ToUInt16` throw the raw .NET
   `IndexOutOfRange` the function exists to replace; the new bound diagnoses all three of 23/24/25
   and hands ≥ 26 to the next guard.
3. **The Clang warning baseline had a floating reference point.** Its per-(path, flag) counts are a
   property of the compiler major, but `linux-clang` used whatever `ubuntu-latest` resolved `clang`
   to — so a runner-image bump could turn the gate red on a push that changed nothing, the same
   defect the "never key on line numbers" rule exists to avoid, one level up. `ANAMORPH_CLANG_VERSION`
   now pins the major in one place (18 — the version the baseline was generated with, and what
   ubuntu-24.04 resolves `clang` to, so no diagnostic moved); both Clang jobs install and use
   `clang-<n>`; the baseline records `# clang-major:` and the checker **refuses to run** on a
   mismatch (exit 2, not 1). The pin also lets `sanitizers` name `libclang-rt-<n>-dev` directly
   instead of scraping `clang --version` for it.
4. **macOS validated a bundle it did not ship.** The gates ran before the packaging step, and
   `strip -x` + `codesign --force --deep` rewrite the Mach-O immediately afterwards. Both formats'
   gates now run **after** packaging against `dist/Anamorph-macOS/`, named explicitly through
   `ANAMORPH_PLUGINVAL_BUNDLE`. Every platform now validates the bytes it ships. The trade is
   recorded rather than discovered: a packaging failure now skips validation, which is the trade
   Linux already made. The AU copy installed for the registry is removed again afterwards.
5. **The CHANGELOG fence tracker closed on a nested opener.** A backtick fence carrying an info
   string (a `cpp`-tagged opener) nested inside an untagged backtick block ended the *outer* fence,
   after which the block's body was read as
   structure and the real closer re-opened one — inverting the mask to EOF. The extractor now
   applies CommonMark §4.5 in full (same character, at least as long, nothing but trailing
   whitespace) and agrees with `scripts/check-docs.py`'s `fence_mask` on all 11 edge cases tested,
   under both mawk and gawk. All 20 existing CHANGELOG versions extract byte-identically, so the fix
   is latent-only — which is the point, since the tracker exists for the first fenced sample anyone
   adds.

Docs synced: `CI_CD` (the perturbation paragraph rewritten with the mechanism and the sweep; a new
compiler-pin paragraph in §The Clang warning baseline; §Known coverage limits' shipped-bytes bullet
rewritten now that all three platforms qualify, and its AU bullet narrowed to the `auval` point; the
pipeline's step 6 given the post-packaging ordering and its trade; the local-reproduction snippet
switched to the pinned compiler), `TESTING_POLICY` (the Level 1b value and the note that it is not
the fill byte), `TESTING` (the closed-AU-gap entry re-stated for the new ordering and the cleanup
step). Validation for this round is recorded in the round's own summary.

**CI + validation round (0.9.4, no version bump) — reviewed against the sibling product Anabasis
and migrated selectively.**

Anabasis' CI is the newer implementation of the same design, so its workflows, lints and scripts
were read against this repository's and adopted where the reasoning transfers. What landed, grouped
by what it closes:

**Coverage that did not exist.** The macOS **AU** is now validated by pluginval at the same
strictness, both modes ×3 — installed into `~/Library/Audio/Plug-Ins/Components/` with the
AudioComponent registry refreshed first, because a never-installed `.component` can report zero
plugin types however correct it is. The universal binary's **x86_64 slice is now executed**, under
Rosetta 2, via a new `ANAMORPH_TEST_RUNNER` prefix in `run-tests.sh`; it was compiled on every push
and run by nothing. Two new jobs: **`linux-clang`** (Clang's warning set is strictly larger than
GCC's, and it builds the LTO'd plugin — the configuration users install and the only one no other
job compiles with a second toolchain) and **`sanitizers`** (ASan + UBSan, then valgrind memcheck
from an unsanitized build; there was no dynamic-analysis coverage at all). Two new lint jobs,
**`docs`** and **`source-lint`**, carrying four checkers with self-tests.

**Defects the review found.** `--random-seed 0` is pluginval's *"pick a random seed"* sentinel, so
the "deterministic" gate was not deterministic — measured, not inferred (seed 0 printed a different
`Random seed:` per run against pluginval 1.0.4; seed 1 printed `0x1` every time). The Linux
`.gnu_debuglink` stored a CI-workspace-relative path no user's machine has. `lipo -archs` output was
printed rather than asserted, and it exits 0 for a thin Mach-O. `find … | head -n1` /
`Select-Object -First 1` picked whichever build enumerated first. `$SUDO DEBIAN_FRONTEND=… apt-get`
breaks when `$SUDO` is empty. `build.sh` exited 1 after a successful build whenever an optional
artefact was absent, silently breaking `build.sh && run-tests.sh`. Every same-repo PR built the
3-OS matrix a second time, and concurrent pushes raced full matrices.

**What was NOT migrated, and why** — recorded so the omissions are decisions rather than gaps:
Anabasis' `preflight` job (a pre-P1 scaffold guard; this repository has had a `CMakeLists.txt`
since long before 0.9.0, so it would be a permanent no-op adding a `needs:` edge to every build);
`cxx23-canary.yml` (it exists to pre-warn a C++20 → C++23 baseline raise, and this project is
*already* at C++23 — there is nothing to canary); the `channel_probe` / `engine_repro` host-side
tools (product code written around a specific Anabasis field report, not CI infrastructure — porting
them means authoring new C++, and pluginval already loads the built bundle through
`juce_audio_processors` at strictness 10); the `macos-*-intel` native-Intel job (a real gap, but
Rosetta execution closes the cheap 95% of it for no extra runner — recorded in `CI_CD.md` §Known
coverage limits, not silently dropped); and `.gitattributes` (Anabasis needs `text eol=lf` because
its snapshot fixture is compared **byte-wise**; this repository's comparison is line-based through
`juce::StringArray::fromLines`, which strips `\r`, so the hazard does not reach it).

**The one adaptation worth naming.** The Clang warning gate could not be adopted as-is: this tree
already carries 14 first-party Clang warning sites, and clearing them means renaming a member across
the editor, adding cases to engine switches and changing float comparisons in DSP code — source work
that belongs in its own review under `DSP_POLICY.md`. Landing the job red teaches people to ignore
it; landing it non-blocking makes a gate that cannot fail. So it asserts **no new** warnings against
a checked-in baseline keyed on `(path, flag)` with a site count — never line numbers, which drift on
unrelated edits — and a falling count is a notice asking the baseline to shrink, never a failure.
The debt list is `scripts/clang-warning-baseline.txt` and is reproduced in `CI_CD.md`.

Docs synced (`DOCUMENTATION_LIFECYCLE_POLICY` trigger map, **CI workflow → `CI_CD.md`,
`TESTING.md`**): `CI_CD` (triggers + concurrency + the same-repo-PR guard, the strictness single
source, the seven-job build matrix and what each non-packaging job is for, pipeline steps 4–7, the
AU gate, the Rosetta slice, the baseline, evidence anchors, a new **Known coverage limits** section,
and a rewritten local-reproduction section), `TESTING_POLICY` (Level 1 restated to include the
warning gate and the lints, a new Level 1b for dynamic analysis, the hard gate reworded for AU +
both formats + the nonzero seed + ambiguity, a new rule 4 requiring every lint to prove it is live,
and the strictness number **removed** in favour of the workflow's `env:` block), `TESTING` (the
pluginval section rewritten for the seed/format/bundle-override/ambiguity behaviour, the CI section
given the four new jobs and the citation-base warning, and the **AU gap struck through as closed**),
`REPOSITORY_MAP` (the tree comment, all nine script rows, the `build.yml` row). Six stale
`file:line` anchors into the two rewritten scripts were re-anchored in this same change set
(`POSTMORTEMS`, `FUTURE_RISKS`, `KNOWN_ISSUES`, `TROUBLESHOOTING`, `TESTING` ×2), which is the rule
`check-citations.py` now enforces for `src/`+`tests/`. Three pre-existing blockquote
lazy-continuation defects that `check-docs.py` found on its first run — a line-wrapped `> 1.5 oct`
landing at column 0 in `CHANGELOG`, `DSP_ALGORITHMS` and `ADR-0015` — were fixed by reflowing, with
no wording change.

**Validation.** actionlint (with shellcheck) clean on all five workflows; every pwsh step and
`run-pluginval.ps1` parsed with the PowerShell 7.4 parser; `bash -n` + shellcheck clean on all
scripts. All four lints green with their self-tests (`check-docs` 57 cases / 99 files,
`check-clang-warnings` 24 cases, `check-portability` 45 files, `check-citations` 198 anchors).
Both suites green under GCC, under Clang, and under ASan + UBSan (140 and 894 checks each time).
pluginval strictness 10 green, both modes ×3, with the seed observably pinned at `0x1`. The lld
probe reports `Success` under Clang and is **absent** under GCC, confirming the shipped Linux link
is unchanged. The new debuglink was read back off the stripped binaries. The warning gate was
adversarially checked: it fails on a new file, fails on an extra site in an already-baselined
file+flag, and ignores a new vendored warning. [Verified]

**AppleClang 21 `-Wimplicit-int-float-conversion` × 4 (0.9.4, no version bump) — a source change
that provably changes no machine code.**

The runner move below surfaced four of these; this follow-up resolves all four. Each is an `int`
operand widened inside a float expression, and each now carries the explicit `(float)` cast that
spells out the conversion the compiler was already performing:

| Site | Before | After |
|---|---|---|
| `src/PluginEditor.cpp:245` | `roundToInt (inner.getWidth() * 0.40f)` | `roundToInt ((float) inner.getWidth() * 0.40f)` |
| `src/PluginEditor.cpp:246` | `roundToInt (getWidth() * 0.40f)` | `roundToInt ((float) getWidth() * 0.40f)` |
| `src/gui/LookAndFeel.cpp:262` | `x0 + k * (barW + gap)` | `x0 + (float) k * (barW + gap)` |
| `src/dsp/VelvetNoise.cpp:30` | `std::round (m * cell + …)` | `std::round ((float) m * cell + …)` |

**Nothing is suppressed.** No `#pragma`, no `-Wno-…`, no change to
`juce_recommended_warning_flags`; the diagnostic is resolved at each site by making the intended
conversion explicit, which is also the idiom the surrounding code already uses (the two lines
above the VelvetNoise site read `(float) decorrSamps / (float) maxTaps`).

**Behaviour is provably unchanged, not argued.** `int * float` performs the usual arithmetic
conversion of the `int` operand to `float` and then multiplies; `(float) i * f` is that same
conversion written out. Verified rather than reasoned: each of the three translation units was
compiled at the shipped flags with `-g0 -fno-lto` appended (so debug metadata and LTO bitcode
cannot mask the comparison) before and after the edit — **all three objects are byte-identical**.
That covers the whole translation unit, which is a stronger statement than the scenario-matrix
twin dump used for the JUCE and C++23 changes. `src/dsp/VelvetNoise.cpp` is DSP code, so this
matters: the velvet tap grid is bit-exact, i.e. Class A.

**Verified on the diagnosing toolchain, and swept for stragglers.** On CI run `31900529457`
(`macos-26-arm64`, AppleClang 21.0.0.21000101) the macOS job's normalised warning set is
**15 sites / 108 instances, `diff`-identical to the `macos-14` / AppleClang 15 set** — the image
change added four diagnostics and this change removed exactly those four, with nothing else moved.
A full local Clang 18 build of both self-test targets (**56 compilations**) then found **zero**
`-Wimplicit-int-float-conversion` anywhere in the project sources, so no unreported site was left
behind; only the pre-existing `-Wsign-conversion`/`-Wswitch-enum`/`-Wmissing-prototypes`/
`-Wfloat-equal`/`-Wshadow*`/`-Wunused-but-set-variable` families remain. Gates re-run on the
change: 140-check DSP + 894-check state suites green, pluginval strictness 10 green in both modes
×3 locally (no retry) and on all three CI platforms.

**A rejected alternative, for the record.** `juce::Rectangle::proportionOfWidth (0.40f)` would
read better at the `PluginEditor` sites but returns `ValueType (w * p)` — a **truncation**, where
the existing code rounds. That is a behaviour change, so the cast was preferred.

**Not gated, and no CHANGELOG entry.** No parameter, serialization, threading, DSP-order or
latency surface is touched, and the machine code is identical, so no
`ARCHITECTURE_REVIEW_GATE` item applies and no ADR is warranted. `CHANGELOG_POLICY` rule 3
excludes it: nothing a user of the plug-in can observe changed.

Docs synced: `CI_CD` (its toolchain paragraph recorded these four as unfixed and now records the
fix and the byte-identical-object evidence) and the **KNOWN_ISSUES** / **FUTURE_RISKS** v0.9.4
version-sync headers, which pointed at `CI_CD` for the same four diagnostics. No other document
named them.

**macOS CI runner `macos-14` → `macos-latest` (0.9.4, no version bump) — a CI-workflow change, one
line of YAML, no source and no build-configuration change.**

`actions/runner-images` marks the macOS 14 images **deprecated**: deprecation opened 2026-07-06,
eight brownouts run through October 2026, and the labels are **fully unsupported on 2026-11-02**,
after which a job carrying `macos-14` is terminated with an error. `macos-latest` currently
resolves to **macOS 26 Arm64**. The `macos` job now uses the floating label, matching
`ubuntu-latest`/`windows-latest` on the other two jobs of the same matrix; nothing else in the job
moved — same configure line (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`,
`CMAKE_OSX_DEPLOYMENT_TARGET=10.13`), same self-tests, same two pluginval strictness-10 gates,
same `dsymutil` → `strip -x` → ad-hoc-codesign order, same `.pkg` packaging. `release.yml` reuses
this job via `workflow_call`, so the release pipeline inherits the new image with **no edit of its
own** — it is the single point of definition, and the workflow files were re-parsed to confirm
`macos` is the only job that names a macOS runner.

**Why the floating label rather than pinning `macos-15`/`macos-26`.** A pinned image is a
scheduled outage: it goes end-of-life and fails the entire matrix on removal day. The floating
label moves with GitHub's rollout, and this job's existing gates are what detect a bad image —
`lipo -archs` proves both universal slices are present (the x86_64 half is cross-compiled on an
arm64 runner), the DSP and state suites prove behaviour, pluginval proves format conformance, and
the packaging self-checks prove the `.pkg`. The trade is recorded rather than assumed: a floating
label can change the AppleClang/SDK under the shipped macOS binaries without a repository commit,
which is the same exposure the project already accepts on `ubuntu-latest`/`windows-latest` and the
same class as ADR-0027's MSVC `/std:c++latest` caveat.

**Validated by the runner it changes.** CI run `31895877794` on `macos-latest` resolved to image
`macos-26-arm64` `20260728.0273.1` and went green end to end: configure accepted
`CMAKE_OSX_DEPLOYMENT_TARGET=10.13`, the universal build succeeded, both self-test suites passed,
pluginval strictness 10 passed in **both** modes ×3, `lipo -archs` reported `x86_64 arm64` for all
three bundles, and `Anamorph-0.9.4-macOS.pkg` built with its three components and passed the
`installer -pkginfo` / `pkgutil --expand` self-checks. The degenerate-dSYM path (`-debug` upload
skipped under Release+LTO) behaves exactly as on `macos-14` — compared against the previous green
macOS job, not assumed. Linux and Windows were unaffected and green in the same run.

**The measured consequence: the macOS compiler moved with the image.** AppleClang
**15.0.0.15000309 (Xcode 15.4) → 21.0.0.21000101 (Xcode 26.6)**. Diffing the macOS warning sets of
the two runs, normalised, gives 15 → 19 distinct sites and 108 → 126 instances: nothing
disappeared, no category changed, and the whole delta is
**`-Wimplicit-int-float-conversion` at four pre-existing sites** —
`src/PluginEditor.cpp:245,246` (`getWidth() * 0.40f`), `src/gui/LookAndFeel.cpp:262`
(`k * (barW + gap)`) and `src/dsp/VelvetNoise.cpp:30` (`m * cell`), each an `int` widened inside a
float expression. **Recorded, not fixed here** — the source was unchanged by this change, so these
were new diagnostics on old code, and Level 1 is not part of the `TESTING_POLICY` hard release
gate; they are **resolved in the follow-up entry above**. Bit-exact macOS output
across the two compilers is **not claimed** — it is not provable headlessly from this repository,
and compiler-level numerical differences are the Class-B changes `DSP_POLICY.md` permits (RH-F4).
The behavioural gate is what carries the claim.

**Not a gated change and not a CHANGELOG entry.** `ARCHITECTURE_REVIEW_GATE`'s Build System item
covers CMake structure, the JUCE version/pin and the dependency set; a runner label is none of
those, and `DEPENDENCY_POLICY`'s pinned-dependency table does not list it — so no ADR, unlike
ADR-0027. `CHANGELOG_POLICY` rule 3 (user-visible changes only) excludes it, matching the
precedent HANDOVER already records for the CI-side PRs #65–#75.

Docs synced (`DOCUMENTATION_LIFECYCLE_POLICY` trigger map, **CI workflow → `CI_CD.md`,
`TESTING.md`**): `CI_CD` (the build-matrix runner cell plus a paragraph on the deprecation dates
and the floating-label choice), `TESTING` (the RH-F3/auval feasibility sentence names the runner),
and the two documents that repeat that same sentence — `KNOWN_ISSUES` KI-014 and
`RELEASE_HARDENING_PLAN` RH-F3 — plus `BUILD`'s toolchain line, whose "Verified on …" record names
the macOS compiler, and the **KNOWN_ISSUES** and **FUTURE_RISKS** v0.9.4 version-sync headers
(each states what the version's changes did to that document; both record "no entry added" with
the reason). `COMPATIBILITY_MATRIX` needed no edit: its macOS row cites the workflow, not an image
label, and the deployment target and both architectures are unchanged. `HANDOVER` needed none: its
Build Status row claims all three CI platforms green, which the new image satisfies.

**Drift found and corrected while re-reading that BUILD line (C6).** It read "AppleClang 15.4",
which is the **Xcode** version of the `macos-14` image; the compiler
`CMAKE_CXX_COMPILER_VERSION` actually reported there was **15.0.0.15000309**. The line now gives
the compiler version with the Xcode version alongside it, for both the old and the new image. The
same conflation appears in `ADR-0027` §Verification and the C++23 worklog; those are dated records
of that change and are left as written.

**Dated records left alone (C6, report-don't-rewrite).** `ADR-0027` §Verification, both C++23
worklog tables, `worklogs/RELEASE_HARDENING_AUDIT_v0.9.0.md:160` and the `macos-14` run-ID comment
in `packaging/macos/build-pkg.sh` state what was measured **on the image of the day**. They are
historical evidence, not live statements, and are not retro-edited.

**C++ standard 17 → 23 (0.9.4, no version bump) — a Build System change with a one-line source diff.**

The C++ standard sits in the `DEPENDENCY_POLICY` pinned-dependency table, so raising it is an
`ARCHITECTURE_REVIEW_GATE` Build System change: it carries an ADR (**ADR-0027**, `Accepted`
2026-08-15 — Architecture Review signed off) and was flagged on the PR for that review. `CMAKE_CXX_STANDARD` moved 17 → 23 in place
on `CMakeLists.txt:16`, so no downstream `CMakeLists.txt:NNN` citation moved; the version stays
0.9.4 and the release date stays 2026-08-15, as commissioned.

**The one source change is a finding, not a cleanup.** `src/dsp/HaasProcessor.cpp` gains
`#include <algorithm>`: libc++ stops including `<algorithm>` transitively at
`_LIBCPP_STD_VER >= 20`, so `std::fill` lost its declaration under `-std=c++2b` and **the macOS
CI job failed** — the defect was invisible to GCC/libstdc++ and to the MSVC STL, both of which
still supply the include. A `-fsyntax-only` sweep of all 27 project translation-unit compilations
under Clang 18 + libc++ + C++23 then found no second occurrence, so no wider include-hygiene pass
was made. The **C++20 fallback was evaluated and not taken**: one missing standard include is an
ordinary compatibility adjustment.

Docs synced: CHANGELOG `[0.9.4]` (a second `### Changed` entry above the JUCE one), new ADR-0027
+ `ADR_INDEX` row, `DEPENDENCY_POLICY` (pin table + a new compliance-log entry), `CODE_STYLE`
(the language line), BUILD (toolchain + the `:16-18` evidence citation), README (Requirements
**and the 0.9.4 status bullet**), COMPATIBILITY_MATRIX (toolchain pin row), HANDOVER (the
C++-standard dependency row, the snapshot paragraph, Current Version, Build Status, Release
Status and Known Blockers), the **KNOWN_ISSUES** and **FUTURE_RISKS** version-sync headers,
`.github/workflows/codeql.yml` (its header comment named the standard), and
`worklogs/CXX23_MIGRATION_v0.9.4.md`. CI_CD needed no edit — it names no standard.
`TESTING.md`/`TESTING_POLICY` needed none: no test was added, changed or removed, and the gate
is unchanged.

**A release-status correction the first pass missed.** Four 0.9.4 statements still described the
version as JUCE-only with **no `src/` change** — the README status bullet, the HANDOVER snapshot
paragraph, and the KNOWN_ISSUES and FUTURE_RISKS version-sync headers. That was true of the JUCE
upgrade alone and stopped being true of the *version* once the C++23 migration landed in it; all
four now name both changes and describe the one-line `src/` diff. The JUCE-scoped statements that
say "no source change" **about the JUCE bump** (CHANGELOG's JUCE entry, the JUCE section below)
are correct as written and are left alone.

**Why the warning counts differ between the two 0.9.4 entries.** The C++23 records cite **27
compilations / 29 instances**; the JUCE records cite **18 / 19**. Same measurement, different
command set: the JUCE cycle measured `AnamorphStateTests` alone, this one measured both self-test
targets, so the 8 shared `AnamorphDSP` sources are counted once per target and
`tests/dsp_tests.cpp` is covered at all. Re-measuring the narrower set at C++23 returns exactly
18/19, so nothing drifted. Stated in CHANGELOG, `DEPENDENCY_POLICY`, ADR-0027 and
`worklogs/CXX23_MIGRATION_v0.9.4.md` §4.4; the numbers themselves are unchanged.

**Dated records left alone (C6, report-don't-rewrite).** `ADR-0022:39`, `ADR-0026:36` and the two
JUCE worklogs each state the toolchain contract **as of that change**, where "C++17" was true.
They are historical records, not live statements, and are not retro-edited.

**One open caveat is carried rather than buried.** MSVC ships no stable `/std:c++23`, so CMake
maps `CXX_STANDARD 23` to `/std:c++latest` on Windows — a documented moving target. It is
recorded in ADR-0027 §Consequences, the DEPENDENCY_POLICY compliance entry, BUILD, HANDOVER and
worklog §5 with the two escape hatches, and it did not block adoption: the Windows job builds and
passes strictness 10 in both modes.

**JUCE 9.0.0 → 9.0.1 dependency upgrade (0.9.4) — a Build System change with no source diff.**

A JUCE bump is an `ARCHITECTURE_REVIEW_GATE` Build System change, so it carries an ADR
(**ADR-0026**, `Accepted` 2026-08-15) and the `DEPENDENCY_POLICY` rule-2 verification, and it was
flagged on the PR for human Architecture Review. The pin moved to the tag's immutable commit
`e18f7f5…`; `project(... VERSION)` moved 0.9.3 → 0.9.4 with the release dated 2026-08-15.
**No C++ source change and no build-dependency change were required**, and that is a finding
rather than an assumption: neither of the two breaking changes upstream records for 9.0.1 reaches
the project, and no JUCE module Anamorph links altered its declared system packages — across all
fourteen module headers the only metadata difference is the `version:` field, so
`scripts/setup-linux.sh` is untouched.

Docs synced: CHANGELOG `[0.9.4]`, ADR-0026 + `ADR_INDEX`, `DEPENDENCY_POLICY` (pin table,
version-lock rule, compliance log), BUILD, TROUBLESHOOTING, README, REPOSITORY_MAP,
COMPATIBILITY_MATRIX, FUTURE_RISKS (RISK-001 + the version-sync header), HANDOVER (the snapshot
paragraph + Current Version / Build Status / Release Status / Known Blockers / the JUCE dependency
row), RELEASE_PROCESS and COMMERCIAL_STATUS (the tag/release-in-preparation statements),
THIRD_PARTY_LICENSES + TRADEMARKS + COMMERCIAL_STATUS + RELEASE_HARDENING_PLAN (the pinned
version each cites), `.github/dependabot.yml`, and `worklogs/JUCE901_UPGRADE_v0.9.4.md`. CI_CD
needed no edit — it is version-free by design.

**A tag-name consequence, recorded rather than assumed.** `git tag` is still empty, so the first
annotated tag becomes **v0.9.4** — v0.9.3 joins 0.9.0-0.9.2 as written-up-but-never-cut. That
moved three live statements: `CHANGELOG_POLICY` rule 2, the CHANGELOG preamble and RISK-003's
mitigation.

**Citation sweep.** Of the 22 JUCE source files cited by name-and-line anywhere in the docs,
exactly **three** differ between the two tags (`juce_audio_formats.h`,
`juce_MouseInputSourceImpl.h`, `juce_Windowing_linux.cpp`); the other 19 are byte-identical, so
their citations stand unchanged. The three were re-anchored where a *live* document cites them
against the current pin — `THIRD_PARTY_LICENSES` §2/§4 (the FLAC/Ogg and MP3 config defaults) and
`KNOWN_ISSUES` KI-018/KI-019 (whose mechanisms are re-stated against 9.0.1, both re-verified
byte-identical in substance). `POSTMORTEMS` and the worklogs were deliberately **not** touched:
each is a dated record that names the tree it was traced in ("from the pinned JUCE 9.0.0
source"), so its line numbers are correct for that tree — the same period-correct-history rule
the 8.0.14 → 9.0.0 migration applied.

**Drift found and reported (C6), not silently carried.** `.github/workflows/codeql.yml` still
described the dependency as "JUCE 8.0.14" — stale since the 9.0.0 migration, i.e. pre-existing,
not introduced here. The sentence's point is version-independent, so the minimal correction was
to drop the number rather than restate it, which also stops the line drifting again.

**What is verified and what is not.** Verified headlessly: 32/32 twin-dump hashes **and**
reported/predicted latencies identical across the two JUCE versions (the ADR-0022 harness re-run,
with the 32 hashes mutually distinct so the matrix discriminates); 140-check DSP and 894-check
state suites green, including the 8.0.14-frozen parameter-registry snapshot; pluginval strictness
10 green locally in both modes ×3; a byte-identical compiler-warning set (19 instances) across the
18 project translation units built against both trees; and the `RELEASE_POLICY` third-party
re-verification (JUCE's `LICENSE.md` and all twelve cited licence files byte-identical). **Not
verifiable headlessly, and therefore done by hand:** the Level-5 manual audition — 9.0.1 changes
editor-adjacent framework code (Linux message-loop scheduling, display enumeration, vblank period;
Windows Direct2D edge painting; macOS Metal-layer guards), and appearance/feel is a human
judgement. That audition was **performed against this build on 2026-08-15**, so **ADR-0026 is
`Accepted`**; the same audition discharges the one ADR-0022 had left open for the 9.0 line, which
is now `Accepted` too. Two of the four long-standing tag blockers close with it (`HANDOVER`
§Release Status): the compatibility checklist and the missing licence remain.

**Probe-state fix (0.9.3) — the staging probe could decide future installs.**

The hard-link probe that chooses the staging location writes one marker into the plug-in directory,
and `ln` refuses an existing target. A run killed between creating and removing that marker left it
there for good, and from then on the probe failed on **every** later run — pinning staging to the
in-scan-path fallback permanently, which is exactly the arrangement the round before had moved away
from. Reproduced against the tree before fixing: with a marker pre-placed in `~/.vst3`, the install
succeeded but staged inside the scan directory. `uninstall.sh` did not list the marker either, so it
survived a full uninstall and contradicted the "leaves nothing that survives a deliberate uninstall"
line in `PACKAGING.md`.

The fix is to make the marker **stateless** rather than to chase its cleanup: it is removed up front
on every run, before the probe and before the recovery paths that never probe, so a leftover can only
ever be litter. Deleting it only *after* use would not have been enough — that is the same
kill-between-two-commands window that created the bug. `-rf` rather than `-f`, so even a directory
under that name cannot pin the choice; nothing but the probe is ever written to that path. The
uninstaller's scratch list gained the marker so an interrupted install leaves nothing behind.

Verified by execution in both modes: stale marker (file and directory), `INT`/`TERM`/`HUP`/`KILL`
delivered inside the probe window itself — after which the installed plug-in is untouched, because
the probe runs before any staging, and the **next run stages outside again** — repeat installs
leaving no marker, and uninstall clearing one. The full transaction matrix was re-run unchanged, so
the stage-and-swap and recovery guarantees are intact. No new known issue: this defect is fixed, not
carried.

**Consolidated installer round (0.9.3) — the transaction finished, and the limitations registered.**

*The transaction, completed rather than patched again.* Two prior rounds each fixed the failure the
review named and left the adjacent one standing; this round took the whole lifecycle. What changed
beyond the previous fix: staging moved **out of the DAW scan path** (`.anamorph-install-stage` next
to the plug-in directory), chosen by a **hard-link probe** — the one operation that cannot cross a
filesystem, where the obvious `mv` probe is no test at all because it silently falls back to
copy-and-unlink, and `~/.vst3` may be a symlink onto another mount. A false negative falls back
inside the plug-in directory and costs only the scan-path property, never atomicity; this was
verified against a real second filesystem, which is also how the missing `mkdir` for that fallback
was found. Both modes now share one `choose_stage_dir`/`reconcile`/`arm_traps` implementation
instead of two near-copies that had already drifted once. The elevation prefix is declared once at
the top and stays empty on the per-user path.

*Sequencing bug found by testing, not reading.* `reconcile` sweeps an empty stage directory as
scratch, so creating that directory **before** the opening reconcile meant an upgrade (destination
present) had its stage directory deleted underneath it. Creating it after reconcile is the fix. The
same class as the previous round's finding: the recovery helper must not destroy what the
transaction still needs.

*Uninstall made consistent with what install owns.* `uninstall.sh` removes both possible stage
directory locations and the staged Standalone, by exact name, so an interrupted install leaves
nothing that survives a deliberate uninstall. It matches no patterns and touches no user data.

*Coexistence warning.* A per-user install now detects an existing system-wide one (`test -e`, no
elevation) and names what is still installed there plus the `sudo ./uninstall.sh` that clears it.
Detection only — the maintainer's decision, recorded here; removing it would need exactly the
elevation the per-user mode exists to avoid.

*macOS assertion strengthened from name to semantics.* Proving `<bundle-version>` is *producible*
did not prove its membership tracks `BundleIsVersionChecked`. The build now runs a controlled A/B on
one payload — same bundle, packaged with pkgbuild's defaults and with the patched plist — and
requires each list to appear with the key on and vanish with it off. `<upgrade-bundle>` gained an
assertion too, which closes the one patched key that had none. The stale "nested bundles are covered
too" claim was corrected in both the script and `PACKAGING.md`: nested bundles appear under the
parent's `ChildBundles`, are not patched, and do not need to be.

*Guarantees reconciled with what the code actually provides.* "Nothing half-installed" was too
strong: the VST3 and the Standalone are two artifacts, each replaced atomically, and a failure
between the two commits leaves a new VST3 with the previous Standalone — both valid, a mixed pair.
`PACKAGING.md` now states the guarantee per artifact with a point-of-failure table, the CHANGELOG
entry says what the user actually gets, and the macOS "every selected component is written" line
notes that components install in sequence with no rollback.

*Registered, not silently solved (DOCUMENTATION_LIFECYCLE trigger: new unresolved limitation).*
**KI-021** (Linux per-user install does not displace a system-wide one) and **KI-022** (macOS
non-relocatable packaging leaves a user-moved copy behind) now exist in `KNOWN_ISSUES.md`, the list
testers and the release checklist actually consult, with the procedure docs cross-referencing them.
Both are deliberate trades and neither had a register entry despite being documented in packaging
procedure — the gap the review named.

*Maintainer sign-off, 2026-08-11 (recorded, not re-requested).* The judgement calls in this round —
that the coexistence warning is wanted, that both limitations are documented rather than
behaviourally redesigned, and that the macOS semantic assertion is required — were approved by the
maintainer in advance. This is a **decision** sign-off; it is not manual testing and is not recorded
as any.

**Final review round (0.9.3) — two defects in the packaging round, plus the visual sign-off.**

*The fix reproduced the defect's own shape.* The INC-012 prevention assertion for version checking
was keyed on `<version-check>`, an element pkgbuild never writes — so it passed unconditionally. A
check that cannot fire is the same silent success as an install that cannot install, which makes this
worth recording rather than quietly correcting: the sibling `<relocate>` assertion was named
correctly and did work, and the asymmetry is invisible in a green build. In `PackageInfo` these
states are membership lists (`<relocate>`, `<bundle-version>`), so the name is now correct **and
proved on every build** — a throwaway component built from the same payload with the defaults left
on is relocatable and version-checked by definition and must match both patterns, and if it does not
the build stops and prints that `PackageInfo`. Verifying an assertion by construction rather than by
inspection is the transferable part; the same reasoning produced the `PackageInfo` **count** guard in
the round before, for the same class of vacuous pass.

*The replacement path destroyed what it was replacing.* `install.sh` removed the installed
`Anamorph.vst3` before copying its replacement, so a copy that failed part-way left the user with no
plug-in at all — contradicting the "nothing half-installed" guarantee this file and `PACKAGING.md`
already asserted. Both modes now stage beside the destination and swap. Two notes: the **per-user**
path had the identical defect and was fixed with it (the review cited only the system-wide path, but
per-user is now the *default* path, so fixing one and leaving the other was not a defensible release
state); and staging beside the destination rather than in `/tmp` keeps the final step a
same-filesystem rename, which additionally replaces a **running** Standalone that `cp` refuses with
`Text file busy` — verified directly, along with a control run proving the pre-fix script destroys
the install under the same injected failure.

*Wording.* The installer's title is now `Anamorph Linux Installer`, matching
`Anamorph Linux Uninstaller`; the two docs that quote the prompt were updated with it.

*Follow-up: the stage-and-swap still had an interruption window.* The swap deleted the destination
before renaming the staged copy in, so between those two commands the staged copy was the only
one — and the cleanup handler removed exactly that on the way out, turning a Ctrl-C into total
loss. The stage-and-swap shape was right; the **order** was not. This round made the old bundle move
**aside** instead of being deleted, so a complete copy exists at every instant, and made cleanup
restore rather than only delete. (The staging paths it used were superseded by the consolidated
round above, which moved them out of the DAW scan path; the ordering property is unchanged.) Three things this round established that are worth
carrying forward: `EXIT` traps are **not** enough for interruption — dash, `/bin/sh` on
Debian/Ubuntu, does not run them when the script is signalled, so `INT`/`TERM`/`HUP` are trapped
explicitly (measured, not assumed); the restore must be **ordered before** the scratch removal
*and* the parked copy kept until the destination is repopulated, a flaw the first draft of this fix
still had and the failure tests caught; and `SIGKILL` — which no handler covers — now leaves the
old bundle parked and recoverable, with the next run's opening `reconcile` restoring it. Verified
by injecting a failing commit rename and by delivering `INT`/`TERM`/`HUP` inside the window in both
modes (elevated included), each against a control run of the previous script that ends with nothing
installed. `PACKAGING.md`'s mechanism paragraph is corrected; its guarantee wording already
described the intended behaviour and stands unchanged.

*Inspected and accepted as non-blocking, with reasons.* A per-user install does not displace an
existing system-wide one — real and, for 0.9.3, the likely upgrade path, but the fix would need the
elevation that mode exists to avoid, so it is **documented** (`INSTALL.txt`, `INSTALLATION.md`,
`PACKAGING.md` §Not chased) rather than coded, mirroring the macOS stale-copy treatment. The `read`
EOF fallback discards a value only when a user types an answer and presses Ctrl-D instead of Enter on
a tty; the cited pipe repro cannot reach it at all, because a pipe fails `[ -t 0 ]` and never
prompts. `plist_put`'s Set/Add fallback is covered by the assertions now that the version key has a
live one. The remaining items (pkgbuild bundle classification, per-component postinstall
non-atomicity, the earlier pop-up/tooltip/focus/z-order/`SpectrumImager` findings) were re-read and
need no change; the macOS guarantee text was checked and does not claim atomicity. The
permission-denied message stays as written — it is maintainer-specified wording (C8), even though it
now also prints for non-permission failures.

*Sign-off recorded (2026-08-11).* The maintainer approved the **visual** items: the equal-width Widen
/ Style-Focus row is intentional, the narrower Simple-mode Widen control is acceptable, the current
pop-up/menu width behaviour is acceptable, and the remaining visual verification items are approved.
Recorded in `TESTING.md` (ADR-0025 disclosure 2) and `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md` §7
and §10. **Scope of that sign-off:** visual/UI only — the behavioural per-platform checks and every
**installer** check (the macOS four-case re-install matrix, a DAW finding `~/.vst3` on Linux) are not
covered and remain owed.

**Packaging round (0.9.3) — `packaging/` only, no `src/` change.** Two independent installer items,
both requested with an explicit scope restriction to their own platform, and both verifiable only
where CI does not go.

*Linux — an install that no longer needs root.* `install.sh`/`uninstall.sh` now prompt for one of two
modes and **default to the per-user one** (`~/.vst3` + `~/.local/bin`), which matches how Linux DAWs
actually scan: `~/.vst3` is the VST3 standard's per-user folder and a default path in
REAPER/Bitwig/Ardour, so nothing is lost by not writing to `/usr`. The design decisions worth
keeping: elevation is **per operation** (`priv() { $SUDO "$@" || fail; }`), never a re-exec of the
script through `sudo`; **root skips the prompt** and installs system-wide, so the previously
documented `sudo ./install.sh` keeps its exact old behaviour rather than becoming a per-user install
into `/root`; a **non-tty stdin** takes the default instead of blocking; and every unrecognised answer
falls back to the default, as specified. The two failure paths fail **closed** — no `sudo` on `PATH`
and a `sudo` the user cannot authenticate both print their message and exit 1 with nothing
half-installed. Verified on Linux against a stubbed payload across the mode matrix, the failure paths
and install→uninstall round-trips in both modes (recorded in `TESTING.md`); what that cannot show is a
real DAW finding `~/.vst3/Anamorph.vst3`, which stays a manual check. Docs synced per the lifecycle
trigger (**Packaging** → `PACKAGING.md`, `RELEASE_PROCESS.md`) plus the user-facing carriers that
asserted the old behaviour: `packaging/linux/INSTALL.txt`, `docs/user/INSTALLATION.md`,
`USER_MANUAL.md` §2.1, `README.md`, `REPOSITORY_MAP.md`, `CI_CD.md` §8.

*macOS — INC-012, an installer that reported success without installing.* `pkgbuild` marks every
bundle it finds **relocatable by default**; Installer.app then resolves the destination by looking the
bundle identifier up in the receipt/Spotlight database and writes over **whatever copy it finds**,
using `--install-location` only when the lookup comes up empty. Move `/Applications/Anamorph.app`
elsewhere — dragging it to the Trash counts, since that is still a file on the volume and still
indexed — and the next install reports success while `/Applications` stays empty. `build-pkg.sh` now
patches the plist `pkgbuild --analyze` produces (rather than hand-writing one, so
`RootRelativeBundlePath` matches by construction and nested bundles are covered) and passes it back
via `--component-plist`. The audit-relevant point is **which** claim was wrong: the pre-fix build-time
self-check verified the *package* thoroughly — three component identifiers, `customize="allow"`, all
choices pre-selected — and that check was correct and remains; relocation is simply not a property of
the archive, only of install-time behaviour, so no amount of package inspection could have caught it.
The new assertions therefore cover the two things that *are* inspectable (no relocatable and no
version-checked bundle in any `PackageInfo`; `pkgutil --expand-full` payload completeness), and the
rest is now an explicit **coverage gap**: `TESTING.md` §"Gaps in the automated coverage" gained a
fifth bullet — *no gate ever installs anything* — carrying the owed four-case re-install matrix per
format. `PACKAGING.md` gained §"macOS reinstall behaviour (idempotency)" recording the destinations,
the guarantee, the three plist keys and the receipt assumption (receipts are still written but are
never read to decide where or whether to copy, so `pkgutil --forget` is never needed to make an
install work).

*Drift found and corrected (C6).* `HANDOVER.md`'s Current Version row still ended the menu-width
paragraph with "no layout code changed in the cycle" — true when written, contradicted by the
equal-width Widen row that landed later in the same cycle. The 2026-08-11 round corrected this
sentence in this file (see the layout entry below) but not the HANDOVER instance. Corrected to name
the Widen row as the cycle's one deliberate layout change; the entry-count in the same row (six
Fixed / two Changed) is now seven / three.

**Follow-up round on the pop-up work (0.9.3).** Four items, three of them corrections to the change
set itself. *(1)* The shield is **always visible and inert**, with only its *interception* toggled —
the shape `dimOverlay` already uses here, so it is one fewer idiom. The reason is `setVisible`'s
repaint cost, not hover: raising the shield cannot disturb hover at all, because every fake mouse
move involved is **asynchronous** and therefore dispatched once the menu is already modal, at which
point `internalMouseEnter`/`internalMouseExit` early-return for every blocked component — and this
editor derives hover **geometrically** rather than from enter/exit in any case. *(2)* The claim that
the preset menu cannot reach the look-and-feel hook was
**re-verified and holds**, with a sharper reason: `MenuWindow` binds `auto& lf = getLookAndFeel()`
*before* parenting and calls `preparePopupMenuWindow` through that bound **reference**, which
parenting cannot rebind — so the separate counter stays and the comment now carries the real
argument. *(3)* The 24 Hz backstop's comment was read twice as covering `presetMenusOpen`; it covers
`openMenus`, and the counter needs no cover because `showPresetMenu` always adds three unconditional
items, so `createWindow` can never return null — the one path that drops the callback. Comment
corrected; **no recovery machinery added for a statically unreachable state**. *(4)* **Tooltips**:
disabling them left a visible tip up and a quick move could raise another, because the setting only
lengthened `millisecondsBeforeTipAppears` and `TooltipWindow::timerCallback` bypasses that delay
entirely while a tip is showing. `getTipFor` is virtual, so tooltips are now switched off **at the
source** and JUCE's own state machine hides rather than shows; `hideTip()` makes the transition
immediate. Filed as **KI-018**, not fixed: the dismissing click is consumed by the shield but still
counts toward JUCE's multi-click run (`registerMouseDown` is component-agnostic), and every lever is
out of bounds — no reset API, a process-global double-click timeout (the KI-017 objection), or
per-control guards that undo the shield's whole point. `COMMERCIAL_STATUS.md` was the one carrier the
0.9.2 → 0.9.3 sweep missed; only its three current-release statements changed, its historical ones
and its review date stand.

**Windows CI failure: a portability defect, and a workflow guard that hid it (0.9.3).** The Windows
job reported `expected exactly one Anamorph.vst3 bundle, found 0` from its staging step. The cause was
a compile error ~90 lines earlier: `constexpr int algoGap` was a block-scope constant read from a
capture-less lambda, which GCC and Clang accept (reading a constexpr value is not an odr-use) and MSVC
19.51 rejects (`C3493`). Moved with its helper to **file scope**, where no capture question arises; a
sweep of all 22 capture-less lambdas in `src/` confirmed it was the only instance. The second defect
is the one worth remembering: `build.yml` gated the randomise-pluginval and staging steps on
`if: ${{ !cancelled() }}`, which is **true after any upstream failure**, so a compile error let both
run against a tree with no plug-in in it and the job's last error was a cascade. Every platform's build
step now carries `id: build` and every consumer of build output is gated on
`steps.build.outcome == 'success'`; `!cancelled()` is kept alongside it so a *pluginval* failure still
stages a beta artifact. The same bare guard existed on Linux (randomise) and macOS (randomise +
packaging), so all three were fixed; `release.yml` and `msvc.yml` use default `success()` semantics and
were never exposed. Documentation-visible outcome: `build.yml`'s header now states the invariant —
every step consuming build output names the step it depends on.

**CI gating completed on Linux, and release dates reconciled (2026-08-11).** The previous pass gated
every consumer of build output on `steps.build.outcome`, which was right for Windows and macOS and
one step short on Linux: there the strip/objcopy step sits between the build and pluginval *because*
the release gate is meant to validate the stripped bytes, so the producer the randomise gate must name
is `strip`, not `build`. Its deterministic sibling already had that gate for free from default
`success()` semantics; the randomise step, carrying an explicit `if:`, had to say so. `strip` subsumes
`build` (it has no `if:` of its own, so a failed build leaves it `skipped`). `build.yml`'s header
invariant is sharpened accordingly: every step names the step that **produces what it consumes** —
which is `build` on Windows and macOS, and `strip` on Linux. Separately, the 0.9.3 **release** date is
now 2026-08-11 everywhere it appears as a release or change-set date (CHANGELOG, HANDOVER, README,
this file's "Last updated", the worklog header). Dates that record an **event** — INC-011's fix-commit
date, the 2026-08-09 manual-verification and sign-off records — keep the date they happened on, per
`POSTMORTEMS.md`'s "dates are the fix commit dates" rule; conflating the two would rewrite history to
tidy a heading.

**Maintainer sign-off on the remaining 0.9.3 review items (2026-08-11).** Reviewed and accepted with
no code change, on the basis that each is a recorded observation rather than a current correctness or
user-visible problem: the tooltip gate depending on `getTipFor` being the only path that can raise a
tip; combo menus outliving the editor's look-and-feel members (pre-existing, and narrowed by the
destructor cancel); the shield z-order invariant being unenforced; `getChildren()` reordering during
`exitModalState`; the repeated idempotent cancel attempts while the editor stays hidden; preset-menu
double-tracking being benign if its premise ever changed; and `SpectrumImager::mouseExit` reaching the
repaint gate through the eased alphas rather than `frameDirty`; the menu chrome budget being +10 px
against 0.9.2 for every menu whose width is text-derived, and the new 64 px floor for a degenerate
one-glyph item (nothing in `src/` produces one) — both are the intended consequence of deriving the
budget from what the drawing spends, and the one place they meet the narrowed Widen box is a visual
check, not a defect. Verified-correct observations
(pop-up feeder coverage, listener teardown ordering, the foreground probe's self-healing sampling, the
inline-edit cancellation reaching exactly the two commit-on-focus-loss paths, the hover snapshot's
completeness) are recorded as confirmations, not actions. The **one** review item that did change code
in this pass is the Windows CI defect above; the layout and CHANGELOG items are documentation.

**Widen / Style / Focus laid out as equal halves (0.9.3, approved design intent 2026-08-11).** The
row reserved a hard-coded 100 px on the right, so WIDEN and its Style/Focus companion were visibly
unequal (156/94 in Simple, 160/94 in Advanced) and the seam between them sat right of centre. They are
now equal width with the gap centred on the column — one constraint, not two: taking the same slice
off each end leaves a gap whose midpoint is the row's midpoint for odd and even widths alike. The
Style/Focus label takes the identical slice from the identical row width, so it is left-aligned with
its box by construction rather than by a second constant kept in step; the WIDEN label keeps the
remainder and does not move. Both edges move left (−31 px Simple, −33 px Advanced). **No
look-and-feel path is involved** — an earlier round had read the request as being about pop-up list
width and introduced `useLegacyMenuWidth` / `widenCombo`, which is reverted in the same commit.

**Focus release narrowed to the application-switch branch (0.9.3, approved 2026-08-11).**
`dismissOrphanedPopupMenus` was releasing keyboard focus on both of its triggers, but only the
app-switch one needs it — suppressing `PopupMenuCompletionCallback`'s `toFront (true)` requires a
window the user has moved *away* from. On the hidden-editor branch the window being re-fronted is the
one they are still working in, so the release bought nothing and cost two things: a re-shown Save
Preset dialog came back with its name field unfocused (the KI-009 class of symptom, and
`focusSaveNameField` is not re-armed by a re-show) and an in-progress inline edit was discarded. The
cancel itself remains unconditional on both branches; only the focus handling is scoped.

**Maintainer sign-off on the residual pop-up limitations (2026-08-11).** Reviewed and **accepted as
documented limitations rather than defects**, closing them for this release: **KI-019** (Linux/X11
never observes an application switch, so that third dismissal is inert there — the platform's
foreground flag is a write-once latch; inert in the safe direction, and the hidden-editor and
destroyed-editor halves work normally) and **KI-020** (pop-up modality is process-global, so with two
Anamorph editors open the dismissing click can still reach the *other* instance's control —
pre-existing, and 0.9.3 closed only the same-instance half). Neither is to be redesigned for 0.9.3:
no broader pop-up architecture change is required. Also accepted unchanged in the same pass: the
sub-menu arrow sitting outside the named width budget (no menu in `src/` uses `addSubMenu`), the
`minimumWide` floor's comment overstating its guarantee (every combo is far wider than 64 px in both
modes), section headers measured in the item font (over-measurement can only widen), the compact
combo lists inheriting the derived budget, the tooltip gate's 42 ms tick latency, future overlay /
z-order hardening, and the cosmetic stale-hover residue after a menu closes. The sign-off covers the
**decision to document rather than change**; it is not a manual test of the implementation and
touches no release gate.

**A stranded pop-up's focus release could apply a half-typed value (0.9.3).** The app-switch dismissal
releases keyboard focus before cancelling, so JUCE's completion callback cannot re-front the host
window. The first revision asserted that was free — on the strength of a sweep that covered
`PluginEditor.{h,cpp}` and not `src/gui/`. Two inline text edits treat losing focus as *"the user
clicked away"* and **apply** what is in the box: `SpectrumImager`'s crossover-frequency chip
(`freqEditor->onFocusLost` → `commitFreqEditor`, a parameter write inside a change gesture plus a
`projectGaps` nudge to the neighbouring splits) and a slider's value box (`createSliderTextBox` builds
the `Label` with `lossOfFocusDiscardsChanges = false`). Either one turns switching application into a
parameter write the user never asked for, with an automation and undo step to match. `cancelInlineTextEdits()`
now runs first and ends both with the **Escape** outcome instead — `SpectrumImager::cancelInlineEdit()`
clears `editingHandle` so the later asynchronous `onFocusLost` finds nothing to commit, and
`Label::hideEditor (true)` is literally what `Label::textEditorEscapeKeyPressed` ends in. Normal
click-away, Return and Escape are untouched, and `saveNameEditor` is deliberately excluded — it has no
focus-loss handler, so its text stays put, which INC-011 requires.

**Review sign-off on the 0.9.3 pop-up round (2026-08-10).** Two review passes raised nine further
items; the maintainer reviewed each and **accepted the current implementation** on six, which are
therefore closed rather than open: the **pop-up width** growing on every menu (intentional visual
adjustment — kept, though the round after trimmed its discretionary half, see the Menu width entry
below); **unconditional shield
`toFront`** on every raise-path refresh (not required — the "nothing intercepting is brought to front
while the shield is raised" invariant holds today and is documented); the shield **staying frontmost**
after the first pop-up (accepted with the current overlay ordering); **`presetMenusOpen` recovery
machinery** (not required — the counter cannot leak, see *(3)* above); **`SpectrumImager::mouseExit`
setting `frameDirty`** (not required — clearing a hover index always moves an ease target, so the tick
gate already opens); and the shield **swallowing scroll and pinch** for as long as a menu is open
(part of the interaction contract, not only the dismissing event). Four were **actioned**: the
PopupShield hover explanation (corrected — the mechanism is asynchronous fake moves plus modal
blocking plus geometric hover, not raise ordering), the worklog's superseded predicate section (now
banner-marked), the **tooltip delay redundancy** (`tooltipsOn ? 600 : 0x3fffffff` removed; the 600 ms
now lives only at the member's construction), and — reversing an earlier accept — the
**hidden-editor pop-up lifetime**, promoted to a fix once the second pass traced its user-facing cost.
The sign-off covers the **direction and the accepted-as-is decisions**; it is not a manual test of the
implementation and touches no release gate.

**A pop-up could outlive the plug-in window (0.9.3, third Fixed entry).** INC-010 gave the preset menu
a parent so that hiding or destroying the editor cancels it; it could not do the same for a ComboBox
or TextEditor drop-down, which JUCE builds as a free-standing **desktop** window with no ancestor in
common with the editor. The watcher that performs that cancel — `ModalComponentManager::ModalItem`, a
`ComponentMovementWatcher` firing on `! isShowing()` and on the deletion of the component *or a
parent* — registers on the modal component and its ancestors, so for a desktop menu it only ever sees
the menu's own visibility and lifetime. `MenuWindow::windowIsStillValid` is no help either, comparing
two `WeakReference`s to the target control that both survive a hide. **Three** ways in, found across
two review rounds: the host **hides** the view, the host **destroys** the editor (the destructor
removed the component listeners but never asked the window to go away), and — maintainer-confirmed,
and the worst — an **application switch with the pointer resting on a menu item**, where JUCE's own
app-change dismissal does not fire because `MouseSourceState::checkButtonState` gates it on
`! reallyContained`. That last one is not desktop-specific: the parented preset menu has the identical
hole. Stranded, the menu is a floating always-on-top strip over a window that is gone (INC-010's exact
reported symptom, one menu type later), still modal and so still blocking every JUCE component in the
process, still counted in `openMenus` so the returning editor spends its first click dismissing it —
and in the app-switch case, clicking it pulls a background plug-in window back in front. One function
now cancels every pop-up the editor owns, in two passes because no single hook sees both kinds
(`openMenus`, plus any **modal child** — which identifies the parented preset menu exactly, since
nothing else the editor owns ever enters a modal state), called unconditionally from the destructor
and conditionally from the 24 Hz tick on `! isShowing()` or a genuine application switch. The tick is
the only observer available: an ancestor's `setVisible`, a peer change and a minimise all end at
`isShowing()` without notifying us, and an app switch has no `Component` event at all. The app-switch
half is **self-calibrating**, after a first attempt got it wrong. `Process::isForegroundProcess()` is
only half of JUCE's own test; the other half — which covers a plug-in whose editor lives in a window
owned by a different process — is module-internal, and the first revision argued it was safe to skip
because Anamorph ships VST3 / AU / Standalone rather than AUv3. That conflated the *format* with the
*hosting mode*: whether a plug-in runs inside the host's process is the **host's** choice (Bitwig
gives every plug-in a helper process by default; bridged and sandboxed hosting does the same), so a
plain VST3 hits it, the call reads `false` permanently while the editor is in active use, and every
menu was cancelled within one tick of opening — the controls were unusable with the mouse. The editor
now records what that call reads at the moment a pop-up **opens**, which only a click on one of its
own controls can produce, and treats a later `false` as an app switch only if it read `true` then;
where it never reads `true`, JUCE's own dismissal remains the only cover, which is exactly the
pre-0.9.3 position rather than a regression. Deliberately **not**
`PopupMenu::dismissAllActiveMenus()`, which is process-global and would close another instance's menu
— the objection that already ruled it out in INC-010. Nothing changes while the plug-in is in front of
the user, so the dismissal contract, the shield's z-order and the one-click behaviour are untouched.

**Menu width: the discretionary part removed (0.9.3).** The width fix summed the chrome
`drawPopupMenuItem` actually spends (12 + 14 + 12 = 38) and then added 12 px of "breathing room" on
top, for 50 against the previous flat 30. That allowance widens **every** menu drawn through the
look-and-feel — including the combo drop-downs — wherever the item text rather than
`withMinimumWidth (box.getWidth())` is the binding constraint, so the discretionary part was silently
changing the relationship between a control and its own list, and the Widen/Style/Focus layout
contract outranks pop-up padding. The margin is now 2 px and is no longer discretionary: it is a
rounding guard, because `drawPopupMenuItem` uses `Graphics::drawText`'s three-argument overload whose
`useEllipsesIfTooBig` defaults to true, so text measuring one sub-pixel over the strip would ellipsise
rather than overhang. Total 40 — still ≥ the 38 actually spent, so the *"Select All"* clipping fix is
intact and now exact. A later round briefly had the **Widen / Style / Focus** combos opt out of the
budget entirely (`useLegacyMenuWidth` plus a `widenCombo` instance); that answered the request in the
wrong dimension and was **reverted** — see the layout entry below. Every menu shares the one budget. This paragraph once ended by asserting that **no layout code changed** in
this cycle. That was true while the menu-width work was the only `resized()` edit on the branch, and
became **false** with the equal-halves commit, which rewrote `layoutAlgoRow` and both `algoOptLabel`
rows — so the claim is withdrawn rather than left contradicting the Widen/Style/Focus layout entry
above, which is the accurate record. What survives it is the narrower point it was making: the
menu-width work *itself* touched no layout code, and no `LookAndFeel` combo/label sizing or drawing
method has been changed at any point in this cycle.

**Pop-up dismissal became one mechanism instead of one predicate (0.9.3).** Verification of the
Settings fix found the same defect on the Save Preset dialog, where it *destroys typed input*: a
right-click opens `TextEditor`'s context menu, and the click that dismisses it was re-delivered to
the backdrop, which closed the dialog. The Settings predicate could not be extended — `TextEditor`'s
menu state is private, and JUCE exposes no universal "this click just dismissed a pop-up" signal
(all three candidates were read in the pinned tree and all three fail; the table is in the worklog).
So the editor now owns the state: `AnamorphLookAndFeel::preparePopupMenuWindow` catches every menu
built through our look-and-feel (ComboBox and TextEditor both set it), the preset menu is counted
directly because its own look-and-feel is null at that moment, and a single transparent
**`PopupShield`** takes the click. The Settings-only predicate was **removed**, not kept alongside —
the contract is "the dismissing click touches nothing underneath", and *underneath* includes controls
that act on the press (`ABControl` toggles A/B, `SpectrumImager` can add a band), so one shield is
both smaller and more complete than a predicate per control. The riskiest property is proved from the
source rather than left to a GUI test: the shield cannot be raised in front of a menu, because
`MenuWindow` sets `alwaysOnTop` and `Component::toFront` inserts a non-always-on-top component behind
every always-on-top sibling. **Two menu-rendering fixes rode along**, both in the shared
look-and-feel rather than patched per menu: `getIdealPopupMenuItemSize` allowed 30 px of chrome
against a layout that spends 38, so the longest item was measured narrower than it draws and JUCE
clipped *"Select All"* to *"Select ..."* — the allowance is now summed from named constants the
drawing code uses, so the two cannot drift; and `drawPopupMenuItem` was **ignoring its `isActive`
argument**, so disabled entries rendered identically to live ones — now dimmed at the 0.4 alpha this
file already uses for a disabled button. All of it is editor-only and joins the existing ADR-0025
entry in `TESTING.md` §Gaps. Maintainer sign-off (2026-08-09) covers the **problem reports and the
required contract**; it is not a manual test of the implementation and touches no release gate.

**The two interaction bugs this cycle opened with, both with non-obvious mechanisms (0.9.3).** *(1)* The Multiband **add-split
preview line** stalled under a moving pointer. The S2 repaint gate skips a frame when nothing it
watches moved — spectrum data, eased alphas, drawn split/width positions — on the stated assumption
that "mouse-driven fields [have] handlers [that] already repaint explicitly". `updateHover()` was the
handler that did not, and `addX`, the preview line's X, is none of the three things the gate watches.
So with the pointer moving *within one band's add zone* (hoverAdd unchanged, `addA` already at 1.0)
over a *settled* spectrum, nothing moved and the line froze; crossing into another hotspot moved an
alpha and it jumped to the cursor. `updateHover` now marks the frame dirty when its output changed —
`frameDirty` rather than `repaint()`, so painting stays paced at one frame per vblank, which is what
the gate is for. The gate itself is untouched: an idle view still stops repainting. *(2)* A **Settings
drop-down's dismissing click also closed Settings**, because JUCE *deliberately re-delivers* it:
`internalMouseDown` dismisses the modal menu via `internalModalInputAttempt()` and then, seeing the
modal loop has exited, passes the same mouse-down to the component underneath
(`juce_Component.cpp:2507-2544`). The **shipped** answer is the editor-level `PopupShield` described
in the entry above — a single always-visible, normally-inert overlay that starts intercepting while
any pop-up is on screen, so the dismissing click reaches no control at all. (The first attempt at
this fix was a Settings-only predicate on `Backdrop` reading `ComboBox::isPopupActive()`; it was
**removed** within the same PR once the same defect turned up on the Save Preset dialog, where
`TextEditor`'s menu state is private and the predicate could not reach it. Nothing named
`swallowsDismissClick` or `isPopupActive` survives in the editor — the shield is the mechanism.)
Reasoning, edge cases and the JUCE-signal analysis: `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md`.
**Neither fix has an automated test** — both are
editor-interaction defects and the harness instantiates no editor and drives no pointer; registered
as a second **ADR-0025** exception with its four disclosures in `TESTING.md` §Gaps, beside INC-010.
The Save Preset case is also filed as **INC-011** — it destroys typed user input, which clears the
same bar INC-008 set for a pure GUI-interaction regression, and `DOCUMENTATION_LIFECYCLE_POLICY`'s
trigger map ("Fix a notable incident → `POSTMORTEMS.md`") therefore applies. Its most transferable
finding is not the JUCE mechanism but the process one: the Settings drop-down was fixed first with a
`ComboBox::isPopupActive()` predicate, a design *incapable* of expressing the `TextEditor` case, so
no amount of testing that fix could have reached its sibling — a fix scoped to what exposed a defect
rather than to its cause cannot find the rest of the class.
**Version carriers swept** for the 0.9.2 → 0.9.3 bump: `CMakeLists.txt`, `CHANGELOG.md`, `README.md`,
`HANDOVER.md`, `KNOWN_ISSUES.md`, `FUTURE_RISKS.md`, `RELEASE_PROCESS.md`, `RELEASE_HARDENING_PLAN.md`,
`CHANGELOG_POLICY.md`, ADR-0024 — every place naming the *release in preparation* or the *first
annotated tag*, which is now **v0.9.3** (0.9.0, 0.9.1 and 0.9.2 were each written up and superseded
before a tag was cut). Historical references to what 0.9.2 introduced are left as they are.

---

Previously: for the **0.9.2 change set** (2026-08-07) — the first `src/` change since 0.9.0.
Four changes, one investigation, three new regression tests, and one governance amendment.

**Governance: `TESTING_POLICY` rule 1 gains a narrow exception (ADR-0025).** The rule ("every bug fix
ships a regression test") was stated unconditionally, while the project has in practice shipped one
fix — INC-010 — without one, because no automated surface reaches a defect that only exists while a
modal child is open and its owner is destroyed. That deviation had been recorded in a Procedure and
in this ledger, both of which rank **below** Policy, so nothing at or above Policy level described
what the project actually does. **ADR-0025** closes that: the default is unchanged, the release gate
is untouched, and the exception is available **only** where the repository has no stable automated
surface reaching the defect (GUI/component lifetime, host-owned UI behaviour, OS-level asynchrony) —
never for a test that is merely hard to write. Invoking it requires four disclosures (why no test
exists, what replaced it, where the gap is tracked, whether infrastructure could close it), and the
exception lapses when the surface appears. `docs/procedures/TESTING.md` §"Gaps in the automated
coverage" is named as the register — the role it already played for the AU-conformance and
golden-audio gaps that `KNOWN_ISSUES.md` KI-014 and `RELEASE_HARDENING_PLAN.md` RH-F3 cite. Per
`ADR_POLICY` rule 5 / `SOURCE_OF_TRUTH`, the ADR is the instrument that makes the Policy change; per
rule 1 it is registered in `ADR_INDEX.md`. **A one-off waiver was explicitly rejected** — the goal
was a rule that describes the engineering reality, not an escape hatch for one entry.

**Preset drop-down lifetime + crash (`src/PluginEditor.cpp`).** Filed as **INC-010**. Three facts,
separated after an adversarial re-read of the pinned JUCE source — the first draft of this entry
(and of the code comment) got the mechanism wrong and is corrected here rather than left standing.
(1) The **leftover menu** is not an oversight in JUCE: `MenuWindow::windowIsStillValid()` dismisses when
`componentAttachedTo != options.getTargetComponent()`, but both are `WeakReference` to
`presetName`, so they null *together* and the comparison is false. (2) The **lost styling is not a
use-after-free** — the MenuWindow copies the look-and-feel into its own `Component::lookAndFeel`
slot (`juce_PopupMenu.cpp:366`), a `WeakReference` that nulls and falls back to `LookAndFeel_V4`
(the `PopupMenu` itself is a stack local, gone long before the editor). (3) The **crash** is the raw `this` in the callback.
The fix is `withParentComponent (this)` (JUCE parents the MenuWindow as a CHILD, cancelled with
result 0 by `ModalComponentManager` on destruction *or* hide; `Component::getLookAndFeel()` then
resolves our LookAndFeel by walking the tree) plus a `SafePointer` callback — which is **not**
redundant, since that cancel is asynchronous and the menu's 20 Hz timer can still emit a non-zero
result in the gap. Two side effects of parenting were neutralised in the same change:
`withMaximumNumColumns (1)` (a parented menu is budgeted against the editor, and JUCE adds COLUMNS
before it scrolls — past ~14 user presets the list would have silently gone two-column) and a
no-op `drawResizableFrame` (JUCE paints a frame over the border ring only when parented). The
"Load Preset…" file chooser, reachable from the same menu, got the same `SafePointer` guard.
No regression test: the failure is a GUI-lifetime use-after-free, which `tests/state_tests.cpp`
cannot express — the harness links the editor but never instantiates it. This is **not** a one-off
waiver: **ADR-0025** amends `TESTING_POLICY` rule 1 with a narrow, disclosure-bound exception for
defects that no automated surface reaches, the default stays "every bug fix ships a regression test",
the release gate is untouched, and INC-010 is the first invocation. Its four required disclosures —
why no test exists, what replaced it (removal of the lifetime by construction, plus a `SafePointer`
for the residual asynchronous window), where the gap is tracked, and what infrastructure would close
it — are recorded in `TESTING.md` §"Gaps in the automated coverage", which that ADR names as the
register, and summarised in INC-010's Prevention field. Synced:
`CHANGELOG.md`, `README.md`, `HANDOVER.md`, `POSTMORTEMS.md` (INC-010).
**Reported, not fixed (C6):** the combo-box popups store an editor-member LookAndFeel the same way
and would lose styling identically, but their callback is `ModalCallbackFunction::forComponent`,
i.e. already SafePointer-based — no memory-safety defect, no reported symptom, seven call sites
across two LookAndFeel subclasses. Out of scope. Likewise `SpectrumImager`'s `freqEditor`
`onFocusLost` can fire during teardown; it is owned by the editor it belongs to, so it is a
different (and lesser) class of hazard.

**Factory-preset identity (`src/PresetManager.{h,cpp}`, `src/PluginProcessor.{h,cpp}`).** The
preset list was searched by NAME and the factory block is list-front, so a user preset sharing a
factory preset's name could never hold the drop-down tick. A factory preset now carries an
immutable internal `factoryId` and a user preset is identified by its file
(`PresetManager::Selection`); the menu, the top bar and the Save Preset field still show the
**name**. The identity rides on `StateSet` through A/B and undo, and — after the maintainer
supplied the Architecture-Review approval the gate requires — **also with the session**, so
reopening a project ticks the row that produced the sound. Six additive metadata fields (3 in
`AnamorphRoot`, 3 per A/B slot); **user preset FILES are unchanged**, parameter restore is
independent of identity restore, and anything unresolvable ticks nothing rather than a same-named
substitute. Recorded as **ADR-0024** (registered in `ADR_INDEX.md`), whose original "never
serialized" clause is reversed by a dated **Amendment** that keeps the original text verbatim above
it — the reversal, its approval and its fallback table are exactly what a future agent would
otherwise re-litigate straight into a Hard Stop. Synced: `SERIALIZATION_REGISTRY.md` (six new field
rows), `SESSION_COMPATIBILITY_POLICY.md` (rule 4's round-trip list), `API_REFERENCE.md`,
`USER_MANUAL.md` §7.2, `TESTING.md`, `TESTING_POLICY.md`, `RELEASE_HARDENING_PLAN.md`,
`REPOSITORY_MAP.md`, `HANDOVER.md`, `CHANGELOG.md`, and **`PRIVACY.md`** — that document states
every claim about what reaches disk, and the session can now carry a preset **file name**, or an
absolute **path** in the one case where the selected preset was opened from outside the preset
folder. The path case is the same class as the Standalone's `lastStateFile` entry the document
already carves out, and it is now carved out alongside it, with the reason the in-folder case stores
a name instead. `ADR-0008` gained the third `StateSet` field and
re-based line anchors (a factual re-sync, not a reversal; ADRs stay append-only). State tests 10,
11 and 12 pin the live behaviour, the id integrity and the whole restore matrix including every
fallback.

Six defects found by review and fixed before merge, each with its own assertion, and every one
verified to fail with its fix disabled. Three from the first adversarial pass: the identity scan
**fell through** to the name scan when the identity was known but absent from the list, so a `.anamorph`
loaded from outside the preset folder ticked the same-named factory row — the exact mis-tick this
change exists to remove; `saveUser` never re-baselined the processor's undo snapshot, so the first
undo after a save restored the pre-save name/identity (fixed with an `onSaved` hook →
`syncCommitted()`, which creates no undo step because a save is not a sound change — and the same
gap existed for a preset switch whose sound is identical to the current one); and `readSlot` left
a stale identity on an A/B slot when a host restored a second session into one live instance.

Three more from the maintainer's follow-up review. **`saveUser` did not flush pending undo
coalescing** before re-baselining: `syncCommitted()` clears `pendingGestureCommit`, so a knob
gesture that had closed but not yet been polled was folded into the new baseline with no undo step
— the edit silently stopped being undoable. `onSaved` now does `pollUndoCoalesce(); syncCommitted();`,
matching the two other program-state jumps (`onAboutToLoad`, and `undo()`/`redo()`). **A factory id
that fails to resolve** applied the plain defaults and then adopted the factory identity anyway;
`load()` now resolves it BEFORE the undo bracket opens — the same rule the user-preset parse three
lines above already followed — asserts it, and fails as a clean no-op otherwise. State test 11 pins
the invariant that makes the assert unreachable: ids present, unique, and every one resolving.

**Two more from a third, independent review of the finished change set**, both introduced by the
plug-in-state work and both now fixed with a discriminating assertion. `encodeSelection` used
`juce::File::isAChildOf` to decide whether a preset lives in the preset folder — but JUCE implements
that **recursively**, so a preset opened from a **sub-folder** was stored by bare name and decoded to
a *different*, same-named file directly in the folder, breaking the `decode(encode(s)) == s`
invariant the header and the ADR both state. Now a **direct-child** test, which is also the honest
one: `refresh()` scans non-recursively, so only a direct child can ever be a menu row. And the new
`else` branch of `commitPresetSwitchUndoStep` did not clear redo, unlike the `if` branch one line
above whose comment states the rule — so undo, then select the same-sounding preset on the other row,
then Redo, and the tick jumped back out of an abandoned `StateSet`. (That branch then had to be
narrowed again — see below.)

**Raised and REFUTED, recorded so it is not re-raised:** a `saveUser` defect for preset names with a
leading `~`. The JUCE facts are real as far as they go — `getChildFile` short-circuits for anything
`isAbsolutePath` accepts, and on macOS/Linux a leading `~` survives `createLegalFileName` — but the
write cannot succeed. `replaceWithText` does not open the target: it writes a hidden sibling built
from `getParentDirectory()`, and for a separator-less path that is the path itself, which is not a
directory. `createLegalFileName` strips `/` and `\`, so every tilde-leading name hits the same
degenerate parent. `saveUser` therefore returns **false**, nothing is written anywhere, and the Save
dialog stays open with the text intact — the save fails *visibly*, which is exactly what the proposed
guard was meant to produce. Verified empirically against the pinned `juce_core` for `~foo`, `~/foo`,
`~` and `~root`, with a normal name as the control. **No code change; no defect.** The refutation and
its probe are in `worklogs/PRESET_MENU_AND_IDENTITY_v0.9.2.md` §7.

**Redo invalidation, narrowed after review.** The `else` branch above cleared redo unconditionally,
so *re-picking the row that is already ticked* — identical sound, identical identity — silently threw
away a redo the user was about to press. It now clears redo only when the identity actually **moved**
(`presets.selection() != committed.selection`), which is the only case where a surviving redo entry
could drag the tick off the row just chosen. The same-sound/**different**-row case still invalidates,
and its assertion is unchanged; a second assertion covers the re-select case, and both were verified
against the pre-fix behaviour.

**The encoder's second ambiguity: a direct-child name that `isAbsolutePath` accepts.** The
`isAChildOf` fix above closed the *nesting* route into a broken `decode(encode(s)) == s`; a leading
`~` was the other one. `decodeSelection` reads a bare name back through
`presetDirectory().getChildFile(name)`, and `getChildFile` short-circuits to the raw `File`
constructor for anything `isAbsolutePath` accepts, so `~foo.anamorph` sitting **directly in** the
preset folder decoded to a literal relative path and the row lost its tick on reload. This does not
contradict the `saveUser` refutation recorded above — that refutation is about the **save** path,
which genuinely cannot create such a file; `USER_MANUAL.md` tells users to manage presets as files, so
a hand-copied one reaches `refresh()` and can be loaded and encoded like any other. The encoder now
requires the bare name to be unambiguous (`! juce::File::isAbsolutePath (name)`) and otherwise takes
the absolute-path branch it already shares with outside-the-folder and sub-folder presets. No preset
file format change, no canonicalisation, no weakening of the no-name-fallback rule. State test 12
gained the round-trip case; verified to fail with the fix disabled.

**A/B slot metadata now follows "absence means default".** `readSlot` read `dst.name` and
`dst.baseline` *inside* the `hasProperty("slotAParams")` branch, so the pre-0.6.4 legacy shape — params
only — left both untouched. `abSlot[]` are processor members and a host may call
`setStateInformation` on one live instance repeatedly, so a legacy session restored after a modern one
kept the **previous** session's preset name and dirty-baseline attached to freshly restored
parameters. Both reads moved out of the branch, next to the identity read that already had this right.
The resulting defaults (`""` / `""`) are the ones `SERIALIZATION_REGISTRY.md` already documented, so
the code caught up to the ledger; no field was added, removed or renamed. **An existing assertion was
changed, not merely added:** state test 5's `slotAName == "Default"` under the comment "legacy slot
keeps pre-restore meta" *pinned the defect* — it described a fresh instance's construction snapshot as
if it were the rule. It now asserts the default, alongside a repeated-restore case that shows why.

**"No baseline recorded" is not "modified" (fifth review round).** The A/B fix above left a second
half unfinished: a pre-0.6.4 slot restores with an empty *baseline* as well as an empty name, and
`isDirty()` is `soundSig() != sigAtLoad`. `soundSig()` is never empty, so an empty baseline compares
unequal to every possible sound and the slot read as **permanently modified** — with no name, the top
bar rendered a bare ` *`: a modified-marker against a preset that does not exist. The project already
has a rule for "restored parameters, no recorded baseline": `adoptRestoredState` sets the restored
state as the clean one, which `SERIALIZATION_REGISTRY.md` documents for the root `presetBaseline` and
state test 4 pins for a v0.2 session. `setMeta` now applies that same rule, so it is one rule with one
spelling instead of two answers to the same question. Unreachable from undo, redo, A/B and copy —
every in-memory producer fills the baseline — so the branch is legacy-restore only. The *empty name*
was left as-is deliberately: the slot genuinely has no preset, and the pre-fix "Default" was a
factual error (the slot's parameters were not the defaults). Maintainer confirmation of the direction
is recorded per the review sign-off; no serialization field changed and `""` keeps its meaning
("absent"), so this is a read-path interpretation, not an `ARCHITECTURE_REVIEW_GATE` item.

**A slot must reset as a whole, or its two halves come from two projects (sixth review round).** The
"absence means default" rule was applied field by field — `dst.selection`, `dst.name` and
`dst.baseline` — but `dst.params` was still only touched inside the two params-present branches. An
`AB` node that exists while a slot's payload cannot be read (neither `slotAParams` nor the pre-0.6.4
`slotA`, or a payload that fails to parse) therefore kept the **previous restore's sound** while its
metadata was reset around it: one slot holding one project's sound under another project's label.
Before this PR both halves were inherited together — consistently stale, which is wrong but not
*mixed* — so this was a defect the earlier rounds introduced, not a pre-existing one. `readSlot` now
resets the slot to a default `StateSet` first and overlays what the node carries. The params default
is not an empty tree but **"lazily initialised from current"**, which the registry already recorded
and which `abEnsureInit()` already implements off `StateSet::isValid()` — so no new mechanism, no new
field, and the slot comes back seeded from the state just restored. The reset also covers the
present-but-unparsable payload for free. Both cases are pinned by state test 9 and were verified to
fail with the reset removed. **No `CHANGELOG.md` entry:** no shipped version writes an `AB` node
lacking both params keys, so there is no user-visible change to report under `CHANGELOG_POLICY` rule
3 — this is corrupt/truncated-state robustness, the category state test 7 covers. Maintainer
confirmation of the direction is recorded per the review sign-off.

**The root preset NAME had the same leak as the slots (seventh review round).** `readSlot`'s rule —
metadata never inherits across a repeated restore — was not applied to `AnamorphRoot`. Both adoption
paths fell back to the live `presets.currentName()`: the `haveBaseline` branch via
`restoredName.isNotEmpty() ? restoredName : presets.currentName()`, and `adoptRestoredState` via
`if (name.isNotEmpty()) current = name;`. `presets` is a processor member, so on a host's second
`setStateInformation` into one instance that is the **previous project's** label — new sound, new
identity, old name, and with no stored identity the name scan could then tick the old project's row.
This became reachable *because* of this PR: an empty preset name is now a real state (a session saved
while sitting on a nameless A/B slot stores `presetName=""`).

**Absent and empty are different answers**, and only `setStateInformation` can tell them apart — the
distinction `haveBaseline` already drew for the sibling field. Absent means a session predating the
field (< 0.6) and resolves to the new `PresetManager::defaultName()`, a **constant**, whose
name-fallback tick is the documented ADR-0024 answer for identity-less state; present-but-empty is
adopted verbatim. `adoptRestoredState` now assigns the name unconditionally, so "what the session
carried" and "what absence means" stop being decided in two places. No serialization field changed,
and no existing assertion moved — state test 4's `preset name falls back to Default` still passes,
because a v0.2 blob has no `presetName` property. Four cases (empty/absent × baseline/no-baseline)
are pinned in state test 12; all eight new assertions were verified to fail with the fix reverted.
Maintainer confirmation of the direction is recorded per the review sign-off.

**An unrecognised chunk is not a restore (eighth review round).** `setStateInformation` handles two
root shapes; anything else matched neither and *fell through* to the adoption block, which clears the
undo history and writes the restored preset name, identity and baseline. With nothing restored, that
relabelled the live sound — after the previous round, with the constant `"Default"` — dropped the
identity to `unknown` so the name scan ticked whatever shared the label, and re-baselined the
dirty-star. The name half was introduced by the previous round; the identity and baseline halves were
**pre-existing**, since `adoptRestoredState` always assigned those two unconditionally. The fix is an
`else { return; }`, which is the same answer the `getXmlFromBinary` guard at the top of the function
already gives an unparsable blob — the identical situation one layer down. It also stops the undo
history being cleared for a session that never loaded; disclosed rather than slipped in, since that
half was not named in the finding.

**`abEnsureInit` now seeds both slots the same way.** It seeded an invalid slot A from
`currentStateSet()` but an invalid slot B from a **copy of slot A**. On the path that runs every time
— construction, both slots invalid — the two are indistinguishable, so this changes nothing there.
They diverged only when slot A was valid and slot B was not, i.e. an `AB` node whose `slotBParams`
alone was missing or unparsable: slot B came back as a duplicate of slot A rather than as the state
just restored, and a later save wrote that duplicate out. The registry and `STATE_SERIALIZATION.md`
had already been written as though the rule were symmetric, so this is the code catching up to the
documented invariant rather than a new one. `currentStateSet()` builds a fresh tree per call, so the
explicit `createCopy()` for slot independence is no longer needed.

**The empty preset label gets a placeholder (ninth review round).** The blank top-bar button a
pre-0.6.4 A/B slot produced — flagged as a maintainer decision under constraint C8 — is now
**No Preset**, with sign-off dated 2026-08-08. It is a *display* substitution in
`refreshPresetDisplay`, deliberately **not** in `PresetManager::currentName()`: that accessor also
feeds the serialized `presetName` and the Save Preset pre-fill, so a placeholder there would be
written into every session saved from a nameless slot and offered as the default preset *file* name.
The stored name stays `""`, the identity stays `unknown`, and `currentIndex()` still ticks nothing.
State test 5 gained the assertion that closes the loop — a re-save must still write `presetName=""`
— and moving the substitution into the accessor as a control fails four assertions. `ADR-0024`'s
"no user-visible string was added" consequence was **false** once this landed and is corrected in
place rather than left to drift; the `CHANGELOG` entry now names the label. `TESTING.md`'s
restore-path sentence was one behind in both numbers (seven/six → eight/seven) and had missed the
tilde case in its fallback list; tests are the source of truth, so the prose moved. The `setMeta`
ordering invariant, already in the header, is now also stated at `applyStateSet` — the two lines that
*are* the order, and the place a future edit would break it.

**Declined in the same round, recorded so it is not re-raised: an `AnamorphRoot` with no `ANAMORPH`
child.** Such a chunk is *recognised*, so it restores the fields it carries and resolves the absent
ones to their documented defaults — while `params.isValid()` is false, so the parameters keep their
current values and the live sound ends up labelled `defaultName()`. It reproduces, and it is
deliberately left alone: the rule this round implements is about *unrecognised* input; field-by-field
handling of a recognised root is the existing design and state test 7's `restoreWithActive` depends
on it (an `AnamorphRoot` carrying only an `AB` child must still apply the clamped `active`); the
obvious alternative — skip adoption when there are no params — re-introduces the cross-restore
leakage the previous round removed; and `getStateInformation` always writes an `ANAMORPH` child, so no
shipped version can produce one. Reasoning in full in `worklogs/…v0.9.2.md` §13.

**`setMeta`'s identity-less overload removed.** The two-argument overload forwarded a
default-constructed `Selection`, so "forget which row produced this sound" — the mis-tick ADR-0024
exists to remove — was something a caller could do without writing it down. Its only caller was a
test, which now passes `Selection()` explicitly. The one-argument `adoptRestoredState` overload was
dead code with the identical shape and went with it. No behaviour change. The header also now records
the precondition `setMeta`'s empty-baseline fallback depends on and the signature cannot enforce: the
parameters the metadata describes must already be applied, because `soundSig()` reads the live APVTS.

**Re-raised and re-refuted: the `~foo` `saveUser` claim.** A later review reported this ledger as
still asserting that `saveUser` "writes outside the folder and still returns success". It does not,
and has not since the round recorded in `worklogs/…v0.9.2.md` §8 — the sentence was removed there and
the entry above has stated the refutation ever since (introduced `9b67b8d`, corrected `55e062d`). The
repository holds no conflicting description: `DOCUMENTATION_COVERAGE.md` and worklog §7 both say the
write fails and `saveUser` returns **false**, and §9 records that the *encode*-side sibling — a
`~`-named file a user copies in by hand — was a separate, real defect. Because the claim keeps coming
back, the refutation now also lives in the **code**, at the `getChildFile` call it is raised against;
per `SOURCE_OF_TRUTH` that outranks every document and is the first thing a reader of
`saveUser` sees.

**Documentation follow-up on the identity match (no behaviour change).** ADR-0024's Consequences now
state the three properties plainly: the match is a raw path-string compare with **no**
canonicalisation (`getLinkedTarget()` considered and rejected — it resolves symlinks but not
`/private/var`, mount aliases or UNC spellings, trading a predictable "no tick" for a partial one);
cross-machine resolution holds only for the name-encoded case, because a stored absolute path fails
`isAbsolutePath` on the other platform; and a file name that looks like a path is stored as a path.
`SERIALIZATION_REGISTRY.md` gained both encoder conditions and the raw-compare note.
`API_REFERENCE.md`, `STATE_SERIALIZATION.md` and the ADR had their `src/` citations re-anchored where
this round's edits moved them.

**Declined, with evidence: restoring `PopupMenu::setLookAndFeel (&lnf)`.** The stated goal was to
make item *measurement* use `AnamorphLookAndFeel` — but it already does. `MenuWindow` parents itself
at `juce_PopupMenu.cpp:370-372` and only then builds items (`:457`), and `ItemComponent` calls
`parent.addAndMakeVisible` *before* `getIdealSize` (`:139-146`), which resolves through
`getLookAndFeel()`. Restoring it would instead re-arm the `~LookAndFeel` assertion, which fires on
any live `WeakReference`: `lnf` is a member and so is destroyed *before* this editor's `Component`
base, i.e. before the menu is asynchronously cancelled. The only two calls that still see the
default look-and-feel are bound one line before the parenting — `setOpaque` (same answer,
`colours::bgPanel` is opaque) and `preparePopupMenuWindow` (a no-op we do not override). A **third**
resolves through it earlier still and is **load-bearing**: `getParentComponentForMenuOptions`
(`juce_PopupMenu.cpp:353`, in the member-init list), whose return value is what installs the parent —
so a process-global default look-and-feel overriding it to return `nullptr` would silently discard
the parenting. Every JUCE look-and-feel inherits `LookAndFeel_V2`'s pass-through, and the default is
not ours to control; recorded in the code as the latent trap it is.

**`focusSaveNameField`'s comment was stale, not its behaviour.** It justified the retry by the
preset menu's own desktop window owning OS focus — which parenting removed. The retry stays, because
the abort it works around is not menu-specific: `Component::takeKeyboardFocus` gives up while the
plug-in's own peer is not OS-focused, and whether it is, at that instant, is the host's call (the
failure KI-009 tracks in REAPER). Comment rewritten; the bounded 4 × 50 ms retry is untouched.

**`Window Size` → `UI Scale` (display name only).** `PARAMETER_COMPATIBILITY_POLICY` permits a
display-name change; the identifier `int_uiScale` and the pre-0.8.4 legacy APVTS id `uiScale` its
migration reads are untouched, so this is not a serialization change and needs no ADR. Recorded
with the repo's own footnote form in `PARAMETER_REGISTRY.md` (`※`, mirroring the `Haas Side` →
`Haas Focus` precedent). Synced: `PARAMETER_REFERENCE.md`, `REPOSITORY_MAP.md`, `USER_MANUAL.md`
(×3), `README.md`, the six source comments naming the control, and a **clarifying annotation** in
ADR-0010 — the ADR body is otherwise left verbatim, since ADRs are append-only.

**Installer component titles.** macOS `<choice title=…>` → *VST3 Plug-in* / *AU Plug-in* /
*Standalone Application*; the two Windows destination-page **labels** → *VST3 Plug-in folder* /
*Standalone Application folder*. Prose sentences keep lowercase "plug-in"/"application" (the
`MsgBox` strings, the `:90` parenthetical, every legal/manual use). The Windows `[Components]`
descriptions ("Install VST3" / "Install Standalone") contain neither phrase and are unchanged, so
the five doc quotes of them stay valid. No CI or self-check assertion matches a title — the macOS
self-check matches `<choice id=…>` and the package identifiers. Synced: `PACKAGING.md`,
`INSTALLATION.md` (the macOS Component table, which had drifted twice over: *AU (Audio Unit)* and
*Standalone app* never matched the installer even before this change — corrected here in the same
pass, and reported rather than silently changed).

**macOS key auto-repeat: investigated, no code change (KI-017).** Holding a letter or digit in a
text field types once and stops while punctuation repeats. Traced through the pinned JUCE: a
focused `TextEditor` makes `findCurrentTextInputTarget()` non-null, so every key-down goes to
`[inputContext handleEvent:]` first and printable characters return via `insertText:` — the path
macOS press-and-hold and the IME own — while "special" keys return via `doCommandBySelector:` and
repeat normally. Everything inside the plug-in was eliminated by inspection (the bounded focus
retry, `setSelectAllWhenFocused`, the 24 Hz timer, the VBlank attachment, the UI-scale transform,
both `getCurrentModifiersRealtime` call sites). Filed as **KI-017** with the two discriminating
checks; no `CHANGELOG` entry, since nothing user-visible changed (`CHANGELOG_POLICY` rule 3).

**First-tag renumbering, swept this time.** The 0.9.0 → 0.9.1 renumbering was recorded here as
incomplete; the 0.9.1 → 0.9.2 one repeated it and is now closed in the same pass:
`CHANGELOG.md` preamble, `CHANGELOG_POLICY.md`, `FUTURE_RISKS.md` (×2), `COMMERCIAL_STATUS.md`
(×3) and `RELEASE_HARDENING_PLAN.md` (×5) all named v0.9.1 as the first annotated tag. Neither
`FUTURE_RISKS.md` nor `COMMERCIAL_STATUS.md` had been touched by the version bump at all.

Prior: for the **third review pass on the 0.9.1 change set** (2026-07-30). Three findings
fixed, three were confirmations. No `src/` change.

**The `Unreleased` guard had a residual hole.** It rejected only a heading containing the word
`Unreleased`; a heading written as a bare `## [0.9.1]` is equally undated and would have published
undated notes. The check now requires an **ISO date** in the heading, which subsumes both cases and
matches the format every existing entry already uses. Exercised against five heading forms
(`— Unreleased`, bare, em-dash-dated, hyphen-dated, two-digit minor) plus both real CHANGELOG
sections. Synced: `release.yml` (tag branch + rehearsal warning), `RELEASE_PROCESS`, `CI_CD`,
`HANDOVER`.

**`FUTURE_RISKS` was edited for 0.9.1 but kept a v0.9.0 version-sync lead**, leaving it and
`KNOWN_ISSUES` disagreeing about which version the status documents are synced to. Re-led to
v0.9.1, recording that ADR-0023 adds no new *risk* — the one-time session break is a known issue
(KI-016), not a forward-looking one.

**More reported-then-corrected line drift (C6), in a document this change set touched.** `BUILD.md`
carried three stale `CMakeLists.txt` citations the previous pass did not report:
`ANAMORPH_BUILD_TESTS` `:27,212` → `:27,219`, `ANAMORPH_BUILD_NUMBER` `:183` → `:188`, compile
definitions `:185-194` → `:190-199`. Sweeping for the same class found a fourth, shared by
`RELEASE_PROCESS` and `RELEASE_POLICY`: the versioning citation `:181-187` → `:186-192`. All five
re-verified against the file. (`build.yml:60,180,438` for the build-number Configure steps was
checked and is correct.)

**Confirmed, no change:** the auval recipe is consistent in every maintained carrier; the guard
interacts correctly with the verbatim notes extraction; the `curl`/`unzip` fix matches what
`run-pluginval.sh` invokes and its re-based citations all resolve.

Prior: for the **second review pass on the 0.9.1 change set** (2026-07-30). Six findings
fixed, no `src/` change.

**The exception was over-claimed.** ADR-0023's status line said the `ARCHITECTURE_REVIEW_GATE` item
and the `COMPATIBILITY_POLICY` exception were "both now cleared". The exception needs all four
conditions and **condition 3 — the Release Compatibility Checklist — has never been completed for
this release**, as `HANDOVER` says in the same change set. The ADR now carries a per-condition table
marking 3 **OPEN** (a release-time gate: it blocks the tag, not the merge), and the exceptions table
in `COMPATIBILITY_POLICY` says the same. The ADR's own rule — "must not claim a green gate it did
not observe" — was what the blanket sentence broke.

**The carve-out was scoped wrongly.** 2a read "no annotated tag exists for any build carrying the
old identity", and the exceptions table called it "spent", which would have closed the only route
for a later `PLUGIN_CODE` or `PRODUCT_NAME` change (a product rename before the first tag) on the
strength of a manufacturer-code change. 2a is a **condition on the state of the world, not a token
an exception consumes**: true while no tag exists — for every identity field at once — and
permanently false from the first tag, again for every field at once. Reworded in
`COMPATIBILITY_POLICY` and ADR-0023.

**The 0.9.0 → 0.9.1 first-tag renumbering was incomplete.** Still claiming v0.9.0 as the first tag:
`RELEASE_PROCESS` (§After release), `CHANGELOG_POLICY` rule 2, `FUTURE_RISKS` RISK-003,
`RELEASE_HARDENING_PLAN` (Version-management row, RH-R6, RH-PR-8 row, RH-F3 timing), `HANDOVER`
(Branch Strategy). All corrected.

**`RELEASE_PROCESS` contradicted the new pipeline check.** It still told maintainers an undated
`Unreleased` heading "would be published with that word in it" and that "the validation only checks
that the section *exists*" — the same change set had made that a fail-closed rejection. Corrected,
with the two practical consequences spelled out (date the heading **in the tagged commit**;
rehearsals only warn).

**The guard's justification was wrong about *what* it protects.** The release **title** is set
separately (`--title "Anamorph <version>"`); the extracted section becomes the **notes body**. The
check is right and stays; the wording is fixed in `release.yml`, `CI_CD`, `RELEASE_PROCESS` and
`HANDOVER`.

**Reported-then-corrected line-number drift (C6).** The previously reported `ADR-0001` citation
`CMakeLists.txt:62-73` → `:124-135` (and `:149-166` → `:228-237` for the tests-link-the-core
range), plus the same drift in `TROUBLESHOOTING` (`:115-125` → `:124-135`), found by the reviewer.
Both re-verified against the file. Reporting came first, in the prior pass; this is the correction.

**Stale version snapshots refreshed:** `HANDOVER`'s snapshot preamble (v0.9.0 was never tagged;
0.9.1 is the release in preparation and the first tag) and `COMMERCIAL_STATUS` (§Last reviewed,
§2, §6). `COMMERCIAL_STATUS` keeps its **2026-07-26** review date deliberately — its substance
(product model, distribution model, open owner/legal decisions) is untouched by a version
renumbering, and a review date that moves on every version bump stops meaning anything.

Prior: for the **ADR-0023 sign-off + the pluginval dependency fix** (2026-07-30). Two
changes, neither touching `src/`.

**ADR-0023 is now `Accepted`.** The maintainer signed off the Architecture Review and performed the
Level-5 identity check on 2026-07-30 — the 0.9.1 build registers under the new identity in a host,
and `auval -v aufx Anmr RTec` was run on macOS. That was the only check that exercises the change,
and no automated gate in this repository could have stood in for it, since nothing in the suite
observes plug-in identity. Status synced in ADR-0023 (incl. its *Verification performed* section),
`ADR_INDEX`, `COMPATIBILITY_POLICY` (exceptions table), and `HANDOVER` (Current Version, Release
Status, Known Blockers — the v0.9.1 tag blockers drop from five to **four**; the remaining four are
the missing licence plus three `RELEASE_POLICY` preconditions, all carried unchanged from the
v0.9.0 audit and all still requiring a human). Deliberately **not** upgraded: the ADR's
`AnamorphTests` / `AnamorphStateTests` / pluginval rows stay `Unverified in-repo` — the sign-off
covered the identity behaviour, which is what needed a human; the machine-checkable gates are
reported by CI on the change set, and an ADR must not claim a green gate it did not observe (C2/C7).

**`scripts/setup-linux.sh` now installs `curl` and `unzip`.** `run-pluginval.sh` calls both to
fetch and extract the pluginval release, and neither was installed — `libcurl4-openssl-dev` is the
development headers, not the CLI. GitHub-hosted runners preinstall both, which is exactly why the
gap never showed in CI and would only have bitten on a fresh machine or a minimal container, i.e.
the case this script exists to cover. Found while fixing the same defect in the sibling repository.
Synced: `BUILD.md` (§Linux dependencies — package list plus a paragraph separating the three
pluginval-only packages from the build dependencies; §Network domains), `TROUBLESHOOTING.md` (a new
`command not found` row). The script gained lines, so the `setup-linux.sh:NNN` citations in both
documents were re-based: package list `:24-32` → `:29-38`, EGL note `:13-15` → `:18-20`, network
domains `:8-13` → `:8-12`, webkit `:31`/`:36` → `:37`/`:42`, `libegl-dev` `:30` → `:36`.

Prior: for the **vendor manufacturer-code change** (2026-07-30, on top of `main` @
`c0fca30`). **No `src/` change; DSP, parameter surface and serialized state bit-identical to
0.9.0.** `PLUGIN_MANUFACTURER_CODE` changes `Anmf` → `RTec` (`CMakeLists.txt:153`) so the vendor
code spells RollyTech rather than the first product, ahead of the second product line member
(Anabasis) adopting the same value. Version bumped to **0.9.1** (`CMakeLists.txt:14`). The code is
host-facing identity — the AU component's manufacturer field, and an input to JUCE's VST3 class
UID — so pre-0.9.1 sessions report the plug-in as missing; that is documented, not fixable, and
one-time. Added: **ADR-0023** (options incl. "keep `Anmf` forever" and the rejected
`Roll`/`RolT`/`RlyT` candidates; opened as `Proposed`, `Accepted` 2026-07-30 — see the head entry)
+ its `ADR_INDEX` row; **KI-016** + its summary-table row.
ADR count in the self-coverage table synced 17 → **18**.
Synced: CHANGELOG (`[0.9.1] ### Changed`, evidence = PR #97 per `CHANGELOG_POLICY` rule 2; the
preamble's "from [0.9.0] onward each release is tagged" claim corrected — 0.9.0 was written up but
never tagged, so the first annotated tag will be v0.9.1), README (§Project status), HANDOVER
(Current Version, Release Status incl. the tag name `v0.9.1`, Known Blockers), COMPATIBILITY_POLICY
(new *Plugin identity change* prohibited-row; an **identity carve-out** to exception condition 2 —
enacted by ADR-0023, because condition 2 as written is unsatisfiable by construction for an
identity change and the policy would otherwise have contradicted itself; and an
"Exceptions granted so far" table recording that the carve-out's 2a ground is spent),
RELEASE_PROCESS (§Tagging — next tag is `v0.9.1`, and date the heading before tagging),
`release.yml` + CI_CD (a fail-closed check rejecting a tag whose CHANGELOG heading is still marked
`Unreleased`, since the heading is published verbatim as the release-notes title), TRADEMARKS
(§1 — the code is a RollyTech name-bearing identifier), PACKAGING (§Plugin identifiers),
KNOWN_ISSUES (version-sync lead), and every `auval -v aufx Anmr Anmf` invocation → `RTec`
(`packaging/macos/INSTALL.txt`, `docs/user/INSTALLATION.md`, `PACKAGING.md`, `TROUBLESHOOTING.md`,
`TESTING.md`, `KNOWN_ISSUES.md` KI-014, `RELEASE_HARDENING_PLAN.md` RH-F3).
**Deliberately not changed:** `worklogs/PRODUCT_READINESS_ROADMAP_v0.8.13.md:36` still carries the
old `auval` recipe — worklogs are a historical evidence trail, not maintained documents, and
rewriting one to match today's code would falsify the record.
**Drift observed, not corrected (constraint C6):** `ADR-0001` cites `CMakeLists.txt:62-73` for the
`AnamorphDSP` INTERFACE library, which now lives at `:124-135` (as `ARCHITECTURE.md` correctly
states); this predates the present change and is out of its scope.
The comment added beside the new code is deliberately same-line, so no `CMakeLists.txt:NNN`
citation anywhere in the documentation shifts.

Prior: for the **product video script worklog** (2026-07-29, on top of `main` @
`82b2f61`). **No `src/` change; no product-document change.** Added (and subsequently revised,
in the same unmerged branch) `worklogs/KEYNOTE_SCRIPT_v0.9.0.md` — a session work product
(marketing draft): a locked product positioning ("width is a method"; the plugin as instrument,
not assistant) and a complete ~7-minute developer-walkthrough video script with production
guardrails, all derived from the existing developer chain. An earlier keynote-style draft in
the same file was superseded by this revision; a Chinese adaptation is deferred until the
English script is approved. It is explicitly marked derived content, may never be cited
as evidence, quotes no unmeasured performance numbers (constraint C2), uses no ™/® symbols
(`TRADEMARKS.md`), and does not alter product status (`docs/COMMERCIAL_STATUS.md`: v0.9.0
remains internal-testing, not for sale). Worklogs sit outside the four documentation classes
(`docs/REPOSITORY_MAP.md` describes `worklogs/` generically), so no
`REPOSITORY_MAP`/`SOURCE_OF_TRUTH`/README class-table change applies; this entry satisfies the
audit obligation. Not a changelog entry (no user-visible product change, `CHANGELOG_POLICY.md`
rule 3).

Prior: for the **artifact & INSTALL.txt cleanup pass** (2026-07-26, on top of `main` @
`2d0a906`). **No `src/` change; no installer or runtime behaviour change.** The three internal
`Anamorph-<OS>-release` artifacts are **removed**, along with the archive-creation steps that fed
them (`zip -ry` / `Compress-Archive` / `ditto -c -k`): each platform now uploads exactly one
customer artifact (`Anamorph-<OS>`, loose files) plus its `-debug` symbols. `release.yml`
downloads those same trees, restores the executable bits the artifact transport drops
(`Anamorph`, `install.sh`, `uninstall.sh`, `*.so` on Linux; `*/Contents/MacOS/*` on macOS;
Windows carries no Unix modes), archives each tree with its entries at the archive root, and then
**fails closed** unless every expected executable is present in the published zip with its mode —
so release assets keep their names, contents and permissions, and no nested archive is
reintroduced. The three `packaging/*/INSTALL.txt` files lose their "Testing & third-party
attribution" section entirely and now carry installation instructions, paths, platform notes and a
copyright line only; the mandatory IJG acknowledgement therefore rests solely on the
release-page `NOTICE` asset that `RELEASE_POLICY` requires on every published release. Synced:
CI_CD (pipeline step 7, artifact table, route note), PACKAGING (artifact table, routes,
attribution table), RELEASE_POLICY (§Artifacts, §Third-party attribution), RELEASE_PROCESS
(§Build the release artifacts, §Tagging step 3), REPOSITORY_MAP, HANDOVER (snapshot base,
distribution), COMMERCIAL_STATUS, KNOWN_ISSUES (KI-015), FUTURE_RISKS (RISK-006), TRADEMARKS §3,
CHANGELOG `[0.9.0]`, this file.

Prior: for the **internal-testing preparation & closed-source product documentation pass**
(2026-07-26, PR #94, on top of `main` @ `aecd448`). **No `src/` change.** The v0.9.0 **release date moved to
2026-07-26** in `CHANGELOG.md` and its two HANDOVER restatements (PR-landing and audit-run dates
left untouched). `SUPPORT.md` was **rewritten from a public support document into the internal
testing guide** — evaluation-only permission, no source-code rights, no redistribution, the
project's testing channel, and six mandatory report fields (version+build, OS, DAW/host, format,
reproduction steps, logs/screenshots); the bug-report form became "Test report — bug" and carries
the closed-source + public-tracker notice. **New:** `EULA.md` (an **unapproved draft**, not in
force and not shipped, every open owner/legal decision marked), `PRIVACY.md` (collects nothing,
sends nothing; every disk write and the one About-screen link cited to source), `TRADEMARKS.md`
(name status, third-party marks used descriptively, the naming obligations IJG/Xiph/zlib impose,
and the `Dim-D` / "Roland Dimension-D-style" review item) and `docs/COMMERCIAL_STATUS.md` (the
internal index of product model, distribution model and the eight open owner/legal decisions —
including the newly recorded fact that the GitHub repository is public with forking enabled while
the product model is closed-source). README regrouped its documentation index into **four
classes** (user / internal-testing / legal / developer). Product-model wording was then **rebalanced on owner instruction**: it is
stated once for a general audience in `README.md` §Licensing and otherwise kept only where it is
operative — the legal class, the internal/testing class (`SUPPORT.md` §1, the bug-report form) and
the developer documents that derive the JUCE-tier consequence. The user-facing set stays on using
the product: `USER_MANUAL` and `INSTALLATION` end with a plain copyright line, every `INSTALL.txt`
carries one in its own bilingual section above and separate from the mandatory third-party
(**superseded 2026-07-26 — see the head entry: `INSTALL.txt` is installation-only, so no
attribution section remains for it to sit above**)
attribution (which is unchanged), and the manual's Quick start and FAQ carry no legal wording at
all.
Synced: README, SUPPORT, REPOSITORY_MAP (root + `docs/` trees, `user/` branch), SOURCE_OF_TRUTH
(four-class scope + per-class authority), HANDOVER (snapshot base, release date, KI-015 wording),
DOCUMENTATION_LIFECYCLE_POLICY (documentation-only trigger table), THIRD_PARTY_LICENSES
(§"Open licensing decisions" #2), CHANGELOG `[0.9.0]`, issue templates, this file (self-coverage
tiers, ADR count).

Prior: for the **flat-artifact / lean-package / closed-source documentation pass**
(2026-07-26, PR #93). **No `src/` change.** The `Anamorph-<OS>` artifacts now upload loose
staged files (payload + `INSTALL.txt`; Linux adds `install.sh`/`uninstall.sh` — extract the
artifact zip once to see them directly; the transport drops exec bits on that route), new
`Anamorph-<OS>-release` artifacts carry the permission-preserving source archives that
`release.yml` publishes byte-identically (**superseded 2026-07-26 — see the head entry: those
artifacts are removed and `release.yml` archives the release zips itself**), and `NOTICE`/`THIRD_PARTY_LICENSES.md`/`SUPPORT.md`
ship **only** as version-named release-page assets — no longer inside any zip or installer
payload; each `INSTALL.txt` carries the IJG acknowledgement + pointer (**superseded 2026-07-26 —
`INSTALL.txt` is installation-only**). README now states the
product model (closed-source commercial; docs grouped user/legal/developer — the grouping
was superseded 2026-07-26, see the head entry), and the
licensing blocker set (KI-015/RISK-006/RH-R11/RH-F1, THIRD_PARTY_LICENSES, NOTICE, HANDOVER)
uniformly records that the model rules out the AGPLv3 arm, so the commercial JUCE licence is
required before commercial distribution. Synced: PACKAGING, CI_CD, RELEASE_PROCESS,
RELEASE_POLICY, REPOSITORY_MAP, SOURCE_OF_TRUTH (doc-class scope), USER_MANUAL (online link
fallbacks), CHANGELOG `[0.9.0]`, this file.

Prior: the **v0.9.0 release-hardening & commercial-readiness audit** (2026-07-25,
on `main` @ `0a98ebd`, PR #92; record: `worklogs/RELEASE_HARDENING_AUDIT_v0.9.0.md`). **No `src/`
change.** Six parallel investigation lenses + adversarial verification over the repository and
the pinned JUCE tree. **New:** `NOTICE` and `THIRD_PARTY_LICENSES.md` (verified third-party
inventory — every component classified compiled-in vs vendored-but-not-built from
`build/build.ninja` and object symbols rather than from JUCE's manifest, which is how FreeType
and stb, both vendored *inside* PlutoVG, were found; the Steinberg VST 3 SDK is **MIT** in JUCE
9.0.0, correcting RH-R10's GPLv3/proprietary claim, with the trademark/distribution review left
explicitly open); `SUPPORT.md`; `.github/ISSUE_TEMPLATE/{bug_report,config}.yml`. **Packaging
(superseded 2026-07-26 — see the head entry):** both attribution files at that point shipped
inside all three zips, installed unconditionally by the Windows installer, and were attached as
version-named release assets (covering the `.pkg` route) — several vendored licences (IJG,
FLAC, Ogg Vorbis) require the notice to accompany a binary distribution; the release-page
assets are now the sole route, with the IJG line in every `INSTALL.txt`. **User docs:** USER_MANUAL gained a Quick start, a Standalone-application
section, system requirements, a TOC and a rewritten FAQ (rescanning per DAW, Windows paths,
Gatekeeper both routes, presets, CPU, latency, automation, session compatibility), and three
defects were fixed — "set Mix to 0 %" was the manual's most-repeated instruction but `mixK` is
Advanced-only (`PluginEditor.cpp:856`), seven controls were documented under host-parameter
names rather than GUI labels, and the MULTIBAND `On` toggle was undocumented; INSTALLATION
gained the missing macOS `mkdir -p`; macOS `INSTALL.txt` dropped its "unsigned developer build
for testing" line. **Policy/plan:** RELEASE_POLICY (artifact list + a new third-party-attribution
precondition), RELEASE_HARDENING_PLAN (RH-R10 corrected, **RH-R11** added for the missing
LICENSE/EULA, §12a post-v0.9.0 follow-ups RH-F1..F6), PERFORMANCE_BUDGET (a required benchmark
procedure for RISK-002 — no infrastructure added), TESTING (a "gaps in the automated coverage"
section: the AU is never auval-validated, and no frozen golden-audio reference exists by
design), KNOWN_ISSUES (**KI-014** AU unvalidated, **KI-015** no declared licence), HANDOVER
(release status: four unsatisfied RELEASE_POLICY preconditions, none fixable by code),
REPOSITORY_MAP, PACKAGING, CI_CD, README, CHANGELOG `[0.9.0]`. Validation: Release build green,
140-check DSP + 774-check state suites green, pluginval strictness 10 green in both modes ×3.
Prior: for the **post-v0.9.0 maintenance audit** (2026-07-24, on `main` @ `4226d2c`):
a repository-wide drift/maintainability pass with **no behaviour change** — no DSP, GUI,
parameter, serialization or CI-gate change, so **no CHANGELOG entry and no version bump**
(CHANGELOG_POLICY rule 3). Fixed: `CMakeLists.txt` — the 9-file wrapper/GUI source list was
duplicated verbatim between the plugin target and `AnamorphStateTests` (a new source added to
one only would silently desync the state suite's coverage) → single `ANAMORPH_PLUGIN_SOURCES`
variable, build graph provably identical (`ninja: no work to do` after reconfigure), plus the
stale "DSP self-tests" section header for a block that builds both suites;
`.github/dependabot.yml` — comment still said JUCE **8.0.14** pinned to a **tag** (it is 9.0.0
pinned by immutable commit SHA, ADR-0022); `release.yml` — stale `v0.8.13` tag example (the
first tag is v0.9.0); `packaging/windows/Anamorph.iss` — validation note cited a CI run and step
name that predate the rewritten script; `packaging/windows/INSTALL.txt` — the zip's own notes
told the reader to run an installer `.exe` that is not in the zip (macOS INSTALL.txt already
said "from the GitHub release"); `BUILD.md` — the Linux dependency list omitted **`libegl-dev`**,
required since JUCE 9, and still called the JUCE pin a "tag" (+ a new EGL row in
TROUBLESHOOTING); `SOURCE_OF_TRUTH.md` — authority level 2 named only `tests/dsp_tests.cpp`,
not `tests/state_tests.cpp`; **KI-002 rewritten** — it claimed manual `xattr` is required for
macOS artifacts full stop, contradicting the v0.9.0 `.pkg` route (payloads are not quarantined;
what remains there is the one-time Gatekeeper approval of the unsigned package), with the same
zip-vs-pkg scope applied in `PACKAGING.md`, `RELEASE_PROCESS.md` and `TROUBLESHOOTING.md`;
`HANDOVER.md` — snapshot HEAD frozen at `86b4273` (pre-#88/#89) → `4226d2c`, and "the three
installable packages" → the two installers + the in-zip Linux scripts; `FUTURE_RISKS.md` /
`KNOWN_ISSUES.md` version-sync headers extended to PR #89. Stale `file:line` evidence
citations corrected where they pointed at unrelated code: RELEASE_POLICY (`build.yml:54,156,373`
→ `:60,180,432`), KNOWN_ISSUES KI-002 (`build.yml:495-498` → `:558-561`; macOS INSTALL.txt
ranges), DEPENDENCY_POLICY (`run-pluginval.sh:34` → `:43-48`), TROUBLESHOOTING
(`run-pluginval.sh:42-44` → `:50-53`; `setup-linux.sh:33` → `:31,36`), PACKAGING (two bare
`INSTALL.txt:` cites qualified to `packaging/macos/`). Validation: Release build green, 140-check
DSP + 774-check state suites green, no new compiler warnings. Reported but NOT fixed (need an
owner / exceed "minimal"): the CMake-version-parsing regex exists in three independent copies
(build.yml Windows PowerShell, build.yml macOS `sed`, release.yml `sed`); the Windows installer
does not remember the VST3 folder across upgrades and its `UninstallDisplayIcon` points at
`Anamorph.exe` even on a VST3-only install; HANDOVER's status cells are multi-thousand-character
single table rows. Prior: for the **v0.9.0 installer/packaging rework** (2026-07-24, PR #89): the
Windows Inno Setup installer gains a **component page** (Install VST3 / Install
Standalone, both pre-selected, ≥1 enforced) and a **single destination page with both
paths** (VST3 above Standalone; the launch-after-install checkbox is removed); the macOS
`.pkg` gains **component selection** (hand-written distribution, `customize="allow"`,
full-install default, system-wide domain); the Linux `install.sh`/`uninstall.sh` move
**into the zip** and switch to **system-wide** installs (`/usr/lib/vst3`,
`/usr/local/bin`, root required) — the separate `Anamorph-<version>-Linux.tar.gz`
package artifact is REMOVED (release archives are flat ZIPs only, payload at the archive
root); all three `INSTALL.txt` files restructured (Installer vs Manual sections, both
system-wide). Docs synced per the lifecycle triggers: PACKAGING (artifact table,
archive-contract note, §Installers rewrite, install-locations table incl. Standalone
row), CI_CD (pipeline item 8 + artifact table), RELEASE_PROCESS (asset list),
INSTALLATION.md (all three platforms + version-number placeholders replacing literal
versions), USER_MANUAL (version-agnostic wording), README (Installing section),
KNOWN_ISSUES (KI-005 wording), RELEASE_HARDENING_PLAN (installer rows/plan wording),
HANDOVER (status rows), REPOSITORY_MAP (packaging rows), CHANGELOG `[0.9.0]` packaging
bullet. Prior: for the **v0.9.0 release preparation** (2026-07-24, PR #87, on top of
`main` @ `86b4273`): version bump 0.8.12 → **0.9.0** + CHANGELOG `[0.9.0]`; **installable
packages** added to CI (Linux `Anamorph-<version>-Linux.tar.gz` + `packaging/linux/`
install/uninstall scripts, Windows `Anamorph-<version>-Windows-Installer.exe` via
`packaging/windows/Anamorph.iss`, macOS `Anamorph-<version>-macOS.pkg` via
`packaging/macos/build-pkg.sh` — all built from the same validated staging dirs, uploaded
as three NEW artifacts, staged fail-closed into the draft release by release.yml alongside
the unchanged zips); **NEW user docs area `docs/user/`** (USER_MANUAL.md — full end-user
manual, attached to releases; INSTALLATION.md — per-platform install guide) closing the
roadmap's "zero user docs" P0 gap; `INSTALL.txt` now ships in all three zips (previously
macOS only). Docs synced per the lifecycle triggers: PACKAGING (installable-packages
section replaces the "no installer" TODO; artifact table + install-locations evidence),
RELEASE_PROCESS (v0.9.0 tag examples; release-asset list), CI_CD (triggers/pipeline/
artifact table incl. the macOS-debug best-effort correction), README (version, user-docs
links, Releases distribution), HANDOVER (all status rows), KNOWN_ISSUES (KI-005 resolved —
installers exist; header re-synced), FUTURE_RISKS (header re-synced), CHANGELOG_POLICY +
CHANGELOG preamble (tags exist from v0.9.0), RELEASE_HARDENING_PLAN (RH-R5 mitigated,
RH-PR-5b/6 skeletons landed, first-tag references v0.8.13 → v0.9.0), REPOSITORY_MAP
(docs/user/ + packaging/* rows), and one stale code comment (SpectrumImager.h alt-click
solo semantics, pre-0.8.10 wording). Prior: for the **product-readiness roadmap review** (v0.8.13 cycle, 2026-07-23, on
`main` @ `dcfad73`; extended the same day with the **item-by-item re-evaluation + independent
gap hunt**: 14 carried items re-classified (Must-now / before-1.0 / nice / defer, with
reasons); NEW findings — **Steinberg VST3-SDK licence compliance + third-party NOTICES**
(recorded as RH-R10 in RELEASE_HARDENING_PLAN §2, the pass's one doc fix beyond the roadmap),
support-workflow gap (no issue templates/SUPPORT.md), the undo/gesture-coalescer test gap
(largest hand-verified-only subsystem; now cheap to cover via the state-harness target), and
a "what 1.0 commits to" policy gap; plus an explicit outdated-assumptions retirement list.
Original entry: (v0.8.13 cycle, 2026-07-23, on
`main` @ `dcfad73` — PRs #82/#83/#84/#85 all merged). Roadmap-only pass, deliberately NOT
another audit: drift review limited to correctness-affecting items (none found). NEW
`worklogs/PRODUCT_READINESS_ROADMAP_v0.8.13.md` — maturity assessment (engineering High /
release Medium-high / commercial Low / UX Medium), blockers split (pre-1.0 vs 0.9.x vs
optional), 4-phase ordered roadmap (v0.8.13 completion → user-facing readiness → commercial
infra → v1.0 prep), documentation-review verdicts (user docs MISSING → Phase-2 item 1;
developer + release docs sufficient/complete), and technical-order rationale (auval before
host matrix; presets before golden-audio; signing before installers; licensing last).
HANDOVER Roadmap row re-pointed at the new plan (the previous pointer directed the next agent
at already-finished work). Prior: for the **RH-PR-8 release-pipeline foundation + its review follow-up
(release-artifact integrity)** — the follow-up archives customer artifacts **at the source**
(`zip -ry` / `Compress-Archive` / `ditto`) because the artifact transport preserves neither
Unix permissions nor symlinks, and turns the release job's staging into a **rename-only**
step (the archives CI validated are published byte-identically; Linux round-trip proven
locally with real build output — 755 bits + `cmp`-identical; annotated-accept /
lightweight-reject tag tests replicated green; PACKAGING.md artifact-layout table updated
to the single-archive contents) — (v0.8.13 cycle, 2026-07-23,
branch `claude/beautiful-sagan-JAUFI` on `main` @ `ee82380` — PR #83 merged). Infrastructure
only, no product behaviour/version change. NEW `.github/workflows/release.yml` (annotated
`vX.Y.Z` tag → fail-closed tag⇄version⇄CHANGELOG validation → the FULL existing `build.yml`
gates reused via a new additive `workflow_call` trigger (6-line `on:`-block diff, branch/PR
behaviour byte-identical; tag pushes triggered nothing before) → **draft** GitHub Release
with versioned artifact copies + `SHA256SUMS.txt` + `RELEASE_MANIFEST.txt` + CHANGELOG-section
notes; `workflow_dispatch` rehearsal mode; `contents: write` scoped to the one release job;
no third-party actions beyond `actions/*` + `gh`; publishing stays manual per RELEASE_POLICY).
No tag created (first: the v0.8.13 release — closes RISK-003 when cut). Docs synced:
RELEASE_PROCESS (§Tagging + release pipeline; stale "no tags TODO" replaced), RELEASE_POLICY
(Artifacts note), CI_CD (Triggers + source-of-truth), FUTURE_RISKS RISK-003 mitigation,
RELEASE_HARDENING_PLAN (§1 baseline rows + §10 RH-PR-8 row per its §13 update protocol),
REPOSITORY_MAP, HANDOVER. Work record:
`worklogs/release-hardening/RH_PR8_RELEASE_PIPELINE.md` (incl. the scoped dependency/security
review: no new third-party actions; SHA-pinning of actions + the pluginval download pin remain
open supply-chain items). Validation: both workflows YAML-parse; the validate/stage shell
logic executed locally against real repo data (version parse, CHANGELOG gate, 46-line notes
extraction); end-to-end proof = the post-merge `workflow_dispatch` rehearsal. Prior: for the **JUCE 8.0.14 → 9.0.0 migration & dependency hardening** (v0.8.13
cycle, 2026-07-23, branch `claude/beautiful-sagan-JAUFI` on `main` @ `1502077` — PR #82
merged). **Dependency migration, zero C++ source changes**: the complete 9.0.0
breaking-change surface has no project exposure (audit table in
`worklogs/JUCE9_MIGRATION_v0.8.13.md` §1.1). CMake pin → the tag's **immutable commit SHA**
`f8f8864…` with new `ANAMORPH_JUCE_VERSION` (supply-chain hardening, audit roadmap item);
`scripts/setup-linux.sh` + `libegl-dev` (JUCE 9 Linux GL uses EGL, not GLX). Validation:
engine output **bit-identical** 8.0.14 vs 9.0.0 (32-scenario twin dump incl. latencies);
140 + 774 suites green under 9.0.0 with the 8.0.14-frozen registry snapshot passing
**unchanged**; `juce_recommended_warning_flags` byte-identical and DSP-TU warnings identical
under both versions (no new warnings); pluginval on the CI gates (local egress 403, ADR-0012
precedent). New **ADR-0022** (Proposed — pending Architecture-Review sign-off + the
DEPENDENCY_POLICY Level-5 audition) + index row. Docs synced: DEPENDENCY_POLICY (SHA-pin rule +
EGL), BUILD, README, TROUBLESHOOTING (pin row + the discovered stale-CMake-cache trap row),
REPOSITORY_MAP, COMPATIBILITY_MATRIX, FUTURE_RISKS RISK-001, KNOWN_ISSUES (KI-011/KI-013
evidence re-verified against the JUCE 9 tree; KI-013 not fixed upstream), HANDOVER — plus a
repo-wide `CMakeLists.txt:NN` citation sweep (+5 shift from the pin block; every cite
re-verified, two pre-existing stale cites fixed: ARCHITECTURE.md, COMPATIBILITY_MATRIX VST3
row). No version bump / CHANGELOG entry (stays inside v0.8.13; a JUCE bump is user-visible at
release time — the release-prep changelog entry will record it, per the 8.0.14 precedent
where the bump shipped inside `[0.8.8]`). Prior: for the **state-serialization & parameter-compatibility regression harness**
(v0.8.13 cycle, 2026-07-23, branch `claude/beautiful-sagan-JAUFI` on `main` @ `823bfbe` —
PR #81 merged). **Validation infrastructure only** — no parameter, serialization, DSP or
user-visible behaviour change; no version bump / CHANGELOG entry (release-prep steps; the
changelog scopes to user-visible changes). NEW: `tests/state_tests.cpp` (9 headless
state-compatibility tests exercising the real `AnamorphAudioProcessor`: schema shape vs
SERIALIZATION_REGISTRY, parameter-registry snapshot vs a frozen fixture, raw-exact
save→load→save round-trip, the v0.2 / pre-0.6.4 / pre-0.8.4 legacy-migration paths via frozen
fixture XMLs, corrupt/foreign-state robustness, user-preset round-trip + exclusion rules, A/B +
view-param preservation), `tests/fixtures/` (registry snapshot + 3 legacy session models), the
`AnamorphStateTests` CMake console target (test block only — shipped targets untouched), and the
blocking CI wiring (`scripts/run-tests.sh` runs both suites fail-closed; the Windows job runs
both exes; step ids/gating unchanged). Docs synced: TESTING.md (new suite section + snapshot
workflow), TESTING_POLICY (Level-2 row + hard gate), RELEASE_COMPATIBILITY_CHECKLIST
(automation annotations on 4 items), CI_CD.md (pipeline step 4), REPOSITORY_MAP, BUILD.md,
DEVELOPMENT.md, README, RELEASE_HARDENING_PLAN (QA-gate row), HANDOVER. The whole edit set was
adversarially verified pre-commit (3 lenses: citation accuracy, test quality, policy/scope);
the pass surfaced and fixed one missed required sync (CI_CD.md), several overstated wordings,
and four test hardenings (recorded in the worklog §4). Design + architecture
record: `worklogs/STATE_HARNESS_v0.8.13.md` (includes the honest remaining-gaps statement:
legacy fixtures are reconstructions; cross-version vN−1→vN reload stays manual). Prior: for the **post-v0.8.12 repository audit & documentation-consistency pass**
(2026-07-22, branch `claude/beautiful-sagan-JAUFI` at `main` @ `64e87c4` — PR #80 merged).
**Documentation-only.** Two things: (1) **retroactive coverage of PR #80** (v0.8.12 GUI interaction
fixes: bare-press no-write + relative Width drag with 3 px threshold in `src/gui/SpectrumImager.{h,cpp}`,
release-outside stuck-press reconcile in `src/PluginEditor.cpp`; recorded in
`worklogs/BANDWIDTH_DRAG_FIX_v0.8.12.md` + `worklogs/MOUSE_RELEASE_STATE_FIX_v0.8.12.md` — PR #80
synced CHANGELOG/HANDOVER/worklogs but missed this file, a lifecycle slip closed here); and
(2) a **full drift audit with minimal corrections**: CHANGELOG `[0.8.12]` re-dated 2026-07-22 (two
of its fixes landed that day) and "MultiBand"/"Bandwidth" normalized to the registry terms
"Multiband"/"Width"; HANDOVER snapshot-HEAD + Build/Release-Status rows refreshed to v0.8.12 (were
frozen at v0.8.11/136 checks) and RH-PR-2 marked shipped; KNOWN_ISSUES + FUTURE_RISKS headers
re-synced (were at v0.8.10) with **KI-013 added** (macOS-inert release-outside reconcile — platform
limitation of the v0.8.12 fix); stale line-number evidence citations refreshed in KNOWN_ISSUES
(KI-001/002/003/006/009/012), FUTURE_RISKS (RISK-002 incl. marking the shipped H1/Wave-3
SoloMonitor skip, RISK-004), POSTMORTEMS (INC-003/004/006/007/009), REPOSITORY_MAP (test count
23→33, `FrameClock.h` + `LR4Xover.h` rows added, CMake cites), README (3-OS pluginval gate scope),
CI_CD (actions @v7), DEPENDENCY_POLICY (`JUCE_*` flags at `CMakeLists.txt:183-188`; "then-current"
qualifiers), PACKAGING + COMPATIBILITY_MATRIX (CMake line cites), ADR_INDEX (130-check/23-test
wording), BUILD + TESTING_POLICY + CODE_STYLE + TROUBLESHOOTING + RELEASE_PROCESS + TESTING (the
same class of post-RH-PR-2 stale CMake/script cites, caught by the pre-commit verification pass),
PERFORMANCE_BUDGET (GUI-redraw row gained its missing Wave-6/0.8.12 record), RELEASE_HARDENING_PLAN
("then-current 136-check" qualifier). The whole edit set was adversarially verified pre-commit
(3 lenses: citation accuracy, history preservation, completeness — see the worklog §1).
Roadmap + deferred-item review recorded in `worklogs/POST_v0.8.12_AUDIT_AND_ROADMAP.md`. No code
change; no version bump. Prior: for **performance Wave 6 — GPU/GUI rendering-efficiency (v0.8.12)** (2026-07-21,
branch `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `c6f3226` — PR #78 merged). **One
behaviour-neutral code change** (`src/gui/SpectrumImager.cpp`, `paintHeadphone`): the per-band solo-
headphone transparency layer was allocating a **plot-sized offscreen framebuffer every Advanced-mode
frame** (JUCE sizes a transparency-layer offscreen to the current clip, which was the whole plot
rounded-rect, not the ~18×15 px glyph); it is now clipped to the glyph (+4 px, covering the earcups +
AA → pixel-identical) and skipped entirely at full opacity. A 5-lens adversarial Workflow (14 agents)
confirmed the idle/Simple/hidden GPU paths are already ~0 and at their frontier, and that the spectrum
**cannot** be made opaque pixel-identically (it nests bottom-flush in a translucent rounded panel, so
its bottom corners straddle a two-colour arc no flat pre-fill reproduces). Build + **140-check suite
green**; no DSP/threading/parameter/serialization/latency change; GPU measurement unavailable in the
headless container (analytical estimate — the affected GL path is macOS/Windows-only, Linux is CPU per
ADR-0011). Version bump `0.8.11 → 0.8.12` (`CMakeLists.txt:14`). Synced: this file, CHANGELOG
(`[0.8.12]` **### Changed**), HANDOVER (Current-Version + Pending-Tasks rows), README (version line).
Evidence: `worklogs/performance/WAVE6_GPU_RENDER_INVESTIGATION.md`. Prior: for the **v0.8.11 final
performance pass & release-readiness audit** (2026-07-20,
branch `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `4aac4eb` — PR #76 = Waves 4+5,
merged). **No code change:** the three remaining named candidates were closed with measured
verdicts. The long-open **GUI fresh-eyes sweep** is DONE — carried in-line after the Workflow
lens was lost to the org token limit a third time; the GUI paint + message-thread surface is
already exhaustively cached/gated across Waves 1–4, the only residual (per-call `Path`/`Font`
locals in the shared `LookAndFeel` slider draws) transient and not worth a restructure. **W3-10**
deferred as Class B (a 50 M-sample probe: `applyWidth(·,·,1.0f)` differs from identity in 15.5 %
of samples, ~1 ULP). **W5-D** prototyped (`scratchpad/kwbench.cpp`): bit-exact vs the scalar
K-weighting chains but only 1.10× at the frozen SSE2 flags — the 4-wide win needs an AVX2/`-march`
build decision (itself numerics-frozen + FMA-divergent). `loudness.process()` confirmed
intentionally unconditional (feeds the live match readout). Release-readiness audit: build +
140-check suite green, no version/test-count drift, no release blockers; documentation-only, so
**no CHANGELOG entry** per CHANGELOG_POLICY rule 3. Synced: this file, PERFORMANCE_BUDGET (final-
pass bullet), HANDOVER (Pending-Tasks + Release-Status rows). Evidence:
`worklogs/performance/FINAL_PASS_v0.8.11_INVESTIGATION.md`. Prior: for **performance Wave 5 — per-block/settled-state runtime optimisation +
v0.8.11 changelog consolidation** (2026-07-20, branch `claude/beautiful-sagan-JAUFI`, rebased
onto main @ `912a755` — the security-tooling/CodeQL-autofix PRs #65–#75; the one rebase
conflict (both sides' new head entry in THIS file) was resolved by keeping both in order).
Eight Class-A trims from a two-lens fresh-eyes Workflow sweep (per-block + per-sample; the GUI
lens was lost to the org token limit for the second time — still open): the
`sameParameters` bitwise no-change gate on per-block parameter adoption (~250 → ~91
instructions per unchanged snapshot), the VelvetNoise parked fast loop (env/gate/history kept
per W3-9), the settled-Width hoist, meter-publish db reuse + bar-fall cache, Level-Match
estBoost memo + MEASURE-coeff cache + silent-block LUFS skip. Rejected with reasons in the
worklog: atomic-exchange load-gating (THREADING_POLICY conservatism), generation-keyed
snapshot cache (incomplete contract); deferred: K-weighting SIMD bank (W5-D), lat==0
mix-ring elimination (W5-A). Callgrind A/B: transparent −4.5 %, hostlike-b64 −5.5 %; twin
dump bit-exact ×19; suite 140 checks; warning set unchanged. Also corrected Wave 4's
drift-contaminated small-buffer datum (real overhead +10–20 %, not 2×). **v0.8.11
consolidation (maintainer instruction):** the `[Unreleased]` Wave-4 entry moved into
`[0.8.11]`, now dated **2026-07-20**, with a new Wave-5 sibling entry; HANDOVER
Current-Version/Release-Status/Pending-Tasks rows re-synced (PRs #60/#61/#62/#63/#76 named;
the CI-/test-only security PRs noted as changelog-exempt per CHANGELOG_POLICY rule 3);
PERFORMANCE_BUDGET gained the Wave-5 bullet and its Wave-4 CHANGELOG citations now point at
`[0.8.11]`. Evidence: `worklogs/performance/WAVE5_INVESTIGATION.md`. Prior: for **performance Wave 4 — idle/background runtime optimisation** (2026-07-19,
unreleased cycle, branch `claude/beautiful-sagan-JAUFI`). Implements the Wave-3 handover's
remaining ranked candidates, all Class A: LevelMeter static-layer cache + opaque (the H2/H13/N2
recipe — the last of the four visualizers; −29…−31 % per meter frame, raw-pixel-identical),
SpectrumImager per-transform dB cache (−92 % of the decay-tail tick) + paint `Path` reuse,
editor 24 Hz memoisations (preset-name shaping keyed on inputs, combo-hover pre-gate, match
readout on value change), Vectorscope hidden-editor gate, Haas parked fast path (rings keep
recording; new Test 34 `testHaasParkedWarmHistory` guards the warm history), vectorized NaN-scan
detector (bit-identical healing, NaN-injection twin rows), segmented scope/bypass ring copies
(publication contract unchanged). Callgrind A/B: transparent floor −4.9 %, haas-parked −12.4 %,
bypass-on −3.0 % whole-run instructions; 19-scenario twin dump bit-exact; suite 33 DSP tests +
A/B guard, checks 136→**140**; warning set byte-stable. A 4-lens verification/discovery
Workflow was lost to an org spend limit — verification was carried in-line against primary
sources; the fresh-eyes sweep is recorded as a follow-up in the worklog. Synced:
PERFORMANCE_BUDGET (GUI row + two new Wave-4 cost bullets), CHANGELOG (`[Unreleased]`, folded
into `[0.8.11] — 2026-07-20` by the Wave-5 consolidation),
TESTING_POLICY + TESTING + README + RELEASE_HARDENING_PLAN QA row (32/136 → 33/140), HANDOVER
(Test Status / Pending Tasks); investigation + validation evidence in
`worklogs/performance/WAVE4_INVESTIGATION.md`. Prior: for the **security-tooling configuration
review** (2026-07-19, branch `security-tooling/config-review`). The four generated GitHub
security configs were optimized against the repository's actual shape: `dependabot.yml` was
**invalid as generated** (`package-ecosystem: ""` — rejected by the Dependabot schema) and now
monitors the only supported ecosystem here, `github-actions` (weekly, grouped into one PR; JUCE
stays FetchContent-pinned + review-gated per `DEPENDENCY_POLICY.md`); `codeql.yml` switched
`c-cpp` from `build-mode: none` (near-zero include resolution — JUCE is absent from the bare
checkout) to a **manual build** mirroring the Linux CI steps but compiling only `Anamorph_VST3`
+ `AnamorphTests` with `-DANAMORPH_BUILD_STANDALONE=OFF`, with alerts scoped to repo-own code
(`paths-ignore: build` excludes the FetchContent'd JUCE tree) and docs-only changes skipping
the workflow; `msvc.yml` gained the **required** build step (juceaide-generated files),
JUCE-as-external suppression (`ignoredIncludePaths`/`ignoredTargetPaths` → `build/_deps`),
path-filtered triggers, and `upload-sarif` v3→v4; `dependency-review.yml` comments on failure
only. Validated: schema (github-workflows + dependabot vendor schemas), local build of the
exact analysis targets, 136/136 self-tests. Synced: CI_CD (§Security scanning),
REPOSITORY_MAP. Prior: for **RH-PR-2 Build Hardening + review follow-up** (2026-07-18, release-hardening
program, ADR-0021, PR #63 `release-hardening/build-hardening`, rebased onto the v0.8.11 bump —
the CHANGELOG entry now lives under `[0.8.11]` **### Security**). Behaviour-neutral binary
hygiene: an `AnamorphHardening` INTERFACE target pins `-fstack-protector-strong`, section GC,
Release `-g`, full RELRO (`-z,relro,-z,now,-z,noexecstack`) on Linux, `-Wl,-dead_strip` on
macOS, and `/guard:cf` + `/DYNAMICBASE /NXCOMPAT` + Release `/Zi`+`/DEBUG /OPT:REF,ICF` on
Windows; CI runs a retain-then-strip pipeline (split `.debug`/dSYM/PDB captured as separate
`Anamorph-<OS>-debug` artifacts, public binaries stripped — Linux VST3 −19.8%, `nm: no
symbols`, dynamic exports untouched; Linux strips before pluginval so the gate validates
shipped bytes; macOS order dsymutil → strip → codesign with `|| true` swallowing removed;
`if-no-files-found: error` everywhere). **Review follow-up (artifact-safety):** customer
uploads are now gated on their strip/staging steps succeeding (`steps.<id>.outcome` — the old
`if: always()` could upload an unstripped Linux binary after a strip failure), the Windows
staging purges ALL debug material from the public copy immediately after the copy and before
any abortable validation (the old order could leak the in-bundle PDB), and both public staging
steps end with an explicit no-symtab/no-`.debug`/no-PDB self-validation. Numerics-affecting
flags untouched; proven by a byte-identical twin engine dump + a green full suite (136 checks
post-Wave-3). Baseline finding recorded: symbol visibility was ALREADY hidden via JUCE's
plugin helpers (plan §1 drift corrected). Synced: new ADR-0021 (+ ADR_INDEX row),
RELEASE_HARDENING_PLAN (§1/§2/§6.1/§10/§12 statuses + the pending QA-row 32/136 sync noted by
the version-bump entry below), CI_CD, PACKAGING, BUILD, REPOSITORY_MAP (worklogs/ entry merged
with Wave 3's), CHANGELOG (`[0.8.11]` ### Security); investigation + validation + review
evidence in `worklogs/release-hardening/RH_PR2_INVESTIGATION.md`. Prior: for the **v0.8.11 version preparation** (2026-07-18, PR
`release/v0.8.11-version-bump` — version/release metadata only, no functional change).
`CMakeLists.txt` project version 0.8.10 → **0.8.11** (single source: `ANAMORPH_VERSION_STRING`
and the JUCE plugin version derive from it); README version line; HANDOVER status rows
(Current Version / Build / Release / Pending Tasks — the completed Wave-3 candidate removed
from the backlog text). CHANGELOG: the `[Unreleased]` Wave-3 entry became **`[0.8.11] —
2026-07-18`** (evidence PR #62, merge `b2481db`), and the two post-release maintenance fixes
recorded under `[0.8.10]` after it shipped — the slow-drag follower regression (PR #60,
`3268cc2`) and the 192 kHz terminal-snap robustness fix (PR #61, `c72d3c3`) — **moved into
`[0.8.11]`** with their evidence lines updated: the released 0.8.10 binaries (PR #59,
2026-07-14) predate both, so `[0.8.10]` claiming them was recorded drift against
CHANGELOG_POLICY rule 2 (no invented history). Deliberately untouched: PR #63's build-hardening
work and files (CMake hardening/CI/ADR-0021/RELEASE_HARDENING_PLAN — including that doc's
still-pending 32/136 QA-row sync noted in the previous entry). Prior: for
**performance Wave 3 — runtime optimisation** (2026-07-18, unreleased cycle,
PR `performance/wave3-runtime-optimization`). Investigation-first wave (baselines, callgrind
attribution and the full decision record live in `worklogs/performance/WAVE3_INVESTIGATION.md`
— a new top-level `worklogs/` directory for session-local records, added to REPOSITORY_MAP).
Four DSP changes + one GUI flag: **(1)** SoloMonitor's H1 cold gate decoupled from cutoff
proximity (gains alone prove the passthrough; a no-solo split drag — ~22 % of the drag-profile
instructions — no longer wakes the bank; Class A, guarded by new `testSoloColdThroughDrag`,
Test 33, proven to fail pre-change); **(2)** per-split LR4 coefficient sharing
(`LR4Xover::copyCoefficientsFrom`): x/dx/ax/dax always share one cutoff, so the glide, the
aligned-block resync and `setBankCutoffs` compute `tan` once per split (12→3 per sample worst
case) and the never-processed `ax[0]`/`dax[0]` are not updated at all (Class A); **(3)** the
phase-compensation allpass is the ladder's first 2nd-order section computed directly
(`LR4Xover::processSampleAllpass` — the recorded 0.8.10 follow-up; Class B ≤ 1.2e-7, 2–24
samples per 204,800 in the twin dump); **(4)** settled output-stage and settled-Mix per-sample
constants hoisted per block (Class A); **(5)** SpectrumImager FFT `ignoreNegativeFreqs=true`
(consumers read bins ≤ N/2 only; identical visuals). Rejected with reasons (recorded in the
worklog): LoudnessMatch off-gating (Measure readout + Apply are live consumers with Match off),
LevelMeters editor-closed gating (held peaks must persist), velvet parked-envelope freeze.
Fair interleaved before/after (session-local, 48 kHz): drags −35…−50 %, settled multiband
−9…−17 %, transparent floor −6.6 %. Suite 32 DSP tests + A/B guard, checks 130→**136**, twin
dump bit-exact on every Class-A row. Synced: PERFORMANCE_BUDGET (allpass follow-up marked done,
H1/crossover-move/GUI rows updated, stale process() line-range corrected), CHANGELOG
([Unreleased]), README, TESTING_POLICY, TESTING, HANDOVER, REPOSITORY_MAP (worklogs/).
**Deliberately NOT touched** (a parallel release-hardening PR owns release documentation):
RELEASE_HARDENING_PLAN.md — its QA-gate row still reads "31 DSP self-tests … (130 checks)" and
needs the one-line 32/136 sync once the PRs land (recorded drift, not silently fixed).
Prior: the **high-sample-rate crossover terminal-snap robustness fix** (2026-07-17,
v0.8.10 maintenance, PR `fix/high-sr-crossover-snap`). Review of the slew-limited smoother found
a numerical edge case, confirmed by exact-float simulation: the per-sample one-pole add stalls
once its move drops below `ulp(f)/2`, and the terminal-snap eps (0.05 + 2e-4·f) out-runs that
stall only up to 96 kHz (margin ≥ 1.76×; 3.55–4.27× at 44.1/48 kHz) — at 192 kHz the margin is
**0.88–0.98×** just past every binade edge ≥ 2048 Hz (parameter-range hard-stall zones
[2049–2093] [4097–4437] [8194–9125] [16388–18500] Hz, resting gap up to 3.75 Hz, both
directions; higher binades to the 86.4 kHz DSP clamp stall too, ≤ 0.4 cents), so a moved crossover
could rest short of its target forever: audio < 0.4 cents off, but the SoloMonitor settled fast
path (H1, needs ≤ 0.05 Hz) never engaged and filters/smoothers stayed hot. Minimal fix in
`MultibandWidth.cpp`/`SoloMonitor.cpp`: the glide **also snaps exactly when the float add can no
longer move the cutoff** — eps, R(f), smoothing, fade thresholds untouched; ≤ 96 kHz
bit-identical (eps snap always fires first). Guarded by `testHighRateCrossoverSnap` (Test 32;
DSP tests 30→**31**, checks 115→**130**): bitwise-exact landing + cold-path engagement at four
rates; pre-fix fails at 192 kHz only (0.4688/0.9375/1.8750/3.75 Hz, never cold — proven by
stash-rebuild). Synced: ADR-0015 (new "High-Sample-Rate Terminal-Snap Robustness" section),
CHANGELOG, README, TESTING_POLICY, TESTING, HANDOVER, RELEASE_HARDENING_PLAN QA row. Test-only
`getLiveCutoff`/`isSettledCold` accessors added to the two headers. Prior: the **crossover
follower slow-drag regression fix** (2026-07-17, post-merge
v0.8.10 maintenance, new PR). The v0.8.10 final flat ~4 oct/s cap was calibrated at a 150 Hz
crossing, but the display maps ~10 octaves onto ~900 px, so ordinary 400–2000 px/s drags are
4–22 oct/s — every normal drag trailed by octaves and crawled after release while violent flicks
escaped via the discrete-jump fade (the reported slow-vs-fast inversion). The glide in
`MultibandWidth`/`SoloMonitor` is now a **slew-limited smoother**: a ~20 ms one-pole demand
clamped per sample to a **frequency-proportional cap R(f) = 4·max(1, f/300 Hz) oct/s** — the
swept-allpass shift stays ≤ 1.25 Hz below 300 Hz (150 Hz crossing still ~14 cents) and ~7 cents
of the crossing above; the one-pole leg de-staircases the 60 Hz UI cadence and tapers arrivals
(a bare clamp landing measured −24 dBc; fref = 300 is the measured spur knee, −41.3 dBc at the
floor). Normal drags now track 1:1 (600 px/s converges 0.01 s after release, was 0.63 s); all
prior artifact bounds hold at the same values. Test 29 gained a normal-drag tracking regression
on both paths (checks 112→**115**; flat-cap re-pin fails both — verified in both directions).
Synced: ADR-0015 (new "Crossover Follower Slow-Drag Regression" section, + ADR_INDEX row),
CHANGELOG, DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, DSP_POLICY inv. 3, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, FUTURE_RISKS RISK-002, KI-012, TESTING, HANDOVER. Prior: the
**PR #59 final review fixes** (2026-07-17, two items). (1) **Forced duck
during an ordinary fade-out** — a forced request (undo/redo/A-B/preset) landing in the ~6 ms
fade-out window of a non-forced discrete duck was consumed but dropped, so the swap finished
with normal-duck semantics (no silent-bottom wholesale swap/smoother snap/clean-slate reset —
a stale Haas tail replayed at 0.494 peak against silent input). The engine now upgrades the
in-flight duck to forced in place (dry-fill stays off: never engaged mid-fade). CHANGELOG +
`testForcedSwapDuringOrdinaryFadeOut` (Test 31; DSP tests 29→**30**, checks 106→**112**;
README, TESTING_POLICY, TESTING, HANDOVER synced). (2) **Crossover fade comments corrected**
(comment-only, `MultibandWidth.cpp/.h`, `SoloMonitor.cpp`): the discrete-jump bank fade's
destination is latched at fade start — movement during the fade waits (glide paused), and after
the fade lands a NEW fade may start toward the then-current targets (skipped if within 0.1 oct);
the old wording implied the fade always (re)targets the newest cutoffs. Prior: the
**v0.8.10 final follower decision** (2026-07-17, PR #59). The
bounded-convergence follower (1.25 oct/s cap + release consolidation) was evaluated in
interactive testing and **rejected for interaction latency**; final design (ADR-0015
"v0.8.10 final decision"): the rate cap rises to a hard **~4 oct/s** (drags ≤ 4 oct/s track
exactly — zero GUI/DSP gap; faster movement keeps a controlled ~15-cent worst FM at a 150 Hz
crossing, ~half the pre-fix implementation; a 6-oct flick catches up in ~1.25 s of continuous
motion) and the **release consolidation is removed entirely** (no timers, no delayed jump);
the discrete-jump bank fade is the only special event left. Synced: ADR-0015 (+ ADR_INDEX row),
DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, DSP_POLICY inv. 3, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, FUTURE_RISKS RISK-002, KI-012 (rewritten to the accepted controlled-FM
trade), CHANGELOG, TESTING, HANDOVER;
Test 29 re-thresholded to the final operating point (18-cent bound, 1.7–2.2 s convergence
window; both rejection directions re-verified; checks stay **106**). Prior: the
**pre-release hardening plan** (2026-07-17, PR #59, docs-only): new
`docs/architecture/RELEASE_HARDENING_PLAN.md` — the planning artifact for the commercial-release
program (licensing, anti-piracy posture, build hardening, signing/notarization, installers,
release pipeline, multi-agent parallelization contract). No code change; decisions it proposes
are gated on future ADR-0016..0020 + Architecture Review. Architecture self-coverage count
updated (15 docs; ADR count synced to 15 after ADR-0015). Prior: the **v0.8.10
follower refinement + investigation record** (2026-07-14, PR #59) — bounded convergence via
rate cap 1.0 → 1.25 oct/s plus release consolidation, with the complete A–H3 architecture
investigation history (including the H3 hostile-review failure on width purity and
the linear-phase roadmap direction) made permanent as **ADR-0015**. Prior: the **third v0.8.10 pre-merge correctness
round** (2026-07-14, PR #59), two
items. (1) **Split-movement final design** — pure-sine testing rejected the second round's
one-pole tracker too (it FMs at the full drag rate: ~50 cents measured at a fast crossing). A
candidate matrix (rate caps, one-pole, chained/consolidated fades) was measured against the
sine protocol; shipped: a **hard ~1 oct/s cutoff rate cap** (swept-allpass shift bounded at
~0.31 Hz, below the pure-tone JND at any drag speed — worst measured chunk 3.6 cents at a
150 Hz crossing, spurs at the −41 dBc floor) plus a **discrete-jump bank crossfade** (target
steps > 1.5 oct between consecutive blocks land in ~12 ms). The audible-position-eases-at-
~1 oct/s trade is recorded as **KI-012** (with the linear-phase escape hatch gated behind an
Architecture Review). Docs: DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, DSP_POLICY inv. 3, FUTURE_RISKS RISK-002, CHANGELOG; Test 29 reworked to
grade the whole movement (drag + entire ease incl. the tone crossing + discrete-jump landing).
(2) **Forced-duck dry-fill output-gain latch** — the fill played the raw ring at unity while
the processed path around it was scaled by Output Gain × Balance; at −24 dB an undo/redo Mix
toggle spiked 15.8×. The fill gain is now latched at fade-out entry like `dryDuckLat`
(SIGNAL_FLOW forced-swap note, CHANGELOG); new `testDryFillRespectsOutputGain` (Test 30). DSP
test count 28→**29**, checks 97→**102** (README, TESTING_POLICY, TESTING, HANDOVER). Prior: the
**second v0.8.10 pre-merge correctness round** (2026-07-14, PR #59), two
fixes. (1) **Split-drag transition rework** — pure-sine testing of the first round's chained bank
crossfades showed modulation sidebands around the tone (−25…−28 dBc during a fast drag: a chain
of ~12 ms fades is amplitude/phase modulation and cannot preserve the magnitude response
mid-fade). Final hybrid, picked by measurement: a bounded-time per-sample one-pole cutoff glide
(τ ≈ 15 ms — flat magnitude at every instant, smooth phase, settles ~75 ms after the last move)
for continuous movement, plus a single bank crossfade only for multi-octave jumps (> 1.5 oct,
where the fade's mod-2π phase wrap beats a glide's chirp). Documented across DSP_ALGORITHMS,
DSP_GRAPH_REFERENCE, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT, DSP_POLICY inv. 3, FUTURE_RISKS
RISK-002, CHANGELOG; Test 29 gained a spectral-purity check (max spur < −31 dBc while a split
crosses a 1 kHz tone at 60 Hz UI cadence; the chained fades measure −28.5 and fail), checks
96→**97** (TESTING, HANDOVER). (2) **KI-011, Apple-Silicon-native tooltip white corners** —
juce::TooltipWindow declares itself opaque while drawTooltip leaves the capsule corners
unpainted; the undefined pixels render white on ARM-native AppKit (Intel/Rosetta showed the
stale transparent backing). The editor now marks the TooltipWindow non-opaque on macOS
(KNOWN_ISSUES KI-011, CHANGELOG; hardware re-test pending, KI-006 pattern). Prior: the first
**v0.8.10 pre-merge correctness round** (2026-07-14, PR #59), three fixes:
(a) **Split-drag pitch shift** — `MultibandWidth` and `SoloMonitor` no longer glide their
crossover cutoffs per sample (a swept LR4's allpass phase rotation audibly detuned the audio
during and after a fast split/band drag); cutoff changes are now ~12 ms fixed-coefficient bank
crossfades (state-copied idle bank at the newest targets). Documented in DSP_ALGORITHMS
(MultibandWidth + SoloMonitor), DSP_GRAPH_REFERENCE (shared crossover sub-bank), PERFORMANCE_BUDGET
(crossover-move cost + the allpass-compensation candidate's obsolete sub-item), DSP_POLICY
invariant 3 wording, CHANGELOG; guarded by `testMultibandSplitDragNoPitchShift` (Test 29 — fails
at ~24 cents on the pre-fix glide). DSP test count 27→**28**, checks 90→**96** (README,
TESTING, HANDOVER). (b) **Band Solo alt-click redesign** — alt-clicking an UNSOLOED band's icon
now solos only that band (exclusive) instead of all bands; soloed-band alt-click (clear all) and
plain click unchanged; CHANGELOG (GUI-only, same `mbSolo` single-gesture write). (c) **Option/
double-click reset undo fix** — `Knob::doReset` now wraps the value write in a host change
gesture (the imager's split/width resets already did), so a reset is one undoable step that
clears redo; `undo()`/`redo()` flush a settled-but-unpolled gesture first. Conforms to ADR-0008's
gesture-coalesced design (no ADR change); CHANGELOG. No parameter/serialization/latency/threading
change; the split-drag fix changes only the transition behaviour of moving crossovers (settled
output bit-identical). Prior: the **v0.8.10 release finalization** (2026-07-14, PR #59). The `[Unreleased]`
CHANGELOG entries (undo/redo forced-duck dry-fill + rapid-swap robustness, multiband flat
recombination, adaptive `FrameClock` GUI refresh) are folded into the `[0.8.10]` section; the
version is bumped to 0.8.10 across CMakeLists / README / HANDOVER / KNOWN_ISSUES / FUTURE_RISKS;
KI-009 (REAPER Save Preset) is carried forward as an open, host-specific issue (not fixed).
Includes the pre-merge review round: (a) Multiband
flat recombination — the crossover reconstruction now phase-compensates each lower band by the
splits above it (allpass telescoping), removing the −17.75 dB dip at close crossovers; documented
in DSP_ALGORITHMS (MultibandWidth) + CHANGELOG, guarded by `testMultibandFlatRecombination`
(Test 28). (b) Rapid forced-swap dry-fill robustness — every forced swap re-evaluates dry-fill,
never reusing a prior swap's stale offset; CHANGELOG + `testRapidForcedSwapDryFill` (Test 27).
(c) FrameClock review — the Advanced-only SpectrumImager now stops its display-rate clock while
hidden (Simple mode), mirroring the meters (no unnecessary vblank ticks). DSP test count
25→**27**, checks 77→**90** (README, TESTING_POLICY, TESTING, HANDOVER; `testRapidForcedSwapDryFill`
gained fade-in and fade-out latency-crossing retarget cases during the pre-merge verification pass).
No parameter/automation/
preset/serialization/latency change; the multiband fix changes only the multiband audio output
(the intended fix — twin dump confirms latency unchanged, non-multiband scenarios identical).
Prior: for the **post-v0.8.9 PR** (three items + a fresh profiling baseline). (1) Undo/Redo
audible-dropout fix — the forced switch duck is now dry-filled from the true-bypass ring;
documented in SIGNAL_FLOW (forced-swap note) + CHANGELOG `[Unreleased]`, guarded by the new
`testForcedSwapNoDropout` (Test 26, count 24→**25** DSP tests, 73→**77** checks). (2) Adaptive
display-rate GUI refresh — new `gui::FrameClock` (VBlank, capped ~120 Hz) replaces the four fixed
60 Hz visualizer timers, with dt-corrected ballistics; new module coverage row + THREAD_MODEL timer
table/top-row + PERFORMANCE_BUDGET GUI row + CHANGELOG `[Unreleased]`. (3) **KI-009** added — the
REAPER Save Preset focus report (host-specific, pending manual investigation), version-sync header
updated. A post-v0.8.9 DSP+GUI profiling baseline was produced (callgrind Ir + wall-clock +
EdBench A/B); per established convention the report stays in the session scratchpad and is **not**
committed (no volatile clock-dependent numbers enter the permanent budget). Prior: the **v0.8.9
release** (finalized 2026-07-12, PR #58) — the `[Unreleased]` CHANGELOG entries from Wave-2 Step-1
and Step-2 (H3/H4/H5/H6/H11/H15/ALG-4, the tooltip revert, and the `viewGenWatcher` destructor
lifecycle fix) folded into `[0.8.9]`; every `CHANGELOG [Unreleased]` evidence citation across the
docs set (PERFORMANCE_BUDGET) updated to `CHANGELOG [0.8.9]`. One new module row (`LR4Xover`, the
flat-state LR4 crossover); H3/H4/H5/H6/H15 documented across DSP_ALGORITHMS, DSP_GRAPH_REFERENCE,
SIGNAL_FLOW, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT, THREAD_MODEL/THREADING_POLICY (two new
generation counters, same staleness-hint pattern), TESTING (new `testDryAlignGateRecomb`, test
count 23→24).
Prior: Wave-2 Step-1 (PR #58) — no module-coverage change; the H11/ALG-4 DSP work documented in
DSP_ALGORITHMS + PERFORMANCE_BUDGET + CHANGELOG, and `AI_AGENT_POLICY.md` gained constraint C8
(UI text requires explicit instruction). Retro-covers PR #57 (KNOWN_ISSUES KI-008 added; no
coverage change — this header was missed in that PR). Prior: the initial 0.8.9 version bump
(PR #56) — no coverage change; the 0.8.8 idle-performance PR (#54) — threading paths
(`soundParamGen`) and the ScopeBuffer per-block publication model documented; prior full audit at
HEAD `c605fbe` (JUCE 8.0.14).

## Code-module coverage

| Module | Documented in | Coverage | Confidence |
|---|---|---|---|
| `AnamorphEngine` (chain/switch machine) | SIGNAL_FLOW, DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, ADR-0004/0005/0006 | Full | Verified |
| `EngineParameters` (POD boundary) | ARCHITECTURE, API_REFERENCE, ADR-0001 | Full | Verified |
| `PluginParameters` / APVTS | PARAMETER_REGISTRY, PARAMETER_REFERENCE, ADR-0002 | Full | Verified |
| `InternalState` | PARAMETER_REGISTRY, STATE_SERIALIZATION, ADR-0010 | Full | Verified |
| `PresetManager` | API_REFERENCE, STATE_SERIALIZATION | Partial (interface + role; preset file format not exhaustively documented) | Verified |
| State save/recall | STATE_SERIALIZATION, SERIALIZATION_REGISTRY | Full | Verified |
| `MidSide` | DSP_ALGORITHMS | Full | Verified |
| `HaasProcessor` | DSP_ALGORITHMS | Full | Verified |
| `VelvetNoise` | DSP_ALGORITHMS | Full | Verified |
| `ChorusEngine` | DSP_ALGORITHMS | Full | Verified |
| `MonoMaker` | DSP_ALGORITHMS, SIGNAL_FLOW, ADR-0006 | Full | Verified |
| `MultibandWidth` | DSP_ALGORITHMS, ADR-0005/0009 | Full | Verified |
| `LR4Xover` (flat-state LR4 crossover, Wave 2 / H6) | DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT + its own bit-exactness contract comment | Full | Verified |
| `SoloMonitor` | DSP_ALGORITHMS, ADR-0004/0006 | Full | Verified |
| `LoudnessMatch` | DSP_ALGORITHMS, ADR-0007 | Full | Verified |
| `Correlation` / `LevelMeters` / `ScopeBuffer` | DSP_ALGORITHMS, THREAD_MODEL | Full | Verified |
| Threading / OpenGL gate | THREAD_MODEL, ADR-0011 | Full | Verified |
| Latency / PDC | LATENCY_MODEL, ADR-0003 | Full | Verified |
| Real-time safety | REALTIME_SAFETY_AUDIT, REALTIME_AUDIO_POLICY | Full | Verified |
| `gui/FrameClock` (adaptive display-rate refresh, post-0.8.9) | THREAD_MODEL, PERFORMANCE_BUDGET, CHANGELOG + its own header contract | Full | Verified |
| `PluginEditor` / `gui/*` | THREAD_MODEL, REPOSITORY_MAP | Partial (threading + lifecycle documented; per-widget layout/LookAndFeel not exhaustively) | Verified |
| Build / CI / packaging | BUILD, CI_CD, PACKAGING | Full | Verified |
| Tests | TESTING, TESTING_POLICY | Full | Verified |
| Performance (numbers) | PERFORMANCE_BUDGET | Structural only | Unverified (no benchmark data — TODOs) |
| Host (DAW) compatibility | COMPATIBILITY_MATRIX | Listed | Unverified (no in-repo DAW tests) |
| AAX, mono→mono | COMPATIBILITY_MATRIX, COMPATIBILITY_POLICY | Documented as excluded | Not Supported |

## Documentation-set self-coverage (deliverables present)

| Tier | Files | Status |
|---|---|---|
| docs root | SOURCE_OF_TRUTH, HANDOVER, REPOSITORY_MAP, DOCUMENTATION_COVERAGE, POSTMORTEMS, KNOWN_ISSUES, FUTURE_RISKS, COMMERCIAL_STATUS | Present |
| user | USER_MANUAL, INSTALLATION | Present |
| architecture | 15 docs (incl. RELEASE_HARDENING_PLAN) + ADR_INDEX + 18 ADRs (0016–0020 reserved, see plan §8) | Present |
| worklogs | performance/ (Waves 3–6 + the v0.8.11 final-pass and crossover-glide investigations), release-hardening/ (RH program working evidence; finalized decisions live in ADRs), root-level v0.8.12 GUI-fix records (`BANDWIDTH_DRAG_FIX_v0.8.12.md`, `MOUSE_RELEASE_STATE_FIX_v0.8.12.md`) + `POST_v0.8.12_AUDIT_AND_ROADMAP.md` + `STATE_HARNESS_v0.8.13.md` | Present |
| procedures | 8 docs | Present |
| policies | 15 docs | Present |
| root — developer/status | README, CHANGELOG, CLAUDE | Present |
| root — legal | NOTICE, THIRD_PARTY_LICENSES, EULA (unapproved draft — not in force), PRIVACY, TRADEMARKS | Present |
| root — internal/testing | SUPPORT | Present |
| .github | ISSUE_TEMPLATE/{bug_report,config}.yml, workflows/*, dependabot.yml | Present |

## Known coverage gaps / TODOs

- **Performance numbers** — `PERFORMANCE_BUDGET.md` carries explicit TODOs; populate from profiling.
- **DAW host matrix** — `COMPATIBILITY_MATRIX.md` hosts are Unverified; populate from manual testing.
- **GUI per-widget reference** — editor layout/LookAndFeel is documented at the threading/lifecycle
  level only; a per-widget reference is not present (low priority — GUI changes don't gate releases).
- **Pre-0.6 version history** — CHANGELOG entries for early versions are Partially Verified (README
  + commits); no git tags exist for exact per-version attribution.

## Update protocol

On any change, set this file's "Last updated" to the new HEAD and adjust the affected rows. A new
module → add a row; a new doc → add to self-coverage; new perf/host data → upgrade the confidence.
