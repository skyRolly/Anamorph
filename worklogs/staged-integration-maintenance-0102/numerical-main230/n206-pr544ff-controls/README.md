# N2-06 PR 544ff751 isolated mutation controls

Eight serial receipt-controlled relinks use the exact committed test header
bytes from PR #244 head `544ff75165b9b89f7b60606a231f3aaba35cc681`,
actual-main `230da0e2`, fresh baseline receipt `31bf22e2` and control spec
`6deacfb898def41797aa87dc74d53cb9406d80a4`. The test header SHA-256
`a6db11b2e114a8c81dce574e38b7b7f191a54298bc2261a997b96a39e3420e54`
matches the PR head. The proposed production engine is byte-identical to
the prior PR head; these newly compiled probes include the corrected typed
access helper. Candidate and mutations alter only that engine translation
unit against main, so these are not fresh complete-PR builds. The receipts
bind their source, dependency, generated/compiler inputs, linked objects,
probe, binary, commands and results under the documented trust premises.
All outer runners exited 0.

| Control | Executable exit | Checks / failures |
| --- | ---: | ---: |
| Unmodified main | 1 expected | 14 / 3 |
| PR engine | 0 | 14 / 0 |
| Restore settled Mix call sites only | 1 expected | 14 / 3 |
| Restore smoothing Mix call sites only | 1 expected | 14 / 1 |
| Select Wet even for non-finite dry | 1 expected | 14 / 1 |
| Alter interior Mix arithmetic | 1 expected | 14 / 1 |
| PR engine, N2-03 oracle | 0 | 392 / 0 |
| Restore only N2-03 Wet call sites with N2-06 preserved | 1 expected | 392 / 44 |

Expected negative exits are not product-test failures. The settled mutation
also affects later settled blocks of the ramp test. New-head hosted and fresh
full-tree results are recorded separately; none is inferred from this matrix.
