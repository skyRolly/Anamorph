# N2-06 typed test helper — local development results

PR #244 head `544ff75165b9b89f7b60606a231f3aaba35cc681`, tree
`89c5b76642debae0b10542e0b3a2694f40aba63d`, corrects the previously
recorded MSVC C3779 test-helper failure using explicitly typed friend returns.
The production engine is unchanged from prior PR head `552c6fe5`.

A Linux/Ninja incremental development build, before this commit was created
but with the same production and test bytes, reports 14 focused, 392
Multiband Enable, 720 ordinary-duck and 5628 full DSP checks passing. The
full DSP log ends in `ALL TESTS PASSED`. Documentation and citation checks
passed locally. The enclosing command-session exit status was unavailable
after the execution session changed; the individual outputs are retained and
must not be described as an observed outer exit. This is not a clean-head or
hosted execution. New-head isolated control runs and hosted validation remain
separate. The initial hosted MSVC failure remains in the predecessor record.
