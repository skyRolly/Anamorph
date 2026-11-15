# N2-04 — conditioned dry history during self-heal

## Mechanism and focused correction

The original [numerical audit](dsp-numerical-audit-0102/original/REPORT.md)
showed that a processed-output non-finite cleared otherwise valid conditioned
dry delay samples in both channels. With a selected nonzero latency, a finite
left-channel overflow could therefore erase valid subsequent right-channel Mix=0
audio. This ring and consumer are distinct from D-02's raw Bypass path and H4's
recursive crossover state. ADR-0009 requires output sanitation and self-heal
without discarding valid audio.

The focused production change replaces the conditioned ring clear at the
existing self-heal with a bounded scan: each non-finite slot becomes zero and
every finite slot retains its bits. The fixed ring is already allocated; the
scan runs only after the existing final output guard reports a non-finite. Wet
node, aligned dry and OS compensation resets remain in their existing order.
Raw bypass, prepare and explicit reset retain their separate semantics. No
graph, latency, parameter, persistence, smoother or normal-path arithmetic
changes. This neither prevents invalid conditioned samples before an existing
self-heal nor corrects N2-05's wet arithmetic at Mix=0.

## Controls and current evidence

The nonshipping [provisional v5 record](https://github.com/skyRolly/Anamorph/tree/a32f9727/worklogs/n2-04-conditioned-history-0102/provisional-v5)
retains the source patch, test header, hashes and raw streams. On reviewed
integration `70bbdc22`, its ordinary local GCC 14/JUCE 9.0.5 development build
passed 109 focused and 5723 full DSP checks. Restoring the original ring clear
failed 24/109; retaining the ring unconditionally without scanning failed
6/109. The controls exercise selected x2/x4/x8 latency stand-ins, observed
actual Chorus/Dimensional wrapping, both channels, natural non-finite input,
injected ring poison, signed zero and the available allocation guard. The
setup failure and earlier latency-premise correction are retained there. These
are provisional development observations, not exact-final-head hosted or
receipt-controlled results.

The standalone selector `--n204-conditioned-history` is also registered in
the full DSP suite. Final-main rebasing, exact-source replay, applicable hosted
validation and owner review remain separate from these provisional results.
No claim of native DAW, listening, hardware or release acceptance is made.

The preliminary documentation check first reported that this separate clone
had no `origin/main`, which prevented its release-link ancestry check; fetching
that ref made the check applicable. The citation check then identified the
expected line shifts from the self-heal insertion, including the metering tap.
The anchors were re-derived and the historical sign-off range retained in its
audit note. The documentation and citation checks passed against rehearsal
base `ca5c6fc2`. These were documentation/environment findings, not DSP test
failures; final-base checks remain required.
