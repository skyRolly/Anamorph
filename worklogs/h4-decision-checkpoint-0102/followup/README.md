# H4 consumption-aware candidate: follow-up controls — 2026-11-15

The isolated, nonshipping consumption-aware candidate and its first 40-check
positive run are committed in PR #240 at `c84ee8028eb12ee147e1c3b51d543696a26ef263`.
The engineering decision remains Proposed; this record is separate follow-up
work, not shipping source.

A freshly rebuilt negative candidate changes only the detector's selection so
that it scans the active bank even when the recorded block also processed the
other bank. The exact patch, resulting tree and original probe are in
`h4-consumption-active-only-spec.json`; its new [receipt and raw output](h4-consumption-active-only-results/identity.json)
record base `003c9a1a2558da6bd89e2624421a6e205eb8acdf`, diagnostic tree
`2ebae9d2c9be067a0e788f37e0d5cc8a6c24864b`, patch SHA-256
`76af9a06cb63039ebd162c3253c2514a84dbe3fd19f9992f2f3e947ee0c44784`, and
pinned JUCE. The runner exited zero because the selected negative executable
exited one as expected. It reported **40 checks, two failures**, specifically
that one live fade bank and the source retiring during the block were missed.
The unmutated candidate passed those same checks. This demonstrates sensitivity
to the named detector boundary; it does not establish natural occurrence of the
injected invalid states or the complete recovery contract.

## Expanded fresh candidate execution

A second fresh build of the same candidate tree executed the expanded probe,
the independent H4 matrix, and the candidate source's full DSP suite. Its
[receipt](h4-consumption-v2-results/identity.json) records source, patch,
JUCE, build, binaries, run commands, streams and the original uncompressed
sample-CSV digest. The raw CSV is retained losslessly as
`h4-samples.csv.gz`; it is a new result, not a rewritten historical artifact.
The separate committed `df89edb67eb0ea36508d85f4842b67ea99c5c10a` validator
`worklogs/h4-dry-state-0102/verify_oracle.py` was run over the recorded matrix
stream and wrote `oracle.json`; its script, input and output hashes are in
`oracle-inputs.sha256` and its exit was zero.

- The 20 Hz and 50 Hz oracle's eight actual-gate, wet-bank, re-entry,
  first-window and tail assertions passed. Peak and tail error were zero for
  these cases against its warm production-module/full-wet-twin reference.
- The source's full DSP suite passed 4,794 checks. This diagnostic source is
  historical base `003c9a1a` plus the candidate patch, not the final combined
  integration source.
- The expanded targeted probe passed **48 checks**. It checked both-channel
  wet recursive-state equality, opposite output, raw and conditioned histories,
  opposite aligned history, preservation of previously unwritten affected
  history, finite newly written aligned history, and two subsequent injected
  recoveries in the zero-latency and actual x4 wrapped/six-sample cases.
- In the first injected-invalid-state consuming block, finite affected output
  differed from the clean reference for **128/128 samples** at zero latency
  (maximum absolute difference about `0.00921214`) and **122/128** with the
  actual x4 wrapped path (maximum about `0.00838126`); all 128 newly written
  affected-channel aligned-history slots differed in each case. These are
  diagnostic differences under deliberately injected invalid state, not an
  audibility or natural-reachability claim. They establish that finite output
  and finite written history are insufficient to claim clean-reference
  reconstruction after the fault. The acceptable exceptional output and how
  previously written or newly sanitized history is recovered remain a contract
  and design question. The independently checked opposite output and valid
  unrelated histories remained equal in these cases.

The candidate remains incomplete: ordinary reconstruction and a subset of
channel-local recovery are evidenced, but a policy and independent oracle for
finite affected-channel data after invalid state, including natural exceptional
arithmetic, fading, glides, Match and repeated latency interactions, are not
established. Measurements of this exact modified candidate must not borrow the
historical A2 instruction ratios.

## Matched instruction checkpoint

The [matched instruction record](H4_INSTRUCTION_METHOD_2026-11-15.md) now
compares shipping H4, historical A2 and this candidate on the same shared host,
including actual wrapped, fade and sustained-glide premises. At 48 kHz / 128,
the candidate is approximately +1.68%, +15.41%, +27.53% and +37.01% whole-engine
instructions for one through four bands at full wet. The actual four-band fade
workload is +47.70%; x4 Drive is +15.10%; partial Mix is +0.27%; sustained
glide is +33.86%. Relative to A2 the complete candidate adds 16–386 instructions
per block in these measured cells. These are not CPU percentages, native
deadline results, allocation measurements or plugin-capacity estimates. The
unresolved exceptional-output/history contract still prevents selecting the
candidate as complete; these cost results inform that decision without accepting
ADR-0074.

The candidate-source full DSP suite also ran existing allocation Test 38 with all allocation-guard liveness checks active: 3,840 armed `setParameters`/`process` calls, 11 observed structural landings, and zero `new`/`malloc` in its measured partial-Mix/Match-enabled matrix. This is useful ordinary-path evidence, not an exceptional-recovery allocation or native real-time guarantee.

## Post-validation arithmetic boundary

In the experimental candidate, dry samples and recursive state are checked
before the Multiband Enable interpolation; the aligned-history write follows
that interpolation. A source-bound [direct production-kernel witness](h4-postguard-kernel-results/identity.json)
uses injected finite operands `-FLT_MAX` and `FLT_MAX` with an interior blend
`0.5` and observes a non-finite result. The kernel's historical interior
arithmetic is intentionally unchanged by N2-03. Thus pre-interpolation
finiteness alone cannot prove that every subsequently published history sample
is finite for all possible operands. That original witness alone did **not**
establish natural engine reachability or an actual history write. Subsequent
[unpatched-production tracing](shipping-trace-70bb-results/README.md) did
establish finite-operand overflow, aligned writes/reads and final self-heal for
a distinct extreme-input case, now N2-09. Its source is reviewed integration
`70bbdc22`, not yet the final staged main. The candidate's full recovery
contract must also protect the later dry-source interpolation. The first probe
compile failed because its namespace collided with the POSIX `access` symbol;
that failed attempt is retained separately, and the corrected probe and runner
exited zero.

## Recovery ownership and timing negative controls

A separate fresh diagnostic build used the same candidate plus test-only,
environment-selected recovery faults. The [specification, source patch,
receipt and streams](h4-recovery-negative-results/identity.json) identify all
inputs and commands. The no-fault run passed **48/0**. Each fault was applied
only after an intentionally invalid dry state was injected; the negative
executables exited one as expected and the outer runner exited zero:

| Single injected recovery fault | Result |
| --- | --- |
| Reset affected-channel wet crossover state | 6/48 failures, including both-channel wet-state preservation |
| Also reset the valid opposite dry channel | 4/48 failures in subsequent opposite-output recovery |
| Clear raw bypass history | 2/48 failures in raw-history preservation |
| Clear aligned history | 7/48 failures, including opposite and unwritten history preservation |
| Omit detection/sanitization at the consuming boundary | 15/48 failures, including valid wet, conditioned and aligned state loss after later self-heal |

These controls demonstrate sensitivity to the named injected failures in the
zero-latency and actual x4 wrapped cases. The delayed-detection control does
not directly trace the intermediate ring write, and none establishes natural
reachability of the injected state or an acceptable affected-channel recovery
policy. The nonshipping faults are not production changes.

A stricter [repeated-recovery instruction experiment](H4_INSTRUCTION_METHOD_2026-11-15.md) observed active dry processing, affected-state reset and sanitized samples in all 256 injected blocks. It measured +117 instructions per 128-sample block for this artificial repeated recovery, both without wrapping and with actual x4 Drive wrapping, relative to the same candidate's normal workload. This is not a natural event rate or worst-case deadline. GCC/x86-64 DWARF reports the same 1,672-byte `MultibandWidth` size in the three matched builds; that is not a portable peak-memory or ABI guarantee.


## Recovery policy and actual history checkpoint — 2026-11-15

The [recovery policy comparison](H4_RECOVERY_POLICY_2026-11-15.md) separates
existing contracts, the owner's design objectives, and an unapproved
exceptional-output policy. A [one-sample boundary trace](history-step-70bb-results/README.md)
observed actual aligned writes, latency-offset reads and first Mix consumption:
injected invalid state changed 32/32 affected history samples and 32/26 output
samples at zero/x4-six-sample latency. Opposite-channel and checked unrelated
histories remained equal in that case. A finite history is not a clean-reference
history. The result is injection evidence, not natural reachability.

The [candidate trace](post-enable-trace-v2-70bb-results/README.md) and
[unpatched production trace](shipping-trace-70bb-results/README.md) independently
identify N2-09: finite opposite-sign dry Enable endpoints can overflow during a
fade, enter aligned history and trigger self-heal. A diagnostic exceptional-only
widening prevented those invalid writes in the selected case but exposed a later
finite-endpoint dry-source interpolation overflow. The latter remains a
counterfactual observation until independently reproduced without the fallback.
No production correction or exceptional policy is authorized; ADR-0074 remains
Proposed.

A [bounded unpatched-production search](shipping-secondary-search-70bb-results/README.md)
found no secondary finite-operand dry-source overflow in 216 extreme-input
cases, including 108 actual x4 wraps. This does not prove unreachability; the
diagnostic Enable-widening control remains positive for that later boundary.

A separate [conditioned-dry fallback specification](h4-conditioned-fallback-70bb-spec.json)
tests a temporary affected-channel policy at an injected first-consumption
boundary, with a switch-off negative control. The specification is not a result
or production policy; its runner outcome and limitations must be recorded
separately before drawing a conclusion.
The first specification commit's raw patch included blank context lines that
`git diff --check` reported as trailing whitespace. Before execution those
context lines were represented as equivalent delete/add pairs; an independent
temporary-index application confirmed the identical diagnostic tree. Both
specification revisions remain in Git history; the later digest is the one to
execute.

The [executed conditioned-dry fallback](conditioned-fallback-70bb-results/README.md)
passed its 30-check injected first-consumption policy oracle; switch-off,
false-clean-history and invalid-history controls were sensitive. Its affected
history still differed from the clean twin, and neither the complete recovery
contract nor native cost is established.

An [actual-main candidate replay specification](h4-candidate-main230-spec.json)
binds the same nonshipping candidate to integrated main `230da0e2`, including
focused recovery, history-boundary controls, full DSP and a separately evaluated
low-frequency CSV oracle. The specification itself is not a result.

The [actual-main candidate replay](actual-main230-results/README.md) now
reproduces the 48-check recovery probe, 28-check history trace, sensitive
negative controls, 5614-check DSP suite and eight low-frequency oracle
assertions on `230da0e2`. A wrong-input oracle attempt is retained separately.
The affected-history and policy limitations remain.

The subsequent [actual-main live-fade collateral experiment](fade-collateral-main230-results/README.md)
passed 277 scoped checks and observed 24/24 resets of valid same-channel
state in the other live dry bank. Its false-preservation and fallback-disabled
negative controls each failed 24 assertions as intended. The allocation guard
observed no allocations in those selected 24 injected recovery calls, with
its liveness checks active. These are deliberate fault injections and do not
establish natural reachability, complete recovery or a real-time deadline.
The affected-channel policy and ADR-0074 remain unaccepted.

The subsequent [bank-local candidate](bank-local-main230-results/README.md)
preserved the other live bank in 24/24 otherwise matched injected recoveries;
its observer passed 277 checks and the opposite-claim and no-fallback controls
were sensitive. The output-only fault path remains conservatively two-bank,
and the changed affected-channel trajectory, post-Enable publication, full
recovery contract and native cost remain unestablished. It is nonshipping.
