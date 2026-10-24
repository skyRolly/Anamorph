# ADR-0065 — Owned Linux desktop/hicolor resource installation

**Status:** Proposed. On 2026-10-24 (Asia/Shanghai), before implementation, the owner
reviewed the concrete paths/ownership/recovery/CI-dependency proposal and answered
**“Approve Linux implementation and testing.”** That clears the implementation-stage
stop for this proposal, including the new CI test tools. Formal independent human
architecture review, ADR acceptance and merge authorization remain pending. The
earlier ADR-0062/0063/0064 approvals do not authorize this decision automatically.

## Context

Main `e76a4a66e6ed50f3ade53be9717c2c002865e3c4` includes merged native icons,
namespace and Windows installer changes. Its Linux scripts install both VST3 and
Standalone in user or system mode, with established independent binary staging,
parked-copy reconciliation and uninstall safeguards. The approved icon source main
`203bb326d02fcddeb4c9989c65cf44010f1eaff6` includes merged #12's canonical desktop/
hicolor names. Linux ELF has no macOS-style embedded application icon contract.

## Problem

Install the approved launcher and hicolor files safely, resolve the real executable
without a desktop-session PATH assumption, and extend process-interruption recovery
without deleting foreign resources or weakening binary replacement.

## Options

- Manual desktop setup only: leaves the requested integration unfinished.
- Unconditional copies/deletes at matching names: cannot establish ownership.
- A new Python/runtime library installer: unnecessary on the supported shell baseline.
- **One shell resource helper, exact hash-owned paths and a recoverable journal:** proposed.
- Replace the binary transaction or extract cross-product shared packaging: unnecessary scope.

## Decision

- Vendor the exact ten source resources under `resources/icons/linux`, with approved
  source SHA, source-manifest digest and per-file SHA-256 provenance. Preserve every
  PNG/SVG byte, optical composition and canonical master. No network asset fetches.
- Ship `desktop-assets/` and `desktop-integration.sh` beside the existing scripts.
  Extraction/portable use installs nothing. The helper uses POSIX shell/text tools and
  Linux core utilities, with no Python/GTK/Bubblewrap product runtime dependency.
- User data root is absolute XDG_DATA_HOME, otherwise HOME/.local/share. Ignore relative
  XDG_DATA_HOME with a diagnostic; trim trailing slashes. HOME must be absolute. Control characters are refused; dot/redundant path components normalize lexically
  only after original ancestry has passed the symlink/ownership checks. Root --user remains refused.
  System data root is /usr/local/share, independent of root HOME/XDG environment.
- Preserve binary destinations, mode/default/privilege semantics and both required
  payloads. Install applications/tech.rolly.anamorph.desktop and nine exact hicolor
  images, using approved raster sizes plus scalable SVG. Shared theme roots stay intact.
- Preserve the source desktop bytes. Generate only installed Exec with the actual
  absolute app path, quoted/escaped under the Desktop Entry rules, including %%.
  Use `/usr/bin/env --` before that path: native GIO checks the first executable before
  field expansion and otherwise rejects a valid app in a literal-percent HOME.
  env is part of the existing coreutils set, invokes no shell and execs the absolute
  app without PATH lookup. The real GIO launch test covers this measured adjustment.
  Do not invent StartupWMClass or alter GUI/window/plugin identity.
- Manage a private 0700 `.tech.rolly.anamorph` directory under the chosen data root.
  Its receipt binds exactly ten destinations to their installed hashes. Refuse foreign
  files without a receipt, modified owned files, symlinks/unsafe ancestors, wrong
  owners/modes and malformed/unknown journals. A marker does not authorize deleting
  unexpected journal entries. No broad theme, other-product or preset cleanup.
- Preflight validates assets and destinations before binary writes. Copy inputs into
  private staging and recheck their pinned hashes before generating/publishing anything.
  Journal old/new resources and receipt; publish via same-directory regular scratch
  files and atomic renames. Errors/signals roll resources back; SIGKILL is reconciled
  on the next install/uninstall. Validate current hashes before rollback; preserve
  foreign changes and backups if recovery cannot safely complete. This is process-
  interruption recovery, not a filesystem power-loss durability claim.
- Binary replacement stays independent as before; a resource failure exits nonzero
  and reports no successful integration, while valid newly installed binaries may
  remain. Retry reconciles resources. Same-root operations must be serialized; a live
  transaction PID is refused. User/system resources and receipts remain isolated.
- Uninstall verifies all resource ownership first and removes only exact owned files;
  retry tolerates already missing owned files. Keep foreign/modified files and presets.
  Use the same XDG root as installation. Optional desktop/icon cache refresh failures
  are visible warnings; no shared theme directory/cache is deleted to force a pass.
- Add test-only Bubblewrap, desktop-file-utils, Python GObject/GTK and SVG-loader
  dependencies in CI. Test real shell entry points and extracted customer archives in
  private user/mount namespaces. Source self-tests precede checker use in every job;
  customer artifact upload requires the installed-resource regression gate.

## Consequences

There are ten new managed desktop/icon paths plus private ownership/recovery state.
Strict refusal needs an operator to preserve/move a foreign or edited file before
retry; there is no silent adoption. Existing VST3/Standalone rollback behavior,
supported formats, namespace, RTec/Anmr, class UIDs, AppId, version, DSP/state/GUI,
parameters and latency stay unchanged. No legacy package identifier handling.

GTK/GIO parser/launch/icon lookup is automated, including 1×/2×. GNOME/KDE appearance,
window association, cache behavior, actual DAWs and other desktops remain manual.
No embedded ELF icon, Icon Composer or native visual certification is implied.

## Related code

`packaging/linux/{install.sh,uninstall.sh,desktop-integration.sh}`,
`resources/icons/linux/PROVENANCE.json`, `scripts/check-linux-desktop.py`,
`tests/linux_desktop_install_tests.py`, `.github/workflows/build.yml`.

## Evidence and confidence

Source hashes, exact installed-resource checks, actual isolated shell commands,
GIO escaped launch and hicolor lookup are Verified within their recorded test scope.
The worklog records failed alternatives, mutation/recovery controls and actual CI
heads. Full native/GUI claims require their own results. See
[the worklog](../../../worklogs/2026-10-24-linux-desktop-integration.md).
