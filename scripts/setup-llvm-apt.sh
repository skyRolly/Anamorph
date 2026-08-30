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
#  for 26.04 only. The upstream STABLE release is 23.x (23.1.0, 2026-08-25), and
#  apt.llvm.org is where upstream ships it for noble -- the same source
#  `apt.llvm.org/llvm.sh` uses. Staying on the stock archive would mean holding
#  the warning gate, the sanitizer host and the SHIPPED Linux compiler three
#  majors behind upstream because of a packaging boundary, which is a
#  distribution fact about Ubuntu rather than a fact about this project.
#
#  `llvm.sh`'s own `CURRENT_LLVM_STABLE` is the cheap check for "what is stable",
#  and it LAGS the release it reports: it still read 22 on 2026-08-30, five days
#  after 23.1.0 shipped. It is a proxy, not the fact -- upstream's release
#  announcement and the per-release docs under releases.llvm.org settle it, the
#  same way ADR-0028 settled the earlier disagreement between that constant and
#  the site's prose. ADR-0033 records the move to 23 and this lag.
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

MAJOR="${1:-}"
case "$MAJOR" in
    ''|*[!0-9]*)
        echo "setup-llvm-apt: usage: $0 <clang-major>  (got '${MAJOR}')" >&2
        exit 2
        ;;
esac

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

# The assertion, not a courtesy print: if the suite resolved to something else,
# or the install half-succeeded, the job stops here rather than at a confusing
# baseline mismatch two steps later.
"clang-${MAJOR}" --version
"clang-${MAJOR}" --version | grep -qE "clang version ${MAJOR}\." || {
    echo "setup-llvm-apt: clang-${MAJOR} reports a version that is not ${MAJOR}.x" >&2
    exit 1
}
command -v "ld.lld-${MAJOR}" >/dev/null 2>&1 || [ -x "/usr/lib/llvm-${MAJOR}/bin/ld.lld" ] || {
    echo "setup-llvm-apt: lld-${MAJOR} is missing; the LTO plugin link needs it" >&2
    exit 1
}
