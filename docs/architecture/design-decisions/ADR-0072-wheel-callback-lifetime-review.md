# ADR-0072 — Wheel callback lifetime and gesture ownership (NR-026 / KI-035)

**Status:** Proposed — human Architecture Review pending, 2026-11-05 UTC.
This proposal grants no implementation, merge, risk-retention or release approval.

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

## Decision (proposed)

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
