# ADR-0069 — VST3-only Linux installation and legacy desktop cleanup

**Status:** Proposed; implementation, validation and PR preparation explicitly
authorized by the owner on 2026-10-31. Human review and manual merge remain required.
This replaces ADR-0065 for the corrected 0.10.0 candidate. The existing tag and
release retain their original source until a separately approved rebuild/replacement.

## Context

Before PR #175 (`fc7428e` and its follow-ups), the Linux scripts staged VST3 and
Standalone binaries with user/system selection and per-operation system elevation.
PR #175 added a shared desktop helper, ten hash-owned launcher/icon resources and
a whole-ancestry permission gate. A group-writable icon directory, including a
personal group's mode 0775, prevented both per-user installation and removal before
binary writes. System mode used a different data root and could still work.

## Problem

Restore convenient installation without requiring directory chmod/chown commands,
remove desktop integration, and preserve safe replacement and ownership boundaries.
The owner explicitly selected VST3-only installation with portable Standalone in
the ZIP, and requires these corrections in 0.10.0 rather than a new version.

## Options

- Require manual icon-directory repair: rejected by the task.
- Automatically chmod/chown shared icon trees: unnecessary and affects other software.
- Keep desktop publication but relax its privileged destination checks: retains an
  unwanted installation model and expands privileged writes.
- **Install only VST3 and retire verified old resources:** selected.

## Decision

- Keep VST3 at `~/.vst3/Anamorph.vst3` or `/usr/lib/vst3/Anamorph.vst3`.
  Preserve prompts, explicit mode flags, root user-mode refusal and per-operation
  sudo in system mode. Never elevate user-mode writes to HOME.
- Preserve same-filesystem staging, parked previous bundles, ordinary-signal rollback,
  killed-swap recovery and explicit `--discard-parked`. Setting the installed module's
  executable permission must succeed before publishing the staged bundle.
- Keep Standalone built, validated and shipped for portable use. Do not install it
  in a bin directory. Windows/macOS, plugin IDs, DSP, state and version remain unchanged.
- Remove the desktop helper, Linux-only desktop/icon source payload, packaging source
  gate and desktop-specific tests. Preserve shared JUCE PNGs, platform assets and tools.
  No AppStream metadata was staged by the preceding implementation; the new archive
  gate rejects desktop/AppStream payloads as well as unexpected top-level members.
- Embed the same literal migration code in both trusted entry scripts. It never
  interprets an old archive helper or publishes resources. User cleanup runs as the
  caller; system cleanup runs through sudo against fixed system-local paths.
- A real private 0700 old ownership directory and exact v1 ten-path receipt authorize
  unlinking only regular, single-link files with matching owner and SHA-256. Reject
  linked ancestry and foreign owners. User cleanup can unlink those owned leaves in
  group/other-writable icon parents without changing the shared directories. Privileged
  cleanup retains strict ancestor ownership/write checks.
- Missing owned files permit retry. Retire a receipt only after its resource set is
  gone and only the checked empty format marker remains. Preserve edited, unowned,
  linked or malformed resources and unknown/interrupted recovery state with warnings;
  those warnings do not block VST3 operations. No broad directory/journal sweep.
- Remove old installed/staged Standalone only on an exact known executable digest:
  `4beedffbcedbe6c14b0a9ecb2c0c51110b32c10ed8de477bd9e9f9525811ca8e`.
  This is the executable from existing release asset 690966946, archive SHA-256
  `d2aea303302bc4b1e975494588a1e25e50b5471d3916b42c7852ffa005bb9963`.
  Other binaries at those names and the portable archive payload are preserved.
- Replace obsolete GUI integration validation with mandatory package and real-UID/
  sudo installer tests on disposable hosted-VM private roots. Keep same-job liveness,
  staged-byte checks, archive extraction, negative/mutation probes and fail-closed
  customer upload. Remove test-only desktop-file/GTK/SVG dependencies.

## Consequences

Shared icon permissions no longer determine whether an audio plugin can be installed
or removed. Migration needs no manual permission repair or ownership reassignment.
Unverifiable legacy files and journals may remain for review; this deliberately avoids
claiming authority over edited/private unknown content. Desktop caches can retain a
stale display until the desktop refreshes; cleanup does not rewrite shared caches.
Same-mode operations must be serialized; no filesystem power-loss durability claim.

These are corrected 0.10.0 candidate files. After human review and merge, rebuild from
the exact merged source, repeat applicable release/native/manual checks, and replace
0.10.0 under separate authorization. This task performs no merge, tag mutation,
release editing or publication. Prior manual approval is not certification of this
changed Linux workflow.

## Related code

`packaging/linux/{install.sh,uninstall.sh,INSTALL.txt}`,
`scripts/check-linux-package.py`, `tests/linux_install_tests.py`,
`tests/linux_install_native_tests.py`, `tests/linux_install_native_plan_tests.py`,
`.github/workflows/build.yml`.

## Evidence and confidence

- Historical binary workflow: `fc7428e^:packaging/linux/install.sh`; PR #175.
- Current implementation and executable preservation/fault/retry cases in the files above.
- Existing 0.10.0 archive downloaded read-only and checked against its API digest;
  release asset identity is also recorded in
  `worklogs/release-0.10.0-p1/draft-replacement.json`.
- Local namespace tests are functional shell evidence, not real-host sudo or DAW proof.
  Native CI results and release limitations are recorded in the release-readiness worklog.
