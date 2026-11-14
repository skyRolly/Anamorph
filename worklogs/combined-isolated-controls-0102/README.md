# Isolated N2 mutation controls

The source and assertions selected here are the historical combined commit
`f2686cb5b0b3687a3e343340f2494f59961aa83a`, not an automatically current
integration. The five engine patches restore only the ordinary N2-02 merge,
premature forced adoption, both N2-03 call sites, the wet call site alone, or
the dry call site alone. The separate clamp patch changes only the endpoint
kernel in `AnamorphEngine.h`. Each patch and expected resulting tree is listed
in `mutations.json`; the specifications also state the intended behavior and
expected selector results. A hash fixes selected bytes, not their meaning.

`run_controls.py` creates a fresh build of both test targets and refuses a
pre-existing build. The engine-only variants are generated from committed patch
blobs using a temporary Git index; it requires the resulting tree to match the
specification and its only changed path to be `AnamorphEngine.cpp`. It then
recompiles that translation unit for each target and relinks only objects
created by the same verified fresh build. The source and JUCE trees, generated
commands, linked object hashes, compiler, configuration, executable and raw
selector results are recorded. The clamp changes a header and therefore uses a
separate fully fresh diagnostic checkout/build through `replay_experiment.py`,
not the single-translation-unit reuse path. No historical complete-engine
replacement is silently reinterpreted as an isolated N2-02 mutation.

All checks assume trusted local Git, compiler, system dependencies and
filesystem, and no concurrent source, build or tool modifications. A committed
specification still needs review of the intended experimental difference. The
results are not an attestation of the host or historical binaries. Until actual
executions and their receipts are recorded, these controls are prepared tests,
not evidence of their expected outcomes.
