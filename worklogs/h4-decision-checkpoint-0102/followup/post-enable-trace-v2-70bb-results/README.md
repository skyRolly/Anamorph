# Candidate trace: Enable, aligned history and later dry interpolation

A fresh source-bound diagnostic execution on source `70bbdc22`, diagnostic
tree `1a303d5fa65d5162b5dec823fc389a06776db664`, specification commit
`a6eab0c74b3457daa0f96e0a76933dce1435080e`, completed with runner and probe
exit 0. The receipt binds the full source, diagnostic patch, JUCE, build,
objects, executable and streams. The instrumentation and test-only comparison
switch are nonshipping; these are not timings or actual post-merge-main results.
The prior failed control and its overly strict 40-count premise are retained
under `post-enable-trace-first-attempt/`.

The traced witness uses two bands, 1000 Hz, Drive 6 dB, Mix 0.4, an actual
Disable ramp and finite +/-0.49*FLT_MAX input alternating every 64 samples. In
both OS-off and actual x4/six-sample-latency execution, the unmodified candidate
path produced **40** Enable outputs that were non-finite despite finite clean
and already-guarded aligned operands and an interior blend. The first operands
were `clean=-0x1.f5c28ep+126`, `aligned=0x1.1b386p+127`,
`blend=0x1.c2aabap-1`, result `+Inf`. The trace observed **40 invalid aligned
history writes, 40 invalid reads, 40 invalid Mix outputs, 40 final scrubs and
four actual self-heal entries** per OS case. The 40 Enable failures match the
previous uninstrumented candidate search for this exact workload. Opposite
channel and broader signal properties are not established by this trace.

A deliberately nonshipping exceptional-only double calculation substituted a
finite result only when this particular aligned Enable expression overflowed
with finite endpoints and an interior coefficient. It produced **zero invalid
aligned writes/reads** in these cases, but it changed later state trajectories:
there were 70 Enable overflows encountered and replaced. **Seven further invalid
Mix outputs and one heal remained**. The trace directly observed each of those
seven at the following dry-source interpolation: its clean and aligned history
reads were both finite, but `clean + 1*(aligned-clean)` became `+Inf` at Mix
0.4, then the Mix output became NaN and the final guard scrubbed it. The first
finite operands and actual wet/Mix values are in `run-0.stdout`. The positive
and diagnostic-control premises all passed; 40 was required only for the
unmodified case, not for a changed recovery trajectory.

This establishes two reachable boundaries in the **candidate** for the stated
extreme finite workload. The widening switch is a discriminating experiment,
not an approved recovery policy or production correction, and does not solve
all exceptional behavior. The shipping comparison is separate. No audible
impact, representative timing, universal reachability or ADR acceptance follows.
