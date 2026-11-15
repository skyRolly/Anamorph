# N2-09: finite aligned Enable endpoints can overflow — production trace

**P3 review assessment, confirmed for the stated extreme finite workload.**
A receipt-checked engine-only diagnostic relink used the fresh unpatched
`70bbdc22` production build recorded in `shipping-search-70bb-results/`,
commit `4ec65e0e2e9d78fe741dfc3029bd5af8c0a40cb9`. Its engine-only trace patch
and probe are from specification commit `a6eab0c74b3457daa0f96e0a76933dce1435080e`;
the trace checkout is the reviewed baseline plus that patch. The receipt here
binds both trees, JUCE, build objects, commands, dependencies and executable.
Runner and probe exited 0. Trace instrumentation and a disabled-by-default
exceptional-only test switch are nonshipping. The uninstrumented clean search
independently observed the same 40 affected samples in each matching OS case.

With two bands, 1000 Hz, Drive 6 dB, Mix 0.4, an actual Multiband Disable fade
and finite +/-0.49*FLT_MAX input alternating every 64 samples, both OS-off and
actual x4/six-sample-latency runs had **40** finite-endpoint aligned Enable
interpolation overflows. Neither clean nor pre-interpolation aligned operands
were non-finite in that trace. A first witness is
`clean=-0x1.f5c28ep+126`, `aligned=0x1.1b386p+127`,
`blend=0x1.c2aabap-1`: the historical interior float subtraction overflows to
`+Inf`. The trace observed **40 invalid aligned history writes and reads, 40
invalid Mix outputs, 40 output scrubs and four self-heals** per OS case. No
non-finite aligned ring slots remained at block end in the uninstrumented
search; the trace proves they had already been written and read. This occurs
at partial Mix with the ordinary H4 full-wet gate inactive.

This is distinct from N2-01's dry-state re-entry, N2-05's Mix=0 wet masking,
N2-06's Mix=1 cancellation, N2-07's Drive cancellation, and N2-08's wet LR4
numerator overflow: the traced operands here were finite at the *dry Enable
interpolation*. N2-03 intentionally retained this interior expression and its
reviewed endpoint fix does not introduce it. No ordinary-audio incidence or
audibility is established. The P3 recommendation reflects only the extreme
finite witness; no production N2-09 correction is authorized by this task.

The diagnostic widening switch was applied only to a non-finite aligned Enable
result with finite endpoints and interior coefficient. It prevented those
invalid ring writes in the selected cases, but altered the subsequent state
trajectory, encountering 70 such overflows and then **seven** invalid Mix
outputs and one heal at a second dry-source interpolation. Its logged clean and
aligned history operands were finite; the existing `clean + 1*(aligned-clean)`
produced infinity. That second boundary is established in the diagnostic
counterfactual, not yet shown independently reachable on unchanged production.
The control is not a complete fix or approved numerical policy. Current N2-03
interior arithmetic remains unchanged; any future correction needs its own
scope, ordinary-bit and recovery analysis, regression controls and owner review.
