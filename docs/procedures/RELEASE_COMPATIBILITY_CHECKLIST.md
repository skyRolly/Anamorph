# RELEASE_COMPATIBILITY_CHECKLIST.md

Hard compatibility gate. **Every box must be checked before a release ships.** This enforces
`docs/policies/COMPATIBILITY_POLICY.md` and its subset policies. A failed item blocks the release
(or requires the COMPATIBILITY_POLICY exception: ADR + migration + Architecture Review).

## Completion record — 0.9.9 (2026-09-28)

**Eight of eight boxes are checked for 0.9.9: six re-run with measured evidence, and two, items 5 and 7,
on the owner's attestation.** The owner reported both verifications complete on 2026-09-28; the hosts,
operating systems, plug-in formats and automation lanes were not supplied and are NOT RECORDED. The
0.9.6 boxes did not carry forward, because 0.9.7–0.9.9 touched every area this checklist covers:
- 0.9.7 changed the reported latency (ADR-0034).
- 0.9.8 moved JUCE to 9.0.2 (ADR-0054) and changed how an edit, Undo and host automation meet.
- 0.9.9 renamed two host-visible names (Dimensional), bounded the preset and host-state parsers
  (ADR-0055/0056), and changed the adoption of bulk swaps (ADR-0057).

Box 6 set the precedent: a touched box is re-run, not carried. The 0.9.6 record below stays as written.

| # | Item | 0.9.9 |
|---|---|---|
| 1 | Parameter IDs unchanged | **PASS** — two display names changed, which is allowed; no ID renamed or removed |
| 2 | Serialization schema verified | **PASS** |
| 3 | Presets migrated | **PASS** — including a user preset written by 0.9.8. The audible half belongs to the 0.9.9 audition, which the owner completed (`LEVEL5_AUDITION.md` §Recorded auditions) |
| 4 | Pluginval passed (both modes) | **PASS** — run here, not inferred from CI |
| 5 | Host matrix verified | **PASS** — attested by the owner, 2026-09-28 (hosts NOT RECORDED) |
| 6 | Latency reporting verified | **PASS** — re-run for 0.9.9 |
| 7 | Automation playback verified | **PASS** — attested by the owner, 2026-09-28 (host and lanes NOT RECORDED) |
| 8 | Session reload verified | **PASS** — a real session written by a rebuilt 0.9.8 binary |

## Completion record — 0.9.6 (2026-09-01)

**Eight of eight boxes are checked: six with measured evidence, two on the maintainer's
attestation.** Every measured tick cites what was run and what it produced. The two attested ones
are the items this checklist's own §Notes names as impossible to prove headlessly; they were
confirmed by the maintainer on 2026-09-01, and the fields that confirmation did not supply are
marked NOT RECORDED rather than inferred.

| # | Item | 0.9.6 |
|---|---|---|
| 1 | Parameter IDs unchanged | **PASS** |
| 2 | Serialization schema verified | **PASS** |
| 3 | Presets migrated | **PASS** (audible half via the Level-5 audition) |
| 4 | Pluginval passed (both modes) | **PASS** — run here, not inferred from CI |
| 5 | Host matrix verified | **PASS** — attested by the maintainer, 2026-09-01 (hosts not recorded) |
| 6 | Latency reporting verified | **PASS — re-verified for 0.9.7** (ADR-0034 changed the reported value; the box is re-run, not carried) |
| 7 | Automation playback verified | **PASS** — attested by the maintainer, 2026-09-01 (host / lanes not recorded) |
| 8 | Session reload verified | **PASS** — real 0.9.5 field capture |

**Items 5 and 7 are closed on the maintainer's attestation (2026-09-01), not on the Level-5
audition record.** Rounds 7–10 declined to tick them from that record, whose per-item outcomes are
NOT RECORDED, and said what would close them: the maintainer confirming the checks were performed.
The maintainer has now confirmed both as completed and verified. As with the audition record, only
what was supplied is recorded — the verdict, the date and the performer; the hosts, operating
systems, plug-in formats and automation lanes exercised were not supplied and are NOT RECORDED.
This satisfies `RELEASE_POLICY.md` precondition 2 the way precondition 7 was satisfied: by the
human sign-off the item is defined as.

## Checklist

- [x] **Parameter IDs unchanged** — **PASS (0.9.9, 2026-09-28; 0.9.6, 2026-09-01).** — diff the parameter set against the previous release; no `pid::`
      ID renamed or removed. (Display-name changes are allowed; record in CHANGELOG.)
      Ref: `docs/architecture/PARAMETER_REGISTRY.md`, `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md`.
      *Automated since the 0.8.13 cycle:* the registry-snapshot test in `tests/state_tests.cpp`
      (`AnamorphStateTests`, CI-blocking on all three platforms) fails on any ID/name/order/
      range/automation-flag change vs `tests/fixtures/parameter_registry.snapshot`.
- [x] **Serialization schema verified** — **PASS (0.9.9, 2026-09-28; 0.9.6, 2026-09-01).** — no field removed or semantically changed in
      `AnamorphRoot` / `ANAMORPH` (APVTS) / `ANAMORPH_INTERNAL` / `AB`; additions tolerate absence.
      Ref: `docs/architecture/SERIALIZATION_REGISTRY.md`, `docs/policies/SESSION_COMPATIBILITY_POLICY.md`.
      *Automated since the 0.8.13 cycle:* schema-shape + raw-exact round-trip + the three
      legacy-format fixtures in `tests/state_tests.cpp`. The cross-version step below stays manual.
- [x] **Presets migrated** — **PASS (0.9.9, 2026-09-28; 0.9.6, 2026-09-01).** — factory presets and a representative user `.anamorph` still load and
      sound identical. Ref: `src/PresetManager.cpp`.
      *Partially automated:* `tests/state_tests.cpp` proves save→reload structural equality +
      exclusion rules + factory loadability; "sound identical" remains a Level-5 (audition) check.
- [x] **Pluginval passed (both modes)** — **PASS (0.9.9, 2026-09-28; 0.9.6, 2026-09-01).** — `scripts/run-pluginval.sh <n> deterministic` **and**
      `scripts/run-pluginval.sh <n> randomise` (`--randomise` ×3) pass on the Linux gate, where
      `<n>` is `ANAMORPH_PLUGINVAL_STRICTNESS` from `.github/workflows/build.yml` — read it there
      rather than from this line, so a raise cannot leave this checklist certifying the old bar.
      Ref: `docs/procedures/TESTING.md`.
- [x] **Host matrix verified** — **PASS (0.9.9, 2026-09-28, attested by the owner; hosts NOT RECORDED; 0.9.6, 2026-09-01, attested by the maintainer; hosts NOT RECORDED).** — load in the target hosts and confirm load + automation + state.
      (Currently Unverified in-repo; this requires manual DAW testing —
      `docs/architecture/COMPATIBILITY_MATRIX.md`.)
- [x] **Latency reporting verified** — **PASS (0.9.9, 2026-09-28; 0.9.7, 2026-09-03, re-run because ADR-0034 changed the
      reported value, so 0.9.6's PASS does not carry).** — reported PDC matches the actual chain
      delay across the oversampling settings; OS-off reports 0; and the number is now a function of
      the Oversampling setting alone, so no parameter move can change it. Ref:
      `docs/architecture/LATENCY_MODEL.md`; tests `testBypassNullAndLatency` (Test 3+4) and
      `testOversamplingLatencyIsFactorOnly` (Test 52). **This box's compatibility meaning changed and
      the change is deliberate**: a session saved in 0.9.6 with Oversampling selected and Drive at 0
      reopens in 0.9.7 with the host compensating a few samples where it previously compensated none.
      Nothing in the session file changed — the schema is untouched and item 2 is unaffected — and the
      audio is the same audio, arriving at the position the plug-in now correctly declares.
- [x] **Automation playback verified** — **PASS (0.9.9, 2026-09-28, attested by the owner; host and lanes NOT RECORDED; 0.9.6, 2026-09-01, attested by the maintainer; host and lanes NOT RECORDED).** — recorded automation on host-visible parameters plays back
      with unchanged meaning. Ref: `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md`.
- [x] **Session reload verified** — **PASS (0.9.9, 2026-09-28, a 0.9.8-written session; 0.9.6, 2026-09-01).** — save a session in the previous version, load it in the new
      version: sound, preset name, dirty-star, and both A/B slots reproduce exactly.
      Ref: `docs/architecture/STATE_SERIALIZATION.md`.
      *Partially automated:* the round-trip + legacy-fixture tests prove the CURRENT binary reads
      the modelled 0.2 / pre-0.6.4 / pre-0.8.4 formats; the true N−1-binary → N load remains
      this manual step (the fixtures are reconstructions, not field captures —
      `worklogs/STATE_HARNESS_0.8.13.md` §5).

## If any box cannot be checked

Stop. Either fix the regression, or — if the change is intentional — satisfy the
`COMPATIBILITY_POLICY.md` exception: an **ADR** + a **migration plan** + **Architecture Review**
sign-off. Document the migration in `STATE_SERIALIZATION.md` / `PARAMETER_REGISTRY.md` and the
CHANGELOG.

## Notes

- The headless gate (DSP self-tests + pluginval) verifies several of these structurally
  (latency, bypass null, no-NaN), but **Host matrix**, **Automation playback**, and **Session
  reload** require manual validation — they cannot be fully proven headlessly.
- The reference precedent for a compatible surface change *with migration* is the 0.8.4 move of
  view params out of the APVTS (`InternalState::migrateFromLegacyApvts`, ADR-0010).

## Evidence for the 0.9.9 completion

Recorded 2026-09-28 against the working tree of `claude/anamorph-comprehensive-review-90tpty` (PR #159) on
`main` @ `b755cfb`. The previous version is **0.9.8**. Its last `main` commit is `661a90b`, which has the same JUCE
pin and the same build configuration apart from the version number. 0.9.9 is the first tag (ADR-0058). No
version before it was tagged, so "the previous version" means the previous *source* version, as it did for
0.9.6.

1. **Parameter IDs unchanged — PASS.** State test 2 (the registry snapshot) passes. Against `661a90b`, the
   snapshot differs in three places:
   - `stepText3` (the pre-0.9.9 label → `Dimensional`);
   - `dimMode`'s `name` (the pre-0.9.9 label → `Dimensional Style`);
   - the two header comment lines.

   `paramCount=36`. Every ID, the choice order, the ranges and the automation flags are unchanged. A display-name
   change is allowed (`PARAMETER_COMPATIBILITY_POLICY.md` rule 2, ADR-0002) and is recorded in `CHANGELOG.md`
   `[0.9.9]` Changed. The 0.9.8-written session in item 8 carries all 36 IDs, and 0.9.9 finds all 36.
2. **Serialization schema verified — PASS.**
   - State tests 1 (schema shape), 3 (raw-exact round-trip), 4/5/6 (the three legacy fixtures), 25 (the 0.9.5
     field capture) and 114 (the ADR-0055 preset-file boundary) pass.
   - What 0.9.8–0.9.9 changed in `SERIALIZATION_REGISTRY.md` is anchors, plus the ADR-0055/0056 parser
     boundaries. Those boundaries refuse malformed input and remove or re-mean no field.
   - Item 8 is the cross-version proof on a real 0.9.8 session.
3. **Presets migrated — PASS.**
   - State tests 8, 10, 11 and 114 pass.
   - A user preset written by the 0.9.8 binary (`saveUser`, then read back by that binary) loads in 0.9.9
     with all 36 parameters equal to nine digits. That includes `algorithm` 3 and `dimMode` 3, as well as the
     name and the clean modified-star.
   - The factory table is unchanged since `661a90b`.
   - "Sound identical" is the 0.9.9 Level-5 audition's to judge. The owner completed it (reported 2026-09-28;
     `LEVEL5_AUDITION.md` §Recorded auditions, 0.9.9); its per-item outcomes are NOT RECORDED.
     Item 8's render is the headless half.
4. **Pluginval passed (both modes) — PASS.** Run in this environment with pluginval 1.0.4 at strictness **10**, a figure read from
   `ANAMORPH_PLUGINVAL_STRICTNESS` in `build.yml`. It ran under `xvfb` against the VST3 built from this tree:
   - `scripts/run-pluginval.sh 10 deterministic vst3`: *ALL 3 deterministic pass(es) succeeded*, seed `0x1`;
   - `scripts/run-pluginval.sh 10 randomise vst3`: *ALL 3 randomise pass(es) succeeded*.

   Both exit 0. The macOS AU and Windows gates run in CI and are not restated here.
5. **Host matrix verified — PASS, attested by the owner (2026-09-28).** 0.9.8 moved JUCE to 9.0.2, which
   changed the plug-in wrappers every host loads, so the 2026-09-01 attestation did not cover this build, and
   this environment is headless with no DAW.
   - The owner reported the 0.9.9 host-matrix verification complete on 2026-09-28, and that report is the
     evidence, recorded the way the 0.9.6 one was.
   - Hosts, operating systems and plug-in formats exercised: NOT RECORDED. They were not supplied, and they
     are not inferred from the matrix.
   - The automated evidence stays separate: pluginval (item 4) and the CI gates on all three platforms. It is
     not a substitute for loading the plug-in in a host.
6. **Latency reporting verified — PASS, re-run for 0.9.9.**
   - DSP Test 3+4 and Test 52 pass, as do State tests 22 and 24.
   - Since `661a90b`, the reported value is computed by the same `engine.predictLatency` from the
     ADR-0057 engine snapshot, where it used to use a fresh `toEngine` read, so the number is unchanged.
   - 0.9.9 changed the reported **tail** (0.1 s → 0.5 s). That is not latency and is not a box here; the audition
     covers it (§Scope for 0.9.9, E).
7. **Automation playback verified — PASS, attested by the owner (2026-09-28).** 0.9.8 changed how a user's
   edit, Undo and a host's automation meet (`CHANGELOG.md` `[0.9.8]`), and 0.9.9 renamed the two names a host
   shows for the fourth algorithm and its voicing, so the 2026-09-01 attestation did not carry.
   - The owner reported the 0.9.9 automation-playback verification complete on 2026-09-28, and that report is
     the evidence.
   - Host and automation lanes exercised: NOT RECORDED. They were not supplied.
   - Automated, and separate: parameter *meaning* is pinned by item 1's registry snapshot and by item 8's
     engine mapping.
8. **Session reload verified — PASS, against a real 0.9.8 session.**
   - **How the capture was made.** `661a90b`'s `src/` and `tests/` were extracted with `git archive` and built
     as the `AnamorphStateTests` link, with a writer harness in place of the suite, against the same JUCE 9.0.2
     pin. That binary wrote a session and a manifest of what it believed the state was. A reader harness linked
     against this tree loaded the capture. Both harnesses are scratch; neither is committed.
   - **The session.** Slot A is active and holds factory preset *Synth Dimension* (`algorithm` 3), then
     the voicing → *Lush* (`dimMode` 3) and Amount 0.72, so it is dirty. Slot B holds *Tape Chorus* with
     Width and Chorus Depth edited, so the two slots differ.
   - **Result: 28 checks, 0 failures.**
     - Every parameter of both slots reproduces to nine digits (36 of 36 each).
     - The preset name *Synth Dimension*, the modified-star and the active slot reproduce.
     - `algorithm` is still raw 3 and reaches the engine as `Algorithm::Dimensional`. `dimMode` is still raw 3 and
       reaches it as mode 4.
     - 0.9.8 showed the pre-0.9.9 labels for the algorithm and its voicing control. 0.9.9 shows `Dimensional`
       and `Dimensional Style` for the same values, with the voicing still *Lush*.
     - 400 blocks of the same input through the restored session render **bit-identically** in the two builds.
   - **Controls.** A manifest with one slot-B value moved by 1e-4 fails the slot-B check. A reference render
     with one sample moved by 1e-7 fails the render check.
   - **SHA-256** of the capture, `capture_0_9_8.session` (10,592 bytes):
     `ef9f7442c40cdab56e56c44192d0e67593ab19e40e019af856ea4e7f3ea13b5e`. The writer is deterministic: two runs
     wrote the same bytes.

## Evidence for the 0.9.6 completion

Recorded 2026-09-01 against the working tree at the head of
`claude/anamorph-ci-workflow-8iu7yk`. Test counts are from that run.

1. **Parameter IDs unchanged — PASS.** `AnamorphStateTests` State test 2 (parameter registry
   snapshot) passes, and `tests/fixtures/parameter_registry.snapshot` is byte-identical to
   `origin/main` and unmodified since the 0.8.13 cycle (`git diff origin/main` empty; last touching
   commit `d6bdb13`). No ID renamed or removed.
2. **Serialization schema verified — PASS.** State tests 1 (schema shape vs
   `SERIALIZATION_REGISTRY.md`), 3 (raw-exact byte-stable round-trip) and the three legacy fixtures
   (4/5/6) pass. The cross-version half this item used to defer to is now item 8.
3. **Presets migrated — PASS.** State tests 8 (save→reload round-trip + exclusions), 10
   (factory/user identity with a shared name) and 11 (factory-preset id integrity) pass. The
   "sound identical" half is a Level-5 judgement and is covered by the 0.9.6 audition
   (`LEVEL5_AUDITION.md`, PASS).
4. **Pluginval passed (both modes) — PASS.** Run in this environment at strictness **10** — read
   from `ANAMORPH_PLUGINVAL_STRICTNESS` in `build.yml`, not from this file — against the built VST3
   under `xvfb`:
   `scripts/run-pluginval.sh 10 deterministic vst3` → *ALL 3 deterministic passes succeeded*;
   `scripts/run-pluginval.sh 10 randomise vst3` → *ALL 3 randomise passes succeeded*. Both exit 0.
   This is a local run of the same script the Linux gate uses; the macOS AU and Windows gates run in
   CI and are not restated here.
5. **Host matrix verified — PASS, attested.** Confirmed by the maintainer on 2026-09-01 as
   completed and verified. Not performed in this environment (headless; see
   `COMPATIBILITY_MATRIX.md`). Hosts, operating systems and plug-in formats exercised: NOT RECORDED
   — not supplied, and not inferred from the matrix.
6. **Latency reporting verified — PASS, re-run for 0.9.7.** `AnamorphTests` Test 3+4 (true-bypass
   null + latency reporting) passes, covering reported PDC against the actual chain delay with OS off
   reporting 0. **Test 52** (0.9.7, ADR-0034) adds the state the suite had never covered — a factor
   selected with the oversampling wrap skipped — and pins reported == actual there by impulse, the
   whole {factor}×{algorithm}×{drive} grid, a live Drive sweep through the engagement threshold, and
   the bit-exactness that proves the wrap is still genuinely skipped. Two 0.9.6 additions extend it:
   State test 22 (an off-message-thread change is deferred to the processor timer and delivers the
   CORRECT value) and State test 24 (a restore reports the RESTORED state's latency, not a rejected
   value's) — both re-instrumented in 0.9.7 onto the Oversampling Setting, since no parameter bears
   latency any more.
7. **Automation playback verified — PASS, attested.** Confirmed by the maintainer on 2026-09-01
   as completed and verified. Parameter *meaning* is pinned structurally by item 1's registry
   snapshot; playback in a host is the maintainer's attestation. Host and automation lanes
   exercised: NOT RECORDED — not supplied.
8. **Session reload verified — PASS, and no longer a reconstruction.** The previous version's
   binary was rebuilt from source (the tree at `2c5e760^`, i.e. 0.9.5, with its own JUCE pin) and
   used to WRITE a real session: `tests/fixtures/field_capture_0_9_5.session` (10,629 bytes) plus
   a `.manifest` recording what that binary believed the state was, including the B slot. State
   test 25 loads the capture into 0.9.6 and asserts against those numbers — so it asks "does
   0.9.6 reproduce what 0.9.5 had", not "does 0.9.6 agree with itself". All four things this
   item names reproduce exactly: sound (5 parameters, both slots), preset name (`Gentle Width`),
   dirty-star (set), and both A/B slots (which differ from each other, so the B leg is not vacuous).

   This closes the caveat the item carried: the pre-existing legacy fixtures are reconstructions
   built by current code, which can only contain what today's understanding says an old format held.
   This one was written by the old binary.

**Note on scope.** 0.9.6 will be the first tagged release; none of 0.9.0–0.9.5 was ever tagged,
so "the previous version" means the previous *source* version, reachable to anyone who built or
took a CI artifact. That is the transition item 8 now covers.
