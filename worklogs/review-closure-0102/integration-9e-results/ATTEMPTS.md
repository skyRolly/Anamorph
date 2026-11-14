# Combined-source validation attempts

Source: `9e9725726b31c39b8de0379c12c324b2e858e503`. The local Python
source/documentation/fixture batch records 35 successful commands in
`local-checks-results.json` (raw logs and auxiliary reports are preserved in `local-checks.tar.gz`); the separate JUCE cache test reports 28 passing
cases in the pinned-toolchain container. The retained command-record script is
a historical invocation with machine-specific paths, not a maintained replay
entry point. These checks do not replace hosted platform or sanitizer jobs.

The first guarded full-suite attempt refused the original integration checkout
because ignored `scripts/__pycache__/` and `tests/__pycache__/` existed there.
It exited 1 before configuring. No files were removed to make it pass; the retry
uses a separate clean worktree at the identical commit. The initial failure log
is retained as `review-closure-integration-9e.log`.

Two extra fresh-build attempts (the H4 channel-preservation trace and final
isolated mutation run) were deliberately stopped with exit 137 while their
JUCE configuration overlapped the full build and the container approached its
8 GiB memory cap. Their incomplete logs are retained as `*-stopped.log.gz`;
they are not test outcomes. Retries use distinct new output directories and
limited build parallelism. An initial bounded H4 retry was also stopped with exit 137 while the large State translation unit was compiling and memory approached the cap; its log is retained. Run further fresh builds only after the integration build completes. No partial build or object is accepted as evidence.


The clean-worktree retry completed with outer exit 0. The exact source tree
`f4d268c17cd6619732f4f998928bcda7ddafc462` and JUCE tree
`f701683c0a1a6735b65d4456c7700f22a522fc5c` were verified before and after
the build and each execution. The focused N2-02 engine, N2-03 endpoint, full
DSP, focused N2-02 processor and full State/isolation runs respectively report
720/0, 392/0, 5614/0, 20/0 and 16555/0; all five executable exits are 0.
The State isolation control also reports success. `fresh-full/identity.json`
records the commands, compiler and linked-object and binary hashes; compressed
raw outputs and the build log preserve their original bytes. This is local
Linux validation, not the exact-head hosted cross-platform or sanitizer matrix.
