# Engineering Review Programme — the standing worklog

**What this is.** The persistent record of the ongoing engineering review & improvement
programme: an evidence-driven, round-based sweep of the whole repository — problems first,
then fragile/incomplete areas, then optimisations — with every significant finding
adversarially verified before it is acted on or filed. One section per round, newest first.
The rendered companion `ENGINEERING_REVIEW_REPORT.html` beside this file is the **live
dashboard** — a VIEW of this worklog, updated and re-committed whenever findings, decisions,
implementations or the roadmap materially change. This worklog is what a later round
reconciles against (the repository's standing worklog rule, `docs/REPOSITORY_MAP.md`).

**Finding IDs.** `ER-<LENS>-<NN>` — lens ∈ {DSP, RT, STATE, GUI, TST, CI, DOC, DEP}, NN from
the round that raised it. IDs are stable across rounds and are what code comments, KI/RISK
entries and CHANGELOG notes cite.

**Method (every round).**
1. Parallel read-only investigation lenses over the subsystems, each primed with the policies,
   `KNOWN_ISSUES`/`FUTURE_RISKS`/`POSTMORTEMS`, the Accepted ADRs and prior worklogs, so known
   or deliberate behaviour is not re-reported.
2. Every finding of severity medium+ gets independent adversarial verification (2 verifiers
   for high/critical — one correctness lens tracing the code path, one context lens checking
   for deliberate/known/rejected status) before it may enter this record as more than a note.
3. Triage per finding: fix now / investigate / regression-test / docs / CI / optimise / defer /
   reject — with Architecture-Review-Gate items **filed, never unilaterally fixed**.
4. Implementations land with regression tests exercising the real code path, the triggered doc
   syncs (`DOCUMENTATION_LIFECYCLE_POLICY`), and full validation before push.
5. Negative results (areas inspected and found sound) are recorded — they are the map of where
   NOT to dig next round.

---

## Round 2 — 2026-08-31 — CI recovery, the activation defect, and two confirmed silences

**Entry state:** round 1 merged to the branch; CI **red** on two warning gates; five carried
roadmap items; three maintainer decisions open. **Exit state:** CI gates fixed at source, three
confirmed defects fixed with regression coverage, RISK-007 measured, D-1 materially corrected,
one new issue filed.

### What was fixed

| ID | What | Evidence that it was real |
|---|---|---|
| CI gates | `-Wshadow-uncaptured-local` (Clang) / `-Wshadow` (GCC) from round 1's `adoptIfAnamorph` lambda shadowing `xml`; four `-Wfloat-equal` from bare `!=` on floats in Tests 43/44/46 | The gate output itself. Fixed at source, **no baseline widened**: the lambda parameter renamed, the comparisons moved to `juce::exactlyEqual` (JUCE's helper for deliberate exact comparison, already the idiom in `state_tests.cpp`). Re-verified with a pinned clang-22 rebuild (no NEW warnings, 17 accepted sites) and a GCC rebuild (1 shadow site in `PluginProcessor.cpp`, the pre-existing baselined one, down from 2) |
| **ER-DSP-06** (new) | **Every activation ducked the audio to near-silence for ~35 ms**, and a restored session additionally opened at the wrong level for ~20 ms | Measured through the real wrapper, before and after. Before: min block RMS / settled = **0.0014** (fresh instance) and **0.0011** (restored), first block **2.4×** too loud. After: 0.982 / 0.983 / 1.000. State test 16 |
| **ER-STATE-03** | **A `value="nan"` in a session or preset silenced the plug-in permanently**, and round-tripped through save | Measured: output peak **0.000000** over 8 blocks before the fix, 0.699720 after. State test 17, which also drives the preset path through a real poisoned file |
| ER-STATE-04, ER-GUI-02, ER-CI-02/03/04/05/06 | Seven verified comment/diagnostic corrections | Each checked against the pinned JUCE or the actual workflow before editing; see below |

### ER-DSP-06 — the root cause was an ordering contract, not the reported symptom

The review item said `snapSmoothers()` "may capture stale engine defaults". That is the symptom.
`AnamorphEngine::prepare()` settles the **whole** engine from its own snapshot `p` — it reads
`p.bypass` and `p.mbEnable` directly, then runs `updateDerived()` and `snapSmoothers()` from it —
so prepare()'s contract is *"`p` is already what the host wants"*. `prepareToPlay` called
`prepare()` first and pushed the parameters in afterwards, breaking that contract on every
activation.

The consequence was **universal, not restricted to restored sessions**, and this is the part the
report did not contain: the engine's struct defaults and the snapshot the wrapper builds disagree
on a discrete field even for a brand-new instance. `dimMode` is the always-active one — the APVTS
choice defaults to index 1 and `toEngine` maps choice→mode as `index + 1`, so the first snapshot
says 2 while `EngineParameters::dimMode` is 1. (Advanced sessions add `mbEnable`: APVTS `true`,
struct `false`. The rest of the Advanced block is gated off in Simple mode and keeps the struct
defaults by design — `toEngine`'s `if (advanced)`.) A discrete difference is exactly what the
click-free switch machine reacts to, so **every** activation got the ~6 ms fade to silence +
~28 ms fade back in that a real settings change deserves.

Fix: `AnamorphEngine::primeParameters()` — adopt a snapshot wholesale, no duck, no ramp — called
from `prepareToPlay` before `prepare()`. Valid precisely because nothing is audible yet, and
documented as **not** a substitute for `setParameters` once audio flows. Two false starts are
recorded here because they cost time and would cost it again: an assertion that `Mix=0` must be a
bit-exact null through the processor is wrong when the multiband allpasses are engaged (the
phase-matched dry is not the input), and a level assertion on an engaged Dim-D session measures
the algorithm's delay lines filling from empty, which is correct behaviour, not a duck.

### ER-STATE-03 — round 1's mechanism was half wrong

- **REFUTED:** `raw="nan"` never reaches the parameter. `reassertParameters`' write gate
  `|norm - current| > 1e-6` is **false** when either side is NaN, so the raw branch is dead on NaN
  — it neither injected the value nor repaired it.
- **CONFIRMED, different ingress:** JUCE's own `apvts.replaceState()` →
  `updateParameterConnectionsToChildTrees` → `setDenormalisedValue` → `setValueNotifyingHost`
  reads `@value`, and its `approximatelyEqual` guard is likewise false for NaN. Second, fully
  independent ingress: `PresetManager::applySoundTree`, which had no gate at all.
- **Impact is not cosmetic:** a NaN continuous parameter latches its smoother target, every output
  sample goes non-finite, and ADR-0009's *sample-level* self-heal then zeroes the block and resets
  the engine on every block — permanent silence with plausible-looking controls, persisted by
  `getStateInformation` writing `nan` straight back out.
- **Fix:** two guards, because the families are disjoint. `reassertParameters` substitutes the
  parameter default for a non-finite value **and** its gate becomes a negated `<=`, so a NaN on
  either side counts as "differs" and is repaired rather than skipped — that inversion is what
  makes it a repair of `replaceState`'s damage rather than a filter. `applySoundTree` takes the
  fallback it already uses for an absent child.

### R2-2 — RISK-007 is now measured

`AnamorphStateTests --state-thread-probe` (committed; never run by the suite, because if the risk
is real then running it *is* the undefined behaviour) drives host `setState`/`getState` from one
thread against the editor tick's reads on the main thread. Under ThreadSanitizer it reports **four
data races**, on exactly the members round 1 reasoned about: `abActive` (write in
`setStateInformation` vs read in `canUndo()`), the `abUndo` vector's internals twice
(`UndoStacks::operator=` vs the main thread's `empty()`), and a `juce::String` reference-count
exchange against a `String` copy. **The code half of D-2 is settled**; what remains is the host
question (VST3 forbids it; the macOS AU does not).

### D-1 corrected — two of its candidate fixes are refuted

Re-verification narrowed KI-027 on three axes and broke two options:

- **Reachability is lower than filed.** Oversampling is **not a host parameter** — it lives in
  `InternalState` (`int_oversample`, default "Off") and no automation lane can move it. At factory
  defaults `predictLatency` is identically 0, so the expensive branch is unreachable until the user
  has selected 2x/4x/8x by hand.
- **Rate is bounded:** VST3 delivers at most one listener dispatch per parameter per block.
- **The inversion is milder on POSIX:** `juce::CriticalSection` enables `PTHREAD_PRIO_INHERIT` on
  Linux/macOS; Windows has none.
- **REFUTED — the editor's 24 Hz poll.** It is the only message-thread tick in `src/`
  (`PluginEditor.cpp` `startTimerHz (24)`, verified by grep) and does not exist with the editor
  closed, so a closed-editor render would never learn about a latency change at all — a worse,
  user-visible defect than the one being fixed.
- **REFUTED — an `AsyncUpdater`.** Its trigger reproduces the same `postMessage` (lock + possible
  reallocation + `write()`) on the audio thread; it removes only the inversion.
- **Surviving design, and what D-1 now asks for:** keep the synchronous call when
  `juce::MessageManager::existsAndIsCurrentThread()`, otherwise set one relaxed atomic flag
  consumed by a processor-owned ~20 Hz `juce::Timer`.

### New finding, filed not fixed

**KI-028 (ER-GUI-04)** — round 1's own value-box gesture fix leaks an open host gesture when the
mouse release is lost. The editor's release-outside reconcile clears the visual `dragging` flag but
cannot reach the `ScopedDragNotification`: `ValueBox` lives in an unnamed namespace inside
`LookAndFeel.cpp`. While the gesture is open `pollUndoCoalesce` commits **no** undo step. Strictly
better than the pre-0.9.6 state (no gesture at all, so no undo step ever), and the two candidate
designs are a decision, so round 3 picks one.

### Verified corrections (checked before editing, none behavioural)

- **ER-STATE-04 — CONFIRMED and worse than filed.** The comment claimed `replaceState` "swaps only
  the tree". In the pinned JUCE it propagates to the parameters, the DSP atomic, the editor's
  attachments **and** the host. The comment now states the real residual `reassertParameters`
  exists for (absent PARAM nodes; exact `raw` vs snapped `value`), so the function's necessity
  survives the correction instead of being undermined by it. Its "trade-off" clause was wrong too:
  an open editor *does* track a host restore.
- **ER-GUI-02 — CONFIRMED on reachability, but the published docs were already right.** The
  over-claim was one clause of one code comment (`cancelInlineTextEdits` has a single call site,
  behind three gates, inert on X11). Wording narrowed; the code deliberately **not** widened — a
  general "leaving the application never writes a half-typed value" guarantee is not reachable at
  that layer.
- **ER-CI-02 — worse than filed.** `build.yml`'s header still described the pre-2026-08-15 macOS
  ordering and contradicted both its own in-job comment and `CI_CD.md`. Rewritten line-count-neutral
  (2→2, 7→7) so no citation moved. Windows is now the only platform validating pre-staging.
- **ER-CI-03 / ER-CI-06** — `codeql.yml` builds with the runner's distribution g++ while claiming to
  match the Linux job (pinned Clang since ADR-0030), and its header over-stated coverage as
  "src/ + tests/" when only `tests/dsp_tests.cpp` is compiled. Comments corrected; the compiler is
  deliberately not pinned (CodeQL's alert set comes from its extractor, and pinning would be a
  Build System change for no analysis benefit).
- **ER-CI-04** — `check-gcc-warnings.py`'s exclusion label still called gcc-13.3.0 "this job's
  pinned pair" after the move to the floating `gcc:16` container. The exclusion still stands on its
  structural leg; the empirical leg is now scoped to the compiler it was measured on, with
  re-measurement a round-3 item. `GATED_FLAGS` unchanged.
- **ER-CI-05** — `release.yml` reported a transient tag fetch failure as "not an annotated tag",
  telling the maintainer to re-create a tag that was almost certainly fine. Infrastructure failure
  and verdict now say different things; both still exit 1.
- **Also corrected:** round 1's own batching rationale for the CI items ("build.yml line shifts
  re-anchor many citations") was over-cautious — only `build.yml` is citation-tracked of the four
  files, and its correction was written line-count-neutral.

### Validation at the end of round 2

`preflight.sh` exit 0. DSP suite **45 tests / 241 checks**; state suite **17 tests / 936 checks**
(924 → 936: State tests 16 and 17). Citation self-test **145 cases**, gate green against all three
bases. `check-realtime` 93 self-test cases + clean scan; `check-gcc-warnings` self-test 17;
`check-docs`, `check-portability`, `check-linux-abi`, `setup-llvm-apt` all green. Pinned clang-22
warning gate: no NEW first-party warnings.

### Round-3 roadmap (revised by what round 2 learned)

1. **KI-028** — pick one of the two designs for the leaked value-box gesture and implement it.
   Highest-priority code item: it degrades undo, and it is a residual of our own fix.
2. **D-1 implementation**, if the maintainer approves the surviving design.
3. **R2-6 / twin-dump transition scenarios** — unchanged from round 1, still on request only.
4. **ER-CI-04 re-measurement** under `gcc:16`, to put the exclusion's empirical leg back on the
   compiler the lane actually runs.
5. **ER-STATE-04.5 (new, informational)** — after a restore that omits a PARAM node, the live tree
   keeps that node without a `value` property, so the next save persists `id` + `raw` only. Not a
   defect today (the `raw` path restores it); worth deciding deliberately.
6. Deferred, unchanged: ER-DSP-05 (chorus LFO phase beyond the tested envelope), ER-DEP-06 (silent
   preset-load failure UX — maintainer-owned copy).

### Checklist (round 2)

- [x] CI warning gates fixed at source, no baseline widened
- [x] First-activation defect root-caused, fixed, regression-tested (proven to fail without the fix)
- [x] R2-1 NaN ingress: mechanism corrected, both ingresses guarded, regression-tested
- [x] R2-2 TSan: RISK-007 measured, instrument committed
- [x] D-1 re-evaluated; two candidates refuted; no implementation
- [x] ER-STATE-04 / ER-GUI-02 / ER-CI-02..06 verified then corrected
- [x] KI-028 filed with both candidate designs
- [x] Worklog + dashboard updated and committed
- [ ] D-1 decided (surviving design)
- [ ] D-2 decided — the code half is now measured
- [ ] D-3 Level-5 audition for the shipping build
- [ ] Round 3 executed

---

## Round 1 — 2026-08-31 — baseline + broad sweep

**Tree at start:** `main` @ `e8f4422` (post-PR #133: toolchain identity work). Branch
`claude/anamorph-ci-workflow-8iu7yk` restarted from it. **Baseline validation:** preflight
exit 0 in 23.4 s — 42 DSP tests / 226 checks, 920 state checks, all eight checker self-tests
green, citation gate green on all three bases, ABI floor within bounds (GLIBC_2.38 /
GLIBCXX_3.4.31 / CXXABI_1.3.9). Build tree: GCC 13.3.0 local (the pinned-Clang gates run in
CI only), CMake 3.28.3/Ninja, JUCE 9.0.1 at the pinned commit.

**Sweep shape:** 8 lenses + 1 validation-baseline agent; 33 raised findings; 19 significant
ones adversarially verified (26 verifier verdicts) → **17 confirmed, 2 refuted**; ~75 areas
ruled out as sound. The supply-chain lens re-ran as a dependency-robustness lens after a
tooling false-positive; it added 6 findings (1 medium).

### Confirmed findings and dispositions

| ID | Title (short) | Sev | Disposition (round 1) |
|---|---|---|---|
| ER-DSP-01 | `process()` trusts `maxBlock` absolutely — oversized host block overruns every scratch buffer (release heap overflow; JUCE says defend) | High→Med (host-contract-violating trigger) | **FIXED**: depth-1 chunk guard in `AnamorphEngine::process`; Test 43 pins safety + bit-exactness vs a conforming-slice twin |
| ER-DSP-02 | `prepare()` re-arms continuous smoothers from neutral — first ~5–20 ms after every prepareToPlay glides wrong (Mix-0 session opens wet) | Med | **FIXED**: `snapSmoothers()` at end of `prepare()` (post-`updateDerived`); Test 44 asserts bit-null from sample 0 |
| ER-DSP-04 | CorrelationMeter has no NaN/Inf guard (ADR-0009 bullet 3 implemented only in LevelMeters); bypass crossfade re-injects raw non-finite input past the self-heal; meter latches NaN until re-prepare | Med | **FIXED**: `sanitize()` of the six accumulators in `publish()`; Test 45 |
| ER-RT-01 | Host automation of Drive/Algorithm re-reports latency from the audio thread: ≥3 locks; on a real latency change heap append + `write()` in the Linux wrapper; plus a priority-inversion variant | High | **FILED as KI-027** — the fix is a threading-model change (Architecture Review Gate hard stop) → maintainer decision **D-1**. Code comment corrected; LATENCY_MODEL/THREAD_MODEL drift recorded in the entry |
| ER-RT-02 | Enforcement-scope hole (narrowed by verification): `setParameters`' own body (and `toEngine`) outside every tier for lock/blocking/IO classes — RTSan never sees them, the lint never seeded them, Test 38 counts allocations only | Med | **FIXED**: `check-realtime.py` seeds `setParameters`+`toEngine` (+3 self-test cases, 90→93); docstring + REALTIME_AUDIO_POLICY scoping corrected. Verifier proved `updateDerived`/`snapSmoothers` were already covered by the same-file closure — the original claim over-reached there |
| ER-RT-03 / ER-STATE-05 | get/setStateInformation mutate message-thread state unguarded; real exposure = macOS AU off-main autosave (VST3 annotates `[UI-thread]`; JUCE hosting/pluginval structurally cannot produce the window) | Med (hyp→confirmed-narrowed) | **FILED as RISK-007** + THREADING_POLICY §Host state calls (assumption documented). TSan two-thread harness = round-2 investigation; any guard is gate-item → **D-2** |
| ER-STATE-01 | PARAM nodes absent from a restored session keep the previous project's values on a REUSED live instance (policy rule 2 held only vacuously, on fresh instances) | Med | **FIXED**: `reassertParameters` applies `getDefaultValue()` for absent nodes (both notify paths; view params still re-overridden by `applyStatePreservingView`); state-suite regression on the v0.2 fixture; SERIALIZATION_REGISTRY row annotated |
| ER-STATE-02 | Parsable-but-wrong-typed A/B slot payload re-types the live APVTS on apply → every later save silently loses all 36 parameters for a fresh instance | Med | **FIXED**: `readSlot` accepts only `apvts.state.getType()` (wrong type = unparsable = slot re-seeded); end-to-end state regression (restore → `abSwitchTo` → re-save → fresh-instance restore); registry sentence extended |
| ER-GUI-01 | Value-box vertical drag is a third gesture-less edit path — no Undo step, no host change gesture; KI-010 claimed the list complete | Low | **FIXED**: `ScopedDragNotification` held across the ValueBox press (knob-drag parity); KI-010 dated correction |
| ER-TST-01 | Tests 2 & 38 ran the whole algorithm×OS matrix with `algoAmount` at its 0 identity default — the engaged wet synthesis of all four algorithms outside both the NaN/denormal and allocation invariants; Dimension-D engaged by NO assertion-bearing test | High | **FIXED**: both matrices at `algoAmount 0.7`; Test 2 sweeps dimMode 1–4 for Dimension-D. Result: all green — the engaged paths were clean, now they are *proven* clean per push |
| ER-TST-02 | The twin-dump/ADR-0032-gate bit-identity claim is steady-state-scoped; blind axes (duck/adopt, crossfades, solo, xover glide, NaN-heal) named nowhere | Med | **DOCUMENTED**: TESTING.md §Gaps coverage-boundary entry + KI-026 scope qualifier. Matrix extension = round-2 candidate (not committed) |
| ER-TST-04 | channelMode/swapLR/inputBalance/polarity + chorusRate/chorusDepth/dimMode: zero behavioural coverage anywhere (chorus family zero even at module level) | Med | **FIXED**: Test 46 — conditioning semantics pinned on the transparent chain (incl. exact polarity sign-flip) + discrimination checks (rate, depth, all six dimMode pairs) |
| ER-CI-01 | `run-pluginval.ps1` retried genuine Win32-exception crashes 3× per pass — the masking removed from macOS 2026-08-18 survived on Windows; its null-exit justification was retired by the KI-007 WaitForExit fix | Med | **FIXED**: real abnormal exit fails immediately; retry only for `$null` (launch failure). TESTING.md §retry, RISK-004, and the sh-side comment re-synced |
| ER-DOC-01 | v0.9.5 renumbering incomplete: 7 documents still named v0.9.4 as the release in preparation / first tag; HANDOVER asserted the Level-5 audition "against the build that ships" for a superseded build; stale open-KI enumeration | High | **FIXED** as part of the 0.9.6 bump: all forward-looking claims renumbered; precondition 7 restated **OPEN** (→ **D-3**); KI enumeration completed (KI-018–023, KI-026) |
| ER-DOC-02 / ER-DEP-02 | `NOTICE` (shipped attribution asset) declared JUCE 9.0.0/f8f8864 while the product ships 9.0.1/e18f7f5 | Med | **FIXED** + `NOTICE` added to the DEPENDENCY_POLICY JUCE-bump re-verification checklist so the next bump cannot miss it |
| ER-DOC-03 | CI_CD.md job inventory omitted `macos-crossslice` and the release-blocking `windows-avx2-ab`; "seven non-packaging jobs" stale (nine); REPOSITORY_MAP same | Med | **FIXED**: two table rows, count, release-blocking carve-out for `macos-crossslice`, REPOSITORY_MAP row |
| ER-DEP-01 | `NOTICE` omitted the AudioUnitSDK Apache-2.0 attribution for the macOS AU while carrying SheenBidi's under the same licence | Med | **FIXED**: AudioUnitSDK section added (© 2000-2021 Apple Inc., from the pinned tree's LICENSE.txt); THIRD_PARTY_LICENSES mandatory-notices updated |
| ER-DEP-05 | One action pin (`build.yml` crossslice checkout) lacked the trailing version comment the Dependabot review convention depends on | Low | **FIXED**: `# v7.0.1` appended |

### Refuted findings (recorded so they are not re-raised without new evidence)

- **ER-DSP-03** — "ordinary discrete duck holds silence for the rest of the block (~host-buffer
  dropout at large buffers)". Mechanism real, but **documented product behaviour** since
  [0.8.10]: the CHANGELOG entry describes exactly this bottom-dwell-until-block-boundary shape.
  Not a defect; not unrecorded.
- **ER-TST-03** — "the state fuzzer can't reach the XML space through zlib framing". The framing
  premise is false: `copyXmlToBinary` in the pinned JUCE (and in 8.0.8, the earliest ever used)
  writes magic + length + **plain UTF-8 XML**, no deflate anywhere. The fuzzer's byte mutations
  reach the parser directly.

### Ruled out as sound (the negative-results map — abbreviated; full lists in the round data)

DSP: NaN/Inf self-heal detector+reset completeness; Nyquist clamp ordering in all three
consumers; Velvet gather ring-collision proof; SoloMonitor/Multiband cold-re-entry snap
ordering; latency latch vs ring sizing; long-session accumulator boundedness; parked-path warm
history; LR4 allpass discipline; n=0/1 edges. RT: ScopeBuffer SPSC contract; engine path
alloc/lock-free end-to-end; meter publication atomics; destruction order both sides; FTZ/DAZ
scope; gate liveness proofs non-gameable. State: 36/36 registration/restore completeness;
A/B index clamp; slot reset-first overlay; preset identity round-trips; load-failure
atomicity; ±inf clamped by NormalisableRange (only NaN survives → ER-STATE-03, round 2);
save→load→save byte-stability; legacy read paths. GUI: GL lifecycle per ADR-0011; repaint
idle-gate economics; DPI layer keying; SafePointer discipline; no mutable statics;
FrameClock internals. Tests: run-tests/preflight exit plumbing; Windows self-test step;
liveness proofs; fixture regime; sanitizer plumbing; no tautologies in ~130 assertion sites.
Build/CI: ccache correctness boundary; no silent gate-skips; tested-bytes==shipped-bytes on
all three platforms; warning-baseline mechanism; windows-avx2-ab gate internals; fail-closed
discovery everywhere; release.yml validation; secrets hygiene. Docs: count claims in
README/HANDOVER/TESTING accurate pre-round (now re-synced); strictness single-authority
discipline; packaging matches documented behaviour; licensing story consistent (sole
inconsistencies = ER-DOC-02/ER-DEP-01, fixed); ADR_INDEX complete. Deps: Actions all
SHA-pinned (one comment gap, fixed); JUCE pin end-to-end in CI; CI log hygiene;
THIRD_PARTY_LICENSES symbol-level method; preset path construction fail-closed; fuzz harness
covers the real entry point.

### Already-known encountered (not re-filed)

KI-001, KI-012/ADR-0015, RISK-002, the A7-9 platform terminal states, the H4 Class-B
level-match trade, KI-016/023/026, KI-003/004/007 host-coverage gaps, RISK-004 (Linux),
RISK-005, KI-015/RISK-006 licensing, pluginval unpinned (RH-F6), the v0.2 abSlot staleness
note (worklog §11), metadata-only AnamorphRoot adoption (deliberate, worklog §13), KI-009,
KI-010 (typed+wheel halves), KI-013, KI-017–KI-022, macos-intel thin-build scope, Windows
staging self-check scope, `gcc:16` floating major, Renovate rejection.

### Version and validation at end of round

Version bumped **0.9.5 → 0.9.6** (CHANGELOG `[0.9.6] — 2026-08-31`, six Fixed entries; the
repo's established pattern — every change set with user-visible fixes gets a version, tags
have never been cut, RISK-003 open). End-state validation: DSP suite **45 tests + guard,
241 checks, 0 failures** (new Tests 43–46; engaged matrices); state suite **15 tests,
924 checks, 0 failures** (2 new regressions); `check-realtime` 93 self-test cases + clean
scan with the widened seed set; preflight + all checker self-tests green; citation gate
green after re-anchoring (see the round's commits).

### Maintainer decisions needed (full evidence in the dashboard §Decisions)

- **D-1 (KI-027 / ER-RT-01):** approve moving latency-notification delivery off the audio
  thread (candidate: atomic flag consumed by the editor's 24 Hz poll, keeping the synchronous
  path for message-thread calls; alternatives: AsyncUpdater, timer). Threading-model gate item.
  Until decided, the defect stands recorded; the latency VALUE is unaffected.
- **D-2 (RISK-007):** whether to add a narrow guard (mutex over the state-set members, or
  callAsync marshalling of the metadata tail) for off-main-thread state calls, or accept the
  AU exposure as documented. Also gated. Round 2 can run the TSan harness first (recommended).
- **D-3 (Level-5 audition):** precondition 7 is open for v0.9.6 — the 2026-08-15 audition
  covered v0.9.4; since then every x86-64 binary's machine code changed (ADR-0031/0032) and
  0.9.6 changed engine behaviour in the defective windows (prepare-settle, oversized blocks).
  Needs a human DAW session; cannot be automated (TESTING_POLICY Level 5).
- **D-4 (version):** 0.9.6 bump + renumbering executed per the repo's established pattern —
  override if you want the round folded differently.

### Roadmap after round 1 (evidence-ranked; the dashboard tracks live state)

1. **R2-1** ER-STATE-03/ER-DEP-03 — NaN parameter injection via session/preset (`raw="nan"`
   passes every clamp; JUCE parses `nan`). Guard (`std::isfinite` fallback to default) +
   state test + a fuzz seed. Small, closes the last non-finite ingress.
2. **R2-2** TSan two-thread state-call harness (feeds D-2 with measurements).
3. **R2-3** CI-file comment drift batch: build.yml header "ONLY LINUX validates the shipped
   bytes" (ER-CI-02), codeql.yml unpinned-compiler comment (ER-CI-03), check-gcc-warnings
   header pair (ER-CI-04) — batched deliberately: build.yml line shifts re-anchor many
   citations, so they land together, once.
3b. **R2-3b** release.yml annotated-tag check misreports a transient fetch failure (ER-CI-05).
4. **R2-4** `-DANAMORPH_JUCE_PATH` revision check (ER-DEP-04): configure-time rev-parse
   WARNING on mismatch (must stay a warning — the twin dump deliberately points at an old tree).
5. **R2-5** GUI low-priority pair: cancelInlineTextEdits scope (ER-GUI-02), KI-013 impact
   escalation note (ER-GUI-03); plus ER-STATE-04 (restore-path comment vs pinned JUCE) —
   verify then correct.
6. **R2-6** Twin-dump transition scenarios (extends the ER-TST-02 boundary) — only if the
   maintainer wants the gate's surface widened; costs hash-churn on every toolset move.
7. **Deferred, revisit-on-evidence:** ER-DSP-05 (chorus LFO float phase at 192 k×8 OS beyond
   the tested envelope), ER-DEP-06 (silent preset-load failure UX — UI copy is
   maintainer-owned, C8), Windows staging loadability probe, auval scope (RH-F3).

### Checklist (round 1)

- [x] Baseline validation recorded
- [x] 8-lens sweep + adversarial verification
- [x] 10 code/tooling fixes implemented with regression coverage
- [x] 2 registry filings (KI-027, RISK-007) for gate-blocked defects
- [x] Doc drift sweep (renumbering, NOTICE, CI inventory, counts, KI-010/026)
- [x] Version 0.9.6 + CHANGELOG
- [x] Worklog + live dashboard committed
- [x] Full validation green at end state
- [ ] D-1..D-4 decided (maintainer)
- [ ] Round 2 scheduled per roadmap
