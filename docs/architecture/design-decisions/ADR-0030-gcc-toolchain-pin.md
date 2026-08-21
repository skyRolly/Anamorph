# ADR-0030 — Linux GCC toolchain pinned to upstream stable 16.2.0, supplied as a digest-pinned image

**Status:** **Accepted** (Build System change — maintainer approval 2026-08-21). The libstdc++
floor consequence in §Consequences is flagged for `RELEASE_COMPATIBILITY_CHECKLIST.md` before
0.9.4 ships.

**Follows from:** `ADR-0028`, which pinned the CI **Clang** toolchain to *upstream stable* and
amended `ARCHITECTURE_REVIEW_GATE.md` with the rule that *who chooses the version* decides whether a
compiler change is gated. This ADR applies the same rule to GCC, where the answer had been "the
runner image chooses" for the compiler that builds the **shipped Linux artifact**.

## Context

Until this change `ANAMORPH_GCC_VERSION` was **13**, and that number was not chosen so much as
inherited: 13.3 is `ubuntu-24.04`'s default `g++`. Only one job named it (`linux-lto-tests`, for the
warning gate); the job that builds the artifact users install — `linux` — named no compiler at all
and simply took whatever `ubuntu-latest` provided. The shipped bytes were therefore GitHub's choice,
which is precisely the position `ADR-0028` rule 3 identifies as gated when the toolchain ships.

GCC 16.1 released 2026-04-30 and **16.2 on 2026-08-07**; 16.2.0 is the latest stable in the series.

## Problem

1. **There is no `apt` package for stable GCC 16 on any supported base.** Noble's archive stops at
   `g++-14` (14.2.0). `ubuntu-toolchain-r/test` packages stable **15.2.0** for noble, but its only
   `g++-16` is a dated trunk snapshot — `16-20260315`, identical across bionic/focal/jammy/noble.
   That is the "newest, not stable" that `ADR-0028` already rejected for Clang 23, so the mechanism
   that serves Clang cannot serve GCC here.
2. **The warning baseline describes one compiler.** `scripts/gcc-warning-baseline.txt` records
   `# gcc-major:` and the gate exits 2 on a mismatch, so the pin cannot move without re-baselining.
3. **The ABI floor is a user-facing compatibility claim**, asserted per push against the stripped
   bytes. A compiler change is exactly the kind of change that can move it.

## Options

- **A. Stay on GCC 13.** Rejected. It leaves the shipped compiler as the runner image's choice —
  the gap this ADR exists to close — and holds the diagnostic gate three majors back for a reason
  that is a fact about Ubuntu, not about this project (`ADR-0028`'s own argument).
- **B. Install `g++-16` from `ubuntu-toolchain-r/test`.** Rejected on the *stable* rule: the only
  GCC 16 there is a pre-release trunk snapshot, and it is older than 16.1 besides.
- **C. Build GCC 16.2 from source in CI.** Rejected on cost and fragility: roughly an hour per cold
  run on a 4-core runner for a toolchain upstream already publishes, plus a bespoke cache.
- **D. The official upstream toolchain image, pinned by digest.** **Chosen.**

## Decision

`gcc:16.2.0@sha256:8d466d9001805b81ecc280645889c368ae28e75724e7ecba03a0336deda0faac` is the Linux
GCC toolchain, as a job `container:` on **all three** Linux GCC jobs — `linux` (ships the artifact),
`merge-check` (the only build on the same-repo PR path) and `linux-lto-tests` (the warning +
LTO gate).

1. **A digest, not a tag.** A tag can be repointed; a digest cannot. "The compiler moved underneath
   us" stops being a monitored failure mode and becomes an impossible one.
2. **`runs-on` is no longer pinned on these jobs.** The host stopped supplying the compiler, so it
   stopped being the unpinned variable that pinning the host was meant to catch.
3. **`merge-check` moves too.** It is the only build on the same-repo PR path; on the host compiler
   it would clear PRs against a compiler the project does not ship, and the first build with the
   shipped one would be the merge. It also shares `linux`'s ccache lineage, whose stated invariant
   is *same compiler*.
4. **The version numbers stay in `env` and are asserted, not trusted.** `jobs.<id>.container` cannot
   read `env`, so the digest is spelled per job. Each job checks `g++ -dumpfullversion` against
   `ANAMORPH_GCC_TOOLCHAIN` before building, so a repointed image without the env block following
   fails in the first minute with both numbers in the log.

## Consequences

**The libstdc++ floor gained a third family, and this is the part that needs review.** `GLIBC` and
`GLIBCXX` did **not** move: the GCC 16.2 artifact asks for `GLIBC_2.38` and `GLIBCXX_3.4.31`,
identical to the GCC 13 build — a newer compiler does not raise a symbol-versioned floor by itself,
because versions bind to the symbols code actually uses. But the GCC 16 exception path pulls
`__cxa_call_terminate@CXXABI_1.3.15`, which libstdc++ first shipped in **GCC 14**.

`check-linux-abi.py` gated `GLIBC` and `GLIBCXX` only, so it would have reported a GCC 13 floor and
passed an artifact that needs a GCC 14 runtime — a silent floor rise in an undeclared family, which
is the one failure mode that gate exists to prevent. `CXXABI_1.3.15` is therefore declared and gated
alongside the other two, with self-test cases for the over-floor and missing-family verdicts.

In practice the libstdc++ requirement moves from **Ubuntu 23.10** (EOL July 2024) to **Ubuntu 24.04+
/ Debian 13+**. Both ship a GCC 14 libstdc++ (`libstdc++.so.6.0.33`, `CXXABI_1.3.15` present —
verified against a stock `debian:trixie-slim`), and the `GLIBC` half of the floor is unchanged, so no
currently-supported distribution is dropped.

**The warning baseline's content did not change.** Same three `(count, flag, path)` entries; only
`# gcc-major:` moved 13 → 16.

**ccache is preserved.** The setup action's `sudo` calls became root-aware; without that the
container (root, no `sudo`) would have taken the graceful-degradation branch and built with no cache.

## Related code

- `.github/workflows/build.yml` — `env.ANAMORPH_GCC_VERSION` / `env.ANAMORPH_GCC_TOOLCHAIN`, the
  three `container:` blocks and their assertion steps.
- `scripts/check-linux-abi.py` — `CXXABI_FLOOR`, `FLOORS`, `floor_summary()`.
- `scripts/gcc-warning-baseline.txt` — `# gcc-major: 16`.
- `.github/actions/setup-linux-build/action.yml` — root-aware `$SUDO`.

## Evidence + confidence

**Verified.** Measured on 2026-08-21 against `gcc:16.2.0` (`g++ (GCC) 16.2.0`):

- **The plugin builds clean.** VST3 + Standalone configured and linked with LTO, no errors.
- **ABI, GCC 16.2 vs GCC 13, same tree.** VST3: `GLIBC_2.38, GLIBCXX_3.4.31` under both;
  `CXXABI_1.3.9` → `CXXABI_1.3.15`. `ldd` resolves every symbol of the GCC 16 artifact on Ubuntu
  24.04. `CXXABI_1.3.15` ← GCC 14.1.0 per the libstdc++ ABI table.
- **The warning gate passes on the real GCC 16 log**: 3 accepted sites in 3 baseline entries, 6
  gated-flag diagnostics in vendored paths correctly excluded. Re-running it with `--gcc-major 13`
  exits 2, so the mismatch guard is live.
- **`check-linux-abi.py --self-test`**: 17 cases, including the new
  "over the CXXABI floor is a breach even at the GLIBCXX floor" case, which returned `([], [])` —
  clean — before the family was declared.
- **Availability**: GNU FTP carries `gcc-16.1.0` and `gcc-16.2.0`; `gcc.gnu.org/releases.html` dates
  them 2026-04-30 and 2026-08-07.

**Not verified here:** the end-to-end CI run. The container's effect on pluginval-under-xvfb,
artifact staging and upload is exercised only by CI itself.
