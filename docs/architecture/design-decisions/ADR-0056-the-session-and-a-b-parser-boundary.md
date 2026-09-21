# ADR-0056 — The session and A/B parser boundary (RISK-014)

**Status:** **Accepted** (owner instruction 2026-09-19; **Architecture Review Gate TRIGGERED and
cleared by that instruction**). Narrowing what `setStateInformation` accepts is a semantic change to
a contract `docs/architecture/SERIALIZATION_REGISTRY.md` records, which
`docs/policies/ARCHITECTURE_REVIEW_GATE.md` gates as a *Serialization Registry change* and
`docs/policies/AI_AGENT_POLICY.md` makes an agent hard stop — the same gate ADR-0055 triggered for
preset files, and one ADR-0055's own scope ruling explicitly did NOT clear for these two paths:
*"Implement preset-file protection only. Do not modify host session blob loading or A/B slot payload
loading in this round. Record those paths as a follow-up risk requiring separate compatibility
review."* This ADR was that review. It was **Proposed** for one round, carrying the measurements and
five options and selecting none, and the owner then ruled:

> **1. Narrow host-state acceptance.** The measured crash, hang, and unbounded-memory behaviors are
> reachable parser-safety defects. **2. Protect both parser surfaces** — the host session XML text
> before its `parseXML`, and each A/B slot payload before its separate `parseXML`. Do not protect
> only the outer session document. **3. Use the established limits** — 256 KB, depth 8, reject
> `DOCTYPE` — and do not invent a different limit merely because the current measured sessions are
> much smaller. **4. Correct the trust classification** so host state is not described as inherently
> trusted merely because it enters through `setStateInformation`.

That is **option C** below, with option D's limits question settled by instruction 3. Implemented in
round 51; what follows keeps the round-50 evidence verbatim and records the implementation after it.

## Context

ADR-0055 (0.9.9) put a byte-level boundary in front of `juce::parseXML` for `.anamorph` preset
files. Two other paths reach the same parser and were deliberately left alone, and
`docs/FUTURE_RISKS.md` records them as **RISK-014**:

1. **The host session blob.** `setStateInformation` → `decodeRestore`
   (`src/PluginProcessor.cpp:2831-2839`) → `AudioProcessor::getXmlFromBinary` → `juce::parseXML`.
2. **The A/B slot payload.** Inside the same decode, `readSlot`'s `adoptIfAnamorph`
   (`src/PluginProcessor.cpp:2946-2956`) calls `juce::parseXML (slotPayload)` on a **string
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

**Option C, with ADR-0055's limits.** Both independently parsed documents are bounded before the
parser, each on its own, at **256 KB**, **depth 8**, and **no `DOCTYPE`**.

### The shared mechanism, and what could NOT be shared

`src/XmlBoundary.h` (`anamorph::xmlBoundary`) carries ONE walk over the text, parameterised by a
`DocumentRule`. The preset path asks it `oneWellFormedDocument`; the two host-state paths ask it
`parserSafetyOnly`. `PresetManager.cpp`'s `presetTextIsAdmissible` is now a two-line call into it,
so ADR-0055's behaviour is not reimplemented — it is the same code, and State test 114's twelve legs
are the proof that it did not move.

The split was established rather than assumed, because the preset scanner could NOT simply be
copied:

| Check | Level | Shared? |
|---|---|---|
| nesting depth | decoded text | **yes** — `readNextElement`/`readChildElements` recurse without bound on any input |
| any `<!` that is not `<!--` or `<![CDATA[` | decoded text | **yes** — a `DOCTYPE` is what reaches `expandExternalEntity` |
| size | bytes (session) / UTF-8 length (payload) | **yes**, as a number; applied at each path's own entry |
| UTF-16 BOM, embedded NUL, UTF-8 validity | **bytes of a FILE** | **no** — these read the bytes the way `String::createStringFromData` is about to. A session chunk is decoded by `String::fromUTF8` instead, and an A/B payload has no bytes of its own: it arrives already decoded, as an attribute value. Copying them would have guarded the wrong decoder |
| one top-level element, whitespace-only surroundings, declaration at offset zero | decoded text | **no** — correctness rules about a *file format*, not parser safety. Instruction 3 named three limits and these are not among them |

### Where each boundary sits

* **Session chunk** — `hostChunkIsAdmissible`, called at the top of `decodeRestore`, before
  `AudioProcessor::getXmlFromBinary` reads anything. Size is answered first, on `sizeInBytes`,
  because that is what bounds the decode. The text it then scans is **not a reconstruction**: it is
  `juce_AudioProcessor.cpp:975-976`'s own expression on its own range, so the scan and the parse
  cannot disagree about where the document ends. That matters — `String::fromUTF8` builds through
  `createFromCharPointer`, whose `while (e < end && ! e.isEmpty())` (`juce_String.cpp:132`) stops at
  the first NUL, so scanning the raw bytes instead would scan past it and refuse documents the
  parser never sees. Below the framing `getXmlFromBinary` requires, the guard defers to it and the
  path is exactly as it was.
* **Each A/B slot payload** — `slotPayloadIsAdmissible`, called inside `adoptIfAnamorph` before its
  own `parseXML`. The payload's size cap is **redundant by containment today** and is stated anyway,
  because the rule is that every independently parsed document carries its own boundary: the payload
  lives inside a chunk already capped at 256 KB and XML unescaping only shrinks, and the one
  mechanism that could have grown it — entity expansion from a `DOCTYPE` in the outer document — is
  refused above. No test can reach that cap without removing the outer one, and State test 116 leg D
  says so rather than pretending otherwise.

### Failure semantics: the ones each path already had

Neither path gained a new outcome, and neither gained any user-facing state.

* A refused chunk is `decodeRestore` returning `false` — the documented *"a chunk of neither
  recognised shape is not a restore at all"* (SERIALIZATION_REGISTRY.md). Not one parameter, not the
  Settings, not the A/B slots is touched, and the sound the user has stays. State test 116 leg A/B/C
  assert exactly that by reading `drive` back at its default.
* A refused payload leaves the slot invalid, so `abEnsureInit()` re-seeds it from
  `currentStateSet()` — precisely the recovery an unparsable or foreign-typed payload already gets
  (ER-STATE-02). Leg D asserts the re-seed, not an error.

### The limits, and why they are ADR-0055's rather than fitted

Instruction 3 is explicit, and the compatibility evidence supports it without strain: the largest
session ever measured is 10 629 bytes at depth 3 and the largest payload 2 051 bytes at depth 2, so
the caps sit at **~25×** and **4×** what the product writes, and at ~128× and 4× what a slot carries.
Fitting tighter numbers to today's sessions would have bought nothing and would have made the next
parameter addition a compatibility event.

### Correction, 2026-09-19 (round 52): a self-closing element occupies a level

**The decision is unchanged; its enforcement was one level short of it.** As shipped in round 51 the
shared walk advanced `depth` on an opening tag and skipped a self-closing one entirely, so
`<a><b><c><d><e><f><g><h><leaf/></h></g></f></e></d></c></b></a>` — **nine** elements deep — passed a
cap of eight, because `<leaf/>` contributed nothing. The cap was never the thing in doubt: **the
limit is still 8**, and no configured value moved.

What the correction restores is agreement with *the one* depth definition this repository already
had, in three independent places that all count a childless element:

* **ADR-0055** records a preset — `<ANAMORPH><PARAM id=.. value=../></ANAMORPH>`, whose deepest
  element is the **self-closing** `PARAM` — as *"nested exactly two deep"*.
* **This ADR's own census** records a session as depth 3 and a slot payload as depth 2, measured by
  the `--risk014-probe census` helper whose `deepest` walk returns `d + 1` at every element,
  childless ones included.
* **`SESSION_COMPATIBILITY_POLICY.md` rule 7** inherited those numbers.

So a second definition was never written down and none is introduced here: the scan is what moved,
onto the definition the documents already used. `<ANAMORPH/>` — a root that opens and closes at once
— is depth 1 under it, admitted at `maxDepth` 1 and refused at 0, which State test 116 leg A2 pins
along with the eight-accepted / nine-refused pair and an end-to-end restore of each through
`setStateInformation`. Both `DocumentRule`s are asserted, because the depth walk is common to them.

The gap was reachable on every surface the boundary covers — preset text, host session chunk and A/B
payload alike — since all three share the walk. It was *not* a route past the cap by more than one
level: the running depth of a non-self-closing chain was always counted, so the overshoot is exactly
the one level a closing leaf occupies, and the hang and unbounded-read boundaries are untouched.

### Correction, 2026-09-21 (round 53): an opening tag that never closes is not a tag the parser stops at

**The decision is unchanged; one sentence of its reasoning was false, and the false half was
load-bearing.** The shared walk carried this premise, and applied it to four constructs at once:

> A `<!--`, a `<![CDATA[`, a `<?` **or an opening tag** that never ends swallows the rest of the text
> for this scan AND for `XmlDocument`, which runs out of data and reports an error without recursing:
> there is no depth hiding behind it.

It is true for the first three and **false for the fourth**, and the reason is positional. In the
pinned JUCE, `juce_XmlDocument.cpp:491-497` treats a quote as a string delimiter only after
`name =`. A quote where an attribute **name** is expected falls to `:508-510`
(`setLastError ("illegal character found in ...", false)`, then `break`); a name followed by a quote
with no `=` falls to `:500-503` (`setLastError ("expected '=' after attribute ...", false);
return node;`). **Both return the element**, and `errorOccurred` is consulted exactly once, at
`:233`, *after* parsing completes — so the parent's `readChildElements` (`:577-580`) carries straight
on and recurses into every element that followed the quote. Those elements are precisely the text the
scan stopped reading, and nothing bounds their nesting.

**Measured on `8dd036e` before the repair**, with the shipped `textIsAdmissible` and the parse
contained in a forked child on a 1 MB `pthread` stack:

| shape (N = 4 000 trailing `<x>`) | size | host rule | preset rule | parser |
|---|---|---|---|---|
| `<r><a "` + N·`<x>` (name position) | 12 007 B | **ADMITS** | refuses | **SIGSEGV** |
| `<r><a '` + N·`<x>` | 12 007 B | **ADMITS** | refuses | **SIGSEGV** |
| `<r><a b"` + N·`<x>` (no `=`) | 12 008 B | **ADMITS** | refuses | **SIGSEGV** |
| `<r><a b="` + N·`<x>` (value position) | 12 009 B | ADMITS | refuses | null |
| `<r><!--` / `<![CDATA[` / `<?` + N·`<x>` | ~12 007 B | ADMITS | refuses | null |
| `<AnamorphRoot>…<a "` + N·`<x>` | 12 134 B | **ADMITS** | refuses | **SIGSEGV** |
| through the chunk framing | 12 016 B | `hostChunkIsAdmissible` **ADMITS** | — | **SIGSEGV** |

Also SIGSEGV on an 8 MB stack at N = 40 000 (120 KB, inside the cap). **The `.anamorph` path was
never affected**: under `oneWellFormedDocument` `skipRanOff` is false and every shape above is
refused, which is why ADR-0055's twelve-leg test could not have seen this.

**The repair is to stop modelling where the parser stops.** A tag walk that reaches the end of the
text is now admitted only when no quote was open — `if (! closed) return quote == 0 && skipRanOff;`.
With no quote open, everything that remained *was* scanned and there is nothing behind it; with one
open, the scan consumed text the parser may resume inside, and it refuses rather than guessing where.

**This is an implementation repair of rule 7, not a new narrowing of it**, and the distinction is
measured rather than argued: every shape the repair newly refuses is one `juce::parseXML` answers
with **null** or does not answer at all — checked across the name-position, no-`=` and value-position
forms, long and short — so **the set of chunks that successfully restore is unchanged**, and a
refusal is the outcome each path already had (§"Failure semantics"). The four retained fixtures, a
writer-shaped session, single- and double-quoted values, escaped `<`/`>` and an apostrophe in a
preset name are all still admitted and still parse. One nuance worth recording so a later round is
not surprised: a chunk truncated *inside an attribute value* now refuses at the boundary where it
previously reached the parser and got null — the restore outcome is identical, the boundary verdict
is not. The truncation shape State test 116 leg E pins is cut at `</ANAM`, with no quote open, and is
untouched.

**Why nothing caught it, which matters more than the defect.** Nothing in the tree compared
`textIsAdmissible`'s verdict against what `juce::parseXML` actually does; State test 116 asserts the
shapes someone thought of, and the six mutants recorded below all weaken a *guard* rather than test
the model. The only generic oracle is the `fuzz` job, whose three corpus seeds could not decode at
all (RISK-016). Round 53 adds both halves: **State test 116 leg E2** for the verdict, on every
platform, and `tests/xml_boundary_differential.cpp` for the contract itself — a Linux-only harness
with process isolation, a self-proving oracle that searches for the toolchain's own crashing depth,
and an assertion that legitimate documents are still admitted so the contract cannot be satisfied by
refusing everything.

## Consequences

**What changed for a user: nothing that a valid session can observe.** Every fixture the repository
retains still restores — the three legacy root formats `SESSION_COMPATIBILITY_POLICY.md` rule 3 keeps
alive, the v0.9.5 field capture, and a live save→restore round trip — and State test 116 leg F
asserts each one rather than printing it.

**What changed for a corrupt one:** the crash, the hang and the unbounded read are gone from both
paths. Re-measured with `--risk014-probe` on this head: 3 000 and 30 000 levels of nesting, and the
one-, two- and three-level recursive-entity `DOCTYPE`s, are each refused in 0–4 ms on both paths,
where before they SIGSEGV'd or did not return. The plug-in now allocates nothing for an oversized
chunk; the host still holds the bytes it is handing over, which no boundary here can change.

> **Amended 2026-09-21 (round 53).** That paragraph was true of the three shapes it names and
> **overstated as a claim about the crash**: an opening tag whose quote never closes was admitted,
> and a 12 KB chunk still SIGSEGV'd through both surfaces until round 53 refused it. See
> §"Correction, 2026-09-21" for the measurement and the repair. The sentence stands for the shapes
> the probe covers; it is the *scope* of "gone" that was wrong, and the round-53 differential
> harness is what now holds the general statement instead of a list of measured shapes.

**What did NOT change, deliberately.** Two documents in one chunk, trailing prose, a NUL disguise,
invalid UTF-8 in an attribute value and a chunk truncated to half its length all still load, exactly
as they did. Those are acceptance questions instruction 3 did not open, and State test 116 leg E
pins them so that a later round narrowing them has to do it on purpose.

**ADR-0055 is unchanged.** Its scanner moved file; its rules, its limits and its twelve-leg test did
not. `PresetManager::maxPresetBytes` and `maxPresetDepth` are now names for
`anamorph::xmlBoundary::maxDocumentBytes` and `maxDocumentDepth`, and leg 0 of State test 116 asserts
they still hold ADR-0055's values.

**Compatibility process:** `RELEASE_COMPATIBILITY_CHECKLIST.md`'s session-reload and preset-migration
checks are what the fixture legs discharge in the suite; the checklist itself is a release-time
gate and is unaffected by this change beyond the evidence now being automatic.

## What was NOT done, and why

**No new thread, no worker parser, no blocking.** Option E stays rejected for ADR-0055's reasons and
would have been a gated Thread Model change on top.

**No byte-level rules on host state.** The session chunk's invalid-UTF-8 tolerance and its NUL
truncation are real and are recorded as the round's residual (see RISK-014). They are neither crash,
hang nor unbounded read, and instruction 3 named three limits.

**No single-document rule on host state**, for the same reason — see the Decision table.

**No suppression anywhere**, and no production change made to satisfy a test. The one change this
round made to a test's *subject* was to give State test 116's A/B legs a payload carrying a
distinguishable `width`, because the first draft's assertions could not tell "refused" from
"admitted but empty" and mutant M2 survived them.

## Related code

* `src/XmlBoundary.h` — `anamorph::xmlBoundary`: the shared walk, the two rules, the two limits.
* `src/PluginProcessor.cpp` — `hostChunkIsAdmissible` and `slotPayloadIsAdmissible` in the file's
  anonymous namespace, and their two call sites in `decodeRestore` and `adoptIfAnamorph`.
* `src/PresetManager.cpp` — `presetTextIsAdmissible`, now a call into the shared walk; the
  byte-level `presetBytesAreAdmissible` stays where it is and why.
* `src/PresetManager.h` — `maxPresetBytes` / `maxPresetDepth`, now names for the shared constants.
* `tests/state_tests.cpp` — **State test 116** (legs 0, A–G) and `--risk014-probe`, the opt-in
  evidence instrument the suite never calls.
* `tests/fuzz_state.cpp` — the libFuzzer target aimed at `setStateInformation`.

## Evidence + confidence

**[Verified].** The round-50 matrix above was produced by running each shape through the real
`setStateInformation` on `de89b1a`, x86-64 Linux, Release, pinned JUCE 9.0.2, one shape per process,
with the 1 MB rows under `ulimit -s 1024` for Windows main-thread parity and peak RSS sampled from
`/proc/<pid>/status` `VmHWM`. The reachability claims about `InputSource`, the framing and the
`TextFormat::dtd` default are read from the pinned JUCE and VST3 SDK sources cited inline.

**The round-51 implementation is verified by measurement, not by inspection:**

* **State 4 625 / 0** (was 4 584; +41 from State test 116), **DSP 396 / 0**, `preflight.sh` exit 0.
* **Six mutants, six kills.** M1 the session guard always admits → 5 failures. M2 the payload guard
  always admits → 3. M3 the shared walk stops refusing `<!` under `parserSafetyOnly` → 2. M4 the
  session size cap removed → 2. M5 depth 8 → 9 → 1. M6 size 256 KB → 128 KB → 1. **M2 and M5
  survived the first draft** and both survivals were real coverage gaps, fixed in the test.
* **libFuzzer over `setStateInformation`**, ASan+UBSan, seeded corpus, 10 990 runs / 181 s: no
  findings, no reports, slowest unit 0 s.
* **The external-entity oracle was rebuilt** (see RISK-014 and TESTING.md): the old probe looked for
  a canary in the serialized state, which cannot distinguish "never opened" from "opened, read, then
  rejected". Leg G now measures a filesystem differential with a positive control — with a counting
  `InputSource` installed the parser opens the file once and the entity resolves to `LEAKED`; through
  `juce::parseXML (const String&)` the result is `leak` and is **byte-identical whether the file
  exists or not**. Identical output across present/absent is the property; canary absence is not.

**Round 52 — the self-closing correction, verified the same way:**

* **State 4 632 / 0** (was 4 625; +7 from leg A2), **DSP 396 / 0**, `preflight.sh` exit 0.
* **Leg A2 kills the pre-fix scan.** Restoring only the lines that skipped a self-closing element
  and re-running produces **4 failures**: nine-deep-with-a-self-closing-leaf refused under
  `parserSafetyOnly`, the same under `oneWellFormedDocument`, `<ANAMORPH/>` refused by a cap of
  zero, and the end-to-end depth-9 `setStateInformation` restore — which on the pre-fix code
  restored `drive` to 0.0375 instead of refusing. Written before the fix, failing on the shipped
  code, passing after: not a test fitted to the answer.
* **M5 re-run (depth 8 → 9) now kills 2 checks**, up from 1, because leg A2's end-to-end restore
  fails alongside leg 0's `maxDocumentDepth == 8`. The cap keeps its coverage, and the constant
  cannot be edited to match a broken walk without the suite saying so.

**Confidence: high** for the boundary, the thresholds, the compatibility evidence and the oracle.
**Two things remain judgements rather than measurements**: the size cap is bounded by the largest
session ever *measured*, not by a proof; and the payload's own size cap is unreachable while the
outer cap holds, so it is argued by containment rather than tested.
