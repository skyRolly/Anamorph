# Hosted exact-head validation — 2026-11-15

Each JSON file is the GitHub Actions API metadata captured after completion,
including its actual `headSha`, event, job/step status and conclusion. A green
PR-event Build & Validate run is the small merge-check job with 16 matrix jobs
skipped; the separate branch-push run is the full matrix.

| Source | Full Build & Validate | CodeQL | Microsoft analysis |
| --- | --- | --- | --- |
| Main `1ade6e1a` (#242 merge) | `43701585510` | `43701585534` | `43701585556` |
| #237 `e64e11fb` | `43708830567` | `43708836084` | `43708836239` |
| #238 `f11dba49` | `43704284663` | `43704292070` | `43704292158` |
| #239 `b08d514f` | `43704285030` | `43704291909` | `43704291934` |
| #240 `22e56b32` | `43710871469` | `43710882228` | `43710882233` |
| Integration `70bbdc22` | `43710889493` | `43710898389` | `43710897923` |

All listed runs completed successfully at the stated SHA. Each full matrix
completed 16 jobs successfully and skipped three conditional jobs. Applicable
Linux, Windows, macOS and Intel/Rosetta execution, full DSP/State tests,
source and documentation lints, the 22-case provenance suite, ordinary-user
cleanup controls, allocation checks, bounded fuzz, ASan/UBSan/vptr, TSan and its
liveness controls, RTSan and its liveness controls, Valgrind/memcheck,
pluginval, packaging and ABI validation are represented by the successful
job/step metadata. CodeQL and Microsoft C++ Code Analysis passed separately.
The Rosetta DSP/State step on final integration actually executed and passed;
its exact check counts were not independently retrieved in this session.

PR merge-check runs are retained too: #237 `43708836275`, #238 `43704289191`,
#239 `43704290607`, and #240 `43710876591`. #238 and #239's recorded PR
merge-checks preceded the advance of their actual #237 base from `10ddde5e` to
`e64e11fb`; their branch-head full matrices and analyzers are exact-head
results. GitHub reports the current PRs mergeable, and final integration
`70bbdc22` contains the newer base and passes its own matrix. Retargets and
actual owner merges require fresh exact-source checks.

The API exposed job and step conclusions, but the CLI returned empty raw logs
for sampled final-source jobs and the direct job-log endpoint returned HTTP 403.
A current CodeQL SARIF artifact was listed but its download also returned 403.
The prior failure-specific cleanup and Rosetta artifacts remain inaccessible;
successful retries do not establish their causes. These access limits do not
turn job success metadata into independently inspected raw logs. Native DAW,
listening, representative real-time hardware and Level-5 acceptance were not
performed.
