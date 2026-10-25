# ADR-0065 — Owned Linux desktop/hicolor resource installation

**Status:** Accepted. The owner directly confirmed on 2026-10-25 that human
architecture review and approval are complete for this existing decision,
including the delivered protected-helper snapshot amendment.
This is the confirmation and record-update date; the original approval date
was not separately specified. Merge/publication, separately required code
review and unexecuted tests are not approved or certified by this confirmation.

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
- Before retiring a committed journal, verify the public receipt and all ten
  installed resources. Write a private cleanup plan outside the journal that binds
  the public hashes and exact journal entries. Delete only those entries; a retry
  verifies the surviving plan and public state even if `format-v1`, `ready`,
  `committed`, or `new-receipt` was already removed. Unknown or changed contents
  are preserved rather than swept up.
- Binary replacement stays independent as before; a resource failure exits nonzero
  and reports no successful integration, while valid newly installed binaries may
  remain. Retry reconciles resources. Same-root operations must be serialized; a live
  transaction PID is refused. User/system resources and receipts remain isolated.
- Uninstall verifies all resource ownership first and removes only exact owned files;
  retry tolerates already missing owned files. Keep foreign/modified files and presets.
  If the receipt is absent, inspect all ten destination names (including dangling
  links) before the caller removes either binary. Any occupied name is a conflict;
  a fully absent resource set permits an idempotent uninstall.
  Use the same XDG root as installation. Optional desktop/icon cache refresh failures
  are visible warnings; no shared theme directory/cache is deleted to force a pass.
- Add test-only Bubblewrap, desktop-file-utils, Python GObject/GTK and SVG-loader
  dependencies in CI. Test real shell entry points and extracted customer archives in
  private user/mount namespaces. Source self-tests precede checker use in every job;
  customer artifact upload requires the installed-resource regression gate.

## Defensive helper capture follow-up (2026-10-25)

The earlier maintenance instruction authorized focused implementation and
ordinary-only checks; the delivery did not itself confer architecture approval.
The owner's direct confirmation on 2026-10-25 now accepts this existing amendment
as part of ADR-0065. Historical reports remain evidence, not approval records. No new runtime dependency, build
architecture, archive member or installation mode is introduced.

Previously, system preflight, publication and uninstall interpreted the replaceable
archive helper through `priv sh`. Its asset hashes could not authenticate code
already being interpreted. Both entry scripts now contain the same literal
bootstrap and reviewed SHA-256 of the complete helper. At each system operation,
including an already-root invocation, the bootstrap rejects missing/nonregular,
linked or group/other-writable input, creates a fresh root-owned 0700 snapshot
under checked root-owned `/tmp` (sticky if writable), captures a 0600 single-link
file, verifies its captured bytes, then interprets only that protected file.
Source metadata checks are hygiene; the embedded digest and protection through use
are the code boundary. Changing the source path or bytes after capture cannot
change the verified input. A later operation captures/verifies again and refuses
a mismatch, including between preflight and publication.

The snapshot passes the original `desktop-assets` directory as the helper's fourth
argument. That directory remains data: existing pinned manifest, exact asset hashes
and private copied-input checks still apply. Direct user invocation retains the
beside-helper default. Snapshot cleanup removes only its exact helper file and
fresh directory on exit/error/ordinary signals; it preserves consumer failure.
SIGKILL may leave a private snapshot; no discovery sweep/adoption is added. Durable
journal cleanup and receiptless-uninstall refusal are unchanged.

**Distribution authenticity assumption:** the entry scripts (including their literal
bootstrap/digest), the shell/core utilities, root and the protected system ancestry
are trusted. Entry code must remain trusted while it is read. This change provides
no signature or authenticity proof for a downloaded distribution, and cannot defend
against replacement of the trusted entry scripts or a hostile process with the same
authority as root. **Replacement protection:** writers of the separate helper or its
archive ancestry can cause refusal but cannot select different interpreted bytes
without defeating SHA-256 or the protected snapshot. Source ownership alone is not
treated as authentication. No demonstrated distinct-principal escalation follows
from the historical report; that original claim remains unproven.

`tests/linux_desktop_helper_tests.py` exercises the production selection/bootstrap
through a recording privilege executor using inert bytes, real temporary files,
links/modes/hashes and exact-path cleanup. Root ownership metadata is modelled; the
recorder replaces final interpretation and never elevates or executes fixture code.
These are defensive implementation and nonprivileged regression evidence only.
Actual root/sudo ownership, shell execution, asset resolution/publication, uninstall,
signal/interruption recovery and installed-resource validation remain unverified on
this local revision. The existing Linux customer-archive hold stays in force.

## Runtime validation reconciliation (2026-10-25)

The preceding unverified-runtime statement records the original local delivery
scope. It is superseded for the bounded tested contract at
`0a667695aa1684eef07c0bd085dc3d39c3a78c73` by actual disposable-VM evidence:
[run 40147299614](https://github.com/skyRolly/Anamorph/actions/runs/40147299614),
native artifact 12582266259 (ZIP SHA-256
`d16c68c3284ea57e6632bb91d475622a23e2e35c91687dd7f22150ed0b035830`).
All 29 tests completed with zero failures/errors. The real ordinary UID/sudo/root
preflight returned 1001/0/0. There were 317 actual root-owned protected snapshots
(0700 directory, 0600 single-link helper, reviewed pinned bytes and original assets
argument), 93 reached cleanup cuts (62 user, 31 system) and six signal cases.
System preflight/publication/uninstall, already-root and real per-operation sudo,
implicit system selection, root user-mode refusal, coexistence, retry/uninstall,
receiptless refusal and unknown-state preservation were exercised in private roots.

Customer artifact 12582391207 has ZIP SHA-256
`c75b84fa50c4bef01ed0a082b54c62dcbb8a2c8f9fd727032ccc3c56e9942103`;
all 19 entries match that native run's staged hashes and modes. The actual gate
controls upload and remains fail-closed for an incomplete or failed later run.
This is functional native evidence at its recorded head, not a release approval or
automatic certification of a later candidate. The ordinary 12-test recorder still
models root metadata and does not supply this native proof. Distribution-authenticity
assumptions and the unproven distinct-principal exploit claim remain unchanged.
GNOME/KDE appearance and real DAW recall remain separate manual work. The decision
stays Accepted under the owner's existing confirmation; no new decision is made.

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
`tests/linux_desktop_install_tests.py`, `tests/linux_desktop_helper_tests.py`,
`.github/workflows/build.yml`.

## Evidence and confidence

Source hashes, exact installed-resource checks, actual isolated shell commands,
GIO escaped launch and hicolor lookup are Verified within their recorded test scope.
The worklog records failed alternatives, mutation/recovery controls and actual CI
heads. Full native/GUI claims require their own results. See
[the worklog](../../../worklogs/2026-10-24-linux-desktop-integration.md).

Earlier on 2026-10-25 (Asia/Shanghai), the owner narrowed that review round to ordinary
validation and deferred sudo, privilege-boundary and adversarial testing. The Linux
customer archive gate remains closed at the follow-up head until isolated installed-
resource validation can run again. The later direct confirmation accepts this ADR; it does not waive that gate.
