# Provisional N2-04 conditioned-history experiment

This is nonshipping development against reviewed intended integration source
`70bbdc22acdfd6286d13b1a15cb445367525c5e5`, not a result on actual final main.
`candidate-source.patch` and `candidate-test.h` preserve the prototype bytes;
the patch changes the existing self-heal's conditioned ring clear into a
bounded, exceptional-path scan that zeros non-finite slots and retains finite
bits. It keeps the aligned dry and OS compensation clears and all other reset
ownership unchanged. The test header is a separately included new input, not
part of the patch. `source-sha256.txt` identifies the candidate/test files.
The original audit, the current H4 limitation, and N2-05 remain independent.

A GCC 14/JUCE 9.0.5 local container build used `/workspace/n204-provisional-build`
with `AnamorphTests --n204-conditioned-history`. Its corrected control returned
**59 checks, 0 failures** (exit 0). Selected 2x/4x/8x Haas/Drive-off cases
observe the base-rate latency stand-in, not actual wrapped processing. They
trigger the output guard with a finite left/right spike, check the unaffected
channel in the recovery block and next delay-history block, and arm the live
allocation guard. Separate direct ring injections and actual non-finite input
exercise poison sanitation and finite opposite-channel history. Exact signed zero
is checked in the injected ring control.

Restoring the original conditioned-ring clear returned **59 checks, 12
failures** (exit 1); changing only that clear to unconditional retention returned
**59 checks, 6 failures** (exit 1), detecting both injected and actual invalid
history. These are expected negative controls. The initial expanded test returned
**56 checks, 6 failures** (exit 1) because it incorrectly expected a final-sample
input poison to reach the output guard in the same block despite selected-factor
latency. The corrected test first observes the ring write and then the delayed
next-block guard. An earlier smaller positive control passed 33/0. All streams
are retained. These provisional builds were ordinary local development builds,
not provenance-receipt controlled executions; full normal/exceptional matrix,
actual wrapped recovery, final-main integration, documentation and CI remain
required before any production PR is ready for owner review.
