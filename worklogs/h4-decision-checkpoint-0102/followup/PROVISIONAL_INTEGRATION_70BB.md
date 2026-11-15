# Nonshipping candidate on the reviewed integration tree

The earlier consumption-aware candidate was based on `003c9a1a`. The owner
requested rebuilding it against the actual integrated production baseline.
While the ordered merges and their hosted checks are in progress, the original
candidate patch was applied provisionally to the independently validated
intended integration `70bbdc22acdfd6286d13b1a15cb445367525c5e5`. The original
zero-context patch requires `git apply --unidiff-zero`; a fresh contextual patch
is recorded here for this exact tree. Inspection shows only the four intended
candidate files changed and the N2-02 adoption and N2-03 endpoint code remains
untouched. This is diagnostic source, never a shipping commit.

`h4-candidate-integration-70bb-spec.json` identifies the source, resulting tree,
patch, probes, required numerical streams and full DSP execution. Its status is
an experiment specification, not a result. Any successful execution is scoped
to `70bbdc22`; once actual main exists, a separately identified replay must
bind the candidate to that exact main commit before claiming integrated-main
validation. Exceptional-output and aligned-history policy remain unresolved.
