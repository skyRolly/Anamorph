#!/bin/sh
# Anamorph Linux installer. Two modes, chosen interactively:
#
#   1) Current user (default, no root)  VST3 -> ~/.vst3, Standalone -> ~/.local/bin
#   2) System-wide  (needs root)        VST3 -> /usr/lib/vst3, Standalone -> /usr/local/bin
#
# ~/.vst3 is the standard per-user VST3 folder and is scanned by most DAWs, so the
# recommended install needs no root at all. Running this script as root
# (sudo ./install.sh) installs system-wide without asking.
#
# An existing installation is replaced safely rather than overwritten: the previous
# version is kept until the new one is fully in place, so an interrupted install
# still leaves a working Anamorph behind and the next run recovers on its own. The
# VST3 plug-in and the Standalone application are separate files and are replaced
# independently of each other.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_SRC="$HERE/Anamorph.vst3"
APP_SRC="$HERE/Anamorph"

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"

# Only a system-wide install sets this; a per-user install never uses sudo.
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

# ------------------------------------------------------------ install helpers
# Both modes install the same way; only the elevation and the summary differ.

# Chooses a temporary working directory for the new version, kept out of the
# folder your DAW scans whenever that is possible.
choose_stage_dir() {            # $1 = plug-in directory; prints the stage directory
    _out="${1%/*}/.anamorph-install-stage"
    _in="$1/.anamorph-install-stage"
    _probe="$1/.anamorph-probe"
    $SUDO rm -rf "$_probe" 2>/dev/null || true
    # An earlier install may have left the previous version parked here; keep
    # using whichever directory holds it so it stays recoverable.
    if [ -d "$_out/Anamorph.vst3.prev" ]; then printf '%s\n' "$_out"; return 0; fi
    if [ -d "$_in/Anamorph.vst3.prev" ];  then printf '%s\n' "$_in";  return 0; fi
    if $SUDO mkdir -p "$_out" 2>/dev/null \
       && $SUDO touch "$_out/.probe" 2>/dev/null \
       && $SUDO ln "$_out/.probe" "$_probe" 2>/dev/null
    then
        $SUDO rm -f "$_out/.probe" "$_probe" 2>/dev/null || true
        printf '%s\n' "$_out"
    else
        $SUDO rm -rf "$_out" 2>/dev/null || true
        printf '%s\n' "$_in"
    fi
}

# Puts back the previous version if an install was interrupted, then clears the
# temporary files. Runs once before installing and again if the script is stopped.
reconcile() {
    if [ ! -e "$VST3_DEST" ] && [ -d "$PREV_VST3" ]; then
        $SUDO mv "$PREV_VST3" "$VST3_DEST" 2>/dev/null || true
    fi
    $SUDO rm -rf "$STAGE_VST3" "$STAGE_APP" 2>/dev/null || true
    # The kept copy is only discarded once the plug-in is back in place.
    if [ -e "$VST3_DEST" ]; then
        $SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
        $SUDO rmdir "$STAGE_DIR" 2>/dev/null || true
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
    STAGE_APP="$BIN_DIR/.Anamorph.new"

    reconcile
    arm_traps
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
        echo "Note: an older system-wide install is still present, so Anamorph may appear"
        echo "      twice in your DAW and the older copy may load instead:"
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

# Only the individual install steps are elevated, never the whole script.
priv() {
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
priv mkdir -p "$STAGE_DIR"

priv cp -R "$VST3_SRC" "$STAGE_VST3"
priv cp "$APP_SRC" "$STAGE_APP"
$SUDO chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anamorph.so" 2>/dev/null || true

[ ! -e "$VST3_DEST" ] || priv mv "$VST3_DEST" "$PREV_VST3"
priv mv "$STAGE_VST3" "$VST3_DEST"
priv mv "$STAGE_APP" "$BIN_DIR/Anamorph"
$SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
$SUDO rmdir "$STAGE_DIR" 2>/dev/null || true
trap - EXIT INT TERM HUP

echo "Installed (system-wide, all users):"
echo "  VST3       -> $VST3_DEST"
echo "  Standalone -> $BIN_DIR/Anamorph"
echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  sudo ./uninstall.sh"
