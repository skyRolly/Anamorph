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
