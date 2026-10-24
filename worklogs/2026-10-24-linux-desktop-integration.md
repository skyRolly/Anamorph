# Linux desktop/hicolor integration — 2026-10-24

Starting main `e76a4a66e6ed50f3ade53be9717c2c002865e3c4`. All five predecessor
PRs are merged; Accepted ADR-0062/0063/0064 remain unchanged in status. Linux work
is independent of the two installed-Mac-receipt PRs and Anabasis Windows correction.

## Architecture approval and scope

Before implementation the concrete paths/ownership/recovery/test-tool proposal was
prepared and shown to the owner. The Architecture Review Gate's dependency-set rule
was explicitly identified, and the owner answered **“Approve Linux implementation
and testing.”** Record date: 2026-10-24 Asia/Shanghai. ADR-0065 remains Proposed;
formal independent review/acceptance and merge authorization remain pending.
No approval is inferred from predecessor ADRs or green CI.

The old testing procedure's blanket “No gate ever installs anything” statement was
reported stale before editing: #172 already includes guarded native Windows installs.
It now distinguishes Windows runtime, isolated Linux resource installation, Mac
metadata and pending GUI/DAW/selection/relocation validation. Historical dated Linux
binary-transaction evidence remains intact. ADR-0063 has a dated scope pointer only.

## Source and preservation

Approved icon main `203bb326d02fcddeb4c9989c65cf44010f1eaff6`, merged #12/#13.
Read the actual assets manifest and independently SHA-256 checked every imported
Git blob. Vendor ten unchanged files under resources/icons/linux, fixed SHA256SUMS
and PROVENANCE.json. No renderer, asset download, Windows ICO or macOS ICNS is used
for the Linux launcher. Source manifest SHA-256: 0fa5cc7418df76e13d01187fc659bf2bbf36063f9b5285eb5de002f3f537d7b5.

| Resource relative to resources/icons/linux | Exact approved SHA-256 |
| --- | --- |
| hicolor/128x128/apps/tech.rolly.anamorph.png | `6ea3c005c17c69498744082d0cfefbaa89b20b3aac48aee5292bcde54156d163` |
| hicolor/16x16/apps/tech.rolly.anamorph.png | `026306d286b636aa9a8bf6a537362cfe638a6e1b206fff8dc23ae3abb2e6e183` |
| hicolor/24x24/apps/tech.rolly.anamorph.png | `019f92e97405a2b5272586ce6bf29ec5bcb6bad5dc2a4ad2b1e6c4b43950e15f` |
| hicolor/256x256/apps/tech.rolly.anamorph.png | `b906dbbaa5ff74f8dcde8bd5cc6e377a5149f9e47eea98ec24bee5c23a1b4845` |
| hicolor/32x32/apps/tech.rolly.anamorph.png | `f2a80389e5f47c23b69a0e8956f0492c5f1194a6970df0726c172bf633fb2002` |
| hicolor/48x48/apps/tech.rolly.anamorph.png | `c833a92c8b3b42f6f8b7a52267fa56354b4d2847d636b52da29f864cb1a1f96d` |
| hicolor/512x512/apps/tech.rolly.anamorph.png | `46fa75f587843b29e7aa0a82cd518cd24f3ef8791b20cdfd3cc48f42c93bcb3c` |
| hicolor/64x64/apps/tech.rolly.anamorph.png | `37bdffc8dd356b3a8536e3ed784dbeed19ca25d27629f2c6bbf520050af34b41` |
| hicolor/scalable/apps/tech.rolly.anamorph.svg | `9d944b2b04abeb92dddc6840cf949d126162169159e937f23a384160d0db56b4` |
| tech.rolly.anamorph.desktop | `8489d9a2e72c59f024addc4236d3388a64db6d4cee8be33da4500111ea51dffb` |

## Installation contract and rejected alternatives

Both existing required binary outputs, mode prompt/defaults, root --user refusal,
per-operation sudo, stage/park/reconcile, paths and scratch cleanup remain as before.
One shared resource shell helper is shipped beside both scripts. User XDG absolute
root (relative ignored with warning), fallback HOME/.local/share; system /usr/local/share
ignores root HOME/XDG. Absolute HOME required; trailing data-root slash supported;
dot/control path components are explicitly refused rather than ambiguously resolved.
No GTK/Python/Bubblewrap runtime dependency; use shell/POSIX text and base Linux tools.

Rejected PATH-only Exec: desktop sessions need not expose ~/.local/bin. Escape the
actual app path under Desktop Entry quoting, including %%. Native GIO testing found
that its constructor checks the executable before expanding %%; a literal percent in
HOME made a standards-escaped direct executable invisible. TryExec did not fix it.
Chosen measured adjustment: `/usr/bin/env --` then the escaped absolute app path.
The same coreutils env executable avoids the premature app lookup, invokes no shell,
and execs the exact app path after field expansion. Real GIO launch succeeded with
spaces, literal percent, quotes, dollar, backtick and backslash in HOME. Source desktop
bytes stay unchanged. No guessed StartupWMClass or GUI/window-ID change.

Rejected unconditional matching-name overwrite/delete: no ownership proof. Instead
keep a private 0700 data-root/.tech.rolly.anamorph receipt of ten exact hashes; unknown,
modified, symlinked, unsafe/wrong-owner resources stop preflight before binary writes.
Copy inputs privately and recheck pinned hashes before rendering/publication. Journal
old/new files and ownership receipt; publish each through a same-directory scratch
regular file and atomic rename. Ordinary error/signal rolls resources back; killed
transactions are reconciled next time. Before restoring, check the whole public set
matches recorded old/new values. Unknown journal entries/foreign changes are preserved
with failure, never swept up. Exact temporary names are journal-owned. Shared theme
roots and presets are never deleted. Resource cache refresh is optional and warning-
visible. Same-root operations must be serialized; this is process-interruption recovery,
not a guarantee about a physical power failure. Binary transactions remain independent;
a resource failure reports failure even if new valid binaries already committed.

Rejected Python installer runtime and cross-product extraction: unnecessary. Test-only
Bubblewrap/desktop-file-utils/GTK/GIO/SVG-loader packages were approved as new CI tools.
No CMake, shipped compiler, dependency pin, format, namespace, host ID, AppId, version,
DSP/state/parameter/GUI/latency source or existing C++ test is changed. No legacy aliases.

## Validation commands and evidence limits

```sh
python3 scripts/check-linux-desktop.py --self-test
python3 scripts/check-linux-desktop.py
python3 scripts/test-icon-preflight.py
python3 tests/linux_desktop_install_tests.py
python3 tests/linux_desktop_install_tests.py --staged-dir dist/Anamorph-Linux --report build/linux-desktop-install.json
scripts/preflight.sh
```

The source checker pins approved bytes, names, provenance and RGBA dimensions. Its
self-test tests good/altered/missing inputs; an independently disabled digest comparison
is killed by the altered-PNG probe. Local preflight always runs the new source gate
without rebuilding. Six actual shell preflight regressions include a corrupt Linux PNG.

Entry-point tests use real shell scripts in private Bubblewrap user/mount namespaces;
there is no fallback to live developer /usr/HOME. They cover fresh/repeat/upgrade,
user/system/XDG/root behavior, exact payload copies, hostile paths, missing/altered
sources, foreign/modified destinations, source/destination ancestor links, private
ownership/receipt errors, unknown journal files, halfway failure, SIGKILL install and
uninstall/recovery, both modes and coexistence, preservation of unrelated icons/presets,
real desktop-file validation and GIO/GTK eight-size × two-scale raster/SVG lookup.
Old/new launcher bytes deliberately differ in the rollback fixture, so no-op rollback
cannot pass merely because approved icon bytes are immutable. Disabled ownership and
parent gates let real unsafe installs succeed and are caught by the same negative probes.

The shipping Linux job repeats against a clean ZIP/extraction of real staged artifacts,
checks installed binary hashes/ELF headers and retains its report. Other fixtures use
explicit harmless stand-ins and are not native product/DAW/GUI evidence. Existing native
DSP/state/pluginval/ABI lanes remain the product behavior gates. GNOME/KDE launcher
appearance, actual icon caches/window matching, real DAW recall/listening and Composer
remain manual. Current CI outcomes are scoped to exact PR heads, recorded separately.

No merge, ADR acceptance, release/tag, version bump, product deployment, icon redesign,
parameter/state/latency behavior or independent identity change was performed.

Hosted CI namespace setup: the first hosted run refused Bubblewrap UID mapping
before any installation. `--hosted-namespace-permission` now first proves its
strict repository/OS/disposable-host guard with good/bad contexts, temporarily sets
kernel.apparmor_restrict_unprivileged_userns=0 only on the GitHub-hosted Linux VM,
and restores its exact original value in finally. Local tests never change kernel
settings; self-hosted/developer/wrong-repository contexts are refused. Installation
still runs in real private filesystem/user namespaces and fails if isolation cannot
start. This enables the sandbox rather than replacing native tests or their gates.

Adversarial uninstall follow-up: at 787752f an unexpected file in the private
ownership root survived, but owned desktop resources had already been removed
before rmdir failed. Reproduced with actual entry points in isolation. The root
now admits only format-v1, receipt and transaction before any recovery/removal.
A regression requires both foreign and owned files/binaries to remain intact
on refusal. The GTK test also exercises a real parent theme inheriting hicolor,
in addition to direct lookup, at eight sizes × two scales. The suite has 20 cases.
