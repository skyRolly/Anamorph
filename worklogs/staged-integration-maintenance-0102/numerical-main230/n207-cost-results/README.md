# N2-07 matched Drive instruction comparison

On virtualized x86-64 Linux, actual-main `230da0e2` and the N2-07 engine-only
candidate diagnostic tree `51e6aa86679c9368433fdb172915bfa788a8de90` were
serially receipt-relinked from the same verified build (`31bf22e2`), pinned
JUCE, GCC 14.2.0 and specification `0f2429836f9fe62c03dfcada675acccd4a0128a5`.
Their source-bound probe and normal executions are in the committed specs and
receipts. The candidate has the same executable changes as PR #245; comments and
a citation differ. It is not an exact-PR build.

The same binaries were then run, serially, in Docker image
`sha256:58079116ef549d981c45d09860e24873d06eca2955f2e2795032ef7eb0177ac4`
with Valgrind 3.24.0 Callgrind, `--collect-atstart=no`, one CPU, 48 kHz,
128 samples, Drive 6 dB, Mix 1, static one/four bands and oversampling off/x4.
Each run warmed 128 blocks and counted 256 actual engine blocks; setup, input
generation and observers are outside the counted region. `wrapped_blocks`
confirmed zero or 256 actual wrapped calls as selected. All eight external
commands and the enclosing container exited 0. The command pattern was:

```
valgrind --tool=callgrind --collect-atstart=no --callgrind-out-file=<artifact> \
  <receipt-bound-drive-cost> 48000 128 <bands> 0 <0-or-1-x4> 1
```

| Bands / wrapping | Main instructions | Candidate instructions | Increase | Additional instructions / block |
| --- | ---: | ---: | ---: | ---: |
| 1 / off | 16,614,429 | 16,876,573 | 1.578% | 1,024.00 |
| 4 / off | 29,579,779 | 29,841,893 | 0.886% | 1,023.88 |
| 1 / x4 | 53,771,085 | 54,819,631 | 1.950% | 4,095.88 |
| 4 / x4 | 66,736,019 | 67,784,685 | 1.571% | 4,096.35 |

Callgrind side files are external to the shared runner: they are retained here
with stdout/stderr, and `validate.py` checks the workload markers, count
summaries, receipt-bound binary hashes before/after, and hashes all retained
artifacts into `comparison.json`. The displayed checksums are coarse workload
observations, not a bit-exact correctness oracle. This is a bounded instruction
comparison, not CPU percentage, wall-clock distribution, native callback
headroom, DAW instance count or a universal performance guarantee. It does not
measure a new H4 candidate.
