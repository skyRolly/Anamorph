# Nonshipping fallback: live-fade collateral and allocation control

A fresh Linux/Ninja Release replay used actual integrated main
`230da0e2b0b81422f7e3964eea2833c48266a963`, selected diagnostic patch
SHA-256 `40cb9e404b0556adc7793b974755b7b90144d654310dfd2a31e35c40b95c8474`,
expected tree `5b8c733bfe8e16f0c8e52eed096341451e058867`, pinned effective
JUCE `d60b7a78030661574a615b586d9325ba7a7fa7dd` and spec/probe commit
`7cee0343ba65ef87c6e1138f8f73871ecf15fa40`. The receipt records the full
source, dependency, build, compiler-input, executable and result identities.
Outer exit was 0; the three executable exits were 0, 1 and 1 as specified.

Across 12 actual live-fade lanes (2/3/4 bands, OS off or actual x4/six-sample
wrapping, injecting either source or destination bank), two deliberate
invalid-state injections each yielded **24 recoveries**. The positive
diagnostic-policy controls passed **277/277**. In all **24/24** recoveries
the fallback reset a valid recursive state in the other simultaneously live
dry bank of the affected channel. The comparator independently advanced a copy
of that bank's own state with its actual conditioned input, including on the
second recovery after the fault engine had already diverged from the clean twin.
This establishes collateral loss in the candidate; it does not establish
natural reachability of the injected state or quantify the resulting audible
transient. Checked Wet-bank recursive states, opposite-channel dry state/output
and aligned history, and raw/conditioned histories matched the clean twin.

The allocation guard's new, aligned-new and malloc liveness checks passed; it
observed **zero allocations in each of the 24 injected recovery calls**. This
is a selected Linux observation, not a complete exceptional-path RTSan proof
or a deadline guarantee. A false assertion that the other live bank was
preserved failed **24/301**; disabling the conditioned fallback failed
**24/277**. These are expected negative controls. The probe checks selected
state families and fault blocks; it does not prove all banks, histories,
trajectories, glides, measurement behavior or recovery durations.

The fallback remains a nonshipping experiment: finite conditioned samples
are not phase-matched historical reconstruction, later Enable arithmetic can
still overflow, and no owner policy has been accepted. A policy preserving an
independently valid same-channel bank or explicitly justifying its reset, plus
its first-consumption and publication behavior, still needs investigation.
Representative native timing and deadline measurements remain unavailable.
ADR-0074 remains Proposed.
