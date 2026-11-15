# N2-04 v6 repeated-recovery development control

This local GCC 14/JUCE 9.0.5 Release development build uses the candidate code
and test bytes identified in `source-sha256.txt`, on reviewed integration
`70bbdc22`; its patch and test header are retained here. It is distinct from
the fresh receipt-controlled v5 replay at source `623d3c33`.

A finite second spike is written to conditioned history at the end of the
first fault block. The first spike causes the existing final-output guard to
run; the test verifies that the pending finite spike survives in the ring,
reaches the output at the reported latency offset, and is self-healed again
without losing the opposite-channel response. It checks a selected x4 latency
stand-in and observed actual x4 Chorus wrapping, subsequent normal output,
and the live allocation guard during both exceptional calls.

The expanded selector passed **125/0**. Replacing only the candidate scan with
the original unconditional clear failed **28/125**; removing the scan and
retaining all slots failed **6/125**. Both mutations exited 1 as expected.
After restoring the corrected source, the full DSP suite passed **5739/0**.
These are ordinary local development executions; exact final PR source, a
source-bound v6 replay, full State and hosted cross-platform gates remain
separate. Earlier v5 logs and its fresh receipt are preserved unchanged.
