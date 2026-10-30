#!/bin/sh
# Anamorph Linux installer. Two modes - `--user` / `--system`, or chosen
# interactively when neither is given and stdin is a terminal:
#
#   1) Current user (default, no root)  VST3 -> ~/.vst3
#   2) System-wide  (needs root)        VST3 -> /usr/lib/vst3
#
# ~/.vst3 is the standard per-user VST3 folder and is scanned by most DAWs, so the
# recommended install needs no root at all. Running this script as root
# (sudo ./install.sh) installs system-wide without asking.
#
# An existing installation is replaced safely rather than overwritten: the previous
# version is kept until the new one is fully in place, so an interrupted install
# still leaves a working Anamorph behind and the next run recovers on its own. The
# Standalone remains portable in the archive and is not installed.
set -eu
umask 022

HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
VST3_SRC="$HERE/Anamorph.vst3"

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"

# Only a system-wide install sets this; a per-user install never uses sudo.
SUDO=''

[ -d "$VST3_SRC" ] || { echo "error: Anamorph.vst3 not found next to install.sh" >&2; exit 1; }

# ---------------------------------------------------------------- mode choice
# --user / --system answer the prompt below up front, which is the only way to
# choose when there is no terminal to ask on (a provisioning script, a CI step).
# Passing both is an error rather than a silent choice, since they differ in
# destination and in privilege; repeating one is fine.
requested=''
for arg in "$@"; do
    case "$arg" in
        --user|--system)
            _want=${arg#--}
            if [ -n "$requested" ] && [ "$requested" != "$_want" ]; then
                echo "error: --user and --system ask for different installs; pass one." >&2
                echo "       They differ in destination and in privilege, so there is no" >&2
                echo "       sensible way to honour both." >&2
                exit 1
            fi
            requested=$_want ;;
        -h|--help)
            echo "usage: ./install.sh [--user|--system]"
            echo "  --user    install for the current user (~/.vst3) - the default"
            echo "  --system  install for all users (/usr/lib/vst3) - needs root"
            echo "With neither option, an interactive run asks; a non-interactive one"
            echo "installs for the current user."
            exit 0 ;;
        *)
            echo "error: unrecognised option '$arg'" >&2
            echo "usage: ./install.sh [--user|--system]" >&2
            exit 1 ;;
    esac
done

mode=user

if [ "$(id -u)" -eq 0 ]; then
    # Under sudo, which home directory $HOME names depends on the sudoers
    # configuration, so a per-user install run as root could land in /root or in
    # your own folder owned by root. Neither is what --user asked for.
    if [ "$requested" = user ]; then
        echo "error: --user cannot be combined with running as root: which home directory" >&2
        echo "       \$HOME names under sudo depends on the sudoers configuration, so the" >&2
        echo "       install location would not be predictable." >&2
        echo "       Re-run without sudo for a per-user install." >&2
        exit 1
    fi
    mode=system
    echo "Running as root - installing system-wide (for all users)."
elif [ -n "$requested" ]; then
    mode="$requested"
    if [ "$mode" = system ]; then
        echo "Installing system-wide (--system)."
    else
        echo "Installing for the current user (--user)."
    fi
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
    echo "Pass --system to install for all users instead."
fi

# ------------------------------------------------------------ install helpers
# Both modes install the same way; only the elevation and the summary differ.

# A staging directory is reused only if this run could have created it: a real
# directory, owned by the account whose writes land in the plug-in folder, and
# not writable by anyone else. Anything it cannot establish is refused.
stage_dir_is_adoptable() {      # $1 = candidate, $2 = uid that must own it
    [ -L "$1" ] && return 1     # a symlink could aim the whole install elsewhere
    [ -e "$1" ] || return 0     # absent - we are about to create it ourselves
    [ -d "$1" ] || return 1     # a file wearing the directory's name
    _st=$(LC_ALL=C stat -c '%u %a' "$1" 2>/dev/null) || return 1
    [ "${_st%% *}" = "$2" ] || return 1
    case "${_st##* }" in
        *[2367][0-7]) return 1 ;;   # group-writable
        *[2367])      return 1 ;;   # other-writable
    esac
    return 0
}

# Creates the staging directory private to the installing account whatever the
# umask would otherwise have made it, so the next run can adopt it again.
make_stage_dir() {              # $1 = directory
    $SUDO mkdir -p "$1" 2>/dev/null || return 1
    $SUDO chmod 700 "$1" 2>/dev/null || return 1
    return 0
}

# The new version is moved into place at the end, and a move is only safe within
# one filesystem - across one it becomes a copy, which is the partly-written
# plug-in this script exists to avoid. A hard link into the plug-in folder tests
# that in one step.
stage_dir_is_same_filesystem() {    # $1 = candidate, $2 = probe path in the destination
    $SUDO rm -f "$1/.probe" "$2" 2>/dev/null || true
    if $SUDO touch "$1/.probe" 2>/dev/null && $SUDO ln "$1/.probe" "$2" 2>/dev/null; then
        $SUDO rm -f "$1/.probe" "$2" 2>/dev/null || true
        return 0
    fi
    $SUDO rm -f "$1/.probe" "$2" 2>/dev/null || true
    return 1
}

# Chooses a temporary working directory for the new version, kept out of the
# folder your DAW scans whenever that is possible. Prints it and returns 0, or
# returns non-zero having printed nothing: no trustworthy directory means no
# install, rather than staging a copy somewhere another account can reach it.
# The two candidates tried here are the two `uninstall.sh` cleans up.
choose_stage_dir() {            # $1 = plug-in directory; prints the stage directory
    _probe="$1/.anamorph-probe"
    if [ -L "$_probe" ] || [ -d "$_probe" ]; then
        echo "error: occupied staging probe preserved: $_probe" >&2
        return 1
    fi
    $SUDO rm -f "$_probe" 2>/dev/null || return 1
    # Who must own the directory: root when the copy is written with elevation,
    # the invoking user otherwise.
    if [ "$mode" = system ]; then _owner=0; else _owner=$(id -u); fi

    # An earlier install may have parked the previous version in either
    # candidate; whichever holds it has to be the one used, or it cannot be put
    # back. Same two tests as creating one.
    for _c in "${1%/*}/.anamorph-install-stage" "$1/.anamorph-install-stage"; do
        $SUDO test -d "$_c/Anamorph.vst3.prev" || continue
        if stage_dir_is_adoptable "$_c" "$_owner" \
           && stage_dir_is_same_filesystem "$_c" "$_probe"; then
            printf '%s\n' "$_c"
            return 0
        fi
        echo "warning: a previous version of the plug-in is parked in" >&2
        echo "         $_c/Anamorph.vst3.prev," >&2
        echo "         but that directory is no longer usable for staging, so this run cannot" >&2
        echo "         put it back. It is left untouched. To recover it, make that directory" >&2
        echo "         usable again (it must be a real directory you own, not writable by" >&2
        echo "         others) and re-run this installer - that is the only thing that puts" >&2
        echo "         it back. './uninstall.sh' leaves it alone and says so; only" >&2
        echo "         './uninstall.sh --discard-parked' deletes it." >&2
    done

    for _c in "${1%/*}/.anamorph-install-stage" "$1/.anamorph-install-stage"; do
        stage_dir_is_adoptable "$_c" "$_owner" || continue
        make_stage_dir "$_c" || continue
        if stage_dir_is_same_filesystem "$_c" "$_probe"; then
            printf '%s\n' "$_c"
            return 0
        fi
        # rmdir, not rm -rf: a directory with anything in it is left alone.
        $SUDO rmdir "$_c" 2>/dev/null || true
    done
    return 1
}

# What both modes print when no staging directory can be used. It names every
# candidate the loops above try, so the remedy is a command rather than a hunt.
stage_dir_advice() {            # $1 = plug-in directory
    echo "error: no usable staging directory beside $1." >&2
    echo "       One is present but is a symlink, owned by another account, or writable" >&2
    echo "       by others - this installer will not stage a privileged copy there." >&2
    echo "       Inspect and remove whichever of these exists, then re-run:" >&2
    echo "         ${1%/*}/.anamorph-install-stage" >&2
    echo "         $1/.anamorph-install-stage" >&2
}

# Puts back the previous version if an install was interrupted, then clears the
# temporary files. Runs once before installing and again if the script is stopped.
reconcile() {
    # On a system-wide install the staging directory belongs to root while this
    # script is still running as you, so looking inside it needs the same
    # elevation the writes do.
    if [ ! -e "$VST3_DEST" ] && $SUDO test -d "$PREV_VST3"; then
        $SUDO mv "$PREV_VST3" "$VST3_DEST" 2>/dev/null || true
    fi
    $SUDO rm -rf "$STAGE_VST3" 2>/dev/null || true
    # The kept copy is only discarded once the plug-in is back in place.
    if [ -e "$VST3_DEST" ]; then
        $SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
    fi
    # The staging directory itself always goes, so an install stopped part-way
    # leaves nothing behind. rmdir declines one that still holds the kept copy.
    $SUDO rmdir "$STAGE_DIR" 2>/dev/null || true
}

arm_traps() {
    trap 'reconcile' EXIT
    trap 'reconcile; exit 130' INT
    trap 'reconcile; exit 143' TERM
    trap 'reconcile; exit 129' HUP
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
        data=$1; app=$2; system=$3; portable=$4; owner=$(id -u)
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
            if ! present "$state"; then
                for relative in $paths; do
                    if present "$data/$relative"; then warn "resource without ownership state"; return; fi
                done
                return 0
            fi
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
            [ ! "$app" -ef "$portable" ] || return 0
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
    ' anamorph-legacy-cleanup "$_data" "$_app" "$mode" "$HERE/Anamorph" || {
        printf 'warning: legacy Anamorph cleanup incomplete; unverified files are kept.\n' >&2
    }
}
# END legacy desktop cleanup

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

    check_plugin_destination "$VST3_DEST"
    cleanup_legacy_desktop
    mkdir -p "$VST3_DIR"
    STAGE_DIR="$(choose_stage_dir "$VST3_DIR")" || {
        stage_dir_advice "$VST3_DIR"
        exit 1
    }
    STAGE_VST3="$STAGE_DIR/Anamorph.vst3"
    PREV_VST3="$STAGE_DIR/Anamorph.vst3.prev"


    # reconcile clears the staging directory, so make_stage_dir has to follow it
    # rather than precede it - these three lines belong in this order.
    reconcile
    arm_traps
    make_stage_dir "$STAGE_DIR" || {
        echo "error: could not create the staging directory $STAGE_DIR." >&2
        exit 1
    }

    cp -R "$VST3_SRC" "$STAGE_VST3"
    chmod 755 "$STAGE_VST3/Contents/x86_64-linux/Anamorph.so"

    # Only ever set the old version aside into an empty slot, so the copy kept
    # for you is always the one this run put there.
    if [ -e "$VST3_DEST" ]; then
        [ ! -e "$PREV_VST3" ] || {
            echo "error: $PREV_VST3 already holds a parked copy, so this run cannot set the" >&2
            echo "       previous version aside. Re-run './install.sh' to let it reconcile," >&2
            echo "       or remove that directory once you are sure of its contents." >&2
            exit 1
        }
        mv "$VST3_DEST" "$PREV_VST3"
    fi
    mv "$STAGE_VST3" "$VST3_DEST"
    # The install is complete here, so tidying up must not fail the run - but it
    # says what it could not remove rather than leaving you to find it.
    rm -rf "$PREV_VST3" 2>/dev/null || {
        echo "note: the previous version could not be removed and is left at" >&2
        echo "      $PREV_VST3." >&2
        echo "      The new version is installed and in use; that copy is only a" >&2
        echo "      leftover and is safe to delete." >&2
    }
    rmdir "$STAGE_DIR" 2>/dev/null || true
    trap - EXIT INT TERM HUP

    echo "Installed (current user only - no root needed):"
    echo "  VST3       -> $VST3_DEST"
    if [ -e "$SYS_VST3_DIR/Anamorph.vst3" ]; then
        echo "Note: an older system-wide install is still present, so Anamorph may appear"
        echo "      twice in your DAW and the older copy may load instead:"
        if [ -e "$SYS_VST3_DIR/Anamorph.vst3" ]; then echo "        $SYS_VST3_DIR/Anamorph.vst3"; fi
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

check_plugin_destination "$VST3_DEST"
cleanup_legacy_desktop
priv mkdir -p "$VST3_DIR"
STAGE_DIR="$(choose_stage_dir "$VST3_DIR")" || {
    stage_dir_advice "$VST3_DIR"
    exit 1
}
STAGE_VST3="$STAGE_DIR/Anamorph.vst3"
PREV_VST3="$STAGE_DIR/Anamorph.vst3.prev"


# Same order as the per-user branch above: reconcile clears the staging
# directory, so make_stage_dir follows it.
reconcile
arm_traps
make_stage_dir "$STAGE_DIR" || { echo "System installation failed: cannot create $STAGE_DIR" >&2; exit 1; }

priv cp -R "$VST3_SRC" "$STAGE_VST3"
$SUDO chmod 755 "$STAGE_VST3/Contents/x86_64-linux/Anamorph.so"

# Same rule as the per-user branch: never park onto an occupied slot.
if [ -e "$VST3_DEST" ]; then
    $SUDO test ! -e "$PREV_VST3" || {
        echo "error: $PREV_VST3 already holds a parked copy, so this run cannot set the" >&2
        echo "       previous version aside. Re-run the installer to let it reconcile," >&2
        echo "       or remove that directory once you are sure of its contents." >&2
        exit 1
    }
    priv mv "$VST3_DEST" "$PREV_VST3"
fi
priv mv "$STAGE_VST3" "$VST3_DEST"
# The install is complete here; clearing the kept copy must not fail the run.
# A leftover in /usr needs root to remove, so it is named rather than swallowed.
$SUDO rm -rf "$PREV_VST3" 2>/dev/null || {
    echo "note: the previous version could not be removed and is left at" >&2
    echo "      $PREV_VST3." >&2
    echo "      The new version is installed and in use; that copy is only a" >&2
    echo "      leftover and is safe to delete." >&2
}
$SUDO rmdir "$STAGE_DIR" 2>/dev/null || true
trap - EXIT INT TERM HUP

echo "Installed (system-wide, all users):"
echo "  VST3       -> $VST3_DEST"
# The mirror of the per-user branch's warning: a per-user copy is scanned too,
# and many DAWs load it in preference to this one. Finding it needs your home
# rather than root's, which is what SUDO_USER answers. Kept in a subshell so a
# courtesy note can never fail an install that has already finished.
(
    user_home=''
    user_as=''
    if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
        if command -v getent >/dev/null 2>&1; then
            user_home=$(getent passwd "$SUDO_USER" 2>/dev/null | cut -d: -f6)
        fi
        [ -n "$user_home" ] || user_home="/home/$SUDO_USER"
        user_as=" as $SUDO_USER"
    elif [ "$(id -u)" -ne 0 ]; then
        user_home="${HOME:-}"      # elevated per-operation; $HOME is still ours
    fi

    if [ -n "$user_home" ]; then
        _uv="$user_home/.vst3/Anamorph.vst3"
        if [ -e "$_uv" ]; then
            echo "Note: a per-user install is also present, so Anamorph may appear twice in"
            echo "      your DAW and the per-user copy may load instead of this one:"
            if [ -e "$_uv" ]; then echo "        $_uv"; fi
            echo "      Remove it${user_as} with:  ./uninstall.sh"
        fi
    fi
) || true

echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  sudo ./uninstall.sh"
