# FUTURE_RISKS.md

Potential technical risks. Each is evidence-based (constraint C7) — no invented risks. ADRs and
postmortems may reference these IDs to close the loop. Severity: Low / Medium / High / Critical.

**Version-synced to v0.9.7 (2026-09-06): the pluginval crash-classification fix.** No new entry and
none closed. RISK-004 is **re-scoped in kind rather than in likelihood**: its title term
"signal-only" is retired, because a crash does not always arrive as a signal exit. On macOS
pluginval traps SIGABRT and friends itself and exits **9**, which `run-pluginval.sh` read as a real
validation failure until this change — so on that platform the risk of a masked crash was never the
retry at all, but the classifier, and it was a *mislabelled* rather than a retried-away crash. The
retry cap and its Linux scoping are unchanged. Prior:
**Version-synced to v0.9.7 (2026-09-03): the ADR-0034 latency change.** No new entry and none
closed. RISK-008 (the reported latency can reach a Linux host late, or not until the editor opens,
in a wrapper configuration that hands over its run loop only through `IPlugFrame`) **narrows
sharply without closing**: the reported number is now a function of the Oversampling Setting alone,
so the only way to raise a value-changing latency request at all is to change that Setting or to
restore session state — and a Setting change made in the editor is by construction made with the
editor open, which is the configuration where the residual does not bite. What keeps the entry open
is the other requester: a host calling `setStateInformation` from its own thread with the editor
closed (RISK-007's thread class) can still leave the report late. RISK-007 itself is unchanged in
kind, but the Oversampling Setting is now the only state field whose off-thread restore can move
the reported latency, which is what State tests 22 and 27 were re-instrumented onto. Prior:
**Round 19 (2026-09-02): RISK-008 gains its real-host evidence** — the maintainer ran the
predicted-failure workflow on Linux in REAPER with the real Anamorph VST3 and the reported latency
updated with the editor both open and closed, so the entry moves from "mechanism confirmed, no host
tested" to **real-host validated for REAPER, with the host-specific residual explicitly unverified**.
No production change; D-1 untouched. Prior:
**Round 15 (2026-09-02): one new entry, RISK-008** — an inspection finding from the ER-STATE-19
verification (a JUCE Linux VST3 wrapper behaviour, host prevalence unknown) — and RISK-007 gains a
round-15 note recording that the same thread class reached `prepareToPlay`, is closed there, and
that its pluginval argument is a VST3-only statement. Prior:
Version-synced to **v0.9.7** (D-2 / ADR-0036, 2026-09-03 — **RISK-007 RESOLVED**: the off-message-thread
state-call exposure is closed by making every piece of program metadata message-thread-owned and giving a
host thread two lock-free exchange cells to reach it through; the entry is kept in full below, with its
measurements, as the record of what was closed and how it was measured. **No new entry.** RISK-008 is
unchanged: the D-2 handoff waits with the rest of the message queue in that host, and the entry's
"no crash and no undefined behaviour" holds for it too — the sound and the oversampling atomic are live,
and host saves are served from the host side's own view.)
Prior sync: **v0.9.6** (the round-1 engineering-review fixes — **one new entry, RISK-007**:
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
| RISK-004 | pluginval crash-only retry could mask a real future editor crash | Medium | Low |
| RISK-005 | Manual-only audio/visual + host validation lets regressions ship green | Medium | Medium |
| RISK-006 | Undeclared licensing: no `LICENSE`/EULA, and the commercial JUCE licence required by the closed-source model is not yet obtained | High | High (already true) |
| RISK-007 | **RESOLVED 2026-09-03 (D-2, ADR-0036)** — State calls on a non-main host thread raced message-thread state (AU autosave; out-of-spec VST3 hosts); program metadata is now message-thread-owned and exchanged through two lock-free cells | — | — |
| RISK-008 | A Linux VST3 host that hands its `IRunLoop` over only through `IPlugFrame` leaves the plug-in's JUCE message queue unserviced while no editor is open (D-1 timer, APVTS value flush) | Medium | Low — real-host validated in REAPER; other Linux hosts unverified |
| RISK-009 | A host that writes one parameter from inside another's dispatch, on two threads in opposite orders, nests two JUCE `listenerLock`s in a cycle | High (were it reached) | Low — no listener in this plug-in creates the nesting; it needs the host to do it on two threads at once. The second inversion round 20 added here (a nested poll against a host thread's whole-sound replacement) was REACHABLE and is CLOSED in round 21 by ADR-0036 §26; round 27's dispatch predicate (§30) closed the two doors whose dispatch the PLUG-IN starts, and round 28's admission (§31) closes every remaining door by construction — no state-replacing command WAITS for `soundReplacement`, whoever started the dispatch. The risk stays OPEN on what is left, which contains no Anamorph lock: JUCE's own APVTS 10 Hz timer blocking on `valueTreeChanging`, and the two-parameter nesting above |
| RISK-010 | The DSP snapshot of the ten multiband parameters is ten independent `load()` calls, so the audio thread can read a layout that never existed as a whole | Medium | **Certain** — it is the shipped reader model; what is bounded is the harm, not the occurrence |
| RISK-011 | A gesture count that returns to zero mid-transaction lets a poll record an undo step for a layout the user never had (the v0.9.8 rounds' residuals U1-U3) | Medium | Low as observed, **structural** as a mechanism — nothing in the current code prevents it |
| RISK-014 | **RESOLVED 2026-09-19 (round 51, ADR-0056), COMPLETED 2026-09-21 (round 53)** — both host-state XML parser surfaces (the session chunk and each A/B slot payload) are bounded at 256 KB / depth 8 / no `DOCTYPE` before `juce::parseXML`, each on its own. Round 51's closure claim was overstated: the boundary admitted an opening tag whose quote never closes, and a 12 KB chunk still SIGSEGV'd until round 53 refused it. Valid and legacy sessions are unchanged in both rounds | — | — |
| RISK-015 | Host state still accepts five shapes a `.anamorph` file now refuses — two documents in one chunk (the first wins), trailing prose or binary, a NUL followed by a second complete document, invalid UTF-8 in an attribute value, and a chunk truncated to half its length | Low — a corrupt session is half-applied or silently truncated rather than refused; no crash, hang or unbounded read | **Certain** as a mechanism: it is the acceptance ADR-0056 deliberately left alone, measured and pinned by State test 116 leg E |
| RISK-016 | **RESOLVED 2026-09-21 (round 53)** — the three `fuzz` corpus seeds were stored in a container no pinned JUCE writes, so they decoded to nothing and the release-blocking fuzz budget started from inputs that reached no parser | Medium (were it reached) | — |

---

## RISK-001 — JUCE version bump
- **Risk:** JUCE is pinned to exactly `9.0.2` (immutable commit `7278278…`, ADR-0054; previously
  `9.0.1` = `e18f7f5…`, ADR-0026; before that `9.0.0` = `f8f8864…`, ADR-0022; before that
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

## RISK-014 — **RESOLVED 2026-09-19** (round 51, ADR-0056): both host-state parser surfaces are bounded

- **What the risk was.** ADR-0055 (0.9.9) put a byte-level boundary in front of `juce::parseXML` for
  `.anamorph` files and its scope ruling left two paths alone: the host session blob
  (`src/PluginProcessor.cpp:2839`, via `getXmlFromBinary`) and the A/B slot payload
  (`:2946-2956`, via `parseXML` on a string the session carried). Round 50 measured both through the
  real `setStateInformation` on `de89b1a` rather than reasoning from the preset path:
  - **Crash, both paths, same thresholds.** SIGSEGV between 2 500 and 3 000 levels of nesting on a
    1 MB stack (a 21 KB chunk) and between 20 000 and 30 000 on 8 MB; at 20 000 the parse *returned*,
    after **12.2 s** on the message thread.
  - **Hang and crash from a `DOCTYPE`, both paths.** A one-level recursive-entity subset of ~150
    bytes had not returned after 60 s on the session path and took 54.1 s on the A/B path; a
    two-level one of ~230 bytes **SIGSEGV'd** on both. `XmlDocument::expandExternalEntity` indexes
    with `ent.indexOf (i + 1, ";")` — the DTD *token* index, not the ampersand it just found.
  - **Unbounded memory, both paths.** Peak RSS tracked input size linearly with no cap: 128 MB of
    payload cost **521 MB** through the session path and **650 MB** through the A/B path.
  - **The A/B payload AMPLIFIED depth rather than inheriting it** — 3 000 levels inside one attribute
    value of a session nested three deep — so a cap on the outer document alone would have refused
    none of it.
- **What was done (ADR-0056, owner instruction 2026-09-19).** Both documents are now bounded before
  the parser, **each on its own**, at **256 KB**, **depth 8** and **no `DOCTYPE`** —
  `anamorph::xmlBoundary` in `src/XmlBoundary.h`, one walk shared with the preset path and
  parameterised by a `DocumentRule` so ADR-0055's rules are the same code rather than a second model
  of the parser. Re-measured with `--risk014-probe` on the fixed head: 3 000 and 30 000 levels, and
  the one-, two- and three-level entity bombs, are each **refused in 0–4 ms on both paths**. What a
  refusal means did not change — `decodeRestore` returns false and touches nothing; a refused payload
  leaves the slot invalid for `abEnsureInit()` to re-seed (ER-STATE-02).
- **CORRECTED 2026-09-19 (round 50) — the arbitrary file read was NOT reachable on either path**, and
  the earlier wording had extrapolated it from the preset measurement.
  `XmlDocument::getFileContents` opens nothing unless an `InputSource` is set
  (`juce_XmlDocument.cpp:182-193`), and both paths reach the parser through
  `parseXML (const String&)`, whose constructor leaves it null (`:38`). Only the `File` overload —
  what the preset path used to call — installs a `FileInputSource`.
- **CORRECTED 2026-09-19 (round 51) — the EVIDENCE for that claim was not sound, and now is.** The
  first probe put a canary in a file and looked for it in the serialized state afterwards, which
  cannot distinguish *"the file was never opened"* from *"the file was opened and read, and the
  document was then rejected"*. State test 116 leg G replaces it with a **filesystem differential
  carrying a positive control**: with a counting `InputSource` installed the parser opens the file
  **once** and the entity resolves to `LEAKED`; through `juce::parseXML (const String&)` the result
  is the unresolved `leak` and is **byte-identical whether the file exists or not**. Identical output
  across present and absent is the property being claimed; the absence of a canary is not.
- **CORRECTED 2026-09-21 (round 53) — the crash was NOT gone from both paths, and this entry said it
  was.** The round-51 closure above is accurate about depth, `DOCTYPE` and size; it was wrong to
  state the outcome as *"the crash … measured gone"*, because the boundary admitted a shape that
  still reached the recursion. `XmlBoundary.h`'s walk answered `skipRanOff` — true for host state —
  for **an opening tag whose quote never closes**, on the premise that such a tag "swallows the rest
  of the text for this scan AND for `XmlDocument`". The premise is positional and the model was
  wrong on one side of it: `juce_XmlDocument.cpp:491-497` treats a quote as a string delimiter only
  after `name =`, so a quote where a NAME is expected errors at `:508-510` and a name with no `=` at
  `:500-503`, **both return the element**, and `errorOccurred` is read once at `:233` *after* parsing
  — so `readChildElements` (`:577-580`) carries on through every element that followed the quote,
  which is the text the scan skipped. Measured: `<r><a "` + 4 000 `<x>` is **12 007 bytes**, was
  admitted, and SIGSEGV'd on a 1 MB stack (and on 8 MB at 40 000); the same through the real framing
  at 12 016 bytes and inside a well-formed `<AnamorphRoot>` prefix at 12 134. The `.anamorph` path
  was never affected — `oneWellFormedDocument` refuses every one of those shapes.
  **Closed in round 53** by refusing a tag that reaches the end of the text with a quote still open.
  It is an implementation repair of `SESSION_COMPATIBILITY_POLICY` rule 7 rather than a new
  narrowing of it: every shape it newly refuses is one `juce::parseXML` answers with null or does
  not answer at all, measured across the name-position, no-`=` and value-position forms, so **the
  set of chunks that RESTORE is unchanged**. Regression coverage is State test 116 leg E2 (the
  verdict, on every platform) and `tests/xml_boundary_differential.cpp` (the contract itself, in
  the `linux` job, with process isolation and a self-proving oracle). **Why nothing caught it:** the
  only generic oracle aimed at this path is the `fuzz` job, whose three corpus seeds could not
  decode at all — see RISK-016.
- **CORRECTED 2026-09-19 (round 52) — "depth 8" was enforced as depth 9, on every surface.** The
  shared walk advanced `depth` on an opening tag and skipped a self-closing one, so a document
  `<a>…<h><leaf/></h>…</a>` nested **nine** elements deep passed a cap of eight, because the closing
  leaf contributed nothing. The limit itself was never in doubt and did not move; what moved is the
  scan, onto the one depth definition already written down — ADR-0055's *"nested exactly two deep"*
  for a preset whose deepest element is the self-closing `PARAM`, this record's depth-3 session, and
  the census walk that returns `d + 1` at every element including childless ones. The overshoot was
  exactly one level (a non-self-closing chain was always counted) and reached preset text, session
  chunk and A/B payload alike, since all three share the walk. **State test 116 leg A2** is the
  regression; restoring the pre-fix lines fails 4 checks.
- **CORRECTED 2026-09-19 (round 52) — round 51's claim that the discredited oracle was gone from the
  probe was itself wrong.** Leg G had been rebuilt, but `--risk014-probe session-system` / `ab-system`
  still ran the canary check. They now call the same `measureExternalEntityAccess()` helper leg G
  asserts on and print the differential with its positive control, and `reportShape` no longer takes
  a canary, so no shape can reach the old oracle.
- **CORRECTED 2026-09-19 (round 50) — the bytes are not only the host's.** The old likelihood rating
  rested on *"the bytes come from the host's own project file rather than from a file the user
  opens"*. In the pinned VST3 SDK, `PresetFile::restoreComponentState` reads the `Comp` chunk and
  calls `component->setState` (`…/public.sdk/source/vst/vstpresetfile.cpp:470-475`), which JUCE
  forwards to `setStateInformation` (`juce_audio_plugin_client_VST3.cpp:2822`): **a `.vstpreset` the
  user picks in the host's browser is a file the user opens and it reaches this parser**, as does an
  `.aupreset` through AU `ClassInfo`. `SERIALIZATION_REGISTRY.md` now records the classification.
- **Compatibility, measured** (`--risk014-probe census`, asserted by State test 116 leg F): every
  session this product has written is nested at most **3** deep and at most **10 629** bytes — this
  build 10 438 B, the v0.9.5 field capture 10 629 B, and the three legacy roots
  `SESSION_COMPATIBILITY_POLICY.md` rule 3 keeps alive 268 / 590 / 740 B at depth 2–3. A slot payload
  is at most **2 051** bytes at depth **2**. None carries a `DOCTYPE`:
  `XmlElement::TextFormat::dtd` defaults empty (`juce_XmlElement.h:207`).
- **Evidence [Verified]:** [ADR-0056](architecture/design-decisions/ADR-0056-the-session-and-a-b-parser-boundary.md)
  (Accepted) carries the full matrix, the shared/not-shared split and the validation; State test 116
  is the regression; `AnamorphStateTests --risk014-probe` remains the opt-in instrument for the
  destructive shapes the suite must never run.
- **What this did NOT close, and it is a residual rather than a gap in the boundary.** ADR-0056's
  ruling named three limits, so host state still accepts shapes a `.anamorph` file refuses: two
  complete documents in one chunk (the first wins), trailing prose or binary, a NUL followed by a
  second complete document (`String::fromUTF8` stops at the first NUL, `juce_String.cpp:132`),
  invalid UTF-8 in an attribute value, and a chunk truncated to half its length. None is a crash, a
  hang or an unbounded read; each is an acceptance question, pinned by State test 116 leg E so that
  changing it later is a decision rather than a drift. **RISK-015** carries them.

## RISK-015 — Host state still takes five shapes a preset file refuses

- **Risk:** ADR-0056 narrowed `setStateInformation` on **size, depth and `DOCTYPE`** — the three
  limits the owner's 2026-09-19 instruction named, and the three that answer a crash, a hang and an
  unbounded read. ADR-0055's *other* rules, which make a `.anamorph` file ONE WELL-FORMED DOCUMENT,
  were deliberately not extended, so a host chunk is still accepted and applied when it is: **two
  complete documents in one chunk** (the first wins, as `XmlDocument::parseDocumentElement` reads one
  element and never looks at the rest); **a document followed by prose, a stray element or raw
  binary**; **a document, one NUL, and a second complete document** (`String::fromUTF8` builds
  through `createFromCharPointer`, whose `while (e < end && ! e.isEmpty())`, `juce_String.cpp:132`,
  stops at the first NUL, so the tail is examined by nobody); **invalid UTF-8 in an attribute
  value**; and **a chunk truncated to half its length**, which is half-applied.
- **Impact:** a corrupt or hand-edited session restores a partial or wrong sound instead of being
  refused whole. Bounded: every one of these is a *correctness* outcome inside the parser-safety
  boundary — none crashes, hangs, or reads unboundedly, and none of them reaches a file.
- **Likelihood (evidence-based):** **Certain as a mechanism, unmeasured in the field.** All five were
  measured through the real `setStateInformation` on `de89b1a` and are re-asserted every build by
  State test 116 leg E, which exists so that a later round changing them does so on purpose.
- **Evidence [Verified]:** [ADR-0056](architecture/design-decisions/ADR-0056-the-session-and-a-b-parser-boundary.md)
  §Consequences and §"What was NOT done"; `docs/architecture/SERIALIZATION_REGISTRY.md`
  §"What host state still accepts".
- **Mitigation:** not taken, and it is a decision rather than an omission: extending the
  single-document rule to host state would narrow serialization acceptance further than the ruling
  covered, which `SESSION_COMPATIBILITY_POLICY.md` rule 1 makes an Architecture Review Gate item in
  its own right. The mechanism is ready — `anamorph::xmlBoundary::DocumentRule::oneWellFormedDocument`
  already exists and is what the preset path asks for — so the change, if it is ever ruled, is one
  enum value at two call sites plus the compatibility case for the truncated and multi-document
  shapes.

## RISK-016 — **RESOLVED 2026-09-21** (round 53): the fuzz corpus reached no parser

- **What the risk was.** `tests/fuzz-corpus/*.bin` are the three seeds libFuzzer starts from, and
  `REPOSITORY_MAP.md` described them as existing "so the fuzzer starts from inputs that already reach
  the parser rather than from noise". All three were stored **zlib-framed** — `56 43 32 21` (the
  correct magic) followed by `78 9c` — but no JUCE this project has ever pinned compresses that
  container: `AudioProcessor::copyXmlToBinary` writes magic, length, plain single-line XML and a NUL
  at 8.0.14, 9.0.0 (`f8f8864`), 9.0.1 (`e18f7f5`) and 9.0.2 (`7278278`) alike, and `getXmlFromBinary`
  is `parseXML (String::fromUTF8 (data + 8, ...))`. A zlib body therefore decoded to nothing:
  `decodeRestore` returned false at its first branch, and no migration, no `repairSerializedValues`
  and no A/B decode was reachable from any seed.
- **Impact.** The `fuzz` job is the only generic oracle aimed at `setStateInformation`, and it is the
  one release-blocking gate whose pass condition is "no sanitizer fired" — which a corpus that
  reaches nothing satisfies perfectly. It is why the RISK-014 residual corrected in round 53 survived
  the round that was looking for it: reaching `<a "` followed by thousands of elements by mutation,
  inside a 90 s budget, from three inputs that do not parse, is not a plausible search.
- **Likelihood:** **certain as a mechanism** — it was the committed state of the corpus, not an edge
  case.
- **What was done.** The three seeds were regenerated through the shipped framing (the five
  statements of `copyXmlToBinary`, so the seed and the wrapper cannot disagree about the container)
  and each now reads back through the wrapper's own expression: 268 / 590 / 740 bytes.
- **What stops it returning, and it is not a comment.** State test 116 **leg F2** asserts on every
  platform on every push that each seed still carries the host-chunk framing AND, fed to the real
  `setStateInformation`, **moves a parameter**. Re-compressing a seed, truncating one, or
  regenerating one through a future JUCE that changes the container fails that leg loudly instead of
  quietly emptying the fuzz budget. Demonstrated in both directions in round 53: green on the
  regenerated corpus, and `width 1.0000 -> 1.0000` with exit 1 when one seed was put back into the
  zlib form.
- **What would re-open it:** a JUCE upgrade that changes the binary-state container (`DEPENDENCY_POLICY`
  rule 2's bit-identity dump covers DSP output, not this), or a seed added by hand rather than
  regenerated. Leg F2 is what turns either into a failure.

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
- **Evidence [Verified]:** src/dsp/AnamorphEngine.cpp:1836 (`soloMonitor.process`, always-on); src/dsp/MultibandWidth.cpp (glide + fade paths);
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
  manifest). The risk **closes when the first release tag is cut** (planned: **v0.9.7** — 0.9.0 through 0.9.6 were each written up but never tagged); until
  then, cite commit SHAs. Historical entries keep SHA evidence permanently.

## RISK-004 — pluginval crash-only retry masking a real crash
- **Risk:** `run-pluginval.sh` retries on a crash to absorb the external X11 flake
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
  **2026-09-06: the macOS half of this risk was never the retry.** macOS has had one attempt since
  2026-08-18, so nothing there could be retried away — but pluginval traps SIGFPE/SIGILL/SIGSEGV/
  SIGBUS/SIGABRT itself on that platform (`kill9WithSomeMercy`, `#if JUCE_MAC`) and exits **9**,
  which the script's "exit <128 is a real validation failure" rule reported as a plug-in defect. A
  crash was therefore masked by *misnaming*, not by retrying. `classify_pass_exit` now reads
  pluginval's own `pluginval received <signal>, exiting immediately` line as well as the code, on
  every host, and `--self-test` proves that decision live.
- **Evidence [Verified]:** scripts/run-pluginval.sh:140-228 (`run_one_pass` and `classify_pass_exit`;
  retry only on a CRASH, cap 3, Linux only);
  scripts/run-pluginval.ps1 (verdict block: crash → immediate failure, retry only on `$null`).
- **Mitigation:** Investigate any repeated crash rather than trusting the pass; keep the cap; a real
  validation failure already fails immediately with no retry. Keep the crash/failure decision in one
  self-tested function — an exit code alone does not carry it.

## RISK-005 — Manual-only audio/visual + host validation
- **Risk:** Audio quality, GUI/vectorscope appearance, and real-DAW host behaviour cannot be verified
  headlessly; a green build + pluginval pass is "ready to audition," not final.
- **Impact:** A sound/visual or host-specific regression can pass CI and reach testers.
- **Likelihood (evidence-based):** Medium — depends on diligence of the manual Level-5 sign-off.
- **Evidence [Verified]:** docs/procedures/TESTING.md ("What cannot be verified headlessly"); `docs/policies/TESTING_POLICY.md` (Level 5);
  `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` (host-matrix item).
- **Mitigation:** Enforce the manual audition + host-matrix line items at release; expand the
  documented host coverage as it is performed.

## RISK-009 — Two parameters' `listenerLock`s nested in opposite orders by a host's re-entrant write
- **Risk:** `juce::AudioProcessorParameter::sendValueChangedMessageToListeners` holds the
  parameter's **own** `listenerLock` for the whole listener loop
  (`juce_audio_processors_headless/processors/juce_AudioProcessorParameter.cpp:111-121`), and
  `beginChangeGesture` does the same for the gesture dispatch (`:82`). A listener that writes a
  **different** parameter from inside that dispatch therefore holds one parameter's lock while
  taking another's. If a host does that for A-then-B on one thread and B-then-A on another — a
  control surface writing back on the message thread while automation writes back on the audio
  thread — the two acquisitions form a cycle and can deadlock. The locks are JUCE's and are taken
  by JUCE around its own dispatch; the plug-in is not a party to the ordering.
- **Impact:** a hang, not a wrong value — and in the worst place, since one of the two threads
  would be the audio thread. Nothing partial is written; the process stops.
- **Likelihood (evidence-based):** **Low.** It requires the HOST to write cross-parameter from
  inside a dispatch, on two threads, in opposite orders, overlapping. **No listener in this
  plug-in creates the nesting at all:** `AnamorphAudioProcessor::parameterValueChanged`
  (`src/PluginProcessor.h:618-621`) is a single relaxed `fetch_add`,
  `ViewGenWatcher::parameterValueChanged` (`src/PluginProcessor.h:823`) the same, and
  `parameterGestureChanged` (`src/PluginProcessor.cpp:1403-1580`) touches two ints — the last
  deliberately, its comment recording that `--d2-stress-probe` once reported this same detector
  for an APVTS/`listenerLock` inversion, closed by **removing** the nesting.
- **How it surfaced:** ThreadSanitizer's deadlock detector, on `AnamorphStateTests` at
  `1c14b9b` — `lock-order-inversion (potential deadlock)`, cycle `M0 => M1 => M0`, both orders
  taken by the **main thread** at different times, so the suite itself cannot deadlock. State
  test 75 leg D supplies one order (hold the edited split, write a neighbour from inside the
  gesture open) and leg G the other (hold a neighbour being spread, write the pin from inside its
  store). The harness's re-entrancy doubles are what create the nesting; they exist precisely to
  stand in for a host that does this.
- **Mitigation:** none available inside this plug-in. Removing the nesting is not ours to do —
  it is the host's write and JUCE's lock. Serialising all parameter writes onto one thread, or
  taking the locks in a fixed global order, is a **threading-model change** and so an
  `ARCHITECTURE_REVIEW_GATE` item, not a fix to slip into a review round. What is done instead:
  the report is kept visible rather than absorbed — `tests/tsan-suppressions.txt` carries ONE
  deadlock entry naming the harness double (`WriteFromInsideAGestureOpen`) and nothing else, so
  an inversion whose stacks contain only production frames still fails the `tsan` job, and the
  canary step proves on every run that data-race detection is untouched. Reopen this risk if a
  host is ever observed writing cross-parameter from inside a dispatch on two threads.
- **ROUND 20 (2026-09-14) — A SECOND, DIFFERENT INVERSION ON THE SAME DETECTOR, and this one does
  NOT have the "harness only" defence. It is ESCALATED as an architecture-review item rather than
  claimed away.** The locks are not two parameters' `listenerLock`s; they are ONE parameter's
  `listenerLock` and the APVTS `valueTreeChanging` lock:
    * one order is pure production plus JUCE — restoring state holds the APVTS lock and, inside it,
      `ParameterAdapter::setNormalisedValue` -> `setValueNotifyingHost` ->
      `sendValueChangedMessageToListeners` takes a parameter's `listenerLock`
      (`juce_AudioProcessorValueTreeState.cpp:413`, `:425`, `:457`);
    * the other is the nested poll this round's review finding is about — `endChangeGesture` holds
      `listenerLock` while it calls the `finalListener`, a host pumps its message loop from that
      callback, the editor's timer runs, and `pollUndoCoalesce` -> `currentStateSet()` ->
      `APVTS::copyState()` takes the APVTS lock.
  **The second edge is reachable in a shipped build** — it needs only a host that pumps inside its
  gesture-end callback, which is exactly the scenario State test 93 reproduces — so unlike RISK-009
  this is not an artefact of a harness double. It is not caused by the round-20 endpoint fix (which
  changes no locking) and is not closed by it; it was SURFACED by that round's test, the first to
  exercise a nested poll at all.
- **ROUND 26 (2026-09-15) — THE ROUND-20 EDGE WAS REACHED BY A DOOR THIS PROJECT ADDED, AND THAT
  DOOR IS CLOSED.** Review finding `src/PluginProcessor.cpp:R651`, *"deferred command flush
  deadlocks"*. The cycle is the round-20 one above, unchanged; what is new is the door. Round 25
  put the BLOCKING poll at the user transaction's outermost `1 -> 0` boundary, and that boundary is
  reachable from inside a parameter listener's dynamic extent, because a host that pumps its
  message loop from a listener callback dispatches whatever UI events are queued — including the
  Add-band click that opens and closes a whole transaction.
  **Reproduced with real threads, deterministically** (State test 100 leg B): with a non-announcing
  holder of `soundReplacement` parked inside `copyStateWithRawValues`, the pumped transaction's
  close took **412.4 ms** on the round-25 tree — it waited until the harness released the holder —
  while the message thread sat in `endChangeGesture` holding mbDrive's `listenerLock`. On the fixed
  tree the same measurement is **0.0 ms** with the holder still parked.
  **Closed by ADR-0036 §29:** `flushDeferredCommands` takes `soundReplacement` with a **try**, never
  a wait, so the message thread can never be the waiting half of the cycle — a proof by
  construction rather than a reachability argument. A refusal consumes nothing (the transaction's
  step still pends, the commands stay queued) and both polls retry it. Mutations M110–M115.
- **AND THE ROUND-20 EDGE ABOVE IS STILL THERE ON THAT SAME DOOR — measured on the FIXED tree.**
  The try closes the `soundReplacement` edge and nothing else. `copyStateWithRawValues` takes
  `soundReplacement` (free, recursively, under the flush's own try) and then **blocks on the APVTS
  `valueTreeChanging` lock** inside `apvts.copyState()`. ThreadSanitizer reports exactly the
  round-20 pair on the round-26 door: `M0 => M1` is the message thread holding a parameter's
  `listenerLock` through `endChangeGesture` and reaching `copyState`; `M1 => M0` is
  `replaceState` holding the APVTS lock and reaching `sendValueChangedMessageToListeners`. Round
  21's timer doors have the identical residual for the identical reason — their try is on
  `soundReplacement` alone too. **So round 26 closes the edge Devin R651 names and leaves the
  round-20 edge exactly where this entry already had it**: escalated, owner-level, a
  threading-model change. `tests/tsan-suppressions.txt` gains one entry naming the harness type
  `PumpedUserInteraction`, with the analysis written out, so the REPORT is quiet and the RISK is
  not.
- **What round 26 does NOT close, and it is the honest half of this entry.** The same cycle stays
  reachable through any OTHER user-action door a pumped click can deliver: an Undo button press
  with no transaction running goes straight to `undo()` -> `pollUndoCoalesce` -> the blocking
  capture, and then to `applyStatePreservingView`'s own blocking acquisition. That surface predates
  round 25 and is not what R651 names. Closing it needs a first-class notion of *"dynamically
  inside a parameter listener dispatch"*, which this codebase does not have — JUCE dispatches to
  the `finalListener` AFTER the plug-in's own listener has returned, so a depth counter kept around
  our callback reads zero at exactly the moment it would need to read one (the same fact round 21
  recorded for the timer doors) — and building one means marking every first-party parameter write
  site. That is a threading-model change and an owner decision, exactly as the round-20 entry
  above says. **This risk therefore stays OPEN.**
- **Disposition: suppressed in the REPORT, recorded as the risk, and referred upward.** A second
  `tests/tsan-suppressions.txt` entry names the harness type (`HostSeat`), so the suite stays
  meaningful and a real host-vs-host inversion on these two locks — with no harness frame in either
  stack — is still reported. Closing the underlying cycle needs a threading-model change, which is
  an `ARCHITECTURE_REVIEW_GATE.md` item and an AI-agent hard stop: **this is an owner decision, not
  an agent's.** Two shapes a reviewer might weigh: keep the poll off the APVTS lock while a gesture
  dispatch is in flight, or defer a re-entrant poll to the next timer tick. Neither is attempted
  here. Severity: **Medium** — a genuine deadlock requires the two orders on two threads, and every
  order observed so far is taken by the message thread alone.
- **ROUND 21 (2026-09-15) — CLOSED as a reachable production deadlock, under the owner's
  instruction that it must not remain an accepted residual. The TSan REPORT remains, and that
  difference is the whole of this entry.**

  **The cycle that was real, and it is not the pair round 20 named.** Round 20 recorded
  `listenerLock` against the APVTS `valueTreeChanging` lock. The review finding this round
  (`src/PluginProcessor.cpp:R1204`) pointed at the same poll, and reconstructing it from the two
  threads showed the lock the poll actually WAITS on is `soundReplacement` — §24's whole-sound
  replacement lock — with the APVTS lock taken *underneath* it by the host thread. Both edges are
  production plus pinned JUCE: a host thread inside an off-message-thread `setStateInformation`
  holds `soundReplacement` across `applySoundTree` and then waits inside `apvts.replaceState` for a
  parameter's `listenerLock`; the message thread reaches the poll from a TIMER, which runs from any
  loop that drains the message queue — including one a host pumps from its gesture-end callback,
  dispatched by JUCE with that same `listenerLock` held.

  **Fixed by ADR-0036 §26:** the two timer entry points never block on `soundReplacement`. Three
  acquisitions were reachable from them, not the one cited line, and all three are gated; the
  user-action doors keep the blocking acquisition they have always had. The round's own regression
  leg G found the third one — `syncCommitted`'s baseline snapshot — after the first attempt gated
  only two and still measured a 4366 ms wait. State test 94 legs F and G build both edges out of
  two real threads and measure the nested poll at **0 ms**; reverting each gate in turn (M84, M85,
  M86) measures ~4.4 s, which is the deadlock with a stopwatch on it.

  **ROUND 20 NAMED THE WRONG LOCK PAIR, AND THE STACKS SAY SO.** Running the suite with the
  suppressions off and reading the four reports, the `HostSeat` one is
  `listenerLock` against the APVTS lock exactly as round 20 described — but BOTH of its orders are
  taken with `soundReplacement` already held, and they were before this round as well:
    * `HostSeat::audioProcessorParameterChangeGestureEnd` -> `pollUndoCoalesceFromTimer` ->
      `pollUndoCoalesceAdopted` -> `currentStateSet` -> `copyStateWithRawValues` -> `copyState`.
      `copyStateWithRawValues` has taken `soundReplacement` since round 17, one line above its
      `copyState`, so the APVTS lock was never reached without it.
    * `undo()` -> `applyUndoEntry` -> `applyStateSet` -> `applyStatePreservingView` ->
      `replaceState` -> `ParameterAdapter::setNormalisedValue` -> `listenerLock`, under
      `applyStatePreservingView`'s own `soundReplacement`. Its production counterpart,
      `applySoundTree` on a host thread, is under the same lock by §24.
  Two threads cannot hold `soundReplacement` at once, so that pair could never close. **What the
  poll actually deadlocked on was `soundReplacement` itself** — the message thread WAITING for it
  while holding a `listenerLock` a host thread's `replaceState` was about to want. That is the pair
  R1204 named and the pair §26 closes. Round 20's escalation pointed at the symptom TSan printed
  rather than at the edge that could hang, and this paragraph corrects it rather than leaving the
  stronger claim standing.

  **THE TSAN REPORT IS STILL THERE, AND IS STILL SUPPRESSED.** `deadlock:HostSeat` matched 3 times
  on the fixed tree. TSan's deadlock detector keeps a PAIRWISE lock-order graph and does not model
  an outer lock that serialises both orders, so `listenerLock -> … -> APVTS` and
  `APVTS -> listenerLock` remain an edge pair in it and remain reported. The suppression therefore
  stays, with its justification rewritten: it is no longer a placeholder for an owner decision, and
  it never named the cycle that could actually hang. Removing the REPORT would mean removing the
  nesting — not reading the parameter tree from inside a gesture dispatch at all — which is a larger
  change with no defect behind it.

  **Residual, stated rather than claimed away.** `PresetManager::saveUser`
  (`src/PresetManager.cpp:1277`) takes `apvts.copyState()` — and so the APVTS lock — WITHOUT
  `soundReplacement`, the only durable reader in the tree that does. It cannot join this cycle: it
  only reads, so it never waits for a `listenerLock`, and it always releases. It is recorded here
  because the rule the paragraphs above rest on — every APVTS acquisition that can happen with a
  `listenerLock` held is under `soundReplacement` — is the rule a future edit would break there
  first.

  Severity of what remains: **Low** — the two-parameter `listenerLock` nesting at the top of this
  entry, which needs a host to write cross-parameter from inside a dispatch on two threads in
  opposite orders, and which no listener in this plug-in creates.
- **ROUND 27 (2026-09-15) — TWO OF THE THREE DOORS ARE MEASURABLY GONE, AND THIS RISK IS STILL
  OPEN. The two statements are one measurement, not a compromise between them.** Review finding
  `src/PluginProcessor.cpp:R1390`, *"timer retry deadlocks state replacement"*, is fixed by
  ADR-0036 §30: `anamorph::param` raises a `thread_local` depth across every parameter dispatch
  this plug-in starts, and both `flushDeferredCommands` and `pollUndoCoalesceFromTimer` refuse to do
  anything at all while that depth is non-zero. `scripts/check-dispatch.py` makes the bracket
  complete rather than merely large, and State test 101 leg J proves the straddle that covers the
  writes JUCE's own attachment makes.
  **The evidence is the suppression file shrinking, which is the opposite of how a risk is usually
  argued away.** Run with suppressions OFF on the round-27 tree, `AnamorphStateTests` raises
  **two** lock-order reports where the round-26 tree raised four. The two that are gone are the two
  the predicate answers: State test 93's `HostSeat` and State test 98's `PumpFromGestureEnd`, both
  of which reach `apvts.copyState()` from `pollUndoCoalesceFromTimer` and both of which pump from a
  gesture the PLUG-IN opened (the editor's attachment, and the imager's `addBandAt`). Their two
  entries are therefore DELETED from `tests/tsan-suppressions.txt` rather than kept — an entry that
  matches nothing widens what a future report can be absorbed by, and deleting it makes a
  regression loud.
- **WHAT SURVIVES, NAMED FROM ITS OWN STACK.** The remaining APVTS-vs-`listenerLock` report is
  State test 100's, and its M0 ⇒ M1 order is
  `endChangeGesture` → `ParameterChangeForwarder` → the host's pump → `mouseDown` → `addBandAt` →
  `~ScopedUserTransaction` → `endUserTransaction` → `flushDeferredCommands` →
  `pollUndoCoalesceAdopted` → `currentStateSet` → `copyStateWithRawValues` → `copyState`. The flush
  did not refuse because **the gesture it is nested in was opened with a raw
  `juce::AudioProcessorParameter::beginChangeGesture`** by the harness — deliberately, because that
  is what a HOST's own gesture looks like, and a host's dispatch raises no depth of ours. So the
  predicate closes every door whose dispatch this plug-in starts and none whose dispatch the host
  starts, which is exactly the boundary `src/ParameterDispatch.h` claims for it and no more.
- **Disposition: RISK-009 remains OPEN.** What is left is the round-20 pair, unchanged in kind:
  `copyStateWithRawValues` blocks on the APVTS `valueTreeChanging` lock while a parameter's
  `listenerLock` is held by a dispatch this plug-in cannot see, and the opposite order is a host
  thread's `setStateInformation` → `applySoundTree` → `replaceState`. Closing it means either
  keeping the poll off the APVTS lock whenever any gesture dispatch is in flight — which needs a
  fact JUCE does not publish — or a threading-model change. Both are
  `ARCHITECTURE_REVIEW_GATE.md` items and an AI-agent hard stop: **this is an owner decision, not
  an agent's.** `deadlock:PumpedUserInteraction` keeps the REPORT quiet and this entry keeps the
  RISK. Severity unchanged: **Medium** for this pair, **Low** for the two-parameter nesting above.
- **ROUND 28 (2026-09-16) — THE COMMAND DOOR IS CLOSED BY CONSTRUCTION, AND THIS RISK IS STILL
  OPEN ON A CYCLE THAT CONTAINS NO ANAMORPH LOCK.** Review finding
  `src/PluginProcessor.cpp:R802-807`, *"direct program commands can deadlock"*, is the door round
  27's bullet named in writing (*"the predicate closes every door whose dispatch this plug-in
  starts and none whose dispatch the host starts"*). It is fixed by ADR-0036 §31, and the fix is
  deliberately **not** a better predicate.
  **What was established first, from the pinned JUCE source, is that a better predicate does not
  exist.** A host's parameter write enters through the same non-virtual `setValueNotifyingHost` the
  plug-in uses (`juce_audio_plugin_client_VST3.cpp:833`, `AU_1.mm:1186`, `VST2:1328`, `LV2:188`;
  AAX calls `sendValueChangedMessageToListeners` at `:994`); the `Listener` signatures carry only an
  index and a value; the flag that would answer is `static thread_local bool
  inParameterChangedCallback` at `juce_audio_plugin_client_VST3.cpp:825`, inside the wrapper
  translation unit; no plug-in callback brackets the host's, because JUCE calls `finalListener`
  LAST and returns; and `juce::MessageManager` exposes no dispatch-depth query at all. So the
  guarantee had to be by construction: `src/StateCommandGate.h` takes ONE `tryEnter` on
  `soundReplacement` at each command's boundary, holds it across the whole body, and QUEUES the
  command when it fails. A command that never waits cannot supply the message thread's edge of the
  cycle, whoever started the dispatch and whether or not anything can tell.
- **THE COMPLETE BLOCKING INVENTORY, because round 27's entry listed the Undo door and not the
  rest.** Eleven acquisitions of `soundReplacement` exist in `src/`. Four were already non-blocking
  (`flushDeferredCommands`, `syncCommitted`'s non-blocking arm, `pollUndoCoalesceFromTimer`,
  `adoptPendingHostState`'s try arm). The seven blocking ones are `applyStatePreservingView`,
  `copyStateWithRawValues`, `applySoundTree` (processor), `adoptPendingHostState`'s blocking arm,
  and `PresetManager`'s `applyDefaults` / `applySoundTree` / the factory branch of `loadAdopted` —
  and **every one of them was reachable from a pumped click** through one of the ten commands
  (`undo`, `redo`, `abSwitchTo`, `abToggle`, `abCopyToOther`, `PresetManager::load` /
  `loadAdopted` / `loadFile` / `step` / `saveUser`), because the only gate was
  `userTransactionDepth`. All ten now pass the admission, so none of the seven is entered by
  waiting. Three further message-thread doors were found and handled in the same sweep:
  `applyAutoGain`'s blocking drain (now admitted), the preset menu's blocking drain in
  `PluginEditor.cpp` (now the non-blocking arm — a menu cannot be deferred, so a refused drain
  costs a possibly-stale tick for the life of one popup), and `pollUndoCoalesce` itself, which was
  the blocking user-action door and is now an admission of its own.
- **AND ONE BLOCKING ACQUISITION OF THE *OTHER* LOCK, which round 27 recorded as a residual and
  round 28 closes.** `PresetManager::writeUserPreset` reaches `apvts.copyState()` — `ScopedLock
  (valueTreeChanging)` — with no `soundReplacement` around it, the only such site in the tree.
  `saveUser`'s admission now holds `soundReplacement` across it, which closes that cycle because
  every thread that can hold `valueTreeChanging` while waiting for a `listenerLock` must take
  `soundReplacement` first (ADR-0036 §31 states that as a rule rather than leaving it an accident
  of the call sites). The cost — a file write under §24's lock — is recorded there.
- **WHAT SURVIVES, AND WHY IT IS NOT REACHABLE FROM ANY LINE THIS PROJECT OWNS.**
  `juce::AudioProcessorValueTreeState` is itself a `private Timer` started at 10 Hz
  (`juce_AudioProcessorValueTreeState.h:119`, `.cpp:274`); its `timerCallback` calls
  `flushParameterValuesToValueTree`, which opens `ScopedLock (valueTreeChanging)` **blocking**, on
  the message thread, from a callback a host's pump delivers like any other message
  (`.cpp:460-462`, `:472-474`). Against a host thread inside `replaceState` — which holds
  `valueTreeChanging` and reaches a parameter's `listenerLock` through `valueTreeRedirected` →
  `updateParameterConnectionsToChildTrees` → `setNewState` → `setDenormalisedValue` →
  `setNormalisedValue` → `setValueNotifyingHost` — that is the same cycle with **no Anamorph lock
  on the waiting side and no Anamorph code on either edge of the wait**. It cannot be converted to
  a try, deferred, or refused from here. Alongside it the entry's original two-parameter
  `listenerLock` nesting is unchanged in kind: it contains no Anamorph lock either.
- **Disposition: RISK-009 remains OPEN**, and the reason is now a different one from round 27's.
  Round 27 left a door this plug-in owned. Round 28 closes every such door by construction and what
  remains is inside JUCE: the APVTS 10 Hz timer's blocking acquisition, and the two-parameter
  nesting a host would have to create. Closing either needs a JUCE change or a threading-model
  change of a different order (taking the message thread out of `replaceState` on the host-thread
  restore path), which is an `ARCHITECTURE_REVIEW_GATE.md` item and an AI-agent hard stop: **an
  owner decision, not an agent's.** `deadlock:PumpedUserInteraction` keeps the REPORT quiet and
  this entry keeps the RISK. Severity: **Low** for both survivors now — each needs the host to do
  something this plug-in cannot make it do, and neither is reached through an Anamorph wait.
- **Round 28b (2026-09-16, re-audited rather than re-asserted).** Two review findings landed against
  the round-28 tree — `R834-835` (deferred commands executed out of order) and `R405-406` (a
  cancelled save's completion closed a newer dialog) — and both are fixed, so the command path was
  walked again for anything Anamorph owns that can WAIT. **Nothing was found, and the inventory is
  the evidence rather than the conclusion.** Every `ScopedLock (soundReplacement)` a command can
  reach — `applyStatePreservingView` (`src/PluginProcessor.cpp:1093`), `copyStateWithRawValues`
  (`:1134`), `applySoundTree` (`:1374`), `PresetManager::applyDefaults`
  (`src/PresetManager.cpp:198`), `PresetManager::applySoundTree` (`:313`) and the factory half of
  `loadAdopted` (`:550`) — runs underneath the gate's own held lock and is a free recursive
  re-entry on the same thread; every drain a command makes is `adoptPendingHostState (false)`. The
  two BLOCKING `adoptPendingHostState()` calls that remain are in `getStateInformation` and
  `setStateInformation`, the host-serialization path rather than a command, and neither fix this
  round adds an acquisition of any kind: one reorders a queue, the other compares two integers.
  **The disposition is therefore unchanged and for the same reason: OPEN on the JUCE-internal
  residual only.** No suppression was added; nothing here is described as an Anamorph defect; the
  three TSan entries are unchanged and each still matches exactly once (`WriteFromInsideAGestureOpen`,
  `PumpedUserInteraction`, `testNoStateCommandWaitsForAReplacement`; ThreadSanitizer exit 0,
  3 917 / 0 under the instrument).
- **Round 35 (2026-09-17) — the same cycle reported at the TIMER door, and MEASURED to a
  disposition.** Review finding `src/PluginProcessor.cpp:R1601-1602`, *"host callback deadlocks timer
  polling"*, walks the chain through `pollUndoCoalesceFromTimer`. **Steps A–D are all true and are
  asserted, not conceded:** a host-started gesture holds the `listenerLock` across the pump;
  `insideDispatch()` reads **zero** there, because it answers for a dispatch this plug-in started;
  the pumped tick reaches `pollUndoCoalesceAdopted`; and that body reaches `apvts.copyState()`.
  **Step E is where it breaks.** State test 113 leg C measures every one of those APVTS acquisitions
  running with `soundReplacement` already held (from a second thread whose `tryEnter` fails, 2 of 2),
  and leg E runs the whole triple — host dispatch, pumped tick, and a concurrent off-message-thread
  `setStateInformation` — with the restore **parked at `soundReplacement`, the parameters untouched**:
  it has not entered `apvts.replaceState` and holds no APVTS lock. Leg D measures the refusal when
  the lock is already taken: microseconds, no APVTS acquisition, nothing consumed. That is ADR-0036
  §31's ordering rule doing the work, and §33 records the verification.
  **Disposition: the reported combination is UNREACHABLE, and the half that makes it so is
  Anamorph-owned** — the ordering rule and the try that rests on it — so it is not dismissed as
  "JUCE owns one of the locks". **RISK-009 itself is unchanged: still OPEN on the JUCE-internal
  residual only** (the APVTS 10 Hz timer's own blocking `valueTreeChanging` acquisition, and a host
  nesting two parameters' listener locks), still Low for both survivors, still with no Anamorph lock
  on the waiting side. No suppression was added; no threading-model change was made. Mutations M220
  (bypass the guard/defer) and M221 (reintroduce the blocking acquisition) are killed by legs B and
  D. Two source comments written before §31 existed, which still described this door as half of an
  open cycle, are corrected.

## RISK-010 — The DSP's multiband snapshot is not a snapshot (ESCALATED as an architecture-review item)
- **Risk:** `PluginParameters::toEngine` builds the per-block DSP view of the multiband layout from
  **ten separate `std::atomic<float>::load()` calls** (`src/PluginParameters.cpp:365-374`), with no
  seqlock, generation counter or coherence guard. The audio thread can therefore observe a band
  count from one instant and a solo word, width or split from another. The GUI is not the only
  writer: host automation writes these parameters from the audio thread through the format wrapper.
- **Impact:** bounded, and the bound is the reason this has been an accepted trade through
  ADR-0041, ADR-0042 and ADR-0044 rather than a defect. `mbBands` is written **last** by every GUI
  topology transaction and read by `toEngine` **before every parameter the count reinterprets**, and
  the loads are `seq_cst` in source order, so a snapshot carrying the NEW count necessarily carries
  the whole of **that** transaction — for a GUI transaction, only the reverse direction, an old
  count under newer values, is reachable. **Narrowed 2026-09-08 (ADR-0046 round), because the
  sentence used to claim more than is proved (and, 2026-09-08 wheel-gesture round, "read
  **first**" corrected to what is actually true — `e.mbEnable` is loaded at
  `src/PluginParameters.cpp:365`, one line ahead of the count, and no topology transaction writes
  `mbEnable`; the solo word, the splits and the widths, which ARE the parameters the count
  reinterprets, are all loaded after it):** the store-order argument covers
  `addBandAt` and `removeBand`, the only writers that order their stores deliberately. It does
  **not** cover a host automation write (which moves one parameter with no transaction around it,
  so there is nothing for it to be incoherent WITH) and it does **not** cover a whole-state
  restore, whose store order is the APVTS's and not this rule's — there a new count CAN be
  published ahead of the values it reinterprets. That third case is why
  the DSP's own repair below is load bearing rather than merely belt-and-braces, and it is part of
  what the escalation is asking to be reviewed. The DSP then repairs what it is given:
  splits clamped to `[20 Hz, 0.45·sr]` and force-ordered `1.1×`, the solo word masked with
  `((1 << bands) - 1)`, the count clamped to `[1, 4]`, every continuous quantity smoothed. The
  result is a legal layout that is briefly not the one the user has — never NaN, never unbounded.
- **Likelihood:** **certain** as a mechanism; it is the shipped reader model, not an edge case.
- **Why it is escalated rather than fixed:** a write-side atomic commit does not help, because the
  READER is what tears — this was measured and is why ADR-0042 rejected that option. Closing it
  means replacing the read: one immutable published layout object, or a seqlock, consumed by the
  DSP. That is a **DSP-parameter-model and threading-model change**, which
  `docs/policies/ARCHITECTURE_REVIEW_GATE.md` makes a hard-stop item requiring human review. It is
  therefore recorded here as the architecture-review item rather than accepted silently inside a
  review round, which is the decision the 2026-09-08 round was asked to make: **accept the trade for
  now AND escalate**, not one or the other.
- **Mitigation until then:** the store order (`mbBands` last) and the DSP's own clamping are load
  bearing and must not be changed casually; ADR-0041 §"why the store order is kept" and ADR-0044
  both depend on them.
- **What should reopen this (added 2026-09-09, because the record had no reopen condition where its
  neighbour RISK-009 does — the gap was found by this round's verify-only audit, not by the
  review):** any change to the **store order** that stops `mbBands` being written last by
  `addBandAt` and `removeBand`; any **new writer** of the multiband set that is not one of those two
  transactions; any weakening or removal of the DSP's own repairs
  (`MultibandWidth.cpp:102-112`, `SoloMonitor.cpp:68-77` and `:85`, and the two
  `setBandCount` clamps at `MultibandWidth.h:56` / `SoloMonitor.h:53`); or a **measured audible
  artefact** from the whole-state-restore case this record already flags as uncovered by the
  store-order argument. Absent one of those, a new round should re-verify this record and move on.
- **Not changed by the 2026-09-09 round, and the reason is worth stating rather than implying.**
  That round fixed three ownership defects in `SpectrumImager` (ADR-0049, ADR-0050, ADR-0051). All
  three are **message-thread writers**; RISK-010 is the **audio-thread reader** in
  `src/PluginParameters.cpp`, which the PR does not touch at all. Fixing a writer cannot narrow a
  tearing window on the reader side, so none of them is evidence about this risk in either
  direction.

## RISK-011 — Undo re-entrancy can split one topology transaction into two undo steps — **RESOLVED (rounds 24 and 25, three doors)**
- **Risk:** `AnamorphAudioProcessor::parameterGestureChanged` counts open gestures and sets
  `pendingGestureCommit` when the count returns to zero (`src/PluginProcessor.cpp:1403-1579`), and
  `pollUndoCoalesce` turns that into an undo entry. A `SpectrumImager` topology transaction is a
  burst of stores, several of which open and close their own gesture (`setBands`, `setSoloMask`,
  `resetParam`), so the open count returns to zero **inside** the burst. A poll that runs there —
  a re-entrant one reached through a listener, or a preset/undo path that polls — records an undo
  step whose state is a layout that existed only mid-transaction and that the user never had.
- **Impact:** an undo history containing a step the user cannot recognise; undoing to it installs a
  half-applied layout (a count without its widths, or a solo word the count no longer reinterprets
  the same way). Not an audio-safety problem — every such layout is still clamped and masked by the
  DSP, as RISK-010 describes — but it is a state-correctness one.
- **Likelihood:** Low as observed (no reported occurrence, and no test in the suite reaches it),
  **structural** as a mechanism: nothing in the current code prevents it.
- **Evidence [Verified]:** `src/PluginProcessor.cpp:1403-1579` (the counter), `:827-834`
  (`pollUndoCoalesce`), `src/gui/SpectrumImager.cpp` `addBandAt` / `removeBand` (the multi-gesture
  bursts). Carried through the v0.9.8 review rounds as residuals **U1–U3** with a deliberate
  no-fix decision; recorded here on 2026-09-08 because a decision carried only in a worklog is a
  decision that gets lost.
- **~~Mitigation until then:~~ SUPERSEDED — the entry is RESOLVED, in two halves, by rounds 24 and
  25.** The paragraph below is kept because its reasoning is why the fix took the shape it did: it
  said a fix means *"either suppressing the poll for the duration of a burst or giving a transaction
  one outer gesture, both of which change the undo model"*. The first of those is what round 24
  built, and it did NOT change the undo model — suppressing the poll turned out to mean SKIPPING it,
  leaving `pendingGestureCommit` standing so the action commits whole a moment later, which is the
  model's own behaviour rather than a new one. The second was never needed.
  ~~none in code. A fix means either suppressing the poll for the duration
  of a burst or giving a transaction one outer gesture, both of which change the undo model and so
  are `ARCHITECTURE_REVIEW_GATE` items in their own right. Deliberately **not** attempted inside a
  GUI review round.~~
- **RESOLVED, 2026-09-15. The mechanism had TWO doors and both are now closed**, each with an
  owner-approved architecture decision recorded in ADR-0008:
  - **The poll (round 24, Devin R1092).** `userTransactionDepth` counts multi-store user actions —
    `addBandAt`, `removeBand`, `resetCrossover`, `commitFreqEditor`, `applyAutoGain` — and
    `pollUndoCoalesceAdopted` skips while it is non-zero, so no poll landing inside a burst can
    commit half of one. Measured before the fix: one Add-band click, one Undo,
    `bands 2 (started 2), solo 0x4 (started 0x2)`. State test 98; mutations M96–M100.
  - **The command (round 25, Devin R1279-1283).** A poll is not the only thing a pumped message loop
    can deliver. Undo, Redo, the A/B switch and Copy, a preset step, load, file load or save are all
    ordinary message-thread commands, and every one of them replaces state or clears
    `pendingGestureCommit` — `undo()` additionally wipes batch ownership, which is what turned the
    interrupted burst's tail into a partial undo step. They are now queued by
    `deferWhileUserTransactionActive` and run at the outermost `1 → 0` transition, AFTER the
    transaction's own step is committed whole. Nothing is dropped. Measured before the fix, State
    test 99 leg B: `bands 2 and solo 0x4 disagree`. Mutations M101–M107.
  - **The DRAIN (round 25, found by writing the coverage rather than reported).** The third door is
    not a command at all: `pollUndoCoalesceFromTimer`'s first line is `adoptPendingHostState`, which
    sits AHEAD of round 24's guard, so a host-restore adoption reached re-entrantly ran
    `adoptRestoreTail` → `syncCommitted()` under the open transaction and wiped the same bookkeeping.
    The burst did not abort by itself because ADR-0036 §12 makes the sound half deliberately skip
    when `soundSetGen` has not moved — so the per-store guards had nothing to see. The non-blocking
    (timer) arm now returns consuming NOTHING while a transaction is open; the cell keeps the restore
    whole and the next door adopts it. Measured before the guard, State test 99 leg I: `bands 3,
    solo 0x4` and `undo step 1 left bands 2 with solo 0x4`. ADR-0036 §28; mutation M108.
- **What would re-open it:** a NEW command that replaces a whole sound/state snapshot, or that
  clears `pendingGestureCommit` / calls `syncCommitted()`, and does not ask
  `deferWhileUserTransactionActive` first — or a NEW non-command path to a whole-state replacement
  that does not test `userTransactionDepth`, which is what the third door was. The enumeration in
  ADR-0008's round-25 command matrix is the list this closure rests on; adding to it without adding
  the guard re-opens the entry. Nothing in the build enforces that today, which is the honest
  residual of this closure — and the third door is the measured proof that the enumeration is the
  weak part: it was written in this same round, and it was one row short.

---

## Adding a risk

Create the next `RISK-NNN` only when a TODO/FIXME, issue, PR discussion, or concrete code limitation
supports it. State the likelihood **basis**, cite evidence with a confidence level, and give a
mitigation. Do not invent risks to fill the template.

## RISK-012 — An undo entry is a whole state, so a host write inside a user's step is undone with it
- **STATUS, 2026-09-13: RESOLVED. Fixed in 0.9.8 by the maintainer-approved amendment to ADR-0008.**
  Round 12 recorded this as an accepted residual; round 13 re-classified it as a confirmed production
  defect against the stated product rule — *host automation is not a user Undo/Redo action* — and
  escalated it, because every correct fix needed per-parameter attribution and that contradicted
  ADR-0008's *"undo/redo stacks of `StateSet` snapshots"* in terms. The maintainer approved that
  amendment on 2026-09-13 and round 14 implemented it. The entry is kept rather than deleted because
  the reasoning across three rounds is the record of why the architecture changed.
- **What the defect was.** An undo entry was a whole `StateSet` snapshot, so the entry pushed beside a
  user's edit necessarily predated any host write that arrived in the commit window, and one Undo took
  that write back with the edit. Its far endpoint was worse: the redo entry was manufactured from the
  LIVE parameters at the moment Undo was pressed (`st.redo.push_back (currentStateSet())`), so any
  automation between the step and the Undo silently became the value Redo restored.
- **What replaced it.** An entry now carries the parameters the user's own batch moved and both of
  their endpoints, and the SAME entry moves between the undo and redo stacks. Attribution comes from
  the change gesture JUCE already reports, read in `parameterGestureChanged` — message-thread-only, so
  **the Thread Model trigger this entry named does not apply to what was built**: the
  `parameterValueChanged` design it named as gated was not the one chosen. The multiband display's two
  unbracketed stores declare themselves through `SpectrumImager::onOwnedWrite`. A preset load and an
  A/B Copy stay whole-state entries. The full statement is ADR-0008's amendment section.
- **Measured, not argued.** State test 86: leg Y asserts the canonical sequence (*A -> B by the user,
  B -> C by the host, Undo gives A, Redo gives B*); leg O asserts that a write in the commit window
  survives the Undo; leg X asserts the same for a write inside a HELD gesture, in both directions;
  leg D2 asserts that a split the scroll pushed aside comes back with it. Mutations M49, M50 and M51
  are each killed by one of them.
- **Round 15 corrected the declaration, and the correction is part of this entry's record.** Reading a
  declared parameter's ending value out of the batch's closing snapshot is right for a coupled store
  made INSIDE a gesture bracket and wrong for one made after the last of them. `resetCrossover` and
  `commitFreqEditor` close the primary split's gesture before calling `spreadSplits`, so a reset's
  pushed neighbours were declared with `before == after` and dropped from the step: one Undo restored
  the reset split and left the neighbours displaced (measured 50 Hz restored, neighbours left at
  249.8/366.6 against the 200/280 the user had). A declaration now carries the value its own store
  installed, and only a store that STOOD makes one. State test 86 legs Z, Z2, Z3 and Z4; mutations
  M55, M56 and M57.
- **One of round 12's three objections was withdrawn as wrong, and the other two are answered.** The
  withdrawn one claimed the fix would break the multiband split drag because `dragCrossoverTo`'s
  pushed neighbours are stored outside any bracket; `SpectrumImager::mouseDown` holds a gesture for
  the whole drag, so they are inside the batch, and `onOwnedWrite` now names them explicitly. The
  depth >= 2 objection is answered by construction: an older entry writes only its own parameters, so
  a host value on any other survives any number of Undo/Redo steps. Redo symmetry is answered by the
  entry moving between the stacks rather than being rebuilt.
- **The residual that remains, and it is not this one.** A host write landing between one of the
  batch's own gestures opening and that same gesture closing is inside the batch by every test the
  coalescer has — ADR-0053 already records that window — and on a parameter the batch owns it can set
  that parameter's after-value. It cannot reach a parameter the batch does not own, and it cannot make
  automation undoable on its own.
- **RECLASSIFIED 2026-09-14 (round 17): that paragraph described a defect, not an accepted residual,
  and it under-described it.** The window was not one gesture wide — the endpoints were
  whole-parameter-list snapshots and the closing one was retaken at EVERY zero-crossing close, so a
  host write between two gestures of one pending batch replaced the FIRST gesture's after-value and
  Redo restored the automation as though the user had produced it. Two further faces followed from
  the same representation: a `before` taken at the batch's open rather than at the parameter's own
  first ownership, and an empty press turning a concurrent host write into an undoable user edit
  (the face round 12 tried to fix with a push gate and withdrew). All three are **FIXED**: endpoints
  are now per parameter, `before` written once at first ownership and `after` only from a value the
  owning gesture or store produced. State test 90 legs A-F; mutations M61-M65. What remains is an
  implementation limit with a stated scope — for a parameter written through JUCE's attachment
  rather than a declaring store, a host write landing after the user's last write to it and before
  that same parameter's own gesture closes is indistinguishable at the close — and it is recorded as
  such in ADR-0008 rather than as a product behaviour.
- **CLOSED 2026-09-14 (round 18): that last sentence was wrong, and the limit it described was the
  same defect on the other control family.** The window was reachable on every knob, slider, value
  box, button and combo — the whole editor outside the multiband display — and it was not narrow:
  a drag writes on `mouseDrag` and closes on a later `mouseUp`, so it spans real message-loop turns.
  Measured on the real Drive knob at `8b136fa`, a host write arriving before the button came up
  became the value Redo restored. The stated reason it could not be closed ("it would need
  `parameterValueChanged`, which is audio-thread-reachable") was also false: JUCE's attachment
  writes the parameter inside the control's own value-changed dispatch, so the write is observable
  on the MESSAGE thread at the control, which is where the editor's `AttachmentWitness` now observes
  it. State test 91 legs A-J; mutations M65-M71. Nothing in ADR-0036 moved.
- **CORRECTED 2026-09-14 (round 19): round 18's "closed in full" was premature, and it is withdrawn.**
  The sentence above claimed there was no remaining window in which host automation can become a
  user's recorded endpoint. There was one, on a third path neither round had looked at: a store the
  multiband display REFUSES. `storeOwned` reads its own write back and declines to declare when a
  controller has replaced the value — and the refusal said so only by declaring nothing, which is
  indistinguishable from an attachment-backed control that has not written yet, so the close fell
  through to its live read and the controller's value became the user's `after`. Measured on the
  real display at `98464db` on three paths (State test 92 legs B, C and E, six failing checks).
  **FIXED**: a refusal is now recorded as a positive fact (`batchEpisodeParam` bit 2) and the close
  skips the parameter. Mutations M72-M75. What remains is the pre-existing narrow in-gesture window
  ADR-0008 has recorded since round 16 — a host write inside the bracket of a store that writes with
  a bare `setValueNotifyingHost` (`resetParam`, `setBands`, `setSoloMask`), where the live read is
  the only endpoint source there has ever been — and RISK-012 stays open against that alone rather
  than being declared closed a third time.
- **CORRECTED AGAIN 2026-09-14 (round 20): the sentence above enumerated ONE remaining window and
  there were two.** It said what remains is the imager's bare-`setValueNotifyingHost` stores. A
  second window was still open and the review found it: an attachment-backed **ComboBox or Button**
  closes its whole gesture inside the control's own listener callback (`setValueAsCompleteGesture`),
  so the close -- and with it `pendingGestureCommit` and the endpoint live read -- runs BEFORE the
  round-18 witness can state anything. The host is entered in that gap as the parameter's
  `finalListener`, and a host that pumps its message loop there gets a nested `pollUndoCoalesce`
  which commits the live read; the witness's later declaration lands in an entry already pushed.
  Measured on the real Algorithm combo at `e9a0353` (State test 93 legs A, E, J). **FIXED**: the
  control states what it is about to ask for before handing over to the attachment, and the close
  prefers that request to its live read. Mutations M76-M82.
- **STATUS AFTER ROUND 20: STILL OPEN, and deliberately NOT reclassified as an accepted residual.**
  What remains is the imager's gesture-bracketed bare stores (`resetParam`, `setBands`,
  `setSoloMask`; `src/gui/SpectrumImager.cpp:899-914` is the shape), which declare no endpoint at
  all, so a host write landing inside their own `setValueNotifyingHost` is still live-read as the
  user's `after`. That violates the stated product rule -- host automation must never become a
  user's endpoint -- so it does not meet the bar for an accepted residual and is recorded as an open
  defect rather than a tolerated one. The round-20 request mechanism generalises to it (such a store
  knows what it is installing), which is the recommended next step; it is **not** done here because
  this round's scope is the attachment path, and widening it is the owner's call.
- **ROUND 21 (2026-09-15): the window round 20 left open is FIXED, and it did not need the request
  mechanism.** Review finding `src/PluginProcessor.cpp:R1078-1081` named exactly the three stores
  the paragraph above names. They now use the read-back shape `storeOwned` has used since round 15 —
  capture `was`, compute what the parameter will render, write, compare — and report through the
  existing `onOwnedWrite` / `onOwnedRefused` callbacks. No parallel attribution system: the mechanism
  that was already correct was simply extended to the three sites that had never been brought under
  it. Their topology-guard branches report a refusal too, because a gesture that opened and closed
  having written nothing leaves the live value equal to whatever a host put there. Measured at
  `16852e1`: State test 94 leg A (a width reset the host answers re-entrantly — Redo landed on the
  host's 1.6400) and leg H2 (the solo mask — Redo landed on the host's 2.0000); both pass with the
  fix and both fail again under mutation M83.
- **STATUS AFTER ROUND 21: STILL OPEN, and still not reclassified.** One window remains and it is
  the one ADR-0008 has always named separately: a gesture that produces **no write at all** — an
  empty press — where the close's live read is the only thing there is, so a host write landing
  inside that press is indistinguishable from the user's own value. It is smaller than any previous
  statement of this risk and it is the last of the enumerated windows, but "smaller" is not
  "closed", and this entry has been declared closed prematurely twice (rounds 18 and 19). It stays
  OPEN until an empty-press leg measures it shut.

- **ROUND 22 (2026-09-15): the empty-press window is CLOSED FOR EVERY GESTURE ANAMORPH'S UI OPENS,
  and what is left is smaller and differently shaped.** The round began by trying the obvious repair
  — delete the close's live read outright, since `noteFirstOwnership` already seeds `after` to
  `before` so an episode that produced nothing would record no step. **Measured, and it is wrong:
  42 assertions across the state suite fail.** A gesture the EDITOR DID NOT OPEN reaches the close in
  the same `ep == 1` state — a host's own generic editor brackets `setValueNotifyingHost` in a
  begin/end pair through the wrapper and declares nothing — and that IS a user edit whose endpoint
  the live value is the only record of. The discriminator cannot live at the close, because at the
  close the two are identical; it lives where the difference exists, which is the editor.
  - `AttachmentWitness` (`src/PluginEditor.h`) now states a REFUSAL at `sliderDragEnded` when nothing
    the control did moved the parameter between its drag start and its drag end. JUCE opens the
    gesture from `SliderParameterAttachment::sliderDragStarted` and closes it from `sliderDragEnded`,
    and the witness's BEFORE hook is registered ahead of JUCE's attachment, so the refusal is recorded
    before `endChangeGesture` runs. That covers every knob, slider and value box: a press that never
    moves the control, and a host push that arrives during one.
  - `SpectrumImager::endGesture` states the same refusal for every gesture opened through it — the
    one place those close. That covers `writeCrossovers`' early exits (topology moved, or the plan
    inside `kSplitMovedPx`), the wheel's split and width branches declining at a rail, and a width
    press inside the 3 px dead zone whose own comment has claimed since ADR-0046 that it makes
    "no automation/undo step". It is stated unconditionally rather than only when nothing was stored,
    because it carries no value and the close already skips a parameter whose store DECLARED an
    endpoint (bit 2) before it consults the refusal bit.
  - `resetCrossover` and `commitFreqEditor` do NOT go through that pair — they bracket their own
    gestures with `beginChangeGesture` / `endChangeGesture` directly — and their `ok = (i < M)` arm
    leaves the gesture open around no store at all when the handle is no longer live (`M` is re-read
    AFTER `beginChangeGesture`, which dispatches, so a host lane can drop Bands inside the open).
    Each therefore states its refusal at its own site, as `resetParam`, `setBands` and `setSoloMask`
    have since round 21. Recorded rather than glossed: this round's first draft put the refusal only
    in `endGesture` and wrote a comment there claiming coverage it did not have.
  - **And that pair's refusal is UNOBSERVABLE on the current head, which is stated rather than
    claimed away.** State test 94 leg L builds the window — the probe drops Bands and automates the
    split from inside the reset's own gesture open — and measures no undo step; mutation **M93**,
    which removes the refusal, measures no undo step either. So something older than round 22 already
    shuts this one, and this round did not isolate which rule. The refusal stays because it makes the
    property local to the two functions rather than dependent on a mechanism two subsystems away, and
    M93 is recorded as a SURVIVOR.
  - None of it is a new mechanism: bit 4 is round 19's refusal, and it has suppressed the close's live
    read since then. What round 22 adds is the missing sentence at the places that never said it.
  - **Evidence.** State test 88 leg O (a knob pressed and released with host automation landing inside
    the press: one gesture bracketed, the host's value live, and NO step recorded — plus a control leg
    where a press that does move the knob is still one undoable step whose Redo restores the user's own
    value), and State test 94 leg K (the same shape on the imager's width line). Mutations **M91** (drop
    the witness's refusal) and **M92** (drop the imager's) each fail exactly their own leg.
- **STATUS AFTER ROUND 22: OPEN, and narrowed to a gesture Anamorph's UI never opened.** What remains
  is a HOST-OPENED, HOST-EMPTY gesture: the host brackets a change gesture through the wrapper, writes
  nothing inside it, and its own automation moves that parameter during the bracket. The close's live
  read then attributes the automation to that gesture. It is not reachable from this plug-in's editor,
  and it cannot be told apart at the close without a threading-model change (`ARCHITECTURE_REVIEW_GATE`)
  — the wrapper's gesture and the wrapper's automation arrive on the same thread through the same API,
  and the 42 failing assertions above are what the plug-in would have to give up to refuse them both.
  It stays OPEN rather than being reclassified: this entry has been declared closed prematurely twice
  (rounds 18 and 19), and a residual is accepted by the owner, not by the agent that narrowed it.
- **ROUND 23 (2026-09-15): THE RESIDUAL IS DISPOSED OF DEFINITIVELY, AND ROUND 22'S STATEMENT OF IT
  WAS WRONG IN BOTH DIRECTIONS.** Round 22 said the remaining class was "a host-opened, host-empty
  gesture" and that it "cannot be told apart at the close without a threading-model change". Both
  halves are corrected by measurement.
  - **A HOST CANNOT OPEN A GESTURE ON THIS PLUG-IN'S PARAMETERS AT ALL.** Measured across the pinned
    JUCE tree at this head: `grep -rnE '(\.|->)(begin|end)ChangeGesture'
    build/_deps/juce-src/modules/juce_audio_plugin_client/` returns **zero**. All 14 "ChangeGesture"
    hits in that directory are the OUTBOUND `audioProcessorParameterChangeGestureBegin/End`
    overrides -- the plug-in telling the host -- across VST3, AU, AUv3, AAX, LV2, VST2, Standalone
    and Unity. LV2 discards a host touch explicitly: `void gesture (LV2_URID, bool) const noexcept {}`,
    with the comment *"The host probably shouldn't send us 'touched' messages."* A host write arrives
    as a VALUE, never as a bracket. **The class round 22 left open is empty in every shipped format.**
  - **AND WHAT WAS ACTUALLY REACHING THE LIVE READ WAS OURS.** Enumerating every gesture opener in
    the tree rather than reasoning about hosts turned up two first-party paths that opened a change
    gesture and declared nothing, so the close's live read decided their endpoint:
    `AnamorphAudioProcessor::applyAutoGain` -- the editor's **Apply Gain** button, the last bare
    bracket in the tree and the one round 21 never reached because it does not live in the imager --
    and `Knob`'s Alt-click and double-click resets, whose ADR-0052 guard `resetWouldMove()` asks the
    SLIDER while the parameter can already be sitting on the default (a parameter written without
    notifying never reaches `ParameterAttachment`; an off-message-thread write reaches it only
    through `triggerAsyncUpdate`). Both are fixed at the site in round 23 by the same sentence every
    other store says -- declare (bit 2) or refuse (bit 4). Measured before the fix, State test 96
    leg A: `Undo -> 6.0000, Redo -> -11.5000`, the host's re-entrant answer standing as the user's
    Redo destination.
  - **THE COMPLETE ATTRIBUTION MATRIX**, every class, as the code stands after round 23:

    | # | Gesture class | Who opens it | What declares the endpoint | User-produced endpoint provable? |
    |---|---|---|---|---|
    | 1 | slider / value-box drag | JUCE attachment (`sliderDragStarted`) | `AttachmentWitness` after-hook, bit 2 | **yes** |
    | 2 | slider / value-box press that moves nothing | JUCE attachment | `notePressEnded`, bit 4 (round 22) | n/a -- nothing was produced |
    | 3 | ComboBox / Button | attachment's `setValueAsCompleteGesture` | the round-20 `attachRequest` | **yes** |
    | 4 | imager drag, wheel branch, width press | `SpectrumImager::beginGesture` | `storeOwned` bit 2, else `endGesture`'s bit 4 (round 22) | **yes** / nothing |
    | 5 | `resetParam`, `setBands`, `setSoloMask` | their own bracket | bit 2 or bit 4 at the site (round 21) | **yes** / nothing |
    | 6 | `resetCrossover`, `commitFreqEditor` | their own bracket | bit 2, else unconditional bit 4 (round 22) | **yes** / nothing |
    | 7 | `Knob` Alt-click / double-click reset | `Knob::mouseDown` / `mouseDoubleClick` | bit 2 if the parameter moved, else bit 4 (**round 23**) | **yes** / nothing |
    | 8 | Apply Gain (`applyAutoGain`) | its own bracket | read-back: bit 2 if the store stood, bit 4 if not (**round 23**) | **yes** / nothing |
    | 9 | generic HOST-editor gesture | **nobody -- no wrapper opens one** | -- | class is empty |
    | 10 | host-only gesture | **nobody** | -- | class is empty |
    | 11 | refused write | -- | bit 4 | correctly none |
    | 12 | no-op write | -- | every wrapper suppresses the callback before it dispatches | correctly none |

  - **WOULD SUCH A BRACKET PRODUCE A STEP IF IT COULD OCCUR? YES -- nothing blocks it.** This was
    traced through `pollUndoCoalesceAdopted` to the push rather than assumed: with `ep == 1` the
    close writes the live value into `batchCloseValue`, the rendered endpoints differ, `edits` is
    non-empty and the entry is pushed. There is no older invariant standing in the way. That matters
    for the harness, which CAN produce the bracket and does so in 42 assertions.
  - **DISPOSITION: the defect class is CLOSED for every shipped format; the live read is KEPT and is
    now harness-only.** After rows 1-8 above, no path a user can drive reaches the `ep == 1` arm, and
    rows 9-10 cannot occur. The read at the batch close therefore has no shipped consumer. It is not
    deleted in this round because deleting it is a decision about what the TEST HARNESS should encode
    -- 42 assertions bracket a bare `setValueNotifyingHost` to stand in for a user edit, a shape no
    wrapper produces -- and that is a separate change from the attribution fixes, which would confound
    the evidence for both if made together. **Recommended, and the owner's to take:** replace those 42
    brackets with a helper that also declares ownership, then delete the arm. Until then this entry
    stays OPEN against the harness shape alone, with no shipped-format exposure.
## RISK-013 — The foreign-write test counts raw parameter stores where everything else asks the rendered value
- **STATUS, 2026-09-13: FORMALLY ACCEPTED RESIDUAL.** Correct and one-directional, and the same item
  the review has also raised as "inaudible writes split scrolls" — not two findings.
- **Risk:** the poll's foreign test is `foreignSinceEdge || (snapGen != gestureEdgeGen)`, and both
  terms are differences of `soundParamGen`, which `parameterValueChanged` bumps for EVERY store (JUCE
  notifies its listeners unconditionally). `foreignSinceEdge` itself is set at a batch's first gesture
  open when the counter moved since the last edge. Every other "did the sound change" test asks
  `soundSignature()`, which signs the RENDERED value — `convertTo0to1 (convertFrom0to1 (v))`, snapped
  to the parameter's own grid (`src/PluginParameters.h`). Three write classes therefore move the
  counter and not the signature: inside one step of any of the 15 host-automatable discrete
  parameters (whose `getValue()` keeps the exact normalised value on purpose); inside one interval of
  an interval-snapped float (29 of the 33 preset-carried parameters); and below the signature's own
  1e-5 quantum on the four log-mapped frequency ranges, which have no interval at all (worst case
  1.38 Hz at 20 kHz).
- **THE TWO HALVES DO NOT DISAGREE — they answer different questions**, and the entry said otherwise
  until 2026-09-13. The non-gesture fold asks *"has the rendered sound changed?"*, which is the right
  question for whether there is anything to record. The foreign test asks *"did somebody else store
  into the batch I am about to commit?"*, and the quantity an entry stores is the RAW value
  (`p->getValue()`), so the store is the right test for it. A rendered foreign test would let an
  off-grid host write merge into a user's scroll, which is the error ADR-0053 §5.3 exists to prevent.
- **Impact:** a scroll chain is ended by a write that changes nothing audible, so each further notch
  becomes its own undo step while such a lane is moving. `foreign` has exactly two consumers — the
  `extend` test and the chain name — and neither changes what an entry CONTAINS, so the error is
  one-directional: **extra undo steps only. No lost edit, no wrong value, and never automation merged
  into a user's step.** One second-order cost: extra steps reach the 128-entry cap sooner, so deep
  history is evicted earlier than a coalesced chain would evict it.
- **Bound:** the write has to land in a gesture-edge gap that contains no 24 Hz poll (a poll otherwise
  absorbs it at the tail), so the ceiling is one chain break per poll period, and only while the user
  is scrolling AND a lane is writing a non-view parameter. With no scroll in flight it has no effect
  at all. Audio-thread-reachable on VST3, where the wrapper compares the host's request against the
  already-snapped `getValue()` — so an off-grid request is never "equal" and notifies every time.
  Not measured in a real host.
- **Round 13 (2026-09-13) — unchanged in kind, marginally more frequent.** The R977-978 ordering fix
  (`fresh`, then `snapGen`, then `foreign`) changed WHEN the counter is read, not WHAT it counts.
  Because `gestureEdgeGen <= gen <= snapGen`, the new predicate is a superset of round 12's: writes
  landing while `soundSignature()` is being built used to be absorbed into both the baseline and the
  edge and counted nowhere, and are now counted. Slightly more often, in the safe direction.
- **Round 14 (2026-09-13) — one face of it CLOSED.** A rendered-preserving host write is never folded
  into `committed` (the fold is gated on `sig != committedSig`), so the baseline stayed at the
  pre-write raw value and the write was swept into whatever step was pushed next. Under ADR-0008 as
  amended a step contains only what the batch declared its own, so an unfolded write on a parameter
  the user did not touch is in no step at all. What remains is purely the granularity cost above.
- **Why the granularity half is not fixed — cost, not impossibility.** Making the two agree means
  comparing each store against a per-parameter cache of the last RENDERED value inside
  `parameterValueChanged`, which the header records as reachable from the AUDIO THREAD. The arithmetic
  is allowed there and the callback is lock-free, so the blockers are architectural: the cache is new
  state written from two threads — a **Thread Model change** under `ARCHITECTURE_REVIEW_GATE.md` — and
  it redefines a cross-thread counter that `PresetManager::isDirty` and the poll's own skip key their
  caches on, where an implementation that MISSES a rendered change leaves a stale dirty-star. Not a
  wheel fix, and wider than the review that found it.
- **Mitigation:** recorded and measured. State test 86 leg W drives a sub-step write on a choice
  parameter and asserts that the rendered value really did not move; the split it causes is **printed
  when it occurs, not asserted**, so the leg stays a measurement and does not lock the current
  granularity in as correct. Leg W reaches only the gesture-open half (`foreignSinceEdge`); the
  `snapGen != gestureEdgeGen` half is uncovered for an inaudible write.
- **Evidence [Verified]:** `src/PluginProcessor.cpp` (`pollUndoCoalesceAdopted`'s foreign test, the
  `extend` gate, the chain name and the non-gesture fold); `src/PluginProcessor.h`
  (`parameterValueChanged`); `src/PluginParameters.h` (`normalisedAsRendered`, the grid note);
  `src/PluginParameters.cpp` (the raw-value discrete parameters). Measured by State test 86 leg W.

## RISK-006 — Undeclared licensing (no LICENSE, no approved EULA, JUCE tier unchosen)
- **Risk:** The repository root has **no `LICENSE` file** and neither installer presents an
  end-user agreement, so the terms under which Anamorph's own source and binaries are offered are
  undeclared. `EULA.md` is an **unapproved draft** (not in force, not shipped) and does not
  change that. The stated product model (owner, 2026-07-26) is **closed-source commercial**; JUCE 9
  modules are dual-licensed **AGPLv3 or commercial**, and a closed-source distribution cannot use
  the AGPLv3 arm — the commercial JUCE tier must be in place before commercial distribution. A
  third strand —
  the Steinberg VST 3 trademark/distribution review — is separate again (the SDK *code* is MIT in
  JUCE 9.0.2; the VST name and plug-in distribution terms are not covered by that grant).
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
- **RESOLVED 2026-09-03 — decision D-2, implemented as ADR-0036 (the 0.9.7 change set).** The
  register entry below is kept in full as the measured record. What closed it: every piece of
  program metadata this entry names — `internal.restoreState`'s tree, `abSlot`/`abActive`/`abUndo`,
  `presets.setMeta`, `syncCommitted` — is now **message-thread state** that only
  the message thread writes and reads directly. An off-message-thread `setStateInformation` applies
  the sound (the APVTS, JUCE-locked) and the oversampling atomic on its own thread and hands the
  DECODED metadata tail to the message thread through one `std::atomic<T*>::exchange` cell (the D-1
  shape with a payload), adopted by the processor's 20 Hz timer and at the top of every message-thread
  entry point that mutates program state; an off-message-thread `getStateInformation` reads an
  immutable snapshot the message thread republishes after every mutation, or — while a restore that
  side handed over is still unadopted — the view it built from that restore. No mutex, no
  `callAsync`, no wait; the audio thread and the message-thread path are unchanged. The four members
  measured below no longer have a second thread touching them; the `juce::String` exchange has no
  cross-thread reader left. **Measured:** `--state-thread-probe`, `--state-prepare-race-probe`,
  `--reprepare-race-probe` and the new `--d2-stress-probe` (host restore + save, off-thread prepare,
  the audio thread and an editor-shaped message thread, all at once) under ThreadSanitizer, repeated
  — silent after, against a report in every pre-change run; State tests 37–41 pin the contract
  deterministically and the `tsan` CI lane keeps it (`docs/procedures/TESTING.md`;
  `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §D-2). The one behaviour nuance is
  recorded in ADR-0036 §Consequences: on the off-message-thread path the metadata tail becomes visible
  to the editor within one timer period (≤ 50 ms) rather than instantly, and a user action landing
  inside that window is ordered after the restore.
- **Risk (as recorded, now closed):** `getStateInformation`/`setStateInformation` mutate non-atomic message-thread-read
  state with no lock or marshalling — `internal.restoreState`, `abSlot`/`abActive`/`abUndo`,
  `presets.setMeta`, `syncCommitted` (src/PluginProcessor.cpp:3085-3184 read
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
  (`setStateInformation`, `src/PluginProcessor.cpp:3085`) and with the same wording plus one added
  sentence — "the documented macOS AU race remains open" — which is this entry's own Likelihood
  bullet restated, not new evidence. Two things were checked rather than assumed. First, the
  concurrency surface has not moved: `src/PluginProcessor.cpp` and `src/PluginProcessor.h` are
  unchanged since round 16, so the code the finding names is byte-identical to what round 20
  measured. Second, the probes were re-run under ThreadSanitizer against the current build:
  `--state-thread-probe` and `--state-prepare-race-probe` each report **the same four races and no
  others**, and `--reprepare-race-probe` is **silent**, so ER-STATE-19/D-1 also remains closed. Each
  report maps one-to-one onto a row already recorded above — `abActive`, written at
  `src/PluginProcessor.cpp:2611`, against `canUndo()`; the `abUndo` vector's internals twice, via
  `UndoStacks::operator=` (`src/PluginProcessor.h:733`) against the reader's iteration; and the
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
