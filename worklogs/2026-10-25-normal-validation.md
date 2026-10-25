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

The TSan lane uses the disposable `ubuntu-22.04` runner image and checks its
existing shadow-memory capability before the unchanged canary and tests. It never
lowers ASLR entropy. An incompatible runner fails visibly; selecting this test-only
image changes no shipping compiler, dependency pin or product artifact.


Optional native macOS integration review consumes independently identified finished
package/staged artifacts through the direct Build manual input `integration-review`.
The JSON manifest names each product's repository, run ID, exact artifact head and
independently measured PKG SHA-256. Its default is empty; reusable Build calls define
no input and Release passes none. Normal validation always retains its full gates.
Seven selection jobs per product use separate fresh hosted Macs, measured choice
IDs and a selection-aware expected receipt/bundle set. The production default-full
exact-three assertion is unchanged. The Anamorph manual review also runs both real
two-product installation orders on separate clean Macs, requiring all six receipts
and preservation of the first product's metadata, listings and installed hashes.
These optional evidence consumers introduce no sibling build dependency, no live
host guard spoof and no receipt forgetting or pre-existing product deletion.
`python3 scripts/test-macos-integration-review.py --self-test` exercises pure fixture
validation only; native installs require the real disposable host guard and actual
identified artifacts. Archive digests are checked against GitHub metadata, and PKG
bytes against the independent manifest digest, before native installation.

The test-only TSan job uses Ubuntu 24.04 container userspace on the Ubuntu 22.04
host so the existing dependency setup and pinned Clang build use the proven package
set. A read-only host capability check precedes setup and retains its actual value.
Both the sanitizer canary and all state probes/suppression checks remain mandatory;
no kernel setting or container security profile is changed. The first Jammy-native
attempt failed during dependency setup; its exact package error was unavailable
through the permitted GitHub read tools, so no specific cause is asserted.

The containerized TSan run reached the liveness canary, which failed. The gate
remains red; passing the entropy precheck is not sanitizer certification. Fresh
test-owned canary output/exit status is retained as structured diagnostic evidence
alongside the host capability artifact. The binary, compiler flags, required race
message, probes and suppression checks are unchanged; no failure is suppressed.


TSan workspace correction (2026-10-25): artifact 12580933816 at head
632d8917a1c1435e65fd82456fa1c24bebe3719f records exit 66 because the container
could not read the suppression file at the host workspace path. Resolve the
suppression file from the checked-out container working directory and export the
same sanitizer flags before the unchanged canary/probes. The production environment
body is exercised in an inert private directory containing spaces with an invalid
host workspace path. It executes no sanitizer, compiler, installer or privileged
operation. Keep the real canary mandatory and retain its output/options; this
fixture does not establish sanitizer liveness. No suppression rule, compiler pin,
container security profile or kernel setting changes.
