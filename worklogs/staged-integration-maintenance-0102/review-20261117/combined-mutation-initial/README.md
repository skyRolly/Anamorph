# Combined mutation attempt: downstream-oracle dependency

The first two serial receipt-controlled relinks completed on source
`b182cc0a6d68b105a0226cfcffd9fe6cfe767003` with the final test-header bytes.
Restoring the N2-04 unconditional clear failed 28/125 N2-04 checks; unsafe
retention failed 6/125. In each case N2-03 passed 392/392 and N2-06 passed
16/16. Their complete receipts, input patches, streams and runner logs are
retained here.

The third relink restored only the settled N2-06 Mix expression. N2-06 failed
3/16 as intended, but the runner had incorrectly required the complete N2-03
selector to pass. It instead reported 24/392 failures because its production
output oracle explicitly expects corrected Mix=1 behavior. The runner correctly
rejected its missing expected positive marker and exited 1; its partial receipt
does not attest completion of the rejected run or its omitted third selector.
The captured N2-03 stream shows all 48 lanes, the 24 downstream output
failures, zero kernel/aligned-dry/reference/interior mismatches and passing
Mix=0.4 output. An independent [dependency validator](../combined-mutations-final-tests/validate_n203_dependency.py)
accepted the settled classification and rejected the wrong interior
classification. Subsequent specifications require those exact negative results
and the separate lane validator; they do not relabel the selector failure as a
pass. This cross-test dependency is not a change to the N2-03 production
kernel. The original attempt and correction are preserved for review.
