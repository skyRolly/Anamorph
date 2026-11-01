# NR-012 — Bounded oversampling alias study

**Date:** 2026-11-01 (Asia/Shanghai). **Source:** integrated main
`00de8c7796f37d08a338256e01830bb312ba105c`, pinned JUCE9.0.4.
**Status:** investigated / measured locally; retain existing behavioral assertions.
No DSP correction, new quality gate or release change.

## CI validation dependency — 2026-11-01 UTC

The original study head `2d56eac` is preserved. Its mandatory sanitizer job hit
the unchanged 60-minute limit twice; cancellation remains an incomplete gate.
PR #201 now targets unmerged #203 (`2c242e5`) so this study's own final head can
use the independently validated concurrent memcheck runner. The branch merge is
additive, preserving both parents; probe/runner/CSV/metadata and engine inputs
are byte-identical to the measured study. Both required suites, detector argv,
coverage warnings and job cap are retained. Three documentation insertion
conflicts preserve both evidence records. This changes validation infrastructure
dependency, not the alias experiment, product architecture or release contents.

Fresh final-head CI and the final SHA are tracked in the
[#201 ledger](https://github.com/skyRolly/Anamorph/pull/201#issuecomment-6549206340).
#203's successful matrix does not pass this new head. No GitHub PR merge is
authorized or performed; main remains `00de8c7`, and the separate NR-006
implementation remains in #202 under its own acceptance gates.

## Question and disposition

R7 §H and NR-012 distinguish proving that oversampling runs from measuring its
alias reduction. This study measures a controlled driven tone through the actual
engine, rather than inferring quality from latency or wrap engagement.

**Selected disposition: retain current behavioral coverage; do not add a universal
alias threshold or monotonic-factor assertion from this sample.** All tested
oversampling factors reduce the selected aggregate alias metric relative to Off,
but one third-harmonic bin alone misses dominant higher-order folds, and ×8 is not
uniformly lower than ×4 in the measured metric. No quality defect is established.
Future assertions need a specified frequency/level grid, metric, reference and
native-platform validation. No DSP/filter change is justified by these observations.

## Method

Nonshipping `tests/experiments/nr012_aliasing_study.cpp` uses the actual engine,
Haas with Amount=0, Drive=12 dB, Mix=1, neutral width, no Multiband/Mono Maker/AGM,
identical L/R tone input with peak 0.4. Factors Off/2×/4×/8× are selected before
prepare. FFT size 32768; bins 7501 and 11203 are coprime to N and above Nyquist/3,
so every modeled odd harmonic from 3 onward is out of the original passband.
At 48 kHz the tones are10987.79296875 and16410.64453125 Hz; at96 kHz both scale
by two. This is four operating cells, not four independent normalized frequencies.

The same float-valued period is reused to avoid accumulated oscillator phase
error. Process 256-sample blocks, discard three complete periods, capture two
successive periods. FFT includes the target bin and its immediate neighbors.
Normalize each selected folded-bin power to measured fundamental power. Record
the third-fold power, sum of modeled odd 3…31 folded powers, dominant modeled bin,
fundamental/output peak, reported engine latency and adjacent-period max delta.
No window is needed for this coherent stationary input/capture. This is not THD,
an integrated full-band alias measure or a perceptual/audio-quality score.

### Instrument controls

- Inject a known 0.01-peak sinusoid at the third-fold bin over the 0.4 carrier.
  Measured ratio must match its analytical −32.0412 dBc within 0.02 dB. Omitting
  that bin must fail the same calibration. This tolerance calibrates the measuring
  instrument, not the production DSP's alias quality.
- Drive 0 at every selected factor and bypassed Drive 12 provide linear references;
  the carrier stays 0.4 within the instrument's numerical allowance.
- Driven Off must exceed its measured linear floor, proving Drive was engaged.
- Every sample is finite and identical between channels (signed zeros normalized
  for the representation comparison); latency agrees with the engine prediction.
- Two complete executions produce byte-identical 36-row CSV; adjacent captured
  periods have measured maximum sample delta 0 in all 36 observations.

## Results and interpretation

Local Clang22.1.8/libstdc++14, Debian13 x86-64, Release/shipped Haswell flags.
Values below are the **odd 3…31 aggregate dBc**, normalized to the measured carrier:

| Normalized tone / 48 kHz frequency | Off | 2× | 4× | 8× |
|---|---|---|---|---|
| bin 7501 /10987.793 Hz | −17.565 | −48.125 | −107.045 | −100.583 |
| bin 11203 /16410.645 Hz | −17.565 | −32.949 | −78.410 | −103.646 |

At 96 kHz the scaled-frequency cells reproduce these values. Linear references
are around−167 dBc in this selected-bin metric; bypass references also remain
far below driven Off. Fundamental peaks in driven cases remain approximately
1.03680 across factors, so the aggregate improvements are not merely lower carrier
gain. Filtered waveform peaks can exceed the Off peak: there is no output clipper
and peak level alone is not this alias metric.

Third-fold-only readings would misleadingly make 2× look nearly complete
(approximately−99.5/−114.0 dBc). Its aggregate is dominated by the modeled 7th/5th
fold bins instead. The high tone's4× aggregate is dominated by the11th fold.
These observations are consistent with higher harmonics folding inside the
oversampled rate before downsampling; the study does not independently separate
every interpolation/filter/rounding contribution.

The low tone's8× result is higher than4× on this metric, while still far below
Off. That alone establishes neither worse perceived quality nor a filter defect.
A denser level/frequency grid, independent/high-precision reference and native
Mac/Windows results are needed before making a general assertion. Runtime FFT
and float errors bound very low observations; no universal noise-floor value is claimed.

Raw [measurements](nr012-alias-study/measurements.csv) and
[source/provenance metadata](nr012-alias-study/metadata.json) preserve all 36 cells,
including controls. They are dated measurement evidence, not golden expectations.

## Reproduction and validation limits

Configure/build the existing optional DSP dump target with the pinned JUCE, then:

```bash
cmake -B build -DANAMORPH_BUILD_DSPDUMP=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target AnamorphDspDump
python3 scripts/run-alias-study.py --build-dir build --output-dir /tmp/nr012-study
/tmp/nr012-study/alias-study > /tmp/nr012-repeat.csv
cmp /tmp/nr012-study/measurements.csv /tmp/nr012-repeat.csv
```

The Linux/Ninja utility reuses that target's **actual compile/link flags and engine/
JUCE objects**, replaces only the console-probe object/output, and records its
commands. It does not rewrite a CMake file, target or production source. Compound/
ambiguous commands are refused. Native MSVC/Apple builds need their own explicit
study target/invocation; this Linux utility is not native-platform evidence.
There is no new production or mandatory quality test. Existing warnings in the
unchanged ScopeBuffer header remain; the study introduces no warning suppression.

Native quality interpretation, broader test design and any performance budget
remain unverified. Historical GCC shipping ABI failure is not repaired by a
Clang study. The 0.10.0 tag/draft/assets/version are untouched; no audition or
publication permission follows from spectra.

## Continuation

NR-011 is separately submitted as [PR #200](https://github.com/skyRolly/Anamorph/pull/200).
The owner has now explicitly approved ADR-0070 in
[PR #199](https://github.com/skyRolly/Anamorph/pull/199) for implementation subject
to its native portability/performance/validation gates. Finish recording this
independent measurement, then implement NR-006 in a focused PR. That approval
does not authorize merging any new PR or changing a release. NR-003 remains Deferred;
NR-010 still requires controlled identified hardware. A fresh GitHub issue search
for REAPER returned no results; it supplies no pinned NR-013 reproducer or host metadata.
