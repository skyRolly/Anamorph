# TESTING.md

How to run and interpret the validation suite. Acceptance levels and the hard gate are defined in
`docs/policies/TESTING_POLICY.md`.

## Headless self-tests (DSP + state)

```bash
scripts/build.sh                 # build (produces AnamorphTests + AnamorphStateTests)
scripts/run-tests.sh             # runs BOTH console apps (fail-closed: a missing binary fails)
```

`run-tests.sh` finds `AnamorphTests` and `AnamorphStateTests` under `build/` and runs both; it
exits non-zero on any failed `check` or missing binary. Evidence [Verified]: scripts/run-tests.sh.

### What the tests cover

`tests/dsp_tests.cpp` has **33 DSP tests** using a `check(cond, "what")` harness, covering: MS
round-trip (bit-exact), transparent default, true-bypass null + latency match, Mono Maker
(post-Mix), Multiband mono-compat, Solo band selectivity + transparency, Level Match
(unity/no-ratchet/silence-freeze/mix-coupling/multiband-unity), crossover automation safety,
NaN recovery, four click-free crossfade tests (transitions, bypass, multiband enable,
solo+multiband-enable), the dry-align gate comb regression (`testDryAlignGateRecomb`,
Wave 2 / H4: a Mix dip after a gated full-wet stretch must re-engage the dry bank
phase-matched — the KI-#1 metric), the split-movement regression
(`testMultibandSplitDragNoPitchShift`, Test 29): the worst 100 ms pitch chunk of a 150 Hz tone
must stay < 18 cents (the accepted controlled-FM bound of the R(f) = 4·max(1, f/300) oct/s
slew-limited smoother, ADR-0015 final + slow-drag fix) through drags and the whole catch-up —
including an unbroken crawl-crossing scenario where the crossover passes the tone (~14 cents
measured; the pre-0.8.10 uncapped ~8 oct/s glide measures ~28 and the interim bare one-pole
tracker ~50, both fail) — the max spectral spur around a 1 kHz tone during a 60 Hz-cadence drag
must stay below −31 dBc (measures −41.3; the interim chained bank crossfades measure −28.5 dBc
and the rejected fref=150 cap variant −27, both fail), a discrete 4-octave target step must land
within ~200 ms via the bank crossfade, a RELEASED 6-octave flick must land by plain gliding well
under a second, and a NORMAL-SPEED drag (150 Hz → 12 kHz over 0.95 s at a 60 Hz cadence,
~600 px/s on the real display) must have its audible band edge AT the target 0.1–0.35 s after
release on both paths (the flat 4 oct/s cap of the slow-drag regression measures 0.47 of full
level on the solo path and 0.60 of the width-0 leak on the multiband path — both fail), all
click-free — on both the Multiband and Solo-monitor paths; and the forced-duck dry-fill gain regression (`testDryFillRespectsOutputGain`, Test 30):
with Output Gain at −24 dB an undo/redo-style Mix toggle must not spike beyond 2× the steady
output (the unscaled raw-level fill measures 15.8× and fails) while still filling the dip; and
the forced-swap-during-fade-out regression (`testForcedSwapDuringOrdinaryFadeOut`, Test 31): a
forced bulk swap landing while an ordinary discrete duck is still fading OUT must keep forced
semantics — stale delay-line audio must not replay after the silent bottom (the pre-fix engine,
which dropped the consumed forced request in that window, measures a 0.494-peak Haas-tail replay
against silent input and fails) — while the upgrade stays click-free and the duck still bottoms
at silence; and the high-sample-rate terminal-snap regression (`testHighRateCrossoverSnap`,
Test 32): a moved crossover must land **bitwise-exactly** on its target and let the solo
monitor's settled fast path go cold, at 44.1/48/96/192 kHz, through targets inside the measured
192 kHz float-stall zones (just above the binade edges ≥ 2048 Hz) including the worst one
(16.6 kHz) — the pre-fix glide, whose one-pole add stalls below `ulp(f)/2` while the gap is
still above the terminal-snap eps, rests 0.4688/0.9375/1.8750/3.75 Hz short at 192 kHz, never
goes cold, and fails, while the normal-rate passes double as the unchanged-behavior guard; and
the solo-monitor cold-through-drag regression (`testSoloColdThroughDrag`, Test 33, Wave 3): with
NOTHING soloed, dragging the splits at UI cadence must leave the monitor's settled fast path
engaged — the bank stays cold, the output buffer is **bit-untouched** on every block — and
re-engaging a solo must snap the cutoffs to the freshest drag targets under the engage crossfade
(the pre-Wave-3 gains+cutoffs gate wakes the bank on the first target move and glides instead of
snapping, failing both the stayed-cold and freshest-snap checks); and the parked-Haas
warm-history regression (`testHaasParkedWarmHistory`, Test 34, Wave 4): with Haas selected and
Amount settled at exactly 0 (under FTZ, as on the real audio thread) every block must pass
through **bit-untouched**, re-engaging on silent input must play back audio recorded WHILE
parked (the delay lines must keep recording through the parked fast path — this fails if a
future change stops the parked ring writes), and re-parking must return to bit-transparency
once the wet glide drains. It
additionally carries **one state-restoration robustness guard**,
`testAbActiveClampOnCorruptState` — it drives a corrupted `<AB active="…">` blob through the same
read+clamp the processor uses (`anamorph::clampAbSlotIndex`, `src/AbSlotIndex.h`) and asserts an
out-of-range A/B index can never index `abSlot[]`/`abUndo[]` out of bounds, while valid 0/1 are
preserved. Evidence [Verified]: tests/dsp_tests.cpp (`main` registers all tests).

### State-compatibility self-tests (v0.8.13 harness)

`tests/state_tests.cpp` (**12 tests**, own console target `AnamorphStateTests`) automates the
COMPATIBILITY policy family against the **real `AnamorphAudioProcessor`** (the target compiles
the plugin sources; the editor is linked but never instantiated — fully headless):
serialized-schema shape (every `SERIALIZATION_REGISTRY.md` field), a **parameter-registry
snapshot** (IDs/names/order/automation flags/step texts exact + range mappings probed at 5
normalised points, vs `tests/fixtures/parameter_registry.snapshot`), a raw-exact
save→load→save round-trip (byte-identical; APVTS + `raw` + InternalState + A/B slots + preset
meta; undo cleared), the three legacy migration paths via frozen fixtures
(`legacy_v0_2_bare_apvts.xml`, `legacy_pre_0_6_4_ab_slots.xml`,
`legacy_pre_0_8_4_view_params.xml`), corrupt/foreign-state robustness (garbage/truncated blob,
out-of-range `AB@active` clamp end-to-end, unknown future fields, corrupt slot XML), the user
preset save→reload round-trip incl. the exclusion rules (`mbSolo` reset, Bypass/`advancedMode`
untouched), A/B + view-param preservation across restore, **factory/user preset identity when a
user preset carries a factory preset's name** (0.9.2: saving under the shared name selects the USER
row, both rows stay individually selectable, an A/B round-trip keeps the identity, an undo after a
save keeps it too, a preset switch invalidates redo when the identity moves even if the two presets
sound identical **but re-picking the already-selected row does not**, and a
`.anamorph` loaded from OUTSIDE the preset folder or a user preset deleted from disk both tick
**nothing** rather than falling back to the same-named factory row),
**factory-id integrity** (ids present, unique, and every one resolving in the table — an
unresolvable id would apply the plain defaults, so exactly one factory preset may sit on the
all-defaults signature), and the **indicator identity across a session reload** (factory and user
identities restore, per A/B slot; an unresolvable factory id, a deleted user preset, a preset nested
in a SUB-folder of the preset folder, a preset whose file NAME `juce::File::isAbsolutePath` accepts
(a leading `~` on POSIX) and a pre-0.9.2 session with no identity each take their documented
fallback; and in EVERY one of those
eight paths — the seven that go through the reload helper plus the A/B slot check — the restored
parameters are asserted bit-identical, because the identity is metadata and must never influence the
sound).
Evidence [Verified]: tests/state_tests.cpp; CMakeLists.txt (`AnamorphStateTests`).

**Changing the parameter surface intentionally** (ADR + `PARAMETER_REGISTRY.md` update
required, per `PARAMETER_COMPATIBILITY_POLICY.md`): re-freeze the snapshot with
`AnamorphStateTests --write-snapshot` and let the snapshot diff be reviewed in the PR. An
**unintentional** change fails the suite on all three CI platforms — that is the point.
The registry comparison is numerically tolerant (1e-4 relative) only for the numeric fields —
the five range-mapping probes, the default, and the interval (libm ULP differences across
platforms); IDs, names, ordering, flags, counts and step texts compare exactly.

### Adding a test

Bug fixes ship a regression test that **fails on the old code and passes on the fix** (the
project's established practice; `docs/policies/TESTING_POLICY.md`). Use the existing
`check(cond, "description")` harness and add the call in `main` (DSP behaviour →
`tests/dsp_tests.cpp`; state/serialization/preset behaviour → `tests/state_tests.cpp`).

## pluginval (VST3 conformance)

```bash
scripts/run-pluginval.sh 10 deterministic   # strictness 10, fixed seed (release gate, mode A)
scripts/run-pluginval.sh 10 randomise        # strictness 10, --randomise x3 (release gate, mode B)
scripts/run-pluginval.sh 10                  # strictness 10, deterministic (default mode)
scripts/run-pluginval.sh                     # default strictness 8 (the working bar)
```

Strictness targets (spec 11.3): `5` development, `8` standard gate, `10` pre-release gold standard.
Each `mode` — `deterministic` (fixed `--random-seed 0`) and `randomise` (`--randomise`, randomised
order + time-seeded fuzzing) — runs **3 consecutive** passes. **Both modes must pass at strictness 10
on all three platforms** (Windows uses `run-pluginval.ps1`): the randomise mode exercises state
restoration under randomised conditions a fixed-seed run can miss. The script downloads pluginval if
absent, finds the built `Anamorph.vst3`, and runs under `xvfb-run` when available (Linux editor tests
need a display). Evidence [Verified]: scripts/run-pluginval.sh / scripts/run-pluginval.ps1.

### Signal-only retry (known X11 host flake)

`run-pluginval.sh` (and `run-pluginval.ps1` on Windows, without the X11-specific retry) treats a real
validation failure (exit < 128) as a failure immediately. On Linux it retries up to 3 times **only on
a signal-crash** (exit ≥ 128) to absorb a use-after-free in **pluginval's own JUCE** X11
`XEmbedComponent` (a `ConfigureNotify`→`callAsync` on rapid editor open/close), not a plugin defect —
the plugin already drops its OpenGL child window on Linux (ADR-0011). Evidence [Verified]:
scripts/run-pluginval.sh (`run_one_pass` retry).

## CI integration

All three CI jobs run the self-tests + pluginval in **both** modes (deterministic ×3 + randomise ×3),
and **all three are blocking** — Windows/macOS no longer use `continue-on-error`, so a non-zero
pluginval exit fails the job on every platform. Linux/macOS use `run-pluginval.sh`; Windows uses
`run-pluginval.ps1`. See `CI_CD.md`. Evidence [Verified]: `.github/workflows/build.yml`.

## Failure analysis

| Symptom | Likely cause | Where to look |
|---|---|---|
| A `check` assertion fails | DSP regression | the named test in `tests/dsp_tests.cpp`; compare against the invariant it guards (`docs/policies/DSP_POLICY.md`) |
| A state-test `check` fails | serialization / parameter-surface regression | the named test in `tests/state_tests.cpp`; if the change is INTENTIONAL it needs the compatibility-policy process (ADR + registry update + `--write-snapshot`) |
| pluginval exits < 128 | real validation failure | the pluginval log line; do **not** retry — it's a genuine defect |
| pluginval exits ≥ 128 (crash) | the known X11 host flake | retried automatically; if it still fails after 3 tries, treat as a failure (`run-pluginval.sh:70-90`, `run_one_pass`) |
| `AnamorphTests`/`AnamorphStateTests` `not found` | not built yet | run `scripts/build.sh` first (`run-tests.sh:10-20`) |

## Gaps in the automated coverage (known, deliberate)

Three things the gates above do **not** do. All are recorded so nobody assumes coverage that
doesn't exist:

- **GUI-lifetime defects have no headless test.** This is a **`TESTING_POLICY` rule-1 exception
  under ADR-0025**, and this entry is the register that ADR names. Its four required disclosures:

  1. *Why no reliable test exists.* The Level-2/3 surface is two console targets
     (`scripts/run-tests.sh`), and `tests/state_tests.cpp:6-8` records that it "compiles the plugin
     sources — the editor is linked but never instantiated". A defect that exists only while a
     **modal child is open and its owner is destroyed** — the 0.9.2 preset drop-down crash,
     **INC-010** — has no object to act on there. Level 4 does open and close the editor, but
     pluginval drives a host we do not control and never opens a menu first.
  2. *What replaced it.* The fix removes the lifetime rather than the symptom: a menu given
     `withParentComponent` is a child component, so `ModalComponentManager`'s
     `ComponentMovementWatcher` cancels it with result 0 on the owner's destruction **or hide**, and
     it has no independent lifetime left to get wrong. The remaining asynchronous window is closed by
     a `SafePointer`. Every other async/modal callback in the editor was audited for the same shape;
     the "Load Preset…" file chooser was the only other one, and it got the same guard. The
     mechanism was re-derived from the pinned JUCE source rather than assumed — see INC-010.
  3. *Where the gap is tracked.* Here, and cross-referenced from `POSTMORTEMS.md` INC-010.
  4. *Whether infrastructure could close it.* **Partly, and concretely.** The *structural* half —
     "is the menu a child of the editor" — becomes assertable the moment the harness instantiates an
     editor, which the sibling plug-in Anabasis already does in its own suite on all three CI
     runners. That is a harness change to prove on the Windows and macOS runners on its own merits,
     not to fold into a crash fix. The *behavioural* half — destroy the owner while the menu is
     modal and then click an item — needs a driven message loop and remains out of reach. Per
     ADR-0025 §5 this entry is revisited when that harness lands, not left standing.

- **The AU is never validated automatically.** `run-pluginval.sh` locates and validates
  `Anamorph.vst3` only, so the macOS `Anamorph.component` — the build Logic Pro and GarageBand
  load — reaches users having passed no format-conformance gate. Apple's `auval` is the tool
  (`auval -v aufx Anmr RTec`, matching the `PLUGIN_CODE` / `PLUGIN_MANUFACTURER_CODE` in
  `CMakeLists.txt:153-154`), but it only sees a component that is *registered*, so a CI step would
  have to copy the built bundle into `~/Library/Audio/Plug-Ins/Components/` and force a registry
  refresh (`killall -9 AudioComponentRegistrar`) before running it. **Ordering matters:** the
  macOS packaging step runs `strip -x` *before* it ad-hoc codesigns, and a stripped-but-unsigned
  arm64 bundle will not load — so the auval step must come **after** the whole packaging step, not
  between its strip and codesign. Whether it is reliable on a headless GitHub `macos-14` runner is
  **unverified from this repository** — see `docs/architecture/RELEASE_HARDENING_PLAN.md`.
- **No frozen golden-audio reference exists.** `tests/fixtures/` holds a parameter-registry
  snapshot and three legacy session XMLs — metadata, not audio. The DSP suite pins *behavioural
  invariants* (exact nulls, click-freeness, spectral-spur and pitch bounds, cold-path bit-identity)
  rather than a stored waveform, which is deliberate: a golden audio file would freeze bit-exact
  output and collide with the Class-B numerical changes `DSP_POLICY.md` explicitly permits. The
  right tool for "did this change alter the sound" is the **twin dump** — build the engine before
  and after, run the same scenario matrix through both, compare hashes and reported latencies —
  which is what the JUCE 9 migration used across 32 scenarios
  (`worklogs/JUCE9_MIGRATION_v0.8.13.md`). That harness is session-local and not committed, so the
  method must currently be re-created per investigation.

## What cannot be verified headlessly

Audio **sound quality** and GUI/vectorscope **visual appearance** cannot be judged in a headless
sandbox. Load the built `.vst3` in a DAW (e.g. Reaper) on a machine with audio + display. A green
build + pluginval pass is "ready to audition," not final sign-off
(`docs/policies/TESTING_POLICY.md` Level 5).
