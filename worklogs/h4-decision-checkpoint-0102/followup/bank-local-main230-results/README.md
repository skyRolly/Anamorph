# Nonshipping bank-local H4 recovery comparison

Fresh Linux/Ninja Release execution used actual integrated main
`230da0e2b0b81422f7e3964eea2833c48266a963`, candidate diagnostic tree
`397000f323940a7fae0336561d672a860edcbe8c`, patch SHA-256
`fd0d2623db8b133576662041d67ed67ebaebdc7ce794605566adb1dee997785d`,
pinned effective JUCE `d60b7a78030661574a615b586d9325ba7a7fa7dd`, and
specification/probe commit `346b4c215083e9117a44fc63283fae33e97ab97f`.
The receipt binds the specified source, dependency, generated inputs, build,
probe, executable, commands and results within its documented trust model.
Outer exit was 0; executable exits were 0, 1 and 1 as specified.

This nonshipping candidate retains the earlier conditioned-dry fault-block
fallback, but when processed recursive state identifies one or more invalid
banks it resets only those banks in the affected channel. When output is
invalid but all monitored states are finite, attribution is unresolved and
the diagnostic still resets both banks of the affected channel. It does not
solve later post-Enable overflow or recreate missing phase-matched history.

In 12 observed live-fade lanes (2/3/4 bands, OS off and actual x4 with six
samples latency, source- or destination-bank injection), **24/24** repeated
injected recoveries preserved the other live bank's independently advanced
state. The selected Wet-bank states, opposite channel, raw and conditioned
histories also matched their references. The focused observer passed
**277/277** checks and observed no allocations in those 24 calls with live
new, aligned-new and malloc guards. A contrary reset claim failed **24/301**
and switching the fallback off failed **24/277** as intended. Compare with
the separately receipted [previous fallback](../fade-collateral-main230-results/README.md),
which reset the valid other bank in all 24 corresponding selected calls.

The injected state does not establish natural reachability. The test does not
establish a complete recovery contract, output-only fault attribution, all
fade/glide/retirement histories, post-Enable publication, exact affected
trajectory, native timing, worst-case allocation or deadline safety. This is
not approval of a product policy or production code. A broader first-
consumption and low-frequency comparison is a separate execution; no result
for it is implied here. ADR-0074 remains Proposed.
