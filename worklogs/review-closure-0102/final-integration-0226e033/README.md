# Final integration checkpoint — 2026-11-15 UTC

Disposable integration commit `0226e033179fe2ba83602d057b97082432762bc7`, tree
`07c0701c6c3923d47e3f902dc9dc17bb32da8847`, includes main
`1ade6e1ad128b067930ab216bc355605388eb1de` and final PR heads #237
`10ddde5e50ec4688fc585d365322e7e561f72388`, #238
`f11dba49b9ff68c48f96d1b4435c9464a1d7bb24`, #239
`b08d514f9f147fd8c1b19e24ec0cba2f9ae57ef6` and #240
`c84ee8028eb12ee147e1c3b51d543696a26ef263`. It is a validation branch, not a
merge of the PRs.

Main `1ade6e1a` is the two-parent merge commit of #242 at `73c68f7a` and main
`51e6250d`, not a squash. Its post-merge Build & Validate run `43701585510`
succeeded (16 jobs passed, three conditional jobs skipped); CodeQL run
`43701585534` and Microsoft run `43701585556` succeeded. The earlier hosted
cleanup failure remains unattributed. The integrated fixture sources match
main exactly, and the local ordinary-user Linux handshake, fault controls and
three original process cases passed; [the report](final-integration-cleanup-process.json)
records ownership-specific finalization. This is not native macOS evidence.

Compared with prior combined source `3669cf716b27de56dd125f29d0b2672598aebdc2`,
all `src/` and `tests/` files are byte-identical. The material changes are the
provenance path and compiler-input controls, lightweight CI guard, documentation
and nonshipping H4 checkpoint, plus ancestry through the actual #242 merge.
Compared with `f2686cb5`, production DSP files are identical and fixture tests
contain the merged deterministic handshake/finalization controls. Compared with
`7764df5c`, the production source differences are H4 explanatory comments;
the endpoint tests include the later actual Match-on oracle and the fixture
has the merged correction. Original historical audit data was not modified.

The pairwise changed-file intersections from the shared #237 base are #238/#239
12, #238/#240 8 and #239/#240 8. The #238/#239 integration conflicts were
additive changelog, index, testing, coverage, architecture and selector entries;
both versions were retained. The subsequent #240 conflicts similarly retained
the H4 decision documents and both correction records. Only one retired-relinker
heading was kept. Shared source merged cleanly; both DSP selector registrations
and state-coverage lint survive. #242's two affected testing and coverage
records were retained without changing the merged fixture. ADR-0074 is Proposed.

On `0226e033`, the source-bound provenance suite passed 21 controls in the
GCC/CMake container. A first host attempt ran 21 but had two errors because
`cmake` was absent; its log is retained and is an environment failure, not a
product-test result. State-coverage lint and its 110-case self-test, portability
and real-time lints and self-tests, documentation checks and citation checks
and their self-tests passed locally. Their streams are retained here. These
checks do not substitute for the full hosted validation or a native real-time
measurement.

Exact-head hosted Build & Validate `43705177178`, CodeQL `43705181454` and
Microsoft analysis `43705181510` were started for this integration identity.
The source-bound full DSP/State and isolated mutation replay is specified in
`integration-0226e033-controls-spec.json`; record its actual completion and
results separately. A prior-head success is not an exact-head result.
