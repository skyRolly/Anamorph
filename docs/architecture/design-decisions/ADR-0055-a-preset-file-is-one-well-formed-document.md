# ADR-0055 — A preset file is one well-formed document

**Status:** **Accepted** (owner instruction 2026-09-18; **Architecture Review Gate TRIGGERED and
cleared by that instruction**). Narrowing what `.anamorph` files the loader accepts is a semantic
change to a contract `docs/architecture/SERIALIZATION_REGISTRY.md` records, which
`docs/policies/ARCHITECTURE_REVIEW_GATE.md` gates as a *Serialization Registry change* and
`docs/policies/AI_AGENT_POLICY.md` makes an agent hard stop. The change was therefore investigated
and reported first, not implemented first: the round-43 investigation was delivered against head
`0e32e65`, and the owner ruled on it — *"Approve tightening preset acceptance rules. Malformed
preset files that violate the serialized XML structure must be rejected. Existing valid presets
remain compatible."* — together with the two limits, the empty-preset ruling, the error-handling
model and the scope boundary recorded below.

## Context

A `.anamorph` file is `apvts.copyState().createXml()` — one `<ANAMORPH>` root carrying `PARAM`
leaves. Measured on `0e32e65`, a file this plug-in writes is **1525 bytes** across **36** `PARAM`
nodes and is nested exactly **two** deep.

Since ER-STATE-24 (2026-09-02) both loaders have shared one acceptance test,
`PresetManager::parseSoundFile`, and that test asked exactly one question: is the root
`apvts.state.getType()`? Everything else a file might contain was whatever
`juce::parseXML` happened to do with it.

## Problem

The reported defect: **a file containing two complete presets loads, and applies the first.**
`XmlDocument::parseDocumentElement` reads one element and returns it, and nothing looks at the
remaining input. That file is corrupt and was being applied as if it were not.

Investigating it found the same mechanism in five more shapes and three far worse ones. All were
measured on `0e32e65` against the pinned JUCE 9.0.2, through the real `PresetManager::loadFile` on a
real processor:

| Input | Behaviour before this ADR |
|---|---|
| Preset A followed by preset B (also B then A; also 1000 documents) | **accepted**, the first applied |
| A valid preset followed by prose, by `<JUNK/>`, or by raw binary | **accepted**, the tail discarded |
| Two `PARAM` nodes claiming the same `id` | **accepted**, the first silently won |
| A foreign child element, or a second `<ANAMORPH>` nested inside the root | **accepted**, silently ignored |
| A `PARAM` with child elements, with no `id`, or with a duplicated attribute | **accepted**, silently ignored |
| Text or a CDATA section inside the root | **accepted**; dropped in Release, `jassertfalse` in Debug |
| ~3 000 levels of nesting (a **21 KB** file) on a 1 MB thread stack | **SIGSEGV** |
| ~28 000 levels on an 8 MB stack | **SIGSEGV** |
| A **220-byte** file whose `DOCTYPE` defines recursive entities | **HANG** — still running after 60 s |
| `<!DOCTYPE x SYSTEM "…">` naming an absolute path or a `../` traversal | **reads that file**, and its contents reach a parameter value |
| A 256 MB file | read whole into memory — peak RSS tracked file size linearly, **no cap** |

The last four are not untidy files. The crash is in `readNextElement` ↔ `readChildElements`, which
recurse into each other with no bound — the `gdb` backtrace at the fault is those two frames
alternating **inside `juce::parseXML`**, before `parseSoundFile` holds anything it could inspect.
The hang is in `XmlDocument::expandEntity`, which indexes its own scan with the enclosing loop's
variable. Neither can be corrected after the fact, which is what decides the shape of the fix.

A second defect, on the other side of the same loader: **a corrupt preset that has reached the user
folder was a silent no-op on both list doors.** `load(index)` and `loadAdopted` returned `void`, so
nothing reached the editor; and because `step` is relative and a failed load leaves `current` where
it was, the next press re-derived the same row. Measured with one corrupt file between two good
ones: four presses of Next gave the same row four times, and every preset beyond it was unreachable
in both directions.

## Options

- **A. Leave the acceptance test as it is and document the tolerance.** Rejected: the reported case
  is a file that means two different things, and the crash and the hang are reachable from a file a
  user merely double-clicks.
- **B. Validate the parsed `ValueTree` only.** Rejected on measurement. It cannot see a second
  document (`parseXML` has already discarded the tail), it cannot see a text node (`fromXml` drops
  it) and it cannot see a duplicated attribute (a ValueTree collapses it) — and it runs *after* the
  three failures that never return.
- **C. Cap the file size only.** Rejected: a **21 KB** file crashes a 1 MB stack, so the cap would
  have to sit at roughly four times a real preset to bound depth by size alone. That is too tight to
  survive a growing parameter set, and it bounds the wrong quantity.
- **D. Parse on a thread with a large stack.** Rejected: it makes the corrupt file survivable rather
  than refused, and it answers neither the hang nor the arbitrary file read.
- **E. A byte-level guard before `juce::parseXML`, plus a structural check on the parsed document,
  both inside the existing `parseSoundFile`.** **Chosen.**

## Decision

### The boundary, in the order `parseSoundFile` applies it

**Before the parser**, on the bytes, because everything in this half detonates inside it:

0. **Every byte of the file reaches the scan.** Added by the amendment below. A file carrying a
   NUL — a zero byte in UTF-8, a zero code unit in UTF-16, or a trailing half code unit — is
   refused, because a `juce::String` ends at the first one and everything after it would be
   validated by nobody.
1. **Size.** A file larger than **256 KB** is refused (`PresetManager::maxPresetBytes`) — 172× a real
   preset.
2. **No `DOCTYPE`.** Refused outright. This closes the hang and the arbitrary file read together,
   and a preset this plug-in writes never contains one, so it costs a legitimate file nothing.
3. **Depth.** Nesting deeper than **8** is refused (`PresetManager::maxPresetDepth`) — 4× a real
   preset, and two orders of magnitude below the shallowest measured crash.
4. **Exactly one top-level element**, with nothing after it but whitespace, comments and processing
   instructions. This is the reported case and every trailing-junk shape at once, and it cannot be
   asked after the parse.

The scan steps over comments, CDATA sections, processing instructions (which is how the `<?xml …?>`
declaration arrives) and quoted attribute values, because each of those may legally contain `<` or
`>` and miscounting one would refuse a real preset.

**After the parser**, on the `XmlElement` — not on the `ValueTree`, for the two reasons option B
failed: every child of the root must be a `PARAM`, no child may be a text element, a `PARAM` must be
a leaf, its attributes must be exactly one `id`, exactly one `value` and at most one `raw`, and no
two `PARAM` nodes may claim the same `id`.

The document is parsed **from the text rather than from the file**, which makes the external-entity
refusal structural as well as textual: an `XmlDocument` built from a string has no `InputSource`, so
`getFileContents` has nothing to open.

### What is deliberately NOT restricted

Each of these is a documented tolerance, and every one is pinned by State test 114 leg E:

- **An `id` this build does not know.** A preset written by a later Anamorph must still load.
- **A missing `PARAM` node** — still that parameter's default.
- **A malformed `value`** — still resolves through `SerializedNumber.h` to the parameter default,
  never to the range minimum.
- **`<ANAMORPH/>` with no children at all** — still valid, and still means "every parameter's
  default". Ruled explicitly by the owner: *"Keep accepting empty presets."* It is the
  missing-`PARAM` rule taken to its limit, and refusing it would contradict that rule for no
  measured benefit.
- **Attributes on the root** — the format's forward-compatibility room.
- **An XML declaration, comments and processing instructions before the root.**

### `raw` is one of ours, and this round measured that rather than assuming it

The comment at `soundSignatureAfterRestoring` said *"A preset file never carries `raw`"*. **It is
false.** An undo, a redo or an A/B apply installs a state-set tree carrying `raw` through
`replaceState`, and `saveUser`'s `apvts.copyState()` then writes the live tree as it stands. The
first implementation of the attribute rule required exactly two attributes and refused files the
plug-in itself had written — caught by State tests 10, 18 and 35 before it left the working tree.
`raw` is therefore accepted, at most once. Nothing about the sound depends on it: a preset is
resolved through `value` alone (`normalisedFromSavedTree`), which is why two signature predictors
exist and not one — that part of the comment is correct and unchanged. The false sentence is
corrected in place.

### The value-less `PARAM` is now a corrupt file

`<PARAM id="width"/>` with no `value` used to be accepted and resolved to the parameter default.
The registry's own words for how such a node comes to exist are *"a truncated write or a hand
edit"* — a description of a corrupt file — and under this ADR a corrupt file is refused rather than
interpreted. The invariant that made it worth a test is unchanged and now holds by a stronger route:
the silent collapse to the range **minimum** (`var()` → 0.0 → mono for `width`) is unreachable
because the document never reaches `applySoundTree`. **The session path is untouched** — a host
session carrying the same node still restores to the default, and State test 18 leg C asserts it.

### Every loader reports, and a relative step steps over

`loadFile` has carried R640's completion contract since round 27. `load`, `loadAdopted` and `step`
now carry the same one: `OpResult` plus an optional `onComplete` called **exactly once** with the
final answer. On top of that contract:

- **An absolute request reports and stays.** The user named that row; a row that will not load must
  not silently move the selection somewhere else.
- **A relative request reports and continues.** "Next" asks for the one after this, so `step` steps
  **over** a row that will not load, bounded by one pass of the list, and answers `failed` only if
  nothing in the list loads. The row is chosen by a pure predicate (`rowIsLoadable`) rather than by
  a failed load's result, so exactly one `loadAdopted` runs and it is the one that owns the
  completion.
- **A missing file and a corrupt file are different events.** A row whose file has vanished is
  describing something that is no longer there, so the list is rescanned and the row goes with it.
  A row whose file is still on disk but is not a readable preset **keeps its row** and reports.
- **Nothing deletes or hides a user's file.** Ever. It is their data, it is plain text, it is
  hand-recoverable, and the corruption may be transient.

### The failure is shown without a dialog

A refused load shows `PRESET UNREADABLE` in the top-bar preset slot for 1.5 s, warn-coloured, and
the preset that **is** loaded keeps its name, its tick and its sound. No modal: an editor raising
one is a known host hazard, this product has never used one, and the Save dialog already establishes
the house answer — a warn-coloured state in the control the user was working in. The knob sweep is
success UI and now runs only on a load that happened, which also removes a 0.45 s animation that
used to play for a load that had been refused.

## Consequences

- **Files this plug-in has ever written still load.** The writer is unchanged; ADR-0024's rule that
  user preset files are byte-for-byte what 0.9.1 wrote is untouched, because nothing here writes.
- **Five shapes that used to load now do not:** a file containing more than one document, a file with
  anything but whitespace or comments after the root, a structurally malformed document, a
  value-less `PARAM`, and (per the amendment) a file whose bytes continue past a NUL. Each is a
  corrupt file by the format's own definition.
- **A preset subtree lifted out of a session by hand still loads**, because `raw` is accepted.
- **ER-STATE-24 is unchanged and re-affirmed**: the root test still runs, still refuses a foreign
  root, and still runs before any per-parameter fallback can reinterpret the document.
- **ER-GUI-06 is unchanged and strengthened**: every check that can refuse still lands before
  `onAboutToLoad`, so a refused load still raises no duck. State test 35 measures it through the
  absolute door, which is the one that still refuses.
- **ADR-0036 §23 is unchanged**: `step` still drains once, at its admission, and still derives every
  row it considers from the session that drain established; `loadAdopted` is still called with
  `drainFirst == false`.
- **The three limits are constants, not literals** (`maxPresetBytes`, `maxPresetDepth`), so the
  regression names the same values the loader enforces.

## What was NOT done, and why

- **The host session blob and the A/B slot payload were left alone.** The same unbounded parser is
  reached from `PluginProcessor.cpp:2751` and `:2860`, and every risk above applies there from a
  corrupted project file. Scoped out by the owner for this round — *"Implement preset-file
  protection only… Record those paths as a follow-up risk requiring separate compatibility
  review."* — because guarding them touches session compatibility and deserves its own evidence.
  Recorded as **RISK-014**.
- **No unknown-`id` rejection**, no per-parameter range validation, no checksum, no version stamp in
  the file. None is supported by a measured failure, and each would cost forward compatibility.

## Related code

`src/PresetManager.cpp` — `presetBytesAreAdmissible`, `presetTextIsAdmissible`,
`presetDocumentIsWellFormed`, `parseSoundFile`, `load`, `loadAdopted`, `step`, `rowIsLoadable`;
`src/PresetManager.h` — `maxPresetBytes`, `maxPresetDepth`, the three loader signatures;
`src/PluginEditor.cpp` — `presetLoadFinished`, `stepPreset`, `refreshPresetDisplay`,
`stepMicroAnims`; `src/gui/LookAndFeel.cpp` — the `warn` property on the preset slot.

## Amendment — 2026-09-18: the file is its bytes, and the warning has a life

Three review findings against the head that first implemented this ADR (`f03aa06`), all three
reproduced before anything was changed. None reopens a decision above; each closes a hole in
carrying one out.

### The boundary read a String, and a String ends at the first NUL

`parseSoundFile` read the file with `loadFileAsString` and scanned the result. A `juce::String` is
NUL-terminated, and every reader below walks it with a CharPointer that stops at the first NUL —
this ADR's own pre-scan and `juce::XmlDocument` alike. One 0x00 therefore made the rest of the file
invisible to the boundary **and** to the parser, so the shapes ruled out above came straight back:

| Input | Behaviour on `f03aa06` |
|---|---|
| Preset A, one NUL, complete preset B (263 bytes) | **accepted** — decoded to 131 bytes, A applied |
| A valid preset, one NUL, trailing prose | **accepted**, the tail unseen |
| A valid preset, one NUL, a tail that is not even valid UTF-8 | **accepted**, the tail unseen |
| The same files **without** the NUL | correctly refused, by rule 4 above |

The refusal is on the bytes and before the decode, because after the decode the evidence is gone.
It is **encoding-aware** rather than a blanket "no zero byte": it reads the bytes the way
`String::createStringFromData` is about to (`juce_String.cpp:1987-2035`), so a UTF-16 preset — every
second byte of which is zero — still loads, while a zero *code unit* or a trailing half code unit in
one is refused for the same reason a NUL byte is in UTF-8. The decode is then spelled
`String::createStringFromData`, which is exactly what `loadFileAsString` was doing
(`juce_File.cpp:568-576` → `juce_InputStream.cpp:241-246` → `juce_MemoryOutputStream.cpp:207-210`),
so no legitimate encoding reads differently than it did.

### The `PRESET UNREADABLE` state could not expire

`presetWarnTime` counted down below `stepMicroAnims`' two idle-gate early returns. That gate seals
when the cursor is outside the editor, no button is held, no sweep is open, the pass settled dark
and no generation or tracked value moved — and a refused load satisfies every one of them, because
it changes no parameter and lights no widget. The warning therefore stopped counting on the next
frame and stayed in the top bar until some unrelated animation happened to wake the pass. The
decrement now runs **above** the gate. The asymmetry with `knobSweepTime`, which stays inside the
gate conditions, is deliberate: a sweep *eases widgets* and needs that pass to run, while the
warning is a text state the 24 Hz `timerCallback` reads, so holding the 44-widget poll open for
1.5 s would animate nothing.

### A load that succeeded left the previous refusal on screen

`presetLoadFinished(true)` set the sweep and left `presetWarnTime` alone, so a preset that loaded
within 1.5 s of a refusal was displayed as `PRESET UNREADABLE`, its own name suppressed, for the
remainder. Success now clears the warning as well as raising the sweep: the two states describe the
same slot, so the later one owns it.

### Scope

Unchanged: the accepted format, the two limits, the empty-preset ruling, the missing-versus-corrupt
split, the no-dialog/no-delete rules, and RISK-014 — the host session blob and the A/B slot payload
were not touched and remain a separate, compatibility-gated question.

## Evidence + confidence

**Verified.** State test 114 is the boundary in six legs — two-and-three-document files, trailing
text/element/binary, ten malformed structures, the three parser-safety shapes (5 000 levels of
nesting; a recursive-entity `DOCTYPE` with an elapsed-time bound, because unguarded it does not fail
but never returns; a `SYSTEM` `DOCTYPE` with a real sibling file to read; a file one byte past the
cap), eleven preserved tolerances, and the list contract including the bounded skip in both
directions and the missing-versus-corrupt split. State test 18 is rewritten for the new rule and
gains the session-path leg; State test 35 drives its refusal through the absolute door and gains a
leg for the skip. The pre-fix behaviour of every leg was measured on `0e32e65` before the guard
existed.

The amendment adds **State test 114 leg G** — six NUL shapes refused, four encodings still accepted
(plain UTF-8, UTF-8 with a mark, UTF-16 LE and BE), and the two UTF-16 shapes that hide bytes
refused — and **State test 115**, which drives the real top-bar button through the real
`presetLoadFinished` and the real frame clock: the state appears, is still shown at 1.0 s, is gone
by 1.75 s with no mouse movement and no other animation, and is cleared instantly by a load that
succeeds. Its pre-fix behaviour was measured on `f03aa06`. Mutation record:
`docs/procedures/TESTING.md`.
