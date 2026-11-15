# Unpatched-production dry-source overflow search

Receipt-controlled diagnostic relink and execution against reviewed integration
`70bbdc22acdfd6286d13b1a15cb445367525c5e5`, JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd`, fresh baseline receipt commit
`4ec65e0e2e9d78fe741dfc3029bd5af8c0a40cb9` and probe/spec commit
`d8bf82b6af5097793beccc318e1cdec73a085230`. `identity.json` records the
exact diagnostic source tree, patch, inputs, commands, executable and outputs.
The engine-only patch adds tracing and a disabled-by-default diagnostic switch;
the switch was **off in every case**. The runner and oracle exited zero.

The bounded search ran 216 cases: 2/3/4 bands, 50/1000/10000 Hz, finite
input magnitudes 0.25/0.49/0.75/0.99 times FLT_MAX, three alternating
patterns, and OS off or observed actual x4 wrapping (108 wrapped cases),
at 48 kHz/128 samples and Mix 0.4. Trace callbacks observed 248400 interior
channel ticks, 2052 finite-endpoint Enable overflows and 96302 non-finite Mix
channel samples. **No** secondary dry-source interpolation overflow with
finite clean and aligned history operands was observed in this matrix. The
prior widening-control run is a positive control for detecting that latter
boundary: it saw seven when the diagnostic Enable fallback was enabled.

This search supports only the tested workload; it does not prove the second
boundary unreachable in all engine states. The earlier selected unpatched
case and receipt established N2-09 independently. Extreme inputs can cause
other invalid paths and recovery, so aggregate non-finite Mix counts must not
be attributed solely to N2-09. Neither trace grants an H4 or numerical-policy
approval, establishes audibility, or describes final staged main.
