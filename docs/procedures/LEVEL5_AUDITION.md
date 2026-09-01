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
machine code or the audible behaviour of the thing being shipped. The v0.9.4 audition of
2026-08-15 is invalid for v0.9.6 on both counts:

- **ADR-0031 / ADR-0032** changed the x86-64 machine code everywhere (`-march=haswell`,
  `-ffp-contract=off`, and the MSVC AVX2 adoption). CI's twin-dump gate proves the two builds are
  bit-identical to each other; it cannot tell you the result sounds right on real hardware.
- **v0.9.6 changed audible behaviour in exactly the windows that were previously defective** — the
  activation duck, the first-block level of a restored session, and the A/B / preset switch.

## Scope for v0.9.6

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

Evidence [Verified]: docs/policies/RELEASE_POLICY.md (precondition 7);
docs/policies/TESTING_POLICY.md (Level 5); docs/procedures/RELEASE_PROCESS.md §7; CHANGELOG.md
`[0.9.6]`.
