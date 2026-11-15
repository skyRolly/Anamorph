# H4 recovery candidate checkpoint — 2026-11-15 UTC

This is nonshipping investigation. ADR-0074 remains Proposed, and the H4 gate
in shipping source is unchanged. A candidate has **not** yet established the
recovery contract needed for selection. The current recommendation is a
bounded follow-up on first-consumption/history correctness, followed by matched
cost measurements only if that contract is established. The owner may instead
retain the documented H4 limitation or defer the correction.

## Observed behavior and independent findings

N2-01 is the ordinary low-frequency re-entry transient from frozen recursive dry
state; V2-01 is the limited aggregate-RMS test coverage. Output-only candidate
A2 can finish a gated block with a non-finite recursive state despite finite dry
and wet outputs. This is distinct from N2-08: the separate extreme-finite zero
block was traced to wet LR4 numerator overflow and existing final-output
recovery. It is also distinct from an injected already-invalid dry state that
was actually consumed on re-entry (128 zero-latency samples and 122 with actual
x4 wrapping and six samples of latency). The latter demonstrates a consumption
boundary, not natural reachability or execution of A2's detector.

## Contract and state lifetimes to establish

For the existing topology, per processed bank and channel there are 0, 4, 10,
or 16 recursively consumed dry floats for 1, 2, 3, or 4 bands: four per active
`dx` split and two in each dedicated `dax` allpass after index zero. The
allpass's unused second section and `dax[0]` are not read by the present
implementation. A settled dormant bank is copied over from the active bank
before it becomes a fade destination; both banks run during a fade, including
the source for the part of a block in which it retires. Structural band-count
changes, re-enable and prepare/reset have separate reset boundaries.

A viable candidate must maintain valid recursive and aligned-delay history in
normal operation; protect what a bank copy or either fade bank can consume;
preserve the exact-wet shortcut and clean gated Measure reference; and prevent
invalid internal state or invalid samples from poisoning the first consuming
block or later delayed reads. An owner must understand the output policy when
recovery is necessary. Finite output does not prove finite state; finite state
does not prove the next arithmetic operation cannot overflow; a finite
previously written history sample does not prove its value was correct.
Recovery must be bounded and preserve valid wet, opposite-channel, raw-dry,
conditioned-dry and unrelated state. The exact permitted affected-channel
exceptional output and any contract change need explicit owner review.

## Consumption-aware experimental scan

A new diagnostic patch maintains the existing dry bank, checks only the bank(s)
actually processed in the last aligned block and the currently consumed dry
integrator states, sanitizes non-finite computed dry samples before aligned
history or Mix sees them, and resets dry state in the affected channel. It is
based on `003c9a1a2558da6bd89e2624421a6e205eb8acdf`, expected diagnostic tree
`2cc679a22948e145f044a922b274cf0af314b55e`, patch SHA-256
`820728b46738e6fae6179134cf0f355ccf2019202a1f1c1fdd69c88a0bf82777` and JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd`. The fresh build's complete input,
commands, object and executable identities are in [the receipt](results/identity.json);
its [specification](h4-consumption-spec.json) references the committed original
experiment packet. These copies are for review and do not alter historical data.

The first scoped run reports **40 checks, zero failures**. Actual module tests
verify the 0/4/10/16 counts, unused and dormant exclusions, both fade banks and
a retiring fade source. Actual engine injection covers first Mix consumption at
zero latency and actual x4 Drive wrapping with six samples of latency. In those
cases both-channel conditioned and raw history, opposite-channel aligned
history, seven previously unwritten affected-channel aligned slots, the checked
opposite wet state and output remain unchanged; output is finite and the
affected dry state is reset. The natural extreme gated witness retains finite
monitored dry and wet output and reports zero invalid left dry `dx` states after
the scan; its subsequent 128-zero block remains N2-08's separate wet case.
These observations neither prove all finite computed dry samples are correct nor
prove all recovery, automation, glide, Match, history, reset or wet-state cases.
The unmodified A2 output guard is known to miss the hidden state.

The smallest next discriminating step is an independent reference for the
first consuming block and every aligned sample written around an invalid-state
or overflowing-arithmetic event, including a finite-output/invalid-state case,
actual fading banks and nonzero latency. It must establish the policy for
already-written history and distinguish a finite but incorrect sample from a
valid one. Then extend repeated recovery, glides, Match, enable and reset cases
and measure the best candidate. A post-block scan alone is not proof of that
contract. A deliberately active-bank-only detector is specified separately as
a fade-boundary negative control; its result must be recorded before calling it
executed.

## Alternatives and cost limits

Shipping H4 has no added cost but retains N2-01. Unguarded warming is a useful
ordinary numerical reference but does not establish exceptional recovery. A2's
output guard misses hidden state. A full stored-state scan catches the recorded
witness but scans states whose current topology does not consume them and has
no established full contract or matched cost. The new consumption-aware scan
reduces the inspected state set, but neither its complete correctness nor its
cost has been established. State-at-write tracking or a combined boundary and
output strategy could reduce the window at a different per-sample cost. A new
filter representation, delay/latency behavior or Mix timing requires its own
architecture/contract review.

The historical A2 whole-engine instruction increases at 48 kHz/128 are about
15.3%, 27.4%, and 36.9% for two, three and four bands; they are not
measurements of this candidate, CPU percentages, deadline guarantees or plugin
instance capacity. Prior short finite-history replay failed relevant low
frequency/high sample-rate cases; reduced-output processing saved under 0.6%
whole-engine instructions in the measured cells; a different allpass transfer
function does not alone establish equivalent floating-point state trajectories.
No representative native hardware measurement is available here. The
[previously committed benchmark packet](../H4_DRY_RECONSTRUCTION_0.10.2.md)
sets out matched instruction and timing workloads; final owner execution must
also inventory and hash every Callgrind side file, check actual fade/glide and
wrapped execution, and report hardware, build, percentile, maximum and recovery
conditions. A shared virtual host is not native real-time acceptance.

Admissible owner outcomes are a supported candidate after its missing evidence,
a separately reviewed contract/architecture alternative, targeted follow-up,
retention of the documented limitation, or deferral. None of this accepts
ADR-0074 or authorizes a production H4 change.
