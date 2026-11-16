# N2-07 v2: failed discrimination premise

The serial baseline relink under specification `922fb148` completed with its
expected negative executable status. The candidate's selected normal-input
ramp reached the endpoint inside a block, but reported
`terminal_discriminating=0`: those samples did not expose a difference between
the direct value and the historical expression. The test therefore reported
13 checks / 1 failure. The runner rejected the missing `13 checks, 0 failures`
marker and exited 1; it did not write a completed candidate identity receipt.
Its partial spec, stdout/stderr, selected patch and log are retained as a
verification failure, not a validated candidate result.

The separately committed v1 controls remain valid for their narrower scope.
The next probe must use a demonstrably discriminating endpoint stimulus and
rerun all affected controls under a new specification and output identity.
The injected OS fade is a boundary exercise, not natural-reachability evidence.
