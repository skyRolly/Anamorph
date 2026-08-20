#!/bin/sh
# Anamorph Linux uninstaller. Two modes, matching the installer:
#
#   1) Current user (default, no root)  ~/.vst3/Anamorph.vst3, ~/.local/bin/Anamorph
#   2) System-wide  (needs root)        /usr/lib/vst3/Anamorph.vst3, /usr/local/bin/Anamorph
#
# Running this script as root (sudo ./uninstall.sh) removes the system-wide
# installation without asking. Your presets and settings are always kept, and so
# is a plug-in copy parked by an interrupted install unless --discard-parked
# says otherwise.
set -eu

# --user / --system mirror install.sh, for a run with no terminal to ask on.
# --discard-parked is the opt-in for the one thing this script keeps by default:
# a copy of your previous plug-in that an interrupted install set aside.
discard_parked=0
requested=''
for arg in "$@"; do
    case "$arg" in
        --user|--system)
            _want=${arg#--}
            if [ -n "$requested" ] && [ "$requested" != "$_want" ]; then
                echo "error: --user and --system name different installations; pass one." >&2
                exit 1
            fi
            requested=$_want ;;
        --discard-parked) discard_parked=1 ;;
        -h|--help)
            echo "usage: ./uninstall.sh [--user|--system] [--discard-parked]"
            echo "  --user            remove the current user's install (~/.vst3) - the default"
            echo "  --system          remove the system-wide install (/usr/lib/vst3) - needs root"
            echo "  --discard-parked  also delete a plug-in copy parked by an"
            echo "                    interrupted install (default: keep it)"
            echo "Repeating an option is accepted; --user and --system together are not."
            echo "With no mode option, an interactive run asks; a non-interactive one"
            echo "removes the current user's install."
            exit 0 ;;
        *)
            echo "error: unrecognised option '$arg'" >&2
            echo "usage: ./uninstall.sh [--user|--system] [--discard-parked]" >&2
            exit 1 ;;
    esac
done

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"
SYS_VST3="$SYS_VST3_DIR/Anamorph.vst3"
SYS_APP="$SYS_BIN_DIR/Anamorph"

# Only a system-wide uninstall sets this; a per-user uninstall never uses sudo.
SUDO=''

# ---------------------------------------------------------------- mode choice
mode=user

if [ "$(id -u)" -eq 0 ]; then
    # Refused for the installer's reason, and here the unpredictable operation
    # would be a deletion: which home $HOME names under sudo depends on the
    # sudoers configuration.
    if [ "$requested" = user ]; then
        echo "error: --user cannot be combined with running as root: which home directory" >&2
        echo "       \$HOME names under sudo depends on the sudoers configuration, so the" >&2
        echo "       files removed would not be predictable." >&2
        echo "       Re-run without sudo to remove a per-user install." >&2
        exit 1
    fi
    mode=system
    echo "Running as root - removing the system-wide installation."
elif [ -n "$requested" ]; then
    mode="$requested"
    if [ "$mode" = system ]; then
        echo "Removing the system-wide installation (--system)."
    else
        echo "Removing the current user's installation (--user)."
    fi
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
    echo "Pass --system to remove the system-wide installation instead."
fi

removed=0

# Clears temporary files an interrupted install may have left behind. Only the
# exact names the installer creates are removed; nothing else is touched. This
# list must match the candidates install.sh stages into, or an interrupted
# install would survive a deliberate uninstall.
remove_install_scratch() {          # $1 = plug-in directory, $2 = bin directory
    for _scratch in "${1%/*}/.anamorph-install-stage" \
                    "$1/.anamorph-install-stage" \
                    "$1/.anamorph-probe" \
                    "$2/.Anamorph.new"
    do
        # -L first: -e follows a symlink, so a dangling one wearing a scratch
        # name would read as absent and never be cleaned.
        if [ -L "$_scratch" ] || [ -e "$_scratch" ]; then
            # A copy of your previous plug-in parked by an interrupted install is
            # kept, not swept up: install.sh is the only thing that can put it
            # back, and this may be the only copy of that version you still have.
            if $SUDO test -d "$_scratch/Anamorph.vst3.prev" && [ "$discard_parked" -eq 0 ]; then
                # Name the mode in the advice: a plain './install.sh' defaults to
                # a per-user install and would never look in the system-wide
                # staging directory this copy may be sitting in.
                if [ "$mode" = system ]; then
                    _restore="./install.sh --system"
                else
                    _restore="./install.sh"
                fi
                echo "note: $_scratch holds a saved copy of your previous Anamorph,"
                echo "      parked there by an interrupted install. It is the only copy of that"
                echo "      version, so it is being KEPT rather than removed."
                echo "      Run '$_restore' to put it back - the mode matters, since each one"
                echo "      only looks in its own staging directory. Or re-run this script with"
                echo "      --discard-parked to delete it along with the rest of the scratch."
                # The marker file beside it is scratch either way, and this is the
                # one path that leaves the directory around it standing.
                $SUDO rm -f "$_scratch/.probe" 2>/dev/null || true
                # Nothing the user asked about was removed, so this does not count
                # towards the summary below.
                continue
            fi
            # Never fatal: leftover scratch is cosmetic, and the plug-in itself
            # has already been removed by the time this runs.
            if $SUDO rm -rf "$_scratch" 2>/dev/null; then
                echo "removed leftover installer file $_scratch"
                removed=1
            else
                echo "note: could not remove $_scratch (left in place)" >&2
            fi
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

# Only the individual removals are elevated, never the whole script.
priv() {
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
