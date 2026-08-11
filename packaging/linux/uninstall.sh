#!/bin/sh
# Removes what packaging/linux/install.sh installed — the same two modes, so a
# per-user install can be removed without root:
#
#   1) current user (default, no root)  ~/.vst3/Anamorph.vst3, ~/.local/bin/Anamorph
#   2) system-wide  (needs root)        /usr/lib/vst3/Anamorph.vst3, /usr/local/bin/Anamorph
#
# Running it AS root (the documented `sudo ./uninstall.sh`) removes the
# system-wide install without asking, exactly as before 0.9.3.
set -eu

SYS_VST3="/usr/lib/vst3/Anamorph.vst3"
SYS_APP="/usr/local/bin/Anamorph"

# ---------------------------------------------------------------- mode choice
mode=user

if [ "$(id -u)" -eq 0 ]; then
    mode=system
    echo "Running as root - removing the system-wide installation."
elif [ -t 0 ]; then
    cat <<'EOF'
Anamorph Linux Uninstaller

Choose which installation to remove:

1) Current user installation (recommended)
   ~/.vst3

2) System-wide installation
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
    echo "Not running on a terminal - removing the current user's installation (the default)."
fi

removed=0

# ------------------------------------------------------- 1) current-user mode
if [ "$mode" = user ]; then
    [ -n "${HOME:-}" ] || {
        echo "error: HOME is not set, so there is no per-user installation to remove." >&2
        echo "       Run 'sudo ./uninstall.sh' to remove a system-wide install instead." >&2
        exit 1
    }
    VST3="$HOME/.vst3/Anamorph.vst3"
    APP="$HOME/.local/bin/Anamorph"

    if [ -d "$VST3" ]; then rm -rf "$VST3"; echo "removed $VST3"; removed=1; fi
    if [ -f "$APP" ];  then rm -f  "$APP";  echo "removed $APP";  removed=1; fi

    [ "$removed" -eq 1 ] || echo "nothing to remove for this user (a system-wide install is removed with:  sudo ./uninstall.sh)"
    echo "Per-user presets/settings (if any) are kept; remove them manually if desired."
    exit 0
fi

# --------------------------------------------------------- 2) system-wide mode
SUDO=''
if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || {
        echo "error: removing a system-wide install needs root, but 'sudo' is not available." >&2
        echo "       Run this script as root instead:  su -c './uninstall.sh'" >&2
        exit 1
    }
    SUDO='sudo'
    echo "Removing the system-wide installation requires administrator privileges."
    echo "You may be prompted for your password."
fi

# Elevate the individual removals only, never the whole script.
priv() {
    # shellcheck disable=SC2086  # $SUDO is deliberately empty when already root
    $SUDO "$@" || {
        echo "System uninstall failed because permission was denied." >&2
        echo "Ensure sudo access is available, or run this script as root." >&2
        exit 1
    }
}

if [ -d "$SYS_VST3" ]; then priv rm -rf "$SYS_VST3"; echo "removed $SYS_VST3"; removed=1; fi
if [ -f "$SYS_APP" ];  then priv rm -f  "$SYS_APP";  echo "removed $SYS_APP";  removed=1; fi

[ "$removed" -eq 1 ] || echo "nothing to remove system-wide (a per-user install is removed with:  ./uninstall.sh)"
echo "Per-user presets/settings (if any) are kept; remove them manually if desired."
