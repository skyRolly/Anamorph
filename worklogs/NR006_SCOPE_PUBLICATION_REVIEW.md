# NR-006 — ScopeBuffer publication review packet

**Recorded:** 2026-10-31 (Asia/Shanghai). **Status:** Ready for Review — decision packet,
not an implemented fix or an Accepted ADR. Progress remains in
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
new write index. The GUI acquires the index and copies frames below it. All readers
are on the message thread; two GUI consumers are not two concurrent reader threads.

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

Any selected correction must keep exactly one audio producer, message-thread
consumers, fixed prepared storage and bounded work. No audio allocation, lock,
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
   the initial writer-to-reader publication. Same-thread read sites remain stateless.
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
the exact ScopeBuffer blob and both message-thread consumers. The missing reverse
ordering/storage protection confirms the finding without relying on a new sanitizer
result. Whole-block coherence is not established. THREAD_MODEL and THREADING_POLICY
now say so consistently; no source/test/build/ADR decision was changed. Existing
DSP Test 61 is single-threaded evidence, not concurrent snapshot validation. The
existing packet's packed-frame recommendation remains conditional on explicit owner
selection of the contract and subsequent human Architecture Review.

The binding gate is
[ARCHITECTURE_REVIEW_GATE.md](../docs/policies/ARCHITECTURE_REVIEW_GATE.md):
“Thread Model change — new thread, new cross-thread path, new atomic ordering”.
It requires human Architecture Review, even with green tests. This is a new decision
about the payload handoff; it does not reopen the existing ADR-0069 approval. Independent
work such as NR-017's read-only diagnostic inventory can proceed while it is reviewed.
