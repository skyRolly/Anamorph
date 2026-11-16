# Manual acceptance scope after the 0.10.2 integration

The 0.10.1 owner attestation of Level-5 and applicable manual acceptance remains
valid within its recorded scope. Its exact human-tested artifact, host and
per-case details were not supplied. This record does not withdraw that
attestation, invent those details, or require repeating unrelated completed
checks. The new automated results are not human acceptance.

For the later integrated production changes on main `230da0e2`, the proposed
targeted owner review is:

- N2-02: host automation of Bypass and Multiband Enable during ordinary switch
  ducks and reversals; compare complete forced preset/A-B/Undo destinations,
  Level-Match behavior and selected oversampling transitions.
- N2-03: Multiband Enable endpoint and in-flight automation with Match off/on,
  partial/full Mix, aligned reference and selected latency configurations.

If the owner separately authorizes any new PR for integration, assess its
changed scope against the final source rather than treating this list as a
completed test: #243 needs self-heal and delayed-history recovery, #244 needs
Mix endpoint/automation and Match/latency interactions, and #245 needs Drive
engagement and smoothing across selected oversampling paths. Extreme and
non-finite cases need suitable controlled test inputs and should not be
represented as an audible-impact claim. A later authorized H4 implementation
would need its own affected-channel recovery, phase/history, Mix re-entry,
Match, latency and representative native performance acceptance.

Apply the current release compatibility and Level-5 procedures to the actual
final release candidate, with the owner deciding the applicable targeted
human/DAW/listening and native checks for its changed scope. The existing
attestation is not silently extended to new DSP behavior, and no new manual,
hardware, DAW, Level-5 or release approval is claimed here. This assignment
authorizes no release preparation, version bump, tag or publication.
