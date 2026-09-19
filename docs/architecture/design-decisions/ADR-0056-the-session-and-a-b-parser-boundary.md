# ADR-0056 — The session and A/B parser boundary (RISK-014)

**Status:** **Proposed — awaiting the owner's decision.** Nothing in this ADR is implemented.
Narrowing what `setStateInformation` accepts is a semantic change to a contract
`docs/architecture/SERIALIZATION_REGISTRY.md` records, which
`docs/policies/ARCHITECTURE_REVIEW_GATE.md` gates as a *Serialization Registry change* and
`docs/policies/AI_AGENT_POLICY.md` makes an agent hard stop — the same gate ADR-0055 triggered for
preset files. ADR-0055's own scope ruling put these two paths **outside** that approval: *"Implement
preset-file protection only. Do not modify host session blob loading or A/B slot payload loading in
this round. Record those paths as a follow-up risk requiring separate compatibility review."* This
ADR is that review's evidence. The decision is the owner's and is recorded here when it is made.

## Context

ADR-0055 (0.9.9) put a byte-level boundary in front of `juce::parseXML` for `.anamorph` preset
files. Two other paths reach the same parser and were deliberately left alone, and
`docs/FUTURE_RISKS.md` records them as **RISK-014**:

1. **The host session blob.** `setStateInformation` → `decodeRestore`
   (`src/PluginProcessor.cpp:2747-2751`) → `AudioProcessor::getXmlFromBinary` → `juce::parseXML`.
2. **The A/B slot payload.** Inside the same decode, `readSlot`'s `adoptIfAnamorph`
   (`src/PluginProcessor.cpp:2858-2862`) calls `juce::parseXML (slotPayload)` on a **string
   attribute value** of the already-parsed session document — a second, independently framed XML
   document one level further in.

Round 50 measured both, on `de89b1a`, x86-64 Linux, Release, pinned JUCE 9.0.2, through the real
entry point. The reproducer is `AnamorphStateTests --risk014-probe <shape>` (see
`docs/procedures/TESTING.md`).

## Problem

### What the framing actually validates

`getXmlFromBinary` (`juce_AudioProcessor.cpp:968-980`) checks **two things**: that the chunk is
longer than 8 bytes and that its first four bytes are `magicXmlNumber` (`0x21324356`, the ASCII
`VC2!`). It then reads a stated length, and hands
`String::fromUTF8 (data + 8, jmin (sizeInBytes - 8, stringLength))` to `juce::parseXML`. There is no
size cap, no depth bound, no DOCTYPE rule and no encoding validation. A corrupt chunk carries the
magic number because the plug-in itself wrote it.

The A/B payload is not framed at all: it is whatever string the `slotAParams` / `slotBParams`
attribute holds, unescaped by the outer parse and passed straight to `parseXML`.

### The measured matrix

`drive` is the witness throughout: default `0.0`, carried as `0.9` plain (`0.0375` normalised), so
the observed value says whether the blob was refused or accepted **and applied**.

| Input shape | Session blob | A/B slot payload |
|---|---|---|
| nesting 2 500, 1 MB stack | accepted, 174 ms | accepted, 91 ms |
| **nesting 3 000, 1 MB stack** (a 21 KB chunk) | **SIGSEGV** | **SIGSEGV** |
| nesting 20 000, 8 MB stack | accepted, **12.2 s** | accepted, 6.2 s |
| **nesting 30 000, 8 MB stack** | **SIGSEGV** | **SIGSEGV** |
| **recursive `DOCTYPE`, 1 level** (ADR-0055's shape, ~150 B) | **no return after 60 s** | **54.1 s** |
| **recursive `DOCTYPE`, 2 levels** (~230 B) | **SIGSEGV** | **SIGSEGV** |
| recursive `DOCTYPE`, 3 levels | **no return after 60 s** | **no return after 60 s** |
| `SYSTEM` `DOCTYPE` + a real file on disk | refused; **file contents did not leak** | refused; **did not leak** |
| 1 / 16 / 64 / 128 MB of payload | accepted; peak RSS 14 / 72 / 265 / **521 MB** | accepted; 16 / 120 / 330 / **650 MB**, 25.6 s |
| **two complete documents** | **accepted, the first applied** | — (the outer parse already yields one) |
| **trailing prose, element and raw binary** | **accepted, applied** | — |
| **one NUL, then a second complete document** | **accepted, the first applied, the tail invisible** | — |
| **invalid UTF-8 in an attribute value** | **accepted, applied** | — |
| **a chunk truncated to half its length** | **accepted, half-applied** | — |
| mismatched tags | refused | refused |
| a stated length far larger than the chunk | refused | — |
| UTF-16 text under the UTF-8 frame | refused | — |
| a foreign root in the payload | — | refused (ER-STATE-02) |

Three findings in that table are worth stating in words.

**The crash and the hang are reachable on BOTH paths, at the same thresholds.**
`readNextElement`/`readChildElements` are mutually recursive with no bound, and
`XmlDocument::expandExternalEntity`'s `ent.indexOf (i + 1, ";")` — indexing by the DTD **token**
index rather than by the ampersand it just found — walks instead of terminating. A ~230-byte chunk
ends the process; a ~150-byte one never returns.

**The A/B payload AMPLIFIES depth rather than inheriting it.** In the `ab-deep` shape the outer
document is nested **three** deep and entirely well formed; the 3 000 levels live inside one
attribute value, invisible to the outer parse, and are only realised when `adoptIfAnamorph` parses
that string as its own document. **A depth cap applied to the session document alone would not have
refused any of the A/B rows above.**

**The external file read is NOT reachable on either path, and that is structural rather than
lucky.** `XmlDocument::getFileContents` opens nothing unless an `InputSource` is set
(`juce_XmlDocument.cpp:182-193`), and both paths reach the parser through `parseXML (const String&)`,
whose `XmlDocument (const String&)` constructor leaves it null (`:38`). Only the `File` overload
installs a `FileInputSource`, and that is what the **preset** path used to call. RISK-014's wording
claimed this shape for both paths; it is now corrected there.

### The trust boundary is not the one the risk record assumed

RISK-014 rated the likelihood low because *"the bytes come from the host's own project file rather
than from a file the user opens"*. The first half is true and the conclusion does not follow, on two
counts established from the API rather than from the name:

* **A `.vstpreset` is a file the user opens, and it lands here.** In the pinned VST3 SDK,
  `PresetFile::restoreComponentState` reads the `Comp` chunk and calls
  `component->setState (readOnlyBStream)`
  (`…/VST3_SDK/public.sdk/source/vst/vstpresetfile.cpp:470-475`); JUCE's `JuceVST3Component::setState`
  forwards it to `pluginInstance->setStateInformation`
  (`juce_audio_plugin_client_VST3.cpp:2822`). A user picking a preset in the host's browser, or
  dragging one onto the plug-in, therefore reaches exactly the same parser as a project open — with
  bytes from a file they obtained from somewhere. The AU (`ClassInfo`) and Standalone paths reach it
  the same way.
* **A project file is itself an exchanged document.** Sessions travel between machines and
  collaborators, are stored and synced, and can be truncated by a crash mid-save — which the
  truncation row above shows is accepted and half-applied.

The preset boundary was justified on "a file the user opens". By that criterion the session path is
inside the same boundary, not outside it.

### The compatibility question a preset does not raise

This is the reason ADR-0055 deferred rather than extended, and it is answerable now. Measured with
`--risk014-probe census`:

| Session | Chunk | XML | Document depth |
|---|---|---|---|
| this build, fresh instance | 10 438 B | 10 429 B | **3** |
| this build, after a save→restore→save round trip | 10 438 B | 10 429 B | **3** |
| `field_capture_v0_9_5.session` (a real v0.9.5 capture) | 10 629 B | 10 620 B | **3** |
| `legacy_v0_2_bare_apvts` (rule 3) | 268 B | 259 B | **2** |
| `legacy_pre_0_6_4_ab_slots` (rule 3) | 590 B | 581 B | **3** |
| `legacy_pre_0_8_4_view_params` (rule 3) | 740 B | 731 B | **3** |

The A/B payloads inside the v0.9.5 capture are **2 046** and **2 051** bytes, each an independent
document nested **two** deep with its own `<?xml …?>` header.

Two further facts bear on any proposed rule:

* **No session this product has ever written contains a `DOCTYPE`.** `copyXmlToBinary` writes
  through `XmlElement::TextFormat()`, whose `dtd` member defaults to empty
  (`juce_XmlElement.h:207`) and which neither `copyXmlToBinary` nor `ValueTree::toXmlString` sets.
* **A refusal already has a defined meaning and needs no new error surface.** `decodeRestore`
  returning `false` is the documented *"a chunk of neither recognised shape is not a restore at
  all"* outcome (`SERIALIZATION_REGISTRY.md`): not one parameter, not the Settings, not the A/B
  slots is touched, and the sound the user has stays. Unlike the preset path, no `PRESET UNREADABLE`
  state or user-facing report would be required.

What the census does **not** settle is an upper bound on a legitimate session. `presetName`,
`presetBaseline` and `presetUserFile` are variable-length, and the two slot payloads roughly double
the parameter block. 10.6 KB is the largest ever measured; what multiple of it is safe to call a cap
is a judgement, not a measurement.

## Options

Presented for the decision; **none is selected here.**

### A — Accept and preserve

Leave both paths as they are. Record the measured evidence in RISK-014 and close the risk as
*accepted*, on the ground that the host owns its own project integrity and nothing in the field has
produced such a chunk.

*Scope:* documentation only.
*Consequence:* a ~230-byte corrupt or hostile chunk inside a project or a `.vstpreset` ends the
host process; a ~150-byte one freezes the host's message thread indefinitely, on open. Both are
measured, both are trivially constructible, and neither leaves the user a way back.

### B — A boundary on the session chunk only

Apply a byte/text-level admission scan to the decoded session text, after `getXmlFromBinary`'s
framing and before `parseXML`, refusing a `DOCTYPE`, a nesting depth beyond a cap and a chunk beyond
a size cap.

*Scope:* lift the ADR-0055 scanner out of `PresetManager.cpp`'s anonymous namespace into a shared
header, one call site in `decodeRestore`, chosen limits, regression coverage, and a
`SERIALIZATION_REGISTRY.md` + `SESSION_COMPATIBILITY_POLICY.md` amendment.
*Consequence:* covers the session rows of the matrix. **Leaves every A/B row unprotected** — the
depth-amplification finding above means a well-formed, shallow session can still carry a 3 000-level
payload that segfaults, and a two-level entity bomb that hangs.

### C — A boundary on both, each over its own text

As B, plus the same scan applied to each slot payload immediately before `adoptIfAnamorph`'s
`parseXML`, with its own limits — the payload is its own document and its size and depth are its
own.

*Scope:* B plus the `readSlot` call site and its coverage.
*Consequence:* the only option that covers everything the investigation measured. A refused payload
already has a defined recovery: the slot stays invalid and `abEnsureInit()` re-seeds it from
`currentStateSet()`, which is precisely what an unparsable or foreign-typed payload gets today
(ER-STATE-02), so no new state is introduced.

### D — Refuse the shapes the writer can never produce, and cap nothing

Refuse a `DOCTYPE` and bound the depth, but impose no size cap.

*Scope:* smaller than B/C; no limit has to be argued against an unbounded history.
*Consequence:* closes the hang and the crash — the two outcomes that are unrecoverable — and leaves
the memory row open: 128 MB of input still costs 521-650 MB of RSS, which is survivable and
self-limiting in a way a SIGSEGV is not.

### E — Parse on a large-stack worker thread

Move the parse off the message thread onto a thread with a large stack, instead of bounding the
input.

*Consequence:* ADR-0055 already rejected this for presets, and the reasoning transfers unchanged: it
makes a corrupt document *survivable* rather than *refused*, answers neither the entity hang nor the
memory growth, and — here — would be a **Thread Model change**, gated in its own right. Recorded so
the option is not re-proposed rather than because it is live.

## Decision

**Not taken.** The owner's decision is required on four points:

1. **Whether to narrow session acceptance at all** — i.e. A versus B/C/D.
2. **Which paths**, if so: the session chunk alone (B), or the chunk and each slot payload (C).
   The depth-amplification measurement is the evidence that these are two questions, not one.
3. **Which limits**, and against what headroom. The measured maxima are 10 629 bytes and depth 3
   for a whole session, and 2 051 bytes and depth 2 for a slot payload; the preset boundary chose
   256 KB and depth 8 against 1 525 bytes and depth 2, i.e. ~172× and 4×.
4. **Whether the `.vstpreset` / `.aupreset` door changes the trust classification** the registry
   records for host state — independently of 1-3, because it is a statement of fact about where the
   bytes come from.

## Consequences

*While the decision is outstanding:* the code is unchanged and every row of the matrix above stands.
RISK-014 remains **open**, with its wording corrected to the measurements rather than to the
round-43 preset-path reasoning.

*If A is chosen:* RISK-014 closes as accepted and this ADR is marked `Accepted` with option A named.

*If B, C or D is chosen:* the Architecture Review Gate clears on that instruction, the registry and
`SESSION_COMPATIBILITY_POLICY.md` record the narrowed acceptance as a semantic change under rule 1,
and `RELEASE_COMPATIBILITY_CHECKLIST.md` runs — the three legacy root formats and the v0.9.5 field
capture are the fixtures the new limits must be proven against, and all four are already in
`tests/fixtures/`.

## What was NOT done, and why

No guard, no extraction of the ADR-0055 scanner, no change to `src/`. The gate is triggered and the
standing ruling explicitly excludes these paths, so implementing first and asking afterwards would
invert the procedure `ARCHITECTURE_REVIEW_GATE.md` §Procedure sets out.

No test asserts the current behaviour either. A regression test is a statement that behaviour is
intended, and whether it is intended is the question this ADR asks; the evidence lives in the probe,
which the suite does not call.

## Related code

* `src/PluginProcessor.cpp:2747-2751` — `decodeRestore`'s `getXmlFromBinary` / `ValueTree::fromXml`.
* `src/PluginProcessor.cpp:2855-2862` — `readSlot`'s `adoptIfAnamorph` and its `parseXML`.
* `src/PluginProcessor.cpp:2607-2657` — `writeState`, the writer both shapes come from.
* `src/PresetManager.cpp:310-548` — the ADR-0055 scanner, today file-private.
* `tests/state_tests.cpp` — `--risk014-probe`, the reproducer for every row above.
* `tests/fuzz_state.cpp` — the libFuzzer target already aimed at `setStateInformation`.

## Evidence + confidence

**[Verified]** — every row of the matrix was produced by running the shape through the real
`setStateInformation` on `de89b1a`, x86-64 Linux, Release, pinned JUCE 9.0.2, one shape per process,
with the 1 MB rows under `ulimit -s 1024` for Windows main-thread parity; peak RSS sampled from
`/proc/<pid>/status` `VmHWM`. The reachability claims about `InputSource`, the framing and the DTD
default are read from the pinned JUCE and VST3 SDK sources cited inline. The census is
`--risk014-probe census` against the four fixtures in `tests/fixtures/`.

**Confidence: high** for the reachability, the thresholds and the trust-boundary facts; **the size
cap is the one open judgement**, because "every session this product has ever written" is bounded by
measurement only up to the largest one measured.
