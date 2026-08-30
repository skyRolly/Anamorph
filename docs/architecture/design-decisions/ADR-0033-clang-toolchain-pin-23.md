# ADR-0033 — CI/release Clang toolchain: the pin moves to upstream stable 23

**Status:** **Accepted** (Build System change — `ARCHITECTURE_REVIEW_GATE.md` rule 1; maintainer-directed
toolchain update 2026-08-30, firing [ADR-0028](ADR-0028-clang-toolchain-pin.md)'s own revisit trigger.)

**Supersedes the VALUE of ADR-0028 only** — `ANAMORPH_CLANG_VERSION: 22` → **23**. ADR-0028's rule
(*upstream stable, not newest*; by major; the install mechanism follows the version and never chooses
it), its amendment to `ARCHITECTURE_REVIEW_GATE.md`, and its `-fsanitize=vptr` restoration are all
unchanged and are not re-argued here. **Supersedes the value clause of
[ADR-0030](ADR-0030-linux-release-toolchain.md)** ("`ANAMORPH_CLANG_VERSION` stays 22") for the same
reason; ADR-0030's decision — Clang builds the shipped Linux artifact, GCC stays as the compatibility
compiler — stands.

## Context

ADR-0028 closed with a revisit trigger stated in its own words: *"Revisit when upstream cuts a new
stable major — 23.1.0 final is the next trigger."* Its option 5 rejected 23 as **not stable**, because
at the time 23.1.0 was at rc3 (2026-08-12) and no final existed.

That trigger has fired. **LLVM 23.1.0 was released on 2026-08-25.**

Since ADR-0028 was accepted the **scope of the pin changed**: ADR-0030 moved the Linux release build
onto this toolchain, so the pinned Clang now compiles and links the shipped Linux VST3 and Standalone.
`DEPENDENCY_POLICY.md`'s compliance log had already recorded the consequence — *"the next bump of this
pin does touch shipped bytes and rules 2–3 apply to it"*. This ADR is the first bump under that rule,
so it carries a twin dump and a compatibility re-verification that ADR-0028 was exempt from.

## Problem

Move the pin to the new upstream stable major without weakening the baseline guard, without changing
the shipped engine output, and without letting a **lagging proxy** decide either way — because the
one-page check ADR-0028 named now disagrees with the fact it was chosen to report.

## The two apt.llvm.org proxies both lag the release, and that is resolved rather than ignored

ADR-0028 named `apt.llvm.org/llvm.sh`'s `CURRENT_LLVM_STABLE` as the trigger check. Read on
2026-08-30, five days after 23.1.0 shipped:

| Source | Reads | Verdict |
|---|---|---|
| `llvm.org` front page | *"Latest LLVM Release!  25 August 2026: LLVM 23.1.0 is now available"*; release-mail list headed `23.1.0: Aug 2026` | **23 released** |
| `releases.llvm.org/23.1.0/docs/ReleaseNotes.html` | HTTP **200** (per-release documentation is published for finals, not for candidates); `releases.llvm.org/24.1.0/…` is 404 | **23 released, and newest** |
| `releases.llvm.org` download table | row `23.1.0`, with the full llvm/clang/lld/libc++/flang source and doc set | **23 released** |
| `apt.llvm.org/llvm.sh` | `CURRENT_LLVM_STABLE=22` | **lags** |
| `apt.llvm.org` home page prose | still calls 23 the development branch; newest branch news is *"Jan 10th 2026 — Snapshot becomes 23, branch 22 created"* | **lags** |
| `apt.llvm.org` suite list | `llvm-toolchain-noble-{21,22,23}`; **no** noble-24 yet | **lags** |

**Upstream's own release announcement and published per-release documentation are the authority.**
This is the same resolution ADR-0028 reached when `llvm.sh` and the site prose disagreed with each
other — it settled the question on upstream's tags, not on apt.llvm.org's bookkeeping. The difference
now is that *both* apt.llvm.org signals lag, so the cheap proxy alone would have answered "not yet"
for a release that had already happened. `DEPENDENCY_POLICY.md` §Update mechanisms is corrected to name
upstream's release page as the trigger and to keep `CURRENT_LLVM_STABLE` as corroboration only.

**This is not a re-reading of ADR-0028's rule.** The rule is *upstream stable*, and 23.1.0 is a final
release, not a candidate; option 5 rejected 23 on a fact that has since changed, not on a principle.

## Options

1. **Stay on 22.** Rejected: 22.x is closed at 22.1.8 (2026-06-16), and holding the pin means the
   warning gate, the sanitizer host **and the shipped Linux compiler** sit a major behind upstream for
   no reason ADR-0028's rule supports. It would also require re-writing ADR-0028's own revisit trigger.
2. **Wait for `CURRENT_LLVM_STABLE` to catch up.** Rejected as letting a proxy outrank the fact it
   proxies. It is kept as corroboration, and its lag is recorded above so the next reader does not
   mistake it for a contradiction.
3. **Pin an exact version.** Rejected for the three reasons ADR-0028 gives, all still true — the
   apt.llvm.org pool retains only the current `.deb` per architecture, the string is
   distribution-specific, and `check-clang-warnings.py` compares majors, so a patch pin is a promise
   nothing checks.
4. **23, by major, from apt.llvm.org — chosen.** Same mechanism, same authority, same guard.

## Decision

**`ANAMORPH_CLANG_VERSION: 23`**, still by MAJOR only, still in the one place it lives
(`.github/workflows/build.yml`), still installed by `scripts/setup-llvm-apt.sh`. Every consumer derives
from that value by interpolation — the `linux`, `merge-check`, `sanitizers`, `realtime` and `fuzz`
installs, `clang-<n>`/`clang++-<n>`, all five ccache lineages, and `--clang-major`.
`scripts/clang-warning-baseline.txt`'s `# clang-major:` line moves in the **same change**, which is
what the checker's exit 2 exists to force.

**No mechanism changed.** No new apt source shape, no new package set (`clang-N`, `lld-N`,
`libclang-rt-N-dev`), no change to the signing-key fingerprint assertion, and no change to the
fail-closed behaviour.

## Evidence + confidence

**Verified.** Measured 2026-08-30 on one tree at `7660d87`, JUCE 9.0.1 at the pinned commit, x86-64
Linux, `clang-23` = `Ubuntu clang version 23.1.0 (++20260818083557+55feb0a3b6b7…)` from
`llvm-toolchain-noble-23`, control `clang-22` = `22.1.8 (++20260714014902+ca7933e47d3a…)` from
`llvm-toolchain-noble-22`, both installed by `scripts/setup-llvm-apt.sh` itself:

- **THE SHIPPED ENGINE OUTPUT DOES NOT MOVE — `DEPENDENCY_POLICY.md` rule 2's bit-identity gate,
  discharged with the committed instrument.** `tests/dsp_dump.cpp` (`-DANAMORPH_BUILD_DSPDUMP=ON`),
  built from the same source against the same JUCE checkout with otherwise identical flags and
  differing only in the compiler: **32/32 scenarios identical**, FNV-1a hash over every output byte
  *and* the reported latency, `diff`-clean. Both binaries' `--self-check` passes ("32 scenarios, all
  repeatable and all distinct"), so the instrument is discriminating rather than agreeing vacuously.
- **The first-party warning census is `diff`-identical, as it was across 18/20/22.** Regenerated from
  a clang-23 Release build of the CI target set: **9 entries / 17 sites**, byte-identical to the
  committed baseline apart from `# clang-major:`. `check-clang-warnings.py --clang-major 23` against
  the committed baseline reports no new first-party warnings.
- **Both suites green on the Clang 23 Release build**: **226 DSP checks / 0 failures**, **920 state
  checks / 0 failures**. And on its **ASan + UBSan + `vptr`** build with `libclang-rt-23-dev`, the
  `sanitizers` job's exact flag set and `halt_on_error=1`: **224 / 0** and **920 / 0** — 224 rather
  than 226 because Test 38's malloc half is by design absent under ASan, which the test announces.
- **The shipped configuration builds and links.** VST3 + Standalone + both test targets, Release, LTO;
  `ANAMORPH_HAVE_LLD` probe succeeds. The version-matched pairing survives the move:
  `clang++-23 -fuse-ld=lld` resolves to `/usr/lib/llvm-23/bin/ld.lld` (measured with
  `-print-prog-name`), the same packaging behaviour `CMakeLists.txt` records for `clang++-22`.
- **`DEPENDENCY_POLICY.md` rule 3, re-verified rather than assumed.** *Latency reporting:* the twin
  dump's latency column is part of the identical comparison above — 32/32, unchanged. *Session
  reload:* the 920-check state suite passes on both the plain and the sanitized clang-23 builds.
- **The ABI floor is unmoved.** `check-linux-abi.py` on the clang-23 VST3 and Standalone:
  `CXXABI_1.3.9, GCC_4.0.0, GLIBC_2.38, GLIBCXX_3.4.31` — the same symbol versions ADR-0030 measured
  at clang-22, and within the declared floor.
- **The realtime leaf-layer gate is still live in both directions under 23.** `-Werror=function-effects`
  with `-Werror=unknown-warning-option` on `tests/realtime_effects.cpp`: the clean compile passes, and
  the `-DANAMORPH_EFFECTS_CANARY` compile fails with a `-Wfunction-effects` attribution. The
  RealtimeSanitizer runtime the `realtime` job needs is present in the 23 install
  (`libclang_rt.rtsan-x86_64.a`), as it is in 22.
- **The UBSan ignorelist still applies cleanly.** Clang 23 introduces version 4 of the Special Case
  List format, which warns on deprecated `./`-prefixed matches; `scripts/ubsan-ignorelist.txt` uses
  none, and the sanitized build emits no special-case-list diagnostic.
- **Release-note review, for the classes that could reach this tree.** The **ABI changes** in 23.1.0
  are `_BitInt` bitfields > 255 bits on MSVC targets, `__regcall` struct passing, an Itanium mangling
  fix for lambdas in instantiated NSDMIs, AArch64 SVE builtin manglings under the Microsoft ABI, and
  the C++20 coroutine resume/destroy calling convention (an ABI break only on i686, MIPS O32,
  PowerPC64 ELFv1 and Lanai). None reaches this pipeline: every job compiles its whole tree, JUCE
  included, from source with one compiler, so there is no mixed-major link anywhere. **Removed
  compiler flags: none.** Two changes were checked against this tree rather than assumed — Clang 23
  *"more aggressively optimizes away stores to objects after they are dead"* (`-fno-lifetime-dse` is
  the escape hatch), which the bit-identical twin dump covers for the engine; and UBSan gaining
  null/alignment/array-bounds checks on aggregate copies, which is new coverage that the sanitized
  suites run clean under.

**Not verified here, and named rather than implied:**

- **The rule-2 Level-5 audition is the human step and has not been performed.** The twin dump leaves
  it nothing to discriminate in the *engine* — the output bytes are identical — but it is the policy's
  step, and it covers the editor path the dump does not reach.
- **macOS and Windows are untouched.** Their compilers are runner-supplied (`ARCHITECTURE_REVIEW_GATE`
  rule 2) and no version this repository pins changed for them.
- **Install cost is not quoted.** ADR-0028's 15 packages / 155 MB / 17.8 s figure was measured on a
  clean 4-core runner; the local measurement here shared a dependency set with the control install and
  is not comparable, so no number is published rather than a misleading one.

## Consequences

- **No shipped byte changes** on any platform: the engine twin dump is identical and the Linux ABI
  floor is unmoved. No DSP, latency, parameter or serialization change, so nothing in
  `CHANGELOG.md` — `CHANGELOG_POLICY.md` rule 3 is *user-visible changes only*, and this is not one.
- **Reproducibility is weaker at 23 than it was at 22, and that is the honest trade.** apt.llvm.org
  publishes branch snapshots, never the tagged `llvmorg-*` build. The 22 pin rested on a **closed**
  branch, i.e. a frozen suite. `release/23.x` is **open**: point releases follow, and the snapshot
  noble-23 carried on 2026-08-30 was built 2026-08-18 — it *predates* the 23.1.0 tag whose version it
  already names. The guard covers the major, so a patch-level diagnostic shift inside 23 surfaces as a
  gate failure rather than a silent pass; a numerics shift would surface in the same twin dump this
  ADR used. `procedures/CI_CD.md` carries this in full.
- **One cold ccache lineage per consuming job, once** — every key interpolates the major, so this is
  automatic.
- **arm64 remains reachable**: apt.llvm.org publishes noble-23 for `amd64 arm64 s390x`, so a future
  `ubuntu-24.04-arm` Clang job could install it.
- **`-Wperf-constraint-implies-noexcept` stays out of `-Wall`** — the forward-looking loss ADR-0028
  recorded at 22 is not undone by 23, and still has to be enabled explicitly if this project ever
  adopts `[[clang::nonblocking]]`.
- **Two corrections land with this change, both about the pin and both recorded here rather than made
  silently.** (1) `ARCHITECTURE_REVIEW_GATE.md` rule 1 still said the Clang pin is gated *"even though
  neither Clang job uploads an artifact"*, which ADR-0030 falsified; it now says the pin was gated then
  as a repository decision and is additionally gated now because it builds the shipped artifact — the
  rule itself is unchanged. (2) `DEPENDENCY_POLICY.md` §Update mechanisms named `CURRENT_LLVM_STABLE`
  as the one-page trigger; it now names upstream's release page, with that constant kept as
  corroboration and its lag stated.
- **The next revisit trigger is 24.1.0 final**, checked the same way: llvm.org's *Latest LLVM Release*
  banner, corroborated by per-release docs at `releases.llvm.org/<version>/`. Not at a fixed cadence,
  not on a release candidate, and not on `CURRENT_LLVM_STABLE` alone.

## Related code

- `.github/workflows/build.yml:114` (`ANAMORPH_CLANG_VERSION: 23`) — the single authority
- `scripts/clang-warning-baseline.txt:31` (`# clang-major: 23`) — the mirror the checker cross-checks
- `scripts/setup-llvm-apt.sh` — installs `clang-<n>`, `lld-<n>`, `libclang-rt-<n>-dev`; unchanged
  except for the prose that named 22 as upstream stable
- `tests/dsp_dump.cpp` — the bit-identity instrument this ADR's rule-2 evidence uses
