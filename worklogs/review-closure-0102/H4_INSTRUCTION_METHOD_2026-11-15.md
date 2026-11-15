# Matched H4 instruction comparison method — 2026-11-15

These are nonshipping, virtual-host instruction experiments, not native CPU
percentages, audio deadlines, wall-clock distributions or plugin-capacity
measurements. Source base `003c9a1a2558da6bd89e2624421a6e205eb8acdf`, Release,
GCC 14 and pinned JUCE are common. The selected original H4, historical A2
output guard and consumption-aware diagnostic have separately committed source
trees and patches. The same eight committed workloads are specified for each;
results count whole-engine `setParameters` plus `process`, 256 measured blocks.
The observer is outside the counted region. It records actual running x4
oversampling and actual fade blocks; a separate bound records genuinely
sustained glide samples. Cases cover one through four bands, x4 Drive wrapping,
partial Mix, repeated crossover jumps/fades and sustained glides at 48 kHz/128.
They do not cover Match, recovery cost, re-entry spikes, allocations, other
rates/block sizes, time distributions or native deadline safety.

The `aec8a89beb1912a8178c01370d8f2633580f3636` specifications require each
Callgrind side file as a new hashed artifact, capture program stdout/stderr and
exits, and preserve product source, generated-input, objects and executable
identities. `callgrind` 3.24.0 runs in image
`sha256:58079116ef549d981c45d09860e24873d06eca2955f2e2795032ef7eb0177ac4`;
its compiler and system libraries are trusted premises, not host attestation.
The host reports KVM and Intel Xeon Platinum 8573C. The virtualized CPU is not
representative hardware evidence. Run checksums and path observers are necessary
execution premises, not a complete signal-correctness oracle.

The first attempt to install Valgrind into an isolated image failed because the
container lacked the environment proxy's trusted CA and package indexes were
unavailable. A second isolated image incorporated the environment-provided CA
and installed the package without disabling TLS verification or altering host
security settings. No production dependency or CI gate was changed.

The first baseline attempt has eight recorded zero-exit runs and all required
stdout/side-file hashes independently rechecked in
`h4-instruction-baseline-results/companion-validation.json`. It proves the
specified counts and observed paths were emitted by those recorded executions.
Its captured host shell exit is unavailable and its combined build log stops
early. The subsequently retrieved [Docker event](h4-instruction-baseline-results/docker-events.jsonl)
records exit 0 for the same container, whose direct init child was the runner.
This establishes its container exit; the missing complete outer log remains a
limitation. Candidate or A2 specifications are not measurements until their
executions and artifacts are retained.

## Matched measured results

The baseline, historical A2 and consumption-aware candidate each ran all eight
committed cases under the same image, compiler, flags, dependency, observer and
measurement boundary. The [data comparator](compare_h4_instruction_results.py)
verified each recorded stream and Callgrind artifact hash, the per-run exits,
matching checksums and actual wrapped/fade/glide premises; its
[comparison](h4-instruction-comparison.json) records totals, per-block and
per-sample values. Four negative controls reject an unrelated container exit, altered Callgrind
data and unexecuted wrapped, fade and sustained-glide paths even when the
changed stdout hash is made self-consistent. These data checks are not an attestation of the
compiler or machine. Baseline container exit 0 was recovered independently
from Docker events; its original host-shell exit and complete combined log are
unavailable. A2 and candidate runners each exited zero with complete logs.

| Workload, 48 kHz / 128 samples | Original Ir/block | A2 vs original | Consumption-aware vs original | Candidate minus A2 Ir/block |
| --- | ---: | ---: | ---: | ---: |
| Full wet, 1 band | 55,719 | +1.65% | +1.68% | 16 |
| Full wet, 2 bands | 73,566 | +15.33% | +15.41% | 60 |
| Full wet, 3 bands | 91,631 | +27.41% | +27.53% | 112 |
| Full wet, 4 bands | 106,365 | +36.86% | +37.01% | 160 |
| Full wet, 4 bands, x4 Drive | 260,688 | +15.04% | +15.10% | 161 |
| Partial Mix, 4 bands | 147,393 | +0.009% | +0.27% | 386 |
| Full wet, 4 bands, repeated fade | 129,603 | +47.51% | +47.70% | 249 |
| Full wet, 4 bands, sustained glide | 120,039 | +33.73% | +33.86% | 160 |

The x4 case observed wrapped execution in all 256 measured blocks, the fade
case observed a fade in 160 blocks, and the glide case proved 32,768 gliding
samples. Dividing per-block values by 128 gives instructions per processed
sample; for settled four-band full wet this is about 831 original versus 1,139
candidate. The candidate-minus-A2 column is the difference between complete
implementations, including their mask/branch/code-layout differences; it is not
a separately isolated scan instruction count. No real-time timing, maximum
callback duration, re-entry spike, Match behavior, exceptional recovery cost,
allocation or memory measurement is established by this table. The small scan
increment does not remove the much larger recurring warm-bank cost.

## Preliminary repeated-recovery cost

A separate candidate-only [fresh instruction experiment](h4-recovery-instruction-results-v1/identity.json)
compared steady full wet against injection of one NaN dry integrator before
each of 256 measured blocks, with injection and observers outside the counted
region. The candidate reported finite dry state after each injection, finite
wet output and matching normal/recovery checksums; actual x4 wrapping was
observed in all selected wrapped blocks. At four bands/48 kHz/128, normal and
injected-recovery totals were 145,730.91 versus 145,849.91 instructions/block
without wrapping and 300,053.58 versus 300,172.58 with x4 Drive: **+119
instructions/block** in each measured case. Both runner exits were zero and
raw Callgrind files and identities are retained.

This is repeated fault injection, not a natural event rate, worst-case bound,
CPU time, callback deadline or proof that every recovery path is correct. The
first probe observed the finite resulting state but did not separately record
the dry-bank mask, exact reset state and sanitized output. A stricter follow-up
specification records those execution premises before the result is used as
fully observed candidate recovery cost.

## Observed repeated-recovery cost and object size

A [stricter fresh repeat](h4-recovery-instruction-results-v2/identity.json)
observed the active dry bank in all 256 blocks, the affected integrator reset
in all 256 injected blocks, sanitized dry samples, finite output and the actual
x4 wrapped path in all selected wrapped blocks. Normal/recovery wet checksums
matched. At four bands/48 kHz/128, the measured totals were 145,730.91 versus
145,847.91 instructions/block without wrapping, and 300,053.58 versus
300,170.58 with x4 Drive: **+117 instructions/block** in these repeated
injected-recovery cases. Per-sample figures and all raw Callgrind artifact
hashes are in `comparison.json` and the receipt. The inner runner exited zero;
the disconnected outer client did not retain its shell exit or complete log.
This is not a natural failure rate, exhaustive worst case, wall-clock or
deadline measurement.

The [matched binary DWARF record](h4-multiband-size.json) reports
`sizeof(MultibandWidth) == 1672` bytes for shipping, A2 and this candidate on
the measured GCC/x86-64 build; the added mask did not change the total declared class size
in this build. It is not a portable ABI, peak-memory or allocation guarantee.
The separate candidate allocation test is scoped in the recovery follow-up.
