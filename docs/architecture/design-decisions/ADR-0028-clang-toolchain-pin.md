# ADR-0028 — CI Clang toolchain: pinned to upstream stable (22) from apt.llvm.org, by major

**Status:** **Accepted** (Build System change — `ARCHITECTURE_REVIEW_GATE.md`; approved by the
maintainer 2026-08-17, together with the policy amendment below.)

**Amends:** `docs/policies/ARCHITECTURE_REVIEW_GATE.md`, the **Build System change** item, with a
*Compiler and toolchain versions* rule. Per `ADR_POLICY.md` rule 5 and `SOURCE_OF_TRUTH.md` ("An ADR
may change a Policy, but only by an explicit new/updated ADR"), this ADR is the instrument that makes
that change; the Policy text carries the rule, this ADR carries the reasoning and the reconciliation.

## Context
`ANAMORPH_CLANG_VERSION` in `.github/workflows/build.yml` is the single authority for the Clang major
two CI jobs use: `linux-clang` (the first-party warning gate) and `sanitizers` (ASan + UBSan, and the
reason the pin also fixes `libclang-rt-<n>-dev`). The pin exists because
`scripts/clang-warning-baseline.txt` records per-`(path, flag)` site counts, and which diagnostics
Clang emits is a property of the major — so an unpinned `clang` made a runner-image bump
indistinguishable from a source regression.

**Neither job uploads an artifact.** The shipped Linux bytes are GCC's, Windows' are MSVC's, macOS's
are AppleClang's. This pin governs *detectors*, not the product — which is why it can move on
measurement alone, with no twin dump and no Level-5 audition.

The pin was **18** for as long as it had existed. This ADR was first written to move it to **20**, the
newest major in Ubuntu's own archives for noble; that step is retained below as an option, because the
reasoning that rejected 21/22 at the time contained a factual error worth recording rather than
quietly deleting. Clang 18.1.0 is 895 days old, 18.x has had no upstream release since 2024-06-20, and
the compiler this job exists to *preempt* — AppleClang on `macos-latest` — is far ahead of it.

## Problem
Put the warning gate and the sanitizer host on a compiler that is worth gating against, without
changing the C++ language standard (a separate contract, ADR-0027), without weakening the baseline
guard, and without letting a **packaging boundary in Ubuntu** decide how current this project's
detectors are.

## Options
1. **Keep 18.** Rejected on currency: an end-of-life compiler is a poor reference point for a gate
   whose entire output is that compiler's diagnostics.
2. **19.** Strictly dominated by 20 — same acquisition route, one major less.
3. **20 — chosen in the first draft of this ADR, now superseded.** `1:20.1.2-0ubuntu1~24.04.3` from
   `noble-updates`/`noble-security` universe: the newest major reachable from the **stock** archives,
   so no new apt source. It was rejected as the final answer once the premise below was corrected: 20
   is not upstream stable, and "what Ubuntu packages for noble" is a fact about Ubuntu's release
   process, not about this project's needs. It is also published for **amd64/i386 only**, so it could
   never serve a future `ubuntu-24.04-arm` job.
4. **22 — chosen.** Upstream **stable** (22.1.8, released 2026-07-10; 22.x is the branch upstream
   still shipped point releases on, and no 22.1.9 was made). Ubuntu publishes no `llvm-toolchain-22`
   for noble, so it comes from **apt.llvm.org**, upstream's own Debian/Ubuntu channel — whose
   `llvm.sh` states `CURRENT_LLVM_STABLE=22`, i.e. upstream agrees this is the stable major.
   *A correction to this ADR's first draft, recorded rather than removed:* that draft rejected 21/22
   as having "no noble publication at all". The verification behind that sentence was of **Ubuntu's**
   archive pockets (`noble`, `-updates`, `-security`, `-backports`), which is accurate; the conclusion
   drawn from it was not. apt.llvm.org publishes noble suites for **17 through 23**, on
   `amd64 arm64 s390x`. Requiring a different install mechanism is an implementation cost, not an
   availability blocker, and it was wrong to treat it as one.
5. **23.** Rejected as **not stable**: at the time of this decision 23.1.0 is at **rc3** (2026-08-12)
   and no final 23.1.0 exists. apt.llvm.org's noble-23 suite is its development snapshot. The rule
   this ADR sets is *upstream stable*, which excludes a release candidate.
6. **Unpinned (`clang`/`clang++`).** Rejected — the exact defect the pin was created to remove, and
   worse now than when the pin landed: the unsuffixed alias is 18 on the current image and 21 on the
   26.04 toolset, so an image move would be a silent multi-major jump.
7. **A full version pin.** Rejected on three grounds, and the first is decisive: apt.llvm.org is a
   *snapshot* archive whose pool **retains only the current `.deb` per architecture**, so an exact
   version pin stops resolving — a hard `apt-get install` failure — as soon as the suite is rebuilt.
   On an open branch that is a matter of days. Second, the string is distribution- and rebuild-specific
   anyway (`1:20.1.2-0ubuntu1~24.04.3` on noble against
   `1:22.1.8~++20260714014902+ca7933e47d3a-1~exp1~…` here). Third, it is unenforceable by the guard
   that gives the pin its value: `check-clang-warnings.py` compares **majors**, so a patch pin would be
   a promise nothing checks.

## Decision
**`ANAMORPH_CLANG_VERSION: 22`, pinned by MAJOR only, in the one place it already lives**, installed
from apt.llvm.org by `scripts/setup-llvm-apt.sh`. Everything else derives from the env value by
interpolation — both jobs' installs, `clang-<n>`/`clang++-<n>`, both ccache lineages, and
`--clang-major`. `scripts/clang-warning-baseline.txt`'s `# clang-major:` line is a **mirror** the
checker cross-checks, not a second authority; the two must move in the same commit or the gate exits 2.

**The rule going forward is *upstream stable*, not *newest*.** A release candidate does not qualify;
neither does "whatever the distribution happens to package". Major granularity is not a compromise: it
is the granularity `apt` names these packages at (`clang-22`) and the granularity the baseline's own
invariant is stated at.

**The install mechanism follows the version; it never chooses it.** `scripts/setup-llvm-apt.sh` takes
the major as an argument, reads the suite codename from `/etc/os-release`, adds one suite with a
`signed-by=` keyring, and installs exactly `clang-N`, `lld-N`, `libclang-rt-N-dev`. It is deliberately
**not** `apt.llvm.org/llvm.sh`, which decides the version itself and installs more broadly — that
would move the authority out of this repository. It is **fail-closed**, unlike the optional ccache
install beside it: the pinned Clang *is* the job, so a partial install stops it rather than letting the
image's default `clang` stand in and trip the baseline guard one step later, reading as a project
problem.

**`-fsanitize=vptr` is now spelled out in the `sanitizers` job, and that is coverage PRESERVED, not
added.** Clang 21 removed `vptr` from the `-fsanitize=undefined` group, so the bare
`address,undefined` this job carried would have silently stopped checking bad downcasts and bad
vtables the moment the pin passed 20. Demonstrated, not inferred: the same bad-downcast program under
both majors — clang-20 reported it from `undefined` alone, clang-22 reported **nothing** until `vptr`
was named, and reported it again once it was. `vptr` requires RTTI, which this project never disables.

### The policy this ADR enacts
`ARCHITECTURE_REVIEW_GATE.md` listed "Build System change — CMake structure, JUCE version/pin,
dependency set" without saying whether a compiler counts, and this ADR's first draft recorded that
gap as an open tension against the AppleClang 15 → 21 precedent instead of closing it. It is closed
now, in the Policy, with the discriminator **who chooses the version**:

- a version **this repository pins** is gated (the Clang major; the C++ standard);
- a version the **runner image supplies** is not gated, because GitHub can change it with no commit
  here — a rule requiring review for that is one the repository cannot obey; it requires *detection
  and record* instead;
- **pinning or unpinning a label** for a toolchain that builds **shipped** artifacts is gated.

**AppleClang 15 → 21 is reconciled by the second rule, not exempted from the first.** No version this
repository pinned changed — AppleClang has never been pinned here — and the compiler that followed the
`macos-14` → `macos-latest` move was GitHub's choice. That move was handled as a CI change with a full
write-up in `CI_CD.md` §Build matrix, which is what the second rule asks for. Under the third rule the
*label* move would be gated today, because it handed a shipping toolchain to the image. Apple's
version numbers are not upstream LLVM's, and the Policy explicitly does not require the two platforms
to carry matching numbers.

**Not bot-updated, and that conclusion was re-tested against the new mechanism rather than inherited.**
Moving to apt.llvm.org does not create a supported automation path: Dependabot has no apt/deb ecosystem
and parses no workflow `env:` key, and while Renovate does have a `deb` datasource, pointing it at an
apt.llvm.org suite would still produce a PR that is **red by construction** — the baseline still names
the old major, so `check-clang-warnings.py` exits 2, and getting to green requires building with the new
compiler and regenerating the site counts, which *is* the review. Such a PR would also burn the full
pluginval gate per firing. The mechanism stays the guard plus a human trigger, recorded in
`DEPENDENCY_POLICY.md` §Update mechanisms.

**Revisit when upstream cuts a new stable major** — 23.1.0 final is the next trigger, and the check is
one page: `apt.llvm.org/llvm.sh`'s `CURRENT_LLVM_STABLE`. Not at a fixed cadence, and not on a release
candidate.

## Consequences
- **No shipped byte changes**, no DSP, latency, parameter or serialization change. Rules 2–3 of
  `DEPENDENCY_POLICY.md` (twin dump, Level-5 audition, compatibility re-verification) have nothing to
  act on, because no shipping toolchain moved.
- **The baseline did not churn — across three majors.** 18, 20 and 22 emit a `diff`-identical
  first-party warning set on this tree (7 entries / 14 sites; 52 diagnostic instances counting
  vendored). Only `# clang-major:` changed. The debt list is stable enough that the gate's value
  survives a compiler move intact.
- **A third-party apt source is now in the Linux Clang jobs**, and the trust surface is narrowed
  three ways rather than merely acknowledged: the key is fetched over HTTPS **and then pinned by
  fingerprint** (`6084F3CF814B57C1CF12EFD515CF4D18AF4F7421`, *Sylvestre Ledru — Debian LLVM
  packages*), so a rotated or substituted key fails the job with a specific message instead of being
  trusted silently; `signed-by=` scopes that key to this one suite, so it can validate nothing else on
  the machine; and the install is fail-closed. If apt.llvm.org is unreachable the two Clang jobs fail
  saying so. The three *shipping* build jobs never touch it.
- **Reproducibility is at major granularity, and the floating part is now a snapshot string** —
  stated rather than glossed, because it is genuinely weaker than the stock archive's
  release-versioned packages. apt.llvm.org publishes *branch snapshots*, never the tagged
  `llvmorg-*` build: noble-22 is `1:22.1.8~++20260714014902+ca7933e47d3a-…`, the 22.x head just after
  22.1.8, and the `~` makes it sort **below** a hypothetical `1:22.1.8-1`. Suites are rebuilt while
  their branch is open and freeze when it closes. **22.x is closed** (22.1.8 is upstream's newest tag;
  23 is the development branch), which the mirror corroborates — noble-22's index is 18 days old while
  noble-23's is 2 — so this suite is in its frozen state rather than merely expected to be. The guard
  covers the major; a patch-level diagnostic shift within 22 would surface as a gate failure, not a
  silent pass.
- **Install cost, per Clang job:** 15 packages, 155 MB, **17.8 s** measured on a 4-core box, plus one
  scoped `apt-get update`. For comparison, clang-20 from the stock archive was 14 packages / 113 MB /
  10.9 s, and clang-18 was preinstalled on the image.
- **One cold ccache lineage per Clang job, once.** Both keys interpolate the major, so this is
  automatic; nothing else was needed.
- **arm64 becomes possible.** apt.llvm.org publishes noble-22 for `amd64 arm64 s390x`, so a future
  `ubuntu-24.04-arm` Clang job could install it — which `clang-20` from the stock archive
  (amd64/i386 only) could not.
- **The `vptr` restoration is the one behavioural change in the pipeline**, and it restores a check
  rather than adding one. If it ever fires, it is reporting a real bad downcast.
- **A silent upstream default is in the detector, unchanged from the 20 evaluation:** Clang ≥ 20 emits
  distinct TBAA tags for incompatible pointers by default, which upstream says may silently change
  behaviour for code with strict-aliasing violations. `-fno-pointer-tbaa` is the escape hatch. Checked
  rather than assumed — see Evidence.
- **Clang 21 and 22 add dozens of new warning flags and escalate three diagnostics to
  error-by-default, and this tree trips none of them** — which is *why* the baseline is unchanged, not
  evidence that the compiler stood still. Per the release notes 21 adds 51 new flags and 22 adds 26,
  21 makes chained comparisons and comparison fold-expressions errors, and 22 makes
  `-Wincompatible-pointer-types` an error. Checked against the actual build log rather than assumed:
  zero occurrences of `-Wunnecessary-virtual-specifier` (new in `-Wextra`, fires on `virtual` members
  of a `final` class), `-Wcharacter-conversion`, `-Wexperimental-lifetime-safety`, or the new
  default-on `-Wgcc-install-dir-libstdcxx` — the last worth naming because it fires on images carrying
  several GCC toolchains, which the runner image does (12/13/14).
- **One forward-looking loss, and it lands in this project's own subject area:** Clang 22 **removes
  `-Wperf-constraint-implies-noexcept` from `-Wall`**. It cannot fire today, because nothing here is
  annotated `[[clang::nonblocking]]`/`nonallocating` — but that is precisely the annotation the
  RealtimeSanitizer note below contemplates. Whoever adopts those attributes must enable that warning
  explicitly, or the diagnostic that pairs with them will be silently absent. Recorded here so the two
  decisions stay attached to each other.
- **RealtimeSanitizer remains reachable and deliberately not taken.** `-fsanitize=realtime` with
  `[[clang::nonblocking]]` (Clang ≥ 20) would put `REALTIME_AUDIO_POLICY.md`'s central invariant under
  a tool instead of under review. Adopting it means annotating the `processBlock` path and is its own
  change under that policy, with its own ADR.
- **The standard library did not move with the compiler.** Both clang-20 and clang-22 select the same
  libstdc++ (`Selected GCC installation: …/13`), so no C++23 *library* surface changed here; the
  library follows the runner's GCC, not the pin.
- **The linker stays deliberately unpinned** in `CMakeLists.txt` (`check_linker_flag` probes it), and
  `scripts/setup-linux.sh` keeps installing the *unsuffixed* `lld`. The pairing is nonetheless
  version-matched in practice: `clang++-22 -fuse-ld=lld` resolves to `/usr/lib/llvm-22/bin/ld.lld`
  (LLD 22.1.8) because the driver searches its own program path first.
- **18 and 20 stay the compilers named by the historical records** (ADR-0027's libc++ proxy build, the
  ccache measurements, `HANDOVER.md`'s verification rows). Those are not rewritten.
- **Nothing is superseded.** ADR-0026's "toolchain contract" line concerns the *product* build; this
  ADR governs a CI validation compiler that contract never named.

## Related code
- `.github/workflows/build.yml` — the `env:` authority (`:78-80`) and its rationale block; both Clang
  jobs' installs, configures, sanitizer flags and ccache keys.
- `scripts/setup-llvm-apt.sh` — the install mechanism, and the reasoning for its shape.
- `scripts/clang-warning-baseline.txt` — `# clang-major: 22` + the 7 accepted entries.
- `scripts/check-clang-warnings.py` — `--clang-major` and the exit-2 mismatch guard, unchanged
  (including the two synthetic `18`s in its self-test, which are not the pin).
- `docs/policies/ARCHITECTURE_REVIEW_GATE.md` — the rule this ADR enacts.
- `docs/policies/DEPENDENCY_POLICY.md` — the Clang row, §Update mechanisms, and the compliance log.
- `docs/procedures/CI_CD.md` — §Cache lineages, §The Clang warning baseline, §Reproducing CI locally.

## Evidence + confidence
**Verified — measured on one tree, JUCE at the pinned commit `e18f7f5…`, the same three targets
`linux-clang` builds, with clang-20 kept as the control:**
- **`diff`-identical warning census, clang-20 vs clang-22**: the same 7 flags at the same counts, 52
  instances, first-party and vendored alike. `--write-baseline` at 22 changes exactly one line.
- The **guard fires** — a clang-22 log against the clang-20 baseline exits **2**, before the baseline
  was regenerated.
- **140-check DSP + 894-check state suites green** under the clang-22 Release build, and again under
  its ASan + UBSan + **vptr** build (`detect_leaks=0`, `halt_on_error=1`) linking
  `libclang-rt-22-dev`.
- **LTO + lld path exercised**: the `Anamorph_VST3` target — the only one carrying
  `juce_recommended_lto_flags` — links, and `check_linker_flag` reports
  `ANAMORPH_HAVE_LLD - Success` under clang-22.
- **`check-portability.py --compile-canary` with `clang++-22`**: the deduced form compiles and the
  explicit `SIMDRegister` form is still rejected, so the lint it guards is still live at this major.
- **The `vptr` regression is reproduced directly**: one bad-downcast program, `-fsanitize=undefined`,
  clang-20 reports "downcast of address … which does not point to an object of type 'B'", clang-22
  reports nothing, and `-fsanitize=undefined,vptr` restores it on 22. `vptr` is accepted for C, C++
  and at link, and is rejected only with `-fno-rtti`, which this project never sets.
- **The install mechanism was run**: `scripts/setup-llvm-apt.sh 22` installs clang/lld/libclang-rt
  22.1.8 from `llvm-toolchain-noble-22`, asserts the reported major, and is a no-op on re-run; it
  exits 2 on a missing or non-numeric argument.
- Upstream state confirmed against `github.com/llvm/llvm-project` releases (22.1.8 newest non-rc;
  23.1.0 at rc3) and `apt.llvm.org` (`CURRENT_LLVM_STABLE=22`; noble suites 17–23; per-suite `Release`
  metadata and package versions read directly).
- **The two apt.llvm.org authorities disagree, and the disagreement is resolved rather than ignored:**
  `llvm.sh` says `CURRENT_LLVM_STABLE=22`, while the site's prose still labels 21 stable / 22
  qualification / 23 development. Upstream's tags settle it — 22.1.8 is the newest non-rc release, so
  the prose is one cycle stale. Anyone re-checking this later will see that page and should not read it
  as contradicting this ADR.
- The signing key's fingerprint was read from the served key locally
  (`6084F3CF814B57C1CF12EFD515CF4D18AF4F7421`), and the script's assertion was tested in both
  directions: the real key passes, a deliberately wrong expected fingerprint fails with exit 1 naming
  expected and actual.

**Not verified here:** a patch-level diagnostic shift *within* major 22 (a future apt.llvm.org rebuild
of the 22.x branch). Accepted by design — the baseline records majors — and it would surface as a gate
failure rather than a silent pass.
