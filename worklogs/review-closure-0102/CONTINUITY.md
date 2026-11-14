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
