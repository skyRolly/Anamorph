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
Anamorph Linux Plugin Installer

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

    mkdir -p "$VST3_DIR" "$BIN_DIR"
    rm -rf "$VST3_DIR/Anamorph.vst3"
    cp -R "$VST3_SRC" "$VST3_DIR/"
    cp "$APP_SRC" "$BIN_DIR/Anamorph"
    chmod 755 "$BIN_DIR/Anamorph" "$VST3_DIR/Anamorph.vst3/Contents/x86_64-linux/Anamorph.so" 2>/dev/null || true

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

priv mkdir -p "$SYS_VST3_DIR" "$SYS_BIN_DIR"
priv rm -rf "$SYS_VST3_DIR/Anamorph.vst3"
priv cp -R "$VST3_SRC" "$SYS_VST3_DIR/"
priv cp "$APP_SRC" "$SYS_BIN_DIR/Anamorph"
# shellcheck disable=SC2086
$SUDO chmod 755 "$SYS_BIN_DIR/Anamorph" "$SYS_VST3_DIR/Anamorph.vst3/Contents/x86_64-linux/Anamorph.so" 2>/dev/null || true

echo "Installed (system-wide, all users):"
echo "  VST3       -> $SYS_VST3_DIR/Anamorph.vst3"
echo "  Standalone -> $SYS_BIN_DIR/Anamorph"
echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  sudo ./uninstall.sh"
