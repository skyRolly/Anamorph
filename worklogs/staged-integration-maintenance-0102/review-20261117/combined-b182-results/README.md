# Combined b182 fresh Linux execution and hosted warning failure

Fresh Linux/Ninja Release execution used combined commit
`b182cc0a6d68b105a0226cfcffd9fe6cfe767003` (tree
`d91c4b01c8fda95d11f09cdc4b6627d16a7a84b5`), pinned JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd`, and spec commit `7807fe9d`.
The seven recorded executable invocations all exited 0 and met their declared
markers. A companion check rehashed every captured stdout/stderr and checked
all recorded exits and markers. The environment restarted before the enclosing
command-session exit could be observed; that outer exit is not asserted.

| Selection | Checks | Failures |
| --- | ---: | ---: |
| N2-04 | 125 | 0 |
| N2-06 | 16 | 0 |
| N2-07 | 20 | 0 |
| N2-03 | 392 | 0 |
| N2-02 engine | 720 | 0 |
| Full DSP | 5,775 | 0 |
| Full State | 16,555 | 0 |

DSP executable SHA-256 `5220ddd4e22dbbeef235db550f9b59581c9fd586c9fd647028766aff594d4dcc`;
State executable `2efc127b5657c7da5f30a3f3b9e9da367e76455d41175ace4bcccc0402f9b692`.
The complete receipt and streams are retained here. This is historical b182,
not the later warning-corrected combined source or actual merged main.

Hosted b182 [Build & Validate 44006909225](https://github.com/skyRolly/Anamorph/actions/runs/44006909225)
failed its Linux Clang and GCC LTO warning gates: both identified one new
`-Wshadow` in the added `n2_mix_endpoint_tests.h`. Its separately dispatched
[CodeQL 44006955845](https://github.com/skyRolly/Anamorph/actions/runs/44006955845)
and [Microsoft analysis 44006958703](https://github.com/skyRolly/Anamorph/actions/runs/44006958703)
passed. PR #244's exact e782 push run also failed those two warning-gate steps;
the original per-job log downloads returned Forbidden, but the retained
GitHub check annotations identify the warning. The local GCC compile log
independently shows the inner `dry` variable shadowing the earlier dry ring.
PR head `3775c392` renames that local variable; it changes no production code,
assertion, formula or warning baseline. Its final hosted validation and the
new combined source must be checked independently. Failed attempts are not
relabeled as passes.
