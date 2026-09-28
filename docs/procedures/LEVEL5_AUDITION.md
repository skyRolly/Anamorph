# LEVEL5_AUDITION.md

The **Level-5 audition** is `RELEASE_POLICY.md` precondition 7 and `TESTING_POLICY.md`'s Level 5:
the human sign-off that a build sounds and behaves right in a real DAW. It is the one gate CI
structurally cannot supply — a green build plus a pluginval pass means *"ready to audition,"* not
*"shipped"* — so this document says what to audition rather than leaving the scope to memory.

> **This protocol cannot be executed by CI or by an automated agent.** It requires a human, a DAW,
> audio output and ears. An audition record that was not produced by a person listening is not a
> Level-5 record, whatever it contains.

## When a previous audition stops counting

An audition is **per-build**, not per-feature. It is invalidated by anything that changes the
machine code or the audible behaviour of the thing being shipped. The rule was applied twice
before 0.9.9, and it governed **0.9.9** in turn. Between the 0.9.6 PASS and 0.9.9 no release audition
was recorded (ADR-0054's 2026-09-17 dependency audition predates 0.9.9's changes), and 0.9.7, 0.9.8
and 0.9.9 each change audible behaviour. So the 0.9.9 audition needed its own scope, derived from the
`[0.9.7]`, `[0.9.8]` and `[0.9.9]` CHANGELOG entries (§Scope for 0.9.9). The owner has performed it
(§Recorded auditions, 0.9.9).

**The 0.9.6 audition of 2026-09-01 does not carry over to 0.9.7** — the release this rule
blocked when it was applied the second time (2026-09-03). **ADR-0034** changed what the plug-in reports to the host and added a delay
element to the chain, and it changed one audible behaviour beyond latency: a forced A/B, preset or
undo swap that crosses the Drive engagement threshold with a factor selected is now
latency-neutral, so it dry-fills instead of dipping to silence. Neither is a thing an automated
gate can audition. The 0.9.6 verdict below is not withdrawn; it simply does not cover this build.

**The 0.9.4 audition of 2026-08-15 was invalid for 0.9.6** on both counts, which is the worked
example the rule was first written from:

- **ADR-0031 / ADR-0032** changed the x86-64 machine code everywhere (`-march=haswell`,
  `-ffp-contract=off`, and the MSVC AVX2 adoption). CI's twin-dump gate proves the two builds are
  bit-identical to each other; it cannot tell you the result sounds right on real hardware.
- **0.9.6 changed audible behaviour in exactly the windows that were previously defective** — the
  activation duck, the first-block level of a restored session, and the A/B / preset switch.

## Scope for 0.9.9

**Performed by the owner; completion reported 2026-09-28** (§Recorded auditions, 0.9.9). This scope was
written first, and the environment that wrote it has no DAW, no audio device and no listener. It is
derived from the `[0.9.7]`, `[0.9.8]` and `[0.9.9]` CHANGELOG entries, because the last recorded
audition before it was 0.9.6's. Which of A–F the owner exercised was not supplied, and the record
does not infer it.

The instruction the scope was written with: audition the final 0.9.9 build in at least one DAW. That
build is the CI artifact of the commit to be tagged `0.9.9`. Record the result per §Recording the
result, naming which of A–F were exercised.

### A. Oversampling, Drive and latency (0.9.7, ADR-0034/0035)
- With Oversampling at 2×, 4× or 8×, sweep Drive through zero and change algorithm while playing.
  Expect no interruption, no click and no dip. Repeat the Drive sweep once with the plug-in bypassed.
- Switch Oversampling from a factor to Off while playing. Drive and the modulation algorithms stay in the sound.
- The host's delay compensation stays aligned while Drive moves. The reported latency now depends on the
  Oversampling setting alone.

### B. Scrolling, dragging and Undo (0.9.8)
- Scroll while dragging a knob, a value box, and a Multiband band or split. Each notch adds to the drag,
  the drag carries on, and the whole gesture is one Undo step.
- With a button held, a notch moves only the held control. With an Alt-reset or a readout
  double-click held, it moves nothing.
- Add a band, remove a band, and reset the splits; undo each. Each is one step, and the splits come back.

### C. Host automation and Undo (0.9.8, 0.9.9)
- Edit a control while the DAW writes automation to it. Your edit undoes, and Undo/Redo never restores
  a value the automation wrote.
- Press a control without moving it while its lane plays back. The lane is not handed to Undo.
- Automate Dimensional Style while another algorithm is selected. The sound does not duck.

### D. A/B, presets and Level Match (0.9.7–0.9.9)
- With Level Match on, A/B between slots that differ in Drive, Mix or Width.
  - Each slot keeps its own match level.
  - Turning Level Match on does not jump louder.
  - Apply Gain never silences the plug-in.
- Load factory and user presets and step with ‹ ›. The name and the modified-star follow. A damaged
  preset file is refused with a message, and ‹ › still work.
- Reopen a project saved in 0.9.8 that uses the fourth algorithm. It shows **Dimensional** and
  **Dimensional Style** with the same voicing.
  - Headless evidence already establishes that the plug-in renders that session bit-identically
    (`RELEASE_COMPATIBILITY_CHECKLIST.md` §0.9.9, item 8).
  - The audition's part is hearing it in a host.

### E. Transport, metering and tail (0.9.9)
- Stop the transport and start it again.
  - The delay lines clear on stop.
  - The peak numbers stay on stop and clear on play.
  - The meters do not freeze.
  - The Level Match readout does not creep while stopped.
- Turn the Multiband on and off at a partial Mix. No click.
- Bounce with the tail included. The full decay of Haas, Chorus and Dimensional is kept.

### F. Host matrix and automation playback
Checklist items 5 and 7 are separate attestations for 0.9.9 (`RELEASE_COMPATIBILITY_CHECKLIST.md`).
They may be performed in the same sitting, but they are recorded there, each on its own. This audition
record does not tick them.

## Scope for 0.9.6

**This scope is the 0.9.6 record and is kept as written.** The audition it describes was performed
and passed (see §Recorded auditions). A 0.9.7 audition needs its own scope, derived the same way
from the `[0.9.7]` CHANGELOG entries; the ADR-0034 latency and Drive-crossing changes named above
are what it has to cover, and group C below is the closest existing analogue.

Derived from the `[0.9.6]` CHANGELOG entries and grouped by what the listener actually has to do.
Each item names the failure it is looking for, so a pass is a statement about something specific.

### A. Activation and restore (the largest audible change set)

1. **Insert the plug-in on a playing track.** Entries 10/12. The first ~35 ms must not dip,
   duck or fade in. Pre-0.9.6 this measured 0.4 % of settled level.
2. **Reopen a saved project** whose settings are NOT defaults (Output Gain well below 0 dB, Mix
   below 100 %). Entries 11/12. It must play at the saved level from the first note — no ramp up
   or down over the first ~20 ms, and no momentarily-wet Mix-0 session.
3. **Change sample rate / buffer size while loaded.** Entry 16. Audio must resume clean, and an
   A/B slot's remembered Level Match must still be applied afterwards.

### B. A/B, presets and undo

4. **Switch A/B and load presets while the transport is STOPPED, then start playback.** Entry 7.
   The first ~32 ms must be at full width — no momentary collapse of the stereo image.
5. **Switch A/B and load presets DURING playback.** The switch should be masked and click-free;
   this is long-standing behaviour and the check is that it has not regressed.
6. **Drag a knob's number readout, then Undo.** Entries 4/18. One drag must be one undo step, and
   the DAW must record it as a normal touch/latch automation gesture.
7. **Drag a number readout and release the mouse OUTSIDE the plug-in window** (over the host, over
   the desktop). Entries 2/4. Undo must keep working afterwards, and the knob must not stay
   visually pressed. **Repeat this one on macOS specifically** — it was the last platform fixed and
   the fix uses an AppKit-only query.

### C. Latency and automation

8. **Automate Drive or Widen Algorithm across its engage threshold during playback, with
   Oversampling ON.** Entry 1. Listen for dropouts, clicks or crackle at the moment the automation
   crosses; the host's delay compensation may re-settle up to ~50 ms later by design, and that
   deferral is the fix, not a defect. What must NOT happen is a glitch in the audio.
9. **Confirm track alignment** against a dry reference after that automation pass: the plug-in must
   end up correctly compensated, not merely glitch-free.

### D. Damaged-state recovery (does a *healthy* file still behave?)

10. **Load ordinary sessions and presets saved by this build and by 0.9.4/0.9.5.** Entries 3/5/6/8/9/
    15/17 all changed what happens to MALFORMED data; the audition's job here is the converse — to
    confirm that well-formed files load exactly as before, with no control landing anywhere
    unexpected. Nothing in this group should be audible at all.

### E. Metering and the wider host matrix

11. **Watch the correlation meter through a bypass toggle and a silent passage.** Entry 14. It must
    not freeze for the rest of the session.
12. **A host with an unusual buffer size** (or one that varies it). Entry 13. No crash, no
    artefacts.

## Recording the result

The result belongs in `ENGINEERING_REVIEW_PROGRAMME.md` (the round's worklog entry) and in the
release checklist. Record, at minimum:

| Field | Why it matters |
|---|---|
| Build identity | The exact artifact / commit auditioned — a build, not a version string |
| DAW + version | Host behaviour is what several of these items exercise |
| OS + CPU architecture | ADR-0031/0032 changed x86-64 code; Apple Silicon and Intel are different slices |
| Plugin format | VST3 / AU / Standalone behave differently on restore and automation |
| Session used | So the audition is repeatable |
| Per-item outcome | Which of A-E were exercised, and what was heard |
| Verdict | Pass / fail, and any defect filed |

An audition that exercised only part of the scope is recorded as **partial**, naming what was
covered. Partial is a legitimate and useful record; a partial audition described as complete is not.

## Recorded auditions

Newest first. A row here is a human's account of listening to a build; nothing else may create one.

### 0.9.9 — **COMPLETED, signed off by the owner**

**Recorded 2026-09-28** from the repository owner's report that the 0.9.9 Level-5 audition has been
completed. The owner performed it. The report is the owner's attestation, and it is the Level-5
evidence: precondition 7 asks for a person's sign-off, not for an artifact a machine can check.

The owner reported the audition complete and directed the release to proceed to its tag. No defect
was reported. The report did not use the word "pass" or give per-item results, so the verdict row
records what was said rather than a stronger word.

| Field | Value |
|---|---|
| Verdict | **Completed — signed off by the owner, no defect reported** |
| Build | The 0.9.9 build (as stated by the owner) |
| Performed by | The repository owner |
| Reported | 2026-09-28 |
| Audition date | **NOT RECORDED** |
| DAW + version | **NOT RECORDED** |
| OS + CPU architecture | **NOT RECORDED** |
| Plugin format (VST3 / AU / Standalone) | **NOT RECORDED** |
| Session used | **NOT RECORDED** |
| Per-item outcome (§Scope for 0.9.9, groups A–F) | **NOT RECORDED** |
| Exact artifact / commit identity | **NOT RECORDED** |

**Three kinds of evidence, kept apart.**
- **Owner-attested:** the audition was completed, by the owner, on the 0.9.9 build. That is all this
  record asserts.
- **Automated:** the headless evidence is recorded where it was produced, not here. It includes the
  0.9.8→0.9.9 cross-version render in `RELEASE_COMPATIBILITY_CHECKLIST.md` item 8 and the CI gates on
  the release head. None of it is a substitute for this sign-off.
- **Not supplied, and not inferred:** every row marked NOT RECORDED. The A–F scope says what *should*
  be exercised; nothing here claims which parts *were*.

As with 0.9.6, the correspondence between the build auditioned and the build tagged rests on the
owner's statement, which is what precondition 7 is by definition.

Evidence [Verified]: docs/policies/RELEASE_POLICY.md (precondition 7); docs/policies/TESTING_POLICY.md
(Level 5); docs/procedures/RELEASE_PROCESS.md §7; §Scope for 0.9.9 above.

### 0.9.6 — **PASS**

**Recorded 2026-09-01** from the maintainer's report that the audition was completed and passed
against the final 0.9.6 build. The maintainer is the human who performed it, and that attestation
is the Level-5 evidence — precondition 7 asks for a person's judgement, not for a machine-checkable
artifact.

| Field | Value |
|---|---|
| Verdict | **PASS** |
| Build | The final 0.9.6 build (as stated by the maintainer) |
| Performed by | The maintainer |
| Audition date | **NOT RECORDED** |
| DAW + version | **NOT RECORDED** |
| OS + CPU architecture | **NOT RECORDED** |
| Plugin format (VST3 / AU / Standalone) | **NOT RECORDED** |
| Session used | **NOT RECORDED** |
| Per-item outcome (groups A–E) | **NOT RECORDED** |
| Exact artifact / commit identity | **NOT RECORDED** |

**The NOT RECORDED rows are deliberate and must not be filled in by anyone who was not there.**
They are marked rather than omitted so a later reader can see the record's granularity: this is a
verdict-level record, not an item-level one. Nobody transcribing it — human or agent — should
infer a DAW, an OS, a format or a per-item result from the protocol above; the protocol says what
*should* be exercised, not what *was*.

**Consequence for correspondence-to-build.** Because no artifact or commit identity was supplied,
the correspondence between what was auditioned and the final 0.9.6 build rests on the maintainer's
statement rather than on anything checkable in this repository. That is sufficient for precondition
7, which is a human sign-off by definition. It is recorded here so the basis of the claim is
visible.

**For the next release:** capturing the table above at audition time costs a minute and makes the
record self-supporting. The blank rows here are the argument for doing that.

Evidence [Verified]: docs/policies/RELEASE_POLICY.md (precondition 7);
docs/policies/TESTING_POLICY.md (Level 5); docs/procedures/RELEASE_PROCESS.md §7; CHANGELOG.md
`[0.9.6]`.
