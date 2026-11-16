# N2-06 PR #244 — final-head replay and hosted checks

PR head `544ff75165b9b89f7b60606a231f3aaba35cc681`, source tree
`89c5b76642debae0b10542e0b3a2694f40aba63d`, based on main `230da0e2`,
was rebuilt in a new Linux/Ninja build by the shared verified experiment runner.
The independently checked JUCE pin is `d60b7a78030661574a615b586d9325ba7a7fa7dd`,
tree `f701683c0a1a6735b65d4456c7700f22a522fc5c`; compiler is Debian GCC
14.2.0. Specification commit `694864fcd6490373c465389f0c313abe99ba42ff`
provides `n206-pr544ff-fresh-spec.json`. The complete receipt and each raw
output are retained here. The runner and all five executable invocations
exited 0:

| Selection | Checks | Failures |
| --- | ---: | ---: |
| `--mix-wet-endpoint` | 14 | 0 |
| `--multiband-endpoints` | 392 | 0 |
| `--n2-controls` | 720 | 0 |
| Full DSP | 5,628 | 0 |
| Full State under Xvfb | 16,555 | 0 |

The existing endpoint oracle reported Match-off 908 visible / 366 masked and
Match-on 904 visible / 370 masked historical Wet-path differences. These are
selected regression lanes, not an independent Level-Match algorithm oracle.
The separate serial mutation receipts are at `../n206-pr544ff-controls`.

Hosted validation of this exact head completed successfully: Build & Validate
[43841294238](https://github.com/skyRolly/Anamorph/actions/runs/43841294238)
(16 successful jobs, three conditional skips); CodeQL
[43841300538](https://github.com/skyRolly/Anamorph/actions/runs/43841300538);
Microsoft C++ Code Analysis
[43841300495](https://github.com/skyRolly/Anamorph/actions/runs/43841300495).
PR merge-source Build run
[43841300447](https://github.com/skyRolly/Anamorph/actions/runs/43841300447)
passed its merge check; its duplicate heavy jobs were conditionally skipped
because the exact-head push run supplies them. The independently resolved PR
merge ref was `7322a3cb752da9a0e12838e775548700f03b49ec` at this checkpoint.
The dependency review also passed. Job metadata is retained here.

The predecessor `552c6fe5` MSVC C3779 failure is retained in
`../n206-pr552c-hosted-failures`. Its separate Windows smoke failure had an
inaccessible original log and remains unattributed. Neither is rewritten as a
result of this successful later head. These automated checks are not listening,
DAW, representative-hardware, Level-5 or release acceptance.
