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

`tests/dsp_tests.cpp` has **23** tests using a `check(cond, "what")` harness, covering: MS
round-trip (bit-exact), transparent default, true-bypass null + latency match, Mono Maker
(post-Mix), Multiband mono-compat, Solo band selectivity + transparency, Level Match
(unity/no-ratchet/silence-freeze/mix-coupling/multiband-unity), crossover automation safety,
NaN recovery, and four click-free crossfade tests (transitions, bypass, multiband enable,
solo+multiband-enable). Evidence [Verified]: tests/dsp_tests.cpp (test functions :53-1218, `main` :1222).

### Adding a test

Bug fixes ship a regression test that **fails on the old code and passes on the fix** (the
project's established practice; `docs/policies/TESTING_POLICY.md`). Use the existing
`check(cond, "description")` harness and add the call in `main`.

## pluginval (VST3 conformance)

```bash
scripts/run-pluginval.sh 10      # strictness 10 = the release gate
scripts/run-pluginval.sh         # default strictness 8 (the working bar)
```

Strictness targets (spec 11.3): `5` development, `8` standard gate, `10` pre-release gold standard.
The script downloads pluginval if absent, finds the built `Anamorph.vst3`, and runs under
`xvfb-run` when available (editor open/close tests need a display).
Evidence [Verified]: scripts/run-pluginval.sh:8-44.

### Signal-only retry (known X11 host flake)

`run-pluginval.sh` retries up to 3 times **only on a signal-crash** (exit ≥ 128), never on a real
validation failure (exit < 128 fails immediately). The crash being worked around lives in
**pluginval's own JUCE** X11 `XEmbedComponent` (a `ConfigureNotify`→`callAsync` use-after-free on
rapid editor open/close), not in the plugin — the plugin already drops its OpenGL child window on
Linux to cut that traffic (ADR-0011). Evidence [Verified]: scripts/run-pluginval.sh:46-76.

## CI integration

All three CI jobs run the self-tests + pluginval; **Linux strictness-10 is the blocking gate**,
Windows/macOS are informational. See `CI_CD.md`. Evidence [Verified]: build.yml:38-42,72-86,126-137.

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
