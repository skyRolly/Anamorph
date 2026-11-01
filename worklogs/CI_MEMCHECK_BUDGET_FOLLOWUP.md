# CI memcheck budget follow-up — 2026-11-01 UTC

**Status:** correction prepared for independent PR review; no merge authorized.
**Source baseline:** integrated main `00de8c7796f37d08a338256e01830bb312ba105c`.
The runtime source/test inputs here remain that baseline. NR-006's implementation
is separately reviewed in #202; this CI change does not depend on that architecture.

## Evidence and disposition

The approved #195–#198 heads passed mandatory CI before sequential integration.
Integrated-main [Build41231806213](https://github.com/skyRolly/Anamorph/actions/runs/41231806213)
then reached the existing 60-minute sanitizer limit on attempt1, inside Valgrind's
state suite. Only that job was retried. Attempt2 job124650617430 ran from
20:35:38 to21:35:37 UTC on 2026-10-31; both suites printed zero failures and both
memcheck summaries reported zero errors, and every step is marked success, but
**GitHub recorded the job and run as cancelled**. Completed test evidence is not
an overall job pass. The cap/finalization race remains material to that status.

The same serial budget was exhausted on unchanged-production #199/#201 and on
NR-006 #202's native-repaired head0991b2f. In #202
[job124663592293](https://github.com/skyRolly/Anamorph/actions/runs/41243991217/job/124663592293),
ASan/UBSan and DSP memcheck completed; state memcheck was still in State147 when
cancelled at60 minutes. #199/#200 retries succeeded, showing runner/cache timing
variation, not a newly failing assertion. No missing state-memcheck verdict is
inferred from the other jobs or from logs preceding cancellation.

**Classification:** recurring CI resource-budget/finalization failures. These
are separate from #202's introduced MSVC DSP stack-fixture failure, which its
repaired native Windows job now passes (941 DSP /10815 state checks, zero
failures; platform counts differ from Linux's945/10980).

**Selected correction:** overlap the independent DSP/state memcheck processes,
then wait for both and fail if either exit status is nonzero or cannot be obtained.
The old serial work comprised setup/build/ASan plus DSP and state memcheck time;
overlap removes the additive DSP+state critical path while retaining the existing
60-minute cap. Actual final-head runtime must still be measured by CI; no saved
minutes or passing budget is claimed before that result.

## Independence, safety and gates

The DSP suite instantiates engine/DSP units and in-memory state helpers; it does
not construct PresetManager or create/save user/session/preset files. State-suite
file/config fixtures therefore have no cross-process DSP peer. Each process has
its own allocator instrumentation, JUCE globals, counters and floating-point mode.
Concurrent execution can contend for runner CPU/memory, so final real-suite CI
remains required; the change establishes no performance baseline or deadline.

The exact existing Valgrind options remain:
`--tool=memcheck --track-origins=yes --leak-check=no --error-exitcode=1 --error-limit=no --num-callers=25`.
Existing unsanitized build flags, allocation-guard disposition and
`ANAMORPH_TESTS_NO_FTZ=1` stay unchanged. Both actual suites and every assertion
remain. No timeout, strictness, detector, suppression, compiler pin, product
thread model, build/link setting, release object or version is changed.

`scripts/run-memcheck.py` uses argv arrays (no shell interpolation), streams
labelled diagnostics immediately, preserves each raw combined stdout/stderr log
and records both command/exit-code results. Both futures are joined even if the
first fails. Missing binaries/validators fail closed. The parent completes only
after its children; it schedules no check-in or persistent background task.
The existing sanitizer job invokes it and retains `Anamorph-memcheck-results`.

**Annotation drift found before handoff:** runner v2.338.2's
[`ActionCommand.TryParseV2`](https://github.com/actions/runner/blob/v2.338.2/src/Runner.Common/ActionCommand.cs)
requires `::` at the beginning after leading whitespace. Ordinary `[suite]`
prefixes would hide existing C++ coverage-warning annotations. The wrapper now
forwards modern/legacy workflow commands unprefixed, and serializes complete
output lines. Six status/overlap controls also assert raw stderr and both emitted
warning commands at column0; a prefix-all-commands mutant must fail. The initial
prefix-only revision is not represented as preserving annotation behavior.


## Validation and remaining work

`python3 scripts/run-memcheck.py --self-test` exercises both-success, DSP-failure,
state-failure and both-failure cases. Each child must see its peer started before
it can finish, so sequentialization fails the success control. Independent frozen
legacy argv checks preserve options; stderr/raw logs and both completion markers
are checked. Missing validator and missing binaries also fail closed. These are
orchestration controls, not a claim that fake children are actual memcheck results.
The same controls run in source-lint and before the real sanitizer step.
Standalone mutants that ignore the state exit status or serialize the workers
are both rejected by these controls. Local docs700/180 files, citation276/610
anchors, 966 Markdown table rows/relative links and whitespace checks pass.
Those are wrapper/document checks, not real memcheck results.

Docs/citations, Markdown tables/links and whitespace checks are run for this diff.
Product/CMake/dependency/version/changelog/test sources are unchanged against
main00de8c7. The final-head native matrix and actual concurrent Valgrind results
must be refreshed in the PR discussion. No main/job success is fabricated; main's
cancelled run remains cancelled until a separately authorized integration and
new successful run supersede it. All new PR merges/releases remain separate owner
decisions. NR-003 stays Deferred; NR-006 performance/manual acceptance stays open.
