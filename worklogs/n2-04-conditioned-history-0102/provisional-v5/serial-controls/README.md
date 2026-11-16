# Sequential receipt-controlled N2-04 v5 controls

These relinks use the committed fresh-build receipt at `dd1556b3`, preparation
head `623d3c33`, pinned JUCE, and probe/spec commit `71ed19cf`. The copied
probe support headers have the same SHA-256 bytes as the product test and
allocation guard at that source. The verified build, source, dependency,
generated inputs and objects were checked before and after each relink; each
mutation was run separately, with no concurrent JUCE index writer. The positive
control ran before the earlier rejected parallel attempts; the two negative
controls here were rerun sequentially into fresh directories.

- Unmodified candidate: executable exit 0, **109 checks / 0 failures**.
- Only the conditioned-ring scan replaced by the original clear: executable
  exit 1, **109 / 24 failures**.
- Only the scan removed, leaving unconditional retention: executable exit 1,
  **109 / 6 failures**.

All outer relink runners exited 0 because the specified positive and negative
statuses and output markers matched. Full patch, command, executable, object,
input and result identities are in each receipt. The earlier overlapping
attempts and index-lock failure remain in `../parallel-attempts/`.

These are v5 controls on a preparation source, not exact-final-head v6 or
post-merge evidence. The v6 development controls and the final maintenance PR
have separate identities and must not be conflated with this receipt.
