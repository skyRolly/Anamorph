# 0.10.2 review closure — starting checkpoint

Created before new investigation on 2026-11-14. The complete previous execution record is `worklogs/independent-review-followup-0102/REVIEW.md` on `codex/0102-independent-review-record`. The identities below are supplied historical review references, not claims about current refs or final validation.

| PR | Reviewed head | Scope |
|---|---|---|
| #237 | `32e60cb6` | Evidence preservation and partial provenance hardening |
| #238 | `61e82e51` | N2-02 production correction |
| #239 | `383353c0` | N2-03 correction and Match-on regression |
| #240 | `039b122d` | H4 decision package and evidence |
| #242 | `915c2fa3` | POSIX cleanup fixture handshake |

Reviewed main `51e6250d34b44d838a51f9a5086105d39d6b0f29`; prior combined `f2686cb5b0b3687a3e343340f2494f59961aa83a`; historical combined `7764df5c2d02eee7a11c475fcca93097f9559dee`. #241 was already merged. Supplied update: #239 macOS rerun `43572823816` completed successfully; verify remotely. Re-fetch heads, actual base refs, mergeability, review threads, CI and logs. Prototype `codex/0102-provenance-prototype` at `af7d4059372e855b236b17b86ff167c204155358`, implementation `efe12d176da0b5fad8c748ed8bb95daee69c0e78`, is unintegrated and its synthetic controls are not provenance closure.

## Independent review items

- **R-242-01:** test lifecycle requires coverage audit as well as testing documentation. Check other PRs for the same drift.
- **R-242-02:** the 250 ms delayed-parent control is scheduler-dependent; replace with bounded, explicitly synchronized positive and negative controls that establish causality and clean up owned processes.
- **R-242-03:** investigate whether missing/malformed identity after leader exit leaves an owned child alive, unreaped, or reported incorrectly. Preserve strict fixture ownership, bounded cleanup and process-local subreaper behavior. Prior hosted failure's inaccessible artifact does not establish its cause.
- **R-237-01:** source/build provenance remains incomplete across replay and relink tools. `git status` can miss a modified tracked file marked assume-unchanged; examine index flags, raw tracked bytes, sparse trees, dependencies, diagnostic patches, source/configuration/objects/probe inputs. Evaluate and integrate or replace the prototype; preserve original historical evidence byte-for-byte. Run real production/diagnostic replays and isolated mutations as well as synthetic controls.
- **R-239-01:** Match-on output regression was corrected with 392 assertions; historical controls reported 88 original, 44 wet-only, 44 dry-only and one clamp failure. Revalidate with trustworthy replay; keep this coverage distinct from the entire Level-Match algorithm.
- **N2-02:** preserve the focused implementation unless new evidence establishes a defect. Review Match/reset coverage and run isolated original-defect and premature forced-adoption controls on final/integrated source.

## H4 boundaries

ADR-0074 is **Proposed**. No production H4 correction, recovery/performance tradeoff or architecture acceptance is authorized. N2-01's ordinary dry-state transient and V2-01's aggregate-RMS coverage limitation are distinct from A2's confirmed hidden invalid dry state, the separately unisolated extreme-input zero block, and the unvalidated state-scan candidate. The prior zero block also occurred after dry reset, without a Mix dip and on unpatched production; do not ascribe it to A2 or dry re-entry without causal evidence. Preserve the earlier distinct A/C dry-consumption experiment. Trace production stages, state ownership and guards; assess candidate recovery, cost and lower-cost alternatives in nonshipping worktrees. Do not infer representative native timing or DAW capacity from shared-host instruction or timing figures.

## Remaining scope and authorization

N2-01 P2, V2-01 P2 verification and N2-04–07 P3 remain open unless independently established evidence changes their disposition. Preserve current product version, JUCE 9.0.5 pin, parameter IDs, mappings, serialization, identity, accepted ADRs, Test 25 thresholds, and original evidence. Keep PRs open; do not merge, enable auto-merge, delete prerequisite branches, tag, release, bump version or perform release preparation. A proposed ADR document is not architecture acceptance. Separate correctness, verification, integration, owner architecture and release decisions. Record exact-head local and hosted results, inaccessible artifacts, unsupported environments and all unresolved work; do not claim native DAW, listening, representative hardware or Level-5 acceptance without evidence.

## Portable H4 benchmark procedure — proposed, not yet executed (2026-11-14)

The committed specifications at `4298c70a165d9e16e1a061eb560342377124f3c3`
select the historical `003c9a1a` baseline and the exact historical A2 patch,
the same four committed benchmark probes, a fresh full test-target build and
separate wall, thread-CPU and Callgrind runs. They are a portable starting
procedure for a representative Linux owner machine, **not new measurements**
and not evidence that A2's recovery is complete. Their end-to-end execution
has not yet been validated. Later recovery candidates need their own reviewed
patch, expected resulting source tree and specification; a hash alone does not
prove the candidate's intended behavior.

On a representative machine, record CPU model/topology, memory, OS/kernel,
compiler and CMake/Ninja/Valgrind versions, Release flags, power/thermal and
scheduler conditions, load, JUCE tree, source/spec/patch identities and all
commands. Use a trusted compiler, Git, filesystem and system libraries, no
concurrent writers, a pinned clean JUCE `d60b7a78030661574a615b586d9325ba7a7fa7dd`,
C++23 build dependencies, Valgrind headers and a clean worktree per variant.
Keep output and build directories outside every checkout. For example, from the
repository after fetching the evidence, H4 and review-record branches:

```sh
git worktree add --detach /tmp/anamorph-h4-base 003c9a1a2558da6bd89e2624421a6e205eb8acdf
git worktree add --detach /tmp/anamorph-h4-a2 003c9a1a2558da6bd89e2624421a6e205eb8acdf
git worktree add --detach /tmp/anamorph-h4-tools 738b7b23211bb4c26258b3f8912efdad03ea4556
git show df89edb67eb0ea36508d85f4842b67ea99c5c10a:worklogs/h4-review-followup-0102/candidate-a2-003c.patch > /tmp/anamorph-a2.patch
git -C /tmp/anamorph-h4-a2 apply --unidiff-zero /tmp/anamorph-a2.patch
python3 /tmp/anamorph-h4-tools/worklogs/dsp-numerical-audit-0102/replay_experiment.py \
  --checkout /tmp/anamorph-h4-base --juce /absolute/clean-juce --out /tmp/h4-base-1 \
  --spec-commit 4298c70a165d9e16e1a061eb560342377124f3c3 \
  --spec-path worklogs/review-closure-0102/h4-benchmark-baseline-spec.json
python3 /tmp/anamorph-h4-tools/worklogs/dsp-numerical-audit-0102/replay_experiment.py \
  --checkout /tmp/anamorph-h4-a2 --juce /absolute/clean-juce --out /tmp/h4-a2-1 \
  --spec-commit 4298c70a165d9e16e1a061eb560342377124f3c3 \
  --spec-path worklogs/review-closure-0102/h4-benchmark-a2-spec.json
```

Use new output directories for every repeat and a balanced baseline/A2/A2/
baseline order; the runner refuses pre-existing outputs. Retain each receipt,
stdout, stderr and exit status. Hash and retain the generated `callgrind-*`
files separately: the runner records the command and executable but does not
hash these side files. The wall probe reports ns/sample, p99, maximum and
re-entry maximum; the thread probe reports thread CPU and wall ns/sample; the
count probes collect whole-engine instructions, with a separate proved-glide
control. Report block cost with its block size, and compare only matched
workloads and toolchains. Timing includes the recorded harness boundaries and
cannot establish DAW instance capacity or deadlines by itself.

The current wall/thread matrices cover 2–4 bands, two rates and block sizes,
full/partial Mix and actual x4 Drive; they do not independently establish a
one-band wall distribution, Match engagement, a bank-fade timing distribution,
exceptional recovery cost, allocation behavior or a complete tail/re-entry
oracle. The proposed instruction runs add a one-band row and x4/partial-Mix
rows, but are unexecuted. Before an owner selects a candidate, add reviewed,
sensitive workloads for those missing cases and the chosen recovery mechanism,
then repeat correctness, allocation and real-time gates. Do not extrapolate
historical shared-host timings or A2 instruction counts to a modified candidate.

## Review closure execution — 2026-11-14 UTC

This section records the new review work; the starting snapshot above remains
historical. The latest directly resolved refs at this checkpoint were main
`51e6250d34b44d838a51f9a5086105d39d6b0f29`, #237
`738b7b23211bb4c26258b3f8912efdad03ea4556`, #238
`f08d4ebfdc3710f3d3cae961a8d2c43a74c85fec`, #239
`30560d330a707f6087f14baacace78ffeac9b4df`, #240
`df89edb67eb0ea36508d85f4842b67ea99c5c10a`, and draft #242
`73c68f7af90fe0d437ea2087d3cb48f9153b7b89`. #237/#242 target main;
#238–#240 target the actual #237 branch. All five were open, mergeable,
auto-merge disabled, with no review submissions or unresolved review threads
returned by the API. A successful pull-request workflow containing only
`merge-check` is not full exact-head validation. #241 had already been merged;
the prior review verified its CodeQL Action SHA against upstream v4.39.3.

### Findings and correction boundaries

- **R-242-01, documentation lifecycle:** confirmed and corrected in #242. The
  live testing and coverage inventories now record the fixture and its tests;
  the pre-edit drift and documentation/citation checks are preserved there.
- **R-242-02, scheduling-dependent negative control:** confirmed and corrected
  in #242. Parent publication is held until observer acknowledgement, and a
  separate child gate proves whether readiness preceded publication. Paused
  observer, missing acknowledgement and timeout controls use deadlines as
  bounds, not as the ordering premise. The old ordering is detected before the
  parent is released, while the corrected ordering produces a complete
  parseable identity before readiness. Normal and concurrent-load repeats are
  retained in #242's `worklogs/cleanup-fixture-race-0102/`.
- **R-242-03, identity-unavailable cleanup:** confirmed as a test-harness
  weakness, not a production Rosetta defect. Cleanup now obtains ownership from
  its direct new-session child and, after leader reaping, only its process-local
  subreaper's group-specific child wait. It uses bounded group termination,
  reaping and observation; it does not discover or signal processes by name.
  Publication, parse/read, timeout, leader-exit and cleanup/reaping fault
  injections include a sensitive control for the old false-success predicate.
  The old hosted failure `d8627e55` remains unattributed: its retained cleanup
  artifact was still forbidden on the 2026-11-14 retry, and `--log-failed`
  returned an empty stream. The confirmed local race is not proof of that
  hosted failure's cause.
- **R-237-01, P2 verification/provenance:** confirmed and partially corrected,
  **still open across the complete requested scope**. A real Git
  assume-unchanged counterexample was reproduced; its details are in
  `assume-unchanged-counterexample.json`. The prototype was adopted and
  strengthened in #237 into a shared raw-byte/tree, diagnostic-patch, pinned
  JUCE, fresh-build and receipt-checked relink mechanism. Seventeen synthetic
  controls pass in the documented container, and real baseline/diagnostic,
  combined mutation, A2 and state-scan executions have separate receipts.
  The old per-PR N2 and H4 relinkers are disabled before output creation, with
  normal and optimized-Python refusal controls. Historical code bodies and
  archived measurements remain preserved. Several H4 historical recovery,
  trajectory and benchmark workloads still need verified specifications and
  actual reruns; the owner benchmark specifications above are unexecuted. The
  complete helper classifications and trust premises are in #237's
  `worklogs/combined-isolated-controls-0102/INVENTORY.md`. Trusted tools,
  system dependencies, filesystem and no concurrent writers remain premises;
  this is not a general compiler/host attestation.
- **R-239-01, P2 verification:** the Match-on production-output gap was
  confirmed and corrected in #239's existing 392-assertion test. A correct
  standalone kernel cannot conceal an incorrect wet call site: on historical
  combined `f2686cb5`, guarded isolated both/wet/dry mutations fail 88/44/44
  assertions while N2-02 remains 720/0. A separately fresh-built clamp mutation
  fails 1/392 and leaves N2-02 720/0. Some wet endpoint bit differences survive
  later rounding (750 Match off, 747 Match on); others are masked. The Match
  oracle uses the actual post-measure target and is not an independent test of
  the entire measurement algorithm. R-239-01 is resolved at the assertion
  level; the broader evidence-provenance dependency remains R-237-01.
- **N2-02, #238:** no new production defect was established. Ordinary entry,
  fade-out/bottom/fade-in, retargeting, structural/factor transitions, forced
  completion and deferral, independent sample clocks and processor response
  are exercised by the focused tests. Guarded combined N2-02-only mutation
  fails 316/720 engine and 12/20 processor assertions while N2-03 passes;
  premature forced adoption fails 8/720. Existing State tests 133 and 136
  cover broader measurement/reset/re-prepare interactions. The subsequent
  dedicated Match/reset probe and its isolated N2-02 mutation are recorded
  below. No unrelated product correction was made.

### H4 and remaining product findings

N2-01 and V2-01 remain P2 production and verification findings. Test 25's
original .90/.95 aggregate-RMS assertions remain intact; the independent
failing-on-main low-frequency oracle remains nonshipping. A2's output-only
recovery is incomplete: actual engine and separate LR4 witnesses show finite
monitored output with hidden invalid dry state. A state-scan experiment catches
that case only; its complete bank/fade/latency/repeated-recovery matrix and
cost are unproved. The later zero block is distinct: on `f2686cb5`, full-wet
and dry-reset controls plus a stage trace locate an extreme-finite wet LR4
numerator overflow followed by existing final-output healing. The copied-state
one-step widening shows representable first-sample arithmetic. Track this as
**N2-08, P3 review assessment**, without an audible-impact or production-fix
claim. The earlier A/C candidate-only Mix-consumption failure remains a
separate observation. N2-04–07 remain open and are not silently closed.

The historical A2 48 kHz/128 whole-engine instruction increases are about
15.3%, 27.4% and 36.9% at two, three and four bands. They do not measure a
state-scan candidate, CPU percentage, deadlines or plugin-instance capacity.
The prior candidate comparison rejects an unbounded re-entry replay, finds a
small state-only saving and identifies the architecture gate for changing the
filter representation or transition contract. No representative native
hardware, listening, DAW or Level-5 acceptance was established. ADR-0074
remains Proposed. Recommend continued targeted recovery and matched native
measurement before selecting a production candidate; the owner may also retain
the documented limitation or defer correction.

### Integration boundaries

The isolated intended integration `9e9725726b31c39b8de0379c12c324b2e858e503`
contains all five current PR heads and current main; production `src/` bytes
match historical combined `f2686cb5` exactly. Against `7764df5c`, the
meaningful executable/test changes are the expanded Match-on endpoint test,
merged #241 workflow pins and #242 fixture controls; H4 source changes are
comments only. Original audit files under `original/` compare byte-for-byte to
`6b55dfad`. Conflict resolution retained both production corrections, test
registrations, state lint, ADR amendments, current N2-08 documentation and
both sides of the coverage/testing records. Current pairwise PR file overlaps
are in `pr-overlaps-2026-11-14.json`: #238/#239 12, #238/#240 8, #239/#240 8;
#242 overlaps each of those three in two documentation files.

Proposed order remains #242, #237, #238, #239, #240, subject to exact-head
checks and the owner's authorization. Keep #237's branch until all three
children have been explicitly retargeted. After each authorized merge, retarget
the next child to the actual current main, inspect its resulting diff and
conflicts, preserve both tests and all documentation amendments, and rerun
required checks on the resulting exact identity. #240 documentation can be
reviewed independently of an H4 production decision; merging it does not
accept ADR-0074. No PR was merged, auto-merge enabled or branch deleted.

The exact combined local and hosted checks in flight are tracked separately in
`integration-9e-results/`; prior green checks on `f2686cb5` are historical.
The local source/documentation/fixture batch has 35 passes and the separate JUCE
cache suite has 28 passes. The first guarded full-build attempt correctly
refused ignored Python caches; the retry uses a clean checkout. Raw CodeQL
SARIF remained unavailable (HTTP 403). Native/manual acceptance and current
hosted platform, sanitizer, fuzz, ABI, pluginval and analyzer results must not
be inferred from earlier heads.


### Additional N2-02 Match/reset control (2026-11-14)

A new nonshipping probe uses a verified receipt for the fresh `9e972572`
combined build and rechecks its source, pinned JUCE, generated rules, compiler
environment, executable and every reused link object before and after relinking.
Across Bypass and Multiband Enable, both initial directions, and host reset or
same-rate re-prepare with a changed sound, it establishes a real ordinary duck,
nonzero published Level-Match gain, live target adoption, an independent 64-tick
smoother clock, settled targets and finite post-reset output. Host reset keeps
the published gain; the changed-sound re-prepare flushes it. The corrected source
passes **56/0**. Restoring only the N2-02 preservation defect, with N2-03 still
correct, produces **16/56 failures** in the eight live-adoption and eight clock
assertions; the reset, gain-policy and finiteness assertions remain passing.
The probe is not a complete independent Level-Match algorithm oracle or a
replacement for processor automation and the full State suite. Exact probe,
patch, receipt, binary, commands, outputs and exit statuses are in
`n2-match-reset-results/`; the negative executable's exit 1 is expected, and
both outer relink runners exited 0.

### Exact combined-source mutation replay (2026-11-14)

The previously fresh-built `9e972572` receipt was independently checked before
controlled relinks. The nonshipping selector wrapper uses byte-identical copies
of the three committed N2 selector headers; its support hashes, commands,
linked-object identities and raw outputs are in `isolated-selector-results/`.
The actual combined test binary separately passed both registered selectors.
On this exact source the wrapper records:

| Isolated source | N2-02 DSP | N2-03 endpoint |
|---|---:|---:|
| Correct combined source | 720 / 0 failed | 392 / 0 failed |
| Restore only N2-02 | 720 / 316 failed | 392 / 0 failed |
| Premature forced adoption | 720 / 8 failed | 392 / 0 failed |
| Restore both N2-03 call sites | 720 / 0 failed | 392 / 88 failed |
| Restore only wet call sites | 720 / 0 failed | 392 / 44 failed |
| Restore only dry call sites | 720 / 0 failed | 392 / 44 failed |

Each selector's recorded exit matched its expected positive or negative result.
The wrapper's base outer exit was observed as 0; the encompassing five-mutation
shell session became unavailable after completion, so its aggregate exit was
not observed. The independent processor mutation was executed on historical
`f2686cb5` (12/20 failures), whose product and test bytes relevant to it match
the current combined source; it was not rerun as a separate final-head State
mutation. The header clamp similarly has a fresh `f2686cb5` diagnostic build,
not an exact-`9e972572` diagnostic build. These limits do not alter the exact
combined source's passing 20-check processor and 392-check endpoint selectors.

### Source-bound H4 channel recovery control (2026-11-14)

New fresh diagnostic builds reproduce the earlier A2 opposite-channel check
with verified source, patch, pinned JUCE, build, probe and binary identities.
Across four repeated extreme finite left-channel stimuli, the traced A2 guard
reports finite input and wet output, four left dry heals, zero right dry heals,
zero final-wet heals and **4/4** right dry-integrator comparisons equal to an
independent reference. A paired mutation changes only the recovery call to
reset both dry channels; all those premises and counters remain the same while
right-state matches become **0/4**. This is a sensitive recovery-ownership
control for this one workload; it does not establish the complete A2 recovery
contract or preservation of every other state family. The separately copied
nonshipping trace support changes counter linkage so a full target can be
built before its probe; the original diagnostic sources are untouched.
Receipts and raw outputs are in `h4-channel-recovery-results/`. The positive
outer runner exit 0 was observed; the negative probe's exit 1 was recorded as
expected, but its outer session became unavailable after completion. The
historical instruction ratios still apply only to the original output-guard
candidate, not the state-scan or these instrumented binaries.

### Exact-head hosted validation and #240 exception

The exact `9e972572` integration passed hosted Build & Validate (16 successful
jobs, three conditional skips), CodeQL and Microsoft C++ Code Analysis. Hosted
metadata records the Linux, macOS, native Intel macOS and Windows builds and
suites, pluginval, packaging and ABI checks; ASan/UBSan/vptr and Valgrind;
fuzz; and TSan/RTSan with liveness controls. The fuzz findings upload and two
optional macOS review jobs were skipped under their workflow conditions. The
Rosetta step has a capability-based skip inside a successful step, so without
raw logs its execution cannot be inferred solely from the green metadata; the
native Intel job is separately successful. Raw SARIF was inaccessible.

Exact-head Build & Validate, CodeQL and Microsoft analysis also passed for
#237 `738b7b23`, #238 `f08d4ebf`, #239 `30560d33`, and #242 `73c68f7a`.
#240 `df89edb6` passed CodeQL and Microsoft analysis, but its first Build &
Validate attempt `43623171585` failed in the macOS `DSP + state self-tests,
x86_64 slice under Rosetta` step. The job's other executed steps and all other applicable jobs succeeded; the
plugin and installer upload steps and the post-cache step were skipped. The raw job log was empty and the macOS debug
artifact download was forbidden, so the failing assertion or infrastructure
cause cannot be established. The failed attempt metadata is retained in
`pr240-hosted-failed-attempt.json`; a failed-job exact-head rerun was requested.
Do not erase or attribute the initial failure without evidence. An earlier
intermediate integration `26b39ef2` hosted workflow also concluded cancelled;
its partial successes do not validate the later combined source.

### Superseding H4 documentation and integration checkpoint

A subsequent drift review found that the dated `NEXT_RELEASE_DEVELOPMENT_ROADMAP`
still read as a current A2 recommendation. Local #240 commit
`b49c1345d07396524618d6be517c21edfa4beb4f` labels that recommendation
historical and updates the H4 decision packet, follow-up, testing inventory and
Proposed ADR-0074 with the source-bound repeated recovery control and its limits.
The isolated integration commit
`3669cf716b27de56dd125f29d0b2672598aebdc2` includes that local #240 commit.
Its only differences from `9e972572` are those six documentation files; `src/`,
`tests/`, scripts and workflows are byte-identical. The earlier exact `9e972572`
green results are therefore useful historical evidence, but are not reported as
hosted validation of the new identity. A new fresh-build mutation/full-DSP
execution and local documentation/fixture checks have started for `3669cf71`.
The separate full State/isolation and header-clamp checks remain to be run.

At this checkpoint the remote #240 ref is still `df89edb6`: its rerun of the
previously failed Rosetta step is in progress. The new documentation commit has
not yet been pushed so that the existing rerun can finish without being
cancelled by the workflow's per-ref concurrency policy. Once its outcome is
recorded, push the new #240 and integration heads and request their exact-head
Build & Validate, CodeQL and Microsoft analysis. Preserve the original failed
attempt regardless of the rerun outcome. No merge or architecture acceptance
is authorized.

### Dry-bank lifetime fault-injection control (2026-11-14)

A nonshipping 13-check probe executed the real `MultibandWidth` module from
verified combined source `9e972572`, relinked against its receipt-checked fresh
build. It injected a NaN directly into selected private recursive states; this
is a deliberate state fault, not proof that each injected state arises under
ordinary input. With three bands, an invalid idle-bank dry state was neither
consumed nor erased during settled processing; a discrete crossover jump then
copied the active bank over it before an actual fade and produced bit-identical
outputs to a clean reference. Injecting the invalid state in the active bank
before the jump propagated it to both fade banks and contaminated only the
observed dry channel, while wet and the other dry channel remained finite.
Separate injections into the source and destination during an active fade each
reached the corresponding dry output. An injected second-section `dax` state,
unused by the allpass method, did not affect the observed output even through
the fade. All **13 checks passed**, with outer and probe exits 0.

This supports the specific bank-copy, active/fade consumption and unused-state
premises behind a possible restricted detector. It does not prove which states
are reachable after finite stimuli, recovery completeness, aligned-history or
oversampling behavior, repeated failures, or Mix re-entry safety. The source,
packet, receipt, executable and raw outputs are in `h4-bank-lifetime-results/`;
`MultibandWidth` source is byte-identical in the newer documentation-only
integration `3669cf71`. No shipping instrumentation was added.

The `3669cf71` integration has since been pushed to the isolated validation
branch. Exact-head hosted runs are Build & Validate `43649435367`, CodeQL
`43649442113` and Microsoft C++ Code Analysis `43649444689`; their outcomes
must be read from those runs, not from `9e972572`. The local #240 documentation
head remains unpushed while the earlier failed-run retry completes. The new
integration's 35 local source/documentation/fixture commands all passed; the
fresh full build, mutations, State isolation and clamp replay are separate
pending executions at this checkpoint.

### Replay inventory supplement

The shared inventory in #237 groups the original historical command records,
N2/H4 retired relinkers, data oracles, fresh-build runner, receipt-checked
relinker and combined mutation runner. The later `replay_lr4_witness.py` is a
maintained, source-checked build of one committed LR4 probe and its committed
minimal JUCE stub; it does not establish a full JUCE or engine execution.
`test_replay_provenance.py` is a synthetic guard test, not a product replay.
The source-specific `record_state_suite.py` and `checks-command-record.py` in
this review record are one-off execution records for their stated source and
premises, not alternative maintained general provenance systems. The committed
benchmark specifications describe a planned shared-runner execution, not a
measurement. Original `original/*.sh`, comparison scripts and archived data
remain historical. This supplement does not close R-237-01: several historical
H4 recovery/trajectory/benchmark scenarios still lack verified actual reruns.

The #240 `df89edb6` failed-job rerun has since completed successfully, including
the Rosetta step and remaining macOS gates. The original attempt's exit-1
annotation and inaccessible detailed log remain recorded; a passing retry does
not establish whether the first failure was a product, test or infrastructure
problem. The retry metadata and annotations are in `pr240-hosted-rerun.json`
and `pr240-hosted-rerun-annotations.json`. This retry validates `df89edb6`, not
the later documentation-only `b49c1345` head.
