# Preserved first post-Enable trace attempt

Source `70bbdc22acdfd6286d13b1a15cb445367525c5e5`, diagnostic tree
`c5223d6cfb52c7f0df74491290f0ff91b24ef933`, spec/probe commit `5749d5de`.
Fresh build and diagnostic trace ran, but the runner exited **1** because the
probe reported `premise_failures=2` and the required success marker was absent;
the runner correctly did not create a completed identity receipt. The probe
incorrectly required exactly 40 original intermediate overflows even for the
widened control. The untouched comparison observed 40; the control observed
70, because removing resets changed later state and therefore opportunities for
subsequent overflow. This is a verification-premise failure, not a passed run.

The retained output is a useful diagnostic hypothesis: with the untouched
candidate, finite opposite-sign operands produced 40 non-finite Enable results,
40 aligned writes/reads, 40 invalid Mix outputs and four self-heals in each
selected OS case. In the exceptional widening control, 70 Enable overflows
were replaced before history writes, with zero non-finite history writes/reads,
but seven later Mix failures and one heal still occurred. A corrected probe and
fresh receipt must verify this result and trace those later operands. The first
attempt's exact stdout, stderr, compressed patch/build log and observed hashes
are preserved; they are not represented as a complete provenance receipt.
