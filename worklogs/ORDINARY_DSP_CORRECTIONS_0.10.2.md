# 0.10.2 ordinary DSP corrections and H4 assessment

Starting main: `113b9189f0ff4d3b6348c553c559c22ee8ade782`, unchanged on fresh fetch.
Product 0.10.1, target maintenance 0.10.2; JUCE remains 9.0.5 at
`d60b7a78030661574a615b586d9325ba7a7fa7dd`.
The mandatory evidence preservation was committed as `6b55dfad` before new
investigation/implementation: [durable archive](dsp-numerical-audit-0102/README.md).
Its original report, controls and raw data are the baseline evidence, not this
conversation or a temporary directory. Evidence prerequisite PR #237 remains open.

## N2-02 ownership and correction

`copyContinuous` previously preserved Bypass/Multiband Enable even though ADR-0004
assigns their targets to independent crossfades. A Normal ordinary entry and any
ordinary fade-out/fade-in refresh call this merge, followed by `updateDerived`.
The latest pending snapshot is refreshed before the merge, so its bottom cannot
restore an older control target. Removing only those fields from the preserved
partition restores live targets without changing any block boundary or tick.
An ordinary bottom does not snap these smoothers. Multiband enable warm/cold
handling stays in `process`; band-count changes still reset topology at the
bottom. Bypass scratch consumption uses the same already-targeted smoother as
its fill gate. Latency remains determined solely by the deferred OS factor.

An incomplete bulk request returns before adoption. A completed forced entry
writes pending state and defers all controls; forced fade-out refreshes pending
state without the ordinary merge. Upgrade from ordinary fade-out sets
`pendingForced` before the merge gate. The bottom snaps complete destination
smoothers and clears that flag. A later ordinary request, including one during
the forced fade-in, uses the ordinary merge again. No atomics, locks, memory
ownership, plugin/parameter identities, persistence, graph, or latency change.

The lint's old partition encoded the same bug. `DISCRETE` still declares stepped
targets; `DUCK_DEFERRED` excludes independently crossfaded targets. Exact preserved-
set checking and a mutation self-test reject swallowing a Bypass request. No
existing test assertion is removed. The live source comment claiming preservation
was neutral is corrected; the original evidence is retained in the archive.

DSP Test 77 checks non-neutral audible controls, actual duck activity, adopted
parameters, targets and an independent smoother clock. It covers 44.1/48/96 kHz,
blocks 64/127/256, both controls/directions, fade-out, fade-in, crossing bottom,
held targets, repeated/rapid returns and ordinary interruption. Completion freeze,
forced bottom, ordinary-to-forced upgrade and forced-to-ordinary re-entry are
separate assertions. State Test 159 uses real host-style writes and processBlock,
with raw-readback, adoption, tick and audible-output checks. Test-private member
access adds no shipping hooks. Exact results, negative controls and final-head
CI identities will be appended after execution, never inferred from history.

## N2-02 executed controls

GCC 14.2, Release, C++23, Haswell, contraction off; pinned JUCE source and unchanged
production objects from a fresh build of the starting SHA. The test and engine
objects were freshly compiled from the recorded files. This local incremental
run is distinct from a fresh hosted build of the final branch. Portable replay:
configure/build this checkout's `AnamorphTests` and `AnamorphStateTests` using the
JUCE 9.0.5 pin, then run:

```sh
python3 worklogs/n2-controls-0102/run_controls.py --checkout . --build /tmp/n2-build \
  --baseline /tmp/main-113b9189 --out /tmp/n2-controls
```

The baseline argument is a clean detached worktree at the starting main SHA.
The script replaces only the engine object and selects the same regressions.
Use Xvfb for the State executable on a headless machine if needed.

| Source/control | DSP Test 77 | State Test 159 | Discriminating result |
|---|---:|---:|---|
| corrected | 720 / 0 failed | 20 / 0 failed | requests and targets live immediately; exact independent smoother clocks |
| original preservation | 720 / 316 failed | 20 / 12 failed | adoption/target/tick assertions fail; short-pulse audio is absent |
| prematurely merge completed forced destination | 720 / 8 failed | not run | specifically the eight forced pre-bottom deferral assertions fail |

The extra factor/band-count entry-pulse lanes verify genuine discrete fields stay
bottom-owned and latency changes only there; the crossfade clock remains live.
The original archival overlap probe was rebuilt using the committed portable
helper: every ducked short-pulse comparison remains identical on original source,
while every no-duck positive control differs. This revalidates the historical
witness without using a prebuilt executable. Logs and file digests are in
[n2-controls-0102](n2-controls-0102/identity.json); gzip restores original bytes.
An early oracle incorrectly demanded audible duck attenuation while True Bypass
was settled On. The corrected premise checks real duck phase and no-duck control
state; its audible duck comparison applies when the raw bypass does not hide it.
That discarded oracle is not counted as production failure evidence.

The full local DSP run before the last 16 structural/factor assertions passed
5,206 checks; the final inventory adds those assertions without replacing any
existing check. Final full-suite and hosted-tip results are recorded separately.
No listening, native DAW, sample-accurate automation or hardware evidence is claimed.

The full State suite passed **16,555 checks** (16,535 existing + 20 new).
