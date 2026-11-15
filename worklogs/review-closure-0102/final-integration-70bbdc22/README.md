# Intended integration — 2026-11-15

The isolated validation branch head is
`70bbdc22acdfd6286d13b1a15cb445367525c5e5`, tree
`e968d53284a38704a1f3982428a134c23252f3c3`. It includes actual main
`1ade6e1ad128b067930ab216bc355605388eb1de`, #237
`e64e11fbf54b910aa6a32acafbeb4c3f64815f20`, #238
`f11dba49b9ff68c48f96d1b4435c9464a1d7bb24`, #239
`b08d514f9f147fd8c1b19e24ec0cba2f9ae57ef6`, and #240
`22e56b32e02420502cf9b091bf8b04baad2c340e`. This is not a merge of the PRs.
The independent H4 research branch is linked from #240 but is not included.

The delta from prior integration `a24b4efd` is five documentation files: an
initial H4 cost statement is explicitly dated and the separately committed
newer recovery/cost evidence is linked. The merge was textually clean. All
shipping source, tests, shared provenance tools and isolated-control code are
byte-identical to `a24b4efd`; all shipping source and tests are byte-identical
to prior validated `3669cf71`. The merged #242 fixture and original audit
archive remain unchanged. ADR-0074 remains Proposed and no H4 candidate is
shipping. [Prior integration records](../final-integration-a24b4efd/README.md)
retain the conflict decisions, local checks and the separate parser failure.

Documentation and citation checks passed locally on this exact identity. The
22 provenance guard tests, State-coverage, portability and real-time checks
passed on `a24b4efd` with identical corresponding code; those are not relabeled
as executions of this commit. The full local product and isolated-mutation
replay is running on `a24b4efd`; its source, tests and runner bytes match this
commit, but its execution identity remains that prior commit. The separate
fresh header-clamp control there returned the expected 392/1 and N2-02 720/0.

The exact-source hosted Build & Validate `43710889493`, CodeQL `43710898389`
and Microsoft analysis `43710897923` were started for `70bbdc22`; record their
actual outcomes before declaring integration ready. The prior `a24b4efd` Build &
Validate run was cancelled by the new source, not a passed final-head run.
The [staged owner procedure](../INTEGRATION_PROCEDURE_2026-11-15.md) records the
repository's enabled automatic branch deletion and the required mitigation.
