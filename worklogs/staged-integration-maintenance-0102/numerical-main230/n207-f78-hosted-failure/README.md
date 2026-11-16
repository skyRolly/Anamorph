# N2-07 PR predecessor: citation self-test failure

PR #245 head `f78b53208623fbd7702194e41bd931d708a837fa` had hosted
source-lint job `133227653209` fail at **Citation-gate self-test (recognition,
mapping and rewriting)** in Build & Validate run `43846435928`. Job metadata is
retained. At initial retrieval the overall workflow was still in progress. After it was
cancelled by the newer push, the job-log redirect returned Forbidden; no
unobserved log content is inferred.

Running the same self-test locally on that exact head reproduced four failures:
automatic citation re-anchoring moved ADR-0005's dry-bank citation, but four
`DELIBERATE_REAIMS` targets in the checker still named the prior span. Current
PR head `44ef025aab46e09a8be7069184ce79071b7d5a09` updates those targets; the
local self-test then passed 267 cases and the normal citation check passed 603
anchors. This changes documentation and checker declarations, not production
or test bytes. Hosted validation of the new head is separate and must not be
inferred from these local results or an earlier source identity.
