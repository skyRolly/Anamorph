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
#  for 26.04 only. The upstream STABLE release is 22.x, and apt.llvm.org is
#  where upstream ships it for noble -- the same source `apt.llvm.org/llvm.sh`
#  uses, whose own `CURRENT_LLVM_STABLE` reads 22. Staying on the stock archive
#  would mean holding the warning gate and the sanitizer host two majors behind
#  upstream because of a packaging boundary, which is a distribution fact about
#  Ubuntu rather than a fact about this project.
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
#  `linux-clang` compares diagnostics against a baseline keyed on this major and
#  `sanitizers` links `libclang-rt-<major>`. A partial install must stop the job,
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

echo "deb [signed-by=${KEYRING}] https://apt.llvm.org/${CODENAME}/ ${SUITE} main" \
    | $SUDO tee "$SOURCE" > /dev/null

# Scoped to the new source: a full `apt-get update` here would also re-fetch
# every Ubuntu index, so an unrelated mirror hiccup would present as an LLVM
# failure. Retries absorb a transient 5xx from apt.llvm.org itself.
$SUDO apt-get update -y -o Acquire::Retries=3 \
    -o Dir::Etc::sourcelist="$SOURCE" -o Dir::Etc::sourceparts="-" \
    -o APT::Get::List-Cleanup="0"

# All three, in one transaction, for BOTH Clang jobs. `linux-clang` does not
# link a sanitizer runtime, but installing the same set from one code path is
# worth more than the few seconds it saves there: two divergent package lists is
# how one job ends up with a toolchain the other does not have.
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
