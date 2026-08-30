# ADR-0030 — Linux release toolchain: Clang ships, GCC verifies

**Status:** **Accepted** (Build System change — maintainer approval 2026-08-21).

**Follows from:** `ADR-0028`, which pinned the CI **Clang** toolchain to *upstream stable* and amended
`ARCHITECTURE_REVIEW_GATE.md` with the rule that *who chooses the version* decides whether a compiler
change is gated. This ADR applies that rule to the toolchain that builds the **shipped Linux
artifact**, where the answer had been "the runner image chooses".

## Context

Until this change the Linux artifact was built by whatever `g++` `ubuntu-latest` provided: the `linux`
job named no compiler at all. `ANAMORPH_GCC_VERSION` existed, but only `linux-lto-tests` consumed it,
for the warning baseline. The shipped bytes were therefore GitHub's choice — the position `ADR-0028`
rule 3 identifies as gated when the toolchain ships.

**GCC 16.2.0 was pinned for the shipping build first, as an intermediate state within this same
unmerged change set** (the `via 20` precedent in `DEPENDENCY_POLICY.md`). That step is not the
decision recorded here; it is retained in the change history because one of its findings — the
unguarded `CXXABI` family, below — outlived it and is now permanent.

## Problem

1. **Which toolchain should build the Linux artifact.** The other two platforms are decided by their
   vendors (MSVC, AppleClang). Linux is the only one this repository chooses, and it had not.
2. **GCC 16 is not obtainable as a released package.** Noble stops at `g++-14`;
   `ubuntu-toolchain-r/test` carries only a trunk snapshot (`16-20260315`) and Ubuntu 26.04 the same
   class (`16-20260322`), both predating the 16.1 release; stable 16.2.0 exists in `apt` only for an
   unreleased Ubuntu series. Shipping from GCC 16 therefore required a container pinned by digest.
3. **A second, near-identical Clang job already existed.** `linux-clang` built the same targets with
   the same compiler in a second build tree, purely because `linux` was GCC's.

## Options

- **A. Keep GCC as the shipping toolchain, pinned by digest.** Rejected. It buys a compiler the
  project cannot obtain from a package source, keeps two full Release builds (GCC's and Clang's), and
  leaves the larger warning set — Clang's — off the artifact that ships.
- **B. Ship Clang, drop GCC entirely.** Rejected. GCC sees two diagnostic classes Clang structurally
  does not (`-Wshadow` outside a constructor; `-Wmisleading-indentation`), and losing a second major
  toolchain loses the compatibility signal that catches what one compiler accepts and another does not.
- **C. Ship Clang; keep GCC as a compatibility build.** **Chosen.**

## Decision

### 1. Clang builds the Linux artifact

`linux` builds the shipped VST3 and Standalone with the pinned `clang-<n>` from apt.llvm.org, linked
by the matching `lld-<n>` that `scripts/setup-llvm-apt.sh` installs beside it. That pairing is a
correctness requirement, not a preference: under `-flto` GNU ld scans a static archive once and does
not rescan, so symbols that only become referenced after cross-TU inlining become undefined
references. `CMakeLists.txt` already probed for lld and already scoped that block to Clang — it now
governs the shipped link rather than a validation job's.

`ANAMORPH_CLANG_VERSION` stays **22**: LLVM 22.1.8 was the current stable line at this decision and 23
unreleased (apt.llvm.org's `-23` repository carried `23.1.0~++` pre-release snapshots), so the existing
pin was already *latest stable*, and the major-line pin takes the newest patch from that line
automatically. **Still 22 as of [ADR-0033](ADR-0033-clang-toolchain-pin-23.md)** (2026-08-30): 23.1.0
shipped 2026-08-25, but no apt.llvm.org suite has been rebuilt from its tag, so the `-23` repository
still carries exactly the pre-release class this sentence names. The rest of this ADR — Clang ships the
Linux artifact, GCC stays as the compatibility compiler, `linux-clang` folds into `linux` — is
untouched either way.

`merge-check` uses the same toolchain. It is the only build on the same-repo PR path; on a different
compiler it would clear PRs against a toolchain the project does not ship.

### 2. `linux-clang` is folded into `linux`

Once `linux` is a Clang Release build, a second one is duplicated infrastructure. The two steps that
were genuinely `linux-clang`'s — the portability compile canary and the first-party warning gate —
moved into `linux`. **No check was lost.** The gate runs *after* the artifact upload, for the reason
the ABI gate does: a validation finding should not withhold the beta artifact that the behavioural
gates already passed, and the job still fails.

### 3. GCC 16 remains, as the compatibility compiler, with a deliberately weak pin

`linux-lto-tests` keeps the LTO suites and the GCC-only warning gate, on `gcc:16` — **the floating
major tag, not a digest**. A compatibility checker wants the newest stable 16.x automatically; a
shipping toolchain must not. Nothing downstream needs the patch: the baseline records the **major**,
and the job asserts the major (`g++ -dumpversion`), so 16.2 → 16.3 is silent and a 17 is loud.

An image rather than `apt` because, per Problem 2, no package source ships a released GCC 16 — this
is the concrete availability reason that justifies a container here and nowhere else.

## Consequences

**The `CXXABI` family stays gated, at GCC 13's value.** The GCC 16 evaluation surfaced that
`check-linux-abi.py` gated `GLIBC` and `GLIBCXX` only, while that artifact's exception path pulled
`__cxa_call_terminate@CXXABI_1.3.15` (first shipped in GCC 14) — a silent floor rise in an undeclared
family. `CXXABI` is now declared at **1.3.14**, GCC 13's value, so the three families describe one
runtime between them. The Clang-built artifact needs only `CXXABI_1.3.9`, so **the supported floor is
unchanged at Ubuntu 23.10 / Debian 13** — the Ubuntu 24.04 floor the GCC 16 step would have imposed
does not apply. A future move to a compiler emitting `1.3.15` now fails the gate rather than passing.

**A Clang bump is no longer exempt from `DEPENDENCY_POLICY` rules 2–3.** `ADR-0028` reasoned that the
Clang pin touched no shipped bytes. That was true then and is recorded as it stood; it is false now,
and the next bump of `ANAMORPH_CLANG_VERSION` must run the twin dump and the compatibility
re-verification.

**ccache lineages are re-keyed.** `linux` and `merge-check` move from `ccache-ubuntu-gcc-release-*`
to `ccache-ubuntu-clang<n>-release-*`. `CCACHE_COMPILERCHECK: content` already made a different
compiler miss rather than collide, so this is about eviction, not correctness: one lineage holding
two compilers' objects in a 500M budget has them evict each other. The first run after this change
is cold by construction.

**No speed optimization is given up.** ccache, the cache strategy, Ninja and the per-job build trees
are unchanged; the pipeline loses one full Release build (`linux-clang`, measured 9m38s cold /
4m13s warm) and gains nothing.

## Related code

- `.github/workflows/build.yml` — `env.ANAMORPH_CLANG_VERSION` / `env.ANAMORPH_GCC_VERSION`, the
  `linux`, `merge-check` and `linux-lto-tests` jobs.
- `CMakeLists.txt` — the Clang/lld LTO block, which now governs the shipped link.
- `scripts/check-linux-abi.py` — `CXXABI_FLOOR`, `FLOORS`, `floor_summary()`.
- `scripts/setup-llvm-apt.sh` — installs `clang-<n>`, `lld-<n>`, `libclang-rt-<n>-dev`.

## Evidence + confidence

**Verified.** Measured on 2026-08-21, `clang-22` / `lld` 22.1.8 and `gcc:16` (16.2.0):

- **The shipped configuration builds and links.** VST3 + Standalone + both test targets, Release,
  LTO; `ANAMORPH_HAVE_LLD` probe succeeds and the artifact records `Linker: Ubuntu LLD 22.1.8`.
- **Suites pass on the Clang Release build**: 162 DSP checks / 0 failures, 911 state checks /
  0 failures.
- **ABI, Clang vs GCC, same tree.** Clang: `GLIBC_2.38, GLIBCXX_3.4.31, CXXABI_1.3.9` — within the
  declared floor. The GCC 16 artifact (`CXXABI_1.3.15`) now **fails** that floor, confirming the gate
  discriminates rather than merely reporting.
- **The Clang warning gate is unchanged by the move.** Run against the *release* configuration
  (Standalone **ON**, unlike the old `linux-clang`): 14 accepted sites in 7 baseline entries, no new
  first-party warnings — so `clang-warning-baseline.txt` needed no edit.
- **GCC 16 still clean** under the floating tag: first-party gated-warning set identical to the
  baseline's three entries, one distinct site each.
- **`check-linux-abi.py --self-test`**: 17 cases, including the over-`CXXABI`-floor case.

**Not verified here:** the end-to-end CI run — `container:`-hosted GCC, pluginval under xvfb, artifact
staging and upload are exercised only by CI itself.
