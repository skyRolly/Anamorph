#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- Linux build dependency setup
#
#  Installs everything needed to build the VST3 headlessly on a fresh Ubuntu
#  machine (no GUI / no IDE). Safe to re-run.
#
#  Network domains this script needs (allow-list these in a restricted sandbox):
#    - Ubuntu apt mirrors (archive.ubuntu.com / ports.ubuntu.com / your mirror)
#  The build itself additionally needs:
#    - github.com           (JUCE source, pinned commit, via CMake FetchContent)
#    - github.com / dl       (pluginval release download, optional)
#  curl + unzip: used by scripts/run-pluginval.sh to fetch and extract that
#  release. NOT implied by libcurl4-openssl-dev, which is the development
#  headers, not the CLI. GitHub-hosted runners preinstall both, so a missing
#  one only ever surfaces on a fresh machine or a minimal container -- exactly
#  the case this script exists to cover.
#  libegl-dev: JUCE 9 creates Linux OpenGL contexts via EGL instead of GLX
#  (juce_opengl linuxPackages "egl gl"), so EGL headers are a build dependency
#  even though Anamorph never attaches a GL context on Linux (ADR-0011).
#
#  lld is LLVM's linker, and it is a REQUIREMENT for the Clang builds rather than
#  a preference: GNU ld scans a static archive once, while Clang's LTO codegen
#  runs after that scan and then needs members the scan passed over, so linking
#  the plugin can fail with hundreds of undefined references to symbols that are
#  demonstrably inside libAnamorph_SharedCode.a. CMakeLists.txt probes for it and
#  falls back with a warning; installing it here is what makes the probe succeed.
#  GCC builds ignore it entirely, so this costs the default toolchain nothing.
# ============================================================================
#  TWO PROFILES, because one caller is not a fresh Ubuntu machine. The GCC
#  compatibility job runs inside the official `gcc` image -- Debian, with its own
#  compiler already installed -- and builds only the two headless test targets.
#  `headless` installs what COMPILING AND LINKING the JUCE targets needs and
#  nothing else; `full` (the default, and what a developer or a packaging job
#  wants) adds the host toolchain, the pluginval fetch/display pair and lld.
#  The lists are kept here rather than in the workflow so there is still ONE
#  place that knows what a build needs; the profile only decides how much of it.
#
#  Both profiles name `python3` EXPLICITLY. Every checker in scripts/ is Python,
#  and GitHub's Ubuntu images preinstall it, so nothing ever had to ask -- but a
#  container carries it only if something else happened to pull it in (measured:
#  `gcc:16` has python3-minimal transitively, which is true today and is not a
#  promise). A gate that cannot run because its interpreter is absent is the
#  failure this line exists to prevent.
# ============================================================================
set -euo pipefail

PROFILE="${1:-full}"
case "$PROFILE" in
    full|headless) ;;
    *)
        echo "setup-linux: usage: $0 [full|headless]  (got '${PROFILE}')" >&2
        exit 2
        ;;
esac

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

$SUDO apt-get update -y

# What compiling and linking the JUCE targets needs, on any profile. The GUI
# development headers are NOT optional for a headless build: the test targets
# link juce_gui_basics, juce_gui_extra and juce_opengl (CMakeLists.txt), so their
# headers are required to compile even though nothing here opens a window.
CORE_PACKAGES="
    cmake git ninja-build pkg-config ca-certificates python3
    libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev
    libfreetype6-dev libfontconfig1-dev
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
    libxinerama-dev libxrandr-dev libxrender-dev
    libglu1-mesa-dev mesa-common-dev libegl-dev
    libwebkit2gtk-4.1-dev libgtk-3-dev
"

# Everything the `headless` profile deliberately leaves out.
#   build-essential -- the HOST toolchain. A container that ships its own
#                      compiler must not have a distribution one installed over
#                      the top of it.
#   curl, unzip     -- run-pluginval.sh fetches and extracts a release.
#   xvfb            -- pluginval needs a display; nothing else here does.
#   lld             -- the Clang LTO link (see above). GCC never reaches it.
FULL_EXTRA_PACKAGES="
    build-essential
    curl unzip
    xvfb
    lld
"

PACKAGES="$CORE_PACKAGES"
if [ "$PROFILE" = "full" ]; then
    PACKAGES="$PACKAGES $FULL_EXTRA_PACKAGES"
fi

# `env` carries the assignment whether $SUDO is "sudo" or empty (running as
# root). A bare `$SUDO DEBIAN_FRONTEND=... apt-get` breaks in the root case: the
# assignment is not in assignment position at parse time (the first word is
# $SUDO), so when $SUDO expands to nothing `DEBIAN_FRONTEND=noninteractive`
# becomes the COMMAND NAME and the script dies with "command not found". CI
# always takes the sudo path, which is why this never failed there -- it fails in
# a root container, i.e. exactly the minimal environment this script exists for.
# Unquoted ON PURPOSE: $PACKAGES is a whitespace-separated LIST and word
# splitting is how it becomes several arguments. The values are literals from
# this file, never caller input.
# shellcheck disable=SC2086
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y $PACKAGES

echo
echo "Anamorph: Linux build dependencies installed (profile: ${PROFILE})."
echo "Note: if 'libwebkit2gtk-4.1-dev' is unavailable on your release, try 'libwebkit2gtk-4.0-dev'."
