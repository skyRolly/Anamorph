# H4 actual aligned-history write/read boundary — provisional integration

Fresh execution at source `70bbdc22acdfd6286d13b1a15cb445367525c5e5`, diagnostic
tree `bd96b406db5de5b85d3c72d9db55165a36860bb9`, spec/probe commit
`9c1a862292b53b2ca613b9cdbed5cf3c4c948162`. The maintained runner produced a
fresh build and retained source/dependency/build/probe/executable/stream
identities in `identity.json`. The outer runner exited zero; the positive and
two expected-negative process exits were 0/1/1. The checkout and JUCE were
unchanged during execution; no other worktree wrote the shared Git repository
during this run. This is the reviewed integration identity, not yet actual
post-merge main.

The independent wet twin remains settled full wet; the faulty and clean twins
execute a Mix re-entry. After ordinary 128-sample warm-up, each processes 32
one-sample blocks so the actual history write position, selected-latency read
position, published aligned scratch sample, smoothed Mix coefficient and output
can be compared at every boundary. The clean reference of the full-wet gate was
observed and distinguished from aligned history; actual x4 wrapping and six
samples of latency were observed in the second case. A deliberately injected
NaN dry integrator was detected and reset. The positive run passed **28/0**:
actual ring writes and read positions matched their independent index model,
outputs matched the wet twin plus the observed delayed dry history, and checked
opposite-channel output/aligned history and conditioned/raw histories remained
identical to the clean twin.

After the injected failure the finite affected aligned history differed from
the clean twin at **32/32** observed writes; affected output differed on
**32/32** samples at zero latency and **26/32** at x4, first appearing at
sample offsets 0 and 6 respectively. Thus actual latency delays consumption of
the changed history, but finite replacement does not reproduce the clean
trajectory. These one-sample blocks complement the previous 128-sample-block
probe; they do not prove equivalent bank-fade behavior or natural reachability
of injected invalid state.

The deliberately false claim that finite retained history equals the clean
reference failed **2/30** checks, one per latency case. A separate probe-side
injection of a non-finite value after publication caused **8/28** failures and
was observed in both cases; it establishes oracle sensitivity, not that the
candidate naturally publishes such a value. The wet-twin reconstruction checks
the Mix consumption arithmetic, not independent correctness of a previously
lost dry trajectory. Acceptable exceptional output and future history remain
an owner contract question. No shipping source or ADR status changed.
