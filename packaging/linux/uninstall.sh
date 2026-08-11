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

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"
SYS_VST3="$SYS_VST3_DIR/Anamorph.vst3"
SYS_APP="$SYS_BIN_DIR/Anamorph"

# Elevation prefix. Stays EMPTY for the per-user path and when already root; only
# system-wide mode below sets it, and only around individual operations.
SUDO=''

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

# Removes the transaction scratch install.sh owns, so a deliberate uninstall does
# not leave behind what an interrupted install left. Only the exact names the
# installer creates are touched: its stage directory (either of the two places
# install.sh may pick — see `choose_stage_dir` there, which prefers a hidden
# directory next to the plug-in directory and falls back to one inside it) and
# the staged Standalone. A parked bundle inside the stage directory goes with it;
# uninstalling means removing Anamorph, so there is nothing left to recover to.
# `$SUDO` is empty in per-user mode. Nothing here matches on a pattern.
remove_install_scratch() {          # $1 = plug-in directory, $2 = bin directory
    for _scratch in "${1%/*}/.anamorph-install-stage" \
                    "$1/.anamorph-install-stage" \
                    "$2/.Anamorph.new"
    do
        if [ -e "$_scratch" ]; then
            # shellcheck disable=SC2086  # $SUDO is deliberately empty outside system mode
            $SUDO rm -rf "$_scratch" && echo "removed leftover installer scratch $_scratch"
        fi
    done
}

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
    remove_install_scratch "$HOME/.vst3" "$HOME/.local/bin"

    [ "$removed" -eq 1 ] || echo "nothing to remove for this user (a system-wide install is removed with:  sudo ./uninstall.sh)"
    echo "Per-user presets/settings (if any) are kept; remove them manually if desired."
    exit 0
fi

# --------------------------------------------------------- 2) system-wide mode
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
remove_install_scratch "$SYS_VST3_DIR" "$SYS_BIN_DIR"

[ "$removed" -eq 1 ] || echo "nothing to remove system-wide (a per-user install is removed with:  ./uninstall.sh)"
echo "Per-user presets/settings (if any) are kept; remove them manually if desired."
