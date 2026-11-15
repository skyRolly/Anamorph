# Final review and decision record — 2026-11-15 UTC

This record separates the reviewed production corrections, provenance
infrastructure, historical reproduction, integration, H4 architecture and
release decisions. No PR was merged, auto-merge enabled, prerequisite branch
deleted, H4 production code changed, ADR accepted, or release prepared.

## Sources and dispositions

- Main `1ade6e1ad128b067930ab216bc355605388eb1de` is the two-parent merge of
  #242 `73c68f7a` into `51e6250d`; all post-merge hosted gates passed.
  R-242-01 (coverage documentation), R-242-02 (deterministic handshake), and
  R-242-03 (identity-independent owned-group finalization) remain resolved.
  The original `d8627e55` hosted cleanup failure remains unattributed.
- #237 `e64e11fbf54b910aa6a32acafbeb4c3f64815f20`: R-237-02 was confirmed
  against the actual prior verifier. The selected model rejects diagnostic
  additions, deletions, renames, symlinks and type/mode changes; content-only
  patches with identical tracked regular paths remain supported. R-237-03 was
  confirmed by an actual receipt-controlled include-shadow build. Generated
  include trees, packet inputs, dependency files, objects, rules and binaries
  are now checked within the documented Linux/Ninja trust model. Actual shadow,
  changed generated file, substituted object, changed include environment and
  shared-header mutation controls reject; clean and reviewed engine-only
  relinks pass. The baseline and `live_toggle` fresh replays match their
  historical 16-row streams. See [controls and receipts](provenance-2026-11-15/CONTROLLED_RELINK_RESULTS.md).
- R-237-04, a newly found P2 verification defect, was confirmed when full State
  printed `16555 checks, 0 failure(s)` and exited zero but the original control
  parser rejected that spelling. The failed runner, raw output and exit are
  retained under `final-integration-0226e033/isolated-attempt/`. The focused
  parser fix accepts real DSP/State formats and rejects malformed summaries;
  22 synthetic guards, actual-output re-parsing, a fresh 17-run replay and
  exact-head hosted checks pass. This was not a product-test failure.
- R-237-01's **maintained-infrastructure component is resolved within its
  explicit supported input model and trust boundary**. Its original broader
  historical-reproduction scope remains open: historical one-off H4
  trajectories, recovery and benchmark records not individually replayed
  remain historical evidence with their original premises. The retired
  unverified per-PR relinkers remain disabled. This remaining historical work
  does not itself block the independently validated N2 corrections.
- #238 `f11dba49b9ff68c48f96d1b4435c9464a1d7bb24` retains the focused N2-02
  implementation. At source `a24b4efd`, 720 engine and 20 processor controls
  pass; isolated N2-02 restoration fails 316 and 12 respectively while
  N2-03 remains 392/0; premature forced adoption fails eight. The earlier
  Match/reset probe passed 56 and its N2-02-only restoration failed 16.
- #239 `b08d514f9f147fd8c1b19e24ec0cba2f9ae57ef6` retains N2-03 and the
  corrected Match-on production-output oracle. R-239-01 is resolved within
  that stated contract: 392 checks pass, both/wet/dry mutations fail 88/44/44,
  and the separately fresh-built clamp fails one while N2-02 remains 720/0.
  The Match-on test does not independently prove the complete Level-Match
  measurement algorithm and makes no audibility claim.
- #240 `22e56b32e02420502cf9b091bf8b04baad2c340e` is a documentation and
  nonshipping evidence package. Its ADR remains Proposed and links the later
  independent H4 follow-up. Integration of that documentation does not accept
  the ADR or authorize a production candidate.

The maintained replay checks raw tracked source bytes rather than trusting
status, binds selected patch bytes and intended trees, rejects unexpected
inputs, checks the pinned effective JUCE tree, generated inputs, target objects,
commands and declared outputs. Unsupported patch path changes fail closed.
Trusted Git/compiler/system headers and libraries, local configuration,
filesystem behavior and no concurrent writers remain premises. These controls
are not a cryptographic attestation of tools, the host or historical binaries;
a patch hash does not prove experimental intent. Declared stdout markers and
side artifacts are hashed; exit-only runs are identified as such and are not
numerical oracles. Historical reports, original manifests, measurements and
sources remain byte-for-byte unchanged.

## PR and integration state

All four open PRs are non-draft, mergeable and have auto-merge disabled. There
were no unresolved GitHub review threads or submitted reviews at the final
query. Their actual bases are `main` for #237 and the #237 branch at
`e64e11fb` for #238–#240. Each exact head passed the full hosted Build &
Validate matrix, CodeQL and Microsoft C++ Code Analysis. Full matrices ran 16
jobs and skipped three conditional jobs. Detailed source, job and step metadata
and run IDs are in [hosted validation](final-hosted-validation-2026-11-15/README.md).
The #238/#239 PR merge-checks predated the latest base advance; their branch
head matrices are exact, and final integration separately includes the latest
base. Owner review and fresh checks after retarget remain required.

Final isolated integration `70bbdc22acdfd6286d13b1a15cb445367525c5e5` includes
current main and all four PR heads. Its full hosted matrix, CodeQL and Microsoft
analysis passed. Its production source, tests and replay tools are byte-identical
to `a24b4efd`, where a fresh local replay completed all 17 controls with inner
exit 0, DSP 5,614/0 and State 16,555/0. The final source's separate local
code/test equivalence and exact hosted results must not be relabeled as a local
execution at the later Git identity. The original audit and merged fixture are
unchanged. [Integration record and conflict decisions](final-integration-70bbdc22/README.md).

GitHub has `delete_branch_on_merge=true`. Before owner-authorized #237 merge,
prevent automatic prerequisite-branch deletion or explicitly retarget all three
children first. The exact sequence, conflict checks, merge-method implications
and revalidation points are in [the owner procedure](INTEGRATION_PROCEDURE_2026-11-15.md).
No owner merge has been authorized by this assignment.

## H4 checkpoint

The separate nonshipping branch `codex/0102-h4-recovery-followup` contains the
updated Proposed ADR and candidate evidence at `worklogs/h4-decision-checkpoint-0102/followup/README.md` on that branch
(head `bbf941bd978502387c7ff22907fe0026c65d1e61`). N2-01 and V2-01 remain
P2. A2 output-only detection misses hidden invalid recursive state. A distinct
injected state is actually consumed at first Mix re-entry; the later extreme
zero block is separately traced to N2-08 wet LR4 overflow, not attributed to
A2. The consumption-aware candidate passed the eight-assertion low-frequency
oracle, 4,794 source-scoped DSP checks and 48 focused checks; its injected
fault produced 128/122 affected-channel differences from a clean reference at
zero/x4 latency. Single-fault controls detected wrong wet/opposite-channel
reset, raw/aligned history clearing, missed timely recovery and a missed live
fade bank. A direct kernel witness shows finite injected extreme operands can
overflow the later Enable interpolation; actual engine reachability and an
intermediate history write remain unproved.

Matched shared-host whole-engine instruction costs for the new candidate at
48 kHz/128 full wet were +1.68%, +15.41%, +27.53%, +37.01% for one through
four bands. The observed four-band fade workload was +47.70%, x4 Drive +15.10%,
partial Mix +0.27% and sustained glide +33.86%. Relative to matched A2 the
complete candidate added 16–386 instructions/block in these cells. A separately
observed repeated injected recovery cost +117 instructions per 128-sample
block with and without actual x4 wrapping; it is not a worst-case deadline
bound. Existing allocation Test 38 passed 3,840 ordinary partial-Mix/Match
calls with live guards; matched GCC/x86-64 DWARF class size was 1,672 bytes in
all three builds. [Measurement methods and limitations](H4_INSTRUCTION_METHOD_2026-11-15.md).

The evidence supports an informed owner choice to continue a bounded
first-consumption/history and exceptional-output policy investigation, retain
the documented limitation, or defer. It does not support unqualified
production selection of any candidate. Full valid-history reconstruction after
invalid state, post-interpolation history safety under actually reachable
inputs, representative native timing/deadline behavior and broader Match,
enable/glide/latency recovery remain unproved. No universal CPU percentage,
plugin capacity, audibility or native acceptance follows from instruction
counts.

## Remaining work and release

N2-04 through N2-07 remain separate P3 findings; N2-08 is a separate P3 review
assessment with an existing JUCE bit-exact LR4 contract. Their future
assignment order and contract boundaries are in
[N2 remaining maintenance](N2_REMAINING_MAINTENANCE_2026-11-15.md). They are
not silently closed or all asserted as automatic release blockers.

The 0.10.2 release is not ready: remaining PRs are open; the owner must choose
and authorize integration, explicitly decide or defer the H4/ADR question and
remaining findings, and complete applicable native, DAW/listening/manual and
Level-5 acceptance plus release authorization. The current task did not run or
claim those acceptances. Raw final job logs and SARIF downloads were
inaccessible; hosted job and step success metadata remains available and is
recorded without inventing raw-log contents.
