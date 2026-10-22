#!/usr/bin/env bash
# GitHub's Ubuntu mirror list preferred Azure in Build & Validate #1777:
# 79-81 MB took 33-34 minutes, exhausting the linux/sanitizers job budgets.
# Select Ubuntu's official HTTPS archive for that exact entry, preserving the
# mirror list's priorities and other entries. No file means no change (the GCC
# container uses Debian sources). Called only by the Linux CI composite action.
set -euo pipefail

prefer_ubuntu_archive() {
    if [ -f "$1" ]; then
        sed -E -i 's#^([[:blank:]]*)https?://azure\.archive\.ubuntu\.com/ubuntu/?([[:blank:]]|$)#\1https://archive.ubuntu.com/ubuntu\2#' "$1"
    fi
}

if [ "${1:-}" = "--self-test" ]; then
    apt_mirror_testdir="$(mktemp -d)"
    trap 'rm -rf "$apt_mirror_testdir"' EXIT
    apt_mirror_checks=0
    apt_mirror_failures=0
    expect_rewrite() {
        printf '%s' "$2" > "$apt_mirror_testdir/actual"
        printf '%s' "$3" > "$apt_mirror_testdir/expected"
        prefer_ubuntu_archive "$apt_mirror_testdir/actual"
        apt_mirror_checks=$((apt_mirror_checks + 1))
        if ! cmp -s "$apt_mirror_testdir/actual" "$apt_mirror_testdir/expected"; then
            echo "configure-ci-apt-mirrors: FAIL -- $1" >&2
            apt_mirror_failures=$((apt_mirror_failures + 1))
        fi
    }
    expect_rewrite 'HTTP Azure with priority' \
        $'http://azure.archive.ubuntu.com/ubuntu\tpriority:1\n' \
        $'https://archive.ubuntu.com/ubuntu\tpriority:1\n'
    expect_rewrite 'HTTPS Azure, trailing slash and leading whitespace' \
        $'  https://azure.archive.ubuntu.com/ubuntu/ priority:2\n' \
        $'  https://archive.ubuntu.com/ubuntu priority:2\n'
    expect_rewrite 'entry without metadata or a final newline' \
        'http://azure.archive.ubuntu.com/ubuntu' \
        'https://archive.ubuntu.com/ubuntu'
    apt_mirror_other=$'# http://azure.archive.ubuntu.com/ubuntu priority:1\nhttps://ports.ubuntu.com/ubuntu-ports priority:2\nhttps://private.example/ubuntu priority:3\n'
    expect_rewrite 'comments, ports and custom mirrors' "$apt_mirror_other" "$apt_mirror_other"
    apt_mirror_other=$'http://azure.archive.ubuntu.com/ubuntu-extra\nhttp://azure.archive.ubuntu.com.evil.example/ubuntu\n'
    expect_rewrite 'URL must match the exact host and archive path' "$apt_mirror_other" "$apt_mirror_other"
    apt_mirror_other=$'https://archive.ubuntu.com/ubuntu priority:1\nhttp://security.ubuntu.com/ubuntu priority:2\n'
    expect_rewrite 'already configured and security mirrors' "$apt_mirror_other" "$apt_mirror_other"
    expect_rewrite 'empty list' '' ''
    prefer_ubuntu_archive "$apt_mirror_testdir/absent"
    apt_mirror_checks=$((apt_mirror_checks + 1))
    if [ -e "$apt_mirror_testdir/absent" ]; then
        echo 'configure-ci-apt-mirrors: FAIL -- an absent list must stay absent' >&2
        apt_mirror_failures=$((apt_mirror_failures + 1))
    fi
    printf 'configure-ci-apt-mirrors: %d checks, %d failures\n' "$apt_mirror_checks" "$apt_mirror_failures"
    [ "$apt_mirror_failures" -eq 0 ]
    exit
fi

if [ "$#" -gt 1 ] || [[ "${1:-}" == --* ]]; then
    echo 'usage: configure-ci-apt-mirrors.sh [mirror-list | --self-test]' >&2
    exit 2
fi
prefer_ubuntu_archive "${1:-/etc/apt/apt-mirrors.txt}"
