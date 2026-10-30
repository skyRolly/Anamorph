#!/bin/sh
# Anamorph Linux uninstaller. Two modes, matching the installer:
#
#   1) Current user (default, no root)  ~/.vst3/Anamorph.vst3
#   2) System-wide  (needs root)        /usr/lib/vst3/Anamorph.vst3
#
# Running this script as root (sudo ./uninstall.sh) removes the system-wide
# installation without asking. Your presets and settings are always kept, and so
# is a plug-in copy parked by an interrupted install unless --discard-parked
# says otherwise.
set -eu
HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

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

remove_install_scratch() {          # $1 = plug-in directory
    $SUDO /bin/sh -c '
        set -eu
        owner=$(id -u)
        probe="$1/.anamorph-probe"
        if [ -e "$probe" ] || [ -L "$probe" ]; then
            if [ ! -L "$probe" ] && [ -f "$probe" ] &&
               [ "$(stat -c "%u %s" -- "$probe")" = "$owner 0" ]; then
                rm -- "$probe"
            else printf "warning: unverified staging probe preserved: %s\n" "$probe" >&2; fi
        fi
        for stage in "${1%/*}/.anamorph-install-stage" "$1/.anamorph-install-stage"; do
            [ -e "$stage" ] || [ -L "$stage" ] || continue
            if [ -L "$stage" ] || [ ! -d "$stage" ] ||
               [ "$(stat -c %u -- "$stage")" != "$owner" ]; then
                printf "warning: unverified installer scratch preserved: %s\n" "$stage" >&2
                continue
            fi
            case "$(stat -c %a -- "$stage")" in
                700|[0-7]700) ;;
                *) printf "warning: writable installer scratch preserved: %s\n" "$stage" >&2; continue ;;
            esac
            safe=1
            for entry in "$stage"/* "$stage"/.[!.]* "$stage"/..?*; do
                [ -e "$entry" ] || [ -L "$entry" ] || continue
                case "${entry##*/}" in
                    Anamorph.vst3|Anamorph.vst3.prev)
                        if [ -L "$entry" ] || [ ! -d "$entry" ] ||
                           [ "$(stat -c %u -- "$entry")" != "$owner" ]; then safe=0; fi ;;
                    .probe)
                        if [ -L "$entry" ] || [ ! -f "$entry" ] ||
                           [ "$(stat -c %u -- "$entry")" != "$owner" ]; then safe=0; fi ;;
                    *) safe=0 ;;
                esac
            done
            if [ "$safe" -eq 0 ]; then
                printf "warning: unknown installer scratch preserved: %s\n" "$stage" >&2
                continue
            fi
            if [ -d "$stage/Anamorph.vst3.prev" ] && [ "$2" -eq 0 ]; then
                printf "note: parked plug-in copy kept in %s; retry install.sh in the same mode or use --discard-parked.\n" "$stage"
                continue
            fi
            rm -rf -- "$stage/Anamorph.vst3" "$stage/Anamorph.vst3.prev"
            rm -f -- "$stage/.probe"
            rmdir -- "$stage"
        done
    ' anamorph-install-scratch "$1" "$discard_parked"
}

# BEGIN plugin destination checks
# Check in the identity that will write. User installs never escalate home writes.
check_plugin_destination() {
    $SUDO /bin/sh -c '
        set -eu
        target=$1; mode=$2; owner=$(id -u); part=$(dirname -- "$target")
        fail() { printf "error: unsafe plug-in destination preserved: %s\n" "$1" >&2; exit 1; }
        case "$part" in /*) ;; *) fail "$part" ;; esac
        while [ "$part" != / ]; do
            [ ! -L "$part" ] || fail "$part"
            if [ -e "$part" ]; then
                [ -d "$part" ] || fail "$part"
                meta=$(stat -c "%u %a" -- "$part")
                uid=${meta%% *}; permissions=${meta#* }
                [ "$uid" = 0 ] || [ "$uid" = "$owner" ] || fail "$part"
                if [ "$mode" = system ]; then
                    case "$permissions" in
                        [1357][0-7][0-7][0-7]) ;;
                        *[2367][0-7]|*[2367]) fail "$part" ;;
                    esac
                fi
            fi
            part=$(dirname -- "$part")
        done
        [ ! -L "$target" ] || fail "$target"
        if [ -e "$target" ]; then
            [ -d "$target" ] && [ "$(stat -c %u -- "$target")" = "$owner" ] || fail "$target"
        fi
    ' anamorph-plugin-destination "$1" "$mode"
}
# END plugin destination checks

# BEGIN legacy desktop cleanup
# Migration only: never install desktop resources or change shared permissions.
# This literal code runs as the caller for --user and via sudo for --system.
cleanup_legacy_desktop() {
    if [ "$mode" = system ]; then
        _data=/usr/local/share
        _app=/usr/local/bin/Anamorph
    else
        _data="$HOME/.local/share"
        case "${XDG_DATA_HOME:-}" in /*) _data=$XDG_DATA_HOME ;; esac
        _app="$HOME/.local/bin/Anamorph"
    fi
    $SUDO /bin/sh -c '
        set -eu
        set -f
        data=$1; app=$2; system=$3; owner=$(id -u)
        warn() { printf "warning: legacy Anamorph resources preserved: %s\n" "$*" >&2; }
        present() { [ -e "$1" ] || [ -L "$1" ]; }
        hash() { sha256sum < "$1" | cut -d " " -f 1; }
        parents() {
            part=$1
            case "$part" in /*) ;; *) return 1 ;; esac
            if printf "%s" "$part" | LC_ALL=C grep -q "[[:cntrl:]]"; then return 1; fi
            while [ "$part" != / ]; do
                [ ! -L "$part" ] || return 1
                if [ -e "$part" ]; then
                    [ -d "$part" ] || return 1
                    meta=$(stat -c "%u %a" -- "$part") || return 1
                    uid=${meta%% *}; permissions=${meta#* }
                    [ "$uid" = 0 ] || [ "$uid" = "$owner" ] || return 1
                    if [ "$system" = system ]; then
                        case "$permissions" in
                            [1357][0-7][0-7][0-7]) ;;
                            *[2367][0-7]|*[2367]) return 1 ;;
                        esac
                    fi
                fi
                part=$(dirname -- "$part")
            done
        }
        owned_file() {
            [ -f "$1" ] && [ ! -L "$1" ] &&
                [ "$(stat -c "%u %h" -- "$1")" = "$owner 1" ]
        }
        paths="applications/tech.rolly.anamorph.desktop
icons/hicolor/16x16/apps/tech.rolly.anamorph.png
icons/hicolor/24x24/apps/tech.rolly.anamorph.png
icons/hicolor/32x32/apps/tech.rolly.anamorph.png
icons/hicolor/48x48/apps/tech.rolly.anamorph.png
icons/hicolor/64x64/apps/tech.rolly.anamorph.png
icons/hicolor/128x128/apps/tech.rolly.anamorph.png
icons/hicolor/256x256/apps/tech.rolly.anamorph.png
icons/hicolor/512x512/apps/tech.rolly.anamorph.png
icons/hicolor/scalable/apps/tech.rolly.anamorph.svg"
        clean_resources() {
            parents "$data" || { warn "unsafe data-root ancestry"; return; }
            data=$(realpath -ms -- "$data")
            state="$data/.tech.rolly.anamorph"
            receipt="$state/receipt"
            present "$state" || return 0
            if [ -L "$state" ] || [ ! -d "$state" ] ||
               [ "$(stat -c "%u %a" -- "$state")" != "$owner 700" ]; then
                warn "unverified ownership state"; return
            fi
            # A killed cleanup may have removed the receipt already. Only an
            # empty checked format marker is then eligible for exact rmdir.
            if present "$receipt"; then
                if ! owned_file "$receipt" ||
                   [ "$(head -n 1 -- "$receipt")" != "tech.rolly.anamorph desktop resources v1" ] ||
                   [ "$(tail -n +2 -- "$receipt" | awk "{print \$2}" | LC_ALL=C sort)" != "$(printf "%s\n" "$paths" | LC_ALL=C sort)" ] ||
                   ! tail -n +2 -- "$receipt" | awk "NF != 2 || length(\$1) != 64 || \$1 ~ /[^0-9a-f]/ {bad=1} END {exit bad}"; then
                    warn "unverified ownership receipt"; return
                fi
                remaining=0
                for relative in $paths; do
                    target="$data/$relative"
                    present "$target" || continue
                    digest=$(awk -v path="$relative" "\$2 == path {print \$1}" "$receipt")
                    if parents "$(dirname -- "$target")" && owned_file "$target" &&
                       [ "$(hash "$target")" = "$digest" ]; then
                        if ! rm -- "$target"; then remaining=1; warn "$relative"; fi
                    else remaining=1; warn "$relative"; fi
                done
                [ "$remaining" -eq 0 ] || return 0
                # Unknown/interrupted journals are retained with their receipt;
                # their mere marker is not deletion authority.
                known=$(find "$state" -mindepth 1 -maxdepth 1 -printf "%f\n" | LC_ALL=C sort)
                if [ "$known" != "$(printf "%s\n" format-v1 receipt | LC_ALL=C sort)" ]; then
                    warn "private recovery state requires review"; return
                fi
            else
                for relative in $paths; do
                    if present "$data/$relative"; then warn "resource without a receipt"; return; fi
                done
                if [ "$(ls -A -- "$state")" != format-v1 ]; then
                    warn "state without a receipt"; return
                fi
            fi
            marker="$state/format-v1"
            if [ -L "$marker" ] || [ ! -d "$marker" ] ||
               [ "$(stat -c "%u %a" -- "$marker")" != "$owner 700" ] ||
               [ -n "$(ls -A -- "$marker")" ]; then
                warn "unverified format marker"; return
            fi
            if present "$receipt"; then rm -- "$receipt"; fi
            rmdir -- "$marker"
            rmdir -- "$state"
        }
        clean_app() {
            app=$1
            present "$app" || return 0
            if ! parents "$(dirname -- "$app")" || ! owned_file "$app"; then
                warn "unverified standalone file"; return
            fi
            digest=$(hash "$app")
            # Exact executable from the existing 0.10.0 Linux release asset.
            if [ "$digest" = 4beedffbcedbe6c14b0a9ecb2c0c51110b32c10ed8de477bd9e9f9525811ca8e ]; then
                rm -- "$app"
            else warn "standalone content differs from a verified package"; fi
        }
        clean_resources
        clean_app "$app"
        clean_app "${app%/*}/.Anamorph.new"
    ' anamorph-legacy-cleanup "$_data" "$_app" "$mode" || {
        printf 'warning: legacy Anamorph cleanup incomplete; unverified files are kept.\n' >&2
    }
}
# END legacy desktop cleanup

# ------------------------------------------------------- 1) current-user mode
if [ "$mode" = user ]; then
    [ -n "${HOME:-}" ] || {
        echo "error: HOME is not set, so there is no per-user installation to remove." >&2
        echo "       Run 'sudo ./uninstall.sh' to remove a system-wide install instead." >&2
        exit 1
    }
    VST3="$HOME/.vst3/Anamorph.vst3"

    check_plugin_destination "$VST3"
    cleanup_legacy_desktop
    if [ -d "$VST3" ]; then rm -rf "$VST3"; echo "removed $VST3"; removed=1; fi
    remove_install_scratch "$HOME/.vst3"

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

check_plugin_destination "$SYS_VST3"
cleanup_legacy_desktop
if [ -d "$SYS_VST3" ]; then priv rm -rf "$SYS_VST3"; echo "removed $SYS_VST3"; removed=1; fi
remove_install_scratch "$SYS_VST3_DIR"

[ "$removed" -eq 1 ] || echo "nothing to remove system-wide (a per-user install is removed with:  ./uninstall.sh)"
echo "Per-user presets/settings (if any) are kept; remove them manually if desired."
