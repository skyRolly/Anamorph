# Build 1957 — native Intel AU teardown investigation

Recorded 2026-10-30, Asia/Shanghai. Owner request: investigate and fix the
macos-intel failure, without weakening the existing validation gate.

## Observed failure

Build 1957 / run 41005362987, source `14237044cb51a80db5656461543c92a066c97ac0`,
job 123898933769, fails in AU randomise pass 2/3 at strictness 10. Seed `0x3e1c70`.
All tests complete and `SUCCESS` is printed, followed by `std::__1::bad_function_call`,
`pluginval received Abort trap: 6, exiting immediately`, and exit 9. The shell gate
correctly classifies a crash and fails immediately; macOS has no crash retry.
Native DSP/state tests and the other three pluginval format/mode groups completed.

The later main Build 1959 / run 41005877545, source `a428f6a`, completes the
same native Intel job (123900549180). Production source is unchanged between
these documentation commits. This establishes intermittency, not crash ownership.
The earlier PR #141 signature is similar, but neither observed tail supplies a
throw stack. Historical wording that assigned it to pluginval itself was stronger
than the retained evidence. No product fix or host-side exemption is established yet.

## Bounded observation

The initial investigation-only workflow `au-teardown-diagnostic.yml` ran on this single
investigation branch, on macos-15-intel. It reconstructed the original source with
build number 1957 and the normal thin AU configuration. These are reconstructed
bytes, not recovered original bytes; the workflow retains their hash and host/toolchain.
The validator remains the official content-locked v1.0.4 executable. No release,
framework/toolchain pin, conformance strictness, normal test invocation or retry changes.

`diagnose-au-teardown.py` runs its inert outcome controls, then proves the native
debugger can catch a deliberately empty std::function invocation before trying
the real AU. It records SIGABRT before pluginval's handler exits, all thread
stacks and loaded images. A bounded campaign uses the observed data seed with
randomised test ordering. A missing debugger result, launch error, unexpected stop,
timeout or no reproduction is not reported as a successful fix. Capturing the
expected crash is a diagnostic outcome, not a passing pluginval validation.

The original full Build matrix remains active and unchanged. The existing 0.10.0
tag and unpublished draft retain their verified identities; this investigation
does not replace or publish the release. Root-cause and regression records follow
the actual native evidence; no speculative production change is made.

### Observer liveness correction

First diagnostic run 41010691687 reconstructs the thin AU and the native canary
does stop on bad_function_call/SIGABRT. LLDB batch mode then skips ordinary `-o`
commands after a crash, so no result marker/full stack was emitted; the liveness
assertion correctly rejected the observer before any product campaign.
The driver now uses LLDB's `-k` crash hooks as well as normal-exit commands.
The next run retrieves retained artifact 12957996229 (API/archive SHA-256
`94b8543ef4af3a66fe77f24edb8e96fa623fb3481eb56ca8bae76bca19d816d6`),
and additionally verifies the AU binary's independently measured SHA-256
`4f574ba6542c07b79c9d772ceb951ffbab54b81bfdcfe4ad3c83992ee7cc258a`.
It reuses those exact reconstructed bytes instead of silently rebuilding them.

### Reproduced abort; first-throw capture required

Run 41011609262, artifact 12958500629 (SHA-256
`eaf801f71f81d38090243a3f3c7c9a8505772a69a08ea44a76bd3b2b515db66c`),
passes the native canary. Five real invocations exit successfully, then the sixth
reproduces the post-SUCCESS abort. The message-thread stack is already inside
AppKit's exception rethrow; the original C++ throw frame has unwound. The validator
worker is still stopping its timeout thread. This still does not establish blame.
The next observation filters __cxa_throw's x86_64 type_info argument for
bad_function_call and stops before unwinding. Its actual canary must prove that
specific first-throw path, rather than accepting a later abort as equivalent.

### PR #189 review corrections

The coverage audit now explicitly records the first-throw follow-up and its native
canary without claiming a root cause. The original artifact contains configure.log
and build.log at its root; after download they are retained/configure.log and
retained/build.log. Uploading retained/ preserves both, so obsolete top-level
upload entries are removed. New reports name diagnostic_commit and au_source_commit
separately and measure the installed AU binary hash. Earlier source_commit fields
refer to the diagnostic checkout, not the AU's historical source; those retained
reports remain unmodified and their producer/source distinction is recorded here.

### First-throw location established

Run 41018800486, artifact 12961026174 (SHA-256
`384da447ba1c93b85821080f2f2a089e7babffbeef42c4761fab99e1d7e7d5ad`),
records diagnostic commit 039361e separately from AU source 1423704 and the
unchanged AU/tool hashes. The typed canary succeeds; three complete invocations
precede the fourth invocation's captured first throw. Native message-thread frames:

```text
libc++abi.dylib: __cxa_throw
AudioToolboxCore: std::__1::__throw_bad_function_call
AudioToolboxCore: invocation function for block in CAEventReceiver::Impl::timerFired()
CoreFoundation: __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__
```

This identifies an empty callback invocation in Apple's AU event-delivery machinery
on the macOS 15 runner, before AppKit unwinds it. The full stack has no Anamorph
callback frame at the throw site. The validator worker is stopping its timeout
thread after SUCCESS. A same-binary/tool comparison on macos-26-intel and an
Apple-only listener lifecycle control are next; the shipping gates are not changed
on the strength of a later passing run alone.

### macOS 26 comparison does not fix the abort

Run 41020131883, artifact 12961621235 (SHA-256
`d0ca26f88aabc3be9c4fc125e46a5e945766de54eb4f31757f00136bab9e23da`),
uses the identical retained AU and validator. On macOS 26.6.1 / Xcode 26.6,
the first invocation reaches the same first throw in CAEventReceiver's timer block.
The Apple-only control independently completes 2000 cycles with 13744 callbacks.
It does not reproduce the real host/AU interaction. A runner-label move is therefore
not a supported fix and the ordinary macos-intel gate remains unchanged on macOS 15.
CI_CD and TESTING now document the control invocation and its exit/liveness meanings.

### Queued disposal control; conformance policy preserved

The drained Apple-only control does not exercise the precise pending-delivery
ordering at the observed throw. A separate `--queued-disposal` mode schedules
listener/component disposal in a timer and blocks the main thread for 25 ms,
making delivery and disposal due in the same run-loop turn. The first ordinary
cycle must still deliver callbacks; the disposal timer must run (otherwise exit 4).
The diagnostic observer now captures both independent modes before the real AU
campaign, retaining their exit states or first-throw stacks. Native results are
pending; this is not yet a reproduction or a causal conclusion.

A typed, same-seed retry was considered and rejected before integration: a system
frame alone does not exclude a host/product lifetime defect, and the independent
control has not reproduced it. No conformance exemption, wrapper retry, testing
policy change or product patch is introduced. RISK-017 tracks the unresolved cause.
The diagnostic workflow now filters relevant paths so documentation-only corrections
do not repeat an unchanged experimental campaign.

Run 41025295282, artifact 12963476507 (SHA-256
`093b9ff53344305d7d883c44622fcce07fbff017b3277620da210c1c29847521`),
passes the actual throw canary but times out the drained control after 180 seconds,
before the queued probe or real AU campaign. Its log proves entry, not progress
or a product deadlock. The independent campaign is now explicitly bounded to
128 cycles per mode under LLDB, with progress flushed every 16 cycles; standalone
control invocations still default to 2000. An observer error or unexercised path
continues to fail diagnosis.

The timer probe may run after CoreFoundation has already drained queued blocks.
A separate `--entry-disposal` variant disposes from a run-loop entry observer,
before that drain, preserving the same initial callback proof and mandatory
disposal check. The observer runs all three modes; the follow-up AU campaign is
bounded to four trials. Neither a missing reproduction nor a clean ordinary
build is called a crash fix.

For completeness, unchanged macOS 26 diagnostic run 41020945093 on bb4f751
completes all 16 AU trials and the drained control (13744 callbacks). Artifact
12962182315, SHA-256
`e6bbe09fae782c217ef0f403e354d619e64bb377d01d426daf29ca8a490f395a`,
confirms the same AU/tool hashes and separate source identities. This strengthens
the intermittency observation; it does not negate the earlier first-throw capture.

External corroboration, not causal proof: [Minihost's pump source](https://github.com/shakfu/minihost/blob/9cc002e8e60aef02d775af24e36c8ac31557bdab/projects/libminihost/minihost_pump_mac.cpp)
records the same framework exception while pumping the default run-loop mode.
Its private-mode workaround excludes framework callbacks and cannot be transplanted
as proof of complete AU conformance here. No such pump suppression is introduced.

### Native control verification; original failure remains open

Run 41025971859 on c173ea7, artifact 12963889677 (SHA-256
`fdf5ba9d8ca3c9e3959d8cf820057e1e3e1e4ffcc72952e548327c17990d6066`),
completes the 128-cycle drained and timer-disposal controls with 880 and 1024
callbacks, respectively. Both inferiors and debuggers exit 0. Fifteen real AU
trials then complete; trial 16 captures the same typed Apple CAEventReceiver
timer-block throw after SUCCESS. The comparison correctly fails its completed
expectation. A later passing run cannot erase this reproduction.

Run 41027283750 on 1db9f3f, artifact 12963559427 (SHA-256
`883a477fe6f0f3ebc0f0085f6fe54dec949ddaffc92b36a1e13f305e12e0258d`),
completes 128 cycles per control: entry 8 callbacks, drained 872, timer 1024;
inferiors and debuggers exit 0. All four AU trials complete. The eight entry-mode
callbacks are from the initial proof cycle; later disposals deliver none. This
does not prove that an AU delivery block was queued at disposal, so the intended
stale-block mechanism is still unproven. Both artifacts were downloaded and their
API archive digests independently matched; both reports retain AU source 1423704,
their distinct diagnostic commit, AU hash 4f574b… and locked validator hash 32ec60….

Local verification: all 15 observer/outcome controls; both always-clean and
never-clean independent-classifier mutations are rejected by those controls;
documentation self-tests/lint, citation self-tests/check and release-note
self-tests/check. The unchanged ordinary crash classifier's 22 controls were
also exercised. These verify diagnostic interpretation and documentation, not
a product crash fix. CI_CD, TESTING and the coverage audit describe the exact
native modes, bounds, liveness and exit meanings required by the PR #189 review.

**Disposition:** the documentation review is addressed. RISK-017 remains open;
no safe product correction is established by these observations. PR #189 stays
a diagnostic draft, with the original conformance gate and unpublished release
unchanged. Further causal isolation must establish pending delivery at disposal
inside the real host lifecycle before selecting a correction.

### Reopened: observe enqueue before disposal

The owner's instruction to solve PR #189 continues the investigation from the
unproven ordering, not from a clean-run assumption. Main's completed P1/release
documentation is merged into this branch; both coverage records are preserved.

`au_pending_block_control.mm` is an independent Apple-only process that intercepts
CFRunLoopPerformBlock from AudioToolbox, queues listener disposal before the
original block, and proves both queue operations completed before disposal.
The Apple AU itself remains alive until after the block returns. This isolates
listener lifetime from product/JUCE destruction. A bounded rendezvous covers an
off-main enqueue; no application thread is created. The disarmed fixture must
exit exactly 5 before the armed observation, so an unobserved path cannot pass.
The first-throw canary and recorded source/binary identities remain required.
Native verification is pending. The real AU follow-up is bounded to one trial
while this mechanism is isolated; no ordinary conformance invocation is changed.

Run 41083898251, artifact 12990198720 (SHA-256
`dcb43d9998e37aaad0482a6148b68cf13e9b213c8103b71fe1a8effc085708ba`),
passes the native typed-throw canary and exact-exit-5 disarmed check. The armed
fixture also exits 5: callbacks are live, but no framework enqueue was intercepted.
This is an unexercised path, not a reproduction or a passing fix. The fixture now
uses AudioUnitSetParameter plus direct AUEventListenerNotify, matching the AU
wrapper, instead of legacy AUParameterSet notifications. A bounded debugger trace
records CFRunLoopPerformBlock callers to distinguish a missing API path from an
interposition/filter failure. Native results for this change are pending.

Run 41084813516, artifact 12991670485 (SHA-256
`d092705b3842481ba71c9b9a0b1be1fb3d21673cbca61c8b053e63e0fb7b8066`),
also exits 5. Its debugger now confirms CFRunLoopPerformBlock calls from
AudioToolboxCore's CAEventReceiver::Impl::createTimer block, while no replacement
function entry is observed. The event path exists; the executable interposer is
ineffective. The next probe places the interposer/control implementation in a
separate linked library and sets DYLD_INSERT_LIBRARIES in the inferior through
LLDB, avoiding protected-launcher environment stripping. The library's measured
hash is added to the report. None of this library is loaded into the actual validator.
