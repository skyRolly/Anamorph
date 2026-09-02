# FUTURE_RISKS.md

Potential technical risks. Each is evidence-based (constraint C7) — no invented risks. ADRs and
postmortems may reference these IDs to close the loop. Severity: Low / Medium / High / Critical.

**Round 19 (2026-09-02): RISK-008 gains its real-host evidence** — the maintainer ran the
predicted-failure workflow on Linux in REAPER with the real Anamorph VST3 and the reported latency
updated with the editor both open and closed, so the entry moves from "mechanism confirmed, no host
tested" to **real-host validated for REAPER, with the host-specific residual explicitly unverified**.
No production change; D-1 untouched. Prior:
**Round 15 (2026-09-02): one new entry, RISK-008** — an inspection finding from the ER-STATE-19
verification (a JUCE Linux VST3 wrapper behaviour, host prevalence unknown) — and RISK-007 gains a
round-15 note recording that the same thread class reached `prepareToPlay`, is closed there, and
that its pluginval argument is a VST3-only statement. Prior:
Version-synced to **v0.9.6** (the round-1 engineering-review fixes — **one new entry, RISK-007**:
the off-main-thread state-call exposure, found by the review's thread-safety lens and recorded
here because the guard that would close it is itself an Architecture-Review-Gate item. The same
sync corrects two pieces of drift per `DOCUMENTATION_LIFECYCLE_POLICY` C6: this header **was
never synced for v0.9.5** — the A7 performance round changed no risk, which is exactly what a
sync note should have said, and this note now says it — and RISK-003/RISK-004 below are updated:
RISK-003's planned first tag is renumbered to the current release in preparation, and RISK-004's
Windows analog is **fixed**, `run-pluginval.ps1` no longer retrying real crashes, so that risk
is Linux-scoped again as its 2026-08-18 note intended).
Prior sync: **v0.9.4** (the JUCE 9.0.0 → 9.0.1 dependency upgrade, ADR-0026 — **no new
risk**: RISK-001 is the risk this change is an instance of, and its mitigation was executed in
full (twin-dump bit-identity, both suites, pluginval strictness 10 in both modes, identical
warning set); no source, no build dependency, no serialized state, parameter or DSP behaviour
changed, and no new limitation appeared. The same version also carries the **C++ standard 17 → 23**
migration (ADR-0027) — likewise **no new risk entry**: its one `src/` change is an added
`#include`, engine output is bit-identical C++17 vs C++23, and its single open caveat (MSVC has no
stable `/std:c++23`, so CMake requests `/std:c++latest`) is carried in ADR-0027 §Consequences with
its escape hatches. The same version also moves the macOS CI job off the **deprecated** `macos-14`
image to `macos-latest` — likewise **no new risk entry**: the floating label is what the other two
jobs already use, its toolchain-drift exposure is the same shape as ADR-0027's MSVC
`/std:c++latest` caveat, and it is recorded with the measured compiler move in
`docs/procedures/CI_CD.md`. The four `-Wimplicit-int-float-conversion` diagnostics that move
surfaced are **fixed** — an explicit `(float)` cast per site, with the three translation units
verified to compile to byte-identical machine code, so no risk attaches to them either.
RISK-003's mitigation now names the release in preparation as the first
tag — v0.9.3 was written up but, like 0.9.0-0.9.2 before it, never cut). Prior sync: **v0.9.3** (six GUI interaction fixes plus an equal-width Widen row: the Multiband add-split preview line, the
unified pop-up dismissal shield, pop-up lifetime across a hidden editor / background application,
menu width, disabled menu items and the Tooltips on/off transition
— **no new risk**: editor-only, with no serialized state, parameter or DSP behaviour changed. The three
new limitations — KI-018, KI-019 and KI-020 — are known *issues* and live in `KNOWN_ISSUES.md`, not
here.
RISK-003's mitigation now names **v0.9.3** as the first tag). Prior sync: **v0.9.2**
(preset drop-down lifetime/crash fix, factory-preset identity, the `UI Scale` label and the installer
component titles — no new risk; the one new limitation is an OS text-input behaviour filed as
KI-017). Prior: **v0.9.1**
(manufacturer-code change, ADR-0023 — no new risk: RISK-003's
mitigation then named v0.9.1 as the first tag, and the one-time session break is a documented
known issue (KI-016), not a forward-looking risk). Prior sync: **v0.9.0** (release-prep,
2026-07-24, PR #87 — packaging/installers + user
docs + version bump — and PR #89, the installer/packaging rework: component selection, system-wide
installs, flat ZIP-only artifacts; no DSP/GUI code change in either, no new risk; the unsigned
installers inherit the existing signing/notarization gap already tracked as RH-PR-3/5). Previously verified against
repository HEAD `64e87c4` (post-v0.8.12 content re-audit), synced to the
**v0.8.12 release** (changelog-dated 2026-07-22, PR #79 performance Wave 6 + PR #80 GUI interaction
fixes — pixel-identical / message-thread-only, no new risk; the **v0.8.11 release** of 2026-07-20
likewise introduced none: PRs #60/#61 — the ADR-0015 crossover-follower fixes, behaviour-changing
by design with the trade tracked as KI-012; PRs #62/#76 — Class-A performance Waves 3–5,
twin-dump validated; PR #63 — RH-PR-2 build hardening, byte-exact). Prior sync: the **v0.8.10 release**
(finalized 2026-07-14, PR #59 — undo/redo forced-duck dry-fill, multiband
flat recombination, adaptive `FrameClock` GUI refresh — introduces no new risk: the engine fixes
are behaviour-preserving (single swaps byte-identical) or a documented magnitude correction
(multiband), `FrameClock` is a message-thread GUI change, and the multiband allpass adds a known
CPU cost tracked in PERFORMANCE_BUDGET, not an open risk). Prior: the v0.8.9 release (finalized
2026-07-12, PR #58 — Wave-2 performance work introduces no new risk: H6 replaces the crossover
filter with a bit-exact local clone, H15 adds two generation counters following the existing
sanctioned staleness-hint pattern, H3/H4/H11 are bounded Class-B changes); before that PR #56
(JUCE 8.0.14) and 0.8.8 (PR #54).

| ID | Risk | Severity | Likelihood |
|---|---|---|---|
| RISK-001 | JUCE version bump silently changes DSP/latency/editor behaviour | High | Medium |
| RISK-002 | Always-on monitor/crossover banks + per-sample coeff recompute → CPU | Medium | Medium |
| RISK-003 | No git release tags → fragile version/CHANGELOG attribution | Low | High (already true) |
| RISK-004 | pluginval signal-only retry could mask a real future editor crash | Medium | Low |
| RISK-005 | Manual-only audio/visual + host validation lets regressions ship green | Medium | Medium |
| RISK-006 | Undeclared licensing: no `LICENSE`/EULA, and the commercial JUCE licence required by the closed-source model is not yet obtained | High | High (already true) |
| RISK-007 | State calls on a non-main host thread race message-thread state (AU autosave; out-of-spec VST3 hosts) | Medium | Low |
| RISK-008 | A Linux VST3 host that hands its `IRunLoop` over only through `IPlugFrame` leaves the plug-in's JUCE message queue unserviced while no editor is open (D-1 timer, APVTS value flush) | Medium | Low — real-host validated in REAPER; other Linux hosts unverified |

---

## RISK-001 — JUCE version bump
- **Risk:** JUCE is pinned to exactly `9.0.1` (immutable commit `e18f7f5…`, ADR-0026; previously
  `9.0.0` = `f8f8864…`, ADR-0022; before that
  tag `8.0.14`, ADR-0012). A future bump can silently change DSP behaviour (oversampling,
  Linkwitz-Riley filters, `dsp::AudioBlock`), reported latency, the parameter/state ABI, and the
  X11 editor-embedding path (the INC-006 crash lives in JUCE's host code).
- **Impact:** Audible DSP/latency drift, session/automation incompatibility, or a returning editor
  crash — none of which the headless gate fully catches.
- **Likelihood (evidence-based):** Medium — dependencies eventually need security/feature updates;
  the pin defers but does not eliminate this. The SHA pin (v0.8.13 cycle) additionally removes the
  re-pointed-tag variant of the risk.
- **Evidence [Verified]:** CMakeLists.txt:70-72 (exact commit); ADR-0011 (X11 in JUCE); `docs/policies/DEPENDENCY_POLICY.md`.
- **Mitigation:** Treat any bump as a Build System change → ADR + Architecture Review; run full DSP
  tests + pluginval (3 OSes) + a manual audition + the RELEASE_COMPATIBILITY_CHECKLIST after. The
  8.0.14→9.0.0 bump additionally proved engine output **bit-identical** via a 32-scenario twin
  dump (ADR-0022) — the pattern to repeat on future bumps, and it **was** repeated for
  9.0.0→9.0.1 (ADR-0026: 32/32 hashes and latencies identical, warning set byte-identical across
  the 18 project translation units).

## RISK-002 — Always-on banks / crossover-move cost (CPU)
- **Risk:** `SoloMonitor` runs every block even with multiband off and no solo (INC-009 invariant;
  since 0.8.9/H1 the settled passthrough goes cold, shrinking this), and `MonoMaker`,
  `MultibandWidth` and `SoloMonitor` recompute Linkwitz-Riley coefficients **per sample** while a
  cutoff glides (since 0.8.10 the multiband/solo cutoffs track under the frequency-proportional
  R(f) = 4·max(1, f/300) oct/s cap — ADR-0015 final + slow-drag fix — so the per-sample
  recompute lasts as long as the drag plus ≤ ~1 s of worst-case catch-up; a discrete step
  instead runs **two banks for one ~12 ms crossfade**, 2× the stage's filter ticks). Under heavy multiband automation or on low-power hosts this could be a
  hot path. Formal budget numbers are not yet committed (session-local Wave-3/4/5 callgrind
  measurements exist — drag scenarios −35…−50 % after Wave 3; see PERFORMANCE_BUDGET).
- **Impact:** Higher-than-necessary CPU in Simple mode and CPU spikes during fast split automation.
- **Likelihood (evidence-based):** Medium — the cost is real and constant. The A7 audit (2026-08-22)
  measured the SR/buffer dependence that this row previously called unmeasured: the split **drag**
  costs **+11.2 %** over the static 4-band state (93.3M vs 84.0M Ir/s, of which `__tan_fma` is
  3.26 %), and nothing in either instrument shows a transient cliff. The multiband as a whole is
  **52.4 %** of the working reference. What is still genuinely unmeasured is the part the row exists
  for — the **instance count on a named machine** — because instruction counts cannot answer it and a
  shared runner is not a wall-clock datum. **This risk therefore stays open**, and the audit says so
  in its own §4.5 rather than claiming otherwise.
- **Evidence [Verified]:** src/dsp/AnamorphEngine.cpp:1333 (`soloMonitor.process`, always-on); src/dsp/MultibandWidth.cpp (glide + fade paths);
  Devin PR #50 review (efficiency note); `docs/architecture/PERFORMANCE_BUDGET.md` (TODOs);
  `worklogs/performance/PERF_AUDIT_v0.9.4_INVESTIGATION.md` §3.1, §4.5.
- **Mitigation:** Formal profiling (PERFORMANCE_BUDGET numeric budgets remain TODO — the harness and
  procedure now exist and were exercised end to end by the A7 audit; what is missing is a named,
  held-still machine to run them on, which is that audit's roadmap item 01). The SoloMonitor
  settled-skip **shipped**: H1 (0.8.9) plus the Wave-3 gains-only cold gate, guarded by Test 33
  (`testSoloColdThroughDrag`) — the settled passthrough now goes fully cold. Correctness is
  unaffected either way.

## RISK-003 — No git release tags
- **Risk:** The repository has no tags, so version/CHANGELOG attribution relies on commit messages.
  Reconstruction is error-prone and cannot be Verified to a release artifact.
- **Impact:** CHANGELOG entries for older versions stay Partially Verified / reconstructed; harder to
  reproduce a specific shipped build.
- **Likelihood (evidence-based):** High — already the case (`git tag` is empty).
- **Evidence [Verified]:** `git tag` empty; `docs/policies/CHANGELOG_POLICY.md`; `docs/procedures/RELEASE_PROCESS.md`.
- **Mitigation:** **Infrastructure shipped (RH-PR-8, v0.8.13 cycle):** annotated `vX.Y.Z` tag
  convention + tag-triggered `release.yml` (fail-closed tag⇄version⇄CHANGELOG validation →
  reused `build.yml` gates → draft GitHub Release with versioned artifacts + SHA-256 sums +
  manifest). The risk **closes when the first release tag is cut** (planned: **v0.9.6** — 0.9.0 through 0.9.5 were each written up but never tagged); until
  then, cite commit SHAs. Historical entries keep SHA evidence permanently.

## RISK-004 — pluginval signal-only retry masking a real crash
- **Risk:** `run-pluginval.sh` retries on a signal-crash to absorb the external X11 flake
  (INC-006/KI-003). A genuine *new* editor crash that also exits with a signal could be retried away
  and pass on a later attempt, hiding a real defect.
- **Impact:** A real crash regression could ship if it happens to pass on retry.
- **Likelihood (evidence-based):** Low, and **lower since 2026-08-18** — the retry is now scoped by
  `uname -s` to the platform its justification names, so macOS gets exactly one attempt and this risk
  no longer applies there at all. On Linux retries stay capped at 3 and a deterministic crash still
  fails all attempts. **Windows no longer carries an analog since 2026-08-31** (ER-CI-01): after
  the KI-007 WaitForExit fix retired the null-exit-code detection problem, `run-pluginval.ps1`'s
  3-attempt loop had been left excusing exclusively genuine Win32-exception crashes; it now fails
  a real abnormal exit immediately and retries only a failed *launch* — so this risk is
  Linux-scoped again, as the 2026-08-18 note intended.
- **Evidence [Verified]:** scripts/run-pluginval.sh:147-198 (`run_one_pass`; retry only on exit ≥128, cap 3);
  scripts/run-pluginval.ps1 (verdict block: crash → immediate failure, retry only on `$null`).
- **Mitigation:** Investigate any repeated crash rather than trusting the pass; keep the cap; a real
  assertion (exit <128) already fails immediately with no retry.

## RISK-005 — Manual-only audio/visual + host validation
- **Risk:** Audio quality, GUI/vectorscope appearance, and real-DAW host behaviour cannot be verified
  headlessly; a green build + pluginval pass is "ready to audition," not final.
- **Impact:** A sound/visual or host-specific regression can pass CI and reach testers.
- **Likelihood (evidence-based):** Medium — depends on diligence of the manual Level-5 sign-off.
- **Evidence [Verified]:** docs/procedures/TESTING.md ("What cannot be verified headlessly"); `docs/policies/TESTING_POLICY.md` (Level 5);
  `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` (host-matrix item).
- **Mitigation:** Enforce the manual audition + host-matrix line items at release; expand the
  documented host coverage as it is performed.

---

## Adding a risk

Create the next `RISK-NNN` only when a TODO/FIXME, issue, PR discussion, or concrete code limitation
supports it. State the likelihood **basis**, cite evidence with a confidence level, and give a
mitigation. Do not invent risks to fill the template.

## RISK-006 — Undeclared licensing (no LICENSE, no approved EULA, JUCE tier unchosen)
- **Risk:** The repository root has **no `LICENSE` file** and neither installer presents an
  end-user agreement, so the terms under which Anamorph's own source and binaries are offered are
  undeclared. `EULA.md` is an **unapproved draft** (not in force, not shipped) and does not
  change that. The stated product model (owner, 2026-07-26) is **closed-source commercial**; JUCE 9
  modules are dual-licensed **AGPLv3 or commercial**, and a closed-source distribution cannot use
  the AGPLv3 arm — the commercial JUCE tier must be in place before commercial distribution. A
  third strand —
  the Steinberg VST 3 trademark/distribution review — is separate again (the SDK *code* is MIT in
  JUCE 9.0.1; the VST name and plug-in distribution terms are not covered by that grant).
- **Impact:** Blocks a commercial release outright, and leaves even a free release legally
  ambiguous for anyone who downloads, redistributes or contributes. Third-party **attribution**
  is a different obligation and is already discharged (`NOTICE` + `THIRD_PARTY_LICENSES.md`
  accompany every download as release-page assets, which since 2026-07-26 carry the IJG
  acknowledgement on their own) — this risk is specifically about Anamorph's *own* terms.
- **Likelihood (evidence-based):** High — already the case (`ls` shows no `LICENSE`/`COPYING`).
- **Evidence [Verified]:** repository root (no licence file); `THIRD_PARTY_LICENSES.md`
  §"Open licensing decisions"; the pinned JUCE tree's `LICENSE.md` (dual licence);
  `docs/KNOWN_ISSUES.md` KI-015.
- **Mitigation:** **None available to engineering** — this is an owner/legal decision, tracked as
  RH-R11 / RH-F1 (and RH-F2 for Steinberg) in `docs/architecture/RELEASE_HARDENING_PLAN.md` and
  indexed with the other open decisions in `docs/COMMERCIAL_STATUS.md` §4. It
  closes when the commercial JUCE licence is obtained and a `LICENSE` (plus an EULA, if the
  product is sold) is added. Until then, cite this risk rather than assuming any particular
  terms.

## RISK-007 — State calls on a non-main host thread (unguarded Anamorph-owned tail)
- **Risk:** `getStateInformation`/`setStateInformation` mutate non-atomic message-thread-read
  state with no lock or marshalling — `internal.restoreState`, `abSlot`/`abActive`/`abUndo`,
  `presets.setMeta`/`adoptRestoredState`, `syncCommitted` (src/PluginProcessor.cpp:926-1181 read
  side, :661-691 write side; the APVTS half is internally locked by JUCE). A host that calls
  state functions off its UI thread while the editor's 24 Hz timer is running races
  `juce::String`/`std::vector`/`ValueTree` state — torn-read UB, crash-class.
- **Impact:** Crash or corrupted preset/undo metadata during a project recall or autosave in
  such a host, with an editor open.
- **Likelihood (evidence-based):** Low. On VST3 (the sole Windows/Linux format) the pinned SDK
  annotates both `getState` and `setState` `[UI-thread]` (ivstcomponent.h:198-204) and JUCE
  debug-asserts it for `setState`, so a race needs an out-of-spec host; JUCE hosting (and thus
  pluginval, strictness 10) wraps restore in `MessageManagerLock`, so the release gate
  structurally cannot produce the window. The genuinely unguarded exposure is the **macOS AU**
  build, where no spec forbids off-main-thread `SaveState`/`RestoreState` (host autosave is the
  real-world case) and the JUCE AU wrapper passes both straight through on the caller's thread.
- **Evidence [Verified]:** engineering-review round 1 (ER-RT-03/ER-STATE-05, adversarially
  verified against the pinned JUCE 9.0.1 and VST3 SDK trees);
  `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 1.
  **MEASURED in round 2 (R2-2), and the races are real.** `AnamorphStateTests
  --state-thread-probe` (tests/state_tests.cpp) drives the modelled interaction —
  one thread calling `setStateInformation`/`getStateInformation`, the main thread
  performing the editor tick's reads — under ThreadSanitizer. It reports **four
  distinct data races**, exactly on the members round 1 predicted:
  1. `abActive` — written by `setStateInformation` (src/PluginProcessor.cpp),
     read by `canUndo()` (src/PluginProcessor.h);
  2. and 3. the `abUndo` vector's internals — `UndoStacks::operator=`
     (src/PluginProcessor.h) against the main thread's iteration/`empty()`;
  4. a `juce::String` reference-count exchange (`juce::Atomic<char*>::exchange`)
     — the PresetManager metadata assignment against `juce::String`'s copy
     constructor on the reading thread.
  So the *code* question is settled: IF a host makes these calls off the main
  thread while an editor is open, this is undefined behaviour, not a theoretical
  concern. What remains open is only the *host* question (see Likelihood).
- **Mitigation:** Recorded here rather than fixed because any lock/hop guard is a
  threading-model change — an Architecture Review Gate item needing maintainer sign-off
  (decision **D-2**; the round-2 measurement above is the evidence it was waiting on)
  (`docs/policies/THREADING_POLICY.md`; the communication tables there and in
  `docs/architecture/THREAD_MODEL.md` deliberately omit state calls, which this entry now
  documents as an assumption, not an oversight). Candidate fix if approved: a narrow mutex over
  the state-set members, or `callAsync` marshalling of the metadata/undo tail. A TSan
  two-thread harness is the cheapest next investigation.
- **Round 15 (2026-09-02, ER-STATE-19):** the same off-message-thread class reached
  `prepareToPlay`. Its latency report — `setLatencySamples` and the engine's `latency2/4/8` —
  raced the processor's own D-1 timer with NO editor open, and on Linux it did not even need an
  out-of-spec host: JUCE's VST3 wrapper services the plug-in's messages from its own thread until
  the host registers an `IRunLoop`, for the plug-in's whole life if it never does. On macOS the
  release gate itself reached it: pluginval calls an AU's `prepareToPlay` — and `setState` — on
  its test thread, hopping to the message thread for VST3 only, so the "pluginval … structurally
  cannot produce the window" argument above is a VST3 statement. For this entry's own state tail
  the macOS gate goes further: pluginval's `BackgroundThreadStateTest` (`Source/tests/BasicTests.cpp`,
  verified this round) holds the editor open on the message thread and calls `getStateInformation`
  / `setStateInformation` from a background thread — on AU, with no hop, that is exactly the window
  this entry describes, exercised on every green macOS run. Green because a data race is not a
  crash and the gate does not run ThreadSanitizer; the Likelihood above is therefore about
  shipping hosts, not about whether the window is ever produced. That instance is
  **closed** (message-thread-only delivery through the D-1 request; relaxed atomics on the engine
  figures; State test 30; `AnamorphStateTests --reprepare-race-probe` under TSan — two reports
  before, silence after). The state-call tail this entry tracks is unchanged and still gated on
  D-2; an off-message-thread prepare against an OPEN editor's reads of engine state is this
  entry's exposure and is covered by it, not by round 15.
- **Round 20 (2026-09-02, ER-STATE-23): re-raised, measured, and found to be entirely this entry —
  no new bug, and no production change.** The finding was that the D-1 latency atomics "do not
  synchronize concurrent restore, prepare, A/B, preset, or engine state". They do not, and were
  never meant to: `latencyUpdateRequest` carries the latency REQUEST and nothing else, so reading
  it as a general state barrier is a category error rather than a defect. The question worth
  answering is what the underlying states actually do, and it splits three ways. **The restore /
  A/B / preset tail is exactly what this entry already records** — the same four TSan reports, on
  the same members, gated on the same D-2 decision. **The ENGINE's plain state does not race at
  all**: `setStateInformation` never writes it, the A/B and preset paths reach the engine only
  through atomics (`injectMatchGainDb`, `requestDuck`), and the two writers that remain —
  `prepareToPlay` and `processBlock` — are mutually excluded by the host contract on VST3 and by
  JUCE's own AU callback lock. **The one pairing D-2's recorded scope does not name** — restore on
  one host thread, `prepareToPlay` on another, editor tick reading — was measured for this round
  with a new probe (`AnamorphStateTests --state-prepare-race-probe` under TSan, three threads):
  **the same four reports and no new ones.** Recorded as covered by the deferred D-2 decision.
  Nothing was added to suppress the report — no mutex, no `callAsync`, no `AsyncUpdater`, no
  state-architecture redesign — because doing so would pre-empt D-2, which is the maintainer's
  call, and would silence the very evidence D-2 is waiting on.
- **Round 21 (2026-09-02, ER-STATE-23 re-raised): re-measured on the current tree, same four
  reports, still no production change.** The finding arrived again, at the same source line
  (`setStateInformation`, `src/PluginProcessor.cpp:926`) and with the same wording plus one added
  sentence — "the documented macOS AU race remains open" — which is this entry's own Likelihood
  bullet restated, not new evidence. Two things were checked rather than assumed. First, the
  concurrency surface has not moved: `src/PluginProcessor.cpp` and `src/PluginProcessor.h` are
  unchanged since round 16, so the code the finding names is byte-identical to what round 20
  measured. Second, the probes were re-run under ThreadSanitizer against the current build:
  `--state-thread-probe` and `--state-prepare-race-probe` each report **the same four races and no
  others**, and `--reprepare-race-probe` is **silent**, so ER-STATE-19/D-1 also remains closed. Each
  report maps one-to-one onto a row already recorded above — `abActive`, written at
  `src/PluginProcessor.cpp:990`, against `canUndo()`; the `abUndo` vector's internals twice, via
  `UndoStacks::operator=` (`src/PluginProcessor.h:184`) against the reader's iteration; and the
  `juce::String` refcount exchange, `juce::String`'s copy constructor against the metadata
  assignment. Nothing new, and again no mutex, `callAsync`, `AsyncUpdater` or state-architecture
  change.

## RISK-008 — A Linux VST3 host that provides its run loop only through `IPlugFrame` starves the plug-in's message queue while the editor is closed
- **Risk:** the pinned JUCE Linux VST3 wrapper services the plug-in's JUCE messages — every
  `juce::Timer`, `callAsync` and `AsyncUpdater` — from an internal background thread until the
  host registers an `IRunLoop`, then stops that thread and attaches to the host's loop
  (`juce_audio_plugin_client_VST3.cpp`, the `EventHandler` / `HostMessageThreadState` machinery).
  The pinned SDK lets a conformant host hand the run loop over EITHER through the factory/host
  context OR only through `IPlugFrame`. In the second kind of host JUCE registers the loop at
  editor attach (`attached()`, `viewRunLoop.emplace`) and unregisters it at editor removal
  (`removed()`, `viewRunLoop.reset()`), and nothing restarts the internal thread until the shared
  `EventHandler` is destroyed at unload. Between an editor close and the next editor open, no
  thread services the plug-in's message queue.
- **Impact:** every JUCE-message consumer in the plug-in pauses with the editor closed in such a
  host: the D-1 latency timer — an audio-thread latency request (Drive/Algorithm automation with
  oversampling engaged) is then reported not within 50 ms but when the editor next opens — and
  the APVTS's own value-flush timer. `prepareToPlay` is unaffected: the host's UI thread stays
  the tagged message thread, so its report is synchronous. No crash and no undefined behaviour;
  a stale host PDC until the editor reopens.
- **Likelihood (evidence-based):** **Low, and no longer unknown.** The predicted failure was
  looked for on a real Linux host and did not occur (the 2026-09-02 REAPER result below). It began
  as a wrapper-behaviour finding verified by reading the pinned tree; a host that provides the run
  loop through the host context is not exposed at all, and the one host actually tested shows the
  behaviour the risk says would break. What remains unverified is every OTHER Linux VST3 host,
  which this repository cannot establish from the inside.
- **Evidence [Verified — wrapper only]:** engineering-review round 15 (raised by the host-contract
  verification lens on ER-STATE-19 and confirmed against the pinned wrapper);
  `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 15.
- **Investigated 2026-09-02 (round 18): the mechanism is CONFIRMED by code reading and its cost
  MEASURED, but no host-visible failure was reproduced, because no Linux VST3 host was available to
  test.** Classified **B — a confirmed technical risk with no demonstrated actionable user-visible
  defect**; no production change. What the round established:
  - **The lifecycle is exactly as filed.** `messageThread->stop()` runs from
    `updateCurrentMessageThread()` when a host run loop is registered, and `messageThread->start()`
    appears in exactly ONE place — the `EventHandler` destructor, which runs at unload. So an
    editor close that unregisters the view's run loop leaves the fds attached to nothing and the
    internal thread stopped, with nothing to restart it.
  - **A stopped queue really does stop the timer.** `juce::Timer` delivers only by posting a
    `CallTimersMessage` for the message thread to run (pinned `juce_Timer.cpp`), so with no
    servicing there are no timer callbacks and the D-1 consumer cannot run.
  - **The request is DEFERRED, not dropped** — a correction to this entry's original wording.
    Measured (`AnamorphStateTests --risk008-probe`, synthetic and labelled as such): across a
    1000 ms unserviced window (20 timer periods) the reported latency does not move, and 22 ms
    after servicing resumes the pending request is delivered in full and the reported value is the
    one the settled state predicts. The atomic request flag is what holds it, so the host is stale
    for exactly the unserviced window rather than permanently.
  - **Scope of the exposure.** Only requests raised OFF the message thread stall: host automation of
    Drive/Algorithm with oversampling engaged, and an off-message-thread re-prepare. Anything on the
    host UI thread is unaffected, because that thread stays tagged as the JUCE message thread after
    the editor closes, so `requestLatencyUpdate()` still delivers synchronously there — which covers
    state restore and any Settings-driven oversampling change.
  - **Evidence limitation as it stood in round 18.** No shipping Linux VST3 host was available in
    that environment, so round 18 could not say whether any host supplies `IRunLoop` only through
    `IPlugFrame`, and it did not claim the issue was reachable in practice. **Superseded by the
    real-host result below**, which supplies the missing half.
- **REAL-HOST VALIDATION, 2026-09-02 (round 19) — performed by the maintainer, not by the review
  harness.** On **Linux, in REAPER, with the real Anamorph VST3**, the reported latency **updates
  successfully both with the Anamorph editor OPEN and with it CLOSED.** That is precisely the
  observable this entry predicts would fail — an editor-closed latency update — and it did not
  fail. This is manual real-host evidence and is recorded as such; it is a different KIND of
  evidence from the synthetic probe above, which measures what an unserviced queue costs and never
  claimed to show that any host produces one.
  - **What it does NOT establish.** It does not show how REAPER supplies `Linux::IRunLoop`. The
    repository contains no evidence on that point — every REAPER reference here concerns unrelated
    matters (KI-009's preset-save focus, VST3 parameter listing, rescan instructions) — so whether
    REAPER hands the loop over through the factory host context, through `IPlugFrame`, or by some
    other route is **not established, and is not guessed at here**. A successful result is
    consistent with REAPER simply not exhibiting the suspected lifecycle, and consistent with
    other explanations this repository cannot distinguish between without evidence it does not
    have. It also says nothing about any other Linux VST3 host.
  - **Disposition: REAL-HOST VALIDATED FOR REAPER; NO ACTIONABLE DEFECT DEMONSTRATED; HOST-SPECIFIC
    RISK REMAINS UNVERIFIED.** The entry stays recorded for the residual — a host using a different
    `IPlugFrame`/`IRunLoop` lifecycle — and that residual does not justify a production change.
- **Mitigation:** recorded, not fixed — any change (restarting the internal message thread on
  unregister, or a host-independent delivery) is a threading-model change and an
  Architecture-Review-Gate item, and D-1 is not reopened by this entry's existence — the REAPER
  result is if anything evidence against needing one. The host census has its first data point and
  REAPER passed; extending it to another Linux host is the only remaining step, and it is an
  observation, not a code change: close the editor, automate Drive across the engage threshold with
  oversampling on, and watch whether the host's PDC updates within 50 ms.
