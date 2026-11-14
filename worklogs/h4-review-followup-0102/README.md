# H4 review follow-up: hidden recursive state

The following is a new isolated experiment, not a rewrite or revalidation of the historical archive. Run at `c64e38b44c655c313a4851e6e985a70ccef550b2` on Linux x86-64 with GCC and contraction off:

```sh
g++ -std=c++17 -O2 -ffp-contract=off -Iworklogs/h4-review-followup-0102/stub -I. worklogs/h4-review-followup-0102/lr4_hidden_state.cpp -o /tmp/anamorph-lr4-witness
/tmp/anamorph-lr4-witness
```

Observed: `first_output=1.05003e+38 hidden_s2=1 next_output_nonfinite=1 other_channel_finite=1` (exit 0). The witness calls the checkout's actual `LR4Xover::processSample` with zero initial state, sample rate 48 kHz, cutoff `48000 * .45 / 1.21` (within the top-down ordered crossover ceiling), and input `0.95 * FLT_MAX`. Test-only private access observes that the first output sum is finite even though `s2[0]` overflowed; the next zero input produces non-finite output. The other channel stays finite. The stub supplies only JUCE's `MathConstants<double>::pi`; filter calculations come from the repository header. This demonstrates an internal arithmetic counterexample, not an end-to-end engine failure or proof that the exact stimulus survives the engine's conditioning, wet processing, all bank/fade cases and guard timing. In particular, position at a full-wet block boundary and Mix re-entry on the next block are plausible failure conditions requiring an actual A2 engine experiment, not established here.

A2's unused-output scan therefore cannot establish that all recursive state is finite at the end of a gated block. Existing positive recovery traces remain valid for their executed stimuli but do not prove recovery completeness. Explore explicit channel-local recursive-state validation after each warm block (including both banks and all actually live state) and compare it with alternative bounded mechanisms. Such a candidate must also address invalid intermediate samples, aligned ring contents, consumption during re-entry, active/inactive/fading banks, repeated recovery and unrelated state. The extra scans and recovery path must be measured; they are not free and may affect the cost/architecture decision. No candidate or production source is changed by this witness.

## Candidate-engine witness and countercontrols

`engine_witness.cpp` executes the A2 candidate in the actual engine at 48 kHz, 128 samples, two bands, full-wet H4, Match off, global Width 0 and a high, ordered low crossover. After eight zero blocks it supplies `0.95*FLT_MAX` on the left **last sample of a block**, zero on the right, then follows with finite input and a Mix dip. A2 reports finite dry output `1.05003e38`, finite wet output `5.25017e37`, one non-finite left `dx` state and zero invalid right `dx` states immediately after the extreme block. The next left block contains 128 zero samples. This is a confirmed counterexample to A2's claim that scanning only unused dry output catches every invalid dry recursive state by the boundary.

Do not attribute that next zero block to dry re-entry on this evidence: `engine_controls.cpp` repeats A2, resets only left dry state before the dip, or leaves Mix at 1. All three still produce 128 left zeros. The stimulus may also contaminate other state; that cause is unisolated. The control itself rules out the stronger claim that the observed zero block requires a dip or this dry state. It does not disprove a separate re-entry failure from hidden dry state under a different stimulus.

`candidate-state-scan.patch` is a **nonshipping experiment** based on N2-03 `003c9a1a` plus A2, with an extra per-block check of all four stored states of every dx/dax in both banks for each channel. In this single case it reports zero invalid dry states after the extreme sample; the subsequent zero block still occurs. This only demonstrates sensitivity to this hidden-state witness. It has no full correctness/recovery matrix, no cost measurement, no proof of channel preservation across all paths and no owner approval. Checking dormant/unused state may also change recovery frequency. Do not treat it as a selected solution.

Build environment: Debian trixie container, GCC 14.2, x86-64 `-march=haswell -ffp-contract=off`, Release, JUCE `d60b7a78030661574a615b586d9325ba7a7fa7dd`. The A2 and experimental variants were applied to isolated `003c9a1a` worktrees; selected test/engine/Multiband/MonoMaker/SoloMonitor translation units were rebuilt and linked with remaining objects from a freshly configured/built `383353c0` tree (its production source matches `003c9a1a`). The identities record those commands and binary hashes, but their `source_sha` is **only the underlying Git HEAD**, not a representation of the dirty candidate tree, all headers, reused objects, compiler or environment. The patches and witness source are retained for reproduction; the object provenance is a locally established execution premise, not a cryptographic attestation. A fresh full build and independent execution should precede any decision.

`engine_cause.cpp` and `production-cause-spec.json` add a nonshipping follow-up
experiment for the extreme-input zero block. The witness observes wet and dry
recursive state at the block boundary, then compares dry-only, individual-channel
wet-only, both-channel wet, and Haas reset controls, a no-dip control and a lower
finite stimulus. Its source is a diagnostic observer and direct state mutation,
not shipping instrumentation. The specification describes a planned execution;
it is not itself a result or a causal conclusion. Results require a fresh build
and independently recorded input, executable and output identities.

The first guarded attempt rejected an abbreviated packet commit before building;
the full SHA is required. The next attempt built the production test target but
the new observer failed to compile because its namespace `access` collided with
the POSIX function. Both logs are retained in `execution-2026-11-14/`; the
observer was renamed and needs a new fresh-build execution. These are input and
diagnostic-tool failures, respectively, not product-test results. Replay-tool
source was also being hardened during the failed build, so that attempt cannot
serve as a final provenance-controlled observation.

The additional `candidate-a2-003c.patch` and `candidate-state-scan-003c.patch`
record the exact previously used dirty trees relative to `003c9a1a`, including
their changed header bytes; the adjacent specifications bind those trees and
select the original engine control probe. They are nonshipping reproductions,
not new candidates or a substitute for verifying the source tree and fresh build.

The first completed fresh production trace is retained under
`execution-2026-11-14/preliminary-production/`. Its wet-bank reset controls
suggested a role for large finite wet recursive state, but that version also
read `dryAlignScratch` while H4 had not written it. That scratch value was not a
valid observation, and the read can invalidate the experiment as a whole. The
preliminary run is retained for audit, not used as causal proof; the observer no
longer reads that scratch, and its corrected execution must be evaluated anew.

`production-stage-trace.patch` is an additional nonshipping diagnostic against
the combined production source. It reads the actual signal at named processing
boundaries and logs the existing final-output self-heal; it changes no intended
signal arithmetic and is unsuitable for performance measurement. Its adjacent
specification fixes the base, patch bytes and resulting tree and selects six
separate controls. The trace does not establish a result merely by existing.
