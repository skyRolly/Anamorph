# NR-026 Option B continuation and review gate

Current decision — 2026-11-06 (Asia/Shanghai): the owner explicitly approves the
B2 architecture and authorizes implementation. ADR-0072 is Accepted for that scope;
PR #221 is integrated at `b9f94d83bb7ed3fde97240ca0868ed7ac1d97512` after reviewed-head
CI completed. This does not accept an implementation, authorize another merge or
release. The following B1 evidence and pre-approval B2 proposal remain unchanged
historical investigation. Current implementation status belongs to the roadmap.


**Disposition: B-modified for design; D for production integration.** The owner
selects product-owned Wheel handling as the preferred direction and authorizes
bounded nonshipping investigation. This is not acceptance of a particular design.
[ADR-0072](../docs/architecture/design-decisions/ADR-0072-wheel-callback-lifetime-review.md)
remains Proposed. No shipping source, approved JUCE 9.0.5 pin, version or release
object changes in this package. The original D recommendation remains dated history.

## Source and boundaries

Latest verified main is `c2f8f19b87daae709c015a062a8b771f049da012` (#218 merged).
PR #219's corrected `b2e36abf423813adb1db126192b7690354f99863` is the B1 product
snapshot; #220's `046569242ed7b570c0b45636a663260cab2f3a6d` supplies its isolated
profile header. Product sources of those two heads are identical. The
[execution manifest](engineering-review/nr026-option-b/execution.json),
[exact overlay archive](engineering-review/nr026-option-b/b1.patch.gz), executed source snapshots,
commands, [matrix](engineering-review/nr026-option-b/matrix.json) and compressed
streams have separate identities. Historical `nr026-repro/` sources, ledgers,
observations and hashes are unchanged. This experiment is not an integrated fix.

## Ownership chain and minimum scope

`Knob::mouseWheelMove` first calls `wheelTakenByAnyPress`. The press register routes
held events to the authoritative `WheelDragOwner`, even over another component;
a claimed but ineligible press consumes without a different edit. Knob's
`takeWheelNotch` filters timestamps, uses `wheelTargetValue` (dominant axis,
reverse direction, proportion mapping and interval floor), synchronously sets the
Slider and banks actual movement in both absolute pixels and velocity debt.
The current freed continuation occurs between the write and banking.

Without a press, `mouseWheelMoveTail` performs the standalone callback, names the
control using processor `ScopedWheelStep`, and `sendWheelToJuce` invokes Slider.
JUCE commits an open text box, checks range/timestamp/delta, applies snap/range rules,
opens `ScopedDragNotification`, sets the value and closes the scope. The editor's
before/after `AttachmentWitness` straddles the real attachment. Its parameter write
can synchronously delete the editor. The JUCE scope then dereferences the freed
Slider. Witness and attachment destruction do not establish a completed gesture.

Processor `parameterGestureChanged` owns batch ownership and pending commit;
`noteOwnedParamWrite`/`Endpoint`/`Refused` identify the user endpoint, and
`pollUndoCoalesce` commits it. `ScopedWheelStep` supplies the name at the final
close; it must stay alive through that close. Existing `ScopedUserTransaction`
and dispatch brackets prevent a pumped poll/state command from committing a
partial action. Preserve their existing admission and nested-key semantics.
All candidate state is message-thread state; no new audio-thread operation,
thread, atomic ordering, lock, scheduler or asynchronous dispatch is proposed.

## B1: exact bounded experiment and results

B1 replaces only standalone Wheel's JUCE scope with a stack lease holding surviving
processor/parameter pointers. It commits text, uses existing wheel arithmetic,
snaps/clamps, checks no-edit, starts the parameter gesture, guards the component
before writing, records/refuses the endpoint through the surviving processor and
ends through the lease. Held Wheel adds a weak component check immediately after
`setValue`, leaving the existing mouse/attachment gesture implementation in place.
The standalone callback is copied to stack and guarded. The patch is experimental,
not an approved implementation; notably it reuses the held timestamp field and
does not prove parity with JUCE's independent standalone timestamp state.

The final 49-case Linux/Xvfb matrix uses actual components and parameter callbacks,
a surviving processor and **host-route AudioProcessorListener counts**. Each case
attempts two edit/destroy/reopen cycles on that same processor. Positive/negative,
horizontal, reversed, zero and duplicate timestamps are crossed with standalone/
held and live/begin/value/end phases. Cases that ASan terminates do not complete
two cycles. Five JUCE GUI TUs, processor, editor and probe are ASan-instrumented;
remaining linked objects are not. This is not a full UBSan or native DAW result.

| Observation | Value/host gesture/history evidence | Disposition |
|---|---|---|
| Standalone live and begin/value/end teardown | 24/24 cases pass both cycles. Positive value teardown: 0.5→0.530000031, Begin=1/End=1, Undo=0.5, Redo=0.530000031, reopened display correct. Begin teardown cancels before edit with 1/1 and no meaningful Undo. Zero movement has 0/0 and no meaningful Undo. | Supports this limited standalone lease; not complete Option B acceptance. |
| Held live, zero and duplicate controls | Seven cases pass. Live applied notches retain one original press gesture, correct Undo/Redo and reopened state. | Selected ordinary component controls only; full drag/mapping matrix unmeasured. |
| Held value teardown | Five cases fail the contract without ASan: positive 0.5→0.530000031, Begin=1/End=0, no Undo; value and reopened display survive. Negative/reverse 0.5→0.470000029 has the same missing close. | Weak component guard alone is insufficient. Redo equality without an Undo step is not a successful Redo operation. |
| Held press begin and release end teardown | Twelve cases ASan-fail: `Slider::Pimpl::mouseDown` at currentDrag assignment, and `Pimpl::mouseUp` at popupDisplay cleanup. These are framework continuations, before Wheel or after its release respectively. | Additional reachable lifecycle boundaries for a complete correction. No claim about post-abort gestures/history. |
| Omit standalone lease End | One deliberate mutant fails: Begin=1/End=0, applied value survives, no Undo. | Proves the history/gesture oracle distinguishes clean sanitizer exit from acceptance. |

Total: **31 passing cases, 12 ASan failures, six contract failures including the
intentional omit-End control**. Failures are retained, not suppressed to make B1 green.

The initial oracle counted mutable parameter-listener delivery. JUCE iterates its
listener array backwards; deleting attachments during that callback can deliver
the observing listener twice as lower entries disappear. The retained initial
observer recorded 2/1 or 1/2 where the host route receives 1/1. Those are **not**
duplicate host Begin/End findings. The final observer separates both counts and
checks the processor's host notification route; raw initial evidence is retained
as oracle calibration. Actual DAW automation recording remains unmeasured.

## B2: concrete proposed architecture for human review

B1 is rejected. Continue Option B by giving a Knob parameter binding a **stable
message-thread interaction record**, held by the binding and copied to each active
event stack. It references the surviving processor/parameter, owns the gesture
state and endpoint, and holds only a weak component handle. The processor continues
to own Undo/history; the record neither stores a second history nor runs on audio.

Proposed record transitions are `idle → beginPending → open → endPending → closed`.
Editor detachment marks the component unavailable and requests cancellation/close;
it cannot free an active event's record. Before calling external Begin, mark the
record open and enter dispatch; before external End, mark it closed and consume
the close request. A depth/token prevents recursive detach/end from dispatching a
second close. A Begin-time detach returns from Begin before the matching End;
it cancels an unapplied store and states refusal. Value-time detach lets the
surviving record state the installed endpoint and complete exactly once after
the synchronous write returns. End-time detach cannot request another End.
No operation reads the dead Slider, Knob, witness or attachment afterward.

A stack lease suffices for standalone Wheel. A held press needs the **same record
from press start through Wheel, subsequent drag and release/cancel**. The Wheel
must borrow that record, not begin an independent gesture or finish the drag.
Wheel banking occurs only while its weak component is live, from actual snapped
movement in both existing banks. On detach, anchor state is discarded with the
component while the surviving record closes the original gesture and preserves
its meaningful endpoint.

The concrete review surface is:

| Surface | Proposed change and compatibility obligation |
|---|---|
| `PluginEditor.h` Knob standalone route | Product event scope, copied callback, independent timestamp parity, finite input/no-edit checks, legal target from existing helpers, stable gesture lease. No `Slider::mouseWheelMove` on this affected route. Preserve fallback for disabled/unbound controls. |
| `PluginEditor.cpp::attachSlider` and a focused binding type | Preserve initial parameter→Slider sync, normalized range conversion/snap functions, value↔text functions, parameter echo suppression and attachment registration order. Give gesture dispatch to the stable record; retain one parameter writer and one host Begin/End owner. The current opaque JUCE attachment has no public cancellation/gesture-transfer API. A custom Knob binding is a candidate, not approved code. |
| Knob press/release framework boundaries | Do not dispatch teardown-capable Begin/End while inside the vulnerable JUCE `mouseDown`/`mouseUp` continuations. A proposed synchronous staging scope records binding intent inside the framework call and flushes before the product event returns. Keep JUCE drag calculations/capture/velocity/modifier behavior. This changes notification timing within the same event and explicitly requires architecture review and parity tests. |
| Wheel-triggered text commitment | Commit the real ValueBox/TextEditor before calculating the notch. `hideTextBox(false)` can enter JUCE `textChanged` and another ScopedDragNotification; a weak pointer outside it is insufficient. Stage parameter/gesture delivery until that framework commit frame returns, then commit the typed edit and re-read the target. Preserve text parsing, normalization, callbacks, text-versus-wheel Undo boundaries and no extra echo. If this requires rewriting general text input, reject/narrow the design and evaluate A. |
| `AttachmentWitness` / processor endpoints | Keep existing request nesting, refusal, endpoint attribution, dispatch depth and named Wheel coalescing. New stable scope must not outlive the processor or allow an attachment destructor to erase a pending installed endpoint. Do not invent a second coalescer or weaken ADR-0008/0052/0053. |
| `gui/LookAndFeel` / ValueBox and press register | Reuse dominant-axis/target helpers and routing/ownership APIs; preserve ValueBox forwarding and existing `DragGestureOwner` cancellation. No Multiband redesign or new physical-device routing. |

This is a reviewable **architecture proposal, not an implemented B2 correction**.
The exact B1 diff is supplied as a rejected reference. A B2 production diff is
not represented as ready: the attachment/parity/staging proof remains outstanding.
It must return as its own implementation PR after architecture review; do not
integrate B1 to obtain partial coverage.

## Acceptance and remaining matrix

For every applied edit with editor destruction and a surviving processor, require
observable applied value, exactly one host Begin/End, no permanently open gesture,
meaningful Undo restoring the preceding value, Redo restoring the applied value,
and correct display after reopen. Count only an actual Undo then Redo as that
proof. Before-edit destruction cancels; Begin-time interruption ends once without
an unapplied edit; End-time interruption cannot double-close. No-edit teardown
must manufacture neither edit nor meaningful step. Repeat cycles on one processor.

Before B2 can be accepted, execute the existing State86/88 and ADR-0053 tests plus
focused actual-component tests for fractional input, rails/interval floors, NaN/
infinity, duplicate timestamps across standalone/held transitions, positive/
negative/horizontal/reverse deltas, repeated same-control grouping and unrelated
control separation, press routing/ineligible presses/multiple devices, actual drag
continuation, absolute/relative/velocity modes, modifier transitions, both banks,
snapping, ValueBox editing/commit/cancel/focus-loss, text-to-wheel Undo, begin/value/
end/destruction-before-edit, repeated reopen and guard/lease/endpoint mutants.
Run full DSP/State/sanitizer/memcheck/TSan/realtime/fuzz/platform/pluginval/static/
documentation gates on the final implementation head. Current #219 CI validates
its filesystem changes, **not this prototype or future B2**.

These unexecuted B2/text/mapping cases remain **unmeasured**, not passing. No broad
native testing is resumed. Processor destruction is a separate boundary: JUCE's
processor destructor expects its editor already gone; that does not guarantee a
raw processor pointer survives arbitrary destruction inside a notification.
This proposal requires the processor to outlive the entire event/record close.
Concurrent/recursive processor teardown needs a separate owner protocol and proof.

## Alternatives, rollback and decision gate

B1 establishes that replacing the standalone scope plus guarding a held return
is insufficient. It does not establish that all product-owned designs are
unmaintainable. **B-modified** is therefore the investigation recommendation.

Option A is a fallback if B2 needs disproportionate framework duplication or
cannot preserve binding/text/drag behavior. Its minimum combined scope includes
JUCE standalone scope lifetime, begin/end mouse continuations where reached,
attachment gesture survival, and the product held-wheel continuation. Weak pointers
alone cannot supply a missing host End or Undo endpoint. A requires an exact
reviewed dependency delta, official-source provenance, rebase burden across JUCE
upgrades and joint product/dependency rollback. No dependency patch or pin change
is authorized or made here. A dependency-only repair would still leave the product
continuation and gesture ownership unproved.

B2 is **not accepted** and no maintainability/compatibility claim is inferred from
B1's partial success. Retain D in production until the concrete architecture clears
[ARCHITECTURE_REVIEW_GATE](../docs/policies/ARCHITECTURE_REVIEW_GATE.md), which says
“Detecting any gated change is an AI Agent Hard Stop — the agent stops and requests
human review rather than proceeding.” Event ownership and changed gesture timing
need review against Accepted ADR-0052/0053; opaque binding replacement is not
covered by a direction preference. The owner must review the stable-record,
attachment and synchronous staging scope, then review its exact implementation
and full acceptance evidence before acceptance/integration. If no bounded B2 or A
has that argument, continue D. C requires an explicit separate owner risk decision.

Rollback for this package is closing its review PR; shipping inputs never moved.
A future B2 rollback must restore binding, event scopes and gesture record together
and rerun the accepted interaction matrix against official 9.0.5. No release
asset rebuild or modification follows from either action.
