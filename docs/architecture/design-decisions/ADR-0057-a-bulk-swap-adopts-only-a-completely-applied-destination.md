# ADR-0057 — A forced bulk swap adopts only a completely applied destination

**Status:** **Proposed** (2026-09-27; **Architecture Review Gate TRIGGERED — implementation blocked**).
The semantic decision below is the owner's, made under the round's delegation (*"You are authorized to
make product/semantic owner decisions for this investigation"*). The architecture that implements it is
a **Thread Model change**: a new atomic ordering on the message → audio request word, and a new
message on that path. `docs/policies/ARCHITECTURE_REVIEW_GATE.md` gates it, and
`docs/policies/AI_AGENT_POLICY.md` and `docs/policies/THREADING_POLICY.md` (*Enforcement*) make it an
agent hard stop. The same delegation withheld exactly that: *"You are NOT authorized to bypass an
existing threading-model hard stop."* No production code changes with this ADR. It carries the
reproduction, the invariant, the options measured on a scratch prototype and a recommendation, and it
waits for human architecture review. The known issue is `docs/KNOWN_ISSUES.md` KI-032; the record is
worklog §T (`worklogs/NONFINITE_PARAMETERS_AND_F13.md`).

## Context

A **forced bulk swap** replaces the whole sound behind a masking duck:
- an A/B switch (`AnamorphAudioProcessor::abSwitchToAdopted`, `src/PluginProcessor.cpp:2177`);
- undo and redo (`src/PluginProcessor.cpp:2075`, `src/PluginProcessor.cpp:2106`);
- a user or factory preset load (`presets.onAboutToLoad`, `src/PluginProcessor.cpp:83`, then
  `PresetManager::applySoundTree`, `src/PresetManager.cpp:559`).

Each raises the duck first and then writes the sound one parameter at a time. The A/B switch requests
the switch (`src/PluginProcessor.cpp:2186`), captures the slot it leaves (`src/PluginProcessor.cpp:2187`)
and applies the destination (`src/PluginProcessor.cpp:2189`). The apply is JUCE's `replaceState`, one
`setValueNotifyingHost` per parameter whose value changes (`src/PluginProcessor.cpp:1106`), then
`reassertParameters` (`src/PluginProcessor.cpp:1113`).

The contract the duck exists for (feedback #1, ADR-0004) is that a bulk swap is applied *entirely* at
the silent bottom, continuous controls included, with the smoothers snapped. The engine keeps the source
live through the ~6 ms fade-out. On every block of the fade-out it records the snapshot the processor
built for that block (`pendingP = np`, `src/dsp/AnamorphEngine.cpp:826`). At the bottom it adopts the
last one (`p = pendingP`, `src/dsp/AnamorphEngine.cpp:1317`) and judges the destination's A/B
Level-Match record against it (`src/dsp/AnamorphEngine.cpp:1407`).

The request word is **relaxed** and carries no completion (`THREAD_MODEL.md`, the request-word row;
ADR-0007, Amendment of 2026-09-25, A/B provenance). So the adoption point is fixed in *audio* time,
two blocks after the request is taken at 256 samples. The application's end is fixed in *wall* time:
~0.1–0.3 ms here, and unbounded behind host notifications, contention or preemption. Nothing relates the
two.

ADR-0036 §24 accepted that *"a reader that samples parameters while a replacement is running still sees
part of the outgoing sound and part of the incoming one — JUCE offers no block-atomic parameter
snapshot and never has … What changes is that the mixture cannot SETTLE."* Its item 5 (§25) classified
a duck consumed before the parameters move as *"a masking miss, never a click."* ADR-0007's A/B
provenance revision of 2026-09-26 recorded the A/B form of it as O8(3), investigated and not changed.
The Devin review of `9e38310`, *"A/B switches can adopt incomplete slot state"*, reports it as a
defect.

## Problem (measured on `9e38310`)

**Reproduced through the processor** (worklog §T1; x86-64 Linux, Release, 48 kHz / 256). The message
thread calls `abSwitchTo` while an audio thread runs `processBlock` at a set multiple of real time. The
probe reads the engine's adopted snapshot at the forced bottom and compares it field by field with both
slots. Every write is timestamped where JUCE stores the raw value the audio thread reads. Six
combinations of changed parameters were tested, 40 trials per combination and speed:

| combination | 1× | 16× | 64× | 112× | 256× / free-running |
|---|---|---|---|---|---|
| Drive + Width | 40 complete | 40 complete | 39 complete, 1 source-only | 8 complete, 12 hybrid, 20 source-only | 0–1 complete, 39–40 source-only |
| Mix + Amount | 40 | 40 | 39 / 1 | 17 / 7 / 16 | 0–1 / 39–40 |
| Haas delay + side | 40 | 40 | 40 | 15 / 4 / 21 | 1–3 / 37–39 |
| Haas → Chorus + rate + depth | 40 | 40 | 40 | 15 / 4 / 21 | 0–1 / 39–40 |
| Multiband bands + split + widths | 40 | 40 | 38 / 2 | 39 / 1 hybrid | 39–40 complete |
| Eight fields | 40 | 40 | 39 / 1 | 0 / 23 / 17 | 0 / 0–2 / 38–40 |

The Multiband combination stays mostly complete because its blocks are heavier: its "free-running"
speed is a much lower multiple of real time.

- **Timings (medians).** The first destination write lands +99–157 µs after the call, after the
  leaving slot's capture. The last lands +103–166 µs, and `abSwitchTo` returns at +226–295 µs. The
  audio thread takes the request at its next block, and the bottom's snapshot is read two blocks
  later:

  | speed | bottom snapshot read at | relative to the writes |
  |---|---|---|
  | 1× | +13.3 ms | after them |
  | 16× | +0.84–0.89 ms | after them |
  | 64× | +0.19–0.23 ms | after them |
  | 112× | +0.12 ms (0.21 ms for the heavier Multiband blocks) | inside the writes: hybrids |
  | 256× / free-running | +0.065–0.09 ms | before the first write: the source whole |

- **The Devin case.** A 112× trial took the request at +27.6 µs and read the bottom's snapshot at
  +122.3 µs. Drive was written at +117.6 µs and Width at +125.5 µs. The bottom adopted B's Drive 12 with
  A's Width 1.0, and `abSwitchTo` returned at +260.3 µs.
- **At real time.** One message-thread stall of ~12 ms after the first destination write (a slow host
  notification, emulated) gave hybrids in 6–7 of 20 trials in every combination. Stalls of 2 ms and
  7 ms gave none.
- **Deterministic, field by field** (State test 139, committed). At every write position the bottom
  adopts exactly the partly written slot: the output from the bottom on is bit-identical to a twin
  whose destination holds exactly those parameters.
  - A same-rate `prepareToPlay` in the fade-in then **flushes** B's valid measured Level-Match record
    (−8.08 dB → 0). A complete switch keeps it (−8.79 dB). The restore was judged against the adopted
    state, so it was not measured.
  - A host reset (`src/dsp/AnamorphEngine.cpp:240`) and the prepare path's prime
    (`src/dsp/AnamorphEngine.h:123`) adopt the partly written slot too, if one lands inside the writes.
  - A factory preset load does the same. After write 17 of 34, the bottom adopted 4 of the preset's
    changed fields and 4 of the previous sound's (worklog §T1).
- **What the listener hears.** The fade-in plays the adopted state, then the late fields arrive as live
  edits: continuous ones glide, and a discrete one opens a second, ordinary duck.
  - The output differs from a complete switch from the bottom block on, by −0.8 to −6.2 dB relative to
    the signal over the fade-in.
  - It becomes bit-identical again 9 blocks after the bottom (State test 139 (C)).
  - It is not a click: every path is still ducked.
- **Not a data race.** Every shared access is a `std::atomic`: the APVTS values (sequentially
  consistent), the request word (relaxed), and engine state the audio thread alone touches.
  - ThreadSanitizer reported nothing in 120 instrumented trials, including the 100 from 16× up, all of
    which adopted the source whole.
  - The leaving slot's record is always the complete source, measured (every trial). A seq_cst load
    that sees any destination write synchronises with that write, so the request, sequenced before it,
    is taken no later than the same block.
  - The defect is **snapshot consistency**: every intermediate snapshot is a valid state, and the
    bottom adopts one neither slot holds.

**Root cause.** The engine adopts at a point it chooses in audio time, and nothing tells it the
destination is complete. ADR-0036's premise that *"the mixture cannot SETTLE"* holds for the parameters:
they end as the destination's. It does not hold for the swap. The forced bottom snaps the smoothers to
the mixture, resets the modules into it and judges the A/B record against it, so the swap's result
**is** the mixture.

## The invariant (owner decision, 2026-09-27)

**A forced bulk swap adopts the complete destination or nothing.** Wherever a pending forced swap is
adopted (its bottom, a host reset that completes it, the prime), the state adopted is one of two:
- the destination exactly as its application wrote it; or
- the source, still live, with the swap still pending.

It is never a mixture, and never the source taken for the destination. The destination's Level-Match
provenance is judged against the complete destination.

## Options

| option | adopts only complete destinations? | threading change | measured / argued |
|---|---|---|---|
| **A. A separate completion atomic** (release / acquire) and a bottom hold | yes | a new cross-thread path **and** a new ordering | works; two atomics and their sequence must stay consistent |
| **B. The two-phase request word** (B2): the request as today, a sequence-tagged completion stored with **release**, the per-block exchange with **acquire**, a bounded bottom hold | yes | a new ordering and a new message on the **existing** path | **recommended**; prototype below |
| **C. An immutable snapshot handed over** (a mailbox) | only with a completion too: the live parameters are still written one by one, and after the adoption the engine reads the late ones as edits | a new path, ownership and a lifetime on the audio thread | dominated by B; duplicates the parameter → engine mapping on the message thread |
| D1. The engine holds while the snapshot keeps changing | no: a stalled writer looks settled, and a burst sees no gap | none | rejected: no guarantee |
| D2. The engine compares against the destination's A/B record | no: no record after a Copy, a restore or a first visit; a Copy can make a mixture look complete | none | rejected |
| D3. Request after the writes | no: the writes go live unmasked, and the leaving capture records a mixture | none | rejected (§R7) |
| D4. `suspendProcessing` around the apply | yes | the audio thread contends JUCE's callback lock | rejected: a lock on the audio thread, and an unmasked gap |
| D5. A second forced duck after the writes | no: the first bottom still adopts the mixture | a new call site only | rejected: a mitigation |
| D6. A content hash of the complete destination in the request word | yes, until automation writes during the swap, after which it never matches | a new message on the path | rejected: automation defeats it, and it is a thread-model change anyway |

**Why every correct option is gated.** To adopt only complete states, the audio thread must learn that
the application ended, with *acquire* semantics relative to the parameter writes. A relaxed flag does
not order them.
- **In the C++ model:** a relaxed store sequenced after the writes creates no happens-before with the
  reader.
- **On ARMv8** (Apple Silicon, a shipped target): a store-release orders only the accesses before it,
  so a later relaxed store can become visible first.
- **x86-64 hides this.** It is TSO, so no x86 run can refute the argument.

Every message → audio path the thread model lists is relaxed (`THREADING_POLICY.md`, *Atomic usage
rules*). The APVTS values are sequentially consistent, but each orders only itself, and no parameter
marks the end of an application. The ordering-critical pairs run audio → GUI (the scope ring),
audio / host → message (D-1) and host ↔ message (D-2). None of them gives the audio thread an acquire
that a completion could ride on.

## Decision (recommended architecture; implementation blocked by the gate)

**B2, the two-phase request word:**
1. **Request, as today.** `requestAbSwitch` / `requestDuck` keep their relaxed CAS / `fetch_or`, plus a
   4-bit sequence number *s* (1–15; 0 keeps today's meaning for engine-API callers, who hand complete
   snapshots to `setParameters`).
2. **Completion.** After the application's last write, the site that raised the duck calls
   `completeBulkApply(s)`: a CAS storing a done flag and *s*, **memory_order_release**. The sites are
   the end of `applyStatePreservingView` (A/B, undo, redo), `PresetManager::applySoundTree` and the
   factory apply.
3. **Consumption.** `setParameters` and the prime take the word with `exchange(0,`
   **`memory_order_acquire`**`)`. A request with *s* ≠ 0 arms "awaiting *s*". A completion carrying
   *s* clears it.
   - Once it clears, the snapshot the processor builds **after** that acquire is complete: the next
     block's. The alternative is to read the word before `toEngine` in `processBlock`.
   - A completion that carries an older *s* releases nothing. This covers a host that pumps the message
     loop inside a notification and nests a second swap.
4. **The hold.** While awaiting, the forced bottom does not adopt. The duck stays at its silent (or
   dry-filled) bottom, one block at a time. A host reset completes nothing, and the prime keeps the
   source, with the swap pending in both cases.
5. **The bounded fallback: abort to the source.** At a cap (proposed: 0.5 s of audio at the bottom) the
   engine fades back in on the source, which is complete.
   - The swap and its A/B restore stay pending, and the live snapshot's partial values are not adopted
     as edits while they do.
   - When the completion arrives, a fresh forced swap adopts the complete destination.
   - The invariant therefore holds unconditionally. A mixture is never the fallback.

**Measured on a scratch prototype** of steps 1–4, A/B only (worklog §T5), on the same 6 combinations ×
12 speeds × 40 trials = 2,880 threaded trials:
- **Adoption.** 0 incomplete adoptions. Every destination was measured at its bottom and kept by a
  same-rate `prepareToPlay`.
- **Holds.** None engaged at 1–16×, so real-time behaviour is unchanged. From 64× up the median hold
  was 1–7 blocks, with a maximum of 66 blocks at 256× (352 ms of audio, 1.4 ms of wall time). No trial
  reached the cap.
- **The existing suites** pass unchanged: DSP 935 / 0 with its output identical in all 73 sections, and
  State 5521 / 0, differing only in the thread-timing counters it always does.
- **State test 139** inverts: 11 of its 26 checks fail on the prototype, every one a
  defect-characterizing check. (D) still passes, because the prototype did not implement step 4's reset
  and prime rule; this ADR adds it.

## Consequences

- **Real time without a stall: no change.** The application ends in ~0.3 ms, and the fade-out takes 6 ms
  of audio.
- **Burst processing** (offline renders, hosts that render ahead or subdivide their callback, a stalled
  message thread): the swap's silent or dry-filled bottom lengthens by the application's remaining wall
  time multiplied by the speed. Measured: a median of 1–7 blocks.
- **Level Match:** the destination's A/B record is judged against the complete destination, so a
  measured record stays measured through an A/B under burst processing (State test 139 (B), inverted).
- **Cost:** one acquire exchange per block, where there is a relaxed one now (the same locked
  instruction on x86-64; `ldaxr` for `ldxr` on ARMv8), and ~16 bytes of engine state. No allocation, no
  lock, no wait, no new thread.
- **Tests:** State test 139 becomes the regression. Legs (A), (B), (C) and (E) invert to "adopts the
  complete destination", and (D) inverts with the reset and prime rule. DSP Tests 70 and 72–74 keep
  sequence 0.

## Required changes on acceptance (the gate record)

| document | change |
|---|---|
| `THREAD_MODEL.md` | The request-word row gains the completion and its ordering. |
| `THREADING_POLICY.md` | The allowed-paths row, and a fourth ordering-critical pair in *Atomic usage rules* (the completion's release, the exchange's acquire). Its *Enforcement* says changing the policy requires an ADR; this is that ADR. |
| ADR-0007, A/B provenance | *"Only the two slot indices and a forget bit cross … no new … memory ordering"* becomes: plus a sequence and a completion, release / acquire. O8(3) closes. |
| ADR-0036 §24 and §25 item 5 | The residual closes for forced swaps (a host restore raises no duck and stays ordinary automation). |
| ADR-0004 | A forced duck's bottom may hold until the destination is complete. |
| `API_REFERENCE.md` | The engine's request API. |
| `KNOWN_ISSUES.md` | KI-032 resolved. |
| `CHANGELOG.md` | The known-issue lead of the release that ships the fix becomes a *Fixed* entry. |

## Why implementation stops here

Steps 2–3 add an atomic ordering (release on the completion, acquire on the per-block exchange) and a
new message to the message → audio path. `THREADING_POLICY.md` (*Enforcement*): *"A change to the
thread model, a new shared-state path, or a new atomic ordering triggers the Architecture Review Gate
and an AI Agent Hard Stop."*

Step 4 changes what the Accepted ADR-0036 §24 / §25 recorded as an accepted residual, and ADR-0007's
statement that the request word adds no ordering.

No Accepted ADR permits any of this. §R7 of the worklog classified B2 the same way. The delegation
authorised the product decision and withheld the threading one.

## Related code

- `src/PluginProcessor.cpp:2177` (`abSwitchToAdopted`), `src/PluginProcessor.cpp:2186` (the request),
  `src/PluginProcessor.cpp:2187` (the leaving capture), `src/PluginProcessor.cpp:2189` (the apply).
- `src/PluginProcessor.cpp:1087` (`beforeSoundReplacementWrites`), `src/PluginProcessor.cpp:1106`
  (`replaceState`), `src/PluginProcessor.cpp:1113` (`reassertParameters`).
- `src/PluginProcessor.cpp:2075`, `src/PluginProcessor.cpp:2106` (undo, redo), `src/PluginProcessor.cpp:83`
  and `src/PresetManager.cpp:559` (a preset load).
- `src/dsp/AnamorphEngine.cpp:611` (`requestAbSwitch`), `src/dsp/AnamorphEngine.cpp:743` (the exchange),
  `src/dsp/AnamorphEngine.cpp:826` (`pendingP = np` mid-duck), `src/dsp/AnamorphEngine.cpp:1286` and
  `src/dsp/AnamorphEngine.cpp:1317` (the bottom and its adoption), `src/dsp/AnamorphEngine.cpp:1407` (the
  A/B restore), `src/dsp/AnamorphEngine.cpp:240` (a host reset completing the swap).
- `src/dsp/AnamorphEngine.h:123` (the prime), `src/dsp/AnamorphEngine.h:198` (`requestDuck`),
  `src/dsp/AnamorphEngine.h:377` (the request word).

## Evidence + confidence

- **State test 139** (26 checks, committed): the field-level adoption, the Level-Match consequence, the
  settling, the host reset and prime, the algorithm-first order, and a threaded race for the `tsan` lane.
  **Verified.**
- **The threaded reproduction, the real-time stall, TSan and the preset load:** scratch probes, with
  their numbers in worklog §T1–§T3. **Verified (measured).**
- **The prototype:** scratch, not in the tree, measured in worklog §T5. **Verified (measured)** for the
  A/B path. The reset, prime, undo, redo and preset legs of the design are **reasoned**, not prototyped.
- **The ordering argument:** reasoned from the C++ memory model and the ARMv8 release semantics. It is
  **not observable on x86-64**, which is why no run here can confirm or refute it.
