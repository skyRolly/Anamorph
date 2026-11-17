# N2-08: copied wet-filter trajectory after the first overflowing numerator

This nonshipping experiment follows up the existing naturally driven N2-08
engine witness; it does not implement an LR4 correction or amend the JUCE
equivalence contract. A receipt-controlled relink used production main
`230da0e2b0b81422f7e3964eea2833c48266a963` (tree
`a322ae4cab7c4656a3fd2beb465f3e62e31d8f9c`), pinned JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd`, fresh-build receipt
`31bf22e2e643f45d56e33d9f67e7c063dafd462e`, and the committed probe and spec
at `71247614de58b8ea167a62b7c6be48e872d69422`. The executable SHA-256 is
`7f6740cf755fbf6331de557e8009279b341b08ff6ce3c4fe98e7a32e274dc0f3`.
The outer relink exited 0. Three positive executions exited 0; the deliberate
false-claim control exited 1 as required. All output and input identities are
in [results-v3](results-v3/identity.json); the GCC 14.2 Linux/Ninja commands and
compiler-input premises are in the receipt. The LR4 header bytes in combined
rehearsal `d5e38a53` match main `230da0e2`, but this is not an execution on
that combined Git tree or on a later merged main.

The probe derives from the archived `engine_cause.cpp` witness. It executes the
real engine's extreme finite stimulus, copies the actual active Wet crossover
state, then follows **copies** for 128 samples of input 0.15. One run holds the
cutoff; another explicitly changes the copied filter's cutoff before each
subsequent sample. That is a filter-level changing-coefficient control, not an
observed engine glide or bank fade. The counterfactual step uses the original
float expressions and state writes except for the specified numerator fallback.

| Copied-filter calculation | Fixed cutoff | Changing cutoff |
| --- | --- | --- |
| Widen only the first sample's first numerator | First non-finite output or state at sample 1 | First non-finite output or state at sample 1 |
| Widen each non-finite first-section numerator with finite operands | Three fallbacks; finite selected output and state through sample 127 | Three fallbacks; finite selected output and state through sample 127 |
| Also allow the same fallback in the second section | Same selected result; no second-section fallback occurred | Same selected result; no second-section fallback occurred |

An independent copy running the actual unmodified `LR4Xover::processSample`
provided the comparison before any fallback. On the selected 0.5 ordinary
pulse and on the smaller `0.1 * FLT_MAX` witness, there were no fallbacks and
the low/high output and four channel-state floats matched bit-for-bit in all
128 samples for each selected fixed/changing-cutoff variant. This is scoped
comparison, not all-input JUCE equivalence. The deliberate assertion that one
widened step protects the full selected trajectory failed twice, once for each
cutoff case. It confirms sensitivity to the specific unsupported claim.

The original one-step result was therefore insufficient even for the next
sample of this selected copy. Repeated fallback is a more plausible experiment,
not a supported production candidate: selected finiteness is not a correct
full reference trajectory, hidden intermediate overflow may remain, the second
section's exceptional branch was not exercised, and actual bank copy/fade,
both channels, repeated extreme input, full-engine output and recovery, full
JUCE twin behavior, real-time cost and allocation remain unestablished. Since
the exceptional result differs from actual LR4/JUCE for this witness, the
existing all-sequence bit-equivalence contract still requires a separate human
numerical decision before a shipping change. No native performance or audible
impact is inferred.

The [preliminary two-execution run](results-v2/identity.json) reported the
same trajectory observations before the stronger assertions and negative
control were added. A still earlier [attempt](abbreviated-spec-attempt.log)
was rejected before execution because it passed an abbreviated rather than a
full spec commit; the guard correctly failed closed. Both attempts are retained.
