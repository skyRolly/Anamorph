# N2-07 Drive endpoint candidate — scoped controls

The nonshipping engine-only diagnostic applies patch SHA-256
`5f301ec52011b217ae0ab42e4a8412dd6313d6d03b4b7300d64574e23490342d`
to actual integrated main `230da0e2`, expected tree
`51e6aa86679c9368433fdb172915bfa788a8de90`, with fresh baseline receipt
`31bf22e2` and probe/spec commit `0e45e6a6`. Serial receipt-controlled
Linux/Ninja relinks recorded source, pinned JUCE, generated/compiler inputs,
objects, binaries, commands and results under the documented trust boundary.
Both outer runners exited 0.

The candidate selects finite saturation at exact Drive blend 1 in the settled,
unwrapped-ramp and held-envelope call sites; other coefficients and invalid
operands retain the historical expression. The independent actual-engine
probe passed **8/8** selected assertions. Main failed **3/8**: positive and
negative `1e8` inputs produced zero rather than saturation near `+/-1.03768`
on the one-band H4 exact-Wet path, and actual x4 wrapping also produced zero.
The candidate produced approximately `+/-1.03768` and a finite x4 result
with six samples latency. The selected non-endpoint output hashes were both
`36e7661d70cc2a10`; this is a bounded sample comparison, not proof of all
interior inputs. The checked unwrapped smoother clocks and non-finite recovery
passed in both modes.

This is a candidate, not a production correction or complete regression.
The selected x4 execution was settled; a live wrapped/base crossfade and its
held-envelope clock, the two individual ramp call sites, signed-zero and
additional invalid-state/automation interactions require independent controls.
There is no native DAW, listening or audible-impact inference. N2-06 and H4
production decisions remain separate.
