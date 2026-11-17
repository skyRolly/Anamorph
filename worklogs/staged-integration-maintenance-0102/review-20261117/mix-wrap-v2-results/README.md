# N2-06 actual wrapped terminal output control

Two serial receipt-controlled Linux/Ninja relinks used main `230da0e2`,
receipt `31bf22e2`, pinned JUCE and committed spec/probe `43cfe456`.
The header bytes are the proposed PR #244 test revision; engine-only candidate
and smoothing-only restoration are recorded in each receipt. Both outer
runners exited 0; positive executable exited 0 and mutation exited 1 as
specified. These are not complete PR-head builds.

The positive ran 16 checks with zero failures. Both subject and one-band wet
twin actually executed x4 and reported six samples of latency. Across six
selected amplitudes, 684 terminal channel samples were observed, 456 differed
from the historical Mix expression, and the actual production output had zero
mismatches against the twin. Restoring only the smoothing Mix expression caused
456 mismatches and two assertion failures, including the new wrapped-output
assertion. The wet twin is an oracle for this selected endpoint path, not an
independent oracle for the complete Level-Match algorithm.

The first +/-1e8-only premise failed under the uncorrected N2-07 source and is
retained in `../mix-wrap-failed-premise`; it is not relabeled as passing.
The existing original-expression, invalid-operand and interior controls remain
separate. No native DAW or audible-impact claim follows.
