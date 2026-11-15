# Provisional N2-04 v5 — production-path and sanitation controls

This is an ordinary local development build against reviewed integration
`70bbdc22acdfd6286d13b1a15cb445367525c5e5`, not a receipt-controlled replay or
a result against final integrated main. GCC 14, JUCE 9.0.5, the
`codex-rolly-native:trixie-gcc14-v1` container and an existing Ninja Release
build at `/workspace/n204-provisional-build` were used. The source patch,
untracked test header and SHA-256 source identities are retained here; the
patch modifies only the exceptional self-heal and test registration.

The candidate replaces clearing the conditioned dry ring with a bounded scan
that preserves finite slots and zeros non-finite slots, keeping the aligned
dry and OS compensation resets. The focused selector passed **109/0**,
and the full candidate DSP suite passed **5723/0**. Actual x2/x4/x8 wrapped
modulation paths, both channels, delayed histories, a natural non-finite input,
injected poisoned ring slots, exact signed-zero preservation and the allocation
guard were exercised. The full suite result includes the 109 selector checks.

Restoring only the original conditioned ring clear produced **109/24 failures**.
Removing the candidate scan, leaving unconditional ring retention, produced
**109/6 failures** in injected and actual-input poison controls. These exit-1
runs are intended negative controls, not product-validation success. An initial
unsafe-control setup attempted to match the baseline clear against an already
corrected source; its assertion failed before any compilation or test. It was
corrected to remove only the candidate scan. The earlier 59-check experiment
and its failed latency premise remain in the sibling provisional record.

The positive and negative control binaries were built sequentially by
`cmake --build /workspace/n204-provisional-build --target AnamorphTests`
inside that container, using `CMAKE_BUILD_PARALLEL_LEVEL=1`; each was run with
`AnamorphTests --n204-conditioned-history`. The unmutated full suite used
`AnamorphTests` without the selector. These are development observations;
they do not establish final-source CI, a complete recovery contract for H4,
N2-05, or approval to merge a maintenance PR. Rebase on actual final main,
independent provenance-controlled replays, documentation and full hosted
validation remain required.
