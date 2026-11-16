# N2-05 flagged Mix-zero diagnostic: injected boundary

This nonshipping engine-only diagnostic against actual main `230da0e2` records
a non-finite `wet - dry` at exact Mix=0 when Dry is finite, selects Dry for
that sample and carries the indication to the existing final reset. Other
coefficients and invalid Dry retain the historical arithmetic. It detects an
invalid difference, which may result from invalid Wet or finite subtraction
overflow; it is not a proof of which earlier stage caused it. Patch SHA-256
`408be66b0a37d7624826a2b197751489e6da447361d6f4ab8c2e0ab632c31a17`,
diagnostic tree `0d4468ccc454613ed2b80ffa425fc3353fc9a44e`, spec
`61a340b957ce101147be9a3ef846b8a854f5e860`. The receipt-controlled runner and
executable exited 0. The spec asserts execution markers; the numerical
comparison below is a separately inspected observation, not an automatic
complete correctness oracle.

For the same deliberately injected invalid Wet LR4 state as the baseline and
naive controls in `../n205-boundary-results`, the diagnostic preserved
128/128 selected first-block Dry samples, left no invalid monitored Wet state
after that block, preserved 128/128 next-block Dry samples, and had 0/128 zeros
on Mix re-entry. The clean twin also had zero re-entry zeros. Unmodified main
lost the first block then reset; the naive endpoint-only selection preserved
that block but lost the re-entry block before resetting. Injected state does
not establish natural reachability.

This is a bounded zero-latency/two-band diagnostic, not a complete production
fix or accepted recovery policy. It does not establish behavior for wrapped
latency, all state families, Match, channel-local preservation, repeated faults,
reset/re-prepare, all finite-difference overflows or other endpoints. The
additional detection boundary requires assessment under the signal-flow
Architecture Review Gate and ADR-0009 before shipping. A separate [replay of the original finite-input witness](../n205-flagged-natural-results/README.md)
returned 128/128 exact Dry samples on the selected extreme input, versus 0/128
on main; it did not directly inspect the prior invalid stage or the flag. No production or H4 correction is included.

The later [actual x4 interaction](../n205-x4-results/README.md) preserved the
selected fault block with this diagnostic but retained only 116/128 exact Dry
samples in the next block because the existing conditioned history was cleared.
Adding the independent #243 engine correction preserved 128/128 in that
selected next block. This is diagnostic evidence of an interaction, not a
merged correction or a complete recovery contract.
