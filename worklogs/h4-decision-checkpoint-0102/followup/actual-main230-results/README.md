# Consumption-aware H4 candidate on actual integrated main

The nonshipping candidate was freshly built and run against actual integrated
main `230da0e2b0b81422f7e3964eea2833c48266a963`, expected diagnostic tree
`17ca85e77e49e4701865287e99d7c402f8981a99`, patch SHA-256
`332c230af9dd82298c79f77db1087d37dc7112ad48ce9c91d15aac72d0927284`,
spec commit `24d2d7f044afd244fc9d4f7805c5b77b25e10fea`, and effective JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd` / tree
`f701683c0a1a6735b65d4456c7700f22a522fc5c`. The JUCE clone and source checkout
were independent from the concurrent N2-04 build. GCC 14.2/Linux/Ninja Release
and the commands, compiler inputs, fresh objects, executables, outputs and
result identities are recorded in `identity.json`. The outer runner exited 0;
its six executable exits were 0, 0, 0, 1, 1 and 0, as specified.

- The scoped consumption/recovery probe passed **48/0**. Its actual state counts
  per processed bank and channel were 0, 4, 10 and 16 for one through four bands.
- With deliberately injected invalid dry state, the 128-sample experiment
  reproduced 128 affected output differences at zero latency (maximum absolute
  difference 0.00921214) and 122 with actual x4/six-sample latency (0.00838126).
  All 128 newly written affected aligned-history slots differed from the clean
  reference in each case. The selected valid opposite-channel and raw/conditioned
  states were preserved by the scoped controls; injection does not establish
  natural reachability.
- The independent one-sample history trace passed **28/0**. A false claim that
  finite history equaled the clean reference failed **2/30**, and injected
  invalid history failed **8/28**, as expected. The affected history differed
  in 32/32 writes and output in 32/32 and 26/32 blocks for zero and x4 latency.
- The full diagnostic-source DSP suite passed **5614/0**. The separate
  `verify_oracle.py` on the **summary matrix** `run-1.stdout` passed **8/8**
  low-frequency assertions at 20 and 50 Hz. Its script SHA-256 is
  `3ed0426619edbb88f40bb18208426a462b4b24deb5b7fc5b040e4075cf5592b3`;
  the matrix SHA-256 is `43bfdc487c1d8447b340f7a42e8be6c4dc8538609b2e0f472333bbd5df5315e3`;
  `oracle-valid.json` is `1b26fc8a49582dceffa2ac1afcb191737e7b6b52ac78e75b23cbff2d1a1806cd`.

The newly generated 43 MB sample CSV has SHA-256
`b6ab7ff13db2b7fe59d6dd025a1bea311c1f52c866bc8b27c6b045498d5ea4f0`.
It is byte-identical to the uncompressed bytes of the already committed
`../provisional-70bb-results/h4-samples.csv.gz`; that existing compressed file
retains the raw bytes without storing a duplicate. The receipt independently
records this execution's CSV identity. The matrix and oracle hashes also match
that earlier execution, but this fresh actual-main run has its own receipt.

The first companion-oracle invocation mistakenly supplied the per-sample CSV
instead of the summary matrix and failed with `KeyError: gate_clean_blocks`;
`oracle-wrong-input.*` retains that verification error. The corrected command
used `run-1.stdout` and produced the 8/8 result above. The runner itself had
already passed; neither error nor rerun altered historical evidence.

These results do not establish an accepted affected-channel exceptional-output
policy, complete fade/recovery coverage, exceptional-path allocation behavior,
representative native timing or deadline safety. The separate conditioned-dry
fallback remains a scoped diagnostic and can reset both dry banks of its
faulted channel. ADR-0074 remains Proposed; no production H4 correction is
included in main.
