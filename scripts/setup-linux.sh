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
#  libegl-dev: JUCE 9 creates Linux OpenGL contexts via EGL instead of GLX
#  (juce_opengl linuxPackages "egl gl"), so EGL headers are a build dependency
#  even though Anamorph never attaches a GL context on Linux (ADR-0011).
# ============================================================================
set -euo pipefail

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

$SUDO apt-get update -y

$SUDO DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential cmake git ninja-build pkg-config \
    libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
    libfreetype6-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libglu1-mesa-dev mesa-common-dev libegl-dev \
    libwebkit2gtk-4.1-dev libgtk-3-dev \
    xvfb

echo
echo "Anamorph: Linux build dependencies installed."
echo "Note: if 'libwebkit2gtk-4.1-dev' is unavailable on your release, try 'libwebkit2gtk-4.0-dev'."
