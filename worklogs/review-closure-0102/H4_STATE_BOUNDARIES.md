# H4 candidate recovery: state and consumption boundaries

Source reading at intended integration `3669cf716b27de56dd125f29d0b2672598aebdc2`;
this is a recovery-design aid, not proof that an unimplemented detector is
complete. Shipping H4 remains unchanged. The injected-state observations below
are from `h4-bank-lifetime-results/` at `9e972572`; the relevant
`MultibandWidth` source is byte-identical at both identities. The actual A2
repeated-recovery controls are in `h4-channel-recovery-results/`.

| State or history | Write, consumption and reset boundary | Consequence for a candidate |
| --- | --- | --- |
| `dx[0..bands-2]` in a processing dry bank | Each active split uses four LR4 integrator floats per channel. It is updated for each aligned dry sample; during a crossover fade **both** banks process dry. `copyBankState` copies active states into the other bank before a fade. | A finite reconstructed output alone does not establish finite internal state. Check or track every state that can feed the current/next processed bank before it can affect a consuming block. A post-block check only works if the following consumption and all stored outputs are protected. |
| `dax[1..bands-2]` in a processing dry bank | The reconstruction allpass uses only first-section `s1/s2` per channel. `dax[0]` is not processed; `s3/s4` of the dedicated `dax` instances are not read by the present allpass implementation. | Scanning untouched second-section or index-zero state on every sample is not justified by present consumption. This conclusion depends on the current dedicated-allpass topology; changing its call sites changes the premise. |
| Dormant crossover bank | In settled processing the inactive bank is not processed. Before the next discrete-jump fade, the active bank's complete state overwrites it, then both banks run. During sustained non-fading glides only the active bank runs. | A proven overwrite before any read can avoid scanning the dormant bank. During a fade both are live, and an invalid active state is copied to the destination. The 13-check fault-injection experiment confirms these specific cases; it does not prove which injected states occur naturally. |
| Coefficients, target frequencies, bank/fade metadata | Cutoffs and fade selection are shared with the wet path. On a jump the destination gets copied state and freshly assigned cutoffs; ordinary glides update the live bank per sample. A non-aligning block does not advance dry integrators. | A dry-channel recovery must preserve valid wet coefficients, fade trajectory and opposite-channel state. State validation cannot assume a static cutoff or stop at only the old bank during a fade. Validity of external parameter and coefficient inputs is a separate premise to verify. |
| One band; topology and Multiband enable | One band uses no crossover: aligned dry is a copy of the input. A band-count structural change resets Multiband at the silent duck bottom; a fully disabled bank is reset when it next starts. Host reset/prepare also reset both banks. | One-band H4 has no recursive dry crossover to maintain. A restricted detector can rely on structural resets only when its tested path actually uses those engine transitions; direct module calls need their own premises. |
| Conditioned clean dry and aligned-dry delay rings | Both are preallocated at prepare and share an advancing index. Their read offset is the selected wet latency. Unchanged H4 writes clean dry into both and uses the clean reference while gated; ordinary partial Mix and Match consume the aligned history. | Warm recursive state alone does not populate the required aligned samples for nonzero latency. A candidate must separately establish what valid aligned history is written, what is read at re-entry and recovery, and retain H4's clean gated Measure reference. A finite ring value is not by itself proof of the correct historical signal. |
| Raw-bypass and oversampling histories | Raw bypass has a separate delay ring; oversampler and compensation histories have their own owners and reset rules. Oversampling selection affects latency even when the wrapping path is inactive. | Do not clear these unrelated histories to heal a dry crossover. Test actual wrapped execution and the selected latency; a selection-only test is not evidence that the oversampler ran. |
| Wet and opposite-channel recursive states | Wet banks use separate `x`/`ax` filters; dry and wet share transition metadata. The channels have separate LR4 integrators. | Recovery should preserve valid wet and opposite-channel state. The finite-extreme A2 experiment preserves the opposite dry state in 4/4 observed blocks; a reset-both mutation preserves 0/4. That control does not establish every wet, ring or oversampling preservation case. |

A2 sanitizes non-finite *computed dry samples* before writing the aligned ring
and resets the affected channel's dry filters, but the hidden-state witness
shows this scan can see finite samples while an integrator is already invalid.
Conversely, merely finding all stored floats finite cannot rule out an overflow
on a subsequent operation: the separately traced extreme-finite wet LR4
numerator demonstrates that general arithmetic limitation. Recovery at or just
before Mix re-entry needs a policy for the already-written delayed samples and
for a failure arising inside the first consuming block; neither is proved by a
single post-gated-block scan. Repeated recovery, actual oversampling, re-entry
and preserving valid histories remain required discriminating experiments.
No candidate recovery contract or representative performance claim follows from
this state map alone.

A later engine-level fault injection at exact source `3669cf71` confirms that
an invalid active dry integrator can remain unobserved through the original
full-wet gate and affect the first Mix re-entry block. The source-bound control
also executes x4 oversampling with Drive and six samples of latency; it
separates that dry-only failure from the extreme wet overflow, and restoring
only the injected state restores the reference output. See the detailed
execution and negative controls in `CONTINUITY.md` and
`h4-reentry-injection-results/`. This deliberately injected fault models a
missed hidden state; it is not itself a naturally generated A2 execution.
