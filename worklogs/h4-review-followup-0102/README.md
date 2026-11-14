# H4 review follow-up: hidden recursive state

The following is a new isolated experiment, not a rewrite or revalidation of the historical archive. Run at `c64e38b44c655c313a4851e6e985a70ccef550b2` on Linux x86-64 with GCC and contraction off:

```sh
g++ -std=c++17 -O2 -ffp-contract=off -Iworklogs/h4-review-followup-0102/stub -I. worklogs/h4-review-followup-0102/lr4_hidden_state.cpp -o /tmp/anamorph-lr4-witness
/tmp/anamorph-lr4-witness
```

Observed: `first_output=1.05003e+38 hidden_s2=1 next_output_nonfinite=1 other_channel_finite=1` (exit 0). The witness calls the checkout's actual `LR4Xover::processSample` with zero initial state, sample rate 48 kHz, cutoff `48000 * .45 / 1.21` (within the top-down ordered crossover ceiling), and input `0.95 * FLT_MAX`. Test-only private access observes that the first output sum is finite even though `s2[0]` overflowed; the next zero input produces non-finite output. The other channel stays finite. The stub supplies only JUCE's `MathConstants<double>::pi`; filter calculations come from the repository header. This demonstrates an internal arithmetic counterexample, not an end-to-end engine failure or proof that the exact stimulus survives the engine's conditioning, wet processing, all bank/fade cases and guard timing. In particular, position at a full-wet block boundary and Mix re-entry on the next block are plausible failure conditions requiring an actual A2 engine experiment, not established here.

A2's unused-output scan therefore cannot establish that all recursive state is finite at the end of a gated block. Existing positive recovery traces remain valid for their executed stimuli but do not prove recovery completeness. Explore explicit channel-local recursive-state validation after each warm block (including both banks and all actually live state) and compare it with alternative bounded mechanisms. Such a candidate must also address invalid intermediate samples, aligned ring contents, consumption during re-entry, active/inactive/fading banks, repeated recovery and unrelated state. The extra scans and recovery path must be measured; they are not free and may affect the cost/architecture decision. No candidate or production source is changed by this witness.
