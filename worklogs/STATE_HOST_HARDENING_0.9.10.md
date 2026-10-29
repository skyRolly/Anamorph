# State and host interaction hardening — 0.9.10 development

**Later P1 decision round (2026-10-30):** current-code reproductions and Proposed ADR-0067/0068
are recorded in `RELEASE_READINESS_0.10.0.md`. Neither proposal is approved or implemented;
KI-029 and F9 remain open. The measurements, rejected guard and accepted completed fixes below
remain historical evidence; this later pointer does not change their outcomes.

## Baseline and authority

- Started from latest `origin/main`, `c4f390f9cccaa9551d680cb1305fa4a669b24587`, after fetching
  `main` and tags. Branch: `codex/0.9.10-state-host-hardening`.
- The annotated `0.9.9` tag peels to that commit. Tag object
  `12d6fabcd121f62cb9eeaf32ae94bc8d32447984` records 2026-10-21 13:52:47 UTC.
- Initial implementation commit: `104d680fd4d36a93792f69f388cc07ea3f31e9f5` (formatter, release guard, tests
  and development version). Documentation/validation follow on the same feature branch.
- Production baseline is current main, including PRs #155, #156, #166 and #167. Their DSP
  self-heal, Level Match, bulk-apply protocol, JUCE upgrade and owner approvals are not reopened.
- JUCE: `3fb5cbc0340758596dac39f7c8de47d562b4896e` (9.0.4). The inherited build cache used
  9.0.2; both cached pin fields were reset through the documented CMake workflow before testing.
- Read `AGENTS.md`, `SOURCE_OF_TRUTH`, the AI/architecture-review, compatibility family,
  real-time, threading, DSP, testing, ADR, release and documentation-lifecycle policies.
  Keep a Changelog 1.1.0 was fetched and read before editing CHANGELOG.
- No new parameter, field, thread, atomic ordering, DSP stage, latency rule or dependency is
  authorized by this work. In particular, ADR-0056 is not approval for another acceptance restriction.

## KI-029: separate the formatter defect from the parameter policy

The production trace is `getStateInformation` → `writeState` → each slot's `toXmlString` and the
root's `createXml` → `var::toString` → `juce::serialiseDouble`. At the pinned JUCE revision,
`serialiseDouble` still converts `input` to `int` before checking NaN. Its finite magnitude guards
also handle infinities; NaN fails both comparisons and reaches the undefined conversion.
Preset saving reaches the same formatter through `writeUserPreset` → `savedSound.createXml`.
Infinities take JUCE's earlier scientific-formatting branch, so this specific undefined integer
conversion is a NaN defect. The repair leaves infinity formatting on the existing path.

Current ingress, measured through the real processor by `--nonfinite-ingress-probe`:

| Entry | Current result and compatibility boundary |
|---|---|
| Host writes | Ordinary continuous controls such as Width and Velvet Density retain NaN. Infinities clamp to endpoints. `RawBool` retains exact non-finite `getValue()` while its converted value/consumer may differ. |
| Parameter text | Width/Density parse `nan` to NaN. Mono Maker Freq maps NaN/+Inf to 500 Hz and −Inf to 20 Hz. The Boolean parser resolves these strings to Off. The editor and VST3 text entry use these parsers. |
| Session restore | Unusable `raw` and `value` resolve to the existing parameter default. A usable `raw` wins. With Width `value="nan" raw="0.71"`, one NaN notification occurs during `replaceState`, the final raw is approximately 0.71, and the next save retains textual `value="nan"` with a finite `raw`. |
| Preset loading | `applySoundTree` validates the serialized **input** before range conversion, reads `value`, and defaults invalid/missing numbers. A preset does not use the session's `raw` preference. |
| A/B | Slot capture retains live non-finite values. Applying a slot uses the shared resolver: usable finite `raw` wins, otherwise usable `value`, otherwise default. Saved NaN Density resolves to 0.5; NaN/±Inf text is unusable before range conversion. |
| Serialization | Numeric NaN reaches JUCE's undefined conversion in live parameters, cached slots and preset saves. A textual `nan` already read from XML is a String property and does not take that numeric formatter path. |

Existing State test 19 already exercises NaN/±Inf and overflow text through real session and
preset loaders; State test 28 covers malformed legacy Settings. They remain in the full suite.
The source predicate rejects both explicit non-finite values and float-narrowing overflow BEFORE
range conversion; host/text setters do not share that serialized-input rule.

The original latches are already fixed: Haas/Velvet Amount reset reseeding (PR #155), Velvet's
finite density target and Mono Maker's finite cutoff handling (PR #156). They remain guarded by
DSP tests 59/64/74 and State tests 123/128/138. No DSP or parameter ingress rule is changed here.
Transient mutes, Mix's behavior, Level Match's reset and the Haas/Chorus float-to-int sites remain
the existing KI-029 owner question, distinct from the XML formatter.

### Admissible serialization correction

`SerializedXml.h` creates a private serialization copy and spells only numeric NaNs with JUCE's
floating-point String constructor. XML attributes are text in both cases. The existing NaN spelling
(including sign) and the existing restore/default rule are preserved; finite properties retain
JUCE's original formatting. The live APVTS, cached slots and the numeric tree used for preset
baseline comparison are not edited. All four XML writer surfaces use the same helper.

This is a formatter repair, not a new parameter or serialization semantic: no field, reader,
automation value, range, default or audible invalid-value rule changes. No ADR acceptance is
claimed. The old §S10 worklog remains historical; its assertion that every serialization repair
necessarily changes the emitted state is superseded by this narrower construction and its tests.

Regression: State test 142, `--nonfinite-save-probe`: finite → NaN → negative NaN → finite → NaN,
both host state threads, both cached slots, preset saving/loading and A/B return legs. It compares
finite XML byte-for-byte with JUCE's existing writer and checks the live value before/after a save.
The comparison uses the parameter's actual pre-save value because Density's finite interval snaps
some requested floats; an early draft comparing against the request was discarded.

### Owner decision still outstanding: audible/non-finite ingress behavior

| Alternative | Consequence / approval needed |
|---|---|
| Preserve | Keeps existing NaN mutes/transients and module-specific recovery. This round preserves it pending a ruling. |
| Stateless default in `toEngine` | Covers all engine readers without changing host readback, but changes the sound. Output Gain could become 0 dB instead of the preceding attenuated value. Parameter-semantics ADR and human review required. |
| Last finite value | Requires a specified initial value, re-prepare/reset lifetime and ownership across the existing readers. A shared cache would be a threading-model decision as well as a parameter decision. |
| Sanitize at the parameter setter | Changes what the host reads back, including ADR-0013's raw-value contract; requires compatibility review and an ADR. |
| Reject text/host writes | Must define what “reject” means for float-returning parsers and void setters, notifications and automation. Also gated. |
| Reject a save | `getStateInformation` has no failure return; an empty/unchanged chunk can lose the session. Unnecessary when the formatter can preserve the existing representation safely. Not selected. |

**Exact outstanding ruling:** choose the meaning of NaN and each infinity at host/text ingress and
at the DSP boundary, including fresh-instance/re-prepare behavior and the malformed `value` plus
usable `raw` restore window. Approve the corresponding ADR/compatibility case before implementing
any such change. Recommendation: take the independent serialization repair; defer audible ingress
changes until the owner specifies that contract.

## F12: advancedMode host writes

`RawBool(..., false)` remains in the APVTS. `isAutomatable()` only describes automation eligibility;
it does not block `setValueNotifyingHost`, wrapper parameter writes or restore.
`setupToggle` installs JUCE's `ButtonAttachment`. `ParameterAttachment::parameterValueChanged`
delivers synchronously on the message thread and posts an AsyncUpdater otherwise.
`ButtonParameterAttachment::setValue` synchronously updates the button on that thread, which can
run the editor's `onClick` → visibility update → `applyUiScale` → `setSize`. Its own callback guard
prevents the attachment from writing back recursively. Initial attachment update precedes the
editor's `onClick` installation; construction subsequently reads the button and lays out the mode.
Attachment destruction cancels its pending updater.

State test 145 / `--advanced-mode-probe` uses the real processor/editor and JUCE event queue:
closed-editor host write; background writes with an open editor; coalesced writes; a synchronous
write inside a resize callback; destruction with a queued update; recreation; and construction
parked between a background restore's APVTS install and metadata publication.
The initial current-code run completed 12 checks with no failure. No thread-affinity, stale-mode,
reentrant-resize or lifetime defect was established. **Preserve the implementation.**

Limits: the editor has no native host peer. This does not reproduce the host's XEmbed UAF or
prove arbitrary host window-grant behavior, macOS OpenGL or Windows GUI behavior. Reopen on a
supported-host trace showing a wrong-thread component operation, stale final mode or lifetime
failure. The historical non-automatable mitigation reduces validator-driven automation; it is not
host-write isolation. KI-007/PARAMETER_REGISTRY wording is qualified accordingly.

## RISK-015: owner decision package, acceptance preserved

### Verified mechanisms and affected classes

`hostChunkIsAdmissible` caps ORIGINAL chunk bytes first. `getXmlFromBinary` checks the magic and a
positive declared length, then parses `String::fromUTF8(data + 8, min(actual - 8, declared))`.
At the pinned native UTF-8 String configuration the sized constructor copies bytes. String/XML
walks stop at NUL; an allocated invisible tail is not scanned. UTF-8 validity is debug-asserted by
JUCE, not rejected by the release reader. Literal attribute segments can retain invalid native
bytes; later Unicode iteration/serialization can replace them with U+FFFD.
`decodeRestore` requires a recognized root with its sound child, then applies parameters and
metadata. Each A/B attribute is unescaped and parsed separately; an inner refusal leaves that slot
invalid for reseeding without rejecting the outer restore. Preset files have their own byte
validator and `oneWellFormedDocument` plus format validation (ADR-0055).

| Class / concrete fixture | Outer host session | Separately parsed A/B payload | Why two enum changes are insufficient |
|---|---|---|---|
| Two complete documents | First recognized document applies | First `ANAMORPH` applies; second ignored | Strict document scanning would newly reject these visible tails, which ADR-0056 explicitly preserved. |
| Prose, another element, binary after a complete root | First document applies | First payload applies | A complete-prefix parser does not examine the tail. |
| NUL then another document | First visible prefix applies | Numeric `&#0;` inside the encoded attribute makes an inner NUL; its visible prefix applies | A String-only scan sees neither invisible tail. Original-byte and decoded-character rules need separate decisions. |
| Invalid UTF-8 inside an attribute | Small invalid attribute is accepted; sound applies | Small invalid inner attribute is accepted; slot applies | `oneWellFormedDocument` is not an original-byte UTF-8 validator; native byte storage and Unicode byte counts differ. |
| Half chunk / parseable truncated document | The historical half-chunk prefix applies; absent fields use existing defaults/reseeding | A partial closing tag after a complete PARAM is parseable and applies; a missing closing tag or open attribute quote can instead refuse | Declared length is clamped, not checked for exactness. “Truncated” is not a uniform parser result. |

State test 116 leg E guards visible multiple-document/trailing/truncated acceptance; it does not
cover all five classes. State test 144 / `--host-acceptance-probe` adds original-byte NUL and invalid
UTF-8 cases, separate inner witnesses, both inner/outer refusal controls, repeats on the same
processor, and declared lengths longer/shorter than the supplied byte range. A complete document
with an oversized declared length still applies; bytes after a shorter declared length remain
invisible even to a strict scan. No production acceptance argument is changed.

### Historical writer and fixture evidence

- Current and historical JUCE `copyXmlToBinary` writer framing is magic + XML byte length + XML +
  one terminal NUL. Current A/B writers emit one XML document per attribute. No writer evidence
  found here requires concatenated documents, prose/binary tails, embedded NUL or invalid UTF-8.
- The three committed legacy XML fixtures and `field_capture_0_9_5.session` remain the frozen
  historical evidence. Existing state-suite round trips, legacy migrations and preset tests run
  against the current code. The 9.0.3 capture recorded by the 9.0.4 upgrade is a **committed
  measurement record**, not an additional committed fixture; do not count it as bytes re-tested here.
- These fixtures cannot prove that no historical host padded a chunk, or that nobody relies on
  recovery of a parseable prefix. Absence of a malformed writer fixture is not authorization to
  change the accepted set. ADR-0056 explicitly kept it; rule 1 and the Architecture Review Gate apply.

### Four available outcomes

| Outcome | Compatibility assessment |
|---|---|
| A. Preserve acceptance | Lowest compatibility risk; preserves partial recovery and silent truncation. An owner can explicitly accept this residual. |
| B. Harden all five | Requires original-byte UTF-8/NUL/framing rules, complete-document checks on both surfaces, and decisions on numeric NUL references and inner-failure scope. Rejects formerly restoring chunks; no prior approval covers it. |
| C. Narrow/modify | Could reject only visible extra documents/tails, or stage byte/framing changes separately. Must state which malformed classes still restore and update tests deliberately. Two enum arguments cover only part of this option. |
| D. Defer | Preserve the current reader while obtaining a specified acceptance decision and any required host captures. This is this round's disposition. |

**Exact outstanding policy decision:** must host chunks be exactly the writer's framing (including
terminal NUL/padding treatment), contain valid original UTF-8, forbid embedded NUL/invisible tails
and describe one complete XML document? Must each decoded A/B payload also be one complete document,
and should a refused payload continue to invalidate only its slot? An owner must explicitly choose
the accepted/rejected forms, migration/recovery rationale and release compatibility requirements,
then approve a Proposed ADR through the Architecture Review Gate. ADR-0056's approval does not do so.

**Recommendation:** D now; preserve acceptance. If the owner elects hardening, use a byte-aware C
proposal with separate framing/encoding/document decisions and retain current inner-failure scope
unless explicitly changed. Request real historical host captures if padding/length tolerance is to
be removed. No safety defect in these bounded acceptance cases establishes urgency for an implicit
compatibility change.

## Size and payload limits

Retain 256 KB / depth 8 / no DOCTYPE. These are accepted ADR-0056 limits, not numbers selected by
this round. The committed 9.0.4 audit records a 12,157-byte session at depth 3 and slots of
2,632 / 2,631 bytes at depth 2; legacy measurements are 268 / 590 / 740 bytes at depth 2–3.
This is empirical coverage, not a proof of a maximum writer output: metadata and retained unknown
properties are variable-length. No new legitimate known-writer/fixture measurement invalidates the
accepted caps. Accepted malformed text can normalize/expand on a later write, so malformed-input
byte identity or a universal writer upper bound is not claimed; that is a residual, not permission
to increase the limits.

**The old unconditional containment claim is false in the current configuration.** A focused
JUCE 9.0.4 mechanism probe measured 90,168 ORIGINAL outer bytes, 90,061 stored inner bytes and
270,061 bytes reported by the inner String's `getNumBytesAsUTF8()`. An invalid native byte is
counted as U+FFFD's three UTF-8 bytes during Unicode iteration. Thus an under-cap outer chunk can
reach the existing inner size refusal independently. The real processor fixture measures a
90,290-byte outer chunk with the same 90,061 / 270,061 inner sizes. State test 144 carries this shape through
the real processor, with an outer Drive witness and a distinct inner Width witness. No limit is
increased and no impossible oversized-inner-under-valid-outer fixture is invented.

Containment still holds for **valid UTF-8**: unescaping shrinks; outer DOCTYPE/entity expansion is
refused. Keep the inner guard because the existing host reader also accepts invalid UTF-8. This is
a correction to the proof/coverage, not a change to acceptance or a new limit. Reopen the limits on
a real historical/current writer or supported-host chunk exceeding them, or a change to decoding,
entity expansion or payload framing that invalidates the remaining conditional argument.

## Focused remaining risks

- **F9 — preserve as a measured residual:** `--restore-lock-probe` publishes a restore between
  the production gate's final drain and lock acquisition, then reaches the preset save's actual
  `onAboutToSave` adoption. A separate thread's non-blocking try in `afterRestoreTake` proves an
  OUTER command still owns `soundReplacement` after adoption's own scoped lock ends. This establishes
  the edge, not a deadlock: the probe takes no blocking opposing lock, and no supported-host trace
  establishes that other half. The general “all drains outside the lock” claim is too strong.

  `--restore-reentry-probe` establishes a related **synthetic reentry correctness failure**. A real
  `AudioProcessorListener` latency callback starts and joins a newer background restore, then pumps
  the real timer door. The host state calls themselves are serialized. Newer Width 0.6 remains live,
  but the older tail resumes after the callback and stamps preset name `older` over it. Without
  pumping, Width 1.6 and name `older` agree. The opt-in diagnostic intentionally fails the final
  metadata assertions; it is not in the default regression suite.

  A candidate `if (adoptingRestore) return;` before the adoption drain was built and rejected. It
  gives the correct final name, but `getStateInformation` INSIDE that callback serializes name
  `Default` with live Width 0.6. The guard weakens ADR-0036 §15's fixed-point save contract and is
  **not shipped**. The diagnostic includes that callback-save assertion so either failure is visible.
  No native supported host has been shown to perform this compound background-restore/pump inside
  the synchronous latency callback. Related reentrant state-call shapes are recorded as examined
  residuals in ADR-0036 §25 / ADR-0008; those records do not approve a fix for this new tail schedule. Do not label this a demonstrated supported-host hang or a closed F9.

  **Reopen/approval:** capture a supported host's opposing lock/pump stack at the outer-held edge,
  or have the owner explicitly require the synthetic nested-state-call shape. Specify whether such
  saves must synchronously reflect the latest restore. A solution must cover Settings writes,
  A/B/undo/preset metadata, inline (generation-zero) restores and saves inside callbacks. Possible
  alternatives are cancellation of superseded tails with a message-thread application token, or a
  coherent snapshot/save protocol; neither is implemented or approved here. Any new cross-thread
  path, ordering or serialization semantic requires its Proposed ADR and Architecture Review;
  the rejected guard is not a substitute. Preserve the existing protocol while that host/support
  evidence and concrete design are decided.
- **F10:** State test 143 / `--nested-release-probe` reproduced eight failures in 42 checks on
  unchanged production code: both mouse-drag and in-press wheel startup, nested/non-nested cycles.
  A release from the first pin's `beginChangeGesture` clears `soloPressBand`, `soloMovedBand` and
  `gestureBands` before the second pin is opened. The resumed handler loses the identifiers needed
  to close either pin and can form an audition mask from −1. `mouseUp` now honors the existing
  `gestureActionDepth` claim before touching identifiers. That initial repair was incomplete:
  PR #168 review proved it discarded the only actual release; the 42-check fixture supplied a
  second release and cancellation. A real delivered release disables the cached-down reconcile.
  The correction retains the release until the outer event returns, then uses normal mouseUp
  processing (including stale cancellation). The review follow-up below records the stronger
  reproduction and final validation. No new threading or parameter model is chosen.
- **RISK-010:** unchanged. The ten independent `seq_cst` loads, GUI count-last store order, DSP
  clamps and ADR-0057 bulk-completion protocol remain. No new writer, weakened repair, changed
  store order or measured whole-state-restore audio artifact was found in this scope. Those remain
  the concrete reopen conditions; no snapshot-system replacement is proposed.

## Initial validation and delivery record (superseded F10 fixture; review follow-up below)

Local x86-64 Linux only, using the actual pinned JUCE source. GCC 14.2 Release and Debian
Clang **22.1.8** RelWithDebInfo. The local LLVM package matches the CI major/version; its embedded
upstream git identity is not supplied by the Debian build, so this is not an assertion that it is
the exact CI toolchain artifact. Source trees, pin and fixture bytes were checked before building.
`build-san` uses the same extended sanitizer set as CI:

```sh
-fsanitize=address,undefined,vptr,float-divide-by-zero,implicit-conversion,unsigned-shift-base,local-bounds,nullability
-fsanitize-ignorelist=/workspace/Anamorph/scripts/ubsan-ignorelist.txt
-fno-omit-frame-pointer
```

The C flags omit `vptr`, as CI does. Both suites used a 64 MB stack for the sanitized run, with:

```sh
ASAN_OPTIONS=detect_leaks=1:check_initialization_order=1:strict_init_order=1:strict_string_checks=1:allow_addr2line=1:external_symbolizer_path=/usr/bin/addr2line
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
```

GUI/state commands use `xvfb-run -a`; every state process runs **serially** because the real preset
folder is shared. The Release native run uses `MALLOC_PERTURB_=1`.

| Executed check | Actual result |
|---|---|
| Original `--nonfinite-save-probe`, pinned JUCE 9.0.4, ASan/UBSan | Exit 1 at NaN→int in `serialiseDouble`; independently symbolized stack below. Negative control for test 142. |
| Original `--nested-release-probe` | 42 checks / **8 failures**, drag and wheel. Negative control for test 143. |
| Fixed `--nonfinite-save-probe`, ASan/UBSan | **108 / 0**, exit 0. |
| Fixed `--nested-release-probe`, ASan/UBSan | **42 / 0**, exit 0. |
| `--host-acceptance-probe`, ASan/UBSan | **68 / 0**, exit 0; outer 90,290 B / inner stored 90,061 B / inner counted 270,061 B. |
| `--advanced-mode-probe`, original and final code | **12 / 0** each; final run under ASan/UBSan, exit 0. |
| Full DSP suite, Clang 22 ASan/UBSan | **940 / 0**, exit 0; expected warning: ASan owns malloc, so the malloc half of the allocation guard is disabled. Operator-new guard remains active. |
| Full state suite, Clang 22 ASan/UBSan | **5,830 / 0**, exit 0, leak detection enabled. Includes legacy migrations, registry snapshot, valid byte-exact round trips, preset and A/B compatibility plus 142–145. |
| Full DSP suite, GCC Release + heap perturbation | **944 / 0**, exit 0; native malloc-family guard active. |
| Full state suite, GCC Release + heap perturbation | **5,830 / 0**, exit 0. |
| Focused valgrind 3.24 memcheck, GCC Release | Test 142 **108 / 0**, test 143 **42 / 0**, each exit 0 and **0 errors / 0 contexts**, no suppressions. `--track-origins=yes --leak-check=no --error-exitcode=1`; leak coverage is the separate ASan run above. |
| Inner-size negative control | A separate processor object removes ONLY `slotPayloadIsAdmissible`'s size comparison, linked with the same sanitized test/other production objects. **68 checks / 1 expected failure**: Width 1.6 applies instead of reseeded 1.0. Original source and normal objects untouched. |
| F9 lock diagnostic | Outer-held `soundReplacement` edge witnessed by another thread's non-blocking try; no blocking cycle claimed. |
| F9 `--restore-reentry-probe`, ASan/UBSan | **30 checks / 4 failures**, exit 1, all final preset-name checks on the two nested legs. Callback-save checks pass on existing code. This unresolved diagnostic is opt-in, excluded from the default suite. |
| Rejected F9 guard, separate real-processor executable | Inside callback: saved preset `Default`, live Width 0.6; after callback: preset `newer`, Width 0.6. Candidate removed; no production adoption change. |
| Static source checks and their self-tests | Portability 120 cases + 63 files; real-time 97 + 51; dispatch 52 + 51; state coverage 110 + all 36 EngineParameters fields / 12 modules. All exit 0. |
| Documentation checks | 700-case self-test; 160 files clean; citations self-test and 613 unchanged anchors versus `origin/main`, exit 0. Existing unnecessary DELIBERATE_REAIMS notes preserved, no general audit. |
| Warning/ABI checker self-tests | Clang 28 cases, GCC 17, Linux ABI 19, exit 0. Actual linked shipping-artifact ABI check is **not** inferred from the self-test. |
| Clang warning classification | No new first-party warnings; accepted baseline sites remain. Local ALSA signedness / X11 comment-encoding warnings are external-header diagnostics, not suppressed source defects. |
| GCC warning classification | **Not run against its required compiler:** checker exits 2 because local GCC 14 does not match the repository's GCC 16 baseline. No baseline change; GCC 16 CI remains required. |

The original sanitizer stack (no dependence on an audible assertion) is:

```text
serialiseDouble (juce_String.cpp:2281): nan is outside the range of representable values of type int
  → var::doubleToString → NamedValueSet::copyToXmlAttributes
  → ValueTree::createXml / toXmlString
  → AnamorphAudioProcessor::writeState (slotAParams)
  → getStateInformation → State test 142
```

Reproduce the targeted tests with the built executable (regular default run has no arguments):

```sh
xvfb-run -a build-san/AnamorphStateTests_artefacts/RelWithDebInfo/AnamorphStateTests --nonfinite-save-probe
xvfb-run -a build-san/AnamorphStateTests_artefacts/RelWithDebInfo/AnamorphStateTests --nested-release-probe
xvfb-run -a build-san/AnamorphStateTests_artefacts/RelWithDebInfo/AnamorphStateTests --host-acceptance-probe
xvfb-run -a build-san/AnamorphStateTests_artefacts/RelWithDebInfo/AnamorphStateTests --advanced-mode-probe
# Expected unresolved failure, not a passing-suite command:
xvfb-run -a build-san/AnamorphStateTests_artefacts/RelWithDebInfo/AnamorphStateTests --restore-reentry-probe
```

Sanitizer negative control: restore the four XML writer calls from starting `c4f390f`, rebuild the
state target with those flags, run `--nonfinite-save-probe` with UBSan halt enabled. Initial F10 negative
control: remove only the new `gestureActionDepth` return at `mouseUp`, rebuild, run
`--nested-release-probe`. The controls were actually executed on the original production calls;
those instructions make them repeatable without depending on a historical binary.

**Execution qualifications:** the first DSP sanitizer attempt reached its 180-second timeout
(exit 124, no sanitizer report); the complete rerun used 1,200 seconds and exited 0. The container
has a two-CPU quota, and builds/runtime competed for it. An early overlapping Ninja invocation
interrupted two cached object builds and caused a missing-object link failure; rebuilding the
normal target resolved it. This was an execution error, not a product failure. Early test drafts
had incorrect finite-snap and XML-fixture expectations; those were corrected against measured
production behavior before the results above. No acceptance change was made to satisfy a fixture.
The first two valgrind attempts could not start because the container lacked loader debug symbols.
Downloaded the exact installed `libc6-dbg 2.41-12+deb13u4`, verified the loader build ID, and exposed
its debuglink file through `--extra-debuginfo-path=/workspace/scratch/memcheck-debuginfo`. The
`/bin/true` startup check and both focused runs then completed; the initial startup exits are not
counted as test executions.

**Not local verification:** no Windows/macOS/native Intel/ARM execution, real host window peer,
manual audition, pluginval against a newly built shipping artifact, CodeQL, MSVC analysis, full
valgrind suites, RTSan/TSan lanes or CI result is claimed here. Native host/UI and per-release sign-off
remain mandatory. The Linux Release test binaries are GCC-built local tools, not a replacement for
the shipping Clang artifact/ABI gate. No dependency/toolchain pin or CI workflow was modified.

## R8 drift check

Reported before correction: RISK-003's empty-tag claim contradicts the fetched annotated tag;
README/HANDOVER/COMMERCIAL_STATUS/RELEASE_HARDENING_PLAN still describe that tag as future work.
The risk table omitted RISK-012's resolved row and RISK-013's accepted-residual row despite their
detailed sections. SERIALIZATION_REGISTRY overstates declared-length rejection and unconditional
inner-size containment. STATE_SERIALIZATION's claim that every malformed `value` is repaired before
JUCE sees it omits the measured usable-`raw` case above. KI-007's automation-flag mitigation is not
host-write isolation. Current test summaries must account for the four new numbered tests; past
worklog counts, release measurements and dated baseline narratives remain historical.


## Delivery and release disposition

Initial implementation commit `104d680fd4d36a93792f69f388cc07ea3f31e9f5` has the exact tree tested in that delivery. The single-release review correction is recorded below.
The connected GitHub tool publishes the feature branch/PR because the shell's token is invalid and
CLI push has no usable credential. GitHub blob/tree SHAs are compared to local Git before moving
the feature ref. No main update, merge, 0.9.10 tag or release is performed.

**Completed:** NaN writer safety and F10 gesture ownership correction; four new default regressions;
focused F12, malformed-state and size investigations; current documentation/register corrections.
**Open decisions:** KI-029 audible/ingress semantics; RISK-015 any new byte/framing/document
restrictions; F9's supported-host/support-contract evidence and compatible reentry design. The
accepted snapshot architecture (RISK-010), advancedMode registration/dispatch and current state
acceptance/caps remain. No ADR is newly Accepted and no broader policy is selected implicitly.

**0.9.10 readiness:** ready for review of the implemented changes, **not release sign-off**. Required
CI/platform/pluginval/ABI/static-analysis lanes and native-host/manual release validation remain;
0.9.9's owner sign-off does not carry forward. F9's synthetic failure needs explicit owner triage
before claiming it closed or approving support for that shape. KI-029 and RISK-015 remain open
owner decisions; preserving their existing behavior is the disposition of this implementation round.

Changed files in the initial delivery (26):

- `CHANGELOG.md`
- `CMakeLists.txt`
- `README.md`
- `docs/COMMERCIAL_STATUS.md`
- `docs/DOCUMENTATION_COVERAGE.md`
- `docs/FUTURE_RISKS.md`
- `docs/HANDOVER.md`
- `docs/KNOWN_ISSUES.md`
- `docs/POSTMORTEMS.md`
- `docs/REPOSITORY_MAP.md`
- `docs/SOURCE_OF_TRUTH.md`
- `docs/architecture/PARAMETER_REGISTRY.md`
- `docs/architecture/RELEASE_HARDENING_PLAN.md`
- `docs/architecture/SERIALIZATION_REGISTRY.md`
- `docs/architecture/STATE_SERIALIZATION.md`
- `docs/architecture/design-decisions/ADR-0050-a-gestures-ownership-ends-with-its-last-on-release-action.md`
- `docs/architecture/design-decisions/ADR-0056-the-session-and-a-b-parser-boundary.md`
- `docs/policies/TESTING_POLICY.md`
- `docs/procedures/TESTING.md`
- `src/PluginProcessor.cpp`
- `src/PresetManager.cpp`
- `src/SerializedXml.h`
- `src/gui/SpectrumImager.cpp`
- `tests/state_hardening_tests.h`
- `tests/state_tests.cpp`
- `worklogs/STATE_HOST_HARDENING_0.9.10.md`


## PR #168 review follow-up — 2026-10-22, sole nested mouse-up

Starting head: `32033f61030b9c07bf7505fefd3958b8c5195e1d`. Scope is the F10 release completion
reported in review; serialization, advancedMode, F9 and all outstanding owner decisions are unchanged.

**Verified failure:** removing the extra mouse-up/cancellation before assertions produces **66
checks / 8 failures** on that head: both pins stay open, and the next Width edit loses its Undo
boundary. The real processor/editor fixture runs with JUCE's cached button already up, so the
editor's cached-down/OS-up safety net cannot run. The earlier 42/0 measurements above were real
executions of an insufficient fixture; they are not evidence that the sole-release case was fixed.

**Correction:** retain the first owner release event without clearing the in-progress action's
identifiers. The drag, routed-wheel and normal-release boundaries count event nesting and replay
only after the outer event and its actions finish. A callback at `beginBandMove` exit would be too
early: the enclosing drag/wheel still uses the band identifiers for preview and stores. Likewise,
a nested no-op event during a store must not drain the pending release while its caller is active.
The deferred event is cleared before replay, preventing a closing callback from replaying it twice.
Replay uses the normal mouseUp owner/staleness gates, preserving release actions and cancelling
stale gestures. Completion uses safe component pointers; it is synchronous on the message thread.
There is no timer retry, posted event, cross-thread path, parameter contract or Undo-model change.

**Regression:** State test 143 uses only the nested release in its opening legs, then asserts exact
open/close counts, no early close, unchanged Solo and the next edit's Undo before any cleanup.
Additional legs cover a release during the first close, a host write making the gesture stale,
and a nested zero-delta wheel event in the remaining store. Passing legs get neither a second
release nor cancellation. Failure-only cleanup comes after all assertions so mutations can repeat
on the same processor without hiding a failed leg.

**Executed validation (this follow-up, local Linux):**

| Check | Actual result |
|---|---|
| Sole-release reproducer on starting PR head | **66 / 8 failures**, exit 1, including unclosed edge gestures and lost later Undo. |
| Final State test 143, GCC Release | **108 / 0**, exit 0: 12 drag/wheel schedules, with no second release/cancellation in passing opening legs. |
| Final State test 143, Clang 22 ASan/extended UBSan | **108 / 0**, exit 0, leak detection enabled. |
| Negative control: discard only the queued release | **108 / 12 failures**, exit 1. Both missing closes and later Undo assertions detect the initial PR behavior. |
| Negative control: ignore event depth while draining | **108 / 4 failures**, exit 1. Each nested no-op event closes the gestures before its outer store finishes. |
| Full state suite, GCC Release with `MALLOC_PERTURB_=1` | **5,896 / 0**, exit 0. |
| Full state suite, Clang 22 ASan/extended UBSan | **5,896 / 0**, exit 0, same strict ASan/UBSan options and 64 MB stack recorded above. |
| Full DSP suite, GCC Release / Clang ASan/UBSan | **944 / 0** and **940 / 0**, each exit 0. The existing ASan malloc-guard limitation remains. |
| Focused valgrind 3.24 memcheck | State 143 **108 / 0**, exit 0; **0 errors / 0 contexts**, no suppressions; `--track-origins=yes --leak-check=no --error-exitcode=1`. ASan supplies separate leak coverage. |
| Static source checks and their self-tests | Portability, real-time, dispatch and state coverage all exit 0. |
| Documentation / citation / Clang checker self-tests | All exit 0. Documentation check: 160 files clean. Citations: 612 anchors unchanged against the starting PR head. |
| Clang 22 warning comparison | Exit 0, no new first-party warnings. Missing baseline-site notices are from the incremental build, not new suppressions or a re-baseline. |

The two final negative controls use a separate `SpectrumImager.cpp` object, compiled at `-O0`
with the same compiler flags/current header, linked with the unchanged Release test and other
production objects. No production source or object is replaced. To repeat the mutations on a
private copy: remove only `deferredMouseUp.emplace(e)` for the first; for the second still decrement
`gestureEventDepth` but omit its positive-depth early return in `finishGestureEvent`. Both use
`--nested-release-probe`. The normal/sanitized binaries and full suites run the unmutated code.

The 142 numbered tests are unchanged; the state assertion count rises from 5,830 to 5,896 because
State 143 expands from 42 to 108 checks. Legacy state/preset and NaN writer coverage are part of
both full runs. No claim is made for new-head CI, native host mouse delivery, Windows/macOS,
pluginval, full valgrind, RTSan/TSan or manual release sign-off. GCC 16's warning gate remains
unavailable locally (GCC 14 here); the repository's compiler pins/baselines are unchanged.

This follow-up changes nine files: `src/gui/SpectrumImager.cpp`, `src/gui/SpectrumImager.h`,
`tests/state_hardening_tests.h`, `CHANGELOG.md`, `docs/POSTMORTEMS.md`, `docs/procedures/TESTING.md`,
`docs/architecture/design-decisions/ADR-0050-a-gestures-ownership-ends-with-its-last-on-release-action.md`,
`docs/DOCUMENTATION_COVERAGE.md` and this worklog. The PR now has 27 changed files: the initial 26
plus `SpectrumImager.h`. No release tag or publication is created.

## Build & Validate #1777 — 2026-10-22, cancellation and Rosetta investigation

Starting commit: `a73c345d1a55c8dd5f1f2088b34def7ee5662977`, PR #168's published head.
Evidence: [run 39846676802](https://github.com/skyRolly/Anamorph/actions/runs/39846676802),
attempt 1 job logs and check annotations, compared with successful push run #1775
([39764585678](https://github.com/skyRolly/Anamorph/actions/runs/39764585678)). The green #1778
was the separate same-repository PR/merge-check path; it did not replace the failed push matrix.

| Job / initial outcome | Verified cause and timing |
|---|---|
| `linux`, cancelled | Annotation: "The job has exceeded the maximum execution time of 40m0s". Setup took **35:21**. Azure's Ubuntu mirror fetched 79.1 MB in **34:25**, 38.3 kB/s. The job expired during the 1 MB-stack re-run, after the regular suites passed. |
| `sanitizers`, cancelled | Annotation: "The job has exceeded the maximum execution time of 1h0m0s". Setup took **45:36**: 80.8 MB in **33:17**, then 22.5 MB for valgrind in **11:26**. ASan/UBSan passed; the job expired about seven minutes into DSP memcheck. |
| `macos`, failed | Rosetta's x86_64 DSP run: **940 checks, 1 failure**, Test 61's exact scope-frame-order assertion. Counts were exact; the oldest-first frame check failed. The x86_64 state run was consequently not reached. Native arm64 suites and the four pluginval gates completed. |

The cancellation annotations establish timeout, not owner cancellation or a macOS failure's
fail-fast cascade. These are independent jobs with no matrix or `needs` edge. The runner's image,
macOS 26.6.2 / image `20261013.0424.1`, and AppleClang `21.0.0.21000101` also match the earlier
green #1775, including its executed Rosetta DSP **940/0**. `ScopeBuffer.h`, Test 61, the workflow
and dependency setup are unchanged from the 0.9.9/main baseline. This does not establish a
Rosetta, compiler or production-ring root cause.

**Selected repairs.** The Linux CI composite action runs the new
`configure-ci-apt-mirrors.sh` self-test and then selects Ubuntu's official HTTPS archive for the
exact Azure archive URL in `/etc/apt/apt-mirrors.txt`, before apt starts. It preserves the mirror
priorities and other entries; the absent-list/container path is a no-op. Package lists, signing
checks, compiler pins, job budgets and test strictness stay as they were. This is CI setup, not a
dependency-set or shipping-toolchain decision. A local HTTP probe independently read the official
archive's release-metadata URL (200, 254,968 bytes, 4.81 s); Azure's equivalent returned
503 through this workspace's proxy. That HTTP probe did not verify a signature and is not a
measurement of the hosted runners; apt's existing signing verification is unchanged.

Test 61 now reports the first mismatch's write count, read length, frame index and hexadecimal
expected/actual samples. Its exact assertion and production ring are unchanged. The old output
only said `frames WRONG`, so it did not distinguish an input, expected-value, copy or read defect.
The added diagnostic moves one live citation in `CI_CD.md` by ten lines; that reference is updated.
No changelog entry is warranted for CI setup and test diagnostics alone.

**Verification executed locally.**

- Mirror helper: **8 checks / 0 failures**, including metadata, exact URL boundaries, unchanged
  custom/ports/security/comment entries, already-configured/empty/absent lists; Bash syntax and
  the composite action's YAML parse also completed. Replacing the rewrite with a no-op (the old
  selection) makes **3/8 checks fail**, exit 1. The fixture test needs no root or network.
- Unmodified Test 61's body extracted into a standalone probe against the real `ScopeBuffer.h`,
  Clang 22 `-O3 -march=haswell -ffp-contract=off`, repeated 1,000 times with heap perturbation:
  **6,000 checks / 0 failures**. This is native Linux, not a Rosetta reproducer.
- The original standalone ring probe also ran 100 repetitions under valgrind 3.24: **600/0**,
  zero errors/contexts, zero suppressions and no heap blocks left at exit; leak checking and
  origin tracking enabled. This does not execute Rosetta or the full processor.
- An isolated ring mutation returning one older frame after wrap fails **3/6** Test 61 checks
  and prints the first mismatch (`written=16758`, one-frame read, expected 16757, actual 16756).
  The diagnostic preserves the existing gate's ability to fail; it is not a repair of the still
  unexplained Rosetta assertion.
- Rebuilt full DSP suites: native GCC Release **944/0** with heap perturbation; Clang 22
  ASan/extended UBSan **940/0**, leak detection and the existing strict options enabled.
- Real-editor sole-release regression re-run: **108/0**. Production sources and state fixtures
  are untouched in this follow-up.
- Portability and realtime checks and their self-tests completed; the Clang checker self-test
  and incremental comparison report no new first-party warnings. Documentation self-test and
  lint completed (160 files clean); the citation self-test and check preserve 612 anchors
  against the starting head after updating the one moved reference.

An explicit re-run of the failed/cancelled jobs was requested for the original head (attempt 2).
Its sanitizer dependency setup completed in **52 seconds**; the Linux retry again stayed in
dependency setup for over ten minutes. Publishing this correction uses the existing per-ref
concurrency rule, which automatically supersedes the active retry with the new-head run. Remote
verification is reported separately on PR #168 for the final head. The current evidence does not
authorize a speculative ring/threading change.


**Original-head retry evidence (attempt 2, before the correction superseded it).** The macOS job's
completed logs show native arm64 DSP **940/0**, state **5,741/0**, then Rosetta x86_64 DSP **940/0**
and state **5,741/0** (Rosetta suites finished at 21:41:44 UTC). Test 61 reports the frames newest
and in order. The first failure did not reproduce on the identical source revision; no evidence
supports choosing a speculative ring or threading repair. The new failure diagnostic remains
useful because the original log cannot identify which frame differed. Publishing `b63cccb`
automatically superseded this retry during pluginval, as announced; that is a separate, expected
cancellation from attempt 1's timeouts. The new push validation is #1779 and the separate PR path
#1780. This documentation correction also uses the normal branch/concurrency workflow.
