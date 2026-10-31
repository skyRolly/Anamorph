# NR-006 — ScopeBuffer publication review packet

**Recorded:** 2026-10-31 (Asia/Shanghai). **Updated:** 2026-11-01.
**Current status:** ADR-0070 Accepted by explicit owner implementation approval
on 2026-11-01; atomic-window correction implemented on its separate development branch;
final-head native/performance/manual acceptance and integration remain pending. Sections 1–5 preserve the earlier packet
and conditional candidate; the substantive selection is in section 6 onward. Progress remains in
[NEXT_RELEASE_DEVELOPMENT_ROADMAP.md](NEXT_RELEASE_DEVELOPMENT_ROADMAP.md).

Execution parent: NR-022 review head `4a431cac2e32493b1d2290fb9fd8d6570b41393f`.
`src/dsp/ScopeBuffer.h` is unchanged from main/release source `4c13688`, blob
`ed24ceab5ced2784d9fe96effdc46f8aeb03f472`. The prior evidence is
[R7 §E](R7_PRODUCTION_PATH_COVERAGE.md#e-the-scopebuffers-cross-thread-half--a-finding-not-a-test).
The owner's existing ADR approval/real-machine attestation is preserved separately;
it is not attributed to a design or prototype that does not yet exist.

## 1. Confirmed mechanism and evidence limits

Evidence [Verified]: `ScopeBuffer::pushBlock/readLatest/writeCount`, current
`Vectorscope::tick` and `SpectrumImager::pushFFT`, plus the historical R7 analysis.
The single audio writer copies two plain float arrays and then release-stores the
new write index. The GUI acquires the index and copies frames below it. Ticks/FFT
run on the message thread; `Vectorscope::paint` also reads the ring. With OpenGL
attached on macOS/Windows, pinned JUCE 9.0.4 can call that paint on its rendering
thread while holding the message-manager lock. GUI read sites are therefore serialized,
but not all message-thread-affine. This corrects the packet's former claim that every
read occurs on the message thread. The policy's literal affinity rule remains an
unresolved NR-006 contract boundary, not a newly approved exception.

Evidence [Verified]: `Vectorscope::paint/readLatest`, the editor's existing OpenGL
attachment, and pinned JUCE's `juce_OpenGLContext.cpp::CachedImage::renderFrame`
(scoped message-manager lock), `paintComponent` (lock assertion), `paintOwner`
(`component.paintEntireComponent`). Linux/BSD do not attach that context. The GUI lock
never excludes `pushBlock`, so it cannot establish audio-to-GUI payload ownership.

The publication orders writes from the committed block before the reader that
acquires it. It does **not** order that reader against a subsequent producer lap:

1. Writer fills old frames and publishes index W0.
2. Reader acquires W0 and starts copying the corresponding newest window.
3. Writer advances far enough to overwrite a frame still being copied.
4. The next plain sample write and reader access have no reader-to-writer
   happens-before edge. The index's one-way release/acquire cannot provide one.

The inline source comment that partially committed frames “can never be observed”
also omits subsequent overwrites. Its correction belongs with the selected protocol;
this documentation-only packet leaves the source blob unchanged.

This is a source-level C++ data-race exposure on lapping, not a claimed fresh
TSan report, supported-host crash or audio corruption. The scope/FFT data are display
inputs; the engine does not read them back into its output. Aligned float access
does not establish language-level race freedom. A window can also contain values
from different producer epochs, and left/right copies are separate in the current
representation. Their field incidence is unmeasured.

R7's TSan/memcpy observation is historical; no new native/TSan experiment is claimed.
Silence from a sanitizer is not a proof of safety where the recorded detector did
not observe equivalent memcpy accesses. The single-threaded Rosetta Test 61 failure,
Test 69 equality anomaly and native Intel AU exception remain separate causal items.

**Documentation drift:** THREAD_MODEL's “whole blocks atomically” described initial
publication too broadly, and its blanket non-atomic-access sentence omitted this
known gap. PR #197's follow-up review also confirmed the same unconditional claim
remained in THREADING_POLICY's Atomic usage rules. Both the architecture table and
binding policy now distinguish prior-write visibility from ownership, subsequent
reuse and stable stereo/block/window snapshots. The policy's non-atomic shared-state
prohibition remains binding; NR-006 is a known implementation gap, not an exception
or an approved best-effort race. This factual correction enacts no new protocol,
memory ordering or policy relaxation and does not mark the current representation
race-free. The source comment remains explicitly identified as overstated above.

## 2. Required invariants and decision

Any selected correction must keep exactly one audio producer, serialized GUI
consumers, fixed prepared storage and bounded work. The decision must explicitly
resolve the current OpenGL-paint/message-thread policy-affinity drift: either approve
a precise serialized GUI-reader contract through the ADR or choose an implementation
that satisfies the existing affinity rule. Neither is silently selected here.
No audio allocation, lock,
wait, message posting, file IO or unbounded retry; no GUI-written audio payload.
Preserve single-threaded count/clamp/wrap/oversized-block behavior, staleness probes,
normal sample bit patterns and the engine's output. No DSP order, parameter,
serialization, latency, supported platform or compiler pin change is proposed.

The unresolved product contract is **whole-window coherence versus display-only
best-effort snapshots when lapped**. Initial publication, indivisible stereo pairs,
and a coherent whole window are different promises. Select the promise explicitly
before choosing an implementation; a post-copy index check cannot undo a C++ race
that already happened on non-atomic payload.

| Option | Properties / cost and risk | Recommended disposition |
|---|---|---|
| Retain plain arrays and document the risk | No code cost, but the formal race remains; documentation cannot establish compliance with the non-atomic shared-state rule | Temporary status only, not a race-free completion claim |
| Atomic float samples, relaxed payload; existing release/acquire index | Fixed bounded stores/loads, removes conflicting plain accesses if atomics are always lock-free; L/R and window can still mix epochs | Feasible minimal alternative, but stereo-pair semantics are weaker |
| Packed stereo frame in an always-lock-free atomic 64-bit word | One indivisible L/R payload per frame; fixed storage; relaxed payload operations plus existing index publication. Whole-window epochs may still differ. Replaces vectorizable memcpy with atomic loops; cost must be measured | **Preferred candidate if best-effort windows are acceptable**, subject to review, compile guards and measured cost |
| Reader-safe fixed buffers / explicit ownership handoff | Can provide coherent immutable windows; adds buffer ownership states, memory/copy cost and more complex lifecycle. Audio must never wait for a slow/closed editor | Prefer only if whole-window coherence is required; needs a complete bounded protocol |
| Plain-payload seqlock with retry, or a mutex | Retrying/discarding does not legalize concurrent non-atomic payload access; an audio mutex or unbounded spin violates RT requirements | Reject these forms |

The preferred candidate is an implementation direction for review, not a selected
or pre-approved contract. Atomic samples cannot silently inherit the old blanket
whole-window claim. If the owner selects stricter coherence, design the fixed-buffer
protocol instead of disguising mixed epochs as a coherent snapshot.

## 3. Proposed implementation boundary after review

One focused PR after an evidence-backed ADR decision; do not combine restore locking,
Rosetta debugging, new DSP or dependency upgrades.

For the packed-frame candidate:

- Use explicit bit-preserving packing/unpacking of two 32-bit floats. Guard the
  representation and `std::atomic<uint64_t>::is_always_lock_free` at compile time on
  every supported toolchain. No runtime fallback to a hidden lock.
- Keep preallocated fixed-capacity storage, monotonic write index and existing
  publication/staleness API. Bound any attempt to reject lapped windows; the GUI may
  retry on its next ordinary frame tick, not spin inside an audio callback.
- State the admitted concurrent-read results precisely: whether mixed frame epochs
  are allowed, whether a lapped read returns no data, and how the consumers handle it.
  A pure packed payload alone guarantees stereo-pair indivisibility, not full-window
  consistency. Do not infer a generation check's sufficiency without a proof.
- Reconcile THREAD_MODEL/THREADING_POLICY/REALTIME_SAFETY_AUDIT and relevant algorithm/
  test references according to the chosen protocol. Do not add a policy exception
  that merely renames the present race. Register an ADR only when the decision is made,
  using the next available index number rather than reserved commercial ADR slots.

No source implementation, changed ABI/layout claim, performance result or new
architecture approval is represented by this packet.

## 4. Observable acceptance and validation gates

1. **Representation proof:** every conflicting payload access uses a defined atomic
   or proven ownership handoff. Review the reader-to-next-overwrite relation, not only
   the initial writer-to-reader publication. GUI read sites remain stateless and
   their thread affinity/serialization must match the explicitly selected contract.
2. **Behavioral regression:** retain DSP Test 61 and its wrap/oversized/newest-frame
   controls. Add a deterministic scheduled lapping/stereo-consistency surface that
   distinguishes the old representation and the selected new guarantee. Use explicit
   test scheduling/hooks rather than timing sleeps; document any instrumentation's
   difference from the shipping path. Whole-window tests must reflect the actual
   chosen contract and must not silently weaken existing single-thread assertions.
3. **Concurrency evidence:** run a genuinely concurrent producer/reader harness with
   suitable race instrumentation and a seeded liveness control. Given the historical
   memcpy blind spot, require the representation proof as well as dynamic evidence;
   no zero-report result by itself closes the item. Verify slow/hidden/closed editors,
   independent GUI consumers and re-prepare/reset lifecycle under the host contract.
4. **Real-time evidence:** existing allocation guard, realtime lint/effect canary,
   RTSan and native CI pass; no audio locks or retries. Atomic lock-freedom guards must
   compile on Linux/MSVC/macOS ARM and Intel. Preserve all mandatory suite gates.
5. **Cost/equivalence:** compare push/read cost and footprint at real reader sizes,
   normal/large blocks and representative rates. Use repeatable relative measurements
   with machine/compiler/flags; do not invent a quiet-machine budget from this sandbox.
   Engine output/latency must be unchanged. Escalate any meaningful CPU regression.
6. **Human acceptance:** architecture review chooses the window/pair contract and
   approves the corresponding ADR before implementation; later visual scope/FFT
   audition validates the changed display behavior. NR-002's earlier attestation is
   not a substitute for observations of a future implementation.

## 5. Review outcome and progress

**NR-006: Ready for Review (packet only).** The formal mechanism is characterized and
descriptive drift is corrected. Runtime correction remains pending selection of the
contract/protocol and its validation surface. No defect is declared fixed.

**Review disposition — 2026-11-01 (Asia/Shanghai): accepted and corrected.** Re-read
the exact ScopeBuffer blob and both GUI consumers. The missing reverse
ordering/storage protection confirms the finding without relying on a new sanitizer
result. Whole-block coherence is not established. THREAD_MODEL and THREADING_POLICY
now say so consistently; no source/test/build/ADR decision was changed. Existing
DSP Test 61 is single-threaded evidence, not concurrent snapshot validation. The
existing packet's packed-frame recommendation remains conditional on explicit owner
selection of the contract and subsequent human Architecture Review.

**Additional review finding: accept/document existing reader-affinity drift.** A
final call-site trace found Vectorscope's paint read and followed it through the
pinned JUCE rendering thread. THREAD_MODEL now describes that existing execution;
THREADING_POLICY retains its prohibition and explicitly records the mismatch rather
than pretending the rule is satisfied or granting a new exception. NR-006 must resolve
both this contract boundary and the plain-payload overwrite gap. No thread, lock,
reader path, payload access or source comment was changed by this factual correction.

**Performance-contract follow-up: accepted and corrected.** The owner's next review
flag identified PERFORMANCE_BUDGET's remaining whole-committed-block promise. A
separate drift check also found its Wave 4 explanation repeating the same index-only
argument. Corrected both, retaining the segmented-copy implementation and historical
measurement while distinguishing serially observed final ring bytes from concurrent
snapshot safety. The performance row now states fixed storage and bounded copy work,
constant per copied frame rather than an unqualified O(1) transfer. This is a factual
documentation correction; no performance experiment, runtime repair, policy exception
or approved architecture decision is claimed.

The binding gate is
[ARCHITECTURE_REVIEW_GATE.md](../docs/policies/ARCHITECTURE_REVIEW_GATE.md):
“Thread Model change — new thread, new cross-thread path, new atomic ordering”.
It requires human Architecture Review, even with green tests. This is a new decision
about the payload handoff; it does not reopen the existing ADR-0069 approval. Independent
work such as NR-017's read-only diagnostic inventory can proceed while it is reviewed.

## 6. Post-integration investigation

**Baseline:** `00de8c7796f37d08a338256e01830bb312ba105c`, after owner-authorized
integration of #195 → #196 → #197 → #198 on 2026-11-01 (Asia/Shanghai).
All four original heads are ancestors; the final tree exactly equals #198 `42ebbbc`.
Fresh integrated local suites: **945 DSP / 10,953 state**, zero failures, under
`MALLOC_PERTURB_=1`. Existing checker warnings remain unchanged.

**Selected decision:** [Accepted ADR-0070](../docs/architecture/design-decisions/ADR-0070-validated-atomic-scope-windows.md).
Choose a generation-validated ring of atomic stereo words, accepting only exact
committed-position windows and rejecting a reused copy once, without retry.
Keep current OpenGL component painting, with an explicit proposed serialized-GUI
reader contract. This replaces the earlier request for the owner to make the
technical selection: the agent has selected and specified D below; the owner must
review/approve that recommendation. The owner has now explicitly approved that design for implementation, subject to
its native portability/performance/validation gates. Production implementation
is still pending; no later PR merge is authorized. No ScopeBuffer or GUI production code changed.

### Source census and lifecycle

| Surface | Verified current execution / ownership |
|---|---|
| Writer | `AnamorphEngine::process` final `scope.pushBlock(L,R,n)`; one audio producer, post-output processing. No other production push site. |
| Storage | Two initialized `array<float,16384>` members; uint64 atomic total. Two segments per channel on wrap; oversized input skips its prefix and publishes the original total increment. |
| `writeCount` | Acquire-loads only the total; does not touch payload or consume frames. Its separate observation need not equal a subsequent read's index. |
| Vectorscope | Constructor reads the hint. Message-thread FrameClock tick scans newest fresh data; paint separately reads the current 1200–8000-frame persistence window, decimating to about 3000 points. Both use its private scratch vectors. |
| SpectrumImager | Message-thread tick invokes `pushFFT`; active window copies 8192 frames, idle window scans fresh tail, wakeup may perform a second full read. `lastSeenCount` currently advances before these copies. Each copy acquires its own index. |
| GUI serialization | FrameClock uses VBlankAttachment/message-thread callbacks. JUCE pin `3fb5cbc` OpenGL `CachedImage::renderFrame` acquires its message-manager lock before `paintComponent` → `paintOwner` → `paintEntireComponent`. Mac/Windows paint can run on that GL thread; lock serializes it against message-thread consumers, never against audio. |
| Editor open/hidden/close | Private GUI buffers allocate during construction; hidden vectorscope tick skips work. Editor first stops meterVBlank/timer and detaches OpenGL; child destructors stop FrameClock. The ring lives in the processor engine and reserves nothing for an editor. |
| Prepare/reset | Host excludes concurrent audio processing. Neither prepare nor reset clears scope arrays/count; history survives re-prepare/reset. GUI can remain open. Preserve this, rather than adding a live plain reset or another writer. |
| Destruction | Existing processor lifetime requires stopped audio and destroyed editors. No proposed payload reclamation or deferred free. |

Evidence [Verified]: named source functions, `ScopeBuffer.h`, engine header's owned
`ScopeBuffer`, GUI headers' capacities; pinned JUCE `juce_OpenGLContext.cpp` and
FrameClock. Static source census was repeated after integration. Test 61 covers
serial empty/newest/wrap/clamp/oversized behavior only. Historical Rosetta equality
and AU teardown findings remain independent and retain their recurrence triggers.

### Memory-model and impact finding

Example at capacity 16384: publish W=16384; the reader acquires W and copies
positions 8192–16383. The producer can next overwrite physical slot 8192 while
the reader loads it. There is writer-to-reader ordering for the first publication,
but no reader-to-next-writer ordering or storage ownership. Plain conflicting
float accesses are a C++ data race regardless of hardware float atomicity.
Independent L/R copies can mix pairs; a ring copy can mix windows. Atomic index
validation afterward cannot undo undefined behavior already incurred.

The historical ~170 ms headroom estimate is conditional on an 8192-frame window,
48 kHz and regular short pushes. It is not a safety proof or universal required
stall: larger blocks can overwrite a selected window during a single push, and
higher sample rates shorten the interval. `n>capacity` writes every physical slot.
Object storage lives long enough; lifetime alone supplies no exclusive access.

The language violation is confirmed. Field incidence, supported-host crash and
audible output corruption remain **unverified**. Display data are not fed back
into the DSP output, but C++ undefined behavior is not formally contained to a
display artifact. No sanitizer or headless test retroactively approves this risk.

### Alternatives evaluated independently

The earlier preferred packed-word candidate was not presumed selected. The new
comparison includes the following complete alternatives and bounded variants:

- **A, atomic individual samples:** payload race freedom with compile-time
  lock-freedom guards; ordinary release/acquire index. Split stereo pairs and
  window epochs remain possible. More payload operations than B and weaker pair
  semantics; not preferred. No claim that atomics alone make a snapshot coherent.
- **B, packed atomic stereo:** bit-preserving 32+32-bit representation, one
  relaxed atomic word per frame and existing release/acquire total. Good pair
  coherence, smallest atomic footprint, no window guarantee. A deterministic
  lapping control produces old first frame plus new following frames while every
  L/R pair is intact. Feasible if that lower contract were explicitly selected;
  it is not the recommendation after considering the FFT/fresh-tail consumers.
- **C, owned snapshots:** producer-private ordinary ring plus three full snapshot
  buffers. Producer fills its writable snapshot, publishes its index/dirty bit
  through one acq_rel middle exchange, then owns the returned middle buffer.
  Consumer exchanges its previous private buffer only after finishing that read.
  The three indices stay distinct; publication transfers visibility, return
  transfers reuse permission. A stalled reader holds one buffer; producer keeps
  cycling the other two. No waiting or unsafe reclamation, and successful whole
  windows are immutable. Full-history copies every push have high small-block
  cost; dropping a publication with two buffers is safe but changes freshness.
- **C variants:** request-only snapshots reduce average copies but can miss the
  final stopped-audio window; maintaining full rolling snapshots in multiple
  buffers copies more, while page ownership needs additional metadata and safe
  exhaustion handling. A slow GUI cannot be allowed to pin the producer's only
  reusable storage. None has a demonstrated benefit sufficient to select it here.
- **D, generation-validated atomic stereo:** two 64-bit atomic words/slot;
  invalidation → release payload → valid absolute tag; reader acquire tag →
  acquire payload → acquire tag. Unique expected tags and atomic coherence
  establish exact positions on success. Failed partial scratch is discarded,
  caches/freshness retained, one attempt only. Protocol/proof/lifetime and every
  memory order are fully specified in ADR-0070. No reader-driven producer state.
- **Other D protocols:** a bounded SPSC queue is safe if full drops are explicit,
  but FIFO retention during a stall conflicts with newest-history recovery unless
  another handoff is added. Hazard/RCU requires bounded reuse permission; unbounded
  retired buffers or waiting for GUI is inadmissible. A correctly ordered
  all-atomic block seqlock can be race-free, but rejects on any push, even without
  physical reuse; a retry bound and immutable scratch rules would still be needed.
- **E, retain/narrow current behavior:** temporary known-gap disposition only.
  Documentation cannot satisfy binding non-atomic access policy. A plain-payload
  seqlock, reader index recheck, mutex, allocating queue or unbounded spin is rejected.

**Ranking:** D, then B if a future review deliberately accepts weaker windows,
then C if native cost or a stricter immutable-storage requirement changes the
tradeoff. A is dominated for this stereo use; E is not a compliant fix.
The agent recommends D conditionally on native lock-free/code-generation and
performance evidence. No native performance target is invented from sandbox time.

### Comparative criteria and GUI recommendation

| Criterion | Selected D versus viable B/C |
|---|---|
| Memory/lifetime | All ring data/tags atomic; fixed lifetime. C additionally proves exclusive ownership. B is race-free but admits mixed epochs. |
| Audio/RT | D has three bounded ordinary atomic stores per retained frame, no RMW/retry/wait; B one. C does an entire available-history copy and one exchange per push. No design may depend on GUI responsiveness. |
| DSP/latency | All are display-side proposals. Engine samples/order/latency must stay unchanged and be verified; no current result is attributed to an implementation. |
| Display | D succeeds with exact committed positions or rejects; retain last accepted display and counters on failure. C always reads an immutable published snapshot; B can transform mixed windows. |
| GUI/threading | Recommend retaining OpenGL and its existing message-manager serialization. Explicit acceptance would amend only the scope affinity rule for this safe payload path. Moving reads into ticks adds cache/refresh behavior; removing GL is wider than this fix. |
| Footprint/cache | D's shared 256 KiB versus B 128 KiB / C approximately 512 KiB. Preallocated acceptance/scratch buffering can add 192 KiB per open GUI. Native cache and contention cost is still unmeasured. |
| Portability | C++ does not mandate all 64-bit atomics lock-free. Compile-time guards plus native library/code-generation evidence required on MSVC x64 and AppleClang Intel/ARM, not inferred from x86 Linux. |
| Complexity/testing | D avoids ownership reclamation and generation retries. Its acquire/release proof is essential; both invalidation and payload acquire are discriminating model controls. C has a three-index ownership invariant and one logical consumer. |
| Maintenance | A selected result must carry its own end/count/status. Independent earlier `writeCount` hints cannot commit freshness for another snapshot. Rejected scratch must never reach FFT/silence/paint consumers. |

## 7. Nonshipping experiments and reproducibility

`tests/experiments/nr006_scope_protocol.cpp` is a standalone research executable,
not linked into any product or CMake target. At the research checkpoint, original
ScopeBuffer was used only as a **serial** reference / producer-private history in C.
The harness now pins that immutable original fixture (see §11); the unsafe plain ring
is never subjected to concurrent runtime access in this experiment.

```bash
# From repository root, with an installed C++23 compiler/library:
c++ -std=c++23 -O3 -march=haswell -ffp-contract=off -pthread -Wall -Wextra -Werror \
  tests/experiments/nr006_scope_protocol.cpp -o /tmp/nr006-protocol
/tmp/nr006-protocol
/tmp/nr006-protocol --bench
python3 tests/experiments/nr006_memory_model.py
```

Use `-march=haswell` only on an x86-64 machine supporting it; native ARM experiments
must record their own supported shipping flags. The local managed environment
activates its retained tools with `source /workspace/.cloud-setup/env.sh`.
For TSan, build the prototype and `tests/tsan_canary.cpp` with
`-std=c++23 -O1 -g -fsanitize=thread -pthread`. Run the canary first and require
nonzero exit **and** `ThreadSanitizer: data race`; only then trust a clean prototype
run. A failed instrumentation/environment precondition is not a clean sanitizer run.

The prototype includes serial wrap/clamp/oversized controls, exact pack/unpack
signed-zero/infinity/NaN-bit controls, scheduled A split-pair and B mixed-window
controls, and a D reuse-between-tag-and-payload rejection/recovery control. Its
real concurrent stress pushes 1,048,576 frames in 64-frame blocks. D has two
independent readers with private 8192-frame destinations; C one logical reader.
Accepted windows are checked against absolute-position samples, with observations
counted while the writer is active. This does not authorize concurrent production
GUI reads or replace production-path regression coverage after approval.

The Python bounded read-from model enumerates one slot's next reuse, enforcing
sequenced-before, release/acquire synchronization and per-atomic coherence.
Proposed ordering: 10 legal observations, 1 accepted, **0 wrong accepted**.
Relaxing the payload load admits 1 wrong accepted observation; removing
invalidation also admits 1. These non-vacuity controls explain why D's proof
requires more than a relaxed packed word plus index. This is a small sanity
model, not exhaustive verification of arbitrary C++ implementations.

Local compiler/native stress, active-canary TSan, raw seven-repeat timing and
machine metadata are retained under [nr006-research/](nr006-research/).
The benchmark prints min/median/max **nanoseconds per call**, measures serial
pushes separately from fixed 8192-frame reads, and keeps outputs observable.
It is not a contended engine benchmark or a user-instance/dropout budget.
Machine: shared managed x86-64 Linux, Xeon Platinum 8573C; Clang 22.1.8 and
GCC 14.2.0 / libstdc++14; `-O3 -march=haswell -ffp-contract=off`, one CPU affinity
for timed runs, 7×2000 calls per cell. Shared frequency/cache/scheduler effects
remain uncontrolled. Native Mac/Windows/ARM and representative modest-machine
measurements are **pending**. Historical Wave 4 −4.9% is not a new-design result.

Representative local **64-frame** median cell, retained with all min/max values
in the raw files (nanoseconds/call; 8192-frame read):

| Candidate | Local bytes | Clang push / read | GCC push / read |
|---|---|---|---|
| Plain serial reference | 131080 | 19.2 / 8002.1 | 17.7 / 8147.6 |
| A split atomics | 131080 | 60.2 / 8385.9 | 60.5 / 8511.2 |
| B packed pairs | 131080 | 30.2 / 7895.3 | 43.5 / 8176.5 |
| C owned full snapshots | 524336 | 16899.6 / 1801.7 | 16642.7 / 1790.2 |
| D tagged pairs | 262152 | 46.9 / 9991.5 | 48.9 / 9383.6 |

These results support investigating D's modest extra fixed per-frame work before
accepting B's mixed-window semantics, and rejecting per-push full-copy C for this
small-block use. They do not establish a percentage of engine cost or acceptance
on other CPUs. Generated atomics must still be checked natively on each platform.

## 8. Implementation and validation plan after human acceptance

1. Implement ADR-0070's fixed atomic representation and explicit read result in
   one production PR. Correct source overclaims, preserve serial Test 61, add no
   other DSP/state/build changes. Never treat post-copy plain-memory validation
   as the fix.
2. Update each read site to commit end/count/caches only for a valid complete
   attempt. Preallocate scratch away from audio; retain last accepted waveform/
   magnitudes and silence/freshness state on rejection. Cover the imager's
   second full read after a silent-tail wakeup. Preserve editor detach/stop order.
3. Add deterministic first/second-tag overwrite, later-slot reuse, wrap, boundary,
   stereo/window, count, staleness, slow/stalled/independent consumer, oversized
   push, re-prepare/reset and invalid-scratch tests. Meaningful old/weak-order/
   removed-validation controls must fail. No timing sleeps as sole oracle.
4. Run actual concurrent production-path stress and TSan with the seeded canary,
   RTSan/effect canary, allocation guards, source scans, full suites/native
   pluginval and unchanged shipping ABI gates. Native compile guards and
   generated instructions must prove no hidden lock/RMW retry. Lock-free alone
   does not establish wait-free WCET.
5. Measure native push/read, full-engine cost and memory on controlled identified
   hardware (at least five repeats, rates/normal/large blocks, active/hidden/
   stalled GUI, median/spread/worst observed block). Obtain owner review of cost;
   do not create a numeric budget from synthetic results. Confirm unchanged
   same-configuration engine output and latency; preserve Class-B/cross-platform rules.
6. Reconcile THREAD_MODEL, THREADING_POLICY, PERFORMANCE_BUDGET, real-time audit,
   tests and source comments with the **Accepted** decision in that implementation
   PR. Native visual/editor lifecycle audition is required and remains unperformed.

## 9. Current-candidate release disposition

**NR-021 recommendation: requires mitigation or additional validation before
publication**, with the source-level violation requiring a reviewed mitigation or
an explicit human architecture/risk disposition; more clean runs alone do not
legalize the plain accesses. The selected 0.10.0 source `4c13688` contains the same
ScopeBuffer blob, so the new finding applies to that draft's source. No new
supported-host crash or automatic severity escalation is established.

Keep the existing 0.10.0 tag, draft, 11 asset identities/digests and version
unchanged. Neither the post-tag NR-022 integration nor this proposal changes
already-built assets. Earlier manual acceptance and ADR-0069 approval do not approve
NR-006's new architecture or waive its binding policy gap. Any replacement,
publication or temporary residual-risk acceptance needs a separate owner decision.

## 10. Current milestone

**Investigated:** yes. **Selected design:** D / Accepted ADR-0070.
**Human architecture approval:** explicit owner approval received 2026-11-01.
**Production implementation/validation:** not started at this decision-record checkpoint.
**Release operation:** none. Continue independent NR-011 transition differential
coverage while this architecture is reviewed; NR-003 stays Deferred and other
recurrence/legal/hardware/host prerequisites remain separate.

## 11. Approved production implementation — 2026-11-01 (Asia/Shanghai)

**Architecture approval:** owner reply “Approve ADR-0070 for implementation”,
recorded in [#199](https://github.com/skyRolly/Anamorph/pull/199#issuecomment-6548395725)
and commit `89694f2`. **Implemented in [#202](https://github.com/skyRolly/Anamorph/pull/202), on branch:**
`fix/nr006-validated-scope-windows`, based on that accepted decision branch.
Source correction commit `7f44bdc699d1509b33f5bdd33d1f50fb017a7a56`; final review head and subsequent
validation attempts are maintained in the [#202 handoff ledger](https://github.com/skyRolly/Anamorph/pull/202#issuecomment-6549093755).
Main remains `00de8c7`; implementation/research/NR-011/NR-012 PRs are not merged.
Sections 1–10 are dated original/proposal evidence; statements there about plain
storage, pending architecture approval or no production edits describe those
checkpoints, not the implementation described here.

### Protocol and consumers

`ScopeBuffer` implements the selected two-atomic64 slot representation and exact
release invalidation → release stereo → release tag → block-index publication.
The reader acquires one end and validates expected tag → acquired stereo → tag,
with one rejection and no retry. The typed result separates valid empty from
failure and returns the actual acquired end. Fresh-tail length uses that same
end. Legacy serial `readLatest` remains for Test 61/diagnostics. Native float32/
IEC559/always-lock-free atomic64 guards have no fallback. Epoch rollover is
excluded, ring wrap supported; prepare/reset do not add a writer or clear history.

`ScopeSnapshotCache` preallocates scratch and accepted buffers during GUI
construction. Vectorscope paint retains the accepted window when copying fails;
its activity poll retains counters on failure. SpectrumImager retains FFT,
silence and freshness state on failure, including a valid wakeup tail followed
by a failed full FFT window. Its next tick can retry that same publication.
The source preserves JUCE component painting under its existing message-manager
serialization; consumer entries assert that contract. No GL attachment change,
new thread, audio lock/allocation, reader ownership or RMW is introduced.

Binding THREADING_POLICY, THREAD_MODEL, PERFORMANCE_BUDGET, real-time audit/policy,
TESTING and navigation/risk references now describe the implemented contract.
The Wave 4 memcpy/−4.9 % result is explicitly historical and not reused as new
performance evidence. Original scope races remain true of main/draft until an
authorized integration/replacement changes those actual source/artifact identities.

### Regression strength and test-fixture correction

State **150–151** add **27 checks**: deterministic slot reuse between tag/payload,
a copied old prefix followed by later-slot reuse, an in-flight overwrite before
new tag/index publication, bit-exact float patterns, empty/clamp/wrap/oversize,
prepare/reset history and actual offscreen Vectorscope/FFT failure-retention/retry.
A producer advances 1,048,576 frames beyond the initial ring while independent
full-window/tail readers verify each accepted sample and stereo position.
Accepted/rejected/overlap counters are printed; assertions run after joining.

`check-scope-snapshots.py --self-test` verifies the immutable original blob
`ed24ceab5ced2784d9fe96effdc46f8aeb03f472`/SHA256 fixture, injects scheduling
hooks into temporary storage and runs all schedules on **one thread**. Original
**3/3 failures**; current **3/3 pass**. Removed-validation mutant **3/3 failures**;
missing-invalidation mutant **1/3 failure**, specifically the in-flight overwrite.
Both Clang22 and GCC14 run these controls. The earlier bounded memory-order
model retains the relaxed-payload/invalidation negative controls. The old
prototype now includes the frozen original fixture, preserving its “Plain”
reference after production changes; metadata records this reproduction amendment
and retains original source hashes/results without relabeling them as current.

**Introduced failure and repair:** doubling inline history first overflowed the
unchanged **1 MB** state stack gate. GNU disassembly showed optimized main reserving
823352 bytes plus the registry test's 273832-byte frame. The factory already
heap-allocates product processors. Test-owned scoped processor locals (268) now
use scoped heap fixtures with the same references, callbacks and lexical destruction.
All **3,064 existing `check(...)` expressions remain byte-identical**. Full state
checks pass at the same 1 MB limit; no assertions, negative controls, detector
suppressions or stack thresholds were weakened. This allocation is test setup,
not an audio callback or a change to product ownership.

**Stress-harness failure and repair:** a full preflight caught a reader that had
only one rejected attempt before the free-running writer completed. ADR-0070
permits reader starvation under lapping, so requiring successful coverage without
controlling scheduling was a harness defect, not a wrong accepted window. A
bounded test-only checkpoint after four writes now lets both readers acknowledge
one post-advance window before the remaining free-running phase. No handshake
exists in `pushBlock`/read APIs; all accepted-data assertions and the 10-second
completion bound remain. Five final-source probe runs and full suites recheck it.

**Local TSan tooling failure:** the initial full run lacked llvm-symbolizer, so
the unchanged named test-75 re-entrancy suppression could not match. GNU addr2line
mapped both stacks to `WriteFromInsideAGestureOpen` / `WriteFromInsideAStore`,
the pair already documented in `tsan-suppressions.txt`. Installed the matching
Clang22 symbolizer into the workspace toolchain and reran with the same four-entry
suppression file. No race/deadlock detector or suppression was weakened. The
initial stopped run is not reported as a full-suite pass.

### Local production evidence and acceptance limits

- Clang22 Release DSP **945** / state **10,980**, zero failures; `MALLOC_PERTURB_=1`.
  Full state **10,980** and DSP **945** also pass the 1 MB stack constraint.
- The original 32-row same-configuration engine dump is byte-identical to its
  saved pre-implementation output, including latency fields; DSP math/order (the metering comment is clarified),
  parameter/serialization registry, dependencies and version metadata are unchanged.
  A temporary TU from NR-011 #200 also produces byte-identical five transition
  audio hashes, final latencies and per-block latency traces against its saved
  original-scope output. It reuses current target flags/engine objects without
  changing or importing #200's scenario source into this PR; raw output/provenance
  are in `nr006-production/transition-twin*`.
- Existing TSan seeded race canary remains live; five genuine concurrent
  production scope probes pass without a race report. After the scheduling
  correction, the final-source full TSan suite passes **10,980 / 0**, with all four
  unchanged suppression entries matched exactly once. Matching LLVM22 symbolization
  repairs the initial local tooling gap. Raw canary/probes and full-suite summary/
  digest are committed in `nr006-production/`; final-head native CI is tracked in
  the #202 ledger. The initial unsymbolized stopped run is a tooling failure, not a pass.
- Realtime source lint **97** liveness cases / zero violations; leaf function-effects
  compile clean and its allocating canary fails with `-Wfunction-effects`.
  Portability/state-coverage controls and full native mandatory CI remain in force.
- Rebuilt VST3/Standalone pass pinned-Clang Linux shipping ABI floors; strictness
  **10** pluginval deterministic **3/3**, randomise **3/3** (no relaxation).
  The earlier GCC14 **failed** shipping-ABI preflight (CXXABI_1.3.15 > 1.3.14)
  remains failed historical evidence; it was not reclassified or repaired.

Raw local cost/code-generation artifacts and commands/input digests are in
[`nr006-production/metadata.json`](nr006-production/metadata.json). The actual
production API (not the earlier approximation) was built beside the original
fixture with identical Clang C++23/O3/Haswell/no-contraction flags, affinity CPU1,
**7 × 2000** calls per cell on shared Xeon 8573C Linux x64. Push medians for
32/64/512/16401 frames: **26.1/45.6/379.8/11855.4 ns**, versus original
**12.0/19.1/92.9/3481.9 ns**. Read8192 medians **6.71–6.92 µs**, versus original
**1.11–2.52 µs** (repeat spread retained). These are repeat-mean serial synthetic
costs, not individual worst-block time, representative acceptance or a new budget.
The current scope is **262152 bytes**, previously **131080**; editor buffers add
**192 KiB** over old scratch. Local x64 disassembly has no helper calls, lock
prefix or memory RMW; register-only alignment `xchg %ax,%ax` is a NOP.

**Still required:** native shipping guards/generated-code evidence (MSVC x64,
AppleClang x64/ARM and Linux), final-head mandatory CI, representative ≥5-repeat
push/read/full-engine active/idle/stalled-GUI and large-block measurements with
observed worst block/footprint, owner cost acceptance, and targeted native visual/
editor-lifecycle audition. Shared-runner synthetic measurements cannot discharge
those conditions. Architecture approval is cleared; implementation, merge,
performance/manual acceptance and release authorization remain separate milestones.
RISK-018 stays open for main and unchanged 0.10.0, with mitigation/additional
validation required before publication. No release operation is performed.
