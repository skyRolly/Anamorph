# N2-09 four actual call sites: initial diagnostic

A serial receipt-controlled engine-only diagnostic used main `230da0e2`,
receipt `31bf22e2`, the combined b182 production engine and committed probe/
patch specification `39e530d2`. The source, JUCE, compiler inputs, objects,
patch, executable, outputs and expected exits are recorded in `identity.json`.
The outer runner exited 0. This was not the exact b182 Git-tree build.

The probe deliberately injected finite opposite-sign operands separately into
all four actual Wet/Dry and L/R Enable call sites during real fades. The
nonshipping exceptional-only double recalculation passed **42/42** checks over
OS off and actual x4/six-sample latency. It observed 40 injected calls, 20
selected dry aligned-history writes, 20 latency-offset reference comparisons,
four originally non-finite operand controls, and 260 actual observations of
each terminal coefficient. Observed finite historical results and terminal
source/destination bits remained unchanged in these selected workloads.
Omitting each of the four call-site fallbacks failed **2** checks in its own
OS-off/x4 negative control; the dry omissions ran 40 checks and the Wet
omissions ran 42 because the probe skips a positive-history assertion for an
intentionally unprotected dry lane. The original non-finite operand test
checked invalid classification at the call boundary; it did not yet observe
the existing final recovery.

The injections establish call-site and selected history sensitivity, not
natural reachability of each injected pair. The separately observed natural
N2-09 dry trace and its secondary dry-source overflow remain distinct. This
candidate exists only in an engine diagnostic, not the shared production
kernel or a production PR. These results do not establish a complete accepted
numerical contract, full Match behavior, broader recovery, compiler portability,
normal/exceptional cost or release acceptance.
