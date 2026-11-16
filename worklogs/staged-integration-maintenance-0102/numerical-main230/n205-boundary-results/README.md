# N2-05: injected wet-state boundary at Mix=0

Actual integrated main `230da0e2`, pinned JUCE and the shared baseline receipt
`31bf22e2` were used in two serial Linux/Ninja relinks under specification
`a402ce240c6ee8fcbf9e054ea7c7a12b16448ed0`. The naive diagnostic changes only
both Mix call sites to select Dry directly at exact Mix=0; patch SHA-256
`278e25651c06465e20ad49d11e6a8b907a32eb1f1ed455c3cb0dfff67e11009f`,
expected tree `267343897cebb85cae34f33998617ca966f0208b`. Both outer runners
and executable invocations exited 0. The probe, patch, commands, complete
receipts and outputs are retained; the diagnostic is not shipping source.

The actual engine was warmed at two bands and Mix=0 with valid `.25` input;
then one active Wet LR4 integrator state was deliberately set to NaN. A clean
twin received no injection. The following observations cover 64 samples on
both channels in each block:

| Observation | Unmodified main | Naive Mix=0 selection |
| --- | ---: | ---: |
| Fault-block exact Dry samples | 0/128 | 128/128 |
| Fault-block zero samples | 128/128 | 0/128 |
| Wet state still invalid after fault block | no | yes |
| Wet state still invalid after next Mix=0 block | no | yes |
| First Mix-reentry block zeros | 0/128 | 128/128 |
| Clean twin zeros in reentry block | 0/128 | 0/128 |
| Wet state invalid after reentry | no | no |

The Mix smoother was observed advancing on re-entry in both runs. The naive
shortcut hides the poisoned wet path from the existing final guard while Mix is
zero, then the transition exposes the poison and triggers the existing reset.
This establishes the tested consumption and recovery boundary; it does not
prove natural reachability of the injected state, all wet-state mechanisms,
or the correctness of any replacement policy. The original separate finite
extreme-input N2-05 witness on actual main remains in `../results`.

The first baseline attempt was rejected before compilation because this
investigation's local Python import wrote an ignored `__pycache__` file into the
verified checkout. Its log is retained as `ignored-cache-rejected.log`; the
specific generated cache was removed, and both valid runs used fresh output
directories. The guard was not weakened. A production design that adds or moves
a non-finite detector must be assessed against the signal-flow Architecture
Review Gate and ADR-0009; no such change is made here.

Alternatives for subsequent work are: (1) retain/defer the current behavior;
(2) carry an explicit invalid-Wet indication from the Mix boundary to the
existing final reset while selecting valid Dry, which changes detection scope
and needs the Architecture Review Gate assessment plus tests of measurement,
other channels and later blocks; or (3) skip Wet computation at Mix=0, which
would also change Wet state continuity and measurement behavior and therefore
is not justified by this witness. The naive selection alone is rejected by the
experiment above. None is selected as an approved production recovery contract.
