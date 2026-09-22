# R7 — production paths no test had run (PR #155)

2026-09-22 · branch `claude/anamorph-comprehensive-review-90tpty` · PR #155 · entered from `f8e6630`
(the PR's finalization: the owner's architecture approval of the R9 reset change recorded in
ADR-0007, the five anchors this PR broke repaired, the final reset-semantics check — all in
`R6_HOST_RESET_SCOPE_AND_STATE_COVERAGE.md` §T).

The road map's R7 item (`GLOBAL_REVIEW_v0.9.9_INVESTIGATION.md` §4) listed five pieces of work
against F14 and F15. It was **not** implemented as a list. Each candidate — those five, plus the
three the round's brief added (the re-prepare Level-Match gain, the ppq/no-position transport
branches, latency under oversampling) — was re-measured on the current tree first, and the
evidence decided whether it became work.

## A. Method

- **Coverage was measured, not grepped.** gcov on `--coverage -fno-inline` builds of both suites
  (GCC 13). "0" below means the line or branch executed zero times across both suites.
- **Behaviour was measured through harnesses** linked against the suites' own objects, engines and
  processors on the heap (`sizeof (AnamorphEngine)` ≈ 138 KB; State test 59's note), and through
  the processor exactly as a host drives it wherever the question was a host's.
- **Every added test was shown live by mutation.** A mutant of the code the test guards, both
  suites run, the source restored byte-identically (`cmp`) and rebuilt. The counts below are the
  whole-suite failure counts under each mutant, so "1" means the new test's leg and nothing else.
- **Where it ran.** Linux x86-64, GCC 13, Release, JUCE 9.0.2; plus the ASan + UBSan lane
  approximated with Clang 18 (§J). Not run locally: MSVC, AppleClang, Clang 22, the TSan / RTSan /
  valgrind lanes, pluginval — those results are CI's, on the pushed head, and are not claimed here.
  (Two small TSan probes in §E4 ran locally, GCC 13's runtime.)

## B. Decisions

| Candidate | Before this round | Decision | Outcome |
|---|---|---|---|
| Re-prepare keeps / clears the Level-Match gain | Pinned: State test 120 leg 2 (§T of the R6 worklog: removing both flushes fails it; either alone is inert, because `prepare()` flushes twice) | **Reject** — already covered | none |
| F14, a non-finite **audio** burst | The ADR-0009 guard's scrub-and-reset block: **0** | **Proceed** | Test 59 |
| F14, a non-finite **parameter** value | 0, and a **defect**: Haas / Velvet latch silent until a re-prepare | **Proceed, fix** | `reset()` reseed in both modules; State test 123 |
| F14, extreme **finite** input | Not ADR-0009's (valid audio passes untouched, as decided) | **Architecture decision** (ADR-0007) | recorded, §C3 |
| Bus-layout negotiation | `isBusesLayoutSupported`: **0** | **Proceed** | State test 124 |
| The mono → stereo up-mix | the `copyFrom` in `processBlock`: **0** | **Proceed** | State test 124 |
| Transport: the sample-clock path | Covered by State tests 120–122 | **Preserve** | none |
| Transport: the ppq fallback | **0** (no test called `setPpqPosition`) | **Proceed** | State test 125 |
| Transport: a playhead with no position | **0** | **Proceed** | State test 125 |
| Multiband bank crossfade | **Runs** (4 fades in the DSP suite — F15's "never executed" does not hold on this tree); its mid-fade `pendingJump` branch: **0** | **Preserve**; the `pendingJump` branch **deferred** | none |
| `ScopeBuffer::readLatest` | `readLatest` **0**; `pushBlock`'s second segment **0** | **Proceed**, single-threaded only; the concurrency half is a **finding** | Test 61; §E4 |
| Latency under oversampling | The engaged wrap's processed-path delay: measured by nothing; the reported numbers: pinned by nothing | **Proceed** | Test 60 |

## C. F14 — ADR-0009's self-heal

### C1. A non-finite burst in the audio (Test 59)

Twin engines, A fed one block with NaN (L) / +Inf (R) on every 7th sample, B the same block with
those samples at 0; four algorithms × Oversampling Off / 2× × Level Match off / on; Drive 8 dB,
Width 1.6, Mix 0.8, Multiband and Mono Maker on. Measured (worst per-block |dB| vs the twin from
10 blocks after the burst):

| | Haas | Velvet | Chorus | Dim-D |
|---|---|---|---|---|
| Level Match off, OS off / 2× | 0.00 / 0.00 (back within 0.1 dB at +4) | 0.17 / 0.18 (within 0.1 dB at +13) | 0.99 / 0.80 | 1.80 / 1.29 |
| Level Match on, OS off / 2× | 0.51 / 0.42 | 0.65 / 0.58 | 1.88 / 1.57 | **2.83** / 2.66 |

No non-finite output block and no non-finite published gain anywhere. Chorus and Dim-D restart
their LFO phase in the self-heal, and with Level Match on A re-converges from a cleared matcher;
that is why the bound is 6 dB (a latched chain reads −180) and the 0.1 dB return is asserted only
for Haas and Velvet with Level Match off.

| Mutant (in the guard) | Test 59 failures |
|---|---|
| Ma: every module reset removed | 3 — silent for good, gain NaN |
| Mb: only `loudness.reset()` removed | 2 — gain NaN for good; with Level Match on, silence |
| Mc: the scrub removed | 4 — every block after the burst non-finite |

Mb is also F13's third sub-finding measured: `LoudnessMatch` has no non-finite guard of its own,
and the self-heal's `loudness.reset()` is what stands in for one. Test 59 now pins that.

### C2. A non-finite parameter value — the latch, and the fix (State test 123)

**Reachable.** JUCE's VST3 wrapper delivers a host's parameter change as
`setValueAndNotifyIfChanged` → `setValueNotifyingHost (v)`; its `approximatelyEqual` early-out is
false for NaN, `NormalisableRange::clampTo0To1` is `jlimit`, which passes NaN, the APVTS listener
stores it in the raw atomic, and `toEngine()` forwards it: `EngineParameters::algoAmount == NaN`.
(A bare `setValue (NaN)` does NOT reach the raw atomic — no listener runs — which is how this
round's first harness came to measure nothing.) A host that sends NaN is buggy; ADR-0009 exists
for that class.

**The latch.** Haas and Velvet glide their wet amount, `currentAmount += k·(target −
currentAmount)`. One NaN target makes `currentAmount` NaN, and no later target can bring it back.
The guard catches the non-finite output every block, zeroes it and resets the modules — but
neither module's `reset()` touched the glide. Only `prepare()` reseeds it (`snapToTargets()`).
Measured through the processor, pre-fix (the level of the same material before the NaN in brackets):

| | 1 s after the host is finite again | then a stop (host reset) and play | then a bare host reset | then a re-prepare |
|---|---|---|---|---|
| Haas (−11.70 dB) | **−180.00** | **−180.00** | **−180.00** | −11.53 |
| Velvet (−10.02 dB) | **−180.00** | **−180.00** | **−180.00** | −9.36 |

Chorus and Dim-D recovered (no such glide). Post-fix, the same harness: Haas −11.83, Velvet −9.80
one second after, and every column recovered. This is the global review's root cause 5 — one
module-reset list serving two recovery paths — in its measured form.

**The fix.** `HaasProcessor::reset()` and `VelvetNoise::reset()` reseed a **non-finite** glide to 0
and leave a finite one alone. 0 is parked identity: while the host still sends NaN, both modules'
parked gates admit a NaN target once the glide is 0 (`! (std::abs (amount) > 0)` in Haas, `!
(targetAmount > 0)` in Velvet), so the stage holds identity instead of re-poisoning; when the target
is finite again, the ordinary glide walks back in. Finite state is untouched, so every other caller
of `reset()` — `prepare()`, the switch duck's bottom, the host reset — is bit-identical, and both
suites' existing checks pass unchanged.

| Mutant | State test 123 failures |
|---|---|
| neither reseed (the pre-fix tree) | 2 — Haas and Velvet |
| Haas's reseed only removed | 1 — Haas |
| Velvet's reseed only removed | 1 — Velvet |

**Hard-stop classes: none.** No parameter, schema, signal order, reported latency or threading
change; `reset()` runs where it always ran.

### C3. Extreme but finite input — recorded, not acted on

ADR-0009 passes valid audio "however loud" untouched, and the guard does not fire. The chain itself
recovers within 0.02–0.09 s of one such block (Level Match off). **Level Match does not**: twin
engines, Haas, Drive 8 dB, one 256-sample block at ±magnitude, published-gain distance from the
twin:

| Burst | +20 dBFS | +60 | +120 | +200 | +400 | +770 (3e38) |
|---|---|---|---|---|---|---|
| worst | 8.73 dB | 19.75 | 20.16 | 20.18 | 20.18 | 2.59 |
| last > 0.5 dB | 3.45 s | 7.13 | 12.68 | 20.04 | 38.47 | 1.24 |

**Drive is what makes it large.** With Drive at 0 the same +20 dBFS block moves the gain 0.39 dB and
+60 dBFS 0.70 dB; at 8 dB, 8.73 and 19.75. The burst reaches the reference tap raw and the
processed tap saturated, and the energy-integrating matcher remembers the difference for its window.
That is the matcher measuring a real, momentary, nonlinear loudness difference with a long memory —
whether it should reject such outliers is ADR-0007's question, not a defect to patch here.

## D. F15 — the paths

### D1. The documented I/O contract (State test 124)

The README ("stereo → stereo and mono → stereo (output is always stereo; mono → mono is not
supported)") and `COMPATIBILITY_MATRIX.md` ("mono duplicated to both channels"), asserted through
the real negotiation and the real `processBlock`: the four layouts accepted or refused as written,
and a mono → stereo processor, fed junk in its output-only second channel, bit-identical to a
stereo → stereo one fed L = R — with widening engaged, and a control that the output really is wide.
Nothing beyond the documents is asserted (surround and disabled-input layouts are refused by the
code too, but no document says so).

| Mutant | State test 124 failures |
|---|---|
| `isBusesLayoutSupported` refuses a mono input | 3 |
| the up-mix `copyFrom` removed | 1 — the junk reaches the chain |

### D2. The transport machine without a sample clock (State test 125)

A ppq-only playhead at 123.4 BPM (a fractional number of samples per block, so the derived position
truncates differently each block), and a playhead that reports play state only. Continuous playback
keeps the held peak; a seek clears it (ppq only); a stop keeps it and the restart clears it.

**The rig had to call `setRateAndBufferSizeDetails` before `prepareToPlay`**, as every JUCE wrapper
does. Without it `getSampleRate()` is 0, every derived position is 0, no jump is ever seen — and the
continuous leg passes for the wrong reason. Found because the seek leg failed on the correct tree.

| Mutant (in `processBlock`) | Failures | Leg |
|---|---|---|
| MT1: the ppq fallback removed | 1 | ppq: a seek clears the held peak |
| MT2: `/ bpm` dropped from the conversion | 1 | ppq: continuous playback does not read as a seek |
| MT3: `* getSampleRate()` dropped | 1 | ppq: a seek clears the held peak |
| MT4: a host with no position is treated as not playing | 1 | play state only: the restart clears it |
| MT5: a host with no position is treated as a seek | 1 | play state only: continuous playback |

State tests 120–122 pass every one of these mutants; only State test 125 sees them.

### D3. Latency under oversampling (Test 60)

Tests 3+4 and 52 both measure a ring delayed **by** the reported number (the bypass ring, the
skipped-wrap stand-in) — they check a ring against the number, never the oversampler against it.
**Measured, O1 below passes both suites**: the oversamplers built without JUCE's integer-latency
flag — JUCE's own default for that argument, one careless refactor away — move the reported latency
4 / 6 / 6 → 3 / 4 / 5 (a hard-stop class) and leave the engaged wrap 0.137 / 0.433 / 0.049 samples
off the number it reports.

Probe, correct tree: the reported latency is **4 / 6 / 6 at 44.1, 48, 88.2, 96 and 192 kHz alike**
(`LATENCY_MODEL.md` said the counts depend on the sample rate; corrected with this measurement, and
its TODO answered), and the engaged wrap's phase delay at 300 Hz equals it within 3e-4 samples.

Test 60, at 44.1 / 48 / 96 kHz: the reported numbers pinned; the engaged wrap's delay within 0.01
samples of them; and a control — at 0.35 fs the engaged chain's delay differs from the skipped
chain's by 0.88 / 0.44 / 0.84 samples, the half-band IIR's own phase — so the delay check cannot pass
through the stand-in ring.

| Mutant | Test 60 failures | |
|---|---|---|
| O1: integer latency off | 2 | the pin and the delay |
| O2: integer latency off, the reported numbers hard-coded back to 4 / 6 / 6 | 2 | the delay (and, incidentally, the control) |
| O3: Drive never engages the wrap | 1 | **the control, and nothing else in either suite** |

O3 is a finding in its own right: with the engagement predicate broken so Drive never runs the
wrap — the shaper then runs without oversampling, which is what the setting exists to prevent —
**both suites passed**. The control is now the only check that Drive engages the wrap at all.
Nothing measures the aliasing reduction itself; recorded in §H.

### D4. The scope ring (Test 61)

Both GUI views' freshness scans depend on `readLatest (dst, n)` returning the newest n frames,
oldest first (`SpectrumImager::pushFFT` scans the window's last `freshN` frames for its silence
tracker). A 441- or 480-frame host block straddles the 16384-frame ring every few dozen blocks; no
suite block size does. Test 61, single-threaded: a ramp over 2.7 laps of 441-frame blocks, every
count the GUI uses, the clamps, and a block larger than the ring.

| Mutant (in `pushBlock`) | Test 61 failures |
|---|---|
| S1: the second copy segment removed | 3 |
| S2: an oversized block placed from the old index | 1 — its own leg |

### D5. Multiband bank crossfade — preserve

The fade path runs in the DSP suite (4 fades under gcov), so F15's "never executed" is out of date.
The branch that remembers a step arriving **during** a fade (`pendingJump`) runs in nothing.
Deferred: it needs a second > 1.5-octave host step inside one ~12 ms fade; if it broke, that second
step would drain through the ~4 oct/s glide over seconds instead of fading — slower, not unsafe —
and nothing measured points at it. Revisit if a fade defect is ever reported.

## E. The ScopeBuffer's cross-thread half — a finding, not a test

### E1. Why no concurrent test

The published contract (THREAD_MODEL.md) is one release-store of the write index per block and an
acquire in the reader, so a reader never copies a frame above the index it read. No deterministic
test can observe that ordering on x86-64, and a timing-based one would be the fragile kind the brief
rules out.

### E2. What a concurrent reader actually is

Formally a data race once the writer laps it: the index orders the writer's stores before the
reader's loads, but nothing orders the reader's loads before the writer's **next** overwrite of the
same frames. `THREAD_MODEL.md`'s "No direct cross-thread access to non-atomic shared state" does not
describe this. In practice it is benign — aligned 4-byte float accesses, display-only data, and a
torn window needs the message thread pre-empted mid-copy for about (16384 − 8192) / fs ≈ 170 ms at
48 kHz — and the vectorscope and imager redraw it on the next tick.

### E3. Decision

**Record; no code change.** Closing it would change the threading model (a hard-stop), for a
display-only effect. The owner decides whether `THREAD_MODEL.md` should say it.

### E4. Why no TSan lane has reported it — measured

A writer at 441-frame blocks and a reader peeking the newest 8192 frames, GCC 13 TSan: **no report**
over 8,230 reads and 81 laps. A minimal pair shows why: the same write-after-read reported once when
the writer stores element by element, and **zero** times when it writes through `memcpy` — which is
what `pushBlock` does since Wave 4. So a TSan job would not see this race even if one drove a
concurrent reader.

## F. Part 10 — the older items, reassessed on this round's evidence

| Item | New evidence from R7 | Decision |
|---|---|---|
| **F13 (1)** `matchGainSmooth` not snapped on a forced duck — a preset load / undo / redo carries the previous match gain | none; still unmeasured, and it touches every preset, undo and A/B with Level Match on | **investigate next** (§I) |
| **F13 (2)** continuous-only swaps do not re-arm the match | none | **architecture decision** (R6c, ADR-0007) — unchanged |
| **F13 (3)** `LoudnessMatch` has no non-finite guard | Test 59 mutant Mb: the self-heal's `loudness.reset()` is what stands in for one, and is now pinned | **preserve** — covered; revisit only if the guard's order changes |
| **F10** re-entrant `mouseUp` leaks two gestures | none | **investigate** — unchanged; R6b waits on it |
| **F9** adoption under the held lock | none | **investigate reachability** — unchanged |
| **F12** `advancedMode` host-writable, synchronous resize | R7's lifecycle items are done, which is what the R6 table said F12 was waiting behind | **investigate** after F13 (1): a host write with an editor open is now the cheapest unmeasured high-severity item |
| **R6a / R6b / R6c / R6d** | none | **defer** — unchanged (R6a's trigger; R6b after F10; R6c / R6d need ADR amendments) |
| **Scalar-state gap** in `check-state-coverage.py` | The NaN latch is a third instance of "state a recovery path does not reach" — but module-internal glide state, not an engine scalar, so the lint's target could not have seen it; State test 123 is what holds it | **preserve** as documented |
| **Citation debt not caused by this PR** | Three more stale anchors found (§H) | **defer** to the standalone documentation pass (road map R8) |
| **Vectorscope frozen frame** | §E: the scope ring is SPSC by contract; blanking still needs a second producer | **architecture decision** — unchanged |
| **Intermittent state failure** | No recurrence: every state-suite failure this round was a deliberate pre-fix or mutant run | **investigate on recurrence only** — unchanged |

## G. Part 11 — conditional membership

**Unchanged: informational, test-only.** Nothing this round showed Test 58 inadequate, found a way
for the condition to regress undetected, or found a mechanical representation the lint could check.

## H. Remaining findings, and drift reported rather than fixed

- **Level Match, extreme finite input** (§C3) — ADR-0007 decision.
- **The scope ring's lapped-frame overwrite** (§E) — a documentation or threading-model decision.
- **Nothing measures oversampling's aliasing reduction** — Test 60's control proves the wrap runs,
  not that it helps. A measurement (harmonic-alias level of a driven high tone, Off vs 2×) would
  settle whether a test is warranted.
- **The multiband `pendingJump` branch** (§D5) — deferred.
- **Stale anchors, present at the merge base:** `COMPATIBILITY_MATRIX.md`'s I/O rows (their
  `:76-86`, `:120-121`, `:204-208` into `PluginProcessor.cpp`); `DSP_ALGORITHMS.md`'s Haas `.cpp:62`
  and Velvet `.cpp:243-251`; `procedures/TESTING.md`'s "53 DSP tests". Reported, not rewritten.
  (Two anchors this round's own insertions shifted — `DSP_ALGORITHMS.md`'s Haas `.cpp:34-43` and
  Velvet `.cpp:99-180` — were re-anchored, and the citation gate re-anchored two tracked ones in
  `DOCUMENTATION_COVERAGE.md`, each read back against the code it named.)
- **This PR's own documentation gap:** `procedures/TESTING.md` has no entry for Tests 55–58 or State
  tests 117–122, and `DOCUMENTATION_COVERAGE.md` no pass entry for rounds R4–R9. R7's tests have both.

## I. What comes next

**F13 (1) — measure the match gain a forced duck carries through a preset load, undo or redo with
Level Match on.** It is the one open item that touches an everyday action (every preset, undo and
A/B with Level Match on), it has never been measured, and its fix — if the number warrants one — sits
in the same component as §C3's ADR-0007 question, so the two can be decided together. The state
suite already drives preset loads and undo through the processor; the measurement is a harness, not
a design.

After it: **F12** (a host write of `advancedMode` with an editor open — the cheapest unmeasured
high-severity item now that R7's lifecycle work is done), then **F10**'s re-entrant `mouseUp`
measurement, which R6b waits on. The documentation pass (R8) stays separate from code rounds.

## J. Validation, on the tree this round commits

- **Suites, Release (GCC 13):** DSP **492 / 0**, State **4 760 / 0**; the same again under
  `ulimit -s 1024`.
- **ASan + UBSan, approximating CI's `sanitizers` lane** — Clang 18, the job's flag set
  (`address,undefined,vptr,float-divide-by-zero,implicit-conversion,unsigned-shift-base,local-bounds,nullability`),
  `scripts/ubsan-ignorelist.txt` unchanged (JUCE reached through a `juce-src` path so its patterns
  match, as they do in CI), `halt_on_error=1`: DSP **490 / 0** (the two checks short are the
  allocation guard's malloc half, which ASan owns; the suite says so), State **4 760 / 0**, and no
  sanitizer report — so neither NaN test drives a non-finite value into a float→int conversion
  anywhere in the chain. **One local limitation, not this round's:** under Clang 18's ASan the state
  suite's `main` frame is ~13.6 MB (every test inlined into it), past the default 8 MB stack, so it
  ran at 64 MB. HEAD overflows identically; R7 adds ~4.3 KB to that frame. CI's Clang 22 lane runs
  the suite at the default stack and passed on `f8e6630`.
- **Stack (`-fstack-usage`, GCC):** suite maxima unchanged — State 709,760 B, DSP 289,440 B; the
  new tests' frames are at most 1,136 B, every engine and processor on the heap.
- **Lints (`scripts/preflight.sh`, exit 0):** `check-docs` (150 files), portability, realtime,
  dispatch and state-coverage, each after its self-test; the citation gate clean against HEAD (540
  anchors), the merge base (531) and `origin/main`, after re-anchoring the two it reported.
- **Warnings:** GCC 13 with the GCC gate's extra flags, and Clang 18 with JUCE's recommended set,
  over the four changed translation units: no warning on any added line. The Clang baseline counts
  per (flag, file), not per line, and the touched files' counts are unchanged.
- **Cost:** the DSP suite 2.8 → 3.5 s natively; the state suite's new tests are a small share of its
  25.5 s. CI's `sanitizers` job took 23 of its 45 minutes on `f8e6630` (valgrind 17).
