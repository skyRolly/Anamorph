# Actual candidate-engine post-Enable overflow search

A source-bound fresh build of the nonshipping consumption-aware candidate on
reviewed integration source `70bbdc22acdfd6286d13b1a15cb445367525c5e5`, tree
`bd96b406db5de5b85d3c72d9db55165a36860bb9`, ran specification/probe commit
`5c573baaa81dbed3f99b0313876fdaaff9119cb2`. The retained receipt and streams
identify source, patch, JUCE, commands, objects and executable. Runner and
selected program exited zero. The target checkout, JUCE, build inputs and
shared source Git metadata were not written concurrently. The actual final
merged main remains a separate identity.

The bounded search ran 216 actual engine cases: 2–4 bands, split 50/1000/10000
Hz, four finite amplitudes up to 0.99*FLT_MAX, three sign patterns and OS off or
actual x4 wrapping. Drive 6 dB, Mix 0.4 and an actual Multiband Disable fade
were used; 124,200 interior smoother ticks ran, clock mismatches were zero,
all 108 requested wrapped cases observed the actual wrapper, and all observed
conditioned dry samples were finite. No non-finite input was injected.

The candidate checks/sanitizes its computed dry bank outputs before the
unchanged production Multiband Enable interpolation. Nevertheless, **2,052
channel-samples of post-Enable aligned scratch were non-finite**, and 2,035
observed delayed measurement-reference samples were non-finite. One example
is two bands, 1000 Hz and a finite +/-0.49*FLT_MAX square signal alternating
every 64 samples: 40 non-finite aligned samples in both OS-off and actual x4
cases. No non-finite aligned-ring slots were observed at block-end in this
search; this does not by itself prove that invalid history was never published
or consumed before the engine's later self-heal. The ring reference observation
shows actual delayed reads can be non-finite in the tested execution.

This establishes actual finite-input reachability in **this candidate** and
invalidates an assumed pre-Enable-only protection. The candidate and original
production share the interior interpolation expression, but this search does
not by itself establish which pre-interpolation values unpatched production
produces or whether its failure has the same mechanism. A targeted trace with
pre/post values and an independently fresh unpatched control is required before
classifying it as a distinct product finding. Do not attribute it to N2-08 or
claim ordinary-input incidence or audible impact.
