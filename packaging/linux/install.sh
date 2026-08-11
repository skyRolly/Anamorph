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
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_SRC="$HERE/Anamorph.vst3"
APP_SRC="$HERE/Anamorph"

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"

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
    STAGE_VST3="$VST3_DIR/.Anamorph.vst3.new"
    STAGE_APP="$BIN_DIR/.Anamorph.new"
    PREV_VST3="$VST3_DIR/.Anamorph.vst3.prev"

    # Stage beside the destination, then swap. Copying straight over the
    # installed plug-in would destroy a working install the moment the copy
    # failed (no space, unreadable payload, an interrupted run); staging keeps
    # the old one until the new one is complete. Staging next to the
    # destination, not in /tmp, keeps every swap step a same-filesystem rename,
    # which cannot fail for space and replaces a RUNNING Standalone that `cp`
    # would refuse with "Text file busy".
    #
    # The swap moves the old bundle ASIDE rather than deleting it, so a COMPLETE
    # copy exists at every instant: the old one in place, then the old one parked
    # next to it while the finished replacement is renamed in, then the new one.
    # Deleting first would open a window in which the destination is empty and
    # the staged copy is the only one -- and the cleanup below would then remove
    # that too, leaving nothing installed.
    #
    # `reconcile` puts the parked copy back if anything stops the run mid-swap.
    # It also runs BEFORE the work starts, which clears scratch (and recovers a
    # parked bundle) left by an earlier run killed with a signal no trap can
    # catch. EXIT alone is not enough to cover interruption: dash -- /bin/sh on
    # Debian and Ubuntu -- does not run EXIT traps when the script is terminated
    # by a signal, so INT, TERM and HUP are trapped explicitly.
    reconcile() {
        if [ ! -e "$VST3_DEST" ] && [ -d "$PREV_VST3" ]; then
            mv "$PREV_VST3" "$VST3_DEST" 2>/dev/null || true
        fi
        rm -rf "$STAGE_VST3" "$STAGE_APP" 2>/dev/null || true
        # The parked copy is dropped ONLY once the destination is populated
        # again: if the restore above could not complete, it is the only copy
        # left and deleting it here would be the very loss this guards against.
        [ ! -e "$VST3_DEST" ] || rm -rf "$PREV_VST3" 2>/dev/null || true
    }

    mkdir -p "$VST3_DIR" "$BIN_DIR"
    reconcile
    trap 'reconcile' EXIT
    trap 'reconcile; exit 130' INT
    trap 'reconcile; exit 143' TERM
    trap 'reconcile; exit 129' HUP

    cp -R "$VST3_SRC" "$STAGE_VST3"
    cp "$APP_SRC" "$STAGE_APP"
    chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anamorph.so" 2>/dev/null || true

    [ ! -e "$VST3_DEST" ] || mv "$VST3_DEST" "$PREV_VST3"
    mv "$STAGE_VST3" "$VST3_DEST"
    mv "$STAGE_APP" "$BIN_DIR/Anamorph"
    rm -rf "$PREV_VST3"
    trap - EXIT INT TERM HUP

    echo "Installed (current user only - no root needed):"
    echo "  VST3       -> $VST3_DIR/Anamorph.vst3"
    echo "  Standalone -> $BIN_DIR/Anamorph"
    case ":${PATH:-}:" in
        *":$BIN_DIR:"*) ;;
        *) echo "Note: $BIN_DIR is not on your PATH - start the Standalone with the full path above." ;;
    esac
    echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  ./uninstall.sh"
    exit 0
fi

# --------------------------------------------------------- 2) system-wide mode
SUDO=''
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

SYS_VST3_DEST="$SYS_VST3_DIR/Anamorph.vst3"
STAGE_VST3="$SYS_VST3_DIR/.Anamorph.vst3.new"
STAGE_APP="$SYS_BIN_DIR/.Anamorph.new"
PREV_VST3="$SYS_VST3_DIR/.Anamorph.vst3.prev"

# Stage beside the destination, then swap — same reasoning, and the same
# move-aside-rather-than-delete ordering, as the per-user path above.
sys_reconcile() {
    if [ ! -e "$SYS_VST3_DEST" ] && [ -d "$PREV_VST3" ]; then
        # shellcheck disable=SC2086  # $SUDO is deliberately empty when already root
        $SUDO mv "$PREV_VST3" "$SYS_VST3_DEST" 2>/dev/null || true
    fi
    # shellcheck disable=SC2086
    $SUDO rm -rf "$STAGE_VST3" "$STAGE_APP" 2>/dev/null || true
    # Parked copy dropped only once the destination is populated again — see the
    # per-user reconcile above.
    # shellcheck disable=SC2086
    [ ! -e "$SYS_VST3_DEST" ] || $SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
}

priv mkdir -p "$SYS_VST3_DIR" "$SYS_BIN_DIR"
sys_reconcile
trap 'sys_reconcile' EXIT
trap 'sys_reconcile; exit 130' INT
trap 'sys_reconcile; exit 143' TERM
trap 'sys_reconcile; exit 129' HUP

priv cp -R "$VST3_SRC" "$STAGE_VST3"
priv cp "$APP_SRC" "$STAGE_APP"
# shellcheck disable=SC2086
$SUDO chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anamorph.so" 2>/dev/null || true

[ ! -e "$SYS_VST3_DEST" ] || priv mv "$SYS_VST3_DEST" "$PREV_VST3"
priv mv "$STAGE_VST3" "$SYS_VST3_DEST"
priv mv "$STAGE_APP" "$SYS_BIN_DIR/Anamorph"
# shellcheck disable=SC2086
$SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
trap - EXIT INT TERM HUP

echo "Installed (system-wide, all users):"
echo "  VST3       -> $SYS_VST3_DIR/Anamorph.vst3"
echo "  Standalone -> $SYS_BIN_DIR/Anamorph"
echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  sudo ./uninstall.sh"
