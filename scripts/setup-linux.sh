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
set -euo pipefail

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

$SUDO apt-get update -y

# `env` carries the assignment whether $SUDO is "sudo" or empty (running as
# root). A bare `$SUDO DEBIAN_FRONTEND=... apt-get` breaks in the root case: the
# assignment is not in assignment position at parse time (the first word is
# $SUDO), so when $SUDO expands to nothing `DEBIAN_FRONTEND=noninteractive`
# becomes the COMMAND NAME and the script dies with "command not found". CI
# always takes the sudo path, which is why this never failed there -- it fails in
# a root container, i.e. exactly the minimal environment this script exists for.
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential cmake git ninja-build pkg-config \
    curl unzip \
    libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
    libfreetype6-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libglu1-mesa-dev mesa-common-dev libegl-dev \
    libwebkit2gtk-4.1-dev libgtk-3-dev \
    xvfb \
    lld

echo
echo "Anamorph: Linux build dependencies installed."
echo "Note: if 'libwebkit2gtk-4.1-dev' is unavailable on your release, try 'libwebkit2gtk-4.0-dev'."
