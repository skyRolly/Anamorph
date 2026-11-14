# Diagnostic source-tree provenance prototype

This branch is an experiment, not a reviewed PR head or the validated combined
source. It extends the historical replay guard with a function that builds an
expected Git tree from an explicit full commit and a patch whose SHA-256 is
specified by the operator, then compares each tracked checkout file's raw bytes,
type and executable bit against that tree. Untracked files and ignored inputs
in the named source directories are refused. A fresh-build function checks the
source and pinned JUCE before and after configuring/building into a new directory.

Ten synthetic controls pass, including exact diagnostic patch, unintended edit,
wrong dependency, stale output-directory refusal and a synthetic fresh build.
Direct local controls accepted the clean original baseline, a clean combined
checkout, and the specified A2 patch on `003c9a1a`; an extra source edit was
rejected. This does not establish compiler, system dependency, generated header
or concurrent-writer provenance, and the patch hash asserts the caller's selected
bytes, not their semantic correctness. Raw Git object hashes are identities, not
an independent build attestation. The tool has not been reviewed on non-POSIX
filesystems, used for an end-to-end product replay, or wired into the N2-02,
N2-03 and H4 relinkers. Those helpers remain unresolved. Do not promote this
prototype as a provenance guarantee or replace the validated PR heads with it
without further design, negative controls and exact-head CI.
