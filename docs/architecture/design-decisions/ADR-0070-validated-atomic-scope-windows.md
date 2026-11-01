# ADR-0070 — Generation-validated atomic stereo windows for ScopeBuffer

**Status: Accepted.** The owner explicitly approved ADR-0070 for implementation
on 2026-11-01 (Asia/Shanghai), replying “Approve ADR-0070 for implementation” to
the submitted PR #199 architecture question, subject to its native portability,
performance and validation gates. Originally proposed by the agent on that date,
against integrated `main` `00de8c7796f37d08a338256e01830bb312ba105c`.
The architecture approval is **cleared**. The owner subsequently separately approved
integration of #199/#202, now on main via `303d37c`/`4255393` (2026-11-02,
Asia/Shanghai). Representative performance/owner cost acceptance and targeted native
visual/editor-lifecycle acceptance remain pending; new PR merges remain separate. The new scope-affinity contract
is effective for the approved atomic implementation, not an exception permitting
the old plain-payload race. Existing release objects remain unchanged.

## Context and problem

`ScopeBuffer` copies reusable plain float arrays, then publishes a release index.
Acquiring that index makes earlier writes visible; it gives the GUI no ownership
against the next overwrite. A reader of W=16384's newest 8192 frames can be reading
slot 8192 as a later push writes position 24576 there. The conflicting plain
accesses have no happens-before relation. A post-copy index check cannot repair
the data race. Aligned float access, display-only use and a clean sanitizer run
do not change the C++ rule. No supported-host crash or audible corruption is
demonstrated. The display-only dataflow does not confine undefined behavior's effects.

The single writer is the end of `AnamorphEngine::process`. Readers are
`Vectorscope::tick`, `Vectorscope::paint` and `SpectrumImager::pushFFT`.
FFT windows contain 8192 frames; vectorscope windows contain 1200–8000.
On macOS/Windows, pinned JUCE component painting can run on the OpenGL thread
with the message-manager lock held. GUI accesses are serialized, but the literal
message-thread-only scope policy is not satisfied. Linux/BSD retain CPU painting.

The [research packet](../../../worklogs/NR006_SCOPE_PUBLICATION_REVIEW.md#6-post-integration-investigation)
contains the source census, lifecycle, alternatives, experiments and limitations.
The investigated original blob is `ed24ceab5ced2784d9fe96effdc46f8aeb03f472`
at the historical integrated main `00de8c7`; #202 replaces it on actual main as
recorded in the packet’s integration checkpoint. Context above describes that original code.

## Required invariants

- One audio producer; producer progress independent of hidden, stalled or absent GUIs.
- No audio allocation, lock, wait, CAS loop or reader-driven retry. Fixed capacity 16384.
- Same engine output, processing order, parameters, state format and reported latency.
- Accepted windows contain the selected committed positions in order, with indivisible
  stereo frames. A contended copy may fail; it must never be displayed or transformed.
- Ordinary serial reads retain newest/count/clamp/wrap/oversized-push behavior.
- Explicit reader-affinity, staleness, storage-lifetime and failure contracts.

## Options considered

| Option | Safety, semantics and cost | Disposition |
|---|---|---|
| A: individually atomic samples | Race-free with lock-free atomics; independent L/R observations can straddle reuse. Same 128 KiB payload, two stores/loads per frame. Bare index publication still permits mixed windows. | Viable lower guarantee; reject as the selected contract because stereo and FFT inputs can mix epochs. |
| B: packed atomic stereo, relaxed payload + release/acquire index | Race-free, bit-preserving pairs with compile guards. One atomic word/frame, 128 KiB. Mixed-window epochs remain possible even though each pair is intact. | Viable and cheapest atomic candidate; deterministic control demonstrates why it does not meet the recommended accepted-window contract. |
| C: full-history immutable triple snapshots | Producer owns staging history and one writable snapshot; one atomic middle-slot exchange hands a completed snapshot to a serialized logical consumer, which returns its previous private slot only after copying. No reclamation race or wait, even with a stalled reader. About 512 KiB; a full available history is copied each block. | Safe complete alternative; reject for disproportionate small-block producer work and extra ownership/lifecycle state. Two buffers alone cannot preserve a held snapshot and always publish without either dropping or waiting. |
| C variant: request-driven snapshots / shared pages | Bounded publication can skip when no free slot exists; request-driven full copies reduce average traffic. Needs explicit stopped-audio final-window behavior, reclamation/page ownership and bounded exhaustion handling. | Not preferred: additional state or staleness changes, with no measured need for them. Never allow the producer to reuse a held buffer. |
| D: generation-validated atomic stereo ring | 256 KiB payload/tags; three stores per retained frame, three loads per attempted frame. No ownership reservation or reader state in the producer. Accept only exact committed positions; fail once on reuse. | **Recommended**, conditional on the native verification gates below. |
| D variants: bounded SPSC queue, hazard/RCU, atomic-payload block seqlock | Queue ownership is safe until exhaustion, but preserving newest history for multiple independent peeks requires another protocol; dropping queued newest data changes recovery. Hazard/RCU needs safe bounded reclamation, and a stalled reader cannot pin writable history indefinitely. A correctly ordered all-atomic seqlock can be safe, but invalidates a read on any overlapping push rather than only reused slots. | No advantage established over per-slot validation for these windows. A plain-payload seqlock remains undefined even if validation detects the overlap. |
| E: retain/document current arrays | Preserves current bytes and cost; still violates the non-atomic shared-state rule. Documentation is no synchronization. | Temporary implementation state only. Not a compliant correction; no risk waiver is enacted. |

Whole-window coherence is chosen because the FFT and silence trackers interpret
positions and fresh-tail length as one history window. Detecting an overwritten
copy and keeping the last accepted display is preferable to silently transforming
mixed history. Maximum coherence is not assumed intrinsically necessary: B is a
real lower-cost alternative, but the local D cost is small enough to justify
native verification before sacrificing that guarantee. The synthetic result does
not establish production cost on ARM or modest hardware.

## Accepted decision and exact protocol

### Representation and lifetime

Use a fixed `std::array<Slot, 16384>`, where `Slot` has **two** separate
`std::atomic<std::uint64_t>` members: `tag` and `stereo`. This does not require a
128-bit atomic. Initialize tags and values to zero at processor construction.
Keep a separate 64-bit atomic total `publishedEnd`, initially zero.

Pack left float bits into the low 32 bits and right into the high 32 bits using
`std::bit_cast<std::uint32_t>` and integer shifts; unpack inversely. Guard
`sizeof(float)==4`, IEC 559 representation and
`std::atomic<std::uint64_t>::is_always_lock_free`. Normal member alignment must
meet each atomic's requirements; never packed structs or byte aliasing of atomics.
No memcpy/SIMD/plain access to atomic payload storage. Guard failures are build
failures, with no hidden-lock fallback or silently unsupported platform.

Slot tag zero means invalid/unwritten; absolute frame position p uses tag p+1.
The lifetime contract excludes exhausting the 64-bit frame counter/tag space.
At 384 kHz that takes over 1.5 million years. This is an explicit finite-epoch
boundary, not a claim of mathematical ABA immunity over arbitrary counter wraps.
Modulo ring wrap is supported; numeric epoch wrap needs a new quiescent-lifetime
design before it can be supported. No live reset of tags or publication counter.

### Producer

For valid nonnegative n, let W be the previous total and E=W+n. Retain only the
newest min(n,capacity) input frames; do not process discarded oversized prefixes.
For each retained absolute position p, in order:

1. Store zero to `slot[p & mask].tag`, **release**.
2. Store the packed stereo frame to `.stereo`, **release**.
3. Store p+1 to `.tag`, **release**.

After all retained frames, store E to `publishedEnd`, **release**, once per block.
The producer's count read may be relaxed because there is exactly one producer.
There is no reader count, free-list, request or reservation to await. Work is
O(min(n,capacity)), with exactly three payload/tag stores per retained frame and
one index publication. No operation is a read-modify-write.

### Reader and admitted results

Load E from `publishedEnd`, **acquire**, exactly once for this attempt.
Clamp the requested count to [0,capacity], then to the available frames E.
For each expected position p in [E-count,E), in order:

1. Acquire-load tag; if not p+1, fail this attempt.
2. Acquire-load packed stereo into a local word.
3. Acquire-load tag again; if not p+1, fail this attempt.
4. Unpack the word into caller-owned **scratch** buffers.

Return an explicit result carrying validity, copied count and that **same E**.
Do not conflate a rejected copy with a successful empty window. Empty initial
windows are valid. On rejection, partially written scratch is uncommitted;
neither waveform, FFT, silence detection nor freshness bookkeeping may consume it.
There is **one attempt per ordinary read**, with no internal retry.

On success, every pair belongs to its requested absolute position from the
committed window ending at E. The window may already be stale when returned;
there is no promise it is the newest index at completion, or that a reader which
is repeatedly suspended ever succeeds. Reuse can cause rejection, never mixed
accepted epochs. There is no all-GUI-consumers-same-epoch guarantee.

`writeCount()` remains an acquire-loaded hint. Consumers must derive freshness
for an accepted copy from its returned E, not an earlier independent hint.
They may retry on the next normal tick/paint, retaining their last accepted data.
Fixed preallocated scratch and accepted buffers swap only on complete success.
No reader allocation in hot copy/paint paths. A failure must not advance
`lastSeenCount`, `lastNonZero`, or a last-window-silent state. SpectrumImager's
second full-window read after a silent-tail wakeup must obey this rule too.

### Memory-model proof

All conflicting ring payload/tag accesses are atomic; slot objects live for the
processor lifetime. No acquisition, release or validation protects plain shared
samples because the new representation contains none.

For position p, acquiring its valid tag (or the committed end) orders its payload
store before the reader. Atomic write-read coherence forbids the payload load
from seeing an earlier incarnation of that slot. If the payload load reads a
**later reuse's release store**, its acquire synchronizes with that store. That
reuse's preceding invalidation then happens before the second tag load. Tag
write-read coherence forbids that load from seeing the old expected p+1; it sees
the invalidation or a later tag and rejects. If the payload comes from p, its
bits are correct even if the writer starts reuse afterward. The unique tag
within the stated lifetime prevents an unrelated incarnation from passing.
Apply this argument independently to every accepted position: they form exactly
the selected committed window. No instantaneous lock or storage ownership is claimed.

Both payload release/acquire and pre-payload invalidation matter. The bounded
read-from model admits a wrong accepted value when either is removed. Relaxed
payloads are sufficient for B's pair-only contract, **not for this proof**.
The before/after tag checks validate defined atomic accesses, not a
post-copy attempt to legalize undefined plain-memory reads.

### GUI affinity and lifecycle

**Preserve current OpenGL component painting.** Permit ScopeBuffer
reads on the message thread or inside pinned JUCE's component-paint path while
holding the message-manager lock; all GUI consumers remain serialized and keep
separate destinations/caches. This Accepted amendment to the scope reader-affinity
rule is effective in the integrated atomic implementation, not a general off-thread permission
for program state, and not an exception for the old racy representation.
The audio producer never acquires that lock. No new thread or audio-to-GUI callback.

Moving all ring access into message-thread ticks and painting a cached snapshot
would also satisfy the old literal affinity rule. It adds refresh/cache lifecycle
and can show an older frame on expose/resize; no demonstrated host issue justifies
replacing the existing GPU path. Removing OpenGL entirely has wider rendering and
performance implications and is rejected for this bounded correction.

Opening an editor constructs its private buffers away from audio. Closing/hidden
editors reserve nothing in the producer. Keep existing FrameClock shutdown and
editor OpenGL detach-before-member-destruction order. Host prepare/reset already
do not clear ScopeBuffer: preserve count/history through both, with host exclusion
of concurrent process calls as today. Do not add a concurrent reset writer.
Processor destruction still requires stopped audio and detached/destroyed editors.

## Consequences, limits and implementation boundary

The engine output and latency are independent of display data and must remain
byte-identical in same-configuration twin dumps. The shared ring grows from
128 KiB to 256 KiB plus its index/alignment; private last-accepted/scratch buffers
can add up to 192 KiB per open editor (vectorscope capacity + FFT window, stereo).
Measure actual sizes. Copy failure retains old display rather than blanking or
running FFT on partial data. No serialized/session/public parameter change.

Atomic loops replace the old vectorizable segmented memcpy. Local x86-64
measurements favor D over full-copy C, but do not bound production WCET, cache
contention or ARM cost. Lock-free is not synonymous with wait-free or a wall-time
deadline: inspect native code generation for bounded non-RMW loads/stores on
every shipping platform. Compiler/library guards must hold on Linux Clang,
MSVC x64, AppleClang x86-64 and arm64; Linux GCC remains a separate test lane.

The focused implementation PR #202 changes ScopeBuffer, its three consumer
paths, regression surfaces and triggered docs. Its integration was separately
approved by the owner and completed at `4255393`. DSP topology, OpenGL attachment
policy, compiler/dependency pins, release objects and unrelated state ownership
remain unchanged. Policies, architecture and source comments implement this
Accepted contract. This integration grants no release operation or future PR merge.

## Acceptance and rollback

1. **Human gate — cleared 2026-11-01:** the owner explicitly accepted this ADR's
   window and GUI-affinity contracts for implementation. Later validation, manual
   acceptance, PR merge and release gates are unchanged.
2. **Portability gate:** native compile-time guards and generated-code inspection
   on all shipping architectures; no lock-based atomic library fallback.
3. **Behavior:** preserve Test 61 and add deterministic overlap/lapping, first/second
   tag overlap, wrap/boundary/oversize, empty, slow/stalled/independent consumers,
   re-prepare/reset and staleness cases. Seed removed invalidation/validation/order
   controls on a suitable model and rejected-copy consumer-state controls. No sleep-only oracle.
4. **Dynamic safety:** genuinely concurrent production-path stress with accepted/
   rejected/overlap counts, TSan preceded by its seeded race canary, existing RTSan,
   allocation/source-policy checks, full suites and native pluginval gates unchanged.
5. **Performance:** repeatable native push/read and full-engine before/after
   measurements, at least five repeats with CPU/OS/compiler/flags/block/rate,
   median/spread/worst observed block, large blocks, active/idle/stalled GUI and
   footprint. Shared-runner results are synthetic only. Representative hardware
   and owner acceptance of the measured cost are outstanding conditions, not an
   invented numeric budget or the old Wave 4 measurement reused as new evidence.
6. **Compatibility/manual:** unchanged same-machine/same-build engine samples and
   latency, full mandatory CI, then targeted native scope/FFT visual audition and
   editor lifecycle checks. Earlier manual approval does not apply to this design.

If native evidence invalidates the recommendation, return to architecture review;
do not silently lower the coherence contract or fall back to plain storage.
A source revert restores the known race exposure, so it is not a safe release
mitigation. Do not publish or replace 0.10.0 from this decision.

## Related code and evidence + confidence

- **Verified source:** `src/dsp/ScopeBuffer.h`; `AnamorphEngine::process` final
  scope push; `Vectorscope::{tick,paint,windowFrames}`;
  `SpectrumImager::pushFFT`; editor constructor/destructor; `FrameClock`.
- **Verified dependency source:** JUCE pin `3fb5cbc0340758596dac39f7c8de47d562b4896e`,
  `modules/juce_opengl/opengl/juce_OpenGLContext.cpp`,
  `CachedImage::{renderFrame,paintComponent,paintOwner}`. No JUCE upgrade proposed.
- **Verified nonshipping experiments:**
  `tests/experiments/nr006_scope_protocol.cpp`,
  `tests/experiments/nr006_memory_model.py` and raw results in the research packet.
  The small model is a bounded sanity check, not a complete C++ model checker;
  the source-level proof above and human review remain necessary.
- **Language basis:** C++ [data races and atomic coherence](https://eel.is/c++draft/intro.races),
  [release/acquire synchronization](https://eel.is/c++draft/atomics.order),
  [lock-free properties](https://eel.is/c++draft/atomics.lockfree),
  [bit representation conversion](https://eel.is/c++draft/bit.cast).
  These are living draft references to the relevant standard rules, not a claim
  that lock freedom of every implementation is mandated by C++.
- **Verified for recorded configurations:** integrated production implementation,
  native compile guards and matching Windows/macOS product/debug codegen,
  deterministic/concurrent/regression controls and final-main CI.
- **Unverified/pending:** representative native production and full-engine/GUI
  costs/owner cost acceptance, and targeted native visual/editor-lifecycle audition.
  No user-visible failure rate or supported-host crash attribution.

## Historical production checkpoint — 2026-11-01

The focused implementation branch implements the protocol above, typed failure/
end results, preallocated accepted GUI windows and the explicit serialized paint
contract. The [packet §11](../../../worklogs/NR006_SCOPE_PUBLICATION_REVIEW.md#11-approved-production-implementation--2026-11-01-asiashanghai)
records source/test evidence, immutable-original and weakened-protocol controls,
local production costs, the repaired test-fixture stack failure and remaining
native/performance/manual gates. No decision invariant is weakened. Architecture
approval is complete; implementation integration and cost/visual acceptance are
not inferred from that approval or local results. The existing draft is unchanged.

Subsequent native guards and matching Windows/macOS product/debug inspection
verify the ordered 64-bit scope protocol instructions for the reviewed native
configurations; the packet retains source/artifact identities and slice digests.
The native test-fixture stack failure is confirmed repaired. A later memcheck
stress-harness scheduling failure is corrected with existing test-only pacing,
retaining its workload/checks/deadline and requiring fresh final-head validation.
Representative cost acceptance and targeted native visual/lifecycle acceptance
remain open. Neither codegen inspection nor that test-only correction changes
the accepted architecture or grants integration/release permission.

## Actual integration checkpoint — 2026-11-02 (Asia/Shanghai)

The owner separately reviewed and authorized both PR integrations. #199 merged
at `303d37c3b6d469d4c0a67231b19a9a3dcbfb9e1b`; #202 at
`425539385ebfcd1ca2bab1befe3de37618ce02d2`. The synchronized candidate's tree
exactly equals final main; shipping scope/consumer/regression source is unchanged
from the approved `0df1cb8`. Final-main Build41315639676, CodeQL and MSVC analysis
complete successfully. All required invariants/acceptance criteria above remain
intact. Representative costs and owner/native visual acceptance are still pending;
this is integration evidence, not final release sign-off. Existing 0.10.0 objects
remain unchanged. Detailed identities and gates are in the
[execution register](../../../worklogs/NEXT_RELEASE_DEVELOPMENT_ROADMAP.md).
