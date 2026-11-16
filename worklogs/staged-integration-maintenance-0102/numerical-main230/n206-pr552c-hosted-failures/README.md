# N2-06 PR 552c6fe5 hosted failure

[Microsoft C++ Code Analysis run 43802224804](https://github.com/skyRolly/Anamorph/actions/runs/43802224804),
job `133084911303`, failed its Build CMake step for PR #244 head
`552c6fe5e7d4c6f92da0ea200adddec85e1358ae`. The retained attempt-specific
log reports MSVC C3779: the new test's private-member access friends use
deduced `auto` return types before MSVC can see a definition. The follow-on
uninitialized/call errors arise in that test helper. The chosen correction
uses the explicit typed friend-return pattern already present in the existing
Multiband endpoint tests. It changes verification code only, not production
DSP; new-head compilation, tests, mutations and hosted checks are required.

A separate Windows native-evidence smoke job `133084740133` in Build run
`43802171712` failed at `Collect diagnostic smoke (Windows MSVC Ninja)`. That
run was still in progress and GitHub had not made its job log available when
first requested. Its cause remains unassigned until the actual job evidence is
accessible; do not infer that it shares the MSVC analysis failure.
