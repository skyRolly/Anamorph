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
