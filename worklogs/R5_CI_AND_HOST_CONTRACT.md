# R5 — The Linux CI failure, and F4 / F2 / F3

**Base:** `main` at `bfd0e06`. **Branch:** `claude/anamorph-comprehensive-review-90tpty`.
**Version:** 0.9.9 (`CMakeLists.txt:14`). **Date:** 2026-09-21.

---

## A. Baseline

`origin/main` is `bfd0e06`. **PR #155 (R4) is open, not merged** — its base is `bfd0e06` and its head
was `77e4aec` when this round began, so R4's three fixes are on this branch and not yet in `main`.
This round therefore continues on the same branch, and "the current implementation" below means the
branch head, with every R4 change already in it. No other development branch was read, fetched or
used.

CI baseline on `77e4aec`: **the `linux` job failed**; `linux-lto-tests`, `sanitizers`, `tsan`,
`fuzz`, `realtime`, `docs`, `source-lint`, `windows`, `windows-avx2-ab`, `macos`, `macos-intel`,
`macos-crossslice`, `merge-check`, `CodeQL` and `PREfast` were all green. So exactly one job was red,
and it was red on a check none of the others run.

Local suites at the start of the round: DSP **469 / 0**, state **4654 / 0**.

---

## B. The Linux CI failure

### The failing job, and its exact output

`linux` / step `check-clang-warnings`
(run 35644435719, job 106481745235):

```
##[error]Clang emitted 1 NEW warning(s) in first-party sources (src/, tests/).
                Fix them -- do not widen the baseline; it is a debt list, not a permission list.
tests/dsp_tests.cpp:5935:26: warning: [-Wunused-lambda-capture]
##[error]-Wunused-lambda-capture in tests/dsp_tests.cpp: 1 site(s), baseline allows 0.
##[error]Process completed with exit code 1.
```

### Root cause

Mine, from R4, and a compiler-coverage gap rather than a logic error. Test 55's control-3 block
declares `const int block = 128`, so `160 * block` inside the `tailDiff` lambda is a **constant
expression**; `block` is therefore not odr-used and the `[block]` capture is dead. Clang diagnoses
that. **GCC has no equivalent diagnostic**, and R4 validated on GCC 13 only — so the warning could
not have been seen locally, which is precisely the gap
`scripts/check-clang-warnings.py`'s own header says the job exists to close.

**Deterministic:** it is a compile-time diagnostic. **Reproduced locally** with clang-18 at the same
file, line and column (5935:26) before the fix and absent after — the local reproduction also emits
the one pre-existing `tests/dsp_tests.cpp:470` `-Wunused-but-set-variable` site that the baseline
carries, which is how it was confirmed faithful rather than merely quiet.
**Unrelated to F4/F2/F3:** it is in a test file, in R4's code, and touches no product source.

### Fix

The start index became a **parameter** instead of a capture. Simply deleting the capture would also
compile, but it would leave the lambda resting on `block` remaining constexpr-usable; a later edit
making it non-const would reintroduce the same warning silently. A dead local in Test 57
(`worstHere`, assigned and then only cast to void) that R4 also left behind was removed.

### Validation, and that the gate still bites

The baseline was **not** widened — it stands at the same 9 entries.

| check | result |
|---|---|
| a log carrying the re-introduced warning | gate **exits 1**, same two error lines — the class is still detected |
| the post-fix clang log | gate exits 0: 3 accepted baseline sites, **0** ungated diagnostics |
| a log from a mismatched clang major | gate **exits 2** and refuses — its version guard is still armed |
| `--self-test` | 28 cases pass |
| `src/dsp/AnamorphEngine.cpp` (R4's other TU) under clang | only its pre-existing baseline `-Wswitch-enum` site |

**And the job is green in CI.** The `linux` job passed on `ef75b8c` (run 35651087654, job
106503393315) — confirmed in CI, not inferred from the local reproduction.

### A second CI failure, also mine, in a different job

Pushing the R5 work turned the **`sanitizers`** job red — an AddressSanitizer
`stack-overflow`, on the **main thread**, inside
`testHostStateIsBoundedBeforeTheParser` (State test 116, from round 2) at
`tests/state_tests.cpp:13953`, which is a lambda whose body constructs an
`AnamorphAudioProcessor` as a stack local. Nothing in that test was touched this round.

**Attributed, not guessed.** `sanitizers` was green on `77e4aec` (R4's head) and on
`ef75b8c` (the CI fix), and failed on `045026b`. The only change to that translation
unit was this round's three new tests.

**The cause is a rule this repository had already measured and written down**, in the note
above State test 59 (D-2 round 13):

> `AnamorphAudioProcessor` is ~138 kB, and a compiler gives each of a function's
> sibling-scope locals its own frame slot rather than reusing one. […] Splitting the legs
> into separate functions is **NOT sufficient, and was measured not to be: the compiler
> inlines them back into one frame and the overflow returns.** Only the heap allocation is
> guaranteed by the language.

Tests 117–119 declared their processors on the stack and so ignored that rule. Measured
here with `-fstack-usage` under ASan: one processor local costs **139 KB** of frame, State
test 116 already needs **1.36 MB**, and the three new tests were carrying **~714 KB**
between them — test 118 alone **418 KB**, holding three. Moving all five to
`std::make_unique`, the way every other heavy state test in the file already does, drops
those frames to **1,040 / 1,440 / 1,008 bytes** and removes the round's entire contribution
to that pressure (TU total 39,830,168 → 39,119,656 bytes).

**I could not reproduce it locally, and say so rather than implying I did.** GCC 13 with
ASan passes (4677 / 0), and so does a Release build under `ulimit -s 1024` — the
reproduction `TESTING.md` prescribes for this class — both before and after the fix,
because GCC does not inline those three functions into `main`. The failure needs the
sanitizers job's clang-22 with `local-bounds`, `implicit-conversion`, `vptr` and the rest,
which this container cannot build (no `libclang_rt` for the installed clang-18). **CI is
therefore the verification for this one, not a local run.**

**The same clang gate had already caught a second would-be failure before it was pushed.** The F2 work
gave `AnamorphEngine::reset` a `ResetScope scope` parameter, and `scope` is already a member of
`AnamorphEngine` (the `ScopeBuffer` at `AnamorphEngine.h:212`) — `-Wshadow`, which the baseline allows
0 of in that file. GCC 13 built it silently; clang-18 reported it; the parameter was renamed to
`resetScope`. Running the clang gate locally over **every TU this round touched** is what turned the
round's first CI failure into the round's own habit, and it is the cheapest of the R6 items in §I.

---

## C. F4 — preset write failure: **CONFIRMED, and worse than the finding said**

### What the finding got right, and what it mis-located

The prior finding said the defect was that "the underlying `appendText()` result is discarded". The
discard is real and is quoted below — but it is **inside JUCE**, not inside Anamorph.
`writeUserPreset` did check what it was given:

```cpp
if (xml == nullptr || ! file.replaceWithText (xml->toString())) return false;
```

So the finding's *shape* was right and its *location* was wrong, and that matters: no amount of
checking at the call site could have helped.

### Root cause, from the pinned JUCE (9.0.2, `72782788ce`)

Three discards in a row, each quoted verbatim:

```cpp
// juce_File.cpp:798-803
bool File::replaceWithText (...) const
{
    TemporaryFile tempFile (*this, TemporaryFile::useHiddenFile);
    tempFile.getFile().appendText (...);            // <-- bool result DISCARDED
    return tempFile.overwriteTargetFileWithTemporary();
}

// juce_TemporaryFile.cpp:100
    if (temporaryFile.exists())                     // <-- EXISTS, not "holds the bytes"

// juce_FileOutputStream.cpp:47-51
FileOutputStream::~FileOutputStream()
{
    flushBuffer();                                  // <-- bool result DISCARDED
    closeHandle();
}
```

and one fact that makes all three bite at once: the default stream buffer is **16384 bytes**
(`juce_FileOutputStream.h:79`) while an Anamorph preset is ~1.5 KB. The whole preset therefore sits
in the buffer — `writeText` returns true **without touching the disk** — and the only real write
happens in the destructor, after every error path has been discarded.

So the answer to "can a failed or partial write be promoted over a previously valid file?" is
**yes**, and the mechanism is (b) an atomicity break caused by (a) a discarded error code — both, not
either.

### Reproduction: a real ENOSPC, not a simulation

A 2 MB tmpfs, filled to 100%, with the preset folder redirected onto it. The redirection is through
`HOME`, because `PresetManager::presetDirectory()` (`PresetManager.cpp:76-80`) resolves
`userApplicationDataDirectory`, and on Linux that is `resolveXDGFolder ("XDG_CONFIG_HOME", "~/.config")`
which reads `~/.config/user-dirs.dirs` and falls back to `~/.config` —
**it never reads the environment variable** (`juce_Files_linux.cpp:88-110, 135`), so `HOME` is the
only lever. Driven through `AnamorphAudioProcessor` → `PresetManager::saveUser`, the real path.

| | before the fix | after the fix |
|---|---|---|
| `saveUser` completion | **ok = TRUE** | ok = **false** |
| the preset file | **0 bytes**, does not parse | **1539 bytes**, parses |
| shown as current preset | `R5Probe` | `Default` (not advanced) |
| edited-since-saved marker | **CLEAN** | **DIRTY** |
| the preset that was there | **destroyed** | **byte-for-byte identical** |
| control: room available | 1539 bytes, ok = TRUE | 1539 bytes, ok = TRUE (unchanged) |

At the raw-JUCE level, with a few KB free rather than none, the same call left a file **truncated at
the free-space boundary** — 8192 bytes of a 20000-byte document — again returning TRUE.

This contradicts the contract `PresetManager.h` states for `saveUser` in its own words: *"the write
itself can only fail during I/O, so that failure travels on `onComplete`."*

### Decision: confirm and fix

`writeTextVerified` in `PresetManager.cpp` performs the same `TemporaryFile` write with the same
`useHiddenFile` flag and `replaceWithText`'s **own default argument set**, so the atomicity is
unchanged, the hidden-sibling path the tilde-name reasoning above `writeUserPreset` depends on still
holds, and a successful save writes byte-for-byte what it wrote before. What it adds is that the
write is **checked**:

1. `failedToOpen()` — the temp file could not be created;
2. `writeText`'s return value — the write was refused;
3. `flush()` then `getStatus().failed()` — the buffer could not be pushed, or `fsync` failed;
4. **the file's size against the byte count the stream accepted** — the short-write case.

Step 4 is not belt-and-braces, and measurement is what establishes that.
`FileOutputStream::writeInternal` records a status **only when `::write` returns −1**
(`juce_SharedCode_posix.h:527-538`), so a write that stores part of the data sets nothing. Measured
under a 512-byte `RLIMIT_FSIZE` with a 7105-character document: `writeText` returned **true**,
`getStatus().failed()` was **false**, the stream reported position **7407** — and the file was
**512 bytes**. Steps 1–3 all pass that case; only step 4 catches it.

On any failure the temp file is never promoted and `~TemporaryFile` removes it, so the preset already
on disk is untouched — which is the invariant the table above measures.

Two further facts, both from the adversarial pass over this analysis:

- **There is a fourth discard, and the check's ORDERING covers it.** `closeHandle()` throws away
  `close()`'s result too (`juce_SharedCode_posix.h:518-525`), so data can still be lost after the last
  `write`. The size comparison is taken **after** the stream's scope closes, so a close-time loss
  appears as a mismatch rather than slipping past.
- **The original path never calls `fsync`.** `flushInternal()` is reached only from
  `FileOutputStream::flush()` (`juce_FileOutputStream.cpp:82-86`), which `replaceWithText` never
  calls — so the rename was not ordered after the data. `writeTextVerified` calls `flush()`, which
  adds that ordering as well as surfacing the error.

**Why the existing suite missed it.** The suite *does* test a failing save — State test 108 leg B
stages the failure with `blocked.createDirectory()`, "a directory stands where the file must go". That
makes the **rename** fail, which `overwriteTargetFileWithTemporary` already reports correctly. The
**write** half was never exercised by anything.

**One further consequence, worth recording.** ADR-0055 (accepted 2026-09-18) made the loader refuse a
preset file that is not one well-formed document, and 0.9.9 added the *PRESET UNREADABLE* indicator
for it. Pre-fix, Anamorph's own writer could produce exactly such a file — the measured 0-byte result
does not parse. The plug-in could create the damaged file its own loader then refuses.

### Confidence

| | |
|---|---|
| implementation | **Verified** — three discards quoted from the pinned source |
| trigger | **Verified** — real ENOSPC on a real filesystem, and `RLIMIT_FSIZE`, both deterministic |
| impact | **Verified** — end-to-end through `AnamorphAudioProcessor`: valid preset destroyed, success reported, marker cleared |

---

## D. F2 — host reset contract: **CONFIRMED**

### Root cause

`AnamorphAudioProcessor` declared no `reset()` override (every `AudioProcessor` virtual it overrides
is listed in `PluginProcessor.h`; `reset` was not among them), and
`AudioProcessor::reset`'s default implementation does nothing. The documented contract is explicit
(`juce_AudioProcessor.h:939-944`):

> *"A plugin can override this to be told when it should reset any playing voices. The default
> implementation does nothing, but a host may call this to tell the plugin that it should stop any
> tails or sounds that have been left running."*

### Actual host-contract semantics, from the wrapper sources

Not a formality, and not a question about any particular DAW:

- **VST3** — `setProcessing (TBool state) { if (! state) getPluginInstance().reset(); }`
  (`juce_audio_plugin_client_VST3.cpp:3475-3479`). A host issues `setProcessing(false)` whenever it
  stops processing.
- **AU** — `Reset()` calls `juceFilter->reset()` (`juce_audio_plugin_client_AU_1.mm:255-263`).
- **Neither is followed by a `prepareToPlay`.** The VST3 side re-prepares through `setupProcessing`
  with `CallPrepareToPlay::no` (`:3470`); the AU side calls `prepareToPlay()` **before** the reset and
  only when not already prepared.

So the request arrives, nothing else cleans up after it, and before this round it reached **no state
at all**.

Two pieces of evidence make the omission unambiguous rather than arguable:

1. `AnamorphEngine::reset()` is written for this exact request — its own comment reads *"so a host
   reset lands in a clean steady state (bit-exact transparent from sample 0)"* — and it clears the
   delay lines, crossover banks, oversamplers, all four dry/bypass/OS-compensation rings and any
   in-flight duck, allocating nothing.
2. It had **exactly one caller in the whole product**: `prepare()`, at `AnamorphEngine.cpp:145`.
   A flush written for a host reset was unreachable from a host.

What could *not* be established, and is not claimed: how often any specific shipped host issues the
call. The finding stands on the plug-in not honouring its own wrapper contract, which is a separate
question and the one the code answers.

### Decision: confirm and fix — but NOT with the wholesale flush

The first version of this fix was `void reset() override { engine.reset(); }`, and **it was wrong**.
Adversarial review of this round's own analysis caught it, and the conflict is with an Accepted ADR,
which is an `ARCHITECTURE_REVIEW_GATE` item — so it is recorded here rather than quietly amended.

`AnamorphEngine::reset()` calls `loudness.reset()`, which zeroes `matchGainDb`
(`LoudnessMatch.cpp`). That value is read by the Level-Match readout
(`PluginEditor.cpp:1652`), by Apply — which writes it into Output Gain (`PluginProcessor.cpp:469`) —
and by the per-slot A/B match (`PluginProcessor.cpp:2184`). **ADR-0007's Decision is that on silence
the measure "holds the last trusted" value**, and its Consequences are "No drift on silence; no
ratchet; no Mix=100% slam". A transport stop is the canonical silence. So routing it into the
wholesale flush would have re-created the exact "slammed loud on the next play" symptom ADR-0007's
Context records, and contradicted what Test 16 (`testLevelMatchSilenceFreeze`) asserts.

**Measured, both ways:** Level Match converges to −5.158 dB; with the wholesale flush a host reset
leaves **0.000 dB**, with the narrowed one it still reads **−5.158 dB**.

The shipped fix therefore narrows the scope:

```cpp
void reset() override { engine.reset (anamorph::AnamorphEngine::ResetScope::audioTailsOnly); }
```

with `ResetScope::everything` (the default, so `prepare()` is byte-for-byte unchanged) and
`audioTailsOnly`, which skips exactly `loudness.reset()`. The reasoning is the host's own request:
it asks the plug-in to stop *tails and sounds*, and a loudness **measurement** is neither. The meters
and the correlation display are still cleared in both scopes — a stopped transport should read empty,
and neither feeds a gain.

**Threading was checked, not assumed.** This is the standing `THREAD_MODEL.md` already gives
`prepareToPlay` (its "Host prepare thread" row): a format callback the host contract guarantees is
not concurrent with `processBlock`, which is why JUCE's own wrappers call it unguarded, and the VST3
wrapper takes `pluginInstance->getCallbackLock()` around the process call itself (`VST3.cpp:3714`).
Nothing is added to the audio path; no lock, wait or async hop is introduced. This was also checked
against the lint rather than argued: `scripts/check-realtime.py` seeds the **bare name** `reset` in
its `AUDIO_FN` set and scans `src/` over both `.cpp` and `.h`, with `juce::ScopedLock|CriticalSection|SpinLock`
on its forbidden list — so the override *is* inside the scan, and a `getCallbackLock()` version of
this fix would have been rejected by `source-lint`. The bare form passes with 0 violations.

**No latency interaction**, also checked rather than assumed: `reset()` contains no latency write, and
the re-report is raised by the *parameter* (`PluginProcessor.cpp:41` via `onOversampleChanged`, and
`:381` in `parameterChanged`), not by the engine's duck adoption — so a reset that resolves a pending
oversampling target cannot leave the reported value stale. The reported latency is untouched.

### Confidence

| | |
|---|---|
| implementation | **Verified** — no override; one engine caller, in `prepare()` |
| trigger | **Verified** — two wrapper call sites quoted, neither followed by a prepare |
| impact | **Verified at the wrapper level** — measured tail peak 2.125 into silence without the reset, **0.000** with it. Per-host frequency: **not established**, and not claimed |

---

## E. F3 — reported tail: **CONFIRMED, and larger than the prior figure**

### Contract interpretation

`getTailLengthSeconds()` is pure virtual, documented only as *"the length of the processor's tail, in
seconds"* — but the consumer settles the semantics. The VST3 wrapper converts it directly:

```cpp
// juce_audio_plugin_client_VST3.cpp:3482-3493
Steinberg::uint32 PLUGIN_API getTailSamples() override
{
    auto tailLengthSeconds = getPluginInstance().getTailLengthSeconds();
    if (tailLengthSeconds <= 0.0 || processSetup.sampleRate <= 0.0) return Vst::kNoTail;
    if (std::isinf (tailLengthSeconds))                             return Vst::kInfiniteTail;
    return (Steinberg::uint32) roundToIntAccurate (tailLengthSeconds * processSetup.sampleRate);
}
```

It is therefore a **bound**, and it may be conservative: under-reporting makes a host stop pulling
`processBlock` while the chain is still sounding — a truncated decay on a freeze, bounce or render;
over-reporting only costs some processing after the source has stopped. The safe direction is up.

This is **not** the reported latency, and `ARCHITECTURE_REVIEW_GATE.md`'s gated item is
*"Latency change — sources, engagement condition, or reported value (`LATENCY_MODEL.md`)"*.
`LATENCY_MODEL.md` does not contain the word "tail". Confirmed before editing, as the roadmap's own
note asked.

### Measured

2 s of white noise (not a tone — a tone can sit in a band the slowest filter barely sees), then pure
silence; the figure is the last sample above a floor relative to the steady peak that produced it.

| configuration | 44.1k | 48k | 96k | 192k |
|---|---|---|---|---|
| shipped defaults | 0.000 | 0.000 | 0.000 | 0.000 |
| multiband on, default splits | 0.006 | 0.007 | 0.005 | 0.006 |
| 4 bands, every split at the 20 Hz floor | 0.105 | 0.108 | 0.107 | 0.086 |
| Haas, amount 1, delay 35 ms | 0.035 | 0.035 | 0.035 | 0.035 |
| Velvet, amount 1, density 1 | 0.045 | 0.045 | 0.045 | 0.045 |
| + Band Solo and Mid Solo | 0.173 | **0.198** | 0.174 | 0.176 |
| + Level Match, Mono Maker 20 Hz, Haas 35 ms, x8 OS, Drive, Mix 0.5 | 0.236 | **0.250** | 0.238 | 0.232 |

Floor sensitivity on the worst case at 48 kHz: **0.250 s** at −60 dB, **0.284 s** at −80 dB,
**0.318 s** at −100 dB.

**What sets it.** The binding element is the LR4 crossover bank at the 20 Hz floor of its own
parameter range — `logFreqRange (20, 20000)` (`PluginParameters.cpp:238-240`), reachable from the UI
and from automation, and preserved by `setCrossovers`' `[20 Hz, 0.45*sr]` clamp at every rate. Being
a filter its decay is set in Hz, so the figure is **sample-rate independent**, which the table
confirms. Band Solo roughly doubles it because the SoloMonitor mirrors the same splits into a
**second bank in series**; the fixed-time elements (the 1–35 ms Haas line, Velvet's sparse FIR, the
oversampling wrap) then add on top. `shipped defaults` reads 0.000 s because the default amount is 0
and the multiband is off — which is exactly why a 0.1 constant survived this long.

The prior review's ~0.18 s was in the right region and low: it had not combined the two crossover
banks with the fixed-delay elements.

**Independently corroborated.** An adversarial pass transcribed `MultibandWidth`'s *summed*
reconstruction topology verbatim and measured its t60 at the same reachable floor as **102.1 ms** —
against the 0.108 s this round measured end-to-end on the processor's own output for that
configuration. Two different instruments, agreeing to ~6%. (That pass's objection was to a
*different* set of figures, derived from a single band normalised to its own peak, which the
multiband's flat summation makes unrepresentative of what the plug-in emits. The measurement used
here never had that problem: it reads the processor's output.)

**One alternative considered and rejected.** The tail could be *derived live* from the current
parameters instead of reported as a bound — and it was checked that this would not upset the host:
the pinned JUCE raises no restart flag for a tail change (there is no `kTail`-style flag anywhere in
its `modules/` tree, unlike latency). It is rejected anyway, because a figure that moves under
automation is more moving surface than a bound needs, and the contract only asks for a bound.

### Decision: confirm and fix

`0.1` → `0.5`, with the derivation and the whole table in the code comment.

**Why 0.5 and not 0.318.** A sweep finds the worst case it was pointed at, not the supremum over a
continuous parameter space, so the reported number carries an explicit ~1.6× margin over the deepest
floor measured instead of being pinned to one measurement. Over-reporting is the safe direction, and
0.5 s is still short enough not to waste a host's time. The margin is stated rather than implied, and
State test 119 asserts the direction that actually matters.

### Confidence

| | |
|---|---|
| implementation | **Verified** — `return 0.1;` was a constant with nothing behind it |
| trigger | **Verified** — every setting in the worst case is at a bound the parameter allows |
| impact | **Partially Verified** — the under-report is measured and the wrapper's use of it is quoted; no host was driven to observe a clipped render, and that is not claimed |

---

## F. Part 6 — cross-subsystem interaction

Checked, with one real connection found and one grouping corrected.

- **F2 and F3 are two halves of one host contract, and JUCE says so.** `reset()`'s documentation is
  about stopping *"any tails … left running"* — the same tail `getTailLengthSeconds()` reports.
  Before this round the plug-in **under-reported the tail and ignored the request to stop it**. That
  is a genuine shared subject. It does **not** imply a shared fix: one is a missing override, the
  other a returned constant, four lines apart in the same header and independent. No abstraction was
  created.
- **F4 shares nothing with either**, and the Global Review's "host contract and user data" grouping
  was partly wrong on its own terms: it described three small diffs on *`AudioProcessor` members
  other than `processBlock`*, and F4 is not an `AudioProcessor` member at all — it is message-thread
  file I/O in `PresetManager`. The grouping was a scheduling convenience, and is recorded as such.
- **F4 does interact with the UI dirty state and the preset baseline**, and that is the point of the
  fix rather than a side effect: pre-fix a failed write went on to advance `current`, `sel` and
  `sigAtLoad` and to fire `onSaved` (which re-bases the processor's undo snapshot). Post-fix
  `writeUserPreset` returns before all of it. Measured, not assumed — see the table in §C.
- **F2 against the latency rings and the reported latency**: no interaction; `reset()` performs no
  latency write and the re-report is parameter-driven (§D).
- **F2 against state save/restore**: none. `reset()` touches engine DSP state only — no APVTS, no
  program metadata, so none of the ADR-0036 exchange machinery is involved.
- **F2 against ADR-0007 (Level Match) — the one interaction that changed a fix.** The wholesale
  engine flush clears the loudness integrators, which ADR-0007 requires to hold across silence. This
  was not visible from F2's own subsystem list and was found by attacking the fix rather than the
  finding. It is why `ResetScope` exists. See §D.
- **F2 against R4's `pendingAlgoReset` fix**: consistent. `reset()` resolves a pending target
  (`p = pendingP`) before clearing the flag, and Test 57 still passes.

---

## G. Changes made

| file | change |
|---|---|
| `tests/dsp_tests.cpp` | the dead lambda capture that failed the `linux` job, and a dead local, both from R4 |
| `src/PresetManager.cpp` | `writeTextVerified` — the atomic preset write, checked (F4) |
| `src/dsp/AnamorphEngine.h`, `.cpp` | `ResetScope` — `everything` (the existing behaviour, still the default) vs `audioTailsOnly`, which spares the Level-Match integrators per ADR-0007 (F2) |
| `src/PluginProcessor.h` | `void reset() override` routing to `ResetScope::audioTailsOnly` (F2); `getTailLengthSeconds()` 0.1 → 0.5 with its derivation (F3) |
| `docs/.../ADR-0007-levelmatch-measure-predict.md` | a dated note that the new host-reset path deliberately does not clear the measure |
| `tests/state_tests.cpp` | State tests 117, 118, 119 |
| `docs/architecture/THREAD_MODEL.md` | a **Host reset** row for the new entry point |
| `docs/architecture/SERIALIZATION_REGISTRY.md`, `docs/FUTURE_RISKS.md`, `scripts/check-citations.py` | five declared re-aims and one glossed citation re-derived by hand against the shifted files |
| `CHANGELOG.md` | three user-visible `[Unreleased]` entries |
| 12 documents | ordinary evidence anchors re-anchored by `check-citations.py --fix` (21) |

**Not changed, deliberately:** the atomic-write strategy (same `TemporaryFile`, same flag, same
argument defaults, same bytes), the loader's acceptance rules (ADR-0055), parameter IDs, the
serialization schema, the threading model, DSP ordering, and the reported latency. No
`ARCHITECTURE_REVIEW_GATE` item is triggered: F4 touches no serialization contract (ADR-0055 governs
what the loader *accepts*; no document governs the write path), F2 adds a host callback under a
contract `THREAD_MODEL.md` already states, and F3 is not a latency change.

### Regression protection

| test | asserts | without its fix |
|---|---|---|
| **117** `testAFailedPresetWriteReportsFailure` | a save with room succeeds and parses; a write the kernel cuts short reports **failure**, leaves the existing preset byte-for-byte unchanged and still parsing, and leaves the sound **dirty** | **5 checks fail** |
| **118** `testAHostResetReachesTheEngine` | through the **processor**: a tail carries into silence without a reset (control), and does not with one; processing still works afterwards; a reset before `prepareToPlay` is harmless; and the Level-Match measurement **survives** the reset (ADR-0007) | **1 check fails** without the override; the ADR-0007 leg fails against the wholesale flush (−5.158 dB → 0.000 dB), so the narrowing is held in place too |
| **119** `testTheReportedTailCoversTheRealTail` | at the worst-case configuration, across 44.1/48/96 kHz, the reported tail is **never shorter** than the measured tail, and is still a bound | **1 check fails** |

Test 117's failure injection is `RLIMIT_FSIZE`, chosen because it fails at the **same syscall** the
real thing does — `FileOutputStream::writeInternal`'s `::write` — and because it reproduces the
short-write case a status check cannot see. It needs no root and no mount. It is POSIX-only and is
compiled out on Windows, where the test says so rather than reporting a pass it did not perform; the
success leg runs everywhere.

Each control check passes in both directions, so none of the three tests is vacuous.

---

## H. Validation

| | |
|---|---|
| DSP suite | **469 / 0** (unchanged — this round adds no DSP test) |
| state suite | **4677 / 0** (was 4654; 23 new checks) |
| pre-fix demonstration | all three fixes reverted together: **7 failures**, in three attributable groups (F4 ×5, F2 ×1, F3 ×1) |
| `check-realtime` | 0 violations, self-test 93 cases |
| `check-dispatch` | every dispatch bracketed, self-test 52 cases |
| `check-portability` | 0 violations, self-test 120 cases |
| `check-docs` | 147 files clean, self-test 464 cases |
| `check-citations` | 531 anchors intact, **self-test 242 cases** |
| the `sanitizers` job | red on the first R5 push (an ASan stack overflow in a pre-existing test, caused by this round's stack-local processors) and fixed by following the heap rule above; **not reproducible locally** — CI is the verification |
| a Release state run under `ulimit -s 1024` | 4677 / 0 — run because `TESTING.md` prescribes it for this class, and reported even though it did **not** catch the defect |
| the repaired `linux` job | **green in CI** on `ef75b8c`, and its warning gate demonstrated to still reject the class, to still refuse a mismatched clang major, and to have an unwidened baseline |

**What adversarial verification changed.** This round's three investigations were each attacked from
three independent lenses. The traces' own verdicts matched the conclusions above, but the attacks
produced one change of substance and two facts worth keeping: the **F2 fix was wrong as first
written** and had to be narrowed for ADR-0007 (§D); the F4 analysis gained the `close()` discard and
the missing `fsync` (§C); and F3's figure gained an independent corroboration (§E). One challenger
claim was itself checked and **refuted** — that `AnamorphAudioProcessor::reset` would sit outside
`check-realtime.py`'s scan; the lint seeds the bare name `reset`, so it does not.

The citation self-test is called out because it briefly **failed** during this round — a hand-edit to
the declared re-aims left one glossed citation pointing at the wrong span, and the self-test is what
caught it. A gate whose liveness checks do not run is not a green gate, so it is reported here by
name rather than folded into a count.

**Not run, stated rather than implied:** Windows, macOS, MSVC, AppleClang, TSan, RTSan, valgrind,
`pluginval`, the sanitizer lanes, and the GCC warning gate (it needs a gcc-16 log this container
cannot produce). Linux GCC 13 Release, plus clang-18 for the one diagnostic, is the whole of what was
exercised locally; the rest of the matrix is whatever CI reports on the pushed head.
`scripts/preflight.sh` skips its suite half here because the build tree is outside `./build`, so both
suites were run directly instead — that is a real gap in the preflight run and is not counted as
coverage.

---

## I. Remaining findings, and the road map

### Open

| finding | state |
|---|---|
| **R4's `pendingAlgoReset` companion** | **FIXED in R4, and open only as unmerged work** — the fix and Test 57 are on this branch, in PR #155. It is not closed by this round and is not being quietly dropped: it is one of the three R4 changes waiting on that PR. |
| **F9** — a restore adoption under the held `soundReplacement` lock | open, untouched |
| **F10** — a re-entrant `mouseUp` during `beginBandMove` leaks two host gestures | open, untouched |
| **F12** — `advancedMode` is host-writable and resizes the editor synchronously | open, untouched |
| **F13** — Level Match state survives transitions that should invalidate it; its tap has no non-finite guard | open, untouched |
| **F14** — the engine-wide NaN/Inf self-heal is never entered by any test | open, untouched |
| **F15** — coverage holes in production-reachable paths | open; R5 narrows one edge (State test 119 drives the 20 Hz crossover floor and the SoloMonitor's second bank, which nothing previously did) |
| **F16** — documentation and register drift | open; R5 adds nothing to it and clears two items (the `THREAD_MODEL` entry point, five stale declared re-aims) |
| **F7**, **RISK-010**, **RISK-015**, the gzip hypothesis, ADR numbering | preserved as no-action; nothing in this round's evidence bears on any of them |

### Closed by this round

F4 (confirm and fix), F2 (confirm and fix), F3 (confirm and fix, with the figure revised upward from
the prior estimate), and the `linux` CI failure.

### What should be next, from this round's evidence

**R6 — mechanical enforcement of invariants held by hand-maintained lists — and the evidence for it
is now this round's own, not an inherited priority.**

R5 was three unrelated defects on paper. Two of them turned out to share a single shape: **a contract
stated in prose with nothing checking it.**

- `PresetManager.h` states that a write failure "travels on `onComplete`". Nothing asserted it, and it
  was false for the one failure mode that matters.
- `AnamorphEngine::reset()`'s comment states that it exists so "a host reset lands in a clean steady
  state". Nothing asserted it, and no host reset could reach it — it had one caller, inside
  `prepare()`.
- `getTailLengthSeconds()` returned a bound with no derivation and no check, and was wrong by 2.5×.

And this round produced a fourth instance from the other direction: the `linux` failure was a
**compiler-coverage** gap of the same kind — an invariant (no new first-party warnings) that one
toolchain checks and the toolchain used locally cannot. R4 had already contributed two instances from
the four hand-maintained `EngineParameters` field lists. That is six, across three rounds, all the
same shape.

R6's work list should therefore be extended with what this round found:

1. a lint that every `EngineParameters` field appears in `sameParameters` (from R4);
2. a lint that every write to `pendingP` is followed by the same derivation (from R4);
3. **a lint that no test declares an `AnamorphAudioProcessor` as a stack local** — the rule
   above State test 59 is measured, load-bearing, and was still ignored by three new tests
   in this very round, with the failure landing in an unrelated pre-existing test in a job
   that cannot be reproduced locally. It is a one-line grep with an obvious exemption list,
   and it is now the item with the freshest evidence behind it;
4. **a check that the one preset write path is the verified one** — trivial to state as a lint
   (`replaceWithText` must not appear in `src/`), and it is the cheapest of the six;
5. **the prime/prepare ordering hazard from R4** belongs here too — `primeParameters` before
   `prepare` is required, documented in a comment, and honoured by the single production caller;
   the reverse order silently produces a mis-configured engine, which cost R4 a published refutation.

**R5's own leftovers are deliberately not promoted.** R7 (coverage) and R8 (documentation) are
unchanged in position: both are additive, neither is a moving risk, and R6 is what would have caught
four of this round's six instances before they shipped.

**What is *not* recommended, and why.** Reaching for F13 next on grounds of severity would be
inherited ordering, not evidence: nothing in R4 or R5 touched Level Match, its confidence labels are
unchanged, and no new measurement bears on it. It stays where the Global Review put it.
