# Conditioned-dry fault-block fallback — scoped engine experiment

This is a **nonshipping diagnostic policy**, not an approved H4 recovery
contract. A fresh source/dependency-checked Linux/Ninja build used reviewed
integration `70bbdc22acdfd6286d13b1a15cb445367525c5e5`, diagnostic tree
`f99091d9ad6c59dbc6f9c4bf7731178909e932a5`, JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd` and spec commit
`6fa1106cf5a96c760482f4d84c5a35c9f4522a0c`. The committed receipt records
the patch, compiler/configuration, fresh objects, probe, executable, commands
and output hashes. The outer runner exited 0; its four executable exits were
0, 1, 1 and 1 as specified. The test-only global switch is not a production
design or performance measurement. The injected invalid recursive state tests
a consumption boundary and does not establish natural reachability.

When the existing consumption-aware diagnostic detects invalid dry output or
consumed recursive state, this variant substitutes finite conditioned dry
(and zero for a non-finite conditioned sample) for the affected channel's
entire fault block before the aligned-history write, then resets that channel's
dry state. A separate switch preserves the prior sanitization-only candidate
as a negative control. This block policy may alter partial-Mix phase and does
not reconstruct the missing allpass trajectory. It does not address every
post-Enable overflow or establish N2-09's correction.

The positive engine oracle passed **30/30** checks. Actual full-wet reference
gating, first re-entry, ring write/read positions, the wet-twin Mix calculation,
the affected dry-state reset, valid opposite-channel output and aligned ring,
and the checked raw and conditioned histories were observed at zero latency
and actual x4 wrapping with six samples latency. The substituted sample was
exactly the finite conditioned input in each fault block. All **32/32** newly
written affected history samples still differed from the clean twin; affected
output differed in **32/32** and **26/32** one-sample consuming blocks, with
first difference at offsets 0 and 6 respectively. No magnitude, perceptual,
long-tail or natural-event incidence claim follows from these counts.

Disabling the fallback failed **2/30** policy assertions; falsely demanding
clean-reference history failed **2/32**; injecting invalid published history
failed **8/30**. The latter is a probe-side negative control, not a naturally
produced candidate failure. These controls show sensitivity to the stated
boundaries, not a complete fade/glide/repeated-recovery/Match matrix or a
proof of all unrelated state. No exceptional-path allocation, representative
native timing, deadline safety or listening test was performed here.

The next decision remains whether the owner would accept a specified affected-
channel phase/history deviation at all, after the engineering oracle covers
all relevant publication and recovery boundaries and cost. Alternatively
retain or defer the documented H4 limitation. ADR-0074 stays Proposed.
