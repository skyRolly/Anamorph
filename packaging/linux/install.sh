#!/bin/sh
# Anamorph Linux installer — two modes, chosen interactively:
#
#   1) current user (default, no root)  VST3 -> ~/.vst3, Standalone -> ~/.local/bin
#   2) system-wide  (needs root)        VST3 -> /usr/lib/vst3, Standalone -> /usr/local/bin
#
# ~/.vst3 is the VST3 SDK's per-user Linux location and is scanned by default by
# REAPER, Bitwig, Ardour and friends, so the recommended install needs no root at
# all. Only mode 2 elevates, and only for the copy itself — the script is never
# re-executed through sudo. Running it AS root (the documented `sudo ./install.sh`)
# selects mode 2 without asking: root has no meaningful per-user home to install
# into, so that invocation keeps behaving exactly as it did before 0.9.3.
#
# REPLACING AN EXISTING INSTALL is a transaction (see `reconcile` below): the
# replacement is built somewhere else, the old bundle is moved ASIDE rather than
# deleted, and only then is the finished copy renamed into place — so a complete
# copy of the plug-in exists at every instant and any failure or interruption
# leaves a working install behind. The VST3 bundle and the Standalone are two
# separate artifacts, each replaced atomically; they are not one transaction (see
# PACKAGING.md).
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_SRC="$HERE/Anamorph.vst3"
APP_SRC="$HERE/Anamorph"

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"

# Elevation prefix. Stays EMPTY for the per-user path and when already root; only
# system-wide mode below sets it, and only around individual operations.
SUDO=''

[ -d "$VST3_SRC" ] || { echo "error: Anamorph.vst3 not found next to install.sh" >&2; exit 1; }
[ -f "$APP_SRC" ]  || { echo "error: Anamorph (Standalone) not found next to install.sh" >&2; exit 1; }

# ---------------------------------------------------------------- mode choice
mode=user

if [ "$(id -u)" -eq 0 ]; then
    mode=system
    echo "Running as root - installing system-wide (for all users)."
elif [ -t 0 ]; then
    cat <<'EOF'
Anamorph Linux Installer

Choose installation location:

1) Install for current user (recommended)
   ~/.vst3

2) Install system-wide
   /usr/lib/vst3

EOF
    printf 'Select [1/2]: '
    choice=''
    read -r choice || choice=''      # EOF on stdin answers with the default
    case "$choice" in
        ''|1) mode=user   ;;
        2)    mode=system ;;
        *)    echo "Unrecognised answer '$choice' - using the default (current user)."
              mode=user ;;
    esac
else
    echo "Not running on a terminal - installing for the current user (the default)."
fi

# ------------------------------------------------- shared transaction helpers
# Both modes run the identical transaction; only the elevation prefix, the
# failure reporting and the closing summary differ.

# Picks where the replacement is built. Preferred: a hidden directory NEXT TO the
# plug-in directory, so an incomplete bundle never appears inside the path DAWs
# scan. That is only usable when it is on the same filesystem as the destination,
# because every commit below must stay a rename — and ~/.vst3 or /usr/lib/vst3
# may be a symlink onto another mount. So the candidate is not assumed but PROBED
# with a HARD LINK, which is the one operation that cannot cross a filesystem —
# `mv` is no test at all here, since it silently falls back to copy-and-unlink.
# If the probe fails for any reason (different mount, or a filesystem without
# hard links), staging falls back to a hidden directory INSIDE the plug-in
# directory, which is the same filesystem by construction. A false negative is
# therefore harmless: it costs the scan-path property, never the atomicity.
choose_stage_dir() {            # $1 = plug-in directory; prints the stage directory
    _out="${1%/*}/.anamorph-install-stage"
    _in="$1/.anamorph-install-stage"
    _probe="$1/.anamorph-probe"
    # The link target below is the ONE thing this script writes into the scan
    # directory, and `ln` refuses to overwrite an existing target. A run killed
    # between creating and removing it would therefore fail the probe on EVERY
    # later run and pin staging to the in-scan-path fallback permanently — a
    # leftover marker silently deciding future installs. Clearing it up front,
    # unconditionally and before the recovery paths that never probe, makes the
    # marker stateless: a leftover can only ever be litter, never a decision.
    # (`-rf`, not `-f`, so even a directory left under that name cannot pin the
    # choice; nothing but this probe is ever written to that path.)
    # shellcheck disable=SC2086  # $SUDO is deliberately empty outside system mode
    $SUDO rm -rf "$_probe" 2>/dev/null || true
    # A run killed outright (SIGKILL, power loss) can leave the previous bundle
    # parked. Keep using whichever directory holds it, so it stays recoverable.
    if [ -d "$_out/Anamorph.vst3.prev" ]; then printf '%s\n' "$_out"; return 0; fi
    if [ -d "$_in/Anamorph.vst3.prev" ];  then printf '%s\n' "$_in";  return 0; fi
    # shellcheck disable=SC2086
    if $SUDO mkdir -p "$_out" 2>/dev/null \
       && $SUDO touch "$_out/.probe" 2>/dev/null \
       && $SUDO ln "$_out/.probe" "$_probe" 2>/dev/null
    then
        # Both ends go now, so nothing survives a successful probe. The staging
        # and commit below never touch this name again.
        # shellcheck disable=SC2086
        $SUDO rm -f "$_out/.probe" "$_probe" 2>/dev/null || true
        printf '%s\n' "$_out"
    else
        # shellcheck disable=SC2086
        $SUDO rm -rf "$_out" 2>/dev/null || true
        printf '%s\n' "$_in"
    fi
}

# Restores the transaction and clears its scratch. Runs BEFORE the work starts —
# which recovers a bundle parked by a run that was killed before any handler
# could run — and again from the traps below. EXIT alone does not cover
# interruption: dash, /bin/sh on Debian and Ubuntu, does not run EXIT traps when
# the script is terminated by a signal, so INT, TERM and HUP are trapped too.
reconcile() {
    if [ ! -e "$VST3_DEST" ] && [ -d "$PREV_VST3" ]; then
        # shellcheck disable=SC2086
        $SUDO mv "$PREV_VST3" "$VST3_DEST" 2>/dev/null || true
    fi
    # shellcheck disable=SC2086
    $SUDO rm -rf "$STAGE_VST3" "$STAGE_APP" 2>/dev/null || true
    # The parked copy is dropped ONLY once the destination is populated again: if
    # the restore above could not complete it is the last copy, and deleting it
    # here would be the very loss this guards against.
    if [ -e "$VST3_DEST" ]; then
        # shellcheck disable=SC2086
        $SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
        # shellcheck disable=SC2086
        $SUDO rmdir "$STAGE_DIR" 2>/dev/null || true   # only when empty
    fi
}

arm_traps() {
    trap 'reconcile' EXIT
    trap 'reconcile; exit 130' INT
    trap 'reconcile; exit 143' TERM
    trap 'reconcile; exit 129' HUP
}

# ------------------------------------------------------- 1) current-user mode
if [ "$mode" = user ]; then
    [ -n "${HOME:-}" ] || {
        echo "error: HOME is not set, so there is no per-user location to install into." >&2
        echo "       Run 'sudo ./install.sh' for a system-wide install instead." >&2
        exit 1
    }
    VST3_DIR="$HOME/.vst3"
    BIN_DIR="$HOME/.local/bin"
    VST3_DEST="$VST3_DIR/Anamorph.vst3"

    mkdir -p "$VST3_DIR" "$BIN_DIR"
    STAGE_DIR="$(choose_stage_dir "$VST3_DIR")"
    STAGE_VST3="$STAGE_DIR/Anamorph.vst3"
    PREV_VST3="$STAGE_DIR/Anamorph.vst3.prev"
    # The Standalone is staged beside ITS destination: $BIN_DIR can be on a
    # different filesystem from the plug-in directory, and its commit must be a
    # rename too. A bin directory is not a plug-in scan path, so nothing here
    # needs to keep an incomplete copy out of it beyond the leading dot.
    STAGE_APP="$BIN_DIR/.Anamorph.new"

    reconcile
    arm_traps
    # After reconcile, which sweeps an empty stage directory away as scratch.
    mkdir -p "$STAGE_DIR"

    cp -R "$VST3_SRC" "$STAGE_VST3"
    cp "$APP_SRC" "$STAGE_APP"
    chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anamorph.so" 2>/dev/null || true

    [ ! -e "$VST3_DEST" ] || mv "$VST3_DEST" "$PREV_VST3"
    mv "$STAGE_VST3" "$VST3_DEST"
    mv "$STAGE_APP" "$BIN_DIR/Anamorph"
    rm -rf "$PREV_VST3"
    rmdir "$STAGE_DIR" 2>/dev/null || true
    trap - EXIT INT TERM HUP

    echo "Installed (current user only - no root needed):"
    echo "  VST3       -> $VST3_DEST"
    echo "  Standalone -> $BIN_DIR/Anamorph"
    case ":${PATH:-}:" in
        *":$BIN_DIR:"*) ;;
        *) echo "Note: $BIN_DIR is not on your PATH - start the Standalone with the full path above." ;;
    esac
    if [ -e "$SYS_VST3_DIR/Anamorph.vst3" ] || [ -e "$SYS_BIN_DIR/Anamorph" ]; then
        echo "Note: an older SYSTEM-WIDE install is still present. DAWs scan both locations,"
        echo "      so Anamorph can appear twice and the DAW decides which one loads - this"
        echo "      update may look as if it did not apply. Still installed system-wide:"
        if [ -e "$SYS_VST3_DIR/Anamorph.vst3" ]; then echo "        $SYS_VST3_DIR/Anamorph.vst3"; fi
        if [ -e "$SYS_BIN_DIR/Anamorph" ];      then echo "        $SYS_BIN_DIR/Anamorph"; fi
        echo "      Remove it with:  sudo ./uninstall.sh"
    fi
    echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  ./uninstall.sh"
    exit 0
fi

# --------------------------------------------------------- 2) system-wide mode
if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || {
        echo "error: a system-wide install needs root, but 'sudo' is not available." >&2
        echo "       Re-run and choose 1) install for current user (~/.vst3, no root needed)," >&2
        echo "       or run this script as root:  su -c './install.sh'" >&2
        exit 1
    }
    SUDO='sudo'
    echo "System-wide installation requires administrator privileges."
    echo "You may be prompted for your password."
fi

# Elevate the individual install operations only, never the whole script.
priv() {
    # shellcheck disable=SC2086  # $SUDO is deliberately empty when already root
    $SUDO "$@" || {
        echo "System installation failed because permission was denied." >&2
        echo "Try using a user installation or ensure sudo access is available." >&2
        exit 1
    }
}

VST3_DIR="$SYS_VST3_DIR"
BIN_DIR="$SYS_BIN_DIR"
VST3_DEST="$VST3_DIR/Anamorph.vst3"

priv mkdir -p "$VST3_DIR" "$BIN_DIR"
STAGE_DIR="$(choose_stage_dir "$VST3_DIR")"
STAGE_VST3="$STAGE_DIR/Anamorph.vst3"
PREV_VST3="$STAGE_DIR/Anamorph.vst3.prev"
STAGE_APP="$BIN_DIR/.Anamorph.new"

reconcile
arm_traps
# After reconcile, which sweeps an empty stage directory away as scratch.
priv mkdir -p "$STAGE_DIR"

priv cp -R "$VST3_SRC" "$STAGE_VST3"
priv cp "$APP_SRC" "$STAGE_APP"
# shellcheck disable=SC2086
$SUDO chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anamorph.so" 2>/dev/null || true

[ ! -e "$VST3_DEST" ] || priv mv "$VST3_DEST" "$PREV_VST3"
priv mv "$STAGE_VST3" "$VST3_DEST"
priv mv "$STAGE_APP" "$BIN_DIR/Anamorph"
# shellcheck disable=SC2086
$SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
# shellcheck disable=SC2086
$SUDO rmdir "$STAGE_DIR" 2>/dev/null || true
trap - EXIT INT TERM HUP

echo "Installed (system-wide, all users):"
echo "  VST3       -> $VST3_DEST"
echo "  Standalone -> $BIN_DIR/Anamorph"
echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  sudo ./uninstall.sh"
