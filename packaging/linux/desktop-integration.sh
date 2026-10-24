#!/bin/sh
# Product-owned XDG resources. Called by install.sh/uninstall.sh, with the same
# per-operation elevation as their binary writes. No Python/runtime library needed.
set -eu
set -f
ACTION=${1:?action required}
DATA=${2:?absolute data root required}
EXE=${3:?absolute installed executable required}
while [ "$DATA" != / ] && [ "${DATA%/}" != "$DATA" ]; do DATA=${DATA%/}; done
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ASSETS="$HERE/desktop-assets"
OWNER=$(id -u)
STATE="$DATA/.tech.rolly.anamorph"
TXN="$STATE/transaction"
RECEIPT="$STATE/receipt"
FORMAT='tech.rolly.anamorph desktop resources v1'
CLEANUP_FORMAT='tech.rolly.anamorph verified cleanup v1'
PATHS='applications/tech.rolly.anamorph.desktop
icons/hicolor/16x16/apps/tech.rolly.anamorph.png
icons/hicolor/24x24/apps/tech.rolly.anamorph.png
icons/hicolor/32x32/apps/tech.rolly.anamorph.png
icons/hicolor/48x48/apps/tech.rolly.anamorph.png
icons/hicolor/64x64/apps/tech.rolly.anamorph.png
icons/hicolor/128x128/apps/tech.rolly.anamorph.png
icons/hicolor/256x256/apps/tech.rolly.anamorph.png
icons/hicolor/512x512/apps/tech.rolly.anamorph.png
icons/hicolor/scalable/apps/tech.rolly.anamorph.svg'
fail() { printf 'error: Anamorph desktop integration: %s\n' "$*" >&2; exit 1; }
hash() { sha256sum < "$1" | cut -d ' ' -f 1; }
present() { [ -e "$1" ] || [ -L "$1" ]; }

absolute_path() {
    case "$1" in /*) ;; *) fail 'an absolute destination is required' ;; esac
    if printf '%s' "$1" | LC_ALL=C grep -q '[[:cntrl:]]'; then fail 'control character in destination'; fi
}
absolute_path "$DATA"
absolute_path "$EXE"
case "$ACTION" in preflight|install|uninstall) ;; *) fail 'unknown resource operation' ;; esac

# Reject symlinks in the WHOLE ancestry, not only the final filename. Shared
# parents may belong to root; writable parents belonging to other users/groups
# cannot protect the exact destination from substitution. No symlink traversal.
safe_parent() {
    guard=$1
    while [ "$guard" != / ]; do
        [ ! -L "$guard" ] || fail "symlink in destination ancestry: $guard"
        if [ -e "$guard" ]; then
            [ -d "$guard" ] || fail "destination parent is not a directory: $guard"
            meta=$(stat -c '%u %a' -- "$guard") || fail "cannot inspect $guard"
            uid=${meta%% *}; permissions=${meta#* }
            [ "$uid" = 0 ] || [ "$uid" = "$OWNER" ] || fail "foreign parent owner: $guard"
            case "$permissions" in
                [1357][0-7][0-7][0-7]) ;; # sticky parent protects its owned children (e.g. /tmp)
                *[2367][0-7]|*[2367]) fail "unsafe writable parent: $guard" ;;
            esac
        fi
        guard=$(dirname -- "$guard")
    done
}
owned_file() {
    [ -f "$1" ] && [ ! -L "$1" ] || fail "not a regular owned file: $1"
    [ "$(stat -c '%u' -- "$1")" = "$OWNER" ] || fail "foreign resource owner: $1"
}
private_dir() {
    [ -d "$1" ] && [ ! -L "$1" ] || fail "unsafe private directory: $1"
    [ "$(stat -c '%u %a' -- "$1")" = "$OWNER 700" ] || fail "private directory ownership/mode: $1"
}
receipt_hash() { awk -v path="$2" '$2 == path { print $1 }' "$1"; }
receipt_shape() {
    owned_file "$1"
    [ "$(head -n 1 -- "$1")" = "$FORMAT" ] || fail 'unknown ownership receipt format'
    actual=$(tail -n +2 -- "$1" | awk '{print $2}' | LC_ALL=C sort)
    expected=$(printf '%s\n' "$PATHS" | LC_ALL=C sort)
    [ "$actual" = "$expected" ] || fail 'ownership receipt path set differs'
    tail -n +2 -- "$1" | while read -r sum relative extra; do
        [ -z "${extra:-}" ] && [ "${#sum}" -eq 64 ] || fail 'malformed ownership digest'
        case "$sum" in *[!0-9a-f]*) fail 'malformed ownership digest' ;; esac
    done
}
source_check() {
    [ ! -L "$ASSETS" ] && [ -d "$ASSETS" ] || fail 'missing/linked desktop-assets directory'
    [ "$(hash "$ASSETS/SHA256SUMS")" = 7bd45fbb113c1d11136e20c0802824b49c8b409ae31ab1ca8fb17db3bc82e127 ] || fail 'unapproved source hash manifest'
    # The pinned manifest fixes the ten relative input names; do not consume an
    # arbitrary user-edited list or discover additional assets with a glob.
    for relative in $PATHS; do
        case "$relative" in applications/*) input=${relative#applications/} ;; icons/*) input=${relative#icons/} ;; esac
        [ -f "$ASSETS/$input" ] && [ ! -L "$ASSETS/$input" ] || fail "missing/linked source asset: $input"
        no_source_links "$ASSETS/$input" "$ASSETS"
    done
    (cd "$ASSETS" && sha256sum --status -c SHA256SUMS) || fail 'source asset hash mismatch'
}
no_source_links() {
    source_part=$1
    while [ "$source_part" != "$2" ]; do
        [ ! -L "$source_part" ] || fail "linked source asset ancestry: $source_part"
        source_part=$(dirname -- "$source_part")
    done
}
check_destinations() {
    if present "$RECEIPT"; then receipt_shape "$RECEIPT"; fi
    for relative in $PATHS; do
        dest="$DATA/$relative"
        safe_parent "$(dirname -- "$dest")"
        if present "$dest"; then
            present "$RECEIPT" || fail "foreign resource without ownership receipt: $dest"
            owned_file "$dest"
            [ "$(hash "$dest")" = "$(receipt_hash "$RECEIPT" "$relative")" ] || fail "modified/foreign resource preserved: $dest"
        fi
        ancestor=$(dirname -- "$dest")
        while [ ! -e "$ancestor" ]; do ancestor=$(dirname -- "$ancestor"); done
        [ -w "$ancestor" ] || fail "unwritable resource destination: $ancestor"
    done
}

# Every destination gets a same-directory temporary regular file, followed by
# an atomic rename. A journaled transaction ID owns exactly those scratch names.
clean_temps() {
    for relative in $PATHS; do
        parent=$(dirname -- "$DATA/$relative")
        safe_parent "$parent"
        scratch="$parent/.tech.rolly.anamorph-$transaction_id.tmp"
        if present "$scratch"; then owned_file "$scratch"; rm -- "$scratch"; fi
    done
}
publish() {
    parent=$(dirname -- "$2")
    safe_parent "$parent"
    mkdir -p -- "$parent"
    safe_parent "$parent"
    scratch="$parent/.tech.rolly.anamorph-$transaction_id.tmp"
    (umask 077; set -C; : > "$scratch") || fail "occupied resource scratch: $scratch"
    cp -- "$1" "$scratch"
    chmod 644 -- "$scratch"
    [ ! -L "$2" ] || fail "symlink substituted at destination: $2"
    mv -fT -- "$scratch" "$2"
}
journal_layout() {
    # Count entries by fixed allowed paths, not line-delimited discovered names.
    # Even an unknown name containing a newline cannot impersonate two entries.
    allowed='pid id ready committed new-receipt old-receipt inputs/SHA256SUMS'
    for resource in $PATHS; do
        case "$resource" in applications/*) input=${resource#applications/} ;; icons/*) input=${resource#icons/} ;; esac
        allowed="$allowed new/$resource old/$resource inputs/$input"
    done
    directories=$({ printf '%s\n' format-v1; for file in $allowed; do
        part=$(dirname -- "$file")
        while [ "$part" != . ]; do printf '%s\n' "$part"; part=$(dirname -- "$part"); done
    done; } | LC_ALL=C sort -u)
}
transaction_shape() {
    private_dir "$TXN/format-v1"
    [ -z "$(ls -A -- "$TXN/format-v1")" ] || fail 'unknown resource transaction format'
    journal_layout
    known=0
    for file in $allowed; do
        if present "$TXN/$file"; then owned_file "$TXN/$file"; known=$((known + 1)); fi
    done
    for dir in $directories; do
        if present "$TXN/$dir"; then
            [ -d "$TXN/$dir" ] && [ ! -L "$TXN/$dir" ] || fail 'linked/non-directory journal path'
            [ "$(stat -c '%u' -- "$TXN/$dir")" = "$OWNER" ] || fail 'foreign journal directory owner'
            known=$((known + 1))
        fi
    done
    actual=$(find "$TXN" -mindepth 1 -printf x | wc -c)
    [ "$actual" -eq "$known" ] || fail 'unknown transaction entries preserved; recovery refused'
}

# The cleanup authority lives OUTSIDE the tree being deleted. It binds the
# public state and every remaining journal entry, so losing format-v1, ready,
# committed or new-receipt during deletion cannot turn cleanup back into rollback.
cleanup_plan_shape() {
    owned_file "$CLEANUP"
    [ "$(stat -c '%a' -- "$CLEANUP")" = 600 ] || fail 'unsafe cleanup plan mode'
    [ "$(head -n 1 -- "$CLEANUP")" = "$CLEANUP_FORMAT" ] || fail 'unknown cleanup plan format'
    cleanup_receipt=$(sed -n '2p' "$CLEANUP")
    case "$cleanup_receipt" in -) ;; *[!0-9a-f]*|'') fail 'invalid cleanup receipt digest' ;; *) [ "${#cleanup_receipt}" -eq 64 ] || fail 'invalid cleanup receipt digest' ;; esac
    cleanup_pid=$(sed -n '3p' "$CLEANUP")
    case "$cleanup_pid" in ''|*[!0-9]*|0) fail 'invalid cleanup process ID' ;; esac
    if [ "$cleanup_pid" != "$$" ] && kill -0 "$cleanup_pid" 2>/dev/null; then fail 'another desktop resource operation is running'; fi
    journal_layout
    public_paths=$(tail -n +4 -- "$CLEANUP" | awk '$1 == "p" {print $4}' | LC_ALL=C sort)
    [ "$public_paths" = "$(printf '%s\n' "$PATHS" | LC_ALL=C sort)" ] || fail 'cleanup public path set differs'
    duplicates=$(tail -n +4 -- "$CLEANUP" | awk '{print $1, $4}' | LC_ALL=C sort | uniq -d)
    [ -z "$duplicates" ] || fail 'duplicate cleanup entry'
    tail -n +4 -- "$CLEANUP" | while read -r entry_kind entry_mode entry_digest entry_path extra; do
        [ -z "${extra:-}" ] && [ -n "$entry_path" ] || fail 'malformed cleanup entry'
        case "$entry_kind" in
            p) case " $PATHS " in *"$entry_path"*) ;; *) fail 'foreign cleanup public path' ;; esac
               [ "$entry_mode" = - ] || fail 'invalid cleanup public mode' ;;
            f) case " $allowed " in *" $entry_path "*) ;; *) fail 'foreign cleanup file' ;; esac ;;
            d) case "
$directories
" in *"
$entry_path
"*) ;; *) fail 'foreign cleanup directory' ;; esac ;;
            *) fail 'unknown cleanup entry type' ;;
        esac
        if [ "$entry_kind" != p ]; then
            case "$entry_mode" in ''|*[!0-7]*) fail 'invalid cleanup entry mode' ;; esac
            [ "${#entry_mode}" -ge 3 ] && [ "${#entry_mode}" -le 4 ] || fail 'invalid cleanup entry mode'
        fi
        case "$entry_digest" in
            -) [ "$entry_kind" != f ] || fail 'missing cleanup file digest' ;;
            *[!0-9a-f]*|'') fail 'invalid cleanup digest' ;;
            *) [ "${#entry_digest}" -eq 64 ] && [ "$entry_kind" != d ] || fail 'invalid cleanup digest' ;;
        esac
    done
}
cleanup_public_state() {
    if [ "$cleanup_receipt" = - ]; then
        ! present "$RECEIPT" || fail 'cleanup ownership receipt changed'
    else
        receipt_shape "$RECEIPT"
        [ "$(hash "$RECEIPT")" = "$cleanup_receipt" ] || fail 'cleanup ownership receipt changed'
    fi
    tail -n +4 -- "$CLEANUP" | while read -r entry_kind entry_mode entry_digest entry_path; do
        [ "$entry_kind" = p ] || continue
        safe_parent "$(dirname -- "$DATA/$entry_path")"
        if [ "$entry_digest" = - ]; then
            ! present "$DATA/$entry_path" || fail 'foreign public resource blocks cleanup'
        else
            owned_file "$DATA/$entry_path"
            [ "$(hash "$DATA/$entry_path")" = "$entry_digest" ] || fail 'foreign public resource blocks cleanup'
        fi
    done
}
cleanup_tree_shape() {
    private_dir "$1"
    remaining_count=0
    # Read without a pipeline: the byte-count comparison needs this shell's count.
    while read -r entry_kind entry_mode entry_digest entry_path; do
        case "$entry_kind" in f|d) ;; *) continue ;; esac
        present "$1/$entry_path" || continue
        [ ! -L "$1/$entry_path" ] || fail 'linked cleanup entry preserved'
        [ "$(stat -c '%u %a' -- "$1/$entry_path")" = "$OWNER $entry_mode" ] || fail 'cleanup entry owner/mode changed'
        if [ "$entry_kind" = f ]; then
            owned_file "$1/$entry_path"
            [ "$(hash "$1/$entry_path")" = "$entry_digest" ] || fail 'foreign journal content blocks cleanup'
        else [ -d "$1/$entry_path" ] || fail 'cleanup directory changed'; fi
        remaining_count=$((remaining_count + 1))
    done < "$CLEANUP"
    [ "$(find "$1" -mindepth 1 -printf x | wc -c)" -eq "$remaining_count" ] || fail 'unknown cleanup entries preserved'
}
resume_cleanup() {
    cleanup_plan_shape
    cleanup_public_state
    if present "$TXN"; then
        ! present "$RETIRED" || fail 'both active and retired journals exist'
        cleanup_tree_shape "$TXN"
        mv -T -- "$TXN" "$RETIRED"
    fi
    if present "$RETIRED"; then
        cleanup_tree_shape "$RETIRED"
        # A deliberately early directory deletion proves cleanup is independent
        # of the old mandatory marker; all other removals are exact planned names.
        if present "$RETIRED/format-v1"; then rmdir -- "$RETIRED/format-v1"; fi
        while read -r entry_kind entry_mode entry_digest entry_path; do
            [ "$entry_kind" = f ] || continue
            if present "$RETIRED/$entry_path"; then
                owned_file "$RETIRED/$entry_path"
                [ "$(hash "$RETIRED/$entry_path")" = "$entry_digest" ] || fail 'foreign journal content blocks cleanup'
                rm -- "$RETIRED/$entry_path"
            fi
        done < "$CLEANUP"
        cleanup_dirs=$(awk '$1 == "d" {print length($4), $4}' "$CLEANUP" | LC_ALL=C sort -rn | cut -d ' ' -f 2-)
        for entry_path in $cleanup_dirs; do
            if present "$RETIRED/$entry_path"; then rmdir -- "$RETIRED/$entry_path"; fi
        done
        rmdir -- "$RETIRED"
    fi
    # Last: a killed run with no retired tree still verifies public state, then
    # removes this regular authority file. It never recursively deletes state.
    rm -- "$CLEANUP"
}
cleanup_document() {
    printf '%s\n' "$CLEANUP_FORMAT"
    if present "$RECEIPT"; then hash "$RECEIPT"; else printf '%s\n' -; fi
    printf '%s\n' "$1"
    for entry_path in $PATHS; do
        if present "$DATA/$entry_path"; then entry_digest=$(hash "$DATA/$entry_path"); else entry_digest=-; fi
        printf 'p - %s %s\n' "$entry_digest" "$entry_path"
    done
    for entry_path in $allowed; do
        if present "$TXN/$entry_path"; then
            printf 'f %s %s %s\n' "$(stat -c '%a' -- "$TXN/$entry_path")" "$(hash "$TXN/$entry_path")" "$entry_path"
        fi
    done
    for entry_path in $directories; do
        if present "$TXN/$entry_path"; then
            printf 'd %s - %s\n' "$(stat -c '%a' -- "$TXN/$entry_path")" "$entry_path"
        fi
    done
}
retire_transaction() {
    check_destinations
    journal_layout
    if present "$CLEANUP_NEW"; then
        owned_file "$CLEANUP_NEW"
        [ "$(stat -c '%a' -- "$CLEANUP_NEW")" = 600 ] || fail 'unsafe cleanup draft mode'
        draft_pid=$(sed -n '3p' "$CLEANUP_NEW")
        case "$draft_pid" in ''|*[!0-9]*) draft_pid=$$ ;; esac
        if [ "$draft_pid" != "$$" ] && kill -0 "$draft_pid" 2>/dev/null; then fail 'another desktop resource operation is running'; fi
        # An interrupted write may leave only a prefix. Its bytes must match the
        # document for this still-validated journal; a magic header alone is not
        # authority to overwrite foreign control content.
        draft_document=$(cleanup_document "$draft_pid")
        draft_size=$(stat -c '%s' -- "$CLEANUP_NEW")
        printf '%s\n' "$draft_document" | head -c "$draft_size" | cmp -s -- "$CLEANUP_NEW" - || fail 'foreign cleanup draft preserved'
    fi
    cleanup_document_value=$(cleanup_document "$$")
    (umask 077; printf '%s\n' "$cleanup_document_value" > "$CLEANUP_NEW")
    mv -T -- "$CLEANUP_NEW" "$CLEANUP"
    resume_cleanup
}
recover() {
    if present "$CLEANUP"; then resume_cleanup; return; fi
    ! present "$RETIRED" || fail 'retired journal lacks verified cleanup authority'
    present "$TXN" || return 0
    private_dir "$TXN"
    if [ ! -e "$TXN/format-v1" ] && [ -z "$(ls -A -- "$TXN")" ]; then rmdir -- "$TXN"; return 0; fi
    transaction_shape
    if [ -f "$TXN/pid" ]; then
        pid=$(cat "$TXN/pid")
        case "$pid" in
            ''|*[!0-9]*) [ ! -f "$TXN/ready" ] || fail 'invalid transaction process ID' ;;
            *) if [ "$pid" != "$$" ] && kill -0 "$pid" 2>/dev/null; then fail 'another desktop resource operation is running'; fi ;;
        esac
    fi
    # Until ready exists, no public resource has been written. The entire
    # incomplete preparation is confined to this checked private product tree.
    if [ ! -f "$TXN/ready" ]; then retire_transaction; return 0; fi
    transaction_id=$(cat "$TXN/id")
    case "$transaction_id" in ''|*[!0-9-]*) fail 'invalid transaction scratch ID' ;; esac
    receipt_shape "$TXN/new-receipt"
    clean_temps
    if [ ! -f "$TXN/committed" ]; then
        # Validate the ENTIRE public set before rollback. Never overwrite a
        # foreign change made after interruption just to repair our journal.
        for relative in $PATHS; do
            dest="$DATA/$relative"
            if present "$dest"; then
                owned_file "$dest"
                value=$(hash "$dest")
                new=$(receipt_hash "$TXN/new-receipt" "$relative")
                old=''
                if [ -f "$TXN/old/$relative" ]; then old=$(hash "$TXN/old/$relative"); fi
                [ "$value" = "$new" ] || [ "$value" = "$old" ] || fail "foreign change blocks resource recovery: $dest"
            fi
        done
        for relative in $PATHS; do
            dest="$DATA/$relative"
            if [ -f "$TXN/old/$relative" ]; then publish "$TXN/old/$relative" "$dest"
            elif present "$dest"; then rm -- "$dest"; fi
        done
        if [ -f "$TXN/old-receipt" ]; then cp -- "$TXN/old-receipt" "$RECEIPT"
        else rm -f -- "$RECEIPT"; fi
        printf 'Recovered previous Anamorph desktop resources.\n'
    else
        # Commit is after all ten resources AND the new receipt. Do not silently
        # discard backups if someone has replaced the current ownership record.
        cmp -s -- "$RECEIPT" "$TXN/new-receipt" || fail 'committed ownership receipt changed'
        check_destinations
        for relative in $PATHS; do
            present "$DATA/$relative" || fail 'committed resource is missing'
        done
    fi
    clean_temps
    retire_transaction
}
refresh_caches() {
    if command -v update-desktop-database >/dev/null 2>&1 && [ -d "$DATA/applications" ]; then
        update-desktop-database "$DATA/applications" || printf 'warning: desktop cache refresh failed; retry for %s\n' "$DATA" >&2
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1 && [ -d "$DATA/icons/hicolor" ]; then
        gtk-update-icon-cache -f -t "$DATA/icons/hicolor" || printf 'warning: icon cache refresh failed; retry for %s\n' "$DATA" >&2
    fi
}

safe_parent "$DATA"
# Check the original ancestry BEFORE lexical normalization: resolving a/../b
# first could conceal a symlink at a. Coreutils -s never follows symlinks.
DATA=$(realpath -ms -- "$DATA")
STATE="$DATA/.tech.rolly.anamorph"
TXN="$STATE/transaction"
RECEIPT="$STATE/receipt"
RETIRED="$STATE/retired"
CLEANUP="$STATE/cleanup-plan"
CLEANUP_NEW="$STATE/cleanup-plan.new"
safe_parent "$DATA"
if present "$STATE"; then
    private_dir "$STATE"
    known=0
    for entry in format-v1 receipt transaction retired cleanup-plan cleanup-plan.new; do
        if present "$STATE/$entry"; then known=$((known + 1)); fi
    done
    actual=$(find "$STATE" -mindepth 1 -maxdepth 1 -printf x | wc -c)
    [ "$actual" -eq "$known" ] || fail 'unknown ownership state entries preserved; operation refused'
    if present "$STATE/format-v1"; then
        private_dir "$STATE/format-v1"
        [ -z "$(ls -A -- "$STATE/format-v1")" ] || fail 'foreign ownership state directory'
    elif [ -n "$(ls -A -- "$STATE")" ]; then fail 'unrecognised nonempty ownership state directory'; fi
    recover
    ! present "$CLEANUP_NEW" || fail 'orphaned cleanup draft preserved'
fi
if [ "$ACTION" = uninstall ]; then
    if ! present "$RECEIPT"; then
        for relative in $PATHS; do
            safe_parent "$(dirname -- "$DATA/$relative")"
            ! present "$DATA/$relative" || fail "resource without ownership receipt preserved: $DATA/$relative"
        done
        if [ -d "$STATE" ] && [ "$(ls -A -- "$STATE")" = format-v1 ]; then
            rmdir -- "$STATE/format-v1"
            rmdir -- "$STATE"
        fi
        printf 'No managed Anamorph desktop resources in %s; foreign files are preserved.\n' "$DATA"
        exit 0
    fi
    check_destinations
    for relative in $PATHS; do
        dest="$DATA/$relative"
        if present "$dest"; then owned_file "$dest"; rm -- "$dest"; fi
    done
    rm -- "$RECEIPT"
    rmdir -- "$STATE/format-v1"
    rmdir -- "$STATE" || fail 'unexpected private resource state remains'
    # Leave shared theme/application directories and other products untouched.
    refresh_caches
    printf 'Removed owned Anamorph desktop resources from %s.\n' "$DATA"
    exit 0
fi

source_check
check_destinations
if [ "$ACTION" = preflight ]; then exit 0; fi
[ -f "$EXE" ] && [ -x "$EXE" ] && [ ! -L "$EXE" ] || fail 'installed Standalone is missing or linked'
mkdir -p -- "$DATA"
safe_parent "$DATA"
if ! present "$STATE"; then (umask 077; mkdir -- "$STATE"); fi
private_dir "$STATE"
if ! present "$STATE/format-v1"; then (umask 077; mkdir -- "$STATE/format-v1"); fi
(umask 077; mkdir -- "$TXN") || fail 'resource transaction is already occupied'
(umask 077; mkdir -- "$TXN/format-v1")
printf '%s\n' "$$" > "$TXN/pid"
transaction_id="$(date +%s)-$$"
printf '%s\n' "$transaction_id" > "$TXN/id"
# Roll back on ordinary errors/signals; SIGKILL leaves the journal for next run.
trap 'status=$?; trap - EXIT INT TERM HUP; recover; exit "$status"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP
mkdir -- "$TXN/new" "$TXN/old"
# Recheck the private copied inputs, closing the source-owner mutation window
# between preflight and elevated publication. Never render from a mutable input.
mkdir -- "$TXN/inputs"
cp -- "$ASSETS/SHA256SUMS" "$TXN/inputs/SHA256SUMS"
for relative in $PATHS; do
    case "$relative" in applications/*) input=${relative#applications/} ;; icons/*) input=${relative#icons/} ;; esac
    mkdir -p -- "$(dirname -- "$TXN/inputs/$input")"
    cp -- "$ASSETS/$input" "$TXN/inputs/$input"
    [ -f "$TXN/inputs/$input" ] && [ ! -L "$TXN/inputs/$input" ] || fail 'linked private input'
    no_source_links "$TXN/inputs/$input" "$TXN/inputs"
done
[ "$(hash "$TXN/inputs/SHA256SUMS")" = 7bd45fbb113c1d11136e20c0802824b49c8b409ae31ab1ca8fb17db3bc82e127 ] || fail 'private source hash manifest changed'
(cd "$TXN/inputs" && sha256sum --status -c SHA256SUMS) || fail 'private copied source hash mismatch'
if present "$RECEIPT"; then cp -- "$RECEIPT" "$TXN/old-receipt"; fi
printf '%s\n' "$FORMAT" > "$TXN/new-receipt"
escaped=$(printf '%s' "$EXE" | sed -e 's/\\/\\\\\\\\/g' -e 's/"/\\\\"/g' -e 's/`/\\\\`/g' -e 's/\$/\\\\$/g' -e 's/%/%%/g')
for relative in $PATHS; do
    new="$TXN/new/$relative"
    mkdir -p -- "$(dirname -- "$new")"
    case "$relative" in
        applications/*)
            while IFS= read -r line; do
                # GIO checks the first executable before expanding %% field
                # escapes; a literal percent in HOME otherwise hides a valid
                # desktop entry. env is from the same existing coreutils set,
                # execs the absolute app path, and never invokes a shell/PATH app.
                if [ "$line" = 'Exec=Anamorph' ]; then printf 'Exec=/usr/bin/env -- "%s"\n' "$escaped"
                else printf '%s\n' "$line"; fi
            done < "$TXN/inputs/tech.rolly.anamorph.desktop" > "$new" ;;
        icons/*) cp -- "$TXN/inputs/${relative#icons/}" "$new" ;;
    esac
    if present "$DATA/$relative"; then
        mkdir -p -- "$(dirname -- "$TXN/old/$relative")"
        cp -- "$DATA/$relative" "$TXN/old/$relative"
    fi
    printf '%s  %s\n' "$(hash "$new")" "$relative" >> "$TXN/new-receipt"
done
printf '%s\n' "$FORMAT" > "$TXN/ready"
for relative in $PATHS; do publish "$TXN/new/$relative" "$DATA/$relative"; done
cp -- "$TXN/new-receipt" "$RECEIPT"
printf '%s\n' "$FORMAT" > "$TXN/committed"
recover
trap - EXIT INT TERM HUP
refresh_caches
printf 'Installed Anamorph launcher and hicolor icons in %s.\n' "$DATA"
