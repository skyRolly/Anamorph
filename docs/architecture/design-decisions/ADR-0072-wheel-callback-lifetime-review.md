# ADR-0072 — Wheel callback lifetime and gesture ownership (NR-026 / KI-035)

**Status:** Accepted for the B2 architecture — owner approval recorded 2026-11-06 (Asia/Shanghai).

The owner explicitly states in the current assignment that human Architecture Review
is complete and approves B2 as reviewed in PR #221. That PR's reviewed head
`89af3c1940613b67fa5010a13f0d03d0ee8bc8b8` is integrated by merge
`b9f94d83bb7ed3fde97240ca0868ed7ac1d97512`. This is owner-provided approval
provenance, not an inferred GitHub approval or an invented earlier review date.

Approved scope: a lifetime-stable message-thread interaction record; exact-once
host gesture ownership; processor-owned Undo/endpoints; standalone event lease;
held Wheel borrowing the press record; lifecycle-safe detachment; focused Knob
binding adaptation and synchronous event-boundary staging; preservation of JUCE
drag calculations and Accepted ADR-0052/0053; official JUCE 9.0.5 unchanged.
The lifecycle contract in the continuation below is binding implementation acceptance.

Implementation is authorized but not yet validated or integrated at this approval
checkpoint. Each actual production diff requires regression/compatibility evidence,
separate human PR review and separate merge authorization. No unrelated attachment
family, DSP, threading, parameter/schema/latency, dependency, native acceptance or
release change is approved. A demonstrated conflict requiring an architecture
revision must return with that specific conflict. The historical D recommendation,
B1 rejection and earlier Proposed-stage wording below remain dated history.

## Context

Integrated `main` after PR #215 is `b0a063d2db982e3b6cdd1c403abce08098d1cfa1`.
Its product build inputs equal `f1d3ff81e0f239d0bec7c8873f54ebfce78e15c7`.
Accepted [ADR-0071](ADR-0071-juce-9.0.5-upgrade.md) authorizes JUCE 9.0.5 at
`d60b7a78030661574a615b586d9325ba7a7fa7dd`. Accepted
[ADR-0053](ADR-0053-a-wheel-belongs-to-the-interaction-it-lands-in.md) governs press
ownership, wheel travel and Undo grouping. Both remain binding.

The [callback review packet](../../../worklogs/GUI_CALLBACK_LIFETIME_REVIEW.md)
and [supporting ledger](../../../worklogs/engineering-review/nr026-juce905.json)
retain distinct standalone and held-wheel observations. The corrected
[Linux reproduction](../../../worklogs/engineering-review/nr026-repro/README.md)
verifies a disposable user/preset/temp environment before the complete State suite
and each diagnostic. The new isolated run reconfirms both live controls and both
teardown failures, with 13,959 ordinary State checks passing.

## Problem

Synchronous Width parameter notification can destroy the editor inside a wheel
write. The standalone case then reads a freed Slider in JUCE's
`ScopedDragNotification` destructor. The held case instead returns from `setValue`
to `Knob::takeWheelNotch` and reads the destroyed knob while banking the movement.
A dependency-only correction cannot resolve both witnesses.

Avoiding the freed read alone does not establish gesture or Undo correctness.
In the pinned JUCE source, `SliderParameterAttachment` destruction removes the
Slider listener, while `ParameterAttachment` destruction removes the parameter
listener and cancels its async update; neither destructor explicitly calls
`endGesture`. Whether another surviving owner completes a particular teardown
gesture needs its own oracle. The ASan-terminated cases provide no such result.
Native DAW incidence and processor destruction during the notification are unmeasured.

## Options

| Option | Proposed scope and compatibility impact | Costs, risks and rollback |
|---|---|---|
| A. Bounded combined correction | Review JUCE `juce_Slider.h/.cpp` scope/notification lifetime and gesture delivery together with first-party `Knob::takeWheelNotch` continuation. Determine whether attachment changes are necessary to close an applied edit exactly once after component destruction. Preserve the approved pin's remaining behavior and ADR-0053 interaction contracts. | Maintained dependency delta, upstream rebasing and exact source/pin provenance. A weak-pointer-only patch is insufficient evidence of gesture completion. Roll back the dependency delta and companion product change together to the approved 9.0.5 inputs. |
| B. Product-owned wheel route | Replace the affected standalone notification route with product-owned lifetime and gesture handling, plus the held continuation correction. Preserve the existing arithmetic, attachment writes, press routing, duplicate suppression, interval/snapping, direct mouse interactions and Undo grouping. | Larger first-party event surface and framework-parity burden; must avoid double gestures through retained attachments. Roll back the complete route and its lifetime owner together. |
| C. Retain implementation with a documented risk | Keep current source while explicitly recording the two verified destruction premises and any resulting release restrictions. Ordinary controls remain passing. | Both defects remain unresolved; no owner risk acceptance is supplied. No rollback is needed because source does not move. A release while unresolved needs a separate owner disposition. |
| D. Defer production implementation | Preserve the current source and evidence until the owner decides the teardown gesture contract and implementation route. Continue independent maintenance. | Delays resolution; does not close KI-035 or authorize release. No production rollback is needed. |

All implementation options must retain parameter IDs/ranges, serialization,
reported latency, DSP order and realtime behavior. No new audio-thread work,
allocation, blocking or cross-thread path is proposed. A source/pin change is gated
by the Dependency Policy; an event-ownership change requires review against ADR-0053.

## Historical interim decision (before the owner-directed Option B continuation)

Recommend **D now**, pending explicit owner review of A–C. The evidence distinguishes
the two lifetime paths but does not yet justify selecting a complete gesture owner
or claiming a production fix. No implementation experiment in shipping source is
authorized by this proposal.

The concrete owner decisions are:

1. For a notch already applied while the processor survives, should editor
   destruction complete exactly one parameter gesture and retain the existing
   meaningful Undo step? This is the proposed acceptance contract for review;
   interrupted/no-edit and processor-destruction cases need explicit scope.
2. Should the implementation be a combined bounded JUCE/product correction (A),
   or may wheel-event ownership move into the product (B)? A chosen route must
   return with an exact diff, dependency identity if applicable, and validation
   evidence for approval before gated production implementation/integration.
3. If neither is authorized, continue D or explicitly consider C with its release
   implications. No risk acceptance is inferred from the JUCE upgrade or CI.

## Validation required for a selected correction

Retain the current exact-source live controls and failure witnesses. Add controls
for destruction at gesture begin, value notification and gesture end, with a
surviving processor. Observe parameter begin/end counts and Undo/Redo state using
surviving witnesses, including reopen and repeated cycles; a clean sanitizer exit
alone is insufficient. Include no-edit/rail events, positive/negative and horizontal
fractional deltas, reverse direction, duplicate event times, active press routing,
snapping and interval floor, modifier/mapping changes, direct drag/reset/value-box
controls, and State 86/88 and the other ADR-0053 regressions.

Instrument all reached JUCE, attachment and first-party continuations for the
chosen route; retain original and guard-removal controls that prove detector
liveness. Preserve the full State/DSP suites, sanitizer/memcheck, realtime/TSan,
fuzz, host-format CI and build/source provenance. Host-specific acceptance remains
a later owner-controlled gate; broad native testing stays deferred in this phase.
Validate a rollback against the approved JUCE/product inputs.

## Consequences

This Proposed record makes the missing review decisions explicit. It leaves
Accepted ADRs intact, retains NR-026/KI-035 as open and separates isolated evidence
tooling from production correction. Any selected implementation requires its exact
scope and approval to be recorded here before this proposal can become Accepted.

## Related code

- `src/PluginEditor.h`: `Knob::mouseWheelMove`, `mouseWheelMoveTail`,
  `sendWheelToJuce` and `takeWheelNotch`.
- `src/gui/LookAndFeel.h`: shared wheel arithmetic and press-ownership helpers.
- Pinned JUCE `modules/juce_gui_basics/widgets/juce_Slider.h/.cpp` and
  `modules/juce_audio_processors/utilities/juce_ParameterAttachments.h/.cpp`.
- `tests/state_tests.cpp`: ordinary State 86/88 and interaction regressions.

## Evidence and confidence

**Verified:** pinned source inspection, source hashes, controlled Linux/Xvfb
live/teardown observations, isolated path witnesses and full ordinary State-suite
results in the linked ledger. **Proposed:** teardown gesture contract, correction
route and expanded validation matrix. **Unmeasured:** post-teardown Undo/gesture
outcomes in sanitizer-terminated cases, native host incidence and processor teardown.
The roadmap owns execution status; supporting ledgers do not authorize architecture.

## Owner-directed Option B continuation — 2026-11-06 UTC

The owner explicitly prefers **B, product-owned handling**, preserving official
JUCE 9.0.5 and Accepted ADR-0052/0053/0071. This authorizes investigation and bounded
nonshipping experiments, not unconditional implementation/integration or Accepted
status. The earlier recommendation of D above was the previous interim disposition.
The [concrete review packet](../../../worklogs/NR026_OPTION_B_REVIEW.md) supplies
an exact rejected B1 overlay, separate source/build/result identities and the
proposed B2 ownership boundary. Historical NR-026 observations are unchanged.

The proposed acceptance contract is now explicit:

- An applied edit survives editor destruction while the processor lives. Require
  exactly one host Begin and End, no open gesture or duplicate End, meaningful
  Undo to the preceding value, actual Redo to the applied value, and correct state
  after reopen. No destroyed component, attachment or editor may be accessed.
- No-edit events produce no meaningful Undo, manufactured change or unrelated edit.
  Preserve zero/invalid/rail behavior and duplicate suppression under ADR-0052.
- A held Wheel belongs to the press owner under ADR-0053. It updates that drag's
  actual anchor/banks with no separate step/gesture and preserves routing,
  absolute/relative/velocity mapping, modifiers, snapping, intervals, multi-device
  ownership, cancellation and once-only completion.
- Standalone input preserves direction/reverse/axis selection, range/snap/interval
  rules, named-control coalescing, unrelated-edit separation and text commitment.
- Investigate destruction before edit, during Begin/value/End, after application,
  interrupted no-edit cases and repeated edit/destroy/reopen cycles. Processor
  destruction is a separate boundary, outside this surviving-processor contract.

**Observed B1:** actual Width components and parameter callbacks, two cycles on
one surviving processor per non-aborting case, with host-route witnesses. Of 49
cases, 31 pass, 12 ASan-fail in held press begin/release end continuations, and six
fail the gesture/history oracle (five held-value cases and one intentional omitted
End). Standalone's 24 cases pass the limited matrix. Held-value teardown leaves
Begin=1/End=0 and no Undo despite an applied value and clean ASan return. B1 does
not satisfy the contract. Parameter-listener duplication during attachment removal
is recorded separately and is not mistaken for duplicate host gestures.

**B-modified proposal:** a lifetime-stable message-thread interaction record,
shared by the active event stack and binding, owns exact-once gesture completion
and survives editor detachment. The processor retains endpoint/Undo ownership.
Standalone uses an event lease; held input borrows the original press record.
Reuse Wheel helpers, routing, dispatch, refusal/endpoint and coalescing infrastructure.
The review surface includes the Knob binding and synchronous event-boundary staging
of teardown-capable notifications, preserving JUCE's drag math rather than rewriting
it. Pending text commitment can re-enter JUCE's textChanged scope and needs its own
proof. The packet identifies exact affected functions, state transitions, compatibility
risks, validation gaps and rollback. B2 is proposed and unimplemented; no final
production diff or complete parity argument is claimed.

**Current decision gate:** recommend further B-modified review; retain D in
production. Review the stable-record, binding replacement and within-event gesture
timing against Accepted ADR-0052/0053 under ARCHITECTURE_REVIEW_GATE Procedure 2–3.
The direction instruction is not that concrete approval. After review, an exact
implementation returns in a separate PR with the complete lifecycle/interaction
matrix and final-head validation. If bounded B2 cannot preserve framework behavior,
evaluate combined A (JUCE plus held product continuation and stable gesture/Undo
ownership), with explicit dependency provenance/maintenance/rollback costs. No JUCE
change is authorized here. If neither has an acceptance argument or required approval,
continue D. C requires a separate explicit owner risk disposition. ADR-0072 remains
**Proposed** until separately documented human architecture acceptance.
