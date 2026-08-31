#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- pinned Clang toolchain from the official LLVM apt repository
#
#  Usage: scripts/setup-llvm-apt.sh <clang-major>
#
#  Installs exactly ONE Clang major -- compiler, lld and the sanitizer runtime
#  -- from apt.llvm.org, LLVM's own Debian/Ubuntu distribution channel. Safe to
#  re-run.
#
#  WHY THIS EXISTS AT ALL. Ubuntu's own archives stop at clang-20 for noble
#  (24.04), which is what `ubuntu-latest` resolves to; 21 and 22 are published
#  for 26.04 only. The pinned major is upstream STABLE, and apt.llvm.org is
#  where upstream ships it for noble -- the same source `apt.llvm.org/llvm.sh`
#  uses. Staying on the stock archive would mean holding the warning gate, the
#  sanitizer host and the SHIPPED Linux compiler majors behind upstream because
#  of a packaging boundary, which is a distribution fact about Ubuntu rather
#  than a fact about this project.
#
#  "STABLE" IS ASSERTED, NOT ASSUMED -- see RELEASE_IDENTITY below. apt.llvm.org
#  publishes rolling BRANCH builds, so a suite named `-N` can carry a commit from
#  BEFORE that major's release. Measured 2026-08-30: every `-23` suite (noble,
#  resolute, bookworm, trixie) was built from the SAME commit `55feb0a3b6b7`
#  (noble's package is `1:23.1.0~++20260818083557+55feb0a3b6b7`; the others
#  differ only in build timestamp), 49 commits BEFORE the `llvmorg-23.1.0` tag
#  (`ea7d852a`, 2026-08-25) -- a pre-release compiler under a released-looking
#  version string. noble-22, by contrast, is built from `ca7933e47d3a`, which IS
#  `llvmorg-22.1.8^{}`. The assertion below is what tells the two apart, and it
#  is why a major with no recorded release identity cannot be installed at all.
#  ADR-0033 carries the measurement and the decision it forced.
#
#  WHY NOT `llvm.sh`. That script decides the version itself from its own
#  notion of stable, installs a broad toolchain, and can add more than one
#  suite. `ANAMORPH_CLANG_VERSION` in `.github/workflows/build.yml` is this
#  repository's single authority for the major, so the suite is added by hand,
#  the major comes from the caller, and exactly three packages are installed.
#  The mechanism follows the version; it never chooses it.
#
#  FAIL-CLOSED, unlike the ccache install beside it. ccache is an optimization
#  the jobs fall back from with a `::warning::`; the pinned Clang IS the job --
#  `linux` BUILDS THE SHIPPED ARTIFACT with it, compares diagnostics against a
#  baseline keyed on this major, and links under `-flto` with the matching
#  `lld-<major>`; `sanitizers` links `libclang-rt-<major>`. A partial install must stop the job,
#  never let it quietly proceed with the image's default `clang`. So: `set -e`,
#  every step checked, and `clang-<major> --version` asserted at the end. If
#  apt.llvm.org is unreachable the job fails saying so, which is the honest
#  outcome -- falling back to a different compiler would trip the baseline
#  guard (exit 2) one step later and read as a project problem instead.
#
#  Transient network failure is absorbed rather than ignored: the key fetch and
#  the index update both retry. A repeatable failure still fails.
#
#  Network domains this script needs: apt.llvm.org (suite + signing key) and the
#  Ubuntu apt mirrors.
#
#  LOCALLY: if the machine already carries an apt.llvm.org source added by
#  `llvm.sh` or by hand, apt refuses the pair with "Conflicting values set for
#  option Signed-By" and names both keyring paths. That message is better than
#  anything this script could add, so it is left to apt -- delete the other
#  `/etc/apt/sources.list.d/*llvm*` file and re-run. CI starts from a clean
#  image, where the case cannot arise.
# ============================================================================
set -euo pipefail

# ---------------------------------------------------------------------------
#  RELEASE-IDENTITY VERIFICATION -- the decision, kept separate from the install
#  so it can be exercised without one (`--self-test`, below).
#
#  WHAT COUNTS AS EVIDENCE. Two INDEPENDENT artefacts carry the upstream build
#  commit, and this script reads both:
#    * `clang-<n> --version` -- baked into the binary when it was compiled;
#    * the INSTALLED PACKAGE VERSION of each of the three packages
#      (`dpkg-query -W`) -- dpkg's record of what apt unpacked, which apt
#      accepted only after the `signed-by=` keyring verified the suite.
#  They come from different places, so requiring every source that HAS a commit
#  to agree catches a binary swapped after install (package version unchanged,
#  `--version` changed) and a repackaged `.deb` (binary unchanged, package
#  version changed) alike. It is also why a compiler whose `--version` carries no
#  commit can still be accepted -- when the package metadata supplies one.
#
#  ABSENCE IS NOT EVIDENCE. If NO source yields a build commit the identity has
#  not been established, and the install FAILS. It is not accepted because its
#  version string "looks like" a release: a version string alone cannot separate
#  `23.1.0` the release from `23.1.0~++<date>+<sha>`, the branch build that
#  predates it by 49 commits -- the exact defect ADR-0033 records. This branch
#  previously passed, which was a verification BYPASS rather than a lenience.
# ---------------------------------------------------------------------------

# The LAST `+<hex>` run of at least 7 characters. apt.llvm.org's form is
# `++<14-digit timestamp>+<sha>`, so the timestamp is never the last one; taking
# the last avoids a "must contain a-f" heuristic, which would reject the ~0.3% of
# 12-character SHA prefixes that happen to be all digits.
build_commit_from() {
    printf '%s' "${1:-}" | grep -oE '\+[0-9a-f]{7,40}' | tr -d '+' | tail -1 || true
}

# Either side may be the abbreviation: apt.llvm.org spells 12 characters, the
# recorded tag commit is the full 40.
commit_prefix_match() {
    [ -n "${1:-}" ] && [ -n "${2:-}" ] || return 1
    case "$1" in "$2"*) return 0 ;; esac
    case "$2" in "$1"*) return 0 ;; esac
    return 1
}

# verify_release_identity <expected version> <expected tag commit> <label=value>...
# Prints every source it was given -- present or absent -- so the log says which
# evidence the verdict rests on, then returns 0 (accept) or 1 (refuse).
verify_release_identity() {
    _expect_version="$1"; _expect_commit="$2"; shift 2
    _version_line=""; _commit=""; _from=""; _conflict=""

    for _pair in "$@"; do
        _label="${_pair%%=*}"; _value="${_pair#*=}"
        [ "$_label" = "compiler --version" ] && _version_line="$_value"
        if [ -z "$_value" ]; then
            echo "setup-llvm-apt:   evidence  ${_label}: (unavailable)"
            continue
        fi
        echo "setup-llvm-apt:   evidence  ${_label}: ${_value}"
        _c="$(build_commit_from "$_value")"
        [ -n "$_c" ] || continue
        _from="${_from}${_from:+, }${_label}"
        if [ -z "$_commit" ]; then
            _commit="$_c"
        elif ! commit_prefix_match "$_commit" "$_c"; then
            _conflict="${_label} says ${_c} while an earlier source says ${_commit}"
        fi
    done

    # 1. THE FULL RELEASE VERSION, from the compiler itself. `22.1.7` and
    #    `22.1.8` are both "22.x" and only one is what this repository recorded.
    case "$_version_line" in
        *"clang version ${_expect_version}"*) ;;
        *)
            echo "setup-llvm-apt: FAIL -- the compiler does not report version ${_expect_version}." >&2
            echo "setup-llvm-apt:   got: ${_version_line:-(no version line)}" >&2
            return 1
            ;;
    esac

    # 2. SOURCES THAT CARRY A COMMIT MUST AGREE. A binary and the package that
    #    claims to contain it disagreeing is a broken install, not a near-miss.
    if [ -n "$_conflict" ]; then
        echo "setup-llvm-apt: FAIL -- the identity sources disagree: ${_conflict}." >&2
        echo "setup-llvm-apt: the installed package and the binary it should contain must match." >&2
        return 1
    fi

    # 3. FAIL CLOSED WHEN NOTHING CARRIES A COMMIT. See the header: absence of
    #    metadata is not evidence of a release.
    if [ -z "$_commit" ]; then
        echo "setup-llvm-apt: FAIL -- no build commit in ANY identity source, so this compiler" >&2
        echo "setup-llvm-apt: cannot be shown to be the llvmorg-${_expect_version} release." >&2
        echo "setup-llvm-apt: A version string alone does not separate a release from a branch" >&2
        echo "setup-llvm-apt: build carrying the same number (ADR-0033), so this fails CLOSED" >&2
        echo "setup-llvm-apt: rather than assuming. If a package source is genuinely expected to" >&2
        echo "setup-llvm-apt: ship no build commit, that is a new mechanism and needs its own" >&2
        echo "setup-llvm-apt: evidence -- do not relax this branch to accept it." >&2
        return 1
    fi

    # 4. THE COMMIT MUST BE THE RECORDED RELEASE TAG'S.
    if ! commit_prefix_match "$_commit" "$_expect_commit"; then
        echo "setup-llvm-apt: FAIL -- this is NOT the ${_expect_version} release build." >&2
        echo "setup-llvm-apt:   built from:             ${_commit}  (per ${_from})" >&2
        echo "setup-llvm-apt:   llvmorg-${_expect_version} is: ${_expect_commit}" >&2
        echo "setup-llvm-apt: apt.llvm.org publishes rolling branch builds; this one is not the" >&2
        echo "setup-llvm-apt: release. Shipping it would break the stable-only rule (ADR-0028," >&2
        echo "setup-llvm-apt: ADR-0033). Do not widen this check to get green." >&2
        return 1
    fi

    echo "setup-llvm-apt: release identity CONFIRMED -- clang version ${_expect_version}, built"
    echo "setup-llvm-apt: from ${_commit} = llvmorg-${_expect_version}; evidence: ${_from}"
    return 0
}

# PROVE THE VERIFIER IS LIVE BEFORE TRUSTING ITS SILENCE -- the same rule the
# seven checkers in `scripts/` carry (`--self-test`), and the reason this one
# exists is that its predecessor was silently accepting case 3 below. Needs no
# network, no apt and no installed compiler: it drives the decision function
# with recorded strings.
self_test() {
    _fail=0
    t_ok="ca7933e47d3a3451d81e72ac174dcb5aa28b59d1"          # llvmorg-22.1.8^{}
    t_23="ea7d852a70e8bdfaf601d6626a760f9771b2c4b4"          # llvmorg-23.1.0^{}
    t_vl="Ubuntu clang version 22.1.8 (++20260714014902+ca7933e47d3a-1~exp1~20260714135019.80)"
    t_pk="1:22.1.8~++20260714014902+ca7933e47d3a-1~exp1~20260714135019.80"
    t_pre_vl="Ubuntu clang version 23.1.0 (++20260818083557+55feb0a3b6b7-1~exp1~20260818083714.47)"
    t_pre_pk="1:23.1.0~++20260818083557+55feb0a3b6b7-1~exp1~20260818083714.47"

    echo "setup-llvm-apt --self-test: release-identity verifier"

    # 1. The real LLVM 22 release, both sources agreeing.
    _expect_case pass "22.1.8 release, compiler + package agree" \
        "22.1.8" "$t_ok" "compiler --version=$t_vl" "dpkg clang-22=$t_pk"

    # 2. A build commit that is not the release tag's -- the ADR-0033 defect,
    #    driven with the real pre-release 23 strings.
    _expect_case fail "23.1.0 pre-release build (49 commits before the tag)" \
        "23.1.0" "$t_23" "compiler --version=$t_pre_vl" "dpkg clang-23=$t_pre_pk"

    # 3. NO build commit anywhere. Version looks right; identity is unproven.
    #    This is the bypass that used to pass.
    _expect_case fail "no build commit in any source (fail closed)" \
        "22.1.8" "$t_ok" "compiler --version=clang version 22.1.8" "dpkg clang-22=1:22.1.8-1ubuntu1"

    # 4. NO commit in `--version`, but the PACKAGE supplies one that matches --
    #    accepted, and accepted for the right reason: package metadata, not the
    #    version string.
    _expect_case pass "commit absent from --version, supplied by the package" \
        "22.1.8" "$t_ok" "compiler --version=clang version 22.1.8" "dpkg clang-22=$t_pk"

    # 5. The mirror image: package metadata unavailable, compiler carries it.
    _expect_case pass "package metadata unavailable, compiler carries the commit" \
        "22.1.8" "$t_ok" "compiler --version=$t_vl" "dpkg clang-22="

    # 6. Sources disagree -- a swapped binary or a repackaged .deb.
    _expect_case fail "compiler and package disagree on the build commit" \
        "22.1.8" "$t_ok" "compiler --version=$t_vl" "dpkg clang-22=$t_pre_pk"

    # 7. Right commit, wrong release version.
    _expect_case fail "version line reports 22.1.7 against a 22.1.8 identity" \
        "22.1.8" "$t_ok" \
        "compiler --version=Ubuntu clang version 22.1.7 (++20260714014902+ca7933e47d3a-1)" \
        "dpkg clang-22=$t_pk"

    # 8. No version line at all (a compiler that could not be run).
    _expect_case fail "no version line at all" \
        "22.1.8" "$t_ok" "compiler --version=" "dpkg clang-22=$t_pk"

    # 9. The abbreviation/full-length pairing the real data uses must match in
    #    both directions, so a 12-character package commit is not rejected
    #    against the 40-character tag.
    _expect_case pass "12-character build commit against the 40-character tag" \
        "22.1.8" "$t_ok" "compiler --version=clang version 22.1.8" "dpkg clang-22=x+ca7933e47d3a-1"

    if [ "$_fail" -ne 0 ]; then
        echo "setup-llvm-apt --self-test: ${_fail} case(s) FAILED" >&2
        return 2
    fi
    echo "setup-llvm-apt --self-test: 9 cases passed"
    return 0
}

_expect_case() {
    _want="$1"; _name="$2"; shift 2
    if verify_release_identity "$@" >/dev/null 2>&1; then _got=pass; else _got=fail; fi
    if [ "$_got" = "$_want" ]; then
        echo "  ok    ${_name} -> ${_got}"
    else
        echo "  FAIL  ${_name} -> ${_got}, want ${_want}" >&2
        _fail=$((_fail + 1))
    fi
}

if [ "${1:-}" = "--self-test" ]; then
    self_test
    exit $?
fi

MAJOR="${1:-}"
case "$MAJOR" in
    ''|*[!0-9]*)
        echo "setup-llvm-apt: usage: $0 <clang-major>  (got '${MAJOR}')" >&2
        exit 2
        ;;
esac

# ---------------------------------------------------------------------------
#  RELEASE_IDENTITY -- what "upstream stable major N" actually IS, upstream.
#
#  One line per major this repository is willing to install:
#     <major>) <full release version> <the llvmorg-<version> tag's COMMIT>
#
#  Why a commit and not just a version string. apt.llvm.org names its packages
#  after the version the release BRANCH is sitting on, not after a tag, so a
#  pre-release build of `release/23.x` is called `23.1.0~++<date>+<sha>` months
#  or weeks before 23.1.0 exists. The version string alone therefore cannot tell
#  a released compiler from a pre-release one -- but the `+<sha>` in it can, and
#  `clang --version` prints it. The tag commits below come from
#  `git ls-remote --tags https://github.com/llvm/llvm-project` (the
#  `llvmorg-<v>^{}` peeled ref), which is upstream's own record.
#
#  FAIL-CLOSED ON AN UNRECORDED MAJOR, and that is the point rather than a
#  side effect: bumping `ANAMORPH_CLANG_VERSION` to a major nobody has looked up
#  stops here, with a message saying what to look up, instead of installing
#  whatever the mirror happens to be building that week.
#
#  WHEN THIS FAILS ON A MAJOR THAT *IS* LISTED, the mirror has rebuilt the suite
#  from a different commit. That is not a bug in this script: under the
#  stable-only rule the compiler is no longer the release. Re-derive the identity
#  from upstream, confirm the new commit is the tag (or a later RELEASE tag), and
#  update the line here in the same change -- never widen the match.
# ---------------------------------------------------------------------------
release_identity() {
    case "$1" in
        22) echo "22.1.8 ca7933e47d3a3451d81e72ac174dcb5aa28b59d1" ;;
        # 23 is deliberately ABSENT. `llvmorg-23.1.0^{}` is
        # ea7d852a70e8bdfaf601d6626a760f9771b2c4b4 (2026-08-25), and no
        # apt.llvm.org suite has been rebuilt from it -- see the header and
        # ADR-0033. Adding the line before the mirror catches up would make this
        # gate fail, which is the correct outcome and not a reason to widen it.
        *)  return 1 ;;
    esac
}

if ! IDENTITY="$(release_identity "$MAJOR")"; then
    echo "setup-llvm-apt: no upstream release identity is recorded for clang-${MAJOR}." >&2
    echo "setup-llvm-apt: this repository installs RELEASED compilers only, and apt.llvm.org" >&2
    echo "setup-llvm-apt: publishes rolling branch builds that can predate a release by weeks." >&2
    echo "setup-llvm-apt: record <version> and the llvmorg-<version> tag commit in" >&2
    echo "setup-llvm-apt: release_identity() -- \`git ls-remote --tags https://github.com/llvm/llvm-project\`" >&2
    echo "setup-llvm-apt: -- and only after checking the suite actually carries that commit." >&2
    exit 2
fi
EXPECT_VERSION="${IDENTITY%% *}"
EXPECT_COMMIT="${IDENTITY##* }"

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

# The suite name is per-distribution, so it is READ from the machine rather than
# assumed: apt.llvm.org publishes `llvm-toolchain-<codename>-<major>`, and a
# hard-coded `noble` would silently point at the wrong suite the day
# `ubuntu-latest` moves. An unknown codename fails here instead of installing
# something unintended.
CODENAME="$(. /etc/os-release && echo "${VERSION_CODENAME:-}")"
if [ -z "$CODENAME" ]; then
    echo "setup-llvm-apt: cannot read VERSION_CODENAME from /etc/os-release" >&2
    exit 1
fi

KEYRING="/etc/apt/keyrings/llvm-apt.gpg"
SOURCE="/etc/apt/sources.list.d/llvm-toolchain-${MAJOR}.list"
SUITE="llvm-toolchain-${CODENAME}-${MAJOR}"

echo "setup-llvm-apt: installing clang-${MAJOR} from apt.llvm.org (${SUITE})"

# The key is FETCHED rather than vendored. Vendoring it would remove one network
# dependency and add a worse one: a committed key that upstream later rotates
# fails the build with a signature error and needs a repository commit to fix,
# on a schedule nobody here controls. It arrives over HTTPS from the same host
# that serves the packages, and `signed-by=` scopes it to this one suite so it
# can never validate anything else on the machine.
$SUDO install -d -m 0755 /etc/apt/keyrings
curl -fsSL --retry 3 --retry-delay 2 https://apt.llvm.org/llvm-snapshot.gpg.key \
    | $SUDO gpg --dearmor --yes -o "$KEYRING"
if [ ! -s "$KEYRING" ]; then
    echo "setup-llvm-apt: the LLVM signing key did not arrive at ${KEYRING}" >&2
    exit 1
fi
$SUDO chmod 0644 "$KEYRING"

# The key is PINNED BY IDENTITY, which is what makes fetching it defensible.
# HTTPS proves the bytes came from apt.llvm.org; this proves the keyring holds
# the key this repository decided to trust AND NOTHING ELSE. If upstream ever
# rotates it, the job fails here with a specific message and a human decides --
# which is the outcome you want from a third-party package source, rather than
# silently trusting whatever key the host served this morning.
#
# WHY THE ASSERTION IS ON PRIMARY KEYS AND NOT ON A SUBSTRING. `signed-by=`
# trusts EVERY key in the keyring for that suite, so "the expected fingerprint
# appears somewhere in this file" is not the guarantee above: a served blob
# carrying the genuine key CONCATENATED with another one satisfies it, and both
# get trusted. The list of primary fingerprints must therefore equal the pinned
# one exactly -- one line, that value. A second primary key makes it two lines
# and fails.
#
# It counts `pub:` records rather than `fpr:` records because the genuine key
# has TWO fingerprints: the primary and one subkey. An "exactly one fingerprint"
# test would reject the real key, so this walks each `pub:` and takes the `fpr:`
# that follows it. Subkeys are deliberately not enumerated -- a subkey is bound
# to its primary by a signature only the primary's holder can make, so it adds
# no identity beyond the primary this pins.
LLVM_KEY_FPR="6084F3CF814B57C1CF12EFD515CF4D18AF4F7421"
# `|| true` so the ASSERTION decides, not `set -e`: on an unreadable or corrupt
# keyring `gpg` exits non-zero, and under `pipefail` that would abort the script
# at this assignment with no message at all. Failing is right; failing silently
# is not, so the empty result falls through to the diagnostic below.
LLVM_PRIMARY_FPRS="$(gpg --show-keys --with-colons "$KEYRING" 2>/dev/null \
    | awk -F: '/^pub:/ { primary = 1; next } /^fpr:/ { if (primary) { print $10; primary = 0 } }' \
    || true)"
if [ "$LLVM_PRIMARY_FPRS" != "$LLVM_KEY_FPR" ]; then
    echo "setup-llvm-apt: ${KEYRING} does not hold exactly the expected LLVM signing key" >&2
    echo "setup-llvm-apt: expected exactly one primary key, ${LLVM_KEY_FPR}" >&2
    echo "setup-llvm-apt:            (Sylvestre Ledru - Debian LLVM packages)" >&2
    echo "setup-llvm-apt: got primary key(s):" >&2
    printf '%s\n' "${LLVM_PRIMARY_FPRS:-(none)}" | sed 's/^/setup-llvm-apt:   /' >&2
    exit 1
fi

echo "deb [signed-by=${KEYRING}] https://apt.llvm.org/${CODENAME}/ ${SUITE} main" \
    | $SUDO tee "$SOURCE" > /dev/null

# Scoped to the new source: a full `apt-get update` here would also re-fetch
# every Ubuntu index, so an unrelated mirror hiccup would present as an LLVM
# failure. Retries absorb a transient 5xx from apt.llvm.org itself.
$SUDO apt-get update -y -o Acquire::Retries=3 \
    -o Dir::Etc::sourcelist="$SOURCE" -o Dir::Etc::sourceparts="-" \
    -o APT::Get::List-Cleanup="0"

# All three, in one transaction, for every Clang job. Only the sanitizer jobs
# link a sanitizer runtime, but installing the same set from one code path is
# worth more than the few seconds it saves elsewhere: two divergent package
# lists is how one job ends up with a toolchain the other does not have.
# `lld-${MAJOR}` is not optional for the release build -- `linux` links the
# shipped plugin under `-flto`, which needs lld to resolve archive members the
# single GNU ld scan passed over (CMakeLists.txt carries the full reasoning).
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    "clang-${MAJOR}" "lld-${MAJOR}" "libclang-rt-${MAJOR}-dev"

# THE ASSERTION, not a courtesy print, and it is a RELEASE-IDENTITY check rather
# than a major check. If the suite resolved to something else, or the install
# half-succeeded, or -- the case that made this necessary -- the mirror is
# serving a pre-release build of the right major, the job stops HERE rather than
# at a confusing baseline mismatch two steps later or, worse, in a shipped
# artifact nobody re-checked.
#
# FOUR SOURCES ARE OFFERED, not one: the compiler's own version line and the
# installed version of each of the three packages. The rules are in
# `verify_release_identity` above -- every source that carries a build commit
# must agree, at least one must carry one, and it must be the recorded release
# tag's. Passing all three packages also means a set that did not come from one
# apt transaction shows up as a source conflict rather than passing quietly.
VERSION_LINE="$("clang-${MAJOR}" --version | head -1)"
PKG_CLANG="$(dpkg-query -W -f='${Version}' "clang-${MAJOR}" 2>/dev/null || true)"
PKG_LLD="$(dpkg-query -W -f='${Version}' "lld-${MAJOR}" 2>/dev/null || true)"
PKG_RT="$(dpkg-query -W -f='${Version}' "libclang-rt-${MAJOR}-dev" 2>/dev/null || true)"

# RECORDED, NOT ASSERTED. Which repository served the bytes is worth having in
# the log -- apt already refused anything the `signed-by=` keyring did not
# validate, so this adds provenance to the record rather than a second gate, and
# `apt-cache policy`'s table is not a stable enough interface to fail a job on.
ORIGIN="$(apt-cache policy "clang-${MAJOR}" 2>/dev/null \
          | awk '/^ \*\*\*/ { f = 1; next } f && NF >= 3 { print $2, $3; exit }' || true)"
echo "setup-llvm-apt: package origin (recorded, not asserted): ${ORIGIN:-unknown}"

verify_release_identity "$EXPECT_VERSION" "$EXPECT_COMMIT" \
    "compiler --version=${VERSION_LINE}" \
    "dpkg clang-${MAJOR}=${PKG_CLANG}" \
    "dpkg lld-${MAJOR}=${PKG_LLD}" \
    "dpkg libclang-rt-${MAJOR}-dev=${PKG_RT}" || exit 1

command -v "ld.lld-${MAJOR}" >/dev/null 2>&1 || [ -x "/usr/lib/llvm-${MAJOR}/bin/ld.lld" ] || {
    echo "setup-llvm-apt: lld-${MAJOR} is missing; the LTO plugin link needs it" >&2
    exit 1
}
