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
NR-006's architecture recommendation awaited human review at the original
checkpoint. The owner has since accepted ADR-0070 in
[PR #199](https://github.com/skyRolly/Anamorph/pull/199); implementation #202 and
its native/cost/visual acceptance and separate merge authorization remain gated.

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
This inert projection is an **aggregate scenario control**: it alone does not
prove each action is effective. The independent action controls below cover that
boundary. These checks run on every dump invocation; their success is silent on the legacy
path so its stdout stays byte-identical. No transition action labels/target
parameters are mixed into the audio hash to manufacture discrimination.

### Review correction: independent action controls

Accepted #200's reported finding. Each omission replay leaves the input and all
unrelated actions live, while suppressing exactly one field during one phase,
one crossover ramp segment, or one reset/transport edge. There are **57 controls**:
54 require a changed audio hash, while three deliberately masked parameter changes
require a changed trace of the engine's **adopted** parameters. Those three are
adoption coverage, not claims of audible effect. Requested values/action labels
are never mixed into output or latency hashes; the internal adoption trace hashes
only explicit fields observed through `getAdoptedParameters()`, not struct padding,
and is not exported as a new record field/schema.

| Action | Omission scope | Controls / required observation |
|---|---|---|
| Bypass | Phases 2, 4, 6, 7 | 4; phase 2 is masked by Mix=0 (adoption), others audio |
| Mix | Phases 1, 3, 5, 7 | 4; phase 3 is bypassed (adoption), others audio |
| Algorithm / OS factor | Each field separately in phases 1–6 | 12 audio |
| Low / mid / high crossover | Each field's ramp separately in phases 1–6 | 18 audio |
| Solo | Phases 1–7 | 7; phase 6 has Multiband disabled (adoption), others audio |
| Band count | Phases 3, 4, 5 | 3 audio |
| Multiband enable | Phases 6, 7 | 2 audio |
| Tail-only / all-state reset | Blocks 64 / 128 | 2 audio |
| Transport stop/resume | Blocks 64, 96, 160, 224, 240 | 5 audio |

A phase is 32 blocks. Parameter holds with unchanged targets and the fixed
noise/silence/zero-length input epochs are shared stimulus, not additional
audibility assertions. The omission granularity is a phase/field or ramp segment,
not each repeated per-block assignment or each sample. Transport now calls only
on scheduled edges, so an omitted resume is not repaired by an identical hold
call. An additional final stop at block 240 exposes the preceding resume's effect
on Velvet's next falling edge. This intentionally changes only the new reset/idle
record's audio hash; the other four transition records, all latency evidence and
the legacy 32-row stdout remain identical to the prior fixture under the same
configuration. No production DSP/transport/latency behavior changes.

Five compiled single-action mutants separately remove Mix phase 5, the mid split
in phase 3, tail reset, resume at block 96, and the masked Mix phase 3 write.
Each passes the original aggregate-only assertions and fails its new named
action control with exit 3. Restoring the source passes all controls. The
[review evidence](nr011-action-review-evidence.json) records source/log identities,
mutant results, comparator checks and legacy byte preservation; the original
`nr011-transition-evidence.json` remains historical evidence for the earlier fixture.

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
or release operation is authorized. NR-006's architecture is owner-Accepted in
#199 and its implementation is separately reviewed in #202; neither is integrated.
NR-003 stays Deferred.


## Action-review validation checkpoint

Corrected fixture passes all five scenario checks, 57 action controls and existing
sample/latency instrument controls. Default stdout remains byte-identical to
`104f60e` under the same flags; both shipped Haswell/FP-contract-off and the
independent no-ISA configuration preserve their own legacy output. Repeated and
cross-configuration new transition comparisons report identical audio, final
latency and traces. Only the intended reset/idle audio differs from the earlier
fixture; four other records and all five latency values/traces are preserved.
Parser self-test still passes 162 production-entry fixtures and its existing
negative controls. Five compiled action mutants pass aggregate-only checks and
fail the new isolated checks; restored source passes. Raw captures and named
failure diagnostics are retained in [nr011-action-review/](nr011-action-review/).

Documentation self-test 700 cases / 180 files, citation self-test 276 cases /
610 anchors against main, Markdown tables/links and whitespace checks pass.
Build emits only the unchanged ScopeBuffer sign-conversion warnings; no baseline
is suppressed or revised. Production source tree is unchanged. Exact-head native,
Windows A/B and sanitizer/analyzer CI is recorded separately in the PR discussion;
the earlier `104f60e` successes do not qualify this correction.
