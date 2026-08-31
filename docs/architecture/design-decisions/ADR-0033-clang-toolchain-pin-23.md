# ADR-0033 — LLVM 23 evaluated and NOT adopted; the pin holds at 22, and "stable" becomes an assertion

**Status:** **Accepted** (Build System change — `ARCHITECTURE_REVIEW_GATE.md` rule 1; maintainer-directed
toolchain update 2026-08-30, firing [ADR-0028](ADR-0028-clang-toolchain-pin.md)'s own revisit trigger.)

**Amends ADR-0028; supersedes nothing.** `ANAMORPH_CLANG_VERSION` stays **22**. ADR-0028's rule
(*upstream stable, not newest*; by major; the install mechanism follows the version and never chooses
it), its `ARCHITECTURE_REVIEW_GATE.md` amendment and its `-fsanitize=vptr` restoration all stand.
[ADR-0030](ADR-0030-linux-release-toolchain.md)'s "stays 22" clause stands too. What this ADR adds is a
**release-identity assertion** at install time, and what it records is why 23 cannot be adopted yet.

> **This ADR's first draft adopted 23, and was wrong.** It moved the pin on the strength of *"23.1.0 was
> released"* without asking the separate question *"is the release what apt.llvm.org actually serves?"*
> It is corrected here rather than deleted, in the same style ADR-0028 used for its own first draft: the
> error is the reason the assertion below exists, and a reader who only sees the fix learns less than
> one who sees what let it through. The measurements that draft gathered are kept in Evidence — they are
> what the next attempt starts from.

## Context

ADR-0028 closed with a revisit trigger in its own words: *"Revisit when upstream cuts a new stable
major — 23.1.0 final is the next trigger."* That trigger fired: **LLVM 23.1.0 was released
2026-08-25** (`llvmorg-23.1.0^{}` = `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`, *"Bump version to
23.1.0"*, committed 2026-08-25T19:15:21Z).

Since ADR-0028 the **scope of the pin changed**: ADR-0030 moved the Linux release build onto this
toolchain, so the pinned Clang compiles and links the shipped Linux VST3 and Standalone.
`DEPENDENCY_POLICY.md`'s compliance log had already recorded the consequence — *"the next bump of this
pin does touch shipped bytes and rules 2–3 apply to it"*.

## Problem

ADR-0028's rule is *upstream stable*. It was enforced by checking the **major**, because at the time
nothing suggested the two could come apart. They can: **apt.llvm.org publishes rolling BRANCH builds**,
so a suite named `-N` serves whatever commit of `release/N.x` it last built, under a version string
that already carries N's release number. A `23.1.0~++<date>+<sha>` package is called 23.1.0 months
before 23.1.0 exists.

So *"a release exists"* and *"this mirror serves it"* are different questions, and only the second one
decides what compiles the shipped bytes.

## The measurement

Read 2026-08-30, five days after the release:

| Question | Answer |
|---|---|
| Does LLVM 23.1.0 exist? | **Yes.** llvm.org: *"25 August 2026: LLVM 23.1.0 is now available"*; `releases.llvm.org/23.1.0/docs/` → HTTP 200; `24.1.0` → 404 |
| `llvmorg-23.1.0^{}` | `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`, committed **2026-08-25T19:15:21Z** |
| What does apt.llvm.org's `-23` suite serve? | `1:23.1.0~++20260818083557+55feb0a3b6b7`, built **2026-08-18** from `55feb0a3b6b776af9bf375ce931dcbeac0ab8e17` |
| Where is that commit relative to the tag? | An **ancestor** — `55feb0a3` is **49 commits before** `ea7d852a` (`git rev-list --count 55feb0a3b6b7..llvmorg-23.1.0^{}`) |
| Is it only noble? | **No.** `noble`, `resolute`, `bookworm` and `trixie` were all built from the same commit `55feb0a3b6b7` (their package strings differ only in build timestamp), all indexed 2026-08-18. The mirror has not rebuilt the 23 branch since before the release |
| Does any Ubuntu series publish `clang-23`? | **No.** Launchpad: `clang-22` reaches `1:22.1.6-1ubuntu2` (stonking) and `1:22.1.2-1ubuntu1` (resolute); `clang-23` is unpublished everywhere. noble stops at `clang-20` |
| Are upstream's own release binaries reachable and verifiable from here? | **Not verifiable.** `github.com/llvm/llvm-project/releases/…` returns 403 through this environment's proxy for non-git HTTP, so their existence and checksums could not be confirmed. Availability is **not** assumed on their behalf |

**And the control, which is what makes the contrast decisive:** noble-22 serves
`1:22.1.8~++20260714014902+ca7933e47d3a`, and `ca7933e47d3a` **is** `llvmorg-22.1.8^{}`. The 22 pin has
been a build of the *release commit itself* all along — a fact ADR-0028 had, in its own words, the
wrong side of (*"never the tagged `llvmorg-*` build"*), corrected in that ADR with a dated note.

So the first draft's move traded a **release-commit build** for a **pre-release branch build** while
believing it was moving from stable to stable.

## Options

1. **Adopt 23 from apt.llvm.org as-is.** *Rejected — this is the defect.* It ships a pre-release
   compiler, 49 commits short of the release, for the shipped Linux artifact. ADR-0028 option 5
   rejected 23 for being a release candidate; a branch build that predates the release is the same
   class of thing wearing the release's number.
2. **Adopt 23 and add the identity assertion anyway.** *Rejected:* the assertion fails against the only
   installable package, so this is "adopt" spelled as a red pipeline. Fail-closed is right; choosing a
   value that is known to fail closed is not.
3. **Pin an exact package version.** *Rejected, unchanged from ADR-0028:* apt.llvm.org's pool retains
   only the current `.deb` per architecture, so an exact version stops resolving on the next rebuild —
   a hard `apt-get install` failure. The *compiler's* identity is pinnable even though the *package's*
   version is not, which is what option 5 uses.
4. **Install upstream's official 23.1.0 release binaries instead of apt.** *Not adoptable here, and
   not rejected on the merits:* their existence could not be verified from this environment (403), so
   adopting them would mean asserting availability this round cannot demonstrate. It also replaces the
   acquisition mechanism for all five Linux jobs, which is its own ADR with its own trust surface
   (release-manager signing keys instead of the scoped apt keyring). Recorded as the first thing to
   evaluate if the mirror never catches up.
5. **Hold at 22, and make "stable" checkable — chosen.** 22.1.8 is a final release, the installed
   package is its tag commit, and the rule ADR-0028 set is satisfied exactly. The gap that let a
   pre-release through is closed by asserting the compiler's identity rather than its major.

## Decision

**`ANAMORPH_CLANG_VERSION` stays 22**, by major, from apt.llvm.org, installed by
`scripts/setup-llvm-apt.sh`. Nothing about the five install paths, the package set, the scoped keyring
or the fingerprint check changes.

**`scripts/setup-llvm-apt.sh` now carries an upstream RELEASE IDENTITY per major** — the full release
version and that release's `llvmorg-<version>` tag commit — and asserts both before the job proceeds:

- **An unrecorded major is refused (exit 2)**, with a message naming what to look up and where. This is
  the property that would have caught the first draft: bumping the pin to a major nobody had verified
  stops immediately instead of installing whatever the mirror was building that week.
- **A recorded major whose installed build commit is not the tag's is refused (exit 1)**, naming both
  commits. The build commit is the only field that distinguishes a release build from a branch build
  wearing the release's version number.
- **The commit is read from FOUR sources**, not one: `clang --version`, baked into the binary at
  compile time, and the installed version of each of the three packages (`dpkg-query -W`), which is
  dpkg's record of what apt unpacked after the `signed-by=` keyring validated the suite. They are
  different artefacts, so requiring every source that carries a commit to **agree** catches a binary
  swapped after install and a repackaged `.deb` alike — and a compiler whose own `--version` omits the
  commit is still verifiable when the package metadata supplies one.
- **If NO source carries a commit the install fails closed (exit 1).** *This ADR's first draft accepted
  that case*, reasoning that a package without a `+<sha>` is "release-versioned by construction". That
  was a verification bypass, not a lenience: absence of metadata is not evidence of a release, and a
  version string alone is exactly what cannot separate `23.1.0` from `23.1.0~++…`, which is the defect
  this whole ADR exists for. Corrected 2026-08-30 in the same round the reviewer found it.
- **`--self-test` drives the decision function** with recorded strings — no apt, no network, no
  compiler — and runs in `source-lint` and `preflight.sh`, so a dead verifier fails in a lint job
  rather than accepting whatever the mirror served.

The identity table records **22 only**. The 23 line is deliberately absent and says so at the point of
absence: adding it before the mirror catches up would make the gate fail, which is the correct outcome
and not a reason to widen the check.

**The condition that lifts the block is single, checkable and written down:** an apt.llvm.org `-23`
suite rebuilt from `ea7d852a` — or from a later 23.x *release* tag. At that point the identity line is
added and the pin moves, and the assertion proves the move rather than the reader trusting it.

## Consequences

- **No toolchain moved, so nothing downstream moved.** `scripts/clang-warning-baseline.txt` still
  records `clang-major: 22`, the ccache lineages are unchanged, and the shipped Linux bytes are the
  ones already in main. `DEPENDENCY_POLICY.md` rules 2–3 have nothing to act on — **no twin dump, no
  Level-5 audition and no compatibility re-verification are owed by this ADR**, for the same reason
  ADR-0028 was exempt from them, arrived at differently: ADR-0028 shipped no bytes, this ADR changes
  no compiler.
- **The install is stricter than it was, and can now fail on a day nothing in this tree changed** — if
  apt.llvm.org rebuilds noble-22 from any other commit. That is a correct failure under a stable-only
  rule, and the script says so where it fails, with the re-derivation command. 22.x is closed (22.1.8
  is upstream's newest 22 tag) and noble-22's index has not moved since 2026-07-30, so the practical
  risk is low; the point is that it would be loud rather than silent.
- **ADR-0028's reproducibility bullet is corrected in place**, with a dated note: its *"never the
  tagged `llvmorg-*` build"* clause was wrong in this project's favour and is exactly the belief that
  made the reverse error possible. Its conclusion — major granularity, no exact package-version pin —
  is unaffected.
- **`DEPENDENCY_POLICY.md` §Update mechanisms gains the second question.** ADR-0028's one-page trigger
  (`CURRENT_LLVM_STABLE`) was already corrected to upstream's release page because it lags; this ADR
  adds that a release existing does not make it installable, and names the `.deb`'s build commit as
  the source that answers the second question.
- **No `CHANGELOG.md` entry.** `CHANGELOG_POLICY.md` rule 3 is user-visible changes only, and the
  compiler did not move.
- **The next revisit is unchanged in form**: llvm.org's *Latest LLVM Release* banner and the per-release
  docs at `releases.llvm.org/<version>/` say what stable is; the suite's own package version says
  whether it is reachable. Both, now, before the pin moves.

## Evidence + confidence

**Verified.** All measurements 2026-08-30 on one x86-64 Linux machine, both compilers installed by
`scripts/setup-llvm-apt.sh` itself.

**The blocker** — every figure in §The measurement above was read directly: the apt suites' `Release`
and `Packages.gz` for four codenames, Launchpad's published-binaries API for Ubuntu, and
`git ls-remote --tags https://github.com/llvm/llvm-project` plus a depth-400 fetch of the tag for the
commit dates and the 49-commit ancestry count.

**The assertion is live in all three directions**, run against the real packages:

| Case | Result |
|---|---|
| `setup-llvm-apt.sh 22` — recorded major, release build | **exit 0**, printing `build commit ca7933e47d3a matches llvmorg-22.1.8` |
| `setup-llvm-apt.sh 23` — no recorded identity | **exit 2**, naming what to record and how to derive it |
| `23` with the true `23.1.0 ea7d852a…` identity recorded (throwaway copy) | **exit 1**: *"clang-23 is NOT the 23.1.0 release build… built from: 55feb0a3b6b7… llvmorg-23.1.0 is: ea7d852a…"* |

The third case is the defect this ADR exists for, detected by the gate rather than by review.

**Carried forward from the withdrawn adoption, because it is what the next attempt starts from** —
measured with `clang-23` = `23.1.0~++20260818083557+55feb0a3b6b7` against control `clang-22` =
`22.1.8~++20260714014902+ca7933e47d3a`, JUCE 9.0.1 at the pinned commit:

- **32/32 twin-dump scenarios bit-identical** (`tests/dsp_dump.cpp`, same source, same JUCE, otherwise
  identical flags): FNV-1a hash over every output byte *and* the reported latency, both `--self-check`s
  passing.
- **First-party warning census `diff`-identical**: 9 entries / 17 sites, only `# clang-major:` would
  have moved — the fourth major in a row (18/20/22/23) to leave this tree's accepted set alone.
- **226 DSP + 920 state checks** green on a clang-23 Release build; **224 + 920** under
  ASan+UBSan+`vptr` with `libclang-rt-23-dev` (224 because Test 38's malloc half is by design absent
  under ASan).
- **Linux ABI floor unmoved** under clang-23: `CXXABI_1.3.9` / `GCC_4.0.0` / `GLIBC_2.38` /
  `GLIBCXX_3.4.31`.
- **Detectors survive the major**: the `-Werror=function-effects` liveness pair fires correctly in both
  directions on 23; `libclang_rt.rtsan-x86_64.a` and `libclang_rt.fuzzer-x86_64.a` are present in the
  23 install; `-fsanitize=realtime` is accepted alone and still rejected combined with `address`;
  `clang++-23 -fuse-ld=lld` resolves to `/usr/lib/llvm-23/bin/ld.lld`; the UBSan ignorelist draws no
  deprecation under Clang 23's new special-case-list v4 format.

**This evidence does not license adoption.** It was taken against the pre-release build and says only
that *that* build is behaviourally indistinguishable on this tree. The release differs from it by 49
commits, so it will be re-taken against the release build when one is installable.

**Not verified here, and named rather than implied:** whether upstream's official 23.1.0 Linux release
tarballs exist and what they hash to — the proxy in this environment returns 403 for non-git GitHub
HTTP, so option 4 is recorded as unevaluated rather than rejected.

## Related code

- `.github/workflows/build.yml` (`ANAMORPH_CLANG_VERSION: 22`) — the single authority for the major
- `scripts/setup-llvm-apt.sh` — `release_identity()` and the two assertions this ADR adds
- `scripts/clang-warning-baseline.txt` (`# clang-major: 22`) — the mirror the warning gate cross-checks
- `tests/dsp_dump.cpp` — the bit-identity instrument the carried-forward evidence used
