# ADR-0074 — H4 dry-state continuity and guarded warm-bank tradeoff

**Status:** Proposed — owner decision pending. No production H4 correction is authorized here.

## Context

Main `113b9189f0ff4d3b6348c553c559c22ee8ade782`, product 0.10.1, pins JUCE 9.0.5
at `d60b7a78030661574a615b586d9325ba7a7fa7dd`. ADR-0005 requires the existing
phase-matched allpass reconstruction at partial Mix; ADR-0004 owns transition
timing and ADR-0009 recovery. Historical H4 commit `36e69f9b` skips recursive
dry-bank work during qualifying full-wet periods, while maintaining delay rings.

## Problem

The delay rings and synchronized coefficients do not maintain recursive filter
state. N2-01 reproduces a .1361084 peak / -2.5659 dB first-window error at a
20 Hz Mix re-entry. V2-01 shows why Test 25's aggregate noise RMS assertion did
not establish worst-frequency or sample-level phase reconstruction.

Simple always-warm diagnostics remove the ordinary transient, but a later
candidate-only extreme-input experiment retains invalid unused dry state through
1.28 s of valid wet audio, then loses a 512-frame block when Mix consumes it.
Successful ordinary diagnostics alone must not be promoted into production.

## Options

- **A2, guarded existing warm bank:** continuously maintain the existing dry
  filters and aligned history during H4, preserve its exact-wet shortcut and
  clean-dry Measure reference, and recover only failed unused dry-channel state.
- **A/C, unguarded warm state:** useful correctness/cost references, insufficient
  recovery for the demonstrated extreme finite sequence. C discards unused
  zero-latency output but adds interface/branch complexity for <.6% instruction saving.
- **Finite replay:** fixed short retained input/coefficients fail at low cutoffs
  and high rates; general state equivalence and bounded real-time replay are not established.
- **Restricted gate:** safe at one band; affected multiband configurations reduce
  to warm processing. Masking/delaying Mix changes the existing transition contract.
- **Retain a documented limitation or defer:** keep N2-01 open and H4 unchanged;
  historical H4 approval does not accept this newly measured error.

## Decision

**Pending the maintainer.** Recommend the guarded warm-state direction, conditional
on the packet's recovery, unrelated-state, latency, cost and final-source validation.
The maintainer must explicitly accept the additional per-block dry work before
implementation. Guarded A2 measures +15.3/+27.4/+36.9% whole-engine instructions for 2/3/4 bands
at 48 kHz/128; the guard itself adds .13–.27% over the warm-bank reference in
selected OS-off rows. Clock measurements remain variable.
Shared-machine clock data are not universal native CPU/instance-count promises.

This proposal preserves DSP topology/order, latency, parameter/persistence and
threading. A changed filter representation, Mix timing, latency or state/thread
model would require its applicable Architecture Review Gate. This assignment
also explicitly reserves the H4 performance tradeoff to the owner even where
the existing graph can be retained. No decision is marked Accepted by an agent.

## Consequences

An approved correction must maintain aligned-delay history as well as filter
state, preserve H4's clean measurement reference, and avoid clearing valid wet,
other-channel or raw histories during unused-dry recovery. The independent
low-frequency regression must accompany the production correction; it currently
fails main and remains optional nonshipping evidence. Existing Test 25 assertions
stay intact with corrected claim text. N2-04–07 are not silently included.

## Related code

`AnamorphEngine::process`, `fullWetIdle`, `MultibandWidth::processBlock`,
`LR4Xover`, `testDryAlignGateRecomb`; optional experiments and diagnostic patches
under `worklogs/h4-dry-state-0102/`.

## Evidence and confidence

[Decision packet](../../../worklogs/H4_DRY_RECONSTRUCTION_0.10.2.md): reproduced
ordinary transient, actual gate/state observations, independent production
references, candidate-only recovery sequence, cost ledgers and controls.
[Original preserved audit](../../../worklogs/dsp-numerical-audit-0102/README.md).
N2-01/V2-01 are Confirmed P2. Candidate evidence is explicitly distinct from
approval, main integration, native DAW behavior and listening.
