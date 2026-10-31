# NR-011 — Differential DSP transition coverage

**Date:** 2026-11-01 (Asia/Shanghai).
**Baseline:** integrated main `00de8c7796f37d08a338256e01830bb312ba105c`.
**Status:** implemented / Ready for Review; final-head native CI and human review
remain separate. No production DSP/latency/threading behavior changed.

## Problem and selected scope

The original 32-case `AnamorphDspDump` applies one engaged parameter snapshot and
resets before hashing. It is useful but does not compare multiple live parameter
transitions, reset/idle cycles or the intermediate engine-latency sequence across
builds. Existing behavioral tests address different properties; no new sonic
defect was established. The owner authorized this independent test batch while
NR-006's architecture recommendation awaits human review in
[PR #199](https://github.com/skyRolly/Anamorph/pull/199).

Selected an **additive opt-in dump mode**, rather than changing the legacy record
matrix or creating a second executable/CMake architecture. This preserves old
tools, historical dump comparisons and ADR-0032's exact 32-row hash-only gate.
The same existing instrument can emit five new transition records for explicit
same-machine/configuration comparisons. No stored golden waveform/hash, DSP
change, cross-architecture bit identity or new Class-B numerical threshold.

## Scenarios and controls

All new cases use fixed-seed LCG input, 48 kHz, 128-sample blocks, eight phases of
32 blocks each (256 process calls). Output audio bytes are hashed separately from
the little-endian encoded engine-latency values sampled before/after every call.
Final latency is retained as an integer. This does not observe host D-1 timer or
latency-callback delivery; state tests retain those responsibilities.

| Identity | Exercised path / non-vacuity |
|---|---|
| `transition-bypass-mix` | Bypass on/off and dry/wet/partial Mix cycles; Haas/Drive-zero with selected ×4 oversampling keeps the selected-but-skipped PDC path. |
| `transition-algorithm-os` | Haas/Chorus/Dimensional/Velvet changes across Off/×2/×4/×8, returning to earlier selections; nonlinear Drive engages wrapping; latency trace must differ from inert actions. |
| `transition-crossover` | Repeated per-block low/mid/high crossover glides and return, with four unequal band widths so movement affects output. |
| `transition-solo-bands` | Engage/clear/re-engage solos, 4→2→3→4 bands, disable/re-enable Multiband and repeat cold low-band engage. |
| `transition-reset-idle` | Velvet noise→silence→audio-tail reset→zero-length idle→restart, all-state reset, repeated transport stop/idle/restart. |

Each scenario is rerun, checked finite, checked at phase ends against
`predictLatency(current parameters)` after enough time for the switch duck, and
compared with the identical-input **inert-action projection**. All five audio
hashes must be distinct. Seeded one-bit output changes must move only audio
evidence; one changed latency observation must move only the latency trace.
These checks run on every dump invocation; their success is silent on the legacy
path so its stdout stays byte-identical. No transition action labels/target
parameters are mixed into the audio hash to manufacture discrimination.

The suite is deliberately bounded, not a combinatorial matrix. It covers selected
representative transitions and cycles. M/S-solo/channel-conditioning/non-finite-
heal combinations, arbitrary rates/blocks and host wrapper delivery remain
outside this differential set. Existing behavioral tests and Accepted numerical
compatibility/contract rules remain in force.

## Record and comparator contracts

Default output remains exactly 32 three-field rows and schema-version-1 JSON.
`AnamorphDspDump --transitions` emits exactly five four-field rows:

```
scenario  audio-fnv1a-64  final-engine-latency  engine-latency-trace-fnv1a-64
```

`scripts/check-dsp-dump.py ab --suite transitions` explicitly selects the new
identity set/schema version 2. It rejects missing, duplicate, malformed, unknown,
mixed-suite and wrong-column records before comparison. Audio differences are
NUMERICAL/exit 1; final/trace latency differences are TRANSITION_LATENCY/exit 1;
invalid/unavailable required inputs are INFRASTRUCTURE/exit 2. Both raw inputs are
preserved and digested; existing report/summary alias refusal remains intact.

Legacy A/B latency remains warning-only exactly as ADR-0032 requires. This new
suite's stricter latency check is separate and explicit; no legacy expectation
was changed to get a pass. Optional `c --suite transitions` stays nonblocking on
all numerical/metadata/invalid-evidence outcomes. Intentional approved Class-B
DSP changes still require their established numerical interpretation; new hashes
are comparative evidence, never a golden-baseline veto on such changes.

Independent `scripts/fixtures/dsp-dump/transitions.txt` contains **synthetic** text
hashes, not recorded audio. The same public CLI self-test covers valid/reordered/
whitespace/case forms, audio/final/trace-only mutations, missing/duplicate/mixed/
malformed evidence, C limits and a seeded metadata-gate bypass. All older
duplicate-overwrite, output-alias, missing-input and informational-C controls stay.

## CI wiring and reproducibility

No CMake/dependency/compiler/pin change. The existing Windows A/B builds emit and
compare both suites. A/B transition audio **and** latency evidence block; C stays
informational. The same-job parser self-test runs before either use. Existing
toolset/flag liveness remains. Existing raw `d-*.txt` / `dsp-dump-*.json` retention
also captures transition evidence on failures. Existing GCC/macOS dump self-check
entry points execute the silent new transition controls as well.

```bash
cmake -B build -DANAMORPH_BUILD_DSPDUMP=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target AnamorphDspDump
# Resolve the executable in this build's AnamorphDspDump_artefacts/Release tree.
AnamorphDspDump --transitions --self-check
AnamorphDspDump --transitions > transitions-a.txt
AnamorphDspDump --transitions > transitions-b.txt
python3 scripts/check-dsp-dump.py --self-test
python3 scripts/check-dsp-dump.py ab --suite transitions --left transitions-a.txt \
  --right transitions-b.txt --report transitions-ab.json
```

Local tools: retained Debian13 x86-64, Clang22.1.8/libstdc++14, pinned JUCE9.0.4
`3fb5cbc0340758596dac39f7c8de47d562b4896e`, Release. Local commands activate
`/workspace/.cloud-setup/env.sh`; the second local build toggles only
`ANAMORPH_X86_ISA_BASELINE=OFF` for an independent build/configuration comparison.
This is not native MSVC or Mac/ARM evidence.

## Validation and review checkpoint

- Fresh pre-change instrument saved before edits. Final default dump stdout is
  byte-identical, including all 32 names/hashes/latencies and headers.
- New transition mode runs repeatably and all five rows validate. Internal
  finite/adoption/repeat/inert/sample/latency controls run before every output.
- Independent local Clang Release builds with the approved ISA option OFF versus
  the shipped Haswell configuration compare **32/32 legacy audio hashes/latencies**
  and **5/5 transition audio hashes/final latencies/traces** equal. This is local
  same-machine evidence, not the still-required native MSVC/macOS/ARM result.
- A temporary instrument mutant replaced `active = control != Control::Inert`
  with `active = false`. The compiled `--transitions --self-check` rejected it
  with **exit 3** and the inert/invalid instrument diagnostic. A `finally` block
  restored exact source bytes, rebuilt and verified identical successful records.
  This mutant changed test infrastructure only, never production source.
- Public parser self-test: **162** production-entry fixtures, including existing
  controls and the metadata-bypass mutant; no expectation weakened.
- YAML parses; both legacy/new A/B exits and existing evidence retention remain
  fail-closed. Local PowerShell is unavailable; native Windows execution remains
  a CI requirement, not inferred from structural validation.
- Documentation self-test **700** / **180** files; citation self-test **276** /
  **610** anchors; **601** table rows, whitespace and source/evidence digests pass.
  The citation gate initially caught two shifted workflow anchors; both were
  re-anchored to the same statements. Realtime **97**, portability **120**, state
  coverage **110** self-tests and scans, and release-note self-test/check pass.
- Full integrated production suites previously passed **945 DSP / 10,953 state**
  under `MALLOC_PERTURB_=1`. Production inputs are unchanged by this batch. Native
  final-head gates remain required; local results are not assigned to them.
- Existing ScopeBuffer sign-conversion/build warnings and Python invalid-escape
  warnings remain. Historical GCC14 ABI preflight failure is neither repaired
  nor relabelled by this test work; Clang remains the shipping compiler.

Local source/evidence digests and comparison summaries are retained in
`nr011-transition-evidence.json`. Final documentation/citation checks and exact-head
CI are recorded with the PR after execution. No PR merge
or release operation is authorized. NR-006 remains Proposed, no production
threading implementation; NR-003 stays Deferred.
