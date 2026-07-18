# TESTING.md

How to run and interpret the validation suite. Acceptance levels and the hard gate are defined in
`docs/policies/TESTING_POLICY.md`.

## DSP self-tests

```bash
scripts/build.sh                 # build (produces AnamorphTests)
scripts/run-tests.sh             # runs the AnamorphTests console app
```

`run-tests.sh` finds `AnamorphTests` under `build/` and runs it; it exits non-zero on any failed
`check`. Evidence [Verified]: scripts/run-tests.sh:7-13.

### What the tests cover

`tests/dsp_tests.cpp` has **32 DSP tests** using a `check(cond, "what")` harness, covering: MS
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
snapping, failing both the stayed-cold and freshest-snap checks). It
additionally carries **one state-restoration robustness guard**,
`testAbActiveClampOnCorruptState` — it drives a corrupted `<AB active="…">` blob through the same
read+clamp the processor uses (`anamorph::clampAbSlotIndex`, `src/AbSlotIndex.h`) and asserts an
out-of-range A/B index can never index `abSlot[]`/`abUndo[]` out of bounds, while valid 0/1 are
preserved. Evidence [Verified]: tests/dsp_tests.cpp (`main` registers all tests).

### Adding a test

Bug fixes ship a regression test that **fails on the old code and passes on the fix** (the
project's established practice; `docs/policies/TESTING_POLICY.md`). Use the existing
`check(cond, "description")` harness and add the call in `main`.

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
| pluginval exits < 128 | real validation failure | the pluginval log line; do **not** retry — it's a genuine defect |
| pluginval exits ≥ 128 (crash) | the known X11 host flake | retried automatically; if it still fails after 3 tries, treat as a failure (`run-pluginval.sh:75`) |
| `AnamorphTests not found` | not built yet | run `scripts/build.sh` first (`run-tests.sh:8-11`) |

## What cannot be verified headlessly

Audio **sound quality** and GUI/vectorscope **visual appearance** cannot be judged in a headless
sandbox. Load the built `.vst3` in a DAW (e.g. Reaper) on a machine with audio + display. A green
build + pluginval pass is "ready to audition," not final sign-off
(`docs/policies/TESTING_POLICY.md` Level 5).
