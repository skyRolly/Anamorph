# N2-05 flagged diagnostic: original finite-input witness

The unmodified archived `probe.cpp` from actual main `230da0e2` was relinked
against the nonshipping flagged engine diagnostic, using pinned JUCE, baseline
receipt `31bf22e2`, diagnostic tree `0d4468ccc454613ed2b80ffa425fc3353fc9a44e`
and committed spec `f235c251078b2424454c10cb01cbe41a5691a91c`. The outer
runner and executable exited 0. Its specification validates execution markers,
not the numerical result; the retained output was independently compared with
`../results/run-0.stdout` from the original actual-main execution.

With the original finite `2e38` Mix=0 input, main returned 0/128 exact Dry
samples and 128/128 zeros; the diagnostic returned 128/128 exact Dry samples
and 0/128 zeros. Both returned the selected `.25` sample on the next block.
For the selected `.25` and `1e20` Mix=0 inputs, both returned 128/128 exact
Dry samples and no zeros. This confirms the output observation under that
reachable input; the probe did not directly inspect which prior stage became
invalid or observe the diagnostic flag. It is separate from the deliberately
injected Wet-state boundary in `../n205-flagged-results`.

The candidate remains nonshipping and lacks the broader recovery, latency,
Match, repeated-fault and architecture review evidence listed there. This
result is neither native DAW acceptance nor proof that the complete problem
is fixed.
