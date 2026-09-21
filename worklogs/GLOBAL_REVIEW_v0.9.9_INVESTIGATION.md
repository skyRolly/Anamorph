# Global review — v0.9.9, `main` @ `7185cad`

**Round:** investigation-only, read-only. No product file was modified.
**Baseline:** `7185cad0d5db878546d650ebb892178e6cefbab2` (merge of PR #151). Verified equal to `origin/main`
before and after the review. A first pass ran against the previous `main` (`4d0471d`); when `main`
advanced mid-round every measurement was re-taken and the fleet restarted, because `src/` and `tests/`
both changed.
**Method:** 18 subsystem finders, each followed by an adversarial verifier applying three lenses
(refute / intent / coverage), then a completeness critic, a cross-subsystem interaction analyst and a
healthy-area auditor, then a second targeted round on the gaps the critic named. 49 agents, 0 errors.
**Candidate findings: 95** — 76 in round 1 (10 refuted, 66 survived) and 19 in round 2 (7 refuted,
12 survived), so **78 survived in total**. The original wording of this line ("76 candidate findings;
10 refuted, 66 survived; round 2 added 12") gave round 1's candidate count as if it were the whole
round's, and never said what §2 does with the 78. **§2 is a prioritised SELECTION, not a partition:**
41 of the 78 are carried into F1–F16, 7 more are referenced in its prose without being presented as
findings, and **30 are not carried at all**. Appendix A reconciles every one of the 78 by id, so the
completeness claim is auditable rather than implied. Findings marked **[verified first-hand]** were
re-derived by the author directly from the code, from JUCE's own source at the pinned commit, or by a
compiled probe; everything else is finder + three-lens verifier evidence and is labelled with its
confidence — see §7 for what "confidence" is now separated into.

---

## 1. Executive assessment

`main` is in good condition and is held there by unusually strong machinery. Everything that can be
mechanically checked is green on this commit, measured rather than assumed:

| Gate | Result on `7185cad` |
|---|---|
| `tests/dsp_tests.cpp` | 53 tests, **396 checks, 0 failures** |
| `tests/state_tests.cpp` | 113 tests, **4632 checks, 0 failures** |
| `--band-move-adopt-probe 3000`, `--solo-alias-probe 300` | zero, exit 0 |
| `check-realtime` / `check-dispatch` / `check-portability` | 50 / 50 / 60 files, 0 violations |
| `check-docs` / `check-citations` | 145 files clean / 531 anchors unmoved |

The build configures and compiles clean against the pinned JUCE 9.0.2 commit with GCC 13 and the
ADR-0031 ISA baseline on.

Three conclusions matter more than the rest.

**First, the newest code carries a confirmed crash.** The round that produced this HEAD closed
RISK-014 by putting a hand-written pre-parse scan (`src/XmlBoundary.h`) in front of `juce::parseXML` on
both host-state surfaces. The scan is right about almost everything, and it is wrong about one of the
four cases its own comment enumerates: an opening tag with an unterminated quote. The scan treats such
a tag as swallowing the rest of the text; JUCE does not, and keeps recursing. A **12 KB** chunk walks
straight through the boundary and takes a SIGSEGV. This is reproduced end to end below. It matters
doubly because `docs/FUTURE_RISKS.md` now records the crash as "measured gone on both paths", which
actively directs the next reviewer away from the one place it still lives.

**Second, the dominant systemic weakness is not any single defect but a recurring shape:** a
correctness invariant held by a hand-maintained list that nothing mechanically checks. The interaction
analyst found six independent instances — the five program-state jumps each spelling out their own
bookkeeping, one discrete-field list answering two different questions, `abMatchGain` invalidation,
`gestureActionDepth` consulted by one of four writers, one module-reset list serving two recovery
paths, and the adopted-not-audited citation convention. The repository already knows this pattern is
its weak point: the RISK-011 closure says of itself *"Nothing in the build enforces that today"* and
records that the enumeration it rests on was one row short when written. Every mechanical gate this
project has was cheap and has paid for itself; the gap is that the newest invariants have none.

**Third, the host contract outside `processBlock` is under-implemented.** `AudioProcessor::reset()` is
not overridden at all, so `AnamorphEngine::reset()` is unreachable from a host flush request, and
`getTailLengthSeconds()` returns a hard-coded `0.1` against a chain that rings longer. These are small,
local, and audible.

Nothing found here suggests the architecture is wrong. The DSP core, the parameter model, the state
machinery and the CI topology are all sound and well protected. The work this review points at is
completing what the last two rounds started and giving the newest invariants the same mechanical
backing the older ones have.

---

## 2. Findings

### F1 — The ADR-0056 host-state boundary admits a document that crashes the parser

> **RESOLVED 2026-09-21 (round 53).** Refused at the boundary; State test 116 leg E2 and
> `tests/xml_boundary_differential.cpp` carry the regression. ADR-0056 §"Correction,
> 2026-09-21", RISK-014's round-53 correction and `SESSION_COMPATIBILITY_POLICY` rule 7 record
> it. Kept here in full because the road map below is derived from it.

- **Classification:** confirmed bug. **Severity:** high. **Confidence:** high — **[verified first-hand,
  reproduced]**
- **Subsystems:** `XmlBoundary` · session restore (`decodeRestore`) · A/B slot decode · host lifecycle
- **Evidence.** `src/XmlBoundary.h:131` sets `skipRanOff = ! oneDocument` — i.e. **true** for host
  state — and `:233` returns it when an opening tag's walk never reaches `>`. The justification at
  `:124-130` is that an unterminated `<!--`, `<![CDATA[`, `<?` *or opening tag* "swallows the rest of
  the text for this scan AND for `XmlDocument`, which runs out of data and reports an error without
  recursing". For the opening tag that premise is false, and the reason is positional. In the pinned
  JUCE 9.0.2, `modules/juce_core/xml/juce_XmlDocument.cpp:491-497` treats a quote as a string
  delimiter **only** after `name =`. A quote in attribute-*name* position falls to `:508-510`
  (`setLastError ("illegal character found in ...", false)` then `break`), and a name followed by a
  quote with no `=` to `:500-503` (`setLastError ("expected '=' after attribute ...", false); return
  node;`). Both **return the element**. `errorOccurred` is consulted exactly once, at `:233`, *after*
  parsing finishes — meanwhile the parent's `readChildElements` (`:577-580`,
  `if (auto* n = readNextElement (true)) childAppender.append (n); else break;`) carries on and
  recurses into every following element.
- **Reproduction** (`scratchpad/quote_repro.cpp`, the shipped `anamorph::xmlBoundary::textIsAdmissible`
  linked against the pinned JUCE, parse run in a forked child on a `pthread` with an explicit stack
  size under an alarm):

  | shape (N = 4000 trailing `<x>`) | size | host rule | preset rule | parser, 1 MB stack |
  |---|---|---|---|---|
  | `<r>` + N·`<x>` *(control)* | 12 003 B | refuses | refuses | CRASHED |
  | `<r><a "` + N·`<x>` | 12 007 B | **ADMITS** | refuses | **CRASHED** |
  | `<r><a '` + N·`<x>` | 12 007 B | **ADMITS** | refuses | **CRASHED** |
  | `<r><a b"` + N·`<x>` | 12 008 B | **ADMITS** | refuses | **CRASHED** |
  | `<r><a b="` + N·`<x>` | 12 009 B | ADMITS | refuses | returned |
  | `<r><!--` / `<![CDATA[` / `<?` + N·`<x>` | ~12 007 B | ADMITS | refuses | returned |
  | `<AnamorphRoot>…<a "` + N·`<x>` *(realistic)* | 12 134 B | **ADMITS** | refuses | **CRASHED** |
  | through the chunk framing (magic + length + text + NUL) | 12 016 B | `hostChunkIsAdmissible` **ADMITS** | — | **CRASHED** |

  At 8 MB of stack the same shapes crash at N = 40 000 (120 KB — still inside the 256 KB cap). The
  premise holds for the three constructs it names besides the tag, and for a quote in *value*
  position (`readQuotedString` genuinely consumes to the end). The `.anamorph` path is **unaffected**:
  under `oneWellFormedDocument` every shape above is refused. The hole is specific to the rule
  ADR-0056 added.
- **Why it matters.** ADR-0056 exists for exactly one measured failure — unbounded mutual recursion in
  `readNextElement`/`readChildElements` — and a 12 KB input walks through both surfaces it bounds. A
  DAW opening a corrupted, synced or hand-edited project dies with the host process, which is the
  outcome the 0.9.9 changelog entry promises is fixed. `docs/FUTURE_RISKS.md:111` states RISK-014 is
  resolved with "the crash, the hang and the unbounded read ... measured gone on both paths"; for the
  malformed-tag case that is now an incorrect closure claim.
- **Regression / failure characteristics.** Not a regression — present since the boundary landed. It
  regresses trivially in future because nothing mechanically ties the hand-written walk to the
  parser's behaviour. Detection is poor: every gate is green and the shape is in no fixture. Diagnosis
  is worse: the failure is a stack overflow with thousands of identical frames, and the boundary's own
  source tells a maintainer this case is safe.
- **Current protection.** None. State test 116's legs cover self-closing depth (A2), well-formed text
  at depth 8/9, a DOCTYPE, the size cap, two documents, trailing prose, a truncated chunk and the four
  fixtures. Grepping the suites for `unterminated`, `skipRanOff` or `textIsAdmissible` finds no
  boundary-vs-parser agreement test. All six mutants recorded in ADR-0056 weaken a *guard*; none tests
  the walk's agreement with the parser on malformed input. The `fuzz` job is the only generic oracle
  and cannot reach this shape (see F5).
- **Shape of the fix** (not applied): `if (! closed)` is reachable only with a quote still open or at a
  bare end-of-text tag, so refusing when a quote is open — or applying the parser's own
  value-position rule — closes it without touching legitimate documents. Every writer-produced shape
  tested (apostrophe in a preset name, escaped A/B payload, declaration at offset zero) is still
  admitted.

> **Correction to my own earlier result.** An initial random sweep I ran reported "no false negative"
> over 12 000 generated documents. That negative was too narrow, not wrong: the generator placed
> unterminated quotes only in attribute-*value* position, which is the safe case. The name-position
> case was found by the fleet's `xml-boundary` finder and then reproduced above.

### F2 — `AudioProcessor::reset()` is not overridden, so a host flush request reaches nothing

- **Classification:** confirmed bug. **Severity:** medium. **Confidence:** high — **[verified
  first-hand]**
- **Subsystems:** host contract · engine state · latency rings · transport
- **Evidence.** `src/PluginProcessor.h` declares no `reset()` override (`grep 'void reset'` finds only
  `resetBatchOwnership`). `grep -rn 'engine\.reset' src/` returns **nothing** — every caller of
  `AnamorphEngine::reset()` is in `tests/`. Internally it is reached only from
  `AnamorphEngine::prepare()` (`src/dsp/AnamorphEngine.cpp:145`). So the only path that flushes engine
  state is a full re-prepare.
- **Why it matters.** VST3 `setProcessing(false)` and AU `Reset` are precisely the "flush your buffers,
  keep your setup" request, and they do not re-prepare. Stale delay-line, oversampler-IIR and
  dry-ring contents therefore survive a transport stop and are heard at the next start, together with
  any in-flight duck.
- **Regression / failure characteristics.** Static — an absent override cannot regress further. Hard to
  attribute in the field: the symptom is a short artefact after transport stop in some hosts and not
  others.
- **Current protection.** None. Two finders reached it independently (`rt-audio-path`, `latency`).
- **Unresolved:** how often shipped hosts call it. The wrapper call sites are proven; host behaviour on
  transport stop is not, and cannot be from this repository.

### F3 — `getTailLengthSeconds()` under-reports the chain's ring

- **Classification:** confirmed bug. **Severity:** medium. **Confidence:** high for the constant
  **[verified first-hand]**, medium for the 0.18 s figure (finder measurement, not re-run here)
- **Evidence.** `src/PluginProcessor.h:44` — `double getTailLengthSeconds() const override { return 0.1; }`,
  a hard-coded constant. The finder measured the LR4 crossovers ringing to −60 dBFS for up to 0.18 s at
  reachable settings.
- **Why it matters.** A host that truncates the tail on bounce cuts the end of the render. Same family
  as F2: host-contract members other than `processBlock` were written once and never revisited.
- **Current protection.** None; no test asserts the reported tail against the measured one.

### F4 — `writeUserPreset` reports success for a write that failed or was truncated

- **Classification:** confirmed bug. **Severity:** high impact / low likelihood → medium.
  **Confidence:** high — **[verified first-hand in JUCE's source]**
- **Subsystems:** presets · file I/O · user data
- **Evidence.** `src/PresetManager.cpp:1211` —
  `if (xml == nullptr || ! file.replaceWithText (xml->toString())) return false;`. In the pinned JUCE,
  `modules/juce_core/files/juce_File.cpp:798-803`:
  ```
  TemporaryFile tempFile (*this, TemporaryFile::useHiddenFile);
  tempFile.getFile().appendText (textToWrite, asUnicode, writeHeaderBytes, lineFeed);
  return tempFile.overwriteTargetFileWithTemporary();
  ```
  `appendText` returns `bool` (`:788-796`, forwarding `FileOutputStream::writeText`) and its result is
  **discarded**. On a full disk or over quota the temp file is short or empty, and
  `overwriteTargetFileWithTemporary()` then moves it over the user's existing preset and returns true.
- **Why it matters.** Silent destruction of a user file, reported as success: the dialog closes, the
  tick moves, the baseline is re-computed from the tree that was *supposed* to be written, and the
  previously-good preset is gone.
- **Regression / failure characteristics.** Latent; needs a full disk, a quota, or a failing network
  volume. Diagnosis is hard because every in-process signal says the save worked.
- **Current protection.** None — no test writes a preset to a failing destination.
- **Shape of the fix:** write through a stream and check, or assert the temp file's size before the
  overwrite.

### F5 — The fuzz corpus cannot decode under any JUCE this project has pinned

> **RESOLVED 2026-09-21 (round 53).** Seeds regenerated through the shipped framing; State
> test 116 leg F2 asserts each one still applies a parameter. Filed as RISK-016.

- **Classification:** missing test coverage (+ documentation inconsistency). **Severity:** medium.
  **Confidence:** high — **[verified first-hand]**
- **Evidence.** All three `tests/fuzz-corpus/*.bin` carry the correct 8-byte framing
  (`56 43 32 21` = `VC2!`, then a length) followed by **zlib** (`78 9c`); decompressing them yields
  `<?xml version="1.0" encoding="UTF-8"?> <!-- Legacy fixture …`. But
  `AudioProcessor::copyXmlToBinary` writes magic + length + **plain single-line XML** + a NUL, with no
  compression, at **every** JUCE this project has pinned — read directly at 8.0.14
  (`2cdfca8`), 9.0.0 (`f8f8864`), 9.0.1 (`e18f7f5`) and 9.0.2 (`7278278`); there is exactly one
  implementation in the tree. `getXmlFromBinary` is
  `parseXML (String::fromUTF8 (data + 8, jmin (size - 8, stated)))`, so a zlib body cannot yield a
  document. Corroborating: the committed `tests/fixtures/field_capture_v0_9_5.session` **is** plain
  text after its header.
- **Why it matters.** `docs/REPOSITORY_MAP.md:115` states the seeds exist "so the fuzzer starts from
  inputs that already reach the parser rather than from noise". They do not reach it. libFuzzer must
  synthesise a valid document from scratch inside a 90 s budget, so nothing downstream of `parseXML` —
  the migrations, `repairSerializedValues`, the A/B decode, `adoptRestoreTail` — is reachable from the
  corpus. This is the only generic oracle aimed at the path ADR-0055/0056 just hardened, and it is why
  F1 survived that round. The `fuzz` job is also the one release-blocking gate with **no liveness
  proof of any kind**, so a corpus that reaches nothing is indistinguishable from a healthy run.
- **Current protection.** None; the discrepancy is invisible because "no sanitizer fired" is the pass
  condition.
- **Refuted along the way, do not raise:** the tempting adjacent hypothesis that a JUCE bump changed
  the container and broke session compatibility. All four pinned versions write plain text, the v0.9.5
  fixture is plain text, and State test 116 leg F restores all four historical fixtures successfully.

### F6 — Four of six release-blocking race probes return success on a zero count with no control leg

> **RESOLVED 2026-09-21 (round 53).** All four now abort when their instrument cannot reach its
> window, demonstrated in both directions.

- **Classification:** confirmed risk. **Severity:** high → medium (verifier). **Confidence:** high
- **Evidence.** *(Attribution corrected 2026-09-21 — the original sentence was wrong in two ways,
  and the count it gave was right.)* **Two** probes refuse to report a zero without a live control:
  `--band-move-adopt-probe` ("the probe's own control line must read *late crossover stores SEEN*")
  and `--solo-alias-probe` ("ABORTS if its control line fails to solo band 1"). The other four are
  `--split-snapshot-probe`, `--add-target-probe`, `--add-edge-probe` and `--band-move-probe`, and
  three of those four **did** have a control — they *printed* its result and carried on regardless,
  which is a weaker defect than "no control" and a more insidious one, because the log looks right.
  `--split-snapshot-probe` had none at all. Contrast Test 38, which asks
  `anamorph::testing::selfCheck()` before trusting its own zero and prints a `::warning::` when a half
  is compiled out (`tests/dsp_tests.cpp:3126-3143`) — the discipline exists in the tree and is simply
  not generalised.
- **Why it matters.** These probes gate `main` on cross-thread windows that no deterministic test can
  enter. A probe whose instrument silently stops reaching its window is a green gate over nothing, and
  its failure mode is *exactly* the silence it prints when healthy.
- **Current protection.** The probes are the protection; nothing checks them.

### F7 — `REALTIME_AUDIO_POLICY` claims wrapper-path coverage that does not exist

- **Classification:** confirmed risk. **Severity:** high → medium (verifier). **Confidence:** high —
  **[verified first-hand]**
- **Evidence.** The Policy states the wrapper audio path's allocations are gated by Test 38's armed
  guard. `AnamorphTests` compiles `tests/dsp_tests.cpp` alone (`CMakeLists.txt:531-533`) and never
  `PluginProcessor.cpp` or `PluginParameters.cpp`; Test 38 constructs a bare `anamorph::AnamorphEngine`
  and arms around `engine.setParameters` + `engine.process`. The RTSan lane likewise builds and runs
  only `AnamorphTests` (`build.yml:3552, 3556`). `ANAMORPH_NONBLOCKING` is applied to exactly one
  function, `AnamorphEngine::process`.
- **Why it matters.** A Policy — higher authority than Architecture under `SOURCE_OF_TRUTH.md` — asserts
  a mechanical guarantee that no build provides. The wrapper body is covered only by the static text
  scan, which its own docstring says cannot follow a call. This is the layer KI-027 historically broke.
- **Nuance that keeps this at medium, not high.** The *substance* is fine: `ParamPointers::toEngine`
  is atomic loads plus `roundToInt` returning a POD (`src/PluginParameters.cpp:326-410`) — I read the
  whole body — and ADR-0029 §5/§8 records the RTSan scope decision and the trigger that would change
  it. The defect is the Policy's claim, not the code.

### F8 — Multiband Enable at Mix < 1 steps the Mix's dry source in a single sample

- **Classification:** confirmed bug. **Severity:** high. **Confidence:** high (finder + verifier
  confirmed; magnitude not measured — this round is read-only)
- **Subsystems:** engine chain · multiband · dry reconstruction (ADR-0005)
- **Evidence.** `mbEnableBlend` crossfades the wet path only; the Mix stage's dry source switches
  between the phase-matched `A(dry)` and the clean dry in one sample, so the transition is
  discontinuous by `(1−m)·(A(dry) − dry)` at any `m < 1` with two or more bands.
- **Why it matters.** A click on a control documented as click-free, on the one path ADR-0004 and
  ADR-0005 exist to keep continuous.
- **Current protection.** Test 23 asserts the Multiband Enable crossfade is click-free and never mutes
  — at Mix = 1, where the dry term is zero and the step cannot appear. The one test that arms the dry
  bank at Mix < 1 asserts only finiteness.
- **Interaction.** The delay-aligned dry bank is shared state for four multiband findings (F8 plus the
  stale dry-twins risk, the untested bank-crossfade path and the narrow flat-recombination coverage).

### F9 — A restore adoption runs underneath the held `soundReplacement` lock on the command paths

- **Classification:** confirmed risk. **Severity:** high. **Confidence:** medium (finder) → confirmed
  (verifier)
- **Evidence.** The `mayBlock == false` try that is supposed to make a late restore "come back at the
  next door" is bypassed on the command paths, so the adoption — which calls out to the host from
  inside itself — runs with the replacement lock held. That is the inversion `StateCommandGate.h`'s own
  header names ("THE DRAIN IS OUTSIDE THE LOCK, AND THAT IS NOT A DETAIL") and that State test 27
  measured hanging in round 21.
- **Why it matters.** It re-opens, on one family of paths, the cycle the whole gate exists to make
  impossible by construction.
- **Current protection.** `tsan_canary` and the TSan suite run, but this is a lock-ordering inversion
  reachable only with a host holding the other edge.
- **Related:** `adoptPendingHostState` has no re-entrancy guard, so a nested adoption from inside the
  tail's host callout adopts the newer restore while the outer tail writes the older one's view
  (medium, plausible).

### F10 — A re-entrant `mouseUp` during `beginBandMove` leaks two host gestures

- **Classification:** confirmed risk. **Severity:** high. **Confidence:** high
- **Evidence.** A `mouseUp` delivered re-entrantly from `beginBandMove`'s first pin open takes the
  quick-click branch (`held == false`, `moved == true`), so `endBandMove` never runs and both pin
  change-gestures stay open.
- **Why it matters.** The host sees an automation touch that never ends — the same class ADR-0050 was
  written to close, on a path it did not cover.
- **Root cause, and this is the more important finding.** `gestureActionDepth` is a guard on
  `gestureBands` that **one of its four writers consults**: `cancelActiveDrag` checks it
  (`src/gui/SpectrumImager.cpp:3254`); `:3105`, `:3151` and `:3587` clear `gestureBands`
  unconditionally. Every re-entrant window the guard was built for is still open through the other
  three.

### F11 — A host automation lane crossing a discrete step faster than ~1 per 3 blocks holds the plug-in in a perpetual duck

- **Classification:** confirmed risk. **Severity:** high. **Confidence:** high (round 2, confirmed)
- **Evidence.** The `FadeIn → FadeOut` re-arm resumes the fade-out from the current phase, so a lane
  that re-triggers before the fade completes never reaches the silent bottom.
- **Why it matters.** Automating any discrete parameter at block cadence — an ordinary thing for a host
  to do — can silence the plug-in indefinitely. ADR-0004, the Accepted decision governing this area,
  never addresses repeated or automated transitions, and all eight of its code citations now point at
  unrelated lines.
- **Current protection.** No test asserts the plug-in still produces output under repeated discrete
  transitions; the one multi-transition test spaces its events 30 blocks apart.
- **Companion (medium, confirmed):** `pendingAlgoReset` is latched at duck entry and is the one
  bottom-of-duck decision never recomputed in the mid-`FadeOut` branch, so a second discrete change
  before the bottom adopts the new algorithm with no module reset.

### F12 — `advancedMode` is host-writable and resizes the editor synchronously

- **Classification:** confirmed risk. **Severity:** high. **Confidence:** high (round 2, confirmed)
- **Evidence.** `isAutomatable() == false` only clears the VST3 `kCanAutomate` flag; every host→plug-in
  write path still applies `advancedMode`, and each accepted write resizes the editor synchronously.
  KI-007 and `PARAMETER_REGISTRY` present the flag as the mitigation that "stabilised the Linux gate",
  which the code does not support.
- **Current protection.** No test performs an `advancedMode` transition while an editor exists.

### F13 — Level Match state survives transitions that should invalidate it, and its tap has no non-finite guard

Three related findings, all confirmed:

- **`matchGainSmooth` is the one continuous smoother a forced duck does not snap** (`snapSmoothers()`
  excludes it deliberately, leaving it "to the injection / loudness re-measure"), so a preset load,
  undo or redo with Level Match engaged emerges from the fade-in still applying the *previous* state's
  match gain. Severity high → medium (verifier).
- **`loudness.softReset()` is gated on `processingDiffers()`**, which enumerates twelve discrete fields
  and no continuous one, so an A/B or preset swap differing only in Drive / Mix / Width / multiband
  widths does not re-arm the match. Design debt, medium. This is the same hand-maintained list asked
  two different questions — one of the round's six root causes.
- **`LoudnessMatch` is the only one of the three analysis taps with no non-finite guard.** A NaN latches
  its integrators and publishes NaN from `getMatchGainDb()`. `CorrelationMeter::publish` sanitizes all
  six accumulators and says why (`src/dsp/Correlation.h:55-62`); this tap does not. Medium → low
  (verifier: reachability depends on the engine self-heal firing first).

### F14 — The engine-wide NaN/Inf self-heal is never entered by any test

- **Classification:** missing test coverage. **Severity:** medium. **Confidence:** high
- **Evidence.** No test feeds non-finite or extreme-magnitude audio to `AnamorphEngine::process`.
  Test 2 asserts no NaN is *produced*; ADR-0009's decision bullet 2 — the recovery path — is untested.
- **Why it matters.** The self-heal is the last line of defence for the whole chain, and its own
  module-reset list cannot reseed `VelvetNoise`/`HaasProcessor` smoothed state (root cause #5), so a
  non-finite parameter target arriving mid-glide can latch `currentAmount`/`currentDensity`.

### F15 — Coverage holes in production-reachable paths

All confirmed, medium unless noted:

- **The transport / seek-reposition state machine in `processBlock`** — roughly half its body — is
  executed by no test in either suite: neither ever installs an `AudioPlayHead`, so `getPlayHead()` is
  null. Reached by both `rt-audio-path` and `test-coverage` independently.
- **No test negotiates a bus layout**, so `isBusesLayoutSupported` and the whole mono→stereo up-mix
  branch run in nothing. (The gate and `processBlock` do agree — verified — but the wrapper layer is
  unexercised.)
- **`MultibandWidth`'s discrete-jump bank-crossfade path (`fading == true`) is never executed**, though
  it is reachable through host automation of a split.
- **`ScopeBuffer` — the only bulk audio→GUI path — has no functional test**: `readLatest` appears in
  zero test files and `pushBlock` only in a never-linked compile-only canary, so wrap-around and
  tearing behaviour is unverified.
- **No test asserts reported latency equals actual delay while the oversampling wrap is running** —
  both `peakPos == lat` assertions measure the true-bypass ring. Low.

### F16 — Documentation and register drift (cluster)

Individually low, collectively the reason a future round will re-do this one's work. All confirmed.

- **`docs/FUTURE_RISKS.md`'s summary table is not an index of its own sections** — 15 sections
  (RISK-001…015), 13 rows; **RISK-012 and RISK-013 have full sections and no row**, while RISK-007,
  also resolved, does get one. The RISK-011 row describes an open residual while its own section title
  says RESOLVED. The round that added RISK-015 to both did not close the gap. **[verified first-hand]**
- **The RISK-011 reopen condition names a deleted symbol.** `FUTURE_RISKS.md:737` phrases it as "does
  not ask `deferWhileUserTransactionActive` first"; `docs/policies/THREADING_POLICY.md:202` states that
  symbol "is deleted rather than kept beside it". ADR-0008's related-code map names it too, plus
  `deferIfBusy`. So the only thing standing between the closure and a silent regression is stated
  against a symbol a reader cannot find. **[verified first-hand]**
- **`docs/KNOWN_ISSUES.md` still declares itself "Version-synced to v0.9.6"** (line 7) at project
  version 0.9.9, and keeps four entries it declares RESOLVED/CLOSED in contradiction of its own opening
  rule. **[verified first-hand]**
- **Stale test counts in four documents, including a Policy that defines the release gate** —
  `TESTING_POLICY.md` states 47+1 / 50 DSP and 40 / 63 state where the binaries report **53** and
  **113**.
- **`STATE_SERIALIZATION.md`'s `setStateInformation` procedure still begins with `getXmlFromBinary`** and
  nowhere records the ADR-0056 scan that now runs before it.
- **Evidence-anchor drift in six of the ten oldest Accepted ADRs** plus three architecture documents:
  anchors point at unrelated code. `check-citations` cannot catch this by design — its own docstring
  says the anchors are "ADOPTED, not audited: a clean run does not mean every citation is correct, it
  means none of them MOVED". The `DELIBERATE_REAIMS` exception list has also grown to **46** entries
  the gate itself reports as no longer needed (19 one commit earlier), each printed as a note rather
  than failing. **[verified first-hand]**
- **`CMakeLists.txt`'s `-fuse-ld=lld` block still argues it cannot affect the released Linux artifact
  because "the SHIPPED Linux binary is built by GCC"** — untrue since ADR-0030 made the Linux artifact
  Clang's. Medium, because it is reasoning a future change would rely on.
- **`CI_CD.md` — the document `build.yml` names as owning how the pipeline is wired — never mentions
  `scripts/check-dispatch.py`** and miscounts the source lints.

### Refuted (10) — do not resurrect without new evidence

`processBlock` before `prepareToPlay` writing through an indeterminate pointer · an
Oversampling-factor change while true-bypassed clicking · transient PDC disagreement around a factor
change · drive smoothers advanced per oversampled sample · `processBlockBypassed` not overridden · the
`skipRanOff` branch for unterminated **comment / CDATA / PI** (safe — measured; the *opening-tag*
sibling is F1) · the preset menu's absolute index racing `refresh()` · the single hard-coded program ·
`ScopeBuffer`'s overwrite margin · the editor never constructed by any Windows gate.

The comment/CDATA/PI refutation is worth noting as evidence the verification worked: the same branch
produced one safe sub-case (correctly refuted) and one unsafe one (F1, confirmed and reproduced).

---

## 3. Priority ordering, and why

**1. F1 (boundary false negative).** Only finding here that is a reproduced crash on a user action, on
code that shipped three days ago, whose register entry says it cannot happen. Impact, likelihood,
detection difficulty and diagnosis difficulty all point the same way, and the fix is small and local.

**2. F5 + F6 (oracles that cannot fail).** These rank above every other defect because they are why F1
survived a round that was specifically looking for it. A dead corpus and four control-less probes mean
the project's protection against this whole class is weaker than it reads. Fixing them changes what
future rounds can find; fixing a defect changes one line.

**3. F11, F8 (audible, reachable, automation-driven).** A perpetual duck under ordinary host automation
and a click on a path documented as click-free. Both are reachable without user error, both are in the
DSP contract, and both are uncovered because the existing tests probe one transition at a time and at
Mix = 1.

**4. F4, F2, F3 (host contract and user data).** Small diffs, real consequences: silent destruction of a
preset file, a flush request that does nothing, a truncated bounce. Ranked below the above only because
each needs an uncommon trigger.

**5. F9, F10, F12 (re-entrancy and ownership).** High severity, low measured likelihood, and each needs
a host behaviour the repository cannot reproduce. F10's root cause — a guard consulted by one of four
writers — matters more than its instance.

**6. F13, F14, F15 (state that outlives its validity, and the coverage that would have caught it).**

**7. F16 (documentation).** Last on severity, but *not* last in the road map: two items in it (the
RISK-011 reopen condition naming a deleted symbol, and the RISK-014 closure claim invalidated by F1)
are load-bearing for the next reviewer and belong with the work they describe.

**Deliberately not ranked high:** F7. The Policy over-claims, but the code it claims about is correct —
I read `toEngine` line by line. This is a documentation correction with a coverage note, not a defect.

---

## 4. Next-step road map

### R1 — Close F1, and correct the two records that now mis-describe it  ·  **DONE (round 53)**

**Problem:** a reproduced crash through both host-state surfaces. **Why first:** highest impact,
smallest diff, and it invalidates a published closure claim.
**Prerequisite — an owner ruling, and it is narrow.** ADR-0056's ruling granted size, depth and
`DOCTYPE`. Refusing a tag that *hides* depth enforces the depth rule already granted rather than
narrowing acceptance further, so this is arguably inside the existing ruling — but
`SESSION_COMPATIBILITY_POLICY` rule 1 makes serialization-acceptance changes an Architecture Review
Gate item, so the gate should be asked rather than assumed. Ask it with the reproduction attached.
**Work:** refuse an opening tag whose walk ends with a quote still open (or adopt the parser's
value-position rule); add the four shapes to State test 116 as legs; correct the RISK-014 entry and
ADR-0056's Consequences to say what is and is not measured gone.
**Complete when:** the four shapes are refused, the legitimate-document set still passes, both suites
are green, and the register no longer claims more than is true.
**Stop if:** the owner rules the shape out of scope — in which case the register must still be
corrected, because the claim is wrong either way.

### R2 — Give the generic oracles a liveness proof (F5, F6)  ·  **DONE (round 53)**

**Problem:** the fuzz corpus reaches nothing and four blocking probes pass on an unchecked zero.
**Why second:** it is the reason R1's defect survived. **Depends on:** nothing.
**Work:** regenerate the three seeds through the shipped `copyXmlToBinary` (they are derived from
`tests/fixtures/*.xml`, so this is mechanical) and add a liveness assertion to the `fuzz` job in the
shape the RTSan canary already uses — a seeded input that *must* be rejected, asserted on both exit
status and signature. Give the four control-less probes the Test 38 treatment: a leg that proves the
instrument still reaches its window, failing loudly when it does not. Correct
`REPOSITORY_MAP.md:115`.
**Complete when:** each of the five gates fails when its instrument is disabled, demonstrated by
running it that way once.
**Defer:** widening the fuzz budget. Time is not the binding constraint; the corpus was.

### R3 — A differential test for the boundary (F1's regression protection)  ·  **DONE (round 53)**

**Problem:** nothing compares `textIsAdmissible`'s verdict against what `parseXML` actually does, so
the next divergence is found the same way this one was.
**Why here:** after R1 (it must test the fixed behaviour) and after R2 (same discipline, same job).
**Work:** promote the probe written for this round — oracle *admitted ⇒ the parser returns*, over a
generator that includes quotes in **name** position, quotes in value position, unterminated
comment/CDATA/PI, stray closing tags, self-closing elements, embedded NULs and `<`/`>` inside
attribute values. It needs a proven-live crash case and process or thread isolation; the version in
this round's scratchpad is fork-based and Linux-only, which suits the `linux` job.
**Complete when:** it fails on the pre-R1 code and passes after, and its liveness case is asserted.
**Stop if:** isolation proves unportable — then keep it Linux-only and say so, rather than dropping it.

### R4 — The two automation-cadence DSP defects (F11, F8)

**Problem:** a perpetual duck under ordinary automation; a dry-source step at Mix < 1.
**Why here:** highest-impact audible defects, but each needs a measurement this read-only round could
not make. **Prerequisite:** measure first — magnitude of the mb-enable step at Mix < 1 with 2+ bands,
and the automation rate at which the duck stops completing. Both are `AnamorphBench`/harness work, not
guesses. **Gate:** a change to the switch machine's re-arm is a DSP-transition change; check it against
`ARCHITECTURE_REVIEW_GATE` and ADR-0004 before implementing, and amend ADR-0004 (which never addresses
repeated transitions) as part of the change.
**Work:** extend Test 23 to Mix < 1 with 2+ bands; add a repeated-discrete-transition test asserting
the plug-in still produces output; then fix.
**Complete when:** the new tests fail before and pass after, and ADR-0004 records the repeated-transition
case.

### R5 — Host contract and user data (F4, F2, F3)

**Problem:** a save that lies, a flush that does nothing, a tail that under-reports.
**Why here:** small independent diffs, no prerequisites, each with a clear completion test. Do them
together — they are one theme (`AudioProcessor` members other than `processBlock`).
**Work:** check the write (stream + explicit result, or verify the temp file's size before the
overwrite) and test it against a failing destination; override `reset()` to call `engine.reset()`;
derive the tail from the chain's actual ring and assert it. **Note:** a reported-latency change is a
hard-stop item, but the *tail* is not latency — confirm that reading before starting.
**Complete when:** a write to a full destination returns failure and leaves the existing preset intact;
a host reset flushes the rings; the reported tail is derived and asserted.

### R6 — Mechanical enforcement for the newest invariants (the round's root causes)

**Problem:** six invariants held by hand-maintained lists, including the RISK-011 closure the register
itself says nothing enforces.
**Why here:** it is the highest-leverage item in the list and the least urgent individually — nothing is
broken *today* because of it. Sequenced after the defects so it is built against a settled tree.
**Work, in cost order:** (a) a lint asserting every state-replacing entry point takes a
`StateCommandGate` — the `check-dispatch.py` shape, which already enforces a structurally identical
rule, with its own `--self-test`; (b) make `gestureActionDepth` consulted by all four writers of
`gestureBands`, not one; (c) give `processingDiffers`' two questions two predicates, or one predicate
and a written reason why one answer serves both; (d) a shared prologue for the five program-state
jumps, so `abSwitchToAdopted` cannot differ from the other four in both directions.
**Complete when:** each invariant fails a build when violated, demonstrated once by violating it.
**Defer (c) and (d)** if either turns out to need an ADR amendment — file the finding and stop; they are
architecture changes, not cleanups.

### R7 — Coverage for the production-reachable unexercised paths (F15, F14)

**Work:** an `AudioPlayHead` stub to reach the transport/seek machine; a bus-layout negotiation test; a
test that drives the multiband bank-crossfade path; functional tests for `ScopeBuffer` wrap-around; a
non-finite-input test that proves the ADR-0009 self-heal restores audio rather than latching silence.
**Why last among the work:** each is additive and independent, and their absence is a known quantity
rather than a moving risk. **Complete when:** each path has one test that would fail if the path were
deleted.

### R8 — Documentation, batched with the work it describes (F16)  ·  **PARTLY DONE (round 53)** — the items that ride with R1/R2 are done; the standalone pass is not

Two items ride with R1 (the RISK-014 closure claim; `STATE_SERIALIZATION.md`'s procedure). The rest is
one pass: the `FUTURE_RISKS` table's two missing rows and the RISK-011 row's contradiction; the
RISK-011 reopen condition re-phrased against `admitStateCommand`; the `KNOWN_ISSUES` sync line and its
four resolved entries; the stale test counts in four documents including `TESTING_POLICY`; the
`-fuse-ld=lld` comment's obsolete premise; `CI_CD.md`'s missing lint; and a sweep of the 46 dead
`DELIBERATE_REAIMS` entries. **Do not** attempt a general audit of the ADR evidence anchors — that is
the "adopted, not audited" problem, and it is a project of its own. Correct the six oldest ADRs' anchors
only where a road-map item already touches them.

### Round-53 outcome, and what the next item is

R1, R2 and R3 are complete; R8's R1/R2-adjacent items are done. **The next item is R4** (F11 and F8),
and the confidence recalibration in §7 sharpens its shape rather than changing its place: both are
implementation-Verified and impact-Unverified, so R4 begins with measurement — the magnitude of the
mb-enable dry step at Mix < 1 with 2+ bands, and the automation rate at which the duck stops
completing — and the fix follows the number. R5 (F4/F2/F3) is unchanged in position and is the
cheapest independent work available if R4's measurement stalls.

One thing this round changed about R6's justification rather than its content: the "invariant held by
a hand-maintained list" pattern gained a *measured* instance. F1 was a hand-written model of another
component's behaviour with nothing comparing the two, and the three probes whose controls printed
instead of asserting were the same shape again. R6 is now the item with the most evidence behind it
after R4.

### Explicit no-action

- **F7** — correct the Policy sentence in R8; do not annotate `processBlock` or widen the RTSan lane.
  ADR-0029 §8 already states the trigger that would change this, and the code is correct.
- **RISK-010** — re-verified: `mbBands` is still stored last in `addBandAt`, and every DSP clamp and
  mask the record names is intact. Its own instruction is "re-verify this record and move on". Done;
  moving on.
- **RISK-015** (five host-state shapes a preset file refuses) — the mechanism is ready
  (`DocumentRule::oneWellFormedDocument`, one enum value at two call sites) and the entry is accurate.
  This is an owner ruling under `SESSION_COMPATIBILITY_POLICY` rule 1, not an agent change. No action
  without it.
- **The gzip/session-compatibility hypothesis** — refuted with evidence; do not revisit.
- **ADR numbering 0016–0020** — legitimately absent and explained; 51 files, 51 index rows, exact
  bijection.

---

## 5. What is already healthy

Substantiated by the health auditor and, where marked, re-verified first-hand. Overclaims the auditor
could not substantiate were discarded rather than repeated.

- **The boundary's *coverage* is complete, even though its walk has F1.** Every XML parse entry point in
  `src/` carries a boundary: `PluginProcessor.cpp:2839` (session chunk), `:2954` (A/B payload),
  `PresetManager.cpp:543` (preset file). The other two `ValueTree::fromXml` sites operate on an
  already-parsed element. The scan also reads *exactly* the string the parser will, by reusing JUCE's
  own `String::fromUTF8` expression on its own range. **[verified first-hand]**
- **The boundary's false-positive margin is large and measured.** Every real and historical document
  sits at depth 2–3 against a cap of 8, and 10.6 KB against 256 KB; State test 116 leg F independently
  reports 10 444 B written against a 262 144 B cap. The two limits have exactly one definition.
  **[verified first-hand]**
- **The walk terminates, is linear, and cannot be given back depth** by stray closing tags; `depth`
  cannot go negative.
- **Oversized host blocks cannot overrun any latency ring** — stack-only slicing with the
  non-allocating `AudioBuffer` constructor, pinned by Test 43. **[verified first-hand]**
- **Reported latency has one source** and cannot drift from what the chain carries; the D-1 request
  protocol loses no request and serves none stale (release store / acquire exchange, clear before read),
  pinned by Tests 52–54. **[verified first-hand]**
- **Parameter snapshotting on the audio path is genuinely allocation-free** — `toEngine` is atomic loads
  plus `roundToInt` returning a POD. **[verified first-hand]**
- **Test 38 proves itself live before reporting a zero**, and says so loudly when a half is compiled
  out. This is the model R2 generalises.
- **Mono compatibility is guaranteed by construction** — `applyWidth` leaves `L + R = 2·mid` for any
  width, and `side` is exactly 0 for `L == R`. No document overclaims it.
- **Layout gate and `processBlock` agree**; the up-mix ordering is right.
- **Serialized numbers cannot become parameter state unless they are plain finite decimals** — one
  resolver serves every restore path.
- **The parameter-dispatch bracket is complete by construction**, not by inspection: four RAII entry
  points, thread-local depth, plus a self-testing lint.
- **All fourteen state-replacing entry points take their gate as the first statement**, and the gate
  cannot leak. (What is missing is enforcement for the *fifteenth* — R6a.)
- **Audio→GUI handoffs are wait-free**, and meter ballistics are sample-rate *and* block-size
  independent, including through the slicer. Both metering taps are hardened against non-finite state
  with the placement argument written down.
- **Every lint self-tests in the same job, ahead of its use**, with measurable case counts (52 / 93 /
  120 / 242 / 464 on this HEAD). **Every silent checker carries an in-job liveness proof asserting both
  exit status and report signature** — including the trap an earlier draft fell into. The TSan
  suppression file's anti-rot gate is the strongest such shape in the pipeline. Warning baselines are
  version-fenced and fail with the correct classification (`return 2`, not 1).
- **The Windows-only stack-overflow class is caught on the Linux runner** via a 1 MB `ulimit` re-run of
  both suites.
- **The pluginval crash-retry cannot launder a deterministic crash.**
- **Packaging and release staging are mutually consistent and fail-closed on mode as well as presence.**
- **ADRs are a closed set** with an exact 51/51 bijection to the index, and ADR-0056's contract is
  carried into both binding documents with its measured numbers.

---

## 6. Unresolved questions

Genuinely open after this round, with what would settle each.

1. **Can the F1 stack overflow be worse than a crash** on the platforms this ships to — guard page
   versus a large single frame skipping it? Needs a platform-specific examination of
   `readNextElement`'s frame size against the guard-page size on Windows and macOS. Treat as a crash
   until answered.
2. **The F1 threshold on the platforms that matter.** Measured here: SIGSEGV between N = 3000 and
   3500 on a 1 MB stack at `-O1` on Linux/x86-64, and at N ≈ 87 360 on 8 MB. MSVC and AppleClang frame
   sizes differ; ADR-0056 measured 2500–3000 on its Clang build. Needs a run per shipped toolchain.
3. **Does the F1 A/B leg reproduce through `setStateInformation` end to end?** Each link is proven
   separately (outer scan admits a 54 KB session, `slotPayloadIsAdmissible` admits the 18 KB payload,
   `parseXML` crashes on it) but not as one call into a live processor. Needs a harness that links the
   plugin sources.
4. **Do shipped hosts call `setProcessing(false)` / AU `Reset` often enough for F2 to be heard?** The
   wrapper call sites are proven; host behaviour on transport stop is not, and cannot be from here.
5. **Magnitude of the F8 dry step**, which depends on the allpass cascade's phase at the test
   frequency. Derived from code only; needs a measurement.
6. **Is the F9 inversion reachable in a real host**, or only in a harness that holds the other edge?
   Needs a host that writes one parameter from inside another's dispatch on two threads.
7. **Is `juce::dsp::Oversampling<float>::reset()` allocation-free on MSVC and AppleClang?** It is called
   from inside the annotated `process`. Linux/Clang is covered by the RTSan lane; the other two
   toolchains are never built by it.
8. **Does any shipped host call `processBlock` before `prepareToPlay`?** The refutation of that finding
   rests on the pinned wrappers all preparing first; a host that does not would make it live again.
9. **Whether a `seeked` reposition is detected correctly under a host that reports a sample position
   while stopped but scrubbing.**

74 further finder-level uncertainties are recorded in the run's own transcript; the nine above are the
ones that would change a road-map decision.

---

## 7. Confidence, recalibrated (added 2026-09-21, round 53)

The single **Confidence:** field used in §2 conflated four different questions, and
`SOURCE_OF_TRUTH.md` already supplies the vocabulary to keep them apart:

| Level | Meaning (verbatim from `SOURCE_OF_TRUTH.md`) |
|---|---|
| **Verified** | Provable from current source code, or code + a test case. |
| **Partially Verified** | Supported by README / commit / PR / code comment, but not fully provable from current code alone. |
| **Unverified** | No sufficient factual evidence; could be true but unproven (e.g. real-DAW host behaviour, performance numbers). |

The four axes, which a single label cannot carry:

- **Implementation** — is the code what the finding says it is?
- **Trigger** — has the path actually been driven, in a harness, to the stated outcome?
- **Host reachability** — does a shipped host do the thing that reaches it?
- **Impact** — has the user-visible consequence been measured, or is it inferred?

A finding can be **Verified** on implementation and **Unverified** on reachability at the same time,
and most of §2's "high confidence" labels were exactly that. Recalibrated, without mechanically
downgrading a finding merely because its real-world trigger was not exercised:

| # | Implementation | Trigger | Host reachability | Impact | Was | Note |
|---|---|---|---|---|---|---|
| F1 | Verified | **Verified** (reproduced end to end, and now a regression test) | Unverified | Verified (SIGSEGV measured) | high | the one finding where "high" was right on every axis except reachability |
| F2 | Verified (no override; `engine.reset()` has zero callers in `src/`) | Unverified | Unverified | Unverified | high | **overstated**: nothing was driven, and whether hosts call `reset()` is a fact about hosts |
| F3 | Verified (the constant) | Unverified | Unverified | Unverified (the 0.18 s ring is a finder measurement, not re-run) | high | **overstated** on three axes; the constant is the only Verified part |
| F4 | Verified (`juce_File.cpp:801` discards `appendText`'s result) | Unverified (no failing destination exercised) | Partially Verified (a full disk is not host-specific) | Partially Verified (follows from the code) | high | implementation is solid; the outcome is reasoned, not measured |
| F5 | Verified | **Verified** (bytes decoded, four JUCE versions read, gate demonstrated in both directions) | n/a | Verified | high | upheld |
| F6 | Verified | **Verified** (all four demonstrated aborting) | n/a | Verified | high | upheld, but its *attribution* was wrong — see the corrected evidence line in §2 |
| F7 | Verified (the Policy's claim is provably unbacked) | n/a | n/a | Verified (documentation defect) | high | upheld; note the *code* it claims about is correct |
| F8 | Partially Verified (mechanism read from code) | Unverified | Unverified | Unverified (magnitude depends on allpass phase; not measured) | high | **overstated**: a code-established mechanism is not a measured click |
| F9 | Partially Verified | Unverified | Unverified | Unverified | high | **overstated**; needs a host holding the other edge |
| F10 | Verified (the four-writer/one-consumer guard is plain in the source) | Unverified | Unverified | Unverified | high | implementation Verified, everything downstream inferred |
| F11 | Partially Verified | Unverified (the "~1 per 3 blocks" figure is a finder measurement) | Unverified | Unverified | high | **overstated** |
| F12 | Verified (`isAutomatable()` only clears `kCanAutomate`) | Unverified | Partially Verified | Unverified | high | |
| F13 | Verified (all three sub-findings are readable facts) | Unverified | Unverified | Unverified | mixed | the sub-finding severities already varied; the axes did not |
| F14 | Verified (grep: no test feeds non-finite audio) | n/a | n/a | n/a | high | upheld — a coverage claim is settled by grep |
| F15 | Verified | n/a | n/a | n/a | high | upheld, same reason |
| F16 | Verified | n/a | n/a | n/a | high | upheld, same reason |

**What this changes in the ordering, and what it does not.** Coverage and documentation findings
(F5, F6, F7, F14, F15, F16) are settled by reading the tree, so their confidence survives intact —
which is why F5/F6 kept their place above the remaining defects in the original ordering and keep it
in §"Updated road map" below. The findings that move are the ones whose *impact* was inferred from a
mechanism: **F8 and F11 are re-rated implementation-Verified / impact-Unverified**, which is what
makes "measure before fixing" a prerequisite of their road-map item rather than a courtesy. **F2 and
F3 do not move in priority** despite the downgrade: their fixes are small, local and independent, and
the cost of being wrong about reachability is a few lines, not a redesign.

**Two errors in this report were found by re-verifying it**, and both are recorded above rather than
quietly edited: the accounting (§front matter, Appendix A) and F6's attribution. Neither changes a
road-map decision, which is itself worth stating — an audit that finds only harmless errors has still
established something.

---

## Appendix A — reconciliation of all 78 surviving findings

76 round-1 candidates (10 refuted) + 19 round-2 candidates (7 refuted) = **78 survivors**. Every one
is listed below with what §2 did with it. "Referenced only" means it appears in §2's prose — inside
another finding's Interaction, Companion or Current-protection paragraph — without being presented as
a finding of its own. "Not carried" means it does not appear in this report at all.

**Disposition: 41 carried into F1–F16 · 7 referenced only · 30 not carried.**

The 30 are not a silent drop any more, and six of them are worth naming here because they are *not*
low-severity items — they were omitted by a threshold this report never stated:

| finding id | class | sev | why it matters |
|---|---|---|---|
| `ab-switch-deletes-pending-undo-step` | confirmed-bug | medium | a direct A/B switch deletes a settled-but-unpolled gesture's undo step (`abSwitchToAdopted` calls `syncCommitted()` with no preceding flush) |
| `scope-window-is-frames-not-time` | confirmed-risk | medium | the vectorscope window is a frame count, so the trail shortens 4× from 48 to 192 kHz |
| `isa-baseline-no-liveness-gcc-clang` | confirmed-risk | medium | the ADR-0031 ISA baseline has no flag-liveness assertion in CI |
| `macos-focus-false-positive` | confirmed-risk | medium | on macOS `peer->isFocused()` is satisfied by a flag `grabFocus()` sets unconditionally |
| `duck-fires-for-inaudible-changes` | design-debt | medium | `discreteDiffers` ducks to silence for changes that provably cannot alter the audio |
| `keyboard-focus-untestable-by-construction` | missing-test-coverage | medium | no gate in the repository can observe the keyboard-focus declaration |

These six are added to §"Remaining findings" and are candidates for the next round; the other 24 are
low-severity or informational and stay in this appendix as the record.

| # | round | dimension / area | finding id | class | sev | verdict | disposition |
|---|---|---|---|---|---|---|---|
| 1 | r1 | xml-boundary | `unterminated-quote-bypass` | confirmed-bug | high | confirmed | **F1** — headline |
| 2 | r1 | xml-boundary | `no-differential-oracle` | missing-test-coverage | medium | confirmed | **F1** — cited as F1 protection; is road-map R3 |
| 3 | r1 | rt-audio-path | `host-reset-unwired` | confirmed-bug | medium | confirmed | **F2** — headline |
| 4 | r1 | latency | `host-reset-never-reaches-engine` | confirmed-bug | medium | confirmed | **F2** — same defect, second dimension |
| 5 | r2 | The AudioProcessor hos | `tail-under-reports-lr4-ringdown` | confirmed-bug | medium | confirmed | **F3** — headline |
| 6 | r1 | presets | `save-write-failure-unreportable` | confirmed-bug | high | confirmed | **F4** — headline |
| 7 | r1 | test-coverage | `fuzz-gate-no-liveness` | confirmed-risk | medium | confirmed | **F5** — headline (with orchestrator corpus-format fact) |
| 8 | r1 | test-coverage | `probe-no-liveness-control` | confirmed-risk | high | confirmed | **F6** — headline |
| 9 | r1 | test-coverage | `wrapper-alloc-claim-unbacked` | confirmed-risk | high | confirmed | **F7** — headline |
| 10 | r1 | rt-audio-path | `wrapper-audio-path-no-runtime-tier` | missing-test-coverage | medium | confirmed | **F7** — same claim, second dimension |
| 11 | r1 | engine-chain | `mb-enable-dry-source-step` | confirmed-bug | high | confirmed | **F8** — headline |
| 12 | r1 | state-gate | `adoption-under-the-gates-own-lock` | confirmed-risk | high | confirmed | **F9** — headline |
| 13 | r1 | state-gate | `reentrant-adoption-splits-the-session` | likely-weakness-needs-validation | medium | plausible | **F9** — named as "Related" in F9 |
| 14 | r1 | spectrumimager | `mouseup-nested-in-beginbandmove` | confirmed-risk | high | confirmed | **F10** — headline |
| 15 | r2 | Host automation of the | `duck-no-rearm-limit` | confirmed-risk | high | confirmed | **F11** — headline |
| 16 | r1 | engine-chain | `algo-reset-latched-not-recomputed` | confirmed-bug | medium | confirmed | **F11** — same defect, round-1 dimension |
| 17 | r2 | Host automation of the | `stale-pending-algo-reset` | confirmed-bug | medium | confirmed | **F11** — named as "Companion" in F11 |
| 18 | r2 | Host automation of the | `adr-0004-silent-and-stale` | doc-inconsistency | low | confirmed | **F11** — named inside F11 |
| 19 | r2 | Host-driven writes to  | `adv-flag-blocks-nothing` | confirmed-risk | high | confirmed | **F12** — headline |
| 20 | r2 | Host-driven writes to  | `ki007-removing-that-trigger` | doc-inconsistency | medium | plausible | **F12** — named inside F12 |
| 21 | r2 | Host-driven writes to  | `adv-transition-with-editor-untested` | missing-test-coverage | medium | confirmed | **F12** — named inside F12 |
| 22 | r1 | levelmatch | `match-smoother-not-landed-on-forced-swap` | confirmed-risk | high | plausible | **F13** — cluster member 1 |
| 23 | r1 | levelmatch | `loudnessmatch-no-nonfinite-guard` | confirmed-bug | medium | plausible | **F13** — cluster member 3 |
| 24 | r1 | levelmatch | `softreset-gate-misses-continuous-only-ab-swaps` | design-debt | medium | confirmed | **F13** — cluster member 2 |
| 25 | r1 | nonlinear-numerics | `selfheal-never-executed-by-any-test` | missing-test-coverage | medium | confirmed | **F14** — headline |
| 26 | r1 | nonlinear-numerics | `nan-param-latches-in-haas-velvet-reset` | likely-weakness-needs-validation | medium | plausible | **F14** — named inside F14 via root cause 5 |
| 27 | r1 | rt-audio-path | `transport-seek-block-untested` | missing-test-coverage | medium | confirmed | **F15** — same gap, second dimension |
| 28 | r1 | multiband | `mb-fade-untested` | missing-test-coverage | medium | confirmed | **F15** — cluster member |
| 29 | r1 | channel-config | `mono-layout-untested` | missing-test-coverage | medium | confirmed | **F15** — cluster member |
| 30 | r1 | scopes-rt-gui | `scopebuffer-untested` | missing-test-coverage | medium | confirmed | **F15** — cluster member |
| 31 | r1 | test-coverage | `playhead-transport-untested` | missing-test-coverage | medium | confirmed | **F15** — cluster member |
| 32 | r1 | latency | `engaged-wrap-delay-unasserted` | missing-test-coverage | low | confirmed | **F15** — cluster member |
| 33 | r1 | build-platform | `lld-comment-inverted-since-adr-0030` | doc-inconsistency | medium | confirmed | **F16** — cluster member |
| 34 | r1 | docs-adr | `adr-evidence-anchors-misaimed` | doc-inconsistency | medium | confirmed | **F16** — cluster member |
| 35 | r1 | docs-adr | `state-serialization-omits-adr-0056-step` | doc-inconsistency | medium | confirmed | **F16** — cluster member |
| 36 | r1 | docs-adr | `suite-inventory-stale-four-docs` | doc-inconsistency | medium | confirmed | **F16** — cluster member |
| 37 | r1 | params | `adr-0008-related-code-deleted-symbols` | doc-inconsistency | low | confirmed | **F16** — cluster member (the deleted-symbol item) |
| 38 | r1 | build-platform | `cicd-omits-dispatch-lint` | doc-inconsistency | low | confirmed | **F16** — cluster member |
| 39 | r1 | test-coverage | `testing-policy-stale-counts` | doc-inconsistency | low | confirmed | **F16** — cluster member |
| 40 | r1 | docs-adr | `handover-release-bookkeeping-self-contradictory` | doc-inconsistency | low | confirmed | **F16** — cluster member |
| 41 | r1 | docs-adr | `resolved-kis-retained-against-own-rule` | doc-inconsistency | low | confirmed | **F16** — cluster member |
| 42 | r1 | params | `ab-switch-deletes-pending-undo-step` | confirmed-bug | medium | confirmed | **not carried** — medium severity — omitted from the headline selection |
| 43 | r1 | scopes-rt-gui | `scope-window-is-frames-not-time` | confirmed-risk | medium | confirmed | **not carried** — medium severity — omitted from the headline selection |
| 44 | r1 | build-platform | `isa-baseline-no-liveness-gcc-clang` | confirmed-risk | medium | plausible | **not carried** — medium severity — omitted from the headline selection |
| 45 | r2 | Keyboard input and typ | `macos-focus-false-positive` | confirmed-risk | medium | confirmed | **not carried** — medium severity — omitted from the headline selection |
| 46 | r2 | Keyboard input and typ | `keyboard-focus-untestable-by-construction` | missing-test-coverage | medium | confirmed | **not carried** — medium severity — omitted from the headline selection |
| 47 | r2 | Host automation of the | `duck-fires-for-inaudible-changes` | design-debt | medium | plausible | **not carried** — medium severity — omitted from the headline selection |
| 48 | r1 | xml-boundary | `preset-walk-unchanged-claim` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 49 | r1 | rt-audio-path | `rt-doc-line-drift` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 50 | r1 | latency | `adr0003-latch-clause-and-test52-comments-stale` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 51 | r1 | engine-chain | `haas-velvet-glide-not-rate-normalised` | likely-weakness-needs-validation | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 52 | r1 | nonlinear-numerics | `adr-0009-related-code-line-drift` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 53 | r1 | levelmatch | `ab-match-memory-capture-race` | likely-weakness-needs-validation | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 54 | r1 | channel-config | `bus-layout-doc-citations-stale` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 55 | r1 | params | `registry-snapshot-scope` | informational | low | confirmed | **not carried** — informational classification — not a defect |
| 56 | r1 | params | `mbfreqhigh-text-roundtrip` | confirmed-bug | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 57 | r1 | params | `abcopy-drops-level-match-memory` | confirmed-risk | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 58 | r1 | params | `undo-window-reverts-concurrent-automation` | likely-weakness-needs-validation | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 59 | r1 | serialization | `ab-slot-payload-never-repaired` | design-debt | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 60 | r1 | serialization | `state-doc-anchors-stale` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 61 | r1 | state-gate | `editor-door-comment-names-the-blocking-poll` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 62 | r1 | presets | `preset-path-doc-drift` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 63 | r1 | editor-sync | `scope-persist-no-resync` | confirmed-bug | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 64 | r1 | editor-sync | `vectorscope-stale-reachability-comment` | doc-inconsistency | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 65 | r1 | editor-sync | `sweep-armed-on-click-for-deferrable-commands` | design-debt | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 66 | r1 | spectrumimager | `doubleclick-reads-split-row-twice` | design-debt | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 67 | r1 | scopes-rt-gui | `idle-gate-two-load-slip` | likely-weakness-needs-validation | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 68 | r1 | scopes-rt-gui | `vectorscope-clip-ring-decimated` | design-debt | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 69 | r1 | build-platform | `merge-result-never-linted` | missing-test-coverage | low | plausible | **not carried** — severity low AND verdict plausible — below the headline threshold used |
| 70 | r1 | build-platform | `objcxx-outside-two-source-lints` | design-debt | low | confirmed | **not carried** — severity low, confirmed — real but not selected for a headline entry |
| 71 | r2 | Keyboard input and typ | `cost-of-flipping-true` | informational | low | confirmed | **not carried** — informational classification — not a defect |
| 72 | r1 | multiband | `mb-fade-stale-dry-twins` | confirmed-risk | medium | confirmed | **referenced only** — named in F8 "Interaction" only |
| 73 | r1 | presets | `step-parses-under-replacement-lock` | confirmed-risk | medium | plausible | **referenced only** — named in the Interactions summary only |
| 74 | r1 | spectrumimager | `wheel-burst-clears-nested-press-stamp` | likely-weakness-needs-validation | medium | plausible | **referenced only** — named in F10 root-cause paragraph only |
| 75 | r1 | spectrumimager | `cancel-releases-wheel-before-declining` | confirmed-risk | medium | confirmed | **referenced only** — named in F10 root-cause paragraph only |
| 76 | r2 | Host automation of the | `no-level-presence-test-under-transitions` | missing-test-coverage | medium | confirmed | **referenced only** — named in F11 "Current protection" only |
| 77 | r1 | multiband | `mb-flat-recomb-envelope` | missing-test-coverage | low | plausible | **referenced only** — named in F8 "Interaction" only |
| 78 | r1 | build-platform | `ci-coverage-boundary` | informational | low | confirmed | **referenced only** — absorbed into F15/healthy prose |
