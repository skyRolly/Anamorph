# Actual integrated main hosted validation — 2026-11-16

Actual main `230da0e2b0b81422f7e3964eea2833c48266a963`, tree
`a322ae4cab7c4656a3fd2beb465f3e62e31d8f9c`, is the authorized merge of #240
after #237, #238 and #239. Hosted results refer to that exact commit.

- [Build & Validate 43796315012](https://github.com/skyRolly/Anamorph/actions/runs/43796315012): attempt 1 had 15 successful jobs, one Windows failure and three conditional skips. Windows failed at Configure after checkout and pluginval-acquisition self-tests; its subsequent build, tests, pluginval and packaging steps were skipped. The job-log redirect returned Forbidden, so the cause remains unattributed.
- The failed Windows job was rerun on the **same commit** as attempt 2 and passed, including configure, build, product identities, DSP/State, deterministic and randomized pluginval, installer/package and install/reinstall/uninstall checks. The overall run now reports success. Other successful jobs are the results of attempt 1; the retry does not explain or erase the original failure. The three review-selection/merge-only conditional jobs remained skipped.
- The successful applicable jobs include Linux, native Intel macOS and Apple Silicon/Rosetta DSP/State and pluginval; cross-slice checks; Linux ABI floor; packaging; ASan/UBSan with vptr; Valgrind/memcheck; TSan and RTSan with liveness controls; fuzz; LTO; AVX2 comparison; source-lint; documentation; allocation and ordinary-user POSIX fixture gates. Exact successful/skipped step metadata is retained in the compressed job API records. An unrelated conditional upload for absent fuzz findings was skipped.
- [CodeQL 43796314706](https://github.com/skyRolly/Anamorph/actions/runs/43796314706) passed its Actions and C/C++ analyses; [Microsoft C++ Code Analysis 43796314750](https://github.com/skyRolly/Anamorph/actions/runs/43796314750) passed. Raw SARIF and the original Windows attempt-specific log were inaccessible in this environment; do not infer findings or failure causes from unavailable contents.

These are hosted automated checks, not a native DAW, listening, representative
performance, Level-5 or release acceptance. Previous combined-source success
and later new-maintenance PR checks retain their own identities.
