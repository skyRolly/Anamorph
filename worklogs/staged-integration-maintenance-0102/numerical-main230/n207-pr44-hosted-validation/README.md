# N2-07 PR #245: exact-head hosted validation

PR head `44ef025aab46e09a8be7069184ce79071b7d5a09`, tree
`a64329c617040f694dd5e540ae887d17055bb603`, is based on main `230da0e2`.
The independently resolved GitHub merge ref at this checkpoint was
`7f5be00f2935d0936eaecc01fcf01a2d05513f5a`, parents main and this head;
its tree is exactly the PR head tree. All results below are for the current
head or that merge source:

- [Build & Validate 43847440499](https://github.com/skyRolly/Anamorph/actions/runs/43847440499): success, 16 successful jobs and three conditional skips. Applicable jobs include Linux, Windows, native Intel macOS, Apple Silicon/Rosetta, pluginval, packaging and ABI, ASan/UBSan/vptr, Valgrind, TSan and RTSan liveness, fuzz, allocation, LTO, documentation, provenance and source lints, and POSIX cleanup controls.
- [PR merge-source Build 43847447816](https://github.com/skyRolly/Anamorph/actions/runs/43847447816): merge check passed; 16 duplicate heavy jobs conditionally skipped because the exact-head push run supplies them. The independently checked merge and head trees are identical.
- [CodeQL 43847447814](https://github.com/skyRolly/Anamorph/actions/runs/43847447814): Actions and C/C++ analyses passed.
- [Microsoft C++ Code Analysis 43847447818](https://github.com/skyRolly/Anamorph/actions/runs/43847447818): passed. Dependency review `43847447828` also passed.

Complete job/step metadata is retained in compressed API records. The earlier
`f78b5320` source-lint self-test failure and inaccessible job log remain in
`../n207-f78-hosted-failure`; they are not recast as current-head success. A
fresh Linux full DSP/State replay at that predecessor has its own identity in
`../n207-pr-f78-fresh-results`; its source and test bytes match this head but
it is not described as an exact-head replay. Test-header mutation receipts and
the separate virtualized instruction comparison are in
`../n207-pr-header-controls` and `../n207-cost-results`.

These automated checks do not establish listening, native DAW, representative
hardware timing, Level-5 or release acceptance. The PR is a new independent
maintenance change and still needs owner review and separate merge authorization.
