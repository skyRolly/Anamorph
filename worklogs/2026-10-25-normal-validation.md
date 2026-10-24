# Normal validation restoration — 2026-10-25

The owner authorized normal project validation on disposable runners. The temporary
publication-only router and deliberate failure marker are removed. Existing event
and deduplication conditions, normal build target coverage and security analysis
permissions are restored. CodeQL runs the existing complete Linux dependency setup.
The routing regression reads production files and covers direct Build and Release
manual invocation on both review branches, including their event/ref collision.
It also checks tags, main, unrelated branches, forks and the absence of invocation
inputs/nested calls; real Release metadata runs only in inert private fixtures.

The previous published workflow fails the review-branch Release regression; the
restored workflow selects the normal matrix. Manual rehearsals must never select
release creation, including from tag refs. No real rehearsal, release, tag, merge
or deployment is required by these fixtures. Normal CI outcomes are separate
runtime evidence and must be read at the actual final candidate.

No dependency pin, product code, artwork, version or identity changes. Accepted
ADR records remain Accepted. No separate code review or native/manual check is
claimed by this source change. Kernel/security settings are never changed: any
missing host capability fails visibly. Earlier publication evidence remains history.

The #175 native driver reuses the complete isolated entry-point assertions and adds
actual per-operation sudo/already-root selection, implicit root mode, root --user
refusal and real protected-snapshot shell observation. Root-owned read-only VM-private
overlays repair fixed-PATH fault injection without production helper, digest or PATH
changes. All-ten receiptless paths, normal signal cleanup, deterministic cleanup cuts
and retry/uninstall are required. The real staged tree and ZIP round trip are checked.
The constant failure placeholder is replaced with this real gate; upload still depends
on its actual success. Execution evidence must be obtained on the final candidate.

The TSan lane uses the disposable `ubuntu-22.04` runner image and checks its
existing shadow-memory capability before the unchanged canary and tests. It never
lowers ASLR entropy. An incompatible runner fails visibly; selecting this test-only
image changes no shipping compiler, dependency pin or product artifact.

The test-only TSan job uses Ubuntu 24.04 container userspace on the Ubuntu 22.04
host so the existing dependency setup and pinned Clang build use the proven package
set. A read-only host capability check precedes setup and retains its actual value.
Both the sanitizer canary and all state probes/suppression checks remain mandatory;
no kernel setting or container security profile is changed. The first Jammy-native
attempt failed during dependency setup; its exact package error was unavailable
through the permitted GitHub read tools, so no specific cause is asserted.

The first real native-gate attempt exhausted the former 40-minute Linux job
budget without a completed report or customer upload. Its observer launched Python
for every dirname call; the test overlay now filters snapshot paths in shell before
starting observation. Atomic per-operation progress remains explicitly incomplete
and ineligible until the full suite finishes. Each test-owned subprocess is bounded
at 45 seconds, the native driver at 30 minutes, and the Linux build/test job at
60 minutes. No cut or ownership/runtime claim is inferred from the timed-out run.
