# GUI callback lifetime review — NR-025 / NR-026

## NR-026 revalidation on integrated JUCE 9.0.5 — 2026-11-05

**Disposition: confirmed still present; architecture/dependency review remains required.** The
9.0.4 and 9.0.5 `juce_Slider.cpp` files differ only in Slider popup-window association. The
wheel path (`Pimpl::mouseWheelMove` → `ScopedDragNotification` → synchronous `setValue`) and
scope constructor/destructor are unchanged. JUCE Slider header, parameter attachment and
parameter listener delivery files are byte-identical between pins. Exact source identities and
paths are recorded in the [revalidation evidence](engineering-review/nr026-juce905.json).
An upstream tag check on 2026-11-05 found no later JUCE 9.0.x or 9.1.x release tag than the
integrated 9.0.5 commit, so a different supported upstream release was not available for this
disposition. Development HEAD is not treated as a supported release candidate.

The original Width wheel probe was recovered byte-for-byte from the source-bound ledger. CMake
configured the real `AnamorphStateTests` target against verified JUCE `d60b7a78030661574a615b586d9325ba7a7fa7dd`.
The editor, probe and all five actual `juce_gui_basics` translation units were ASan-instrumented;
other linked objects were unchanged. Under Xvfb the live control changes Width 0.50→0.53, sends
one gesture begin/end, exits 0 and reports no ASan error. Synchronous editor destruction during
the same notification reports UAF in `juce::Slider::ScopedDragNotification::~ScopedDragNotification`
at JUCE `juce_Slider.cpp:1440`; the storage is freed by the Width parameter listener dispatch.
This confirms the original NR-026 failure still occurs under 9.0.5.

A separate held-knob-press probe also uses the production event route. Its live control changes
Width 0.50→0.53 and balances one begin/end. When the same listener destroys the editor, ASan
reports a second read-after-destruction at `Slider::getValue()` inside `Knob::takeWheelNotch`,
after its synchronous `setValue` returns to freed component storage. The dispatcher reaches this
via `wheelTakenByAnyPress`. This is a companion product-side lifetime witness in the held-wheel
path; it does not replace or resolve the standalone JUCE scope failure.

On the same exact target, the full State suite passes 13,959 checks. State 86 confirms ordinary
scroll grouping and Undo; State 88 confirms an in-press wheel notch and Undo. Neither drives
synchronous editor destruction from the Width notification. The teardown cases terminate at ASan,
so gesture cleanup, Undo state and further teardown are not certified for those cases. This is a
deterministic component reproduction, not DAW incidence or native/manual acceptance. Exact commands,
source hashes, output identities and limitations are in the linked JSON. No product or dependency
source was changed during this investigation.

### Architecture Review decision package

The Accepted ADR-0071 authorizes the JUCE 9.0.5 pin only. The Architecture Review Gate and
Dependency Policy still require human review for a dependency source/pin/patch change. Do not
interpret the passing JUCE upgrade acceptance or CI as approval for an added JUCE patch.

| Candidate | Evidence and costs | Gate / disposition |
|---|---|---|
| Bounded JUCE repair | The invalid read is JUCE's RAII drag scope retaining a `Slider&` across synchronous listener delivery. A repair must safely handle component and Pimpl destruction, preserve exactly-once begin/end for a live slider, and define gesture cleanup if the slider dies during dispatch. A pointer-only check after `Slider::mouseWheelMove` cannot stop the current destructor read. | Dependency/build change; requires a proposed ADR, exact source diff and JUCE patch identity, compatibility review and owner Architecture Review before implementation/integration. No exact patch selected yet because gesture ownership on destruction needs a validated design. |
| Product-owned wheel handling | Could avoid JUCE's standalone scope, but must reproduce delta direction, interval floor, snapping, duplicate filtering, attachment notification and one-step Undo. It also needs stack-owned component checks after synchronous writes. Broadens ownership beyond this proven event and risks Accepted ADR-0053 semantics. | Keep as an alternative for review; no product redesign selected without focused prototype and differential regression. |
| Retain current JUCE and document risk | Preserves approved dependency provenance and current event behavior; leaves both verified teardown paths reachable under the tested synchronous-destruction premise. | A hold disposition, not a correction. Requires an explicit owner risk/release disposition if shipping while unresolved. |

Recommended review question: select whether to pursue a bounded upstream JUCE repair or a product-owned wheel path, with gesture cleanup and Undo acceptance conditions above. If a dependency patch is selected, the follow-up implementation must be separately authorized after Architecture Review. Native DAW incidence remains unknown and no manual host run was initiated.

## Current integrated restoration and SpectrumImager review

PR #211 is integrated at `c35a9d6b` with State157 A→B→C→B coverage; its production
guards were already supplied by #209. PR #212 merged at `82c98e45`, retaining
State157 and the SpectrumImager correction and regression fixtures. The multiband
changelog cites integrated commits `af8edfdd` and `05b207e3`. NR-026/KI-035 is
a separate issue; its 9.0.4 witness must be revalidated on 9.0.5. Native/manual
acceptance stays deferred.

2026-11-03 (Asia/Shanghai). Source baseline:
`29150f312ec4a43ec85f35e61028d8338057d927`, after owner-authorized #208/#207.
This is a bounded implementation review, not certification of every GUI event or
manual host acceptance. Native/manual/performance execution remains owner-deferred.

## Historical integrated guards and State157 follow-up — 2026-11-03

This snapshot predates the subsequent #211 and #212 merges.

#209 and #210 are merged at `0d395366` and `0cd95120`; retain both actual-editor
corrections and their independent controls. PR #211's parameter-triggered restore
case reaches the same protected menu/timer callouts as #209's latency trigger.
Its source guards are superseded by integrated #209, not a distinct remaining
production fix. State157's original one-adoption fixture missed TESTING_POLICY
rule3a; use same-processor A→B→C→B with all actual callback legs and live/destructive
controls. See the [cycle ledger](engineering-review/gui-restore-adoption-lifetime.json).
The old #211 citation and compiler-warning failures are actual checker/source
integration failures, separate from historical local disk exhaustion.
#211/#212 remain open and unmerged for separate human review. Native execution
is still deferred. NR-026/KI-035's pinned-JUCE Slider risk remains unfixed and
architecture/dependency-gated; SpectrumImager coverage must not claim to fix it.

## Integrated baseline and controls

#208 merged at `a78b73334dbbbd70dc6ab6de9fd155643983d1f6`; #207 at
`29150f312ec4a43ec85f35e61028d8338057d927`. #207's final additive head is
`ab7998804a2b2161a532ff1e3de26a8ec6e8039b`; its tree equals the merge tree.
Reviewed processor/editor source, both test headers and immutable original control
records are byte-identical to their independent reviewed versions. Both reviewed
heads remain ancestors; there was no force push or dropped contributor commit.
Actual combined-main preflight:945 DSP/12,234 state/18 Python. Actual-wrapper
UBSan passes208; actual-editor ASan passes Undo/Redo/A-B. Re-running the original
wrapper yields33 failures/four UBSan categories; original and captured-reference
editor variants each reproduce all three UAFs with the combined driver.

The initial discovery/inventory records name `ab799880` because they ran on the
validated integration candidate before the merge. Its complete tree equals
`29150f3`; the new State155 original controls separately compile the editor taken
directly from `29150f3`. Preserve those actual identities instead of relabeling
earlier executions as post-merge tests.

The synchronized head's five workflows completed; main's separate CI ledger is
in the roadmap. Memcheck artifact13159780263 independently verifies completed
DSP/state, exit0/zero errors; existing leak checking is disabled. Native DSP945
differs from memcheck934 due to the existing allocation/FTZ adaptations, not
removed tests. The #208-main first build lost two unacquired macOS jobs, with no
runner/steps and explicit capacity annotations; its failed-job retry completed.
That is an infrastructure event, not a product test failure or a waived gate.

## Entry-point inventory and dispositions

| Entry / ownership / callout | Evidence and disposition |
|---|---|
| Undo/Redo component-owned closures; A/B control handler | NR-024 already integrated. Actual original/fixed/reference controls retained; do not reopen its correction. Stack-owned weak handles matter when component destruction also destroys the executing closure. |
| Preset-name component-owned click → `showPresetMenu` → pending adoption | **NR-025 confirmed.** Synchronous latency listener deletes the editor; method resumes reading `processor` through freed `this`. A live no-display popup probe initially hit JUCE's missing-display path; the same binary under Xvfb completes, isolating that fixture limitation. |
| Actual editor timer override → `pollUndoCoalesceFromTimer` → pending adoption | **NR-025 confirmed.** Same real latency callback, then freed Undo/processor fields. Live timer control completes and refreshes the button. |
| Copy A/B tail call | Actual owned callback, close/live controls both complete restore/adoption; close listener fires once and dispatch returns balanced. No post-command GUI access in the inspected path; preserve. |
| Previous/Next preset → processor-owned independent completion capturing weak editor | Both actual owned callbacks pass close/live probes; selection reaches Gentle Width/Super Wide, latency commits, editor closes once, dispatch clears. No NR-025 post-call access; preserve this interleaving. |
| Apply Gain tail call | Correct component ID `apply` gives close/live exit0 with completed adoption. An initial fixture searched for text `Apply` instead of actual `Apply Gain`; exit3 meant target not found, not a product failure. It does not establish teardown during every possible gain-write phase. |
| Save Preset / deferred completion | Source review: outgoing callback tails into save; independent completion carries weak editor and attempt identity. Existing State117/attempt controls retained. No new teardown witness here; source review alone does not prove every nested notification safe. |
| Popup selection and file chooser completion | Existing weak editor checks and parented popup ownership inspected. The invocation path before popup creation was the new NR-025 boundary. Native host/window lifetime observations remain deferred. |
| Attached Width knob wheel → JUCE synchronous write | **NR-026 confirmed.** Editor-only instrumentation misses dependency reads. Rebuilding all three actual `juce_gui_basics` TUs under ASan exposes a `ScopedDragNotification` destructor dereferencing the deleted Slider. Live control exits0. |
| Other knob drag/reset/key, ComboBox/Toggle attachment notifications | Source reaches synchronous callouts; no blanket safety claim. Expand teardown probes only with actual event and ownership premises. Wheel evidence is not proof that all these paths fail identically. |
| Internal Settings/oversampling values, Advanced/DPI/scale, resize callouts | Identified as separate candidates: `setSize`/parent callbacks and `Value` listeners can call out. Existing F12 protections retained. Further deterministic probes needed; no speculative broad guard refactor. |
| Timers, VBlank, listeners/destruction | Existing registrations/destruction sequencing inspected; the proven timer failure is specifically post-adoption. This fix does not certify every host resize, animation or listener callback. |

## NR-025 / KI-034 — restore adoption outlives the editor

**Priority P1 / confidence Verified.** Controlled message-thread latency callback
closes the actual editor while its processor remains alive. The author sets
oversampling combo ID2 (2x, engine index1), prepares48000/128 and serializes a real
state. The recipient starts Off, creates its editor, and accepts that state from
a joined host thread without pumping GUI events. Latency remains0 until the
production menu/timer action adopts it and delivers the synchronous notification.
Original ASan reports post-adoption UAF in both actions. This does not establish
named-DAW incidence or the cause of historical validator signal exits.

**Correction in [PR #209](https://github.com/skyRolly/Anamorph/pull/209), integrated at `0d395366`:** method-local `Component::SafePointer` on the stack,
checked immediately after each processor callout. The preset click additionally
captures weak ownership and copies it onto the stack before invoking the method;
a stale copied callback cannot adopt another pending restore after editor death.
The processor still completes restoration and latency publication. No scheduling,
new thread, atomic protocol, parameter/state format, reported value or DSP change.

**Alternatives:** postponing all latency notifications changes Accepted state/
latency semantics; cancelling restoration when a GUI dies loses processor-owned
work. Keeping editors alive changes host ownership. Raw pointers or borrowed
closure state do not meet the demonstrated lifetime boundary. Local weak guards
address only the invalid accesses while preserving those existing contracts.

**Regression:** State155 uses actual component-owned invocation and the actual
private timer override via the C++ explicit-instantiation access exemption. No
shipping test seam, private macro, class-definition mismatch or sleeping timer.
Two cycles per case check complete restore/latency, real destruction, cleared
dispatch, fresh attached Width readback and stale callbacks. Positive timer
continuation checks an actual enabled→disabled Undo indicator. Explicit positive
popup continuation requires a display and opens the real menu; Linux CI uses
existing Xvfb, and absence of a display is a failing prerequisite, not a pass.
Original and independently removed guards must fail under ASan; suppressing all
post-call work must fail the survivor controls. These focused TU controls do not
replace the unchanged full sanitizer jobs.

## NR-026 / KI-035 — pinned JUCE wheel teardown review packet

**Priority P1 / confidence Verified for this source interleaving.** Pin:
JUCE9.0.4 / `3fb5cbc0340758596dac39f7c8de47d562b4896e`. Actual Width wheel event
begins a gesture and changes the parameter synchronously. The registered real
parameter listener deletes the editor. JUCE's `Slider::ScopedDragNotification`
holds a `Slider&`; its destructor then reads `sliderBeingDragged.pimpl` in freed
storage. Actual GUI-module ASan reports heap-use-after-free; the matching live
control exits0. Changing only editor instrumentation had exited0, a demonstrated
instrumentation blind spot, not negative evidence of safety. No linked headers
or dependency class definitions were patched for this witness.

The partially instrumented run also observed begin1/end0. Because that path has
undefined behavior, treat this as a cleanup risk to evaluate in a consistent
prototype, not an independently well-defined proof of a second defect. A future
fix must show balanced gestures, valid processor Undo/dispatch and normal wheel
behavior as well as absence of the reported UAF. A weak pointer around only the
product's return from `Slider::mouseWheelMove` cannot repair a read inside JUCE.

**Disposition: stop gated production implementation; request human Architecture
Review of the dependency strategy.** [Architecture Review Gate](../docs/policies/ARCHITECTURE_REVIEW_GATE.md)
includes “Build System change — CMake structure, JUCE version/pin, dependency set”
and calls a gated change an AI Agent Hard Stop. [Dependency Policy](../docs/policies/DEPENDENCY_POLICY.md)
requires an ADR and Architecture Review for a JUCE bump. No dependency bytes,
pin, CMake structure or Accepted protocol are changed by the NR-025 correction.

| Option | Evaluation / required evidence |
|---|---|
| Reviewed bounded JUCE patch or upstream version | Investigate a complete notification/gesture-lifetime correction, then propose exact source/pin/patch identity for human review before shipping. Must protect begin/end callbacks and callers, not merely null-check one destructor. Rebuild affected TUs consistently; no ODR-mismatched prototype. |
| Product-owned gesture/event redesign | Potentially avoids the unsafe JUCE scope, but duplicates framework event semantics and must preserve Accepted ADR-0053 press/wheel grouping, modifiers, attachments and Undo. Larger surface; do not select without a supported design and required review. |
| Preserve current dependency pending decision | Accurately retains a confirmed conditional UAF and release risk; no claim of safety from passing existing CI or unavailable native incidence. It is a hold, not a fix. |

**Recommendation:** investigate a bounded, consistently built dependency-level
prototype and gesture cleanup before choosing a shipping patch/version. Do not
implement an incomplete pointer-only destructor change or defer arbitrary host
destruction to hide the witness. A final ADR, human approval and ordinary
compatibility/sanitizer/pluginval gates are required for the selected gated change.
No upstream message, dependency change or manual audition is authorized here.

## Evidence and remaining work

Machine-readable records and exact command/source/log hashes are retained in
`engineering-review/gui-restore-lifetime.json`; scratch raw evidence remains under
`/workspace/scratch/anamorph-integration207208/`. The ordinary default State155
cases, explicit displayed control, original/fixed/mutations and combined-main
records are separate scopes. Historical GCC ABI failures, unattributed validator
signal exits, native Intel AU risk and GitHub alert-state403 stay unresolved in
their original classifications. CodeQL workflow success does not clear alerts.

Next: revalidate NR-026 against the integrated dependency and investigate remaining actual
Settings/resize and attachment teardown entry points with consistent instrumentation.
Any NR-026 production dependency decision remains human-gated. Native performance,
host/editor visual and assistive acceptance remain deferred without waiver.
Current main and a reviewed branch are not the protected0.10.0 draft artifacts.
