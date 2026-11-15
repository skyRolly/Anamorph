# Current controlled-relink controls — 2026-11-15 UTC

The maintained tools at #237 commit `10ddde5e50ec4688fc585d365322e7e561f72388` were run against a fresh build of historical combined source `3669cf716b27de56dd125f29d0b2672598aebdc2`. The committed [fresh build receipt](fresh-product/identity.json) records the product tree, JUCE checkout, compiler, build, environment, objects and executable. The fresh product selectors returned 720/0 N2-02 checks and 392/0 N2-03 checks. This is not validation of a subsequently changed combined source.

A receipt-checked relink of the independently recorded injected-state probe completed all four declared runs, including the two deliberately failing no-fault controls; [identity, commands, dependencies, executable hash, exit statuses and raw streams](controlled-relink-current/identity.json) are retained. This verifies that the new input boundary permits this legitimate execution; it does not broaden the probe's original claim.

The following controls each modified only the named build input, attempted an actual Anamorph receipt-controlled relink, restored the input in a finally block, and checked that no output directory was created. [Results](controlled-relink-current/negative-results.json) and individual error streams are retained.

| Mutation | Actual outcome |
|---|---|
| Added a higher-priority `dsp/AnamorphEngine.h` shadow | Rejected: generated compiler-input directory changed |
| Changed an existing generated file | Rejected: generated compiler-input directory changed |
| Substituted the existing engine object | Rejected: recorded fresh-build object changed |
| Changed `CPATH` | Rejected: compiler/build environment differs |
| Restored inputs and rechecked | Accepted |

These controls test actual generated inputs and objects, not merely a synthetic CMake fixture. They assume the recorded fresh build was honestly produced, trusted compiler/system inputs, a stable filesystem, and no concurrent writers; they do not attest the host or historical executions. The input-boundary controls do not prove the numerical correctness of a declared experiment or the reviewer's intent in selecting a patch.
