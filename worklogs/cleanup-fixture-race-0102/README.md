# POSIX cleanup fixture identity publication race

The ordinary-user cleanup regression's `leader_exit` fixture forks. Before this
change the child could write `ready` before the parent had completed writing
`identity.json`; the harness waits for `ready` then immediately parses that JSON.
A local run on 2026-11-14 observed `JSONDecodeError` from an empty identity file,
and the independent cleanup report saw an orphaned zombie after the identity
was unavailable. The separately retained idle rerun of the old fixture passed.
The failure is a verification-fixture race; it does not establish a bug in
`diagnose-rosetta-scope.py` or in audio processing.

The child now waits for the parent's `identity-written` marker before publishing
its own ready marker. The parent creates that marker only after `write_text`
returns, so observing readiness implies identity publication has finished. The
existing 20-second suite budget, cleanup semantics, process-group targeting and
ordinary-user runner requirements are unchanged. The new source-lint test runs
the actual fixture with the parent deliberately delayed by 250 ms: a mutation
restoring the old order must expose readiness without complete identity, while
the fixed fixture must expose complete identity. The harness uses a process-local
subreaper and kills/reaps only its own fixture process group.

Hosted integration run 43574998394 failed the same named POSIX cleanup step, but
its raw log and retained artifact were inaccessible from this environment. The
local failure and deterministic control establish this race independently; they
do **not** establish that it caused the hosted failure. Retain the failed hosted
result and rerun the unchanged gate with this correction; do not weaken, skip or
reinterpret it. No native macOS or Rosetta conclusion follows.

## Follow-up review (2026-11-14)

The paragraph above is the original review record. Its 250 ms delay widened a
race but did not make the negative control deterministic. The follow-up uses a
blocked parent and explicit observer acknowledgement; a child gate marker shows
that the fixed child has reached the publication boundary. The legacy control
observes readiness and unavailable identity before permitting the parent to
publish. Paused observers, missing acknowledgements and observer timeouts are
bounded and cleaned up. The original `leader_exit` fixture ordering is unchanged.

The old independent finalizer also assumed an unknown identity meant no child
needed reaping. If the leader had exited, it could skip the group and report the
unknown child as reaped. The fault controls demonstrate that condition without
leaving the fixture behind. The new finalizer operates on the new-session group
only while a live/unreaped owned leader or adopted group child establishes its
identity, reaps group children using the process-local subreaper, checks the
group, and reports any signal, wait or observation error. This is a fixture
harness correction; it is not a change to production Rosetta cleanup. The earlier
hosted failure remains unattributed because its raw artifact was unavailable.

`review-control-results.json` records the follow-up executions and exact test
file SHA-256s. On the shared ordinary-user Linux container (three CPUs in the
affinity mask, cgroup quota two CPUs), each of the handshake, fault-injection,
and original three-case process suites passed three times without synthetic load
and three times with two CPU-bound workers. The workers were terminated and
reaped. These are shared-host controls, not representative real-time or native
macOS measurements. The documentation and citation checks passed locally; hosted
results for a later commit must be checked against that commit separately.
