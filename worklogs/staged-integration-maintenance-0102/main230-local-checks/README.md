# Actual-main local checks — 2026-11-16

These local checks used actual integrated main
`230da0e2b0b81422f7e3964eea2833c48266a963`, tree
`a322ae4cab7c4656a3fd2beb465f3e62e31d8f9c`, in clean checkouts. They are
supplementary Linux evidence, not hosted cross-platform or manual acceptance.

Documentation checked 210 files; citation comparison against prior main
`377173bb` retained 603 anchors. State-coverage lint passed its 112 self-tests
and the current 36-field/12-module coverage check. The provenance guard suite
passed 22 controls. Portability passed 120 self-tests and scanned 95 files with
zero violations; real-time lint passed 97 self-tests and scanned 54 files with
zero violations.

The #242 ordinary-user Linux fixture handshake and fault controls passed,
including the legacy ordering negative control, missing/malformed identity,
publication/leader/child timeouts and bounded cleanup failures. The original
three process cases returned report status `verified` under UID 1000; the
fixture-owned group children were reaped and the process-local subreaper was
restored. The report's `consumer_head` field is null because no hosted SHA
environment was supplied; the commands were run in the separate checkout at
the exact commit and tree above. These do not establish native macOS behavior.

The final-main hosted matrix and analyzers are tracked separately. In
particular, its first Windows Configure attempt failed with inaccessible raw
logs and must not be replaced by a claim about its cause.
