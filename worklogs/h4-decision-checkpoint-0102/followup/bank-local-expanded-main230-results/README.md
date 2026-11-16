# Bank-local H4 candidate: first consumption and low-frequency controls

Fresh Linux/Ninja execution used actual main
`230da0e2b0b81422f7e3964eea2833c48266a963`, diagnostic tree
`397000f323940a7fae0336561d672a860edcbe8c`, patch SHA-256
`fd0d2623db8b133576662041d67ed67ebaebdc7ce794605566adb1dee997785d`,
pinned JUCE `d60b7a78030661574a615b586d9325ba7a7fa7dd` and specification
commit `30f59e988dddfc69352850a9dbc4d611d825252d`. The receipt records all
seven executions, expected exits, declared markers and the required CSV. The
outer command-session exit was **not observed** after the execution session
changed; the last receipt contains all seven completed results, and a separate
companion validator rehashed each stdout/stderr and the required CSV, checked
all expected exits and markers, and verified the independent oracle result.
This is stronger than an exit-only report but does not invent an observed
outer status.

- Existing consumed-state and boundary probe: **48/48** passed.
- Conditioned-fallback first-consumption/history probe: **30/30** passed;
  false-clean, invalid-history and no-fallback controls failed **2/32**,
  **8/30** and **2/30** respectively, as expected. At zero/x4-six-sample
  latency the affected history still differed in 32/32 selected writes,
  and output differed in 32/32 and 26/32 selected samples.
- Candidate full DSP: **5614/0**. Independent `verify_oracle.py` on the
  summary matrix `run-5.stdout` passed **8/8** low-frequency assertions.
  The companion oracle command exited 0.

The new 43,225,520-byte sample CSV SHA-256 is
`b6ab7ff13db2b7fe59d6dd025a1bea311c1f52c866bc8b27c6b045498d5ea4f0`,
byte-identical to the uncompressed CSV already retained as
`../provisional-70bb-results/h4-samples.csv.gz`; it is not duplicated here.
The matrix SHA-256 is `43bfdc487c1d8447b340f7a42e8be6c4dc8538609b2e0f472333bbd5df5315e3`;
the oracle script is `3ed0426619edbb88f40bb18208426a462b4b24deb5b7fc5b040e4075cf5592b3`,
and oracle result is `1b26fc8a49582dceffa2ac1afcb191737e7b6b52ac78e75b23cbff2d1a1806cd`.

These bounded nonshipping checks supplement the separate live-fade bank-local
experiment; they do not establish natural reachability of injected state, a
complete output-only-fault or post-Enable policy, phase-matched recovery,
representative native performance or deadline safety. ADR-0074 remains
Proposed and production H4 is unchanged.
