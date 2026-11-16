# Integrated-main numerical witnesses — 2026-11-16

A fresh Linux/Ninja Release replay used actual main
`230da0e2b0b81422f7e3964eea2833c48266a963`, tree
`a322ae4cab7c4656a3fd2beb465f3e62e31d8f9c`, independent effective JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd`, and committed specification
`3e869f2b1b03acc6c79f9fc8ccf1259333176621`. The receipt records the
compiler, generated inputs, fresh objects, probe bytes, executables, commands,
output hashes, exit status and declared output checks. The outer runner and
all three programs exited zero. No production fix is included in this replay.

- N2-05: for the archived finite `2e38` Mix=0 input, 128/128 observed
  output samples were zero instead of the finite dry input; the next moderate
  block recovered. Lower inputs retained exact dry in this selected case.
- N2-06: the archived actual-engine Width=0, Mix=1 witness produced `(0, 8)`
  for independently calculated Wet `(4, 4)`. A separate deterministic,
  deliberately cancellation-sensitive ordinary finite-input experiment
  examined 16,384 output samples. Its independent historical-expression
  model matched actual production output in every sample; 6,794 samples
  differed bitwise from direct Wet and the maximum absolute difference was
  `5.96046447754e-08`. This one synthetic input distribution is not an
  incidence, audibility or all-configurations claim. It establishes that an
  unconditional Mix=1 selection changes normal finite rounding and must be
  assessed alongside the endpoint contract and invalid-state recovery.
- N2-07: the archived actual one-band full-wet-gated Drive=6 witness produced
  zero for both `+1e8` and `-1e8`, independently of the Mix reblend.

The accompanying raw stdout, stderr, compressed build log and receipt preserve
source, execution and result identities. These are selected Linux diagnostic
executions, not native DAW or listening evidence. The independent overlap
lines are archived-probe observations and are not new isolated N2-02 mutation
controls. Further production proposals must preserve each finding's separate
state, endpoint, timing and accepted architecture contracts.
