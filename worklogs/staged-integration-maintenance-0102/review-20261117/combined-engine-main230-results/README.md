# Combined production engine: preliminary focused controls

A serial receipt-controlled Linux/Ninja relink used baseline main
`230da0e2b0b81422f7e3964eea2833c48266a963`, receipt `31bf22e2`, pinned JUCE,
and spec/test packet `7807fe9dcfb682e3f26b5554d6b346006ceb56cc`. Its
engine-only patch produces the exact `AnamorphEngine.cpp` bytes in combined
rehearsal `b182cc0a6d68b105a0226cfcffd9fe6cfe767003`; all other `src/` bytes
also match between main 230 and that rehearsal. The selected copied test
headers match b182, including the latest wrapped Mix test. The outer runner and
all five executables exited 0, with validated output markers:

| Selector | Checks | Failures |
| --- | ---: | ---: |
| N2-04 conditioned history | 125 | 0 |
| N2-06 Mix endpoint | 16 | 0 |
| N2-07 Drive endpoint | 20 | 0 |
| N2-03 Enable endpoints | 392 | 0 |
| N2-02 engine controls | 720 | 0 |

This is an independently receipted combined-*engine* diagnostic against
verified main-230 build objects and copied current test headers. Its Git source
tree, executable and source identity are not the b182 full build. Full DSP,
State, isolated mutations, hosted exact-source validation and actual main
integration remain separate. The preceding 0ea stale-test attempt is retained
in `../stale-head-attempt/`.
