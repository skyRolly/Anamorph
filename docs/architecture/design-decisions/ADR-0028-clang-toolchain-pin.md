# ADR-0028 — CI Clang toolchain pin 18 → 20, held as a major-only pin

**Status:** **Proposed** (Build System change by the ADR-0027 reading — `ARCHITECTURE_REVIEW_GATE.md`;
awaiting Architecture Review. A green build does not clear this.)

## Context
`ANAMORPH_CLANG_VERSION` in `.github/workflows/build.yml` is the single authority for the Clang major
that two CI jobs use: `linux-clang` (the first-party warning gate) and `sanitizers` (ASan + UBSan, and
the reason the pin also fixes `libclang-rt-<n>-dev`). The pin was introduced because
`scripts/clang-warning-baseline.txt` records per-`(path, flag)` site counts, and which diagnostics
Clang emits is a property of the major — so an unpinned `clang` made a runner-image bump
indistinguishable from a source regression.

**Neither job uploads an artifact.** The shipped Linux bytes are GCC's, Windows' are MSVC's, macOS's
are AppleClang's. This pin therefore governs *detectors*, not the product.

The value was 18 and had been since the pin landed. As of 2026-08-17 Clang 18.1.0 is 895 days old,
the 18.x branch has had no release since 18.1.8 (2024-06-20), four majors have shipped since, and
23.1.0 is at rc3. Meanwhile the compiler this job exists to *preempt* — AppleClang on `macos-latest`,
now 21 — is three majors ahead of it, which is the gap that let four
`-Wimplicit-int-float-conversion` sites reach the tree and surface only from the macOS runner.

## Problem
Decide the Clang major, and the granularity at which it is pinned, on evidence rather than on
recency — without changing the C++ language standard (a separate contract, ADR-0027), without
weakening the baseline guard, and without adding a third-party apt source to a repository that
verifies no download checksums.

## Options
1. **Keep 18.** Zero churn, and the "18 stops being installable" fear is unfounded: 26.04
   (`resolute`) publishes `llvm-toolchain-18` (1:18.1.8-20ubuntu8) on every architecture, so the pin
   would survive the eventual `ubuntu-latest` move. Rejected on currency, not on breakage: an
   end-of-life compiler that no longer receives upstream fixes is a poor reference point for a gate
   whose whole output is that compiler's diagnostics, and it leaves the gap to AppleClang 21 at three
   majors.
2. **19.** Installable from stock noble archives (1:19.1.1). Strictly dominated by 20 — same
   acquisition route, one major less C++23, and not preinstalled on the 26.04 image.
3. **20 — chosen.** `1:20.1.2-0ubuntu1~24.04.3` from `noble-updates`/`noble-security` universe:
   stock archives, no new apt source. Also the newest major reachable that way.
4. **21 or 22.** `llvm-toolchain-21`/`-22` have **no noble publication at all** (verified: no
   `noble`, `noble-updates`, `noble-security` or `noble-backports` row) — they exist only for 26.04.
   Reaching them today means adding `apt.llvm.org`: a third-party apt source and key, for a compiler
   this project does not ship with. Rejected. They would also buy nothing on the language axis —
   Clang 21 and 22 shipped release notes with **empty** "C++23 Feature Support" sections.
5. **Unpinned (`clang`/`clang++`).** Rejected — it is the exact defect the pin was created to
   remove, and it is worse now than when the pin landed: the unsuffixed alias is 18 on the current
   image and 21 on the 26.04 toolset, so the image move would be a silent two-major jump.
6. **A full version pin** (`clang-20=1:20.1.2-0ubuntu1~24.04.3`). Rejected on two independent
   grounds. It converts a near-zero risk into a certainty — the exact epoch-revision is
   release-specific (noble 1:20.1.2, resolute 1:20.1.8), so the pin breaks as an apt resolution error
   the day the image moves — and it is unenforceable by the guard that gives the pin its value:
   `check-clang-warnings.py` compares majors only, so a patch pin is a promise nothing checks.

## Decision
**`ANAMORPH_CLANG_VERSION: 20`, pinned by MAJOR only, in the one place it already lives.** Everything
else derives from it by interpolation — the two jobs' `apt` installs, `clang-<n>`/`clang++-<n>`, both
ccache lineages, and `--clang-major`. `scripts/clang-warning-baseline.txt`'s `# clang-major:` line is a
**mirror** that the checker cross-checks, not a second authority; the two must move in the same commit
or the gate exits 2.

Major granularity is not a compromise: it is the granularity `apt` names these packages at
(`clang-20`) and the granularity the baseline's own invariant is stated at.

**Not automated, and that is a capability finding, not a preference.** Dependabot parses no workflow
`env:` key in any of its 33 ecosystems. Renovate could match the value with a `custom.regex` manager,
but the resulting PR would be **red by construction** — the baseline still names the old major, so
`check-clang-warnings.py` exits 2 — and it could not be made green by any bot, because green requires
building with the new compiler and regenerating the site counts, which *is* the review. Such a PR would
also burn the full pluginval gate per firing. The mechanism is instead the guard plus a human trigger,
recorded in `DEPENDENCY_POLICY.md` §Update mechanisms. Note that "`apt-get install clang-20` began
failing" is self-announcing: it fails loudly at install time, on every push, before any build.

**Revisit when** `ubuntu-latest` moves to 26.04 (which preinstalls 20/21/22 and makes 21/22 free), or
at a release-hardening pass — not because a new Clang exists.

## Consequences
- **No shipped byte changes**, no DSP, latency, parameter or serialization change; rules 2–3 of
  `DEPENDENCY_POLICY.md` (twin dump, Level-5 audition, compatibility re-verification) have nothing to
  act on, because no shipping toolchain moved.
- **The baseline did not churn.** Only `# clang-major:` changed; all 7 entries and 14 sites are
  byte-identical, so the diff that "is the entire review" is one line.
- **Install cost, per Clang job, forever:** `clang-18` is preinstalled on the image (`apt-get install`
  answers *already the newest version*), while `clang-20` + `lld-20` + `libclang-rt-20-dev` is a real
  14-package, 113 MB install — 10.9 s measured on a 4-core box.
- **One cold ccache lineage per Clang job, once.** Both jobs now key on the major (`sanitizers` did
  not before this change and would have restored a lineage of objects the new compiler can never hit).
- **A silent upstream change is now in the detector:** Clang 20 enables distinct TBAA tags for
  incompatible pointers by default, which upstream says may silently change behaviour for code with
  strict-aliasing violations. Checked rather than assumed — see Evidence. `-fno-pointer-tbaa` is the
  escape hatch if it ever matters.
- **`clang-20` is amd64/i386-only on 24.04**, so a future `ubuntu-24.04-arm` job could not install it.
  On 26.04 every major 18–22 is available on all architectures.
- **The linker stays deliberately unpinned** (`CMakeLists.txt`, `check_linker_flag`), and
  `scripts/setup-linux.sh` keeps installing the *unsuffixed* `lld`. Incidentally the pairing improves:
  `clang++-20 -fuse-ld=lld` resolves to `/usr/lib/llvm-20/bin/ld.lld`, its own major, because the
  driver searches its own program path first.
- 18.x stays the compiler named by the **historical** records (ADR-0027's libc++ proxy build, the
  ccache measurements, the `HANDOVER.md` verification rows). Those are not rewritten.

## Related code
- `.github/workflows/build.yml` — the `env:` authority and its rationale block; both Clang jobs'
  installs, configures and ccache keys.
- `scripts/clang-warning-baseline.txt` — `# clang-major: 20` + the 7 accepted entries.
- `scripts/check-clang-warnings.py` — `--clang-major`, and the exit-2 mismatch guard in both
  directions. Unchanged by this ADR (including the two synthetic `18`s in its self-test, which are
  not the pin).
- `docs/procedures/CI_CD.md` §Cache lineages, §The Clang warning baseline, §Reproducing CI locally.
- `docs/policies/DEPENDENCY_POLICY.md` — the Clang row and §Update mechanisms.

## Evidence + confidence
**Verified (measured, one tree, JUCE at the pinned commit `e18f7f5…`, the same three targets
`linux-clang` builds):**
- A **clang-18 control** build regenerates the committed baseline **byte-identically** (7 entries /
  14 sites), establishing that the measurement harness reproduces CI before anything is attributed to
  the compiler.
- **clang-20 emits a `diff`-identical 52-instance warning-flag census** to clang-18, first-party and
  vendored alike; `--write-baseline` at 20 changes exactly one line.
- 140-check DSP + 894-check state suites green under the clang-20 Release build, **and** again under
  its ASan+UBSan build (`detect_leaks=0`, `halt_on_error=1`) linking `libclang-rt-20-dev`.
- `check-portability.py --compile-canary` with `clang++-20`: the deduced form compiles and the
  explicit `SIMDRegister` form is still rejected, so the lint it guards is still live at this major.
- The guard was **observed to fire in both directions** (exit 2 on a clang-20 log against the
  clang-18 baseline, and on a clang-18 log against the regenerated one) — the reason an automated
  bump cannot slip through.
- Package availability confirmed against `packages.ubuntu.com` and Canonical's `madison` for noble
  and resolute; runner contents against `actions/runner-images` manifests; upstream dates, C++23
  status and the ABI/TBAA notes against `releases.llvm.org` and the per-release Clang release notes.

**Not verified here:** a patch-level diagnostic shift within major 20 (20.1.2 → 20.1.8 across a future
image move). The baseline records majors by design, so this is accepted rather than covered; it would
surface as a gate failure, not as a silent pass.
