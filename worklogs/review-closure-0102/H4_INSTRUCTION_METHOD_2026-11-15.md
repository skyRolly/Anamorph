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
Its captured outer runner/container exit is unavailable and its combined build
log stops early; do not report that outer status as a pass. Any comparison that
uses it must retain this limitation or repeat the baseline with a captured
outer exit. Candidate or A2 specifications are not measurements until their
executions and artifacts are retained.
